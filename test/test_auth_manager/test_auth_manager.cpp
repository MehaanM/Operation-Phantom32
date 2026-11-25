// test_auth_manager.cpp — Unit tests for the auth_manager authentication logic.
//
// Tests session token lifecycle, IP lockout, password validation,
// constant-time comparison, and credential helpers.
// All tests run on native (no Arduino dependencies).

#include <unity.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>

// ============================================================
// MINIMAL STUBS (native env)
// ============================================================

#define AM_TOKEN_LEN       64
#define AM_MAX_SESSIONS    8
#define AM_SESSION_TTL_S   1800
#define AM_MAX_ATTEMPTS    5
#define AM_LOCKOUT_S       300
#define AM_MIN_PASS_LEN    8
#define AM_MAX_PASS_LEN    64
#define AM_APIKEY_LEN      32

struct AmSession {
  char     token[AM_TOKEN_LEN + 1];
  uint32_t created_s;
  uint32_t last_used_s;
  bool     valid;
  char     remote_ip[16];
};

struct AmLockout {
  char     ip[16];
  uint32_t fail_count;
  uint32_t locked_until_s;
};

static AmSession  g_am_sessions[AM_MAX_SESSIONS];
static AmLockout  g_am_lockouts[16];
static int        g_am_lockout_count;
static char       g_am_password[AM_MAX_PASS_LEN + 1];
static char       g_am_apikey[AM_APIKEY_LEN + 1] __attribute__((unused));

static uint32_t g_mock_time_s = 1000;

// Constant-time compare (prevent timing attacks).
static bool am_ct_strcmp(const char *a, const char *b) {
  if (!a || !b) return false;
  size_t la = strlen(a), lb = strlen(b);
  volatile uint8_t diff = (la != lb) ? 1 : 0;
  size_t lmax = la > lb ? la : lb;
  for (size_t i = 0; i < lmax; i++) {
    uint8_t ca = (i < la) ? (uint8_t)a[i] : 0;
    uint8_t cb = (i < lb) ? (uint8_t)b[i] : 0;
    diff |= (ca ^ cb);
  }
  return diff == 0;
}

// Generate a hex token (fake random for tests).
static void am_gen_token(char *out, size_t len) {
  static uint32_t seed = 0xDEADBEEF;
  for (size_t i = 0; i < len; i += 2) {
    seed = seed * 1664525u + 1013904223u;
    uint8_t b = (uint8_t)(seed >> 16);
    snprintf(out + i, 3, "%02x", b);
  }
  out[len] = '\0';
}

// Session management stubs.
static void am_sessions_reset() {
  memset(g_am_sessions, 0, sizeof(g_am_sessions));
}

static AmSession* am_find_session(const char *token) {
  if (!token || strlen(token) != AM_TOKEN_LEN) return nullptr;
  for (int i = 0; i < AM_MAX_SESSIONS; i++) {
    if (!g_am_sessions[i].valid) continue;
    if (am_ct_strcmp(g_am_sessions[i].token, token)) return &g_am_sessions[i];
  }
  return nullptr;
}

static AmSession* am_create_session(const char *ip) {
  for (int i = 0; i < AM_MAX_SESSIONS; i++) {
    if (!g_am_sessions[i].valid) {
      am_gen_token(g_am_sessions[i].token, AM_TOKEN_LEN);
      g_am_sessions[i].created_s  = g_mock_time_s;
      g_am_sessions[i].last_used_s = g_mock_time_s;
      g_am_sessions[i].valid       = true;
      if (ip) strncpy(g_am_sessions[i].remote_ip, ip, 15);
      return &g_am_sessions[i];
    }
  }
  return nullptr; // no free slot
}

static bool am_invalidate_session(const char *token) {
  AmSession *s = am_find_session(token);
  if (!s) return false;
  s->valid = false;
  memset(s->token, 0, sizeof(s->token));
  return true;
}

static bool am_session_valid(const char *token) {
  AmSession *s = am_find_session(token);
  if (!s) return false;
  if ((g_mock_time_s - s->created_s) > AM_SESSION_TTL_S) {
    s->valid = false;
    return false;
  }
  s->last_used_s = g_mock_time_s;
  return true;
}

static int am_session_count() {
  int c = 0;
  for (int i = 0; i < AM_MAX_SESSIONS; i++) if (g_am_sessions[i].valid) c++;
  return c;
}

// Lockout stubs.
static void am_lockouts_reset() {
  memset(g_am_lockouts, 0, sizeof(g_am_lockouts));
  g_am_lockout_count = 0;
}

static AmLockout* am_find_lockout(const char *ip) {
  for (int i = 0; i < g_am_lockout_count; i++)
    if (strcmp(g_am_lockouts[i].ip, ip) == 0) return &g_am_lockouts[i];
  return nullptr;
}

static AmLockout* am_ensure_lockout(const char *ip) {
  AmLockout *l = am_find_lockout(ip);
  if (l) return l;
  if (g_am_lockout_count >= 16) return nullptr;
  l = &g_am_lockouts[g_am_lockout_count++];
  strncpy(l->ip, ip, 15);
  l->fail_count = 0;
  l->locked_until_s = 0;
  return l;
}

static bool am_is_locked(const char *ip) {
  AmLockout *l = am_find_lockout(ip);
  if (!l) return false;
  if (l->locked_until_s == 0) return false;
  return g_mock_time_s < l->locked_until_s;
}

static void am_record_fail(const char *ip) {
  AmLockout *l = am_ensure_lockout(ip);
  if (!l) return;
  l->fail_count++;
  if (l->fail_count >= AM_MAX_ATTEMPTS)
    l->locked_until_s = g_mock_time_s + AM_LOCKOUT_S;
}

static void am_record_success(const char *ip) {
  AmLockout *l = am_find_lockout(ip);
  if (!l) return;
  l->fail_count = 0;
  l->locked_until_s = 0;
}

// Password validation.
static bool am_validate_password(const char *pw, char *err_out = nullptr) {
  if (!pw) { if (err_out) strcpy(err_out, "null"); return false; }
  int len = strlen(pw);
  if (len < AM_MIN_PASS_LEN) { if (err_out) snprintf(err_out, 64, "too short (%d)", len); return false; }
  if (len > AM_MAX_PASS_LEN) { if (err_out) snprintf(err_out, 64, "too long (%d)", len); return false; }
  bool has_upper = false, has_digit = false;
  for (int i = 0; i < len; i++) {
    if (isupper(pw[i])) has_upper = true;
    if (isdigit(pw[i])) has_digit = true;
  }
  if (!has_upper) { if (err_out) strcpy(err_out, "needs uppercase"); return false; }
  if (!has_digit) { if (err_out) strcpy(err_out, "needs digit"); return false; }
  return true;
}

static void reset_all() {
  am_sessions_reset();
  am_lockouts_reset();
  g_mock_time_s = 1000;
  memset(g_am_password, 0, sizeof(g_am_password));
  strcpy(g_am_password, "Phantom32!");
}

// ============================================================
// TESTS: Constant-time comparison
// ============================================================

void test_ct_strcmp_equal() {
  TEST_ASSERT_TRUE(am_ct_strcmp("hello", "hello"));
}

void test_ct_strcmp_not_equal() {
  TEST_ASSERT_FALSE(am_ct_strcmp("hello", "world"));
}

void test_ct_strcmp_different_length() {
  TEST_ASSERT_FALSE(am_ct_strcmp("hello", "helloo"));
}

void test_ct_strcmp_empty_equal() {
  TEST_ASSERT_TRUE(am_ct_strcmp("", ""));
}

void test_ct_strcmp_null_a() {
  TEST_ASSERT_FALSE(am_ct_strcmp(nullptr, "hello"));
}

void test_ct_strcmp_null_b() {
  TEST_ASSERT_FALSE(am_ct_strcmp("hello", nullptr));
}

void test_ct_strcmp_case_sensitive() {
  TEST_ASSERT_FALSE(am_ct_strcmp("Hello", "hello"));
}

// ============================================================
// TESTS: Session creation and lookup
// ============================================================

void test_session_create() {
  reset_all();
  AmSession *s = am_create_session("192.168.4.2");
  TEST_ASSERT_NOT_NULL(s);
  TEST_ASSERT_TRUE(s->valid);
  TEST_ASSERT_EQUAL(AM_TOKEN_LEN, (int)strlen(s->token));
}

void test_session_find_valid() {
  reset_all();
  AmSession *s = am_create_session("192.168.4.2");
  char tok[AM_TOKEN_LEN + 1];
  strncpy(tok, s->token, AM_TOKEN_LEN);
  tok[AM_TOKEN_LEN] = '\0';
  TEST_ASSERT_NOT_NULL(am_find_session(tok));
}

void test_session_find_wrong_token() {
  reset_all();
  am_create_session("192.168.4.2");
  char bad[AM_TOKEN_LEN + 1];
  memset(bad, 'z', AM_TOKEN_LEN);
  bad[AM_TOKEN_LEN] = '\0';
  TEST_ASSERT_NULL(am_find_session(bad));
}

void test_session_invalidate() {
  reset_all();
  AmSession *s = am_create_session("192.168.4.2");
  char tok[AM_TOKEN_LEN + 1];
  strncpy(tok, s->token, AM_TOKEN_LEN);
  tok[AM_TOKEN_LEN] = '\0';
  TEST_ASSERT_TRUE(am_invalidate_session(tok));
  TEST_ASSERT_NULL(am_find_session(tok));
}

void test_session_max_cap() {
  reset_all();
  for (int i = 0; i < AM_MAX_SESSIONS; i++)
    TEST_ASSERT_NOT_NULL(am_create_session("192.168.4.2"));
  TEST_ASSERT_NULL(am_create_session("192.168.4.2")); // over capacity
}

void test_session_count() {
  reset_all();
  am_create_session("192.168.4.2");
  am_create_session("192.168.4.3");
  TEST_ASSERT_EQUAL(2, am_session_count());
}

void test_session_valid_alive() {
  reset_all();
  AmSession *s = am_create_session("192.168.4.2");
  char tok[AM_TOKEN_LEN + 1];
  strncpy(tok, s->token, AM_TOKEN_LEN);
  tok[AM_TOKEN_LEN] = '\0';
  TEST_ASSERT_TRUE(am_session_valid(tok));
}

void test_session_expired() {
  reset_all();
  AmSession *s = am_create_session("192.168.4.2");
  char tok[AM_TOKEN_LEN + 1];
  strncpy(tok, s->token, AM_TOKEN_LEN);
  tok[AM_TOKEN_LEN] = '\0';
  g_mock_time_s += AM_SESSION_TTL_S + 1;
  TEST_ASSERT_FALSE(am_session_valid(tok));
}

void test_session_not_expired_before_ttl() {
  reset_all();
  AmSession *s = am_create_session("192.168.4.2");
  char tok[AM_TOKEN_LEN + 1];
  strncpy(tok, s->token, AM_TOKEN_LEN);
  tok[AM_TOKEN_LEN] = '\0';
  g_mock_time_s += AM_SESSION_TTL_S - 1;
  TEST_ASSERT_TRUE(am_session_valid(tok));
}

void test_session_slot_reuse_after_invalidate() {
  reset_all();
  for (int i = 0; i < AM_MAX_SESSIONS; i++) am_create_session("1.2.3.4");
  TEST_ASSERT_EQUAL(AM_MAX_SESSIONS, am_session_count());
  char tok[AM_TOKEN_LEN + 1];
  strncpy(tok, g_am_sessions[0].token, AM_TOKEN_LEN);
  tok[AM_TOKEN_LEN] = '\0';
  am_invalidate_session(tok);
  TEST_ASSERT_NOT_NULL(am_create_session("5.6.7.8")); // should reuse slot
}

// ============================================================
// TESTS: IP lockout
// ============================================================

void test_lockout_not_locked_initially() {
  reset_all();
  TEST_ASSERT_FALSE(am_is_locked("10.0.0.1"));
}

void test_lockout_below_threshold() {
  reset_all();
  for (int i = 0; i < AM_MAX_ATTEMPTS - 1; i++) am_record_fail("10.0.0.1");
  TEST_ASSERT_FALSE(am_is_locked("10.0.0.1"));
}

void test_lockout_at_threshold() {
  reset_all();
  for (int i = 0; i < AM_MAX_ATTEMPTS; i++) am_record_fail("10.0.0.1");
  TEST_ASSERT_TRUE(am_is_locked("10.0.0.1"));
}

void test_lockout_expires() {
  reset_all();
  for (int i = 0; i < AM_MAX_ATTEMPTS; i++) am_record_fail("10.0.0.1");
  g_mock_time_s += AM_LOCKOUT_S + 1;
  TEST_ASSERT_FALSE(am_is_locked("10.0.0.1"));
}

void test_lockout_cleared_on_success() {
  reset_all();
  for (int i = 0; i < AM_MAX_ATTEMPTS; i++) am_record_fail("10.0.0.1");
  g_mock_time_s += AM_LOCKOUT_S + 1; // expire first
  am_record_success("10.0.0.1");
  TEST_ASSERT_FALSE(am_is_locked("10.0.0.1"));
  AmLockout *l = am_find_lockout("10.0.0.1");
  TEST_ASSERT_NOT_NULL(l);
  TEST_ASSERT_EQUAL(0u, l->fail_count);
}

void test_lockout_independent_ips() {
  reset_all();
  for (int i = 0; i < AM_MAX_ATTEMPTS; i++) am_record_fail("10.0.0.1");
  TEST_ASSERT_FALSE(am_is_locked("10.0.0.2")); // different IP unaffected
}

// ============================================================
// TESTS: Password validation
// ============================================================

void test_password_valid() {
  TEST_ASSERT_TRUE(am_validate_password("Phantom32!"));
}

void test_password_too_short() {
  TEST_ASSERT_FALSE(am_validate_password("Pass1!"));
}

void test_password_no_uppercase() {
  TEST_ASSERT_FALSE(am_validate_password("phantom32!"));
}

void test_password_no_digit() {
  TEST_ASSERT_FALSE(am_validate_password("PhantomPass!"));
}

void test_password_exact_min_len() {
  TEST_ASSERT_TRUE(am_validate_password("Phantom1")); // 8 chars, upper + digit
}

void test_password_null() {
  TEST_ASSERT_FALSE(am_validate_password(nullptr));
}

void test_password_error_message() {
  char err[64];
  am_validate_password("short", err);
  TEST_ASSERT_NOT_EQUAL(0, strlen(err));
}

// ============================================================
// TESTS: Token format
// ============================================================

// ============================================================
// TESTS: Uncovered branches (null/boundary/full-table paths)
// ============================================================

void test_find_session_null_token() {
  reset_all();
  am_create_session("1.2.3.4");
  TEST_ASSERT_NULL(am_find_session(nullptr));
}

void test_find_session_short_token() {
  reset_all();
  am_create_session("1.2.3.4");
  TEST_ASSERT_NULL(am_find_session("tooshort"));
}

void test_find_session_one_over_len() {
  reset_all();
  am_create_session("1.2.3.4");
  char tok[AM_TOKEN_LEN + 2];
  memset(tok, 'a', AM_TOKEN_LEN + 1);
  tok[AM_TOKEN_LEN + 1] = '\0';
  TEST_ASSERT_NULL(am_find_session(tok));
}

void test_create_session_null_ip() {
  reset_all();
  AmSession *s = am_create_session(nullptr);
  TEST_ASSERT_NOT_NULL(s);
  TEST_ASSERT_TRUE(s->valid);
}

void test_invalidate_session_not_found() {
  reset_all();
  char bad[AM_TOKEN_LEN + 1];
  memset(bad, 'z', AM_TOKEN_LEN);
  bad[AM_TOKEN_LEN] = '\0';
  TEST_ASSERT_FALSE(am_invalidate_session(bad));
}

void test_session_valid_null_token() {
  reset_all();
  TEST_ASSERT_FALSE(am_session_valid(nullptr));
}

void test_session_valid_at_exact_ttl() {
  reset_all();
  AmSession *s = am_create_session("1.2.3.4");
  char tok[AM_TOKEN_LEN + 1];
  strncpy(tok, s->token, AM_TOKEN_LEN);
  tok[AM_TOKEN_LEN] = '\0';
  g_mock_time_s += AM_SESSION_TTL_S; // exactly at TTL — NOT expired (> not >=)
  TEST_ASSERT_TRUE(am_session_valid(tok));
}

void test_lockout_table_full() {
  reset_all();
  char ip[16];
  for (int i = 0; i < 16; i++) {
    snprintf(ip, sizeof(ip), "10.0.%d.1", i);
    TEST_ASSERT_NOT_NULL(am_ensure_lockout(ip));
  }
  // 17th entry must fail — table full
  TEST_ASSERT_NULL(am_ensure_lockout("99.99.99.99"));
}

void test_record_success_ip_not_in_table() {
  reset_all();
  // Should be a no-op without crash — IP has no lockout entry
  am_record_success("192.168.100.100");
  TEST_ASSERT_FALSE(am_is_locked("192.168.100.100"));
}

void test_password_too_long() {
  char pw[AM_MAX_PASS_LEN + 2];
  memset(pw, 'a', AM_MAX_PASS_LEN + 1);
  pw[0] = 'A'; pw[1] = '1';
  pw[AM_MAX_PASS_LEN + 1] = '\0';
  TEST_ASSERT_FALSE(am_validate_password(pw));
}

void test_password_error_msg_null() {
  char err[64] = {0};
  am_validate_password(nullptr, err);
  TEST_ASSERT_EQUAL_STRING("null", err);
}

void test_password_error_msg_too_long() {
  char pw[AM_MAX_PASS_LEN + 2];
  memset(pw, 'a', AM_MAX_PASS_LEN + 1);
  pw[0] = 'A'; pw[1] = '1';
  pw[AM_MAX_PASS_LEN + 1] = '\0';
  char err[64] = {0};
  am_validate_password(pw, err);
  TEST_ASSERT_NOT_NULL(strstr(err, "too long"));
}

void test_password_error_msg_needs_digit() {
  char err[64] = {0};
  am_validate_password("PhantomPass", err);
  TEST_ASSERT_EQUAL_STRING("needs digit", err);
}

void test_session_last_used_updated() {
  reset_all();
  AmSession *s = am_create_session("1.2.3.4");
  char tok[AM_TOKEN_LEN + 1];
  strncpy(tok, s->token, AM_TOKEN_LEN);
  tok[AM_TOKEN_LEN] = '\0';
  g_mock_time_s += 100;
  am_session_valid(tok);
  TEST_ASSERT_EQUAL(g_mock_time_s, s->last_used_s);
}

void test_lockout_at_exact_expiry_time() {
  reset_all();
  for (int i = 0; i < AM_MAX_ATTEMPTS; i++) am_record_fail("10.1.2.3");
  g_mock_time_s += AM_LOCKOUT_S; // exactly at locked_until_s — not locked (< not <=)
  TEST_ASSERT_FALSE(am_is_locked("10.1.2.3"));
}

void test_session_count_all_invalid() {
  reset_all(); // all sessions invalid after reset
  TEST_ASSERT_EQUAL(0, am_session_count());
}

void test_token_hex_chars_only() {
  reset_all();
  AmSession *s = am_create_session("1.2.3.4");
  for (int i = 0; i < AM_TOKEN_LEN; i++) {
    char c = s->token[i];
    bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    TEST_ASSERT_TRUE(ok);
  }
}

void test_token_length_exact() {
  reset_all();
  AmSession *s = am_create_session("1.2.3.4");
  TEST_ASSERT_EQUAL(AM_TOKEN_LEN, (int)strlen(s->token));
}

// ============================================================
// ENTRY POINT
// ============================================================

void setUp()   { reset_all(); }
void tearDown() {}

int main(int argc, char **argv) {
  UNITY_BEGIN();

  RUN_TEST(test_ct_strcmp_equal);
  RUN_TEST(test_ct_strcmp_not_equal);
  RUN_TEST(test_ct_strcmp_different_length);
  RUN_TEST(test_ct_strcmp_empty_equal);
  RUN_TEST(test_ct_strcmp_null_a);
  RUN_TEST(test_ct_strcmp_null_b);
  RUN_TEST(test_ct_strcmp_case_sensitive);

  RUN_TEST(test_session_create);
  RUN_TEST(test_session_find_valid);
  RUN_TEST(test_session_find_wrong_token);
  RUN_TEST(test_session_invalidate);
  RUN_TEST(test_session_max_cap);
  RUN_TEST(test_session_count);
  RUN_TEST(test_session_valid_alive);
  RUN_TEST(test_session_expired);
  RUN_TEST(test_session_not_expired_before_ttl);
  RUN_TEST(test_session_slot_reuse_after_invalidate);

  RUN_TEST(test_lockout_not_locked_initially);
  RUN_TEST(test_lockout_below_threshold);
  RUN_TEST(test_lockout_at_threshold);
  RUN_TEST(test_lockout_expires);
  RUN_TEST(test_lockout_cleared_on_success);
  RUN_TEST(test_lockout_independent_ips);

  RUN_TEST(test_password_valid);
  RUN_TEST(test_password_too_short);
  RUN_TEST(test_password_no_uppercase);
  RUN_TEST(test_password_no_digit);
  RUN_TEST(test_password_exact_min_len);
  RUN_TEST(test_password_null);
  RUN_TEST(test_password_error_message);

  RUN_TEST(test_token_hex_chars_only);
  RUN_TEST(test_token_length_exact);

  // Uncovered branch tests
  RUN_TEST(test_find_session_null_token);
  RUN_TEST(test_find_session_short_token);
  RUN_TEST(test_find_session_one_over_len);
  RUN_TEST(test_create_session_null_ip);
  RUN_TEST(test_invalidate_session_not_found);
  RUN_TEST(test_session_valid_null_token);
  RUN_TEST(test_session_valid_at_exact_ttl);
  RUN_TEST(test_lockout_table_full);
  RUN_TEST(test_record_success_ip_not_in_table);
  RUN_TEST(test_password_too_long);
  RUN_TEST(test_password_error_msg_null);
  RUN_TEST(test_password_error_msg_too_long);
  RUN_TEST(test_password_error_msg_needs_digit);
  RUN_TEST(test_session_last_used_updated);
  RUN_TEST(test_lockout_at_exact_expiry_time);
  RUN_TEST(test_session_count_all_invalid);

  return UNITY_END();
}

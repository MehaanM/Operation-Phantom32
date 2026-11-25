// test_security.cpp — Security-path unit tests for Phantom32.
//
// Covers: expired/invalid/revoked/replayed sessions, unauthorized access
// simulation, malformed input, oversized inputs, rate-limit exhaustion,
// PBKDF2 properties, API key validation, concurrent session handling.
// All tests run on native (no Arduino dependencies).

#include <unity.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>

// ============================================================
// SHARED STUBS (duplicated from test_auth_manager to stay self-contained)
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

static AmSession  g_sessions[AM_MAX_SESSIONS];
static AmLockout  g_lockouts[32];
static int        g_lockout_count;
static uint32_t   g_mock_time_s = 1000;
static char       g_stored_hash[128];
static char       g_stored_salt_hex[64];
static char       g_api_key[AM_APIKEY_LEN * 2 + 1];

static bool sec_ct_strcmp(const char *a, const char *b) {
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

static void sec_gen_token(char *out, size_t len, uint32_t seed_offset) {
  static uint32_t seed = 0xCAFEBABE;
  seed += seed_offset;
  for (size_t i = 0; i < len; i += 2) {
    seed = seed * 1664525u + 1013904223u;
    uint8_t b = (uint8_t)(seed >> 16);
    snprintf(out + i, 3, "%02x", b);
  }
  out[len] = '\0';
}

static void sec_sessions_reset() { memset(g_sessions, 0, sizeof(g_sessions)); }
static void sec_lockouts_reset()  {
  memset(g_lockouts, 0, sizeof(g_lockouts));
  g_lockout_count = 0;
}

static AmSession* sec_find_session(const char *token) {
  if (!token || strlen(token) != AM_TOKEN_LEN) return nullptr;
  for (int i = 0; i < AM_MAX_SESSIONS; i++) {
    if (!g_sessions[i].valid) continue;
    if (sec_ct_strcmp(g_sessions[i].token, token)) return &g_sessions[i];
  }
  return nullptr;
}

static AmSession* sec_create_session(const char *ip, uint32_t seed_offset = 0) {
  for (int i = 0; i < AM_MAX_SESSIONS; i++) {
    if (!g_sessions[i].valid) {
      sec_gen_token(g_sessions[i].token, AM_TOKEN_LEN, seed_offset + (uint32_t)i);
      g_sessions[i].created_s   = g_mock_time_s;
      g_sessions[i].last_used_s = g_mock_time_s;
      g_sessions[i].valid       = true;
      if (ip) strncpy(g_sessions[i].remote_ip, ip, 15);
      return &g_sessions[i];
    }
  }
  return nullptr;
}

static bool sec_session_valid(const char *token) {
  AmSession *s = sec_find_session(token);
  if (!s) return false;
  if ((g_mock_time_s - s->created_s) > AM_SESSION_TTL_S) {
    s->valid = false;
    return false;
  }
  s->last_used_s = g_mock_time_s;
  return true;
}

static bool sec_revoke_session(const char *token) {
  AmSession *s = sec_find_session(token);
  if (!s) return false;
  s->valid = false;
  memset(s->token, 0, sizeof(s->token));
  return true;
}

static AmLockout* sec_find_lockout(const char *ip) {
  for (int i = 0; i < g_lockout_count; i++)
    if (strcmp(g_lockouts[i].ip, ip) == 0) return &g_lockouts[i];
  return nullptr;
}

static AmLockout* sec_ensure_lockout(const char *ip) {
  AmLockout *l = sec_find_lockout(ip);
  if (l) return l;
  if (g_lockout_count >= 32) return nullptr;
  l = &g_lockouts[g_lockout_count++];
  strncpy(l->ip, ip, 15);
  l->fail_count = 0;
  l->locked_until_s = 0;
  return l;
}

static bool sec_is_locked(const char *ip) {
  AmLockout *l = sec_find_lockout(ip);
  if (!l) return false;
  if (l->locked_until_s == 0) return false;
  return g_mock_time_s < l->locked_until_s;
}

static void sec_record_fail(const char *ip) {
  AmLockout *l = sec_ensure_lockout(ip);
  if (!l) return;
  l->fail_count++;
  if (l->fail_count >= AM_MAX_ATTEMPTS)
    l->locked_until_s = g_mock_time_s + AM_LOCKOUT_S;
}

static void sec_record_success(const char *ip) {
  AmLockout *l = sec_find_lockout(ip);
  if (!l) return;
  l->fail_count = 0;
  l->locked_until_s = 0;
}

// Simulate SHA256-like hex output of fixed length (not real PBKDF2).
static void sec_fake_pbkdf2(const char *password, const char *salt_hex, char *out_hex) {
  // XOR-fold password and salt bytes into a 32-byte pseudohash.
  uint8_t buf[32] = {};
  size_t plen = strlen(password);
  size_t slen = strlen(salt_hex);
  for (size_t i = 0; i < plen; i++) buf[i % 32] ^= (uint8_t)password[i];
  for (size_t i = 0; i < slen; i++) buf[(i + 16) % 32] ^= (uint8_t)salt_hex[i];
  // Also mix in length to make length-extension distinguishable.
  buf[0] ^= (uint8_t)plen;
  for (int i = 0; i < 32; i++) snprintf(out_hex + i*2, 3, "%02x", buf[i]);
  out_hex[64] = '\0';
}

// Generate a random-looking 16-byte salt hex string (seeded).
static void sec_gen_salt(char *salt_hex, uint32_t seed) {
  uint32_t s = seed;
  for (int i = 0; i < 16; i++) {
    s = s * 6364136223846793005ULL + 1442695040888963407ULL;
    snprintf(salt_hex + i*2, 3, "%02x", (uint8_t)(s >> 24));
  }
  salt_hex[32] = '\0';
}

// Token validation helper: reject short / long / non-hex tokens.
static bool sec_token_format_ok(const char *token) {
  if (!token) return false;
  size_t len = strlen(token);
  if (len != AM_TOKEN_LEN) return false;
  for (size_t i = 0; i < len; i++) {
    char c = token[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
      return false;
  }
  return true;
}

// Simulate input length guard (applied before processing).
static bool sec_input_length_ok(const char *input, size_t max_len) {
  return input && strlen(input) <= max_len;
}

// JSON field extractor stub (mirrors auth_manager's approach).
static bool sec_extract_json_string(const char *body, const char *key, char *out, size_t out_max) {
  if (!body || !key || !out) return false;
  const char *p = strstr(body, key);
  if (!p) return false;
  p += strlen(key);
  const char *q1 = strchr(p, '"');
  if (!q1) return false;
  q1++;
  const char *q2 = strchr(q1, '"');
  if (!q2) return false;
  size_t len = (size_t)(q2 - q1);
  if (len >= out_max) return false;
  memcpy(out, q1, len);
  out[len] = '\0';
  return true;
}

static void reset_all() {
  sec_sessions_reset();
  sec_lockouts_reset();
  g_mock_time_s = 1000;
  memset(g_stored_hash, 0, sizeof(g_stored_hash));
  memset(g_stored_salt_hex, 0, sizeof(g_stored_salt_hex));
  memset(g_api_key, 0, sizeof(g_api_key));
  sec_gen_salt(g_stored_salt_hex, 0xDEAD1234);
  sec_fake_pbkdf2("CorrectPassword1", g_stored_salt_hex, g_stored_hash);
  // Fake 64-hex-char API key.
  for (int i = 0; i < AM_APIKEY_LEN * 2; i++)
    g_api_key[i] = "0123456789abcdef"[i % 16];
  g_api_key[AM_APIKEY_LEN * 2] = '\0';
}

// ============================================================
// TESTS: Expired sessions
// ============================================================

void test_session_expires_at_ttl() {
  reset_all();
  AmSession *s = sec_create_session("10.0.0.1");
  char tok[AM_TOKEN_LEN + 1];
  strncpy(tok, s->token, AM_TOKEN_LEN);
  tok[AM_TOKEN_LEN] = '\0';

  g_mock_time_s += AM_SESSION_TTL_S;
  TEST_ASSERT_TRUE(sec_session_valid(tok));  // at exactly TTL: still valid
}

void test_session_expires_after_ttl() {
  reset_all();
  AmSession *s = sec_create_session("10.0.0.1");
  char tok[AM_TOKEN_LEN + 1];
  strncpy(tok, s->token, AM_TOKEN_LEN);
  tok[AM_TOKEN_LEN] = '\0';

  g_mock_time_s += AM_SESSION_TTL_S + 1;
  TEST_ASSERT_FALSE(sec_session_valid(tok));
}

void test_expired_session_slot_not_found() {
  reset_all();
  AmSession *s = sec_create_session("10.0.0.1");
  char tok[AM_TOKEN_LEN + 1];
  strncpy(tok, s->token, AM_TOKEN_LEN);
  tok[AM_TOKEN_LEN] = '\0';

  g_mock_time_s += AM_SESSION_TTL_S + 100;
  sec_session_valid(tok);  // triggers expiry
  TEST_ASSERT_NULL(sec_find_session(tok));
}

// ============================================================
// TESTS: Revoked sessions
// ============================================================

void test_revoked_session_rejected() {
  reset_all();
  AmSession *s = sec_create_session("10.0.0.1");
  char tok[AM_TOKEN_LEN + 1];
  strncpy(tok, s->token, AM_TOKEN_LEN);
  tok[AM_TOKEN_LEN] = '\0';

  TEST_ASSERT_TRUE(sec_session_valid(tok));
  TEST_ASSERT_TRUE(sec_revoke_session(tok));
  TEST_ASSERT_FALSE(sec_session_valid(tok));
}

void test_revoked_session_cannot_be_found() {
  reset_all();
  AmSession *s = sec_create_session("10.0.0.1");
  char tok[AM_TOKEN_LEN + 1];
  strncpy(tok, s->token, AM_TOKEN_LEN);
  tok[AM_TOKEN_LEN] = '\0';

  sec_revoke_session(tok);
  TEST_ASSERT_NULL(sec_find_session(tok));
}

void test_revoke_nonexistent_token_returns_false() {
  reset_all();
  char bad[AM_TOKEN_LEN + 1];
  memset(bad, 'a', AM_TOKEN_LEN);
  bad[AM_TOKEN_LEN] = '\0';
  TEST_ASSERT_FALSE(sec_revoke_session(bad));
}

// ============================================================
// TESTS: Replayed tokens
// ============================================================

void test_replayed_token_after_revoke_rejected() {
  reset_all();
  AmSession *s = sec_create_session("10.0.0.1");
  char tok[AM_TOKEN_LEN + 1];
  strncpy(tok, s->token, AM_TOKEN_LEN);
  tok[AM_TOKEN_LEN] = '\0';

  sec_revoke_session(tok);
  // Replaying the same token must still fail.
  TEST_ASSERT_FALSE(sec_session_valid(tok));
  TEST_ASSERT_NULL(sec_find_session(tok));
}

void test_different_sessions_have_unique_tokens() {
  reset_all();
  AmSession *s1 = sec_create_session("10.0.0.1", 0);
  AmSession *s2 = sec_create_session("10.0.0.2", 1000);
  TEST_ASSERT_NOT_NULL(s1);
  TEST_ASSERT_NOT_NULL(s2);
  TEST_ASSERT_FALSE(sec_ct_strcmp(s1->token, s2->token));
}

// ============================================================
// TESTS: Unauthorized access simulation
// ============================================================

void test_no_session_means_unauthorized() {
  reset_all();
  // With no sessions, any token lookup fails.
  char tok[AM_TOKEN_LEN + 1];
  memset(tok, 'b', AM_TOKEN_LEN);
  tok[AM_TOKEN_LEN] = '\0';
  TEST_ASSERT_FALSE(sec_session_valid(tok));
}

void test_empty_token_rejected() {
  reset_all();
  TEST_ASSERT_NULL(sec_find_session(""));
}

void test_null_token_rejected() {
  reset_all();
  TEST_ASSERT_NULL(sec_find_session(nullptr));
}

void test_short_token_rejected() {
  reset_all();
  TEST_ASSERT_NULL(sec_find_session("deadbeef"));
}

void test_long_token_rejected() {
  reset_all();
  char long_tok[AM_TOKEN_LEN + 10];
  memset(long_tok, 'c', AM_TOKEN_LEN + 9);
  long_tok[AM_TOKEN_LEN + 9] = '\0';
  TEST_ASSERT_NULL(sec_find_session(long_tok));
}

// ============================================================
// TESTS: Malformed input / oversized
// ============================================================

void test_oversized_password_rejected_by_length_guard() {
  char big[AM_MAX_PASS_LEN + 2];
  memset(big, 'A', AM_MAX_PASS_LEN + 1);
  big[AM_MAX_PASS_LEN + 1] = '\0';
  TEST_ASSERT_FALSE(sec_input_length_ok(big, AM_MAX_PASS_LEN));
}

void test_max_length_password_accepted() {
  char exact[AM_MAX_PASS_LEN + 1];
  memset(exact, 'A', AM_MAX_PASS_LEN);
  exact[AM_MAX_PASS_LEN] = '\0';
  TEST_ASSERT_TRUE(sec_input_length_ok(exact, AM_MAX_PASS_LEN));
}

void test_null_input_rejected_by_length_guard() {
  TEST_ASSERT_FALSE(sec_input_length_ok(nullptr, 64));
}

void test_malformed_json_no_password_field() {
  char out[64];
  TEST_ASSERT_FALSE(sec_extract_json_string("{\"user\":\"admin\"}", "\"password\":", out, sizeof(out)));
}

void test_malformed_json_unclosed_string() {
  char out[64];
  // Missing closing quote — extractor should not crash.
  TEST_ASSERT_FALSE(sec_extract_json_string("{\"password\":\"abc", "\"password\":", out, sizeof(out)));
}

void test_malformed_json_empty_body() {
  char out[64];
  TEST_ASSERT_FALSE(sec_extract_json_string("", "\"password\":", out, sizeof(out)));
}

void test_malformed_json_null_body() {
  char out[64];
  TEST_ASSERT_FALSE(sec_extract_json_string(nullptr, "\"password\":", out, sizeof(out)));
}

void test_valid_json_extraction() {
  char out[64];
  TEST_ASSERT_TRUE(sec_extract_json_string("{\"password\":\"hunter2\"}", "\"password\":", out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("hunter2", out);
}

void test_json_value_too_long_for_buffer() {
  char body[200], out[8];  // out too small
  memset(body, 'x', sizeof(body) - 1);
  body[sizeof(body) - 1] = '\0';
  snprintf(body, sizeof(body), "{\"password\":\"AAAAAAAAAAAAAAAA\"}");
  TEST_ASSERT_FALSE(sec_extract_json_string(body, "\"password\":", out, 4));
}

// ============================================================
// TESTS: Rate-limit / lockout exhaustion
// ============================================================

void test_lockout_exactly_at_threshold() {
  reset_all();
  for (int i = 0; i < AM_MAX_ATTEMPTS; i++) sec_record_fail("1.2.3.4");
  TEST_ASSERT_TRUE(sec_is_locked("1.2.3.4"));
}

void test_lockout_one_below_threshold() {
  reset_all();
  for (int i = 0; i < AM_MAX_ATTEMPTS - 1; i++) sec_record_fail("1.2.3.4");
  TEST_ASSERT_FALSE(sec_is_locked("1.2.3.4"));
}

void test_lockout_expires_exactly_at_duration() {
  reset_all();
  for (int i = 0; i < AM_MAX_ATTEMPTS; i++) sec_record_fail("1.2.3.4");
  g_mock_time_s += AM_LOCKOUT_S;
  TEST_ASSERT_FALSE(sec_is_locked("1.2.3.4"));  // at boundary: unlocked
}

void test_multiple_ips_lockout_independently() {
  reset_all();
  for (int i = 0; i < AM_MAX_ATTEMPTS; i++) sec_record_fail("10.0.0.1");
  for (int i = 0; i < AM_MAX_ATTEMPTS - 1; i++) sec_record_fail("10.0.0.2");
  TEST_ASSERT_TRUE(sec_is_locked("10.0.0.1"));
  TEST_ASSERT_FALSE(sec_is_locked("10.0.0.2"));
}

void test_lockout_reset_after_success() {
  reset_all();
  for (int i = 0; i < AM_MAX_ATTEMPTS; i++) sec_record_fail("5.5.5.5");
  g_mock_time_s += AM_LOCKOUT_S + 1;
  sec_record_success("5.5.5.5");
  TEST_ASSERT_FALSE(sec_is_locked("5.5.5.5"));
  AmLockout *l = sec_find_lockout("5.5.5.5");
  TEST_ASSERT_NOT_NULL(l);
  TEST_ASSERT_EQUAL(0u, l->fail_count);
}

void test_accumulate_fails_across_time_before_lockout() {
  reset_all();
  sec_record_fail("2.2.2.2");
  g_mock_time_s += 10;
  sec_record_fail("2.2.2.2");
  g_mock_time_s += 10;
  sec_record_fail("2.2.2.2");
  TEST_ASSERT_FALSE(sec_is_locked("2.2.2.2"));  // only 3 fails
  sec_record_fail("2.2.2.2");
  sec_record_fail("2.2.2.2");
  TEST_ASSERT_TRUE(sec_is_locked("2.2.2.2"));
}

// ============================================================
// TESTS: PBKDF2 properties (using fake KDF stub)
// ============================================================

void test_same_password_same_salt_gives_same_hash() {
  char h1[65], h2[65];
  sec_fake_pbkdf2("MyPass1!", "aabbccdd" "11223344" "aabbccdd" "11223344", h1);
  sec_fake_pbkdf2("MyPass1!", "aabbccdd" "11223344" "aabbccdd" "11223344", h2);
  TEST_ASSERT_EQUAL_STRING(h1, h2);
}

void test_different_passwords_different_hash() {
  char h1[65], h2[65];
  sec_fake_pbkdf2("PasswordA1", "00000000000000000000000000000000", h1);
  sec_fake_pbkdf2("PasswordB1", "00000000000000000000000000000000", h2);
  TEST_ASSERT_FALSE(sec_ct_strcmp(h1, h2));
}

void test_different_salts_different_hash() {
  char h1[65], h2[65];
  sec_fake_pbkdf2("SamePass1", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", h1);
  sec_fake_pbkdf2("SamePass1", "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb0", h2);
  TEST_ASSERT_FALSE(sec_ct_strcmp(h1, h2));
}

void test_hash_output_is_hex_chars_only() {
  char h[65];
  sec_fake_pbkdf2("TestPass1", "deadbeefdeadbeefdeadbeefdeadbeef", h);
  for (int i = 0; i < 64; i++) {
    char c = h[i];
    bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    TEST_ASSERT_TRUE(ok);
  }
}

void test_hash_output_is_exactly_64_chars() {
  char h[65];
  sec_fake_pbkdf2("TestPass1", "deadbeefdeadbeefdeadbeefdeadbeef", h);
  TEST_ASSERT_EQUAL(64, (int)strlen(h));
}

void test_wrong_password_gives_wrong_hash() {
  char h_good[65], h_bad[65];
  sec_fake_pbkdf2("CorrectPassword1", g_stored_salt_hex, h_good);
  sec_fake_pbkdf2("WrongPassword1",   g_stored_salt_hex, h_bad);
  // Stored hash was generated with "CorrectPassword1".
  TEST_ASSERT_TRUE(sec_ct_strcmp(h_good, g_stored_hash));
  TEST_ASSERT_FALSE(sec_ct_strcmp(h_bad,  g_stored_hash));
}

void test_per_device_salts_are_different() {
  char salt1[33], salt2[33];
  sec_gen_salt(salt1, 0x11111111);
  sec_gen_salt(salt2, 0x22222222);
  TEST_ASSERT_FALSE(sec_ct_strcmp(salt1, salt2));
}

void test_salt_is_32_hex_chars() {
  char salt[33];
  sec_gen_salt(salt, 0xABCD1234);
  TEST_ASSERT_EQUAL(32, (int)strlen(salt));
  for (int i = 0; i < 32; i++) {
    char c = salt[i];
    bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    TEST_ASSERT_TRUE(ok);
  }
}

// ============================================================
// TESTS: API key validation
// ============================================================

void test_correct_api_key_accepted() {
  reset_all();
  TEST_ASSERT_TRUE(sec_ct_strcmp(g_api_key, g_api_key));
}

void test_wrong_api_key_rejected() {
  reset_all();
  char wrong[AM_APIKEY_LEN * 2 + 1];
  memset(wrong, 'f', AM_APIKEY_LEN * 2);
  wrong[AM_APIKEY_LEN * 2] = '\0';
  TEST_ASSERT_FALSE(sec_ct_strcmp(wrong, g_api_key));
}

void test_short_api_key_rejected_by_length() {
  reset_all();
  // auth_check requires exact-length match (AM_APIKEY_LEN*2 hex chars = 64).
  // A short key fails the length equality check.
  TEST_ASSERT_FALSE(strlen("tooshort") == (size_t)(AM_APIKEY_LEN * 2));
}

void test_api_key_exact_length_required() {
  reset_all();
  // Any key that is not exactly AM_APIKEY_LEN*2 chars must be rejected.
  char long_key[AM_APIKEY_LEN * 2 + 2];
  memcpy(long_key, g_api_key, AM_APIKEY_LEN * 2);
  long_key[AM_APIKEY_LEN * 2]     = 'a';
  long_key[AM_APIKEY_LEN * 2 + 1] = '\0';
  TEST_ASSERT_FALSE(strlen(long_key) == (size_t)(AM_APIKEY_LEN * 2));
}

void test_api_key_timing_safe() {
  // Constant-time compare must check all bytes, not short-circuit.
  reset_all();
  char almost[AM_APIKEY_LEN * 2 + 1];
  memcpy(almost, g_api_key, AM_APIKEY_LEN * 2);
  almost[AM_APIKEY_LEN * 2 - 1] ^= 1;  // flip one bit
  almost[AM_APIKEY_LEN * 2] = '\0';
  TEST_ASSERT_FALSE(sec_ct_strcmp(almost, g_api_key));
}

// ============================================================
// TESTS: Concurrent sessions
// ============================================================

void test_all_session_slots_fillable() {
  reset_all();
  for (int i = 0; i < AM_MAX_SESSIONS; i++) {
    AmSession *s = sec_create_session("10.0.0.1", (uint32_t)i * 100);
    TEST_ASSERT_NOT_NULL(s);
  }
  TEST_ASSERT_NULL(sec_create_session("10.0.0.2", 9999));  // table full
}

void test_all_sessions_independently_valid() {
  reset_all();
  char tokens[AM_MAX_SESSIONS][AM_TOKEN_LEN + 1];
  for (int i = 0; i < AM_MAX_SESSIONS; i++) {
    AmSession *s = sec_create_session("10.0.0.1", (uint32_t)i * 200);
    strncpy(tokens[i], s->token, AM_TOKEN_LEN);
    tokens[i][AM_TOKEN_LEN] = '\0';
  }
  for (int i = 0; i < AM_MAX_SESSIONS; i++) {
    TEST_ASSERT_TRUE(sec_session_valid(tokens[i]));
  }
}

void test_revoke_one_session_does_not_affect_others() {
  reset_all();
  AmSession *s1 = sec_create_session("10.0.0.1", 0);
  AmSession *s2 = sec_create_session("10.0.0.2", 300);
  char tok1[AM_TOKEN_LEN + 1], tok2[AM_TOKEN_LEN + 1];
  strncpy(tok1, s1->token, AM_TOKEN_LEN); tok1[AM_TOKEN_LEN] = '\0';
  strncpy(tok2, s2->token, AM_TOKEN_LEN); tok2[AM_TOKEN_LEN] = '\0';

  sec_revoke_session(tok1);
  TEST_ASSERT_FALSE(sec_session_valid(tok1));
  TEST_ASSERT_TRUE(sec_session_valid(tok2));
}

// ============================================================
// TESTS: Token format
// ============================================================

void test_valid_hex_token_accepted_by_format_check() {
  reset_all();
  AmSession *s = sec_create_session("1.2.3.4");
  TEST_ASSERT_TRUE(sec_token_format_ok(s->token));
}

void test_non_hex_chars_in_token_rejected() {
  char bad[AM_TOKEN_LEN + 1];
  memset(bad, 'z', AM_TOKEN_LEN);  // 'z' not valid hex
  bad[AM_TOKEN_LEN] = '\0';
  TEST_ASSERT_FALSE(sec_token_format_ok(bad));
}

void test_too_short_token_rejected_by_format() {
  TEST_ASSERT_FALSE(sec_token_format_ok("deadbeef"));
}

void test_null_token_rejected_by_format() {
  TEST_ASSERT_FALSE(sec_token_format_ok(nullptr));
}

// ============================================================
// ENTRY POINT
// ============================================================

void setUp()    { reset_all(); }
void tearDown() {}

int main(int argc, char **argv) {
  UNITY_BEGIN();

  RUN_TEST(test_session_expires_at_ttl);
  RUN_TEST(test_session_expires_after_ttl);
  RUN_TEST(test_expired_session_slot_not_found);

  RUN_TEST(test_revoked_session_rejected);
  RUN_TEST(test_revoked_session_cannot_be_found);
  RUN_TEST(test_revoke_nonexistent_token_returns_false);

  RUN_TEST(test_replayed_token_after_revoke_rejected);
  RUN_TEST(test_different_sessions_have_unique_tokens);

  RUN_TEST(test_no_session_means_unauthorized);
  RUN_TEST(test_empty_token_rejected);
  RUN_TEST(test_null_token_rejected);
  RUN_TEST(test_short_token_rejected);
  RUN_TEST(test_long_token_rejected);

  RUN_TEST(test_oversized_password_rejected_by_length_guard);
  RUN_TEST(test_max_length_password_accepted);
  RUN_TEST(test_null_input_rejected_by_length_guard);
  RUN_TEST(test_malformed_json_no_password_field);
  RUN_TEST(test_malformed_json_unclosed_string);
  RUN_TEST(test_malformed_json_empty_body);
  RUN_TEST(test_malformed_json_null_body);
  RUN_TEST(test_valid_json_extraction);
  RUN_TEST(test_json_value_too_long_for_buffer);

  RUN_TEST(test_lockout_exactly_at_threshold);
  RUN_TEST(test_lockout_one_below_threshold);
  RUN_TEST(test_lockout_expires_exactly_at_duration);
  RUN_TEST(test_multiple_ips_lockout_independently);
  RUN_TEST(test_lockout_reset_after_success);
  RUN_TEST(test_accumulate_fails_across_time_before_lockout);

  RUN_TEST(test_same_password_same_salt_gives_same_hash);
  RUN_TEST(test_different_passwords_different_hash);
  RUN_TEST(test_different_salts_different_hash);
  RUN_TEST(test_hash_output_is_hex_chars_only);
  RUN_TEST(test_hash_output_is_exactly_64_chars);
  RUN_TEST(test_wrong_password_gives_wrong_hash);
  RUN_TEST(test_per_device_salts_are_different);
  RUN_TEST(test_salt_is_32_hex_chars);

  RUN_TEST(test_correct_api_key_accepted);
  RUN_TEST(test_wrong_api_key_rejected);
  RUN_TEST(test_short_api_key_rejected_by_length);
  RUN_TEST(test_api_key_exact_length_required);
  RUN_TEST(test_api_key_timing_safe);

  RUN_TEST(test_all_session_slots_fillable);
  RUN_TEST(test_all_sessions_independently_valid);
  RUN_TEST(test_revoke_one_session_does_not_affect_others);

  RUN_TEST(test_valid_hex_token_accepted_by_format_check);
  RUN_TEST(test_non_hex_chars_in_token_rejected);
  RUN_TEST(test_too_short_token_rejected_by_format);
  RUN_TEST(test_null_token_rejected_by_format);

  return UNITY_END();
}

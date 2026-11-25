// test_coverage.cpp — Targeted branch-coverage tests.
//
// Exercises the ELSE branches of conditionals that are not covered by the
// primary test suites: null-pointer guards, boundary lengths, loop exits,
// error-message paths, all JSON escape characters, and rate-limiter edge cases.
// All tests run on native (no Arduino dependencies).

#include <unity.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>
#include <limits.h>

#ifdef NATIVE_ENV

// ============================================================
// INLINE STUBS — mirrors src/ logic without Arduino deps
// ============================================================

// ---- constant-time strcmp ----
static bool ct_strcmp(const char *a, const char *b) {
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

// ---- input validation helpers ----
static char g_last_error[128];
static void set_error(const char *msg) {
  strncpy(g_last_error, msg, sizeof(g_last_error) - 1);
  g_last_error[sizeof(g_last_error) - 1] = '\0';
}

static bool check_ssid(const char *ssid, int max_len) {
  if (!ssid) { set_error("null"); return false; }
  int len = (int)strlen(ssid);
  if (len == 0) { set_error("empty"); return false; }
  if (len > max_len) { set_error("too long"); return false; }
  for (int i = 0; i < len; i++) {
    uint8_t c = (uint8_t)ssid[i];
    if (c < 0x20 || c > 0x7E) { set_error("non-printable"); return false; }
  }
  return true;
}

static bool check_mac_str(const char *m) {
  if (!m) return false;
  int len = (int)strlen(m);
  if (len == 17) {
    for (int i = 0; i < 17; i++) {
      if (i % 3 == 2) { if (m[i] != ':') return false; }
      else { if (!isxdigit(m[i])) return false; }
    }
    return true;
  } else if (len == 12) {
    for (int i = 0; i < 12; i++) if (!isxdigit(m[i])) return false;
    return true;
  }
  return false;
}

static bool check_channel(int ch) { return ch >= 1 && ch <= 14; }

static bool check_int_range(const char *s, long mn, long mx, long *out) {
  if (!s) return false;
  char *end;
  long v = strtol(s, &end, 10);
  if (*end != '\0') return false;
  if (v < mn || v > mx) return false;
  if (out) *out = v;
  return true;
}

static bool check_ip(const char *ip) {
  if (!ip) return false;
  int len = (int)strlen(ip);
  if (len < 7 || len > 15) return false;
  int dots = 0;
  for (int i = 0; i < len; i++) {
    if (ip[i] == '.') dots++;
    else if (!isdigit(ip[i])) return false;
  }
  return dots == 3;
}

// ---- JSON / HTML escape helper ----
static void json_escape(const char *in, char *out, int out_sz) {
  int o = 0;
  for (int i = 0; in[i] && o < out_sz - 2; i++) {
    char c = in[i];
    if (c == '"' && o + 2 < out_sz)        { out[o++] = '\\'; out[o++] = '"'; }
    else if (c == '\\' && o + 2 < out_sz)  { out[o++] = '\\'; out[o++] = '\\'; }
    else if (c == '\n' && o + 2 < out_sz)  { out[o++] = '\\'; out[o++] = 'n'; }
    else if (c == '\r' && o + 2 < out_sz)  { out[o++] = '\\'; out[o++] = 'r'; }
    else if (c == '\t' && o + 2 < out_sz)  { out[o++] = '\\'; out[o++] = 't'; }
    else if (c == '<' && o + 4 < out_sz)   { out[o++]='\\'; out[o++]='u'; out[o++]='0'; out[o++]='0'; out[o++]='3'; out[o++]='c'; }
    else if (c == '>' && o + 4 < out_sz)   { out[o++]='\\'; out[o++]='u'; out[o++]='0'; out[o++]='0'; out[o++]='3'; out[o++]='e'; }
    else if (c == '&' && o + 4 < out_sz)   { out[o++]='\\'; out[o++]='u'; out[o++]='0'; out[o++]='0'; out[o++]='2'; out[o++]='6'; }
    else out[o++] = c;
  }
  out[o] = '\0';
}

// ---- token bucket (rate limiter) ----
struct Bucket {
  int32_t tokens;
  int32_t capacity;
  int32_t refill;
};

static void bucket_init(Bucket *b, int32_t cap, int32_t refill) {
  if (!b) return;
  b->capacity = cap;
  b->tokens   = cap;
  b->refill   = refill;
}

static bool bucket_consume(Bucket *b, int32_t n) {
  if (!b || n <= 0) return false;
  if (b->tokens < n) return false;
  b->tokens -= n;
  return true;
}

static void bucket_refill(Bucket *b) {
  if (!b) return;
  b->tokens += b->refill;
  if (b->tokens > b->capacity) b->tokens = b->capacity;
}

static bool bucket_empty(Bucket *b) {
  if (!b) return true;
  return b->tokens == 0;
}

// ---- password validator ----
#define PASS_MIN 8
#define PASS_MAX 64

static bool validate_password(const char *pw, char *err_out) {
  if (!pw) { if (err_out) strcpy(err_out, "null"); return false; }
  int len = (int)strlen(pw);
  if (len < PASS_MIN) { if (err_out) snprintf(err_out, 64, "too short (%d)", len); return false; }
  if (len > PASS_MAX) { if (err_out) snprintf(err_out, 64, "too long (%d)", len); return false; }
  bool has_upper = false, has_digit = false;
  for (int i = 0; i < len; i++) {
    if (isupper((unsigned char)pw[i])) has_upper = true;
    if (isdigit((unsigned char)pw[i])) has_digit = true;
  }
  if (!has_upper) { if (err_out) strcpy(err_out, "needs uppercase"); return false; }
  if (!has_digit) { if (err_out) strcpy(err_out, "needs digit"); return false; }
  return true;
}

// ============================================================
// TESTS: ct_strcmp branches
// ============================================================

void test_ct_strcmp_both_null() {
  TEST_ASSERT_FALSE(ct_strcmp(nullptr, nullptr));
}

void test_ct_strcmp_a_null_b_valid() {
  TEST_ASSERT_FALSE(ct_strcmp(nullptr, "token"));
}

void test_ct_strcmp_a_valid_b_null() {
  TEST_ASSERT_FALSE(ct_strcmp("token", nullptr));
}

void test_ct_strcmp_a_longer() {
  TEST_ASSERT_FALSE(ct_strcmp("abcde", "abc"));
}

void test_ct_strcmp_b_longer() {
  TEST_ASSERT_FALSE(ct_strcmp("abc", "abcde"));
}

void test_ct_strcmp_same_length_differ_last() {
  TEST_ASSERT_FALSE(ct_strcmp("abcx", "abcy"));
}

void test_ct_strcmp_same_length_differ_first() {
  TEST_ASSERT_FALSE(ct_strcmp("xabc", "yabc"));
}

void test_ct_strcmp_single_char_match() {
  TEST_ASSERT_TRUE(ct_strcmp("a", "a"));
}

void test_ct_strcmp_single_char_mismatch() {
  TEST_ASSERT_FALSE(ct_strcmp("a", "b"));
}

// ============================================================
// TESTS: check_ssid branches
// ============================================================

void test_ssid_null() {
  TEST_ASSERT_FALSE(check_ssid(nullptr, 32));
}

void test_ssid_empty() {
  TEST_ASSERT_FALSE(check_ssid("", 32));
}

void test_ssid_exactly_max_len() {
  char s[33];
  memset(s, 'A', 32);
  s[32] = '\0';
  TEST_ASSERT_TRUE(check_ssid(s, 32));
}

void test_ssid_one_over_max_len() {
  char s[34];
  memset(s, 'A', 33);
  s[33] = '\0';
  TEST_ASSERT_FALSE(check_ssid(s, 32));
}

void test_ssid_printable_low_boundary() {
  char s[2] = {0x20, 0};  // space — lowest printable ASCII
  TEST_ASSERT_TRUE(check_ssid(s, 32));
}

void test_ssid_printable_high_boundary() {
  char s[2] = {0x7E, 0};  // tilde — highest printable ASCII
  TEST_ASSERT_TRUE(check_ssid(s, 32));
}

void test_ssid_non_printable_low() {
  char s[2] = {0x1F, 0};  // just below printable
  TEST_ASSERT_FALSE(check_ssid(s, 32));
}

void test_ssid_non_printable_high() {
  char s[2] = {(char)0x7F, 0};  // DEL — just above printable
  TEST_ASSERT_FALSE(check_ssid(s, 32));
}

void test_ssid_non_printable_in_middle() {
  char s[5] = {'A', 'B', 0x01, 'D', 0};
  TEST_ASSERT_FALSE(check_ssid(s, 32));
}

void test_ssid_single_valid_char() {
  TEST_ASSERT_TRUE(check_ssid("X", 32));
}

// ============================================================
// TESTS: check_mac_str branches
// ============================================================

void test_mac_null() {
  TEST_ASSERT_FALSE(check_mac_str(nullptr));
}

void test_mac_too_short() {
  TEST_ASSERT_FALSE(check_mac_str("ABCDE"));
}

void test_mac_len_11() {
  TEST_ASSERT_FALSE(check_mac_str("ABCDEF01234"));
}

void test_mac_len_13() {
  TEST_ASSERT_FALSE(check_mac_str("ABCDEF0123456"));
}

void test_mac_len_18() {
  TEST_ASSERT_FALSE(check_mac_str("AA:BB:CC:DD:EE:FF:"));
}

void test_mac_12hex_uppercase() {
  TEST_ASSERT_TRUE(check_mac_str("AABBCCDDEEFF"));
}

void test_mac_12hex_lowercase() {
  TEST_ASSERT_TRUE(check_mac_str("aabbccddeeff"));
}

void test_mac_12hex_mixed() {
  TEST_ASSERT_TRUE(check_mac_str("AaBbCcDdEeFf"));
}

void test_mac_12hex_nonhex_char() {
  TEST_ASSERT_FALSE(check_mac_str("AABBCCDDEEGG"));
}

void test_mac_17_valid() {
  TEST_ASSERT_TRUE(check_mac_str("AA:BB:CC:DD:EE:FF"));
}

void test_mac_17_wrong_separator() {
  TEST_ASSERT_FALSE(check_mac_str("AA-BB-CC-DD-EE-FF"));
}

void test_mac_17_nonhex_digit() {
  TEST_ASSERT_FALSE(check_mac_str("GG:BB:CC:DD:EE:FF"));
}

void test_mac_17_colon_in_wrong_pos() {
  TEST_ASSERT_FALSE(check_mac_str("A:ABBCCDDEEFF6"));
}

// ============================================================
// TESTS: check_channel branches
// ============================================================

void test_channel_min() {
  TEST_ASSERT_TRUE(check_channel(1));
}

void test_channel_max() {
  TEST_ASSERT_TRUE(check_channel(14));
}

void test_channel_below_min() {
  TEST_ASSERT_FALSE(check_channel(0));
}

void test_channel_above_max() {
  TEST_ASSERT_FALSE(check_channel(15));
}

void test_channel_negative() {
  TEST_ASSERT_FALSE(check_channel(-1));
}

void test_channel_mid() {
  TEST_ASSERT_TRUE(check_channel(6));
}

// ============================================================
// TESTS: check_int_range branches
// ============================================================

void test_int_range_null() {
  TEST_ASSERT_FALSE(check_int_range(nullptr, 0, 100, nullptr));
}

void test_int_range_non_numeric() {
  TEST_ASSERT_FALSE(check_int_range("abc", 0, 100, nullptr));
}

void test_int_range_trailing_chars() {
  TEST_ASSERT_FALSE(check_int_range("42abc", 0, 100, nullptr));
}

void test_int_range_at_min() {
  long v = -1;
  TEST_ASSERT_TRUE(check_int_range("0", 0, 100, &v));
  TEST_ASSERT_EQUAL(0, (int)v);
}

void test_int_range_at_max() {
  long v = -1;
  TEST_ASSERT_TRUE(check_int_range("100", 0, 100, &v));
  TEST_ASSERT_EQUAL(100, (int)v);
}

void test_int_range_below_min() {
  TEST_ASSERT_FALSE(check_int_range("-1", 0, 100, nullptr));
}

void test_int_range_above_max() {
  TEST_ASSERT_FALSE(check_int_range("101", 0, 100, nullptr));
}

void test_int_range_null_out_param() {
  TEST_ASSERT_TRUE(check_int_range("50", 0, 100, nullptr));
}

void test_int_range_negative_range() {
  long v;
  TEST_ASSERT_TRUE(check_int_range("-5", -10, -1, &v));
  TEST_ASSERT_EQUAL(-5, (int)v);
}

// ============================================================
// TESTS: check_ip branches
// ============================================================

void test_ip_null() {
  TEST_ASSERT_FALSE(check_ip(nullptr));
}

void test_ip_too_short() {
  TEST_ASSERT_FALSE(check_ip("1.1.1"));
}

void test_ip_too_long() {
  TEST_ASSERT_FALSE(check_ip("255.255.255.255.0"));
}

void test_ip_non_digit() {
  TEST_ASSERT_FALSE(check_ip("192.168.a.1"));
}

void test_ip_too_few_dots() {
  TEST_ASSERT_FALSE(check_ip("192168001"));
}

void test_ip_too_many_dots() {
  TEST_ASSERT_FALSE(check_ip("1.2.3.4.5"));
}

void test_ip_valid() {
  TEST_ASSERT_TRUE(check_ip("192.168.4.1"));
}

void test_ip_min_length() {
  TEST_ASSERT_TRUE(check_ip("1.2.3.4"));
}

// ============================================================
// TESTS: json_escape branches
// ============================================================

void test_json_escape_no_special() {
  char out[64];
  json_escape("hello", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("hello", out);
}

void test_json_escape_double_quote() {
  char out[64];
  json_escape("say \"hi\"", out, sizeof(out));
  TEST_ASSERT_NOT_NULL(strstr(out, "\\\""));
}

void test_json_escape_backslash() {
  char out[64];
  json_escape("a\\b", out, sizeof(out));
  TEST_ASSERT_NOT_NULL(strstr(out, "\\\\"));
}

void test_json_escape_newline() {
  char out[64];
  json_escape("line1\nline2", out, sizeof(out));
  TEST_ASSERT_NOT_NULL(strstr(out, "\\n"));
}

void test_json_escape_carriage_return() {
  char out[64];
  json_escape("a\rb", out, sizeof(out));
  TEST_ASSERT_NOT_NULL(strstr(out, "\\r"));
}

void test_json_escape_tab() {
  char out[64];
  json_escape("a\tb", out, sizeof(out));
  TEST_ASSERT_NOT_NULL(strstr(out, "\\t"));
}

void test_json_escape_less_than() {
  char out[64];
  json_escape("<script>", out, sizeof(out));
  TEST_ASSERT_NOT_NULL(strstr(out, "\\u003c"));
}

void test_json_escape_greater_than() {
  char out[64];
  json_escape("a>b", out, sizeof(out));
  TEST_ASSERT_NOT_NULL(strstr(out, "\\u003e"));
}

void test_json_escape_ampersand() {
  char out[64];
  json_escape("a&b", out, sizeof(out));
  TEST_ASSERT_NOT_NULL(strstr(out, "\\u0026"));
}

void test_json_escape_empty_string() {
  char out[64];
  json_escape("", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("", out);
}

// ============================================================
// TESTS: validate_password branches
// ============================================================

void test_password_exactly_max_len() {
  char pw[PASS_MAX + 2];
  memset(pw, 'a', PASS_MAX);
  pw[0] = 'A';  // uppercase
  pw[1] = '1';  // digit
  pw[PASS_MAX] = '\0';
  TEST_ASSERT_TRUE(validate_password(pw, nullptr));
}

void test_password_one_over_max_len() {
  char pw[PASS_MAX + 3];
  memset(pw, 'a', PASS_MAX + 1);
  pw[0] = 'A';
  pw[1] = '1';
  pw[PASS_MAX + 1] = '\0';
  TEST_ASSERT_FALSE(validate_password(pw, nullptr));
}

void test_password_error_message_null() {
  char err[64] = {0};
  validate_password(nullptr, err);
  TEST_ASSERT_EQUAL_STRING("null", err);
}

void test_password_error_message_too_short() {
  char err[64] = {0};
  validate_password("Ab1", err);
  TEST_ASSERT_NOT_NULL(strstr(err, "too short"));
}

void test_password_error_message_too_long() {
  char pw[PASS_MAX + 3];
  memset(pw, 'a', PASS_MAX + 1);
  pw[0] = 'A';
  pw[1] = '1';
  pw[PASS_MAX + 1] = '\0';
  char err[64] = {0};
  validate_password(pw, err);
  TEST_ASSERT_NOT_NULL(strstr(err, "too long"));
}

void test_password_error_message_needs_uppercase() {
  char err[64] = {0};
  validate_password("phantom32", err);
  TEST_ASSERT_EQUAL_STRING("needs uppercase", err);
}

void test_password_error_message_needs_digit() {
  char err[64] = {0};
  validate_password("PhantomPass", err);
  TEST_ASSERT_EQUAL_STRING("needs digit", err);
}

// ============================================================
// TESTS: token bucket branches
// ============================================================

void test_bucket_null_init() {
  bucket_init(nullptr, 10, 1);  // should not crash
  TEST_ASSERT_TRUE(true);       // survived
}

void test_bucket_null_consume() {
  TEST_ASSERT_FALSE(bucket_consume(nullptr, 1));
}

void test_bucket_consume_zero() {
  Bucket b;
  bucket_init(&b, 10, 1);
  TEST_ASSERT_FALSE(bucket_consume(&b, 0));  // n <= 0 branch
}

void test_bucket_consume_negative() {
  Bucket b;
  bucket_init(&b, 10, 1);
  TEST_ASSERT_FALSE(bucket_consume(&b, -1));
}

void test_bucket_null_refill() {
  bucket_refill(nullptr);  // should not crash
  TEST_ASSERT_TRUE(true);
}

void test_bucket_refill_caps_at_capacity() {
  Bucket b;
  bucket_init(&b, 10, 5);
  b.tokens = 8;
  bucket_refill(&b);
  TEST_ASSERT_EQUAL(10, b.tokens);  // capped at capacity
}

void test_bucket_refill_does_not_exceed_cap() {
  Bucket b;
  bucket_init(&b, 10, 100);
  b.tokens = 10;
  bucket_refill(&b);
  TEST_ASSERT_EQUAL(10, b.tokens);
}

void test_bucket_consume_exactly_all() {
  Bucket b;
  bucket_init(&b, 5, 1);
  TEST_ASSERT_TRUE(bucket_consume(&b, 5));
  TEST_ASSERT_EQUAL(0, b.tokens);
}

void test_bucket_consume_more_than_available() {
  Bucket b;
  bucket_init(&b, 5, 1);
  TEST_ASSERT_FALSE(bucket_consume(&b, 6));
  TEST_ASSERT_EQUAL(5, b.tokens);  // unchanged
}

void test_bucket_null_empty() {
  TEST_ASSERT_TRUE(bucket_empty(nullptr));
}

void test_bucket_not_empty_when_has_tokens() {
  Bucket b;
  bucket_init(&b, 5, 1);
  TEST_ASSERT_FALSE(bucket_empty(&b));
}

void test_bucket_empty_after_drain() {
  Bucket b;
  bucket_init(&b, 1, 1);
  bucket_consume(&b, 1);
  TEST_ASSERT_TRUE(bucket_empty(&b));
}

#endif  // NATIVE_ENV

// ============================================================
// ENTRY POINT
// ============================================================

void setUp() {}
void tearDown() {}

int main(int argc, char **argv) {
#ifndef NATIVE_ENV
  return 0;
#else
  UNITY_BEGIN();

  // ct_strcmp branches
  RUN_TEST(test_ct_strcmp_both_null);
  RUN_TEST(test_ct_strcmp_a_null_b_valid);
  RUN_TEST(test_ct_strcmp_a_valid_b_null);
  RUN_TEST(test_ct_strcmp_a_longer);
  RUN_TEST(test_ct_strcmp_b_longer);
  RUN_TEST(test_ct_strcmp_same_length_differ_last);
  RUN_TEST(test_ct_strcmp_same_length_differ_first);
  RUN_TEST(test_ct_strcmp_single_char_match);
  RUN_TEST(test_ct_strcmp_single_char_mismatch);

  // check_ssid branches
  RUN_TEST(test_ssid_null);
  RUN_TEST(test_ssid_empty);
  RUN_TEST(test_ssid_exactly_max_len);
  RUN_TEST(test_ssid_one_over_max_len);
  RUN_TEST(test_ssid_printable_low_boundary);
  RUN_TEST(test_ssid_printable_high_boundary);
  RUN_TEST(test_ssid_non_printable_low);
  RUN_TEST(test_ssid_non_printable_high);
  RUN_TEST(test_ssid_non_printable_in_middle);
  RUN_TEST(test_ssid_single_valid_char);

  // check_mac_str branches
  RUN_TEST(test_mac_null);
  RUN_TEST(test_mac_too_short);
  RUN_TEST(test_mac_len_11);
  RUN_TEST(test_mac_len_13);
  RUN_TEST(test_mac_len_18);
  RUN_TEST(test_mac_12hex_uppercase);
  RUN_TEST(test_mac_12hex_lowercase);
  RUN_TEST(test_mac_12hex_mixed);
  RUN_TEST(test_mac_12hex_nonhex_char);
  RUN_TEST(test_mac_17_valid);
  RUN_TEST(test_mac_17_wrong_separator);
  RUN_TEST(test_mac_17_nonhex_digit);
  RUN_TEST(test_mac_17_colon_in_wrong_pos);

  // check_channel branches
  RUN_TEST(test_channel_min);
  RUN_TEST(test_channel_max);
  RUN_TEST(test_channel_below_min);
  RUN_TEST(test_channel_above_max);
  RUN_TEST(test_channel_negative);
  RUN_TEST(test_channel_mid);

  // check_int_range branches
  RUN_TEST(test_int_range_null);
  RUN_TEST(test_int_range_non_numeric);
  RUN_TEST(test_int_range_trailing_chars);
  RUN_TEST(test_int_range_at_min);
  RUN_TEST(test_int_range_at_max);
  RUN_TEST(test_int_range_below_min);
  RUN_TEST(test_int_range_above_max);
  RUN_TEST(test_int_range_null_out_param);
  RUN_TEST(test_int_range_negative_range);

  // check_ip branches
  RUN_TEST(test_ip_null);
  RUN_TEST(test_ip_too_short);
  RUN_TEST(test_ip_too_long);
  RUN_TEST(test_ip_non_digit);
  RUN_TEST(test_ip_too_few_dots);
  RUN_TEST(test_ip_too_many_dots);
  RUN_TEST(test_ip_valid);
  RUN_TEST(test_ip_min_length);

  // json_escape branches
  RUN_TEST(test_json_escape_no_special);
  RUN_TEST(test_json_escape_double_quote);
  RUN_TEST(test_json_escape_backslash);
  RUN_TEST(test_json_escape_newline);
  RUN_TEST(test_json_escape_carriage_return);
  RUN_TEST(test_json_escape_tab);
  RUN_TEST(test_json_escape_less_than);
  RUN_TEST(test_json_escape_greater_than);
  RUN_TEST(test_json_escape_ampersand);
  RUN_TEST(test_json_escape_empty_string);

  // validate_password branches
  RUN_TEST(test_password_exactly_max_len);
  RUN_TEST(test_password_one_over_max_len);
  RUN_TEST(test_password_error_message_null);
  RUN_TEST(test_password_error_message_too_short);
  RUN_TEST(test_password_error_message_too_long);
  RUN_TEST(test_password_error_message_needs_uppercase);
  RUN_TEST(test_password_error_message_needs_digit);

  // token bucket branches
  RUN_TEST(test_bucket_null_init);
  RUN_TEST(test_bucket_null_consume);
  RUN_TEST(test_bucket_consume_zero);
  RUN_TEST(test_bucket_consume_negative);
  RUN_TEST(test_bucket_null_refill);
  RUN_TEST(test_bucket_refill_caps_at_capacity);
  RUN_TEST(test_bucket_refill_does_not_exceed_cap);
  RUN_TEST(test_bucket_consume_exactly_all);
  RUN_TEST(test_bucket_consume_more_than_available);
  RUN_TEST(test_bucket_null_empty);
  RUN_TEST(test_bucket_not_empty_when_has_tokens);
  RUN_TEST(test_bucket_empty_after_drain);

  return UNITY_END();
#endif
}

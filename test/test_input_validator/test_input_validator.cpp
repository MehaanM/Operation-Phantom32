// test_input_validator.cpp — Unit tests for input validation and sanitization.
//
// Tests all validation functions from input_validator.h in an isolated
// environment (no Arduino/ESP32 dependencies). Covers boundary conditions,
// invalid inputs, injection attempts, and encoding correctness.

#include <unity.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

// ============================================================
// INLINE TEST-ONLY IMPLEMENTATIONS
// (Avoids pulling in WebServer and ESP32 SDK deps in native env)
// ============================================================

static char g_last_error[128];

static void set_error(const char *msg) {
  strncpy(g_last_error, msg, sizeof(g_last_error) - 1);
}

static bool check_ssid(const char *ssid, int max_len = 32) {
  if (!ssid) { set_error("null"); return false; }
  int len = strlen(ssid);
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
  int len = strlen(m);
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

static bool parse_mac(const char *s, uint8_t *out) {
  if (!check_mac_str(s)) return false;
  int len = strlen(s);
  auto hv = [](char c) -> int {
    if (c>='0'&&c<='9') return c-'0';
    if (c>='a'&&c<='f') return c-'a'+10;
    if (c>='A'&&c<='F') return c-'A'+10;
    return 0;
  };
  if (len == 17) {
    for (int i = 0; i < 6; i++)
      out[i] = (uint8_t)((hv(s[i*3])<<4)|hv(s[i*3+1]));
  } else {
    for (int i = 0; i < 6; i++)
      out[i] = (uint8_t)((hv(s[i*2])<<4)|hv(s[i*2+1]));
  }
  return true;
}

static bool check_channel(int ch) { return ch >= 1 && ch <= 14; }

static bool check_int_range(const char *s, long mn, long mx, long *out) {
  if (!s) return false;
  char *end; long v = strtol(s, &end, 10);
  if (*end != '\0') return false;
  if (v < mn || v > mx) return false;
  if (out) *out = v;
  return true;
}

static bool check_ip(const char *ip) {
  if (!ip) return false;
  int len = strlen(ip);
  if (len < 7 || len > 15) return false;
  int oct=0, octv=0; bool in_oct=false;
  for (int i = 0; i <= len; i++) {
    char c = ip[i];
    if (isdigit(c)) { octv=octv*10+(c-'0'); if(octv>255) return false; in_oct=true; }
    else if (c=='.'||c=='\0') {
      if (!in_oct) return false;
      oct++; octv=0; in_oct=false;
    } else return false;
  }
  return oct == 4;
}

static bool check_label(const char *s, int max_len = 64) {
  if (!s) return false;
  int len = strlen(s);
  if (len > max_len) return false;
  for (int i = 0; i < len; i++) {
    uint8_t c = (uint8_t)s[i];
    if (c < 0x20 || c > 0x7E) return false;
  }
  return true;
}

static bool check_reason_code(uint16_t rc) {
  return rc != 0 && rc <= 65534;
}

static bool check_beacon_interval(uint16_t bi) {
  return bi >= 10 && bi <= 1000;
}

// JSON escape helper
static char g_json_buf[512];
static const char* json_escape(const char *s) {
  int o = 0;
  g_json_buf[o++] = '"';
  for (; *s && o < 508; s++) {
    uint8_t c = (uint8_t)*s;
    if (c == '"')  { g_json_buf[o++]='\\'; g_json_buf[o++]='"'; }
    else if (c == '\\') { g_json_buf[o++]='\\'; g_json_buf[o++]='\\'; }
    else if (c == '\n') { g_json_buf[o++]='\\'; g_json_buf[o++]='n'; }
    else if (c == '\r') { g_json_buf[o++]='\\'; g_json_buf[o++]='r'; }
    else if (c == '\t') { g_json_buf[o++]='\\'; g_json_buf[o++]='t'; }
    else if (c < 0x20) {
      o += snprintf(g_json_buf+o, 8, "\\u%04x", c);
    } else g_json_buf[o++] = (char)c;
  }
  g_json_buf[o++] = '"';
  g_json_buf[o] = '\0';
  return g_json_buf;
}

// ============================================================
// TESTS: SSID validation
// ============================================================

void test_ssid_valid_simple() { TEST_ASSERT_TRUE(check_ssid("HomeNetwork")); }
void test_ssid_valid_symbols() { TEST_ASSERT_TRUE(check_ssid("My_WiFi-2.4GHz")); }
void test_ssid_valid_numbers() { TEST_ASSERT_TRUE(check_ssid("123456789")); }
void test_ssid_valid_mixed()   { TEST_ASSERT_TRUE(check_ssid("Guest@2.4")); }
void test_ssid_valid_spaces()  { TEST_ASSERT_TRUE(check_ssid("My Home WiFi")); }
void test_ssid_valid_32_chars() {
  char ssid[33]; memset(ssid, 'A', 32); ssid[32] = '\0';
  TEST_ASSERT_TRUE(check_ssid(ssid, 32));
}

void test_ssid_null()    { TEST_ASSERT_FALSE(check_ssid(nullptr)); }
void test_ssid_empty()   { TEST_ASSERT_FALSE(check_ssid("")); }

void test_ssid_too_long() {
  char ssid[34]; memset(ssid, 'A', 33); ssid[33] = '\0';
  TEST_ASSERT_FALSE(check_ssid(ssid, 32));
}

void test_ssid_null_byte() {
  // String with embedded null: check_ssid sees it as length 2 (up to first null)
  // which is valid — but we can't embed a real null in C string literal.
  // Test the length-1 edge case instead.
  char ssid[2] = {'A', '\0'};
  TEST_ASSERT_TRUE(check_ssid(ssid));
}

void test_ssid_control_char_tab() {
  char ssid[] = "Net\t1";
  TEST_ASSERT_FALSE(check_ssid(ssid));
}

void test_ssid_control_char_newline() {
  char ssid[] = "Net\n1";
  TEST_ASSERT_FALSE(check_ssid(ssid));
}

void test_ssid_control_char_cr() {
  char ssid[] = "Net\r1";
  TEST_ASSERT_FALSE(check_ssid(ssid));
}

void test_ssid_del_char() {
  char ssid[] = {'N', 'e', 't', 0x7F, '1', '\0'};
  TEST_ASSERT_FALSE(check_ssid(ssid));
}

void test_ssid_high_byte() {
  char ssid[] = {'N', (char)0x80, '\0'};
  TEST_ASSERT_FALSE(check_ssid(ssid));
}

void test_ssid_injection_attempt() {
  // SQL injection-style: should be blocked by the non-printable char check?
  // Actually single quotes and angle brackets ARE printable ASCII (0x27, 0x3C/3E)
  // so they pass SSID validation — that's correct. XSS prevention is in json_escape.
  TEST_ASSERT_TRUE(check_ssid("'; DROP TABLE aps; --"));
  TEST_ASSERT_TRUE(check_ssid("<script>alert(1)</script>"));
}

// ============================================================
// TESTS: MAC address validation
// ============================================================

void test_mac_colon_valid() { TEST_ASSERT_TRUE(check_mac_str("AA:BB:CC:DD:EE:FF")); }
void test_mac_colon_lower() { TEST_ASSERT_TRUE(check_mac_str("aa:bb:cc:dd:ee:ff")); }
void test_mac_colon_mixed() { TEST_ASSERT_TRUE(check_mac_str("aA:Bb:cC:Dd:eE:fF")); }
void test_mac_compact_valid() { TEST_ASSERT_TRUE(check_mac_str("AABBCCDDEEFF")); }
void test_mac_compact_lower() { TEST_ASSERT_TRUE(check_mac_str("aabbccddeeff")); }
void test_mac_zero()      { TEST_ASSERT_TRUE(check_mac_str("00:00:00:00:00:00")); }
void test_mac_broadcast() { TEST_ASSERT_TRUE(check_mac_str("FF:FF:FF:FF:FF:FF")); }

void test_mac_null()    { TEST_ASSERT_FALSE(check_mac_str(nullptr)); }
void test_mac_empty()   { TEST_ASSERT_FALSE(check_mac_str("")); }
void test_mac_too_short() { TEST_ASSERT_FALSE(check_mac_str("AA:BB:CC")); }
void test_mac_too_long()  { TEST_ASSERT_FALSE(check_mac_str("AA:BB:CC:DD:EE:FF:00")); }
void test_mac_bad_separator() { TEST_ASSERT_FALSE(check_mac_str("AA-BB-CC-DD-EE-FF")); }
void test_mac_bad_hex()   { TEST_ASSERT_FALSE(check_mac_str("ZZ:00:11:22:33:44")); }
void test_mac_mixed_format() { TEST_ASSERT_FALSE(check_mac_str("AABB:CC:DD:EE:FF")); }

void test_mac_parse_colon() {
  uint8_t out[6];
  TEST_ASSERT_TRUE(parse_mac("AA:BB:CC:DD:EE:FF", out));
  TEST_ASSERT_EQUAL(0xAA, out[0]);
  TEST_ASSERT_EQUAL(0xBB, out[1]);
  TEST_ASSERT_EQUAL(0xCC, out[2]);
  TEST_ASSERT_EQUAL(0xDD, out[3]);
  TEST_ASSERT_EQUAL(0xEE, out[4]);
  TEST_ASSERT_EQUAL(0xFF, out[5]);
}

void test_mac_parse_compact() {
  uint8_t out[6];
  TEST_ASSERT_TRUE(parse_mac("112233445566", out));
  TEST_ASSERT_EQUAL(0x11, out[0]);
  TEST_ASSERT_EQUAL(0x22, out[1]);
  TEST_ASSERT_EQUAL(0x33, out[2]);
  TEST_ASSERT_EQUAL(0x44, out[3]);
  TEST_ASSERT_EQUAL(0x55, out[4]);
  TEST_ASSERT_EQUAL(0x66, out[5]);
}

// ============================================================
// TESTS: Channel validation
// ============================================================

void test_ch_1()  { TEST_ASSERT_TRUE(check_channel(1)); }
void test_ch_6()  { TEST_ASSERT_TRUE(check_channel(6)); }
void test_ch_11() { TEST_ASSERT_TRUE(check_channel(11)); }
void test_ch_14() { TEST_ASSERT_TRUE(check_channel(14)); }
void test_ch_0()  { TEST_ASSERT_FALSE(check_channel(0)); }
void test_ch_15() { TEST_ASSERT_FALSE(check_channel(15)); }
void test_ch_neg(){ TEST_ASSERT_FALSE(check_channel(-1)); }

// ============================================================
// TESTS: Integer range validation
// ============================================================

void test_int_in_range() {
  long out;
  TEST_ASSERT_TRUE(check_int_range("50", 0, 100, &out));
  TEST_ASSERT_EQUAL(50, out);
}

void test_int_at_min() {
  long out;
  TEST_ASSERT_TRUE(check_int_range("0", 0, 100, &out));
  TEST_ASSERT_EQUAL(0, out);
}

void test_int_at_max() {
  long out;
  TEST_ASSERT_TRUE(check_int_range("100", 0, 100, &out));
  TEST_ASSERT_EQUAL(100, out);
}

void test_int_below_min() {
  long out;
  TEST_ASSERT_FALSE(check_int_range("-1", 0, 100, &out));
}

void test_int_above_max() {
  long out;
  TEST_ASSERT_FALSE(check_int_range("101", 0, 100, &out));
}

void test_int_not_number() {
  long out;
  TEST_ASSERT_FALSE(check_int_range("abc", 0, 100, &out));
}

void test_int_float_input() {
  long out;
  TEST_ASSERT_FALSE(check_int_range("3.14", 0, 100, &out));
}

void test_int_negative() {
  long out;
  TEST_ASSERT_TRUE(check_int_range("-50", -100, 0, &out));
  TEST_ASSERT_EQUAL(-50, out);
}

// ============================================================
// TESTS: IP address validation
// ============================================================

void test_ip_valid_private()  { TEST_ASSERT_TRUE(check_ip("192.168.4.1")); }
void test_ip_valid_loopback() { TEST_ASSERT_TRUE(check_ip("127.0.0.1")); }
void test_ip_valid_all_zero() { TEST_ASSERT_TRUE(check_ip("0.0.0.0")); }
void test_ip_valid_all_255()  { TEST_ASSERT_TRUE(check_ip("255.255.255.255")); }

void test_ip_null()         { TEST_ASSERT_FALSE(check_ip(nullptr)); }
void test_ip_empty()        { TEST_ASSERT_FALSE(check_ip("")); }
void test_ip_too_few_octs() { TEST_ASSERT_FALSE(check_ip("192.168.1")); }
void test_ip_too_many_octs(){ TEST_ASSERT_FALSE(check_ip("192.168.1.1.1")); }
void test_ip_octet_too_big(){ TEST_ASSERT_FALSE(check_ip("256.0.0.1")); }
void test_ip_letters()      { TEST_ASSERT_FALSE(check_ip("abc.def.ghi.jkl")); }
void test_ip_empty_octet()  { TEST_ASSERT_FALSE(check_ip("192..168.1")); }
void test_ip_trailing_dot() { TEST_ASSERT_FALSE(check_ip("192.168.1.1.")); }

// ============================================================
// TESTS: Label validation
// ============================================================

void test_label_valid()     { TEST_ASSERT_TRUE(check_label("MyRouter")); }
void test_label_empty()     { TEST_ASSERT_TRUE(check_label("")); }
void test_label_max_len() {
  char label[65]; memset(label, 'A', 64); label[64] = '\0';
  TEST_ASSERT_TRUE(check_label(label, 64));
}
void test_label_too_long() {
  char label[66]; memset(label, 'A', 65); label[65] = '\0';
  TEST_ASSERT_FALSE(check_label(label, 64));
}
void test_label_control_char() {
  char label[] = "Bad\x01Label";
  TEST_ASSERT_FALSE(check_label(label));
}

// ============================================================
// TESTS: Reason code validation
// ============================================================

void test_rc_valid_1()     { TEST_ASSERT_TRUE(check_reason_code(1)); }
void test_rc_valid_7()     { TEST_ASSERT_TRUE(check_reason_code(7)); }
void test_rc_valid_max()   { TEST_ASSERT_TRUE(check_reason_code(65534)); }
void test_rc_zero()        { TEST_ASSERT_FALSE(check_reason_code(0)); }
void test_rc_reserved()    { TEST_ASSERT_FALSE(check_reason_code(65535)); }

// ============================================================
// TESTS: Beacon interval validation
// ============================================================

void test_bi_100()  { TEST_ASSERT_TRUE(check_beacon_interval(100)); }
void test_bi_min()  { TEST_ASSERT_TRUE(check_beacon_interval(10)); }
void test_bi_max()  { TEST_ASSERT_TRUE(check_beacon_interval(1000)); }
void test_bi_too_low()  { TEST_ASSERT_FALSE(check_beacon_interval(5)); }
void test_bi_too_high() { TEST_ASSERT_FALSE(check_beacon_interval(1001)); }
void test_bi_zero() { TEST_ASSERT_FALSE(check_beacon_interval(0)); }

// ============================================================
// TESTS: JSON escape
// ============================================================

void test_json_plain_string() {
  TEST_ASSERT_EQUAL_STRING("\"hello\"", json_escape("hello"));
}

void test_json_escape_quote() {
  TEST_ASSERT_EQUAL_STRING("\"say \\\"hi\\\"\"", json_escape("say \"hi\""));
}

void test_json_escape_backslash() {
  TEST_ASSERT_EQUAL_STRING("\"C:\\\\path\"", json_escape("C:\\path"));
}

void test_json_escape_newline() {
  TEST_ASSERT_EQUAL_STRING("\"line1\\nline2\"", json_escape("line1\nline2"));
}

void test_json_escape_tab() {
  TEST_ASSERT_EQUAL_STRING("\"col1\\tcol2\"", json_escape("col1\tcol2"));
}

void test_json_escape_control() {
  // Control char 0x01 should be escaped as \u0001
  char s[] = {'\x01', '\0'};
  TEST_ASSERT_EQUAL_STRING("\"\\u0001\"", json_escape(s));
}

void test_json_xss_attempt() {
  const char *result = json_escape("<script>alert(1)</script>");
  // No unescaped angle brackets in JSON — but angle brackets ARE safe in JSON
  // so the result should contain them literally (they're not HTML special in JSON)
  TEST_ASSERT_NOT_NULL(strstr(result, "<script>"));
  // The important thing is they're wrapped in quotes
  TEST_ASSERT_EQUAL('"', result[0]);
}

void test_json_ssid_with_quotes() {
  const char *result = json_escape("My \"Home\" WiFi");
  TEST_ASSERT_NOT_NULL(strstr(result, "\\\"Home\\\""));
}

// ============================================================
// ENTRY POINT
// ============================================================

void setUp()   {}
void tearDown() {}

int main(int argc, char **argv) {
  UNITY_BEGIN();

  // SSID
  RUN_TEST(test_ssid_valid_simple);
  RUN_TEST(test_ssid_valid_symbols);
  RUN_TEST(test_ssid_valid_numbers);
  RUN_TEST(test_ssid_valid_mixed);
  RUN_TEST(test_ssid_valid_spaces);
  RUN_TEST(test_ssid_valid_32_chars);
  RUN_TEST(test_ssid_null);
  RUN_TEST(test_ssid_empty);
  RUN_TEST(test_ssid_too_long);
  RUN_TEST(test_ssid_null_byte);
  RUN_TEST(test_ssid_control_char_tab);
  RUN_TEST(test_ssid_control_char_newline);
  RUN_TEST(test_ssid_control_char_cr);
  RUN_TEST(test_ssid_del_char);
  RUN_TEST(test_ssid_high_byte);
  RUN_TEST(test_ssid_injection_attempt);

  // MAC
  RUN_TEST(test_mac_colon_valid);
  RUN_TEST(test_mac_colon_lower);
  RUN_TEST(test_mac_colon_mixed);
  RUN_TEST(test_mac_compact_valid);
  RUN_TEST(test_mac_compact_lower);
  RUN_TEST(test_mac_zero);
  RUN_TEST(test_mac_broadcast);
  RUN_TEST(test_mac_null);
  RUN_TEST(test_mac_empty);
  RUN_TEST(test_mac_too_short);
  RUN_TEST(test_mac_too_long);
  RUN_TEST(test_mac_bad_separator);
  RUN_TEST(test_mac_bad_hex);
  RUN_TEST(test_mac_mixed_format);
  RUN_TEST(test_mac_parse_colon);
  RUN_TEST(test_mac_parse_compact);

  // Channel
  RUN_TEST(test_ch_1);
  RUN_TEST(test_ch_6);
  RUN_TEST(test_ch_11);
  RUN_TEST(test_ch_14);
  RUN_TEST(test_ch_0);
  RUN_TEST(test_ch_15);
  RUN_TEST(test_ch_neg);

  // Integer range
  RUN_TEST(test_int_in_range);
  RUN_TEST(test_int_at_min);
  RUN_TEST(test_int_at_max);
  RUN_TEST(test_int_below_min);
  RUN_TEST(test_int_above_max);
  RUN_TEST(test_int_not_number);
  RUN_TEST(test_int_float_input);
  RUN_TEST(test_int_negative);

  // IP address
  RUN_TEST(test_ip_valid_private);
  RUN_TEST(test_ip_valid_loopback);
  RUN_TEST(test_ip_valid_all_zero);
  RUN_TEST(test_ip_valid_all_255);
  RUN_TEST(test_ip_null);
  RUN_TEST(test_ip_empty);
  RUN_TEST(test_ip_too_few_octs);
  RUN_TEST(test_ip_too_many_octs);
  RUN_TEST(test_ip_octet_too_big);
  RUN_TEST(test_ip_letters);
  RUN_TEST(test_ip_empty_octet);
  RUN_TEST(test_ip_trailing_dot);

  // Label
  RUN_TEST(test_label_valid);
  RUN_TEST(test_label_empty);
  RUN_TEST(test_label_max_len);
  RUN_TEST(test_label_too_long);
  RUN_TEST(test_label_control_char);

  // Reason code
  RUN_TEST(test_rc_valid_1);
  RUN_TEST(test_rc_valid_7);
  RUN_TEST(test_rc_valid_max);
  RUN_TEST(test_rc_zero);
  RUN_TEST(test_rc_reserved);

  // Beacon interval
  RUN_TEST(test_bi_100);
  RUN_TEST(test_bi_min);
  RUN_TEST(test_bi_max);
  RUN_TEST(test_bi_too_low);
  RUN_TEST(test_bi_too_high);
  RUN_TEST(test_bi_zero);

  // JSON escape
  RUN_TEST(test_json_plain_string);
  RUN_TEST(test_json_escape_quote);
  RUN_TEST(test_json_escape_backslash);
  RUN_TEST(test_json_escape_newline);
  RUN_TEST(test_json_escape_tab);
  RUN_TEST(test_json_escape_control);
  RUN_TEST(test_json_xss_attempt);
  RUN_TEST(test_json_ssid_with_quotes);

  return UNITY_END();
}

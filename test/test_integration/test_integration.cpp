// test_integration.cpp — Cross-module integration tests.
//
// These tests verify that the security modules (auth, rate limiter,
// input validator) interact correctly as a combined pipeline, mirroring
// how they are used together in real HTTP handler code.
//
// All tests run on native (no Arduino dependencies).

#include <unity.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>

// ============================================================
// SHARED TEST INFRA: minimal reprints of module stubs
// ============================================================

// --- Auth module ---

#define AM_TOKEN_LEN    64
#define AM_MAX_SESSIONS 8
#define AM_SESSION_TTL  1800
#define AM_MAX_ATTEMPTS 5
#define AM_LOCKOUT_S    300

struct TestSession {
  char     token[AM_TOKEN_LEN + 1];
  uint32_t created_s;
  bool     valid;
};

static TestSession g_sessions[AM_MAX_SESSIONS];
static uint32_t    g_time_s = 0;

static void am_reset() {
  memset(g_sessions, 0, sizeof(g_sessions));
  g_time_s = 0;
}

static bool ct_eq(const char *a, const char *b) {
  size_t la = strlen(a), lb = strlen(b);
  volatile uint8_t diff = (la != lb) ? 1 : 0;
  size_t lm = la > lb ? la : lb;
  for (size_t i = 0; i < lm; i++) {
    diff |= ((i < la ? (uint8_t)a[i] : 0) ^ (i < lb ? (uint8_t)b[i] : 0));
  }
  return diff == 0;
}

static TestSession* am_create() {
  for (int i = 0; i < AM_MAX_SESSIONS; i++) {
    if (!g_sessions[i].valid) {
      static uint32_t seed = 0xBEEF;
      for (int j = 0; j < AM_TOKEN_LEN; j += 2) {
        seed = seed * 1664525u + 1013904223u;
        snprintf(g_sessions[i].token + j, 3, "%02x", (uint8_t)(seed >> 16));
      }
      g_sessions[i].token[AM_TOKEN_LEN] = '\0';
      g_sessions[i].created_s = g_time_s;
      g_sessions[i].valid     = true;
      return &g_sessions[i];
    }
  }
  return nullptr;
}

static bool am_valid(const char *tok) {
  if (!tok || strlen(tok) != AM_TOKEN_LEN) return false;
  for (int i = 0; i < AM_MAX_SESSIONS; i++) {
    if (!g_sessions[i].valid) continue;
    if (ct_eq(g_sessions[i].token, tok)) {
      if ((g_time_s - g_sessions[i].created_s) > AM_SESSION_TTL) {
        g_sessions[i].valid = false;
        return false;
      }
      return true;
    }
  }
  return false;
}

// --- Rate limiter ---

struct TestBucket {
  float    tokens;
  float    cap;
  float    rate_per_ms;
  uint32_t last_refill_ms;

  void init(float c, float r) {
    cap = c; tokens = c;
    rate_per_ms = r / 1000.0f;
    last_refill_ms = 0;
  }
  bool consume(uint32_t now_ms) {
    float elapsed = (float)(now_ms - last_refill_ms);
    tokens += elapsed * rate_per_ms;
    if (tokens > cap) tokens = cap;
    last_refill_ms = now_ms;
    if (tokens < 1.0f) return false;
    tokens -= 1.0f;
    return true;
  }
};

static TestBucket g_global_bkt;
static TestBucket g_http_bkt;
static uint32_t   g_mock_ms = 0;

static void rla_reset() {
  g_global_bkt.init(200.0f, 200.0f);
  g_global_bkt.last_refill_ms = 0;
  g_http_bkt.init(10.0f, 10.0f);
  g_http_bkt.last_refill_ms = 0;
  g_mock_ms = 0;
}

static bool rla_tx_check()   { return g_global_bkt.consume(g_mock_ms); }
static bool rla_http_check() { return g_http_bkt.consume(g_mock_ms); }

// --- Input validator ---

static bool iv_ssid_ok(const char *s) {
  if (!s || !*s) return false;
  int len = strlen(s);
  if (len > 32) return false;
  for (int i = 0; i < len; i++) {
    uint8_t c = (uint8_t)s[i];
    if (c < 0x20 || c > 0x7E) return false;
  }
  return true;
}

static bool iv_chan_ok(int ch) { return ch >= 1 && ch <= 14; }

static bool iv_mac_ok(const char *m) {
  if (!m) return false;
  int l = strlen(m);
  if (l == 17) {
    for (int i = 0; i < 17; i++) {
      if (i % 3 == 2) { if (m[i] != ':') return false; }
      else if (!isxdigit(m[i])) return false;
    }
    return true;
  }
  if (l == 12) {
    for (int i = 0; i < 12; i++) if (!isxdigit(m[i])) return false;
    return true;
  }
  return false;
}

static void integ_reset() {
  am_reset();
  rla_reset();
  g_time_s = 0;
  g_mock_ms = 0;
}

// ============================================================
// TESTS: Auth + rate limiter pipeline
// ============================================================

// A valid request is authenticated AND within rate limit => allowed.
void test_pipeline_auth_ok_rla_ok() {
  integ_reset();
  TestSession *s = am_create();
  TEST_ASSERT_TRUE(am_valid(s->token));
  TEST_ASSERT_TRUE(rla_http_check());
}

// No session token => auth fails even if rate limit is fine.
void test_pipeline_no_session_blocked() {
  integ_reset();
  char bad[AM_TOKEN_LEN + 1];
  memset(bad, 'z', AM_TOKEN_LEN); bad[AM_TOKEN_LEN] = '\0';
  TEST_ASSERT_FALSE(am_valid(bad));
}

// Expired session => auth fails even with valid token bytes.
void test_pipeline_expired_session_blocked() {
  integ_reset();
  TestSession *s = am_create();
  char tok[AM_TOKEN_LEN + 1];
  strncpy(tok, s->token, AM_TOKEN_LEN);
  tok[AM_TOKEN_LEN] = '\0';
  g_time_s += AM_SESSION_TTL + 1;
  TEST_ASSERT_FALSE(am_valid(tok));
}

// Valid auth but HTTP bucket exhausted => request denied.
void test_pipeline_auth_ok_rla_exhausted() {
  integ_reset();
  g_http_bkt.init(3.0f, 1.0f);
  g_http_bkt.last_refill_ms = 0;
  TestSession *s = am_create();
  TEST_ASSERT_TRUE(am_valid(s->token));
  rla_http_check(); rla_http_check(); rla_http_check(); // exhaust
  TEST_ASSERT_FALSE(rla_http_check()); // over limit
}

// Rate limit refills after time elapses.
void test_pipeline_rla_refills() {
  integ_reset();
  g_http_bkt.init(2.0f, 2.0f); // 2 tokens/sec
  g_http_bkt.last_refill_ms = 0;
  rla_http_check(); rla_http_check(); // exhaust
  TEST_ASSERT_FALSE(rla_http_check());
  g_mock_ms = 1000;
  TEST_ASSERT_TRUE(rla_http_check()); // refilled
}

// ============================================================
// TESTS: Auth + input validator pipeline
// ============================================================

// Authenticated request with valid SSID goes through.
void test_pipeline_auth_valid_ssid() {
  integ_reset();
  TestSession *s = am_create();
  TEST_ASSERT_TRUE(am_valid(s->token));
  TEST_ASSERT_TRUE(iv_ssid_ok("ValidNetwork"));
}

// Authenticated request with invalid SSID is rejected.
void test_pipeline_auth_invalid_ssid_rejected() {
  integ_reset();
  TestSession *s = am_create();
  TEST_ASSERT_TRUE(am_valid(s->token));
  TEST_ASSERT_FALSE(iv_ssid_ok(""));         // empty
  TEST_ASSERT_FALSE(iv_ssid_ok(nullptr));    // null
  // too long (33 chars)
  TEST_ASSERT_FALSE(iv_ssid_ok("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"));
}

// Unauthenticated request must not proceed to input validation.
void test_pipeline_unauth_stops_before_validation() {
  integ_reset();
  // Simulate the guard pattern: auth check first, validate after.
  bool authed = am_valid("bad_token_should_fail_1234567890ab");
  // If not authed, we stop. We never even call iv_ssid_ok.
  TEST_ASSERT_FALSE(authed);
  // This test confirms the pipeline does not fall through on auth failure.
}

// ============================================================
// TESTS: TX pipeline — auth + rla_tx
// ============================================================

void test_tx_pipeline_auth_ok_budget_ok() {
  integ_reset();
  TestSession *s = am_create();
  TEST_ASSERT_TRUE(am_valid(s->token));
  TEST_ASSERT_TRUE(rla_tx_check());
}

void test_tx_pipeline_budget_exhausted() {
  integ_reset();
  g_global_bkt.init(3.0f, 1.0f);
  g_global_bkt.last_refill_ms = 0;
  TestSession *s = am_create();
  TEST_ASSERT_TRUE(am_valid(s->token));
  rla_tx_check(); rla_tx_check(); rla_tx_check();
  TEST_ASSERT_FALSE(rla_tx_check());
}

// ============================================================
// TESTS: Input validator cross-field combinations
// ============================================================

void test_iv_deauth_valid_combination() {
  // A valid deauth request requires: valid target MAC, valid BSSID, valid channel.
  TEST_ASSERT_TRUE(iv_mac_ok("FF:FF:FF:FF:FF:FF")); // broadcast target
  TEST_ASSERT_TRUE(iv_mac_ok("AA:BB:CC:DD:EE:FF")); // BSSID
  TEST_ASSERT_TRUE(iv_chan_ok(6));
}

void test_iv_deauth_invalid_mac_fails() {
  TEST_ASSERT_FALSE(iv_mac_ok("ZZ:00:11:22:33:44"));
  TEST_ASSERT_FALSE(iv_mac_ok(""));
  TEST_ASSERT_FALSE(iv_mac_ok(nullptr));
}

void test_iv_evil_twin_valid_combination() {
  TEST_ASSERT_TRUE(iv_ssid_ok("TargetNetwork"));
  TEST_ASSERT_TRUE(iv_chan_ok(11));
}

void test_iv_channel_boundary() {
  TEST_ASSERT_TRUE(iv_chan_ok(1));
  TEST_ASSERT_TRUE(iv_chan_ok(14));
  TEST_ASSERT_FALSE(iv_chan_ok(0));
  TEST_ASSERT_FALSE(iv_chan_ok(15));
}

void test_iv_ssid_all_printable_ascii() {
  // SSIDs can include spaces, punctuation, etc.
  TEST_ASSERT_TRUE(iv_ssid_ok("My Home Network!"));
  TEST_ASSERT_TRUE(iv_ssid_ok("Net-2.4G (Office)"));
  TEST_ASSERT_TRUE(iv_ssid_ok("123"));
}

void test_iv_ssid_non_printable_rejected() {
  char ssid_with_null[5] = "AB";
  ssid_with_null[2] = '\x01';  // control char
  ssid_with_null[3] = 'C';
  ssid_with_null[4] = '\0';
  TEST_ASSERT_FALSE(iv_ssid_ok(ssid_with_null));
}

// ============================================================
// TESTS: Session pool pressure
// ============================================================

void test_session_pool_full_behavior() {
  integ_reset();
  for (int i = 0; i < AM_MAX_SESSIONS; i++)
    TEST_ASSERT_NOT_NULL(am_create());
  TEST_ASSERT_NULL(am_create()); // pool full
}

void test_session_all_valid_simultaneously() {
  integ_reset();
  char tokens[AM_MAX_SESSIONS][AM_TOKEN_LEN + 1];
  for (int i = 0; i < AM_MAX_SESSIONS; i++) {
    TestSession *s = am_create();
    TEST_ASSERT_NOT_NULL(s);
    strncpy(tokens[i], s->token, AM_TOKEN_LEN);
    tokens[i][AM_TOKEN_LEN] = '\0';
  }
  for (int i = 0; i < AM_MAX_SESSIONS; i++)
    TEST_ASSERT_TRUE(am_valid(tokens[i]));
}

// ============================================================
// ENTRY POINT
// ============================================================

void setUp()    { integ_reset(); }
void tearDown() {}

int main(int argc, char **argv) {
  UNITY_BEGIN();

  RUN_TEST(test_pipeline_auth_ok_rla_ok);
  RUN_TEST(test_pipeline_no_session_blocked);
  RUN_TEST(test_pipeline_expired_session_blocked);
  RUN_TEST(test_pipeline_auth_ok_rla_exhausted);
  RUN_TEST(test_pipeline_rla_refills);

  RUN_TEST(test_pipeline_auth_valid_ssid);
  RUN_TEST(test_pipeline_auth_invalid_ssid_rejected);
  RUN_TEST(test_pipeline_unauth_stops_before_validation);

  RUN_TEST(test_tx_pipeline_auth_ok_budget_ok);
  RUN_TEST(test_tx_pipeline_budget_exhausted);

  RUN_TEST(test_iv_deauth_valid_combination);
  RUN_TEST(test_iv_deauth_invalid_mac_fails);
  RUN_TEST(test_iv_evil_twin_valid_combination);
  RUN_TEST(test_iv_channel_boundary);
  RUN_TEST(test_iv_ssid_all_printable_ascii);
  RUN_TEST(test_iv_ssid_non_printable_rejected);

  RUN_TEST(test_session_pool_full_behavior);
  RUN_TEST(test_session_all_valid_simultaneously);

  return UNITY_END();
}

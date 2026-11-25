// test_evil_twin.cpp — Unit tests for evil twin credential capture logic.
//
// Tests credential ring buffer management, deduplication, HTML-escaping
// of SSIDs in portal pages, log ring buffer, and attack state machine.
// All tests run on native (no Arduino dependencies).

#include <unity.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

// ============================================================
// MINIMAL STUBS
// ============================================================

#define ET_MAX_CREDS  64
#define ET_MAX_LOG   128
#define ET_SSID_LEN   33
#define ET_PASS_LEN   65
#define ET_IP_LEN     16

typedef enum { ET_IDLE=0, ET_RUNNING=1, ET_STOPPING=2 } EtState;

struct EtCred {
  char     ssid[ET_SSID_LEN];
  char     password[ET_PASS_LEN];
  char     src_ip[ET_IP_LEN];
  uint32_t ts_ms;
  bool     valid;
};

struct EtLogEntry {
  char     msg[64];
  uint32_t ts_ms;
};

static EtCred     g_et_creds[ET_MAX_CREDS];
static int        g_et_cred_head;
static int        g_et_cred_count;
static EtLogEntry g_et_log[ET_MAX_LOG];
static int        g_et_log_head;
static int        g_et_log_count;
static EtState    g_et_state;
static uint32_t   g_mock_ms;

static void et_reset() {
  memset(g_et_creds, 0, sizeof(g_et_creds));
  memset(g_et_log, 0, sizeof(g_et_log));
  g_et_cred_head  = 0;
  g_et_cred_count = 0;
  g_et_log_head   = 0;
  g_et_log_count  = 0;
  g_et_state      = ET_IDLE;
  g_mock_ms       = 0;
}

// Add credential to ring buffer (deduplicated by SSID+pass+IP).
static bool et_add_cred(const char *ssid, const char *pass, const char *ip) {
  if (!ssid || !pass || !ip) return false;
  // Deduplicate.
  for (int i = 0; i < ET_MAX_CREDS; i++) {
    if (!g_et_creds[i].valid) continue;
    if (strcmp(g_et_creds[i].ssid, ssid) == 0 &&
        strcmp(g_et_creds[i].password, pass) == 0 &&
        strcmp(g_et_creds[i].src_ip, ip) == 0) return false; // duplicate
  }
  EtCred *c = &g_et_creds[g_et_cred_head];
  memset(c, 0, sizeof(EtCred));
  strncpy(c->ssid, ssid, ET_SSID_LEN - 1);
  strncpy(c->password, pass, ET_PASS_LEN - 1);
  strncpy(c->src_ip, ip, ET_IP_LEN - 1);
  c->ts_ms = g_mock_ms;
  c->valid = true;
  g_et_cred_head = (g_et_cred_head + 1) % ET_MAX_CREDS;
  if (g_et_cred_count < ET_MAX_CREDS) g_et_cred_count++;
  return true;
}

static int et_cred_count() {
  int c = 0;
  for (int i = 0; i < ET_MAX_CREDS; i++) if (g_et_creds[i].valid) c++;
  return c;
}

static void et_clear_creds() {
  memset(g_et_creds, 0, sizeof(g_et_creds));
  g_et_cred_head = 0; g_et_cred_count = 0;
}

// Add log entry to ring buffer.
static void et_log(const char *msg) {
  EtLogEntry *e = &g_et_log[g_et_log_head];
  strncpy(e->msg, msg, 63);
  e->ts_ms = g_mock_ms;
  g_et_log_head = (g_et_log_head + 1) % ET_MAX_LOG;
  if (g_et_log_count < ET_MAX_LOG) g_et_log_count++;
}

// HTML-escape helper (mirrors iv_ssid_to_html).
static void et_html_escape(const char *in, char *out, size_t out_sz) {
  size_t j = 0;
  for (size_t i = 0; in[i] && j < out_sz - 1; i++) {
    char c = in[i];
    const char *rep = nullptr;
    if      (c == '&') rep = "&amp;";
    else if (c == '<') rep = "&lt;";
    else if (c == '>') rep = "&gt;";
    else if (c == '"') rep = "&quot;";
    else if (c == '\'') rep = "&#39;";
    if (rep) {
      size_t rlen = strlen(rep);
      if (j + rlen >= out_sz) break;
      memcpy(out + j, rep, rlen);
      j += rlen;
    } else {
      out[j++] = c;
    }
  }
  out[j] = '\0';
}

// State machine transitions.
static bool et_start() {
  if (g_et_state != ET_IDLE) return false;
  g_et_state = ET_RUNNING;
  return true;
}
static bool et_stop() {
  if (g_et_state != ET_RUNNING) return false;
  g_et_state = ET_STOPPING;
  return true;
}
static void et_finish_stop() { g_et_state = ET_IDLE; }

// ============================================================
// TESTS: Credential ring buffer
// ============================================================

void test_cred_add_first() {
  et_reset();
  TEST_ASSERT_TRUE(et_add_cred("HomeNet", "pass1234", "192.168.4.5"));
  TEST_ASSERT_EQUAL(1, et_cred_count());
}

void test_cred_add_no_duplicate() {
  et_reset();
  et_add_cred("HomeNet", "pass1234", "192.168.4.5");
  TEST_ASSERT_FALSE(et_add_cred("HomeNet", "pass1234", "192.168.4.5"));
  TEST_ASSERT_EQUAL(1, et_cred_count());
}

void test_cred_different_ssid_allowed() {
  et_reset();
  et_add_cred("Net1", "pass1234", "192.168.4.5");
  TEST_ASSERT_TRUE(et_add_cred("Net2", "pass1234", "192.168.4.5"));
  TEST_ASSERT_EQUAL(2, et_cred_count());
}

void test_cred_different_pass_allowed() {
  et_reset();
  et_add_cred("HomeNet", "pass1111", "192.168.4.5");
  TEST_ASSERT_TRUE(et_add_cred("HomeNet", "pass2222", "192.168.4.5"));
  TEST_ASSERT_EQUAL(2, et_cred_count());
}

void test_cred_different_ip_allowed() {
  et_reset();
  et_add_cred("HomeNet", "pass1234", "192.168.4.5");
  TEST_ASSERT_TRUE(et_add_cred("HomeNet", "pass1234", "192.168.4.6"));
  TEST_ASSERT_EQUAL(2, et_cred_count());
}

void test_cred_null_ssid() {
  et_reset();
  TEST_ASSERT_FALSE(et_add_cred(nullptr, "pass1234", "192.168.4.5"));
}

void test_cred_null_pass() {
  et_reset();
  TEST_ASSERT_FALSE(et_add_cred("HomeNet", nullptr, "192.168.4.5"));
}

void test_cred_null_ip() {
  et_reset();
  TEST_ASSERT_FALSE(et_add_cred("HomeNet", "pass1234", nullptr));
}

void test_cred_clear() {
  et_reset();
  et_add_cred("Net1", "pass1234", "1.2.3.4");
  et_add_cred("Net2", "pass5678", "1.2.3.5");
  et_clear_creds();
  TEST_ASSERT_EQUAL(0, et_cred_count());
}

void test_cred_max_cap_wraps() {
  et_reset();
  for (int i = 0; i < ET_MAX_CREDS; i++) {
    char ssid[32], ip[16];
    snprintf(ssid, 32, "net%d", i);
    snprintf(ip, 16, "10.0.0.%d", i % 254 + 1);
    et_add_cred(ssid, "pass1234", ip);
  }
  TEST_ASSERT_EQUAL(ET_MAX_CREDS, et_cred_count());
  // Adding one more should overwrite oldest (ring buffer).
  et_add_cred("newnet", "newpass", "9.9.9.9");
  TEST_ASSERT_EQUAL(ET_MAX_CREDS, et_cred_count()); // count stays at max
}

void test_cred_stores_ssid() {
  et_reset();
  et_add_cred("TargetSSID", "password123", "10.0.0.1");
  bool found = false;
  for (int i = 0; i < ET_MAX_CREDS; i++) {
    if (g_et_creds[i].valid && strcmp(g_et_creds[i].ssid, "TargetSSID") == 0) {
      found = true; break;
    }
  }
  TEST_ASSERT_TRUE(found);
}

// ============================================================
// TESTS: Log ring buffer
// ============================================================

void test_log_add_first() {
  et_reset();
  et_log("Portal started");
  TEST_ASSERT_EQUAL(1, g_et_log_count);
}

void test_log_add_multiple() {
  et_reset();
  et_log("msg1"); et_log("msg2"); et_log("msg3");
  TEST_ASSERT_EQUAL(3, g_et_log_count);
}

void test_log_max_cap() {
  et_reset();
  for (int i = 0; i < ET_MAX_LOG + 2; i++) et_log("overflow test");
  TEST_ASSERT_EQUAL(ET_MAX_LOG, g_et_log_count);
}

void test_log_stores_message() {
  et_reset();
  et_log("capture from 10.0.0.5");
  TEST_ASSERT_EQUAL_STRING("capture from 10.0.0.5", g_et_log[0].msg);
}

void test_log_stores_timestamp() {
  et_reset();
  g_mock_ms = 50000;
  et_log("test message");
  TEST_ASSERT_EQUAL(50000u, g_et_log[0].ts_ms);
}

// ============================================================
// TESTS: HTML escaping
// ============================================================

void test_html_escape_clean() {
  char out[128];
  et_html_escape("NormalSSID", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("NormalSSID", out);
}

void test_html_escape_ampersand() {
  char out[128];
  et_html_escape("A&B", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("A&amp;B", out);
}

void test_html_escape_less_than() {
  char out[128];
  et_html_escape("A<B", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("A&lt;B", out);
}

void test_html_escape_greater_than() {
  char out[128];
  et_html_escape("A>B", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("A&gt;B", out);
}

void test_html_escape_quote() {
  char out[128];
  et_html_escape("A\"B", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("A&quot;B", out);
}

void test_html_escape_all_special() {
  char out[256];
  et_html_escape("<script>&\"'</script>", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("&lt;script&gt;&amp;&quot;&#39;&lt;/script&gt;", out);
}

// ============================================================
// TESTS: Attack state machine
// ============================================================

void test_state_starts_idle() {
  et_reset();
  TEST_ASSERT_EQUAL(ET_IDLE, g_et_state);
}

void test_state_start_from_idle() {
  et_reset();
  TEST_ASSERT_TRUE(et_start());
  TEST_ASSERT_EQUAL(ET_RUNNING, g_et_state);
}

void test_state_start_when_running_fails() {
  et_reset();
  et_start();
  TEST_ASSERT_FALSE(et_start());
}

void test_state_stop_from_running() {
  et_reset();
  et_start();
  TEST_ASSERT_TRUE(et_stop());
  TEST_ASSERT_EQUAL(ET_STOPPING, g_et_state);
}

void test_state_stop_from_idle_fails() {
  et_reset();
  TEST_ASSERT_FALSE(et_stop());
}

void test_state_finish_returns_idle() {
  et_reset();
  et_start();
  et_stop();
  et_finish_stop();
  TEST_ASSERT_EQUAL(ET_IDLE, g_et_state);
}

void test_state_restart_after_stop() {
  et_reset();
  et_start(); et_stop(); et_finish_stop();
  TEST_ASSERT_TRUE(et_start());
  TEST_ASSERT_EQUAL(ET_RUNNING, g_et_state);
}

// ============================================================
// TESTS: Uncovered branches (placed after stub definitions)
// ============================================================

void test_html_escape_buffer_too_small() {
  char out[6];
  et_html_escape("A&B", out, 6); // &amp; = 5 chars, won't fit in remaining 5 → break
  out[5] = '\0';
  TEST_ASSERT_TRUE(out[0] == 'A');
}

void test_state_stop_from_stopping_fails() {
  et_reset();
  et_start();
  et_stop(); // now ET_STOPPING
  TEST_ASSERT_FALSE(et_stop()); // != ET_RUNNING
}

void test_state_start_from_stopping_fails() {
  et_reset();
  et_start();
  et_stop(); // now ET_STOPPING
  TEST_ASSERT_FALSE(et_start()); // != ET_IDLE
}

void test_cred_add_all_null() {
  et_reset();
  TEST_ASSERT_FALSE(et_add_cred(nullptr, nullptr, nullptr));
}

// ============================================================
// ENTRY POINT
// ============================================================

void setUp()    { et_reset(); }
void tearDown() {}

int main(int argc, char **argv) {
  UNITY_BEGIN();

  RUN_TEST(test_cred_add_first);
  RUN_TEST(test_cred_add_no_duplicate);
  RUN_TEST(test_cred_different_ssid_allowed);
  RUN_TEST(test_cred_different_pass_allowed);
  RUN_TEST(test_cred_different_ip_allowed);
  RUN_TEST(test_cred_null_ssid);
  RUN_TEST(test_cred_null_pass);
  RUN_TEST(test_cred_null_ip);
  RUN_TEST(test_cred_clear);
  RUN_TEST(test_cred_max_cap_wraps);
  RUN_TEST(test_cred_stores_ssid);

  RUN_TEST(test_log_add_first);
  RUN_TEST(test_log_add_multiple);
  RUN_TEST(test_log_max_cap);
  RUN_TEST(test_log_stores_message);
  RUN_TEST(test_log_stores_timestamp);

  RUN_TEST(test_html_escape_clean);
  RUN_TEST(test_html_escape_ampersand);
  RUN_TEST(test_html_escape_less_than);
  RUN_TEST(test_html_escape_greater_than);
  RUN_TEST(test_html_escape_quote);
  RUN_TEST(test_html_escape_all_special);

  RUN_TEST(test_state_starts_idle);
  RUN_TEST(test_state_start_from_idle);
  RUN_TEST(test_state_start_when_running_fails);
  RUN_TEST(test_state_stop_from_running);
  RUN_TEST(test_state_stop_from_idle_fails);
  RUN_TEST(test_state_finish_returns_idle);
  RUN_TEST(test_state_restart_after_stop);

  // Uncovered branch tests
  RUN_TEST(test_html_escape_buffer_too_small);
  RUN_TEST(test_state_stop_from_stopping_fails);
  RUN_TEST(test_state_start_from_stopping_fails);
  RUN_TEST(test_cred_add_all_null);

  return UNITY_END();
}

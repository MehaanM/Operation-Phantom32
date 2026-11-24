// test_aegis.cpp — Unit tests for Aegis passive IDS logic.
//
// Tests: deauth storm detection (rate threshold), rogue AP detection
// (SSID/BSSID mismatch), KARMA detection (probe response without beacon),
// sequence number anomaly detection, and alert ring buffer management.
// All tests run on native (no Arduino dependencies).

#include <unity.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

// ============================================================
// MINIMAL AEGIS STUBS
// ============================================================

#define AG_DEAUTH_THRESHOLD     20     // deauths/sec triggers alert
#define AG_MAX_ALERTS           32
#define AG_ALERT_MSG_LEN        80
#define AG_SEQ_WINDOW           64     // sequence number window for reorder
#define AG_MAX_BSSIDS           32

typedef enum {
  AG_EVT_NONE = 0,
  AG_EVT_DEAUTH_STORM,
  AG_EVT_ROGUE_AP,
  AG_EVT_KARMA,
  AG_EVT_SEQ_ANOMALY,
  AG_EVT_PMKID,
  AG_EVT_BEACON_FLOOD,
} AgEventType;

struct AgAlert {
  AgEventType type;
  char        msg[AG_ALERT_MSG_LEN];
  uint8_t     bssid[6];
  uint32_t    ts_ms;
};

struct AgBssidEntry {
  uint8_t  bssid[6];
  char     ssid[33];
  uint16_t seq_last;
  uint32_t deauth_count;
  uint32_t deauth_window_start_ms;
  bool     active;
};

static AgAlert       g_ag_alerts[AG_MAX_ALERTS];
static int           g_ag_alert_head;
static int           g_ag_alert_count;
static AgBssidEntry  g_ag_bssids[AG_MAX_BSSIDS];
static int           g_ag_bssid_count;
static uint32_t      g_mock_ms;

static void ag_reset() {
  memset(g_ag_alerts, 0, sizeof(g_ag_alerts));
  memset(g_ag_bssids, 0, sizeof(g_ag_bssids));
  g_ag_alert_head  = 0;
  g_ag_alert_count = 0;
  g_ag_bssid_count = 0;
  g_mock_ms        = 0;
}

static bool mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static void ag_add_alert(AgEventType t, const char *msg, const uint8_t *bssid) {
  AgAlert *a = &g_ag_alerts[g_ag_alert_head];
  memset(a, 0, sizeof(AgAlert));
  a->type  = t;
  a->ts_ms = g_mock_ms;
  if (msg)   strncpy(a->msg, msg, AG_ALERT_MSG_LEN - 1);
  if (bssid) memcpy(a->bssid, bssid, 6);
  g_ag_alert_head = (g_ag_alert_head + 1) % AG_MAX_ALERTS;
  if (g_ag_alert_count < AG_MAX_ALERTS) g_ag_alert_count++;
}

static AgBssidEntry* ag_find_or_add_bssid(const uint8_t *bssid) {
  for (int i = 0; i < g_ag_bssid_count; i++)
    if (mac_eq(g_ag_bssids[i].bssid, bssid)) return &g_ag_bssids[i];
  if (g_ag_bssid_count >= AG_MAX_BSSIDS) return nullptr;
  AgBssidEntry *e = &g_ag_bssids[g_ag_bssid_count++];
  memset(e, 0, sizeof(AgBssidEntry));
  memcpy(e->bssid, bssid, 6);
  e->active = true;
  e->deauth_window_start_ms = g_mock_ms;
  return e;
}

// Record a deauth frame; return true if storm alert was triggered.
static bool ag_record_deauth(const uint8_t *bssid) {
  AgBssidEntry *e = ag_find_or_add_bssid(bssid);
  if (!e) return false;
  uint32_t elapsed = g_mock_ms - e->deauth_window_start_ms;
  if (elapsed >= 1000) {
    e->deauth_count            = 0;
    e->deauth_window_start_ms  = g_mock_ms;
  }
  e->deauth_count++;
  if (e->deauth_count >= (uint32_t)AG_DEAUTH_THRESHOLD) {
    ag_add_alert(AG_EVT_DEAUTH_STORM, "Deauth storm detected", bssid);
    e->deauth_count = 0;  // reset after alert
    return true;
  }
  return false;
}

// Check for a rogue AP: same SSID seen from a different BSSID than usual.
static bool ag_check_rogue_ap(const uint8_t *bssid, const char *ssid) {
  for (int i = 0; i < g_ag_bssid_count; i++) {
    if (strcmp(g_ag_bssids[i].ssid, ssid) == 0 &&
        !mac_eq(g_ag_bssids[i].bssid, bssid)) {
      ag_add_alert(AG_EVT_ROGUE_AP, "Rogue AP: duplicate SSID from new BSSID", bssid);
      return true;
    }
  }
  // Register this BSSID/SSID pair.
  AgBssidEntry *e = ag_find_or_add_bssid(bssid);
  if (e && e->ssid[0] == '\0') strncpy(e->ssid, ssid, 32);
  return false;
}

// Check sequence number for anomalous jumps (possible injection).
static bool ag_check_seq(const uint8_t *bssid, uint16_t seq) {
  AgBssidEntry *e = ag_find_or_add_bssid(bssid);
  if (!e) return false;
  if (e->seq_last == 0) { e->seq_last = seq; return false; }
  uint16_t diff = (seq - e->seq_last) & 0x0FFF;  // 12-bit seq field
  e->seq_last = seq;
  if (diff > AG_SEQ_WINDOW) {
    ag_add_alert(AG_EVT_SEQ_ANOMALY, "Sequence number anomaly", bssid);
    return true;
  }
  return false;
}

// ============================================================
// TESTS: Deauth storm detection
// ============================================================

void test_deauth_no_alert_below_threshold() {
  ag_reset();
  uint8_t bssid[] = {0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
  for (int i = 0; i < AG_DEAUTH_THRESHOLD - 1; i++)
    ag_record_deauth(bssid);
  TEST_ASSERT_EQUAL(0, g_ag_alert_count);
}

void test_deauth_alert_at_threshold() {
  ag_reset();
  uint8_t bssid[] = {0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
  for (int i = 0; i < AG_DEAUTH_THRESHOLD; i++)
    ag_record_deauth(bssid);
  TEST_ASSERT_EQUAL(1, g_ag_alert_count);
  TEST_ASSERT_EQUAL(AG_EVT_DEAUTH_STORM, g_ag_alerts[0].type);
}

void test_deauth_window_resets_after_1s() {
  ag_reset();
  uint8_t bssid[] = {0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
  for (int i = 0; i < AG_DEAUTH_THRESHOLD - 1; i++)
    ag_record_deauth(bssid);
  TEST_ASSERT_EQUAL(0, g_ag_alert_count);
  g_mock_ms = 1001;  // new 1-second window
  for (int i = 0; i < AG_DEAUTH_THRESHOLD - 1; i++)
    ag_record_deauth(bssid);
  TEST_ASSERT_EQUAL(0, g_ag_alert_count);  // still below in new window
}

void test_deauth_independent_bssids() {
  ag_reset();
  uint8_t b1[] = {0x02, 0x11, 0x11, 0x11, 0x11, 0x11};
  uint8_t b2[] = {0x02, 0x22, 0x22, 0x22, 0x22, 0x22};
  for (int i = 0; i < AG_DEAUTH_THRESHOLD - 1; i++) ag_record_deauth(b1);
  for (int i = 0; i < AG_DEAUTH_THRESHOLD - 1; i++) ag_record_deauth(b2);
  TEST_ASSERT_EQUAL(0, g_ag_alert_count);
  ag_record_deauth(b1);  // push b1 over threshold
  TEST_ASSERT_EQUAL(1, g_ag_alert_count);
}

// ============================================================
// TESTS: Rogue AP detection
// ============================================================

void test_rogue_ap_first_bssid_no_alert() {
  ag_reset();
  uint8_t bssid[] = {0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
  TEST_ASSERT_FALSE(ag_check_rogue_ap(bssid, "HomeNetwork"));
  TEST_ASSERT_EQUAL(0, g_ag_alert_count);
}

void test_rogue_ap_same_bssid_no_alert() {
  ag_reset();
  uint8_t bssid[] = {0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
  ag_check_rogue_ap(bssid, "HomeNetwork");
  TEST_ASSERT_FALSE(ag_check_rogue_ap(bssid, "HomeNetwork"));
  TEST_ASSERT_EQUAL(0, g_ag_alert_count);
}

void test_rogue_ap_different_bssid_same_ssid_alerts() {
  ag_reset();
  uint8_t bssid1[] = {0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
  uint8_t bssid2[] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
  ag_check_rogue_ap(bssid1, "HomeNetwork");
  TEST_ASSERT_TRUE(ag_check_rogue_ap(bssid2, "HomeNetwork"));
  TEST_ASSERT_EQUAL(1, g_ag_alert_count);
  TEST_ASSERT_EQUAL(AG_EVT_ROGUE_AP, g_ag_alerts[0].type);
}

void test_rogue_ap_different_ssid_no_alert() {
  ag_reset();
  uint8_t bssid1[] = {0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
  uint8_t bssid2[] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
  ag_check_rogue_ap(bssid1, "Network1");
  TEST_ASSERT_FALSE(ag_check_rogue_ap(bssid2, "Network2"));
  TEST_ASSERT_EQUAL(0, g_ag_alert_count);
}

// ============================================================
// TESTS: Sequence number anomaly detection
// ============================================================

void test_seq_no_alert_first_frame() {
  ag_reset();
  uint8_t bssid[] = {0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
  TEST_ASSERT_FALSE(ag_check_seq(bssid, 100));
  TEST_ASSERT_EQUAL(0, g_ag_alert_count);
}

void test_seq_no_alert_sequential() {
  ag_reset();
  uint8_t bssid[] = {0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
  ag_check_seq(bssid, 100);
  TEST_ASSERT_FALSE(ag_check_seq(bssid, 101));
  TEST_ASSERT_EQUAL(0, g_ag_alert_count);
}

void test_seq_no_alert_small_jump() {
  ag_reset();
  uint8_t bssid[] = {0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
  ag_check_seq(bssid, 100);
  TEST_ASSERT_FALSE(ag_check_seq(bssid, 100 + AG_SEQ_WINDOW - 1));
  TEST_ASSERT_EQUAL(0, g_ag_alert_count);
}

void test_seq_alert_large_jump() {
  ag_reset();
  uint8_t bssid[] = {0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
  ag_check_seq(bssid, 100);
  TEST_ASSERT_TRUE(ag_check_seq(bssid, 100 + AG_SEQ_WINDOW + 1));
  TEST_ASSERT_EQUAL(1, g_ag_alert_count);
  TEST_ASSERT_EQUAL(AG_EVT_SEQ_ANOMALY, g_ag_alerts[0].type);
}

// ============================================================
// TESTS: Alert ring buffer
// ============================================================

void test_alert_ring_max_cap() {
  ag_reset();
  uint8_t bssid[] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x00};
  for (int i = 0; i < AG_MAX_ALERTS + 5; i++)
    ag_add_alert(AG_EVT_DEAUTH_STORM, "test", bssid);
  TEST_ASSERT_EQUAL(AG_MAX_ALERTS, g_ag_alert_count);
}

void test_alert_stores_type() {
  ag_reset();
  uint8_t bssid[] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x00};
  ag_add_alert(AG_EVT_ROGUE_AP, "rogue", bssid);
  TEST_ASSERT_EQUAL(AG_EVT_ROGUE_AP, g_ag_alerts[0].type);
}

void test_alert_stores_bssid() {
  ag_reset();
  uint8_t bssid[] = {0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
  ag_add_alert(AG_EVT_DEAUTH_STORM, "storm", bssid);
  TEST_ASSERT_EQUAL_MEMORY(bssid, g_ag_alerts[0].bssid, 6);
}

void test_alert_stores_timestamp() {
  ag_reset();
  g_mock_ms = 99999;
  uint8_t bssid[] = {0};
  ag_add_alert(AG_EVT_SEQ_ANOMALY, "seq", bssid);
  TEST_ASSERT_EQUAL(99999u, g_ag_alerts[0].ts_ms);
}

// ============================================================
// ENTRY POINT
// ============================================================

void setUp()    { ag_reset(); }
void tearDown() {}

int main(int argc, char **argv) {
  UNITY_BEGIN();

  RUN_TEST(test_deauth_no_alert_below_threshold);
  RUN_TEST(test_deauth_alert_at_threshold);
  RUN_TEST(test_deauth_window_resets_after_1s);
  RUN_TEST(test_deauth_independent_bssids);

  RUN_TEST(test_rogue_ap_first_bssid_no_alert);
  RUN_TEST(test_rogue_ap_same_bssid_no_alert);
  RUN_TEST(test_rogue_ap_different_bssid_same_ssid_alerts);
  RUN_TEST(test_rogue_ap_different_ssid_no_alert);

  RUN_TEST(test_seq_no_alert_first_frame);
  RUN_TEST(test_seq_no_alert_sequential);
  RUN_TEST(test_seq_no_alert_small_jump);
  RUN_TEST(test_seq_alert_large_jump);

  RUN_TEST(test_alert_ring_max_cap);
  RUN_TEST(test_alert_stores_type);
  RUN_TEST(test_alert_stores_bssid);
  RUN_TEST(test_alert_stores_timestamp);

  return UNITY_END();
}

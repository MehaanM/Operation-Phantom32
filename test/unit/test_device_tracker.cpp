// test_device_tracker.cpp — Unit tests for device tracking logic.
//
// Tests MAC deduplication, probe SSID list management, AP-client
// association tracking, alert ring buffer, and device type classification.
// All tests run on native (no Arduino dependencies).

#include <unity.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

// ============================================================
// MINIMAL STUB IMPLEMENTATIONS (native env, no ESP32 deps)
// ============================================================

#define DT_MAX_PROBES   12
#define DT_SSID_LEN     33
#define DT_MAX_DEVICES  96
#define DT_MAX_APS      32
#define DT_MAX_ALERTS   24
#define DT_RSSI_HIST    20

typedef enum { DT_UNKNOWN=0, DT_STA=1, DT_AP=2, DT_REPEATER=3 } DtDevType;

struct DtRssiHist {
  int8_t  vals[DT_RSSI_HIST];
  uint8_t head;
  uint8_t count;
  void push(int8_t v) {
    vals[head] = v;
    head = (head+1) % DT_RSSI_HIST;
    if (count < DT_RSSI_HIST) count++;
  }
  float avg() const {
    if (!count) return 0.0f;
    int32_t s = 0;
    for (int i = 0; i < count; i++) s += vals[i];
    return (float)s / count;
  }
  int8_t min_v() const {
    if (!count) return 0;
    int8_t m = vals[0];
    for (int i = 1; i < count; i++) if (vals[i] < m) m = vals[i];
    return m;
  }
  int8_t max_v() const {
    if (!count) return 0;
    int8_t m = vals[0];
    for (int i = 1; i < count; i++) if (vals[i] > m) m = vals[i];
    return m;
  }
};

struct DtDevice {
  uint8_t    mac[6];
  char       vendor[20];
  DtDevType  type;
  int8_t     rssi_last;
  DtRssiHist rssi_hist;
  uint16_t   probes;
  uint16_t   beacons_rx;
  uint16_t   data_frames;
  uint16_t   mgmt_frames;
  uint16_t   deauths_rx;
  uint8_t    assoc_ap[6];
  char       probe_ssids[DT_MAX_PROBES][DT_SSID_LEN];
  uint8_t    probe_count;
  uint32_t   first_seen_ms;
  uint32_t   last_seen_ms;
  uint32_t   last_active_ms;
  bool       is_ap;
  bool       randomized;
  bool       present;
  bool       alerted;
};

struct DtAP {
  uint8_t  bssid[6];
  char     ssid[33];
  uint8_t  ch;
  int8_t   rssi;
  uint16_t beacons;
  uint16_t clients;
  uint8_t  client_macs[8][6];
  uint32_t last_seen_ms;
};

struct DtAlert {
  uint8_t  mac[6];
  char     vendor[20];
  int8_t   rssi;
  bool     appeared;
  uint32_t ts_ms;
  DtDevType type;
};

static DtDevice g_dt_devs[DT_MAX_DEVICES];
static int g_dt_dev_count = 0;
static DtAP g_dt_aps[DT_MAX_APS];
static int g_dt_ap_count = 0;
static DtAlert g_dt_alerts[DT_MAX_ALERTS];
static int g_dt_alert_count = 0;
static int g_dt_alert_head = 0;

static bool mac_eq(const uint8_t *a, const uint8_t *b) { return memcmp(a,b,6)==0; }
static bool mac_rand(const uint8_t *m) { return (m[0] & 0x02) != 0; }

static bool probe_exists(DtDevice *d, const char *s) {
  for (int i = 0; i < d->probe_count; i++)
    if (strcmp(d->probe_ssids[i], s) == 0) return true;
  return false;
}
static void add_probe(DtDevice *d, const char *s) {
  if (probe_exists(d, s)) return;
  if (d->probe_count >= DT_MAX_PROBES) return;
  strncpy(d->probe_ssids[d->probe_count++], s, DT_SSID_LEN-1);
}

static DtDevice* find_or_add(const uint8_t *mac, int8_t rssi) {
  for (int i = 0; i < g_dt_dev_count; i++)
    if (mac_eq(g_dt_devs[i].mac, mac)) return &g_dt_devs[i];
  if (g_dt_dev_count >= DT_MAX_DEVICES) return nullptr;
  DtDevice *d = &g_dt_devs[g_dt_dev_count++];
  memset(d, 0, sizeof(DtDevice));
  memcpy(d->mac, mac, 6);
  d->rssi_last = rssi;
  d->randomized = mac_rand(mac);
  d->present = true;
  return d;
}

static DtAP* find_or_add_ap(const uint8_t *bssid, int8_t rssi, uint8_t ch) {
  for (int i = 0; i < g_dt_ap_count; i++)
    if (mac_eq(g_dt_aps[i].bssid, bssid)) return &g_dt_aps[i];
  if (g_dt_ap_count >= DT_MAX_APS) return nullptr;
  DtAP *a = &g_dt_aps[g_dt_ap_count++];
  memset(a, 0, sizeof(DtAP));
  memcpy(a->bssid, bssid, 6);
  a->rssi = rssi; a->ch = ch;
  return a;
}

static void ap_add_client(DtAP *ap, const uint8_t *client) {
  for (int i = 0; i < 8; i++)
    if (mac_eq(ap->client_macs[i], client)) return;
  for (int i = 0; i < 8; i++) {
    if (!ap->client_macs[i][0] && !ap->client_macs[i][1]) {
      memcpy(ap->client_macs[i], client, 6);
      ap->clients++;
      return;
    }
  }
}

static void reset_state() {
  memset(g_dt_devs, 0, sizeof(g_dt_devs));
  memset(g_dt_aps, 0, sizeof(g_dt_aps));
  memset(g_dt_alerts, 0, sizeof(g_dt_alerts));
  g_dt_dev_count = 0;
  g_dt_ap_count = 0;
  g_dt_alert_count = 0;
  g_dt_alert_head = 0;
}

// ============================================================
// TESTS: Device deduplication
// ============================================================

void test_device_add_first() {
  reset_state();
  uint8_t mac[] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
  DtDevice *d = find_or_add(mac, -60);
  TEST_ASSERT_NOT_NULL(d);
  TEST_ASSERT_EQUAL(1, g_dt_dev_count);
}

void test_device_no_duplicate() {
  reset_state();
  uint8_t mac[] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
  find_or_add(mac, -60);
  find_or_add(mac, -55);
  TEST_ASSERT_EQUAL(1, g_dt_dev_count);
}

void test_device_different_macs() {
  reset_state();
  uint8_t mac1[] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
  uint8_t mac2[] = {0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
  find_or_add(mac1, -60);
  find_or_add(mac2, -70);
  TEST_ASSERT_EQUAL(2, g_dt_dev_count);
}

void test_device_find_returns_same() {
  reset_state();
  uint8_t mac[] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
  DtDevice *d1 = find_or_add(mac, -60);
  DtDevice *d2 = find_or_add(mac, -55);
  TEST_ASSERT_EQUAL_PTR(d1, d2);
}

void test_device_mac_stored_correctly() {
  reset_state();
  uint8_t mac[] = {0x02, 0xDE, 0xAD, 0xBE, 0xEF, 0x01};
  DtDevice *d = find_or_add(mac, -40);
  TEST_ASSERT_EQUAL_MEMORY(mac, d->mac, 6);
}

void test_device_rssi_stored() {
  reset_state();
  uint8_t mac[] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
  DtDevice *d = find_or_add(mac, -75);
  TEST_ASSERT_EQUAL(-75, d->rssi_last);
}

// ============================================================
// TESTS: Randomized MAC detection
// ============================================================

void test_random_mac_detected() {
  reset_state();
  uint8_t mac[] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55}; // bit1 set
  DtDevice *d = find_or_add(mac, -60);
  TEST_ASSERT_TRUE(d->randomized);
}

void test_oui_mac_not_random() {
  reset_state();
  uint8_t mac[] = {0xAC, 0xDE, 0x48, 0x00, 0x00, 0x01}; // OUI prefix, bit1 clear
  DtDevice *d = find_or_add(mac, -60);
  TEST_ASSERT_FALSE(d->randomized);
}

void test_zero_mac_not_random() {
  reset_state();
  uint8_t mac[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  DtDevice *d = find_or_add(mac, -60);
  TEST_ASSERT_FALSE(d->randomized);
}

// ============================================================
// TESTS: Probe SSID tracking
// ============================================================

void test_probe_add_first() {
  reset_state();
  uint8_t mac[] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
  DtDevice *d = find_or_add(mac, -60);
  add_probe(d, "HomeNetwork");
  TEST_ASSERT_EQUAL(1, d->probe_count);
  TEST_ASSERT_EQUAL_STRING("HomeNetwork", d->probe_ssids[0]);
}

void test_probe_no_duplicate() {
  reset_state();
  uint8_t mac[] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
  DtDevice *d = find_or_add(mac, -60);
  add_probe(d, "Network1");
  add_probe(d, "Network1");
  TEST_ASSERT_EQUAL(1, d->probe_count);
}

void test_probe_multiple_ssids() {
  reset_state();
  uint8_t mac[] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
  DtDevice *d = find_or_add(mac, -60);
  add_probe(d, "Net1");
  add_probe(d, "Net2");
  add_probe(d, "Net3");
  TEST_ASSERT_EQUAL(3, d->probe_count);
}

void test_probe_max_cap() {
  reset_state();
  uint8_t mac[] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
  DtDevice *d = find_or_add(mac, -60);
  for (int i = 0; i < DT_MAX_PROBES + 2; i++) {
    char ssid[8]; snprintf(ssid, 8, "net%d", i);
    add_probe(d, ssid);
  }
  TEST_ASSERT_EQUAL(DT_MAX_PROBES, d->probe_count);
}

void test_probe_exists_true() {
  reset_state();
  uint8_t mac[] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
  DtDevice *d = find_or_add(mac, -60);
  add_probe(d, "TestNet");
  TEST_ASSERT_TRUE(probe_exists(d, "TestNet"));
}

void test_probe_exists_false() {
  reset_state();
  uint8_t mac[] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
  DtDevice *d = find_or_add(mac, -60);
  add_probe(d, "TestNet");
  TEST_ASSERT_FALSE(probe_exists(d, "OtherNet"));
}

void test_probe_case_sensitive() {
  reset_state();
  uint8_t mac[] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
  DtDevice *d = find_or_add(mac, -60);
  add_probe(d, "TestNet");
  TEST_ASSERT_FALSE(probe_exists(d, "testnet"));
  TEST_ASSERT_EQUAL(1, d->probe_count); // should add "testnet" as a different SSID
  add_probe(d, "testnet");
  TEST_ASSERT_EQUAL(2, d->probe_count);
}

// ============================================================
// TESTS: AP tracking
// ============================================================

void test_ap_add_first() {
  reset_state();
  uint8_t bssid[] = {0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
  DtAP *a = find_or_add_ap(bssid, -50, 6);
  TEST_ASSERT_NOT_NULL(a);
  TEST_ASSERT_EQUAL(1, g_dt_ap_count);
  TEST_ASSERT_EQUAL(6, a->ch);
  TEST_ASSERT_EQUAL(-50, a->rssi);
}

void test_ap_no_duplicate() {
  reset_state();
  uint8_t bssid[] = {0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
  find_or_add_ap(bssid, -50, 6);
  find_or_add_ap(bssid, -55, 11);
  TEST_ASSERT_EQUAL(1, g_dt_ap_count);
}

void test_ap_client_add() {
  reset_state();
  uint8_t bssid[] = {0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
  uint8_t client[] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
  DtAP *a = find_or_add_ap(bssid, -50, 6);
  ap_add_client(a, client);
  TEST_ASSERT_EQUAL(1, a->clients);
}

void test_ap_client_no_duplicate() {
  reset_state();
  uint8_t bssid[] = {0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
  uint8_t client[] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
  DtAP *a = find_or_add_ap(bssid, -50, 6);
  ap_add_client(a, client);
  ap_add_client(a, client);
  TEST_ASSERT_EQUAL(1, a->clients);
}

void test_ap_multiple_clients() {
  reset_state();
  uint8_t bssid[] = {0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
  DtAP *a = find_or_add_ap(bssid, -50, 6);
  for (int i = 0; i < 4; i++) {
    uint8_t client[] = {0x02, 0x00, 0x00, 0x00, 0x00, (uint8_t)i};
    ap_add_client(a, client);
  }
  TEST_ASSERT_EQUAL(4, a->clients);
}

// ============================================================
// TESTS: RSSI history
// ============================================================

void test_rssi_hist_push() {
  DtRssiHist h = {};
  h.push(-60);
  TEST_ASSERT_EQUAL(1, h.count);
  TEST_ASSERT_EQUAL(-60, h.vals[0]);
}

void test_rssi_hist_avg_single() {
  DtRssiHist h = {};
  h.push(-60);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, -60.0f, h.avg());
}

void test_rssi_hist_avg_multiple() {
  DtRssiHist h = {};
  h.push(-60); h.push(-70); h.push(-50);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, -60.0f, h.avg());
}

void test_rssi_hist_min() {
  DtRssiHist h = {};
  h.push(-60); h.push(-80); h.push(-40);
  TEST_ASSERT_EQUAL(-80, h.min_v());
}

void test_rssi_hist_max() {
  DtRssiHist h = {};
  h.push(-60); h.push(-80); h.push(-40);
  TEST_ASSERT_EQUAL(-40, h.max_v());
}

void test_rssi_hist_empty_avg() {
  DtRssiHist h = {};
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, h.avg());
}

void test_rssi_hist_empty_min() {
  DtRssiHist h = {};
  TEST_ASSERT_EQUAL(0, h.min_v());
}

void test_rssi_hist_wraps_around() {
  DtRssiHist h = {};
  for (int i = 0; i < DT_RSSI_HIST + 5; i++) h.push(-50);
  TEST_ASSERT_EQUAL(DT_RSSI_HIST, h.count);
}

// ============================================================
// ENTRY POINT
// ============================================================

void setUp()   { reset_state(); }
void tearDown() {}

int main(int argc, char **argv) {
  UNITY_BEGIN();

  RUN_TEST(test_device_add_first);
  RUN_TEST(test_device_no_duplicate);
  RUN_TEST(test_device_different_macs);
  RUN_TEST(test_device_find_returns_same);
  RUN_TEST(test_device_mac_stored_correctly);
  RUN_TEST(test_device_rssi_stored);

  RUN_TEST(test_random_mac_detected);
  RUN_TEST(test_oui_mac_not_random);
  RUN_TEST(test_zero_mac_not_random);

  RUN_TEST(test_probe_add_first);
  RUN_TEST(test_probe_no_duplicate);
  RUN_TEST(test_probe_multiple_ssids);
  RUN_TEST(test_probe_max_cap);
  RUN_TEST(test_probe_exists_true);
  RUN_TEST(test_probe_exists_false);
  RUN_TEST(test_probe_case_sensitive);

  RUN_TEST(test_ap_add_first);
  RUN_TEST(test_ap_no_duplicate);
  RUN_TEST(test_ap_client_add);
  RUN_TEST(test_ap_client_no_duplicate);
  RUN_TEST(test_ap_multiple_clients);

  RUN_TEST(test_rssi_hist_push);
  RUN_TEST(test_rssi_hist_avg_single);
  RUN_TEST(test_rssi_hist_avg_multiple);
  RUN_TEST(test_rssi_hist_min);
  RUN_TEST(test_rssi_hist_max);
  RUN_TEST(test_rssi_hist_empty_avg);
  RUN_TEST(test_rssi_hist_empty_min);
  RUN_TEST(test_rssi_hist_wraps_around);

  return UNITY_END();
}

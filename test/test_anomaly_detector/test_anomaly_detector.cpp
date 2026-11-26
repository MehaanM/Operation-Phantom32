// test_anomaly_detector.cpp — Tests for NetworkObserver + AnomalyDetector (Phase 8).

#include "unity.h"
#include "hal/NetworkObserver.h"
#include "core/AnomalyDetector.h"

void setUp() {}
void tearDown() {}

// ── NetworkObserver ───────────────────────────────────────────────────────────

static NetworkObservation makeObs(const char *ssid, uint8_t ch, int8_t rssi,
                                  uint8_t enc, uint64_t ts) {
  NetworkObservation o = {};
  strncpy(o.ssid, ssid, sizeof(o.ssid) - 1);
  o.channel = ch;
  o.rssi = rssi;
  o.encryptionType = enc;
  o.timestampMs = ts;
  return o;
}

static void test_observer_stores_observation() {
  NetworkObserver no;
  no.observe(makeObs("phantom", 6, -65, 3, 1000));
  TEST_ASSERT_EQUAL(1, (int)no.observationCount());
}

static void test_observer_clear() {
  NetworkObserver no;
  no.observe(makeObs("phantom", 6, -65, 3, 1000));
  no.clear();
  TEST_ASSERT_EQUAL(0, (int)no.observationCount());
}

static void test_observer_at_valid_index() {
  NetworkObserver no;
  no.observe(makeObs("net", 11, -70, 2, 2000));
  const NetworkObservation *o = no.at(0);
  TEST_ASSERT_NOT_NULL(o);
  TEST_ASSERT_EQUAL_STRING("net", o->ssid);
  TEST_ASSERT_EQUAL(11, o->channel);
}

static void test_observer_at_out_of_range_null() {
  NetworkObserver no;
  TEST_ASSERT_NULL(no.at(0));
}

static void test_observer_anonymize_clears_bssid() {
  NetworkObserver no;
  no.setAnonymize(true);
  NetworkObservation obs = makeObs("secret", 1, -50, 3, 1000);
  uint8_t bssid[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
  memcpy(obs.bssid, bssid, 6);
  no.observe(obs);
  const NetworkObservation *stored = no.at(0);
  TEST_ASSERT_NOT_NULL(stored);
  uint8_t zero[6] = {};
  TEST_ASSERT_EQUAL_MEMORY(zero, stored->bssid, 6);
  TEST_ASSERT_TRUE(stored->anonymized);
}

static void test_observer_no_anonymize_keeps_bssid() {
  NetworkObserver no;
  no.setAnonymize(false);
  NetworkObservation obs = makeObs("open", 6, -60, 0, 1000);
  uint8_t bssid[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
  memcpy(obs.bssid, bssid, 6);
  no.observe(obs);
  const NetworkObservation *stored = no.at(0);
  TEST_ASSERT_NOT_NULL(stored);
  TEST_ASSERT_EQUAL_MEMORY(bssid, stored->bssid, 6);
}

static void test_observer_expire_older_than() {
  NetworkObserver no;
  no.observe(makeObs("old", 6, -70, 3, 100));
  no.observe(makeObs("new", 6, -60, 3, 5000));
  no.expireOlderThan(1000);
  TEST_ASSERT_EQUAL(1, (int)no.observationCount());
  TEST_ASSERT_EQUAL_STRING("new", no.at(0)->ssid);
}

static void test_observer_channel_utilization() {
  NetworkObserver no;
  no.observe(makeObs("a", 6, -60, 3, 1000));
  no.observe(makeObs("b", 6, -60, 3, 2000));
  no.observe(makeObs("c", 11, -60, 3, 3000));
  uint8_t util6 = no.channelUtilization(6);
  TEST_ASSERT_EQUAL(66u, util6);  // 2/3 * 100 = 66
}

static void test_observer_unique_network_count() {
  NetworkObserver no;
  no.observe(makeObs("alpha", 6, -60, 3, 1000));
  no.observe(makeObs("beta", 11, -70, 2, 2000));
  no.observe(makeObs("alpha", 6, -65, 3, 3000));  // duplicate
  TEST_ASSERT_EQUAL(2, (int)no.uniqueNetworkCount());
}

static void test_observer_rssi_trend_improving() {
  NetworkObserver no;
  for (int i = 0; i < 8; ++i) {
    int8_t rssi = (int8_t)(-80 + i * 3);
    no.observe(makeObs("ap", 6, rssi, 3, (uint64_t)i * 1000));
  }
  int8_t trend = no.rssiTrend("ap");
  TEST_ASSERT_EQUAL(1, trend);
}

// ── AnomalyDetector ───────────────────────────────────────────────────────────

static void makeBaselineBssid(uint8_t bssid[6], uint8_t v) {
  for (int i = 0; i < 6; ++i) bssid[i] = v;
}

static void test_add_baseline_ok() {
  AnomalyDetector ad;
  uint8_t bssid[6] = {1, 2, 3, 4, 5, 6};
  TEST_ASSERT_TRUE(ad.addBaseline("corp", bssid, -80, -40, 6, 3));
}

static void test_add_baseline_null_ssid_fails() {
  AnomalyDetector ad;
  uint8_t bssid[6] = {};
  TEST_ASSERT_FALSE(ad.addBaseline(nullptr, bssid, -80, -40, 6, 3));
}

static void test_no_anomaly_matching_baseline() {
  AnomalyDetector ad;
  uint8_t bssid[6] = {1, 2, 3, 4, 5, 6};
  ad.addBaseline("corp", bssid, -80, -40, 6, 3);
  NetworkObservation obs = makeObs("corp", 6, -60, 3, 1000);
  memcpy(obs.bssid, bssid, 6);
  TEST_ASSERT_FALSE(ad.analyze(obs, 1000));
  TEST_ASSERT_EQUAL(0, (int)ad.anomalyCount());
}

static void test_channel_change_anomaly() {
  AnomalyDetector ad;
  uint8_t bssid[6] = {1, 2, 3, 4, 5, 6};
  ad.addBaseline("corp", bssid, -80, -40, 6, 3);
  NetworkObservation obs = makeObs("corp", 11, -60, 3, 2000);  // channel changed
  memcpy(obs.bssid, bssid, 6);
  TEST_ASSERT_TRUE(ad.analyze(obs, 2000));
  TEST_ASSERT_EQUAL(1, (int)ad.anomalyCount());
  TEST_ASSERT_EQUAL((int)AnomalyType::CHANNEL_CHANGE,
                    (int)ad.anomalyAt(0)->type);
}

static void test_encryption_change_anomaly() {
  AnomalyDetector ad;
  uint8_t bssid[6] = {1, 2, 3, 4, 5, 6};
  ad.addBaseline("corp", bssid, -80, -40, 6, 3);
  NetworkObservation obs = makeObs("corp", 6, -60, 0, 3000);  // encryption changed
  memcpy(obs.bssid, bssid, 6);
  TEST_ASSERT_TRUE(ad.analyze(obs, 3000));
  TEST_ASSERT_EQUAL(1, (int)ad.anomalyCount());
  TEST_ASSERT_EQUAL((int)AnomalyType::ENCRYPTION_CHANGE,
                    (int)ad.anomalyAt(0)->type);
}

static void test_duplicate_ssid_anomaly() {
  AnomalyDetector ad;
  uint8_t bssid1[6] = {1, 2, 3, 4, 5, 6};
  uint8_t bssid2[6] = {9, 9, 9, 9, 9, 9};
  ad.addBaseline("corp", bssid1, -80, -40, 6, 3);
  NetworkObservation obs = makeObs("corp", 6, -60, 3, 4000);
  memcpy(obs.bssid, bssid2, 6);  // different BSSID
  TEST_ASSERT_TRUE(ad.analyze(obs, 4000));
  TEST_ASSERT_EQUAL(1, (int)ad.anomalyCount());
  TEST_ASSERT_EQUAL((int)AnomalyType::DUPLICATE_SSID,
                    (int)ad.anomalyAt(0)->type);
}

static void test_confidence_at_least_nonzero() {
  AnomalyDetector ad;
  uint8_t bssid[6] = {1, 2, 3, 4, 5, 6};
  ad.addBaseline("corp", bssid, -80, -40, 6, 3);
  NetworkObservation obs = makeObs("corp", 11, -60, 0, 5000);
  memcpy(obs.bssid, bssid, 6);
  ad.analyze(obs, 5000);
  if (ad.anomalyCount() > 0) {
    TEST_ASSERT_TRUE(ad.anomalyAt(0)->confidence > 0);
    TEST_ASSERT_TRUE(ad.anomalyAt(0)->confidence <= 100);
  }
}

static void test_false_positive_threshold_suppresses() {
  AnomalyDetector ad;
  ad.setFalsePositiveThreshold(80);
  uint8_t bssid[6] = {1, 2, 3, 4, 5, 6};
  ad.addBaseline("corp", bssid, -80, -40, 6, 3);
  // Single deviation only scores 60 (channel) — below threshold.
  NetworkObservation obs = makeObs("corp", 11, -60, 3, 1000);
  memcpy(obs.bssid, bssid, 6);
  ad.analyze(obs, 1000);
  TEST_ASSERT_EQUAL(0, (int)ad.anomalyCount());
  TEST_ASSERT_EQUAL(80, (int)ad.falsePositiveThreshold());
}

static void test_clear_anomalies_only() {
  AnomalyDetector ad;
  uint8_t bssid[6] = {1, 2, 3, 4, 5, 6};
  ad.addBaseline("corp", bssid, -80, -40, 6, 3);
  NetworkObservation obs = makeObs("corp", 11, -60, 3, 1000);
  memcpy(obs.bssid, bssid, 6);
  ad.analyze(obs, 1000);
  TEST_ASSERT_TRUE(ad.anomalyCount() > 0);
  ad.clearAnomalies();
  TEST_ASSERT_EQUAL(0, (int)ad.anomalyCount());
}

static void test_clear_all() {
  AnomalyDetector ad;
  uint8_t bssid[6] = {1, 2, 3, 4, 5, 6};
  ad.addBaseline("corp", bssid, -80, -40, 6, 3);
  NetworkObservation obs = makeObs("corp", 11, -60, 3, 1000);
  memcpy(obs.bssid, bssid, 6);
  ad.analyze(obs, 1000);
  ad.clear();
  TEST_ASSERT_EQUAL(0, (int)ad.anomalyCount());
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_observer_stores_observation);
  RUN_TEST(test_observer_clear);
  RUN_TEST(test_observer_at_valid_index);
  RUN_TEST(test_observer_at_out_of_range_null);
  RUN_TEST(test_observer_anonymize_clears_bssid);
  RUN_TEST(test_observer_no_anonymize_keeps_bssid);
  RUN_TEST(test_observer_expire_older_than);
  RUN_TEST(test_observer_channel_utilization);
  RUN_TEST(test_observer_unique_network_count);
  RUN_TEST(test_observer_rssi_trend_improving);

  RUN_TEST(test_add_baseline_ok);
  RUN_TEST(test_add_baseline_null_ssid_fails);
  RUN_TEST(test_no_anomaly_matching_baseline);
  RUN_TEST(test_channel_change_anomaly);
  RUN_TEST(test_encryption_change_anomaly);
  RUN_TEST(test_duplicate_ssid_anomaly);
  RUN_TEST(test_confidence_at_least_nonzero);
  RUN_TEST(test_false_positive_threshold_suppresses);
  RUN_TEST(test_clear_anomalies_only);
  RUN_TEST(test_clear_all);

  return UNITY_END();
}

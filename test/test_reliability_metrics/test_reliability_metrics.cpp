// test_reliability_metrics.cpp — Tests for ReliabilityMetrics (Phase 14).

#include "unity.h"
#include "core/ReliabilityMetrics.h"

void setUp() {}
void tearDown() {}

// ── registerModule ────────────────────────────────────────────────────────────

static void test_register_module_ok() {
  ReliabilityMetrics rm;
  TEST_ASSERT_TRUE(rm.registerModule("WiFi", 0));
  TEST_ASSERT_EQUAL(1, (int)rm.moduleCount());
}

static void test_register_null_fails() {
  ReliabilityMetrics rm;
  TEST_ASSERT_FALSE(rm.registerModule(nullptr, 0));
}

static void test_register_empty_fails() {
  ReliabilityMetrics rm;
  TEST_ASSERT_FALSE(rm.registerModule("", 0));
}

static void test_register_updates_existing() {
  ReliabilityMetrics rm;
  rm.registerModule("WiFi", 0);
  TEST_ASSERT_TRUE(rm.registerModule("WiFi", 5000));
  TEST_ASSERT_EQUAL(1, (int)rm.moduleCount());
  const ModuleReliability *m = rm.metricsFor("WiFi");
  TEST_ASSERT_NOT_NULL(m);
  TEST_ASSERT_EQUAL_UINT64(5000, m->uptimeStartMs);
}

static void test_register_full_fails() {
  ReliabilityMetrics rm;
  for (size_t i = 0; i < PHANTOM_RM_MAX_MODULES; ++i) {
    char name[8];
    name[0] = (char)('a' + i % 26);
    name[1] = (char)('0' + i);
    name[2] = '\0';
    rm.registerModule(name, 0);
  }
  TEST_ASSERT_FALSE(rm.registerModule("overflow", 0));
}

// ── recordFailure / recordRecovery ────────────────────────────────────────────

static void test_record_failure_increments_count() {
  ReliabilityMetrics rm;
  rm.registerModule("BLE", 0);
  rm.recordFailure("BLE", 1000);
  const ModuleReliability *m = rm.metricsFor("BLE");
  TEST_ASSERT_NOT_NULL(m);
  TEST_ASSERT_EQUAL_UINT32(1, m->failureCount);
  TEST_ASSERT_EQUAL_UINT64(1000, m->lastFailureMs);
  TEST_ASSERT_FALSE(m->running);
}

static void test_record_failure_unknown_fails() {
  ReliabilityMetrics rm;
  TEST_ASSERT_FALSE(rm.recordFailure("missing", 1000));
}

static void test_record_recovery_increments_count() {
  ReliabilityMetrics rm;
  rm.registerModule("BLE", 0);
  rm.recordFailure("BLE", 1000);
  rm.recordRecovery("BLE", 2000);
  const ModuleReliability *m = rm.metricsFor("BLE");
  TEST_ASSERT_NOT_NULL(m);
  TEST_ASSERT_EQUAL_UINT32(1, m->recoveryCount);
  TEST_ASSERT_TRUE(m->running);
}

static void test_record_recovery_unknown_fails() {
  ReliabilityMetrics rm;
  TEST_ASSERT_FALSE(rm.recordRecovery("missing", 1000));
}

// ── recordUnexpectedReset ─────────────────────────────────────────────────────

static void test_record_unexpected_reset() {
  ReliabilityMetrics rm;
  rm.registerModule("WDT", 0);
  rm.recordUnexpectedReset("WDT");
  const ModuleReliability *m = rm.metricsFor("WDT");
  TEST_ASSERT_NOT_NULL(m);
  TEST_ASSERT_EQUAL_UINT32(1, m->unexpectedResets);
}

static void test_record_unexpected_reset_unknown_fails() {
  ReliabilityMetrics rm;
  TEST_ASSERT_FALSE(rm.recordUnexpectedReset("missing"));
}

// ── updateUptime ──────────────────────────────────────────────────────────────

static void test_update_uptime_running_module() {
  ReliabilityMetrics rm;
  rm.registerModule("mod", 0);
  rm.updateUptime("mod", 5000);
  const ModuleReliability *m = rm.metricsFor("mod");
  TEST_ASSERT_NOT_NULL(m);
  TEST_ASSERT_EQUAL_UINT64(5000, m->totalUptimeMs);
}

static void test_update_uptime_stopped_module_fails() {
  ReliabilityMetrics rm;
  rm.registerModule("mod", 0);
  rm.recordFailure("mod", 1000);
  TEST_ASSERT_FALSE(rm.updateUptime("mod", 2000));
}

static void test_update_uptime_unknown_fails() {
  ReliabilityMetrics rm;
  TEST_ASSERT_FALSE(rm.updateUptime("missing", 1000));
}

// ── mtbfMs ────────────────────────────────────────────────────────────────────

static void test_mtbf_one_failure_returns_zero() {
  ReliabilityMetrics rm;
  rm.registerModule("mod", 0);
  rm.recordFailure("mod", 1000);
  TEST_ASSERT_EQUAL_UINT64(0, rm.mtbfMs("mod"));
}

static void test_mtbf_two_failures() {
  ReliabilityMetrics rm;
  rm.registerModule("mod", 0);
  rm.recordFailure("mod", 2000); // uptime = 2000
  rm.recordRecovery("mod", 2000);
  rm.recordFailure("mod", 5000); // uptime += 3000, total = 5000, failures = 2
  // MTBF = totalUptime / (failures - 1) = 5000 / 1 = 5000
  TEST_ASSERT_EQUAL_UINT64(5000, rm.mtbfMs("mod"));
}

static void test_mtbf_unknown_zero() {
  ReliabilityMetrics rm;
  TEST_ASSERT_EQUAL_UINT64(0, rm.mtbfMs("missing"));
}

// ── recoveryRatePercent ───────────────────────────────────────────────────────

static void test_recovery_rate_no_failures_100() {
  ReliabilityMetrics rm;
  rm.registerModule("mod", 0);
  TEST_ASSERT_EQUAL(100, (int)rm.recoveryRatePercent("mod"));
}

static void test_recovery_rate_full() {
  ReliabilityMetrics rm;
  rm.registerModule("mod", 0);
  rm.recordFailure("mod", 1000);
  rm.recordRecovery("mod", 1500);
  TEST_ASSERT_EQUAL(100, (int)rm.recoveryRatePercent("mod"));
}

static void test_recovery_rate_partial() {
  ReliabilityMetrics rm;
  rm.registerModule("mod", 0);
  rm.recordFailure("mod", 1000);
  rm.recordFailure("mod", 2000);
  rm.recordRecovery("mod", 2500); // 1 recovery / 2 failures = 50%
  TEST_ASSERT_EQUAL(50, (int)rm.recoveryRatePercent("mod"));
}

static void test_recovery_rate_unknown_100() {
  ReliabilityMetrics rm;
  TEST_ASSERT_EQUAL(100, (int)rm.recoveryRatePercent("missing"));
}

// ── failuresPer100Hours ───────────────────────────────────────────────────────

static void test_failures_per_100hours() {
  ReliabilityMetrics rm;
  rm.registerModule("x", 0);
  // updateUptime at t=360000000 → totalUptimeMs = 360000000, uptimeStartMs = 360000000
  rm.updateUptime("x", 360000000ULL);
  // recordFailure at same timestamp → adds 0 more uptime, failureCount=1
  rm.recordFailure("x", 360000000ULL);
  // failuresPer100Hours = 1 * 360000000 / 360000000 = 1
  TEST_ASSERT_EQUAL_UINT32(1, rm.failuresPer100Hours("x"));
}

static void test_failures_per_100hours_zero_uptime() {
  ReliabilityMetrics rm;
  rm.registerModule("mod", 0);
  TEST_ASSERT_EQUAL_UINT32(0, rm.failuresPer100Hours("mod"));
}

static void test_failures_per_100hours_unknown_zero() {
  ReliabilityMetrics rm;
  TEST_ASSERT_EQUAL_UINT32(0, rm.failuresPer100Hours("missing"));
}

static void test_metrics_for_null_returns_null() {
  ReliabilityMetrics rm;
  TEST_ASSERT_NULL(rm.metricsFor(nullptr));
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_register_module_ok);
  RUN_TEST(test_register_null_fails);
  RUN_TEST(test_register_empty_fails);
  RUN_TEST(test_register_updates_existing);
  RUN_TEST(test_register_full_fails);

  RUN_TEST(test_record_failure_increments_count);
  RUN_TEST(test_record_failure_unknown_fails);
  RUN_TEST(test_record_recovery_increments_count);
  RUN_TEST(test_record_recovery_unknown_fails);

  RUN_TEST(test_record_unexpected_reset);
  RUN_TEST(test_record_unexpected_reset_unknown_fails);

  RUN_TEST(test_update_uptime_running_module);
  RUN_TEST(test_update_uptime_stopped_module_fails);
  RUN_TEST(test_update_uptime_unknown_fails);

  RUN_TEST(test_mtbf_one_failure_returns_zero);
  RUN_TEST(test_mtbf_two_failures);
  RUN_TEST(test_mtbf_unknown_zero);

  RUN_TEST(test_recovery_rate_no_failures_100);
  RUN_TEST(test_recovery_rate_full);
  RUN_TEST(test_recovery_rate_partial);
  RUN_TEST(test_recovery_rate_unknown_100);

  RUN_TEST(test_failures_per_100hours);
  RUN_TEST(test_failures_per_100hours_zero_uptime);
  RUN_TEST(test_failures_per_100hours_unknown_zero);
  RUN_TEST(test_metrics_for_null_returns_null);

  return UNITY_END();
}

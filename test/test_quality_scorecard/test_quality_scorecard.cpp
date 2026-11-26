// test_quality_scorecard.cpp — Tests for QualityScorecard (Phase 18).

#include "unity.h"
#include "core/QualityScorecard.h"

void setUp() {}
void tearDown() {}

// ── trackMetric ───────────────────────────────────────────────────────────────

static void test_track_metric_ok() {
  QualityScorecard qs;
  TEST_ASSERT_TRUE(qs.trackMetric("branch_cov", "%", 80.0, 70.0, true));
  TEST_ASSERT_EQUAL(1, (int)qs.metricCount());
}

static void test_track_null_fails() {
  QualityScorecard qs;
  TEST_ASSERT_FALSE(qs.trackMetric(nullptr, "%", 80.0, 70.0, true));
}

static void test_track_empty_fails() {
  QualityScorecard qs;
  TEST_ASSERT_FALSE(qs.trackMetric("", "%", 80.0, 70.0, true));
}

static void test_track_updates_existing() {
  QualityScorecard qs;
  qs.trackMetric("cov", "%", 80.0, 70.0, true);
  TEST_ASSERT_TRUE(qs.trackMetric("cov", "%", 85.0, 75.0, true));
  TEST_ASSERT_EQUAL(1, (int)qs.metricCount());
  const QualityMetric *m = qs.metricFor("cov");
  TEST_ASSERT_NOT_NULL(m);
  TEST_ASSERT_EQUAL(85, (int)m->warnThreshold);
}

static void test_track_invalid_threshold_higher_is_better() {
  QualityScorecard qs;
  // warnThreshold <= failThreshold when higherIsBetter=true is invalid
  TEST_ASSERT_FALSE(qs.trackMetric("cov", "%", 70.0, 80.0, true));
  TEST_ASSERT_FALSE(qs.trackMetric("cov", "%", 70.0, 70.0, true));
}

static void test_track_invalid_threshold_lower_is_better() {
  QualityScorecard qs;
  // warnThreshold >= failThreshold when higherIsBetter=false is invalid
  TEST_ASSERT_FALSE(qs.trackMetric("warn", "n", 10.0, 5.0, false));
  TEST_ASSERT_FALSE(qs.trackMetric("warn", "n", 10.0, 10.0, false));
}

static void test_track_full_fails() {
  QualityScorecard qs;
  for (size_t i = 0; i < PHANTOM_QS_MAX_METRICS; ++i) {
    char name[8];
    name[0] = 'm';
    name[1] = (char)('0' + i % 10);
    name[2] = (char)('a' + i / 10);
    name[3] = '\0';
    qs.trackMetric(name, "", 0.0, 0.0, true);
  }
  TEST_ASSERT_FALSE(qs.trackMetric("overflow", "", 0.0, 0.0, true));
}

// ── recordValue / statusFor ───────────────────────────────────────────────────

static void test_record_value_pass() {
  QualityScorecard qs;
  qs.trackMetric("cov", "%", 80.0, 70.0, true);
  qs.recordValue("cov", 90.0);
  TEST_ASSERT_EQUAL((int)MetricStatus::PASS, (int)qs.statusFor("cov"));
}

static void test_record_value_warn() {
  QualityScorecard qs;
  qs.trackMetric("cov", "%", 80.0, 70.0, true);
  qs.recordValue("cov", 75.0);
  TEST_ASSERT_EQUAL((int)MetricStatus::WARN, (int)qs.statusFor("cov"));
}

static void test_record_value_fail() {
  QualityScorecard qs;
  qs.trackMetric("cov", "%", 80.0, 70.0, true);
  qs.recordValue("cov", 60.0);
  TEST_ASSERT_EQUAL((int)MetricStatus::FAIL, (int)qs.statusFor("cov"));
}

static void test_record_lower_is_better_pass() {
  QualityScorecard qs;
  qs.trackMetric("warnings", "count", 5.0, 10.0, false);
  qs.recordValue("warnings", 2.0);
  TEST_ASSERT_EQUAL((int)MetricStatus::PASS, (int)qs.statusFor("warnings"));
}

static void test_record_lower_is_better_warn() {
  QualityScorecard qs;
  qs.trackMetric("warnings", "count", 5.0, 10.0, false);
  qs.recordValue("warnings", 7.0);
  TEST_ASSERT_EQUAL((int)MetricStatus::WARN, (int)qs.statusFor("warnings"));
}

static void test_record_lower_is_better_fail() {
  QualityScorecard qs;
  qs.trackMetric("warnings", "count", 5.0, 10.0, false);
  qs.recordValue("warnings", 15.0);
  TEST_ASSERT_EQUAL((int)MetricStatus::FAIL, (int)qs.statusFor("warnings"));
}

static void test_record_unknown_metric_fails() {
  QualityScorecard qs;
  TEST_ASSERT_FALSE(qs.recordValue("missing", 50.0));
}

static void test_status_for_null_unknown() {
  QualityScorecard qs;
  TEST_ASSERT_EQUAL((int)MetricStatus::UNKNOWN, (int)qs.statusFor(nullptr));
}

static void test_status_for_unrecorded_unknown() {
  QualityScorecard qs;
  qs.trackMetric("cov", "%", 80.0, 70.0, true);
  TEST_ASSERT_EQUAL((int)MetricStatus::UNKNOWN, (int)qs.statusFor("cov"));
}

// ── trend ─────────────────────────────────────────────────────────────────────

static void test_first_sample_trend_stable() {
  QualityScorecard qs;
  qs.trackMetric("cov", "%", 80.0, 70.0, true);
  qs.recordValue("cov", 75.0);  // first sample — no prior to compare against
  const QualityMetric *m = qs.metricFor("cov");
  TEST_ASSERT_NOT_NULL(m);
  TEST_ASSERT_EQUAL((int)MetricTrend::STABLE, (int)m->trend);
}

static void test_trend_improving() {
  QualityScorecard qs;
  qs.trackMetric("cov", "%", 80.0, 70.0, true);
  qs.recordValue("cov", 70.0);
  qs.recordValue("cov", 85.0);
  const QualityMetric *m = qs.metricFor("cov");
  TEST_ASSERT_NOT_NULL(m);
  TEST_ASSERT_EQUAL((int)MetricTrend::IMPROVING, (int)m->trend);
}

static void test_trend_regressing() {
  QualityScorecard qs;
  qs.trackMetric("cov", "%", 80.0, 70.0, true);
  qs.recordValue("cov", 90.0);
  qs.recordValue("cov", 75.0);
  const QualityMetric *m = qs.metricFor("cov");
  TEST_ASSERT_NOT_NULL(m);
  TEST_ASSERT_EQUAL((int)MetricTrend::REGRESSING, (int)m->trend);
}

// ── pass/warn/failCount ───────────────────────────────────────────────────────

static void test_pass_warn_fail_counts() {
  QualityScorecard qs;
  qs.trackMetric("a", "", 80.0, 70.0, true);
  qs.trackMetric("b", "", 80.0, 70.0, true);
  qs.trackMetric("c", "", 80.0, 70.0, true);
  qs.recordValue("a", 90.0);  // PASS
  qs.recordValue("b", 75.0);  // WARN
  qs.recordValue("c", 60.0);  // FAIL
  TEST_ASSERT_EQUAL(1, (int)qs.passCount());
  TEST_ASSERT_EQUAL(1, (int)qs.warnCount());
  TEST_ASSERT_EQUAL(1, (int)qs.failCount());
}

// ── saveSnapshot / regressionVs ───────────────────────────────────────────────

static void test_snapshot_ok() {
  QualityScorecard qs;
  qs.trackMetric("cov", "%", 80.0, 70.0, true);
  qs.recordValue("cov", 85.0);
  TEST_ASSERT_TRUE(qs.saveSnapshot("v1", 1000));
}

static void test_snapshot_null_fails() {
  QualityScorecard qs;
  TEST_ASSERT_FALSE(qs.saveSnapshot(nullptr, 1000));
}

static void test_snapshot_updates_existing() {
  QualityScorecard qs;
  qs.trackMetric("cov", "%", 80.0, 70.0, true);
  qs.recordValue("cov", 85.0);
  qs.saveSnapshot("v1", 1000);
  qs.recordValue("cov", 90.0);
  TEST_ASSERT_TRUE(qs.saveSnapshot("v1", 2000));
}

static void test_regression_positive() {
  QualityScorecard qs;
  qs.trackMetric("cov", "%", 80.0, 70.0, true);
  qs.recordValue("cov", 90.0);
  qs.saveSnapshot("v1", 1000);
  qs.recordValue("cov", 80.0);  // dropped 10 → regression = 10
  TEST_ASSERT_EQUAL(10, (int)qs.regressionVs("v1", "cov"));
}

static void test_regression_negative_on_improvement() {
  QualityScorecard qs;
  qs.trackMetric("cov", "%", 80.0, 70.0, true);
  qs.recordValue("cov", 80.0);
  qs.saveSnapshot("v1", 1000);
  qs.recordValue("cov", 90.0);  // improved → regression = -10
  TEST_ASSERT_EQUAL(-10, (int)qs.regressionVs("v1", "cov"));
}

static void test_regression_null_params() {
  QualityScorecard qs;
  TEST_ASSERT_EQUAL(0, (int)qs.regressionVs(nullptr, "cov"));
  TEST_ASSERT_EQUAL(0, (int)qs.regressionVs("v1", nullptr));
}

static void test_regression_unknown_snapshot() {
  QualityScorecard qs;
  qs.trackMetric("cov", "%", 80.0, 70.0, true);
  qs.recordValue("cov", 85.0);
  TEST_ASSERT_EQUAL(0, (int)qs.regressionVs("missing", "cov"));
}

// ── metricFor / generateReport ────────────────────────────────────────────────

static void test_metric_for_null_returns_null() {
  QualityScorecard qs;
  TEST_ASSERT_NULL(qs.metricFor(nullptr));
}

static void test_generate_report_null_safe() {
  QualityScorecard qs;
  qs.generateReport(nullptr);  // must not crash
}

static void test_generate_report_produces_output() {
  QualityScorecard qs;
  qs.trackMetric("branch_cov", "%", 80.0, 70.0, true);
  qs.recordValue("branch_cov", 85.0);
  char buf[512];
  FILE *f = fmemopen(buf, sizeof(buf), "w");
  TEST_ASSERT_NOT_NULL(f);
  qs.generateReport(f);
  fclose(f);
  TEST_ASSERT_NOT_NULL(strstr(buf, "branch_cov"));
  TEST_ASSERT_NOT_NULL(strstr(buf, "PASS"));
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_track_metric_ok);
  RUN_TEST(test_track_null_fails);
  RUN_TEST(test_track_empty_fails);
  RUN_TEST(test_track_updates_existing);
  RUN_TEST(test_track_invalid_threshold_higher_is_better);
  RUN_TEST(test_track_invalid_threshold_lower_is_better);
  RUN_TEST(test_track_full_fails);

  RUN_TEST(test_record_value_pass);
  RUN_TEST(test_record_value_warn);
  RUN_TEST(test_record_value_fail);
  RUN_TEST(test_record_lower_is_better_pass);
  RUN_TEST(test_record_lower_is_better_warn);
  RUN_TEST(test_record_lower_is_better_fail);
  RUN_TEST(test_record_unknown_metric_fails);
  RUN_TEST(test_status_for_null_unknown);
  RUN_TEST(test_status_for_unrecorded_unknown);

  RUN_TEST(test_first_sample_trend_stable);
  RUN_TEST(test_trend_improving);
  RUN_TEST(test_trend_regressing);

  RUN_TEST(test_pass_warn_fail_counts);

  RUN_TEST(test_snapshot_ok);
  RUN_TEST(test_snapshot_null_fails);
  RUN_TEST(test_snapshot_updates_existing);
  RUN_TEST(test_regression_positive);
  RUN_TEST(test_regression_negative_on_improvement);
  RUN_TEST(test_regression_null_params);
  RUN_TEST(test_regression_unknown_snapshot);

  RUN_TEST(test_metric_for_null_returns_null);
  RUN_TEST(test_generate_report_null_safe);
  RUN_TEST(test_generate_report_produces_output);

  return UNITY_END();
}

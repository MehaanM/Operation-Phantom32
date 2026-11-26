// test_benchmark_suite.cpp — Tests for BenchmarkSuite (Phase 13).

#include "unity.h"
#include "core/BenchmarkSuite.h"

void setUp() {}
void tearDown() {}

// ── registerBenchmark ─────────────────────────────────────────────────────────

static void test_register_benchmark_ok() {
  BenchmarkSuite bs;
  TEST_ASSERT_TRUE(bs.registerBenchmark("init_time", 5000));
  TEST_ASSERT_EQUAL(1, (int)bs.benchmarkCount());
}

static void test_register_null_fails() {
  BenchmarkSuite bs;
  TEST_ASSERT_FALSE(bs.registerBenchmark(nullptr));
}

static void test_register_empty_fails() {
  BenchmarkSuite bs;
  TEST_ASSERT_FALSE(bs.registerBenchmark(""));
}

static void test_register_updates_budget() {
  BenchmarkSuite bs;
  bs.registerBenchmark("foo", 1000);
  TEST_ASSERT_TRUE(bs.registerBenchmark("foo", 5000));
  TEST_ASSERT_EQUAL(1, (int)bs.benchmarkCount());
  const BenchmarkResult *r = bs.resultFor("foo");
  TEST_ASSERT_NOT_NULL(r);
  TEST_ASSERT_EQUAL_UINT64(5000, r->budgetUs);
}

static void test_register_full_registry_fails() {
  BenchmarkSuite bs;
  for (size_t i = 0; i < PHANTOM_BS_MAX_BENCHMARKS; ++i) {
    char name[8];
    name[0] = (char)('a' + i % 26);
    name[1] = (char)('0' + i);
    name[2] = '\0';
    bs.registerBenchmark(name, 1000);
  }
  TEST_ASSERT_FALSE(bs.registerBenchmark("overflow", 1000));
}

// ── recordSample / resultFor ──────────────────────────────────────────────────

static void test_record_sample_min_max_avg() {
  BenchmarkSuite bs;
  bs.registerBenchmark("tick");
  bs.recordSample("tick", 100);
  bs.recordSample("tick", 200);
  bs.recordSample("tick", 300);
  const BenchmarkResult *r = bs.resultFor("tick");
  TEST_ASSERT_NOT_NULL(r);
  TEST_ASSERT_EQUAL_UINT64(100, r->minUs);
  TEST_ASSERT_EQUAL_UINT64(300, r->maxUs);
  TEST_ASSERT_EQUAL_UINT64(200, r->avgUs());
  TEST_ASSERT_EQUAL_UINT32(3, r->sampleCount);
}

static void test_record_sample_unknown_fails() {
  BenchmarkSuite bs;
  TEST_ASSERT_FALSE(bs.recordSample("missing", 100));
}

static void test_result_for_null_returns_null() {
  BenchmarkSuite bs;
  TEST_ASSERT_NULL(bs.resultFor(nullptr));
}

static void test_result_for_unknown_returns_null() {
  BenchmarkSuite bs;
  TEST_ASSERT_NULL(bs.resultFor("not_registered"));
}

static void test_result_avg_zero_no_samples() {
  BenchmarkSuite bs;
  bs.registerBenchmark("empty");
  const BenchmarkResult *r = bs.resultFor("empty");
  TEST_ASSERT_NOT_NULL(r);
  TEST_ASSERT_EQUAL_UINT64(0, r->avgUs());
}

// ── anyOverBudget ─────────────────────────────────────────────────────────────

static void test_any_over_budget_true() {
  BenchmarkSuite bs;
  bs.registerBenchmark("slow", 100);
  bs.recordSample("slow", 500); // exceeds 100
  TEST_ASSERT_TRUE(bs.anyOverBudget());
}

static void test_any_over_budget_false() {
  BenchmarkSuite bs;
  bs.registerBenchmark("fast", 1000);
  bs.recordSample("fast", 50);
  TEST_ASSERT_FALSE(bs.anyOverBudget());
}

static void test_no_budget_never_over() {
  BenchmarkSuite bs;
  bs.registerBenchmark("nobud"); // budgetUs=0 means no limit
  bs.recordSample("nobud", 999999);
  TEST_ASSERT_FALSE(bs.anyOverBudget());
}

// ── saveHistory / regressionVsHistory ────────────────────────────────────────

static void test_save_history_and_detect_regression() {
  BenchmarkSuite bs;
  bs.registerBenchmark("init");
  bs.recordSample("init", 100);
  TEST_ASSERT_TRUE(bs.saveHistory("v1.0.0", 1000));
  bs.resetSamples();
  bs.recordSample("init", 200); // slower
  int64_t reg = bs.regressionVsHistory("init", "v1.0.0");
  TEST_ASSERT_TRUE(reg > 0);
}

static void test_regression_negative_on_improvement() {
  BenchmarkSuite bs;
  bs.registerBenchmark("init");
  bs.recordSample("init", 200);
  bs.saveHistory("v1.0.0", 1000);
  bs.resetSamples();
  bs.recordSample("init", 100); // faster (improvement)
  int64_t reg = bs.regressionVsHistory("init", "v1.0.0");
  TEST_ASSERT_TRUE(reg < 0);
}

static void test_regression_null_params_zero() {
  BenchmarkSuite bs;
  bs.registerBenchmark("init");
  TEST_ASSERT_EQUAL(0LL, bs.regressionVsHistory(nullptr, "v1.0.0"));
  TEST_ASSERT_EQUAL(0LL, bs.regressionVsHistory("init", nullptr));
}

static void test_regression_unknown_history_zero() {
  BenchmarkSuite bs;
  bs.registerBenchmark("init");
  bs.recordSample("init", 100);
  TEST_ASSERT_EQUAL(0LL, bs.regressionVsHistory("init", "nope"));
}

static void test_save_history_null_fails() {
  BenchmarkSuite bs;
  TEST_ASSERT_FALSE(bs.saveHistory(nullptr, 1000));
}

// ── resetSamples / clear ──────────────────────────────────────────────────────

static void test_reset_samples_clears_stats() {
  BenchmarkSuite bs;
  bs.registerBenchmark("init");
  bs.recordSample("init", 100);
  bs.resetSamples();
  const BenchmarkResult *r = bs.resultFor("init");
  TEST_ASSERT_NOT_NULL(r);
  TEST_ASSERT_EQUAL_UINT32(0, r->sampleCount);
}

static void test_clear_removes_all() {
  BenchmarkSuite bs;
  bs.registerBenchmark("init");
  bs.recordSample("init", 100);
  bs.clear();
  TEST_ASSERT_EQUAL(0, (int)bs.benchmarkCount());
  TEST_ASSERT_NULL(bs.resultFor("init"));
}

static void test_regression_on_benchmark_not_in_history_zero() {
  BenchmarkSuite bs;
  bs.registerBenchmark("init");
  bs.registerBenchmark("other");
  bs.recordSample("init", 100);
  bs.saveHistory("snap", 1000);
  bs.resetSamples();
  bs.recordSample("other", 200);
  // "other" was registered but had no sample when saveHistory was called, so avg=0
  TEST_ASSERT_EQUAL(0LL, bs.regressionVsHistory("other", "snap") - 200);
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_register_benchmark_ok);
  RUN_TEST(test_register_null_fails);
  RUN_TEST(test_register_empty_fails);
  RUN_TEST(test_register_updates_budget);
  RUN_TEST(test_register_full_registry_fails);

  RUN_TEST(test_record_sample_min_max_avg);
  RUN_TEST(test_record_sample_unknown_fails);
  RUN_TEST(test_result_for_null_returns_null);
  RUN_TEST(test_result_for_unknown_returns_null);
  RUN_TEST(test_result_avg_zero_no_samples);

  RUN_TEST(test_any_over_budget_true);
  RUN_TEST(test_any_over_budget_false);
  RUN_TEST(test_no_budget_never_over);

  RUN_TEST(test_save_history_and_detect_regression);
  RUN_TEST(test_regression_negative_on_improvement);
  RUN_TEST(test_regression_null_params_zero);
  RUN_TEST(test_regression_unknown_history_zero);
  RUN_TEST(test_save_history_null_fails);

  RUN_TEST(test_reset_samples_clears_stats);
  RUN_TEST(test_clear_removes_all);
  RUN_TEST(test_regression_on_benchmark_not_in_history_zero);

  return UNITY_END();
}

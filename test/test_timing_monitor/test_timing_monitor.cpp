// test_timing_monitor.cpp — Tests for TimingMonitor (Phase 4).

#include "unity.h"
#include "core/TimingMonitor.h"

void setUp() {}
void tearDown() {}

// ── registerModule ─────────────────────────────────────────────────────────

static void test_register_module_returns_true() {
  TimingMonitor tm;
  TEST_ASSERT_TRUE(tm.registerModule("WiFi"));
}

static void test_register_null_name_rejected() {
  TimingMonitor tm;
  TEST_ASSERT_FALSE(tm.registerModule(nullptr));
}

static void test_register_duplicate_updates_budget() {
  TimingMonitor tm;
  tm.registerModule("WiFi", 100);
  TEST_ASSERT_TRUE(tm.registerModule("WiFi", 200));
  // Count stays at 1.
  TEST_ASSERT_EQUAL(1, (int)tm.moduleCount());
}

static void test_register_full_returns_false() {
  TimingMonitor tm;
  for (size_t i = 0; i < PHANTOM_TM_MAX_MODULES; ++i) {
    char name[8];
    name[0] = (char)('A' + (i % 26));
    name[1] = (char)('0' + i);
    name[2] = '\0';
    tm.registerModule(name);
  }
  TEST_ASSERT_FALSE(tm.registerModule("overflow"));
}

// ── record ─────────────────────────────────────────────────────────────────

static void test_record_returns_false_for_unknown_module() {
  TimingMonitor tm;
  TEST_ASSERT_FALSE(tm.record("Unknown", 10));
}

static void test_record_returns_true_for_registered() {
  TimingMonitor tm;
  tm.registerModule("M");
  TEST_ASSERT_TRUE(tm.record("M", 10));
}

static void test_record_one_sample_stats() {
  TimingMonitor tm;
  tm.registerModule("M");
  tm.record("M", 42);
  const TimingStats *s = tm.statsFor("M");
  TEST_ASSERT_NOT_NULL(s);
  TEST_ASSERT_EQUAL_UINT32(1, s->sampleCount);
  TEST_ASSERT_EQUAL_UINT32(42, s->minMs);
  TEST_ASSERT_EQUAL_UINT32(42, s->maxMs);
}

static void test_min_tracks_smallest_sample() {
  TimingMonitor tm;
  tm.registerModule("M");
  tm.record("M", 50);
  tm.record("M", 10);
  tm.record("M", 30);
  TEST_ASSERT_EQUAL_UINT32(10, tm.statsFor("M")->minMs);
}

static void test_max_tracks_largest_sample() {
  TimingMonitor tm;
  tm.registerModule("M");
  tm.record("M", 10);
  tm.record("M", 99);
  tm.record("M", 50);
  TEST_ASSERT_EQUAL_UINT32(99, tm.statsFor("M")->maxMs);
  TEST_ASSERT_EQUAL_UINT32(99, tm.wcetMs("M"));
}

static void test_avg_ms_correct() {
  TimingMonitor tm;
  tm.registerModule("M");
  tm.record("M", 10);
  tm.record("M", 20);
  tm.record("M", 30);
  // sum = 60, count = 3, avg = 20.
  TEST_ASSERT_EQUAL_UINT32(20, tm.avgMs("M"));
}

static void test_stats_for_no_samples_returns_null() {
  TimingMonitor tm;
  tm.registerModule("M");
  TEST_ASSERT_NULL(tm.statsFor("M"));
}

static void test_stats_for_unknown_returns_null() {
  TimingMonitor tm;
  TEST_ASSERT_NULL(tm.statsFor("Unknown"));
}

static void test_avg_ms_no_samples_returns_zero() {
  TimingMonitor tm;
  tm.registerModule("M");
  TEST_ASSERT_EQUAL_UINT32(0, tm.avgMs("M"));
}

static void test_wcet_ms_no_samples_returns_zero() {
  TimingMonitor tm;
  tm.registerModule("M");
  TEST_ASSERT_EQUAL_UINT32(0, tm.wcetMs("M"));
}

// ── Budget enforcement ─────────────────────────────────────────────────────

static void test_not_over_budget_when_within_limit() {
  TimingMonitor tm;
  tm.registerModule("M", 100);
  tm.record("M", 50);
  TEST_ASSERT_FALSE(tm.isOverBudget("M"));
}

static void test_over_budget_when_wcet_exceeds_limit() {
  TimingMonitor tm;
  tm.registerModule("M", 100);
  tm.record("M", 101);
  TEST_ASSERT_TRUE(tm.isOverBudget("M"));
}

static void test_zero_budget_never_over() {
  TimingMonitor tm;
  tm.registerModule("M", 0);
  tm.record("M", 99999);
  TEST_ASSERT_FALSE(tm.isOverBudget("M"));
}

static void test_exactly_at_budget_not_over() {
  TimingMonitor tm;
  tm.registerModule("M", 100);
  tm.record("M", 100);
  TEST_ASSERT_FALSE(tm.isOverBudget("M"));
}

static void test_violation_count_increments() {
  TimingMonitor tm;
  tm.registerModule("M", 10);
  tm.record("M", 20);  // violation
  tm.record("M", 5);   // no violation
  tm.record("M", 15);  // violation
  TEST_ASSERT_EQUAL_UINT32(2, tm.statsFor("M")->violations);
}

// ── Reset ──────────────────────────────────────────────────────────────────

static void test_reset_clears_samples() {
  TimingMonitor tm;
  tm.registerModule("M");
  tm.record("M", 42);
  TEST_ASSERT_TRUE(tm.reset("M"));
  TEST_ASSERT_NULL(tm.statsFor("M"));
}

static void test_reset_unknown_returns_false() {
  TimingMonitor tm;
  TEST_ASSERT_FALSE(tm.reset("Unknown"));
}

static void test_reset_keeps_budget() {
  TimingMonitor tm;
  tm.registerModule("M", 50);
  tm.record("M", 100);  // over budget
  tm.reset("M");
  // After reset + new recording within budget: should not be over.
  tm.record("M", 10);
  TEST_ASSERT_FALSE(tm.isOverBudget("M"));
}

// ── is_over_budget edge cases ──────────────────────────────────────────────

static void test_is_over_budget_unknown_returns_false() {
  TimingMonitor tm;
  TEST_ASSERT_FALSE(tm.isOverBudget("Unknown"));
}

static void test_is_over_budget_no_samples_returns_false() {
  TimingMonitor tm;
  tm.registerModule("M", 10);
  TEST_ASSERT_FALSE(tm.isOverBudget("M"));
}

// ── Main ───────────────────────────────────────────────────────────────────

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_register_module_returns_true);
  RUN_TEST(test_register_null_name_rejected);
  RUN_TEST(test_register_duplicate_updates_budget);
  RUN_TEST(test_register_full_returns_false);

  RUN_TEST(test_record_returns_false_for_unknown_module);
  RUN_TEST(test_record_returns_true_for_registered);
  RUN_TEST(test_record_one_sample_stats);
  RUN_TEST(test_min_tracks_smallest_sample);
  RUN_TEST(test_max_tracks_largest_sample);
  RUN_TEST(test_avg_ms_correct);
  RUN_TEST(test_stats_for_no_samples_returns_null);
  RUN_TEST(test_stats_for_unknown_returns_null);
  RUN_TEST(test_avg_ms_no_samples_returns_zero);
  RUN_TEST(test_wcet_ms_no_samples_returns_zero);

  RUN_TEST(test_not_over_budget_when_within_limit);
  RUN_TEST(test_over_budget_when_wcet_exceeds_limit);
  RUN_TEST(test_zero_budget_never_over);
  RUN_TEST(test_exactly_at_budget_not_over);
  RUN_TEST(test_violation_count_increments);

  RUN_TEST(test_reset_clears_samples);
  RUN_TEST(test_reset_unknown_returns_false);
  RUN_TEST(test_reset_keeps_budget);

  RUN_TEST(test_is_over_budget_unknown_returns_false);
  RUN_TEST(test_is_over_budget_no_samples_returns_false);

  return UNITY_END();
}

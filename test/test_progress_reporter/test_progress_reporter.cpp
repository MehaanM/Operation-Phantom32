// test_progress_reporter.cpp — Tests for ProgressReporter (Phase 20).

#include "unity.h"
#include "core/ProgressReporter.h"

void setUp() {}
void tearDown() {}

// ── startOp ───────────────────────────────────────────────────────────────────

static void test_start_op_ok() {
  ProgressReporter pr;
  TEST_ASSERT_TRUE(pr.startOp("flash", 10));
  TEST_ASSERT_EQUAL(1, (int)pr.totalOpCount());
  TEST_ASSERT_TRUE(pr.isActive("flash"));
}

static void test_start_op_null_fails() {
  ProgressReporter pr;
  TEST_ASSERT_FALSE(pr.startOp(nullptr, 10));
}

static void test_start_op_empty_fails() {
  ProgressReporter pr;
  TEST_ASSERT_FALSE(pr.startOp("", 10));
}

static void test_start_op_resets_existing() {
  ProgressReporter pr;
  pr.startOp("flash", 10);
  pr.advanceStep("flash", "step1");
  TEST_ASSERT_TRUE(pr.startOp("flash", 20));
  TEST_ASSERT_EQUAL(0, (int)pr.percentComplete("flash"));
}

static void test_start_op_full_fails() {
  ProgressReporter pr;
  for (size_t i = 0; i < PHANTOM_PROG_MAX_OPS; ++i) {
    char name[8];
    name[0] = 'o';
    name[1] = (char)('0' + i);
    name[2] = '\0';
    pr.startOp(name, 5);
  }
  TEST_ASSERT_FALSE(pr.startOp("overflow", 5));
}

// ── advanceStep ───────────────────────────────────────────────────────────────

static void test_advance_step_ok() {
  ProgressReporter pr;
  pr.startOp("flash", 4);
  TEST_ASSERT_TRUE(pr.advanceStep("flash", "erasing"));
  TEST_ASSERT_EQUAL(25, (int)pr.percentComplete("flash"));
}

static void test_advance_step_null_op_fails() {
  ProgressReporter pr;
  TEST_ASSERT_FALSE(pr.advanceStep(nullptr, "msg"));
}

static void test_advance_step_unknown_fails() {
  ProgressReporter pr;
  TEST_ASSERT_FALSE(pr.advanceStep("missing", "msg"));
}

static void test_advance_step_capped_at_total() {
  ProgressReporter pr;
  pr.startOp("flash", 2);
  pr.advanceStep("flash", "s1");
  pr.advanceStep("flash", "s2");
  pr.advanceStep("flash", "s3"); // should be capped
  TEST_ASSERT_EQUAL(100, (int)pr.percentComplete("flash"));
}

static void test_advance_step_inactive_fails() {
  ProgressReporter pr;
  pr.startOp("flash", 4);
  pr.completeOp("flash");
  TEST_ASSERT_FALSE(pr.advanceStep("flash", "extra"));
}

// ── completeOp ────────────────────────────────────────────────────────────────

static void test_complete_op_ok() {
  ProgressReporter pr;
  pr.startOp("flash", 10);
  pr.completeOp("flash");
  TEST_ASSERT_FALSE(pr.isActive("flash"));
  TEST_ASSERT_TRUE(pr.isComplete("flash"));
  TEST_ASSERT_EQUAL(100, (int)pr.percentComplete("flash"));
}

static void test_complete_op_null_safe() {
  ProgressReporter pr;
  pr.completeOp(nullptr); // must not crash
}

// ── percentComplete ───────────────────────────────────────────────────────────

static void test_percent_complete_zero_total_steps() {
  ProgressReporter pr;
  pr.startOp("boot", 0);
  TEST_ASSERT_EQUAL(100, (int)pr.percentComplete("boot"));
}

static void test_percent_complete_null_zero() {
  ProgressReporter pr;
  TEST_ASSERT_EQUAL(0, (int)pr.percentComplete(nullptr));
}

static void test_percent_complete_unknown_zero() {
  ProgressReporter pr;
  TEST_ASSERT_EQUAL(0, (int)pr.percentComplete("missing"));
}

// ── isActive / isComplete / opFor / activeCount ───────────────────────────────

static void test_is_active_false_before_start() {
  ProgressReporter pr;
  TEST_ASSERT_FALSE(pr.isActive("flash"));
}

static void test_is_complete_false_before_complete() {
  ProgressReporter pr;
  pr.startOp("flash", 4);
  TEST_ASSERT_FALSE(pr.isComplete("flash"));
}

static void test_active_count() {
  ProgressReporter pr;
  pr.startOp("op1", 10);
  pr.startOp("op2", 5);
  pr.completeOp("op1");
  TEST_ASSERT_EQUAL(1, (int)pr.activeCount());
}

static void test_op_for_null_returns_null() {
  ProgressReporter pr;
  TEST_ASSERT_NULL(pr.opFor(nullptr));
}

static void test_op_for_unknown_returns_null() {
  ProgressReporter pr;
  TEST_ASSERT_NULL(pr.opFor("missing"));
}

static void test_op_for_ok() {
  ProgressReporter pr;
  pr.startOp("boot", 8);
  const ProgressOp *op = pr.opFor("boot");
  TEST_ASSERT_NOT_NULL(op);
  TEST_ASSERT_EQUAL_UINT32(8, op->totalSteps);
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_start_op_ok);
  RUN_TEST(test_start_op_null_fails);
  RUN_TEST(test_start_op_empty_fails);
  RUN_TEST(test_start_op_resets_existing);
  RUN_TEST(test_start_op_full_fails);

  RUN_TEST(test_advance_step_ok);
  RUN_TEST(test_advance_step_null_op_fails);
  RUN_TEST(test_advance_step_unknown_fails);
  RUN_TEST(test_advance_step_capped_at_total);
  RUN_TEST(test_advance_step_inactive_fails);

  RUN_TEST(test_complete_op_ok);
  RUN_TEST(test_complete_op_null_safe);

  RUN_TEST(test_percent_complete_zero_total_steps);
  RUN_TEST(test_percent_complete_null_zero);
  RUN_TEST(test_percent_complete_unknown_zero);

  RUN_TEST(test_is_active_false_before_start);
  RUN_TEST(test_is_complete_false_before_complete);
  RUN_TEST(test_active_count);
  RUN_TEST(test_op_for_null_returns_null);
  RUN_TEST(test_op_for_unknown_returns_null);
  RUN_TEST(test_op_for_ok);

  return UNITY_END();
}

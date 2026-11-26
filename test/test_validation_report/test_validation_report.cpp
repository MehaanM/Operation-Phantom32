// test_validation_report.cpp — Tests for ValidationReport (Phase 2).

#ifndef PHANTOM32_NATIVE_TEST
#define PHANTOM32_NATIVE_TEST
#endif
#include "unity.h"
#include "core/ValidationReport.h"
#include <string.h>

void setUp() {}
void tearDown() {}

// ── Basic registration ─────────────────────────────────────────────────────

static void test_add_returns_true_for_valid_check() {
  ValidationReport rpt;
  TEST_ASSERT_TRUE(rpt.add("Cat", "Name", [] { return true; }));
}

static void test_add_null_category_rejected() {
  ValidationReport rpt;
  TEST_ASSERT_FALSE(rpt.add(nullptr, "Name", [] { return true; }));
}

static void test_add_null_name_rejected() {
  ValidationReport rpt;
  TEST_ASSERT_FALSE(rpt.add("Cat", nullptr, [] { return true; }));
}

static void test_add_null_fn_rejected() {
  ValidationReport rpt;
  TEST_ASSERT_FALSE(rpt.add("Cat", "Name", nullptr));
}

static void test_check_count_increments() {
  ValidationReport rpt;
  rpt.add("Cat", "A", [] { return true; });
  rpt.add("Cat", "B", [] { return false; });
  TEST_ASSERT_EQUAL(2, (int)rpt.checkCount());
}

static void test_add_skip_registers_permanently_skipped() {
  ValidationReport rpt;
  TEST_ASSERT_TRUE(rpt.addSkip("Cat", "HWTest"));
  TEST_ASSERT_EQUAL(1, (int)rpt.checkCount());
}

static void test_registry_full_returns_false() {
  ValidationReport rpt;
  for (size_t i = 0; i < PHANTOM_VR_MAX_CHECKS; ++i) {
    char name[8];
    name[0] = (char)('A' + (i % 26));
    name[1] = (char)('0' + (i / 26));
    name[2] = '\0';
    rpt.add("C", name, [] { return true; });
  }
  TEST_ASSERT_FALSE(rpt.add("C", "overflow", [] { return true; }));
}

// ── Run results ────────────────────────────────────────────────────────────

static void test_run_all_pass_returns_true() {
  ValidationReport rpt;
  rpt.add("Cat", "A", [] { return true; });
  rpt.add("Cat", "B", [] { return true; });
  TEST_ASSERT_TRUE(rpt.run(nullptr));
}

static void test_run_one_fail_returns_false() {
  ValidationReport rpt;
  rpt.add("Cat", "A", [] { return true; });
  rpt.add("Cat", "B", [] { return false; });
  TEST_ASSERT_FALSE(rpt.run(nullptr));
}

static void test_run_empty_report_returns_false() {
  // No checks registered → not qualified.
  ValidationReport rpt;
  TEST_ASSERT_FALSE(rpt.run(nullptr));
}

static void test_pass_count_after_run() {
  ValidationReport rpt;
  rpt.add("C", "P1", [] { return true; });
  rpt.add("C", "P2", [] { return true; });
  rpt.add("C", "F1", [] { return false; });
  rpt.run(nullptr);
  TEST_ASSERT_EQUAL(2, (int)rpt.passCount());
}

static void test_fail_count_after_run() {
  ValidationReport rpt;
  rpt.add("C", "P1", [] { return true; });
  rpt.add("C", "F1", [] { return false; });
  rpt.add("C", "F2", [] { return false; });
  rpt.run(nullptr);
  TEST_ASSERT_EQUAL(2, (int)rpt.failCount());
}

static void test_skip_count_for_added_skips() {
  ValidationReport rpt;
  rpt.add("C", "Pass", [] { return true; });
  rpt.addSkip("C", "Skip1");
  rpt.addSkip("C", "Skip2");
  rpt.run(nullptr);
  TEST_ASSERT_EQUAL(2, (int)rpt.skipCount());
}

static void test_result_of_pass_check() {
  ValidationReport rpt;
  rpt.add("C", "MyCheck", [] { return true; });
  rpt.run(nullptr);
  TEST_ASSERT_EQUAL(ValidationReport::Result::PASS, rpt.resultOf("MyCheck"));
}

static void test_result_of_fail_check() {
  ValidationReport rpt;
  rpt.add("C", "MyFail", [] { return false; });
  rpt.run(nullptr);
  TEST_ASSERT_EQUAL(ValidationReport::Result::FAIL, rpt.resultOf("MyFail"));
}

static void test_result_of_skip_check() {
  ValidationReport rpt;
  rpt.addSkip("C", "MySkip");
  rpt.run(nullptr);
  TEST_ASSERT_EQUAL(ValidationReport::Result::SKIP, rpt.resultOf("MySkip"));
}

static void test_result_of_unknown_name_returns_skip() {
  ValidationReport rpt;
  rpt.run(nullptr);
  TEST_ASSERT_EQUAL(ValidationReport::Result::SKIP, rpt.resultOf("NonExistent"));
}

static void test_result_of_null_name_returns_skip() {
  ValidationReport rpt;
  rpt.run(nullptr);
  TEST_ASSERT_EQUAL(ValidationReport::Result::SKIP, rpt.resultOf(nullptr));
}

// ── check_at accessor ──────────────────────────────────────────────────────

static void test_check_at_valid_index() {
  ValidationReport rpt;
  rpt.add("Arch", "CycleDetect", [] { return true; });
  rpt.run(nullptr);
  const ValidationReport::Check *c = rpt.checkAt(0);
  TEST_ASSERT_NOT_NULL(c);
  TEST_ASSERT_EQUAL_STRING("Arch", c->category);
}

static void test_check_at_out_of_bounds_returns_null() {
  ValidationReport rpt;
  TEST_ASSERT_NULL(rpt.checkAt(0));
}

// ── Run output to FILE ─────────────────────────────────────────────────────

static void test_run_prints_to_file() {
  ValidationReport rpt;
  rpt.add("Architecture", "Module dependency validation", [] { return true; });
  rpt.add("Memory", "Leak detection", [] { return true; });
  rpt.addSkip("Reliability", "Hardware long-duration test");
  char buf[1024] = {};
  FILE *f = fmemopen(buf, sizeof(buf) - 1, "w");
  TEST_ASSERT_NOT_NULL(f);
  rpt.run(f);
  fclose(f);
  TEST_ASSERT_NOT_NULL(strstr(buf, "PHANTOM32 ENGINEERING VALIDATION"));
  TEST_ASSERT_NOT_NULL(strstr(buf, "PASS"));
  TEST_ASSERT_NOT_NULL(strstr(buf, "SKIP"));
}

static void test_run_qualified_string_in_output() {
  ValidationReport rpt;
  rpt.add("C", "A", [] { return true; });
  char buf[512] = {};
  FILE *f = fmemopen(buf, sizeof(buf) - 1, "w");
  rpt.run(f);
  fclose(f);
  TEST_ASSERT_NOT_NULL(strstr(buf, "RELEASE QUALIFIED"));
}

static void test_run_fail_string_in_output() {
  ValidationReport rpt;
  rpt.add("C", "A", [] { return false; });
  char buf[512] = {};
  FILE *f = fmemopen(buf, sizeof(buf) - 1, "w");
  rpt.run(f);
  fclose(f);
  TEST_ASSERT_NOT_NULL(strstr(buf, "FAIL"));
}

// ── Realistic engineering validation scenario ──────────────────────────────

static void test_full_engineering_validation_scenario() {
  ValidationReport rpt;
  // Architecture category.
  rpt.add("Architecture", "Module dependency validation", [] { return true; });
  rpt.add("Architecture", "Circular dependency detection", [] { return true; });
  rpt.add("Architecture", "Lifecycle contracts", [] { return true; });
  // Memory category.
  rpt.add("Memory", "Leak detection", [] { return true; });
  rpt.add("Memory", "Allocation failure testing", [] { return true; });
  rpt.add("Memory", "Resource budget enforcement", [] { return true; });
  // Reliability.
  rpt.add("Reliability", "Recovery tests", [] { return true; });
  rpt.add("Reliability", "Fault injection", [] { return true; });
  rpt.addSkip("Reliability", "Long-duration simulation");
  // Compatibility.
  rpt.add("Compatibility", "HAL contract tests", [] { return true; });
  rpt.add("Compatibility", "Board compilation", [] { return true; });
  rpt.addSkip("Compatibility", "Physical hardware tests");

  TEST_ASSERT_TRUE(rpt.run(nullptr));
  TEST_ASSERT_EQUAL(10, (int)rpt.passCount());
  TEST_ASSERT_EQUAL(0, (int)rpt.failCount());
  TEST_ASSERT_EQUAL(2, (int)rpt.skipCount());
}

// ── Main ───────────────────────────────────────────────────────────────────

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_add_returns_true_for_valid_check);
  RUN_TEST(test_add_null_category_rejected);
  RUN_TEST(test_add_null_name_rejected);
  RUN_TEST(test_add_null_fn_rejected);
  RUN_TEST(test_check_count_increments);
  RUN_TEST(test_add_skip_registers_permanently_skipped);
  RUN_TEST(test_registry_full_returns_false);

  RUN_TEST(test_run_all_pass_returns_true);
  RUN_TEST(test_run_one_fail_returns_false);
  RUN_TEST(test_run_empty_report_returns_false);
  RUN_TEST(test_pass_count_after_run);
  RUN_TEST(test_fail_count_after_run);
  RUN_TEST(test_skip_count_for_added_skips);

  RUN_TEST(test_result_of_pass_check);
  RUN_TEST(test_result_of_fail_check);
  RUN_TEST(test_result_of_skip_check);
  RUN_TEST(test_result_of_unknown_name_returns_skip);
  RUN_TEST(test_result_of_null_name_returns_skip);

  RUN_TEST(test_check_at_valid_index);
  RUN_TEST(test_check_at_out_of_bounds_returns_null);

  RUN_TEST(test_run_prints_to_file);
  RUN_TEST(test_run_qualified_string_in_output);
  RUN_TEST(test_run_fail_string_in_output);

  RUN_TEST(test_full_engineering_validation_scenario);

  return UNITY_END();
}

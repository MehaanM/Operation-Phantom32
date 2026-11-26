// test_phantom_doctor.cpp — Tests for PhantomDoctor (Phase 19).

#include "unity.h"
#include "core/PhantomDoctor.h"

void setUp() {}
void tearDown() {}

// ── registerCheck ─────────────────────────────────────────────────────────────

static void test_register_check_ok() {
  PhantomDoctor pd;
  TEST_ASSERT_TRUE(pd.registerCheck("BoardID", "Board identification"));
  TEST_ASSERT_EQUAL(1, (int)pd.checkCount());
}

static void test_register_null_fails() {
  PhantomDoctor pd;
  TEST_ASSERT_FALSE(pd.registerCheck(nullptr, "desc"));
}

static void test_register_empty_fails() {
  PhantomDoctor pd;
  TEST_ASSERT_FALSE(pd.registerCheck("", "desc"));
}

static void test_register_duplicate_ok() {
  PhantomDoctor pd;
  pd.registerCheck("BoardID", "desc");
  TEST_ASSERT_TRUE(pd.registerCheck("BoardID", "other desc"));
  TEST_ASSERT_EQUAL(1, (int)pd.checkCount());
}

static void test_register_full_fails() {
  PhantomDoctor pd;
  for (size_t i = 0; i < PHANTOM_PD_MAX_CHECKS; ++i) {
    char name[8];
    name[0] = 'C';
    name[1] = (char)('0' + i % 10);
    name[2] = (char)('a' + i / 10);
    name[3] = '\0';
    pd.registerCheck(name, "");
  }
  TEST_ASSERT_FALSE(pd.registerCheck("overflow", ""));
}

// ── setCheckResult ────────────────────────────────────────────────────────────

static void test_set_check_result_pass() {
  PhantomDoctor pd;
  pd.registerCheck("Memory", "Memory check");
  TEST_ASSERT_TRUE(pd.setCheckResult("Memory", CheckStatus::PASS, "OK"));
  const DiagnosticCheck *c = pd.checkFor("Memory");
  TEST_ASSERT_NOT_NULL(c);
  TEST_ASSERT_EQUAL((int)CheckStatus::PASS, (int)c->status);
  TEST_ASSERT_EQUAL_STRING("OK", c->detail);
}

static void test_set_check_result_fail() {
  PhantomDoctor pd;
  pd.registerCheck("Storage", "Storage check");
  TEST_ASSERT_TRUE(pd.setCheckResult("Storage", CheckStatus::FAIL, "not found"));
  TEST_ASSERT_EQUAL(1, (int)pd.failCount());
}

static void test_set_check_result_null_fails() {
  PhantomDoctor pd;
  TEST_ASSERT_FALSE(pd.setCheckResult(nullptr, CheckStatus::PASS, ""));
}

static void test_set_check_result_unknown_fails() {
  PhantomDoctor pd;
  TEST_ASSERT_FALSE(pd.setCheckResult("missing", CheckStatus::PASS, ""));
}

// ── addWarning ────────────────────────────────────────────────────────────────

static void test_add_warning_ok() {
  PhantomDoctor pd;
  TEST_ASSERT_TRUE(pd.addWarning("Display module unavailable"));
  TEST_ASSERT_EQUAL(1, (int)pd.warningCount());
}

static void test_add_warning_null_fails() {
  PhantomDoctor pd;
  TEST_ASSERT_FALSE(pd.addWarning(nullptr));
}

static void test_add_warning_empty_fails() {
  PhantomDoctor pd;
  TEST_ASSERT_FALSE(pd.addWarning(""));
}

// ── finalise / allPassed ──────────────────────────────────────────────────────

static void test_finalise_ok() {
  PhantomDoctor pd;
  TEST_ASSERT_TRUE(pd.finalise(5000));
  TEST_ASSERT_TRUE(pd.isComplete());
}

static void test_finalise_twice_fails() {
  PhantomDoctor pd;
  pd.finalise(1000);
  TEST_ASSERT_FALSE(pd.finalise(2000));
}

static void test_all_passed_no_failures() {
  PhantomDoctor pd;
  pd.registerCheck("BoardID", "");
  pd.registerCheck("Memory", "");
  pd.setCheckResult("BoardID", CheckStatus::PASS, "");
  pd.setCheckResult("Memory", CheckStatus::PASS, "");
  TEST_ASSERT_TRUE(pd.allPassed());
}

static void test_all_passed_with_failure() {
  PhantomDoctor pd;
  pd.registerCheck("Storage", "");
  pd.setCheckResult("Storage", CheckStatus::FAIL, "");
  TEST_ASSERT_FALSE(pd.allPassed());
}

// ── counts ────────────────────────────────────────────────────────────────────

static void test_pass_warn_fail_counts() {
  PhantomDoctor pd;
  pd.registerCheck("a", "");
  pd.registerCheck("b", "");
  pd.registerCheck("c", "");
  pd.setCheckResult("a", CheckStatus::PASS, "");
  pd.setCheckResult("b", CheckStatus::WARN, "");
  pd.setCheckResult("c", CheckStatus::FAIL, "");
  TEST_ASSERT_EQUAL(1, (int)pd.passCount());
  TEST_ASSERT_EQUAL(1, (int)pd.warnCount());
  TEST_ASSERT_EQUAL(1, (int)pd.failCount());
}

// ── checkFor / generateReport ─────────────────────────────────────────────────

static void test_check_for_null_returns_null() {
  PhantomDoctor pd;
  TEST_ASSERT_NULL(pd.checkFor(nullptr));
}

static void test_check_for_unknown_returns_null() {
  PhantomDoctor pd;
  TEST_ASSERT_NULL(pd.checkFor("missing"));
}

static void test_generate_report_null_safe() {
  PhantomDoctor pd;
  pd.generateReport(nullptr); // must not crash
}

static void test_generate_report_produces_output() {
  PhantomDoctor pd;
  pd.registerCheck("BoardID", "Board ID check");
  pd.setCheckResult("BoardID", CheckStatus::PASS, "ESP32");
  pd.addWarning("Optional storage not detected");
  pd.finalise(9000);
  char buf[1024];
  FILE *f = fmemopen(buf, sizeof(buf), "w");
  TEST_ASSERT_NOT_NULL(f);
  pd.generateReport(f);
  fclose(f);
  TEST_ASSERT_NOT_NULL(strstr(buf, "BoardID"));
  TEST_ASSERT_NOT_NULL(strstr(buf, "PASS"));
  TEST_ASSERT_NOT_NULL(strstr(buf, "Optional storage"));
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_register_check_ok);
  RUN_TEST(test_register_null_fails);
  RUN_TEST(test_register_empty_fails);
  RUN_TEST(test_register_duplicate_ok);
  RUN_TEST(test_register_full_fails);

  RUN_TEST(test_set_check_result_pass);
  RUN_TEST(test_set_check_result_fail);
  RUN_TEST(test_set_check_result_null_fails);
  RUN_TEST(test_set_check_result_unknown_fails);

  RUN_TEST(test_add_warning_ok);
  RUN_TEST(test_add_warning_null_fails);
  RUN_TEST(test_add_warning_empty_fails);

  RUN_TEST(test_finalise_ok);
  RUN_TEST(test_finalise_twice_fails);
  RUN_TEST(test_all_passed_no_failures);
  RUN_TEST(test_all_passed_with_failure);

  RUN_TEST(test_pass_warn_fail_counts);

  RUN_TEST(test_check_for_null_returns_null);
  RUN_TEST(test_check_for_unknown_returns_null);
  RUN_TEST(test_generate_report_null_safe);
  RUN_TEST(test_generate_report_produces_output);

  return UNITY_END();
}

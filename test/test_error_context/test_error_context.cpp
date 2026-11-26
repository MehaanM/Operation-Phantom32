// test_error_context.cpp — Tests for ErrorContext (Phase 20).

#include "unity.h"
#include "core/ErrorContext.h"

void setUp() {}
void tearDown() {}

// ── registerCode ──────────────────────────────────────────────────────────────

static void test_register_code_ok() {
  ErrorContext ec;
  TEST_ASSERT_TRUE(ec.registerCode(0x0001, "ERR_BOOT", "Boot failed",
                                   "Check power supply", ErrorSeverity::ERROR));
  TEST_ASSERT_EQUAL(1, (int)ec.registeredCount());
}

static void test_register_null_name_fails() {
  ErrorContext ec;
  TEST_ASSERT_FALSE(ec.registerCode(0x0001, nullptr, "desc", "hint",
                                    ErrorSeverity::WARNING));
}

static void test_register_updates_existing_code() {
  ErrorContext ec;
  ec.registerCode(0x0001, "ERR_A", "first", "h1", ErrorSeverity::INFO);
  TEST_ASSERT_TRUE(ec.registerCode(0x0001, "ERR_B", "second", "h2",
                                   ErrorSeverity::ERROR));
  TEST_ASSERT_EQUAL(1, (int)ec.registeredCount());
  const ErrorCode *c = ec.codeFor(0x0001);
  TEST_ASSERT_NOT_NULL(c);
  TEST_ASSERT_EQUAL_STRING("ERR_B", c->name);
}

static void test_register_full_still_ok_for_existing() {
  ErrorContext ec;
  for (size_t i = 0; i < PHANTOM_EC_MAX_CODES; ++i) {
    ec.registerCode((uint16_t)(i + 1), "NAME", "d", "h", ErrorSeverity::INFO);
  }
  // Updating existing code = ok even when full
  TEST_ASSERT_TRUE(ec.registerCode(1, "NAME2", "d", "h", ErrorSeverity::INFO));
}

// ── push / pop ────────────────────────────────────────────────────────────────

static void test_push_ok() {
  ErrorContext ec;
  ec.registerCode(0x0001, "ERR_BOOT", "Boot failed", "hint", ErrorSeverity::ERROR);
  TEST_ASSERT_TRUE(ec.push(0x0001, "boot_sequence", 1000));
  TEST_ASSERT_EQUAL(1, (int)ec.activeCount());
}

static void test_push_unknown_code_fails() {
  ErrorContext ec;
  TEST_ASSERT_FALSE(ec.push(0x9999, "ctx", 1000));
}

static void test_push_max_active_fails() {
  ErrorContext ec;
  for (size_t i = 0; i < PHANTOM_EC_MAX_ACTIVE; ++i) {
    ec.registerCode((uint16_t)(i + 1), "NAME", "d", "h", ErrorSeverity::INFO);
    ec.push((uint16_t)(i + 1), "ctx", (uint64_t)i);
  }
  ec.registerCode(0x0100, "EXTRA", "d", "h", ErrorSeverity::INFO);
  TEST_ASSERT_FALSE(ec.push(0x0100, "ctx", 999));
}

static void test_pop_removes_first_match() {
  ErrorContext ec;
  ec.registerCode(0x0001, "ERR_A", "d", "h", ErrorSeverity::ERROR);
  ec.push(0x0001, "ctx1", 1000);
  ec.push(0x0001, "ctx2", 2000);
  TEST_ASSERT_TRUE(ec.pop(0x0001));
  TEST_ASSERT_EQUAL(1, (int)ec.activeCount());
}

static void test_pop_unknown_fails() {
  ErrorContext ec;
  TEST_ASSERT_FALSE(ec.pop(0x9999));
}

// ── hasActive / activeCount / registeredCount ─────────────────────────────────

static void test_has_active_true() {
  ErrorContext ec;
  ec.registerCode(0x0001, "ERR_A", "d", "h", ErrorSeverity::WARNING);
  ec.push(0x0001, "ctx", 1000);
  TEST_ASSERT_TRUE(ec.hasActive(0x0001));
}

static void test_has_active_false() {
  ErrorContext ec;
  ec.registerCode(0x0001, "ERR_A", "d", "h", ErrorSeverity::WARNING);
  TEST_ASSERT_FALSE(ec.hasActive(0x0001));
}

// ── codeFor / activeAt ────────────────────────────────────────────────────────

static void test_code_for_null_code_zero() {
  ErrorContext ec;
  TEST_ASSERT_NULL(ec.codeFor(0x0000));
}

static void test_code_for_unknown_null() {
  ErrorContext ec;
  TEST_ASSERT_NULL(ec.codeFor(0xFFFF));
}

static void test_active_at_ok() {
  ErrorContext ec;
  ec.registerCode(0x0002, "ERR_B", "d", "h", ErrorSeverity::CRITICAL);
  ec.push(0x0002, "myctx", 5000);
  const ActiveError *a = ec.activeAt(0);
  TEST_ASSERT_NOT_NULL(a);
  TEST_ASSERT_EQUAL(0x0002, a->code);
}

static void test_active_at_out_of_range_null() {
  ErrorContext ec;
  TEST_ASSERT_NULL(ec.activeAt(0));
}

// ── generateReport ────────────────────────────────────────────────────────────

static void test_generate_report_null_safe() {
  ErrorContext ec;
  ec.generateReport(nullptr);
}

static void test_generate_report_produces_output() {
  ErrorContext ec;
  ec.registerCode(0x0001, "ERR_BOOT", "Boot failed", "Check power", ErrorSeverity::ERROR);
  ec.push(0x0001, "boot_seq", 1000);
  char buf[512];
  FILE *f = fmemopen(buf, sizeof(buf), "w");
  TEST_ASSERT_NOT_NULL(f);
  ec.generateReport(f);
  fclose(f);
  TEST_ASSERT_NOT_NULL(strstr(buf, "ERR_BOOT"));
  TEST_ASSERT_NOT_NULL(strstr(buf, "Check power"));
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_register_code_ok);
  RUN_TEST(test_register_null_name_fails);
  RUN_TEST(test_register_updates_existing_code);
  RUN_TEST(test_register_full_still_ok_for_existing);

  RUN_TEST(test_push_ok);
  RUN_TEST(test_push_unknown_code_fails);
  RUN_TEST(test_push_max_active_fails);
  RUN_TEST(test_pop_removes_first_match);
  RUN_TEST(test_pop_unknown_fails);

  RUN_TEST(test_has_active_true);
  RUN_TEST(test_has_active_false);

  RUN_TEST(test_code_for_null_code_zero);
  RUN_TEST(test_code_for_unknown_null);
  RUN_TEST(test_active_at_ok);
  RUN_TEST(test_active_at_out_of_range_null);

  RUN_TEST(test_generate_report_null_safe);
  RUN_TEST(test_generate_report_produces_output);

  return UNITY_END();
}

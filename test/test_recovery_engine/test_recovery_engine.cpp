// test_recovery_engine.cpp — Tests for RecoveryEngine (Phase 19).

#include "unity.h"
#include "core/RecoveryEngine.h"

void setUp() {}
void tearDown() {}

// ── recordFailure / currentState ──────────────────────────────────────────────

static void test_initial_state_normal() {
  RecoveryEngine re;
  TEST_ASSERT_EQUAL((int)RecoveryState::NORMAL, (int)re.currentState());
}

static void test_low_failure_degraded() {
  RecoveryEngine re;
  re.recordFailure("wifi", "scan slow", FailureSeverity::LOW, true, 1000);
  TEST_ASSERT_EQUAL((int)RecoveryState::DEGRADED, (int)re.currentState());
}

static void test_medium_failure_degraded() {
  RecoveryEngine re;
  re.recordFailure("ble", "init failed", FailureSeverity::MEDIUM, true, 1000);
  TEST_ASSERT_EQUAL((int)RecoveryState::DEGRADED, (int)re.currentState());
}

static void test_high_failure_safe_mode() {
  RecoveryEngine re;
  re.recordFailure("storage", "corrupt", FailureSeverity::HIGH, false, 1000);
  TEST_ASSERT_EQUAL((int)RecoveryState::SAFE_MODE, (int)re.currentState());
}

static void test_critical_failure_failed_state() {
  RecoveryEngine re;
  re.recordFailure("watchdog", "reset", FailureSeverity::CRITICAL, false, 1000);
  TEST_ASSERT_EQUAL((int)RecoveryState::FAILED, (int)re.currentState());
}

static void test_record_null_name_fails() {
  RecoveryEngine re;
  TEST_ASSERT_FALSE(re.recordFailure(nullptr, "desc", FailureSeverity::LOW, true, 0));
}

static void test_record_empty_name_fails() {
  RecoveryEngine re;
  TEST_ASSERT_FALSE(re.recordFailure("", "desc", FailureSeverity::LOW, true, 0));
}

static void test_record_full_fails() {
  RecoveryEngine re;
  for (size_t i = 0; i < PHANTOM_RE_MAX_FAILURES; ++i) {
    char name[8];
    name[0] = 'f';
    name[1] = (char)('0' + i % 10);
    name[2] = (char)('a' + i / 10);
    name[3] = '\0';
    re.recordFailure(name, "", FailureSeverity::LOW, true, 0);
  }
  TEST_ASSERT_FALSE(re.recordFailure("overflow", "", FailureSeverity::LOW, true, 0));
}

// ── attemptRecovery ───────────────────────────────────────────────────────────

static void test_attempt_recovery_resolves() {
  RecoveryEngine re;
  re.recordFailure("wifi", "fail", FailureSeverity::MEDIUM, true, 1000);
  TEST_ASSERT_TRUE(re.attemptRecovery("wifi", 2000));
  TEST_ASSERT_EQUAL(0, (int)re.activeFailureCount());
  TEST_ASSERT_EQUAL((int)RecoveryState::NORMAL, (int)re.currentState());
}

static void test_attempt_recovery_null_fails() {
  RecoveryEngine re;
  TEST_ASSERT_FALSE(re.attemptRecovery(nullptr, 1000));
}

static void test_attempt_recovery_unknown_fails() {
  RecoveryEngine re;
  TEST_ASSERT_FALSE(re.attemptRecovery("missing", 1000));
}

static void test_recovery_clears_safe_mode() {
  RecoveryEngine re;
  re.recordFailure("storage", "fail", FailureSeverity::HIGH, true, 1000);
  TEST_ASSERT_EQUAL((int)RecoveryState::SAFE_MODE, (int)re.currentState());
  re.attemptRecovery("storage", 2000);
  TEST_ASSERT_EQUAL((int)RecoveryState::NORMAL, (int)re.currentState());
}

// ── enterSafeMode ─────────────────────────────────────────────────────────────

static void test_enter_safe_mode_explicit() {
  RecoveryEngine re;
  re.enterSafeMode(1000);
  TEST_ASSERT_EQUAL((int)RecoveryState::SAFE_MODE, (int)re.currentState());
}

// ── hasRecoverable ────────────────────────────────────────────────────────────

static void test_has_recoverable_true() {
  RecoveryEngine re;
  re.recordFailure("wifi", "fail", FailureSeverity::LOW, true, 1000);
  TEST_ASSERT_TRUE(re.hasRecoverable());
}

static void test_has_recoverable_false() {
  RecoveryEngine re;
  re.recordFailure("storage", "fail", FailureSeverity::HIGH, false, 1000);
  TEST_ASSERT_FALSE(re.hasRecoverable());
}

// ── saveSnapshot / snapshotFor ────────────────────────────────────────────────

static void test_save_snapshot_ok() {
  RecoveryEngine re;
  re.recordFailure("wifi", "fail", FailureSeverity::LOW, true, 1000);
  TEST_ASSERT_TRUE(re.saveSnapshot("snap1", 1000));
  const DiagnosticSnapshot *s = re.snapshotFor("snap1");
  TEST_ASSERT_NOT_NULL(s);
  TEST_ASSERT_EQUAL(1, (int)s->activeFailures);
  TEST_ASSERT_EQUAL((int)RecoveryState::DEGRADED, (int)s->state);
}

static void test_save_snapshot_null_fails() {
  RecoveryEngine re;
  TEST_ASSERT_FALSE(re.saveSnapshot(nullptr, 1000));
}

static void test_snapshot_for_null_returns_null() {
  RecoveryEngine re;
  TEST_ASSERT_NULL(re.snapshotFor(nullptr));
}

static void test_snapshot_for_unknown_null() {
  RecoveryEngine re;
  TEST_ASSERT_NULL(re.snapshotFor("missing"));
}

// ── failureAt ─────────────────────────────────────────────────────────────────

static void test_failure_at_ok() {
  RecoveryEngine re;
  re.recordFailure("wifi", "scan slow", FailureSeverity::LOW, true, 1000);
  const FailureRecord *r = re.failureAt(0);
  TEST_ASSERT_NOT_NULL(r);
  TEST_ASSERT_EQUAL_STRING("wifi", r->name);
}

static void test_failure_at_out_of_range_null() {
  RecoveryEngine re;
  TEST_ASSERT_NULL(re.failureAt(0));
}

// ── registerHandler / handler callbacks ───────────────────────────────────────

static bool s_handler_called = false;
static bool handler_succeeds(const char * /*name*/, void * /*ctx*/) {
  s_handler_called = true;
  return true;
}
static bool handler_fails(const char * /*name*/, void * /*ctx*/) {
  s_handler_called = true;
  return false;
}

static void test_register_handler_ok() {
  RecoveryEngine re;
  TEST_ASSERT_TRUE(re.registerHandler("wifi", handler_succeeds));
  TEST_ASSERT_EQUAL(1, (int)re.handlerCount());
}

static void test_register_handler_null_name_fails() {
  RecoveryEngine re;
  TEST_ASSERT_FALSE(re.registerHandler(nullptr, handler_succeeds));
}

static void test_register_handler_null_fn_fails() {
  RecoveryEngine re;
  TEST_ASSERT_FALSE(re.registerHandler("wifi", nullptr));
}

static void test_register_handler_upserts() {
  RecoveryEngine re;
  re.registerHandler("wifi", handler_succeeds);
  TEST_ASSERT_TRUE(re.registerHandler("wifi", handler_fails));
  TEST_ASSERT_EQUAL(1, (int)re.handlerCount());
}

static void test_handler_invoked_on_recovery() {
  RecoveryEngine re;
  s_handler_called = false;
  re.registerHandler("wifi", handler_succeeds);
  re.recordFailure("wifi", "fail", FailureSeverity::LOW, true, 1000);
  re.attemptRecovery("wifi", 2000);
  TEST_ASSERT_TRUE(s_handler_called);
}

static void test_handler_failure_prevents_resolution() {
  RecoveryEngine re;
  re.registerHandler("wifi", handler_fails);
  re.recordFailure("wifi", "fail", FailureSeverity::LOW, true, 1000);
  TEST_ASSERT_FALSE(re.attemptRecovery("wifi", 2000));
  // Still active — not resolved
  TEST_ASSERT_EQUAL(1, (int)re.activeFailureCount());
}

static void test_no_handler_still_resolves() {
  RecoveryEngine re;
  // No handler registered — backward compat: resolve unconditionally
  re.recordFailure("wifi", "fail", FailureSeverity::LOW, true, 1000);
  TEST_ASSERT_TRUE(re.attemptRecovery("wifi", 2000));
  TEST_ASSERT_EQUAL(0, (int)re.activeFailureCount());
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_initial_state_normal);
  RUN_TEST(test_low_failure_degraded);
  RUN_TEST(test_medium_failure_degraded);
  RUN_TEST(test_high_failure_safe_mode);
  RUN_TEST(test_critical_failure_failed_state);
  RUN_TEST(test_record_null_name_fails);
  RUN_TEST(test_record_empty_name_fails);
  RUN_TEST(test_record_full_fails);

  RUN_TEST(test_attempt_recovery_resolves);
  RUN_TEST(test_attempt_recovery_null_fails);
  RUN_TEST(test_attempt_recovery_unknown_fails);
  RUN_TEST(test_recovery_clears_safe_mode);

  RUN_TEST(test_enter_safe_mode_explicit);

  RUN_TEST(test_has_recoverable_true);
  RUN_TEST(test_has_recoverable_false);

  RUN_TEST(test_save_snapshot_ok);
  RUN_TEST(test_save_snapshot_null_fails);
  RUN_TEST(test_snapshot_for_null_returns_null);
  RUN_TEST(test_snapshot_for_unknown_null);

  RUN_TEST(test_failure_at_ok);
  RUN_TEST(test_failure_at_out_of_range_null);

  RUN_TEST(test_register_handler_ok);
  RUN_TEST(test_register_handler_null_name_fails);
  RUN_TEST(test_register_handler_null_fn_fails);
  RUN_TEST(test_register_handler_upserts);
  RUN_TEST(test_handler_invoked_on_recovery);
  RUN_TEST(test_handler_failure_prevents_resolution);
  RUN_TEST(test_no_handler_still_resolves);

  return UNITY_END();
}

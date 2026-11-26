// test_fault_injector.cpp — Tests for FaultInjector (Phase 14).

#include "unity.h"
#include "core/FaultInjector.h"

void setUp() {}
void tearDown() {}

// ── addFault (immediate) ──────────────────────────────────────────────────────

static void test_add_immediate_fault_active() {
  FaultInjector fi;
  TEST_ASSERT_TRUE(fi.addFault(FaultType::FAILED_INIT, "i2c", "init failed"));
  TEST_ASSERT_EQUAL(1, (int)fi.totalFaultCount());
  TEST_ASSERT_EQUAL(1, (int)fi.activeFaultCount());
  const FaultEntry *f = fi.faultAt(0);
  TEST_ASSERT_NOT_NULL(f);
  TEST_ASSERT_EQUAL((int)FaultState::ACTIVE, (int)f->state);
}

static void test_add_null_target_fails() {
  FaultInjector fi;
  TEST_ASSERT_FALSE(fi.addFault(FaultType::FAILED_INIT, nullptr, "desc"));
}

// ── addFault (scheduled / pending) ───────────────────────────────────────────

static void test_add_scheduled_fault_pending() {
  FaultInjector fi;
  fi.addFault(FaultType::MISSING_PERIPHERAL, "spi", "not found", 5000);
  const FaultEntry *f = fi.faultAt(0);
  TEST_ASSERT_NOT_NULL(f);
  TEST_ASSERT_EQUAL((int)FaultState::PENDING, (int)f->state);
  TEST_ASSERT_EQUAL(0, (int)fi.activeFaultCount());
}

static void test_tick_activates_pending_at_scheduled_time() {
  FaultInjector fi;
  fi.addFault(FaultType::MISSING_PERIPHERAL, "spi", "not found", 5000);
  fi.tick(3000);
  TEST_ASSERT_EQUAL(0, (int)fi.activeFaultCount()); // not yet
  fi.tick(5000);
  TEST_ASSERT_EQUAL(1, (int)fi.activeFaultCount()); // now active
  TEST_ASSERT_TRUE(fi.hasFault("spi"));
}

static void test_tick_does_not_activate_before_time() {
  FaultInjector fi;
  fi.addFault(FaultType::MISSING_PERIPHERAL, "spi", "desc", 10000);
  fi.tick(1000);
  TEST_ASSERT_FALSE(fi.hasFault("spi"));
}

// ── auto-resolve ──────────────────────────────────────────────────────────────

static void test_auto_resolve_on_tick() {
  FaultInjector fi;
  fi.addFault(FaultType::DELAYED_HARDWARE, "uart", "slow", 0, 3000);
  TEST_ASSERT_TRUE(fi.hasFault("uart")); // immediately active
  fi.tick(3000);
  TEST_ASSERT_FALSE(fi.hasFault("uart")); // auto-resolved
  TEST_ASSERT_EQUAL(0, (int)fi.activeFaultCount());
}

static void test_auto_resolve_not_before_time() {
  FaultInjector fi;
  fi.addFault(FaultType::DELAYED_HARDWARE, "uart", "slow", 0, 5000);
  fi.tick(2000);
  TEST_ASSERT_TRUE(fi.hasFault("uart")); // still active
}

// ── hasFault / hasFaultOfType ─────────────────────────────────────────────────

static void test_has_fault_inactive_false() {
  FaultInjector fi;
  TEST_ASSERT_FALSE(fi.hasFault("any"));
}

static void test_has_fault_null_target() {
  FaultInjector fi;
  fi.addFault(FaultType::FAILED_INIT, "mod", "desc");
  TEST_ASSERT_FALSE(fi.hasFault(nullptr));
}

static void test_has_fault_of_type_true() {
  FaultInjector fi;
  fi.addFault(FaultType::WATCHDOG_RESET, "wdt", "reset");
  TEST_ASSERT_TRUE(fi.hasFaultOfType("wdt", FaultType::WATCHDOG_RESET));
}

static void test_has_fault_of_type_wrong_type() {
  FaultInjector fi;
  fi.addFault(FaultType::WATCHDOG_RESET, "wdt", "reset");
  TEST_ASSERT_FALSE(fi.hasFaultOfType("wdt", FaultType::FAILED_INIT));
}

static void test_has_fault_of_type_null_target() {
  FaultInjector fi;
  fi.addFault(FaultType::FAILED_INIT, "mod", "desc");
  TEST_ASSERT_FALSE(fi.hasFaultOfType(nullptr, FaultType::FAILED_INIT));
}

// ── resolveFault ──────────────────────────────────────────────────────────────

static void test_resolve_fault_manual() {
  FaultInjector fi;
  fi.addFault(FaultType::RESOURCE_EXHAUSTION, "heap", "oom");
  TEST_ASSERT_TRUE(fi.resolveFault("heap", FaultType::RESOURCE_EXHAUSTION, 1000));
  TEST_ASSERT_FALSE(fi.hasFault("heap"));
  TEST_ASSERT_EQUAL(0, (int)fi.activeFaultCount());
}

static void test_resolve_fault_not_found_false() {
  FaultInjector fi;
  TEST_ASSERT_FALSE(fi.resolveFault("missing", FaultType::FAILED_INIT, 1000));
}

static void test_resolve_fault_null_target_false() {
  FaultInjector fi;
  fi.addFault(FaultType::FAILED_INIT, "mod", "desc");
  TEST_ASSERT_FALSE(fi.resolveFault(nullptr, FaultType::FAILED_INIT, 1000));
}

// ── events ────────────────────────────────────────────────────────────────────

static void test_event_recorded_on_immediate_fault() {
  FaultInjector fi;
  fi.addFault(FaultType::CORRUPTED_CONFIG, "cfg", "bad");
  TEST_ASSERT_GREATER_THAN(0, (int)fi.eventCount());
  const FaultEvent *e = fi.eventAt(0);
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_FALSE(e->resolved);
}

static void test_event_recorded_on_resolve() {
  FaultInjector fi;
  fi.addFault(FaultType::CORRUPTED_CONFIG, "cfg", "bad");
  fi.resolveFault("cfg", FaultType::CORRUPTED_CONFIG, 1000);
  TEST_ASSERT_EQUAL(2, (int)fi.eventCount()); // fault + resolve
  const FaultEvent *e = fi.eventAt(1);
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_TRUE(e->resolved);
}

static void test_event_at_out_of_range_null() {
  FaultInjector fi;
  TEST_ASSERT_NULL(fi.eventAt(0));
}

// ── clear ─────────────────────────────────────────────────────────────────────

static void test_clear_resets_everything() {
  FaultInjector fi;
  fi.addFault(FaultType::FAILED_INIT, "mod", "desc");
  fi.clear();
  TEST_ASSERT_EQUAL(0, (int)fi.totalFaultCount());
  TEST_ASSERT_EQUAL(0, (int)fi.activeFaultCount());
  TEST_ASSERT_EQUAL(0, (int)fi.eventCount());
}

static void test_registry_full_fails() {
  FaultInjector fi;
  for (size_t i = 0; i < PHANTOM_FI_MAX_FAULTS; ++i) {
    char t[8];
    t[0] = (char)('a' + i % 26);
    t[1] = (char)('0' + i);
    t[2] = '\0';
    fi.addFault(FaultType::FAILED_INIT, t, "desc");
  }
  TEST_ASSERT_FALSE(fi.addFault(FaultType::FAILED_INIT, "overflow", "desc"));
}

static void test_fault_at_out_of_range_null() {
  FaultInjector fi;
  TEST_ASSERT_NULL(fi.faultAt(0));
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_add_immediate_fault_active);
  RUN_TEST(test_add_null_target_fails);

  RUN_TEST(test_add_scheduled_fault_pending);
  RUN_TEST(test_tick_activates_pending_at_scheduled_time);
  RUN_TEST(test_tick_does_not_activate_before_time);

  RUN_TEST(test_auto_resolve_on_tick);
  RUN_TEST(test_auto_resolve_not_before_time);

  RUN_TEST(test_has_fault_inactive_false);
  RUN_TEST(test_has_fault_null_target);
  RUN_TEST(test_has_fault_of_type_true);
  RUN_TEST(test_has_fault_of_type_wrong_type);
  RUN_TEST(test_has_fault_of_type_null_target);

  RUN_TEST(test_resolve_fault_manual);
  RUN_TEST(test_resolve_fault_not_found_false);
  RUN_TEST(test_resolve_fault_null_target_false);

  RUN_TEST(test_event_recorded_on_immediate_fault);
  RUN_TEST(test_event_recorded_on_resolve);
  RUN_TEST(test_event_at_out_of_range_null);

  RUN_TEST(test_clear_resets_everything);
  RUN_TEST(test_registry_full_fails);
  RUN_TEST(test_fault_at_out_of_range_null);

  return UNITY_END();
}

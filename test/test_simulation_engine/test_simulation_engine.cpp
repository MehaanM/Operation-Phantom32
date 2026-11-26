// test_simulation_engine.cpp — Tests for VirtualClock + SimulationEngine (Phase 16).

#include "unity.h"
#include "core/SimulationEngine.h"
#include "hal/VirtualClock.h"

void setUp() {}
void tearDown() {}

// ── VirtualClock ──────────────────────────────────────────────────────────────

static void test_clock_starts_at_zero() {
  VirtualClock vc;
  TEST_ASSERT_EQUAL_UINT64(0, vc.nowMs());
}

static void test_clock_advance() {
  VirtualClock vc;
  vc.advance(1000);
  vc.advance(500);
  TEST_ASSERT_EQUAL_UINT64(1500, vc.nowMs());
}

static void test_clock_set_abs_time() {
  VirtualClock vc;
  vc.set(99999);
  TEST_ASSERT_EQUAL_UINT64(99999, vc.nowMs());
}

static void test_clock_freeze_stops_advance() {
  VirtualClock vc;
  vc.advance(1000);
  vc.freeze();
  TEST_ASSERT_TRUE(vc.isFrozen());
  vc.advance(5000);
  TEST_ASSERT_EQUAL_UINT64(1000, vc.nowMs()); // did not advance
}

static void test_clock_resume_resumes_advance() {
  VirtualClock vc;
  vc.freeze();
  vc.advance(1000); // ignored
  vc.resume();
  TEST_ASSERT_FALSE(vc.isFrozen());
  vc.advance(500);
  TEST_ASSERT_EQUAL_UINT64(500, vc.nowMs());
}

static void test_clock_reset() {
  VirtualClock vc;
  vc.advance(10000);
  vc.freeze();
  vc.reset();
  TEST_ASSERT_EQUAL_UINT64(0, vc.nowMs());
  TEST_ASSERT_FALSE(vc.isFrozen());
}

// ── SimulationEngine — recordEvent ────────────────────────────────────────────

static void test_record_event_ok() {
  SimulationEngine se;
  TEST_ASSERT_TRUE(se.recordEvent(SimEventType::MODULE_START, "wifi", "starting", 1000));
  TEST_ASSERT_EQUAL(1, (int)se.eventCount());
}

static void test_record_event_null_target_ok() {
  SimulationEngine se;
  TEST_ASSERT_TRUE(se.recordEvent(SimEventType::CLOCK_ADVANCE, nullptr, nullptr, 500));
  TEST_ASSERT_EQUAL(1, (int)se.eventCount());
}

static void test_record_event_fields_stored() {
  SimulationEngine se;
  se.recordEvent(SimEventType::HARDWARE_FAULT, "i2c", "stuck", 2000);
  const SimEvent *e = se.eventAt(0);
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL((int)SimEventType::HARDWARE_FAULT, (int)e->type);
  TEST_ASSERT_EQUAL_UINT64(2000, e->timestampMs);
  TEST_ASSERT_EQUAL_STRING("i2c", e->target);
  TEST_ASSERT_EQUAL_STRING("stuck", e->payload);
}

static void test_event_at_out_of_range_null() {
  SimulationEngine se;
  TEST_ASSERT_NULL(se.eventAt(0));
}

// ── createScenario / addScenarioEvent ─────────────────────────────────────────

static void test_create_scenario_ok() {
  SimulationEngine se;
  TEST_ASSERT_TRUE(se.createScenario("power_loss", "simulates power loss", 10000));
  TEST_ASSERT_EQUAL(1, (int)se.scenarioCount());
}

static void test_create_scenario_null_fails() {
  SimulationEngine se;
  TEST_ASSERT_FALSE(se.createScenario(nullptr, "desc", 1000));
}

static void test_create_scenario_empty_fails() {
  SimulationEngine se;
  TEST_ASSERT_FALSE(se.createScenario("", "desc", 1000));
}

static void test_create_scenario_updates_existing() {
  SimulationEngine se;
  se.createScenario("s1", "first", 1000);
  TEST_ASSERT_TRUE(se.createScenario("s1", "updated", 2000));
  TEST_ASSERT_EQUAL(1, (int)se.scenarioCount());
  const SimScenario *s = se.scenarioFor("s1");
  TEST_ASSERT_NOT_NULL(s);
  TEST_ASSERT_EQUAL_UINT64(2000, s->durationMs);
}

static void test_create_scenario_full_fails() {
  SimulationEngine se;
  for (size_t i = 0; i < PHANTOM_SE_MAX_SCENARIOS; ++i) {
    char name[8];
    name[0] = (char)('a' + i);
    name[1] = '\0';
    se.createScenario(name, "desc", 1000);
  }
  TEST_ASSERT_FALSE(se.createScenario("overflow", "desc", 1000));
}

static void test_add_scenario_event_ok() {
  SimulationEngine se;
  se.createScenario("s1", "desc", 5000);
  TEST_ASSERT_TRUE(
      se.addScenarioEvent("s1", SimEventType::POWER_LOSS, "psu", "off", 2000));
  TEST_ASSERT_EQUAL(1, (int)se.scenarioFor("s1")->eventCount);
}

static void test_add_scenario_event_null_scenario_fails() {
  SimulationEngine se;
  TEST_ASSERT_FALSE(
      se.addScenarioEvent(nullptr, SimEventType::POWER_LOSS, "psu", "off", 2000));
}

// ── validateScenario ──────────────────────────────────────────────────────────

static void test_validate_valid_scenario() {
  SimulationEngine se;
  se.createScenario("s1", "desc", 5000);
  se.addScenarioEvent("s1", SimEventType::MODULE_START, "mod", "start", 1000);
  TEST_ASSERT_TRUE(se.validateScenario("s1"));
}

static void test_validate_empty_scenario_fails() {
  SimulationEngine se;
  se.createScenario("empty", "no events", 5000);
  TEST_ASSERT_FALSE(se.validateScenario("empty"));
}

static void test_validate_event_beyond_duration_fails() {
  SimulationEngine se;
  se.createScenario("s1", "desc", 1000);
  se.addScenarioEvent("s1", SimEventType::POWER_LOSS, "psu", "off", 5000); // relMs > durationMs
  TEST_ASSERT_FALSE(se.validateScenario("s1"));
}

static void test_validate_unknown_scenario_false() {
  SimulationEngine se;
  TEST_ASSERT_FALSE(se.validateScenario("missing"));
}

// ── replayScenario ────────────────────────────────────────────────────────────

static void test_replay_scenario_returns_event_count() {
  SimulationEngine se;
  se.createScenario("boot", "desc", 10000);
  se.addScenarioEvent("boot", SimEventType::MODULE_START, "wifi", "start", 500);
  se.addScenarioEvent("boot", SimEventType::MODULE_START, "ble", "start", 1000);
  size_t replayed = se.replayScenario("boot", 50000);
  TEST_ASSERT_EQUAL(2, (int)replayed);
}

static void test_replay_scenario_adds_to_event_log() {
  SimulationEngine se;
  se.createScenario("s1", "desc", 5000);
  se.addScenarioEvent("s1", SimEventType::NETWORK_SCAN, "wifi", "scan", 100);
  se.replayScenario("s1", 10000);
  TEST_ASSERT_EQUAL(1, (int)se.eventCount());
  // The event should have startMs (10000) + relMs (100) = 10100 as timestamp
  const SimEvent *e = se.eventAt(0);
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL_UINT64(10100, e->timestampMs);
  TEST_ASSERT_TRUE(e->processed);
}

static void test_replay_unknown_scenario_zero() {
  SimulationEngine se;
  TEST_ASSERT_EQUAL(0, (int)se.replayScenario("missing", 0));
}

// ── countEventsOfType ─────────────────────────────────────────────────────────

static void test_count_events_of_type() {
  SimulationEngine se;
  se.recordEvent(SimEventType::HARDWARE_FAULT, "uart", "err", 1000);
  se.recordEvent(SimEventType::HARDWARE_FAULT, "spi", "err", 2000);
  se.recordEvent(SimEventType::HARDWARE_RECOVERY, "uart", "ok", 3000);
  TEST_ASSERT_EQUAL(2, (int)se.countEventsOfType(SimEventType::HARDWARE_FAULT));
  TEST_ASSERT_EQUAL(1, (int)se.countEventsOfType(SimEventType::HARDWARE_RECOVERY));
  TEST_ASSERT_EQUAL(0, (int)se.countEventsOfType(SimEventType::USER_INPUT));
}

// ── snapshot ──────────────────────────────────────────────────────────────────

static void test_snapshot_ok() {
  SimulationEngine se;
  TEST_ASSERT_TRUE(se.snapshot("before_test", 5000));
  TEST_ASSERT_EQUAL(1, (int)se.snapshotCount());
}

static void test_snapshot_null_fails() {
  SimulationEngine se;
  TEST_ASSERT_FALSE(se.snapshot(nullptr, 1000));
}

static void test_snapshot_empty_name_fails() {
  SimulationEngine se;
  TEST_ASSERT_FALSE(se.snapshot("", 1000));
}

static void test_snapshot_updates_existing() {
  SimulationEngine se;
  se.snapshot("snap", 1000);
  TEST_ASSERT_TRUE(se.snapshot("snap", 2000));
  TEST_ASSERT_EQUAL(1, (int)se.snapshotCount());
}

// ── clearEvents ───────────────────────────────────────────────────────────────

static void test_clear_events() {
  SimulationEngine se;
  se.recordEvent(SimEventType::MODULE_STOP, "wifi", "down", 1000);
  se.clearEvents();
  TEST_ASSERT_EQUAL(0, (int)se.eventCount());
}

static void test_scenario_for_unknown_null() {
  SimulationEngine se;
  TEST_ASSERT_NULL(se.scenarioFor("missing"));
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_clock_starts_at_zero);
  RUN_TEST(test_clock_advance);
  RUN_TEST(test_clock_set_abs_time);
  RUN_TEST(test_clock_freeze_stops_advance);
  RUN_TEST(test_clock_resume_resumes_advance);
  RUN_TEST(test_clock_reset);

  RUN_TEST(test_record_event_ok);
  RUN_TEST(test_record_event_null_target_ok);
  RUN_TEST(test_record_event_fields_stored);
  RUN_TEST(test_event_at_out_of_range_null);

  RUN_TEST(test_create_scenario_ok);
  RUN_TEST(test_create_scenario_null_fails);
  RUN_TEST(test_create_scenario_empty_fails);
  RUN_TEST(test_create_scenario_updates_existing);
  RUN_TEST(test_create_scenario_full_fails);
  RUN_TEST(test_add_scenario_event_ok);
  RUN_TEST(test_add_scenario_event_null_scenario_fails);

  RUN_TEST(test_validate_valid_scenario);
  RUN_TEST(test_validate_empty_scenario_fails);
  RUN_TEST(test_validate_event_beyond_duration_fails);
  RUN_TEST(test_validate_unknown_scenario_false);

  RUN_TEST(test_replay_scenario_returns_event_count);
  RUN_TEST(test_replay_scenario_adds_to_event_log);
  RUN_TEST(test_replay_unknown_scenario_zero);

  RUN_TEST(test_count_events_of_type);

  RUN_TEST(test_snapshot_ok);
  RUN_TEST(test_snapshot_null_fails);
  RUN_TEST(test_snapshot_empty_name_fails);
  RUN_TEST(test_snapshot_updates_existing);

  RUN_TEST(test_clear_events);
  RUN_TEST(test_scenario_for_unknown_null);

  return UNITY_END();
}

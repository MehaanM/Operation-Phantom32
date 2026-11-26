// test_observability_dashboard.cpp — Tests for ObservabilityDashboard (Phase 19).

#include "unity.h"
#include "core/ObservabilityDashboard.h"

void setUp() {}
void tearDown() {}

// ── addEntry ──────────────────────────────────────────────────────────────────

static void test_add_entry_ok() {
  ObservabilityDashboard od;
  TEST_ASSERT_TRUE(od.addEntry(DashboardEntryType::LOG, EntryLevel::INFO,
                               "WiFi", "scan started", 1000));
  TEST_ASSERT_EQUAL(1, (int)od.entryCount());
}

static void test_add_entry_null_source_ok() {
  ObservabilityDashboard od;
  TEST_ASSERT_TRUE(od.addEntry(DashboardEntryType::DIAGNOSTIC, EntryLevel::DEBUG,
                               nullptr, "event", 500));
  TEST_ASSERT_EQUAL(1, (int)od.entryCount());
}

static void test_add_entry_null_message_ok() {
  ObservabilityDashboard od;
  TEST_ASSERT_TRUE(od.addEntry(DashboardEntryType::LOG, EntryLevel::WARN,
                               "src", nullptr, 500));
  TEST_ASSERT_EQUAL(1, (int)od.entryCount());
}

static void test_add_entry_ring_buffer() {
  ObservabilityDashboard od;
  for (size_t i = 0; i < PHANTOM_OD_MAX_ENTRIES + 5; ++i) {
    od.addEntry(DashboardEntryType::LOG, EntryLevel::INFO, "s", "m", (uint64_t)i);
  }
  // ring buffer wraps — count stays at MAX
  TEST_ASSERT_EQUAL((int)PHANTOM_OD_MAX_ENTRIES, (int)od.entryCount());
}

// ── updateModuleStatus ────────────────────────────────────────────────────────

static void test_update_module_status_ok() {
  ObservabilityDashboard od;
  TEST_ASSERT_TRUE(od.updateModuleStatus("WiFi", true, "ready", 1000));
  TEST_ASSERT_EQUAL(1, (int)od.moduleCount());
  const ModuleStatusEntry *m = od.moduleStatusFor("WiFi");
  TEST_ASSERT_NOT_NULL(m);
  TEST_ASSERT_TRUE(m->healthy);
}

static void test_update_module_status_null_fails() {
  ObservabilityDashboard od;
  TEST_ASSERT_FALSE(od.updateModuleStatus(nullptr, true, "ok", 1000));
}

static void test_update_module_status_upserts() {
  ObservabilityDashboard od;
  od.updateModuleStatus("BLE", true, "ok", 1000);
  TEST_ASSERT_TRUE(od.updateModuleStatus("BLE", false, "failed", 2000));
  TEST_ASSERT_EQUAL(1, (int)od.moduleCount());
  TEST_ASSERT_FALSE(od.moduleStatusFor("BLE")->healthy);
}

static void test_update_module_status_full_fails() {
  ObservabilityDashboard od;
  for (size_t i = 0; i < PHANTOM_OD_MAX_MODULES; ++i) {
    char name[8];
    name[0] = 'm';
    name[1] = (char)('0' + i % 10);
    name[2] = (char)('a' + i / 10);
    name[3] = '\0';
    od.updateModuleStatus(name, true, "ok", 0);
  }
  TEST_ASSERT_FALSE(od.updateModuleStatus("overflow", true, "ok", 0));
}

// ── countByLevel / countByType / hasErrors ────────────────────────────────────

static void test_count_by_level() {
  ObservabilityDashboard od;
  od.addEntry(DashboardEntryType::LOG, EntryLevel::INFO, "s", "m", 1000);
  od.addEntry(DashboardEntryType::LOG, EntryLevel::WARN, "s", "m", 2000);
  od.addEntry(DashboardEntryType::LOG, EntryLevel::ERROR, "s", "m", 3000);
  TEST_ASSERT_EQUAL(1, (int)od.countByLevel(EntryLevel::INFO));
  TEST_ASSERT_EQUAL(1, (int)od.countByLevel(EntryLevel::WARN));
  TEST_ASSERT_EQUAL(1, (int)od.countByLevel(EntryLevel::ERROR));
}

static void test_count_by_type() {
  ObservabilityDashboard od;
  od.addEntry(DashboardEntryType::LOG, EntryLevel::INFO, "s", "m", 1000);
  od.addEntry(DashboardEntryType::HEALTH_CHECK, EntryLevel::INFO, "s", "m", 2000);
  od.addEntry(DashboardEntryType::LOG, EntryLevel::INFO, "s", "m", 3000);
  TEST_ASSERT_EQUAL(2, (int)od.countByType(DashboardEntryType::LOG));
  TEST_ASSERT_EQUAL(1, (int)od.countByType(DashboardEntryType::HEALTH_CHECK));
}

static void test_has_errors_true() {
  ObservabilityDashboard od;
  od.addEntry(DashboardEntryType::DIAGNOSTIC, EntryLevel::ERROR, "s", "m", 1000);
  TEST_ASSERT_TRUE(od.hasErrors());
}

static void test_has_errors_false() {
  ObservabilityDashboard od;
  od.addEntry(DashboardEntryType::LOG, EntryLevel::INFO, "s", "m", 1000);
  TEST_ASSERT_FALSE(od.hasErrors());
}

static void test_has_errors_critical() {
  ObservabilityDashboard od;
  od.addEntry(DashboardEntryType::DIAGNOSTIC, EntryLevel::CRITICAL, "s", "m", 1000);
  TEST_ASSERT_TRUE(od.hasErrors());
}

// ── entryAt / moduleStatusFor ─────────────────────────────────────────────────

static void test_entry_at_out_of_range_null() {
  ObservabilityDashboard od;
  TEST_ASSERT_NULL(od.entryAt(0));
}

static void test_entry_at_ok() {
  ObservabilityDashboard od;
  od.addEntry(DashboardEntryType::LOG, EntryLevel::INFO, "WiFi", "ready", 1000);
  const DashboardEntry *e = od.entryAt(0);
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL_STRING("WiFi", e->source);
}

static void test_module_status_for_null_returns_null() {
  ObservabilityDashboard od;
  TEST_ASSERT_NULL(od.moduleStatusFor(nullptr));
}

static void test_module_status_for_unknown_null() {
  ObservabilityDashboard od;
  TEST_ASSERT_NULL(od.moduleStatusFor("missing"));
}

// ── generateReport ────────────────────────────────────────────────────────────

static void test_generate_report_null_safe() {
  ObservabilityDashboard od;
  od.generateReport(nullptr);
}

static void test_generate_report_produces_output() {
  ObservabilityDashboard od;
  od.addEntry(DashboardEntryType::LOG, EntryLevel::INFO, "BLE", "started", 1000);
  od.updateModuleStatus("BLE", true, "ok", 1000);
  char buf[1024];
  FILE *f = fmemopen(buf, sizeof(buf), "w");
  TEST_ASSERT_NOT_NULL(f);
  od.generateReport(f);
  fclose(f);
  TEST_ASSERT_NOT_NULL(strstr(buf, "BLE"));
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_add_entry_ok);
  RUN_TEST(test_add_entry_null_source_ok);
  RUN_TEST(test_add_entry_null_message_ok);
  RUN_TEST(test_add_entry_ring_buffer);

  RUN_TEST(test_update_module_status_ok);
  RUN_TEST(test_update_module_status_null_fails);
  RUN_TEST(test_update_module_status_upserts);
  RUN_TEST(test_update_module_status_full_fails);

  RUN_TEST(test_count_by_level);
  RUN_TEST(test_count_by_type);
  RUN_TEST(test_has_errors_true);
  RUN_TEST(test_has_errors_false);
  RUN_TEST(test_has_errors_critical);

  RUN_TEST(test_entry_at_out_of_range_null);
  RUN_TEST(test_entry_at_ok);
  RUN_TEST(test_module_status_for_null_returns_null);
  RUN_TEST(test_module_status_for_unknown_null);

  RUN_TEST(test_generate_report_null_safe);
  RUN_TEST(test_generate_report_produces_output);

  return UNITY_END();
}

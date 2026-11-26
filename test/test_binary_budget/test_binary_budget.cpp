// test_binary_budget.cpp — Tests for BinaryBudget (Phase 12).

#include "unity.h"
#include "core/BinaryBudget.h"

void setUp() {}
void tearDown() {}

// ── addEntry / update ─────────────────────────────────────────────────────────

static void test_add_entry_ok() {
  BinaryBudget bb;
  TEST_ASSERT_TRUE(bb.addEntry("flash", 1024 * 1024, 900 * 1024));
  TEST_ASSERT_EQUAL(1, (int)bb.entryCount());
}

static void test_add_entry_null_fails() {
  BinaryBudget bb;
  TEST_ASSERT_FALSE(bb.addEntry(nullptr, 1024, 512));
}

static void test_add_entry_empty_name_fails() {
  BinaryBudget bb;
  TEST_ASSERT_FALSE(bb.addEntry("", 1024, 512));
}

static void test_add_entry_updates_existing() {
  BinaryBudget bb;
  bb.addEntry("flash", 1024, 512);
  TEST_ASSERT_TRUE(bb.addEntry("flash", 2048, 1024));
  TEST_ASSERT_EQUAL(1, (int)bb.entryCount());
  const BudgetEntry *e = bb.entryFor("flash");
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL_UINT32(2048, e->budget);
}

static void test_add_entry_full_registry_fails() {
  BinaryBudget bb;
  for (size_t i = 0; i < PHANTOM_BB_MAX_SECTIONS; ++i) {
    char name[8];
    name[0] = (char)('a' + i % 26);
    name[1] = (char)('0' + i);
    name[2] = '\0';
    bb.addEntry(name, 1024, 512);
  }
  TEST_ASSERT_FALSE(bb.addEntry("overflow", 1024, 512));
}

static void test_update_entry() {
  BinaryBudget bb;
  bb.addEntry("heap", 65536, 55000);
  TEST_ASSERT_TRUE(bb.update("heap", 40000));
  const BudgetEntry *e = bb.entryFor("heap");
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL_UINT32(40000, e->current);
}

static void test_update_unknown_fails() {
  BinaryBudget bb;
  TEST_ASSERT_FALSE(bb.update("missing", 100));
}

// ── Budget checks ─────────────────────────────────────────────────────────────

static void test_all_within_budget_true() {
  BinaryBudget bb;
  bb.addEntry("flash", 1024, 900);
  bb.update("flash", 800);
  TEST_ASSERT_TRUE(bb.allWithinBudget());
}

static void test_all_within_budget_false() {
  BinaryBudget bb;
  bb.addEntry("flash", 1024, 900);
  bb.update("flash", 1500);
  TEST_ASSERT_FALSE(bb.allWithinBudget());
}

static void test_is_over_budget_true() {
  BinaryBudget bb;
  bb.addEntry("heap", 10000, 9000);
  bb.update("heap", 12000);
  TEST_ASSERT_TRUE(bb.isOverBudget("heap"));
}

static void test_is_over_budget_false() {
  BinaryBudget bb;
  bb.addEntry("heap", 10000, 9000);
  bb.update("heap", 5000);
  TEST_ASSERT_FALSE(bb.isOverBudget("heap"));
}

static void test_is_over_budget_unknown_false() {
  BinaryBudget bb;
  TEST_ASSERT_FALSE(bb.isOverBudget("missing"));
}

static void test_is_near_budget_true() {
  BinaryBudget bb;
  bb.addEntry("flash", 10000, 9000);
  bb.update("flash", 9500);
  TEST_ASSERT_TRUE(bb.isNearBudget("flash"));
}

static void test_is_near_budget_false() {
  BinaryBudget bb;
  bb.addEntry("flash", 10000, 9000);
  bb.update("flash", 5000);
  TEST_ASSERT_FALSE(bb.isNearBudget("flash"));
}

static void test_percent_used() {
  BinaryBudget bb;
  bb.addEntry("flash", 1000, 900);
  bb.update("flash", 750);
  TEST_ASSERT_EQUAL(75, (int)bb.percentUsed("flash"));
}

static void test_percent_used_zero_budget() {
  BinaryBudget bb;
  bb.addEntry("flash", 0, 0);
  TEST_ASSERT_EQUAL(0, (int)bb.percentUsed("flash"));
}

static void test_percent_used_caps_at_100() {
  BinaryBudget bb;
  bb.addEntry("flash", 1000, 900);
  bb.update("flash", 2000);
  TEST_ASSERT_EQUAL(100, (int)bb.percentUsed("flash"));
}

// ── Snapshots / regression ────────────────────────────────────────────────────

static void test_save_snapshot() {
  BinaryBudget bb;
  bb.addEntry("flash", 1024, 900);
  bb.update("flash", 500);
  TEST_ASSERT_TRUE(bb.saveSnapshot("v1.0.0", 1000));
  TEST_ASSERT_EQUAL(1, (int)bb.snapshotCount());
}

static void test_save_snapshot_null_fails() {
  BinaryBudget bb;
  TEST_ASSERT_FALSE(bb.saveSnapshot(nullptr, 1000));
}

static void test_save_snapshot_updates_existing() {
  BinaryBudget bb;
  bb.addEntry("flash", 1024, 900);
  bb.update("flash", 500);
  bb.saveSnapshot("v1.0.0", 1000);
  bb.update("flash", 600);
  bb.saveSnapshot("v1.0.0", 2000);
  TEST_ASSERT_EQUAL(1, (int)bb.snapshotCount());
  TEST_ASSERT_EQUAL_UINT64(2000, bb.snapshotFor("v1.0.0")->timestampMs);
}

static void test_regression_vs_snapshot_positive() {
  BinaryBudget bb;
  bb.addEntry("flash", 1024, 900);
  bb.update("flash", 500);
  bb.saveSnapshot("baseline", 1000);
  bb.update("flash", 700);  // grew by 200
  TEST_ASSERT_EQUAL(200, (int)bb.regressionVs("baseline", "flash"));
}

static void test_regression_vs_snapshot_negative() {
  BinaryBudget bb;
  bb.addEntry("flash", 1024, 900);
  bb.update("flash", 700);
  bb.saveSnapshot("baseline", 1000);
  bb.update("flash", 500);  // shrank by 200 (improvement)
  TEST_ASSERT_EQUAL(-200, (int)bb.regressionVs("baseline", "flash"));
}

static void test_regression_vs_unknown_snapshot_zero() {
  BinaryBudget bb;
  bb.addEntry("flash", 1024, 900);
  bb.update("flash", 700);
  TEST_ASSERT_EQUAL(0, (int)bb.regressionVs("missing", "flash"));
}

static void test_regression_vs_null_params_zero() {
  BinaryBudget bb;
  bb.addEntry("flash", 1024, 900);
  bb.saveSnapshot("snap", 1000);
  TEST_ASSERT_EQUAL(0, (int)bb.regressionVs("snap", nullptr));
  TEST_ASSERT_EQUAL(0, (int)bb.regressionVs(nullptr, "flash"));
}

static void test_entry_for_null_returns_null() {
  BinaryBudget bb;
  TEST_ASSERT_NULL(bb.entryFor(nullptr));
}

static void test_snapshot_for_null_returns_null() {
  BinaryBudget bb;
  TEST_ASSERT_NULL(bb.snapshotFor(nullptr));
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_add_entry_ok);
  RUN_TEST(test_add_entry_null_fails);
  RUN_TEST(test_add_entry_empty_name_fails);
  RUN_TEST(test_add_entry_updates_existing);
  RUN_TEST(test_add_entry_full_registry_fails);
  RUN_TEST(test_update_entry);
  RUN_TEST(test_update_unknown_fails);

  RUN_TEST(test_all_within_budget_true);
  RUN_TEST(test_all_within_budget_false);
  RUN_TEST(test_is_over_budget_true);
  RUN_TEST(test_is_over_budget_false);
  RUN_TEST(test_is_over_budget_unknown_false);
  RUN_TEST(test_is_near_budget_true);
  RUN_TEST(test_is_near_budget_false);
  RUN_TEST(test_percent_used);
  RUN_TEST(test_percent_used_zero_budget);
  RUN_TEST(test_percent_used_caps_at_100);

  RUN_TEST(test_save_snapshot);
  RUN_TEST(test_save_snapshot_null_fails);
  RUN_TEST(test_save_snapshot_updates_existing);
  RUN_TEST(test_regression_vs_snapshot_positive);
  RUN_TEST(test_regression_vs_snapshot_negative);
  RUN_TEST(test_regression_vs_unknown_snapshot_zero);
  RUN_TEST(test_regression_vs_null_params_zero);
  RUN_TEST(test_entry_for_null_returns_null);
  RUN_TEST(test_snapshot_for_null_returns_null);

  return UNITY_END();
}

// test_resource_manager.cpp — Unit tests for ResourceManager.

#include "unity.h"
#include "core/ResourceManager.h"

void setUp() {}
void tearDown() {}

// ── registerModule ─────────────────────────────────────────────────────────

static void test_register_single_module() {
  ResourceManager rm;
  TEST_ASSERT_TRUE(rm.registerModule("WiFi", {}));
  TEST_ASSERT_EQUAL(1, (int)rm.moduleCount());
}

static void test_register_null_name_rejected() {
  ResourceManager rm;
  TEST_ASSERT_FALSE(rm.registerModule(nullptr, {}));
  TEST_ASSERT_EQUAL(0, (int)rm.moduleCount());
}

static void test_register_duplicate_updates_budget() {
  ResourceManager rm;
  rm.registerModule("WiFi", {.maxHeapBytes = 1000});
  rm.registerModule("WiFi", {.maxHeapBytes = 2000});
  // Count stays at 1; budget updated.
  TEST_ASSERT_EQUAL(1, (int)rm.moduleCount());
}

static void test_register_full_registry_rejected() {
  ResourceManager rm;
  for (int i = 0; i < 16; ++i) {
    char name[8];
    name[0] = 'A' + i;
    name[1] = '\0';
    rm.registerModule(name, {});
  }
  TEST_ASSERT_EQUAL(16, (int)rm.moduleCount());
  TEST_ASSERT_FALSE(rm.registerModule("overflow", {}));
}

// ── recordAlloc / recordFree ───────────────────────────────────────────────

static void test_record_alloc_increases_heap_used() {
  ResourceManager rm;
  rm.registerModule("Mod", {});
  rm.recordAlloc("Mod", 500);
  TEST_ASSERT_EQUAL(500, (int)rm.heapUsed("Mod"));
}

static void test_record_alloc_accumulates() {
  ResourceManager rm;
  rm.registerModule("Mod", {});
  rm.recordAlloc("Mod", 100);
  rm.recordAlloc("Mod", 200);
  TEST_ASSERT_EQUAL(300, (int)rm.heapUsed("Mod"));
}

static void test_record_free_decreases_heap_used() {
  ResourceManager rm;
  rm.registerModule("Mod", {});
  rm.recordAlloc("Mod", 1000);
  rm.recordFree("Mod", 400);
  TEST_ASSERT_EQUAL(600, (int)rm.heapUsed("Mod"));
}

static void test_record_free_clamps_at_zero() {
  ResourceManager rm;
  rm.registerModule("Mod", {});
  rm.recordAlloc("Mod", 100);
  rm.recordFree("Mod", 500);  // more than allocated
  TEST_ASSERT_EQUAL(0, (int)rm.heapUsed("Mod"));
}

static void test_record_alloc_unknown_module_fails() {
  ResourceManager rm;
  TEST_ASSERT_FALSE(rm.recordAlloc("Ghost", 100));
}

static void test_record_free_unknown_module_fails() {
  ResourceManager rm;
  TEST_ASSERT_FALSE(rm.recordFree("Ghost", 100));
}

// ── isOverBudget ───────────────────────────────────────────────────────────

static void test_not_over_budget_initially() {
  ResourceManager rm;
  rm.registerModule("Mod", {.maxHeapBytes = 1000});
  TEST_ASSERT_FALSE(rm.isOverBudget("Mod"));
}

static void test_over_budget_after_alloc() {
  ResourceManager rm;
  rm.registerModule("Mod", {.maxHeapBytes = 500});
  rm.recordAlloc("Mod", 600);
  TEST_ASSERT_TRUE(rm.isOverBudget("Mod"));
}

static void test_exactly_at_budget_not_over() {
  ResourceManager rm;
  rm.registerModule("Mod", {.maxHeapBytes = 500});
  rm.recordAlloc("Mod", 500);
  TEST_ASSERT_FALSE(rm.isOverBudget("Mod"));
}

static void test_unlimited_budget_never_over() {
  ResourceManager rm;
  rm.registerModule("Mod", {.maxHeapBytes = 0});  // 0 = unlimited
  rm.recordAlloc("Mod", 999999);
  TEST_ASSERT_FALSE(rm.isOverBudget("Mod"));
}

static void test_over_budget_unknown_module_returns_false() {
  ResourceManager rm;
  TEST_ASSERT_FALSE(rm.isOverBudget("Ghost"));
}

// ── totalHeapTracked ───────────────────────────────────────────────────────

static void test_total_heap_tracked_sums_all_modules() {
  ResourceManager rm;
  rm.registerModule("A", {});
  rm.registerModule("B", {});
  rm.recordAlloc("A", 300);
  rm.recordAlloc("B", 700);
  TEST_ASSERT_EQUAL(1000, (int)rm.totalHeapTracked());
}

static void test_total_heap_tracked_zero_initially() {
  ResourceManager rm;
  rm.registerModule("A", {});
  TEST_ASSERT_EQUAL(0, (int)rm.totalHeapTracked());
}

// ── unregisterModule ───────────────────────────────────────────────────────

static void test_unregister_removes_module() {
  ResourceManager rm;
  rm.registerModule("A", {});
  rm.registerModule("B", {});
  TEST_ASSERT_TRUE(rm.unregisterModule("A"));
  TEST_ASSERT_EQUAL(1, (int)rm.moduleCount());
}

static void test_unregister_unknown_returns_false() {
  ResourceManager rm;
  TEST_ASSERT_FALSE(rm.unregisterModule("Ghost"));
}

static void test_unregister_then_reregister() {
  ResourceManager rm;
  rm.registerModule("A", {.maxHeapBytes = 100});
  rm.recordAlloc("A", 50);
  rm.unregisterModule("A");
  rm.registerModule("A", {.maxHeapBytes = 200});
  TEST_ASSERT_EQUAL(0, (int)rm.heapUsed("A"));  // fresh registration
  TEST_ASSERT_FALSE(rm.isOverBudget("A"));
}

// ── heapUsed on unknown ─────────────────────────────────────────────────────

static void test_heap_used_unknown_module_returns_zero() {
  ResourceManager rm;
  TEST_ASSERT_EQUAL(0, (int)rm.heapUsed("Ghost"));
}

// ── main ───────────────────────────────────────────────────────────────────

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_register_single_module);
  RUN_TEST(test_register_null_name_rejected);
  RUN_TEST(test_register_duplicate_updates_budget);
  RUN_TEST(test_register_full_registry_rejected);
  RUN_TEST(test_record_alloc_increases_heap_used);
  RUN_TEST(test_record_alloc_accumulates);
  RUN_TEST(test_record_free_decreases_heap_used);
  RUN_TEST(test_record_free_clamps_at_zero);
  RUN_TEST(test_record_alloc_unknown_module_fails);
  RUN_TEST(test_record_free_unknown_module_fails);
  RUN_TEST(test_not_over_budget_initially);
  RUN_TEST(test_over_budget_after_alloc);
  RUN_TEST(test_exactly_at_budget_not_over);
  RUN_TEST(test_unlimited_budget_never_over);
  RUN_TEST(test_over_budget_unknown_module_returns_false);
  RUN_TEST(test_total_heap_tracked_sums_all_modules);
  RUN_TEST(test_total_heap_tracked_zero_initially);
  RUN_TEST(test_unregister_removes_module);
  RUN_TEST(test_unregister_unknown_returns_false);
  RUN_TEST(test_unregister_then_reregister);
  RUN_TEST(test_heap_used_unknown_module_returns_zero);
  return UNITY_END();
}

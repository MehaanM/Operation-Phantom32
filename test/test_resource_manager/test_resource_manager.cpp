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

// ── peakHeapUsed ──────────────────────────────────────────────────────────

static void test_peak_heap_tracks_high_water_mark() {
  ResourceManager rm;
  rm.registerModule("Mod", {});
  rm.recordAlloc("Mod", 500);
  rm.recordAlloc("Mod", 300);
  rm.recordFree("Mod", 400);
  // Current = 400, peak = 800.
  TEST_ASSERT_EQUAL(400, (int)rm.heapUsed("Mod"));
  TEST_ASSERT_EQUAL(800, (int)rm.peakHeapUsed("Mod"));
}

static void test_peak_heap_not_lowered_by_free() {
  ResourceManager rm;
  rm.registerModule("Mod", {});
  rm.recordAlloc("Mod", 1000);
  rm.recordFree("Mod", 1000);
  TEST_ASSERT_EQUAL(0, (int)rm.heapUsed("Mod"));
  TEST_ASSERT_EQUAL(1000, (int)rm.peakHeapUsed("Mod"));
}

static void test_peak_heap_unknown_module_returns_zero() {
  ResourceManager rm;
  TEST_ASSERT_EQUAL(0, (int)rm.peakHeapUsed("Ghost"));
}

// ── alloc/free count tracking ─────────────────────────────────────────────

static void test_alloc_count_increments() {
  ResourceManager rm;
  rm.registerModule("Mod", {});
  rm.recordAlloc("Mod", 100);
  rm.recordAlloc("Mod", 200);
  TEST_ASSERT_EQUAL(2, (int)rm.allocCount("Mod"));
}

static void test_free_count_increments() {
  ResourceManager rm;
  rm.registerModule("Mod", {});
  rm.recordAlloc("Mod", 500);
  rm.recordFree("Mod", 200);
  TEST_ASSERT_EQUAL(1, (int)rm.freeCount("Mod"));
}

static void test_no_mismatch_when_balanced() {
  ResourceManager rm;
  rm.registerModule("Mod", {});
  rm.recordAlloc("Mod", 100);
  rm.recordFree("Mod", 100);
  TEST_ASSERT_FALSE(rm.hasMismatch("Mod"));
}

static void test_mismatch_when_alloc_exceeds_free() {
  ResourceManager rm;
  rm.registerModule("Mod", {});
  rm.recordAlloc("Mod", 100);
  rm.recordAlloc("Mod", 200);
  rm.recordFree("Mod", 100);
  TEST_ASSERT_TRUE(rm.hasMismatch("Mod"));
}

static void test_mismatch_unknown_module_returns_false() {
  ResourceManager rm;
  TEST_ASSERT_FALSE(rm.hasMismatch("Ghost"));
}

// ── overflow detection ─────────────────────────────────────────────────────

static void test_alloc_zero_bytes_succeeds() {
  ResourceManager rm;
  rm.registerModule("Mod", {});
  TEST_ASSERT_TRUE(rm.recordAlloc("Mod", 0));
  TEST_ASSERT_EQUAL(0, (int)rm.heapUsed("Mod"));
  TEST_ASSERT_EQUAL(1, (int)rm.allocCount("Mod"));
}

static void test_alloc_overflow_rejected() {
  ResourceManager rm;
  rm.registerModule("Mod", {});
  rm.recordAlloc("Mod", (size_t)-1);  // near SIZE_MAX
  // Second alloc would overflow.
  bool ok = rm.recordAlloc("Mod", 1);
  TEST_ASSERT_FALSE(ok);
}

// ── reset ─────────────────────────────────────────────────────────────────

static void test_reset_clears_all_counters() {
  ResourceManager rm;
  rm.registerModule("Mod", {.maxHeapBytes = 1000});
  rm.recordAlloc("Mod", 500);
  rm.recordFree("Mod", 100);
  rm.reset("Mod");
  TEST_ASSERT_EQUAL(0, (int)rm.heapUsed("Mod"));
  TEST_ASSERT_EQUAL(0, (int)rm.peakHeapUsed("Mod"));
  TEST_ASSERT_EQUAL(0, (int)rm.allocCount("Mod"));
  TEST_ASSERT_EQUAL(0, (int)rm.freeCount("Mod"));
  TEST_ASSERT_FALSE(rm.hasMismatch("Mod"));
}

static void test_alloc_count_unknown_returns_zero() {
  ResourceManager rm;
  TEST_ASSERT_EQUAL(0, (int)rm.allocCount("Ghost"));
}

static void test_free_count_unknown_returns_zero() {
  ResourceManager rm;
  TEST_ASSERT_EQUAL(0, (int)rm.freeCount("Ghost"));
}

static void test_reset_unknown_module_returns_false() {
  ResourceManager rm;
  TEST_ASSERT_FALSE(rm.reset("Ghost"));
}

static void test_reset_keeps_budget_intact() {
  ResourceManager rm;
  rm.registerModule("Mod", {.maxHeapBytes = 500});
  rm.recordAlloc("Mod", 600);
  TEST_ASSERT_TRUE(rm.isOverBudget("Mod"));
  rm.reset("Mod");
  TEST_ASSERT_FALSE(rm.isOverBudget("Mod"));  // heap cleared; budget kept
  rm.recordAlloc("Mod", 600);
  TEST_ASSERT_TRUE(rm.isOverBudget("Mod"));  // budget still enforced
}

// ── Allocation-failure injection (Phase 4) ────────────────────────────────

static void test_fail_after_zero_disables_injection() {
  ResourceManager rm;
  rm.registerModule("M", {});
  rm.setFailAfter(0);
  TEST_ASSERT_TRUE(rm.recordAlloc("M", 100));
}

static void test_fail_after_one_blocks_next_alloc() {
  ResourceManager rm;
  rm.registerModule("M", {});
  rm.setFailAfter(1);
  TEST_ASSERT_FALSE(rm.recordAlloc("M", 100));  // injected failure
  // State must not have changed.
  TEST_ASSERT_EQUAL_UINT32(0, rm.allocCount("M"));
  TEST_ASSERT_EQUAL_UINT32(0, rm.heapUsed("M"));
}

static void test_fail_after_resumes_after_n() {
  ResourceManager rm;
  rm.registerModule("M", {});
  rm.setFailAfter(2);
  TEST_ASSERT_FALSE(rm.recordAlloc("M", 100));  // 1st injection
  TEST_ASSERT_FALSE(rm.recordAlloc("M", 100));  // 2nd injection
  TEST_ASSERT_TRUE(rm.recordAlloc("M", 100));   // succeeds again
  TEST_ASSERT_EQUAL_UINT32(100, rm.heapUsed("M"));
}

static void test_fail_remaining_counts_down() {
  ResourceManager rm;
  rm.setFailAfter(3);
  TEST_ASSERT_EQUAL_UINT32(3, rm.failRemaining());
  rm.registerModule("M", {});
  rm.recordAlloc("M", 1);  // fails but decrements
  TEST_ASSERT_EQUAL_UINT32(2, rm.failRemaining());
}

static void test_fail_injection_unregistered_module_still_counts() {
  // The injection fires even for unregistered modules (to simplify test setup).
  ResourceManager rm;
  rm.setFailAfter(1);
  TEST_ASSERT_FALSE(rm.recordAlloc("nobody", 100));
  TEST_ASSERT_EQUAL_UINT32(0, rm.failRemaining());
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
  RUN_TEST(test_peak_heap_tracks_high_water_mark);
  RUN_TEST(test_peak_heap_not_lowered_by_free);
  RUN_TEST(test_peak_heap_unknown_module_returns_zero);
  RUN_TEST(test_alloc_count_increments);
  RUN_TEST(test_free_count_increments);
  RUN_TEST(test_no_mismatch_when_balanced);
  RUN_TEST(test_mismatch_when_alloc_exceeds_free);
  RUN_TEST(test_mismatch_unknown_module_returns_false);
  RUN_TEST(test_alloc_zero_bytes_succeeds);
  RUN_TEST(test_alloc_overflow_rejected);
  RUN_TEST(test_reset_clears_all_counters);
  RUN_TEST(test_alloc_count_unknown_returns_zero);
  RUN_TEST(test_free_count_unknown_returns_zero);
  RUN_TEST(test_reset_unknown_module_returns_false);
  RUN_TEST(test_reset_keeps_budget_intact);

  RUN_TEST(test_fail_after_zero_disables_injection);
  RUN_TEST(test_fail_after_one_blocks_next_alloc);
  RUN_TEST(test_fail_after_resumes_after_n);
  RUN_TEST(test_fail_remaining_counts_down);
  RUN_TEST(test_fail_injection_unregistered_module_still_counts);

  return UNITY_END();
}

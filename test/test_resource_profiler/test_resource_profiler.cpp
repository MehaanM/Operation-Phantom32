// test_resource_profiler.cpp — Tests for ResourceProfiler (Phase 19).

#include "unity.h"
#include "core/ResourceProfiler.h"

void setUp() {}
void tearDown() {}

// ── registerModule ────────────────────────────────────────────────────────────

static void test_register_module_ok() {
  ResourceProfiler rp;
  TEST_ASSERT_TRUE(rp.registerModule("WiFi"));
  TEST_ASSERT_EQUAL(1, (int)rp.moduleCount());
}

static void test_register_null_fails() {
  ResourceProfiler rp;
  TEST_ASSERT_FALSE(rp.registerModule(nullptr));
}

static void test_register_empty_fails() {
  ResourceProfiler rp;
  TEST_ASSERT_FALSE(rp.registerModule(""));
}

static void test_register_duplicate_ok() {
  ResourceProfiler rp;
  rp.registerModule("WiFi");
  TEST_ASSERT_TRUE(rp.registerModule("WiFi"));
  TEST_ASSERT_EQUAL(1, (int)rp.moduleCount());
}

static void test_register_full_fails() {
  ResourceProfiler rp;
  for (size_t i = 0; i < PHANTOM_RP_MAX_MODULES; ++i) {
    char name[8];
    name[0] = 'm';
    name[1] = (char)('0' + i % 10);
    name[2] = (char)('a' + i / 10);
    name[3] = '\0';
    rp.registerModule(name);
  }
  TEST_ASSERT_FALSE(rp.registerModule("overflow"));
}

// ── recordRam / recordCpu / recordStorage ─────────────────────────────────────

static void test_record_ram_ok() {
  ResourceProfiler rp;
  rp.registerModule("WiFi");
  TEST_ASSERT_TRUE(rp.recordRam("WiFi", 4096));
  const ModuleProfile *p = rp.profileFor("WiFi");
  TEST_ASSERT_NOT_NULL(p);
  TEST_ASSERT_EQUAL_UINT32(4096, p->ramBytes);
}

static void test_record_cpu_ok() {
  ResourceProfiler rp;
  rp.registerModule("BLE");
  TEST_ASSERT_TRUE(rp.recordCpu("BLE", 500));
  TEST_ASSERT_EQUAL_UINT32(500, rp.profileFor("BLE")->cpuUs);
}

static void test_record_storage_ok() {
  ResourceProfiler rp;
  rp.registerModule("Storage");
  TEST_ASSERT_TRUE(rp.recordStorage("Storage", 8192));
  TEST_ASSERT_EQUAL_UINT32(8192, rp.profileFor("Storage")->storageBytes);
}

static void test_record_unknown_fails() {
  ResourceProfiler rp;
  TEST_ASSERT_FALSE(rp.recordRam("missing", 1000));
  TEST_ASSERT_FALSE(rp.recordCpu("missing", 100));
  TEST_ASSERT_FALSE(rp.recordStorage("missing", 500));
}

// ── totalRam / totalCpu / totalStorage ────────────────────────────────────────

static void test_totals() {
  ResourceProfiler rp;
  rp.registerModule("WiFi");
  rp.registerModule("BLE");
  rp.recordRam("WiFi", 4096);
  rp.recordRam("BLE", 2048);
  rp.recordCpu("WiFi", 300);
  rp.recordCpu("BLE", 200);
  rp.recordStorage("WiFi", 1024);
  rp.recordStorage("BLE", 512);
  TEST_ASSERT_EQUAL_UINT32(6144, rp.totalRam());
  TEST_ASSERT_EQUAL_UINT32(500, rp.totalCpu());
  TEST_ASSERT_EQUAL_UINT32(1536, rp.totalStorage());
}

// ── saveSnapshot / ramRegressionVs ────────────────────────────────────────────

static void test_save_snapshot_ok() {
  ResourceProfiler rp;
  rp.registerModule("WiFi");
  rp.recordRam("WiFi", 4096);
  TEST_ASSERT_TRUE(rp.saveSnapshot("v1", 1000));
}

static void test_save_snapshot_null_fails() {
  ResourceProfiler rp;
  TEST_ASSERT_FALSE(rp.saveSnapshot(nullptr, 1000));
}

static void test_save_snapshot_updates_existing() {
  ResourceProfiler rp;
  rp.registerModule("WiFi");
  rp.recordRam("WiFi", 4096);
  rp.saveSnapshot("v1", 1000);
  rp.recordRam("WiFi", 5000);
  TEST_ASSERT_TRUE(rp.saveSnapshot("v1", 2000));
}

static void test_ram_regression_positive() {
  ResourceProfiler rp;
  rp.registerModule("WiFi");
  rp.recordRam("WiFi", 4096);
  rp.saveSnapshot("v1", 1000);
  rp.recordRam("WiFi", 5000); // regression = 5000 - 4096 = 904
  TEST_ASSERT_EQUAL(904, (int)rp.ramRegressionVs("v1", "WiFi"));
}

static void test_ram_regression_negative_on_improvement() {
  ResourceProfiler rp;
  rp.registerModule("WiFi");
  rp.recordRam("WiFi", 5000);
  rp.saveSnapshot("v1", 1000);
  rp.recordRam("WiFi", 4000); // improvement = -1000
  TEST_ASSERT_EQUAL(-1000, (int)rp.ramRegressionVs("v1", "WiFi"));
}

static void test_ram_regression_null_params() {
  ResourceProfiler rp;
  TEST_ASSERT_EQUAL(0, (int)rp.ramRegressionVs(nullptr, "WiFi"));
  TEST_ASSERT_EQUAL(0, (int)rp.ramRegressionVs("v1", nullptr));
}

static void test_ram_regression_unknown_snapshot() {
  ResourceProfiler rp;
  rp.registerModule("WiFi");
  rp.recordRam("WiFi", 4096);
  TEST_ASSERT_EQUAL(0, (int)rp.ramRegressionVs("missing", "WiFi"));
}

// ── profileFor / generateReport ───────────────────────────────────────────────

static void test_profile_for_null_returns_null() {
  ResourceProfiler rp;
  TEST_ASSERT_NULL(rp.profileFor(nullptr));
}

static void test_profile_for_unknown_returns_null() {
  ResourceProfiler rp;
  TEST_ASSERT_NULL(rp.profileFor("missing"));
}

static void test_generate_report_null_safe() {
  ResourceProfiler rp;
  rp.generateReport(nullptr);
}

static void test_generate_report_produces_output() {
  ResourceProfiler rp;
  rp.registerModule("WiFi");
  rp.recordRam("WiFi", 4096);
  char buf[512];
  FILE *f = fmemopen(buf, sizeof(buf), "w");
  TEST_ASSERT_NOT_NULL(f);
  rp.generateReport(f);
  fclose(f);
  TEST_ASSERT_NOT_NULL(strstr(buf, "WiFi"));
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_register_module_ok);
  RUN_TEST(test_register_null_fails);
  RUN_TEST(test_register_empty_fails);
  RUN_TEST(test_register_duplicate_ok);
  RUN_TEST(test_register_full_fails);

  RUN_TEST(test_record_ram_ok);
  RUN_TEST(test_record_cpu_ok);
  RUN_TEST(test_record_storage_ok);
  RUN_TEST(test_record_unknown_fails);

  RUN_TEST(test_totals);

  RUN_TEST(test_save_snapshot_ok);
  RUN_TEST(test_save_snapshot_null_fails);
  RUN_TEST(test_save_snapshot_updates_existing);
  RUN_TEST(test_ram_regression_positive);
  RUN_TEST(test_ram_regression_negative_on_improvement);
  RUN_TEST(test_ram_regression_null_params);
  RUN_TEST(test_ram_regression_unknown_snapshot);

  RUN_TEST(test_profile_for_null_returns_null);
  RUN_TEST(test_profile_for_unknown_returns_null);
  RUN_TEST(test_generate_report_null_safe);
  RUN_TEST(test_generate_report_produces_output);

  return UNITY_END();
}

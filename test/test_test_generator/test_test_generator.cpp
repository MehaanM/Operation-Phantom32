// test_test_generator.cpp — Tests for TestGenerator (Phase 19).

#include "unity.h"
#include "core/TestGenerator.h"

void setUp() {}
void tearDown() {}

// ── createSpec ────────────────────────────────────────────────────────────────

static void test_create_spec_ok() {
  TestGenerator tg;
  TEST_ASSERT_TRUE(tg.createSpec("WiFiModule", "INetwork"));
  TEST_ASSERT_EQUAL(1, (int)tg.specCount());
}

static void test_create_spec_null_fails() {
  TestGenerator tg;
  TEST_ASSERT_FALSE(tg.createSpec(nullptr, "INet"));
}

static void test_create_spec_empty_fails() {
  TestGenerator tg;
  TEST_ASSERT_FALSE(tg.createSpec("", "INet"));
}

static void test_create_spec_updates_existing() {
  TestGenerator tg;
  tg.createSpec("WiFiModule", "INetwork");
  TEST_ASSERT_TRUE(tg.createSpec("WiFiModule", "INetworkV2"));
  TEST_ASSERT_EQUAL(1, (int)tg.specCount());
  const ModuleTestSpec *s = tg.specFor("WiFiModule");
  TEST_ASSERT_NOT_NULL(s);
  TEST_ASSERT_EQUAL_STRING("INetworkV2", s->interfaceName);
}

static void test_create_spec_full_fails() {
  TestGenerator tg;
  for (size_t i = 0; i < PHANTOM_TG_MAX_SPECS; ++i) {
    char name[8];
    name[0] = 'M';
    name[1] = (char)('0' + i % 10);
    name[2] = (char)('a' + i / 10);
    name[3] = '\0';
    tg.createSpec(name, "IFace");
  }
  TEST_ASSERT_FALSE(tg.createSpec("overflow", "IFace"));
}

// ── addTestCase ───────────────────────────────────────────────────────────────

static void test_add_test_case_ok() {
  TestGenerator tg;
  tg.createSpec("BLE", "IBLE");
  TEST_ASSERT_TRUE(tg.addTestCase("BLE", TestCaseType::HAPPY_PATH,
                                  "test_scan_ok", "Scan returns results",
                                  "Init BLE first"));
  TEST_ASSERT_EQUAL(1, (int)tg.caseCount("BLE"));
}

static void test_add_test_case_null_module_fails() {
  TestGenerator tg;
  TEST_ASSERT_FALSE(tg.addTestCase(nullptr, TestCaseType::HAPPY_PATH,
                                   "test_ok", "desc", ""));
}

static void test_add_test_case_null_name_fails() {
  TestGenerator tg;
  tg.createSpec("BLE", "IBLE");
  TEST_ASSERT_FALSE(
      tg.addTestCase("BLE", TestCaseType::HAPPY_PATH, nullptr, "desc", ""));
}

static void test_add_test_case_unknown_module_fails() {
  TestGenerator tg;
  TEST_ASSERT_FALSE(tg.addTestCase("missing", TestCaseType::HAPPY_PATH,
                                   "test_ok", "desc", ""));
}

// ── generateFromInterface ─────────────────────────────────────────────────────

static void test_generate_happy_path_only() {
  TestGenerator tg;
  tg.createSpec("WiFi", "INet");
  TEST_ASSERT_TRUE(tg.generateFromInterface("WiFi", "scan", false, false));
  TEST_ASSERT_EQUAL(1, (int)tg.caseCount("WiFi")); // only happy path
}

static void test_generate_with_null_params() {
  TestGenerator tg;
  tg.createSpec("WiFi", "INet");
  tg.generateFromInterface("WiFi", "connect", true, false);
  // happy path + null input = 2
  TEST_ASSERT_EQUAL(2, (int)tg.caseCount("WiFi"));
}

static void test_generate_with_state_machine() {
  TestGenerator tg;
  tg.createSpec("BLE", "IBLE");
  tg.generateFromInterface("BLE", "start", false, true);
  // happy path + state machine = 2
  TEST_ASSERT_EQUAL(2, (int)tg.caseCount("BLE"));
}

static void test_generate_with_all_options() {
  TestGenerator tg;
  tg.createSpec("Storage", "IStorage");
  tg.generateFromInterface("Storage", "write", true, true);
  // happy path + null input + state machine = 3
  TEST_ASSERT_EQUAL(3, (int)tg.caseCount("Storage"));
}

static void test_generate_null_module_fails() {
  TestGenerator tg;
  TEST_ASSERT_FALSE(tg.generateFromInterface(nullptr, "scan", false, false));
}

static void test_generate_null_method_fails() {
  TestGenerator tg;
  tg.createSpec("WiFi", "INet");
  TEST_ASSERT_FALSE(tg.generateFromInterface("WiFi", nullptr, false, false));
}

// ── specFor / caseCount / printSkeleton ───────────────────────────────────────

static void test_spec_for_null_returns_null() {
  TestGenerator tg;
  TEST_ASSERT_NULL(tg.specFor(nullptr));
}

static void test_spec_for_unknown_returns_null() {
  TestGenerator tg;
  TEST_ASSERT_NULL(tg.specFor("missing"));
}

static void test_case_count_unknown_zero() {
  TestGenerator tg;
  TEST_ASSERT_EQUAL(0, (int)tg.caseCount("missing"));
}

static void test_case_count_null_zero() {
  TestGenerator tg;
  TEST_ASSERT_EQUAL(0, (int)tg.caseCount(nullptr));
}

static void test_print_skeleton_null_safe() {
  TestGenerator tg;
  tg.printSkeleton("WiFi", nullptr); // must not crash
}

static void test_print_skeleton_produces_output() {
  TestGenerator tg;
  tg.createSpec("BLE", "IBLE");
  tg.addTestCase("BLE", TestCaseType::HAPPY_PATH, "test_scan_ok", "Scan ok", "");
  char buf[512];
  FILE *f = fmemopen(buf, sizeof(buf), "w");
  TEST_ASSERT_NOT_NULL(f);
  tg.printSkeleton("BLE", f);
  fclose(f);
  TEST_ASSERT_NOT_NULL(strstr(buf, "test_scan_ok"));
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_create_spec_ok);
  RUN_TEST(test_create_spec_null_fails);
  RUN_TEST(test_create_spec_empty_fails);
  RUN_TEST(test_create_spec_updates_existing);
  RUN_TEST(test_create_spec_full_fails);

  RUN_TEST(test_add_test_case_ok);
  RUN_TEST(test_add_test_case_null_module_fails);
  RUN_TEST(test_add_test_case_null_name_fails);
  RUN_TEST(test_add_test_case_unknown_module_fails);

  RUN_TEST(test_generate_happy_path_only);
  RUN_TEST(test_generate_with_null_params);
  RUN_TEST(test_generate_with_state_machine);
  RUN_TEST(test_generate_with_all_options);
  RUN_TEST(test_generate_null_module_fails);
  RUN_TEST(test_generate_null_method_fails);

  RUN_TEST(test_spec_for_null_returns_null);
  RUN_TEST(test_spec_for_unknown_returns_null);
  RUN_TEST(test_case_count_unknown_zero);
  RUN_TEST(test_case_count_null_zero);
  RUN_TEST(test_print_skeleton_null_safe);
  RUN_TEST(test_print_skeleton_produces_output);

  return UNITY_END();
}

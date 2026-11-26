// test_doc_registry.cpp — Tests for DocRegistry (Phase 17).

#include "unity.h"
#include "core/DocRegistry.h"

void setUp() {}
void tearDown() {}

// ── Helpers ───────────────────────────────────────────────────────────────────

static APIDocEntry makeAPIEntry(const char *symbol, bool isPublic = true,
                                const char *desc = "A function") {
  APIDocEntry e = {};
  strncpy(e.symbol, symbol, sizeof(e.symbol) - 1);
  strncpy(e.module, "CoreModule", sizeof(e.module) - 1);
  strncpy(e.description, desc, sizeof(e.description) - 1);
  strncpy(e.returns, "bool", sizeof(e.returns) - 1);
  strncpy(e.since, "1.0.0", sizeof(e.since) - 1);
  e.isPublic = isPublic;
  e.isDeprecated = false;
  e.valid = true;
  return e;
}

static TroubleshootingEntry makeTrouble(const char *code, const char *symptom) {
  TroubleshootingEntry t = {};
  strncpy(t.code, code, sizeof(t.code) - 1);
  strncpy(t.symptom, symptom, sizeof(t.symptom) - 1);
  strncpy(t.cause, "Unknown cause", sizeof(t.cause) - 1);
  strncpy(t.resolution, "Check the logs", sizeof(t.resolution) - 1);
  t.valid = true;
  return t;
}

static CompatibilityEntry makeCompat(const char *feature, uint8_t level = 2) {
  CompatibilityEntry c = {};
  strncpy(c.feature, feature, sizeof(c.feature) - 1);
  strncpy(c.boardPattern, "esp32*", sizeof(c.boardPattern) - 1);
  c.level = level;
  c.valid = true;
  return c;
}

// ── addAPIEntry / docFor ──────────────────────────────────────────────────────

static void test_add_api_entry_ok() {
  DocRegistry dr;
  APIDocEntry e = makeAPIEntry("setChannel");
  TEST_ASSERT_TRUE(dr.addAPIEntry(e));
  TEST_ASSERT_EQUAL(1, (int)dr.apiEntryCount());
}

static void test_add_api_entry_invalid_fails() {
  DocRegistry dr;
  APIDocEntry e = makeAPIEntry("setChannel");
  e.valid = false;
  TEST_ASSERT_FALSE(dr.addAPIEntry(e));
}

static void test_add_api_entry_empty_symbol_fails() {
  DocRegistry dr;
  APIDocEntry e = makeAPIEntry("setChannel");
  e.symbol[0] = '\0';
  TEST_ASSERT_FALSE(dr.addAPIEntry(e));
}

static void test_add_api_entry_updates_existing() {
  DocRegistry dr;
  dr.addAPIEntry(makeAPIEntry("fn"));
  APIDocEntry updated = makeAPIEntry("fn", false, "Updated description");
  TEST_ASSERT_TRUE(dr.addAPIEntry(updated));
  TEST_ASSERT_EQUAL(1, (int)dr.apiEntryCount());
  const APIDocEntry *e = dr.docFor("fn");
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL_STRING("Updated description", e->description);
}

static void test_add_api_entry_full_fails() {
  DocRegistry dr;
  for (size_t i = 0; i < PHANTOM_DR_MAX_ENTRIES; ++i) {
    char sym[16];
    sym[0] = 'f';
    sym[1] = (char)('a' + i % 26);
    sym[2] = (char)('0' + i % 10);
    sym[3] = '\0';
    dr.addAPIEntry(makeAPIEntry(sym));
  }
  TEST_ASSERT_FALSE(dr.addAPIEntry(makeAPIEntry("overflow")));
}

static void test_doc_for_symbol() {
  DocRegistry dr;
  dr.addAPIEntry(makeAPIEntry("getChannel"));
  const APIDocEntry *e = dr.docFor("getChannel");
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL_STRING("getChannel", e->symbol);
}

static void test_doc_for_null_returns_null() {
  DocRegistry dr;
  TEST_ASSERT_NULL(dr.docFor(nullptr));
}

static void test_doc_for_unknown_returns_null() {
  DocRegistry dr;
  TEST_ASSERT_NULL(dr.docFor("nobody"));
}

// ── addTroubleshootingEntry / troubleFor ──────────────────────────────────────

static void test_add_troubleshooting_entry_ok() {
  DocRegistry dr;
  TEST_ASSERT_TRUE(dr.addTroubleshootingEntry(makeTrouble("E-001", "Scan fails")));
  TEST_ASSERT_EQUAL(1, (int)dr.troubleshootingCount());
}

static void test_add_troubleshooting_invalid_fails() {
  DocRegistry dr;
  TroubleshootingEntry t = makeTrouble("E-001", "symptom");
  t.valid = false;
  TEST_ASSERT_FALSE(dr.addTroubleshootingEntry(t));
}

static void test_add_troubleshooting_empty_code_fails() {
  DocRegistry dr;
  TroubleshootingEntry t = makeTrouble("E-001", "symptom");
  t.code[0] = '\0';
  TEST_ASSERT_FALSE(dr.addTroubleshootingEntry(t));
}

static void test_add_troubleshooting_updates_existing() {
  DocRegistry dr;
  dr.addTroubleshootingEntry(makeTrouble("E-001", "first"));
  TroubleshootingEntry updated = makeTrouble("E-001", "updated");
  TEST_ASSERT_TRUE(dr.addTroubleshootingEntry(updated));
  TEST_ASSERT_EQUAL(1, (int)dr.troubleshootingCount());
  TEST_ASSERT_EQUAL_STRING("updated", dr.troubleFor("E-001")->symptom);
}

static void test_trouble_for_code() {
  DocRegistry dr;
  dr.addTroubleshootingEntry(makeTrouble("E-002", "No BLE scan results"));
  const TroubleshootingEntry *t = dr.troubleFor("E-002");
  TEST_ASSERT_NOT_NULL(t);
  TEST_ASSERT_EQUAL_STRING("E-002", t->code);
}

static void test_trouble_for_null_returns_null() {
  DocRegistry dr;
  TEST_ASSERT_NULL(dr.troubleFor(nullptr));
}

static void test_trouble_for_unknown_returns_null() {
  DocRegistry dr;
  TEST_ASSERT_NULL(dr.troubleFor("E-999"));
}

// ── addCompatibilityEntry ─────────────────────────────────────────────────────

static void test_add_compatibility_entry_ok() {
  DocRegistry dr;
  TEST_ASSERT_TRUE(dr.addCompatibilityEntry(makeCompat("BLE5")));
  TEST_ASSERT_EQUAL(1, (int)dr.compatibilityCount());
}

static void test_add_compatibility_invalid_fails() {
  DocRegistry dr;
  CompatibilityEntry c = makeCompat("BLE5");
  c.valid = false;
  TEST_ASSERT_FALSE(dr.addCompatibilityEntry(c));
}

static void test_add_compatibility_empty_feature_fails() {
  DocRegistry dr;
  CompatibilityEntry c = makeCompat("BLE5");
  c.feature[0] = '\0';
  TEST_ASSERT_FALSE(dr.addCompatibilityEntry(c));
}

static void test_add_compatibility_full_fails() {
  DocRegistry dr;
  for (size_t i = 0; i < PHANTOM_DR_MAX_COMPAT; ++i) {
    char feat[16];
    feat[0] = 'F';
    feat[1] = (char)('0' + i % 10);
    feat[2] = '\0';
    dr.addCompatibilityEntry(makeCompat(feat));
  }
  TEST_ASSERT_FALSE(dr.addCompatibilityEntry(makeCompat("overflow")));
}

// ── isDocumented / undocumentedPublicCount ────────────────────────────────────

static void test_is_documented_true() {
  DocRegistry dr;
  dr.addAPIEntry(makeAPIEntry("setMode"));
  TEST_ASSERT_TRUE(dr.isDocumented("setMode"));
}

static void test_is_documented_false() {
  DocRegistry dr;
  TEST_ASSERT_FALSE(dr.isDocumented("setMode"));
}

static void test_undocumented_public_count_zero() {
  DocRegistry dr;
  dr.addAPIEntry(makeAPIEntry("fn", true, "documented"));
  TEST_ASSERT_EQUAL(0, (int)dr.undocumentedPublicCount());
}

static void test_undocumented_public_count_nonzero() {
  DocRegistry dr;
  APIDocEntry e = makeAPIEntry("fn");
  e.description[0] = '\0'; // no description → undocumented
  dr.addAPIEntry(e);
  TEST_ASSERT_EQUAL(1, (int)dr.undocumentedPublicCount());
}

static void test_private_undocumented_not_counted() {
  DocRegistry dr;
  APIDocEntry e = makeAPIEntry("internal_fn", false); // isPublic=false
  e.description[0] = '\0';
  dr.addAPIEntry(e);
  TEST_ASSERT_EQUAL(0, (int)dr.undocumentedPublicCount());
}

// ── generateReport ────────────────────────────────────────────────────────────

static void test_generate_report_does_not_crash_null() {
  DocRegistry dr;
  dr.generateReport(nullptr); // must not crash
}

static void test_generate_report_produces_output() {
  DocRegistry dr;
  dr.addAPIEntry(makeAPIEntry("connect", true, "Connects to network"));
  char buf[1024];
  FILE *f = fmemopen(buf, sizeof(buf), "w");
  TEST_ASSERT_NOT_NULL(f);
  dr.generateReport(f);
  fclose(f);
  TEST_ASSERT_NOT_NULL(strstr(buf, "connect"));
}

static void test_generate_report_includes_troubleshooting() {
  DocRegistry dr;
  dr.addTroubleshootingEntry(makeTrouble("E-001", "Module crash"));
  char buf[1024];
  FILE *f = fmemopen(buf, sizeof(buf), "w");
  TEST_ASSERT_NOT_NULL(f);
  dr.generateReport(f);
  fclose(f);
  TEST_ASSERT_NOT_NULL(strstr(buf, "E-001"));
}

static void test_generate_report_deprecated_symbol() {
  DocRegistry dr;
  APIDocEntry e = makeAPIEntry("oldFn", true, "Old function");
  e.isDeprecated = true;
  dr.addAPIEntry(e);
  char buf[512];
  FILE *f = fmemopen(buf, sizeof(buf), "w");
  TEST_ASSERT_NOT_NULL(f);
  dr.generateReport(f);
  fclose(f);
  TEST_ASSERT_NOT_NULL(strstr(buf, "DEPRECATED"));
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_add_api_entry_ok);
  RUN_TEST(test_add_api_entry_invalid_fails);
  RUN_TEST(test_add_api_entry_empty_symbol_fails);
  RUN_TEST(test_add_api_entry_updates_existing);
  RUN_TEST(test_add_api_entry_full_fails);
  RUN_TEST(test_doc_for_symbol);
  RUN_TEST(test_doc_for_null_returns_null);
  RUN_TEST(test_doc_for_unknown_returns_null);

  RUN_TEST(test_add_troubleshooting_entry_ok);
  RUN_TEST(test_add_troubleshooting_invalid_fails);
  RUN_TEST(test_add_troubleshooting_empty_code_fails);
  RUN_TEST(test_add_troubleshooting_updates_existing);
  RUN_TEST(test_trouble_for_code);
  RUN_TEST(test_trouble_for_null_returns_null);
  RUN_TEST(test_trouble_for_unknown_returns_null);

  RUN_TEST(test_add_compatibility_entry_ok);
  RUN_TEST(test_add_compatibility_invalid_fails);
  RUN_TEST(test_add_compatibility_empty_feature_fails);
  RUN_TEST(test_add_compatibility_full_fails);

  RUN_TEST(test_is_documented_true);
  RUN_TEST(test_is_documented_false);
  RUN_TEST(test_undocumented_public_count_zero);
  RUN_TEST(test_undocumented_public_count_nonzero);
  RUN_TEST(test_private_undocumented_not_counted);

  RUN_TEST(test_generate_report_does_not_crash_null);
  RUN_TEST(test_generate_report_produces_output);
  RUN_TEST(test_generate_report_includes_troubleshooting);
  RUN_TEST(test_generate_report_deprecated_symbol);

  return UNITY_END();
}

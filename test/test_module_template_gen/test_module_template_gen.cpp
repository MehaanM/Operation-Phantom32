// test_module_template_gen.cpp — Tests for ModuleTemplateGen (Phase 11).

#include "unity.h"
#include "core/ModuleTemplateGen.h"

void setUp() {}
void tearDown() {}

// ── create ────────────────────────────────────────────────────────────────────

static void test_create_valid_descriptor() {
  ModuleTemplateDescriptor d = ModuleTemplateGen::create("ScanModule");
  TEST_ASSERT_TRUE(d.valid);
  TEST_ASSERT_EQUAL_STRING("ScanModule", d.moduleName);
}

static void test_create_null_name_invalid() {
  ModuleTemplateDescriptor d = ModuleTemplateGen::create(nullptr);
  TEST_ASSERT_FALSE(d.valid);
}

static void test_create_empty_name_invalid() {
  ModuleTemplateDescriptor d = ModuleTemplateGen::create("");
  TEST_ASSERT_FALSE(d.valid);
}

static void test_create_has_default_sections() {
  ModuleTemplateDescriptor d = ModuleTemplateGen::create("Mod");
  TEST_ASSERT_GREATER_THAN(0, (int)d.sectionCount);
}

static void test_create_has_lifecycle_section() {
  ModuleTemplateDescriptor d = ModuleTemplateGen::create("Mod");
  bool found = false;
  for (size_t i = 0; i < d.sectionCount; ++i) {
    if (strcmp(d.sections[i].name, "lifecycle") == 0) {
      found = true;
      break;
    }
  }
  TEST_ASSERT_TRUE(found);
}

static void test_create_initial_sections_missing() {
  ModuleTemplateDescriptor d = ModuleTemplateGen::create("Mod");
  for (size_t i = 0; i < d.sectionCount; ++i) {
    TEST_ASSERT_EQUAL((int)TemplateSectionStatus::MISSING, (int)d.sections[i].status);
  }
}

// ── markComplete / markStub ───────────────────────────────────────────────────

static void test_mark_complete_existing_section() {
  ModuleTemplateGen gen;
  ModuleTemplateDescriptor d = ModuleTemplateGen::create("Mod");
  TEST_ASSERT_TRUE(gen.markComplete(d, "lifecycle"));
  bool found = false;
  for (size_t i = 0; i < d.sectionCount; ++i) {
    if (strcmp(d.sections[i].name, "lifecycle") == 0) {
      TEST_ASSERT_EQUAL((int)TemplateSectionStatus::COMPLETE, (int)d.sections[i].status);
      found = true;
    }
  }
  TEST_ASSERT_TRUE(found);
}

static void test_mark_complete_unknown_section() {
  ModuleTemplateGen gen;
  ModuleTemplateDescriptor d = ModuleTemplateGen::create("Mod");
  TEST_ASSERT_FALSE(gen.markComplete(d, "nonexistent"));
}

static void test_mark_complete_null_name() {
  ModuleTemplateGen gen;
  ModuleTemplateDescriptor d = ModuleTemplateGen::create("Mod");
  TEST_ASSERT_FALSE(gen.markComplete(d, nullptr));
}

static void test_mark_stub_existing_section() {
  ModuleTemplateGen gen;
  ModuleTemplateDescriptor d = ModuleTemplateGen::create("Mod");
  TEST_ASSERT_TRUE(gen.markStub(d, "tests"));
  for (size_t i = 0; i < d.sectionCount; ++i) {
    if (strcmp(d.sections[i].name, "tests") == 0) {
      TEST_ASSERT_EQUAL((int)TemplateSectionStatus::STUB, (int)d.sections[i].status);
    }
  }
}

static void test_mark_stub_null_name() {
  ModuleTemplateGen gen;
  ModuleTemplateDescriptor d = ModuleTemplateGen::create("Mod");
  TEST_ASSERT_FALSE(gen.markStub(d, nullptr));
}

// ── validate ──────────────────────────────────────────────────────────────────

static void test_validate_fails_when_required_missing() {
  ModuleTemplateGen gen;
  ModuleTemplateDescriptor d = ModuleTemplateGen::create("Mod");
  TEST_ASSERT_FALSE(gen.validate(d));
}

static void test_validate_passes_when_all_required_complete() {
  ModuleTemplateGen gen;
  ModuleTemplateDescriptor d = ModuleTemplateGen::create("Mod");
  for (size_t i = 0; i < d.sectionCount; ++i) {
    if (d.sections[i].required) d.sections[i].status = TemplateSectionStatus::COMPLETE;
  }
  TEST_ASSERT_TRUE(gen.validate(d));
}

static void test_validate_invalid_descriptor_fails() {
  ModuleTemplateGen gen;
  ModuleTemplateDescriptor d = {};
  TEST_ASSERT_FALSE(gen.validate(d));
}

// ── missingRequiredCount / completenessScore ─────────────────────────────────

static void test_missing_required_count_all_missing() {
  ModuleTemplateGen gen;
  ModuleTemplateDescriptor d = ModuleTemplateGen::create("Mod");
  size_t expected = 0;
  for (size_t i = 0; i < d.sectionCount; ++i) {
    if (d.sections[i].required) ++expected;
  }
  TEST_ASSERT_EQUAL(expected, gen.missingRequiredCount(d));
}

static void test_missing_required_count_zero_when_all_complete() {
  ModuleTemplateGen gen;
  ModuleTemplateDescriptor d = ModuleTemplateGen::create("Mod");
  for (size_t i = 0; i < d.sectionCount; ++i) {
    d.sections[i].status = TemplateSectionStatus::COMPLETE;
  }
  TEST_ASSERT_EQUAL(0, (int)gen.missingRequiredCount(d));
}

static void test_completeness_score_zero_all_missing() {
  ModuleTemplateGen gen;
  ModuleTemplateDescriptor d = ModuleTemplateGen::create("Mod");
  TEST_ASSERT_EQUAL(0, (int)gen.completenessScore(d));
}

static void test_completeness_score_100_all_complete() {
  ModuleTemplateGen gen;
  ModuleTemplateDescriptor d = ModuleTemplateGen::create("Mod");
  for (size_t i = 0; i < d.sectionCount; ++i) {
    d.sections[i].status = TemplateSectionStatus::COMPLETE;
  }
  TEST_ASSERT_EQUAL(100, (int)gen.completenessScore(d));
}

static void test_completeness_score_empty_descriptor() {
  ModuleTemplateGen gen;
  ModuleTemplateDescriptor d = {};
  TEST_ASSERT_EQUAL(0, (int)gen.completenessScore(d));
}

// ── addSection / addDependency ────────────────────────────────────────────────

static void test_add_custom_section() {
  ModuleTemplateGen gen;
  ModuleTemplateDescriptor d = ModuleTemplateGen::create("Mod");
  size_t before = d.sectionCount;
  TEST_ASSERT_TRUE(gen.addSection(d, "metrics", false));
  TEST_ASSERT_EQUAL(before + 1, d.sectionCount);
}

static void test_add_section_null_fails() {
  ModuleTemplateGen gen;
  ModuleTemplateDescriptor d = ModuleTemplateGen::create("Mod");
  TEST_ASSERT_FALSE(gen.addSection(d, nullptr, true));
}

static void test_add_dependency() {
  ModuleTemplateGen gen;
  ModuleTemplateDescriptor d = ModuleTemplateGen::create("Mod");
  TEST_ASSERT_TRUE(gen.addDependency(d, "ConfigManager"));
  TEST_ASSERT_EQUAL(1, (int)d.depCount);
}

static void test_add_dependency_null_fails() {
  ModuleTemplateGen gen;
  ModuleTemplateDescriptor d = ModuleTemplateGen::create("Mod");
  TEST_ASSERT_FALSE(gen.addDependency(d, nullptr));
}

static void test_add_dependency_full_fails() {
  ModuleTemplateGen gen;
  ModuleTemplateDescriptor d = ModuleTemplateGen::create("Mod");
  for (size_t i = 0; i < PHANTOM_MTG_MAX_DEPS; ++i) {
    gen.addDependency(d, "dep");
  }
  TEST_ASSERT_FALSE(gen.addDependency(d, "overflow"));
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_create_valid_descriptor);
  RUN_TEST(test_create_null_name_invalid);
  RUN_TEST(test_create_empty_name_invalid);
  RUN_TEST(test_create_has_default_sections);
  RUN_TEST(test_create_has_lifecycle_section);
  RUN_TEST(test_create_initial_sections_missing);

  RUN_TEST(test_mark_complete_existing_section);
  RUN_TEST(test_mark_complete_unknown_section);
  RUN_TEST(test_mark_complete_null_name);
  RUN_TEST(test_mark_stub_existing_section);
  RUN_TEST(test_mark_stub_null_name);

  RUN_TEST(test_validate_fails_when_required_missing);
  RUN_TEST(test_validate_passes_when_all_required_complete);
  RUN_TEST(test_validate_invalid_descriptor_fails);

  RUN_TEST(test_missing_required_count_all_missing);
  RUN_TEST(test_missing_required_count_zero_when_all_complete);
  RUN_TEST(test_completeness_score_zero_all_missing);
  RUN_TEST(test_completeness_score_100_all_complete);
  RUN_TEST(test_completeness_score_empty_descriptor);

  RUN_TEST(test_add_custom_section);
  RUN_TEST(test_add_section_null_fails);
  RUN_TEST(test_add_dependency);
  RUN_TEST(test_add_dependency_null_fails);
  RUN_TEST(test_add_dependency_full_fails);

  return UNITY_END();
}

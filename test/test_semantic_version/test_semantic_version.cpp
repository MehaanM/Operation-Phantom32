// test_semantic_version.cpp — Tests for SemanticVersion (Phase 11).

#include "unity.h"
#include "core/SemanticVersion.h"

void setUp() {}
void tearDown() {}

// ── Parsing ───────────────────────────────────────────────────────────────────

static void test_parse_valid_version() {
  SemanticVersion v = SemanticVersion::parse("1.2.3");
  TEST_ASSERT_TRUE(v.valid);
  TEST_ASSERT_EQUAL(1, v.major);
  TEST_ASSERT_EQUAL(2, v.minor);
  TEST_ASSERT_EQUAL(3, v.patch);
}

static void test_parse_with_prerelease() {
  SemanticVersion v = SemanticVersion::parse("2.0.0-alpha.1");
  TEST_ASSERT_TRUE(v.valid);
  TEST_ASSERT_EQUAL(2, v.major);
  TEST_ASSERT_EQUAL_STRING("alpha.1", v.preRelease);
}

static void test_parse_with_build_meta() {
  SemanticVersion v = SemanticVersion::parse("1.0.0+build.123");
  TEST_ASSERT_TRUE(v.valid);
  TEST_ASSERT_EQUAL_STRING("build.123", v.buildMeta);
}

static void test_parse_prerelease_and_build() {
  SemanticVersion v = SemanticVersion::parse("1.0.0-beta+sha.abc");
  TEST_ASSERT_TRUE(v.valid);
  TEST_ASSERT_EQUAL_STRING("beta", v.preRelease);
  TEST_ASSERT_EQUAL_STRING("sha.abc", v.buildMeta);
}

static void test_parse_null_invalid() {
  SemanticVersion v = SemanticVersion::parse(nullptr);
  TEST_ASSERT_FALSE(v.valid);
}

static void test_parse_empty_invalid() {
  SemanticVersion v = SemanticVersion::parse("");
  TEST_ASSERT_FALSE(v.valid);
}

static void test_parse_too_few_parts_invalid() {
  SemanticVersion v = SemanticVersion::parse("1.2");
  TEST_ASSERT_FALSE(v.valid);
}

static void test_parse_zero_version() {
  SemanticVersion v = SemanticVersion::parse("0.0.0");
  TEST_ASSERT_TRUE(v.valid);
  TEST_ASSERT_EQUAL(0, v.major);
}

// ── Compare ───────────────────────────────────────────────────────────────────

static void test_compare_equal_versions() {
  SemanticVersion a = SemanticVersion::parse("1.2.3");
  SemanticVersion b = SemanticVersion::parse("1.2.3");
  TEST_ASSERT_EQUAL(0, a.compare(b));
}

static void test_compare_major_less() {
  SemanticVersion a = SemanticVersion::parse("1.0.0");
  SemanticVersion b = SemanticVersion::parse("2.0.0");
  TEST_ASSERT_TRUE(a.compare(b) < 0);
}

static void test_compare_major_greater() {
  SemanticVersion a = SemanticVersion::parse("3.0.0");
  SemanticVersion b = SemanticVersion::parse("2.9.9");
  TEST_ASSERT_TRUE(a.compare(b) > 0);
}

static void test_compare_minor_difference() {
  SemanticVersion a = SemanticVersion::parse("1.1.0");
  SemanticVersion b = SemanticVersion::parse("1.2.0");
  TEST_ASSERT_TRUE(a < b);
  TEST_ASSERT_FALSE(a > b);
}

static void test_compare_patch_difference() {
  SemanticVersion a = SemanticVersion::parse("1.0.1");
  SemanticVersion b = SemanticVersion::parse("1.0.2");
  TEST_ASSERT_TRUE(a <= b);
  TEST_ASSERT_TRUE(b >= a);
}

static void test_operators_equal() {
  SemanticVersion a = SemanticVersion::parse("1.2.3");
  SemanticVersion b = SemanticVersion::parse("1.2.3");
  TEST_ASSERT_TRUE(a == b);
}

// ── Satisfies ─────────────────────────────────────────────────────────────────

static void test_satisfies_gte() {
  SemanticVersion v = SemanticVersion::parse("1.5.0");
  TEST_ASSERT_TRUE(v.satisfies(">=1.0.0"));
  TEST_ASSERT_FALSE(v.satisfies(">=2.0.0"));
}

static void test_satisfies_lte() {
  SemanticVersion v = SemanticVersion::parse("1.5.0");
  TEST_ASSERT_TRUE(v.satisfies("<=2.0.0"));
  TEST_ASSERT_FALSE(v.satisfies("<=1.0.0"));
}

static void test_satisfies_eq() {
  SemanticVersion v = SemanticVersion::parse("1.2.3");
  TEST_ASSERT_TRUE(v.satisfies("==1.2.3"));
  TEST_ASSERT_FALSE(v.satisfies("==1.2.4"));
}

static void test_satisfies_gt() {
  SemanticVersion v = SemanticVersion::parse("2.0.0");
  TEST_ASSERT_TRUE(v.satisfies(">1.9.9"));
  TEST_ASSERT_FALSE(v.satisfies(">2.0.0"));
}

static void test_satisfies_lt() {
  SemanticVersion v = SemanticVersion::parse("1.0.0");
  TEST_ASSERT_TRUE(v.satisfies("<2.0.0"));
  TEST_ASSERT_FALSE(v.satisfies("<1.0.0"));
}

static void test_satisfies_caret_compatible() {
  SemanticVersion v = SemanticVersion::parse("1.5.3");
  TEST_ASSERT_TRUE(v.satisfies("^1.0.0"));
  TEST_ASSERT_FALSE(v.satisfies("^2.0.0"));
}

static void test_satisfies_tilde_approximate() {
  SemanticVersion v = SemanticVersion::parse("1.2.5");
  TEST_ASSERT_TRUE(v.satisfies("~1.2.0"));
  TEST_ASSERT_FALSE(v.satisfies("~1.3.0"));
}

static void test_satisfies_bare_version() {
  SemanticVersion v = SemanticVersion::parse("1.0.0");
  TEST_ASSERT_TRUE(v.satisfies("1.0.0"));
  TEST_ASSERT_FALSE(v.satisfies("1.0.1"));
}

static void test_satisfies_null_constraint() {
  SemanticVersion v = SemanticVersion::parse("1.0.0");
  TEST_ASSERT_FALSE(v.satisfies(nullptr));
}

static void test_satisfies_invalid_self() {
  SemanticVersion v = SemanticVersion::parse("");
  TEST_ASSERT_FALSE(v.satisfies(">=1.0.0"));
}

static void test_satisfies_invalid_target() {
  SemanticVersion v = SemanticVersion::parse("1.0.0");
  TEST_ASSERT_FALSE(v.satisfies(">=not-a-version"));
}

// ── toString ──────────────────────────────────────────────────────────────────

static void test_tostring_basic() {
  SemanticVersion v = SemanticVersion::parse("3.14.0");
  char buf[32];
  v.toString(buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("3.14.0", buf);
}

static void test_tostring_with_prerelease() {
  SemanticVersion v = SemanticVersion::parse("1.0.0-rc.1");
  char buf[32];
  v.toString(buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("1.0.0-rc.1", buf);
}

static void test_tostring_null_buf_safe() {
  SemanticVersion v = SemanticVersion::parse("1.0.0");
  v.toString(nullptr, 0); // must not crash
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_parse_valid_version);
  RUN_TEST(test_parse_with_prerelease);
  RUN_TEST(test_parse_with_build_meta);
  RUN_TEST(test_parse_prerelease_and_build);
  RUN_TEST(test_parse_null_invalid);
  RUN_TEST(test_parse_empty_invalid);
  RUN_TEST(test_parse_too_few_parts_invalid);
  RUN_TEST(test_parse_zero_version);

  RUN_TEST(test_compare_equal_versions);
  RUN_TEST(test_compare_major_less);
  RUN_TEST(test_compare_major_greater);
  RUN_TEST(test_compare_minor_difference);
  RUN_TEST(test_compare_patch_difference);
  RUN_TEST(test_operators_equal);

  RUN_TEST(test_satisfies_gte);
  RUN_TEST(test_satisfies_lte);
  RUN_TEST(test_satisfies_eq);
  RUN_TEST(test_satisfies_gt);
  RUN_TEST(test_satisfies_lt);
  RUN_TEST(test_satisfies_caret_compatible);
  RUN_TEST(test_satisfies_tilde_approximate);
  RUN_TEST(test_satisfies_bare_version);
  RUN_TEST(test_satisfies_null_constraint);
  RUN_TEST(test_satisfies_invalid_self);
  RUN_TEST(test_satisfies_invalid_target);

  RUN_TEST(test_tostring_basic);
  RUN_TEST(test_tostring_with_prerelease);
  RUN_TEST(test_tostring_null_buf_safe);

  return UNITY_END();
}

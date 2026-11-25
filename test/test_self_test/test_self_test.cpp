// test_self_test.cpp — Unit tests for SelfTest framework.

#include "unity.h"
#include "core/SelfTest.h"
#include <stdio.h>

void setUp() {}
void tearDown() {}

// ── add ────────────────────────────────────────────────────────────────────

static void test_add_single_test() {
  SelfTest st;
  TEST_ASSERT_TRUE(st.add("hw", [] { return true; }));
  TEST_ASSERT_EQUAL(1, (int)st.testCount());
}

static void test_add_null_name_rejected() {
  SelfTest st;
  TEST_ASSERT_FALSE(st.add(nullptr, [] { return true; }));
  TEST_ASSERT_EQUAL(0, (int)st.testCount());
}

static void test_add_null_fn_rejected() {
  SelfTest st;
  TEST_ASSERT_FALSE(st.add("hw", nullptr));
  TEST_ASSERT_EQUAL(0, (int)st.testCount());
}

static void test_add_up_to_max() {
  SelfTest st;
  for (int i = 0; i < 16; ++i) {
    char name[8];
    name[0] = 'A' + i;
    name[1] = '\0';
    TEST_ASSERT_TRUE(st.add(name, [] { return true; }));
  }
  TEST_ASSERT_EQUAL(16, (int)st.testCount());
}

static void test_add_beyond_max_rejected() {
  SelfTest st;
  for (int i = 0; i < 16; ++i) {
    char name[8];
    name[0] = 'A' + i;
    name[1] = '\0';
    st.add(name, [] { return true; });
  }
  TEST_ASSERT_FALSE(st.add("overflow", [] { return true; }));
  TEST_ASSERT_EQUAL(16, (int)st.testCount());
}

// ── run — all pass ─────────────────────────────────────────────────────────

static void test_run_all_pass_returns_true() {
  SelfTest st;
  st.add("A", [] { return true; });
  st.add("B", [] { return true; });
  FILE *dev = fopen("/dev/null", "w");
  bool ok = st.run(dev);
  fclose(dev);
  TEST_ASSERT_TRUE(ok);
}

static void test_run_all_pass_counts() {
  SelfTest st;
  st.add("A", [] { return true; });
  st.add("B", [] { return true; });
  FILE *dev = fopen("/dev/null", "w");
  st.run(dev);
  fclose(dev);
  TEST_ASSERT_EQUAL(2, (int)st.passCount());
  TEST_ASSERT_EQUAL(0, (int)st.failCount());
}

// ── run — with failures ────────────────────────────────────────────────────

static void test_run_with_failure_returns_false() {
  SelfTest st;
  st.add("good", [] { return true; });
  st.add("bad", [] { return false; });
  FILE *dev = fopen("/dev/null", "w");
  bool ok = st.run(dev);
  fclose(dev);
  TEST_ASSERT_FALSE(ok);
}

static void test_run_counts_pass_and_fail() {
  SelfTest st;
  st.add("p1", [] { return true; });
  st.add("p2", [] { return true; });
  st.add("f1", [] { return false; });
  FILE *dev = fopen("/dev/null", "w");
  st.run(dev);
  fclose(dev);
  TEST_ASSERT_EQUAL(2, (int)st.passCount());
  TEST_ASSERT_EQUAL(1, (int)st.failCount());
}

static void test_run_all_fail() {
  SelfTest st;
  st.add("f1", [] { return false; });
  st.add("f2", [] { return false; });
  FILE *dev = fopen("/dev/null", "w");
  st.run(dev);
  fclose(dev);
  TEST_ASSERT_EQUAL(0, (int)st.passCount());
  TEST_ASSERT_EQUAL(2, (int)st.failCount());
}

// ── resultOf ───────────────────────────────────────────────────────────────

static void test_result_of_pass() {
  SelfTest st;
  st.add("hw", [] { return true; });
  FILE *dev = fopen("/dev/null", "w");
  st.run(dev);
  fclose(dev);
  TEST_ASSERT_TRUE(st.resultOf("hw"));
}

static void test_result_of_fail() {
  SelfTest st;
  st.add("mem", [] { return false; });
  FILE *dev = fopen("/dev/null", "w");
  st.run(dev);
  fclose(dev);
  TEST_ASSERT_FALSE(st.resultOf("mem"));
}

static void test_result_of_unknown_returns_false() {
  SelfTest st;
  TEST_ASSERT_FALSE(st.resultOf("doesNotExist"));
}

// ── empty run ──────────────────────────────────────────────────────────────

static void test_run_empty_returns_true() {
  SelfTest st;
  FILE *dev = fopen("/dev/null", "w");
  bool ok = st.run(dev);
  fclose(dev);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL(0, (int)st.passCount());
  TEST_ASSERT_EQUAL(0, (int)st.failCount());
}

// ── entryAt ────────────────────────────────────────────────────────────────

static void test_entry_at_valid_index() {
  SelfTest st;
  st.add("check", [] { return true; });
  FILE *dev = fopen("/dev/null", "w");
  st.run(dev);
  fclose(dev);
  const SelfTest::Entry *e = st.entryAt(0);
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL_STRING("check", e->name);
  TEST_ASSERT_TRUE(e->ran);
  TEST_ASSERT_TRUE(e->passed);
}

static void test_entry_at_out_of_bounds_returns_null() {
  SelfTest st;
  TEST_ASSERT_NULL(st.entryAt(0));
}

// ── run twice resets counts ────────────────────────────────────────────────

static void test_second_run_resets_counts() {
  SelfTest st;
  st.add("toggle", [] { return true; });
  FILE *dev = fopen("/dev/null", "w");
  st.run(dev);
  st.run(dev);
  fclose(dev);
  TEST_ASSERT_EQUAL(1, (int)st.passCount());
  TEST_ASSERT_EQUAL(0, (int)st.failCount());
}

// ── main ───────────────────────────────────────────────────────────────────

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_add_single_test);
  RUN_TEST(test_add_null_name_rejected);
  RUN_TEST(test_add_null_fn_rejected);
  RUN_TEST(test_add_up_to_max);
  RUN_TEST(test_add_beyond_max_rejected);
  RUN_TEST(test_run_all_pass_returns_true);
  RUN_TEST(test_run_all_pass_counts);
  RUN_TEST(test_run_with_failure_returns_false);
  RUN_TEST(test_run_counts_pass_and_fail);
  RUN_TEST(test_run_all_fail);
  RUN_TEST(test_result_of_pass);
  RUN_TEST(test_result_of_fail);
  RUN_TEST(test_result_of_unknown_returns_false);
  RUN_TEST(test_run_empty_returns_true);
  RUN_TEST(test_entry_at_valid_index);
  RUN_TEST(test_entry_at_out_of_bounds_returns_null);
  RUN_TEST(test_second_run_resets_counts);
  return UNITY_END();
}

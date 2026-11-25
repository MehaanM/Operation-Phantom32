// test_module_lifecycle.cpp — Unit tests for IModule and ModuleLifecycle.
//
// All tests run on native (no Arduino dependencies).

#include "unity.h"
#include "core/IModule.h"
#include "core/ModuleLifecycle.h"

void setUp() {}
void tearDown() {}

// ── Minimal concrete module for testing ────────────────────────────────────

struct MockModule : public IModule {
  bool initResult = true;
  int initCalls = 0;
  int updateCalls = 0;
  int shutdownCalls = 0;

  bool initialize() override {
    initCalls++;
    if (initResult) {
      setRunning();
      return true;
    }
    setError();
    return false;
  }
  void update() override { updateCalls++; }
  void shutdown() override {
    shutdownCalls++;
    setStopped();
  }
  const char *name() const override { return "MockModule"; }
  const char *version() const override { return "1.0.0"; }
  const char *description() const override { return "Test module"; }
};

struct NamedModule : public IModule {
  const char *label;
  explicit NamedModule(const char *n) : label(n) {}
  bool initialize() override {
    setRunning();
    return true;
  }
  void update() override {}
  void shutdown() override { setStopped(); }
  const char *name() const override { return label; }
  const char *version() const override { return "1.0"; }
  const char *description() const override { return ""; }
};

// Module that declares a dependency on another named module.
struct DependentModule : public IModule {
  const char *label;
  const char *dep;
  int initOrder = -1;
  static int orderCounter;

  DependentModule(const char *n, const char *d) : label(n), dep(d) {}

  bool initialize() override {
    initOrder = orderCounter++;
    setRunning();
    return true;
  }
  void update() override {}
  void shutdown() override { setStopped(); }
  const char *name() const override { return label; }
  const char *version() const override { return "1.0"; }
  const char *description() const override { return ""; }

  size_t dependencyCount() const override { return dep ? 1u : 0u; }
  const char *dependency(size_t i) const override { return (i == 0) ? dep : nullptr; }
};
int DependentModule::orderCounter = 0;

// Module that sets itself to Error state inside update() (for autoRestart tests).
struct FlakyModule : public IModule {
  bool failOnUpdate = false;
  int initCalls = 0;

  bool initialize() override {
    ++initCalls;
    setRunning();
    return true;
  }
  void update() override {
    if (failOnUpdate)
      setError();
  }
  void shutdown() override { setStopped(); }
  const char *name() const override { return "FlakyModule"; }
  const char *version() const override { return "1.0"; }
  const char *description() const override { return ""; }
};

// ── IModule state machine ──────────────────────────────────────────────────

static void test_initial_state_uninitialized() {
  MockModule m;
  TEST_ASSERT_EQUAL(IModule::State::Uninitialized, m.state());
}

static void test_initialize_transitions_to_running() {
  MockModule m;
  TEST_ASSERT_TRUE(m.initialize());
  TEST_ASSERT_EQUAL(IModule::State::Running, m.state());
  TEST_ASSERT_TRUE(m.isRunning());
}

static void test_failed_initialize_transitions_to_error() {
  MockModule m;
  m.initResult = false;
  TEST_ASSERT_FALSE(m.initialize());
  TEST_ASSERT_EQUAL(IModule::State::Error, m.state());
  TEST_ASSERT_TRUE(m.hasError());
}

static void test_shutdown_transitions_to_stopped() {
  MockModule m;
  m.initialize();
  m.shutdown();
  TEST_ASSERT_EQUAL(IModule::State::Stopped, m.state());
  TEST_ASSERT_FALSE(m.isRunning());
}

static void test_is_not_running_when_stopped() {
  MockModule m;
  m.initialize();
  m.shutdown();
  TEST_ASSERT_FALSE(m.isRunning());
}

static void test_is_not_error_when_ok() {
  MockModule m;
  m.initialize();
  TEST_ASSERT_FALSE(m.hasError());
}

// ── ModuleLifecycle registration ───────────────────────────────────────────

static void test_register_increases_count() {
  ModuleLifecycle<4> lc;
  MockModule m;
  TEST_ASSERT_TRUE(lc.registerModule(&m));
  TEST_ASSERT_EQUAL(1, lc.count());
}

static void test_register_full_returns_false() {
  ModuleLifecycle<2> lc;
  MockModule a, b, c;
  lc.registerModule(&a);
  lc.registerModule(&b);
  TEST_ASSERT_FALSE(lc.registerModule(&c));
  TEST_ASSERT_EQUAL(2, lc.count());
}

static void test_register_null_returns_false() {
  ModuleLifecycle<4> lc;
  TEST_ASSERT_FALSE(lc.registerModule(nullptr));
  TEST_ASSERT_EQUAL(0, lc.count());
}

static void test_capacity_matches_template_param() {
  ModuleLifecycle<8> lc;
  TEST_ASSERT_EQUAL(8, lc.capacity());
}

// ── ModuleLifecycle initializeAll ──────────────────────────────────────────

static void test_initialize_all_calls_each_module() {
  ModuleLifecycle<4> lc;
  MockModule a, b;
  lc.registerModule(&a);
  lc.registerModule(&b);
  lc.initializeAll();
  TEST_ASSERT_EQUAL(1, a.initCalls);
  TEST_ASSERT_EQUAL(1, b.initCalls);
}

static void test_initialize_all_returns_true_when_all_succeed() {
  ModuleLifecycle<4> lc;
  MockModule a, b;
  lc.registerModule(&a);
  lc.registerModule(&b);
  TEST_ASSERT_TRUE(lc.initializeAll());
}

static void test_initialize_all_returns_false_when_required_fails() {
  ModuleLifecycle<4> lc;
  MockModule bad;
  bad.initResult = false;
  lc.registerModule(&bad, true /*required*/);
  TEST_ASSERT_FALSE(lc.initializeAll());
}

static void test_initialize_all_returns_true_when_optional_fails() {
  ModuleLifecycle<4> lc;
  MockModule bad;
  bad.initResult = false;
  lc.registerModule(&bad, false /*optional*/);
  TEST_ASSERT_TRUE(lc.initializeAll());
}

static void test_initialize_all_continues_after_optional_failure() {
  ModuleLifecycle<4> lc;
  MockModule bad, good;
  bad.initResult = false;
  lc.registerModule(&bad, false);
  lc.registerModule(&good);
  lc.initializeAll();
  TEST_ASSERT_EQUAL(1, good.initCalls);
}

// ── ModuleLifecycle updateAll ──────────────────────────────────────────────

static void test_update_all_calls_running_modules() {
  ModuleLifecycle<4> lc;
  MockModule m;
  lc.registerModule(&m);
  lc.initializeAll();
  lc.updateAll();
  TEST_ASSERT_EQUAL(1, m.updateCalls);
}

static void test_update_all_skips_non_running_modules() {
  ModuleLifecycle<4> lc;
  MockModule m;
  m.initResult = false;
  lc.registerModule(&m);
  lc.initializeAll();
  lc.updateAll();
  TEST_ASSERT_EQUAL(0, m.updateCalls);
}

static void test_update_all_multiple_times() {
  ModuleLifecycle<4> lc;
  MockModule m;
  lc.registerModule(&m);
  lc.initializeAll();
  lc.updateAll();
  lc.updateAll();
  lc.updateAll();
  TEST_ASSERT_EQUAL(3, m.updateCalls);
}

// ── ModuleLifecycle shutdownAll ────────────────────────────────────────────

static void test_shutdown_all_calls_each_running_module() {
  ModuleLifecycle<4> lc;
  MockModule a, b;
  lc.registerModule(&a);
  lc.registerModule(&b);
  lc.initializeAll();
  lc.shutdownAll();
  TEST_ASSERT_EQUAL(1, a.shutdownCalls);
  TEST_ASSERT_EQUAL(1, b.shutdownCalls);
}

static void test_shutdown_all_skips_non_running_modules() {
  ModuleLifecycle<4> lc;
  MockModule m;
  m.initResult = false;
  lc.registerModule(&m);
  lc.initializeAll();
  lc.shutdownAll();
  TEST_ASSERT_EQUAL(0, m.shutdownCalls);
}

// ── ModuleLifecycle find ───────────────────────────────────────────────────

static void test_find_returns_module_by_name() {
  ModuleLifecycle<4> lc;
  NamedModule a("Alpha"), b("Beta");
  lc.registerModule(&a);
  lc.registerModule(&b);
  TEST_ASSERT_EQUAL_PTR(&b, lc.find("Beta"));
}

static void test_find_returns_null_for_missing_name() {
  ModuleLifecycle<4> lc;
  NamedModule a("Alpha");
  lc.registerModule(&a);
  TEST_ASSERT_NULL(lc.find("Missing"));
}

static void test_find_empty_lifecycle_returns_null() {
  ModuleLifecycle<4> lc;
  TEST_ASSERT_NULL(lc.find("Anything"));
}

// ── ModuleLifecycle summary ────────────────────────────────────────────────

static void test_summary_all_running() {
  ModuleLifecycle<4> lc;
  MockModule a, b;
  lc.registerModule(&a);
  lc.registerModule(&b);
  lc.initializeAll();
  auto s = lc.summary();
  TEST_ASSERT_EQUAL(2, s.total);
  TEST_ASSERT_EQUAL(2, s.running);
  TEST_ASSERT_EQUAL(0, s.failed);
}

static void test_summary_counts_failed() {
  ModuleLifecycle<4> lc;
  MockModule bad, good;
  bad.initResult = false;
  lc.registerModule(&bad);
  lc.registerModule(&good);
  lc.initializeAll();
  auto s = lc.summary();
  TEST_ASSERT_EQUAL(1, s.failed);
  TEST_ASSERT_EQUAL(1, s.running);
}

static void test_summary_counts_stopped_after_shutdown() {
  ModuleLifecycle<4> lc;
  MockModule m;
  lc.registerModule(&m);
  lc.initializeAll();
  lc.shutdownAll();
  auto s = lc.summary();
  TEST_ASSERT_EQUAL(1, s.stopped);
  TEST_ASSERT_EQUAL(0, s.running);
}

static void test_summary_empty_lifecycle() {
  ModuleLifecycle<4> lc;
  auto s = lc.summary();
  TEST_ASSERT_EQUAL(0, s.total);
  TEST_ASSERT_EQUAL(0, s.running);
}

// ── Dependency resolution ─────────────────────────────────────────────────

static void test_dependency_no_deps_initializes_normally() {
  DependentModule::orderCounter = 0;
  ModuleLifecycle<4> lc;
  DependentModule a("A", nullptr);
  lc.registerModule(&a);
  lc.initializeAll();
  TEST_ASSERT_TRUE(a.isRunning());
}

static void test_dependency_resolves_before_dependent() {
  DependentModule::orderCounter = 0;
  ModuleLifecycle<4> lc;
  // Register B first, but B depends on A — so A must init before B.
  DependentModule b("B", "A");
  DependentModule a("A", nullptr);
  lc.registerModule(&b);
  lc.registerModule(&a);
  lc.initializeAll();
  TEST_ASSERT_LESS_THAN(b.initOrder, a.initOrder);  // a.initOrder < b.initOrder
}

static void test_dependency_chain_a_before_b_before_c() {
  DependentModule::orderCounter = 0;
  ModuleLifecycle<8> lc;
  DependentModule c("C", "B");
  DependentModule b("B", "A");
  DependentModule a("A", nullptr);
  lc.registerModule(&c);
  lc.registerModule(&b);
  lc.registerModule(&a);
  lc.initializeAll();
  TEST_ASSERT_LESS_THAN(b.initOrder, a.initOrder);
  TEST_ASSERT_LESS_THAN(c.initOrder, b.initOrder);
}

static void test_dependency_missing_dep_still_initializes() {
  DependentModule::orderCounter = 0;
  ModuleLifecycle<4> lc;
  DependentModule a("A", "NotRegistered");
  lc.registerModule(&a);
  lc.initializeAll();
  TEST_ASSERT_TRUE(a.isRunning());
}

// ── restartModule ─────────────────────────────────────────────────────────

static void test_restart_module_reinitializes() {
  ModuleLifecycle<4> lc;
  MockModule m;
  lc.registerModule(&m);
  lc.initializeAll();
  TEST_ASSERT_EQUAL(1, m.initCalls);
  bool ok = lc.restartModule("MockModule");
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL(2, m.initCalls);  // shutdown + initialize called again
  TEST_ASSERT_TRUE(m.isRunning());
}

static void test_restart_unknown_module_returns_false() {
  ModuleLifecycle<4> lc;
  TEST_ASSERT_FALSE(lc.restartModule("Ghost"));
}

static void test_restart_failed_module_recovers() {
  ModuleLifecycle<4> lc;
  MockModule m;
  m.initResult = false;
  lc.registerModule(&m);
  lc.initializeAll();
  TEST_ASSERT_TRUE(m.hasError());
  m.initResult = true;
  lc.restartModule("MockModule");
  TEST_ASSERT_TRUE(m.isRunning());
}

// ── autoRestart ───────────────────────────────────────────────────────────

static void test_auto_restart_default_off() {
  ModuleLifecycle<4> lc;
  TEST_ASSERT_FALSE(lc.autoRestart());
}

static void test_set_auto_restart() {
  ModuleLifecycle<4> lc;
  lc.setAutoRestart(true);
  TEST_ASSERT_TRUE(lc.autoRestart());
}

static void test_auto_restart_recovers_error_in_update() {
  ModuleLifecycle<4> lc;
  lc.setAutoRestart(true);
  FlakyModule f;
  lc.registerModule(&f);
  lc.initializeAll();
  TEST_ASSERT_EQUAL(1, f.initCalls);
  f.failOnUpdate = true;
  lc.updateAll();  // sets Error state, then auto-restarts
  TEST_ASSERT_EQUAL(2, f.initCalls);
  TEST_ASSERT_TRUE(f.isRunning());
}

static void test_no_auto_restart_leaves_error_state() {
  ModuleLifecycle<4> lc;
  lc.setAutoRestart(false);
  FlakyModule f;
  lc.registerModule(&f);
  lc.initializeAll();
  f.failOnUpdate = true;
  lc.updateAll();
  TEST_ASSERT_EQUAL(1, f.initCalls);
  TEST_ASSERT_TRUE(f.hasError());
}

// ── Main ───────────────────────────────────────────────────────────────────

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_initial_state_uninitialized);
  RUN_TEST(test_initialize_transitions_to_running);
  RUN_TEST(test_failed_initialize_transitions_to_error);
  RUN_TEST(test_shutdown_transitions_to_stopped);
  RUN_TEST(test_is_not_running_when_stopped);
  RUN_TEST(test_is_not_error_when_ok);

  RUN_TEST(test_register_increases_count);
  RUN_TEST(test_register_full_returns_false);
  RUN_TEST(test_register_null_returns_false);
  RUN_TEST(test_capacity_matches_template_param);

  RUN_TEST(test_initialize_all_calls_each_module);
  RUN_TEST(test_initialize_all_returns_true_when_all_succeed);
  RUN_TEST(test_initialize_all_returns_false_when_required_fails);
  RUN_TEST(test_initialize_all_returns_true_when_optional_fails);
  RUN_TEST(test_initialize_all_continues_after_optional_failure);

  RUN_TEST(test_update_all_calls_running_modules);
  RUN_TEST(test_update_all_skips_non_running_modules);
  RUN_TEST(test_update_all_multiple_times);

  RUN_TEST(test_shutdown_all_calls_each_running_module);
  RUN_TEST(test_shutdown_all_skips_non_running_modules);

  RUN_TEST(test_find_returns_module_by_name);
  RUN_TEST(test_find_returns_null_for_missing_name);
  RUN_TEST(test_find_empty_lifecycle_returns_null);

  RUN_TEST(test_summary_all_running);
  RUN_TEST(test_summary_counts_failed);
  RUN_TEST(test_summary_counts_stopped_after_shutdown);
  RUN_TEST(test_summary_empty_lifecycle);

  RUN_TEST(test_dependency_no_deps_initializes_normally);
  RUN_TEST(test_dependency_resolves_before_dependent);
  RUN_TEST(test_dependency_chain_a_before_b_before_c);
  RUN_TEST(test_dependency_missing_dep_still_initializes);

  RUN_TEST(test_restart_module_reinitializes);
  RUN_TEST(test_restart_unknown_module_returns_false);
  RUN_TEST(test_restart_failed_module_recovers);

  RUN_TEST(test_auto_restart_default_off);
  RUN_TEST(test_set_auto_restart);
  RUN_TEST(test_auto_restart_recovers_error_in_update);
  RUN_TEST(test_no_auto_restart_leaves_error_state);

  return UNITY_END();
}

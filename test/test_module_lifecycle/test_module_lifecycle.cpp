// test_module_lifecycle.cpp — Unit tests for IModule and ModuleLifecycle.
//
// All tests run on native (no Arduino dependencies).

#include "unity.h"
#include "core/IModule.h"
#include "core/ModuleLifecycle.h"
#include <stdio.h>

void setUp() {}
void tearDown() {}

// ── Minimal concrete module for testing ────────────────────────────────────

struct MockModule : public IModule {
  bool initResult = true;
  int initCalls = 0;
  int updateCalls = 0;
  int shutdownCalls = 0;
  char label[16];

  MockModule() {
    static int counter = 0;
    snprintf(label, sizeof(label), "Mock%d", counter++);
  }

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
  const char *name() const override { return label; }
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
  ModuleLifecycle<4> lc;
  MockModule a, b, c, d, e;
  lc.registerModule(&a);
  lc.registerModule(&b);
  lc.registerModule(&c);
  lc.registerModule(&d);
  TEST_ASSERT_FALSE(lc.registerModule(&e));
  TEST_ASSERT_EQUAL(4, lc.count());
}

static void test_register_null_returns_false() {
  ModuleLifecycle<4> lc;
  TEST_ASSERT_FALSE(lc.registerModule(nullptr));
  TEST_ASSERT_EQUAL(0, lc.count());
}

static void test_capacity_matches_template_param() {
  ModuleLifecycle<4> lc;
  TEST_ASSERT_EQUAL(4, lc.capacity());
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

// ── Cycle detection ───────────────────────────────────────────────────────

static void test_cycle_detection_fails_cycle_modules() {
  DependentModule::orderCounter = 0;
  ModuleLifecycle<4> lc;
  // A depends on B, B depends on A — cycle.
  DependentModule a("A", "B");
  DependentModule b("B", "A");
  lc.registerModule(&a);
  lc.registerModule(&b);
  bool ok = lc.initializeAll();
  TEST_ASSERT_FALSE(ok);
  TEST_ASSERT_TRUE(lc.cycleDetected());
  TEST_ASSERT_TRUE(a.hasError());
  TEST_ASSERT_TRUE(b.hasError());
}

static void test_cycle_independent_modules_still_run() {
  DependentModule::orderCounter = 0;
  ModuleLifecycle<4> lc;
  DependentModule indep("I", nullptr);  // no deps — not in cycle
  DependentModule a("A", "B");
  DependentModule b("B", "A");
  lc.registerModule(&indep);
  lc.registerModule(&a);
  lc.registerModule(&b);
  lc.initializeAll();
  TEST_ASSERT_TRUE(indep.isRunning());
  TEST_ASSERT_TRUE(a.hasError());
  TEST_ASSERT_TRUE(b.hasError());
}

static void test_cycle_module_count_and_names() {
  DependentModule::orderCounter = 0;
  ModuleLifecycle<4> lc;
  DependentModule a("Alpha", "Beta");
  DependentModule b("Beta", "Alpha");
  lc.registerModule(&a);
  lc.registerModule(&b);
  lc.initializeAll();
  TEST_ASSERT_EQUAL(2, (int)lc.cycleModuleCount());
  // Both names must appear (order not guaranteed).
  bool foundAlpha = false, foundBeta = false;
  for (size_t i = 0; i < lc.cycleModuleCount(); ++i) {
    const char *n = lc.cycleModuleName(i);
    if (n && strcmp(n, "Alpha") == 0)
      foundAlpha = true;
    if (n && strcmp(n, "Beta") == 0)
      foundBeta = true;
  }
  TEST_ASSERT_TRUE(foundAlpha);
  TEST_ASSERT_TRUE(foundBeta);
}

static void test_cycle_required_module_fails_overall() {
  DependentModule::orderCounter = 0;
  ModuleLifecycle<4> lc;
  DependentModule a("CycA", "CycB");
  DependentModule b("CycB", "CycA");
  lc.registerModule(&a, /*required=*/true);
  lc.registerModule(&b);
  bool ok = lc.initializeAll();
  TEST_ASSERT_FALSE(ok);
  TEST_ASSERT_TRUE(lc.cycleDetected());
}

static void test_no_cycle_flag_when_clean() {
  DependentModule::orderCounter = 0;
  ModuleLifecycle<4> lc;
  DependentModule a("A", nullptr);
  lc.registerModule(&a);
  lc.initializeAll();
  TEST_ASSERT_FALSE(lc.cycleDetected());
  TEST_ASSERT_EQUAL(0, (int)lc.cycleModuleCount());
}

static void test_cycle_name_out_of_bounds_returns_null() {
  DependentModule::orderCounter = 0;
  ModuleLifecycle<4> lc;
  DependentModule a("A", nullptr);
  lc.registerModule(&a);
  lc.initializeAll();
  TEST_ASSERT_NULL(lc.cycleModuleName(99));
}

// ── Restart backoff and circuit breaker ───────────────────────────────────

// Module that starts Running on first initialize(), but fails on all
// subsequent calls; update() always sets Error.
// Module whose name() returns nullptr — should be rejected by registerModule.
struct NullNameModule : public IModule {
  bool initialize() override {
    setRunning();
    return true;
  }
  void update() override {}
  void shutdown() override { setStopped(); }
  const char *name() const override { return nullptr; }
  const char *version() const override { return "1.0"; }
  const char *description() const override { return ""; }
};

// Module with dependencyCount=1 but dependency(0) returns nullptr.
// Exercises the defensive null guard inside topoSort's inner loop.
struct NullDepModule : public IModule {
  const char *label;
  explicit NullDepModule(const char *n) : label(n) {}
  bool initialize() override {
    setRunning();
    return true;
  }
  void update() override {}
  void shutdown() override { setStopped(); }
  const char *name() const override { return label; }
  const char *version() const override { return "1.0"; }
  const char *description() const override { return ""; }
  size_t dependencyCount() const override { return 1; }
  const char *dependency(size_t /*i*/) const override { return nullptr; }
};

// Module that succeeds on odd initialize() calls and fails on even ones.
// Used to drive many restart attempts without opening the circuit.
struct AlternatingModule : public IModule {
  int initCalls = 0;
  bool initialize() override {
    ++initCalls;
    if (initCalls % 2 == 1) {
      setRunning();
      return true;
    }
    setError();
    return false;
  }
  void update() override { setError(); }
  void shutdown() override { setStopped(); }
  const char *name() const override { return "Alternating"; }
  const char *version() const override { return "1.0"; }
  const char *description() const override { return ""; }
};

struct TripwireModule : public IModule {
  int initCalls = 0;
  bool initialize() override {
    ++initCalls;
    if (initCalls == 1) {
      setRunning();
      return true;
    }
    setError();
    return false;
  }
  void update() override { setError(); }
  void shutdown() override { setStopped(); }
  const char *name() const override { return "Tripwire"; }
  const char *version() const override { return "1.0"; }
  const char *description() const override { return ""; }
};

static void test_circuit_opens_after_max_restarts() {
  // TripwireModule starts OK then consistently fails on restart.
  // updateAll retries Error-state modules; circuit opens after kMaxRestarts.
  ModuleLifecycle<4> lc;
  lc.setAutoRestart(true);
  TripwireModule tw;
  lc.registerModule(&tw);
  lc.initializeAll();  // initCalls=1, Running

  uint32_t t = 0;
  for (uint32_t i = 0; i < ModuleLifecycle<4>::kMaxRestarts * 4u; ++i) {
    t += 100000u;
    lc.updateAll(t);
    if (lc.isCircuitOpen("Tripwire"))
      break;
  }
  TEST_ASSERT_TRUE(lc.isCircuitOpen("Tripwire"));
}

static void test_restart_backoff_blocks_too_soon() {
  // FlakyModule succeeds on restart but fails again on the next update.
  // After the first restart (immediate at t=0), the second needs backoff.
  ModuleLifecycle<4> lc;
  lc.setAutoRestart(true);
  FlakyModule flaky;
  lc.registerModule(&flaky);
  lc.initializeAll();  // initCalls=1
  flaky.failOnUpdate = true;

  lc.updateAll(0);                        // update→Error, restart→initCalls=2
  TEST_ASSERT_EQUAL(2, flaky.initCalls);  // initial + 1 restart

  // Module Running again; next update sets Error. Second restart needs
  // backoff=1000ms, so it should be blocked at t=500.
  lc.updateAll(500);
  TEST_ASSERT_EQUAL(2, flaky.initCalls);  // no new restart
}

static void test_manual_restart_resets_circuit() {
  // Open the circuit on a TripwireModule, then verify manual restart clears it.
  ModuleLifecycle<4> lc;
  lc.setAutoRestart(true);
  TripwireModule tw;
  lc.registerModule(&tw);
  lc.initializeAll();

  uint32_t t = 0;
  for (uint32_t i = 0; i < 20u; ++i) {
    t += 100000u;
    lc.updateAll(t);
    if (lc.isCircuitOpen("Tripwire"))
      break;
  }
  TEST_ASSERT_TRUE(lc.isCircuitOpen("Tripwire"));

  // Manual restart resets the circuit breaker state.
  lc.restartModule("Tripwire");
  TEST_ASSERT_FALSE(lc.isCircuitOpen("Tripwire"));
}

// ── Hardened registration ──────────────────────────────────────────────────

static void test_register_null_module_name_rejected() {
  ModuleLifecycle<4> lc;
  NullNameModule nm;
  TEST_ASSERT_FALSE(lc.registerModule(&nm));
  TEST_ASSERT_EQUAL(0, (int)lc.count());
}

static void test_stopped_module_not_restarted_by_auto_restart() {
  // A Stopped module (not in Error) is never retried even when autoRestart is on.
  ModuleLifecycle<4> lc;
  lc.setAutoRestart(true);
  MockModule m;
  lc.registerModule(&m);
  lc.initializeAll();
  lc.shutdownAll();  // m is now Stopped, not Error
  int prevInitCalls = m.initCalls;
  lc.updateAll(0);
  TEST_ASSERT_EQUAL(prevInitCalls, m.initCalls);
}

static void test_no_auto_restart_no_retry_on_error() {
  // With autoRestart off, modules stuck in Error state are never retried.
  ModuleLifecycle<4> lc;
  lc.setAutoRestart(false);
  FlakyModule f;
  lc.registerModule(&f);
  lc.initializeAll();
  f.failOnUpdate = true;
  lc.updateAll();  // update sets Error
  TEST_ASSERT_TRUE(f.hasError());
  lc.updateAll();  // Error module is NOT retried — still Error, initCalls unchanged
  TEST_ASSERT_EQUAL(1, f.initCalls);
  TEST_ASSERT_TRUE(f.hasError());
}

static void test_restart_count_and_circuit_accessors_on_unknown() {
  ModuleLifecycle<4> lc;
  TEST_ASSERT_EQUAL(0u, lc.restartCount("Ghost"));
  TEST_ASSERT_FALSE(lc.isCircuitOpen("Ghost"));
  TEST_ASSERT_FALSE(lc.isCircuitOpen(nullptr));
  TEST_ASSERT_EQUAL(0u, lc.restartCount(nullptr));
}

static void test_register_duplicate_pointer_rejected() {
  ModuleLifecycle<4> lc;
  MockModule m;
  TEST_ASSERT_TRUE(lc.registerModule(&m));
  TEST_ASSERT_FALSE(lc.registerModule(&m));
  TEST_ASSERT_EQUAL(1, (int)lc.count());
}

static void test_register_duplicate_name_rejected() {
  ModuleLifecycle<4> lc;
  NamedModule a("Same"), b("Same");
  TEST_ASSERT_TRUE(lc.registerModule(&a));
  TEST_ASSERT_FALSE(lc.registerModule(&b));
  TEST_ASSERT_EQUAL(1, (int)lc.count());
}

static void test_find_null_name_returns_null() {
  ModuleLifecycle<4> lc;
  MockModule m;
  lc.registerModule(&m);
  TEST_ASSERT_NULL(lc.find(nullptr));
}

static void test_restart_null_name_returns_false() {
  ModuleLifecycle<4> lc;
  TEST_ASSERT_FALSE(lc.restartModule(nullptr));
}

// ── restartModule ─────────────────────────────────────────────────────────

static void test_restart_module_reinitializes() {
  ModuleLifecycle<4> lc;
  MockModule m;
  lc.registerModule(&m);
  lc.initializeAll();
  TEST_ASSERT_EQUAL(1, m.initCalls);
  bool ok = lc.restartModule(m.name());
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
  lc.restartModule(m.name());
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

// ── Targeted src-branch coverage tests ───────────────────────────────────

static void test_circuit_open_blocks_further_auto_restarts() {
  // After the circuit opens, updateAll must NOT call tryAutoRestart for
  // modules whose circuit is open. Calling updateAll again after opening
  // exercises the circuitOpen=true early-return branch in tryAutoRestart.
  ModuleLifecycle<4> lc;
  lc.setAutoRestart(true);
  TripwireModule tw;
  lc.registerModule(&tw);
  lc.initializeAll();

  uint32_t t = 0;
  while (!lc.isCircuitOpen("Tripwire")) {
    t += 100000u;
    lc.updateAll(t);
  }
  // Capture restart count at circuit open.
  uint32_t countAtOpen = lc.restartCount("Tripwire");
  // Call updateAll multiple more times — circuit is open so no new restarts.
  lc.updateAll(t + 100000u);
  lc.updateAll(t + 200000u);
  TEST_ASSERT_EQUAL(countAtOpen, lc.restartCount("Tripwire"));
}

static void test_summary_counts_uninitialized_modules() {
  // Calling summary() before initializeAll() exercises the Uninitialized
  // (default) case of the switch in summary().
  ModuleLifecycle<4> lc;
  MockModule m;
  lc.registerModule(&m);
  auto s = lc.summary();
  TEST_ASSERT_EQUAL(1, (int)s.total);
  TEST_ASSERT_EQUAL(0, (int)s.running);
  TEST_ASSERT_EQUAL(0, (int)s.failed);
  TEST_ASSERT_EQUAL(0, (int)s.stopped);
}

static void test_two_dep_module_queued_after_both_deps_initialized() {
  // Registers modules A, B, C where C depends on BOTH A and B.
  // Kahn's BFS decrements inDegree[C] twice: 2→1 (not queued) then 1→0
  // (queued). This covers the `if (--inDegree[i] == 0)` false branch.
  ModuleLifecycle<4> lc;
  NamedModule a("DepA"), b("DepB");
  struct TwoDep : public IModule {
    bool initialize() override { setRunning(); return true; }
    void update() override {}
    void shutdown() override { setStopped(); }
    const char *name() const override { return "TwoDepC"; }
    const char *version() const override { return "1.0"; }
    const char *description() const override { return ""; }
    size_t dependencyCount() const override { return 2; }
    const char *dependency(size_t i) const override {
      return i == 0 ? "DepA" : "DepB";
    }
  } c;
  lc.registerModule(&a);
  lc.registerModule(&b);
  lc.registerModule(&c);
  TEST_ASSERT_TRUE(lc.initializeAll());
  TEST_ASSERT_FALSE(lc.cycleDetected());
  TEST_ASSERT_TRUE(a.isRunning());
  TEST_ASSERT_TRUE(b.isRunning());
  TEST_ASSERT_TRUE(c.isRunning());
}

static void test_restart_at_nonzero_index_covers_loop_false_branch() {
  // Register two modules; restart the SECOND one so the loop passes index 0
  // (strcmp returns false) before finding the match at index 1. This covers
  // the false branch of the strcmp condition in restartModule's loop.
  ModuleLifecycle<4> lc;
  MockModule first, second;
  lc.registerModule(&first);
  lc.registerModule(&second);
  lc.initializeAll();
  bool ok = lc.restartModule(second.name());
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL(2, second.initCalls);  // initial + restart
}

// ── Null-dependency and shift-cap branches ────────────────────────────────

static void test_null_dep_module_initializes_normally() {
  // NullDepModule declares dependencyCount=1 but dependency(0)=nullptr.
  // topoSort must not crash on the nullptr guard; the module should still run.
  ModuleLifecycle<4> lc;
  NullDepModule nd("NullDep");
  lc.registerModule(&nd);
  TEST_ASSERT_TRUE(lc.initializeAll());
  TEST_ASSERT_FALSE(lc.cycleDetected());
  TEST_ASSERT_TRUE(nd.isRunning());
}

static void test_null_dep_mixed_with_normal_module() {
  // A normal module + a NullDepModule: both initialize, no cycle.
  ModuleLifecycle<4> lc;
  MockModule m;
  NullDepModule nd("NullDepB");
  lc.registerModule(&m);
  lc.registerModule(&nd);
  TEST_ASSERT_TRUE(lc.initializeAll());
  TEST_ASSERT_TRUE(m.isRunning());
  TEST_ASSERT_TRUE(nd.isRunning());
}

static void test_backoff_shift_cap_at_six_restarts() {
  // Drive restartAttempts past 5 to hit the capped-shift branch (shift=5).
  // AlternatingModule succeeds on odd initialize() calls so the circuit never
  // opens while consecutiveFails resets on each success.
  ModuleLifecycle<4> lc;
  lc.setAutoRestart(true);
  AlternatingModule alt;
  lc.registerModule(&alt);
  lc.initializeAll();     // initCalls=1 (odd → Running)
  // updateAll drives update()->Error, then tryAutoRestart.
  // We use large time steps so backoff never blocks.
  uint32_t t = 0;
  for (int i = 0; i < 8; ++i) {
    t += 100000u;
    lc.updateAll(t);
  }
  // After many attempts restartAttempts must be >= 6, confirming the
  // capped-shift branch was exercised.
  TEST_ASSERT_TRUE(lc.restartCount("Alternating") >= 6u);
}

// ── Phase 1 lifecycle state tests ─────────────────────────────────────────

// Module that records its own state() at the START of initialize().
struct StateCapturingModule : public IModule {
  IModule::State stateAtInit = IModule::State::Uninitialized;
  IModule::State stateAtShutdown = IModule::State::Uninitialized;

  bool initialize() override {
    stateAtInit = state();
    setRunning();
    return true;
  }
  void update() override {}
  void shutdown() override {
    stateAtShutdown = state();
    setStopped();
  }
  const char *name() const override { return "StateCapturing"; }
  const char *version() const override { return "1.0"; }
  const char *description() const override { return ""; }
};

static void test_begin_initializing_sets_state_before_init() {
  ModuleLifecycle<4> lc;
  StateCapturingModule m;
  lc.registerModule(&m);
  lc.initializeAll();
  TEST_ASSERT_EQUAL(IModule::State::Initializing, m.stateAtInit);
}

static void test_begin_stopping_sets_state_before_shutdown() {
  ModuleLifecycle<4> lc;
  StateCapturingModule m;
  lc.registerModule(&m);
  lc.initializeAll();
  lc.shutdownAll();
  TEST_ASSERT_EQUAL(IModule::State::Stopping, m.stateAtShutdown);
}

static void test_failed_state_when_circuit_opens() {
  // After kMaxRestarts consecutive failures the circuit opens and module → Failed.
  ModuleLifecycle<4> lc;
  lc.setAutoRestart(true);
  MockModule m;
  m.initResult = false;
  lc.registerModule(&m);
  lc.initializeAll();  // first attempt → Error
  // Drive restarts with increasing timestamps so backoff never blocks.
  uint32_t t = 0;
  for (int i = 0; i < 20; ++i) {
    t += 100000u;
    lc.updateAll(t);
    if (lc.isCircuitOpen(m.name()))
      break;
  }
  TEST_ASSERT_TRUE(m.isFailed());
  TEST_ASSERT_TRUE(lc.isCircuitOpen(m.name()));
}

static void test_restart_module_clears_failed_state() {
  // After circuit opens (Failed), restartModule() with a succeeding module
  // must bring it back to Running.
  ModuleLifecycle<4> lc;
  lc.setAutoRestart(true);
  MockModule m;
  m.initResult = false;
  lc.registerModule(&m);
  lc.initializeAll();
  uint32_t t = 0;
  for (int i = 0; i < 20; ++i) {
    t += 100000u;
    lc.updateAll(t);
    if (lc.isCircuitOpen(m.name()))
      break;
  }
  TEST_ASSERT_TRUE(m.isFailed());

  m.initResult = true;
  TEST_ASSERT_TRUE(lc.restartModule(m.name()));
  TEST_ASSERT_TRUE(m.isRunning());
  TEST_ASSERT_FALSE(lc.isCircuitOpen(m.name()));
}

// Module that always fails; used for dep-failure propagation tests.
struct AlwaysFailModule : public IModule {
  const char *label;
  explicit AlwaysFailModule(const char *n) : label(n) {}
  bool initialize() override {
    setError();
    return false;
  }
  void update() override {}
  void shutdown() override { setStopped(); }
  const char *name() const override { return label; }
  const char *version() const override { return "1.0"; }
  const char *description() const override { return ""; }
};

static void test_dep_failure_propagates_to_dependent() {
  ModuleLifecycle<4> lc;
  AlwaysFailModule a("DepA");
  DependentModule b("DepB", "DepA");
  lc.registerModule(&a);
  lc.registerModule(&b);
  lc.initializeAll();
  TEST_ASSERT_TRUE(a.hasError());
  // b depends on a which failed — b must be Error, not Running
  TEST_ASSERT_TRUE(b.hasError());
  TEST_ASSERT_FALSE(b.isRunning());
}


static void test_dep_failure_does_not_call_dependent_initialize() {
  // initOrder is set only inside initialize(); if propagation skips it, it stays -1.
  DependentModule::orderCounter = 0;
  ModuleLifecycle<4> lc;
  AlwaysFailModule a("DepX");
  DependentModule b("DepY", "DepX");
  lc.registerModule(&a);
  lc.registerModule(&b);
  lc.initializeAll();
  TEST_ASSERT_EQUAL(-1, b.initOrder);
}

static void test_health_check_default_running() {
  MockModule m;
  m.initialize();
  TEST_ASSERT_TRUE(m.healthCheck());
}

static void test_health_check_default_not_running() {
  MockModule m;
  TEST_ASSERT_FALSE(m.healthCheck());
}

static void test_health_check_default_stopped() {
  MockModule m;
  m.initialize();
  m.shutdown();
  TEST_ASSERT_FALSE(m.healthCheck());
}

static void test_api_version_matches_constant() {
  MockModule m;
  TEST_ASSERT_EQUAL(IModule::kApiVersion, m.apiVersion());
}

static void test_failed_blocked_from_auto_escape() {
  // A module in Failed state cannot escape via setError() — it must stay Failed.
  MockModule m;
  m.initialize();     // → Running
  m.markError();      // → Error  (framework call, unrestricted)
  m.markFailed();     // → Failed
  // Now simulate a subclass calling setError() — must be ignored.
  m.markError();      // should be blocked because markError() checks !Failed
  TEST_ASSERT_TRUE(m.isFailed());
}

static void test_failed_counted_in_summary() {
  ModuleLifecycle<4> lc;
  lc.setAutoRestart(true);
  MockModule m;
  m.initResult = false;
  lc.registerModule(&m);
  lc.initializeAll();
  uint32_t t = 0;
  for (int i = 0; i < 20; ++i) {
    t += 100000u;
    lc.updateAll(t);
    if (lc.isCircuitOpen(m.name()))
      break;
  }
  TEST_ASSERT_TRUE(m.isFailed());
  auto s = lc.summary();
  TEST_ASSERT_EQUAL(1, s.failed);
}

static void test_is_initializing_helper() {
  MockModule m;
  m.beginInitializing();
  TEST_ASSERT_TRUE(m.isInitializing());
  TEST_ASSERT_FALSE(m.isRunning());
}

static void test_is_stopping_helper() {
  MockModule m;
  m.initialize();  // → Running
  m.beginStopping();
  TEST_ASSERT_TRUE(m.isStopping());
  TEST_ASSERT_FALSE(m.isRunning());
}

static void test_is_stopped_helper() {
  MockModule m;
  m.initialize();
  m.shutdown();
  TEST_ASSERT_TRUE(m.isStopped());
}

static void test_is_failed_helper() {
  MockModule m;
  m.initialize();
  m.markError();
  m.markFailed();
  TEST_ASSERT_TRUE(m.isFailed());
}

static void test_mark_error_blocked_from_failed() {
  MockModule m;
  m.initialize();
  m.markError();
  m.markFailed();
  TEST_ASSERT_TRUE(m.isFailed());
  m.markError();  // must be a no-op
  TEST_ASSERT_TRUE(m.isFailed());
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

  RUN_TEST(test_cycle_detection_fails_cycle_modules);
  RUN_TEST(test_cycle_independent_modules_still_run);
  RUN_TEST(test_cycle_module_count_and_names);
  RUN_TEST(test_cycle_required_module_fails_overall);
  RUN_TEST(test_no_cycle_flag_when_clean);
  RUN_TEST(test_cycle_name_out_of_bounds_returns_null);

  RUN_TEST(test_circuit_opens_after_max_restarts);
  RUN_TEST(test_restart_backoff_blocks_too_soon);
  RUN_TEST(test_manual_restart_resets_circuit);

  RUN_TEST(test_register_null_module_name_rejected);
  RUN_TEST(test_stopped_module_not_restarted_by_auto_restart);
  RUN_TEST(test_no_auto_restart_no_retry_on_error);
  RUN_TEST(test_restart_count_and_circuit_accessors_on_unknown);

  RUN_TEST(test_register_duplicate_pointer_rejected);
  RUN_TEST(test_register_duplicate_name_rejected);
  RUN_TEST(test_find_null_name_returns_null);
  RUN_TEST(test_restart_null_name_returns_false);

  RUN_TEST(test_restart_module_reinitializes);
  RUN_TEST(test_restart_unknown_module_returns_false);
  RUN_TEST(test_restart_failed_module_recovers);

  RUN_TEST(test_auto_restart_default_off);
  RUN_TEST(test_set_auto_restart);
  RUN_TEST(test_auto_restart_recovers_error_in_update);
  RUN_TEST(test_no_auto_restart_leaves_error_state);

  RUN_TEST(test_circuit_open_blocks_further_auto_restarts);
  RUN_TEST(test_summary_counts_uninitialized_modules);
  RUN_TEST(test_two_dep_module_queued_after_both_deps_initialized);
  RUN_TEST(test_restart_at_nonzero_index_covers_loop_false_branch);

  RUN_TEST(test_null_dep_module_initializes_normally);
  RUN_TEST(test_null_dep_mixed_with_normal_module);
  RUN_TEST(test_backoff_shift_cap_at_six_restarts);

  RUN_TEST(test_begin_initializing_sets_state_before_init);
  RUN_TEST(test_begin_stopping_sets_state_before_shutdown);
  RUN_TEST(test_failed_state_when_circuit_opens);
  RUN_TEST(test_restart_module_clears_failed_state);
  RUN_TEST(test_dep_failure_propagates_to_dependent);
  RUN_TEST(test_dep_failure_does_not_call_dependent_initialize);
  RUN_TEST(test_health_check_default_running);
  RUN_TEST(test_health_check_default_not_running);
  RUN_TEST(test_health_check_default_stopped);
  RUN_TEST(test_api_version_matches_constant);
  RUN_TEST(test_failed_blocked_from_auto_escape);
  RUN_TEST(test_failed_counted_in_summary);
  RUN_TEST(test_is_initializing_helper);
  RUN_TEST(test_is_stopping_helper);
  RUN_TEST(test_is_stopped_helper);
  RUN_TEST(test_is_failed_helper);
  RUN_TEST(test_mark_error_blocked_from_failed);

  return UNITY_END();
}

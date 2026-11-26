#pragma once
#include "IModule.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// ModuleLifecycle — manages a fixed-capacity registry of IModule instances.
//
// Features
//   - Dependency-aware initialization: modules are initialized in topological
//     order based on dependency() declarations. Cycle detection fails affected
//     modules and reports their names; independent modules still run.
//   - Dependency failure propagation: if any declared dependency is in Error
//     or Failed state after its own initialization attempt, the dependent
//     module is also marked Error and skipped, preventing partial initialization
//     of a module whose prerequisites are unavailable.
//   - Failure isolation: a failed module does not prevent others from starting
//     unless it is marked required.
//   - Auto-restart with backoff: modules that enter Error state during
//     updateAll() are automatically restarted with exponential backoff.
//     After kMaxRestarts consecutive failed restarts the circuit opens,
//     the module is marked Failed, and no further automatic restarts occur.
//   - Manual restart: restartModule(name) resets the circuit breaker, clears
//     the Failed state, and performs an immediate recovery attempt.
//   - Lifecycle state driven by framework: ModuleLifecycle calls
//     beginInitializing() / beginStopping() / markFailed() on each module so
//     the IModule state machine always reflects the current operation.
//
// Usage
//   ModuleLifecycle<8> lifecycle;
//   lifecycle.registerModule(&diagEngine,  /*required=*/true);
//   lifecycle.registerModule(&wifiScanner, /*required=*/false);
//   if (!lifecycle.initializeAll()) { /* handle required failure or cycle */ }
//   while (true) lifecycle.updateAll(millis());
//   lifecycle.shutdownAll();

template <size_t N>
class ModuleLifecycle {
 public:
  static constexpr uint32_t kMaxRestarts = 3;
  static constexpr uint32_t kBaseBackoffMs = 1000;

  struct Registration {
    IModule *module = nullptr;
    bool required = false;
    bool cycleError = false;
    uint32_t restartAttempts = 0;   // total attempts (drives backoff timing)
    uint32_t consecutiveFails = 0;  // consecutive failures (drives circuit)
    uint32_t lastRestartMs = 0;
    bool circuitOpen = false;
  };

  struct Summary {
    size_t total = 0;
    size_t running = 0;
    size_t failed = 0;  // includes both Error and Failed states
    size_t stopped = 0;
  };

  // Add a module to the registry.
  // Rejects: null pointer, full registry, duplicate pointer, duplicate name,
  // null module name.
  bool registerModule(IModule *m, bool required = false) {
    if (!m || count_ >= N || !m->name())
      return false;
    for (size_t i = 0; i < count_; ++i) {
      if (slots_[i].module == m)
        return false;
      if (strcmp(slots_[i].module->name(), m->name()) == 0)
        return false;
    }
    slots_[count_++] = {m, required};
    return true;
  }

  // Initialize all modules in dependency-resolved order.
  //
  // Cycle-involved modules are marked Error and skipped; independent modules
  // initialize normally. If a module's dependency is in Error or Failed state
  // after its initialization attempt, the dependent is also marked Error and
  // skipped (dependency failure propagation).
  //
  // Returns false if a cycle is detected OR a required module fails.
  bool initializeAll() {
    bool ok = true;
    size_t order[N];
    size_t cycleIdx[N];
    size_t cycleCount = 0;
    size_t n = topoSort(order, cycleIdx, cycleCount);

    cycleDetected_ = (cycleCount > 0);
    cycleCount_ = cycleCount;
    for (size_t i = 0; i < cycleCount; ++i)
      cycleSlots_[i] = cycleIdx[i];

    // Fail cycle-involved modules; they cannot initialize safely.
    for (size_t i = 0; i < cycleCount; ++i) {
      slots_[cycleIdx[i]].module->beginInitializing();
      slots_[cycleIdx[i]].module->markError();
      slots_[cycleIdx[i]].cycleError = true;
      if (slots_[cycleIdx[i]].required)
        ok = false;
    }
    if (cycleDetected_)
      ok = false;

    for (size_t i = 0; i < n; ++i) {
      size_t idx = order[i];

      // Propagate failures from dependencies: if any dep is in Error or Failed,
      // skip this module rather than trying to initialize it without its prerequisites.
      if (hasFailedDep(idx)) {
        slots_[idx].module->beginInitializing();
        slots_[idx].module->markError();
        if (slots_[idx].required)
          ok = false;
        continue;
      }

      slots_[idx].module->beginInitializing();
      if (!slots_[idx].module->initialize()) {
        if (slots_[idx].required)
          ok = false;
      }
    }
    return ok;
  }

  // Tick all Running modules. If autoRestart is enabled:
  //   - Modules that enter Error during update() are restarted immediately.
  //   - Modules already in Error state (from a previous failed restart) are
  //     retried with exponential backoff (up to kMaxRestarts consecutive
  //     failures before the circuit opens and the module is marked Failed).
  // Pass nowMs = millis() on real hardware; pass 0 in tests (first restart
  // is always immediate when restartAttempts == 0).
  void updateAll(uint32_t nowMs = 0) {
    for (size_t i = 0; i < count_; ++i) {
      IModule *m = slots_[i].module;
      if (m->isRunning()) {
        m->update();
        if (autoRestart_ && m->hasError())
          tryAutoRestart(i, nowMs);
      } else if (autoRestart_ && m->hasError()) {
        tryAutoRestart(i, nowMs);
      }
    }
  }

  // Shut down all Running modules in reverse registration order.
  void shutdownAll() {
    for (size_t i = count_; i > 0; --i) {
      IModule *m = slots_[i - 1].module;
      if (m->isRunning()) {
        m->beginStopping();
        m->shutdown();
      }
    }
  }

  // Manual recovery: resets the circuit breaker, clears Failed state, and
  // immediately attempts shutdown → initialize. Returns false if name is null
  // or not found, or if re-initialization fails.
  bool restartModule(const char *name) {
    if (!name)
      return false;
    for (size_t i = 0; i < count_; ++i) {
      if (strcmp(slots_[i].module->name(), name) == 0) {
        slots_[i].circuitOpen = false;
        slots_[i].restartAttempts = 0;
        slots_[i].consecutiveFails = 0;
        slots_[i].lastRestartMs = 0;
        return doRestart(slots_[i].module);
      }
    }
    return false;
  }

  // Find a registered module by name. Returns nullptr if name is null or
  // not found.
  IModule *find(const char *name) const {
    if (!name)
      return nullptr;
    for (size_t i = 0; i < count_; ++i)
      if (strcmp(slots_[i].module->name(), name) == 0)
        return slots_[i].module;
    return nullptr;
  }

  Summary summary() const {
    Summary s;
    s.total = count_;
    for (size_t i = 0; i < count_; ++i) {
      switch (slots_[i].module->state()) {
        case IModule::State::Running:
          ++s.running;
          break;
        case IModule::State::Error:
        case IModule::State::Failed:
          ++s.failed;
          break;
        case IModule::State::Stopped:
          ++s.stopped;
          break;
        // Initializing, Stopping, Uninitialized: transient or not yet started.
        default:
          break;
      }
    }
    return s;
  }

  void setAutoRestart(bool enabled) { autoRestart_ = enabled; }
  bool autoRestart() const { return autoRestart_; }
  size_t count() const { return count_; }
  size_t capacity() const { return N; }

  // Cycle-detection accessors (valid after initializeAll()).
  bool cycleDetected() const { return cycleDetected_; }
  size_t cycleModuleCount() const { return cycleCount_; }
  const char *cycleModuleName(size_t i) const {
    if (i >= cycleCount_)
      return nullptr;
    return slots_[cycleSlots_[i]].module->name();
  }

  // Per-module restart state accessors.
  uint32_t restartCount(const char *name) const {
    if (!name)
      return 0;
    for (size_t i = 0; i < count_; ++i)
      if (strcmp(slots_[i].module->name(), name) == 0)
        return slots_[i].restartAttempts;
    return 0;
  }

  bool isCircuitOpen(const char *name) const {
    if (!name)
      return false;
    for (size_t i = 0; i < count_; ++i)
      if (strcmp(slots_[i].module->name(), name) == 0)
        return slots_[i].circuitOpen;
    return false;
  }

 private:
  // Topological sort (Kahn's algorithm). Writes init order into out[],
  // cycle-involved indices into cycleOut[], sets cycleCount.
  // Returns number of non-cycle elements written into out[].
  size_t topoSort(size_t out[N], size_t cycleOut[N], size_t &cycleCount) const {
    size_t inDegree[N] = {};
    for (size_t i = 0; i < count_; ++i) {
      IModule *m = slots_[i].module;
      for (size_t d = 0; d < m->dependencyCount(); ++d) {
        const char *dep = m->dependency(d);
        if (dep && indexOf(dep) < count_)
          ++inDegree[i];
      }
    }

    size_t queue[N];
    size_t qHead = 0, qTail = 0;
    for (size_t i = 0; i < count_; ++i)
      if (inDegree[i] == 0)
        queue[qTail++] = i;

    size_t written = 0;
    while (qHead < qTail) {
      size_t cur = queue[qHead++];
      out[written++] = cur;
      const char *curName = slots_[cur].module->name();
      for (size_t i = 0; i < count_; ++i) {
        if (inDegree[i] == 0)
          continue;
        IModule *m = slots_[i].module;
        for (size_t d = 0; d < m->dependencyCount(); ++d) {
          if (m->dependency(d) && strcmp(m->dependency(d), curName) == 0) {
            if (--inDegree[i] == 0)
              queue[qTail++] = i;
          }
        }
      }
    }

    cycleCount = 0;
    for (size_t i = 0; i < count_; ++i)
      if (inDegree[i] > 0)
        cycleOut[cycleCount++] = i;

    return written;
  }

  size_t indexOf(const char *name) const {
    for (size_t i = 0; i < count_; ++i)
      if (strcmp(slots_[i].module->name(), name) == 0)
        return i;
    return count_;
  }

  // Returns true if any declared dependency of slot idx is in Error or Failed state.
  bool hasFailedDep(size_t idx) const {
    IModule *m = slots_[idx].module;
    for (size_t d = 0; d < m->dependencyCount(); ++d) {
      const char *dep = m->dependency(d);
      if (!dep)
        continue;
      size_t depIdx = indexOf(dep);
      if (depIdx < count_ &&
          (slots_[depIdx].module->hasError() || slots_[depIdx].module->isFailed()))
        return true;
    }
    return false;
  }

  // Shutdown (if running) then initialize. Drives beginStopping / beginInitializing
  // so the IModule state machine is updated around each operation.
  bool doRestart(IModule *m) {
    if (m->isRunning()) {
      m->beginStopping();
      m->shutdown();
    }
    m->beginInitializing();
    return m->initialize();
  }

  // Auto-restart with exponential backoff and circuit breaker.
  // First restart is always immediate (restartAttempts == 0).
  // Backoff is based on total attempts; circuit on consecutive failures.
  // Successful restart resets consecutiveFails; failed restart increments it.
  // Circuit opens after kMaxRestarts consecutive failures, marking the module Failed.
  void tryAutoRestart(size_t idx, uint32_t nowMs) {
    Registration &slot = slots_[idx];
    if (slot.circuitOpen)
      return;
    if (slot.restartAttempts > 0) {
      uint32_t shift = (slot.restartAttempts - 1u) < 5u ? (slot.restartAttempts - 1u) : 5u;
      uint32_t backoff = kBaseBackoffMs << shift;
      if ((nowMs - slot.lastRestartMs) < backoff)
        return;
    }
    bool ok = doRestart(slot.module);
    ++slot.restartAttempts;
    slot.lastRestartMs = nowMs;
    if (ok) {
      slot.consecutiveFails = 0;
    } else {
      ++slot.consecutiveFails;
      if (slot.consecutiveFails >= kMaxRestarts) {
        slot.circuitOpen = true;
        slot.module->markFailed();
      }
    }
  }

  Registration slots_[N] = {};
  size_t count_ = 0;
  bool autoRestart_ = false;
  bool cycleDetected_ = false;
  size_t cycleCount_ = 0;
  size_t cycleSlots_[N] = {};
};

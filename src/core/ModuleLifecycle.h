#pragma once
#include "IModule.h"
#include <stddef.h>
#include <string.h>

// ModuleLifecycle — manages a fixed-capacity registry of IModule instances.
//
// Features
//   - Dependency-aware initialization: modules are initialized in topological
//     order based on dependency() declarations. Cycles fall back to
//     registration order.
//   - Failure isolation: a failed module does not prevent others from starting
//     unless it is marked required.
//   - Auto-restart: modules that enter Error state during updateAll() are
//     automatically restarted (shutdown → initialize) when autoRestart is on.
//   - Manual restart: restartModule(name) can recover any named module.
//
// Usage
//   ModuleLifecycle<8> lifecycle;
//   lifecycle.registerModule(&diagEngine,  /*required=*/true);
//   lifecycle.registerModule(&wifiScanner, /*required=*/false);
//   lifecycle.initializeAll();
//   while (true) lifecycle.updateAll();
//   lifecycle.shutdownAll();

template <size_t N>
class ModuleLifecycle {
 public:
  struct Registration {
    IModule *module = nullptr;
    bool required = false;
  };

  struct Summary {
    size_t total = 0;
    size_t running = 0;
    size_t failed = 0;
    size_t stopped = 0;
  };

  // Add a module to the registry. required=true makes initializeAll() fail if
  // this module cannot initialize.
  bool registerModule(IModule *m, bool required = false) {
    if (!m || count_ >= N)
      return false;
    slots_[count_++] = {m, required};
    return true;
  }

  // Initialize all modules in dependency-resolved order.
  // Returns false only if a required module fails.
  bool initializeAll() {
    bool ok = true;
    size_t order[N];
    size_t n = topoSort(order);
    for (size_t i = 0; i < n; ++i) {
      IModule *m = slots_[order[i]].module;
      if (!m->initialize()) {
        if (slots_[order[i]].required)
          ok = false;
      }
    }
    return ok;
  }

  // Tick all Running modules. If autoRestart is enabled, any module that
  // enters Error state after update() is immediately restarted.
  void updateAll() {
    for (size_t i = 0; i < count_; ++i) {
      IModule *m = slots_[i].module;
      if (!m->isRunning())
        continue;
      m->update();
      if (autoRestart_ && m->hasError())
        doRestart(m);
    }
  }

  // Shut down all Running modules in reverse registration order.
  void shutdownAll() {
    for (size_t i = count_; i > 0; --i) {
      IModule *m = slots_[i - 1].module;
      if (m->isRunning())
        m->shutdown();
    }
  }

  // Restart a module by name: shutdown (if running) then re-initialize.
  // Returns false if the module is not found or re-initialization fails.
  bool restartModule(const char *name) {
    for (size_t i = 0; i < count_; ++i) {
      if (strcmp(slots_[i].module->name(), name) == 0)
        return doRestart(slots_[i].module);
    }
    return false;
  }

  // Find a registered module by name.
  IModule *find(const char *name) const {
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
          ++s.failed;
          break;
        case IModule::State::Stopped:
          ++s.stopped;
          break;
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

 private:
  // Topological sort using Kahn's algorithm. Writes init order into out[].
  // Returns number of elements written (== count_ unless there is a cycle).
  // On cycle or missing dependency, falls back to registration order.
  size_t topoSort(size_t out[N]) const {
    // in-degree: number of dependencies each slot has that are registered.
    size_t inDegree[N] = {};
    for (size_t i = 0; i < count_; ++i) {
      IModule *m = slots_[i].module;
      for (size_t d = 0; d < m->dependencyCount(); ++d) {
        const char *dep = m->dependency(d);
        if (dep && indexOf(dep) < count_)
          ++inDegree[i];
      }
    }

    // Simple queue (array-backed, no dynamic allocation).
    size_t queue[N];
    size_t qHead = 0, qTail = 0;
    for (size_t i = 0; i < count_; ++i)
      if (inDegree[i] == 0)
        queue[qTail++] = i;

    size_t written = 0;
    while (qHead < qTail) {
      size_t cur = queue[qHead++];
      out[written++] = cur;
      // For every module that depends on slots_[cur], reduce its in-degree.
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

    if (written != count_) {
      // Cycle detected — fall back to registration order.
      for (size_t i = 0; i < count_; ++i)
        out[i] = i;
      return count_;
    }
    return written;
  }

  // Find the slot index of a module by name. Returns count_ if not found.
  size_t indexOf(const char *name) const {
    for (size_t i = 0; i < count_; ++i)
      if (strcmp(slots_[i].module->name(), name) == 0)
        return i;
    return count_;
  }

  // Shutdown (if running) then re-initialize. Returns new initialize() result.
  bool doRestart(IModule *m) {
    if (m->isRunning())
      m->shutdown();
    return m->initialize();
  }

  Registration slots_[N] = {};
  size_t count_ = 0;
  bool autoRestart_ = false;
};

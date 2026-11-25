#pragma once
#include "IModule.h"
#include <stddef.h>
#include <string.h>

// ModuleLifecycle — manages a fixed-capacity registry of IModule instances.
//
// Responsibilities
//   - Register modules before boot.
//   - Initialize all modules in registration order; failures are isolated.
//   - Tick all Running modules each loop cycle.
//   - Shut down all modules in reverse registration order on teardown.
//   - Expose lifecycle state for diagnostics and health reporting.
//
// Usage
//   ModuleLifecycle<8> lifecycle;
//   lifecycle.registerModule(&myModule);
//   lifecycle.initializeAll();
//   while (true) lifecycle.updateAll();
//   lifecycle.shutdownAll();

template <size_t N>
class ModuleLifecycle {
public:
    struct Registration {
        IModule* module   = nullptr;
        bool     required = false;  // true → boot fails if initialize() returns false
    };

    struct Summary {
        size_t total       = 0;
        size_t running     = 0;
        size_t failed      = 0;
        size_t stopped     = 0;
    };

    // Add a module to the registry.
    // required: if true, initializeAll() returns false when this module fails.
    // Returns false if the registry is full.
    bool registerModule(IModule* m, bool required = false) {
        if (!m || count_ >= N) return false;
        slots_[count_++] = {m, required};
        return true;
    }

    // Initialize all registered modules in order.
    // Returns false if any required module fails to initialize.
    bool initializeAll() {
        bool ok = true;
        for (size_t i = 0; i < count_; ++i) {
            IModule* m = slots_[i].module;
            if (!m->initialize()) {
                if (slots_[i].required) ok = false;
            }
        }
        return ok;
    }

    // Tick all Running modules.
    void updateAll() {
        for (size_t i = 0; i < count_; ++i) {
            IModule* m = slots_[i].module;
            if (m->isRunning()) m->update();
        }
    }

    // Shut down all modules in reverse registration order.
    void shutdownAll() {
        for (size_t i = count_; i > 0; --i) {
            IModule* m = slots_[i - 1].module;
            if (m->isRunning()) m->shutdown();
        }
    }

    // Find a registered module by name. Returns nullptr if not found.
    IModule* find(const char* name) const {
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
                case IModule::State::Running: s.running++;  break;
                case IModule::State::Error:   s.failed++;   break;
                case IModule::State::Stopped: s.stopped++;  break;
                default: break;
            }
        }
        return s;
    }

    size_t count()    const { return count_; }
    size_t capacity() const { return N; }

private:
    Registration slots_[N] = {};
    size_t       count_    = 0;
};

# Module API

Phantom32 provides a formal C++ lifecycle interface for all subsystems.
Every module implements `IModule` and is managed by `ModuleLifecycle`.

---

## IModule interface

```cpp
#include "core/IModule.h"
```

| Method | Required | Description |
|---|---|---|
| `initialize()` | Yes | Allocate resources; return `false` on failure |
| `update()` | Yes | Called from main loop; must not block |
| `shutdown()` | Yes | Release all resources |
| `name()` | Yes | Short string identifier (literal, never nullptr) |
| `version()` | Yes | Semantic version string |
| `description()` | Yes | One-line human description |

### Lifecycle states

```
Uninitialized → initialize() → Running → update()... → shutdown() → Stopped
                             └─ (failure) ──────────────────────────→ Error
```

| State | Meaning |
|---|---|
| `Uninitialized` | Before `initialize()` |
| `Running` | Active and receiving `update()` ticks |
| `Stopped` | Cleanly shut down |
| `Error` | `initialize()` failed |

### Writing a module

```cpp
#include "core/IModule.h"

class ChannelScanner : public IModule {
public:
    bool initialize() override {
        // allocate buffers, start WiFi promiscuous mode, etc.
        setRunning();
        return true;
    }

    void update() override {
        // called every loop(); scan one channel, update stats
    }

    void shutdown() override {
        // stop promiscuous mode, free memory
        setStopped();
    }

    const char* name()        const override { return "ChannelScanner"; }
    const char* version()     const override { return "1.0.0"; }
    const char* description() const override { return "802.11 channel utilization scanner"; }
};
```

---

## ModuleLifecycle

```cpp
#include "core/ModuleLifecycle.h"

// N = maximum number of modules (compile-time capacity).
ModuleLifecycle<16> lifecycle;
```

### Registration

```cpp
ChannelScanner scanner;
DiagnosticsEngine diag;

// required=true → initializeAll() fails if this module fails
lifecycle.registerModule(&diag,    /*required=*/true);
lifecycle.registerModule(&scanner, /*required=*/false);
```

### Boot sequence

```cpp
if (!lifecycle.initializeAll()) {
    // at least one required module failed
    Serial.println("Boot failed — required module could not initialize");
    return;
}
```

### Main loop

```cpp
void loop() {
    lifecycle.updateAll();  // ticks all Running modules
}
```

### Shutdown (e.g. OTA update)

```cpp
lifecycle.shutdownAll();  // reverse registration order
```

### Introspection

```cpp
auto s = lifecycle.summary();
Serial.printf("Modules: %d running, %d failed, %d stopped\n",
    s.running, s.failed, s.stopped);

IModule* m = lifecycle.find("ChannelScanner");
if (m && m->isRunning()) { /* ... */ }
```

---

## Design rules

1. **No global state.** Modules must be constructible without side effects.
2. **Idempotent initialize.** Calling it twice must not leak.
3. **Non-blocking update.** `update()` must return within one loop budget
   (~10 ms). Expensive work uses a state machine or FreeRTOS task.
4. **Safe shutdown.** `shutdown()` must be callable even after a failed
   `initialize()`.
5. **Testable.** Modules must compile and run on native (host) with minimal
   stubs. See `test/test_module_lifecycle/` for patterns.

---

## Running module tests

```bash
pio test -e native --filter test_module_lifecycle
pio test -e native_asan --filter test_module_lifecycle
```

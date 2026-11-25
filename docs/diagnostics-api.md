# Diagnostics API

The `DiagnosticsEngine` is a portable, independently testable subsystem that
provides real-time health monitoring for Phantom32 firmware.

---

## Overview

| Feature | Detail |
|---|---|
| Header | `src/core/DiagnosticsEngine.h` |
| Base class | `IModule` |
| Platform dependency | None (guarded by `PHANTOM32_NATIVE_TEST`) |
| Test coverage | `test/test_diagnostics/` (36 tests) |
| JSON endpoint | `GET /api/diagnostics` |

---

## Quick start

```cpp
#include "core/DiagnosticsEngine.h"

DiagnosticsEngine diag;

// Optional: customize thresholds before initialize()
diag.warnHeapBytes     = 20000;   // WARN below this many free bytes
diag.criticalHeapBytes =  8000;   // CRITICAL below this

diag.initialize();   // → State::Running

// Take a snapshot
auto snap = diag.snapshot();
Serial.printf("Heap free: %u  Health: %s\n",
    snap.heap_free,
    DiagnosticsEngine::healthString(snap.health));

// Serialize to JSON
char buf[512];
diag.toJson(buf, sizeof(buf));
server.send(200, "application/json", buf);
```

---

## Snapshot fields

| Field | Type | Description |
|---|---|---|
| `uptime_ms` | `uint64_t` | Milliseconds since boot |
| `heap_free` | `uint32_t` | Free heap bytes (current) |
| `heap_min` | `uint32_t` | Minimum ever free heap (high-water mark) |
| `heap_total` | `uint32_t` | Total heap bytes |
| `psram_free` | `uint32_t` | Free PSRAM bytes (0 if no PSRAM) |
| `psram_total` | `uint32_t` | Total PSRAM bytes (0 if no PSRAM) |
| `reset_reason` | `const char*` | Last reset cause (see below) |
| `health` | `HealthLevel` | `OK`, `WARN`, or `CRITICAL` |

---

## Reset reason values

| Value | Cause |
|---|---|
| `POWERON` | Normal power-on |
| `SOFTWARE` | Software-initiated reset |
| `PANIC` | Firmware panic / exception |
| `INT_WDT` | Interrupt watchdog timeout |
| `TASK_WDT` | Task watchdog timeout |
| `WDT` | Other watchdog |
| `BROWNOUT` | Supply voltage brownout |
| `SDIO` | SDIO reset |
| `UNKNOWN` | Unrecognized reset cause |

---

## Health thresholds

```cpp
diag.warnHeapBytes     = 20000;  // default: heap_free < 20 kB → WARN
diag.criticalHeapBytes =  8000;  // default: heap_free <  8 kB → CRITICAL
```

Thresholds are strictly less than: `heap_free < threshold → escalate`.
Adjust before calling `initialize()`.

---

## JSON output format

`toJson(buf, capacity)` writes a null-terminated JSON object:

```json
{
  "firmware":    "3.2.1",
  "board":       "doom_s3",
  "uptime_ms":   12345678,
  "heap_free":   120000,
  "heap_min":    98000,
  "heap_total":  327680,
  "heap_pct":    63,
  "psram_free":  7340032,
  "psram_total": 8388608,
  "reset_reason":"POWERON",
  "health":      "OK"
}
```

Returns the number of bytes written (excluding null), or `0` on truncation.
A 512-byte buffer is sufficient for all current fields.

---

## Testing without hardware

The engine is fully testable on a host machine. All ESP32 platform calls are
replaced by a mock layer when `PHANTOM32_NATIVE_TEST` is defined:

```bash
pio test -e native --filter test_diagnostics
```

The mock platform exposes mutable globals for precise control:

```cpp
ph_platform::_mock_free  = 5000;    // simulate low-memory condition
ph_platform::_mock_reset = "PANIC"; // simulate panic reset
```

---

## Extending DiagnosticsEngine

To add a new metric:

1. Add a field to `Snapshot`.
2. Populate it in `snapshot()` via the platform layer.
3. Add a platform stub to the `PHANTOM32_NATIVE_TEST` block.
4. Serialize it in `toJson()`.
5. Add tests to `test_diagnostics.cpp`.

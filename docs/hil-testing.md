# Hardware-in-the-Loop (HIL) Testing

This document describes the current state of hardware validation, what a full
HIL test pipeline would add, the reference hardware setup, and the CI workflow
stub (`hil.yml`) that shows how automated HIL fits into the existing pipeline.

---

## Current state

The CI pipeline runs three layers of automated testing on every push, none of
which require physical hardware:

| Layer | What it covers | What it misses |
|---|---|---|
| Native unit tests (1461 tests) | Logic, security paths, edge cases | Real WiFi/BLE driver behavior |
| AddressSanitizer + UBSan | Memory safety, undefined behavior | Hardware-specific timing |
| Firmware build matrix (13 boards) | Compilation correctness, link-time memory budgets | Runtime correctness on target |

The gap is runtime behavior on physical silicon: promiscuous callback
scheduling, actual heap consumption at steady state, 802.11 frame injection
latency, NVS read/write reliability, and watchdog behavior under sustained
load.

The `docs/hardware-validation.md` document records manual validation results
from the ESP32-WROOM-32D reference board (boot banner output, heap at boot,
flash/DRAM utilization). HIL testing automates this layer.

---

## What HIL adds

A HIL test run flashes firmware to a real board, connects to it over serial or
WiFi, and asserts expected behavior against live output. It catches:

- **Driver regressions** — a code change that compiles cleanly but causes the
  promiscuous sniffer callback to deadlock or the BLE stack to fault.
- **Heap regressions** — features that compile within the static budget but
  exhaust the runtime heap under realistic load.
- **NVS correctness** — credential storage, session persistence, and
  partition-table correctness cannot be exercised with native stubs.
- **Timing correctness** — rate limiters, session expiry, and channel hopper
  intervals depend on real `millis()` / `esp_timer_get_time()` clock sources.
- **Boot reliability** — the firmware must boot cleanly on all 13 target
  boards, not just compile for them.

---

## Reference HIL setup

The minimum hardware for a HIL test cell:

```
┌─────────────────────────────────────────────────────┐
│  Linux runner (GitHub self-hosted or Raspberry Pi)  │
│                                                     │
│  esptool.py ──────── USB-UART ──────── ESP32 DUT    │
│  serial monitor ─────────────────────────────────── │
│  Python test script ──── serial/WiFi assertions ─── │
│                                                     │
│  Optional: 802.11 monitor NIC ────────────────────  │
│            (verify injected frames on the air)      │
└─────────────────────────────────────────────────────┘
```

**Required:**
- Self-hosted GitHub Actions runner on Linux with USB access
- CP2102 or CH340 USB-UART bridge to the DUT (or native USB on S3/C3)
- `esptool.py` (included in PlatformIO toolchain)
- Python 3.x + `pyserial` for serial test harness

**Optional for frame-injection validation:**
- Second ESP32 acting as a monitor-mode sniffer, or a USB WiFi adapter
  that supports monitor mode (e.g. Alfa AWUS036ACH with rtl8812au)

---

## HIL test categories

### Boot validation

Assert that firmware boots cleanly and prints the expected banner:

```
[doom] Chip: ESP32 rev3 @ 240 MHz  Free heap: XXXXXX bytes
[doom] Flash: X.X MB  DRAM used: XX.X%
[doom] Auth: PBKDF2-HMAC-SHA256 / 100000 iter  Salt: ...
[doom] PhantomDoctor: PASS (N checks)
[doom] Phantom32 ready.
```

Fail if:
- Boot takes more than 10 seconds (watchdog firing or loop stall)
- `PhantomDoctor: FAIL` or `PhantomDoctor: INCOMPLETE` appears
- Free heap at boot is below 100 KB (regression in static footprint)

### Heap stability under load

Start one attack tool, run for 60 seconds, assert:
- Free heap never drops below 32 KB (warning threshold)
- Heap watermark after 60 s is within 20% of heap at start (no sustained leak)

### NVS round-trip

Write a credential, restart the board, read it back over the serial CLI,
assert the hash matches.

### Rate limiter enforcement

Send 300 HTTP requests in 1 second from the test runner; assert that the
response code and rate-limit headers reflect enforcement after the 100 req/s
per-IP limit is hit.

### Session expiry

Authenticate, wait 31 minutes (or spoof the clock via a debug API if one is
wired), attempt a protected API call, assert 401.

---

## CI workflow stub

The file `.github/workflows/hil.yml` defines the HIL job. It is **disabled
by default** (`on: workflow_dispatch` only) until a self-hosted runner with
hardware access is registered.

To enable it on every push to `main`, change the trigger to:

```yaml
on:
  push:
    branches: [ main ]
  workflow_dispatch:
```

And register a self-hosted runner with the label `hil-esp32` on the machine
that has the DUT connected.

The stub is in `.github/workflows/hil.yml`.

---

## Mapping HIL to the existing test matrix

| Requirement | Native test | HIL test |
|---|---|---|
| Session token expiry at boundary | `test_session_expiry_*` | Session expiry (clock-based) |
| IP lockout after 5 failed attempts | `test_lockout_*` | Rate limiter enforcement |
| PBKDF2 determinism | `test_pbkdf2_*` | NVS round-trip |
| PhantomDoctor allPassed() | `test_all_passed_*` | Boot validation |
| RecoveryEngine state transitions | `test_high_failure_safe_mode` | Heap stability → LowHeap trigger |
| Heap above critical at boot | — (firmware integration) | Boot validation |
| NVS credential persistence | — (NVS stub only) | NVS round-trip |

The native tests cover the logic layer. HIL covers the integration layer where
the two meet hardware.

---

## Running HIL locally

With a DUT connected and esptool.py on PATH:

```bash
# 1. Flash the firmware
pio run -e doom --target upload

# 2. Run the serial test harness
python3 scripts/hil_runner.py --port /dev/ttyUSB0 --baud 115200

# 3. Check the report
cat hil_report.json
```

`scripts/hil_runner.py` is the reference serial harness (to be written when a
self-hosted runner is provisioned). Its interface is documented in the stub
workflow.

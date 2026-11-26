# Phantom32 Architecture

## Overview

Phantom32 is a Wi-Fi security research firmware for ESP32 microcontrollers,
built on the PlatformIO/Arduino framework. It exposes a local HTTP web UI that
lets an authorized operator configure and run 802.11 research tools.

```
                   ┌────────────────────────────────────────┐
                   │  ESP32 (Phantom32 Firmware)            │
                   │                                        │
  Operator         │  ┌─────────┐   ┌────────────────────┐ │
  browser  ──────▶ │  │  AP     │──▶│   WebServer :80    │ │
  192.168.4.x      │  │Phantom32│   │   (auth_manager.h) │ │
                   │  └─────────┘   └──────────┬─────────┘ │
                   │                           │            │
                   │  ┌───────────────────────▼──────────┐ │
                   │  │         Route Dispatcher          │ │
                   │  │   input_validator + rate_limiter  │ │
                   │  └──────────┬────────────────────────┘ │
                   │             │                          │
  802.11 frames    │  ┌──────────▼─────────────────────┐   │
  ◀──────────────  │  │  Tool Modules (ToolModule[])   │   │
  ──────────────▶  │  │  beacon_spam, deauth,          │   │
                   │  │  evil_twin, probe_flood, ...   │   │
                   │  └───────────────────────────────-┘   │
                   │                                        │
                   │  ┌──────────────────────────────────┐  │
                   │  │  FreeRTOS Tasks                  │  │
                   │  │  - wifi_tx_task (frame injection)│  │
                   │  │  - promisc_task (frame capture)  │  │
                   │  │  - webserver_task (HTTP)         │  │
                   │  └──────────────────────────────────┘  │
                   └────────────────────────────────────────┘
```

---

## Module Map

### Core Modules

| File | Purpose |
|------|---------|
| `src/main.cpp` | Setup, loop, FreeRTOS task init, AP config |
| `src/extended_modules.h` | Analytics module registry; guarded by `PHANTOM32_LITE` |

### Security Modules (new)

| File | Purpose |
|------|---------|
| `src/auth_manager.h` | HTTP authentication (session tokens, lockout, login portal) |
| `src/rate_limiter_adv.h` | TX and HTTP rate limiting via token bucket algorithm |
| `src/input_validator.h` | Input sanitization and validation for all web UI inputs |

### Tool Modules

| File | Tool Name | Description |
|------|-----------|-------------|
| `src/beacon_spam.h` | Beacon Spam | Flood nearby clients with fake AP beacons |
| `src/deauth.h` | Deauth | Send targeted deauthentication frames |
| `src/evil_twin.h` | Evil Twin | Rogue AP + captive portal to capture credentials |
| `src/probe_flood.h` | Probe Flood | Send fake probe request frames for a target SSID pool |
| `src/probe_harvest.h` | Probe Harvest | Passive collection of probe requests from nearby stations |
| `src/device_tracker.h` | Device Tracker | Passive STA/AP detection, RSSI history, vendor lookup |
| `src/channel_hopper.h` | Channel Hopper | Automated channel scanning |
| `src/aegis.h` | Aegis | Passive 802.11 anomaly detection (deauth storm, rogue AP) |
| `src/karma_attack.h` | KARMA | Auto-respond to directed probe requests (KARMA attack) |
| `src/pmkid.h` | PMKID | PMKID frame capture assistance |
| `src/multicast_tracker.h` | Multicast Tracker | Track multicast/broadcast frame patterns |
| `src/scan_result_cache.h` | Scan Cache | Cache and age out active AP scan results |

### Analytics Modules (PSRAM only)

The following modules are compiled only when `BOARD_HAS_PSRAM` is defined
(and therefore `PHANTOM32_LITE` is NOT defined). They require > 100 KB RAM.

- `src/frame_stats.h` — per-tool TX frame counters, PPS graph
- `src/rssi_map.h` — per-BSSID RSSI history with 128-sample ring buffer
- `src/sequence_tracker.h` — 802.11 sequence number tracking for de-duplicate
- `src/association_tracker.h` — STA↔AP association state machine
- ... (see `src/extended_modules.h` for full list)

---

## Memory Layout

### Non-PSRAM Boards (`PHANTOM32_LITE`)

```
ESP32-WROOM-32D — 320 KB DRAM total
├── ~40 KB: Arduino/FreeRTOS stack + heap
├── ~32 KB: WebServer + HTTP buffers
├── ~12 KB: beacon_spam, deauth (tool frames)
├── ~15 KB: evil_twin (8 creds × 97 B + 16 log × 68 B)
├── ~12 KB: probe_flood (8 clients × PflTarget[4])
├── ~10 KB: device_tracker (20 devices × ~479 B)
├── ~8  KB: probe_harvest (16 entries)
└── ~114 KB used (35.6% of 320 KB) ✓
```

### PSRAM Boards

PSRAM boards (WROVER, S3, etc.) have 4–8 MB of additional PSRAM. Analytics
modules allocate their large buffers via `ps_malloc()` into PSRAM, keeping
DRAM pressure low.

---

## HTTP Authentication Flow

```
Client              Phantom32
  │                     │
  │  GET /               │
  │─────────────────────▶│
  │  302 → /login        │
  │◀─────────────────────│
  │                     │
  │  POST /login         │
  │  {password: ...}     │
  │─────────────────────▶│
  │                     │── verify password (constant-time)
  │                     │── check IP lockout
  │                     │── generate 256-bit token
  │                     │── set-cookie: phantom32_token=<token>
  │  200 OK + cookie     │
  │◀─────────────────────│
  │                     │
  │  GET /api/status     │
  │  Cookie: phantom32_token=<token>
  │─────────────────────▶│
  │                     │── auth_check() validates token
  │  200 JSON response   │
  │◀─────────────────────│
```

---

## Rate Limiting Architecture

```
Every esp_wifi_80211_tx() call:
  rla_check("tool_name")
       │
       ├── Global token bucket (200 fps combined)
       │   tokens -= 1
       │   if tokens < 0 → DENY (drop frame)
       │
       └── Per-tool token bucket (configurable per tool)
           tokens -= 1
           if tokens < 0 → DENY (drop frame)

Every HTTP request:
  rla_check_http(remote_ip)
       │
       └── Per-IP token bucket (300 req/min default)
           if tokens < 0 → HTTP 429
```

---

## FreeRTOS Task Layout

| Task | Priority | Stack | Core | Purpose |
|------|----------|-------|------|---------|
| `webserver_task` | 2 | 8 KB | 0 | HTTP request handling |
| `wifi_tx_task` | 5 | 4 KB | 1 | Frame injection from tool queues |
| `promisc_task` | 4 | 4 KB | 1 | Promiscuous frame dispatch to tools |
| `housekeeping_task` | 1 | 2 KB | 0 | Session expiry, rate limit token refill |
| `loop()` (Arduino) | 1 | 8 KB | 1 | Tool tick functions, NVS commit |

---

## NVS Namespace Map

| Namespace | Key | Description |
|-----------|-----|-------------|
| `phantom32` | `password` | Hashed login password |
| `phantom32` | `apikey` | API key for programmatic access |
| `rla` | `global_fps` | Global TX rate limit (fps) |
| `rla` | `http_rpm` | HTTP rate limit (req/min) |
| `rla` | `tool_<n>` | Per-tool cap/rate overrides |
| `beacon` | `ssid_count` | Number of beacon spam SSIDs |
| `beacon` | `ssid_<n>` | Beacon spam SSID pool entries |

---

## Build Configuration

### `platformio.ini` Environment Naming Convention

```
doom           → ESP32 DevKit v1 (WROOM-32D, non-PSRAM, LITE)
doom_nodemcu   → NodeMCU-32 (non-PSRAM, LITE)
doom_wrover    → WROVER-KIT (4 MB PSRAM, full analytics)
doom_s3        → ESP32-S3 generic (PSRAM, full analytics)
doom_c3        → ESP32-C3 (RISC-V, reduced feature set)
```

### Flag Inheritance

```ini
[env]                          # base flags, all envs inherit
build_flags = -std=gnu++14 -Os -DCORE_DEBUG_LEVEL=1

[env:doom]                     # LITE env
build_flags = ${env.build_flags} -DPHANTOM32_LITE

[env:doom_wrover]              # PSRAM env
build_flags = ${env.build_flags} -DBOARD_HAS_PSRAM
board_build.arduino.memory_type = qio_opi
```

---

## Adding a New Tool

1. Create `src/my_tool.h` with:
   - A struct `MyToolState` for the tool's data
   - An `mt_init()` function that registers routes via `server.on(...)`
   - An `mt_tick()` function called from `loop()`
   - If injecting frames: call `rla_check("my_tool")` before every
     `esp_wifi_80211_tx()` call
   - If accepting user input: call `iv_check_*()` on every argument

2. Add `#include "my_tool.h"` to `src/extended_modules.h` inside the
   `#ifndef PHANTOM32_LITE` block (or outside it if the tool is small
   enough for non-PSRAM boards).

3. Register in `ext_register_routes()`.

4. Add dispatch in `ext_dispatch_frame()` if the tool uses promiscuous frames.

5. Add unit tests to `test/unit/test_my_tool.cpp`.

6. Add a new env to `platformio.ini` only if a new board variant is needed;
   otherwise the existing matrix covers it.

---

## Dependency Graph

Allowed dependency directions (→ means "may include/call"):

```
main.cpp
  → auth_manager.h
  → input_validator.h
  → rate_limiter_adv.h
  → board.h
  → module.h (ToolModule registry)
  → src/core/*.h (diagnostic runtime)
  → src/[tool_name].h (tool modules)
  → extended_modules.h

src/[tool_name].h (tool modules)
  → input_validator.h
  → rate_limiter_adv.h
  → board.h
  → ESP32 SDK (esp_wifi, esp_bt, Arduino)

src/core/*.h (diagnostic runtime)
  → stdlib only (stdint.h, string.h, stdio.h)
  → ESP32 SDK ONLY where guarded by #ifndef PHANTOM32_NATIVE_TEST

extended_modules.h
  → src/[analytics_tool].h

test/test_*/
  → Unity (test framework)
  → src/core/*.h and src/[module].h under test
  → platform stubs (stubs only the specific symbols needed)
```

**Prohibited dependency directions (never allowed):**

- Tool modules must NOT include other tool modules.
- `src/core/*.h` must NOT include any `src/[tool].h` file.
- `src/core/*.h` must NOT include Arduino framework headers except under
  `#ifndef PHANTOM32_NATIVE_TEST`.
- `auth_manager.h` must NOT call any tool module.

---

## Startup Lifecycle

```
Power-on / reset
  │
  ▼
setup()                                      ← src/main.cpp
  ├─ board_init()                            ← src/board.h: GPIO, LED
  ├─ LittleFS.begin()                        ← filesystem mount
  ├─ auth_manager_init()                     ← load/generate credentials from NVS
  ├─ rate_limiter_init()                     ← load limits from NVS
  ├─ WiFi.softAP(SSID, password, channel)   ← start AP
  ├─ server.begin()                          ← register routes + start HTTP server
  ├─ [for each ToolModule with setup fn]     ← tool initialization
  │   └─ module.setup()
  ├─ g_phantom_doctor.runChecks()            ← Heap / Storage / WiFi diagnostics
  ├─ g_phantom_doctor.finalise()             ← lock the diagnostic report
  ├─ xTaskCreate(channel_hop_task, ...)      ← FreeRTOS task for channel cycling
  └─ Serial.printf("[doom] Phantom32 ready") ← boot banner
  │
  ▼
loop()  [runs forever on Arduino main task]
  ├─ server.handleClient()                   ← HTTP dispatch
  ├─ [for each active ToolModule with loop fn]
  │   └─ module.loop()                       ← tool tick
  ├─ [every 30 s] g_resource_profiler.sampleSystem()
  ├─ [every 30 s] g_quality_scorecard.recordValue("FreeHeap", ...)
  └─ [every 30 s] if heap < CRIT_THRESHOLD → g_recovery_engine.recordFailure(...)
```

---

## Shutdown Lifecycle

The firmware has no software-initiated shutdown path. The ESP32 is a
single-task embedded system; the loop runs until a hardware reset or power
loss. "Shutdown" is therefore equivalent to a reboot.

**On reboot or power loss:**

- FreeRTOS tasks are terminated by the hardware reset.
- RAM state (session tokens, RecoveryEngine state, rate limiter token counts)
  is lost. This is by design — session tokens are RAM-only intentionally
  (see `ADR-005`).
- NVS-backed state (credentials, rate limit configuration, beacon SSID pool)
  survives across reboot.
- The `g_recovery_engine` and `g_phantom_doctor` globals restart fresh on the
  next `setup()` call.

---

## Error Propagation

| Error category | Detection point | Propagation path | Operator visibility |
|---|---|---|---|
| Boot diagnostic failure (Heap/Storage/WiFi) | `setup()` → `g_phantom_doctor.runChecks()` | `PhantomDoctor.overallStatus()` returns FAIL/WARN | `GET /api/phantom-doctor`; serial boot banner |
| Runtime heap critical (< 16 KB) | `loop()` heap watchdog, every 30 s | `g_recovery_engine.recordFailure("LowHeap", SEV_HIGH)` → state → SAFE_MODE | RecoveryEngine state; serial log |
| HTTP authentication failure | `auth_check()` on every protected route | Returns HTTP 401/429; increments IP failed-attempt counter | HTTP response code; IP lockout timer |
| Input validation failure | `input_validator.h` on every web request argument | Returns HTTP 400 JSON `{"ok":false,"error":"..."}` | HTTP response body |
| Rate limit exceeded | `rla_check()` / `rla_check_http()` | TX: frame dropped silently; HTTP: 429 response | HTTP 429; tool TX counter shows drop |
| NVS read error at boot | `auth_manager_init()` | Credential regeneration; fresh first-boot sequence | Serial: new credentials printed |
| NVS write error at runtime | `nvs_set_*` returns error | In-memory value used; logged to serial | Serial warning |
| RecoveryEngine CRITICAL failure | `re.recordFailure(..., SEV_CRITICAL, ...)` | RecoveryEngine state → FAILED; no auto-recovery | `GET /api/phantom-doctor`; `activeFailureCount() > 0` |

Errors do not propagate across module boundaries beyond the shared global state
listed above. A tool module that sets a RecoveryEngine failure does not directly
affect any other tool's execution.

---

## Resource Ownership

| Resource | Owner | Lifetime | Notes |
|---|---|---|---|
| Session token array | `auth_manager.h` — static array in BSS | Process lifetime | RAM-only; never written to NVS |
| PBKDF2 hash + salt | `auth_manager.h` — static array in BSS | Process lifetime | Loaded from NVS on startup; re-generated if NVS read fails |
| IP lockout table | `auth_manager.h` — static array in BSS | Process lifetime (counts reset on reboot) | Per-IP failed attempt counters |
| TX token bucket (global + per-tool) | `rate_limiter_adv.h` — static struct | Process lifetime | NVS-persisted limits, RAM-tracked tokens |
| HTTP token bucket (per-IP) | `rate_limiter_adv.h` — static struct | Process lifetime | RAM-only |
| ToolModule registry `g_tools[]` | `main.cpp` — global array | Process lifetime | Statically allocated, never modified at runtime |
| FreeRTOS task handles | `main.cpp` — global handles | From `xTaskCreate()` to watchdog reset | Tasks never deliberately deleted |
| WebServer instance | `main.cpp` — global `WebServer server(80)` | Process lifetime | Single HTTP server shared by all routes |
| LittleFS filesystem | Arduino framework; mounted in `setup()` | Process lifetime | Unmount not performed |
| NVS namespace `phantom32` | `auth_manager.h` | NVS flash partition | Survives reboots |
| NVS namespace `rla` | `rate_limiter_adv.h` | NVS flash partition | Survives reboots |
| RecoveryEngine state | `main.cpp` — global `g_recovery_engine` | Process lifetime (RAM only) | Clears on reboot |
| Diagnostic report | `main.cpp` — global `g_phantom_doctor` | Process lifetime (locked after setup) | No runtime mutation after `finalise()` |

---

## Thread and Task Ownership

The ESP32's FreeRTOS scheduler runs on two cores (LX6/LX7) or one core (C3).

| Task | Core affinity | Priority | Stack | Owns |
|---|---|---|---|---|
| Arduino `loop()` (app_main) | Core 1 | 1 | 8 KB | HTTP dispatch, tool ticks, heap watchdog |
| `channel_hop_task` | Core 0 | 1 | 2 KB | 802.11 channel cycling via `esp_wifi_set_channel()` |
| ESP-IDF WiFi task (internal) | Core 0 | 23 | (SDK-managed) | 802.11 MAC + PHY; promiscuous callback dispatch |
| ESP-IDF BT/NimBLE task (internal) | Core 0 | (SDK-managed) | (SDK-managed) | BLE advertising + scanning |
| ESP-IDF timer task (internal) | Core 0 | 22 | (SDK-managed) | esp_timer callbacks |

**Thread-safety rules:**

- The promiscuous callback runs in the WiFi task context. It must never call
  `malloc()`, `new`, or `String` construction. Data is written to pre-allocated
  ring buffers via atomic index operations only.
- Tool `loop()` functions run on the Arduino main task. They must not block for
  more than a few milliseconds (the HTTP server shares the same task).
- NVS reads/writes are protected by the ESP-IDF NVS lock internally; no
  additional mutex is needed.
- The session token array and IP lockout table in `auth_manager.h` are accessed
  only from the HTTP route handlers (Arduino main task). No cross-task access.

---

## Hardware Abstraction Boundaries

| Boundary | File | Rule |
|---|---|---|
| Board-specific pin assignments and LED behavior | `src/board.h` | All tool code uses `board.h` constants; no raw GPIO numbers elsewhere |
| ESP32-specific SDK calls (WiFi, NVS, heap APIs) | `#ifndef PHANTOM32_NATIVE_TEST` guards in each file | Any file that guards its SDK calls compiles cleanly on the native host |
| FreeRTOS task creation | `src/main.cpp` only | Tool modules do not create or delete FreeRTOS tasks |
| `esp_wifi_80211_tx()` (frame injection) | Tool modules, behind `rla_check()` | No tool bypasses the rate limiter for injection |
| `esp_random()` (hardware RNG) | `src/auth_manager.h` only | No other file calls `esp_random()` |
| LittleFS filesystem access | `src/pcap_mgr.h` and `src/main.cpp` | Analytics modules do not access LittleFS directly |

---

## Configuration Flow

```
platformio.ini (compile-time)
  └─ build_flags: -DPHANTOM32_LITE / -DBOARD_HAS_PSRAM / -DPHANTOM32_NATIVE_TEST
       │
       └─ Selected at pio run time; determines pool sizes and feature flags

src/main.cpp compile-time #defines
  └─ AP_SSID, AP_CHANNEL, AP_MAX_CLIENTS, HEAP_WARN_THRESHOLD, HEAP_CRIT_THRESHOLD
       │
       └─ Fixed at build time; not runtime-configurable

NVS flash (runtime-persisted)
  ├─ auth_manager: password hash + salt, API key
  ├─ rate_limiter_adv: global_fps, http_rpm, per-tool limits
  └─ beacon_spam: SSID pool

Runtime HTTP API (session-scoped)
  ├─ POST /api/auth/change-password  → updates NVS password hash
  ├─ POST /api/rla/global            → updates NVS rate limits
  └─ POST /api/rla/tool              → updates NVS per-tool limit
```

---

## Logging Flow

Phantom32 uses the Arduino `Serial` interface for all log output. There is no
in-memory log buffer or structured logging layer.

```
setup() / loop()
  └─ Serial.printf("[doom] ...")
       │
       ▼
  USB-UART → host serial monitor (115200 baud)
```

**Log categories by prefix:**

| Prefix | Subsystem | Examples |
|---|---|---|
| `[doom]` | Firmware core | Boot banner, heap watchdog, AP status |
| `[auth]` | Authentication | Credential generation, lockout events |
| `[rla]` | Rate limiter | Limit load/save |
| `[pcap]` | PCAP manager | File open/close, frame count |
| (tool name) | Tool modules | Tool-specific events |

Structured JSON logging and remote log shipping are not implemented.
The ObservabilityDashboard (`src/core/ObservabilityDashboard.h`) provides an
in-memory ring buffer of `DashboardEntry` structs, accessible via
`GET /api/phantom-doctor` for the diagnostic layer only.

---

## Testing Boundaries

| What native tests cover | What native tests cannot cover |
|---|---|
| All logic in `src/core/*.h` modules | Raw 802.11 frame injection (`esp_wifi_80211_tx()`) |
| Session lifecycle, lockout, token validation | Promiscuous callback dispatch from the WiFi driver |
| Input validation for all argument types | NVS read/write (stubbed in tests) |
| Rate limiter token bucket algorithm | LittleFS filesystem operations |
| Evil twin state machine, credential ring | FreeRTOS task scheduling |
| Diagnostic status transitions | Physical AP association / DHCP |
| Recovery handler invocation and state machine | ESP32 heap fragmentation over sustained load |
| Resource profiler per-module tracking | Hardware-specific timing (`millis()`, `esp_timer_get_time()`) |
| QualityScorecard metric thresholds | BLE advertising / scanning (NimBLE driver) |

**Platform stub strategy:** Each test file stubs only the specific platform
symbols it exercises. Stubs are designed to faithfully model the real behavior
(e.g., `millis()` returns an incrementing counter; `esp_get_free_heap_size()`
returns a configurable value). No mocking framework is used; stubs are plain C
functions in the test file.

**Hardware-in-the-loop (HIL)** testing covers the gap between native unit tests
and physical hardware. The framework is documented in
[docs/hil-testing.md](./hil-testing.md) and the CI stub is in
`.github/workflows/hil.yml` (disabled until a self-hosted runner is registered).

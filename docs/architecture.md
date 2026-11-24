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

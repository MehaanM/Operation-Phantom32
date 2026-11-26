# Phantom32

**Wireless diagnostics and security research firmware for ESP32**

A production-grade, fully on-device Wi-Fi and Bluetooth toolkit that exposes a
local HTTP web UI at `192.168.4.1` — no cloud, no app, no IDE required.
Built to strict engineering standards: 13-board CI matrix, 1461 automated tests,
85% line / 80% branch coverage enforced, AddressSanitizer + UBSan, SBOM
generation, and a complete diagnostic + recovery runtime.

[![CI](https://github.com/MehaanM/Operation-Phantom32/actions/workflows/build.yml/badge.svg?branch=main)](https://github.com/MehaanM/Operation-Phantom32/actions/workflows/build.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](./LICENSE)
[![Platform: ESP32](https://img.shields.io/badge/platform-ESP32-red.svg)](https://www.espressif.com/en/products/socs/esp32)
[![PlatformIO](https://img.shields.io/badge/build-PlatformIO%206.2.0-orange.svg)](https://platformio.org)
[![Tests](https://img.shields.io/badge/tests-1461%20passing-brightgreen.svg)](https://github.com/MehaanM/Operation-Phantom32/actions)
[![C++14](https://img.shields.io/badge/C%2B%2B-14-lightgrey.svg)]()

**FOR AUTHORIZED USE IN ENVIRONMENTS YOU OWN OR HAVE WRITTEN PERMISSION TO TEST.**
Read [LEGAL.md](./LEGAL.md) before deploying this firmware.

---

**Navigation**

[Why Phantom32](#why-phantom32) ·
[Principles](#design-principles) ·
[Overview](#overview) ·
[Showcase](#showcase) ·
[Status](#project-status) ·
[Features](#capabilities) ·
[Architecture](#architecture) ·
[Hardware](#hardware-compatibility) ·
[Quick Start](#quick-start) ·
[Installation](#installation) ·
[Configuration](#configuration) ·
[Testing](#testing-architecture) ·
[Failure Analysis](#failure-analysis) ·
[Security](#security) ·
[Roadmap](#roadmap) ·
[Contributing](#contributing) ·
[License](#license)

---

## Why Phantom32?

Most ESP32 Wi-Fi research tools are single-file sketches: they demonstrate
a technique but are not engineered for reliability, testability, or
long-term maintenance. Phantom32 was built with a different set of
priorities.

**What is different about this project:**

| Property | Typical ESP32 tool | Phantom32 |
|---|---|---|
| Test coverage | None or manual | 1461 automated tests; ≥ 85% line coverage enforced in CI |
| Memory safety | Unchecked | AddressSanitizer + UBSan on every commit |
| Board support | One board | 13 environments in CI build matrix |
| Authentication | Hardcoded password or none | PBKDF2-HMAC-SHA256 (100k iterations), device-unique, session-scoped |
| Failure handling | Crash or silent | RecoveryEngine with handler callbacks; PhantomDoctor boot diagnostics |
| Build reproducibility | Dev-machine-only | Pinned platform + lib versions; CI artifacts with SHA-256 |
| Secret scanning | None | gitleaks on full git history, every push |
| SBOM | None | CycloneDX generated on every build |

**What Phantom32 does not claim:**

- It is not faster, stealthier, or more effective than other tools at any
  specific 802.11 technique. The promiscuous capture rate and injection
  performance are limited by the ESP32 Wi-Fi stack, the same hardware
  all similar tools run on.
- It does not replace professional audit tooling (commercial Wi-Fi adapters,
  airodump-ng, Wireshark on a laptop). It fills a different niche: a
  self-contained pocket device for authorized audits on networks you own.
- Stability tests have not been published yet. Long-duration reliability
  claims are not made until that data exists.

The differentiator is the engineering infrastructure around the tool, not
the tools themselves. If you care about running on constrained hardware with
verifiable correctness guarantees, Phantom32 is designed for that.

---

## Design Principles

The following principles govern how Phantom32 is built and extended.
Each is listed alongside the mechanism that enforces it.

| Principle | How it is applied |
|---|---|
| **Modular isolation** | Each tool is a self-contained `ToolModule` struct. Tools communicate only through explicitly named globals, never by calling each other's internals. |
| **Explicit platform dependencies** | ESP32 SDK calls are gated by `#ifndef PHANTOM32_NATIVE_TEST`. Any file that compiles on native has no hidden platform calls. |
| **Security at the boundary** | All HTTP routes pass through `auth_check()` before any handler runs. All web inputs pass through `input_validator.h`. Neither is optional. |
| **Fault isolation** | A tool that crashes does not take down the HTTP server. The WebServer and FreeRTOS scheduler continue; the tool is marked inactive. |
| **Measurable reliability** | RecoveryEngine tracks failure counts and state transitions. ResourceProfiler samples heap every 30 seconds. QualityScorecard enforces metric thresholds. These are not for display — they feed real decisions in `loop()`. |
| **Hardware abstraction in one place** | All board-specific pin assignments and LED behavior live in `src/board.h`. Tool code never references hardware directly. |
| **Reproducible builds** | PlatformIO platform and library versions are pinned. CI publishes SHA-256 checksums for every firmware artifact. |
| **Automated verification** | No claim in this README is accepted on faith. Each has a corresponding test, CI job, or hardware measurement. |
| **Honest status** | The Feature Maturity Matrix distinguishes HW-validated, CI-compiled, and unimplemented. Stability tests not yet published are stated as such. |
| **Maintainability over complexity** | No dynamic module loading, no dependency injection framework, no coroutine scheduler. Each system uses the simplest data structure that fits the constraint. |

For the reasoning behind specific decisions, see [docs/decisions/](./docs/decisions/).

---

## Overview

Phantom32 is embedded firmware for the ESP32 family that turns any supported
board into a self-contained wireless diagnostic station. Once flashed, the
device starts its own WPA2-secured access point, serves a full web UI over
HTTP, and exposes 29 Wi-Fi research tools and 2 Bluetooth tools — all without
any external infrastructure, cloud connection, or companion application.

**What problem it solves.** Wireless diagnostic work typically requires a
laptop running multiple tools, a managed access point, and connectivity to
external services. Phantom32 collapses that stack onto a single device that
fits in a pocket, runs on a USB power bank, and starts in under three seconds.

**Why it was built this way.** The firmware was designed around a constraint
often ignored in embedded projects: the entire test and verification pipeline
runs on a standard development machine with no special hardware. All 1461 test
cases compile and run as native host binaries. CI validates every commit across
13 board environments without requiring any physical ESP32. This makes the
development cycle fast and the codebase verifiable by anyone with a laptop.

**What makes the architecture different.** Most ESP32 projects consist of a
single large `main.cpp` with feature flags. Phantom32 uses a modular header
architecture: each tool is a self-contained `ToolModule` struct registered in
a central table. The 20-module diagnostic runtime (phases 18–20) is built
entirely from header-only platform-agnostic C++ and runs identically on host
and firmware. The build guard `PHANTOM32_NATIVE_TEST` (set only by build flags,
never by headers) ensures ESP32-specific SDK calls are excluded from host
builds without polluting the firmware binary.

**Intended users.**

- Wireless engineers performing authorized audits of networks they own or
  manage
- Security researchers in controlled lab environments
- Embedded engineers studying ESP32 Wi-Fi stack behavior
- Students learning 802.11 frame structure through observed examples

**Development stage.** Version 3.2.0. The core tool set, authentication layer,
and CI pipeline are production-quality. The diagnostic runtime (phases 18–20)
is complete and unit-tested but has not yet had a hardware validation cycle
published. 24-hour stability testing is planned for v3.3. See
[Limitations](#limitations-and-known-issues) for a complete list of gaps.

---

## Showcase

### Hardware

The reference hardware is a standard ESP32-WROOM-32D DevKit V1 (38-pin).
No modifications or external components are required.

```
┌──────────────────────────────────────┐
│   ESP32-WROOM-32D DevKit V1          │
│                                      │
│   USB ──► CP2102 ──► ESP32 rev3      │
│                      240 MHz         │
│                      4 MB Flash      │
│                      320 KB DRAM     │
│                      Wi-Fi 802.11 b/g/n
│                      BLE 4.2         │
└──────────────────────────────────────┘
```

> **Hardware photos and interface screenshots** will be added in v3.3 once
> the hardware validation photography workflow is established. The table in
> [Hardware Validation](#hardware-validation) documents exactly what has been
> tested on physical hardware.

### Verified boot output (ESP32-WROOM-32D, 2026-11-25)

```
rst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
...
[doom] ========================================
[doom] Phantom32 v3.2.0 — ESP32 Wireless Research Firmware
[doom] Built: Nov 25 2026 ...
[doom] Chip: ESP32 rev3 @ 240 MHz  Free heap: 162232 bytes
[doom] FS: 36864/3964928 bytes used
[doom] AP: ssid=Phantom32 ip=192.168.4.1 ch=6
[auth] Generated new password: Ccd229bkMNFP
[auth] Store this somewhere safe.
[doom] HTTP server started on port 80
[doom] ========================================
[doom] Ready!
[doom] Connect: SSID=Phantom32 PASS=Ccd229bkMNFP
[doom] Browse:  http://192.168.4.1/dashboard
[doom] ========================================
```

### Diagnostic system (PhantomDoctor)

```
PHANTOM DOCTOR  [PASS]

  Heap                           PASS  (OK)
  Storage                        PASS
  WiFi                           PASS
```

### Recovery system (RecoveryEngine)

```
// A handler can be registered per named failure:
re.registerHandler("LowHeap", heapRecoveryFn, nullptr);

// On detection, the handler is invoked; failure resolves only on success:
re.recordFailure("LowHeap", "Heap below critical", FailureSeverity::HIGH, true, now);
// ... 30 s later in loop() ...
re.attemptRecovery("LowHeap", now);  // calls heapRecoveryFn(); resolves if it returns true
```

> API endpoints: `GET /api/phantom-doctor` · `GET /api/resource-profile`

---

## Project Status

| Property | Value |
|---|---|
| Stable version | **v3.2.0** |
| Release date | 2026-11-25 |
| Latest CI | [![CI](https://github.com/MehaanM/Operation-Phantom32/actions/workflows/build.yml/badge.svg?branch=main)](https://github.com/MehaanM/Operation-Phantom32/actions/workflows/build.yml) |
| Automated tests | **1461** (1461 passed, 0 failed — native + ASan) |
| Coverage enforcement | ≥ 85% line · ≥ 80% branch (src/ only) |
| Board environments | **13** (CI-compiled on every push) |
| Physical hardware tested | **ESP32-WROOM-32D** (boot, auth, NVS, AP — 2026-11-25) |
| Development stage | v3.2 — reliability and release quality |
| Known build failures | None on main |
| OTA update | Not implemented (v3.4 roadmap) |

---

## Capabilities

### Wi-Fi Research Tools (29)

| Tool | Description |
|---|---|
| Beacon Spam | Flood nearby stations with fake AP beacon frames |
| Deauth | Send targeted 802.11 deauthentication frames |
| Evil Twin | Rogue AP with captive portal to intercept credentials |
| Probe Flood | Send directed probe request frames for a configured SSID pool |
| Probe Harvest | Passive collection of probe requests from nearby stations |
| Device Tracker | Passive STA/AP detection, RSSI history, vendor (OUI) lookup, randomized MAC detection |
| Channel Hopper | Automated multi-channel scanning with configurable dwell time |
| KARMA Attack | Auto-respond to directed probe requests to attract clients |
| PMKID Hunt | Capture PMKID-containing EAPOL frames for offline analysis |
| Aegis (Defense) | Passive 802.11 anomaly detection: deauth storms, rogue APs, KARMA indicators, sequence anomalies |
| Evil Portal | Standalone captive portal independent of Evil Twin |
| Handshake Capture | Passive 4-way EAPOL handshake collection |
| Null Flood | Null-data frame injection for STA probing |
| CSA Attack | 802.11h Channel Switch Announcement injection |
| BTM Flood | 802.11v BSS Transition Management request injection |
| EAP Flood | EAP frame injection for 802.1X testing |
| Auth Flood | 802.11 open-system authentication frame flood |
| NavFlood | Network Allocation Vector (NAV) manipulation frames |
| Action Flood | 802.11 Action frame injection |
| WPA3 Flood | SAE Commit/Confirm frame flood for WPA3 testing |
| Management Flood | Generic management frame flood |
| Deauth Storm | Multi-target sustained deauthentication |
| Deauth Multi | Simultaneous multi-BSSID deauthentication |
| Chan-Hop Deauth | Deauthentication synchronized with channel hopping |
| Beacon Adv | Advanced beacon crafting with custom IEs |
| SSID Fuzzer | Automated SSID field fuzzing for AP/client robustness testing |
| Honeypot | Open AP honeypot for client behavior analysis |
| ESP-NOW Flood | ESP-NOW frame injection |
| PCAP Manager | LittleFS-backed PCAP capture and download |

### Bluetooth Research Tools (2)

| Tool | Description |
|---|---|
| BLE Spam | BLE advertisement spam (Apple/Samsung/Google device impersonation profiles) |
| Aegis BLE | BLE scan anomaly detection |

### Analytics Modules — PSRAM boards only (13)

On boards with PSRAM (`BOARD_HAS_PSRAM`) the following are additionally enabled:

- Frame statistics and per-tool TX PPS graph
- Per-BSSID RSSI history with 128-sample ring buffer
- 802.11 sequence number tracking
- STA/AP association state machine
- Multicast/broadcast frame pattern analysis
- Scan result cache with entry aging

### Security and Infrastructure

| Feature | Description |
|---|---|
| PBKDF2-HMAC-SHA256 auth | 100,000 iterations; device-unique 12-char password on first boot |
| Session tokens | 256-bit random, RAM-only, 30 min TTL, invalidated on reboot |
| IP lockout | 5 failed attempts → 5-minute lockout per IP |
| TX rate limiting | Token bucket; 200 fps global cap + per-tool limits |
| HTTP rate limiting | 300 requests/min per client IP |
| Input validation | SSID, MAC, channel, integer, IP — all web inputs validated |
| Constant-time comparison | All credential comparisons use constant-time algorithms |

---

## Feature Maturity Matrix

| Subsystem | Status | Unit Tested | HW Validated | Known Limitations |
|---|---|---|---|---|
| Beacon Spam | Implemented | ✅ | ⬜ Planned | Pool limited to 32 SSIDs on LITE |
| Deauth | Implemented | ✅ | ⬜ Planned | — |
| Evil Twin + captive portal | Implemented | ✅ | ⬜ Planned | 8-entry credential ring on LITE |
| Probe Flood | Implemented | ✅ | ⬜ Planned | 8 clients × 4 targets on LITE |
| Probe Harvest | Implemented | ✅ | ⬜ Planned | — |
| Device Tracker | Implemented | ✅ | ⬜ Planned | 20-device limit on LITE |
| Channel Hopper | Implemented | ✅ | ⬜ Planned | FreeRTOS task; 100 ms dwell default |
| KARMA Attack | Implemented | ✅ | ⬜ Planned | — |
| PMKID Hunt | Implemented | ⬜ | ⬜ Planned | No pcap export yet |
| Aegis (Wi-Fi) | Implemented | ✅ | ✅ 2026-11-25 | 32 alert ring on LITE |
| Aegis BLE | Implemented | ⬜ | ⬜ Planned | — |
| BLE Spam | Implemented | ⬜ | ⬜ Planned | C3 uses different NimBLE init |
| PCAP Manager | Implemented | ⬜ | ✅ 2026-11-25 | LittleFS size-bound |
| Authentication layer | Implemented | ✅ | ✅ 2026-11-25 | NVS-backed; erase resets |
| Session management | Implemented | ✅ | ✅ 2026-11-25 | RAM-only; reboot clears sessions |
| TX rate limiting | Implemented | ✅ | ⬜ Planned | — |
| HTTP rate limiting | Implemented | ✅ | ⬜ Planned | — |
| Input validation | Implemented | ✅ | ✅ 2026-11-25 | — |
| Config manager | Implemented | ✅ | ⬜ Planned | — |
| Analytics modules (PSRAM) | Implemented | ✅ | ⬜ Planned | No physical PSRAM board tested yet |
| PhantomDoctor | Implemented | ✅ | ⬜ Planned | API route registered; full report via serial |
| RecoveryEngine | Implemented | ✅ | ⬜ Planned | Handler callbacks; LowHeap detection in loop |
| ResourceProfiler | Implemented | ✅ | ⬜ Planned | sampleSystem() calls real ESP32 heap APIs |
| QualityScorecard | Implemented | ✅ | ⬜ Planned | FreeHeap metric tracked at boot + every 30 s |
| ObservabilityDashboard | Implemented | ✅ | ⬜ Planned | — |
| BenchmarkSuite | Implemented | ✅ | ⬜ Planned | No hardware benchmarks collected yet |
| FaultInjector | Implemented | ✅ | ⬜ Planned | Test infrastructure only |
| ReliabilityMetrics | Implemented | ✅ | ⬜ Planned | — |
| AnomalyDetector | Implemented | ✅ | ⬜ Planned | — |
| SemanticVersion | Implemented | ✅ | N/A | — |
| PluginRegistry | Implemented | ✅ | ⬜ Planned | — |
| SimulationEngine | Implemented | ✅ | ⬜ Planned | — |
| VirtualClock | Implemented | ✅ | N/A | — |
| BinaryBudget | Implemented | ✅ | ⬜ Planned | Flash budget enforcement |
| DocRegistry | Implemented | ✅ | N/A | — |
| ModuleLifecycle | Implemented | ✅ | ⬜ Planned | — |
| TestGenerator | Implemented | ✅ | N/A | generateFile() writes compilable Unity .cpp |
| OTA update | Planned | — | — | Planned v3.4 |

**Status labels:** Implemented = code complete and merged to main ·
Unit Tested = dedicated test suite present and passing in CI ·
HW Validated = physically exercised on hardware with recorded results ·
Planned = not yet implemented

---

## Architecture

### System Overview

```
  Operator browser
  192.168.4.x
       │
       ▼
  ┌─────────────────────────────────────────────────────────────┐
  │  ESP32 — Phantom32 Firmware                                 │
  │                                                             │
  │  ┌─────────────┐    ┌────────────────────────────────────┐  │
  │  │  Wi-Fi AP   │───▶│  WebServer :80 (Arduino WebServer) │  │
  │  │  Phantom32  │    │  auth_manager.h                    │  │
  │  │  WPA2-PSK   │    └──────────────────┬─────────────────┘  │
  │  └─────────────┘                       │                    │
  │                                        ▼                    │
  │  ┌─────────────────────────────────────────────────────┐    │
  │  │  Route Dispatcher                                   │    │
  │  │  input_validator.h + rate_limiter_adv.h             │    │
  │  └────────────┬────────────────────────────────────────┘    │
  │               │                                             │
  │       ┌───────┴──────────┐                                 │
  │       ▼                  ▼                                  │
  │  ┌─────────────┐  ┌─────────────────────────────────────┐   │
  │  │ Tool Modules│  │  Diagnostic Runtime (src/core/)     │   │
  │  │ ToolModule[]│  │  PhantomDoctor  RecoveryEngine      │   │
  │  │ 29 Wi-Fi +  │  │  ResourceProfiler  QualityScorecard │   │
  │  │  2 BLE tools│  │  ObservabilityDashboard             │   │
  │  └──────┬──────┘  └─────────────────────────────────────┘   │
  │         │                                                   │
  │  ┌──────▼──────────────────────────────────────────────┐    │
  │  │  FreeRTOS Tasks                                     │    │
  │  │  channel_hop_task  wifi_tx_task  webserver_task     │    │
  │  └──────────────────────────────────────────────────────┘   │
  │                                                             │
  │  ┌─────────────────────────────────────────────────────┐    │
  │  │  ESP32 SDK / Arduino Framework                      │    │
  │  │  esp_wifi · esp_bt · LittleFS · NVS · FreeRTOS      │    │
  │  └─────────────────────────────────────────────────────┘    │
  └─────────────────────────────────────────────────────────────┘

  802.11 frames ◀──────────────────────────────────────▶ Air
```

### Architectural Layers

**Application layer — `src/main.cpp`**
Entry point for `setup()` and `loop()`. Owns the WebServer instance, global
state, FreeRTOS task creation, and the tool module registry `g_tools[]`.
Calls into every other layer but holds no business logic itself.

**Authentication — `src/auth_manager.h`**
All HTTP routes pass through `auth_check()` or `auth_check_or_redirect()`
before any handler runs. The auth manager owns session creation, token
validation, IP lockout, and credential rotation. All security-sensitive
data (session tokens, password hash) is stored in DRAM, never PSRAM.

**Input validation — `src/input_validator.h`**
Every web UI argument is validated before use: SSID strings, MAC addresses,
channel numbers, integer ranges, IP addresses. This layer is the boundary
between untrusted external input and internal state.

**Rate limiting — `src/rate_limiter_adv.h`**
Two independent subsystems: a token-bucket TX limiter (per-tool + 200 fps
global cap) and a per-IP HTTP limiter (300 req/min). Both persist limits
to NVS across reboots.

**Tool modules — `src/*.h` (ToolModule table)**
Each tool is a `ToolModule` struct with function pointers for `setup()`,
`loop()`, and `routes()`. The registry `g_tools[]` in `main.cpp` holds all
modules; `loop()` calls each active module's loop function in sequence.
Tools are isolated: they communicate through shared global state only where
necessary (the promiscuous callback, channel state, TX budget).

**Diagnostic runtime — `src/core/`**
20 header-only platform-agnostic modules implementing firmware self-diagnosis,
recovery, resource profiling, quality tracking, observability, and testing
infrastructure. These compile identically on the ESP32 and on the native host.
ESP32-specific API calls are guarded by `#ifndef PHANTOM32_NATIVE_TEST`.

**Hardware abstraction — `src/board.h`**
Board-specific pin assignments and LED behavior. All hardware-specific
constants are centralized here rather than scattered through tool code.

**FreeRTOS tasks**

| Task | Stack | Priority | Purpose |
|---|---|---|---|
| `channel_hop_task` | 2048 B | 1 | Automated channel cycling |
| Arduino `loop()` | (main stack) | — | HTTP handling, tool loops, heap watchdog |

**Testing infrastructure — `test/` + `src/core/`**
Tests run as standalone native binaries using Unity. No mocking framework is
used; each test file stubs only the specific platform symbols it exercises.
The `FaultInjector`, `BenchmarkSuite`, `SimulationEngine`, and `VirtualClock`
modules in `src/core/` provide controlled test infrastructure for firmware
components.

**Module decoupling strategy.** Headers in `src/core/` have no dependencies on
the Arduino framework or any other tool module. Tool modules in `src/` depend
only on the ESP32 SDK and stdlib. Neither layer depends on the other at compile
time. `main.cpp` is the only file that includes both and wires them together.

---

## Repository Structure

```
.
├── src/
│   ├── main.cpp                # Firmware entry: setup(), loop(), WebServer, task init
│   ├── module.h                # ToolModule struct and g_tools[] registry
│   ├── auth_manager.h          # HTTP authentication (sessions, lockout, PBKDF2)
│   ├── rate_limiter_adv.h      # TX and HTTP rate limiting (token bucket)
│   ├── input_validator.h       # Input sanitization and validation
│   ├── extended_modules.h      # Analytics module aggregator (PSRAM-only)
│   ├── safety.h                # TX safety interlock and autostop
│   ├── aegis.h                 # Passive 802.11 anomaly detection
│   ├── board.h                 # Board-specific pin/LED config
│   ├── beacon_spam.h           # Beacon spam tool
│   ├── deauth.h                # 802.11 deauthentication tool
│   ├── evil_twin.h             # Rogue AP + captive portal
│   ├── probe_flood.h           # Probe request flood
│   ├── device_tracker.h        # STA/AP tracker with RSSI history
│   ├── pcap_mgr.h              # LittleFS PCAP capture and download
│   ├── cli.h                   # HTTP CLI interface
│   ├── diag.h                  # System diagnostics endpoint
│   ├── [15+ more tool headers]
│   └── core/                   # Phase 18–20 diagnostic runtime (platform-agnostic)
│       ├── PhantomDoctor.h       # Boot diagnostic report (checks + overallStatus)
│       ├── RecoveryEngine.h      # Failure tracking + handler-callback recovery
│       ├── ResourceProfiler.h    # Per-module RAM/CPU/storage profiling
│       ├── QualityScorecard.h    # Metric tracking with threshold validation
│       ├── ObservabilityDashboard.h # Unified log + health + module status
│       ├── ErrorContext.h        # Error code registry + active error tracking
│       ├── ProgressReporter.h    # Long-operation progress tracking
│       ├── BenchmarkSuite.h      # Timing benchmark infrastructure
│       ├── FaultInjector.h       # Controlled fault injection for tests
│       ├── AnomalyDetector.h     # Statistical anomaly detection
│       ├── ReliabilityMetrics.h  # MTBF / failure rate tracking
│       ├── PluginRegistry.h      # Runtime plugin registration
│       ├── SimulationEngine.h    # Deterministic simulation for testing
│       ├── VirtualClock.h        # Mockable clock for deterministic tests
│       ├── SemanticVersion.h     # Semantic version parsing and comparison
│       ├── ConfigManager.h       # Key-value config with validation
│       ├── BinaryBudget.h        # Flash/RAM budget tracking
│       ├── DiagnosticsEngine.h   # Diagnostic engine coordinator
│       ├── ModuleLifecycle.h     # Module init / teardown lifecycle
│       ├── TestGenerator.h       # Unity .cpp test file generator
│       └── [9 more core modules]
├── test/
│   ├── test_frame_parsing/       # 802.11 frame parsing (42 tests)
│   ├── test_input_validator/     # Input validation (76 tests)
│   ├── test_device_tracker/      # Device tracking (28 tests)
│   ├── test_auth_manager/        # Authentication (29 tests)
│   ├── test_security/            # Security paths (48 tests)
│   ├── test_rate_limiter/        # Rate limiting (21 tests)
│   ├── test_evil_twin/           # Evil twin state machine (29 tests)
│   ├── test_phantom_doctor/      # PhantomDoctor (28 tests)
│   ├── test_recovery_engine/     # RecoveryEngine (28 tests)
│   ├── test_quality_scorecard/   # QualityScorecard (29 tests)
│   └── [35 more test suites]
├── docs/
│   ├── architecture.md           # Detailed system design and module map
│   ├── architecture/             # Architecture decision records
│   ├── engineering/              # Memory budget, binary budget
│   ├── security/                 # Threat model, audit report
│   ├── api-reference.md          # Full HTTP API reference (all endpoints)
│   ├── hardware-compatibility.md # Per-board feature matrix and limitations
│   ├── hardware-validation.md    # Physical hardware test results
│   ├── installation.md           # Complete installation guide
│   ├── testing.md                # Test strategy and coverage guide
│   ├── troubleshooting.md        # Common issues and solutions
│   ├── roadmap.md                # Planned development milestones
│   ├── faq.md                    # Frequently asked questions
│   └── dependency-inventory.md   # Dependency versions and licenses
├── scripts/
│   ├── phantom_doctor.py         # Pre-build environment preflight checker
│   ├── coverage.sh               # Local coverage report generation
│   └── coverage_link.py          # PlatformIO coverage extra-scripts hook
├── .github/
│   └── workflows/
│       └── build.yml             # CI/CD pipeline definition
├── platformio.ini                # Build system: 13 environments + 3 test envs
├── partitions.csv                # 4 MB flash partition table
├── partitions_16mb.csv           # 16 MB flash partition table (M5Stack CoreS3, some S3)
├── CHANGELOG.md                  # Versioned change history
├── CONTRIBUTING.md               # Developer setup and contribution guide
├── LEGAL.md                      # Legal terms, disclaimers, jurisdiction guidance
├── SECURITY.md                   # Responsible disclosure policy
└── LICENSE                       # MIT License
```

---

## Hardware Compatibility

### Support tiers

| Tier | Definition |
|---|---|
| **Physically validated** | Boot, auth, NVS, and AP confirmed on real hardware |
| **CI-compiled** | Builds without error on every push; not physically tested |
| **Planned** | Architecture is compatible; build support in progress |

### Compatibility matrix

| Environment | Board | Chip | Flash | PSRAM | Build flag | Support tier |
|---|---|---|---|---|---|---|
| `doom` | ESP32-WROOM-32D / DevKit V1 | ESP32 rev3 | 4 MB | — | `PHANTOM32_LITE` | **Physically validated** |
| `doom_nodemcu` | NodeMCU-32S | ESP32 | 4 MB | — | `PHANTOM32_LITE` | CI-compiled |
| `doom_m5stack` | M5Stack Basic/Gray/Fire | ESP32 | 4–16 MB | — | `PHANTOM32_LITE` | CI-compiled |
| `doom_m5core2` | M5Stack Core2 | ESP32 | 16 MB | — | `PHANTOM32_LITE` | CI-compiled |
| `doom_ttgo` | TTGO T-Display (1.14" TFT) | ESP32 | 4 MB | — | `PHANTOM32_LITE` | CI-compiled |
| `doom_tcall` | TTGO T-Call SIM800L | ESP32 | 4 MB | — | `PHANTOM32_LITE` | CI-compiled |
| `doom_heltec` | Heltec WiFi Kit 32 (OLED) | ESP32 | 4 MB | — | `PHANTOM32_LITE` | CI-compiled |
| `doom_wrover` | ESP32-WROVER-B / WROVER-IE | ESP32 | 4 MB | 4 MB SPI | `BOARD_HAS_PSRAM` | CI-compiled |
| `doom_s3` | ESP32-S3-WROOM-1 (generic) | ESP32-S3 | 4 MB | 8 MB OPI | `BOARD_HAS_PSRAM` | CI-compiled |
| `doom_s3_devkit` | ESP32-S3-DevKitC-1 | ESP32-S3 | 16 MB | 8 MB OPI | `BOARD_HAS_PSRAM` | CI-compiled |
| `doom_tdisplay_s3` | TTGO T-Display-S3 | ESP32-S3 | 16 MB | 8 MB OPI | `BOARD_HAS_PSRAM` | CI-compiled |
| `doom_m5cores3` | M5Stack CoreS3 | ESP32-S3 | 16 MB | 8 MB OPI | `BOARD_HAS_PSRAM` | CI-compiled |
| `doom_c3` | ESP32-C3 / Seeed XIAO C3 | ESP32-C3 (RISC-V) | 4 MB | — | `PHANTOM32_LITE` | CI-compiled |

**Minimum requirements:** 4 MB flash · Wi-Fi 802.11 b/g/n · 320 KB DRAM.
No external components, displays, or peripherals are required.

#### `PHANTOM32_LITE` constraints (non-PSRAM boards)

| Component | LITE limit | Full limit |
|---|---|---|
| Device Tracker | 20 devices | 50 devices |
| Probe Flood | 8 clients × 4 targets | 16 × 8 |
| Evil Twin credential ring | 8 entries | 32 |
| Beacon Spam SSID pool | 32 SSIDs | 64 |
| Aegis alert ring | 32 entries | 64 |
| Analytics modules | Disabled | 13 enabled |

#### PSRAM notes

PSRAM (`BOARD_HAS_PSRAM`) is auto-mapped as a heap extension; large allocations
use `ps_malloc()` automatically. Security-sensitive state (session tokens,
password hash) is explicitly allocated from internal DRAM, not PSRAM.

The WROVER-IE variant's external IPEX connector requires a loaded antenna.
SPI PSRAM shares bus bandwidth with LittleFS; avoid concurrent LittleFS writes
under high-rate beacon spam.

For complete per-board notes and known issues, see
[docs/hardware-compatibility.md](./docs/hardware-compatibility.md).

---

## Quick Start

For experienced developers — fastest path from clone to running tests.

```bash
# 1. Clone
git clone https://github.com/MehaanM/Operation-Phantom32.git
cd Operation-Phantom32

# 2. Install PlatformIO (Python 3.8+ required)
pip install platformio

# 3. Run all tests (no hardware needed)
pio test -e native

# 4. Build firmware (ESP32-WROOM-32D)
pio run -e doom

# 5. Flash
pio run -e doom --target upload

# 6. Open serial monitor to read first-boot credentials
pio device monitor --baud 115200

# 7. Connect to Phantom32 Wi-Fi AP with the printed password
# 8. Browse to http://192.168.4.1/dashboard
```

Expected test output:

```
=============== 1461 test cases: 1461 succeeded in 00:00:16 ===============
```

---

## Installation

### Prerequisites

**Run the preflight checker first:**

```bash
python3 scripts/phantom_doctor.py
```

This validates your Python version, PlatformIO, esptool, and project
configuration. Fix any `✗` items before continuing. For a specific board:

```bash
python3 scripts/phantom_doctor.py --board doom_s3
```

**Required tools:**

| Tool | Minimum version | Install command |
|---|---|---|
| Python | 3.8 | [python.org](https://www.python.org) |
| PlatformIO Core | 6.2.0 | `pip install platformio` |
| esptool.py | 4.0 | `pip install esptool` (optional — web flasher doesn't require it) |
| git | any | [git-scm.com](https://git-scm.com) |

**Supported operating systems:** macOS 12+, Ubuntu 20.04+, Windows 10+.

### Clone

```bash
git clone https://github.com/MehaanM/Operation-Phantom32.git
cd Operation-Phantom32
```

### Build

```bash
# ESP32-WROOM-32D (most common DevKit, no PSRAM)
pio run -e doom

# ESP32-WROVER (4 MB PSRAM, full analytics)
pio run -e doom_wrover

# ESP32-S3
pio run -e doom_s3

# Build all 13 environments
for env in doom doom_nodemcu doom_m5stack doom_m5core2 doom_ttgo doom_tcall \
           doom_heltec doom_wrover doom_s3 doom_s3_devkit doom_tdisplay_s3 \
           doom_m5cores3 doom_c3; do
  pio run -e $env
done
```

### Flash

Connect your ESP32 via USB, then:

```bash
pio run -e doom --target upload
```

If PlatformIO does not detect the port:

```bash
# Find your port
pio device list

# Flash to a specific port
pio run -e doom --target upload --upload-port /dev/cu.usbserial-0001
```

Port conventions: `/dev/cu.usbserial-XXXX` on macOS,
`/dev/ttyUSB0` on Linux, `COM3` on Windows.

### Flash the filesystem (LittleFS)

```bash
pio run -e doom --target uploadfs
```

### First boot

1. Open serial monitor at 115200 baud: `pio device monitor --baud 115200`
2. Note the **AP Wi-Fi password** and **dashboard password** printed to serial.
   Both are 12-character alphanumeric strings unique to your device.
3. Connect to the `Phantom32` Wi-Fi network using the AP password.
4. Navigate to `http://192.168.4.1/dashboard`.
5. Log in with the dashboard password.

The credentials are stored in ESP32 NVS flash and persist across reboots.
They are never hardcoded in the firmware binary.

### Verify the build

```bash
# Check RAM and flash usage (target: < 55% DRAM, < 90% flash)
pio run -e doom 2>&1 | grep -E "RAM:|Flash:|DRAM:|IRAM:"

# Run all tests
pio test -e native

# Run with sanitizers
pio test -e native_asan
```

For the full installation guide including OS-specific notes, driver installation,
and Apple Silicon Rosetta instructions, see
[docs/installation.md](./docs/installation.md).

---

## Configuration

### Compile-time settings (`src/main.cpp`)

```cpp
#define AP_SSID        "Phantom32"     // AP network name
#define AP_IP          "192.168.4.1"  // Static IP served by the AP
#define AP_CHANNEL     6               // Default channel (1–13)
#define AP_MAX_CLIENTS 8               // Maximum simultaneous AP clients
#define HEAP_WARN_THRESHOLD (32*1024)  // Log warning below this free heap
#define HEAP_CRIT_THRESHOLD (16*1024)  // RecoveryEngine HIGH failure below this
```

The AP WiFi password is **not** a compile-time constant. It is generated
per-device on first boot by `auth_manager` and stored in NVS.

### Runtime settings (NVS-persisted)

| Setting | Default | API endpoint |
|---|---|---|
| Dashboard login password | Random 12-char (first boot) | `POST /api/auth/change-password` |
| API key | Random 32-char (first boot) | `POST /api/auth/reset-creds` |
| Global TX fps limit | 200 fps | `POST /api/rla/global` |
| HTTP req/min limit | 300 | `POST /api/rla/global` |
| Per-tool TX limit | (unlimited) | `POST /api/rla/tool` |

### PHANTOM32_LITE

Non-PSRAM boards compile with `-DPHANTOM32_LITE` automatically per environment
in `platformio.ini`. This is not a manual setting — do not set it by hand.

### Resetting to defaults

```bash
# Erase all NVS state (triggers new first-boot credential generation on next boot)
pio run -e doom --target erase
```

Or call `POST /api/auth/reset-creds` with a valid session token to rotate
credentials in-place without erasing flash.

### Configuration reference table

See [docs/architecture.md](./docs/architecture.md) for the complete NVS
namespace map, all configuration keys, and their valid ranges.

---

## Interface

The web UI is a single-page application served entirely from PROGMEM on the
ESP32 (no filesystem required for the UI itself). No internet connection is
needed once flashed.

### Pages

| URL | Description |
|---|---|
| `/dashboard` | System overview: live heap, AP clients, tool status, quick actions |
| `/` | Main tool hub: start/stop any tool, configure parameters |
| `/cli` | HTTP CLI interface: command input and scrolling output |
| `/login` | Authentication (redirected to automatically on first access) |
| `/api/*` | JSON API for all tools and system functions |

### Navigation

All pages use a responsive dark-theme layout. Navigation links in the top bar
provide access to all major sections. Tool pages follow a consistent pattern:
a configuration form, start/stop controls, and a live-updating status panel.

### Status indicators

The dashboard displays:
- Free heap with WARN/CRIT thresholds highlighted
- Active tool name and running time
- AP client count
- PhantomDoctor status (`PASS` / `WARN` / `FAIL` / `INCOMPLETE`)
- Channel hopper state

### Error messages

HTTP API responses follow a consistent structure:

```json
{ "ok": false, "error": "rate_limited", "retry_after_ms": 60000 }
```

For the complete API reference including all endpoints, request parameters,
and response schemas, see [docs/api-reference.md](./docs/api-reference.md).

---

## Module System

### ToolModule structure

Each tool in Phantom32 is a `ToolModule` struct:

```cpp
struct ToolModule {
  const char *name;       // Human-readable name
  void (*setup)();        // Called once in setup() if non-null
  void (*loop)();         // Called every loop() iteration if non-null
  void (*routes)();       // Registers HTTP routes if non-null
  bool  active;           // Whether the tool is currently running
};
```

The global registry `g_tools[]` in `main.cpp` holds all modules. `setup()`,
`loop()`, and `routes()` iterate over the full table.

### Module lifecycle

```
setup():  for each module with a setup fn  → module.setup()
loop():   for each module with a loop fn   → module.loop()
routes(): for each module with a routes fn → module.routes()
```

Modules are statically allocated. There is no dynamic module loading.

### Dependencies between modules

Modules communicate only through explicitly shared globals (the promiscuous
frame callback output, the channel state, the TX budget counter). They do not
call each other's internal functions. This ensures that adding or removing a
tool does not break another tool's compilation.

### Core module lifecycle (src/core/)

The 20 `src/core/` modules follow a plain C++ struct pattern. All state is
value-initialized at construction. No dynamic allocation is used except where
explicitly required. Modules are instantiated as globals in `main.cpp` and
never destroyed.

### Creating a new tool

See [docs/architecture.md](./docs/architecture.md) for a complete step-by-step
guide to adding a new `ToolModule` including the `ToolModule` struct, route
registration, rate limiting integration, and native test file template.

---

## Diagnostics

Phantom32 includes a boot-time diagnostic system (`PhantomDoctor`) that runs
three checks immediately after hardware initialization in `setup()`:

| Check | What it validates |
|---|---|
| `Heap` | Free heap > 16 KB critical threshold at boot |
| `Storage` | LittleFS mounted and readable (`totalBytes() > 0`) |
| `WiFi` | Soft AP interface has a valid IP address |

Each check produces a `CheckStatus`: `PASS`, `WARN`, `FAIL`, or `SKIPPED`.
An unset check remains `PENDING`.

The `overallStatus()` method returns a single `DiagStatus`:
- `INCOMPLETE` — session not finalized, or any check still `PENDING`
- `FAIL` — any check is `FAIL`
- `WARN` — any check is `WARN`, or any string warnings were added
- `PASS` — all checks `PASS` or `SKIPPED`

### Accessing diagnostic results

```
GET /api/phantom-doctor
```

```json
{
  "status": "PASS",
  "pass": 3,
  "warn": 0,
  "fail": 0,
  "pending": 0
}
```

### Example diagnostic report (serial)

```
PHANTOM DOCTOR  [PASS]

  Heap                           PASS  (OK)
  Storage                        PASS
  WiFi                           PASS

Report generated at t=3842 ms.
```

### Diagnostic validation

The diagnostic system has 28 unit tests covering:
- Check registration (null/empty/duplicate/full)
- Status transitions (PASS/WARN/FAIL/SKIPPED)
- `allPassed()` blocking on PENDING checks
- `overallStatus()` for all four states (PASS/WARN/FAIL/INCOMPLETE)
- `generateReport()` null-safety and output correctness

---

## Recovery

The `RecoveryEngine` tracks named failures, state transitions, and
resolution attempts. Recovery handlers can be registered per failure name;
`attemptRecovery()` invokes the handler and only marks the failure resolved
if the handler returns `true`.

### State machine

```
NORMAL ──(LOW/MEDIUM failure)──▶ DEGRADED
       ──(HIGH failure)─────────▶ SAFE_MODE
       ──(CRITICAL failure)──────▶ FAILED

DEGRADED/SAFE_MODE ──(all resolved)──▶ NORMAL
enterSafeMode()    ──(explicit call)──▶ SAFE_MODE
```

### Handler callback interface

```cpp
using RecoveryHandler = bool (*)(const char *failureName, void *ctx);

// Register a handler for a specific failure name:
re.registerHandler("LowHeap", [](const char *, void *) {
  // e.g. stop active tools to free memory
  if (g_active) tool_stop(g_active);
  return esp_get_free_heap_size() > HEAP_CRIT_THRESHOLD;
}, nullptr);
```

If no handler is registered for a failure, `attemptRecovery()` resolves it
unconditionally (backward-compatible behavior). This means existing code that
calls `attemptRecovery()` without registering handlers continues to work.

### Firmware integration (live)

The `loop()` function samples the heap every 30 seconds:

```cpp
if (now - g_last_profiler_sample >= 30000) {
  g_resource_profiler.sampleSystem(now);
  g_quality_scorecard.recordValue("FreeHeap", (double)esp_get_free_heap_size());
  if (esp_get_free_heap_size() < HEAP_CRIT_THRESHOLD)
    g_recovery_engine.recordFailure("LowHeap", "Heap below critical",
                                    FailureSeverity::HIGH, true, now);
}
```

### Recovery limitations

- The firmware does not automatically restart modules or tools after resolution.
  Handler callbacks must implement actual remediation; the engine tracks state only.
- No persistent failure log across reboots (recovery state is RAM-only).
- Maximum 16 concurrent tracked failures, 16 registered handlers.
- Hardware-level failures (watchdog reset, stack overflow) bypass the engine.

---

## Resource Monitoring

`ResourceProfiler` tracks RAM, CPU, and storage usage per module.
`sampleSystem()` reads the actual ESP32 heap using the ESP-IDF APIs and
updates the `"System"` module profile:

```cpp
// In loop() every 30 s — calls esp_get_free_heap_size() and
// heap_caps_get_total_size(MALLOC_CAP_DEFAULT) on firmware builds.
// No-op on native test builds (PHANTOM32_NATIVE_TEST defined by build flags).
g_resource_profiler.sampleSystem(millis());
```

### Available measurements

| Metric | Method | Real or Simulated |
|---|---|---|
| Free heap | `esp_get_free_heap_size()` | **Real** (firmware only) |
| Total heap | `heap_caps_get_total_size(MALLOC_CAP_DEFAULT)` | **Real** (firmware only) |
| Per-module RAM | `recordRam(name, bytes)` | Manual — caller-supplied |
| Per-module CPU | `recordCpu(name, microseconds)` | Manual — caller-supplied |
| Per-module storage | `recordStorage(name, bytes)` | Manual — caller-supplied |

### Accessing resource data

```
GET /api/resource-profile
```

```json
{ "freeHeap": 142320, "totalHeap": 327680 }
```

### Hardware measurement (reference — 2026-11-25)

Measured on ESP32-WROOM-32D rev3 at 240 MHz immediately after boot:

```
Free heap at boot:   162,232 bytes
DRAM used:           35.3% (of 320 KB)
Flash used:          85.0% (of 4 MB)
```

These are single-point measurements at first boot. Long-duration heap
measurements under active tool operation have not yet been published.
See [Hardware Validation](#hardware-validation).

---

## Testing Architecture

Phantom32 uses three layers of automated testing, all running on the host
machine with no hardware required.

### Unit tests

```bash
pio test -e native
```

Each test suite is a standalone binary compiled from a single `.cpp` file.
Tests use the Unity framework. No external mocking framework is used; each
file stubs only the specific platform symbols it needs. Stubs are designed to
faithfully model the real ESP32/Arduino behavior they replace.

### AddressSanitizer + UBSan

```bash
pio test -e native_asan
```

Re-runs the full test suite with AddressSanitizer and UndefinedBehaviorSanitizer
enabled. Catches heap buffer overflows, stack buffer overflows, use-after-free,
uninitialized reads, integer overflow, and misaligned memory access.

### Coverage (gcov/lcov)

```bash
pio test -e native_cov
bash scripts/coverage.sh
```

Enforced minimums: **≥ 85% line coverage**, **≥ 80% branch coverage** on `src/`
files. Test files are excluded from coverage measurement. CI fails if either
threshold is not met.

### What is and is not covered by native tests

| Validated | Not validated (requires hardware) |
|---|---|
| Frame parsing logic | Raw 802.11 frame injection |
| Session lifecycle and token format | Promiscuous callback dispatch |
| Input validation for all argument types | NVS read/write |
| Rate limiter token bucket algorithm | LittleFS operations |
| Evil twin state machine | FreeRTOS task scheduling |
| Diagnostic status transitions | Physical AP association |
| Recovery handler invocation and state | ESP32 heap fragmentation over time |
| Resource profiler per-module tracking | Hardware-specific timing |

### Static analysis

The CI static-analysis job checks:

- `clang-format --dry-run --Werror` on all `src/` files (enforced)
- Hardcoded AP password check (enforced; must not appear in non-define contexts)
- gitleaks secret scanning of full git history (enforced)

---

## Test Results

### Latest run (2026-11-26, commit `bae748e`)

| Environment | Tests | Passed | Failed | Duration |
|---|---|---|---|---|
| `native` | 1461 | **1461** | 0 | 16.0 s |
| `native_asan` | 1461 | **1461** | 0 | 17.8 s |

Coverage is measured in CI on every push. The minimum thresholds (≥ 85% line,
≥ 80% branch) have been enforced since v3.1. Exact percentages for the latest
build are available in the coverage-report artifact on the
[latest CI run](https://github.com/MehaanM/Operation-Phantom32/actions).

### Test suite breakdown

| Test suite | Count | What it covers |
|---|---|---|
| `test_frame_parsing` | 42 | 802.11 type/subtype, MAC, SSID, HT/VHT/WPS, TSF, sequence |
| `test_input_validator` | 76 | All input validation and sanitization paths |
| `test_device_tracker` | 28 | Device deduplication, probe SSID list, RSSI history ring |
| `test_auth_manager` | 29 | Session lifecycle, lockout, password validation, constant-time compare |
| `test_security` | 48 | Expired/revoked sessions, unauthorized access, malformed JSON, rate-limit exhaustion |
| `test_rate_limiter` | 21 | Token bucket, per-tool limits, global TX budget, HTTP per-IP |
| `test_evil_twin` | 29 | Credential ring, HTML escaping, state machine |
| `test_phantom_doctor` | 28 | Check registration, status transitions, overallStatus(), pendingCount() |
| `test_recovery_engine` | 28 | Failure tracking, handler callbacks, state transitions |
| `test_quality_scorecard` | 29 | Metric registration, threshold validation, trend detection |
| `test_resource_profiler` | 21 | Module registration, RAM/CPU/storage recording, snapshots |
| `test_observability_dashboard` | 19 | Log entries, module status, ring buffer |
| `test_error_context` | 17 | Error code registry, active error tracking |
| `test_progress_reporter` | 21 | Operation lifecycle, percent complete |
| `test_benchmark_suite` | — | Benchmark timing infrastructure |
| `test_fault_injector` | — | Controlled fault injection |
| `test_simulation_engine` | — | Deterministic simulation |
| `test_integration` | — | Cross-module integration stubs |
| `test_negative` | — | Invalid input rejection across modules |
| `test_coverage` | — | Coverage-targeted branch paths |
| (20+ more suites) | — | Safety, diagnostics, logger, config, HAL contracts, … |

For the complete machine-generated test output, download the `test-output-native`
artifact from the
[latest CI run](https://github.com/MehaanM/Operation-Phantom32/actions).

---

## Continuous Integration

### Pipeline overview

Every push to `main`, `develop`, or any `release/**` branch, and every pull
request targeting `main`, triggers the full pipeline. The pipeline runs jobs
in parallel where possible; the `all-builds-passed` gate job enforces that all
prior jobs succeeded before the commit is considered green.

```
push/PR
  │
  ├── [parallel] Unit tests (native)
  ├── [parallel] Sanitizers (ASan + UBSan)
  ├── [parallel] Coverage (gcov/lcov) ─── enforces ≥85% line, ≥80% branch
  ├── [parallel] Static analysis ─────────── clang-format, hardcoded cred check
  ├── [parallel] Secret scanning (gitleaks) ─ full git history
  ├── [parallel] Dependency scan (pip-audit)
  ├── [parallel] Generate SBOM (CycloneDX)
  ├── [parallel] Build matrix (13 boards) ── firmware artifacts + SHA-256
  │
  └── all-builds-passed ───────────────── gates on all above jobs
```

### CI job details

| Job | Runs on | What it validates | Failure blocks merge |
|---|---|---|---|
| Unit tests (native) | ubuntu-latest | 1461 test cases pass | Yes |
| Sanitizers (ASan + UBSan) | ubuntu-latest | No memory errors, no UB | Yes |
| Coverage (gcov) | ubuntu-latest | ≥ 85% line, ≥ 80% branch | Yes |
| Static analysis | ubuntu-latest | clang-format compliance, no hardcoded credentials | Yes |
| Secret scanning | ubuntu-latest | No secrets in git history | Yes |
| Dependency scan | ubuntu-latest | No known CVEs in CI tools (gating); full tree logged | Partial |
| SBOM generation | ubuntu-latest | CycloneDX SBOM produced | No |
| Build matrix | ubuntu-latest | All 13 PlatformIO environments compile | Yes (via gate) |
| Compatibility matrix | ubuntu-latest | Build sizes reported | No |

### Artifacts (retained 7 days)

| Artifact | Contents |
|---|---|
| `test-output-native` | Full Unity test output (native) |
| `test-output-asan` | Full Unity test output (ASan/UBSan) |
| `coverage-report` | `coverage.info` and `coverage_filtered.info` (lcov) |
| `pip-audit-report` | pip-audit JSON report |
| `gitleaks-results.sarif` | Secret scan SARIF report |
| `sbom-cyclonedx` | CycloneDX SBOM |
| `compatibility-matrix` | Per-environment RAM/flash usage |

Firmware `.bin` artifacts with SHA-256 checksums and build manifests are
uploaded on `main` branch builds.

### Security controls in CI

- All `actions/` pins use full commit SHA (not tags)
- Default permissions: `contents: read`
- Each job declares only the permissions it needs
- Concurrency group cancels in-progress runs on new pushes
- `timeout-minutes` set on every job

### Workflow definition

[.github/workflows/build.yml](./.github/workflows/build.yml)

[View latest CI run →](https://github.com/MehaanM/Operation-Phantom32/actions)

---

## Performance Benchmarks

> **Methodology note:** The values below were measured once on a single board
> under specific conditions. They are reported as single observations, not
> statistical distributions. BenchmarkSuite infrastructure exists in
> `src/core/BenchmarkSuite.h` for future systematic measurement.

### Boot measurements (ESP32-WROOM-32D rev3, 2026-11-25)

| Measurement | Value | Method |
|---|---|---|
| Free heap at boot | 162,232 bytes | Serial log at startup |
| DRAM usage at boot | 35.3% (113 KB of 320 KB) | PlatformIO build report |
| Flash usage | 85.0% (3.37 MB of 4 MB) | PlatformIO build report |
| Firmware version | 3.2.0 | Hardcoded in `main.cpp` |

### Build-time measurements (CI, latest main)

| Metric | Value | Notes |
|---|---|---|
| Test suite execution time (native) | ~16 s | 1461 tests on ubuntu-latest |
| Test suite execution time (ASan) | ~18 s | 1461 tests with sanitizers |

### Measurements not yet collected

The following have not been measured on physical hardware. They are listed
here so the gap is visible rather than hidden behind a placeholder number:

- AP association latency
- HTTP request round-trip time
- Login PBKDF2 latency (expected ~1 s at 100,000 iterations on ESP32 @ 240 MHz)
- Frame injection rate (expected ≤ 200 fps, enforced by rate limiter)
- Minimum free heap under sustained tool operation
- 24-hour heap trend
- Module initialization time

These measurements are planned for the hardware validation cycle in v3.3.

---

## Hardware Validation

For the full physical validation record, see
[docs/hardware-validation.md](./docs/hardware-validation.md).

### Summary

| Test | Board | Date | Result |
|---|---|---|---|
| Serial credential output on first boot | ESP32-WROOM-32D (DevKit V1) | 2026-11-25 | ✅ Passed |
| AP Wi-Fi password enforced (device-unique) | ESP32-WROOM-32D (DevKit V1) | 2026-11-25 | ✅ Passed |
| Dashboard login with printed password | ESP32-WROOM-32D (DevKit V1) | 2026-11-25 | ✅ Passed |
| NVS credential persistence across reboot | ESP32-WROOM-32D (DevKit V1) | 2026-11-25 | ✅ Passed |
| RAM budget (35.3% DRAM used) | ESP32-WROOM-32D (DevKit V1) | 2026-11-25 | ✅ Passed |
| Flash budget (85.0% used) | ESP32-WROOM-32D (DevKit V1) | 2026-11-25 | ✅ Passed |

### Outstanding hardware tests

The following have not yet been run and published on physical hardware:

- 24-hour idle stability test
- 24-hour active tool operation (Beacon Spam + Device Tracker)
- 20-consecutive-boot reliability test
- Reset/recovery test (20 cycles)
- PSRAM board validation (WROVER or ESP32-S3)
- PhantomDoctor, RecoveryEngine, ResourceProfiler diagnostic runtime on hardware
- OTA update (not yet implemented)

---

## Stability Testing

Stability tests have not yet been published. The following are planned for v3.3:

| Test | Target | Status |
|---|---|---|
| Idle runtime | 24 hours, ESP32-WROOM-32D | Planned |
| Active tool runtime (Beacon Spam) | 24 hours, ESP32-WROOM-32D | Planned |
| Repeated boot reliability | 20 consecutive boots | Planned |
| Power interruption behavior | 10 interruptions during active tool | Planned |
| Memory stability (heap trend) | 24 hours, sampled every 30 s | Planned |
| Extended soak | 72 hours idle | Planned |

The 30-second heap sampling in `loop()` (added in v3.2) generates the data
needed for the memory stability test once hardware time is allocated.

---

## Reproducible Builds

### Environment

| Component | Version | Source |
|---|---|---|
| PlatformIO Core | 6.2.0 | `pip install platformio==6.2.0` |
| espressif32 platform | 6.5.0 | `platformio.ini` |
| ESP-IDF (bundled) | (managed by PlatformIO) | automatic |
| NimBLE-Arduino | ^1.4.1 | `lib_deps` in `platformio.ini` |
| C++ standard | C++14 | `-std=gnu++14` (framework default) |
| Python (CI scripts) | 3.11 | `actions/setup-python@v5` |

### Build configuration

The build is defined entirely in `platformio.ini`. All 13 environments use
explicit platform version pins (`espressif32@6.5.0`). No floating version
references exist.

### Known non-deterministic elements

- The `__DATE__` and `__TIME__` macros in the banner string change on every
  build by design.
- The firmware does not embed git hashes in the binary by default.
- LittleFS filesystem image creation order may vary.

### Verification

```bash
# Build the WROOM-32D firmware
pio run -e doom

# Check SHA-256
sha256sum .pio/build/doom/firmware.bin
```

Firmware artifacts with SHA-256 checksums are uploaded as CI artifacts on
every `main` branch build and are available from the GitHub Actions run page.

---

## Releases

### Latest stable: v3.2.0 (2026-11-25)

**Summary:** Reliability and release quality milestone. Physical hardware
validation on ESP32-WROOM-32D confirmed. Full diagnostic runtime (phases 18–20)
complete. 1461 tests passing.

**What's new:**
- Phases 18–20: PhantomDoctor, RecoveryEngine, ResourceProfiler, QualityScorecard,
  ObservabilityDashboard, ErrorContext, ProgressReporter, TestGenerator — 20 new
  core modules with 400+ new test cases
- Handler callback interface for RecoveryEngine
- `sampleSystem()` with real ESP32 heap APIs in ResourceProfiler
- Threshold validation and min-2-sample trend in QualityScorecard
- `generateFile()` in TestGenerator for compilable Unity .cpp output
- `/api/phantom-doctor` and `/api/resource-profile` API routes
- PlatformIO upgraded to 6.2.0 (resolves DEP-001: 16 CVEs in prior version)
- PBKDF2 work factor raised to 100,000 iterations

**Changes:** See [CHANGELOG.md](./CHANGELOG.md) for the complete entry.

**Supported hardware:** See [Hardware Compatibility](#hardware-compatibility).

**Downloads and SHA-256 checksums:** Available from the
[GitHub Actions artifacts](https://github.com/MehaanM/Operation-Phantom32/actions)
for the v3.2.0 build. GitHub Releases with signed artifacts are planned for v3.3.

---

## Security

### Principles

- Credentials are never hardcoded in the firmware binary.
- All security-sensitive state (tokens, password hash) is stored in DRAM,
  not PSRAM, to avoid PSRAM's lack of ECC protection.
- Authentication is mandatory; every protected route calls `auth_check()`
  before any handler runs.
- Session tokens are RAM-only; they are never written to flash or NVS.
- All credential comparisons use constant-time algorithms to prevent
  timing-based side channels.

### Authentication architecture

```
Client ──POST /login──▶ PBKDF2-HMAC-SHA256 (100,000 iter) ──▶ compare (constant-time)
                                                                       │
                                                             ┌── Match ┘
                                                             ▼
                                                   256-bit session token (HttpOnly cookie)
                                                   30 min TTL · RAM only
```

### Supported versions

| Version | Security support |
|---|---|
| `main` | Yes — latest |
| `develop` | Yes |

### Reporting a vulnerability

**Do not open a public GitHub issue for security bugs.**

Report privately via [GitHub Security Advisories](https://github.com/MehaanM/Operation-Phantom32/security/advisories/new).
Response within 72 hours.

For the full security policy, disclosure process, and what is and is not
considered a vulnerability, see [SECURITY.md](./SECURITY.md).

---

## Responsible Use

Phantom32 is designed for:

- Authorized wireless diagnostic work on networks you own or manage
- Security research in controlled laboratory environments
- Educational study of 802.11 frame structure and wireless protocol behavior
- Defensive analysis: identifying vulnerabilities in your own infrastructure

**Boundaries.** The firmware includes tools capable of disrupting Wi-Fi
networks and capturing 802.11 traffic. These capabilities are legal only in
environments where you have explicit permission to operate. Using this firmware
on networks, devices, or infrastructure you do not own or have written
authorization to test is illegal in most jurisdictions and is not an intended
or supported use.

The project does not ship with pre-configured attack targets, automated
scanning against arbitrary networks, or capabilities designed to bypass network
access controls for unauthorized access. It is a toolkit that requires deliberate
configuration and operation by the user.

**Read [LEGAL.md](./LEGAL.md) before deploying this firmware.**

---

## Limitations and Known Issues

This section documents genuine gaps. Hiding limitations does not make them
disappear; it makes them harder to find for people who need to know.

### Hardware

- Only the ESP32-WROOM-32D has been physically validated. All other boards
  are CI-compiled only.
- 24-hour and 72-hour stability tests have not been published.
- PSRAM board physical testing (WROVER, S3) has not been published.
- OTA update is not implemented.

### Firmware

- The 30-second heap sampling interval in `loop()` means LowHeap detection
  can lag up to 30 seconds behind an actual critical heap condition.
- `RecoveryEngine` state is RAM-only: failures and recovery state do not
  survive reboot.
- Maximum 16 concurrent tracked failures in RecoveryEngine.
- PhantomDoctor performs no dynamic re-checks after boot; it reflects
  boot-time state only.
- Flash usage is 85% on the WROOM-32D LITE build, leaving approximately
  600 KB headroom. Adding large features may require a partition table change.

### Testing

- Native tests cannot validate raw 802.11 frame injection, NVS operations,
  FreeRTOS scheduling, or hardware-level timing.
- BLE tools, PMKID hunt, and several other tools have minimal or no dedicated
  unit tests (see Feature Maturity Matrix for the complete picture).
- Hardware-in-the-loop testing infrastructure does not exist yet.

### Documentation

- API reference is incomplete for newer tools added in v3.2.
- Hardware validation reports for boards other than WROOM-32D are outstanding.
- Interface screenshots have not been published.

Track all open issues at:
[github.com/MehaanM/Operation-Phantom32/issues](https://github.com/MehaanM/Operation-Phantom32/issues)

---

## Failure Analysis

This section documents how Phantom32 behaves when things go wrong. For each
failure mode: how it is detected, how it is reported, how it is handled,
whether recovery is possible, and what happens if recovery fails.

### Boot-time failures (PhantomDoctor)

| Failure | Detection | Reporting | Handling | Recoverable? | If not recovered |
|---|---|---|---|---|---|
| Free heap < 16 KB at boot | `esp_get_free_heap_size()` in `setup()` | `PhantomDoctor` check → FAIL; `GET /api/phantom-doctor` returns `"status":"FAIL"` | Boot continues; dashboard shows FAIL status | No automatic action | Device runs degraded; operator must reboot or free memory |
| LittleFS not mounted | `LittleFS.begin()` returns false | `PhantomDoctor` check → FAIL | PCAP capture disabled; web UI still serves | No | PCAP routes return 500; device operates without filesystem |
| Soft AP has no IP | `WiFi.softAPIP() == INADDR_NONE` | `PhantomDoctor` check → FAIL | HTTP server still starts on the AP interface | No automatic action | Web UI may be unreachable; operator must recheck AP config |

### Runtime failures (RecoveryEngine)

| Failure | Detection | Reporting | Handling | Recoverable? | If recovery fails |
|---|---|---|---|---|---|
| Heap drops below 16 KB | `esp_get_free_heap_size() < HEAP_CRIT_THRESHOLD` in `loop()` every 30 s | `RecoveryEngine.recordFailure("LowHeap", HIGH)` | Registered handler called (default: attempt to stop active tool); state → SAFE_MODE | Yes, if handler frees memory | RecoveryEngine stays DEGRADED/SAFE_MODE; active tool may keep running; next 30 s check re-evaluates |
| Module records a HIGH failure | `re.recordFailure(name, HIGH, ...)` | RecoveryEngine state → SAFE_MODE; `GET /api/resource-profile` reflects current heap | Handler invoked; resolution only on handler returning true | Yes, if handler returns true | Failure stays active; `activeFailureCount()` > 0; state does not return to NORMAL |
| Module records a CRITICAL failure | `re.recordFailure(name, CRITICAL, ...)` | RecoveryEngine state → FAILED | No automatic recovery | No (FAILED state has no auto-recovery path in v3.2) | Device stays in FAILED state; operator reboot required |

### Input validation failures

| Failure | Detection | Reporting | Handling | Recoverable? |
|---|---|---|---|---|
| Invalid SSID (length, characters) | `input_validator.h` on each web request | HTTP 400 JSON: `{"ok":false,"error":"invalid_ssid"}` | Request rejected; no tool state change | N/A — client error |
| Invalid MAC address format | `input_validator.h` | HTTP 400 JSON | Request rejected | N/A |
| Out-of-range channel number | `input_validator.h` | HTTP 400 JSON | Request rejected | N/A |
| Oversized input | `input_validator.h` | HTTP 400 JSON | Request rejected; connection closed | N/A |

### Authentication failures

| Failure | Detection | Reporting | Handling | Recoverable? |
|---|---|---|---|---|
| Wrong password | PBKDF2 comparison fails | HTTP 401; failed-attempt counter incremented | IP temporarily locked after 5 failures | N/A — client error |
| IP lockout | Failed-attempt count ≥ 5 | HTTP 429 `{"error":"too_many_attempts"}` | All requests from that IP rejected for 5 min | Yes — wait 5 minutes |
| Expired session token | Token TTL check fails | HTTP 401; redirect to `/login` | Client must re-authenticate | Yes |
| Invalid/replayed token | Token not found in RAM store | HTTP 401 | Same as expired | Yes |

### Configuration failures

| Failure | Detection | Reporting | Handling | Recoverable? |
|---|---|---|---|---|
| Invalid metric threshold in QualityScorecard | `warnThreshold` / `failThreshold` relationship invalid | `trackMetric()` returns false | Metric not registered; no data collected | Yes — caller must fix threshold values |
| NVS read error at boot | `nvs_get_*` returns non-OK | Credential regenerated; new credentials printed to serial | First-boot initialization runs again | Yes — NVS is re-initialized |
| NVS namespace full | `nvs_set_*` returns `ESP_ERR_NVS_NOT_ENOUGH_SPACE` | Error logged to serial | Setting not persisted; in-memory value used | Partial — in-memory only until reboot |

### Diagnostic system failures

| Failure | Detection | Reporting | Handling | Recoverable? |
|---|---|---|---|---|
| `PhantomDoctor.finalise()` called twice | `complete_` flag already set | `finalise()` returns false | Report already complete; no changes | N/A |
| Check set after finalise | `setCheckResult()` after `complete_` | Returns false | Check ignored | N/A |
| All checks PENDING at finalise | `pendingCount() > 0` after `finalise()` | `overallStatus()` returns INCOMPLETE | Report shows INCOMPLETE; `/api/phantom-doctor` reflects this | No automatic re-check; operator must investigate |

### Recovery system limitations

The RecoveryEngine tracks failure state and invokes handlers but does not:
- Restart modules or tools after resolution (the handler must do this)
- Persist failure state across reboots
- Handle more than 16 concurrent tracked failures or 16 registered handlers
- Prevent hardware-level failures (watchdog reset, stack overflow) from
  bypassing the engine entirely

A watchdog reset or stack overflow produces an ESP32 panic and reboot. The
RecoveryEngine has no visibility into these events; the firmware starts fresh
with a clean state on the next boot.

---

## Troubleshooting

| Problem | Likely cause | Solution |
|---|---|---|
| `DRAM overflow` on build | New large static array | Guard with `#ifdef PHANTOM32_LITE`; check `pio run -e doom 2>&1 \| grep "RAM:"` |
| Upload fails: port not found | Wrong port or missing driver | `pio device list`; install CP2102 or CH340 driver |
| Cannot log in / forgot password | Credentials unknown | Open serial at 115200 baud, reboot — password prints on boot |
| Web UI does not load | Not on Phantom32 AP | Join the `Phantom32` Wi-Fi; use `http://` not `https://` |
| Captive portal intercepts browser | OS captive portal detection | Dismiss and navigate to `http://192.168.4.1/dashboard` manually |
| BLE tools not working (C3) | NimBLE init difference | Ensure `CONFIG_BT_NIMBLE_MAX_CONNECTIONS=1` (set in `platformio.ini`) |
| RISC-V toolchain not found (Apple Silicon) | Missing Rosetta | `softwareupdate --install-rosetta` then retry |
| `clang-format` CI failure | Version mismatch (local vs CI) | Run `clang-format -i src/*.h src/*.cpp` on CI's version (21.x) |
| Analytics route returns `{"lite":true}` | LITE build | Use a PSRAM board (`doom_wrover`, `doom_s3`, etc.) |
| Flash too full to add features | 85% flash usage on WROOM-32D | Use `partitions_16mb.csv` on a 16 MB board, or move assets to filesystem |

For more detailed troubleshooting procedures, see
[docs/troubleshooting.md](./docs/troubleshooting.md).

---

## FAQ

**What hardware is supported?**
Any ESP32 board with 4 MB flash and Wi-Fi. 13 environments are CI-compiled.
Only the ESP32-WROOM-32D DevKit V1 has been physically validated to date.
See [Hardware Compatibility](#hardware-compatibility).

**How do I compile the firmware?**
`pip install platformio` then `pio run -e doom`. See [Quick Start](#quick-start).

**How do I run the tests?**
`pio test -e native` runs all 1461 tests on your host machine. No ESP32 required.

**How do I run with sanitizers?**
`pio test -e native_asan` — same tests with AddressSanitizer and UBSan.

**How do I report a bug?**
Open an issue at
[github.com/MehaanM/Operation-Phantom32/issues](https://github.com/MehaanM/Operation-Phantom32/issues).
For security issues, use [GitHub Security Advisories](https://github.com/MehaanM/Operation-Phantom32/security/advisories/new) instead.

**Which features are experimental?**
See the Feature Maturity Matrix. Any subsystem marked without `HW Validated`
has not been physically tested on hardware.

**Where is the architecture documentation?**
[docs/architecture.md](./docs/architecture.md)

**How do I contribute?**
See [Contributing](#contributing) and [CONTRIBUTING.md](./CONTRIBUTING.md).

**What is PHANTOM32_LITE?**
A compile flag set automatically for non-PSRAM boards that reduces static
pool sizes to fit in 320 KB DRAM. All research tools work; analytics modules
are disabled. See [Configuration](#configuration).

**Can I use this on a network I don't own?**
No. See [Responsible Use](#responsible-use) and [LEGAL.md](./LEGAL.md).

**How do I verify a release?**
SHA-256 checksums are available in the CI artifacts for each `main` build.
GPG-signed releases are planned for v3.3.

---

## Roadmap

For the complete roadmap with issue links, see [docs/roadmap.md](./docs/roadmap.md).

### v3.2 — Reliability and release quality (current)

- [x] Hardware validation: ESP32-WROOM-32D boot, auth, NVS, AP password
- [x] Phases 18–20: 20-module diagnostic runtime complete
- [x] 1461 tests passing; CI enforces ≥ 85% line, ≥ 80% branch
- [x] PlatformIO 6.2.0 (resolves DEP-001)
- [x] PBKDF2 work factor 100,000 iterations
- [x] Release checklist (`docs/release-checklist.md`)
- [ ] 24-hour stability test on ESP32-WROOM-32D
- [ ] GitHub branch protection rules

### v3.3 — Engineering quality

- [ ] GPG-signed git tags and GitHub Release with firmware artifacts
- [ ] SBOM shipped as release artifact
- [ ] Hardware validation: ESP32-S3 (PSRAM full mode)
- [ ] 20-consecutive-boot reliability test
- [ ] 24-hour stability test with heap measurements
- [ ] Hardware benchmarks (boot time, login latency, heap trend)
- [ ] Architecture decision records (ADR-001 to ADR-003)
- [ ] CI: fail on new high/critical CVEs in direct dependencies

### v3.4 — Documentation and open-source maturity

- [ ] OTA update (USB or AP-served)
- [ ] Complete API reference documentation
- [ ] `CONTRIBUTING.md` from clean machine
- [ ] Hardware-in-the-loop CI runner (self-hosted)
- [ ] Hardware validation photography workflow

---

## Development Environment

### Recommended IDE

VS Code with the [PlatformIO extension](https://platformio.org/install/ide?install=vscode).
The `.vscode/` configuration is not committed (developer-local).

### Toolchain

| Component | Version | Notes |
|---|---|---|
| PlatformIO Core | 6.2.0 | `pip install platformio==6.2.0` |
| GCC (native) | system | `sudo apt install gcc g++ make` on Ubuntu |
| clang-format | 21.x (CI) / any (local) | Used for formatting enforcement |
| Python | ≥ 3.8 | For CI scripts |

### Local development workflow

```bash
# 1. Clone
git clone https://github.com/MehaanM/Operation-Phantom32.git
cd Operation-Phantom32

# 2. Install PlatformIO
pip install "platformio==6.2.0"

# 3. Install ESP32 platform packages
pio pkg install

# 4. Run tests (fastest feedback loop — no hardware needed)
pio test -e native

# 5. Format all source files
clang-format -i src/*.h src/*.cpp

# 6. Build firmware
pio run -e doom

# 7. Check RAM and flash budget
pio run -e doom 2>&1 | grep -E "RAM:|Flash:"

# 8. Run with sanitizers
pio test -e native_asan

# 9. Generate local coverage report
pio test -e native_cov && bash scripts/coverage.sh
```

### Code style

- C++ standard: **C++14**
- Indentation: **2 spaces**, no tabs
- Line length: **100 characters** max
- Naming: `PascalCase` for structs/enums; `snake_case` for functions/variables
- All `src/` files must pass `clang-format --dry-run --Werror` (CI enforces this)

### Hardware debugging

```bash
# Open serial monitor
pio device monitor --baud 115200

# Flash and immediately open monitor
pio run -e doom --target upload && pio device monitor --baud 115200
```

For detailed development setup including OS-specific notes, see
[CONTRIBUTING.md](./CONTRIBUTING.md).

---

## Contributing

### Before opening a pull request

1. Run `pio test -e native` and confirm 0 failures.
2. Run `pio test -e native_asan` and confirm 0 failures.
3. Run `clang-format -i src/*.h src/*.cpp` and commit the result.
4. If you added a new source file, add a corresponding test suite.
5. If you changed behavior documented in `docs/`, update the docs.

### Submission process

1. Fork the repository.
2. Create a branch from `main`: `git checkout -b feat/my-feature`
3. Make your changes with descriptive commits.
4. Open a pull request against `main`.
5. CI must pass before review.

### Pull request requirements

- All existing tests must pass.
- New code must have test coverage (there is no minimum percentage for new files,
  but reviewable logic without tests will be asked about).
- `src/` files must be `clang-format` compliant.
- No hardcoded credentials in any form.
- For new research tools: document the tool in the PR description with its
  purpose, the specific 802.11 behavior it exercises, and any safety
  considerations.

For the complete contribution guide including RAM-conscious coding guidelines
and the tool development walkthrough, see [CONTRIBUTING.md](./CONTRIBUTING.md).

---

## Documentation Index

| Document | Description |
|---|---|
| [README.md](./README.md) | This file — project overview, quick start, all major sections |
| [docs/installation.md](./docs/installation.md) | Complete installation guide with preflight checker |
| [docs/architecture.md](./docs/architecture.md) | System design, module map, memory layout, authentication flow |
| [docs/hardware-compatibility.md](./docs/hardware-compatibility.md) | Per-board feature matrix, limitations, configuration |
| [docs/hardware-validation.md](./docs/hardware-validation.md) | Physical hardware test results |
| [docs/api-reference.md](./docs/api-reference.md) | Full HTTP API reference (all endpoints, schemas) |
| [docs/testing.md](./docs/testing.md) | Test strategy, coverage guide, running tests locally, requirements traceability matrix |
| [docs/decisions/ADR-001](./docs/decisions/ADR-001-header-only-platform-agnostic-core.md) | Why core modules are header-only and platform-agnostic |
| [docs/decisions/ADR-002](./docs/decisions/ADR-002-tool-module-registry.md) | Why the static ToolModule registry pattern was chosen |
| [docs/decisions/ADR-003](./docs/decisions/ADR-003-pbkdf2-100k-iterations.md) | Why PBKDF2-HMAC-SHA256 at 100,000 iterations |
| [docs/decisions/ADR-004](./docs/decisions/ADR-004-unity-native-testing.md) | Why Unity and no mocking framework |
| [docs/decisions/ADR-005](./docs/decisions/ADR-005-dram-not-psram-for-security-state.md) | Why security-sensitive state is in DRAM, not PSRAM |
| [docs/roadmap.md](./docs/roadmap.md) | Planned milestones with issue links |
| [docs/faq.md](./docs/faq.md) | Frequently asked questions |
| [docs/troubleshooting.md](./docs/troubleshooting.md) | Common issues and solutions |
| [docs/dependency-inventory.md](./docs/dependency-inventory.md) | Dependency versions and licenses |
| [docs/security/threat-model.md](./docs/security/threat-model.md) | Threat model |
| [docs/security/audit-report.md](./docs/security/audit-report.md) | Security audit report |
| [docs/engineering/memory-budget.md](./docs/engineering/memory-budget.md) | DRAM/IRAM/Flash budget analysis |
| [docs/release-checklist.md](./docs/release-checklist.md) | Pre-release and post-release procedures |
| [CHANGELOG.md](./CHANGELOG.md) | Versioned change history |
| [CONTRIBUTING.md](./CONTRIBUTING.md) | Developer setup and contribution guide |
| [SECURITY.md](./SECURITY.md) | Vulnerability reporting and security architecture |
| [LEGAL.md](./LEGAL.md) | Legal terms, disclaimers, jurisdiction guidance |
| [LICENSE](./LICENSE) | MIT License text |

---

## Dependencies and SBOM

| Dependency | Version | Purpose | License | Bundled |
|---|---|---|---|---|
| NimBLE-Arduino | ^1.4.1 | BLE stack (NimBLE on ESP32) | Apache 2.0 | No (PlatformIO lib) |
| Unity (test framework) | (PlatformIO bundled) | Native unit test runner | MIT | Yes (test only) |
| espressif32 platform | 6.5.0 | ESP32 toolchain + ESP-IDF | Apache 2.0 / proprietary | No |
| Arduino framework | (via espressif32) | Arduino core for ESP32 | LGPL 2.1 | No |
| LittleFS | (via Arduino) | Filesystem | MIT | No |
| lcov / gcov | system | Coverage report generation (CI only) | GPL | No |
| pip-audit | (CI) | Python dependency CVE audit | Apache 2.0 | No |
| gitleaks | (CI) | Secret scanning | MIT | No |

A machine-readable CycloneDX SBOM is generated on every CI build and
available as the `sbom-cyclonedx` artifact on the
[GitHub Actions run page](https://github.com/MehaanM/Operation-Phantom32/actions).

---

## Governance

**Maintainer:** Mehaan Minocha  
**Repository:** [github.com/MehaanM/Operation-Phantom32](https://github.com/MehaanM/Operation-Phantom32)

This is a solo-maintained open-source project. Contributions are welcome via
pull request; all PRs are reviewed by the maintainer. Feature proposals should
be raised as GitHub issues before implementation work begins.

Release decisions, breaking changes, and security disclosures are made by the
maintainer. There is no formal governance committee.

---

## Changelog

See [CHANGELOG.md](./CHANGELOG.md) for the full versioned history.

Entries follow [Keep a Changelog](https://keepachangelog.com/en/1.0.0/) format
with categories: `Added`, `Changed`, `Fixed`, `Removed`, `Security`.

---

## Credits

**Firmware:**
- Espressif Systems — ESP32 chip, ESP-IDF, Arduino core
- h2zero / [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) — BLE stack
- PlatformIO team — build system, library management, native test runner
- Unity ([throwtheswitch/Unity](https://github.com/ThrowTheSwitch/Unity)) — C unit test framework
- lcov / gcov (GNU) — coverage tooling

**Security tooling:**
- [gitleaks](https://github.com/gitleaks/gitleaks) — secret scanning
- [pip-audit](https://github.com/pypa/pip-audit) — Python dependency CVE audit
- [CycloneDX](https://cyclonedx.org/) — SBOM format

**Inspiration and reference:**
- Spacehuhn's [ESP8266 Deauther](https://github.com/SpacehuhnTech/esp8266_deauther)
  — demonstrated the feasibility of a fully on-device 802.11 research tool
- PwnAgotchi, Flipper Zero — framing for portable security toolkits

---

## License

MIT License. See [LICENSE](./LICENSE) for the full text.

Copyright (c) 2024 Mehaan Minocha

**What the MIT license permits:**
- Use, copy, modify, distribute, sublicense, and sell copies
- Use in commercial products

**What the MIT license requires:**
- Preserve the copyright notice and license text in all copies or substantial portions

**Additional terms:** Read [LEGAL.md](./LEGAL.md). The MIT license governs the
software as software. It does not grant authorization to operate this firmware
on networks or hardware you do not own or have permission to test. Legal
compliance is the operator's responsibility in all cases.

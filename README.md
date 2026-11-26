# Phantom32

**Wireless diagnostics and security research firmware for ESP32**

A production-grade, fully on-device Wi-Fi and Bluetooth toolkit that exposes a
local HTTP web UI at `192.168.4.1` — no cloud, no app, no IDE required.
Built to strict engineering standards: 13-board CI matrix, 1461 automated tests,
85% line / 80% branch coverage enforced, AddressSanitizer + UBSan, and a
complete diagnostic + recovery runtime.

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
[Status](#project-status) ·
[Features](#capabilities) ·
[Architecture](#architecture) ·
[Hardware](#hardware-compatibility) ·
[Quick Start](#quick-start) ·
[Installation](#installation) ·
[CI](#continuous-integration) ·
[Security](#security) ·
[Responsible Use](#responsible-use) ·
[Limitations](#limitations-and-known-issues) ·
[Docs](#documentation-index) ·
[License](#license)

---

## Why Phantom32?

Most ESP32 Wi-Fi research tools are single-file sketches that demonstrate
a technique but are not engineered for reliability, testability, or long-term
maintenance. Phantom32 was built with a different set of priorities.

| Property | Typical ESP32 tool | Phantom32 |
|---|---|---|
| Test coverage | None or manual | 1461 automated tests; ≥ 85% line coverage enforced in CI |
| Memory safety | Unchecked | AddressSanitizer + UBSan on every commit |
| Board support | One board | 13 environments in CI build matrix |
| Authentication | Hardcoded password or none | PBKDF2-HMAC-SHA256 (100k iterations), device-unique, session-scoped |
| Failure handling | Crash or silent | RecoveryEngine with handler callbacks; PhantomDoctor boot diagnostics |
| Build reproducibility | Dev-machine-only | Pinned platform + lib versions; CI artifacts with SHA-256 |
| Secret scanning | None | gitleaks on full git history, every push |

**What Phantom32 does not claim:**

- It is not faster, stealthier, or more effective than other tools at any
  specific 802.11 technique. The promiscuous capture rate and injection
  performance are limited by the ESP32 Wi-Fi stack.
- It does not replace professional audit tooling. It fills a different niche:
  a self-contained pocket device for authorized audits on networks you own.
- Stability tests have not been published yet. Long-duration reliability
  claims are not made until that data exists.

The differentiator is the engineering infrastructure around the tool, not
the tools themselves. For reasoning behind specific decisions, see
[docs/decisions/](./docs/decisions/).

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

---

## Showcase

### Verified boot output (ESP32-WROOM-32D, 2026-11-25)

```
rst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
...
[doom] ========================================
[doom] Phantom32 v3.2.0 — ESP32 Wireless Research Firmware
[doom] Chip: ESP32 rev3 @ 240 MHz  Free heap: 162232 bytes
[doom] FS: 36864/3964928 bytes used
[doom] AP: ssid=Phantom32 ip=192.168.4.1 ch=6
[auth] Generated new password: Ccd229bkMNFP
[doom] HTTP server started on port 80
[doom] Ready!
[doom] Connect: SSID=Phantom32 PASS=Ccd229bkMNFP
[doom] Browse:  http://192.168.4.1/dashboard
[doom] ========================================
```

> **Hardware photos and interface screenshots** will be added in v3.3 once the
> hardware validation photography workflow is established. See
> [docs/hardware-validation.md](./docs/hardware-validation.md).

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
| Aegis (Defense) | Passive 802.11 anomaly detection: deauth storms, rogue APs, KARMA indicators |
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

### Analytics Modules — PSRAM boards only

On boards with PSRAM (`BOARD_HAS_PSRAM`) the following are additionally enabled:
per-tool TX PPS graph, per-BSSID RSSI history (128-sample ring buffer),
sequence number tracking, STA/AP association state machine, and scan result
cache with entry aging.

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

For the complete feature maturity matrix (HW-validated vs. CI-compiled vs.
unimplemented), see [docs/hardware-validation.md](./docs/hardware-validation.md).

---

## Architecture

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

**Key architectural properties:**

- Each tool is a self-contained `ToolModule` struct registered in `g_tools[]`.
  Tools communicate only through explicitly named globals, never by calling
  each other's internals.
- All ESP32 SDK calls are gated by `#ifndef PHANTOM32_NATIVE_TEST`. Any file
  that compiles on the native host has no hidden platform calls.
- All HTTP routes pass through `auth_check()` before any handler runs.
- The 20-module diagnostic runtime in `src/core/` is entirely header-only,
  platform-agnostic C++, and runs identically on host and firmware.
- Security-sensitive state (session tokens, password hash) is stored in DRAM,
  never PSRAM.

For the full architectural description including module map, NVS namespace,
and authentication flow, see [docs/architecture.md](./docs/architecture.md).

---

## Hardware Compatibility

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

**`PHANTOM32_LITE` constraints** (non-PSRAM original ESP32 boards): pool sizes
are halved for static arrays to fit within 320 KB DRAM. Research tools all work;
analytics modules are disabled. See [docs/hardware-compatibility.md](./docs/hardware-compatibility.md)
for per-module limits and [docs/memory-constraints.md](./docs/memory-constraints.md)
for the full DRAM budget breakdown.

---

## Quick Start

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

Expected test output: `1461 test cases: 1461 succeeded`

---

## Installation

**Run the preflight checker first:**

```bash
python3 scripts/phantom_doctor.py
```

**Required tools:**

| Tool | Minimum version | Install |
|---|---|---|
| Python | 3.8 | [python.org](https://www.python.org) |
| PlatformIO Core | 6.2.0 | `pip install platformio` |
| esptool.py | 4.0 | `pip install esptool` |
| git | any | [git-scm.com](https://git-scm.com) |

**Supported OS:** macOS 12+, Ubuntu 20.04+, Windows 10+.

**Build a specific board:**

```bash
pio run -e doom          # ESP32-WROOM-32D (most common)
pio run -e doom_wrover   # ESP32-WROVER (4 MB PSRAM, full analytics)
pio run -e doom_s3       # ESP32-S3
```

**Flash:**

```bash
pio run -e doom --target upload
pio run -e doom --target uploadfs   # LittleFS filesystem
```

**First boot:** Open serial at 115200 baud. Note the AP Wi-Fi password and
dashboard password printed to serial. Connect to the `Phantom32` Wi-Fi network,
then browse to `http://192.168.4.1/dashboard`.

For complete OS-specific instructions and Apple Silicon notes, see
[docs/installation.md](./docs/installation.md).

---

## Continuous Integration

Every push triggers the full pipeline. All jobs must pass before the commit
is considered green (`all-builds-passed` gate job).

```
push/PR
  ├── Unit tests (native)              1461 tests, enforced
  ├── Sanitizers (ASan + UBSan)        1461 tests, enforced
  ├── Coverage (gcov/lcov)             ≥85% line, ≥80% branch enforced
  ├── Static analysis                  clang-format, hardcoded cred check
  ├── Secret scanning (gitleaks)       full git history
  ├── Dependency scan (pip-audit)      CVE check on CI Python deps
  ├── CI tooling SBOM (CycloneDX)      see note below
  └── Build matrix (13 boards)         firmware artifacts + SHA-256
```

**SBOM note:** The `sbom-cyclonedx` CI artifact is a CycloneDX inventory of
the **CI Python tooling environment** (PlatformIO, lcov, pip-audit, etc.),
not the firmware itself. The firmware's direct dependency is NimBLE-Arduino
(^1.4.1); the full firmware dependency inventory is in
[docs/dependency-inventory.md](./docs/dependency-inventory.md). A firmware
SBOM shipped as a release artifact is planned for v3.3.

**Test results (latest):**

| Environment | Tests | Passed | Duration |
|---|---|---|---|
| `native` | 1461 | **1461** | ~16 s |
| `native_asan` | 1461 | **1461** | ~18 s |

Coverage percentages for the latest build are in the `coverage-report` artifact
on the [latest CI run](https://github.com/MehaanM/Operation-Phantom32/actions).

For CI security controls and all artifact details, see
[.github/workflows/build.yml](./.github/workflows/build.yml).

---

## Security

- Credentials are never hardcoded in the firmware binary.
- All security-sensitive state (tokens, password hash) is stored in DRAM,
  not PSRAM, to avoid PSRAM's lack of ECC protection.
- Authentication is mandatory; every protected route calls `auth_check()`
  before any handler runs.
- Session tokens are RAM-only; they are never written to flash or NVS.
- All credential comparisons use constant-time algorithms.

```
Client ──POST /login──▶ PBKDF2-HMAC-SHA256 (100,000 iter) ──▶ compare (constant-time)
                                                                       │
                                                             ┌── Match ┘
                                                             ▼
                                                   256-bit session token (HttpOnly cookie)
                                                   30 min TTL · RAM only
```

Report vulnerabilities privately via
[GitHub Security Advisories](https://github.com/MehaanM/Operation-Phantom32/security/advisories/new).
See [SECURITY.md](./SECURITY.md) for the full policy.

---

## Responsible Use

Phantom32 is designed for authorized wireless diagnostic work on networks you
own or manage, security research in controlled laboratory environments,
educational study of 802.11 frame structure, and defensive analysis of your
own infrastructure.

**Boundaries.** The firmware includes tools capable of disrupting Wi-Fi
networks and capturing 802.11 traffic. These capabilities are legal only in
environments where you have explicit permission to operate. Using this firmware
on networks, devices, or infrastructure you do not own or have written
authorization to test is illegal in most jurisdictions.

**Read [LEGAL.md](./LEGAL.md) before deploying this firmware.**

---

## Limitations and Known Issues

This section documents genuine gaps. Hiding limitations does not make them
disappear; it makes them harder to find for people who need to know.

**Hardware:**
- Only the ESP32-WROOM-32D has been physically validated. All other boards are CI-compiled only.
- 24-hour and 72-hour stability tests have not been published.
- PSRAM board physical testing (WROVER, S3) has not been published.
- OTA update is not implemented.

**Firmware:**
- LowHeap detection can lag up to 30 seconds behind an actual critical heap condition.
- `RecoveryEngine` state is RAM-only: failures do not survive reboot.
- Maximum 16 concurrent tracked failures in RecoveryEngine.
- PhantomDoctor performs no dynamic re-checks after boot.
- Flash usage is 85% on the WROOM-32D LITE build (~600 KB headroom).

**Testing:**
- Native tests cannot validate raw 802.11 frame injection, NVS operations,
  FreeRTOS scheduling, or hardware-level timing.
- BLE tools, PMKID hunt, and several other tools have minimal or no dedicated unit tests.
- Hardware-in-the-loop testing infrastructure is not yet automated
  (see [docs/hil-testing.md](./docs/hil-testing.md)).

Track all open issues at:
[github.com/MehaanM/Operation-Phantom32/issues](https://github.com/MehaanM/Operation-Phantom32/issues)

---

## Documentation Index

| Document | Description |
|---|---|
| [docs/installation.md](./docs/installation.md) | Complete installation guide with preflight checker |
| [docs/architecture.md](./docs/architecture.md) | System design, module map, memory layout, authentication flow |
| [docs/hardware-compatibility.md](./docs/hardware-compatibility.md) | Per-board feature matrix, limitations, configuration |
| [docs/hardware-validation.md](./docs/hardware-validation.md) | Physical hardware test results and feature maturity matrix |
| [docs/memory-constraints.md](./docs/memory-constraints.md) | DRAM/IRAM/Flash budget, FreeRTOS task inventory, PHANTOM32_LITE pool sizes |
| [docs/hil-testing.md](./docs/hil-testing.md) | HIL testing framework, CI stub, and gap analysis vs. native tests |
| [docs/api-reference.md](./docs/api-reference.md) | Full HTTP API reference (all endpoints, schemas) |
| [docs/testing.md](./docs/testing.md) | Test strategy, coverage guide, requirements traceability matrix |
| [docs/decisions/](./docs/decisions/) | Architecture Decision Records (ADR-001 through ADR-005) |
| [docs/roadmap.md](./docs/roadmap.md) | Planned milestones with issue links |
| [docs/faq.md](./docs/faq.md) | Frequently asked questions |
| [docs/troubleshooting.md](./docs/troubleshooting.md) | Common issues and solutions |
| [docs/dependency-inventory.md](./docs/dependency-inventory.md) | Firmware dependency versions and licenses |
| [docs/security/threat-model.md](./docs/security/threat-model.md) | Threat model |
| [CHANGELOG.md](./CHANGELOG.md) | Versioned change history |
| [CONTRIBUTING.md](./CONTRIBUTING.md) | Developer setup, code style, and contribution guide |
| [SECURITY.md](./SECURITY.md) | Vulnerability reporting and security architecture |
| [LEGAL.md](./LEGAL.md) | Legal terms, disclaimers, jurisdiction guidance |

---

## Contributing

1. Run `pio test -e native` and `pio test -e native_asan` — confirm 0 failures.
2. Run `clang-format -i src/*.h src/*.cpp` and commit the result.
3. Add a test suite for any new source file.
4. Update `docs/` for any behavior changes.
5. For new research tools: document purpose, 802.11 behavior, and safety
   considerations in the PR description.

See [CONTRIBUTING.md](./CONTRIBUTING.md) for the complete guide including
RAM-conscious coding guidelines and tool development walkthrough.

---

## Dependencies

| Dependency | Version | Purpose | License |
|---|---|---|---|
| NimBLE-Arduino | ^1.4.1 | BLE stack | Apache 2.0 |
| Unity (test framework) | (PlatformIO bundled) | Native unit test runner | MIT |
| espressif32 platform | 6.5.0 | ESP32 toolchain + ESP-IDF | Apache 2.0 |
| Arduino framework | (via espressif32) | Arduino core for ESP32 | LGPL 2.1 |
| LittleFS | (via Arduino) | Filesystem | MIT |

For the complete inventory including CI tooling and licenses, see
[docs/dependency-inventory.md](./docs/dependency-inventory.md).

---

## Credits

**Firmware:** Espressif Systems (ESP32, ESP-IDF, Arduino core) ·
h2zero/[NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) ·
PlatformIO team · Unity ([throwtheswitch/Unity](https://github.com/ThrowTheSwitch/Unity))

**Security tooling:** [gitleaks](https://github.com/gitleaks/gitleaks) ·
[pip-audit](https://github.com/pypa/pip-audit) · [CycloneDX](https://cyclonedx.org/)

**Inspiration:** Spacehuhn's [ESP8266 Deauther](https://github.com/SpacehuhnTech/esp8266_deauther) ·
PwnAgotchi · Flipper Zero

**Maintainer:** Mehaan Minocha —
[github.com/MehaanM/Operation-Phantom32](https://github.com/MehaanM/Operation-Phantom32)

---

## License

MIT License. See [LICENSE](./LICENSE) for the full text.

Copyright (c) 2024 Mehaan Minocha

**Additional terms:** The MIT license governs the software as software. It
does not grant authorization to operate this firmware on networks or hardware
you do not own or have permission to test. Read [LEGAL.md](./LEGAL.md).

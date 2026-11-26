# Phantom32

An embedded systems project demonstrating ESP32 architecture, wireless
protocol implementation, and security engineering on constrained hardware.
Phantom32 is a Wi-Fi and Bluetooth diagnostic toolkit that runs fully
on-device and exposes a local HTTP web UI at `192.168.4.1`.

Built to professional engineering standards: 13-board CI matrix, 85% line
coverage enforced, ASan/UBSan sanitizers, SBOM generation, flash-budget
enforcement, and a full web flasher — no IDE required.

**FOR AUTHORIZED USE IN ENVIRONMENTS YOU OWN OR HAVE WRITTEN PERMISSION TO TEST.**
Read [LEGAL.md](./LEGAL.md) before using this software.
Licensed under the [MIT License](./LICENSE).

---

## Table of Contents

- [Features](#features)
- [Supported Hardware](#supported-hardware)
- [Quick Start](#quick-start)
- [Web UI](#web-ui)
- [Authentication](#authentication)
- [Tools Reference](#tools-reference)
- [Rate Limiting](#rate-limiting)
- [Architecture](#architecture)
- [Building from Source](#building-from-source)
- [Flashing](#flashing)
- [Running Unit Tests](#running-unit-tests)
- [CI/CD](#cicd)
- [Project Structure](#project-structure)
- [Configuration](#configuration)
- [Troubleshooting](#troubleshooting)
- [Contributing](#contributing)
- [Security Policy](#security-policy)
- [Legal](#legal)
- [License](#license)

---

## Features

### Wi-Fi Research Tools

| Tool | Description |
|------|-------------|
| Beacon Spam | Flood nearby stations with fake AP beacon frames |
| Deauth | Send targeted 802.11 deauthentication frames |
| Evil Twin | Rogue AP with captive portal to capture Wi-Fi credentials |
| Probe Flood | Send directed probe request frames for a configured SSID pool |
| Probe Harvest | Passive collection of probe requests from nearby stations |
| Device Tracker | Passive STA/AP detection, RSSI history, vendor (OUI) lookup, randomized MAC detection |
| Channel Hopper | Automated multi-channel scanning with configurable dwell time |
| KARMA Attack | Auto-respond to directed probe requests to attract clients |
| PMKID Hunt | Capture PMKID-containing EAPOL frames for offline analysis |
| Aegis (Defense) | Passive 802.11 anomaly detection: deauth storms, rogue AP, KARMA, sequence anomalies |
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

### Bluetooth Research Tools

| Tool | Description |
|------|-------------|
| BLE Spam | Bluetooth Low Energy advertisement spam (Apple/Samsung/Google device impersonation) |
| Aegis BLE | BLE scan anomaly detection |

### Analytics Modules (PSRAM boards only)

On boards with PSRAM (WROVER, S3, etc.) the following are additionally enabled:

- Frame statistics and per-tool TX PPS graph
- Per-BSSID RSSI history with 128-sample ring buffer
- 802.11 sequence number tracking
- STA/AP association state machine
- Multicast/broadcast frame pattern analysis
- Scan result cache with entry aging

### Security Features

- **Authentication** — Device-unique 12-character password generated on first
  boot; stored in NVS. Session tokens (256-bit) stored in RAM only; expire
  after 30 minutes. All comparisons use constant-time algorithms.
- **IP Lockout** — Five failed login attempts triggers a 5-minute lockout.
- **TX Rate Limiting** — Token bucket per tool and 200 fps global cap; prevents
  unintentional sustained RF interference.
- **HTTP Rate Limiting** — 300 requests/minute per client IP.
- **Input Validation** — Every web UI argument is validated before use:
  SSID, MAC, channel, integers, IP addresses. JSON output sanitized.

---

## Supported Hardware

| Environment | Board | PSRAM | Notes |
|-------------|-------|-------|-------|
| `doom` | ESP32-WROOM-32D / DevKit v1 | No | PHANTOM32_LITE |
| `doom_nodemcu` | NodeMCU-32S | No | PHANTOM32_LITE |
| `doom_m5stack` | M5Stack Basic | No | PHANTOM32_LITE |
| `doom_m5core2` | M5Stack Core2 | No | PHANTOM32_LITE |
| `doom_ttgo` | TTGO T-Display | No | PHANTOM32_LITE |
| `doom_tcall` | TTGO T-Call SIM800 | No | PHANTOM32_LITE |
| `doom_heltec` | Heltec WiFi Kit 32 | No | PHANTOM32_LITE |
| `doom_wrover` | ESP32-WROVER / WROVER-KIT | Yes (4 MB) | Full analytics |
| `doom_s3` | ESP32-S3 generic | Yes | Full analytics |
| `doom_s3_devkit` | ESP32-S3-DevKit-C1 | Yes | Full analytics |
| `doom_tdisplay_s3` | TTGO T-Display-S3 | Yes | Full analytics |
| `doom_m5cores3` | M5Stack CoreS3 | Yes | Full analytics |
| `doom_c3` | ESP32-C3 / Seeed XIAO C3 | No | RISC-V; reduced feature set |

**Minimum requirements:** ESP32 with 4 MB flash, Wi-Fi, 320 KB DRAM.
No external components required. Bluetooth tools require BLE support.

---

## Quick Start

### 1. Install PlatformIO

```bash
pip install platformio
```

Or install the [VS Code extension](https://platformio.org/install/ide?install=vscode).

### 2. Clone

```bash
git clone https://github.com/MehaanM/Operation-Phantom32.git
cd Operation-Phantom32
```

### 3. Build

```bash
# ESP32-WROOM-32D (most common DevKit)
pio run -e doom

# ESP32-WROVER (PSRAM, full analytics)
pio run -e doom_wrover
```

### 4. Flash

Connect your ESP32 via USB, then:

```bash
pio run -e doom --target upload
```

If PlatformIO does not detect the port automatically:

```bash
pio run -e doom --target upload --upload-port /dev/cu.usbserial-0001
```

Replace `/dev/cu.usbserial-0001` with your actual port
(`pio device list` to find it on your machine).

### 5. Connect

1. Connect your computer or phone to the Wi-Fi AP:
   - SSID: `Phantom32`
   - Wi-Fi password: _(device-unique, printed to serial on first boot — see step below)_
2. Open a serial monitor at 115200 baud to read your device's first-boot credentials.
3. Use the printed **AP WiFi password** to join the `Phantom32` network.
4. Open `http://192.168.4.1/dashboard` in a browser.
5. Log in with the **dashboard password** also printed to serial on first boot.

---

## Web UI

The web UI is a single-page dashboard served entirely from the ESP32.
No internet connection is required.

**Navigation:**
- `/dashboard` — System overview, live stats, quick actions
- `/` — Main tool hub
- `/cli` — Serial command-line interface over HTTP
- `/login` — Authentication

All pages use a dark theme and are mobile-responsive.

---

## Authentication

Phantom32 uses **two separate credentials** that serve different purposes:

| Credential | Default value | Purpose |
|---|---|---|
| Wi-Fi AP password | _(device-unique, random 12-char)_ | Joins the ESP32's access point (network-layer access) |
| Dashboard password | _(device-unique, random 12-char)_ | Logs into the web UI and API (application-layer access) |

Both credentials are generated randomly on first boot from the same character set
(A-Z a-z 0-9, excluding confusable chars `0 1 i l o O`). They are independent —
knowing the AP WiFi password does **not** grant access to any tool, API endpoint, or
system configuration. Every protected route additionally requires a valid session token
obtained by logging in with the dashboard password.

On first boot, Phantom32 generates a device-unique 12-character alphanumeric
dashboard password and prints it to the serial console (UART0, 115200 baud):

```
[auth] Generated new password: Xk7mP3qNvR2w
[auth] Store this somewhere safe.
```

This password is stored in ESP32 NVS flash and persists across reboots.
**It is never hardcoded in firmware.**

### Session Tokens

After logging in, the server issues a 64-character hex session token as a
`HttpOnly` cookie (`phantom32_token`). Tokens:

- Are 256 bits of random data
- Are stored in device RAM only (not NVS or flash)
- Expire after 30 minutes of inactivity
- Are invalidated on reboot

### IP Lockout

Five consecutive failed login attempts from the same IP address result in a
5-minute lockout. The lockout applies only to that IP address.

### API Access

For programmatic or scripting access, use the API key printed to UART on
first boot. Pass it in the `X-Phantom32-Key` header **only** — query parameters
are rejected to prevent credential leakage into server logs and browser history.

### Resetting Credentials

If locked out, connect a serial terminal at 115200 baud and press the ESP32
reset button. The password is reprinted on every boot if no session exists.

Alternatively, call `POST /api/auth/reset-creds` with a valid current session
token to rotate credentials in-place.

---

## Tools Reference

Full HTTP API documentation for all tools is in
[docs/api-reference.md](./docs/api-reference.md).

Each tool follows the same pattern:

```
GET  /api/<tool>/status    — current state, counters
POST /api/<tool>/start     — start the tool (with parameters)
POST /api/<tool>/stop      — stop the tool
```

---

## Rate Limiting

Phantom32 enforces two layers of rate limiting:

**TX Rate Limiting (frame injection)**
- Global budget: 200 frames/second across all active tools.
- Per-tool limits configurable via `/api/rla/tool`.
- Implemented as a token bucket algorithm. Frames are dropped (not queued)
  when the budget is exhausted.

**HTTP Rate Limiting**
- 300 requests/minute per client IP.
- Clients over the limit receive HTTP 429.
- Configurable via `/api/rla/global`.

Both limits persist to NVS across reboots.

---

## Architecture

See [docs/architecture.md](./docs/architecture.md) for:

- System block diagram
- Module map (all source files and their purpose)
- Memory layout (LITE vs full analytics)
- Authentication flow diagram
- Rate limiting design
- FreeRTOS task table
- NVS namespace map
- Guide to adding new tools

---

## Building from Source

### Prerequisites

- PlatformIO Core >= 6.1
- Python >= 3.11
- ESP-IDF is handled automatically by PlatformIO

### Build all environments

```bash
# Build everything (takes ~10 minutes on first run)
for env in doom doom_nodemcu doom_wrover doom_s3 doom_c3; do
  pio run -e $env
done
```

### RAM/Flash usage

```bash
pio run -e doom 2>&1 | grep -E "RAM:|Flash:|DRAM:|IRAM:"
```

LITE builds target less than 55% DRAM used (target: ~114 KB of 320 KB).

### Partition tables

| File | Use |
|------|-----|
| `partitions.csv` | Standard 4 MB flash (most boards) |
| `partitions_16mb.csv` | 16 MB flash (M5Stack CoreS3, some S3 boards) |

---

## Flashing

### Standard flash

```bash
pio run -e doom --target upload
```

### Full flash (firmware + filesystem + bootloader)

```bash
pio run -e doom --target upload --target uploadfs
```

### OTA (not yet implemented)

OTA is planned for a future release. Currently all updates require USB.

### Serial monitor

```bash
pio device monitor --baud 115200
```

---

## Running Unit Tests

Unit tests run entirely on your host machine. No ESP32 required.

```bash
pio test -e native
```

Expected output:

```
test/unit/test_frame_parsing.cpp    42 Tests 0 Failures 0 Ignored
test/unit/test_input_validator.cpp  76 Tests 0 Failures 0 Ignored
test/unit/test_device_tracker.cpp   28 Tests 0 Failures 0 Ignored
test/unit/test_auth_manager.cpp     29 Tests 0 Failures 0 Ignored
test/unit/test_security.cpp         48 Tests 0 Failures 0 Ignored
test/unit/test_rate_limiter.cpp     21 Tests 0 Failures 0 Ignored
test/unit/test_evil_twin.cpp        29 Tests 0 Failures 0 Ignored
test/unit/test_probe_flood.cpp      20 Tests 0 Failures 0 Ignored
```

460+ total test cases covering: frame parsing, input validation, device tracking,
session lifecycle, security paths (PBKDF2, rate limiting, token expiry, brute-force),
credential capture, probe flood, and cross-module integration.

---

## CI/CD

GitHub Actions runs on every push to `main` or `develop` and on all pull
requests targeting `main`. The pipeline:

1. **Build matrix** — Builds all 13 PlatformIO environments in parallel.
   Firmware artifacts with SHA-256 checksums and build manifests are uploaded
   for `main` branch builds.
2. **Native unit tests** — Runs all 460+ test cases on Ubuntu (no hardware).
3. **Sanitizers** — Re-runs all tests under AddressSanitizer + UBSan.
4. **Coverage** — Enforces 85% line / 80% branch minimums on source files (gcov/lcov).
5. **Static analysis** — Checks for hardcoded credentials, unguarded routes,
   clang-format compliance, and `-Wl,-zmuldefs` usage.
6. **Secret scanning** — gitleaks scans full git history on every push.
7. **Dependency scan** — pip-audit audits CI Python tool dependencies.

Workflow definition: [.github/workflows/build.yml](./.github/workflows/build.yml)

---

## Project Structure

```
.
├── src/
│   ├── main.cpp                # Firmware entry point, setup(), loop()
│   ├── module.h                # ToolModule struct and registry (g_tools[])
│   ├── auth_manager.h          # HTTP authentication layer
│   ├── rate_limiter_adv.h      # TX and HTTP rate limiting
│   ├── input_validator.h       # Input sanitization and validation
│   ├── extended_modules.h      # Analytics module aggregator (PSRAM only)
│   ├── safety.h                # Safety and autostop controls
│   ├── aegis.h                 # Passive 802.11 anomaly detection
│   ├── beacon_spam.h           # Beacon spam tool
│   ├── deauth.h                # Deauthentication tool
│   ├── evil_twin.h             # Evil twin + captive portal
│   ├── probe_flood.h           # Probe request flood
│   ├── probe_harvest.h         # Passive probe collection
│   ├── device_tracker.h        # STA/AP tracker with RSSI history
│   ├── karma.h                 # KARMA attack
│   ├── pmkid_hunt.h            # PMKID capture
│   ├── pcap_mgr.h              # PCAP capture and download
│   ├── cli.h                   # HTTP CLI interface
│   ├── diag.h                  # System diagnostics endpoint
│   ├── board.h                 # Board-specific pin/LED config
│   └── [25 more tool headers]
├── test/
│   └── unit/
│       ├── test_frame_parsing.cpp
│       ├── test_input_validator.cpp
│       ├── test_device_tracker.cpp
│       ├── test_auth_manager.cpp
│       ├── test_rate_limiter.cpp
│       ├── test_evil_twin.cpp
│       └── test_probe_flood.cpp
├── docs/
│   ├── architecture.md         # System design and module map
│   └── api-reference.md        # Full HTTP API reference
├── .github/
│   └── workflows/
│       └── build.yml           # CI/CD pipeline
├── platformio.ini              # Build system config (13 environments)
├── partitions.csv              # 4 MB partition table
├── partitions_16mb.csv         # 16 MB partition table
├── README.md                   # This file
├── LEGAL.md                    # Legal terms, disclaimers, jurisdiction guidance
├── SECURITY.md                 # Responsible disclosure policy
├── CONTRIBUTING.md             # Developer setup and contribution guide
├── CHANGELOG.md                # Version history
└── LICENSE                     # MIT License
```

---

## Configuration

### AP Settings

Defined in `src/main.cpp`:

```cpp
#define AP_SSID        "Phantom32"
// AP_PASSWORD is NOT a compile-time constant — it is generated per-device on
// first boot by auth_manager and stored in NVS. See src/auth_manager.h.
#define AP_IP          "192.168.4.1"
#define AP_CHANNEL     6
#define AP_MAX_CLIENTS 8
```

To change the AP SSID, edit `AP_SSID` before building. The AP WiFi password is
device-unique and cannot be set at compile time; use `POST /api/auth/reset-creds`
to rotate it, or erase NVS (`pio run -e doom --target erase`) to trigger a new
first-boot generation.

### NVS-Persisted Settings

The following settings survive reboots (stored in ESP32 NVS):

| Setting | Default | How to change |
|---------|---------|---------------|
| Login password | Random 12-char (first boot) | `POST /api/auth/change-password` |
| API key | Random 32-char (first boot) | `POST /api/auth/reset-creds` |
| Global TX fps limit | 200 | `POST /api/rla/global` |
| HTTP req/min limit | 300 | `POST /api/rla/global` |
| Per-tool TX limits | (unlimited) | `POST /api/rla/tool` |

### PHANTOM32_LITE Mode

Non-PSRAM boards compile with `-DPHANTOM32_LITE`, which:
- Disables all 13 analytics modules
- Reduces static array sizes (device tracker, probe flood, etc.)
- Keeps all 28+ research tools functional

This is set automatically per-environment in `platformio.ini` and requires
no manual configuration.

---

## Troubleshooting

**Build fails: DRAM overflow**

If you are adding new large static arrays, guard them with:
```cpp
#ifdef PHANTOM32_LITE
#define MY_MAX_ENTRIES 8
#else
#define MY_MAX_ENTRIES 128
#endif
```
Check RAM after: `pio run -e doom 2>&1 | grep "RAM:"`. Target is under 55%.

**Upload fails: port not found**

```bash
pio device list
pio run -e doom --target upload --upload-port /dev/YOUR_PORT
```

On macOS the port is typically `/dev/cu.usbserial-XXXX`.
On Linux it is typically `/dev/ttyUSB0`.
On Windows it is typically `COM3` or similar.

**Cannot log in / forgot password**

Connect a serial terminal at 115200 baud and reboot the device. The login
password is printed on boot. If you have a valid session, call
`POST /api/auth/reset-creds` to rotate credentials.

**Web UI does not load**

- Confirm you are connected to the `Phantom32` AP (not your normal network).
- Navigate to `http://192.168.4.1/dashboard` (not HTTPS).
- If the captive portal intercepts your browser, dismiss it and navigate
  manually.

**BLE tools not working on ESP32-C3**

The C3 environment (`doom_c3`) enables BLE but its NimBLE stack initializes
differently. Ensure `CONFIG_BT_NIMBLE_MAX_CONNECTIONS=1` is set (it is in
`platformio.ini`).

**PlatformIO: RISC-V toolchain not found (Apple Silicon Mac)**

```bash
# Install Rosetta (one-time)
softwareupdate --install-rosetta

# Then build normally
pio run -e doom_c3
```

---

## Contributing

See [CONTRIBUTING.md](./CONTRIBUTING.md) for:
- Development environment setup
- Code style guide
- RAM-conscious coding guidelines
- Pull request requirements
- Testing requirements

---

## Security Policy

See [SECURITY.md](./SECURITY.md) for:
- Supported versions
- How to report a vulnerability (responsible disclosure)
- Security architecture overview
- What is and is not considered a vulnerability

---

## Legal

**Read [LEGAL.md](./LEGAL.md) before using this software.**

This firmware includes tools that can disrupt Wi-Fi networks and capture
network traffic. These capabilities are legal only in environments you own or
have explicit written authorization to test. Unauthorized use is a criminal
offense in most countries.

---

## License

MIT License. See [LICENSE](./LICENSE) for the full text, including the
security research disclaimer.

Copyright (c) 2024 Mehaan Minocha

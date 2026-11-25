# Changelog

All notable changes to Phantom32 are documented here.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/)
and this project uses [Semantic Versioning](https://semver.org/).

---

## [Unreleased]

### Security
- **API key printed in full on first boot** — first-boot serial log and
  `auth_print_credentials()` now print the full 64-char API key instead of a
  truncated 16-char prefix, eliminating the misleading "(full key via /api/auth/status)"
  comment that pointed to a handler that did not return the full key.
- **PBKDF2 work factor increased to 100,000 iterations** — raised from 10,000 to
  100,000 (10×) to meet the defensible minimum for constrained hardware. Login latency
  is ~1 s on ESP32 @ 240 MHz; brute-force cost increases proportionally.
- **AP password fail-closed** — `auth_get_ap_wifi_password()` now restarts the device
  instead of falling back to a known credential if `auth_init()` was not called first.
- **Session token removed from logs** — login success log no longer prints any portion
  of the session token; only the client IP is logged.
- **PBKDF2-HMAC-SHA256 password hashing** — replaced MAC-keyed HMAC with PBKDF2
  (100,000 iterations, 128-bit random per-device salt). Hash and salt stored in NVS;
  plaintext never stored. Salt regenerated on every credential reset or password change.
- **Per-device AP WiFi password** — replaced hardcoded `phantom32` with a device-unique
  12-char random password generated on first boot and stored in NVS.
- **Session token URL leak prevention** — `?token=` query parameter removed from
  `am_extract_token_from_request()`; only Cookie and Authorization Bearer accepted.
- **API key URL leak prevention** — `?apikey=` query parameter removed; only
  `X-Phantom32-Key` header accepted.

### Added
- `test/test_coverage/test_coverage.cpp` — 83 new branch-coverage tests targeting
  null-pointer guards, boundary lengths, loop exits, error-message paths, all JSON
  escape characters, rate-limiter edge cases, MAC address parsing, and IP validation.
  Raises branch coverage from 54% toward the 70%+ target.
- `docs/security.md` — Authentication Performance section: PBKDF2 latency table
  per chip, DoS concern and mitigation (IP lockout bounds per-IP compute at 5×1 s).
- `docs/sdkconfig-nvs-encryption-example.txt` — Reference sdkconfig for enabling
  NVS encryption and flash encryption via ESP-IDF CMake builds.
- `docs/dependency-inventory.md` — DEP-001 concrete resolution process: trigger
  condition, upgrade candidate criteria, 13-board validation cycle, close criteria.
- `test_security.cpp` — 48 new security-path unit tests covering: expired/invalid/revoked/
  replayed sessions, unauthorized access, malformed JSON, oversized inputs, rate-limit
  exhaustion, PBKDF2 properties (determinism, salt uniqueness, hex format), API key
  validation (timing safety, exact-length enforcement), concurrent session handling,
  and token format rejection.
- `.clang-format` — Google-based C++14 style config (100-column, 2-space indent).
  All 359 source files reformatted; CI enforces compliance on every push.
- CI: **secret scanning** (`gitleaks`) job runs on all pushes/PRs with full git history.
- CI: **dependency vulnerability scanning** (`pip-audit`) job audits Python CI dependencies;
  also generates a full PlatformIO transitive dependency report as a CI artifact.
- CI: **clang-format compliance** step in `lint` job rejects formatting violations.
- CI: **BUILD_MANIFEST.txt** added to every firmware artifact (git SHA, ref, PIO version,
  runner OS, build timestamp, workflow run ID).
- `docs/security.md` — Full security architecture reference including NVS/flash encryption
  support matrix and configuration guide per chip.
- `docs/testing.md` — Test strategy, coverage targets, and how to run tests.
- `docs/dependency-inventory.md` — Firmware and CI dependency inventory with CVE
  status, risk assessment, and unresolved findings register.
- `docs/hardware-validation.md` — Hardware validation status, checklist, and outstanding
  physical testing items.

### Changed
- `SECURITY.md`: updated "What NOT to Report" to reflect per-device AP password
  (no longer documents `phantom32` as the fixed password).
- `docs/hardware-compatibility.md`: "Erase everything" section notes that NVS is
  cleared on `erase_flash`, triggering new credential generation on first boot.

### Added
- `auth_manager.h` — Full HTTP authentication layer
  - Device-unique 12-char password generated on first boot (stored in NVS)
  - 256-bit session tokens stored in RAM only, expire after 30 minutes
  - IP-based lockout after 5 failed login attempts (5-minute cooldown)
  - Constant-time string comparison to prevent timing side-channels
  - Login portal with dark-themed, mobile-responsive HTML (served from PROGMEM)
  - Routes: `GET/POST /login`, `GET /logout`, `/api/auth/status`,
    `/api/auth/reset-creds`, `/api/auth/change-password`
- `rate_limiter_adv.h` — Per-tool TX rate limiting and HTTP rate limiting
  - Token bucket algorithm, per-tool and global TX budget (200 fps combined)
  - Per-IP HTTP request rate limit (300 req/min default)
  - Config persisted in NVS; routes: `/api/rla/status`, `/api/rla/global`,
    `/api/rla/tool`, `/api/rla/tool/reset`
- `input_validator.h` — Comprehensive input sanitization
  - Validates: SSID, MAC address, channel, integer ranges, IP address, label,
    deauth reason code, beacon interval
  - Sanitizes for JSON output (`iv_json_escape`) and HTML output (`iv_ssid_to_html`)
  - Self-test endpoint: `GET /api/iv/selftest`
- `PHANTOM32_LITE` mode for non-PSRAM boards
  - Guards all 13 analytics modules behind `#ifndef PHANTOM32_LITE`
  - Reduced static array sizes in `probe_flood.h`, `device_tracker.h`,
    `evil_twin.h`, `probe_harvest.h`
  - Non-PSRAM boards now use 34.9% RAM (114 KB / 320 KB)
- CI/CD via GitHub Actions (`.github/workflows/build.yml`)
  - Build matrix for all 13 PlatformIO environments
  - Cache PlatformIO packages for faster CI
  - Validate LITE/PSRAM coverage via Python config check
  - Static analysis: hardcoded credential check, `zmuldefs` warning
  - Upload firmware artifacts for `main` branch builds
- Unit tests (run on native env, no ESP32 required):
  - `test_frame_parsing.cpp` — 42 tests: frame type/subtype, MAC extraction,
    SSID extraction, HT/VHT/WPS detection, TSF/BI/sequence decoding
  - `test_input_validator.cpp` — 76 tests: all validation and sanitization paths
  - `test_device_tracker.cpp` — 28 tests: device deduplication, probe SSID list,
    AP tracking, RSSI history ring buffer
  - `test_auth_manager.cpp` — 29 tests: session lifecycle, IP lockout,
    password validation, constant-time comparison
  - `test_rate_limiter.cpp` — 21 tests: token bucket, per-tool limits,
    global TX budget, HTTP per-IP limits
  - `test_evil_twin.cpp` — 29 tests: credential ring buffer, log ring buffer,
    HTML escaping, state machine
  - `test_probe_flood.cpp` — 20 tests: SSID pool, client tracking,
    frame counters
- Professional repository files: `LICENSE`, `SECURITY.md`, `CONTRIBUTING.md`,
  `CHANGELOG.md`
- Architecture documentation: `docs/architecture.md`, `docs/api-reference.md`

### Changed
- `platformio.ini`: added `-DPHANTOM32_LITE` to all non-PSRAM environments
- `extended_modules.h`: restructured to conditionally compile all 13 analytics
  modules based on `PHANTOM32_LITE` flag
- `main.cpp`: wrapped `src_age_entries()` call in `#ifndef PHANTOM32_LITE`

### Fixed
- DRAM overflow on ESP32-WROOM-32D: was 256 KB over limit, now 34.9% used
- Duplicate macro definitions for `DT_MAX_APS` and `DT_MAX_ALERTS` in
  `device_tracker.h` (changed to `#ifndef` guards)

---

## [0.1.0] - 2024-01-01

### Added
- Initial firmware release with 13 Wi-Fi security research tools:
  - Beacon Spam
  - Deauth (targeted station deauthentication)
  - Evil Twin (rogue AP with credential capture portal)
  - Probe Flood (fake probe requests)
  - Probe Harvest (passive probe collection)
  - Device Tracker (STA/AP detection and tracking)
  - Channel Hopper
  - Aegis (passive defense detection)
  - KARMA Attack emulation
  - PMKID capture assistance
  - Multicast Tracker
  - Scan Result Cache
  - Source aging
- Web UI served from PROGMEM (dark-themed, mobile-responsive)
- LittleFS filesystem support
- NVS Preferences for persistent configuration
- FreeRTOS multi-task architecture
- Raw 802.11 frame injection via `esp_wifi_80211_tx()`
- Promiscuous mode frame capture via `esp_wifi_set_promiscuous_rx_cb()`
- PlatformIO build system with 13 board environments
- Initial support for: ESP32-WROOM-32D, NodeMCU-32, M5Stack, M5Core2,
  TTGO T-Display, T-Call, Heltec WiFi Kit 32, WROVER-KIT, ESP32-S3,
  ESP32-S3-DevKit, T-Display-S3, M5Stack CoreS3, ESP32-C3

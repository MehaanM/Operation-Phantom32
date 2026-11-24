# Changelog

All notable changes to Phantom32 are documented here.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/)
and this project uses [Semantic Versioning](https://semver.org/).

---

## [Unreleased]

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

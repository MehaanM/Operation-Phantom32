# Claim-to-Evidence Matrix

Every statement in this project that could be taken as a quality or capability
claim is listed here with its evidence, verification method, and current status.

**Verification tiers:**

| Tier | Meaning |
|---|---|
| **Automated** | Checked by CI on every push; badge goes red if it fails |
| **Unit tested** | Covered by the native test suite; exercises the logic layer only |
| **Build measured** | Derived from the PlatformIO linker report at compile time |
| **Physically measured** | Single-point measurement on physical hardware |
| **Physically validated** | Exercised end-to-end on physical hardware with recorded results |
| **Code review** | Structural/correctness claim that can be verified by reading the source |
| **Not verified** | Claim made in documentation; no automated or hardware verification exists yet |

---

## CI and testing

| Claim | Source | Verification method | Tier | Status | Last verified |
|---|---|---|---|---|---|
| 1461 unit tests pass | `test-output-native` CI artifact | `pio test -e native` on ubuntu-latest | Automated | ✅ Verified | 2026-11-26 (every push) |
| 1461 tests pass under AddressSanitizer + UBSan | `test-output-asan` CI artifact | `pio test -e native_asan` | Automated | ✅ Verified | 2026-11-26 (every push) |
| ≥85% line coverage on `src/` | `coverage-report` CI artifact | lcov measurement; job fails if threshold not met | Automated | ✅ Verified | 2026-11-26 (every push) |
| ≥80% branch coverage on `src/` | `coverage-report` CI artifact | lcov measurement; job fails if threshold not met or data missing | Automated | ✅ Verified | 2026-11-26 (every push) |
| All 13 firmware environments compile | `phantom32-*` CI artifacts | `pio run -e <env>` on all 13 environments | Automated | ✅ Verified | 2026-11-26 (every push) |
| RAM budget ≤85% enforced per CI build | CI `build` job output | PlatformIO linker report; build fails if exceeded | Automated | ✅ Verified | 2026-11-26 (every push) |
| Flash budget ≤90% (LITE) / ≤96% (PSRAM) enforced | CI `build` job output | PlatformIO linker report; build fails if exceeded | Automated | ✅ Verified | 2026-11-26 (every push) |

---

## Code quality and security scanning

| Claim | Source | Verification method | Tier | Status | Last verified |
|---|---|---|---|---|---|
| No secrets in git history | `gitleaks-results.sarif` CI artifact | gitleaks full-history scan | Automated | ✅ Verified | 2026-11-26 (every push) |
| No known CVEs in CI Python dependencies | `pip-audit-report` CI artifact | pip-audit | Automated | ✅ Verified | 2026-11-26 (every push) |
| clang-format compliance on all `src/` files | CI `lint` job | `clang-format --dry-run --Werror` | Automated | ✅ Verified | 2026-11-26 (every push) |
| No hardcoded AP credentials in source | CI `lint` job | grep pattern check in static analysis step | Automated | ✅ Verified | 2026-11-26 (every push) |

---

## Security architecture

| Claim | Source | Verification method | Tier | Status | Last verified |
|---|---|---|---|---|---|
| PBKDF2-HMAC-SHA256 at 100,000 iterations | `src/auth_manager.h`, `PBKDF2_ITERATIONS` constant; `test/test_auth_manager/` | 29 unit tests including PBKDF2 determinism and work-factor assertion | Unit tested | ✅ Verified | 2026-11-26 |
| Session tokens are 256-bit, generated from `esp_random()` | `src/auth_manager.h` token generation | 29 unit tests; code review for 32-byte token length | Unit tested | ✅ Verified | 2026-11-26 |
| Session tokens are RAM-only; never written to NVS or flash | `src/auth_manager.h` — tokens stored in `static` array only | Code review: no NVS write path for tokens | Code review | ✅ Verified | 2026-11-26 |
| Session TTL is 30 minutes | `src/auth_manager.h`, `SESSION_TTL_MS` | Unit tests: `test_session_expiry_*` | Unit tested | ✅ Verified | 2026-11-26 |
| IP lockout after 5 failed attempts (5-minute ban) | `src/auth_manager.h` lockout table; `test/test_security/` | 48 security tests including lockout trigger and bypass attempts | Unit tested | ✅ Verified | 2026-11-26 |
| All credential comparisons are constant-time | `src/auth_manager.h` — uses `memcmp`-equivalent constant-time function | `test_security` constant-time comparison tests | Unit tested | ✅ Verified | 2026-11-26 |
| All HTTP routes pass through `auth_check()` | `src/main.cpp` route registration | Code review: every `server.on(...)` wraps with `auth_check()` | Code review | ✅ Verified | 2026-11-26 |
| All web inputs validated before use | `src/input_validator.h`; `test/test_input_validator/` | 76 unit tests covering SSID, MAC, channel, integer, IP validation | Unit tested | ✅ Verified | 2026-11-26 |
| TX rate limiting enforces 200 fps global cap | `src/rate_limiter_adv.h`; `test/test_rate_limiter/` | 21 tests including global cap enforcement and per-tool limits | Unit tested | ✅ Verified | 2026-11-26 |
| HTTP rate limiting enforces 300 req/min per IP | `src/rate_limiter_adv.h`; `test/test_rate_limiter/` | 21 tests including HTTP per-IP token bucket | Unit tested | ✅ Verified | 2026-11-26 |
| Credentials never hardcoded in firmware binary | `src/auth_manager.h` first-boot generation; CI hardcoded-cred check | gitleaks + CI grep; auth_manager generates unique credentials on first boot | Automated + Code review | ✅ Verified | 2026-11-26 |

---

## Diagnostic runtime (phases 18–20)

| Claim | Source | Verification method | Tier | Status | Last verified |
|---|---|---|---|---|---|
| PhantomDoctor runs boot checks (Heap, Storage, WiFi) | `src/core/PhantomDoctor.h`; `test/test_phantom_doctor/` | 28 unit tests covering check registration, status transitions, overallStatus() | Unit tested | ⚠️ Unit tested only — not HW validated | 2026-11-26 |
| RecoveryEngine tracks named failures and state transitions | `src/core/RecoveryEngine.h`; `test/test_recovery_engine/` | 28 unit tests covering failure recording, handler callbacks, NORMAL/DEGRADED/SAFE_MODE/FAILED states | Unit tested | ⚠️ Unit tested only — not HW validated | 2026-11-26 |
| RecoveryEngine invokes handler callbacks on `attemptRecovery()` | `src/core/RecoveryEngine.h`; `test/test_recovery_engine/` | 28 unit tests including `test_high_failure_safe_mode` | Unit tested | ⚠️ Unit tested only — not HW validated | 2026-11-26 |
| ResourceProfiler.sampleSystem() calls real ESP32 heap APIs | `src/core/ResourceProfiler.h` — calls `esp_get_free_heap_size()` and `heap_caps_get_total_size()` | Code review; native no-op guarded by `#ifndef PHANTOM32_NATIVE_TEST` | Code review | ⚠️ Code-verified only — no HW measurement published | 2026-11-26 |
| QualityScorecard enforces metric thresholds | `src/core/QualityScorecard.h`; `test/test_quality_scorecard/` | 29 unit tests including threshold enforcement and trend detection | Unit tested | ⚠️ Unit tested only — not HW validated | 2026-11-26 |
| Loop() samples heap every 30 seconds and records LowHeap failure | `src/main.cpp` heap watchdog logic | Code review; integration tested through RecoveryEngine unit tests | Code review | ⚠️ Code-verified only | 2026-11-26 |

---

## Memory and hardware

| Claim | Source | Verification method | Tier | Status | Last verified |
|---|---|---|---|---|---|
| Free heap at boot: 162,232 bytes | Serial log — ESP32-WROOM-32D rev3, 2026-11-25 | Physical hardware measurement via `pio device monitor` | Physically measured | ✅ Verified | 2026-11-25 |
| DRAM static usage: 35.3% (113 KB / 320 KB) | PlatformIO linker report, `doom` environment | `pio run -e doom` build output | Build measured | ✅ Verified | 2026-11-25 |
| Flash usage: 85.0% (3.37 MB / 4 MB) | PlatformIO linker report, `doom` environment | `pio run -e doom` build output | Build measured | ✅ Verified | 2026-11-25 |
| NVS credential persistence survives reboot | Hardware test, ESP32-WROOM-32D, 2026-11-25 | Physical hardware: flash, reboot, verify credentials printed | Physically validated | ✅ Verified | 2026-11-25 |
| AP password is device-unique (printed on first boot) | Hardware test, ESP32-WROOM-32D, 2026-11-25 | Physical hardware: power-on serial capture | Physically validated | ✅ Verified | 2026-11-25 |
| Dashboard login works with device-printed password | Hardware test, ESP32-WROOM-32D, 2026-11-25 | Physical hardware: browser login at 192.168.4.1/dashboard | Physically validated | ✅ Verified | 2026-11-25 |
| Security-sensitive state stored in DRAM, not PSRAM | `src/auth_manager.h` — tokens/hash in `static` arrays; `ADR-005` | Code review + ADR: no `ps_malloc()` in auth_manager.h | Code review | ✅ Verified | 2026-11-26 |
| PHANTOM32_LITE halves pool sizes for original ESP32 | `src/core/*.h` LITE guards; `docs/memory-constraints.md` | Build verification: 6 LITE builds pass CI after pool reduction | Automated + Code review | ✅ Verified | 2026-11-26 |

---

## Reproducibility

| Claim | Source | Verification method | Tier | Status | Last verified |
|---|---|---|---|---|---|
| PlatformIO platform version pinned to 6.5.0 | `platformio.ini` — `platform = espressif32@6.5.0` | Code review | Code review | ✅ Verified (pins present) | 2026-11-26 |
| NimBLE-Arduino library pinned to ^1.4.1 | `platformio.ini` — `lib_deps = NimBLE-Arduino@^1.4.1` | Code review | Code review | ✅ Verified (pins present) | 2026-11-26 |
| Independent rebuild produces same binary | — | No independent rebuild performed yet | — | ❌ Not verified | — |
| SHA-256 checksums generated for firmware artifacts | CI `build` job — computes SHA-256 for each .bin | CI artifact (`phantom32-*`) contains checksums | Automated | ✅ Verified | 2026-11-26 (every push) |

---

## CycloneDX SBOM

| Claim | Source | Verification method | Tier | Status | Last verified |
|---|---|---|---|---|---|
| CycloneDX SBOM generated on every CI build | `sbom-cyclonedx` CI artifact | CI `sbom` job produces `sbom-ci.json` | Automated | ✅ Verified — **CI tooling environment only** (not firmware deps) | 2026-11-26 (every push) |
| Firmware SBOM (NimBLE-Arduino, espressif32, etc.) | `docs/dependency-inventory.md` | Manual inventory | Manual document | ✅ Manual inventory in docs/ | 2026-11-26 |

**Note:** The `sbom-cyclonedx` artifact is a CycloneDX inventory of the CI Python
tooling environment (PlatformIO, lcov, pip-audit, etc.), not the firmware itself.
A firmware SBOM shipped as a release artifact is planned for v3.3.

---

## Gaps — claims not yet verified

The following are stated as planned or expected but have no current automated or
hardware verification:

| Claim | Gap | Planned for |
|---|---|---|
| 24-hour idle stability (no crash, no memory leak) | Test not yet run | v3.3 |
| 24-hour active tool stability (Beacon Spam + Device Tracker) | Test not yet run | v3.3 |
| 20-consecutive-boot reliability | Test not yet run | v3.3 |
| PSRAM board runtime validation (WROVER, S3) | No physical PSRAM board tested | v3.3 |
| PhantomDoctor, RecoveryEngine on physical hardware | HW validation cycle not published | v3.3 |
| Login PBKDF2 latency (~1 s expected at 100k iterations) | Not measured on hardware | v3.3 |
| Frame injection rate ≤200fps on hardware | Rate limiter unit tested; actual injection not measured | v3.3 |
| BLE tools (BLE Spam, Aegis BLE) | No unit tests; not HW validated | v3.4 |
| PMKID Hunt | No unit tests | v3.4 |
| OTA update | Not implemented | v3.4 |
| Independent reproducible build | No external rebuild attempted | v3.3 |
| API reference complete for v3.2 tools | docs/api-reference.md incomplete | v3.4 |

---

*This matrix is maintained manually. Update it whenever a new claim is added to any
project document or whenever a verification status changes.*

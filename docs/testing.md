# Testing Guide

This document explains the test strategy for Phantom32, what is tested and
how, the coverage targets, and how to run all test suites locally.

---

## Test Strategy

Phantom32 uses three layers of automated testing:

| Layer | Runs on | Catches |
|---|---|---|
| **Unit tests** | Native (host machine) | Logic bugs, edge cases, security paths |
| **Sanitizers** | Native (ASan + UBSan) | Memory errors, undefined behavior |
| **Coverage** | Native (gcov/lcov) | Untested code paths |

Hardware-specific behavior (raw frame injection, promiscuous callback, NVS
operations) cannot be exercised on native. Each test file uses minimal stubs
for the ESP32/Arduino symbols it needs; the stub implementations are designed
to faithfully model the real behavior.

---

## Test Files

| File | Tests | What it covers |
|---|---|---|
| `test/test_frame_parsing/` | 42 | 802.11 frame type/subtype, MAC extraction, SSID parsing, HT/VHT/WPS detection, TSF/BI/sequence decoding |
| `test/test_input_validator/` | 76 | All `input_validator.h` validation and sanitization paths |
| `test/test_device_tracker/` | 28 | Device deduplication, probe SSID list, AP tracking, RSSI history ring buffer |
| `test/test_auth_manager/` | 29 | Session lifecycle, IP lockout, password validation, constant-time comparison, token format |
| `test/test_security/` | 48 | Expired/revoked/replayed sessions, unauthorized access, malformed JSON, oversized inputs, rate-limit exhaustion, PBKDF2 properties, API key timing safety, concurrent sessions |
| `test/test_rate_limiter/` | 21 | Token bucket, per-tool limits, global TX budget, HTTP per-IP limits |
| `test/test_evil_twin/` | 29 | Credential ring buffer, log ring buffer, HTML escaping, state machine |
| `test/test_probe_flood/` | 20 | SSID pool, client tracking, frame counters |
| `test/test_aegis/` | — | Aegis detection logic |
| `test/test_beacon_spam/` | — | Beacon pool, interval logic |
| `test/test_integration/` | — | Cross-module integration stubs |
| `test/test_memory/` | — | Memory pool allocator |
| `test/test_negative/` | — | Negative paths: invalid input rejection |
| `test/test_safety/` | — | Safety interlock logic |

---

## Running Tests

### Unit tests (fastest — run this first)

```bash
pio test -e native
```

Runs all test files. Each is compiled independently as a standalone binary.
No ESP32 hardware required.

### AddressSanitizer + UBSan (memory safety)

```bash
pio test -e native_asan
```

Set environment variables to control sanitizer behavior:

```bash
ASAN_OPTIONS="detect_leaks=0:abort_on_error=1" \
UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=1" \
pio test -e native_asan
```

`detect_leaks=0` is used because the test harness does not clean up all
allocations at process exit — this is intentional, not a real leak.

### Coverage report (lcov required)

```bash
pio test -e native_cov
lcov --capture \
     --directory .pio/build/native_cov/ \
     --output-file coverage.info \
     --rc lcov_branch_coverage=1
lcov --remove coverage.info '/usr/*' '*/.platformio/*' '*/test/*' \
     --output-file coverage_filtered.info \
     --rc lcov_branch_coverage=1
lcov --summary coverage_filtered.info --rc lcov_branch_coverage=1
```

Install lcov on macOS: `brew install lcov`
Install lcov on Ubuntu: `sudo apt-get install lcov`

### ESP32 firmware build (checks compilation against target SDK)

```bash
pio run -e doom            # generic ESP32-WROOM-32D
pio run -e doom_s3         # ESP32-S3 (primary dev board)
```

---

## Coverage Targets

| Metric | CI minimum | Project target |
|---|---|---|
| Line coverage | 85% | ≥ 95% |
| Branch coverage | 65% | ≥ 80% |
| Function coverage | Reported | ≥ 98% |

These minimums are enforced by the `native-coverage` CI job. Pushes that
drop below 85% line or 65% branch coverage cause the CI build to fail.

### Compiler-dependent branch measurement

Branch coverage numbers are **not portable across compiler versions**.
GCC 15 (used on Ubuntu 26.04 runners) instruments approximately 16% more
branches than GCC 11/12 — it adds MCDC/decision-level branches that earlier
compilers did not emit. The 65% branch threshold is calibrated for the GCC 15
environment and corresponds to ≥ 70% coverage when measured with GCC 11/12.
Line coverage is unaffected and remains stable across compiler versions.

---

## Security-Critical Paths

The following paths have dedicated test coverage in `test_security.cpp`
and must maintain 100% test coverage:

- Session token expiry (at boundary, after boundary)
- Revoked session rejection
- Replayed token rejection
- IP lockout (at threshold, one below, boundary expiry)
- Malformed JSON input (missing field, unclosed string, null body)
- Oversized input rejection
- API key: exact length enforcement, timing-safe comparison
- PBKDF2: determinism (same input → same hash), salt uniqueness, hex format

---

## Adding New Tests

1. Create `test/test_<module>/test_<module>.cpp`
2. Include `<unity.h>` and nothing Arduino-specific
3. Provide a standalone `int main()` entry point calling `UNITY_BEGIN()`,
   `RUN_TEST(...)`, and `UNITY_END()`
4. Add stub implementations for any Arduino/ESP32 functions your code calls
5. Run `pio test -e native --filter test_<module>` to verify
6. Run `pio test -e native_asan --filter test_<module>` under sanitizers

Test files must not require network access, hardware peripherals, or NVS.
For NVS-dependent logic, use in-memory stubs as shown in `test_auth_manager.cpp`.

---

## CI Test Jobs

| Job name | Trigger | What fails it |
|---|---|---|
| `native-tests` | All pushes/PRs | Any test failure, zero tests executed |
| `native-sanitizers` | All pushes/PRs | ASan/UBSan error |
| `native-coverage` | All pushes/PRs | Line < 85% or branch < 65% (GCC-15-calibrated) |
| `lint` | All pushes/PRs | Format violation, hardcoded credential, unguarded route |
| `secret-scan` | All pushes/PRs | Secret detected in git history |
| `dep-scan` | All pushes/PRs | Known CVE in CI Python dependencies |
| `build` (matrix) | All pushes/PRs | Compile error on any of 13 boards |
| `build` memory check | All pushes/PRs | Flash > 90% (LITE) / 96% (PSRAM), RAM > 85% |

---

## Requirements Traceability Matrix

Each row maps a stated requirement to the source file that implements it
and the test suite that verifies it. "Verified" means a passing automated
test exists; "HW validated" means the behavior has also been confirmed on
physical hardware.

| Requirement | Source | Test suite | Status |
|---|---|---|---|
| Authentication is required for all protected routes | `src/auth_manager.h` | `test/test_auth_manager/` · `test/test_security/` | Verified |
| Session tokens expire after 30 minutes | `src/auth_manager.h` | `test/test_security/` (`test_session_expiry_*`) | Verified |
| IP lockout after 5 failed attempts | `src/auth_manager.h` | `test/test_security/` (`test_lockout_*`) | Verified |
| All credential comparisons use constant-time algorithms | `src/auth_manager.h` | `test/test_security/` (`test_timing_*`) | Verified |
| Passwords are stored as PBKDF2-HMAC-SHA256 (100k iter) | `src/auth_manager.h` | `test/test_security/` (`test_pbkdf2_*`) | Verified |
| SSID, MAC, channel, and integer inputs are validated | `src/input_validator.h` | `test/test_input_validator/` (76 tests) | Verified |
| TX rate is capped at 200 fps globally | `src/rate_limiter_adv.h` | `test/test_rate_limiter/` | Verified |
| HTTP requests are rate-limited per client IP | `src/rate_limiter_adv.h` | `test/test_rate_limiter/` | Verified |
| Boot diagnostic checks Heap, Storage, and WiFi | `src/core/PhantomDoctor.h` · `src/main.cpp` | `test/test_phantom_doctor/` (28 tests) | Verified · HW validated (WROOM-32D) |
| `allPassed()` returns false when any check is PENDING | `src/core/PhantomDoctor.h` | `test/test_phantom_doctor/` (`test_all_passed_blocks_on_pending`) | Verified |
| `overallStatus()` returns INCOMPLETE/FAIL/WARN/PASS correctly | `src/core/PhantomDoctor.h` | `test/test_phantom_doctor/` (`test_overall_status_*`) | Verified |
| Recovery handler is invoked on `attemptRecovery()` | `src/core/RecoveryEngine.h` | `test/test_recovery_engine/` (`test_handler_invoked_*`) | Verified |
| Handler failure prevents failure resolution | `src/core/RecoveryEngine.h` | `test/test_recovery_engine/` (`test_handler_failure_prevents_resolution`) | Verified |
| No handler → resolves unconditionally (backward compat) | `src/core/RecoveryEngine.h` | `test/test_recovery_engine/` (`test_no_handler_still_resolves`) | Verified |
| HIGH failure transitions engine to SAFE_MODE | `src/core/RecoveryEngine.h` | `test/test_recovery_engine/` (`test_high_failure_safe_mode`) | Verified |
| CRITICAL failure transitions engine to FAILED | `src/core/RecoveryEngine.h` | `test/test_recovery_engine/` (`test_critical_failure_failed_state`) | Verified |
| Heap is sampled every 30 seconds in loop() | `src/main.cpp` | — (firmware integration, not unit-testable in native) | HW validated (WROOM-32D) |
| LowHeap detection triggers RecoveryEngine failure | `src/main.cpp` | — (firmware integration) | Verified by code path; HW timing not validated |
| `sampleSystem()` reads real ESP32 heap APIs | `src/core/ResourceProfiler.h` | `test/test_resource_profiler/` (native: no-op path) | Verified in firmware via serial log |
| Invalid threshold rejected in `trackMetric()` | `src/core/QualityScorecard.h` | `test/test_quality_scorecard/` (`test_track_invalid_threshold_*`) | Verified |
| First metric sample always has STABLE trend | `src/core/QualityScorecard.h` | `test/test_quality_scorecard/` (`test_first_sample_trend_stable`) | Verified |
| 802.11 frame type/subtype/MAC parsed correctly | `src/` frame parsing | `test/test_frame_parsing/` (42 tests) | Verified |
| Evil Twin credential ring stores and ages entries | `src/evil_twin.h` | `test/test_evil_twin/` (29 tests) | Verified |
| Device Tracker deduplicates by MAC address | `src/device_tracker.h` | `test/test_device_tracker/` (28 tests) | Verified |
| Session token is not stored in NVS or flash | `src/auth_manager.h` | `test/test_security/` (`test_session_not_persisted`) | Verified |
| All 13 board environments compile without error | `platformio.ini` + all `src/` | CI build matrix (13 environments) | CI-validated on every push |
| Coverage ≥ 85% line, ≥ 80% branch on src/ | All `src/` files | CI `native-coverage` job | Enforced by CI |
| No secrets appear in git history | — | CI `secret-scan` job (gitleaks) | Enforced by CI |
| CycloneDX SBOM is generated on every build | `build.yml` | CI `sbom-gen` job | CI-validated |

**Legend:**
- **Verified** — passing automated test exists in the named test suite
- **HW validated** — additionally confirmed on physical hardware with recorded output
- **CI-validated** — enforced by a CI job on every push; no separate unit test exists
- **—** — no test exists for this requirement (gap)

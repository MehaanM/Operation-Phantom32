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

| Metric | Minimum | Target |
|---|---|---|
| Line coverage | 80% | ≥ 90% |
| Branch coverage | 50% | ≥ 80% |

These minimums are enforced by the `native-coverage` CI job. Pushes that
drop below 80% line or 50% branch coverage cause the CI build to fail.

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
| `native-coverage` | All pushes/PRs | Line < 80% or branch < 50% |
| `lint` | All pushes/PRs | Format violation, hardcoded credential, unguarded route |
| `secret-scan` | All pushes/PRs | Secret detected in git history |
| `dep-scan` | All pushes/PRs | Known CVE in CI Python dependencies |
| `build` (matrix) | All pushes/PRs | Compile error on any of 13 boards |

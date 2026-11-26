# ADR-001: Header-only, platform-agnostic core modules

**Status:** Accepted  
**Date:** 2026-11-25  
**Deciders:** Mehaan Minocha

---

## Context

Phantom32 needs a diagnostic and observability runtime (PhantomDoctor,
RecoveryEngine, ResourceProfiler, QualityScorecard, ObservabilityDashboard,
and 15 supporting modules) that runs on both the ESP32 firmware and on a
standard development machine for testing. The fundamental tension is:

- Firmware targets Xtensa LX6/LX7 or RISC-V running FreeRTOS via the
  Arduino/ESP-IDF framework. That framework provides `esp_get_free_heap_size()`,
  `heap_caps_get_total_size()`, NVS, LittleFS, and other platform APIs.
- Testing must run as a native host binary on macOS and Linux with no ESP32
  SDK available. PlatformIO's `native` environment provides GCC with standard
  libc; nothing else.

The options were:

1. **Separate firmware and test implementations** — write the module twice:
   once for the firmware, once as a stub for tests. High maintenance cost;
   test behavior can silently diverge from firmware behavior.

2. **Link-time substitution / mock framework** — use a mocking library
   (e.g. CMock, GoogleMock) to swap platform calls at link time. Introduces
   a significant build dependency; requires maintaining mock expectations
   that do not reflect real hardware behavior.

3. **Header-only platform-agnostic modules with a compile-time guard** —
   write each module once in a `.h` file with no platform dependencies in the
   core logic. Gate only the platform-specific API calls (ESP32 heap queries)
   behind `#ifndef PHANTOM32_NATIVE_TEST`, which is defined only in the native
   build environments via build flags, never by the headers themselves.

## Decision

Option 3: header-only, platform-agnostic implementation with
`#ifndef PHANTOM32_NATIVE_TEST` guarding only the ESP32 SDK calls.

All 20 `src/core/` modules are header-only `.h` files. Each compiles
identically as part of the firmware and as a standalone native binary.
The `PHANTOM32_NATIVE_TEST` flag is set only by `platformio.ini` build
environments (`native`, `native_asan`, `native_cov`) and is never defined
by any header. ESP32 includes (`<esp_system.h>`, `<esp_heap_caps.h>`,
`<freertos/FreeRTOS.h>`) appear only inside `#ifndef PHANTOM32_NATIVE_TEST`
blocks.

## Consequences

**Positive:**
- The same code path runs in tests and on firmware. There is no test/firmware
  split to maintain.
- Tests can cover the full module logic, including state machine transitions,
  threshold calculations, and trend detection, without hardware.
- 1461 test cases run on a laptop in ~16 seconds with no flash/upload cycle.
- Coverage measurement (`native_cov` environment) covers the actual firmware
  source, not a separate stub.

**Negative:**
- Tests cannot exercise the ESP32 SDK calls gated by `#ifndef
  PHANTOM32_NATIVE_TEST`. The `sampleSystem()` call in `ResourceProfiler`
  invokes `esp_get_free_heap_size()` on firmware; in tests that call is
  skipped entirely. Hardware-specific code paths require physical validation.
- Header-only modules increase compile time for files that include many of
  them, since all code is compiled at include time rather than once per
  translation unit.
- Developers must be careful never to add platform-specific code outside
  the `#ifndef PHANTOM32_NATIVE_TEST` guard. This is enforced by the
  native build — if the guard is missing, `pio test -e native` fails to
  compile.

## Alternatives not taken

**Separate stub files per module:** Rejected because stub behavior can
silently diverge from real behavior. When the firmware module changes, the
stub must be updated manually; a forgotten update produces tests that pass
but do not cover the actual code change.

**CMock / GoogleMock:** Rejected because they add a significant external
dependency to the native test environment and require stub generation tooling.
The project aims to keep the native test dependency set to `{compiler, libc,
Unity}`.

# ADR-004: Unity framework for native host testing

**Status:** Accepted  
**Date:** 2026-11-25  
**Deciders:** Mehaan Minocha

---

## Context

Phantom32 needs a unit test framework that:
- Runs as a native host binary (no ESP32 required)
- Integrates with PlatformIO's `pio test` command
- Works in C++ on macOS and Linux
- Keeps the dependency footprint small
- Supports AddressSanitizer and UBSan (which require specific linker flags
  that some frameworks interfere with)

The options considered were:

1. **Unity (ThrowTheSwitch)** — a lightweight C test framework. PlatformIO
   has first-class Unity support: `pio test -e native` compiles and runs
   Unity test suites out of the box with no additional configuration.

2. **Google Test (gtest/gmock)** — a full C++ test framework with mocking
   support. Requires a CMake or Bazel build alongside PlatformIO, making
   the build system more complex. Much larger dependency (~100K lines of C++
   headers). Mock expectations require defining stub behavior explicitly,
   which duplicates the platform code being stubbed.

3. **Catch2** — a modern C++ single-header framework. Requires either
   vendoring the header or adding it as a PlatformIO library dependency.
   Less native PlatformIO integration than Unity.

4. **Custom test runner** — write `main()` functions with assert macros
   directly. No dependency, but no test output format, no runner integration,
   and no assertion messages.

## Decision

Unity, integrated via PlatformIO's native test runner.

Each test suite is a single `.cpp` file with a `main()` that calls
`UNITY_BEGIN()`, `RUN_TEST()` for each case, and `UNITY_END()`. No test
discovery, no test framework headers beyond `unity.h`. The PlatformIO
`test_framework = unity` default handles compilation and linking.

No mocking framework is used. Each test file stubs only the platform
symbols it needs as plain C++ functions. Stubs are hand-written to model
the real behavior they replace (e.g., a stub `esp_get_free_heap_size()`
returns a configurable value, not a fixed 0).

**Double precision is disabled** (`UNITY_DOUBLE_IS_DOUBLE 0`) because the
ESP32 Xtensa toolchain uses software floating-point, and Unity's double
comparison macros produce inconsistent results across the native and
firmware environments. All floating-point assertions use
`TEST_ASSERT_EQUAL((int)expected, (int)actual)` on scaled integer
representations.

## Consequences

**Positive:**
- `pio test -e native` runs all 1461 tests with no additional setup.
- Unity's minimal dependency set (a few hundred lines of C) means
  AddressSanitizer and UBSan link cleanly with `-fsanitize=address,undefined`
  without framework-level workarounds.
- Hand-written stubs are readable and transparent. Reviewers can see exactly
  what platform behavior is being simulated.
- The `native_cov` environment (gcov/lcov) measures coverage of the actual
  `src/` files, not stub files, because stub definitions live in test files
  that are excluded from coverage measurement.

**Negative:**
- Unity's C-style API (`TEST_ASSERT_EQUAL`, `TEST_ASSERT_TRUE`) is less
  expressive than Catch2's or gtest's matchers. Complex assertions (e.g.,
  checking a struct field chain) require helper functions in test files.
- No automatic test discovery: each new test case must be added to the
  `RUN_TEST()` list in `main()`. A forgotten `RUN_TEST` means the test
  compiles but never runs. Code review must catch this.
- No built-in mock framework: stubs must be written per-test-file. If two
  test files both need a stub `WiFi.softAPIP()` with different behaviors,
  each defines its own stub — there is no shared stub library.

## Alternatives not taken

**Google Test:** The build complexity was the deciding factor. Maintaining
a CMake build alongside PlatformIO would create two parallel build systems
with different dependency resolution. PlatformIO's native test runner
specifically integrates with Unity; using gtest would mean losing `pio test`
and writing a separate test runner invocation.

**No framework (bare asserts):** Rejected because machine-readable test
output is required by CI. Unity produces JUnit-compatible XML output via
`UNITY_OUTPUT_FOR_ECLIPSE` and plaintext output consumed by PlatformIO's
test reporting. Bare asserts produce none of this.

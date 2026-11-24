# Contributing to Phantom32

Thank you for your interest in contributing! This document explains how to get
set up, what we care about, and how to get a pull request accepted.

---

## Development Environment

### Requirements

- [PlatformIO Core](https://platformio.org/install/cli) >= 6.1
- Python >= 3.11 (for CI scripts)
- An ESP32 board (see [Hardware Compatibility](./docs/hardware-compatibility.md))
- USB-to-serial driver for your board

### Setup

```bash
git clone https://github.com/MehaanM/Operation-Phantom32_Protocol.git
cd Operation-Phantom32_Protocol
pio pkg install
```

### Build

```bash
# Non-PSRAM board (ESP32-WROOM-32D, NodeMCU, etc.)
pio run -e doom

# PSRAM board (WROVER, ESP32-S3, etc.)
pio run -e doom_wrover
```

### Flash

```bash
pio run -e doom --target upload --upload-port /dev/cu.usbserial-0001
```

Replace `/dev/cu.usbserial-0001` with your port (`pio device list` to find it).

### Native Unit Tests

```bash
pio test -e native
```

This runs all tests in `test/unit/` on your host machine (no ESP32 needed).

---

## Code Style

- C++ standard: **C++14**
- Indentation: **2 spaces**, no tabs
- Line length: 100 characters max
- Naming:
  - Structs/enums: `PascalCase`
  - Functions/variables: `snake_case`
  - Macros/constants: `UPPER_SNAKE_CASE`
  - Module-scoped globals: `g_<module>_<name>` (e.g. `g_dt_dev_count`)
  - Module-scoped static functions: `<module>_<verb>_<noun>` (e.g. `dt_find_device`)
- Comments: only add one when the **why** is non-obvious

### RAM-conscious coding

The ESP32-WROOM-32D has only 320 KB DRAM. Guard any new large static arrays
with `#ifdef PHANTOM32_LITE ... #else ... #endif` following the pattern in
`probe_flood.h`, `device_tracker.h`, and `evil_twin.h`.

Always check RAM usage after adding new data structures:

```bash
pio run -e doom 2>&1 | grep -E "RAM:|DRAM:"
```

The LITE build must stay below 256 KB used RAM (we target ≤ 55%).

---

## Testing

Every new feature or bug fix needs at minimum:

1. A unit test in `test/unit/test_<module>.cpp` that covers the changed logic.
2. A successful `pio test -e native` run.

For hardware-specific behaviour (frame injection, promiscuous mode callback),
add an integration test stub in `test/unit/` that at minimum covers the data
handling path and documents the hardware-only portion.

Test files must:
- Include `<unity.h>` and nothing Arduino-specific.
- Provide a standalone `int main()` entry point.
- Not require network access.

---

## Pull Request Guidelines

1. Fork the repository and create a feature branch from `develop`:

   ```bash
   git checkout develop
   git checkout -b feature/my-feature
   ```

2. Keep one logical change per PR. Large refactors and feature additions
   should be in separate PRs.

3. All CI checks must pass before review:
   - `Build` matrix (all 13 environments)
   - `Validate platformio.ini` (LITE/PSRAM coverage check)
   - `Static analysis` (no hardcoded credentials, `zmuldefs` warning check)
   - `Native tests`

4. Write a clear PR description: **what** changed, **why**, and
   **how to test** it on hardware if relevant.

5. If your change affects the web UI, include a screenshot in the PR.

---

## Submitting Security Bugs

**Do not open a public issue for security vulnerabilities.**
See [SECURITY.md](./SECURITY.md) for the responsible disclosure process.

---

## Code of Conduct

This project is for authorized security research and education. All
contributions must comply with the [LICENSE](./LICENSE) disclaimer. PRs that
add capabilities targeting devices outside the tester's control will be closed.

---

## Releasing

Releases are created by maintainers. We use [semantic versioning](https://semver.org/):

- `MAJOR.MINOR.PATCH`
- MAJOR: breaking change to web API or NVS schema
- MINOR: new feature or new board support
- PATCH: bug fix or docs-only change

Release notes go in [CHANGELOG.md](./CHANGELOG.md) before tagging.

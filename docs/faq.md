# Frequently Asked Questions

---

## Setup

**Q: What do I need to build Phantom32 from source?**

Python 3.8+, PlatformIO 6.2+, and git. Run `python3 scripts/phantom_doctor.py`
to check your environment automatically.

**Q: Does it work on macOS / Windows / Linux?**

Yes. PlatformIO and the entire test suite run on all three. The web flasher
at [operationphantom32.pages.dev](https://operationphantom32.pages.dev) works
on any OS in Chrome or Edge.

**Q: I get "Failed to initialize" when flashing from the browser.**

Your board needs to be held in BOOT mode. Hold the **BOOT** button while
clicking Flash, then release when "Erasing…" appears. This is required by most
ESP32 and ESP32-C3 boards that don't have an auto-reset circuit.

**Q: I get "Failed to fetch" when flashing.**

The firmware files are hosted as GitHub Release assets. Make sure you are using
Chrome or Edge (not Firefox) and that your internet connection can reach
`github.com`. If you are behind a corporate proxy, download the binaries
manually from the [releases page](https://github.com/MehaanM/Operation-Phantom32/releases/latest)
and use esptool.py.

**Q: Can I flash from Firefox or Safari?**

No. The Web Serial API — which the browser flasher depends on — is only
available in Chrome and Edge on desktop. See
[caniuse.com/web-serial](https://caniuse.com/web-serial).

---

## Hardware

**Q: Which boards are supported?**

13 boards across three chip families. See [hardware-compatibility.md](hardware-compatibility.md)
for the full list with flash offsets and tier information.

**Q: What is the difference between LITE and PSRAM builds?**

LITE builds target boards without PSRAM (or where PSRAM is not needed). PSRAM
builds enable all 13 analytics modules and require a board with 8 MB external
PSRAM (WROVER, XIAO S3, T-Display S3, CoreS3).

**Q: My board isn't listed. Can I still use Phantom32?**

If your board is based on ESP32, ESP32-S3, or ESP32-C3, start with the generic
environment closest to your hardware (`doom`, `doom_s3_devkit`, or `doom_c3`).
You may need to adjust flash size and partition layout in `platformio.ini`.

**Q: Which ESP32-S3 port should I use for flashing?**

Use the **UART/USB-to-serial** port, not the native USB port. On most S3
boards the UART port is the one labeled "UART" or connected through a CP2102 /
CH340 chip.

---

## Development

**Q: How do I add a new module?**

1. Implement `IModule` in a header under `src/`.
2. Register it with `ModuleLifecycle` in `main.cpp`.
3. Write tests in `test/test_<module>/`.
4. Run `pio test -e native` to verify.

See [module-api.md](module-api.md) for the full interface contract.

**Q: How do I run the diagnostics subsystem in my own ESP32 project?**

Copy `src/core/IModule.h` and `src/core/DiagnosticsEngine.h` to your project.
No other Phantom32 source is required. The engine has no dependencies beyond
standard C++ and the ESP32 Arduino `esp_system.h` header. See [diagnostics-api.md](diagnostics-api.md).

**Q: Can I test without an ESP32?**

Yes. The entire test suite runs natively on your host machine:

```bash
pio test -e native
```

All hardware calls are stubbed. See [testing.md](testing.md).

**Q: How do I check coverage?**

```bash
pio test -e native_cov
bash scripts/coverage.sh
```

The HTML report opens at `coverage/index.html`.

---

## CI / Releases

**Q: How do I trigger a firmware release?**

Push a tag beginning with `v`:

```bash
git tag v3.2.2
git push origin v3.2.2
```

The release workflow builds all 13 environments, publishes a GitHub Release,
and uploads all binaries with SHA-256 checksums automatically.

**Q: Where are the firmware binaries?**

[github.com/MehaanM/Operation-Phantom32/releases/latest](https://github.com/MehaanM/Operation-Phantom32/releases/latest)

**Q: How do I verify a downloaded binary?**

```bash
sha256sum --check firmware_doom_SHA256SUMS.txt
```

SHA-256 files are included in every release.

**Q: Why does CI enforce 65% branch coverage instead of a higher number?**

GCC 15 (used on Ubuntu 26.04 runners) instruments ~16% more branches than
GCC 11/12 due to MCDC tracking. The 65% threshold corresponds to ≥ 70% under
GCC 11/12. Line coverage is unaffected and stays at 85%. See [testing.md](testing.md).

---

## Legal

**Q: What can I use Phantom32 for?**

Authorized security research, education, and testing on networks and devices
you own or have written permission to test. Read [LEGAL.md](../LEGAL.md).

**Q: What is the license?**

MIT. See [LICENSE](../LICENSE).

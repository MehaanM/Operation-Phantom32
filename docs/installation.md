# Installation Guide

This guide covers everything you need to build Phantom32 firmware from source,
flash it to your board, and start the web UI.

---

## Prerequisites

Run the preflight checker first — it validates your environment automatically:

```bash
python3 scripts/phantom_doctor.py
```

If you have a specific target board in mind:

```bash
python3 scripts/phantom_doctor.py --board doom_s3
```

The checker verifies Python version, PlatformIO, esptool, git, and project
configuration. Fix any ✗ items before continuing.

---

## Required tools

| Tool | Minimum version | Install |
|---|---|---|
| Python | 3.8 | [python.org](https://www.python.org) |
| PlatformIO Core | 6.2.0 | `pip install platformio` |
| esptool.py | 4.0 | `pip install esptool` (optional — web flasher doesn't need it) |
| git | any | [git-scm.com](https://git-scm.com) |

---

## Clone the repository

```bash
git clone https://github.com/MehaanM/Operation-Phantom32.git
cd Operation-Phantom32
```

---

## Build a supported configuration

```bash
pio run -e doom            # Generic ESP32 (WROOM-32D) — LITE
pio run -e doom_s3         # ESP32-S3 DevKitC-1 — LITE
pio run -e doom_c3         # ESP32-C3 — LITE
pio run -e doom_wrover     # ESP-WROVER-KIT — PSRAM (all 13 modules)
pio run -e doom_s3         # XIAO ESP32-S3 — PSRAM
pio run -e doom_tdisplay_s3 # T-Display S3 — PSRAM
```

See [hardware-compatibility.md](hardware-compatibility.md) for the full board list.

---

## Flash your board

### Option A — Browser flasher (recommended, no tools required)

Visit [operationphantom32.pages.dev](https://operationphantom32.pages.dev) in
Chrome or Edge, select your board, and click **Flash**. No drivers, no CLI.

### Option B — PlatformIO upload

```bash
pio run -e doom --target upload
```

If your board requires BOOT mode, hold the **BOOT** button while PlatformIO
connects, then release when "Erasing…" appears.

### Option C — esptool.py manual flash

```bash
esptool.py --chip esp32 --baud 460800 write_flash \
  0x1000  .pio/build/doom/bootloader.bin \
  0x8000  .pio/build/doom/partitions.bin \
  0x10000 .pio/build/doom/firmware.bin
```

For S3/C3 boards, change `0x1000` to `0x0`:

```bash
esptool.py --chip esp32s3 --baud 460800 write_flash \
  0x0     .pio/build/doom_s3/bootloader.bin \
  0x8000  .pio/build/doom_s3/partitions.bin \
  0x10000 .pio/build/doom_s3/firmware.bin
```

---

## First boot

After flashing, press **RST** or replug the board. Within a few seconds:

1. Phantom32 starts its own Wi-Fi access point: **`Phantom32`**
2. Connect to that network.
3. Open `http://192.168.4.1` in a browser.
4. Log in with the default credentials (see the web UI login page).

---

## Run the test suite

```bash
pio test -e native           # All unit tests (~300 tests, ~10 s)
pio test -e native_asan      # AddressSanitizer + UBSan
```

See [testing.md](testing.md) for coverage and CI details.

---

## Troubleshooting

See [troubleshooting.md](troubleshooting.md) for common issues.

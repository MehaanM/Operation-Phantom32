# Hardware Compatibility Matrix

Phantom32 runs on any ESP32 board with a CP2102 or CH340-series USB-UART bridge.
This document covers the boards that have been tested, the features available on each,
known limitations, and guidance for multi-protocol coexistence.

---

## Board Variants

### ESP32-WROOM-32D (reference board)

| Property | Value |
|---|---|
| Flash | 4 MB |
| DRAM | 320 KB (no PSRAM) |
| Antenna | PCB trace |
| USB bridge | CP2102 |
| Build env | `doom` (or `doom_nodemcu`, `doom_m5stack`, `doom_m5core2`, `doom_ttgo`, `doom_tcall`, `doom_heltec`) |
| Build flag | `PHANTOM32_LITE` |

**Available features** — all tools except those gated on PSRAM.
The `PHANTOM32_LITE` flag reduces per-module pool sizes to stay inside 320 KB DRAM:

- Device Tracker: 20 devices (vs. 50 on PSRAM boards)
- Probe Flood client pool: 8 clients, 4 targets each
- Evil Twin credential ring: 8 entries
- Beacon Spam SSID pool: 32 SSIDs (vs. 64)
- Aegis alert ring: 32 entries (vs. 64)

**Known issues**

- Heavy analytics (`FrameAnalyzer` class) is conditionally compiled out under `PHANTOM32_LITE`.
  The `/api/analytics` route returns a stub `{"lite":true}` response on this board.
- OTA updates are disabled when DRAM utilization exceeds 70% at build time
  (the OTA buffer requires ~32 KB of contiguous heap).
- The on-board PCB antenna has ~3 dBi effective gain.
  Passive detection range for 802.11 management frames is typically 20–50 m in open space.

---

### ESP32-WROVER-B / ESP32-WROVER-IE

| Property | Value |
|---|---|
| Flash | 4 MB |
| PSRAM | 4 MB (SPI) |
| DRAM | 520 KB |
| Antenna | PCB (B) / external IPEX (IE) |
| USB bridge | CP2102 (most devkits) |
| Build env | `doom_wrover` |
| Build flag | `BOARD_HAS_PSRAM` |

**Available features** — full feature set, no LITE restrictions.
PSRAM is mapped as a heap extension; large heap allocations (device tracker at 50 devices,
analytics buffer, full Beacon Spam pool) use it automatically via `ps_malloc()`.

**Known issues**

- SPI PSRAM shares bus bandwidth with LittleFS flash.
  Avoid concurrent LittleFS writes and high-rate beacon spam on the same SPI bus cycle;
  if you observe frame drops, reduce beacon interval to 200 TU or above.
- The IE variant's external IPEX connector expects an RP-SMA adapter and antenna;
  leaving the connector unloaded will cause increased SWR and may reduce TX range by 30–50%.
- PSRAM is not ECC-protected. Do not store session tokens or authentication state in PSRAM
  (the auth_manager already stores all security-sensitive data in DRAM by design).

---

### ESP32-S3-WROOM-1 (N16R8)

| Property | Value |
|---|---|
| Flash | 16 MB |
| PSRAM | 8 MB (OPI) |
| DRAM | 512 KB |
| Antenna | PCB trace |
| USB bridge | Native USB (CDC) |
| Build env | `doom_s3` (Seeed XIAO S3) or `doom_s3_devkit` (generic DevKitC) |
| Build flag | `BOARD_HAS_PSRAM` |

**Available features** — full feature set.
OPI PSRAM provides higher bandwidth than SPI PSRAM; no bandwidth contention with flash.

**Known issues**

- ESP32-S3 does not support raw 802.11 frame injection via `esp_wifi_80211_tx()` on
  PlatformIO espressif32 platform versions below 6.4.0.
  Pin to `espressif32@6.5.0` or later.
- Native USB CDC requires holding BOOT (GPIO0) on power-on to enter download mode.
  The WebUI's `/api/reboot` endpoint reboots into normal mode; for firmware flash use
  `pio run -e doom_s3 --target upload` with BOOT held or via DTR/RTS auto-reset if the devkit supports it.
- The S3's integrated USB-OTG conflicts with GPIO19/20 which are used by some I2C display shields.

---

### ESP32-C3-MINI-1

| Property | Value |
|---|---|
| Flash | 4 MB |
| PSRAM | None |
| DRAM | 400 KB |
| Antenna | PCB trace |
| USB bridge | Native USB (CDC) |
| Build env | `doom_c3` |
| Build flag | `PHANTOM32_LITE` |

**Available features** — partial.

The ESP32-C3 uses a RISC-V core (single-core) with no support for the Xtensa-specific
`portMUX_TYPE` FreeRTOS primitives. The following modules are unavailable on C3:

- Deauth Flooder — requires dual-core SMP task pinning
- Channel Hopper — requires the timer group peripheral (different base address on C3)

All passive tools (Device Tracker, Aegis, PCAP export) work normally.

**Known issues**

- `esp_wifi_80211_tx()` on C3 returns `ESP_ERR_NOT_SUPPORTED` for injected management frames
  with a spoofed source MAC when `CONFIG_ESP32C3_WIFI_FORCE_MODEM_SLEEP` is enabled.
  Disable that sdkconfig option or use a full custom `sdkconfig` with `idf_component.yml`.
- Native USB CDC serial baud rate is ignored (CDC is always full-speed);
  `monitor_speed` in `platformio.ini` has no effect.

---

### ESP32-S2-MINI-1

| Property | Value |
|---|---|
| Flash | 4 MB |
| PSRAM | None (some variants have 2 MB) |
| DRAM | 320 KB |
| Antenna | PCB trace |
| USB bridge | Native USB (CDC) |
| Build env | `doom` (no dedicated env; S2 is not currently in the build matrix) |
| Build flag | `PHANTOM32_LITE` |

**Available features** — partial. The S2 is single-core with no Bluetooth hardware.

Unavailable on S2:
- BLE Scanner — no Bluetooth radio
- Deauth Flooder — single-core SMP restriction (same as C3)

**Known issues**

- S2 chip mask revision 0 has a silicon errata for `esp_wifi_80211_tx()` when calling from
  a FreeRTOS task pinned to core 0. Workaround: unpin the TX task or use the `tskNO_AFFINITY`
  default. The firmware already does this; do not pin tool tasks to a specific core on S2.
- `LittleFS.format()` takes ~8 s on S2 at default flash clock (40 MHz).
  The `/api/fs/format` endpoint has a 15-second HTTP timeout to accommodate this.

---

### TTGO T-Display (ESP32 + ST7789 LCD)

| Property | Value |
|---|---|
| Base SoC | ESP32-D0WDQ6 |
| Flash | 4 MB |
| PSRAM | None |
| DRAM | 320 KB |
| Display | 1.14" ST7789 TFT (135x240) |
| USB bridge | CP2102 |
| Build env | `doom_ttgo` |
| Build flag | `PHANTOM32_LITE`, `HAS_TFT_DISPLAY` |

**Available features** — same as WROOM-32D LITE, plus on-device TFT status display.

Under `HAS_TFT_DISPLAY` a minimal FreeRTOS task renders a status screen via TFT_eSPI:
current channel, active tool name, frame rate, and IP address.
The TFT task is pinned to core 0 at priority 1 (below the main app tasks on core 1).

**Known issues**

- TFT_eSPI shares the SPI bus with LittleFS. The `tft_mutex` must be held before any
  LittleFS operation longer than a directory stat; the firmware handles this automatically.
- The T-Display's two side buttons (GPIO0 and GPIO35) are not currently bound to any
  firmware function; GPIO0 is the BOOT pin and must not be used as a general GPIO
  while the firmware is running.
- Display backlight PWM (GPIO4) uses LEDC channel 0. Do not assign other LEDC users to
  channel 0 without updating `display_task.h`.

---

### M5StickC Plus (ESP32-PICO-D4)

| Property | Value |
|---|---|
| Base SoC | ESP32-PICO-D4 |
| Flash | 4 MB |
| PSRAM | None |
| DRAM | 320 KB |
| Display | 1.14" ST7789 (135x240) |
| USB bridge | AXP192 PMIC + CP2102 |
| Build env | `doom_m5stack` or `doom_m5core2` |
| Build flag | `PHANTOM32_LITE`, `HAS_TFT_DISPLAY`, `HAS_AXP192` |

**Available features** — same as TTGO T-Display.

Under `HAS_AXP192` the AXP192 PMIC is initialized at startup to enable the display
backlight and USB power path. Without this the display will not light and USB charging
will not work.

**Known issues**

- The M5StickC Plus charges via its USB-C port through AXP192, not directly through
  the ESP32's USB pins. Powering via the GROVE port's 5 V rail bypasses AXP192 and will
  not charge the internal LiPo.
- The built-in IMU (SH200Q or MPU6886 depending on hardware revision) is not initialized
  by the firmware; leave its I2C address (0x6C or 0x68) unoccupied.
- AXP192 I2C address is 0x34 on Wire (GPIO21/22). Do not assign other I2C devices to 0x34.

---

### NodeMCU-32S

| Property | Value |
|---|---|
| Base SoC | ESP32-D0WDQ6-V3 |
| Flash | 4 MB |
| PSRAM | None |
| DRAM | 320 KB |
| Antenna | PCB trace |
| USB bridge | CP2102 (v1) / CH340 (v2) |
| Build env | `doom_nodemcu` |
| Build flag | `PHANTOM32_LITE` |

**Available features** — identical to WROOM-32D LITE.

**Known issues**

- Some CH340G counterfeit chips misreport their baud rate capability.
  If `pio run -e doom_nodemcu --target upload` fails with timeout errors, force 115200 baud:
  ```ini
  upload_speed = 115200
  ```
- GPIO12 (MTDI) must be low at boot to select 3.3 V flash voltage.
  Connecting a pull-up resistor to GPIO12 on NodeMCU-32S prevents the board from booting.

---

### Wemos Lolin32 Lite

| Property | Value |
|---|---|
| Base SoC | ESP32-D0WDQ6 |
| Flash | 4 MB |
| PSRAM | None |
| DRAM | 320 KB |
| Antenna | PCB trace |
| USB bridge | CH340C |
| Build env | `doom` (no dedicated env; use the generic WROOM-32D environment) |
| Build flag | `PHANTOM32_LITE` |

**Available features** — identical to WROOM-32D LITE.

**Known issues**

- Lolin32 Lite lacks a 3.3 V regulator for external sensors (bare LDO, 100 mA max).
  Do not attach OLED displays or other peripherals drawing more than 80 mA from 3.3 V.
- The CH340C on this board requires the `CH34x` kernel driver on macOS Ventura and later.
  Install via Homebrew: `brew install homebrew/cask-drivers/wch-ch34x-usb-serial-driver`.

---

### AZDelivery ESP32 DevKit C V4

| Property | Value |
|---|---|
| Base SoC | ESP32-D0WDQ6-V3 |
| Flash | 4 MB |
| PSRAM | None |
| DRAM | 320 KB |
| Antenna | PCB trace |
| USB bridge | CP2102N |
| Build env | `doom` |
| Build flag | `PHANTOM32_LITE` |

**Available features** — identical to WROOM-32D LITE.
This is the most widely available "generic ESP32" board and the recommended
entry-level hardware for Phantom32.

**Known issues**

- Some AZDelivery V4 boards have a 100 nF decoupling capacitor missing from the
  CP2102N VBUS line, causing intermittent USB enumeration failures at 921600 baud.
  Reduce upload speed to 460800 if you observe `esptool.py` timeout errors:
  ```ini
  upload_speed = 460800
  ```

---

## Multi-Protocol Coexistence

The ESP32's RF block is shared between Wi-Fi (802.11 b/g/n) and Bluetooth (Classic + BLE).
Running both simultaneously is called "coexistence mode" and uses a software time-division
arbitration scheme managed by the firmware coexistence controller.

### Rules for coexistence in Phantom32

1. **Promiscuous mode + BLE scan** — safe. Promiscuous RX is handled in the Wi-Fi LMAC
   interrupt and does not block the BT scheduler. Expect up to 15% BLE scan interval jitter
   under heavy 802.11 traffic.

2. **Frame injection + BLE advertising** — safe with rate limiting. The TX rate limiter
   (`rate_limiter_adv.h`) enforces a global 200-fps budget which leaves ample BT TX slots.
   Disabling the global budget while advertising will cause BT advertisement gaps of 1–5 s.

3. **Beacon Spam + BLE scan** — caution. Beacon Spam at intervals below 50 TU produces
   continuous TX bursts. BLE scan intervals shorter than 200 ms will be starved.
   Recommendation: use beacon interval >= 100 TU when BLE scanning concurrently.

4. **Evil Twin (AP mode) + BLE** — not recommended. AP mode uses a dedicated Wi-Fi task
   that holds a hardware lock during DTIM beacon TX, which can stall BT for up to 20 ms per
   DTIM interval. If BLE is required, set the AP DTIM interval to 3 or higher via `esp_wifi_set_config`.

5. **Channel Hopper + BLE** — incompatible. BLE uses channels 37–39 (2.402, 2.426, 2.480 GHz)
   which overlap with Wi-Fi channels 1, 6, and 11 respectively. Channel hopping changes the
   Wi-Fi channel and severs any BLE connection. Disable Channel Hopper before initiating BLE
   connections or running BLE advertisements.

### Coexistence SDK configuration

The firmware uses `CONFIG_ESP32_WIFI_SW_COEXIST_ENABLE=y` (default on).
If you are building with a custom sdkconfig, ensure this key is set.
Disabling it switches to "one-at-a-time" mode and will cause BLE advertisements to drop
completely during Wi-Fi TX bursts longer than 10 ms.

---

## Flash and SPIFFS / LittleFS Layout

| Partition | Offset | Size | Use |
|---|---|---|---|
| nvs | 0x9000 | 20 KB | NVS (credentials, rate-limit config) |
| otadata | 0xE000 | 8 KB | OTA rollback data |
| app0 | 0x10000 | 1.75 MB | Firmware image (slot 0) |
| app1 | 0x1E0000 | 1.75 MB | OTA firmware image (slot 1) |
| spiffs | 0x3B0000 | 320 KB | LittleFS (logs, PCAP captures) |

The partition table is defined in `partitions.csv`. Do not shrink `spiffs` below 256 KB;
LittleFS requires a minimum of 4 erase blocks (4 × 4 KB = 16 KB) for metadata,
and PCAP captures can reach ~200 KB before the auto-rotate policy truncates the oldest file.

On boards with 16 MB flash (ESP32-S3 N16R8) a custom `partitions_16mb.csv` is used
which extends `app0`/`app1` to 3.5 MB each and `spiffs` to 4 MB.

---

## Flashing and Recovery

### Standard flash

```bash
pio run -e <env> --target upload
```

Replace `<env>` with the board-specific environment name from the table above.

### Recovery mode (manual BOOT hold)

1. Hold the BOOT (GPIO0) button.
2. Press and release the EN (RST) button while still holding BOOT.
3. Release BOOT.
4. Run `pio run -e <env> --target upload`.

### Erase everything (factory reset)

```bash
esptool.py --port /dev/cu.usbserial-XXXX erase_flash
```

After erase, flash normally. NVS is cleared; the device will generate a new password on
first boot.

---

## Board Selection Guide

| Use case | Recommended board | Reason |
|---|---|---|
| General testing / development | AZDelivery ESP32 DevKit C V4 | Use env `doom`; widely available, CP2102N |
| Long-range passive capture | WROVER-IE with external antenna | Use env `doom_wrover`; IPEX connector, PSRAM |
| Portable / battery | M5StickC Plus | Use env `doom_m5stack`; built-in LiPo, AXP192 |
| On-device status display | TTGO T-Display | Use env `doom_ttgo`; built-in TFT |
| Full feature set + performance | Seeed XIAO ESP32-S3 | Use env `doom_s3`; OPI PSRAM, 16 MB flash |
| BLE-focused work | Any WROVER variant | Use env `doom_wrover`; PSRAM for BLE+Wi-Fi |

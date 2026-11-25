# Hardware Validation

This document describes what hardware validation has been performed for Phantom32,
what has been validated automatically via CI, and what physical testing is still
outstanding.

---

## Automated Validation (CI)

The following is validated on every commit via GitHub Actions:

| Check | Method | Boards covered |
|-------|--------|---------------|
| Compilation success | `pio run -e <env>` | All 13 environments |
| RAM budget (≤ 85% DRAM) | PlatformIO memory report | All 13 environments |
| Flash budget | PlatformIO size report | All 13 environments |
| Unit logic (460+ tests) | `pio test -e native` on Ubuntu | Host (native) |
| Memory safety | ASan + UBSan | Host (native) |
| LITE/PSRAM flag coverage | Python config check | All 13 environments |
| Auth route guards | Python regex audit | Source-level |

Firmware artifacts (`.bin`, SHA-256 checksums, build manifests) are uploaded for
every `main` branch build and are available from the GitHub Actions run artifacts.

---

## Physical Hardware Testing

The table below tracks physical validation status. Items marked **Not yet tested**
are known gaps.

### Boot and First-Boot

| Test | Board | Status | Notes |
|------|-------|--------|-------|
| Serial credential output on first boot | ESP32-WROOM-32D (DevKit V1) | Not yet published | Manual test |
| AP WiFi password enforced (device-unique) | ESP32-WROOM-32D (DevKit V1) | Not yet published | |
| Dashboard login with printed password | ESP32-WROOM-32D (DevKit V1) | Not yet published | |
| NVS credential persistence across reboot | ESP32-WROOM-32D (DevKit V1) | Not yet published | |

### Recovery Behavior

| Test | Board | Status |
|------|-------|--------|
| `POST /api/auth/reset-creds` rotates AP password | ESP32-WROOM-32D | Not yet published |
| `esptool.py erase_flash` triggers new first-boot | ESP32-WROOM-32D | Not yet published |
| Lockout clears after 5-minute window | ESP32-WROOM-32D | Not yet published |

### Memory Stability

| Test | Board | Duration | Status |
|------|-------|----------|--------|
| Idle stability | ESP32-WROOM-32D | 24 hours | Not yet published |
| Active tool operation (Beacon Spam + Device Tracker) | ESP32-WROOM-32D | 24 hours | Not yet published |
| Idle stability | ESP32-S3 (PSRAM) | 24 hours | Not yet published |
| Active tool operation (all analytics enabled) | ESP32-S3 (PSRAM) | 24 hours | Not yet published |
| Extended soak | ESP32-WROOM-32D | 72 hours | Not yet published |

### PSRAM Board Validation

| Test | Board | Status |
|------|-------|--------|
| Full analytics module load | ESP32-WROVER or ESP32-S3 | Not yet published |
| PSRAM allocation under load | ESP32-S3 | Not yet published |
| `BOARD_HAS_PSRAM` path confirmed at runtime | Any PSRAM board | Not yet published |

---

## What "Not yet published" Means

These tests have been or can be run on physical hardware, but the results have not
been captured and published as CI artifacts or test reports. They are listed here as
outstanding items so that the gap is visible to reviewers rather than hidden.

A future release will add:
1. A structured hardware test report (JSON or Markdown) in `test/hardware/`
2. A CI step that uploads the report as an artifact when run from a hardware
   test runner (self-hosted GitHub Actions runner)
3. A PSRAM board build validation on at least one PSRAM-equipped device

---

## How to Run Hardware Tests

### Setup

1. Flash the firmware: `pio run -e doom --target upload`
2. Connect serial monitor: `pio device monitor --baud 115200`
3. Record the printed first-boot credentials.

### Boot validation checklist

```
[ ] Serial shows [AUTH] First-boot credentials block
[ ] AP WiFi password is 12 alphanumeric chars (not "phantom32")
[ ] Dashboard password is 12 alphanumeric chars
[ ] API key is 64 hex chars
[ ] Can join Phantom32 AP with the printed AP WiFi password
[ ] Can log in at http://192.168.4.1/dashboard with the printed password
[ ] Session token set as HttpOnly cookie (check browser dev tools)
[ ] Token NOT present in response body
[ ] Reboot: same credentials loaded (not regenerated)
[ ] 5 wrong passwords → "Account locked" for 5 minutes
```

### Memory stability checklist

```
[ ] Run for 24 hours with Beacon Spam active
[ ] No crash or watchdog reset
[ ] Free heap at end ≥ free heap at start (± 1 KB tolerance)
[ ] pio device monitor shows no DRAM or IRAM exhaustion warnings
```

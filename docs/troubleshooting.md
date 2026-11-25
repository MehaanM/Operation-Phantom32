# Troubleshooting Guide — Phantom32

---

## Build issues

### Symptom: `PlatformIO` command not found
**Likely cause:** PlatformIO not installed or not in PATH.  
**Diagnostic:** Run `pio --version`.  
**Resolution:** `pip install platformio==6.2.0`  
**If unresolved:** Ensure `~/.local/bin` is in your PATH. Try `python3 -m platformio --version`.

---

### Symptom: `No such file or directory: .pio/build/doom/firmware.bin`
**Likely cause:** Build failed before producing output.  
**Diagnostic:** Run `pio run -e doom 2>&1 | tail -30` and look for the first error.  
**Resolution:** Most commonly a missing package — run `pio pkg update` then retry.

---

### Symptom: `RAM: [==========] 100.0%` or link error about DRAM overflow
**Likely cause:** Too much static data for the board's RAM.  
**Diagnostic:** Check which environment you're building (`pio run -e <env>`).  
**Resolution:** Non-PSRAM boards must have `-DPHANTOM32_LITE` in their `build_flags`. Check `platformio.ini`.

---

### Symptom: `-Werror=format-truncation` compile error
**Likely cause:** `snprintf` into a buffer the compiler thinks is too small for worst-case input.  
**Resolution:** Increase the buffer to `char buf[20]` instead of `char buf[16]` for IP address fields.

---

## Serial / USB issues

### Symptom: Device not detected (`/dev/cu.usbserial-*` missing on macOS)
**Likely cause:** USB driver not installed.  
**Diagnostic:** Run `ls /dev/cu.*` before and after plugging in.  
**Resolution:** Install the CP2102 or CH340 driver for your board's USB chip. See [Espressif documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/establish-serial-connection.html).

---

### Symptom: Flash upload fails with `A fatal error occurred: Failed to connect to ESP32`
**Likely cause:** ESP32 not in bootloader mode during upload.  
**Diagnostic:** Watch for `Connecting .....____` in the upload output — if it times out, the chip is not responding.  
**Resolution:**
1. Hold the BOOT button while pressing and releasing the EN/RST button.
2. Release BOOT only after the upload starts (when you see `Writing at 0x...`).
3. Some boards enter bootloader automatically via DTR/RTS — try a different USB cable (data cable, not charge-only).

---

### Symptom: Serial monitor shows garbled characters
**Likely cause:** Baud rate mismatch.  
**Resolution:** Use 115200 baud: `pio device monitor --baud 115200`

---

## Firmware startup failures

### Symptom: `E (639) esp_core_dump_flash: Incorrect size of core dump image`
**Likely cause:** A previous firmware left a crash dump partition that the new firmware doesn't recognize.  
**Risk:** None — this is informational. The device boots normally.  
**Resolution:** Erase flash if you want a clean state: `esptool.py erase_flash`. Note: this erases NVS, so credentials will be regenerated on next boot.

---

### Symptom: Device reboots immediately after `[AUTH] auth_init failed`
**Likely cause:** NVS partition is corrupt or incompatible with the current credential schema.  
**Diagnostic:** Serial shows `[AUTH] auth_init failed` → `ESP.restart()`.  
**Resolution:** Erase NVS: `esptool.py --port /dev/cu.usbserial-0001 erase_region 0x9000 0x5000`. The device will generate new credentials on the next boot.

---

## Credential / login issues

### Symptom: Cannot connect to Phantom32 AP
**Likely cause:** Wrong AP password.  
**Diagnostic:** The AP password is printed to the serial console on every boot: `[doom] Connect: SSID=Phantom32 PASS=...`  
**Resolution:** Connect a serial monitor at 115200 baud and read the password from the boot log.

---

### Symptom: "Account locked" after correct password
**Likely cause:** 5 failed login attempts triggered the IP lockout (5-minute cooldown).  
**Diagnostic:** Wait 5 minutes, then try again from the same IP.  
**Resolution:** Lockout expires automatically. If still locked after 5 minutes, reboot the device (lockout table is RAM-only and clears on restart).

---

### Symptom: Need to reset credentials (forgot password / clean install)
**Resolution:** `POST /api/auth/reset-creds` with a valid API key. This regenerates the AP password, dashboard password, and API key. New credentials are printed to the serial console. Alternatively, erase NVS (see above) for a full first-boot reset.

---

## Memory / stability issues

### Symptom: Device crashes or watchdog resets during operation
**Likely cause:** Stack overflow in a task, or heap exhaustion.  
**Diagnostic:** Serial output will show `Guru Meditation Error` with a register dump. Look for `LoadProhibited` (null pointer) or `IntegerDivideByZero`.  
**Resolution:** Note the backtrace and open a bug report. Include the full serial log from boot to crash.

---

### Symptom: Web UI becomes unresponsive after long operation
**Likely cause:** Heap fragmentation or AsyncWebServer buffer exhaustion.  
**Diagnostic:** Check free heap via `GET /api/sys` → `free_heap` field.  
**Resolution:** Reboot the device. Report the heap value and uptime in a bug report if this is reproducible.

---

## How to collect logs for bug reports

```bash
# macOS / Linux
pio device monitor --port /dev/cu.usbserial-0001 --baud 115200 | tee phantom32-log.txt

# Windows (PowerShell)
pio device monitor --port COM3 --baud 115200 | Tee-Object -FilePath phantom32-log.txt
```

Before submitting: **review the log and remove any credential values** (AP password, session tokens, API key).

---

## How to report a reproducible bug

Include in your bug report:
1. Firmware version (from serial boot: `Version X.Y.Z`)
2. Board model and revision
3. Steps to reproduce (from a cold boot)
4. Expected vs actual behaviour
5. Serial log (credentials removed)
6. Free heap at time of issue (from `/api/sys` if accessible)

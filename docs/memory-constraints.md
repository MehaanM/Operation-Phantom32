# Memory Constraints

This document covers the static memory layout of Phantom32 firmware, the heap
allocation strategy, FreeRTOS task stack sizing, and per-board flash/RAM
footprints. All figures are from real builds on the reference hardware
(ESP32-WROOM-32D, 2026-11-25) unless stated otherwise.

---

## Memory regions on ESP32

| Region | Size | Purpose |
|---|---|---|
| DRAM (data + BSS) | 320 KB (LX6) / 512 KB (S3) | Static globals, stack, heap |
| IRAM | 128 KB | ISR code, time-critical functions |
| Flash (code + rodata) | 4–16 MB (board-dependent) | Firmware image, LittleFS partition |
| PSRAM (optional) | 4–8 MB | Heap extension on WROVER / S3 boards |

The 320 KB figure for DRAM on the original ESP32 is the user-accessible pool
after the WiFi/BLE stack and OS overhead. The raw chip total is higher, but
the RF stack is mapped into separate internal memory and is not available to
application code.

---

## Static DRAM budget (WROOM-32D reference build)

Measured from the PlatformIO linker report (`doom` environment):

| Component | Approx. BSS (bytes) | Notes |
|---|---|---|
| WiFi / BLE stack reserve | ~90 KB | Fixed OS overhead; not in BSS |
| Phantom32 static globals | ~36 KB | `main.cpp` globals, tool state |
| Phase 18-20 diagnostics (non-LITE) | ~12 KB | RE, PD, QS, OD, RP combined |
| Phase 18-20 diagnostics (LITE) | ~6 KB | Halved pool sizes |
| Auth manager (sessions + lockout) | ~1.4 KB | 8 × 32-byte tokens + lockout table |
| Rate limiter + tool registry | ~3 KB | Static arrays in `rate_limiter_adv.h` |
| **Total BSS at link time** | **113 KB** | 35.3% of 320 KB DRAM |
| **Free heap at boot** | **162,232 bytes** | Measured on WROOM-32D at 240 MHz |

The 35.3% static utilization leaves ~207 KB available as heap. The heap is
further consumed at runtime by WiFi/BLE stack internals (~80–100 KB at steady
state), leaving the ~162 KB reported at boot.

### PHANTOM32_LITE pool sizes

Under `PHANTOM32_LITE` (original ESP32 without PSRAM), static array capacities
in the five phases 18-20 modules are halved to stay within budget:

| Module | Full | LITE |
|---|---|---|
| RecoveryEngine: failures / snapshots / handlers | 16 / 8 / 16 | 8 / 4 / 8 |
| PhantomDoctor: checks / warnings | 16 / 32 | 8 / 16 |
| QualityScorecard: metrics / snapshots | 20 / 16 | 10 / 8 |
| ObservabilityDashboard: entries / modules | 32 / 16 | 16 / 8 |
| ResourceProfiler: modules / snapshots | 16 / 8 | 8 / 4 |

Tool-module pool sizes (Device Tracker, Beacon Spam, Evil Twin, Aegis) are
also reduced under `PHANTOM32_LITE` — see `docs/hardware-compatibility.md` for
per-module figures.

---

## Heap allocation strategy

### Rule 1 — security-sensitive state stays in DRAM

Session tokens, the PBKDF2 hash/salt, and the IP lockout table are declared as
`static` arrays in `auth_manager.h`. Static storage in a non-PSRAM-aware
translation unit goes to BSS (DRAM). No `ps_malloc()` or
`heap_caps_malloc(MALLOC_CAP_SPIRAM)` appears in `auth_manager.h`. The
rationale is documented in `docs/decisions/ADR-005-dram-not-psram-for-security-state.md`.

### Rule 2 — large analytics buffers use the default allocator

Ring buffers, device tables, and analytics data (`extended_modules.h`) are
allocated with `malloc()` / `new`. On boards with `CONFIG_SPIRAM_USE_MALLOC`
(WROVER, S3), the allocator serves large requests from PSRAM automatically.
On non-PSRAM boards these fall back to DRAM, which is why `PHANTOM32_LITE`
reduces their capacities.

### Rule 3 — heap watermark monitoring

`loop()` samples `esp_get_free_heap_size()` every 30 seconds:

| Threshold | Value | Action |
|---|---|---|
| Warning | 32 KB | Serial warning + RecoveryEngine `SEV_MEDIUM` failure |
| Critical | 16 KB | RecoveryEngine `SEV_HIGH` failure; PhantomDoctor check → FAIL |

OTA updates are additionally disabled when the heap drops below 32 KB of
contiguous free memory, because the OTA staging buffer requires ~32 KB.

### Rule 4 — no dynamic allocation in the WiFi callback path

The 802.11 promiscuous callback (`wifi_sniffer_packet_handler`) runs in the
WiFi task context at interrupt priority. No `malloc()`, `new`, or `String`
construction occurs inside it; all data is written into pre-allocated ring
buffers via atomic index operations.

---

## FreeRTOS task inventory

All tasks use statically-known stack sizes set at `xTaskCreate` time.
No dynamic stack resizing occurs at runtime.

| Task name | Stack (words) | Stack (bytes) | Priority | Purpose |
|---|---|---|---|---|
| `chan_hop` | 2048 | 8 KB | 1 | Main channel hopper (global) |
| `bsp_task` | 2048 | 8 KB | 5 | Beacon spoofer |
| `chop_task` | 2048 | 8 KB | 6 | Channel hopper (per-tool) |
| `ci_probe` | 2048 | 8 KB | 4 | Client isolator probe |
| `fsc_scan` | 2048 | 8 KB | 5 | Frequency scanner |
| `hsforce` / `hsf_probe` | 2048 | 8 KB | 4 | Handshake force |
| `en_run` | 3072 | 12 KB | 4 | ESP-NOW flood |
| `karma` | 3072 | 12 KB | 5 | Karma AP |
| `nf_run` | 3072 | 12 KB | 5 | Null flood |
| `wdg_mon` | 3072 | 12 KB | 5 | Heap watchdog monitor |
| `sched` | 2560 | 10 KB | — | Scheduler |
| Most attack tasks | 4096 | 16 KB | 5 | Standard attack workload |
| `cs_scan` | 8192 | 32 KB | 3 | Certificate scanner (TLS parsing) |
| `pwnface` | 8192 | 32 KB | 5 | PwnFace capture (deep packet work) |

**Stack sizing rationale:**

- 2 KB (2048 words) — tasks that only write into a pre-allocated buffer and
  call no string-building functions.
- 3–4 KB — tasks that call `snprintf`, build JSON responses, or use the
  Arduino `String` class (each `String` allocation uses stack-local
  temporaries).
- 8 KB — tasks that perform TLS parsing (`cs_scan`) or complex frame assembly
  (`pwnface`), where the call stack depth and local buffer requirements are
  larger.

**Stack overflow detection:** FreeRTOS stack watermark checking is enabled in
debug builds (`CONFIG_FREERTOS_WATCHPOINT_END_OF_STACK`). The `/tasks` CLI
command reports the high-water mark for each running task.

---

## Per-board flash / RAM footprint

Figures from the CI build matrix (PlatformIO linker report, `pio run`).
"Static RAM" is BSS + data segment at link time. "Free heap" is measured at
firmware boot on physical hardware where available, otherwise derived from
linker output.

| Board / env | Flash used | Flash total | RAM (static) | DRAM total | PSRAM | Free heap at boot |
|---|---|---|---|---|---|---|
| ESP32-WROOM-32D (`doom`) | 85.0% / 3.37 MB | 4 MB | 35.3% / 113 KB | 320 KB | — | **162,232 bytes** |
| ESP32-WROVER-B (`doom_wrover`) | ~84% | 4 MB | ~32% | 320 KB | 4 MB | ~160 KB (DRAM) + ~3.9 MB (PSRAM) |
| M5Stack Basic (`doom_m5stack`) | ~85% | 4 MB | ~35% | 320 KB | — | ~160 KB |
| M5Stack Core2 (`doom_m5core2`) | ~85% | 16 MB | ~35% | 320 KB | — | ~160 KB |
| M5Stack CoreS3 (`doom_m5cores3`) | ~65% | 16 MB | ~28% | 512 KB | 8 MB | ~380 KB (DRAM) + ~7.9 MB (PSRAM) |
| TTGO T-Display (`doom_ttgo`) | ~85% | 4 MB | ~35% | 320 KB | — | ~160 KB |
| TTGO T-Call (`doom_tcall`) | ~85% | 4 MB | ~35% | 320 KB | — | ~160 KB |
| Heltec WiFi Kit 32 (`doom_heltec`) | ~85% | 4 MB | ~35% | 320 KB | — | ~160 KB |
| ESP32-S3 / XIAO S3 (`doom_s3`) | ~65% | 16 MB | ~27% | 512 KB | 8 MB | ~380 KB (DRAM) + ~7.9 MB (PSRAM) |
| ESP32-S3 DevKitC-1 (`doom_s3_devkit`) | ~65% | 16 MB | ~27% | 512 KB | — | ~380 KB |
| TTGO T-Display S3 (`doom_tdisplay_s3`) | ~65% | 16 MB | ~27% | 512 KB | 8 MB | ~380 KB (DRAM) |
| NodeMCU-32S (`doom_nodemcu`) | ~85% | 4 MB | ~35% | 320 KB | — | ~160 KB |
| ESP32-C3 Seeed XIAO (`doom_c3`) | ~70% | 4 MB | ~20% | 400 KB | — | ~280 KB |

**Notes on the table:**

- PSRAM-equipped boards report two heap pools. `ESP.getFreeHeap()` returns
  the combined DRAM + PSRAM heap when `CONFIG_SPIRAM_USE_MALLOC` is active.
  The DRAM pool alone is queried with
  `heap_caps_get_free_size(MALLOC_CAP_INTERNAL)`.
- S3 and C3 figures are from the CI build report (linker output), not live
  hardware measurement, so "free heap at boot" is an estimate.
- The ESP32-C3 has a single-core RISC-V processor with 400 KB of DRAM and
  no PSRAM option on the XIAO variant. It uses `PHANTOM32_LITE` but has
  more DRAM headroom than the original ESP32, which is why C3 flash usage
  is lower.

---

## CI memory budget enforcement

The `build` CI job enforces hard limits on every push:

| Metric | LITE builds | PSRAM builds |
|---|---|---|
| Flash budget | ≤ 90% | ≤ 96% |
| RAM budget | ≤ 85% | ≤ 85% |

Builds that exceed either threshold fail the CI job, preventing firmware from
shipping with less than 10% flash headroom (needed for OTA staging) or less
than 15% free DRAM (needed to avoid OOM crashes under peak load).

---

## Adding a new feature: memory checklist

Before merging a feature that adds static state:

1. **Estimate BSS delta.** Add up the sizes of new `static` arrays and global
   structs. A `char name[32]` field × 16 entries × 3 structs = 1536 bytes.
2. **Check PHANTOM32_LITE headroom.** The reference LITE build (WROOM-32D)
   has ~207 KB available heap at boot. Reserve at least 48 KB for WiFi/BLE
   stack growth under load, leaving ~159 KB for all dynamic allocations.
   If the new BSS pushes static usage above ~40% of 320 KB, add a
   `#ifdef PHANTOM32_LITE` guard that halves the pool size.
3. **Run the firmware build locally.** `pio run -e doom` prints the RAM/Flash
   report. The CI enforces ≤ 85% RAM, but target ≤ 55% for headroom.
4. **Check task stack.** New FreeRTOS tasks that call `snprintf` or build
   `String` objects need at least 3072 words. TLS / deep packet work needs
   8192 words.

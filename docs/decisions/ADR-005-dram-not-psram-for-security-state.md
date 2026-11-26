# ADR-005: Security-sensitive state in DRAM, not PSRAM

**Status:** Accepted  
**Date:** 2026-11-25  
**Deciders:** Mehaan Minocha

---

## Context

Boards with PSRAM (ESP32-WROVER, ESP32-S3) map external SPI PSRAM as a
heap extension. PlatformIO's `psramInit()` / `CONFIG_SPIRAM_USE_MALLOC`
allows the heap allocator to satisfy large allocations from PSRAM
transparently. The analytics modules in `extended_modules.h` benefit
from this: ring buffers and device tables that would overflow internal
DRAM at 320 KB can be placed in 4–8 MB PSRAM.

However, PSRAM has properties that make it unsuitable for security-
sensitive data:

1. **No ECC.** ESP32's internal DRAM has no parity or ECC, but it is
   considerably harder to reach physically than PSRAM. The SPI bus
   connecting PSRAM is exposed on the PCB and in some variants on the
   module's external pins.

2. **Cold-boot attack surface.** DRAM content is volatile and decays
   within milliseconds to seconds of power removal on most implementations.
   PSRAM is SDRAM and retains content for longer at low temperatures — the
   same mechanism exploited in cold-boot attacks on laptop DRAM.

3. **SPI bus eavesdropping.** On boards where the PSRAM SPI bus is
   accessible (WROVER modules with exposed pads), bus analysis equipment
   can read PSRAM traffic in real time. Internal DRAM is not externally
   accessible via any bus.

The security-sensitive state in Phantom32 is:
- 256-bit session tokens (RAM-only, 30 min TTL)
- PBKDF2 password hash + salt (also written to NVS, but the RAM copy
  is used for comparison)
- IP lockout table

## Decision

All security-sensitive state is explicitly allocated from internal DRAM,
not PSRAM, regardless of board configuration.

In practice this means:
- Session tokens and the IP lockout table are declared as `static` arrays
  in `auth_manager.h`. Static storage in a non-PSRAM-aware translation
  unit goes to BSS or data segment, which is in DRAM.
- The password hash comparison buffer is stack-allocated inside
  `auth_check()`. Stack is in DRAM.
- No `ps_malloc()` or `heap_caps_malloc(MALLOC_CAP_SPIRAM)` calls appear
  in `auth_manager.h`.

The analytics modules and device-tracking ring buffers that benefit from
PSRAM continue to use the default heap allocator on PSRAM boards, since
their content (RSSI histories, probe SSID lists, beacon counts) is not
security-sensitive.

## Consequences

**Positive:**
- Session tokens are not visible on the PSRAM SPI bus, even on WROVER
  boards with exposed pads.
- The cold-boot attack surface is reduced: even if a cold-boot attack
  succeeded in reading PSRAM contents, no credential material would be
  present.
- The decision is auditable: search `auth_manager.h` for `ps_malloc` or
  `MALLOC_CAP_SPIRAM` and find none.

**Negative:**
- Internal DRAM is consumed by security state even on PSRAM boards, where
  it is the more scarce resource. The security state is small: session
  tokens (256 bits × 8 max sessions = 256 bytes), hash + salt (64 bytes),
  IP lockout table (8 entries × ~12 bytes = ~96 bytes). Total: ~400 bytes.
  Acceptable.
- Developers adding features to `auth_manager.h` must remember not to use
  `ps_malloc()`. This constraint is not enforced by the compiler. It is
  documented here and in a comment at the top of `auth_manager.h`.

## Alternatives not taken

**Allow allocator to decide:** Rejected because `CONFIG_SPIRAM_USE_MALLOC`
on PSRAM boards can route `malloc()` calls to PSRAM depending on allocation
size and heap fragmentation. The allocator's behavior is not deterministic
enough to guarantee security-sensitive state stays in DRAM.

**Hardware flash encryption for session state:** Session tokens are RAM-
only by design; they do not persist to flash. Flash encryption does not
apply to RAM contents. This ADR covers the RAM-side decision; flash
encryption is a separate layer documented in `SECURITY.md` as a recommended
production hardening step.

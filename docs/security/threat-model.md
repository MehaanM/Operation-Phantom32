# Threat Model — Phantom32

**Version:** 3.2.0  
**Date:** 2026-11-25  
**Scope:** ESP32 firmware, web dashboard, credential storage, and physical device

---

## System Overview

Phantom32 is a Wi-Fi security research toolkit that runs on ESP32-class microcontrollers. It operates as a standalone device: the ESP32 hosts a web dashboard over its own SoftAP network. There is no cloud connectivity, no mobile app, and no remote management channel.

**Trust boundary:** The AP network created by Phantom32 is the only network it services. Everything beyond the AP (the internet, the user's home network) is out of scope.

---

## Assets

| Asset | Sensitivity | Why it matters |
|-------|-------------|----------------|
| Dashboard API key | High | Authorizes all tool operations |
| Session tokens | High | Grant authenticated access during a session |
| AP WiFi password | High | Controls who can reach the dashboard at all |
| Dashboard password | High | Authenticates operator to the web UI |
| Captured credentials (Evil Twin) | Very High | Contains third-party Wi-Fi passwords |
| NVS storage | High | Persists all credentials across reboots |
| Firmware integrity | High | Compromised firmware defeats all controls |

---

## Threat Table

| Asset | Threat | Mitigation | Verification | Residual risk |
|-------|--------|------------|--------------|---------------|
| AP WiFi password | Brute-force or guessing | 12-char random alphanumeric, generated per device at first boot | Serial output confirms not "phantom32"; unit test | Weak if attacker captures handshake offline |
| Dashboard session | Session hijacking via token theft | 256-bit tokens, RAM-only (not NVS), 30-min TTL, HttpOnly cookie | Unit tests: session lifecycle, token format | Token in RAM is lost on reboot |
| Dashboard session | Replay attack | 256-bit entropy makes token collision infeasible | — | Negligible |
| Login endpoint | Brute-force login | 5-attempt IP lockout (5-min cooldown), constant-time comparison | Unit tests: lockout table, timing | Attacker rotates IP; per-device rate limit only |
| Login endpoint | Timing side-channel | `ct_strcmp` constant-time comparison for all secrets | Unit tests: ct_strcmp branches | Implementation is software constant-time, not hardware |
| API key | Interception in transit | Key only accepted via `X-Phantom32-Key` header (no URL param) | Unit test + route audit | No TLS; network-layer eavesdropping possible |
| Session token | URL leakage | Token only accepted via Cookie or Authorization Bearer | Unit test + route audit | None on compliant clients |
| Captured credentials | Unauthorized read | Auth-gated API; RAM+NVS only; no network egress | Route auth audit in CI | Physical attacker with UART access bypasses all controls |
| NVS storage | Credential extraction | Credentials stored in NVS; flash encryption requires ESP-IDF CMake (see docs/sdkconfig-nvs-encryption-example.txt) | — | Without flash encryption, NVS readable with physical access |
| Firmware | Malicious update | No OTA implemented; firmware updated only by physical flashing | — | Physical access required for firmware replacement |
| Web interface | XSS / injection | Input validated via `input_validator.h`; HTML output escaped via `iv_ssid_to_html` | Unit tests: 76 input validation tests | Only operator-controlled inputs; no untrusted third-party data shown to other users |
| Web interface | CSRF | Session token required for all state-changing operations | Route auth audit | No SameSite cookie attribute enforcement at HTTP layer |
| Device availability | Resource exhaustion (HTTP) | Per-IP rate limit (300 req/min), global TX budget (200 fps) | Unit tests: rate limiter | Sustained DoS from multiple IPs on the same AP could degrade service |
| Device availability | Memory exhaustion | Static allocation for all major subsystems; heap monitored at boot | Build CI: RAM budget ≤ 85% | Long-running operation heap fragmentation not stress-tested |

---

## Attacker Assumptions

### In scope

- **Remote attacker on the AP network** — has joined the Phantom32 AP (knows or guessed the AP password). Can reach port 80. Cannot reach the device over any other network interface.
- **Passive network observer on the AP** — can capture all HTTP traffic between operator and device (no TLS).
- **Physical attacker with UART access** — can read serial output, potentially extract NVS via esptool if flash encryption is not enabled.

### Out of scope

- Attackers on external networks (device has no internet connectivity).
- Attackers who have already compromised the operator's browser or machine.
- Hardware fault injection, side-channel power analysis.
- Attacks against ESP-IDF or the underlying WiFi stack.

---

## Known Limitations

1. **No TLS.** All dashboard traffic is plaintext HTTP. Session tokens and the API key are visible to any passive observer on the AP network. Mitigation: the AP network is a closed research network; do not operate on shared or untrusted networks.

2. **No flash encryption by default.** NVS credentials are readable by anyone with physical access and esptool. ESP-IDF CMake builds support NVS + flash encryption (see `docs/sdkconfig-nvs-encryption-example.txt`); PlatformIO Arduino framework does not expose this configuration path.

3. **IP-based lockout only.** The lockout table holds 16 entries. An attacker with 17+ source IPs (or one that cycles IPs) can bypass per-IP lockout. The global failed-attempt rate and device responsiveness are unaffected.

4. **Software constant-time only.** `ct_strcmp` avoids early-exit timing, but compiler optimizations and cache effects mean it is not hardware-level constant-time. This is acceptable for the threat model (no hardware timing attacker assumed).

5. **Evil Twin credentials stored in RAM.** Captured credentials are held in a ring buffer in heap RAM. They are lost on reboot and are not persistently stored. This is a deliberate design choice.

---

## Security Claims and Evidence

| Claim | Evidence |
|-------|----------|
| "Per-device AP password" | First-boot NVS generation + hardware validation (docs/hardware-validation.md) |
| "Session tokens are 256-bit" | `AM_TOKEN_LEN 64` hex chars = 256-bit entropy in `auth_manager.h` |
| "Constant-time comparison" | `ct_strcmp` implementation + unit tests covering all branches |
| "IP lockout after 5 attempts" | `AM_MAX_ATTEMPTS 5` + unit tests: lockout table, exact boundary |
| "Tokens not in URLs" | Route audit in CI: `am_extract_token_from_request` grep |
| "All 16 sensitive routes auth-gated" | CI static analysis: Python regex audit on every push |
| "PBKDF2 100,000 iterations" | `AM_PBKDF2_ITER 100000` in `auth_manager.h` |

---

## Out-of-Scope Features

The following features interact with Wi-Fi networks beyond the device's own AP. They are designed for **authorized security research in controlled lab environments only**:

- Deauthentication (Deauth tool) — 802.11 management frame injection
- Evil Twin — rogue AP with credential capture portal
- Probe Flood — fake probe request injection
- Beacon Spam — fake AP advertisement injection
- KARMA attack emulation

These features have safety controls (rate limiting, IP lockout on the dashboard) and are documented in SECURITY.md. Using them against networks or devices you do not own or have explicit written permission to test is illegal in most jurisdictions.

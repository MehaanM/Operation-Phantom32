# ADR-003: PBKDF2-HMAC-SHA256 at 100,000 iterations for credential hashing

**Status:** Accepted  
**Date:** 2026-11-25  
**Deciders:** Mehaan Minocha

---

## Context

Phantom32 serves an authenticated HTTP dashboard. The login credential
(a 12-character device-unique password generated on first boot) must be
stored in ESP32 NVS flash so it persists across reboots. An attacker who
gains physical access to the device can read NVS flash via USB with
`esptool.py read_flash`. The question is how to store the credential such
that NVS read access does not immediately yield the credential in usable form.

The options were:

1. **Plaintext storage** — store the password as a string in NVS. Any NVS
   read yields the credential immediately. Unacceptable.

2. **Simple hash (SHA-256, MD5)** — store `SHA256(password)`. Fast to
   compute, which means fast to brute-force on a GPU. A 12-character
   alphanumeric password has ~62^12 ≈ 3×10^21 possibilities, but short
   passwords and common patterns reduce the effective space dramatically.
   Not acceptable for a security-focused project.

3. **PBKDF2-HMAC-SHA256 with iteration count** — a deliberately slow key
   derivation function. Each login attempt requires computing
   `PBKDF2(password, salt, iterations)`. High iteration count raises the
   cost of offline brute-force attacks proportionally to the iteration count.

## Decision

PBKDF2-HMAC-SHA256 with **100,000 iterations** and a per-device random
salt stored alongside the hash in NVS.

The iteration count was chosen as follows:

- OWASP recommends ≥ 600,000 iterations for PBKDF2-SHA256 on modern server
  hardware as of 2023. ESP32 is embedded hardware running at 240 MHz, so
  the same absolute count is not feasible without an unacceptably long login
  latency.
- Target: login latency ≤ 2 seconds on ESP32 @ 240 MHz. This is a user
  experience constraint; a longer delay would make the device appear broken
  during authentication.
- Measurement: 100,000 iterations on ESP32-WROOM-32D rev3 @ 240 MHz
  produces an estimated 0.8–1.2 s latency (single login attempt). This is
  acceptable for a device that authenticates one operator at a time.
- At 100,000 iterations, an attacker with a GPU achieving 10^9 PBKDF2
  operations/second would need approximately 95 years to exhaust a 12-
  character random alphanumeric password space.

## Consequences

**Positive:**
- Significantly raises the cost of offline brute-force attacks against
  captured NVS flash.
- The salt ensures that two devices with the same password produce different
  NVS contents, preventing precomputed rainbow table attacks.
- Credentials are never stored in plaintext anywhere in the firmware or
  in transit (HTTP is local-network AP-only, no TLS; login is not exposed
  to the internet).

**Negative:**
- Login latency is ~1 second, which is noticeable. This is accepted because:
  (a) authentication happens once per session, not per request; (b) the
  device is used by one operator at a time.
- The 100,000 iteration count is below OWASP's current server-side
  recommendation. This gap is accepted because (a) the attack surface
  requires physical device access to obtain the hash, (b) the credential is
  randomly generated and 12 characters long (not a user-chosen weak
  password), and (c) higher iteration counts produce unacceptable latency
  on this hardware.

## Alternatives not taken

**bcrypt / scrypt / Argon2:** ESP32 SDK does not include a battle-tested
implementation of these algorithms in the Arduino framework. Pulling in an
external C implementation would add a dependency and require auditing.
PBKDF2-HMAC-SHA256 is available via the ESP-IDF `esp_tls.h` / mbedTLS
layer that Arduino wraps, keeping the dependency surface small.

**Device-level flash encryption:** ESP32 supports hardware flash encryption
via eFuse. This is a complementary control, not a substitute — it protects
at the hardware layer rather than the application layer. It is not enabled
by default in Phantom32 because (a) burning eFuses is irreversible and can
brick a device if done incorrectly, and (b) it requires a key management
strategy (manufacturing, recovery) that is out of scope for a development-
focused project. Enabling flash encryption is documented as a recommended
hardening step for production deployments in `SECURITY.md`.

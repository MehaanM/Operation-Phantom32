# Security Architecture

This document describes Phantom32's security model: how credentials are
generated and stored, how the web UI is protected, and what threat vectors
are in and out of scope.

---

## Credential Model

### Dashboard password

| Property | Detail |
|---|---|
| Generation | 12-char alphanumeric (charset excludes confusable chars: `0 1 i l o O`) |
| Storage | PBKDF2-HMAC-SHA256 hex + random salt in NVS; plaintext never stored |
| KDF | PBKDF2-HMAC-SHA256, 100,000 iterations, 128-bit (16-byte) random salt |
| Salt | Generated on first boot; regenerated on every password reset or change |
| Disclosed | Once, on UART serial at first boot |

Credentials are stored under the `phantom32` NVS namespace.
See `src/auth_manager.h` — `AM_NVS_PASS_HASH_KEY` and `AM_NVS_PASS_SALT_KEY`.

### AP WiFi password

| Property | Detail |
|---|---|
| Generation | Same 12-char alphanumeric generator as dashboard password |
| Storage | Plaintext in NVS (`AM_NVS_AP_WIFI_PW_KEY`) |
| Independence | Different from dashboard password — generated separately |
| Disclosed | Once, on UART serial at first boot |

The AP WiFi password controls who can join the Phantom32 Wi-Fi network.
Joining the network is not sufficient to access the dashboard — a session
login is still required.

### API key

| Property | Detail |
|---|---|
| Generation | 32 random bytes → 64-char hex string |
| Storage | Hex string in NVS (`AM_NVS_APIKEY_KEY`) |
| Transport | `X-Phantom32-Key` header **only**; query params rejected to prevent log leakage |

---

## Session Management

| Property | Detail |
|---|---|
| Token size | 32 bytes (256-bit), hex-encoded |
| Storage | RAM only; invalidated on reboot |
| Transport | `Cookie: phantom32_token=<hex>` or `Authorization: Bearer <hex>` |
| Expiry | 30 minutes of inactivity (`AM_SESSION_TTL_S`) |
| Max concurrent | 8 sessions (`AM_SESSION_MAX`) |
| Eviction | LRU when table is full |
| Timing safety | All token and password comparisons use `am_ct_strcmp()` (constant-time) |
| Cookie flags | `HttpOnly; SameSite=Strict` — token not accessible from JavaScript |

Session tokens are **never** accepted via URL query parameters.
URLs appear in server access logs and browser history.

---

## IP Lockout

After `AM_LOCKOUT_ATTEMPTS` (5) consecutive failed login attempts from an IP:

- The IP is blocked for `AM_LOCKOUT_S` (300 seconds, 5 minutes).
- Blocked IPs receive HTTP 403 immediately without running password verification.
- Lockout entries are stored in RAM; reboot clears them.
- A successful login clears the fail counter for that IP.
- API key failures increment the fail counter for the requesting IP.

---

## Route Protection

Every HTTP route except `/login` and `/health` calls `auth_check()` before
executing. The CI `lint` job verifies that all 16 sensitive routes have
`auth_check` guards (see the "Check all 16 sensitive routes" step in
`.github/workflows/build.yml`).

Routes that serve HTML also call `auth_redirect_login()` on failure (HTTP 302)
rather than the JSON 401 response used by API routes.

---

## Input Validation

All web UI arguments are validated by `src/input_validator.h` before use:

| Field | Rule |
|---|---|
| SSID | Printable ASCII, max 32 bytes |
| MAC | Exactly 12 or 17 hex characters |
| Channel | Integer in [1, 14] |
| IP address | Dotted-decimal, 7–15 chars |
| Integers | Range-checked before use |
| Labels | Printable ASCII, max 32 bytes |
| JSON output | HTML-escaped via `iv_json_escape()` |

---

## Transport Security

The HTTP server listens **only** on the ESP32's own AP (192.168.4.1). The AP
itself is WPA2-protected with a per-device random password.

HTTPS is not currently implemented on non-PSRAM boards (mbedTLS TLS stack
requires ~40 KB contiguous heap while the full firmware is running).
PSRAM variants have sufficient heap; HTTPS is planned.

---

## NVS Security

Credentials are stored in the ESP32's NVS (Non-Volatile Storage) partition
with wear-leveling. NVS is not exposed via any HTTP endpoint.

**Without NVS encryption:** A physical flash dump with `esptool.py read_flash`
exposes the API key and AP WiFi password in plaintext. The dashboard password
is stored only as a PBKDF2-HMAC-SHA256 hash and salt — recovery requires
cracking PBKDF2 offline (100,000 iterations), which provides meaningful
brute-force resistance even after a flash dump.

### NVS / Flash Encryption Support

ESP-IDF supports hardware-backed flash encryption and Secure Boot V2, which
protect NVS contents against physical flash extraction. Configuration varies
by chip:

| Chip | Flash Encryption | Secure Boot V2 | NVS Encryption |
|------|-----------------|----------------|----------------|
| ESP32 (original) | Yes (eFuse-based, AES-256) | Yes | Yes (requires flash encryption) |
| ESP32-S3 | Yes | Yes | Yes |
| ESP32-C3 | Yes | Yes | Yes |
| ESP32-WROVER | Yes (same as ESP32) | Yes | Yes |

**To enable flash encryption on ESP32/S3/C3:**

1. Enable in `sdkconfig`: `CONFIG_FLASH_ENCRYPTION_ENABLED=y`
2. For development mode (reversible): `CONFIG_FLASH_ENCRYPTION_MODE_DEVELOPMENT=y`
3. For production (one-way, irreversible): `CONFIG_FLASH_ENCRYPTION_MODE_RELEASE=y`
4. Enable NVS encryption: `CONFIG_NVS_ENCRYPTION=y`
5. Build and flash; the device burns eFuses on first boot.

**Warning:** Production flash encryption is irreversible. A device with release-mode
encryption that loses its key material is permanently unrecoverable. Test
in development mode first on a non-production device.

**Current status:** Flash encryption and NVS encryption are **not enabled** in the
default Phantom32 builds. This is documented here as a known gap — users who require
protection against physical flash extraction should enable it in their build
configuration. This is consistent with the threat model: the primary threat surface
is network-based (web UI exploitation), not physical hardware access.

Enabling flash encryption requires ESP-IDF `sdkconfig` changes that are outside the
scope of the PlatformIO Arduino framework build used by this project. A future release
targeting ESP-IDF CMake builds will include hardened NVS encryption support.

---

## Threat Model

The full threat model is documented in [`docs/threat-model.md`](./threat-model.md).

### In scope

- Authentication bypass via web interface
- Session token theft or forgery
- Credential exposure over UART (console access is owner access by design)
- Input validation bypasses leading to firmware crash or code execution
- NVS credential exposure via crafted HTTP requests
- Rate-limiter bypass enabling sustained hardware damage

### Out of scope

- Physical flash dump (NVS encryption is documented but not enabled by default; see NVS Security section)
- RF side-channels (the device is itself a radio research tool)
- Attacks requiring physical reset (press BOOT + EN = factory state, as designed)
- Independent 802.11 frame injection (the tool is designed to inject frames)

---

## Security Contacts

See [`SECURITY.md`](../SECURITY.md) for the responsible disclosure process
and contact information.

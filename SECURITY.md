# Security Policy

## Supported Versions

| Version | Supported |
|---------|-----------|
| main    | Yes       |
| develop | Yes       |

## Responsible Disclosure

If you discover a security vulnerability in this project, please report it
responsibly. **Do not open a public GitHub issue** for security bugs.

**Contact:** Open a [GitHub Security Advisory](https://github.com/MehaanM/Operation-Phantom32_Protocol/security/advisories/new)
to report a vulnerability privately. We will respond within 72 hours.

## What to Report

- Authentication bypasses in the web UI
- Session token leakage or forgery
- Credentials exposed in firmware over UART
- Input validation bypasses leading to firmware crash or code execution
- NVS credential exposure via crafted HTTP requests
- Rate limiter bypass enabling local hardware damage (sustained full TX power)

## What NOT to Report

The following are intentional design choices, not vulnerabilities:

- The device broadcasts a WPA2-protected AP named `Phantom32` with default
  password `phantom32` — this is by design; the Web UI behind it also requires
  a session login
- Frame injection capabilities — the device is a security research tool
- Default credentials printed to UART on first boot — console access is
  assumed to be the owner's access

## Security Architecture

### Authentication (auth_manager.h)

- Device-unique 12-character alphanumeric password generated on first boot and
  stored in NVS (never hardcoded in firmware).
- Session tokens are 256-bit random values stored in RAM only; they are
  invalidated on reboot.
- Five failed login attempts from an IP triggers a 5-minute lockout.
- All string comparisons for token/password validation use constant-time
  comparison (`am_ct_strcmp`) to prevent timing side-channels.

### Input Validation (input_validator.h)

- Every web UI argument is validated before use:
  - SSID: printable ASCII only, max 32 bytes
  - MAC: exactly 12 or 17 hex characters
  - Channel: integer in [1, 14]
  - Integers: range-checked before use
- JSON output HTML-escapes all user-supplied strings to prevent XSS.

### Rate Limiting (rate_limiter_adv.h)

- Per-tool TX rate limiting via token bucket prevents unintentional sustained
  RF interference.
- Global TX budget of 200 frames/second across all tools.
- Per-IP HTTP rate limit of 300 requests/minute.

### NVS Credential Storage

- Credentials stored in ESP32 NVS with wear-leveling partition.
- Not exposed via any HTTP endpoint.
- Password reset requires physical serial (UART) access or the
  `/api/auth/reset-creds` endpoint (requires current session token).

### Transport Security

- The HTTP server listens only on the ESP32's own AP (192.168.4.1).
- All sensitive API endpoints require a valid session token.
- No HTTPS currently (ESP32-WROOM-32D lacks sufficient RAM for mbedTLS while
  the full firmware is running); planned for PSRAM variants.

## Vulnerability Scoring

We follow the [CVSS v3.1](https://www.first.org/cvss/calculator/3.1) framework
when evaluating reported vulnerabilities.

## Legal

Use of this firmware is governed by the MIT License and the Disclaimer
in [LICENSE](./LICENSE). Unauthorized use against networks you do not own is
illegal. See [LEGAL.md](./LEGAL.md) for jurisdiction-specific guidance.

# Phantom32 HTTP API Reference

Base URL: `http://192.168.4.1`

All endpoints except `/login` and `/api/iv/selftest` require authentication.
Pass the session token as:
- Cookie: `phantom32_token=<token>`
- Header: `Authorization: Bearer <token>`
- Query param: `?token=<token>`

---

## Authentication

### POST /login

Authenticate with the device password. Returns a session cookie.

**Request body** (form-encoded or JSON):
```
password=<your_password>
```

**Response:**
- `200 OK` — Login successful; `Set-Cookie: phantom32_token=<64-char-hex>; HttpOnly; Path=/`
- `401 Unauthorized` — Wrong password (JSON: `{"error":"invalid_credentials"}`)
- `403 Forbidden` — IP is locked out (JSON: `{"error":"locked","retry_after_s":300}`)

---

### GET /login

Serve the login HTML page.

---

### GET /logout

Invalidate the current session token.

**Response:**
- `200 OK` — Session cleared (JSON: `{"ok":true}`)

---

### GET /api/auth/status

Return authentication status for the current token.

**Response:**
```json
{
  "authenticated": true,
  "session_age_s": 42,
  "ttl_s": 1758,
  "remote_ip": "192.168.4.5"
}
```

---

### POST /api/auth/change-password

Change the device password. Requires current session.

**Request body** (form-encoded):
```
old_password=<current>&new_password=<new>
```

**Response:**
- `200 OK` — Password changed (JSON: `{"ok":true}`)
- `400 Bad Request` — Validation failure (JSON: `{"error":"..."}`)
- `401 Unauthorized` — Wrong current password

---

### POST /api/auth/reset-creds

Reset credentials to a new randomly generated password.
**Requires current valid session. Use when locked out via UART.**

**Response:**
```json
{"ok":true,"new_password":"<12-char-alphanumeric>"}
```

> **Note:** The new password is printed once to UART (115200 baud) and in the
> response body. It is not stored in plaintext anywhere afterwards.

---

## Status

### GET /

Serve the main web UI index page.

---

### GET /api/status/all

Return aggregate status of all tools and system state.

**Response:**
```json
{
  "uptime_ms": 12345,
  "heap_free": 142000,
  "rssi": -45,
  "channel": 6,
  "tools": {
    "beacon_spam": {"running": false, "frames_sent": 0},
    "deauth": {"running": false, "frames_sent": 0},
    "evil_twin": {"running": false, "creds_captured": 0},
    "probe_flood": {"running": false, "frames_sent": 0},
    "device_tracker": {"devices": 0, "aps": 0}
  }
}
```

---

## Rate Limiter

### GET /api/rla/status

Return current rate limiter state for all tools.

**Response:**
```json
{
  "global_fps": 200,
  "global_tokens": 198.3,
  "http_rpm": 300,
  "tools": [
    {"name": "deauth", "cap": 50, "rate_fps": 10, "tokens": 50.0, "allowed": 42, "denied": 0},
    {"name": "beacon_spam", "cap": 100, "rate_fps": 50, "tokens": 95.2, "allowed": 7, "denied": 0}
  ]
}
```

---

### POST /api/rla/global

Set the global TX frame rate limit.

**Request body**:
```
fps=<1..200>
```

**Response:** `{"ok":true,"fps":100}`

---

### POST /api/rla/tool

Set a per-tool rate limit.

**Request body**:
```
tool=<tool_name>&cap=<1..200>&rate=<1..200>
```

**Response:** `{"ok":true}`

---

### POST /api/rla/tool/reset

Reset a per-tool rate limiter to defaults.

**Request body**:
```
tool=<tool_name>
```

---

## Input Validator

### GET /api/iv/selftest

Run the input validator's built-in self-test suite (no auth required).

**Response:**
```json
{
  "tests": [
    {"name": "ssid_valid", "ok": true},
    {"name": "ssid_too_long", "ok": true},
    {"name": "mac_colon", "ok": true},
    ...
  ]
}
```

All `ok` values should be `true`. Any `false` value indicates a validator
regression.

---

## Tool: Beacon Spam

### GET /api/beacon/status

Return beacon spam state.

**Response:**
```json
{"running": false, "ssid_count": 3, "frames_sent": 0, "channel": 6}
```

---

### POST /api/beacon/start

Start beacon spam.

**Request params (form or query):**
```
channel=<1..14>    (optional, defaults to current channel)
interval=<10..1000>  (beacon interval in TUs, default 100)
```

---

### POST /api/beacon/stop

Stop beacon spam.

---

### POST /api/beacon/ssid/add

Add an SSID to the beacon spam pool.

**Request params:**
```
ssid=<1..32 printable ASCII chars>
```

---

### POST /api/beacon/ssid/clear

Clear the SSID pool.

---

## Tool: Deauth

### GET /api/deauth/status

**Response:**
```json
{
  "running": false,
  "target_mac": "FF:FF:FF:FF:FF:FF",
  "bssid": "AA:BB:CC:DD:EE:FF",
  "reason": 7,
  "frames_sent": 0
}
```

---

### POST /api/deauth/start

Start sending deauth frames.

**Request params:**
```
target=<MAC>      (client MAC, or FF:FF:FF:FF:FF:FF for broadcast)
bssid=<MAC>       (AP BSSID)
reason=<1..65534> (IEEE 802.11 reason code, default 7)
channel=<1..14>
```

---

### POST /api/deauth/stop

Stop deauth.

---

## Tool: Evil Twin

### GET /api/evil_twin/status

**Response:**
```json
{
  "running": false,
  "target_ssid": "",
  "creds_captured": 0,
  "log_count": 0
}
```

---

### POST /api/evil_twin/start

Start the evil twin AP and captive portal.

**Request params:**
```
ssid=<target SSID to impersonate>
channel=<1..14>
```

---

### POST /api/evil_twin/stop

Stop the evil twin AP.

---

### GET /api/evil_twin/creds

Return captured credentials.

**Response:**
```json
{
  "count": 2,
  "creds": [
    {"ssid": "HomeNet", "password": "hunter2", "ip": "192.168.4.5", "ts_ms": 12345},
    {"ssid": "HomeNet", "password": "hunter3", "ip": "192.168.4.6", "ts_ms": 67890}
  ]
}
```

---

### POST /api/evil_twin/creds/clear

Clear captured credentials.

---

## Tool: Probe Flood

### GET /api/probe_flood/status

**Response:**
```json
{"running": false, "ssid_count": 0, "client_count": 0, "frames_sent": 0}
```

---

### POST /api/probe_flood/start

Start sending probe requests.

**Request params:**
```
channel=<1..14>
```

---

### POST /api/probe_flood/stop

Stop probe flood.

---

### POST /api/probe_flood/ssid/add

Add an SSID to probe for.

**Request params:**
```
ssid=<SSID>
channel=<1..14>
```

---

## Tool: Device Tracker

### GET /api/dt/status

**Response:**
```json
{
  "device_count": 5,
  "ap_count": 3,
  "alert_count": 0
}
```

---

### GET /api/dt/devices

Return tracked device list.

**Response:**
```json
{
  "devices": [
    {
      "mac": "02:11:22:33:44:55",
      "vendor": "Unknown",
      "type": 1,
      "rssi": -65,
      "randomized": true,
      "probe_count": 2,
      "probes": ["HomeNet", "Office"]
    }
  ]
}
```

---

### GET /api/dt/aps

Return tracked AP list.

---

### POST /api/dt/clear

Clear tracked devices and APs.

---

## HTTP Status Codes

| Code | Meaning |
|------|---------|
| 200 | Success |
| 302 | Redirect to /login (not authenticated) |
| 400 | Bad request (invalid parameters; `error` field has details) |
| 401 | Unauthorized (missing or invalid session token) |
| 403 | Forbidden (IP locked out after too many failed attempts) |
| 404 | Unknown endpoint |
| 429 | Too Many Requests (HTTP rate limit exceeded) |
| 500 | Internal error |

---

## Programmatic Access (API Key)

For automation, you can use an API key instead of session cookies:

```bash
curl -H "X-Phantom32-Key: <apikey>" http://192.168.4.1/api/status/all
```

Or as a query param:

```bash
curl "http://192.168.4.1/api/status/all?apikey=<apikey>"
```

The API key is generated alongside the password on first boot and printed to
UART. It does not expire (unlike session tokens) and survives reboots.
Rotate it via `/api/auth/reset-creds`.

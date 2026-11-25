# Architecture Overview — Phantom32

**Version:** 3.2.0

---

## System Context

Phantom32 runs entirely on the ESP32 microcontroller. There is no cloud backend, no mobile app, and no external service dependency at runtime.

```
┌────────────────────────────────────────────────────────────────┐
│  Operator's device (browser / curl)                            │
└────────────────────┬───────────────────────────────────────────┘
                     │  HTTP (port 80)
                     │  802.11 SoftAP — Phantom32 AP
┌────────────────────▼───────────────────────────────────────────┐
│  ESP32 (Xtensa LX6 @ 240 MHz)                                  │
│                                                                 │
│  ┌──────────────────┐   ┌────────────────────────────────────┐  │
│  │  Web server       │   │  Analytics modules (13 tools)      │  │
│  │  (AsyncWebServer) │   │  Beacon Spam / Deauth / Evil Twin  │  │
│  │  port 80          │   │  Probe Flood / Harvest / Tracker   │  │
│  └────────┬─────────┘   │  Channel Hopper / Aegis / KARMA    │  │
│           │              │  PMKID / Multicast / Scan / Age    │  │
│  ┌────────▼─────────┐   └────────────────────────────────────┘  │
│  │  auth_manager.h  │                                            │
│  │  rate_limiter.h  │   ┌────────────────────────────────────┐  │
│  │  input_validator │   │  FreeRTOS tasks                    │  │
│  │  safety.h        │   │  Task: web server                  │  │
│  └──────────────────┘   │  Task: tool execution              │  │
│                          │  Task: Aegis / promisc listener    │  │
│  ┌──────────────────┐   └────────────────────────────────────┘  │
│  │  NVS (flash)     │                                            │
│  │  Credentials     │   ┌────────────────────────────────────┐  │
│  │  Config          │   │  802.11 layer                      │  │
│  └──────────────────┘   │  esp_wifi_80211_tx (inject)        │  │
│                          │  esp_wifi_set_promiscuous_rx_cb    │  │
│                          └────────────────────────────────────┘  │
└────────────────────────────────────────────────────────────────┘
```

---

## Major Modules

| Module | File | Responsibility |
|--------|------|----------------|
| Main entry point | `src/main.cpp` | FreeRTOS task setup, HTTP route registration, WiFi init |
| Authentication | `src/auth_manager.h` | PBKDF2 login, session tokens, IP lockout, API key |
| Rate limiter | `src/rate_limiter_adv.h` | Per-tool token bucket, global TX budget, per-IP HTTP rate |
| Input validation | `src/input_validator.h` | SSID/MAC/IP/label/JSON/HTML sanitization |
| Safety controls | `src/safety.h` | Per-tool enable/disable, blocklist, audit log |
| Analytics modules | `src/extended_modules.h` | Conditional compilation of 13 research tools |
| Beacon Spam | `src/beacon_spam.h` | 802.11 beacon frame injection |
| Deauth | `src/deauth.h` | Deauthentication frame injection |
| Evil Twin | `src/evil_twin.h` | Rogue AP, captive portal, credential ring buffer |
| Probe Flood | `src/probe_flood.h` | Probe request injection |
| Probe Harvest | `src/probe_harvest.h` | Passive probe collection |
| Device Tracker | `src/device_tracker.h` | STA/AP detection, RSSI history |
| Aegis | `src/aegis.h` | Passive defense detection |

---

## Dependency Graph

```
main.cpp
├── auth_manager.h       (no deps on other modules)
├── rate_limiter_adv.h   (no deps on other modules)
├── input_validator.h    (no deps on other modules)
├── safety.h             → auth_manager.h
└── extended_modules.h
    ├── beacon_spam.h    → rate_limiter_adv.h, input_validator.h
    ├── deauth.h         → rate_limiter_adv.h
    ├── evil_twin.h      → rate_limiter_adv.h, input_validator.h, auth_manager.h
    ├── probe_flood.h    → rate_limiter_adv.h
    ├── device_tracker.h → input_validator.h
    └── aegis.h          (passive listener — no deps)
```

**Key invariant:** `auth_manager.h`, `rate_limiter_adv.h`, and `input_validator.h` have no dependencies on other Phantom32 modules. They can be unit-tested in isolation on native hardware with no stubs beyond their own headers.

---

## State Management

| State | Where stored | Lifetime |
|-------|-------------|----------|
| Credentials (hash, salt, API key, AP password) | NVS (flash) | Persistent across reboots |
| Session tokens | RAM (`g_am_sessions[]`) | Until reboot or 30-min TTL |
| IP lockout table | RAM (`g_am_lockouts[]`) | Until reboot |
| Rate limiter token buckets | RAM | Until reboot |
| Captured Evil Twin credentials | RAM ring buffer | Until reboot |
| Tool enable/disable state | NVS | Persistent |
| Rate limiter config | NVS | Persistent |

---

## Initialization Sequence

```
1. Serial init (115200 baud)
2. NVS Preferences init
3. auth_init()           → load/generate credentials from NVS
4. rla_init()            → load rate limiter config from NVS
5. WiFi SoftAP start     → SSID=Phantom32, password from NVS
6. HTTP server start     → register all routes
7. extended_modules init → start tools per saved enable state
8. FreeRTOS scheduler    → tasks begin executing
```

If `auth_init()` fails (e.g. NVS corruption), the device restarts rather than falling back to default credentials. This is a fail-closed design.

---

## Error Propagation

- HTTP handlers return JSON `{"error": "..."}` with appropriate HTTP status codes.
- Authentication failures return 401 or 403 and increment the IP lockout counter.
- Input validation failures return 400.
- Internal errors (NVS write failure, memory exhaustion) log to serial and return 500.
- Critical init failures (auth_init) → `ESP.restart()`.

---

## PHANTOM32_LITE Mode

Non-PSRAM boards compile with `-DPHANTOM32_LITE`. This disables the 13 analytics modules and reduces static RAM by approximately 30%. The authentication, rate limiting, input validation, and dashboard remain fully functional in LITE mode.

PSRAM boards (`BOARD_HAS_PSRAM`) enable the full analytics suite with extended buffer sizes.

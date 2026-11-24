# Phantom32 Threat Model

**Version:** 1.0.0
**Date:** 2026-11-24
**Scope:** Phantom32 firmware running on a supported ESP32 board in AP mode on a local Wi-Fi network. Physical hardware security is out of scope.

---

## 1. System Overview

Phantom32 is an embedded security-research toolkit that runs on an ESP32 microcontroller. It:

- Creates a Wi-Fi access point (SSID `Phantom32`, AP password `phantom32` by default) so the operator can connect a laptop or phone.
- Serves an HTTP dashboard on port 80 at 192.168.4.1 (or the configured AP gateway IP).
- Performs passive 802.11 frame capture via promiscuous mode and active frame injection via `esp_wifi_80211_tx()` on request from the authenticated dashboard user.
- Logs events and captures to LittleFS.
- Exposes a BLE interface (NimBLE) for proximity-based status read-out.

The device has no outbound internet connectivity. All control is local.

---

## 2. Trust Boundaries

```
[ Internet ]   <-- no connection
      |
      X (no path)
      |
[ Operator laptop / phone ]
      |
   [ Wi-Fi AP ]  <--- AP password: phantom32 (default; shared secret)
      |
[ Phantom32 HTTP dashboard ]  <--- session token (256-bit, RAM-only)
      |
[ ESP32 hardware / radio ]
```

Boundaries:

| Boundary | Description |
|----------|-------------|
| **B1** AP join | Anyone who knows the AP password can connect to the 192.168.4.x subnet. |
| **B2** Dashboard auth | An HTTP session token is required for all sensitive routes. Issued only after correct dashboard password entry. |
| **B3** Physical | Anyone with physical access to the board can flash new firmware, read NVS via JTAG, or attach a serial console. |
| **B4** Radio | The ESP32 radio can be driven to inject or capture 802.11 frames. Only authenticated dashboard users can trigger injection. |

---

## 3. Threat Actors

| Actor | Access level | Motivation |
|-------|-------------|------------|
| **Authorized operator** | Physical possession + AP credential | Legitimate security researcher using the tool. |
| **Malicious AP-connected client** | Knows AP password, on the 192.168.4.x subnet | Attempts to abuse device capabilities without dashboard auth. |
| **Passive RF observer** | Within radio range | Captures 802.11 traffic between operator laptop and device AP. |
| **Physical attacker** | Direct physical access to board | Extracts credentials from NVS, reflashes firmware. |
| **LAN-based attacker** | Same subnet as a misconfigured device | Reaches HTTP port 80 without prior AP knowledge (atypical deployment). |

---

## 4. Attack Surface

### 4.1 HTTP API (port 80)

The dashboard server is the primary attack surface for a network-connected attacker.

| Risk | Mitigation |
|------|-----------|
| Unauthenticated access to sensitive routes | All 16 sensitive routes guarded by `auth_check()`. CI verifies this on every push. |
| Brute-force of dashboard password | IP-based lockout: 5 failures → 300-second lockout. Constant-time `am_ct_strcmp` prevents timing oracle. |
| Session token theft via network sniffing | AP traffic is WPA2-encrypted (AP password required to decrypt). Token is 256-bit random, 30-minute TTL. |
| Session fixation / CSRF | No CSRF tokens yet — open issue (see §6). Dashboard is single-user; session is RAM-only. |
| Input injection (channel, interval params) | Integer range validation on all numeric inputs. String inputs length-checked. |
| Malformed HTTP leading to crash | Arduino WebServer library handles framing; no raw socket parsing in firmware. |

### 4.2 Wi-Fi AP

| Risk | Mitigation |
|------|-----------|
| AP password is a shared default (`phantom32`) | Default is documented as a "network join" secret, not an authentication credential. The dashboard has a separate device-unique password. Users are advised to change the AP password post-deployment. |
| Rogue client performs deauth storm or frame injection from the AP subnet | Victim network is isolated from the AP subnet by design; no bridge mode. Safety module enforces TX rate budgets. |
| AP SSID `Phantom32` is a recognizable beacon | Operators should change SSID before field use. This is a tool for controlled environments. |

### 4.3 BLE

| Risk | Mitigation |
|------|-----------|
| BLE advertisement leaks device presence | Intentional for proximity-based status read-out. Can be disabled in `config_manager.h`. |
| BLE write characteristics allow unauthenticated control | BLE interface is currently read-only for status. No write characteristics exposed. |

### 4.4 LittleFS / NVS

| Risk | Mitigation |
|------|-----------|
| NVS readable via JTAG | Physical access assumption. JTAG fuse-blowing not implemented; physical security is operator responsibility. |
| Dashboard password stored in NVS | Password is hashed (HMAC-SHA256 with device-unique salt) before NVS write. |
| Flash capture data includes real traffic | Captures are stored locally; no network egress. Operator is responsible for secure deletion. |

### 4.5 Frame Injection

| Risk | Mitigation |
|------|-----------|
| Operator injects management frames at target networks | Safety module enforces TX budget (default 10 frames/s) and autostop threshold. Per-BSSID blocklist prevents accidents. All injection requires dashboard auth. |
| Out-of-range channel injection | Channel validated to [1, 14] before `esp_wifi_set_channel()` call. |

---

## 5. Security Controls Summary

| Control | Mechanism | Verified by |
|---------|-----------|------------|
| Dashboard authentication | 256-bit session token, 30-min TTL, RAM-only | Integration tests, CI auth audit |
| Brute-force prevention | IP lockout after 5 failures, 300-s duration | Unit tests (test_negative.cpp) |
| Timing-safe password check | `am_ct_strcmp` (volatile XOR loop) | Unit tests (test_negative.cpp) |
| Input validation | Range checks on all numeric route params | Unit tests, CI build |
| TX rate limiting | Token bucket per injection type | Unit tests (test_safety.cpp) |
| TX blocklist | Per-BSSID RAM blocklist | Unit tests (test_safety.cpp) |
| No hardcoded credentials | AP password in `#define` only; dashboard password device-unique in NVS | CI credential lint |
| Auth coverage | All 16 sensitive routes guarded | CI Python audit script |
| Duplicate-symbol prevention | `-Wl,-zmuldefs` banned; CI fails hard if reintroduced | CI lint |
| Memory safety | ASan + UBSan native test run | CI `native-sanitizers` job |

---

## 6. Known Gaps and Open Issues

| Gap | Severity | Tracking |
|-----|----------|---------|
| No CSRF protection | Medium | AP is single-network, single-user, so CSRF risk is reduced, but SameSite=Strict cookie attribute and a CSRF token should be added. |
| AP password is a known default | Medium | Generate a unique AP password at first boot and display it on serial. |
| HTTP only (no TLS) | Medium | ESP32 mbedTLS supports HTTPS; AP-level WPA2 provides transport encryption today. |
| No secure-erase of LittleFS captures | Low | `esp_partition_erase_range` with multiple passes should be offered as a wipe option. |
| BLE pairing unauthenticated | Low | BLE read characteristics expose no secrets today; add BLE passkey pairing before adding any write characteristics. |
| Fail-safe for corrupted safety config | Low | On NVS checksum mismatch, device should revert to most-restrictive defaults rather than arbitrary values. |
| Centralized authorization | Architectural | Per-route `auth_check()` calls are correct but fragile; a middleware registry pattern would prevent future omissions. |

---

## 7. Deployment Recommendations

1. Change the AP password immediately after first boot.
2. Change the AP SSID to a non-identifiable name before field use.
3. The dashboard password is device-unique and randomly generated at first boot — record it from the serial console or the QR code on the boot splash.
4. Use the device only on networks you own or have explicit written authorization to test.
5. Enable the safety module autostop threshold (default 80%) to prevent runaway injection.
6. Store captured PCAP data on encrypted storage; use the `/api/fs/format` wipe after each engagement.
7. When not in active use, power off the device — the AP disappears and BLE advertisement stops.

---

## 8. Responsible Use

Phantom32 is a research and educational tool. Capturing or injecting 802.11 frames on networks without explicit written authorization from the network owner is illegal in most jurisdictions (CFAA in the US, Computer Misuse Act in the UK, and equivalents elsewhere). The authors accept no liability for misuse. See `LEGAL.md` for the full legal notice.

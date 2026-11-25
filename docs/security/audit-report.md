# Security Audit Report — Phantom32

**Version:** 3.2.0  
**Date:** 2026-11-25  
**Reviewer:** Self-review (see §Independent Review Status)  
**Scope:** Authentication, session management, input validation, HTTP endpoints, storage

---

## Summary

| Category | Findings | Critical | High | Medium | Low | Closed |
|----------|----------|----------|------|--------|-----|--------|
| Authentication | 2 | 0 | 0 | 1 | 1 | 2 |
| Session management | 1 | 0 | 0 | 0 | 1 | 1 |
| Input validation | 0 | 0 | 0 | 0 | 0 | — |
| HTTP endpoints | 1 | 0 | 1 | 0 | 0 | 1 |
| Storage | 1 | 0 | 1 | 0 | 0 | 0 (documented) |
| Memory safety | 0 | 0 | 0 | 0 | 0 | — |

---

## Findings

### AUTH-001 — API key printed in full at boot [Low] [Closed]

**Finding:** `auth_print_credentials()` previously printed only the first 16 chars of the 64-char API key, with a note pointing to a handler that did not return the full key.  
**Risk:** Operator could not verify the API key without additional steps.  
**Remediation:** `Serial.printf` now prints the full API key. (commit 02ba52b)  
**Status:** Closed.

---

### AUTH-002 — PBKDF2 iteration count was 10,000 [Medium] [Closed]

**Finding:** The original work factor (10,000 iterations) was below the OWASP 2023 minimum recommendation of 600,000 for PBKDF2-HMAC-SHA256.  
**Risk:** Offline brute-force of the dashboard password if NVS is extracted.  
**Remediation:** Raised to 100,000 iterations (constrained by ESP32 @ 240 MHz: ~1 s/login). Notes: 100,000 is the defensible minimum for constrained hardware; it does not meet the full OWASP recommendation.  
**Status:** Closed. Residual risk documented in threat model (NVS-001).

---

### SESS-001 — Session token appeared in serial log [Low] [Closed]

**Finding:** Login success log printed a portion of the session token alongside the client IP.  
**Risk:** Log capture via UART reveals a valid session token.  
**Remediation:** Login success log now prints only client IP. (commit 02ba52b)  
**Status:** Closed.

---

### HTTP-001 — Token accepted via URL query parameter [High] [Closed]

**Finding:** `am_extract_token_from_request()` accepted `?token=` as a valid token source.  
**Risk:** Token logged in server access logs, browser history, and HTTP referer headers.  
**Remediation:** URL parameter extraction removed; only Cookie and Authorization Bearer accepted.  
**Status:** Closed.

---

### STOR-001 — NVS not encrypted [High] [Open — documented limitation]

**Finding:** Credentials in NVS (dashboard password hash, salt, AP password, API key) are readable by an attacker with physical access and esptool.  
**Risk:** Complete credential extraction without authentication.  
**Remediation available:** ESP-IDF CMake builds support NVS + flash encryption. Reference configuration provided in `docs/sdkconfig-nvs-encryption-example.txt`.  
**Status:** Open. Not implementable in PlatformIO Arduino framework. Documented limitation. Users requiring encryption at rest should use the ESP-IDF CMake build path.

---

## Independent Review Status

This audit was performed by the same developer who wrote the implementation. It is **not** an independent security review.

An independent review is listed on the project roadmap (see `docs/roadmap.md`, milestone v4.0). Until it is completed, treat the security claims in this document as self-assessed, not externally verified.

**What this means in practice:**
- The tests pass. The logic reviewed here appears correct.
- An independent reviewer may identify issues the author did not.
- Do not deploy this firmware in environments where security failures have real-world consequences without an independent review.

---

## Methodology

| Area | Method |
|------|--------|
| Authentication logic | Manual code review of `auth_manager.h` |
| Session management | Unit test coverage + manual review |
| Input validation | Unit test coverage (76 tests) + CI static analysis |
| Route guards | CI automated check: Python regex audit of all 16 sensitive routes |
| Token handling | Grep audit: `token`, `apikey`, `Bearer`, `Cookie` extraction paths |
| Storage | Review of NVS read/write paths in `auth_manager.h` |
| Memory safety | AddressSanitizer + UBSan clean (CI) |
| Timing safety | Manual review of `ct_strcmp` implementation |

# Dependency Inventory

This document lists all build-time and runtime dependencies used by Phantom32,
their versions, and any known vulnerability status as of the last manual review.
The CI `dep-scan` job audits Python CI tooling automatically; firmware dependencies
require manual tracking and are listed here.

---

## CI / Build Toolchain (Python)

| Package | Version | Purpose | CVE status |
|---------|---------|---------|-----------|
| `platformio` | 6.1.16 (pinned) | Firmware build system | 16 CVEs in transitive deps (see below) |
| `pip-audit` | latest | Dependency vulnerability scanner | No known CVEs |

### PlatformIO 6.1.16 — Known CVE Notes

PlatformIO 6.1.16 carries 16 known CVEs in its transitive Python dependency tree.
These are build-tool vulnerabilities (not firmware vulnerabilities) in packages
such as `requests`, `urllib3`, and `certifi`. They affect the CI build runner, not
the firmware running on the ESP32.

**Risk assessment**: Low. The CI environment is ephemeral and isolated
(GitHub Actions `ubuntu-latest`). No user data or device credentials pass through the
build toolchain. The CVEs are in HTTP client libraries used for PlatformIO's package
registry — exploiting them would require a MITM on the CI runner's network during
package download, which is mitigated by cache pinning
(`key: pio-${{ matrix.env }}-${{ hashFiles('platformio.ini') }}-v3`).

**Resolution path**: Upgrading PlatformIO requires a full 13-board CI validation
cycle to verify no toolchain regression. This is tracked as a maintenance item.
Interim mitigation: pin the PlatformIO version to avoid unexpected upgrades;
audit pip-audit separately (automated in CI).

---

## ESP-IDF / Arduino Firmware Libraries

Managed by PlatformIO with `espressif32@6.5.0` and
`framework-arduinoespressif32@3.20014.231204 (2.0.14)`.

| Component | Version | Notes |
|-----------|---------|-------|
| ESP-IDF (bundled) | 4.4.x (via arduino 2.0.14) | ESP32 platform SDK |
| `mbedTLS` | 2.28.x (bundled in ESP-IDF) | TLS/crypto; used for PBKDF2 |
| `Arduino-ESP32` | 2.0.14 | Framework layer |
| `WebServer` | bundled | HTTP server |
| `Preferences` | bundled | NVS wrapper |
| `LittleFS` | bundled | Filesystem |

### mbedTLS

The `mbedTLS` library bundled with ESP-IDF 4.4.x is a fork maintained by Espressif
(`components/mbedtls`). Security patches are backported by Espressif; the version
does not track upstream mbedTLS semantic versioning directly.

Phantom32 uses mbedTLS for PBKDF2-HMAC-SHA256 password derivation only
(`mbedtls_pkcs5_pbkdf2_hmac()`). No TLS socket connections are made in the
current release (HTTPS is listed as a future feature for PSRAM boards).

**CVE tracking**: Espressif publishes security advisories at
https://github.com/espressif/esp-idf/security/advisories. Subscribe to that
repository for firmware-level vulnerability notifications.

---

## Third-Party Libraries (PlatformIO registry)

No third-party PlatformIO library dependencies. All functionality is implemented
against the bundled ESP-IDF + Arduino-ESP32 framework APIs.

---

## Generating a Live Dependency Report

To generate a full PlatformIO package manifest for the `doom` environment:

```bash
pio pkg show -e doom 2>/dev/null || pio run -e doom --verbose 2>&1 | grep "packages"
```

The CI pipeline's `dep-scan` job generates a `pip_audit.json` artifact on every
run with the full vulnerability report for CI Python tooling. Download it from the
GitHub Actions run artifacts page.

---

## Unresolved Findings Register

| ID | Package | CVEs | Severity | Mitigation | Status |
|----|---------|------|----------|-----------|--------|
| DEP-001 | platformio 6.1.16 (transitive) | 16 (urllib3, requests, certifi) | Medium | Pinned version; ephemeral CI; no user data in build | Open — awaiting 13-board upgrade cycle |

New findings should be added to this register rather than suppressed in CI.

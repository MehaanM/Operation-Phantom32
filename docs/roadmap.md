# Roadmap — Phantom32

This roadmap reflects planned development direction. Items are not guaranteed to ship on any specific timeline.

---

## v3.2 — Reliability and release quality (current)

Focus: hardware validation, engineering quality, and release maturity.

- [x] Hardware validation on ESP32-WROOM-32D (boot, NVS, AP password)
- [x] PlatformIO 6.2.0 upgrade (DEP-001 resolved)
- [x] Branch coverage ≥ 70% (targeted tests in existing test files)
- [x] Release checklist (`docs/release-checklist.md`)
- [x] v1.0.0 / v3.2.0 tagged release
- [x] CI: SHA-pinned actions, minimal permissions, timeout limits
- [x] CI: Flash budget enforcement (≤ 90%)
- [x] CI: Coverage thresholds enforced (≥ 85% line, ≥ 70% branch)
- [x] CI: SBOM generation (CycloneDX)
- [x] Architecture documentation (`docs/architecture/overview.md`)
- [x] Threat model (`docs/security/threat-model.md`)
- [x] Security audit report (`docs/security/audit-report.md`)
- [x] Memory budget documentation (`docs/engineering/memory-budget.md`)
- [x] Troubleshooting guide (`docs/troubleshooting.md`)
- [x] PR and issue templates
- [ ] GitHub branch protection rules (require CI, require PR)
- [ ] 24-hour stability test on ESP32-WROOM-32D

---

## v3.3 — Engineering quality

Focus: reproducible builds, dependency management, additional hardware validation.

- [ ] Signed Git tags (GPG)
- [ ] GitHub Release with firmware artifacts and SHA-256 checksums
- [ ] SBOM shipped as release artifact (not just CI artifact)
- [ ] Formal dependency update policy (Dependabot or manual schedule)
- [ ] CI: fail on new high/critical CVEs in direct dependencies
- [ ] Hardware validation: ESP32-S3 (PSRAM full mode)
- [ ] Hardware validation: 20-consecutive-boot stability test
- [ ] Hardware validation: reset/recovery test (20 cycles)
- [ ] Hardware validation: 24-hour stability test with heap measurements
- [ ] Architecture: data-flow diagram
- [ ] Architecture: ADR-001 (platform choice rationale)
- [ ] Architecture: ADR-002 (authentication design)
- [ ] Architecture: ADR-003 (memory strategy)
- [ ] Feature maturity classification table in README

---

## v3.4 — Documentation and open-source maturity

Focus: documentation completeness, contribution workflow.

- [ ] API reference documentation (all endpoints, request/response schema)
- [ ] `CONTRIBUTING.md`: development setup from a clean machine
- [ ] `CONTRIBUTING.md`: OS/toolchain compatibility matrix
- [ ] Automated formatting enforcement on PRs
- [ ] Hardware compatibility matrix (which boards are tested vs. compile-only)
- [ ] Responsible use guide for authorized lab environments
- [ ] README: coverage badge, hardware validation badge

---

## v4.0 — Major release

Focus: independent verification and production-grade release process.

- [ ] Independent security review (external reviewer)
- [ ] Security review findings published in `docs/security/audit-report.md`
- [ ] All critical/high findings from independent review resolved
- [ ] Complete hardware validation matrix (all supported boards with physical test results)
- [ ] NVS + flash encryption for ESP-IDF CMake build variant
- [ ] Reproducible build verification (two independent machines produce matching SHA-256)
- [ ] Full PSRAM board validation (ESP32-S3, WROVER)

---

## Not planned

- Cloud connectivity or remote management
- Mobile app
- OTA firmware update (requires secure update channel design; deferred to v4.0 research)
- Support for non-ESP32 platforms

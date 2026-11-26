#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// SecurityAuditLog — structured security event logging and detection (Phase 7).
//
// Categorised events (AUTH_FAILURE, CONFIG_CHANGE, MODULE_ACCESS, …) are
// stored in a fixed-size ring buffer with deduplication. Detection helpers
// examine the buffer to identify patterns that signal active threats, such as
// repeated authentication failures or suspicious configuration changes within
// a sliding time window.

static constexpr size_t PHANTOM_SAL_RING_SIZE = 32;

enum class SecurityCategory : uint8_t {
  AUTH_FAILURE,
  CONFIG_CHANGE,
  MODULE_ACCESS,
  NETWORK_EXPOSURE,
  PRIVILEGE_ESCALATION,
  UNKNOWN
};

enum class SecuritySeverity : uint8_t { INFO, WARNING, CRITICAL };

struct SecurityEvent {
  uint64_t timestampMs;
  SecurityCategory category;
  SecuritySeverity severity;
  char source[24];
  char description[64];
  uint16_t count;
};

class SecurityAuditLog {
 public:
  // --- Logging ---

  void logEvent(SecurityCategory cat, SecuritySeverity sev, const char *source, const char *desc,
                uint64_t nowMs) {
    if (!source || !desc)
      return;
    // Dedup: same category + severity + source → increment most-recent match.
    for (size_t k = 0; k < size_; ++k) {
      SecurityEvent &e = buf_[(head_ + PHANTOM_SAL_RING_SIZE - size_ + k) % PHANTOM_SAL_RING_SIZE];
      if (e.category == cat && e.severity == sev &&
          strncmp(e.source, source, sizeof(e.source)) == 0) {
        ++e.count;
        e.timestampMs = nowMs;
        return;
      }
    }
    SecurityEvent ev = {};
    ev.timestampMs = nowMs;
    ev.category = cat;
    ev.severity = sev;
    ev.count = 1;
    strncpy(ev.source, source, sizeof(ev.source) - 1);
    strncpy(ev.description, desc, sizeof(ev.description) - 1);
    buf_[head_] = ev;
    head_ = (head_ + 1) % PHANTOM_SAL_RING_SIZE;
    if (size_ < PHANTOM_SAL_RING_SIZE)
      ++size_;
  }

  void logAuthFailure(const char *source, uint64_t nowMs) {
    logEvent(SecurityCategory::AUTH_FAILURE, SecuritySeverity::WARNING, source,
             "Authentication failure", nowMs);
  }
  void logConfigChange(const char *key, const char *source, uint64_t nowMs) {
    logEvent(SecurityCategory::CONFIG_CHANGE, SecuritySeverity::INFO, source, key ? key : "unknown",
             nowMs);
  }
  void logModuleAccess(const char *module, const char *source, uint64_t nowMs) {
    logEvent(SecurityCategory::MODULE_ACCESS, SecuritySeverity::INFO, source,
             module ? module : "unknown", nowMs);
  }

  // --- Detection ---

  // Returns true if `source` has AUTH_FAILURE events totalling >= threshold
  // within the last windowMs milliseconds ending at nowMs.
  bool hasRepeatedAuthFailures(const char *source, uint16_t threshold, uint64_t windowMs,
                               uint64_t nowMs) const {
    if (!source || threshold == 0)
      return false;
    uint32_t total = 0;
    for (size_t k = 0; k < size_; ++k) {
      const SecurityEvent &e =
          buf_[(head_ + PHANTOM_SAL_RING_SIZE - size_ + k) % PHANTOM_SAL_RING_SIZE];
      if (e.category != SecurityCategory::AUTH_FAILURE)
        continue;
      if (strncmp(e.source, source, sizeof(e.source)) != 0)
        continue;
      if (nowMs >= windowMs && e.timestampMs < nowMs - windowMs)
        continue;
      total += e.count;
    }
    return total >= threshold;
  }

  // Returns true if `key` has CONFIG_CHANGE events totalling >= threshold
  // within the last windowMs milliseconds ending at nowMs.
  bool hasSuspiciousConfigChanges(const char *key, uint16_t threshold, uint64_t windowMs,
                                  uint64_t nowMs) const {
    if (!key || threshold == 0)
      return false;
    uint32_t total = 0;
    for (size_t k = 0; k < size_; ++k) {
      const SecurityEvent &e =
          buf_[(head_ + PHANTOM_SAL_RING_SIZE - size_ + k) % PHANTOM_SAL_RING_SIZE];
      if (e.category != SecurityCategory::CONFIG_CHANGE)
        continue;
      if (strncmp(e.description, key, sizeof(e.description)) != 0)
        continue;
      if (nowMs >= windowMs && e.timestampMs < nowMs - windowMs)
        continue;
      total += e.count;
    }
    return total >= threshold;
  }

  // --- Ring buffer ---

  size_t eventCount() const { return size_; }

  const SecurityEvent *eventAt(size_t i) const {
    if (i >= size_)
      return nullptr;
    return &buf_[(head_ + PHANTOM_SAL_RING_SIZE - size_ + i) % PHANTOM_SAL_RING_SIZE];
  }

  void clear() {
    size_ = 0;
    head_ = 0;
  }

  // --- Severity summary ---

  uint32_t criticalCount() const { return countBySeverity(SecuritySeverity::CRITICAL); }
  uint32_t warningCount() const { return countBySeverity(SecuritySeverity::WARNING); }

  // --- Report ---

  void generateReport(FILE *out) const {
    if (!out)
      return;
    fprintf(out, "\n  PHANTOM32 SECURITY AUDIT LOG\n");
    fprintf(out, "  ═══════════════════════════════\n");
    fprintf(out, "  Events: %zu  Critical: %u  Warnings: %u\n\n", size_, criticalCount(),
            warningCount());
    for (size_t k = 0; k < size_; ++k) {
      const SecurityEvent *e = eventAt(k);
      if (!e)
        continue;
      fprintf(out, "  [%s] %s — %s (×%u)\n", severityStr(e->severity), e->source, e->description,
              (unsigned)e->count);
    }
    fprintf(out, "  ═══════════════════════════════\n\n");
  }

  void exportJson(char *buf, size_t bufLen) const {
    if (!buf || bufLen == 0)
      return;
    size_t pos = 0;
    auto append = [&](const char *s) {
      while (*s && pos + 1 < bufLen)
        buf[pos++] = *s++;
    };
    append("{\"events\":[");
    for (size_t k = 0; k < size_; ++k) {
      const SecurityEvent *e = eventAt(k);
      if (!e)
        continue;
      if (k > 0)
        append(",");
      char tmp[160];
      snprintf(tmp, sizeof(tmp), "{\"cat\":%u,\"sev\":%u,\"src\":\"%s\",\"count\":%u}",
               (unsigned)e->category, (unsigned)e->severity, e->source, (unsigned)e->count);
      append(tmp);
    }
    append("]}");
    buf[pos] = '\0';
  }

 private:
  SecurityEvent buf_[PHANTOM_SAL_RING_SIZE] = {};
  size_t head_ = 0;
  size_t size_ = 0;

  static const char *severityStr(SecuritySeverity s) {
    switch (s) {
      case SecuritySeverity::CRITICAL:
        return "CRITICAL";
      case SecuritySeverity::WARNING:
        return "WARNING";
      default:
        return "INFO";
    }
  }

  uint32_t countBySeverity(SecuritySeverity sev) const {
    uint32_t n = 0;
    for (size_t k = 0; k < size_; ++k) {
      const SecurityEvent *e = eventAt(k);
      if (e && e->severity == sev)
        n += e->count;
    }
    return n;
  }
};

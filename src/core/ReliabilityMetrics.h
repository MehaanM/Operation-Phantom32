#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// ReliabilityMetrics — MTBF, recovery rate, and uptime tracking per module (Phase 14).
// Records failures, recoveries, and unexpected resets for each registered module.
// MTBF requires at least two failures to be meaningful (returns 0 otherwise).

static constexpr size_t PHANTOM_RM_MAX_MODULES = 16;

struct ModuleReliability {
  char name[32];
  uint64_t uptimeStartMs;
  uint64_t totalUptimeMs;
  uint32_t failureCount;
  uint32_t recoveryCount;
  uint32_t unexpectedResets;
  uint64_t lastFailureMs;
  uint64_t lastRecoveryMs;
  bool running;
  bool valid;
};

class ReliabilityMetrics {
 public:
  bool registerModule(const char *name, uint64_t nowMs) {
    if (!name || !*name)
      return false;
    for (size_t i = 0; i < count_; ++i) {
      if (strcmp(modules_[i].name, name) == 0) {
        modules_[i].uptimeStartMs = nowMs;
        modules_[i].running = true;
        return true;
      }
    }
    if (count_ >= PHANTOM_RM_MAX_MODULES)
      return false;
    strncpy(modules_[count_].name, name, sizeof(modules_[count_].name) - 1);
    modules_[count_].uptimeStartMs = nowMs;
    modules_[count_].running = true;
    modules_[count_].valid = true;
    ++count_;
    return true;
  }

  bool recordFailure(const char *name, uint64_t nowMs) {
    ModuleReliability *m = find(name);
    if (!m)
      return false;
    ++m->failureCount;
    m->lastFailureMs = nowMs;
    if (m->running) {
      m->totalUptimeMs += nowMs - m->uptimeStartMs;
      m->running = false;
    }
    return true;
  }

  bool recordRecovery(const char *name, uint64_t nowMs) {
    ModuleReliability *m = find(name);
    if (!m)
      return false;
    ++m->recoveryCount;
    m->lastRecoveryMs = nowMs;
    m->uptimeStartMs = nowMs;
    m->running = true;
    return true;
  }

  bool recordUnexpectedReset(const char *name) {
    ModuleReliability *m = find(name);
    if (!m)
      return false;
    ++m->unexpectedResets;
    return true;
  }

  bool updateUptime(const char *name, uint64_t nowMs) {
    ModuleReliability *m = find(name);
    if (!m || !m->running)
      return false;
    m->totalUptimeMs += nowMs - m->uptimeStartMs;
    m->uptimeStartMs = nowMs;
    return true;
  }

  uint64_t mtbfMs(const char *name) const {
    const ModuleReliability *m = findConst(name);
    if (!m || m->failureCount < 2)
      return 0;
    return m->totalUptimeMs / (uint64_t)(m->failureCount - 1);
  }

  uint8_t recoveryRatePercent(const char *name) const {
    const ModuleReliability *m = findConst(name);
    if (!m || m->failureCount == 0)
      return 100;
    uint32_t rate = (m->recoveryCount * 100u) / m->failureCount;
    return rate > 100u ? 100u : (uint8_t)rate;
  }

  uint32_t failuresPer100Hours(const char *name) const {
    const ModuleReliability *m = findConst(name);
    if (!m || m->totalUptimeMs == 0)
      return 0;
    return (uint32_t)((uint64_t)m->failureCount * 360000000ULL / m->totalUptimeMs);
  }

  size_t moduleCount() const { return count_; }
  const ModuleReliability *metricsFor(const char *name) const { return findConst(name); }

 private:
  ModuleReliability modules_[PHANTOM_RM_MAX_MODULES] = {};
  size_t count_ = 0;

  ModuleReliability *find(const char *name) {
    if (!name)
      return nullptr;
    for (size_t i = 0; i < count_; ++i) {
      if (strcmp(modules_[i].name, name) == 0)
        return &modules_[i];
    }
    return nullptr;
  }

  const ModuleReliability *findConst(const char *name) const {
    if (!name)
      return nullptr;
    for (size_t i = 0; i < count_; ++i) {
      if (strcmp(modules_[i].name, name) == 0)
        return &modules_[i];
    }
    return nullptr;
  }
};

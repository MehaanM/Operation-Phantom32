#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// FaultInjector — fault injection framework for reliability and chaos testing (Phase 14).
// Schedules simulated hardware and software faults that transition PENDING → ACTIVE
// on the first tick() where nowMs ≥ scheduledMs, and auto-resolve at resolveMs when set.

static constexpr size_t PHANTOM_FI_MAX_FAULTS = 16;
static constexpr size_t PHANTOM_FI_MAX_EVENTS = 32;

enum class FaultType : uint8_t {
  MISSING_PERIPHERAL,
  FAILED_INIT,
  CORRUPTED_CONFIG,
  RESOURCE_EXHAUSTION,
  DELAYED_HARDWARE,
  UNEXPECTED_SHUTDOWN,
  MEMORY_PRESSURE,
  WATCHDOG_RESET
};

enum class FaultState : uint8_t { INACTIVE, PENDING, ACTIVE, RESOLVED };

struct FaultEntry {
  FaultType type;
  FaultState state;
  char target[32];
  char description[64];
  uint64_t scheduledMs;
  uint64_t resolveMs;
  bool autoResolve;
  uint16_t triggerCount;
  bool valid;
};

struct FaultEvent {
  FaultType type;
  char target[32];
  uint64_t timestampMs;
  bool resolved;
};

class FaultInjector {
 public:
  bool addFault(FaultType type, const char *target, const char *description,
                uint64_t scheduledMs = 0, uint64_t resolveMs = 0) {
    if (!target || count_ >= PHANTOM_FI_MAX_FAULTS)
      return false;
    FaultEntry &f = faults_[count_++];
    f.type = type;
    f.state = scheduledMs > 0 ? FaultState::PENDING : FaultState::ACTIVE;
    strncpy(f.target, target, sizeof(f.target) - 1);
    if (description)
      strncpy(f.description, description, sizeof(f.description) - 1);
    f.scheduledMs = scheduledMs;
    f.resolveMs = resolveMs;
    f.autoResolve = resolveMs > 0;
    f.valid = true;
    if (f.state == FaultState::ACTIVE) {
      ++f.triggerCount;
      recordEvent(type, target, scheduledMs, false);
    }
    return true;
  }

  void tick(uint64_t nowMs) {
    for (size_t i = 0; i < count_; ++i) {
      FaultEntry &f = faults_[i];
      if (f.state == FaultState::PENDING && nowMs >= f.scheduledMs) {
        f.state = FaultState::ACTIVE;
        ++f.triggerCount;
        recordEvent(f.type, f.target, nowMs, false);
      }
      if (f.state == FaultState::ACTIVE && f.autoResolve && nowMs >= f.resolveMs) {
        f.state = FaultState::RESOLVED;
        recordEvent(f.type, f.target, nowMs, true);
      }
    }
  }

  bool hasFault(const char *target) const {
    if (!target)
      return false;
    for (size_t i = 0; i < count_; ++i) {
      if (faults_[i].state == FaultState::ACTIVE && strcmp(faults_[i].target, target) == 0)
        return true;
    }
    return false;
  }

  bool hasFaultOfType(const char *target, FaultType type) const {
    if (!target)
      return false;
    for (size_t i = 0; i < count_; ++i) {
      if (faults_[i].state == FaultState::ACTIVE && faults_[i].type == type &&
          strcmp(faults_[i].target, target) == 0)
        return true;
    }
    return false;
  }

  bool resolveFault(const char *target, FaultType type, uint64_t nowMs) {
    if (!target)
      return false;
    for (size_t i = 0; i < count_; ++i) {
      if (faults_[i].state == FaultState::ACTIVE && faults_[i].type == type &&
          strcmp(faults_[i].target, target) == 0) {
        faults_[i].state = FaultState::RESOLVED;
        recordEvent(type, target, nowMs, true);
        return true;
      }
    }
    return false;
  }

  void clear() {
    count_ = 0;
    evtCount_ = 0;
    for (size_t i = 0; i < PHANTOM_FI_MAX_FAULTS; ++i)
      faults_[i] = {};
    for (size_t i = 0; i < PHANTOM_FI_MAX_EVENTS; ++i)
      events_[i] = {};
  }

  size_t activeFaultCount() const {
    size_t n = 0;
    for (size_t i = 0; i < count_; ++i) {
      if (faults_[i].state == FaultState::ACTIVE)
        ++n;
    }
    return n;
  }

  size_t totalFaultCount() const { return count_; }
  const FaultEntry *faultAt(size_t i) const { return i < count_ ? &faults_[i] : nullptr; }
  size_t eventCount() const { return evtCount_; }
  const FaultEvent *eventAt(size_t i) const { return i < evtCount_ ? &events_[i] : nullptr; }

 private:
  FaultEntry faults_[PHANTOM_FI_MAX_FAULTS] = {};
  size_t count_ = 0;
  FaultEvent events_[PHANTOM_FI_MAX_EVENTS] = {};
  size_t evtCount_ = 0;

  void recordEvent(FaultType type, const char *target, uint64_t ts, bool resolved) {
    if (evtCount_ >= PHANTOM_FI_MAX_EVENTS)
      return;
    events_[evtCount_].type = type;
    if (target)
      strncpy(events_[evtCount_].target, target, sizeof(events_[evtCount_].target) - 1);
    events_[evtCount_].timestampMs = ts;
    events_[evtCount_].resolved = resolved;
    ++evtCount_;
  }
};

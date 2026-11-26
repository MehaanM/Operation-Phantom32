#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// RecoveryEngine — safe-mode boot and controlled recovery state machine (Phase 19).
// Tracks active failures, transitions system state, and preserves diagnostics.
// Recovery handlers can be registered per-failure-name; attemptRecovery() invokes
// the handler and only marks a failure resolved if the handler succeeds.

#ifdef PHANTOM32_LITE
static constexpr size_t PHANTOM_RE_MAX_FAILURES = 8;
static constexpr size_t PHANTOM_RE_MAX_SNAPSHOTS = 4;
static constexpr size_t PHANTOM_RE_MAX_HANDLERS = 8;
#else
static constexpr size_t PHANTOM_RE_MAX_FAILURES = 16;
static constexpr size_t PHANTOM_RE_MAX_SNAPSHOTS = 8;
static constexpr size_t PHANTOM_RE_MAX_HANDLERS = 16;
#endif

enum class RecoveryState : uint8_t { NORMAL, DEGRADED, SAFE_MODE, RECOVERING, FAILED };
// Avoid LOW/HIGH — Arduino esp32-hal-gpio.h defines them as 0x0/0x1 macros.
enum class FailureSeverity : uint8_t { SEV_LOW, SEV_MEDIUM, SEV_HIGH, SEV_CRITICAL };

// Handler signature: returns true if recovery succeeded, false if it failed.
using RecoveryHandler = bool (*)(const char *failureName, void *ctx);

struct FailureRecord {
  char name[32];
  char description[64];
  FailureSeverity severity;
  uint64_t timestampMs;
  bool recoverable;
  bool resolved;
  bool valid;
};

struct DiagnosticSnapshot {
  char label[32];
  RecoveryState state;
  size_t activeFailures;
  uint64_t timestampMs;
  bool valid;
};

struct HandlerRecord {
  char name[32];
  RecoveryHandler fn;
  void *ctx;
  bool valid;
};

struct RecoveryEngine {
  bool recordFailure(const char *name, const char *description, FailureSeverity severity,
                     bool recoverable, uint64_t nowMs) {
    if (!name || !name[0])
      return false;
    for (size_t i = 0; i < failCount_; ++i) {
      if (!failures_[i].resolved && strncmp(failures_[i].name, name, 31) == 0) {
        failures_[i].severity = severity;
        failures_[i].recoverable = recoverable;
        failures_[i].timestampMs = nowMs;
        recomputeState();
        return true;
      }
    }
    if (failCount_ >= PHANTOM_RE_MAX_FAILURES)
      return false;
    FailureRecord &r = failures_[failCount_++];
    memset(&r, 0, sizeof(r));
    strncpy(r.name, name, 31);
    if (description)
      strncpy(r.description, description, 63);
    r.severity = severity;
    r.timestampMs = nowMs;
    r.recoverable = recoverable;
    r.resolved = false;
    r.valid = true;
    recomputeState();
    return true;
  }

  // Register a recovery handler for a named failure.
  // handler must not be null. Updates the handler if one is already registered
  // for this name. Returns false on null/empty name or null handler.
  bool registerHandler(const char *name, RecoveryHandler handler, void *ctx = nullptr) {
    if (!name || !name[0] || !handler)
      return false;
    for (size_t i = 0; i < handlerCount_; ++i) {
      if (strncmp(handlers_[i].name, name, 31) == 0) {
        handlers_[i].fn = handler;
        handlers_[i].ctx = ctx;
        return true;
      }
    }
    if (handlerCount_ >= PHANTOM_RE_MAX_HANDLERS)
      return false;
    HandlerRecord &h = handlers_[handlerCount_++];
    memset(&h, 0, sizeof(h));
    strncpy(h.name, name, 31);
    h.fn = handler;
    h.ctx = ctx;
    h.valid = true;
    return true;
  }

  RecoveryState currentState() const { return state_; }

  // If a handler is registered for this failure, it is invoked first.
  // The failure is only marked resolved if no handler is registered,
  // or if the registered handler returns true.
  bool attemptRecovery(const char *failureName, uint64_t nowMs) {
    if (!failureName)
      return false;
    for (size_t i = 0; i < failCount_; ++i) {
      if (!failures_[i].resolved && strncmp(failures_[i].name, failureName, 31) == 0) {
        // Look up handler
        RecoveryHandler fn = nullptr;
        void *ctx = nullptr;
        for (size_t h = 0; h < handlerCount_; ++h) {
          if (strncmp(handlers_[h].name, failureName, 31) == 0) {
            fn = handlers_[h].fn;
            ctx = handlers_[h].ctx;
            break;
          }
        }
        // Handler registered: must succeed to resolve
        if (fn && !fn(failureName, ctx))
          return false;
        failures_[i].resolved = true;
        failures_[i].timestampMs = nowMs;
        recomputeState();
        return true;
      }
    }
    return false;
  }

  void enterSafeMode(uint64_t nowMs) {
    (void)nowMs;
    state_ = RecoveryState::SAFE_MODE;
  }

  bool saveSnapshot(const char *label, uint64_t nowMs) {
    if (!label || !label[0])
      return false;
    for (size_t i = 0; i < snapCount_; ++i) {
      if (strncmp(snaps_[i].label, label, 31) == 0) {
        fillSnap(snaps_[i], label, nowMs);
        return true;
      }
    }
    if (snapCount_ >= PHANTOM_RE_MAX_SNAPSHOTS)
      return false;
    fillSnap(snaps_[snapCount_++], label, nowMs);
    return true;
  }

  size_t activeFailureCount() const {
    size_t n = 0;
    for (size_t i = 0; i < failCount_; ++i)
      if (!failures_[i].resolved)
        ++n;
    return n;
  }

  bool hasRecoverable() const {
    for (size_t i = 0; i < failCount_; ++i)
      if (!failures_[i].resolved && failures_[i].recoverable)
        return true;
    return false;
  }

  const FailureRecord *failureAt(size_t i) const {
    return (i < failCount_) ? &failures_[i] : nullptr;
  }

  const DiagnosticSnapshot *snapshotFor(const char *label) const {
    if (!label)
      return nullptr;
    for (size_t i = 0; i < snapCount_; ++i)
      if (strncmp(snaps_[i].label, label, 31) == 0)
        return &snaps_[i];
    return nullptr;
  }

  size_t totalFailureCount() const { return failCount_; }
  size_t handlerCount() const { return handlerCount_; }

 private:
  FailureRecord failures_[PHANTOM_RE_MAX_FAILURES] = {};
  DiagnosticSnapshot snaps_[PHANTOM_RE_MAX_SNAPSHOTS] = {};
  HandlerRecord handlers_[PHANTOM_RE_MAX_HANDLERS] = {};
  size_t failCount_ = 0;
  size_t snapCount_ = 0;
  size_t handlerCount_ = 0;
  RecoveryState state_ = RecoveryState::NORMAL;

  void recomputeState() {
    bool hasCritical = false, hasHigh = false, hasMedium = false;
    for (size_t i = 0; i < failCount_; ++i) {
      if (failures_[i].resolved)
        continue;
      if (failures_[i].severity == FailureSeverity::SEV_CRITICAL)
        hasCritical = true;
      if (failures_[i].severity == FailureSeverity::SEV_HIGH)
        hasHigh = true;
      if (failures_[i].severity == FailureSeverity::SEV_MEDIUM)
        hasMedium = true;
    }
    if (hasCritical)
      state_ = RecoveryState::FAILED;
    else if (hasHigh)
      state_ = RecoveryState::SAFE_MODE;
    else if (hasMedium)
      state_ = RecoveryState::DEGRADED;
    else if (activeFailureCount() > 0)
      state_ = RecoveryState::DEGRADED;
    else
      state_ = RecoveryState::NORMAL;
  }

  void fillSnap(DiagnosticSnapshot &s, const char *label, uint64_t nowMs) const {
    memset(&s, 0, sizeof(s));
    strncpy(s.label, label, 31);
    s.state = state_;
    s.activeFailures = activeFailureCount();
    s.timestampMs = nowMs;
    s.valid = true;
  }
};

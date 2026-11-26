#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// TimingMonitor — per-module CPU execution time tracking.
//
// Records the wall-clock time each module's update() takes and maintains min,
// max, and running average (using integer arithmetic). The worst-case execution
// time (WCET) is the max sample ever seen.
//
// Usage:
//   TimingMonitor tm;
//   tm.registerModule("WiFiScanner");
//   ...
//   uint32_t start = nowMs();
//   module.update();
//   tm.record("WiFiScanner", nowMs() - start);
//   ...
//   auto stats = tm.statsFor("WiFiScanner");
//   if (stats && stats->wcetMs > budget) { /* deadline violation */ }
//
// All operations are O(N) in the number of registered modules; suitable for
// single-core bare-metal firmware where N ≤ 16.

static constexpr size_t PHANTOM_TM_MAX_MODULES = 16;

struct TimingStats {
  char name[32] = {};
  uint32_t sampleCount = 0;
  uint32_t minMs = UINT32_MAX;  // UINT32_MAX until first sample
  uint32_t maxMs = 0;           // = WCET
  uint32_t sumMs = 0;           // sum of all samples (wraps after ~49 days * 65536 modules)
  uint32_t budgetMs = 0;        // 0 = no limit
  uint32_t violations = 0;      // samples that exceeded budgetMs
};

class TimingMonitor {
 public:
  // Register a module for timing. If already registered, updates budget only.
  // Returns false if full, name is null, or count would overflow.
  bool registerModule(const char *name, uint32_t budgetMs = 0) {
    if (!name || count_ >= PHANTOM_TM_MAX_MODULES)
      return false;
    TimingStats *e = find(name);
    if (e) {
      e->budgetMs = budgetMs;
      return true;
    }
    TimingStats &slot = entries_[count_++];
    strncpy(slot.name, name, sizeof(slot.name) - 1);
    slot.name[sizeof(slot.name) - 1] = '\0';
    slot.sampleCount = 0;
    slot.minMs = UINT32_MAX;
    slot.maxMs = 0;
    slot.sumMs = 0;
    slot.budgetMs = budgetMs;
    slot.violations = 0;
    return true;
  }

  // Record one timing sample for a named module.
  // Returns false if name is not registered.
  bool record(const char *name, uint32_t elapsedMs) {
    TimingStats *e = find(name);
    if (!e)
      return false;
    ++e->sampleCount;
    if (elapsedMs < e->minMs)
      e->minMs = elapsedMs;
    if (elapsedMs > e->maxMs)
      e->maxMs = elapsedMs;
    // Saturating add to prevent overflow flipping the sum negative.
    if (e->sumMs + elapsedMs >= e->sumMs)
      e->sumMs += elapsedMs;
    if (e->budgetMs > 0 && elapsedMs > e->budgetMs)
      ++e->violations;
    return true;
  }

  // Returns null if not registered or no samples yet.
  const TimingStats *statsFor(const char *name) const {
    const TimingStats *e = findConst(name);
    if (!e || e->sampleCount == 0)
      return nullptr;
    return e;
  }

  // Average execution time (integer, rounds down). Returns 0 if no samples.
  uint32_t avgMs(const char *name) const {
    const TimingStats *e = findConst(name);
    if (!e || e->sampleCount == 0)
      return 0;
    return e->sumMs / e->sampleCount;
  }

  // Worst-case execution time. Returns 0 if no samples.
  uint32_t wcetMs(const char *name) const {
    const TimingStats *e = findConst(name);
    if (!e || e->sampleCount == 0)
      return 0;
    return e->maxMs;
  }

  // Returns true if the WCET exceeds the registered budget (0 budget = never).
  bool isOverBudget(const char *name) const {
    const TimingStats *e = findConst(name);
    if (!e || e->budgetMs == 0 || e->sampleCount == 0)
      return false;
    return e->maxMs > e->budgetMs;
  }

  // Reset timing statistics for a named module without unregistering it.
  bool reset(const char *name) {
    TimingStats *e = find(name);
    if (!e)
      return false;
    e->sampleCount = 0;
    e->minMs = UINT32_MAX;
    e->maxMs = 0;
    e->sumMs = 0;
    e->violations = 0;
    return true;
  }

  size_t moduleCount() const { return count_; }

 private:
  TimingStats *find(const char *name) {
    for (size_t i = 0; i < count_; ++i)
      if (strncmp(entries_[i].name, name, sizeof(entries_[i].name)) == 0)
        return &entries_[i];
    return nullptr;
  }
  const TimingStats *findConst(const char *name) const {
    for (size_t i = 0; i < count_; ++i)
      if (strncmp(entries_[i].name, name, sizeof(entries_[i].name)) == 0)
        return &entries_[i];
    return nullptr;
  }

  TimingStats entries_[PHANTOM_TM_MAX_MODULES] = {};
  size_t count_ = 0;
};

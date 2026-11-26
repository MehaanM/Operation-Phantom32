#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// BinaryBudget — flash/RAM budget tracking with regression detection (Phase 12).
// Entries record hard budget limits and warning thresholds for named resource
// categories (e.g. flash_total, heap_min). Snapshots capture a labeled baseline
// so regressions can be measured against a known good build.

static constexpr size_t PHANTOM_BB_MAX_SECTIONS = 16;
static constexpr size_t PHANTOM_BB_MAX_HISTORY = 8;

struct BudgetEntry {
  char name[32];
  uint32_t budget;
  uint32_t warningLevel;
  uint32_t current;
  bool valid;
};

struct BudgetSnapshot {
  char label[32];
  uint64_t timestampMs;
  uint32_t values[PHANTOM_BB_MAX_SECTIONS];
  size_t entryCount;
  bool valid;
};

class BinaryBudget {
 public:
  bool addEntry(const char *name, uint32_t budget, uint32_t warningLevel) {
    if (!name || !*name)
      return false;
    for (size_t i = 0; i < count_; ++i) {
      if (strcmp(entries_[i].name, name) == 0) {
        entries_[i].budget = budget;
        entries_[i].warningLevel = warningLevel;
        return true;
      }
    }
    if (count_ >= PHANTOM_BB_MAX_SECTIONS)
      return false;
    strncpy(entries_[count_].name, name, sizeof(entries_[count_].name) - 1);
    entries_[count_].budget = budget;
    entries_[count_].warningLevel = warningLevel;
    entries_[count_].current = 0;
    entries_[count_].valid = true;
    ++count_;
    return true;
  }

  bool update(const char *name, uint32_t currentValue) {
    BudgetEntry *e = findEntry(name);
    if (!e)
      return false;
    e->current = currentValue;
    return true;
  }

  bool allWithinBudget() const {
    for (size_t i = 0; i < count_; ++i) {
      if (entries_[i].current > entries_[i].budget)
        return false;
    }
    return true;
  }

  bool isOverBudget(const char *name) const {
    const BudgetEntry *e = entryFor(name);
    return e && e->current > e->budget;
  }

  bool isNearBudget(const char *name) const {
    const BudgetEntry *e = entryFor(name);
    return e && e->current >= e->warningLevel;
  }

  uint8_t percentUsed(const char *name) const {
    const BudgetEntry *e = entryFor(name);
    if (!e || e->budget == 0)
      return 0;
    uint32_t pct = (e->current * 100u) / e->budget;
    return pct > 100u ? 100u : (uint8_t)pct;
  }

  bool saveSnapshot(const char *label, uint64_t timestampMs) {
    if (!label || !*label)
      return false;
    for (size_t i = 0; i < snapCount_; ++i) {
      if (strcmp(snapshots_[i].label, label) == 0) {
        snapshots_[i].timestampMs = timestampMs;
        for (size_t j = 0; j < count_; ++j)
          snapshots_[i].values[j] = entries_[j].current;
        snapshots_[i].entryCount = count_;
        return true;
      }
    }
    if (snapCount_ >= PHANTOM_BB_MAX_HISTORY)
      return false;
    strncpy(snapshots_[snapCount_].label, label, sizeof(snapshots_[snapCount_].label) - 1);
    snapshots_[snapCount_].timestampMs = timestampMs;
    for (size_t j = 0; j < count_; ++j)
      snapshots_[snapCount_].values[j] = entries_[j].current;
    snapshots_[snapCount_].entryCount = count_;
    snapshots_[snapCount_].valid = true;
    ++snapCount_;
    return true;
  }

  // Positive = current is larger (regression), negative = current is smaller (improvement).
  int32_t regressionVs(const char *snapshotLabel, const char *entryName) const {
    const BudgetSnapshot *snap = findSnapshot(snapshotLabel);
    if (!snap || !entryName)
      return 0;
    for (size_t i = 0; i < count_ && i < snap->entryCount; ++i) {
      if (strcmp(entries_[i].name, entryName) == 0) {
        return (int32_t)entries_[i].current - (int32_t)snap->values[i];
      }
    }
    return 0;
  }

  size_t entryCount() const { return count_; }
  const BudgetEntry *entryFor(const char *name) const {
    if (!name)
      return nullptr;
    for (size_t i = 0; i < count_; ++i) {
      if (strcmp(entries_[i].name, name) == 0)
        return &entries_[i];
    }
    return nullptr;
  }
  size_t snapshotCount() const { return snapCount_; }
  const BudgetSnapshot *snapshotFor(const char *label) const { return findSnapshot(label); }

 private:
  BudgetEntry entries_[PHANTOM_BB_MAX_SECTIONS] = {};
  size_t count_ = 0;
  BudgetSnapshot snapshots_[PHANTOM_BB_MAX_HISTORY] = {};
  size_t snapCount_ = 0;

  BudgetEntry *findEntry(const char *name) {
    if (!name)
      return nullptr;
    for (size_t i = 0; i < count_; ++i) {
      if (strcmp(entries_[i].name, name) == 0)
        return &entries_[i];
    }
    return nullptr;
  }

  const BudgetSnapshot *findSnapshot(const char *label) const {
    if (!label)
      return nullptr;
    for (size_t i = 0; i < snapCount_; ++i) {
      if (strcmp(snapshots_[i].label, label) == 0)
        return &snapshots_[i];
    }
    return nullptr;
  }
};

#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// ObservabilityDashboard — unified interface for logs, health, resources,
// test results, diagnostics, and module status (Phase 19).

#ifndef PHANTOM32_NATIVE_TEST
#define PHANTOM32_NATIVE_TEST
#endif

static constexpr size_t PHANTOM_OD_MAX_ENTRIES = 32;
static constexpr size_t PHANTOM_OD_MAX_MODULES = 16;

enum class DashboardEntryType : uint8_t {
  LOG,
  HEALTH_CHECK,
  RESOURCE_METRIC,
  TEST_RESULT,
  DIAGNOSTIC,
  MODULE_STATUS
};

enum class EntryLevel : uint8_t { DEBUG, INFO, WARN, ERROR, CRITICAL };

struct DashboardEntry {
  DashboardEntryType type;
  EntryLevel level;
  char source[32];
  char message[80];
  uint64_t timestampMs;
  bool valid;
};

struct ModuleStatusEntry {
  char name[32];
  char statusMsg[48];
  uint64_t lastUpdateMs;
  bool healthy;
  bool valid;
};

struct ObservabilityDashboard {
  bool addEntry(DashboardEntryType type, EntryLevel level, const char *source, const char *message,
                uint64_t nowMs) {
    size_t slot = head_ % PHANTOM_OD_MAX_ENTRIES;
    DashboardEntry &e = entries_[slot];
    memset(&e, 0, sizeof(e));
    e.type = type;
    e.level = level;
    if (source)
      strncpy(e.source, source, 31);
    if (message)
      strncpy(e.message, message, 79);
    e.timestampMs = nowMs;
    e.valid = true;
    ++head_;
    if (count_ < PHANTOM_OD_MAX_ENTRIES)
      ++count_;
    return true;
  }

  bool updateModuleStatus(const char *name, bool healthy, const char *statusMsg, uint64_t nowMs) {
    if (!name || !name[0])
      return false;
    for (size_t i = 0; i < modCount_; ++i) {
      if (strncmp(mods_[i].name, name, 31) == 0) {
        mods_[i].healthy = healthy;
        mods_[i].lastUpdateMs = nowMs;
        if (statusMsg)
          strncpy(mods_[i].statusMsg, statusMsg, 47);
        return true;
      }
    }
    if (modCount_ >= PHANTOM_OD_MAX_MODULES)
      return false;
    ModuleStatusEntry &m = mods_[modCount_++];
    memset(&m, 0, sizeof(m));
    strncpy(m.name, name, 31);
    m.healthy = healthy;
    m.lastUpdateMs = nowMs;
    if (statusMsg)
      strncpy(m.statusMsg, statusMsg, 47);
    m.valid = true;
    return true;
  }

  size_t countByLevel(EntryLevel level) const {
    size_t n = 0;
    for (size_t i = 0; i < count_; ++i)
      if (entries_[i].valid && entries_[i].level == level)
        ++n;
    return n;
  }

  size_t countByType(DashboardEntryType type) const {
    size_t n = 0;
    for (size_t i = 0; i < count_; ++i)
      if (entries_[i].valid && entries_[i].type == type)
        ++n;
    return n;
  }

  bool hasErrors() const {
    for (size_t i = 0; i < count_; ++i) {
      if (entries_[i].valid &&
          (entries_[i].level == EntryLevel::ERROR || entries_[i].level == EntryLevel::CRITICAL))
        return true;
    }
    return false;
  }

  const DashboardEntry *entryAt(size_t i) const {
    if (i >= count_)
      return nullptr;
    return &entries_[i % PHANTOM_OD_MAX_ENTRIES];
  }

  const ModuleStatusEntry *moduleStatusFor(const char *name) const {
    if (!name)
      return nullptr;
    for (size_t i = 0; i < modCount_; ++i)
      if (strncmp(mods_[i].name, name, 31) == 0)
        return &mods_[i];
    return nullptr;
  }

  size_t entryCount() const { return count_; }
  size_t moduleCount() const { return modCount_; }

  void generateReport(FILE *out) const {
    if (!out)
      return;
    fprintf(out, "Observability Dashboard\n");
    fprintf(out, "  Entries: %u  Modules: %u\n", (unsigned)count_, (unsigned)modCount_);
    for (size_t i = 0; i < modCount_; ++i) {
      fprintf(out, "  [%s] %s: %s\n", mods_[i].healthy ? "OK" : "FAIL", mods_[i].name,
              mods_[i].statusMsg);
    }
    for (size_t i = 0; i < count_; ++i) {
      if (!entries_[i].valid)
        continue;
      const char *lvl = "DEBUG";
      if (entries_[i].level == EntryLevel::INFO)
        lvl = "INFO";
      else if (entries_[i].level == EntryLevel::WARN)
        lvl = "WARN";
      else if (entries_[i].level == EntryLevel::ERROR)
        lvl = "ERROR";
      else if (entries_[i].level == EntryLevel::CRITICAL)
        lvl = "CRITICAL";
      fprintf(out, "  [%s] %s: %s\n", lvl, entries_[i].source, entries_[i].message);
    }
  }

 private:
  DashboardEntry entries_[PHANTOM_OD_MAX_ENTRIES] = {};
  ModuleStatusEntry mods_[PHANTOM_OD_MAX_MODULES] = {};
  size_t head_ = 0;
  size_t count_ = 0;
  size_t modCount_ = 0;
};

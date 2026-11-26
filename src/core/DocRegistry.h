#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// DocRegistry — code-driven API documentation and troubleshooting database (Phase 17).
// Keeps structured APIDocEntry records per symbol, TroubleshootingEntry records per error
// code, and CompatibilityEntry records per feature×board. generateReport() emits a
// human-readable summary useful for onboarding documentation.

static constexpr size_t PHANTOM_DR_MAX_ENTRIES = 32;
static constexpr size_t PHANTOM_DR_MAX_TROUBLE = 16;
static constexpr size_t PHANTOM_DR_MAX_COMPAT = 16;

struct APIDocEntry {
  char symbol[48];
  char module[32];
  char description[128];
  char returns[64];
  char since[16];
  bool isPublic;
  bool isDeprecated;
  char deprecatedSince[16];
  bool valid;
};

struct TroubleshootingEntry {
  char code[16];
  char symptom[80];
  char cause[80];
  char resolution[128];
  char since[16];
  bool valid;
};

struct CompatibilityEntry {
  char feature[48];
  char boardPattern[32];
  uint8_t level;  // 0=unsupported, 1=compile-only, 2=tested, 3=validated
  bool valid;
};

class DocRegistry {
 public:
  bool addAPIEntry(const APIDocEntry &entry) {
    if (!entry.valid || !entry.symbol[0])
      return false;
    for (size_t i = 0; i < apiCount_; ++i) {
      if (strcmp(apiEntries_[i].symbol, entry.symbol) == 0) {
        apiEntries_[i] = entry;
        return true;
      }
    }
    if (apiCount_ >= PHANTOM_DR_MAX_ENTRIES)
      return false;
    apiEntries_[apiCount_++] = entry;
    return true;
  }

  const APIDocEntry *docFor(const char *symbol) const {
    if (!symbol)
      return nullptr;
    for (size_t i = 0; i < apiCount_; ++i) {
      if (strcmp(apiEntries_[i].symbol, symbol) == 0)
        return &apiEntries_[i];
    }
    return nullptr;
  }

  bool addTroubleshootingEntry(const TroubleshootingEntry &entry) {
    if (!entry.valid || !entry.code[0])
      return false;
    for (size_t i = 0; i < troubleCount_; ++i) {
      if (strcmp(troubles_[i].code, entry.code) == 0) {
        troubles_[i] = entry;
        return true;
      }
    }
    if (troubleCount_ >= PHANTOM_DR_MAX_TROUBLE)
      return false;
    troubles_[troubleCount_++] = entry;
    return true;
  }

  const TroubleshootingEntry *troubleFor(const char *code) const {
    if (!code)
      return nullptr;
    for (size_t i = 0; i < troubleCount_; ++i) {
      if (strcmp(troubles_[i].code, code) == 0)
        return &troubles_[i];
    }
    return nullptr;
  }

  bool addCompatibilityEntry(const CompatibilityEntry &entry) {
    if (!entry.valid || !entry.feature[0])
      return false;
    if (compatCount_ >= PHANTOM_DR_MAX_COMPAT)
      return false;
    compatEntries_[compatCount_++] = entry;
    return true;
  }

  size_t apiEntryCount() const { return apiCount_; }
  size_t troubleshootingCount() const { return troubleCount_; }
  size_t compatibilityCount() const { return compatCount_; }

  bool isDocumented(const char *symbol) const { return docFor(symbol) != nullptr; }

  size_t undocumentedPublicCount() const {
    size_t n = 0;
    for (size_t i = 0; i < apiCount_; ++i) {
      if (apiEntries_[i].isPublic && apiEntries_[i].description[0] == '\0')
        ++n;
    }
    return n;
  }

  void generateReport(FILE *out) const {
    if (!out)
      return;
    fprintf(out, "=== API Documentation Report ===\n");
    fprintf(out, "Entries: %zu\n", apiCount_);
    for (size_t i = 0; i < apiCount_; ++i) {
      const APIDocEntry &e = apiEntries_[i];
      fprintf(out, "[%s] %s: %s\n",
              e.isDeprecated ? "DEPRECATED" : (e.isPublic ? "PUBLIC" : "INTERNAL"), e.symbol,
              e.description);
    }
    if (troubleCount_ > 0) {
      fprintf(out, "\n=== Troubleshooting ===\n");
      for (size_t i = 0; i < troubleCount_; ++i) {
        fprintf(out, "[%s] %s\n", troubles_[i].code, troubles_[i].symptom);
      }
    }
  }

 private:
  APIDocEntry apiEntries_[PHANTOM_DR_MAX_ENTRIES] = {};
  size_t apiCount_ = 0;
  TroubleshootingEntry troubles_[PHANTOM_DR_MAX_TROUBLE] = {};
  size_t troubleCount_ = 0;
  CompatibilityEntry compatEntries_[PHANTOM_DR_MAX_COMPAT] = {};
  size_t compatCount_ = 0;
};

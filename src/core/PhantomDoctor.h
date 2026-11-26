#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// PhantomDoctor — automated firmware diagnosis report (Phase 19).
// Registers named checks, records their results, and generates a report.

#ifndef PHANTOM32_NATIVE_TEST
#define PHANTOM32_NATIVE_TEST
#endif

static constexpr size_t PHANTOM_PD_MAX_CHECKS = 16;
static constexpr size_t PHANTOM_PD_MAX_WARNINGS = 32;

enum class CheckStatus : uint8_t { PENDING, PASS, WARN, FAIL, SKIPPED };

struct DiagnosticCheck {
  char name[32];
  char description[64];
  char detail[64];
  CheckStatus status;
  bool valid;
};

struct PhantomDoctor {
  bool registerCheck(const char *name, const char *description) {
    if (!name || !name[0])
      return false;
    for (size_t i = 0; i < checkCount_; ++i) {
      if (strncmp(checks_[i].name, name, 31) == 0)
        return true;
    }
    if (checkCount_ >= PHANTOM_PD_MAX_CHECKS)
      return false;
    DiagnosticCheck &c = checks_[checkCount_++];
    memset(&c, 0, sizeof(c));
    strncpy(c.name, name, 31);
    if (description)
      strncpy(c.description, description, 63);
    c.status = CheckStatus::PENDING;
    c.valid = true;
    return true;
  }

  bool setCheckResult(const char *name, CheckStatus status, const char *detail) {
    if (!name)
      return false;
    for (size_t i = 0; i < checkCount_; ++i) {
      if (strncmp(checks_[i].name, name, 31) == 0) {
        checks_[i].status = status;
        if (detail)
          strncpy(checks_[i].detail, detail, 63);
        return true;
      }
    }
    return false;
  }

  bool addWarning(const char *warning) {
    if (!warning || !warning[0])
      return false;
    if (warnCount_ >= PHANTOM_PD_MAX_WARNINGS)
      return false;
    strncpy(warnings_[warnCount_], warning, 63);
    warnings_[warnCount_][63] = '\0';
    ++warnCount_;
    return true;
  }

  bool finalise(uint64_t timestampMs) {
    if (complete_)
      return false;
    complete_ = true;
    timestampMs_ = timestampMs;
    return true;
  }

  size_t passCount() const {
    size_t n = 0;
    for (size_t i = 0; i < checkCount_; ++i)
      if (checks_[i].status == CheckStatus::PASS)
        ++n;
    return n;
  }

  size_t failCount() const {
    size_t n = 0;
    for (size_t i = 0; i < checkCount_; ++i)
      if (checks_[i].status == CheckStatus::FAIL)
        ++n;
    return n;
  }

  size_t warnCount() const {
    size_t n = 0;
    for (size_t i = 0; i < checkCount_; ++i)
      if (checks_[i].status == CheckStatus::WARN)
        ++n;
    return n;
  }

  bool allPassed() const {
    for (size_t i = 0; i < checkCount_; ++i)
      if (checks_[i].status == CheckStatus::FAIL)
        return false;
    return true;
  }

  bool isComplete() const { return complete_; }
  size_t checkCount() const { return checkCount_; }
  size_t warningCount() const { return warnCount_; }

  const DiagnosticCheck *checkFor(const char *name) const {
    if (!name)
      return nullptr;
    for (size_t i = 0; i < checkCount_; ++i)
      if (strncmp(checks_[i].name, name, 31) == 0)
        return &checks_[i];
    return nullptr;
  }

  void generateReport(FILE *out) const {
    if (!out)
      return;
    fprintf(out, "PHANTOM DOCTOR\n\n");
    for (size_t i = 0; i < checkCount_; ++i) {
      const char *tag = "PENDING";
      if (checks_[i].status == CheckStatus::PASS)
        tag = "PASS";
      else if (checks_[i].status == CheckStatus::FAIL)
        tag = "FAIL";
      else if (checks_[i].status == CheckStatus::WARN)
        tag = "WARN";
      else if (checks_[i].status == CheckStatus::SKIPPED)
        tag = "SKIPPED";
      fprintf(out, "  %-30s %s\n", checks_[i].name, tag);
    }
    if (warnCount_ > 0) {
      fprintf(out, "\nWarnings:\n");
      for (size_t i = 0; i < warnCount_; ++i)
        fprintf(out, "  %s\n", warnings_[i]);
    }
    if (complete_)
      fprintf(out, "\nReport generated at t=%llu ms.\n", (unsigned long long)timestampMs_);
  }

 private:
  DiagnosticCheck checks_[PHANTOM_PD_MAX_CHECKS] = {};
  char warnings_[PHANTOM_PD_MAX_WARNINGS][64] = {};
  size_t checkCount_ = 0;
  size_t warnCount_ = 0;
  bool complete_ = false;
  uint64_t timestampMs_ = 0;
};

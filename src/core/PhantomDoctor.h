#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// PhantomDoctor — automated firmware diagnosis report (Phase 19).
// Registers named checks, records their results, and generates a report.
// overallStatus() reflects PASS/WARN/FAIL/INCOMPLETE — PENDING checks or an
// unfinalized session always yield INCOMPLETE. allPassed() is strict: any
// PENDING or FAIL returns false.

static constexpr size_t PHANTOM_PD_MAX_CHECKS = 16;
static constexpr size_t PHANTOM_PD_MAX_WARNINGS = 32;

enum class CheckStatus : uint8_t { PENDING, PASS, WARN, FAIL, SKIPPED };

// Overall diagnostic session result.
enum class DiagStatus : uint8_t { PASS, WARN, FAIL, INCOMPLETE };

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

  size_t pendingCount() const {
    size_t n = 0;
    for (size_t i = 0; i < checkCount_; ++i)
      if (checks_[i].status == CheckStatus::PENDING)
        ++n;
    return n;
  }

  // Returns false if any check is FAIL or PENDING (incomplete diagnostic).
  bool allPassed() const {
    for (size_t i = 0; i < checkCount_; ++i)
      if (checks_[i].status == CheckStatus::FAIL || checks_[i].status == CheckStatus::PENDING)
        return false;
    return true;
  }

  // Strict overall status:
  //   INCOMPLETE — not finalized, or any check still PENDING
  //   FAIL       — any check is FAIL
  //   WARN       — any check is WARN, or any string warnings were added
  //   PASS       — all checks PASS or SKIPPED
  DiagStatus overallStatus() const {
    if (!complete_ || pendingCount() > 0)
      return DiagStatus::INCOMPLETE;
    for (size_t i = 0; i < checkCount_; ++i)
      if (checks_[i].status == CheckStatus::FAIL)
        return DiagStatus::FAIL;
    for (size_t i = 0; i < checkCount_; ++i)
      if (checks_[i].status == CheckStatus::WARN)
        return DiagStatus::WARN;
    if (warnCount_ > 0)
      return DiagStatus::WARN;
    return DiagStatus::PASS;
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
    const char *statusStr = "INCOMPLETE";
    DiagStatus ds = overallStatus();
    if (ds == DiagStatus::PASS)
      statusStr = "PASS";
    else if (ds == DiagStatus::WARN)
      statusStr = "WARN";
    else if (ds == DiagStatus::FAIL)
      statusStr = "FAIL";
    fprintf(out, "PHANTOM DOCTOR  [%s]\n\n", statusStr);
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
      fprintf(out, "  %-30s %s", checks_[i].name, tag);
      if (checks_[i].detail[0])
        fprintf(out, "  (%s)", checks_[i].detail);
      fputc('\n', out);
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

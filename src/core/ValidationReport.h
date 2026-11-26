#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// ValidationReport — categorized engineering self-validation framework.
//
// Produces a structured PHANTOM32 ENGINEERING VALIDATION report from named
// test functions. Tests are grouped into categories; each returns PASS, FAIL,
// or SKIP. The overall status is RELEASE QUALIFIED only when every non-skipped
// test passes.
//
// Usage:
//   ValidationReport rpt;
//   rpt.add("Architecture", "Module dependency validation", [] { return true; });
//   rpt.add("Architecture", "Cycle detection",             [] { return true; });
//   rpt.add("Memory",       "Leak detection",              [] { return false; });
//   rpt.run(stdout);
//
// Output:
//   PHANTOM32 ENGINEERING VALIDATION
//   ══════════════════════════════════════
//
//   Architecture
//     Module dependency validation     PASS
//     Cycle detection                  PASS
//
//   Memory
//     Leak detection                   FAIL
//
//   ══════════════════════════════════════
//   Passed: 2  Failed: 1  Skipped: 0
//   Overall status: FAIL

static constexpr size_t PHANTOM_VR_MAX_CHECKS = 64;
static constexpr size_t PHANTOM_VR_MAX_CATEGORY = 24;
static constexpr size_t PHANTOM_VR_MAX_NAME = 48;

typedef bool (*VRCheckFn)();

class ValidationReport {
 public:
  enum class Result : uint8_t { PASS, FAIL, SKIP };

  struct Check {
    char category[PHANTOM_VR_MAX_CATEGORY] = {};
    char name[PHANTOM_VR_MAX_NAME] = {};
    VRCheckFn fn = nullptr;
    Result result = Result::SKIP;
    bool ran = false;
  };

  // Register a check under a named category.
  // Returns false if full or any argument is null.
  bool add(const char *category, const char *name, VRCheckFn fn) {
    if (!category || !name || !fn || count_ >= PHANTOM_VR_MAX_CHECKS)
      return false;
    Check &c = checks_[count_++];
    strncpy(c.category, category, sizeof(c.category) - 1);
    strncpy(c.name, name, sizeof(c.name) - 1);
    c.fn = fn;
    c.result = Result::SKIP;
    c.ran = false;
    return true;
  }

  // Add a check that is permanently skipped (use for unimplemented platform checks).
  bool addSkip(const char *category, const char *name) {
    if (!category || !name || count_ >= PHANTOM_VR_MAX_CHECKS)
      return false;
    Check &c = checks_[count_++];
    strncpy(c.category, category, sizeof(c.category) - 1);
    strncpy(c.name, name, sizeof(c.name) - 1);
    c.fn = nullptr;
    c.result = Result::SKIP;
    c.ran = true;
    return true;
  }

  // Run all registered checks and print the report to `out` (default: stdout).
  // Returns true if every non-skipped check passed.
  bool run(FILE *out = stdout) {
    passed_ = 0;
    failed_ = 0;
    skipped_ = 0;

    // Execute all checks.
    for (size_t i = 0; i < count_; ++i) {
      Check &c = checks_[i];
      if (c.fn) {
        c.result = c.fn() ? Result::PASS : Result::FAIL;
        c.ran = true;
      } else {
        c.result = Result::SKIP;
        c.ran = true;
      }
      if (c.result == Result::PASS)
        ++passed_;
      else if (c.result == Result::FAIL)
        ++failed_;
      else
        ++skipped_;
    }

    // Print report header.
    bool qualified = (failed_ == 0 && count_ > 0);
    if (out) {
      fprintf(out, "\n  PHANTOM32 ENGINEERING VALIDATION\n");
      fprintf(out, "  ══════════════════════════════════════\n");

      // Print checks grouped by category.
      char lastCat[PHANTOM_VR_MAX_CATEGORY] = {};
      for (size_t i = 0; i < count_; ++i) {
        const Check &c = checks_[i];
        if (strncmp(c.category, lastCat, sizeof(lastCat)) != 0) {
          fprintf(out, "\n  %s\n", c.category);
          strncpy(lastCat, c.category, sizeof(lastCat) - 1);
          lastCat[sizeof(lastCat) - 1] = '\0';
        }
        const char *tag = resultTag(c.result);
        // Pad name to column 40 for aligned status tags.
        int nameLen = (int)strnlen(c.name, sizeof(c.name));
        int pad = 40 - nameLen;
        if (pad < 1)
          pad = 1;
        fprintf(out, "    %s%*s%s\n", c.name, pad, "", tag);
      }

      // Footer.
      fprintf(out, "\n  ══════════════════════════════════════\n");
      fprintf(out, "  Passed: %u  Failed: %u  Skipped: %u\n", (unsigned)passed_, (unsigned)failed_,
              (unsigned)skipped_);
      fprintf(out, "  Overall status: %s\n\n", qualified ? "RELEASE QUALIFIED" : "FAIL");
    }
    return qualified;
  }

  // Accessors (valid after run()).
  uint16_t passCount() const { return passed_; }
  uint16_t failCount() const { return failed_; }
  uint16_t skipCount() const { return skipped_; }
  size_t checkCount() const { return count_; }

  // Look up a named check's result after run().
  Result resultOf(const char *name) const {
    if (!name)
      return Result::SKIP;
    for (size_t i = 0; i < count_; ++i)
      if (strncmp(checks_[i].name, name, sizeof(checks_[i].name)) == 0)
        return checks_[i].result;
    return Result::SKIP;
  }

  const Check *checkAt(size_t i) const { return (i < count_) ? &checks_[i] : nullptr; }

 private:
  static const char *resultTag(Result r) {
    switch (r) {
      case Result::PASS:
        return "PASS";
      case Result::FAIL:
        return "FAIL";
      default:
        return "SKIP";
    }
  }

  Check checks_[PHANTOM_VR_MAX_CHECKS] = {};
  size_t count_ = 0;
  uint16_t passed_ = 0;
  uint16_t failed_ = 0;
  uint16_t skipped_ = 0;
};

#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// SelfTest — startup firmware self-test framework.
//
// On boot, the firmware registers named test functions and runs them all.
// Results are printed as a PASS/FAIL table to stdout (or any FILE*).
//
// Usage:
//   SelfTest st;
//   st.add("Hardware detection", [] { return detectHardware(); });
//   st.add("Memory integrity",   [] { return checkMemory(); });
//   bool allOk = st.run();
//
// Printed output:
//   PHANTOM SELF-TEST
//   ──────────────────────────────
//   [PASS] Hardware detection
//   [PASS] Memory integrity
//   ──────────────────────────────
//   Result: 2/2 passed

typedef bool (*SelfTestFn)();

static constexpr size_t PHANTOM_SELFTEST_MAX = 16;

class SelfTest {
 public:
  struct Entry {
    char name[48] = {};
    SelfTestFn fn = nullptr;
    bool passed = false;
    bool ran = false;
  };

  bool add(const char *name, SelfTestFn fn) {
    if (!name || !fn || count_ >= PHANTOM_SELFTEST_MAX)
      return false;
    Entry &e = entries_[count_++];
    strncpy(e.name, name, sizeof(e.name) - 1);
    e.name[sizeof(e.name) - 1] = '\0';
    e.fn = fn;
    e.ran = false;
    e.passed = false;
    return true;
  }

  // Run all registered tests. Returns true if every test passed.
  bool run(FILE *out = stdout) {
    passed_ = 0;
    failed_ = 0;
    fprintf(out, "\n  PHANTOM SELF-TEST\n");
    fprintf(out, "  ──────────────────────────────\n");
    for (size_t i = 0; i < count_; ++i) {
      entries_[i].passed = entries_[i].fn();
      entries_[i].ran = true;
      if (entries_[i].passed)
        ++passed_;
      else
        ++failed_;
      fprintf(out, "  [%s] %s\n", entries_[i].passed ? "PASS" : "FAIL", entries_[i].name);
    }
    fprintf(out, "  ──────────────────────────────\n");
    fprintf(out, "  Result: %u/%zu passed\n\n", (unsigned)passed_, count_);
    return failed_ == 0;
  }

  uint8_t passCount() const { return passed_; }
  uint8_t failCount() const { return failed_; }
  size_t testCount() const { return count_; }

  // Look up the result of a named test after run().
  bool resultOf(const char *name) const {
    for (size_t i = 0; i < count_; ++i)
      if (strncmp(entries_[i].name, name, sizeof(entries_[i].name)) == 0)
        return entries_[i].passed;
    return false;
  }

  // Access raw entry for inspection.
  const Entry *entryAt(size_t i) const { return (i < count_) ? &entries_[i] : nullptr; }

 private:
  Entry entries_[PHANTOM_SELFTEST_MAX] = {};
  size_t count_ = 0;
  uint8_t passed_ = 0;
  uint8_t failed_ = 0;
};

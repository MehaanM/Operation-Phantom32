#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// ErrorContext — standardized error handling with actionable messages (Phase 20).
// Registers named error codes with hints, tracks active errors, and reports them.

#ifndef PHANTOM32_NATIVE_TEST
#define PHANTOM32_NATIVE_TEST
#endif

static constexpr size_t PHANTOM_EC_MAX_CODES = 32;
static constexpr size_t PHANTOM_EC_MAX_ACTIVE = 16;

enum class ErrorSeverity : uint8_t { INFO, WARNING, ERROR, CRITICAL };

struct ErrorCode {
  uint16_t code;
  char name[32];
  char description[80];
  char hint[80];
  ErrorSeverity severity;
  bool valid;
};

struct ActiveError {
  uint16_t code;
  char context[48];
  uint64_t timestampMs;
  bool valid;
};

struct ErrorContext {
  bool registerCode(uint16_t code, const char *name, const char *description, const char *hint,
                    ErrorSeverity severity) {
    if (!name || !name[0])
      return false;
    for (size_t i = 0; i < codeCount_; ++i) {
      if (codes_[i].code == code) {
        strncpy(codes_[i].name, name, 31);
        if (description)
          strncpy(codes_[i].description, description, 79);
        if (hint)
          strncpy(codes_[i].hint, hint, 79);
        codes_[i].severity = severity;
        return true;
      }
    }
    if (codeCount_ >= PHANTOM_EC_MAX_CODES)
      return false;
    ErrorCode &ec = codes_[codeCount_++];
    memset(&ec, 0, sizeof(ec));
    ec.code = code;
    strncpy(ec.name, name, 31);
    if (description)
      strncpy(ec.description, description, 79);
    if (hint)
      strncpy(ec.hint, hint, 79);
    ec.severity = severity;
    ec.valid = true;
    return true;
  }

  bool push(uint16_t code, const char *context, uint64_t nowMs) {
    if (!codeFor(code))
      return false;
    if (activeCount_ >= PHANTOM_EC_MAX_ACTIVE)
      return false;
    ActiveError &a = active_[activeCount_++];
    memset(&a, 0, sizeof(a));
    a.code = code;
    if (context)
      strncpy(a.context, context, 47);
    a.timestampMs = nowMs;
    a.valid = true;
    return true;
  }

  bool pop(uint16_t code) {
    for (size_t i = 0; i < activeCount_; ++i) {
      if (active_[i].valid && active_[i].code == code) {
        for (size_t j = i + 1; j < activeCount_; ++j)
          active_[j - 1] = active_[j];
        --activeCount_;
        memset(&active_[activeCount_], 0, sizeof(active_[activeCount_]));
        return true;
      }
    }
    return false;
  }

  size_t activeCount() const { return activeCount_; }

  bool hasActive(uint16_t code) const {
    for (size_t i = 0; i < activeCount_; ++i)
      if (active_[i].valid && active_[i].code == code)
        return true;
    return false;
  }

  const ErrorCode *codeFor(uint16_t code) const {
    for (size_t i = 0; i < codeCount_; ++i)
      if (codes_[i].valid && codes_[i].code == code)
        return &codes_[i];
    return nullptr;
  }

  const ActiveError *activeAt(size_t i) const { return (i < activeCount_) ? &active_[i] : nullptr; }

  size_t registeredCount() const { return codeCount_; }

  void generateReport(FILE *out) const {
    if (!out)
      return;
    if (activeCount_ == 0) {
      fprintf(out, "No active errors.\n");
      return;
    }
    fprintf(out, "Active errors:\n");
    for (size_t i = 0; i < activeCount_; ++i) {
      const ErrorCode *ec = codeFor(active_[i].code);
      if (ec) {
        fprintf(out, "  [%u] %s: %s\n", (unsigned)active_[i].code, ec->name, ec->hint);
      } else {
        fprintf(out, "  [%u] (unknown)\n", (unsigned)active_[i].code);
      }
    }
  }

 private:
  ErrorCode codes_[PHANTOM_EC_MAX_CODES] = {};
  ActiveError active_[PHANTOM_EC_MAX_ACTIVE] = {};
  size_t codeCount_ = 0;
  size_t activeCount_ = 0;
};

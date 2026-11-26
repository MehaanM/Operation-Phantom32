#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// ProgressReporter — progress tracking for long-running operations (Phase 20).

#ifndef PHANTOM32_NATIVE_TEST
#define PHANTOM32_NATIVE_TEST
#endif

static constexpr size_t PHANTOM_PROG_MAX_OPS = 8;

struct ProgressOp {
  char name[32];
  char statusMsg[48];
  uint32_t totalSteps;
  uint32_t currentStep;
  bool active;
  bool valid;
};

struct ProgressReporter {
  bool startOp(const char *name, uint32_t totalSteps) {
    if (!name || !name[0])
      return false;
    for (size_t i = 0; i < opCount_; ++i) {
      if (strncmp(ops_[i].name, name, 31) == 0) {
        ops_[i].totalSteps = totalSteps;
        ops_[i].currentStep = 0;
        ops_[i].active = true;
        ops_[i].statusMsg[0] = '\0';
        return true;
      }
    }
    if (opCount_ >= PHANTOM_PROG_MAX_OPS)
      return false;
    ProgressOp &op = ops_[opCount_++];
    memset(&op, 0, sizeof(op));
    strncpy(op.name, name, 31);
    op.totalSteps = totalSteps;
    op.active = true;
    op.valid = true;
    return true;
  }

  bool advanceStep(const char *name, const char *statusMsg) {
    ProgressOp *op = findOp(name);
    if (!op || !op->active)
      return false;
    if (op->currentStep < op->totalSteps)
      ++op->currentStep;
    if (statusMsg)
      strncpy(op->statusMsg, statusMsg, 47);
    return true;
  }

  bool completeOp(const char *name) {
    ProgressOp *op = findOp(name);
    if (!op)
      return false;
    op->currentStep = op->totalSteps;
    op->active = false;
    return true;
  }

  uint8_t percentComplete(const char *name) const {
    if (!name)
      return 0;
    for (size_t i = 0; i < opCount_; ++i) {
      if (strncmp(ops_[i].name, name, 31) == 0) {
        if (ops_[i].totalSteps == 0)
          return 100;
        uint32_t pct = (ops_[i].currentStep * 100u) / ops_[i].totalSteps;
        return (uint8_t)(pct > 100 ? 100 : pct);
      }
    }
    return 0;
  }

  bool isActive(const char *name) const {
    if (!name)
      return false;
    for (size_t i = 0; i < opCount_; ++i)
      if (strncmp(ops_[i].name, name, 31) == 0)
        return ops_[i].active;
    return false;
  }

  bool isComplete(const char *name) const {
    if (!name)
      return false;
    for (size_t i = 0; i < opCount_; ++i) {
      if (strncmp(ops_[i].name, name, 31) == 0)
        return ops_[i].currentStep >= ops_[i].totalSteps;
    }
    return false;
  }

  const ProgressOp *opFor(const char *name) const {
    if (!name)
      return nullptr;
    for (size_t i = 0; i < opCount_; ++i)
      if (strncmp(ops_[i].name, name, 31) == 0)
        return &ops_[i];
    return nullptr;
  }

  size_t activeCount() const {
    size_t n = 0;
    for (size_t i = 0; i < opCount_; ++i)
      if (ops_[i].active)
        ++n;
    return n;
  }

  size_t totalOpCount() const { return opCount_; }

 private:
  ProgressOp ops_[PHANTOM_PROG_MAX_OPS] = {};
  size_t opCount_ = 0;

  ProgressOp *findOp(const char *name) {
    if (!name)
      return nullptr;
    for (size_t i = 0; i < opCount_; ++i)
      if (strncmp(ops_[i].name, name, 31) == 0)
        return &ops_[i];
    return nullptr;
  }
};

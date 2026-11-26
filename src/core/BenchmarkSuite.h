#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// BenchmarkSuite — standardised performance benchmarks with regression detection (Phase 13).
// Records timing samples per named benchmark, maintains min/max/avg, enforces optional
// CPU-time budgets, and detects regressions against named history snapshots.

static constexpr size_t PHANTOM_BS_MAX_BENCHMARKS = 16;
static constexpr size_t PHANTOM_BS_MAX_HISTORY = 128;  // 16 * 8

struct BenchmarkResult {
  char name[32];
  uint64_t minUs;
  uint64_t maxUs;
  uint64_t sumUs;
  uint32_t sampleCount;
  uint64_t budgetUs;
  bool valid;

  uint64_t avgUs() const { return sampleCount > 0 ? sumUs / sampleCount : 0; }
  bool isOverBudget() const { return budgetUs > 0 && avgUs() > budgetUs; }
};

struct BenchmarkHistoryEntry {
  char benchName[32];
  char label[32];
  uint64_t avgUs;
  uint64_t timestampMs;
  bool valid;
};

class BenchmarkSuite {
 public:
  bool registerBenchmark(const char *name, uint64_t budgetUs = 0) {
    if (!name || !*name)
      return false;
    for (size_t i = 0; i < count_; ++i) {
      if (strcmp(results_[i].name, name) == 0) {
        results_[i].budgetUs = budgetUs;
        return true;
      }
    }
    if (count_ >= PHANTOM_BS_MAX_BENCHMARKS)
      return false;
    BenchmarkResult &r = results_[count_++];
    strncpy(r.name, name, sizeof(r.name) - 1);
    r.budgetUs = budgetUs;
    r.minUs = UINT64_MAX;
    r.valid = true;
    return true;
  }

  bool recordSample(const char *name, uint64_t elapsedUs) {
    BenchmarkResult *r = findResult(name);
    if (!r)
      return false;
    if (r->sampleCount == 0 || elapsedUs < r->minUs)
      r->minUs = elapsedUs;
    if (elapsedUs > r->maxUs)
      r->maxUs = elapsedUs;
    r->sumUs += elapsedUs;
    ++r->sampleCount;
    return true;
  }

  const BenchmarkResult *resultFor(const char *name) const { return findConstResult(name); }

  bool anyOverBudget() const {
    for (size_t i = 0; i < count_; ++i) {
      if (results_[i].isOverBudget())
        return true;
    }
    return false;
  }

  bool saveHistory(const char *label, uint64_t timestampMs) {
    if (!label || !*label)
      return false;
    for (size_t i = 0; i < count_; ++i) {
      if (histCount_ >= PHANTOM_BS_MAX_HISTORY)
        return false;
      strncpy(history_[histCount_].benchName, results_[i].name, 31);
      strncpy(history_[histCount_].label, label, 31);
      history_[histCount_].avgUs = results_[i].avgUs();
      history_[histCount_].timestampMs = timestampMs;
      history_[histCount_].valid = true;
      ++histCount_;
    }
    return true;
  }

  // Positive = current avg is higher (regression), negative = improvement.
  int64_t regressionVsHistory(const char *benchName, const char *historyLabel) const {
    if (!benchName || !historyLabel)
      return 0;
    const BenchmarkResult *cur = findConstResult(benchName);
    if (!cur)
      return 0;
    for (size_t i = 0; i < histCount_; ++i) {
      if (strcmp(history_[i].benchName, benchName) == 0 &&
          strcmp(history_[i].label, historyLabel) == 0) {
        return (int64_t)cur->avgUs() - (int64_t)history_[i].avgUs;
      }
    }
    return 0;
  }

  void resetSamples() {
    for (size_t i = 0; i < count_; ++i) {
      results_[i].minUs = UINT64_MAX;
      results_[i].maxUs = 0;
      results_[i].sumUs = 0;
      results_[i].sampleCount = 0;
    }
  }

  void clear() {
    count_ = 0;
    histCount_ = 0;
    for (size_t i = 0; i < PHANTOM_BS_MAX_BENCHMARKS; ++i)
      results_[i] = {};
    for (size_t i = 0; i < PHANTOM_BS_MAX_HISTORY; ++i)
      history_[i] = {};
  }

  size_t benchmarkCount() const { return count_; }

 private:
  BenchmarkResult results_[PHANTOM_BS_MAX_BENCHMARKS] = {};
  size_t count_ = 0;
  BenchmarkHistoryEntry history_[PHANTOM_BS_MAX_HISTORY] = {};
  size_t histCount_ = 0;

  BenchmarkResult *findResult(const char *name) {
    if (!name)
      return nullptr;
    for (size_t i = 0; i < count_; ++i) {
      if (strcmp(results_[i].name, name) == 0)
        return &results_[i];
    }
    return nullptr;
  }

  const BenchmarkResult *findConstResult(const char *name) const {
    if (!name)
      return nullptr;
    for (size_t i = 0; i < count_; ++i) {
      if (strcmp(results_[i].name, name) == 0)
        return &results_[i];
    }
    return nullptr;
  }
};

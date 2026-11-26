#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// QualityScorecard — independent per-metric quality tracking (Phase 18).
// Each metric has its own threshold and status; no combined score.
// Thresholds are validated on registration. Trend requires >= 2 samples.

static constexpr size_t PHANTOM_QS_MAX_METRICS = 20;
static constexpr size_t PHANTOM_QS_MAX_SNAPSHOTS = 16;

enum class MetricStatus : uint8_t { UNKNOWN, PASS, WARN, FAIL };
enum class MetricTrend : uint8_t { STABLE, IMPROVING, REGRESSING };

struct QualityMetric {
  char name[32];
  char unit[16];
  double value;
  double warnThreshold;
  double failThreshold;
  bool higherIsBetter;
  MetricStatus status;
  MetricTrend trend;
  uint32_t sampleCount;  // trend requires >= 2
  bool valid;
};

struct MetricSnapshot {
  char label[32];
  uint64_t timestampMs;
  double values[PHANTOM_QS_MAX_METRICS];
  size_t count;
  bool valid;
};

struct QualityScorecard {
  // Registers (or updates) a metric. Validates thresholds:
  //   higherIsBetter=true  → warnThreshold must be > failThreshold
  //   higherIsBetter=false → warnThreshold must be < failThreshold
  bool trackMetric(const char *name, const char *unit, double warnThreshold, double failThreshold,
                   bool higherIsBetter) {
    if (!name || !name[0])
      return false;
    if (higherIsBetter && warnThreshold <= failThreshold)
      return false;
    if (!higherIsBetter && warnThreshold >= failThreshold)
      return false;
    for (size_t i = 0; i < metricCount_; ++i) {
      if (strncmp(metrics_[i].name, name, 31) == 0) {
        metrics_[i].warnThreshold = warnThreshold;
        metrics_[i].failThreshold = failThreshold;
        metrics_[i].higherIsBetter = higherIsBetter;
        if (unit)
          strncpy(metrics_[i].unit, unit, 15);
        return true;
      }
    }
    if (metricCount_ >= PHANTOM_QS_MAX_METRICS)
      return false;
    QualityMetric &m = metrics_[metricCount_++];
    memset(&m, 0, sizeof(m));
    strncpy(m.name, name, 31);
    if (unit)
      strncpy(m.unit, unit, 15);
    m.warnThreshold = warnThreshold;
    m.failThreshold = failThreshold;
    m.higherIsBetter = higherIsBetter;
    m.status = MetricStatus::UNKNOWN;
    m.trend = MetricTrend::STABLE;
    m.sampleCount = 0;
    m.valid = true;
    return true;
  }

  bool recordValue(const char *name, double value) {
    if (!name)
      return false;
    for (size_t i = 0; i < metricCount_; ++i) {
      if (strncmp(metrics_[i].name, name, 31) == 0) {
        double prev = metrics_[i].value;
        metrics_[i].value = value;
        metrics_[i].status = computeStatus(metrics_[i], value);
        ++metrics_[i].sampleCount;
        // Trend requires at least 2 samples; first sample stays STABLE.
        if (metrics_[i].sampleCount >= 2) {
          bool improved = metrics_[i].higherIsBetter ? (value > prev) : (value < prev);
          bool worsened = metrics_[i].higherIsBetter ? (value < prev) : (value > prev);
          if (improved)
            metrics_[i].trend = MetricTrend::IMPROVING;
          else if (worsened)
            metrics_[i].trend = MetricTrend::REGRESSING;
          else
            metrics_[i].trend = MetricTrend::STABLE;
        } else {
          metrics_[i].trend = MetricTrend::STABLE;
        }
        return true;
      }
    }
    return false;
  }

  MetricStatus statusFor(const char *name) const {
    if (!name)
      return MetricStatus::UNKNOWN;
    for (size_t i = 0; i < metricCount_; ++i) {
      if (strncmp(metrics_[i].name, name, 31) == 0)
        return metrics_[i].status;
    }
    return MetricStatus::UNKNOWN;
  }

  bool saveSnapshot(const char *label, uint64_t timestampMs) {
    if (!label || !label[0])
      return false;
    for (size_t i = 0; i < snapCount_; ++i) {
      if (strncmp(snaps_[i].label, label, 31) == 0) {
        fillSnapshot(snaps_[i], label, timestampMs);
        return true;
      }
    }
    if (snapCount_ >= PHANTOM_QS_MAX_SNAPSHOTS)
      return false;
    fillSnapshot(snaps_[snapCount_++], label, timestampMs);
    return true;
  }

  double regressionVs(const char *label, const char *metricName) const {
    if (!label || !metricName)
      return 0.0;
    const MetricSnapshot *snap = snapshotFor(label);
    if (!snap)
      return 0.0;
    for (size_t i = 0; i < metricCount_; ++i) {
      if (strncmp(metrics_[i].name, metricName, 31) == 0) {
        double baseline = (i < snap->count) ? snap->values[i] : 0.0;
        double cur = metrics_[i].value;
        return metrics_[i].higherIsBetter ? (baseline - cur) : (cur - baseline);
      }
    }
    return 0.0;
  }

  size_t failCount() const {
    size_t n = 0;
    for (size_t i = 0; i < metricCount_; ++i)
      if (metrics_[i].status == MetricStatus::FAIL)
        ++n;
    return n;
  }

  size_t warnCount() const {
    size_t n = 0;
    for (size_t i = 0; i < metricCount_; ++i)
      if (metrics_[i].status == MetricStatus::WARN)
        ++n;
    return n;
  }

  size_t passCount() const {
    size_t n = 0;
    for (size_t i = 0; i < metricCount_; ++i)
      if (metrics_[i].status == MetricStatus::PASS)
        ++n;
    return n;
  }

  size_t metricCount() const { return metricCount_; }

  const QualityMetric *metricFor(const char *name) const {
    if (!name)
      return nullptr;
    for (size_t i = 0; i < metricCount_; ++i)
      if (strncmp(metrics_[i].name, name, 31) == 0)
        return &metrics_[i];
    return nullptr;
  }

  void generateReport(FILE *out) const {
    if (!out)
      return;
    fprintf(out, "Quality Scorecard\n");
    for (size_t i = 0; i < metricCount_; ++i) {
      const char *tag = "UNKNOWN";
      if (metrics_[i].status == MetricStatus::PASS)
        tag = "PASS";
      else if (metrics_[i].status == MetricStatus::WARN)
        tag = "WARN";
      else if (metrics_[i].status == MetricStatus::FAIL)
        tag = "FAIL";
      const char *trend = "";
      if (metrics_[i].trend == MetricTrend::IMPROVING)
        trend = " ^";
      else if (metrics_[i].trend == MetricTrend::REGRESSING)
        trend = " v";
      fprintf(out, "  %-24s %s%s (%.2f %s)\n", metrics_[i].name, tag, trend, metrics_[i].value,
              metrics_[i].unit);
    }
  }

 private:
  QualityMetric metrics_[PHANTOM_QS_MAX_METRICS] = {};
  MetricSnapshot snaps_[PHANTOM_QS_MAX_SNAPSHOTS] = {};
  size_t metricCount_ = 0;
  size_t snapCount_ = 0;

  static MetricStatus computeStatus(const QualityMetric &m, double value) {
    if (m.higherIsBetter) {
      if (value >= m.warnThreshold)
        return MetricStatus::PASS;
      if (value >= m.failThreshold)
        return MetricStatus::WARN;
      return MetricStatus::FAIL;
    } else {
      if (value <= m.warnThreshold)
        return MetricStatus::PASS;
      if (value <= m.failThreshold)
        return MetricStatus::WARN;
      return MetricStatus::FAIL;
    }
  }

  void fillSnapshot(MetricSnapshot &s, const char *label, uint64_t timestampMs) const {
    memset(&s, 0, sizeof(s));
    strncpy(s.label, label, 31);
    s.timestampMs = timestampMs;
    s.count = metricCount_;
    for (size_t i = 0; i < metricCount_; ++i)
      s.values[i] = metrics_[i].value;
    s.valid = true;
  }

  const MetricSnapshot *snapshotFor(const char *label) const {
    if (!label)
      return nullptr;
    for (size_t i = 0; i < snapCount_; ++i)
      if (strncmp(snaps_[i].label, label, 31) == 0)
        return &snaps_[i];
    return nullptr;
  }
};

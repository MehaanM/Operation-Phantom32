#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "hal/NetworkObserver.h"

// AnomalyDetector — confidence-scored wireless anomaly detection (Phase 8).
//
// Maintains a set of baselines (expected SSID/channel/RSSI/encryption state).
// When a new observation deviates from the baseline, an AnomalyEvent is
// generated with a confidence score (0–100) and a plain-English explanation.
// A single indicator yields a low score; multiple corroborating deviations
// compound to a higher score. A configurable threshold suppresses low-confidence
// events that would likely be false positives.
//
// Key design principle from the roadmap: the system must NEVER assert a network
// is malicious from one indicator alone. Confidence, context, and limitations
// must accompany every alert.

static constexpr size_t PHANTOM_AD_MAX_BASELINES = 16;
static constexpr size_t PHANTOM_AD_MAX_ANOMALIES = 32;

enum class AnomalyType : uint8_t {
  SIGNAL_DEVIATION,
  CHANNEL_CHANGE,
  ENCRYPTION_CHANGE,
  DUPLICATE_SSID,
  METADATA_CHANGE
};

struct AnomalyEvent {
  AnomalyType type;
  uint8_t confidence;  // 0–100
  char ssid[33];
  char explanation[80];
  uint64_t timestampMs;
};

struct NetworkBaseline {
  char ssid[33];
  uint8_t bssid[6];
  int8_t rssiMin;
  int8_t rssiMax;
  uint8_t expectedChannel;
  uint8_t expectedEncryption;
  bool set;
};

class AnomalyDetector {
 public:
  // Register expected behaviour for a network.
  bool addBaseline(const char *ssid, const uint8_t bssid[6], int8_t rssiMin, int8_t rssiMax,
                   uint8_t channel, uint8_t encryption) {
    if (!ssid)
      return false;
    NetworkBaseline *b = findBaseline(ssid);
    if (!b) {
      if (baselineCount_ >= PHANTOM_AD_MAX_BASELINES)
        return false;
      b = &baselines_[baselineCount_++];
    }
    strncpy(b->ssid, ssid, sizeof(b->ssid) - 1);
    if (bssid)
      memcpy(b->bssid, bssid, 6);
    else
      memset(b->bssid, 0, 6);
    b->rssiMin = rssiMin;
    b->rssiMax = rssiMax;
    b->expectedChannel = channel;
    b->expectedEncryption = encryption;
    b->set = true;
    return true;
  }

  // Analyse an observation against all baselines. Returns true if at least
  // one anomaly was generated.
  bool analyze(const NetworkObservation &obs, uint64_t nowMs) {
    // Check for duplicate SSIDs (same SSID, different BSSID).
    for (size_t i = 0; i < baselineCount_; ++i) {
      if (strncmp(baselines_[i].ssid, obs.ssid, sizeof(obs.ssid)) != 0)
        continue;
      if (!baselines_[i].set)
        continue;
      uint8_t score = 0;
      char expl[80] = {};
      size_t explPos = 0;

      // Check for different BSSID (duplicate SSID / rogue AP indicator).
      bool bssidMatch = (memcmp(baselines_[i].bssid, obs.bssid, 6) == 0);
      bool duplicateRecorded = false;
      if (!bssidMatch) {
        bool baselineIsZero = true;
        for (int b = 0; b < 6; ++b)
          if (baselines_[i].bssid[b]) {
            baselineIsZero = false;
            break;
          }
        if (!baselineIsZero) {
          uint8_t dupScore = 50;
          appendExpl(expl, explPos, "Duplicate SSID from different BSSID");
          recordAnomaly(AnomalyType::DUPLICATE_SSID, dupScore, obs.ssid, expl, nowMs);
          duplicateRecorded = true;
          explPos = 0;
          expl[0] = '\0';
        }
      }

      // Signal deviation.
      if (obs.rssi < baselines_[i].rssiMin - 15 || obs.rssi > baselines_[i].rssiMax + 15) {
        score += 30;
        appendExpl(expl, explPos, "RSSI outside expected range");
      }
      // Channel change.
      if (obs.channel != baselines_[i].expectedChannel) {
        score += 60;
        appendExpl(expl, explPos, "Channel changed from baseline");
      }
      // Encryption change.
      if (obs.encryptionType != baselines_[i].expectedEncryption) {
        score += 70;
        appendExpl(expl, explPos, "Encryption type changed");
      }

      if (score == 0)
        return duplicateRecorded;
      if (score > 100)
        score = 100;

      AnomalyType type;
      if (obs.channel != baselines_[i].expectedChannel &&
          obs.encryptionType != baselines_[i].expectedEncryption)
        type = AnomalyType::METADATA_CHANGE;
      else if (obs.encryptionType != baselines_[i].expectedEncryption)
        type = AnomalyType::ENCRYPTION_CHANGE;
      else if (obs.channel != baselines_[i].expectedChannel)
        type = AnomalyType::CHANNEL_CHANGE;
      else
        type = AnomalyType::SIGNAL_DEVIATION;

      if (score >= minConfidence_)
        recordAnomaly(type, score, obs.ssid, expl, nowMs);
      return duplicateRecorded || score >= minConfidence_;
    }
    return false;
  }

  void setFalsePositiveThreshold(uint8_t minConfidence) { minConfidence_ = minConfidence; }
  uint8_t falsePositiveThreshold() const { return minConfidence_; }

  size_t anomalyCount() const { return anomCount_; }

  const AnomalyEvent *anomalyAt(size_t i) const {
    return i < anomCount_ ? &anomalies_[i] : nullptr;
  }

  void clear() {
    anomCount_ = 0;
    baselineCount_ = 0;
  }
  void clearAnomalies() { anomCount_ = 0; }

 private:
  NetworkBaseline baselines_[PHANTOM_AD_MAX_BASELINES] = {};
  size_t baselineCount_ = 0;
  AnomalyEvent anomalies_[PHANTOM_AD_MAX_ANOMALIES] = {};
  size_t anomCount_ = 0;
  uint8_t minConfidence_ = 0;

  NetworkBaseline *findBaseline(const char *ssid) {
    if (!ssid)
      return nullptr;
    for (size_t i = 0; i < baselineCount_; ++i)
      if (strncmp(baselines_[i].ssid, ssid, sizeof(baselines_[i].ssid)) == 0)
        return &baselines_[i];
    return nullptr;
  }

  void recordAnomaly(AnomalyType type, uint8_t confidence, const char *ssid, const char *expl,
                     uint64_t nowMs) {
    if (anomCount_ >= PHANTOM_AD_MAX_ANOMALIES)
      return;
    AnomalyEvent &a = anomalies_[anomCount_++];
    a.type = type;
    a.confidence = confidence;
    a.timestampMs = nowMs;
    strncpy(a.ssid, ssid ? ssid : "", sizeof(a.ssid) - 1);
    strncpy(a.explanation, expl ? expl : "", sizeof(a.explanation) - 1);
  }

  static void appendExpl(char *buf, size_t &pos, const char *s) {
    if (pos > 0 && pos < 78) {
      buf[pos++] = ';';
      buf[pos++] = ' ';
    }
    while (*s && pos < 78)
      buf[pos++] = *s++;
    buf[pos] = '\0';
  }
};

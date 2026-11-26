#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// NetworkObserver — passive wireless network observation (Phase 8).
//
// Records NetworkObservations (SSID, BSSID, channel, RSSI, encryption) in a
// fixed-size ring buffer. Privacy mode anonymises the BSSID before storage.
// Automatic data expiry, channel-utilisation reporting, and per-SSID RSSI
// trend analysis support the wireless diagnostics platform.

static constexpr size_t PHANTOM_NO_RING_SIZE = 64;
static constexpr uint8_t PHANTOM_NO_MAX_CHANNELS = 14;
static constexpr uint8_t kNoEncryption = 0;

struct NetworkObservation {
  char ssid[33];
  uint8_t bssid[6];
  int8_t rssi;
  uint8_t channel;
  uint8_t encryptionType;  // 0=OPEN, 1=WEP, 2=WPA, 3=WPA2, 4=WPA3
  uint64_t timestampMs;
  bool anonymized;
};

class NetworkObserver {
 public:
  void setAnonymize(bool on) { anonymize_ = on; }
  bool anonymizing() const { return anonymize_; }

  void observe(const NetworkObservation &obs) {
    NetworkObservation copy = obs;
    if (anonymize_) {
      memset(copy.bssid, 0, sizeof(copy.bssid));
      copy.anonymized = true;
    }
    buf_[head_] = copy;
    head_ = (head_ + 1) % PHANTOM_NO_RING_SIZE;
    if (size_ < PHANTOM_NO_RING_SIZE)
      ++size_;
  }

  size_t observationCount() const { return size_; }

  const NetworkObservation *at(size_t i) const {
    if (i >= size_)
      return nullptr;
    return &buf_[(head_ + PHANTOM_NO_RING_SIZE - size_ + i) % PHANTOM_NO_RING_SIZE];
  }

  void clear() {
    size_ = 0;
    head_ = 0;
  }

  // Remove observations older than cutoffMs.
  void expireOlderThan(uint64_t cutoffMs) {
    size_t newSize = 0;
    for (size_t k = 0; k < size_; ++k) {
      const NetworkObservation *e = at(k);
      if (e && e->timestampMs >= cutoffMs)
        ++newSize;
    }
    if (newSize == size_)
      return;
    // Compact: rebuild buffer keeping only unexpired entries.
    NetworkObservation tmp[PHANTOM_NO_RING_SIZE] = {};
    size_t w = 0;
    for (size_t k = 0; k < size_; ++k) {
      const NetworkObservation *e = at(k);
      if (e && e->timestampMs >= cutoffMs)
        tmp[w++] = *e;
    }
    for (size_t i = 0; i < w; ++i)
      buf_[i] = tmp[i];
    head_ = w % PHANTOM_NO_RING_SIZE;
    size_ = w;
  }

  // Count of observations on a given channel (percentage of total; 0 if none).
  uint8_t channelUtilization(uint8_t channel) const {
    if (size_ == 0)
      return 0;
    size_t cnt = 0;
    for (size_t k = 0; k < size_; ++k) {
      const NetworkObservation *e = at(k);
      if (e && e->channel == channel)
        ++cnt;
    }
    return (uint8_t)((cnt * 100u) / size_);
  }

  // RSSI trend for the named SSID: compares older half vs. newer half.
  // Returns positive (improving), negative (degrading), or 0 (stable/unknown).
  int8_t rssiTrend(const char *ssid) const {
    if (!ssid || size_ < 4)
      return 0;
    int32_t oldSum = 0, oldCnt = 0, newSum = 0, newCnt = 0;
    for (size_t k = 0; k < size_; ++k) {
      const NetworkObservation *e = at(k);
      if (!e || strncmp(e->ssid, ssid, sizeof(e->ssid)) != 0)
        continue;
      if (k < size_ / 2) {
        oldSum += e->rssi;
        ++oldCnt;
      } else {
        newSum += e->rssi;
        ++newCnt;
      }
    }
    if (oldCnt == 0 || newCnt == 0)
      return 0;
    int32_t diff = (newSum / newCnt) - (oldSum / oldCnt);
    if (diff > 2)
      return 1;
    if (diff < -2)
      return -1;
    return 0;
  }

  size_t uniqueNetworkCount() const {
    static constexpr size_t kMax = 32;
    char seen[kMax][33] = {};
    size_t uniq = 0;
    for (size_t k = 0; k < size_; ++k) {
      const NetworkObservation *e = at(k);
      if (!e)
        continue;
      bool found = false;
      for (size_t j = 0; j < uniq; ++j)
        if (strncmp(seen[j], e->ssid, sizeof(e->ssid)) == 0) {
          found = true;
          break;
        }
      if (!found && uniq < kMax)
        strncpy(seen[uniq++], e->ssid, sizeof(e->ssid) - 1);
    }
    return uniq;
  }

 private:
  NetworkObservation buf_[PHANTOM_NO_RING_SIZE] = {};
  size_t head_ = 0;
  size_t size_ = 0;
  bool anonymize_ = false;
};

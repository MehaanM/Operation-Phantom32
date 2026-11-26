#pragma once
#include <stdint.h>

// VirtualClock — controllable deterministic time source for simulation testing (Phase 16).
// All simulation components share one VirtualClock so scenarios replay identically.
// Freeze halts time advancement; advance/set both respect it.

struct VirtualClock {
  VirtualClock() : nowMs_(0), frozen_(false) {}

  uint64_t nowMs() const { return nowMs_; }

  void advance(uint64_t deltaMs) {
    if (!frozen_)
      nowMs_ += deltaMs;
  }

  void set(uint64_t absMs) { nowMs_ = absMs; }

  void freeze() { frozen_ = true; }
  void resume() { frozen_ = false; }
  bool isFrozen() const { return frozen_; }

  void reset() {
    nowMs_ = 0;
    frozen_ = false;
  }

 private:
  uint64_t nowMs_;
  bool frozen_;
};

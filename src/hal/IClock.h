#pragma once
#include <stdint.h>

// IClock — hardware abstraction for timekeeping.
// Implementations: ESP32 (esp_timer), native (std::chrono / clock()).
class IClock {
 public:
  virtual ~IClock() = default;
  virtual uint64_t nowMs() const = 0;  // milliseconds since boot
  virtual uint64_t nowUs() const = 0;  // microseconds since boot
  virtual void delayMs(uint32_t ms) = 0;
};

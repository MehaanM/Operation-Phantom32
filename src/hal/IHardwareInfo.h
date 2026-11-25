#pragma once
#include <stdint.h>

// IHardwareInfo — read-only board identity and capability query.
// Implementations: ESP32 (esp_system + Arduino), native (mock).
class IHardwareInfo {
 public:
  virtual ~IHardwareInfo() = default;
  virtual const char *boardId() const = 0;     // e.g. "doom_s3"
  virtual const char *chipFamily() const = 0;  // e.g. "ESP32-S3"
  virtual uint32_t flashSizeBytes() const = 0;
  virtual uint32_t heapTotalBytes() const = 0;
  virtual bool hasPsram() const = 0;
  virtual uint32_t psramSizeBytes() const = 0;
  virtual uint64_t macAddress() const = 0;  // 48-bit EUI-48
};

#pragma once
#include <stddef.h>
#include <stdint.h>

// IStorage — hardware abstraction for key-value persistent storage.
// Implementations: NVS (ESP32), in-memory map (native tests), LittleFS.
class IStorage {
 public:
  virtual ~IStorage() = default;
  virtual bool open(const char *ns) = 0;
  virtual void close() = 0;
  virtual bool putString(const char *key, const char *value) = 0;
  virtual bool getString(const char *key, char *buf, size_t len) = 0;
  virtual bool putU32(const char *key, uint32_t value) = 0;
  virtual bool getU32(const char *key, uint32_t *out) = 0;
  virtual bool remove(const char *key) = 0;
  virtual void clear() = 0;
  virtual size_t usedBytes() const = 0;
  virtual size_t totalBytes() const = 0;
};

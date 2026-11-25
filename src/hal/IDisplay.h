#pragma once
#include <stdint.h>

// IDisplay — hardware abstraction for simple text/pixel display output.
// Implementations: TFT_eSPI (ESP32), null display (headless devices).
class IDisplay {
 public:
  virtual ~IDisplay() = default;
  virtual bool begin() = 0;
  virtual void clear() = 0;
  virtual void setCursor(uint16_t x, uint16_t y) = 0;
  virtual void setTextColor(uint32_t rgb) = 0;
  virtual void print(const char *text) = 0;
  virtual void flush() = 0;
  virtual uint16_t width() const = 0;
  virtual uint16_t height() const = 0;
};

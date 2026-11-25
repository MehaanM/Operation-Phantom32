#pragma once
#include "IModule.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

// DiagnosticsEngine — portable embedded health monitoring subsystem.
//
// Provides heap statistics, uptime, reset-reason classification, firmware
// identification, and a JSON health export. All platform-specific calls are
// gated behind PHANTOM32_NATIVE_TEST so the engine can be unit-tested on a
// host machine without any ESP32/Arduino headers.
//
// JSON export format (GET /api/diagnostics):
//   {
//     "firmware":    "3.2.1",
//     "board":       "doom_s3",
//     "uptime_ms":   12345678,
//     "heap_free":   120000,
//     "heap_min":    98000,
//     "heap_total":  327680,
//     "heap_pct":    63,
//     "psram_free":  7340032,
//     "psram_total": 8388608,
//     "reset_reason":"POWERON",
//     "health":      "OK"
//   }

#ifdef PHANTOM32_NATIVE_TEST
// ── Mock platform API for host-side unit tests ─────────────────────────────
#include <ctime>
namespace ph_platform {
static uint32_t _mock_free = 150000;
static uint32_t _mock_min = 100000;
static uint32_t _mock_total = 327680;
static uint32_t _mock_psram_free = 0;
static uint32_t _mock_psram_total = 0;
static const char *_mock_reset = "POWERON";

inline uint32_t heapFree() {
  return _mock_free;
}
inline uint32_t heapMinFree() {
  return _mock_min;
}
inline uint32_t heapTotal() {
  return _mock_total;
}
inline uint32_t psramFree() {
  return _mock_psram_free;
}
inline uint32_t psramTotal() {
  return _mock_psram_total;
}
inline uint64_t uptimeMs() {
  return (uint64_t)(clock() * 1000 / CLOCKS_PER_SEC);
}
inline const char *resetReason() {
  return _mock_reset;
}
}  // namespace ph_platform
#else
// ── Real ESP32 platform API ────────────────────────────────────────────────
#include <Arduino.h>
#include <esp_system.h>
namespace ph_platform {
inline uint32_t heapFree() {
  return (uint32_t)esp_get_free_heap_size();
}
inline uint32_t heapMinFree() {
  return (uint32_t)esp_get_minimum_free_heap_size();
}
inline uint32_t heapTotal() {
  return (uint32_t)ESP.getHeapSize();
}
inline uint32_t psramFree() {
  return (uint32_t)ESP.getFreePsram();
}
inline uint32_t psramTotal() {
  return (uint32_t)ESP.getPsramSize();
}
inline uint64_t uptimeMs() {
  return (uint64_t)esp_timer_get_time() / 1000ULL;
}
inline const char *resetReason() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:
      return "POWERON";
    case ESP_RST_SW:
      return "SOFTWARE";
    case ESP_RST_PANIC:
      return "PANIC";
    case ESP_RST_INT_WDT:
      return "INT_WDT";
    case ESP_RST_TASK_WDT:
      return "TASK_WDT";
    case ESP_RST_WDT:
      return "WDT";
    case ESP_RST_BROWNOUT:
      return "BROWNOUT";
    case ESP_RST_SDIO:
      return "SDIO";
    default:
      return "UNKNOWN";
  }
}
}  // namespace ph_platform
#endif  // PHANTOM32_NATIVE_TEST

class DiagnosticsEngine : public IModule {
 public:
  // HealthLevel drives the "health" field in JSON output.
  enum class HealthLevel : uint8_t { OK, WARN, CRITICAL };

  struct Snapshot {
    uint64_t uptime_ms = 0;
    uint32_t heap_free = 0;
    uint32_t heap_min = 0;
    uint32_t heap_total = 0;
    uint32_t psram_free = 0;
    uint32_t psram_total = 0;
    const char *reset_reason = "UNKNOWN";
    HealthLevel health = HealthLevel::OK;
  };

  // Thresholds (configurable before initialize()).
  uint32_t warnHeapBytes = 20000;     // heap_free below this → WARN
  uint32_t criticalHeapBytes = 8000;  // heap_free below this → CRITICAL

  // IModule interface.
  bool initialize() override {
    setRunning();
    return true;
  }
  void update() override { /* pull from platform on demand */ }
  void shutdown() override { setStopped(); }
  const char *name() const override { return "DiagnosticsEngine"; }
  const char *version() const override { return firmwareVersion(); }
  const char *description() const override {
    return "Heap, uptime, reset-reason, and health monitoring";
  }

  // Take a fresh snapshot from the platform layer.
  Snapshot snapshot() const {
    Snapshot s;
    s.uptime_ms = ph_platform::uptimeMs();
    s.heap_free = ph_platform::heapFree();
    s.heap_min = ph_platform::heapMinFree();
    s.heap_total = ph_platform::heapTotal();
    s.psram_free = ph_platform::psramFree();
    s.psram_total = ph_platform::psramTotal();
    s.reset_reason = ph_platform::resetReason();
    s.health = classify(s.heap_free);
    return s;
  }

  // Heap used as a percentage of total (0–100). Returns 0 if total is 0.
  static uint8_t heapUsedPct(const Snapshot &s) {
    if (s.heap_total == 0)
      return 0;
    uint32_t used = s.heap_total > s.heap_free ? s.heap_total - s.heap_free : 0;
    return (uint8_t)((used * 100UL) / s.heap_total);
  }

  // Serialize snapshot to a JSON object (null-terminated, written into buf).
  // Returns number of bytes written (excluding null), or 0 on truncation.
  size_t toJson(char *buf, size_t capacity) const {
    if (!buf || capacity == 0)
      return 0;
    Snapshot s = snapshot();
    uint8_t heap_pct = heapUsedPct(s);
    const char *health_str = s.health == HealthLevel::CRITICAL ? "CRITICAL"
                             : s.health == HealthLevel::WARN   ? "WARN"
                                                               : "OK";
    int n = snprintf(buf, capacity,
                     "{"
                     "\"firmware\":\"%s\","
                     "\"board\":\"%s\","
                     "\"uptime_ms\":%llu,"
                     "\"heap_free\":%lu,"
                     "\"heap_min\":%lu,"
                     "\"heap_total\":%lu,"
                     "\"heap_pct\":%u,"
                     "\"psram_free\":%lu,"
                     "\"psram_total\":%lu,"
                     "\"reset_reason\":\"%s\","
                     "\"health\":\"%s\""
                     "}",
                     firmwareVersion(), boardId(), (unsigned long long)s.uptime_ms,
                     (unsigned long)s.heap_free, (unsigned long)s.heap_min,
                     (unsigned long)s.heap_total, (unsigned)heap_pct, (unsigned long)s.psram_free,
                     (unsigned long)s.psram_total, s.reset_reason, health_str);
    if (n < 0 || (size_t)n >= capacity)
      return 0;
    return (size_t)n;
  }

  static const char *healthString(HealthLevel h) {
    switch (h) {
      case HealthLevel::WARN:
        return "WARN";
      case HealthLevel::CRITICAL:
        return "CRITICAL";
      default:
        return "OK";
    }
  }

 private:
  HealthLevel classify(uint32_t heap_free) const {
    if (heap_free < criticalHeapBytes)
      return HealthLevel::CRITICAL;
    if (heap_free < warnHeapBytes)
      return HealthLevel::WARN;
    return HealthLevel::OK;
  }

  static const char *firmwareVersion() {
#ifdef PHANTOM32_VERSION
    return PHANTOM32_VERSION;
#else
    return "dev";
#endif
  }
  static const char *boardId() {
#ifdef PHANTOM32_BOARD
    return PHANTOM32_BOARD;
#else
    return "unknown";
#endif
  }
};

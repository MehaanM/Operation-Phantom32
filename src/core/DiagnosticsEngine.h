#pragma once
#include "IModule.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

// DiagnosticsEngine — portable embedded health monitoring subsystem.
//
// Provides heap statistics, uptime, reset-reason classification, structured
// diagnostic events with a ring buffer, crash-loop detection, and system health
// scoring. All platform-specific calls are gated behind PHANTOM32_NATIVE_TEST.
//
// Structured events (addEvent):
//   Each event carries a severity, monotone ID, timestamp, module tag, 4-char
//   error code, and a human-readable message. The engine deduplicates runs of
//   identical (severity + code + module) events by incrementing a repeat counter
//   instead of storing duplicates. Events older than the ring buffer capacity
//   are silently dropped.
//
// Crash-loop detection:
//   Each call to recordBoot() increments the in-memory boot counter. If that
//   counter exceeds kCrashLoopThreshold within kCrashWindowMs the engine sets
//   crashLoopDetected() = true so the firmware can enter safe mode.
//
// Health scoring (0–100):
//   100 = everything healthy, deducted for heap pressure, event severity, and
//   crash-loop detection. Use healthScore() to surface a single KPI.
//
// JSON snapshot format (toJson):
//   {"firmware":"…","board":"…","uptime_ms":…,"heap_free":…,"heap_min":…,
//    "heap_total":…,"heap_pct":…,"psram_free":…,"psram_total":…,
//    "reset_reason":"…","health":"OK","health_score":95,"boot_count":1,
//    "crash_loop":false,"event_count":0}
//
// JSON event log format (toJsonEvents):
//   [{"id":0,"ts":1234,"severity":"WARN","module":"WiFi","code":"HEAP",
//     "msg":"heap below threshold","count":1},…]

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

// ── Diagnostic severity ────────────────────────────────────────────────────

enum class DiagSeverity : uint8_t { DEBUG = 0, INFO, WARN, ERROR, CRITICAL };

// ── Structured diagnostic event ───────────────────────────────────────────

struct DiagEvent {
  uint32_t id = 0;           // monotone event ID (wraps at UINT32_MAX)
  uint64_t timestampMs = 0;  // uptime when event was recorded
  DiagSeverity severity = DiagSeverity::INFO;
  char module[16] = {};  // originating module tag
  char code[8] = {};     // 4-char error/event code, e.g. "HEAP"
  char msg[64] = {};     // human-readable description
  uint16_t count = 1;    // dedup counter: runs of same (severity+code+module)
};

// ── Ring buffer for DiagEvent ──────────────────────────────────────────────
// Capacity N slots. Oldest event is overwritten when full.

template <size_t N>
class DiagRingBuffer {
 public:
  static_assert(N > 0, "DiagRingBuffer capacity must be > 0");

  void push(const DiagEvent &ev) {
    // Deduplication: if the last stored event has the same severity+code+module,
    // just increment its counter and update its timestamp.
    if (size_ > 0) {
      DiagEvent &last = slots_[(head_ + N - 1) % N];
      if (last.severity == ev.severity && strncmp(last.code, ev.code, sizeof(last.code)) == 0 &&
          strncmp(last.module, ev.module, sizeof(last.module)) == 0) {
        if (last.count < 65535)
          ++last.count;
        last.timestampMs = ev.timestampMs;
        return;
      }
    }
    slots_[head_] = ev;
    head_ = (head_ + 1) % N;
    if (size_ < N)
      ++size_;
  }

  size_t size() const { return size_; }
  size_t capacity() const { return N; }
  bool empty() const { return size_ == 0; }

  // Index 0 = oldest, size()-1 = newest.
  const DiagEvent *at(size_t i) const {
    if (i >= size_)
      return nullptr;
    size_t tail = (head_ + N - size_) % N;
    return &slots_[(tail + i) % N];
  }

  void clear() {
    head_ = 0;
    size_ = 0;
  }

 private:
  DiagEvent slots_[N] = {};
  size_t head_ = 0;
  size_t size_ = 0;
};

// ── DiagnosticsEngine ──────────────────────────────────────────────────────

static constexpr size_t DIAG_RING_CAPACITY = 32;

class DiagnosticsEngine : public IModule {
 public:
  // Crash-loop detection: if boot count reaches kCrashLoopThreshold
  // within kCrashWindowMs, crashLoopDetected() returns true.
  static constexpr uint8_t kCrashLoopThreshold = 5;
  static constexpr uint32_t kCrashWindowMs = 60000;  // 60 s

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

  // ── IModule interface ────────────────────────────────────────────────────

  bool initialize() override {
    setRunning();
    return true;
  }
  void update() override { /* pull from platform on demand */ }
  void shutdown() override { setStopped(); }
  const char *name() const override { return "DiagnosticsEngine"; }
  const char *version() const override { return firmwareVersion(); }
  const char *description() const override {
    return "Heap, uptime, reset-reason, health monitoring, and event ring buffer";
  }

  // ── Platform snapshot ────────────────────────────────────────────────────

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

  static uint8_t heapUsedPct(const Snapshot &s) {
    if (s.heap_total == 0)
      return 0;
    uint32_t used = s.heap_total > s.heap_free ? s.heap_total - s.heap_free : 0;
    return (uint8_t)((used * 100UL) / s.heap_total);
  }

  // ── Structured event recording ───────────────────────────────────────────

  // Record a structured diagnostic event. Severity < minEventSeverity is dropped.
  void addEvent(DiagSeverity severity, const char *module, const char *code, const char *msg) {
    if (severity < minEventSeverity_)
      return;
    DiagEvent ev;
    ev.id = nextId_++;
    ev.timestampMs = ph_platform::uptimeMs();
    ev.severity = severity;
    if (module)
      strncpy(ev.module, module, sizeof(ev.module) - 1);
    if (code)
      strncpy(ev.code, code, sizeof(ev.code) - 1);
    if (msg)
      strncpy(ev.msg, msg, sizeof(ev.msg) - 1);
    ev.count = 1;
    ring_.push(ev);
  }

  // Convenience wrappers.
  void logInfo(const char *module, const char *code, const char *msg) {
    addEvent(DiagSeverity::INFO, module, code, msg);
  }
  void logWarn(const char *module, const char *code, const char *msg) {
    addEvent(DiagSeverity::WARN, module, code, msg);
  }
  void logError(const char *module, const char *code, const char *msg) {
    addEvent(DiagSeverity::ERROR, module, code, msg);
  }
  void logCritical(const char *module, const char *code, const char *msg) {
    addEvent(DiagSeverity::CRITICAL, module, code, msg);
  }

  // Ring buffer accessors.
  size_t eventCount() const { return ring_.size(); }
  const DiagEvent *eventAt(size_t i) const { return ring_.at(i); }
  void clearEvents() { ring_.clear(); }

  // Filter: events below this severity are silently dropped by addEvent().
  void setMinEventSeverity(DiagSeverity s) { minEventSeverity_ = s; }
  DiagSeverity minEventSeverity() const { return minEventSeverity_; }

  // ── Crash-loop detection ─────────────────────────────────────────────────

  // Call once per boot (before initialize). Tracks rapid-reboot loops.
  void recordBoot(uint32_t nowMs = 0) {
    uint32_t ts = nowMs ? nowMs : (uint32_t)ph_platform::uptimeMs();
    if (bootCount_ == 0) {
      firstBootMs_ = ts;
    }
    ++bootCount_;
    if (!crashLoop_ && bootCount_ >= kCrashLoopThreshold) {
      uint32_t elapsed = ts - firstBootMs_;
      if (elapsed < kCrashWindowMs) {
        crashLoop_ = true;
        addEvent(DiagSeverity::CRITICAL, "Boot", "CLOP", "Crash loop detected");
      }
    }
  }

  bool crashLoopDetected() const { return crashLoop_; }
  uint32_t bootCount() const { return bootCount_; }

  void resetCrashLoop() {
    crashLoop_ = false;
    bootCount_ = 0;
    firstBootMs_ = 0;
  }

  // ── Health scoring (0–100) ───────────────────────────────────────────────

  // Compute a holistic health score:
  //   Start at 100; deduct for heap pressure, error/critical events, crash loop.
  uint8_t healthScore() const {
    int score = 100;
    Snapshot s = snapshot();

    // Heap pressure (up to -40).
    if (s.heap_total > 0) {
      uint8_t pct = heapUsedPct(s);
      if (pct > 90)
        score -= 40;
      else if (pct > 75)
        score -= 20;
      else if (pct > 60)
        score -= 10;
    }

    // Event severity (up to -30 each for CRITICAL, -15 for ERROR).
    for (size_t i = 0; i < ring_.size(); ++i) {
      const DiagEvent *ev = ring_.at(i);
      if (!ev)
        continue;
      if (ev->severity == DiagSeverity::CRITICAL)
        score -= 30;
      else if (ev->severity == DiagSeverity::ERROR)
        score -= 15;
    }

    // Crash loop (-25).
    if (crashLoop_)
      score -= 25;

    if (score < 0)
      score = 0;
    return (uint8_t)score;
  }

  // ── JSON serialization ───────────────────────────────────────────────────

  size_t toJson(char *buf, size_t capacity) const {
    if (!buf || capacity == 0)
      return 0;
    Snapshot s = snapshot();
    uint8_t heap_pct = heapUsedPct(s);
    const char *health_str = s.health == HealthLevel::CRITICAL ? "CRITICAL"
                             : s.health == HealthLevel::WARN   ? "WARN"
                                                               : "OK";
    int n =
        snprintf(buf, capacity,
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
                 "\"health\":\"%s\","
                 "\"health_score\":%u,"
                 "\"boot_count\":%lu,"
                 "\"crash_loop\":%s,"
                 "\"event_count\":%zu"
                 "}",
                 firmwareVersion(), boardId(), (unsigned long long)s.uptime_ms,
                 (unsigned long)s.heap_free, (unsigned long)s.heap_min, (unsigned long)s.heap_total,
                 (unsigned)heap_pct, (unsigned long)s.psram_free, (unsigned long)s.psram_total,
                 s.reset_reason, health_str, (unsigned)healthScore(), (unsigned long)bootCount_,
                 crashLoop_ ? "true" : "false", ring_.size());
    if (n < 0 || (size_t)n >= capacity)
      return 0;
    return (size_t)n;
  }

  // Serialize the event ring to a JSON array. Returns bytes written, 0 on truncation.
  size_t toJsonEvents(char *buf, size_t capacity) const {
    if (!buf || capacity == 0)
      return 0;
    size_t written = 0;
    auto append = [&](const char *s) -> bool {
      size_t len = strlen(s);
      if (written + len >= capacity)
        return false;
      memcpy(buf + written, s, len);
      written += len;
      return true;
    };
    if (!append("["))
      return 0;
    for (size_t i = 0; i < ring_.size(); ++i) {
      const DiagEvent *ev = ring_.at(i);
      if (!ev)
        continue;
      if (i > 0 && !append(","))
        return 0;
      char tmp[256];
      int n = snprintf(tmp, sizeof(tmp),
                       "{\"id\":%lu,\"ts\":%llu,\"severity\":\"%s\","
                       "\"module\":\"%s\",\"code\":\"%s\","
                       "\"msg\":\"%s\",\"count\":%u}",
                       (unsigned long)ev->id, (unsigned long long)ev->timestampMs,
                       severityString(ev->severity), ev->module, ev->code, ev->msg,
                       (unsigned)ev->count);
      if (n < 0 || (size_t)n >= sizeof(tmp))
        return 0;
      if (!append(tmp))
        return 0;
    }
    if (!append("]"))
      return 0;
    buf[written] = '\0';
    return written;
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

  static const char *severityString(DiagSeverity s) {
    switch (s) {
      case DiagSeverity::DEBUG:
        return "DEBUG";
      case DiagSeverity::INFO:
        return "INFO";
      case DiagSeverity::WARN:
        return "WARN";
      case DiagSeverity::ERROR:
        return "ERROR";
      case DiagSeverity::CRITICAL:
        return "CRITICAL";
      default:
        return "UNKNOWN";
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

  DiagRingBuffer<DIAG_RING_CAPACITY> ring_;
  uint32_t nextId_ = 0;
  DiagSeverity minEventSeverity_ = DiagSeverity::DEBUG;

  // Crash-loop state.
  uint32_t bootCount_ = 0;
  uint32_t firstBootMs_ = 0;
  bool crashLoop_ = false;
};

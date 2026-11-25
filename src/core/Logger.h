#pragma once
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

// Logger — structured, severity-filtered logging with pluggable sinks.
//
// Usage (global singleton):
//   Logger::global().addSink(&mySerialSink);
//   PHANTOM_LOG_INFO("Boot", "Firmware v%s starting", PHANTOM32_VERSION);
//
// Usage (local instance, e.g. in tests):
//   Logger log;
//   log.addSink(&captureSink);
//   log.log(LogLevel::WARN, "Test", "heap low: %u", freeBytes);

enum class LogLevel : uint8_t {
  DEBUG = 0,
  INFO,
  WARN,
  ERROR,
  CRITICAL,
};

static constexpr size_t PHANTOM_LOG_MAX_SINKS = 4;
static constexpr size_t PHANTOM_LOG_BUF = 256;

class ILogSink {
 public:
  virtual ~ILogSink() = default;
  virtual void write(LogLevel level, const char *tag, const char *msg) = 0;
};

class Logger {
 public:
  bool addSink(ILogSink *sink) {
    if (!sink || sinkCount_ >= PHANTOM_LOG_MAX_SINKS)
      return false;
    sinks_[sinkCount_++] = sink;
    return true;
  }

  void setMinLevel(LogLevel level) { minLevel_ = level; }
  LogLevel minLevel() const { return minLevel_; }
  size_t sinkCount() const { return sinkCount_; }

  // Clear all sinks and reset min level — useful between unit tests.
  void reset() {
    for (size_t i = 0; i < PHANTOM_LOG_MAX_SINKS; ++i)
      sinks_[i] = nullptr;
    sinkCount_ = 0;
    minLevel_ = LogLevel::DEBUG;
  }

  void log(LogLevel level, const char *tag, const char *fmt, ...) {
    if (level < minLevel_ || sinkCount_ == 0)
      return;
    char buf[PHANTOM_LOG_BUF];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    for (size_t i = 0; i < sinkCount_; ++i)
      sinks_[i]->write(level, tag, buf);
  }

  // Global singleton — shared across the firmware.
  static Logger &global() {
    static Logger instance;
    return instance;
  }

 private:
  Logger() = default;
  ILogSink *sinks_[PHANTOM_LOG_MAX_SINKS] = {};
  size_t sinkCount_ = 0;
  LogLevel minLevel_ = LogLevel::DEBUG;
};

// Convenience macros targeting the global singleton.
// NOLINTNEXTLINE — intentional variadic macro forwarding
#define PHANTOM_LOG(lvl, tag, fmt, ...) Logger::global().log(lvl, tag, fmt, ##__VA_ARGS__)
#define PHANTOM_LOG_DEBUG(tag, fmt, ...) PHANTOM_LOG(LogLevel::DEBUG, tag, fmt, ##__VA_ARGS__)
#define PHANTOM_LOG_INFO(tag, fmt, ...) PHANTOM_LOG(LogLevel::INFO, tag, fmt, ##__VA_ARGS__)
#define PHANTOM_LOG_WARN(tag, fmt, ...) PHANTOM_LOG(LogLevel::WARN, tag, fmt, ##__VA_ARGS__)
#define PHANTOM_LOG_ERROR(tag, fmt, ...) PHANTOM_LOG(LogLevel::ERROR, tag, fmt, ##__VA_ARGS__)
#define PHANTOM_LOG_CRITICAL(tag, fmt, ...) PHANTOM_LOG(LogLevel::CRITICAL, tag, fmt, ##__VA_ARGS__)

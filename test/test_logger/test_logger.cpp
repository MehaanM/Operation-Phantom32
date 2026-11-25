// test_logger.cpp — Unit tests for Logger and ILogSink.

#include "unity.h"
#include "core/Logger.h"
#include <string.h>

void setUp() {
  Logger::global().reset();
}
void tearDown() {
  Logger::global().reset();
}

// ── Test sink ──────────────────────────────────────────────────────────────

struct CaptureSink : public ILogSink {
  LogLevel lastLevel = LogLevel::DEBUG;
  char lastTag[64] = {};
  char lastMsg[PHANTOM_LOG_BUF] = {};
  int callCount = 0;

  void write(LogLevel level, const char *tag, const char *msg) override {
    lastLevel = level;
    strncpy(lastTag, tag, sizeof(lastTag) - 1);
    strncpy(lastMsg, msg, sizeof(lastMsg) - 1);
    ++callCount;
  }
};

// ── addSink ────────────────────────────────────────────────────────────────

static void test_add_sink_success() {
  CaptureSink s;
  TEST_ASSERT_TRUE(Logger::global().addSink(&s));
  TEST_ASSERT_EQUAL(1, (int)Logger::global().sinkCount());
}

static void test_add_null_sink_rejected() {
  TEST_ASSERT_FALSE(Logger::global().addSink(nullptr));
  TEST_ASSERT_EQUAL(0, (int)Logger::global().sinkCount());
}

static void test_add_max_sinks() {
  CaptureSink s1, s2, s3, s4;
  TEST_ASSERT_TRUE(Logger::global().addSink(&s1));
  TEST_ASSERT_TRUE(Logger::global().addSink(&s2));
  TEST_ASSERT_TRUE(Logger::global().addSink(&s3));
  TEST_ASSERT_TRUE(Logger::global().addSink(&s4));
  TEST_ASSERT_EQUAL(4, (int)Logger::global().sinkCount());
}

static void test_add_beyond_max_rejected() {
  CaptureSink s1, s2, s3, s4, s5;
  Logger::global().addSink(&s1);
  Logger::global().addSink(&s2);
  Logger::global().addSink(&s3);
  Logger::global().addSink(&s4);
  TEST_ASSERT_FALSE(Logger::global().addSink(&s5));
  TEST_ASSERT_EQUAL(4, (int)Logger::global().sinkCount());
}

// ── log routing ────────────────────────────────────────────────────────────

static void test_log_delivers_to_sink() {
  CaptureSink s;
  Logger::global().addSink(&s);
  Logger::global().log(LogLevel::INFO, "Boot", "ready");
  TEST_ASSERT_EQUAL(1, s.callCount);
  TEST_ASSERT_EQUAL_STRING("Boot", s.lastTag);
  TEST_ASSERT_EQUAL_STRING("ready", s.lastMsg);
}

static void test_log_no_sinks_is_safe() {
  // No sinks registered — must not crash.
  Logger::global().log(LogLevel::INFO, "X", "msg");
}

static void test_log_delivers_to_multiple_sinks() {
  CaptureSink s1, s2;
  Logger::global().addSink(&s1);
  Logger::global().addSink(&s2);
  Logger::global().log(LogLevel::WARN, "Mod", "both");
  TEST_ASSERT_EQUAL(1, s1.callCount);
  TEST_ASSERT_EQUAL(1, s2.callCount);
}

static void test_log_level_propagated() {
  CaptureSink s;
  Logger::global().addSink(&s);
  Logger::global().log(LogLevel::ERROR, "T", "oops");
  TEST_ASSERT_EQUAL((int)LogLevel::ERROR, (int)s.lastLevel);
}

// ── level filtering ────────────────────────────────────────────────────────

static void test_min_level_default_is_debug() {
  TEST_ASSERT_EQUAL((int)LogLevel::DEBUG, (int)Logger::global().minLevel());
}

static void test_set_min_level() {
  Logger::global().setMinLevel(LogLevel::WARN);
  TEST_ASSERT_EQUAL((int)LogLevel::WARN, (int)Logger::global().minLevel());
}

static void test_message_below_min_level_suppressed() {
  CaptureSink s;
  Logger::global().addSink(&s);
  Logger::global().setMinLevel(LogLevel::WARN);
  Logger::global().log(LogLevel::INFO, "T", "quiet");
  TEST_ASSERT_EQUAL(0, s.callCount);
}

static void test_message_at_min_level_passes() {
  CaptureSink s;
  Logger::global().addSink(&s);
  Logger::global().setMinLevel(LogLevel::WARN);
  Logger::global().log(LogLevel::WARN, "T", "loud");
  TEST_ASSERT_EQUAL(1, s.callCount);
}

static void test_message_above_min_level_passes() {
  CaptureSink s;
  Logger::global().addSink(&s);
  Logger::global().setMinLevel(LogLevel::WARN);
  Logger::global().log(LogLevel::ERROR, "T", "louder");
  TEST_ASSERT_EQUAL(1, s.callCount);
}

// ── format strings ────────────────────────────────────────────────────────

static void test_printf_formatting() {
  CaptureSink s;
  Logger::global().addSink(&s);
  Logger::global().log(LogLevel::INFO, "Fmt", "val=%d str=%s", 42, "hi");
  TEST_ASSERT_EQUAL_STRING("val=42 str=hi", s.lastMsg);
}

static void test_long_message_truncated_not_crashed() {
  CaptureSink s;
  Logger::global().addSink(&s);
  char longMsg[512];
  memset(longMsg, 'x', sizeof(longMsg) - 1);
  longMsg[sizeof(longMsg) - 1] = '\0';
  Logger::global().log(LogLevel::DEBUG, "T", "%s", longMsg);
  TEST_ASSERT_EQUAL(1, s.callCount);
  TEST_ASSERT_LESS_OR_EQUAL(PHANTOM_LOG_BUF - 1, (int)strlen(s.lastMsg));
}

// ── macros ─────────────────────────────────────────────────────────────────

static void test_macro_info() {
  CaptureSink s;
  Logger::global().addSink(&s);
  PHANTOM_LOG_INFO("MacroTest", "macro works %d", 1);
  TEST_ASSERT_EQUAL(1, s.callCount);
  TEST_ASSERT_EQUAL((int)LogLevel::INFO, (int)s.lastLevel);
  TEST_ASSERT_EQUAL_STRING("MacroTest", s.lastTag);
}

static void test_macro_warn() {
  CaptureSink s;
  Logger::global().addSink(&s);
  PHANTOM_LOG_WARN("W", "warn msg");
  TEST_ASSERT_EQUAL((int)LogLevel::WARN, (int)s.lastLevel);
}

static void test_macro_error() {
  CaptureSink s;
  Logger::global().addSink(&s);
  PHANTOM_LOG_ERROR("E", "err");
  TEST_ASSERT_EQUAL((int)LogLevel::ERROR, (int)s.lastLevel);
}

static void test_macro_critical() {
  CaptureSink s;
  Logger::global().addSink(&s);
  PHANTOM_LOG_CRITICAL("C", "crit");
  TEST_ASSERT_EQUAL((int)LogLevel::CRITICAL, (int)s.lastLevel);
}

static void test_macro_debug() {
  CaptureSink s;
  Logger::global().addSink(&s);
  PHANTOM_LOG_DEBUG("D", "debug");
  TEST_ASSERT_EQUAL((int)LogLevel::DEBUG, (int)s.lastLevel);
}

// ── reset ──────────────────────────────────────────────────────────────────

static void test_reset_clears_sinks() {
  CaptureSink s;
  Logger::global().addSink(&s);
  Logger::global().reset();
  TEST_ASSERT_EQUAL(0, (int)Logger::global().sinkCount());
  // Logging after reset must not deliver to cleared sink.
  Logger::global().log(LogLevel::INFO, "T", "after reset");
  TEST_ASSERT_EQUAL(0, s.callCount);
}

static void test_reset_restores_min_level() {
  Logger::global().setMinLevel(LogLevel::CRITICAL);
  Logger::global().reset();
  TEST_ASSERT_EQUAL((int)LogLevel::DEBUG, (int)Logger::global().minLevel());
}

// ── local instance ─────────────────────────────────────────────────────────

static void test_local_logger_independent_of_global() {
  Logger local;
  CaptureSink s;
  local.addSink(&s);
  local.log(LogLevel::INFO, "Local", "hi");
  TEST_ASSERT_EQUAL(1, s.callCount);
  // Global singleton untouched.
  TEST_ASSERT_EQUAL(0, (int)Logger::global().sinkCount());
}

// ── main ───────────────────────────────────────────────────────────────────

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_add_sink_success);
  RUN_TEST(test_add_null_sink_rejected);
  RUN_TEST(test_add_max_sinks);
  RUN_TEST(test_add_beyond_max_rejected);
  RUN_TEST(test_log_delivers_to_sink);
  RUN_TEST(test_log_no_sinks_is_safe);
  RUN_TEST(test_log_delivers_to_multiple_sinks);
  RUN_TEST(test_log_level_propagated);
  RUN_TEST(test_min_level_default_is_debug);
  RUN_TEST(test_set_min_level);
  RUN_TEST(test_message_below_min_level_suppressed);
  RUN_TEST(test_message_at_min_level_passes);
  RUN_TEST(test_message_above_min_level_passes);
  RUN_TEST(test_printf_formatting);
  RUN_TEST(test_long_message_truncated_not_crashed);
  RUN_TEST(test_macro_info);
  RUN_TEST(test_macro_warn);
  RUN_TEST(test_macro_error);
  RUN_TEST(test_macro_critical);
  RUN_TEST(test_macro_debug);
  RUN_TEST(test_reset_clears_sinks);
  RUN_TEST(test_reset_restores_min_level);
  RUN_TEST(test_local_logger_independent_of_global);
  return UNITY_END();
}

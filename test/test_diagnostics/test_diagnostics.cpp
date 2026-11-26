// test_diagnostics.cpp — Unit tests for DiagnosticsEngine.
//
// Tests run on native (host machine). PHANTOM32_NATIVE_TEST is set via
// build_flags in platformio.ini for all native environments.

#include "unity.h"
#include "core/DiagnosticsEngine.h"
#include <string.h>

void setUp()    {}
void tearDown() {}

// ── Lifecycle ──────────────────────────────────────────────────────────────

static void test_initial_state_is_uninitialized() {
    DiagnosticsEngine eng;
    TEST_ASSERT_EQUAL(IModule::State::Uninitialized, eng.state());
    TEST_ASSERT_FALSE(eng.isRunning());
}

static void test_initialize_sets_running() {
    DiagnosticsEngine eng;
    TEST_ASSERT_TRUE(eng.initialize());
    TEST_ASSERT_EQUAL(IModule::State::Running, eng.state());
    TEST_ASSERT_TRUE(eng.isRunning());
}

static void test_shutdown_sets_stopped() {
    DiagnosticsEngine eng;
    eng.initialize();
    eng.shutdown();
    TEST_ASSERT_EQUAL(IModule::State::Stopped, eng.state());
    TEST_ASSERT_FALSE(eng.isRunning());
}

static void test_name_not_null() {
    DiagnosticsEngine eng;
    TEST_ASSERT_NOT_NULL(eng.name());
}

static void test_version_not_null() {
    DiagnosticsEngine eng;
    TEST_ASSERT_NOT_NULL(eng.version());
}

static void test_description_not_null() {
    DiagnosticsEngine eng;
    TEST_ASSERT_NOT_NULL(eng.description());
}

// ── Snapshot ───────────────────────────────────────────────────────────────

static void test_snapshot_heap_free_matches_mock() {
    ph_platform::_mock_free  = 150000;
    ph_platform::_mock_total = 327680;
    DiagnosticsEngine eng;
    eng.initialize();
    auto s = eng.snapshot();
    TEST_ASSERT_EQUAL_UINT32(150000, s.heap_free);
}

static void test_snapshot_heap_total_matches_mock() {
    ph_platform::_mock_total = 327680;
    DiagnosticsEngine eng;
    eng.initialize();
    auto s = eng.snapshot();
    TEST_ASSERT_EQUAL_UINT32(327680, s.heap_total);
}

static void test_snapshot_reset_reason_not_null() {
    DiagnosticsEngine eng;
    eng.initialize();
    auto s = eng.snapshot();
    TEST_ASSERT_NOT_NULL(s.reset_reason);
}

static void test_snapshot_reset_reason_matches_mock() {
    ph_platform::_mock_reset = "SOFTWARE";
    DiagnosticsEngine eng;
    eng.initialize();
    auto s = eng.snapshot();
    TEST_ASSERT_EQUAL_STRING("SOFTWARE", s.reset_reason);
    ph_platform::_mock_reset = "POWERON";
}

// ── Health classification ──────────────────────────────────────────────────

static void test_health_ok_when_heap_above_warn() {
    ph_platform::_mock_free = 50000;
    DiagnosticsEngine eng;
    eng.initialize();
    auto s = eng.snapshot();
    TEST_ASSERT_EQUAL(DiagnosticsEngine::HealthLevel::OK, s.health);
}

static void test_health_warn_when_heap_below_warn_threshold() {
    DiagnosticsEngine eng;
    eng.warnHeapBytes     = 30000;
    eng.criticalHeapBytes = 5000;
    ph_platform::_mock_free = 20000;
    eng.initialize();
    auto s = eng.snapshot();
    TEST_ASSERT_EQUAL(DiagnosticsEngine::HealthLevel::WARN, s.health);
}

static void test_health_critical_when_heap_below_critical_threshold() {
    DiagnosticsEngine eng;
    eng.warnHeapBytes     = 30000;
    eng.criticalHeapBytes = 10000;
    ph_platform::_mock_free = 5000;
    eng.initialize();
    auto s = eng.snapshot();
    TEST_ASSERT_EQUAL(DiagnosticsEngine::HealthLevel::CRITICAL, s.health);
}

static void test_health_ok_at_exact_warn_boundary() {
    DiagnosticsEngine eng;
    eng.warnHeapBytes     = 20000;
    eng.criticalHeapBytes = 5000;
    ph_platform::_mock_free = 20000;
    eng.initialize();
    auto s = eng.snapshot();
    // Equal to threshold → OK (threshold is strictly-less-than)
    TEST_ASSERT_EQUAL(DiagnosticsEngine::HealthLevel::OK, s.health);
}

static void test_health_critical_at_zero_heap() {
    DiagnosticsEngine eng;
    ph_platform::_mock_free = 0;
    eng.initialize();
    auto s = eng.snapshot();
    TEST_ASSERT_EQUAL(DiagnosticsEngine::HealthLevel::CRITICAL, s.health);
}

// ── heapUsedPct ────────────────────────────────────────────────────────────

static void test_heap_used_pct_zero_when_all_free() {
    DiagnosticsEngine::Snapshot s;
    s.heap_free  = 327680;
    s.heap_total = 327680;
    TEST_ASSERT_EQUAL_UINT8(0, DiagnosticsEngine::heapUsedPct(s));
}

static void test_heap_used_pct_100_when_full() {
    DiagnosticsEngine::Snapshot s;
    s.heap_free  = 0;
    s.heap_total = 327680;
    TEST_ASSERT_EQUAL_UINT8(100, DiagnosticsEngine::heapUsedPct(s));
}

static void test_heap_used_pct_half() {
    DiagnosticsEngine::Snapshot s;
    s.heap_free  = 160000;
    s.heap_total = 320000;
    uint8_t pct = DiagnosticsEngine::heapUsedPct(s);
    TEST_ASSERT_EQUAL_UINT8(50, pct);
}

static void test_heap_used_pct_zero_total_safe() {
    DiagnosticsEngine::Snapshot s;
    s.heap_free  = 0;
    s.heap_total = 0;
    TEST_ASSERT_EQUAL_UINT8(0, DiagnosticsEngine::heapUsedPct(s));
}

// ── JSON serialization ─────────────────────────────────────────────────────

static void test_json_contains_firmware_key() {
    DiagnosticsEngine eng;
    eng.initialize();
    char buf[512];
    size_t n = eng.toJson(buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"firmware\""));
}

static void test_json_contains_health_key() {
    ph_platform::_mock_free = 100000;
    DiagnosticsEngine eng;
    eng.initialize();
    char buf[512];
    size_t n = eng.toJson(buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"health\""));
}

static void test_json_health_ok_string() {
    ph_platform::_mock_free = 150000;
    DiagnosticsEngine eng;
    eng.warnHeapBytes     = 20000;
    eng.criticalHeapBytes = 5000;
    eng.initialize();
    char buf[512];
    eng.toJson(buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"OK\""));
}

static void test_json_health_warn_string() {
    DiagnosticsEngine eng;
    eng.warnHeapBytes     = 30000;
    eng.criticalHeapBytes = 5000;
    ph_platform::_mock_free = 10000;
    eng.initialize();
    char buf[512];
    eng.toJson(buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"WARN\""));
}

static void test_json_health_critical_string() {
    DiagnosticsEngine eng;
    eng.warnHeapBytes     = 30000;
    eng.criticalHeapBytes = 10000;
    ph_platform::_mock_free = 3000;
    eng.initialize();
    char buf[512];
    eng.toJson(buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"CRITICAL\""));
}

static void test_json_truncation_returns_zero() {
    DiagnosticsEngine eng;
    eng.initialize();
    char buf[8];
    size_t n = eng.toJson(buf, sizeof(buf));
    TEST_ASSERT_EQUAL(0, n);
}

static void test_json_null_buf_returns_zero() {
    DiagnosticsEngine eng;
    eng.initialize();
    size_t n = eng.toJson(nullptr, 512);
    TEST_ASSERT_EQUAL(0, n);
}

static void test_json_zero_capacity_returns_zero() {
    DiagnosticsEngine eng;
    eng.initialize();
    char buf[512];
    size_t n = eng.toJson(buf, 0);
    TEST_ASSERT_EQUAL(0, n);
}

static void test_json_contains_uptime_key() {
    DiagnosticsEngine eng;
    eng.initialize();
    char buf[512];
    eng.toJson(buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"uptime_ms\""));
}

static void test_json_contains_reset_reason() {
    ph_platform::_mock_reset = "POWERON";
    DiagnosticsEngine eng;
    eng.initialize();
    char buf[512];
    eng.toJson(buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, "POWERON"));
}

static void test_json_is_valid_object() {
    DiagnosticsEngine eng;
    eng.initialize();
    char buf[512];
    size_t n = eng.toJson(buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(2, n);
    TEST_ASSERT_EQUAL('{', buf[0]);
    TEST_ASSERT_EQUAL('}', buf[n - 1]);
}

// ── healthString ───────────────────────────────────────────────────────────

static void test_health_string_ok() {
    TEST_ASSERT_EQUAL_STRING("OK", DiagnosticsEngine::healthString(DiagnosticsEngine::HealthLevel::OK));
}
static void test_health_string_warn() {
    TEST_ASSERT_EQUAL_STRING("WARN", DiagnosticsEngine::healthString(DiagnosticsEngine::HealthLevel::WARN));
}
static void test_health_string_critical() {
    TEST_ASSERT_EQUAL_STRING("CRITICAL", DiagnosticsEngine::healthString(DiagnosticsEngine::HealthLevel::CRITICAL));
}

// ── PSRAM ──────────────────────────────────────────────────────────────────

static void test_snapshot_psram_free_matches_mock() {
    ph_platform::_mock_psram_free  = 7000000;
    ph_platform::_mock_psram_total = 8388608;
    DiagnosticsEngine eng;
    eng.initialize();
    auto s = eng.snapshot();
    TEST_ASSERT_EQUAL_UINT32(7000000, s.psram_free);
    ph_platform::_mock_psram_free  = 0;
    ph_platform::_mock_psram_total = 0;
}

static void test_json_contains_psram_free() {
    ph_platform::_mock_psram_free  = 7000000;
    ph_platform::_mock_psram_total = 8388608;
    DiagnosticsEngine eng;
    eng.initialize();
    char buf[512];
    eng.toJson(buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"psram_free\""));
    ph_platform::_mock_psram_free  = 0;
    ph_platform::_mock_psram_total = 0;
}

// ── DiagRingBuffer (Phase 3) ───────────────────────────────────────────────

static void test_ring_buffer_empty_initially() {
    DiagRingBuffer<8> rb;
    TEST_ASSERT_EQUAL(0, (int)rb.size());
    TEST_ASSERT_TRUE(rb.empty());
}

static void test_ring_buffer_push_increases_size() {
    DiagRingBuffer<8> rb;
    DiagEvent ev;
    rb.push(ev);
    TEST_ASSERT_EQUAL(1, (int)rb.size());
}

static void test_ring_buffer_capacity() {
    DiagRingBuffer<4> rb;
    TEST_ASSERT_EQUAL(4, (int)rb.capacity());
}

static void test_ring_buffer_at_returns_null_when_empty() {
    DiagRingBuffer<4> rb;
    TEST_ASSERT_NULL(rb.at(0));
}

static void test_ring_buffer_wraps_on_overflow() {
    DiagRingBuffer<2> rb;
    DiagEvent a, b, c;
    a.id = 1; b.id = 2; c.id = 3;
    strncpy(a.code, "A", sizeof(a.code));
    strncpy(b.code, "B", sizeof(b.code));
    strncpy(c.code, "C", sizeof(c.code));
    strncpy(a.module, "M", sizeof(a.module));
    strncpy(b.module, "N", sizeof(b.module));
    strncpy(c.module, "O", sizeof(c.module));
    rb.push(a); rb.push(b); rb.push(c);
    TEST_ASSERT_EQUAL(2, (int)rb.size());
    // Oldest is b (index 0), newest is c (index 1).
    TEST_ASSERT_EQUAL_UINT32(2, rb.at(0)->id);
    TEST_ASSERT_EQUAL_UINT32(3, rb.at(1)->id);
}

static void test_ring_buffer_dedup_same_severity_code_module() {
    DiagRingBuffer<8> rb;
    DiagEvent ev;
    ev.severity = DiagSeverity::WARN;
    strncpy(ev.code, "HEAP", sizeof(ev.code));
    strncpy(ev.module, "WiFi", sizeof(ev.module));
    ev.count = 1;
    rb.push(ev);
    rb.push(ev);  // same severity+code+module → dedup
    TEST_ASSERT_EQUAL(1, (int)rb.size());
    TEST_ASSERT_EQUAL(2, (int)rb.at(0)->count);
}

static void test_ring_buffer_no_dedup_different_code() {
    DiagRingBuffer<8> rb;
    DiagEvent a, b;
    a.severity = b.severity = DiagSeverity::WARN;
    strncpy(a.code, "HEAP", sizeof(a.code));
    strncpy(b.code, "PSRM", sizeof(b.code));
    strncpy(a.module, "M", sizeof(a.module));
    strncpy(b.module, "M", sizeof(b.module));
    rb.push(a); rb.push(b);
    TEST_ASSERT_EQUAL(2, (int)rb.size());
}

static void test_ring_buffer_clear() {
    DiagRingBuffer<4> rb;
    DiagEvent ev; rb.push(ev);
    rb.clear();
    TEST_ASSERT_EQUAL(0, (int)rb.size());
    TEST_ASSERT_TRUE(rb.empty());
}

// ── DiagnosticsEngine events (Phase 3) ────────────────────────────────────

static void test_add_event_increases_count() {
    DiagnosticsEngine eng;
    eng.addEvent(DiagSeverity::INFO, "M", "TEST", "msg");
    TEST_ASSERT_EQUAL(1, (int)eng.eventCount());
}

static void test_add_event_respects_min_severity() {
    DiagnosticsEngine eng;
    eng.setMinEventSeverity(DiagSeverity::ERROR);
    eng.addEvent(DiagSeverity::INFO, "M", "T", "m");   // below min — dropped
    eng.addEvent(DiagSeverity::WARN, "M", "T", "m");   // below min — dropped
    eng.addEvent(DiagSeverity::ERROR, "M", "T", "m");  // exactly at min — kept
    TEST_ASSERT_EQUAL(1, (int)eng.eventCount());
}

static void test_event_at_returns_correct_event() {
    DiagnosticsEngine eng;
    eng.addEvent(DiagSeverity::WARN, "WiFi", "HEAP", "heap low");
    const DiagEvent *ev = eng.eventAt(0);
    TEST_ASSERT_NOT_NULL(ev);
    TEST_ASSERT_EQUAL(DiagSeverity::WARN, ev->severity);
    TEST_ASSERT_EQUAL_STRING("WiFi", ev->module);
    TEST_ASSERT_EQUAL_STRING("HEAP", ev->code);
}

static void test_clear_events_empties_buffer() {
    DiagnosticsEngine eng;
    eng.addEvent(DiagSeverity::INFO, "M", "T", "m");
    eng.clearEvents();
    TEST_ASSERT_EQUAL(0, (int)eng.eventCount());
}

static void test_log_warn_convenience() {
    DiagnosticsEngine eng;
    eng.logWarn("Mod", "W001", "warning");
    TEST_ASSERT_EQUAL(1, (int)eng.eventCount());
    TEST_ASSERT_EQUAL(DiagSeverity::WARN, eng.eventAt(0)->severity);
}

static void test_log_error_convenience() {
    DiagnosticsEngine eng;
    eng.logError("Mod", "E001", "error");
    TEST_ASSERT_EQUAL(DiagSeverity::ERROR, eng.eventAt(0)->severity);
}

static void test_log_critical_convenience() {
    DiagnosticsEngine eng;
    eng.logCritical("Mod", "C001", "crit");
    TEST_ASSERT_EQUAL(DiagSeverity::CRITICAL, eng.eventAt(0)->severity);
}

static void test_severity_string_all_values() {
    TEST_ASSERT_EQUAL_STRING("DEBUG",    DiagnosticsEngine::severityString(DiagSeverity::DEBUG));
    TEST_ASSERT_EQUAL_STRING("INFO",     DiagnosticsEngine::severityString(DiagSeverity::INFO));
    TEST_ASSERT_EQUAL_STRING("WARN",     DiagnosticsEngine::severityString(DiagSeverity::WARN));
    TEST_ASSERT_EQUAL_STRING("ERROR",    DiagnosticsEngine::severityString(DiagSeverity::ERROR));
    TEST_ASSERT_EQUAL_STRING("CRITICAL", DiagnosticsEngine::severityString(DiagSeverity::CRITICAL));
}

// ── Crash-loop detection (Phase 3) ────────────────────────────────────────

static void test_crash_loop_not_detected_initially() {
    DiagnosticsEngine eng;
    TEST_ASSERT_FALSE(eng.crashLoopDetected());
    TEST_ASSERT_EQUAL_UINT32(0, eng.bootCount());
}

static void test_record_boot_increments_count() {
    DiagnosticsEngine eng;
    eng.recordBoot(100);
    TEST_ASSERT_EQUAL_UINT32(1, eng.bootCount());
}

static void test_crash_loop_detected_after_threshold() {
    DiagnosticsEngine eng;
    // 5 boots within 60 s → crash loop
    for (uint32_t i = 0; i < DiagnosticsEngine::kCrashLoopThreshold; ++i)
        eng.recordBoot((uint32_t)(i * 1000u));  // 0, 1000, 2000, 3000, 4000 ms
    TEST_ASSERT_TRUE(eng.crashLoopDetected());
}

static void test_crash_loop_not_detected_outside_window() {
    DiagnosticsEngine eng;
    // 5 boots spread over 2 × window → not a crash loop
    uint32_t window2 = DiagnosticsEngine::kCrashWindowMs * 2u;
    for (uint32_t i = 0; i < DiagnosticsEngine::kCrashLoopThreshold; ++i)
        eng.recordBoot(i * (window2 / DiagnosticsEngine::kCrashLoopThreshold));
    TEST_ASSERT_FALSE(eng.crashLoopDetected());
}

static void test_reset_crash_loop_clears_state() {
    DiagnosticsEngine eng;
    for (uint32_t i = 0; i < DiagnosticsEngine::kCrashLoopThreshold; ++i)
        eng.recordBoot((uint32_t)(i * 1000u));
    TEST_ASSERT_TRUE(eng.crashLoopDetected());
    eng.resetCrashLoop();
    TEST_ASSERT_FALSE(eng.crashLoopDetected());
    TEST_ASSERT_EQUAL_UINT32(0, eng.bootCount());
}

// ── Health scoring (Phase 3) ───────────────────────────────────────────────

static void test_health_score_100_when_healthy() {
    ph_platform::_mock_free  = 150000;
    ph_platform::_mock_total = 327680;
    DiagnosticsEngine eng;
    eng.initialize();
    // No events, no crash loop, low heap usage → 100.
    TEST_ASSERT_EQUAL(100, (int)eng.healthScore());
}

static void test_health_score_deducted_for_critical_event() {
    ph_platform::_mock_free  = 150000;
    ph_platform::_mock_total = 327680;
    DiagnosticsEngine eng;
    eng.initialize();
    eng.logCritical("M", "C", "crit");
    TEST_ASSERT_TRUE(eng.healthScore() < 100);
}

static void test_health_score_deducted_for_crash_loop() {
    ph_platform::_mock_free  = 150000;
    ph_platform::_mock_total = 327680;
    DiagnosticsEngine eng;
    for (uint32_t i = 0; i < DiagnosticsEngine::kCrashLoopThreshold; ++i)
        eng.recordBoot(i * 1000u);
    TEST_ASSERT_TRUE(eng.healthScore() < 100);
}

static void test_health_score_minimum_zero() {
    ph_platform::_mock_free  = 0;
    ph_platform::_mock_total = 327680;
    DiagnosticsEngine eng;
    eng.initialize();
    // Flood the ring with CRITICAL events to drive score to 0.
    for (int i = 0; i < 10; ++i) {
        char code[8];
        code[0] = 'A' + i; code[1] = '\0';
        eng.logCritical("M", code, "crit");
    }
    TEST_ASSERT_EQUAL(0, (int)eng.healthScore());
}

// ── JSON events (Phase 3) ──────────────────────────────────────────────────

static void test_json_events_empty_array_when_no_events() {
    DiagnosticsEngine eng;
    char buf[64];
    size_t n = eng.toJsonEvents(buf, sizeof(buf));
    TEST_ASSERT_TRUE(n > 0);
    TEST_ASSERT_EQUAL_STRING("[]", buf);
}

static void test_json_events_contains_event_fields() {
    DiagnosticsEngine eng;
    eng.logWarn("WiFi", "HEAP", "heap low");
    char buf[512];
    size_t n = eng.toJsonEvents(buf, sizeof(buf));
    TEST_ASSERT_TRUE(n > 0);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"severity\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"module\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"code\""));
}

static void test_json_includes_boot_count_and_crash_loop() {
    DiagnosticsEngine eng;
    eng.initialize();
    char buf[512];
    size_t n = eng.toJson(buf, sizeof(buf));
    TEST_ASSERT_TRUE(n > 0);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"boot_count\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"crash_loop\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"health_score\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"event_count\""));
}

// ── Main ───────────────────────────────────────────────────────────────────

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_initial_state_is_uninitialized);
    RUN_TEST(test_initialize_sets_running);
    RUN_TEST(test_shutdown_sets_stopped);
    RUN_TEST(test_name_not_null);
    RUN_TEST(test_version_not_null);
    RUN_TEST(test_description_not_null);

    RUN_TEST(test_snapshot_heap_free_matches_mock);
    RUN_TEST(test_snapshot_heap_total_matches_mock);
    RUN_TEST(test_snapshot_reset_reason_not_null);
    RUN_TEST(test_snapshot_reset_reason_matches_mock);

    RUN_TEST(test_health_ok_when_heap_above_warn);
    RUN_TEST(test_health_warn_when_heap_below_warn_threshold);
    RUN_TEST(test_health_critical_when_heap_below_critical_threshold);
    RUN_TEST(test_health_ok_at_exact_warn_boundary);
    RUN_TEST(test_health_critical_at_zero_heap);

    RUN_TEST(test_heap_used_pct_zero_when_all_free);
    RUN_TEST(test_heap_used_pct_100_when_full);
    RUN_TEST(test_heap_used_pct_half);
    RUN_TEST(test_heap_used_pct_zero_total_safe);

    RUN_TEST(test_json_contains_firmware_key);
    RUN_TEST(test_json_contains_health_key);
    RUN_TEST(test_json_health_ok_string);
    RUN_TEST(test_json_health_warn_string);
    RUN_TEST(test_json_health_critical_string);
    RUN_TEST(test_json_truncation_returns_zero);
    RUN_TEST(test_json_null_buf_returns_zero);
    RUN_TEST(test_json_zero_capacity_returns_zero);
    RUN_TEST(test_json_contains_uptime_key);
    RUN_TEST(test_json_contains_reset_reason);
    RUN_TEST(test_json_is_valid_object);

    RUN_TEST(test_health_string_ok);
    RUN_TEST(test_health_string_warn);
    RUN_TEST(test_health_string_critical);

    RUN_TEST(test_snapshot_psram_free_matches_mock);
    RUN_TEST(test_json_contains_psram_free);

    RUN_TEST(test_ring_buffer_empty_initially);
    RUN_TEST(test_ring_buffer_push_increases_size);
    RUN_TEST(test_ring_buffer_capacity);
    RUN_TEST(test_ring_buffer_at_returns_null_when_empty);
    RUN_TEST(test_ring_buffer_wraps_on_overflow);
    RUN_TEST(test_ring_buffer_dedup_same_severity_code_module);
    RUN_TEST(test_ring_buffer_no_dedup_different_code);
    RUN_TEST(test_ring_buffer_clear);

    RUN_TEST(test_add_event_increases_count);
    RUN_TEST(test_add_event_respects_min_severity);
    RUN_TEST(test_event_at_returns_correct_event);
    RUN_TEST(test_clear_events_empties_buffer);
    RUN_TEST(test_log_warn_convenience);
    RUN_TEST(test_log_error_convenience);
    RUN_TEST(test_log_critical_convenience);
    RUN_TEST(test_severity_string_all_values);

    RUN_TEST(test_crash_loop_not_detected_initially);
    RUN_TEST(test_record_boot_increments_count);
    RUN_TEST(test_crash_loop_detected_after_threshold);
    RUN_TEST(test_crash_loop_not_detected_outside_window);
    RUN_TEST(test_reset_crash_loop_clears_state);

    RUN_TEST(test_health_score_100_when_healthy);
    RUN_TEST(test_health_score_deducted_for_critical_event);
    RUN_TEST(test_health_score_deducted_for_crash_loop);
    RUN_TEST(test_health_score_minimum_zero);

    RUN_TEST(test_json_events_empty_array_when_no_events);
    RUN_TEST(test_json_events_contains_event_fields);
    RUN_TEST(test_json_includes_boot_count_and_crash_loop);

    return UNITY_END();
}

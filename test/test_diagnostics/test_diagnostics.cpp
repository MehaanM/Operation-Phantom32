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

    return UNITY_END();
}

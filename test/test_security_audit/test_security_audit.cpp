// test_security_audit.cpp — Tests for SecurityAuditLog (Phase 7).

#include "unity.h"
#include "core/SecurityAuditLog.h"

void setUp() {}
void tearDown() {}

// ── Basic logging ──────────────────────────────────────────────────────────────

static void test_log_single_event() {
  SecurityAuditLog log;
  log.logEvent(SecurityCategory::AUTH_FAILURE, SecuritySeverity::WARNING,
               "wifi", "bad key", 1000);
  TEST_ASSERT_EQUAL(1, (int)log.eventCount());
}

static void test_null_source_ignored() {
  SecurityAuditLog log;
  log.logEvent(SecurityCategory::AUTH_FAILURE, SecuritySeverity::WARNING,
               nullptr, "msg", 1000);
  TEST_ASSERT_EQUAL(0, (int)log.eventCount());
}

static void test_null_desc_ignored() {
  SecurityAuditLog log;
  log.logEvent(SecurityCategory::AUTH_FAILURE, SecuritySeverity::WARNING,
               "src", nullptr, 1000);
  TEST_ASSERT_EQUAL(0, (int)log.eventCount());
}

static void test_event_fields_stored_correctly() {
  SecurityAuditLog log;
  log.logEvent(SecurityCategory::CONFIG_CHANGE, SecuritySeverity::INFO,
               "admin", "changed channel", 5000);
  const SecurityEvent *e = log.eventAt(0);
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL((int)SecurityCategory::CONFIG_CHANGE, (int)e->category);
  TEST_ASSERT_EQUAL((int)SecuritySeverity::INFO, (int)e->severity);
  TEST_ASSERT_EQUAL_STRING("admin", e->source);
  TEST_ASSERT_EQUAL(5000ull, e->timestampMs);
}

// ── Helpers ───────────────────────────────────────────────────────────────────

static void test_log_auth_failure_helper() {
  SecurityAuditLog log;
  log.logAuthFailure("ble", 2000);
  const SecurityEvent *e = log.eventAt(0);
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL((int)SecurityCategory::AUTH_FAILURE, (int)e->category);
}

static void test_log_config_change_helper() {
  SecurityAuditLog log;
  log.logConfigChange("ssid", "user", 3000);
  const SecurityEvent *e = log.eventAt(0);
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL((int)SecurityCategory::CONFIG_CHANGE, (int)e->category);
}

static void test_log_module_access_helper() {
  SecurityAuditLog log;
  log.logModuleAccess("NetworkScanner", "app", 4000);
  const SecurityEvent *e = log.eventAt(0);
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL((int)SecurityCategory::MODULE_ACCESS, (int)e->category);
}

// ── Deduplication ─────────────────────────────────────────────────────────────

static void test_dedup_same_category_severity_source() {
  SecurityAuditLog log;
  log.logAuthFailure("ble", 1000);
  log.logAuthFailure("ble", 2000);
  log.logAuthFailure("ble", 3000);
  TEST_ASSERT_EQUAL(1, (int)log.eventCount());
  const SecurityEvent *e = log.eventAt(0);
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL(3, (int)e->count);
}

static void test_no_dedup_different_source() {
  SecurityAuditLog log;
  log.logAuthFailure("ble", 1000);
  log.logAuthFailure("wifi", 2000);
  TEST_ASSERT_EQUAL(2, (int)log.eventCount());
}

static void test_no_dedup_different_category() {
  SecurityAuditLog log;
  log.logEvent(SecurityCategory::AUTH_FAILURE, SecuritySeverity::WARNING,
               "src", "d", 1000);
  log.logEvent(SecurityCategory::CONFIG_CHANGE, SecuritySeverity::WARNING,
               "src", "d", 2000);
  TEST_ASSERT_EQUAL(2, (int)log.eventCount());
}

// ── Ring buffer ───────────────────────────────────────────────────────────────

static void test_ring_wraps_at_max_size() {
  SecurityAuditLog log;
  for (int i = 0; i < (int)PHANTOM_SAL_RING_SIZE + 5; ++i) {
    char src[8];
    src[0] = (char)('a' + i % 26);
    src[1] = (char)('0' + i % 10);
    src[2] = '\0';
    log.logEvent(SecurityCategory::NETWORK_EXPOSURE, SecuritySeverity::INFO,
                 src, "d", (uint64_t)i * 1000);
  }
  TEST_ASSERT_EQUAL((int)PHANTOM_SAL_RING_SIZE, (int)log.eventCount());
}

static void test_event_at_out_of_range_null() {
  SecurityAuditLog log;
  TEST_ASSERT_NULL(log.eventAt(0));
}

// ── Clear ─────────────────────────────────────────────────────────────────────

static void test_clear_resets_count() {
  SecurityAuditLog log;
  log.logAuthFailure("x", 1000);
  log.logAuthFailure("y", 2000);
  log.clear();
  TEST_ASSERT_EQUAL(0, (int)log.eventCount());
}

// ── Severity counts ───────────────────────────────────────────────────────────

static void test_critical_count() {
  SecurityAuditLog log;
  log.logEvent(SecurityCategory::PRIVILEGE_ESCALATION, SecuritySeverity::CRITICAL,
               "proc", "escalation", 1000);
  log.logEvent(SecurityCategory::PRIVILEGE_ESCALATION, SecuritySeverity::CRITICAL,
               "proc2", "escalation", 2000);
  log.logEvent(SecurityCategory::AUTH_FAILURE, SecuritySeverity::WARNING,
               "wifi", "fail", 3000);
  TEST_ASSERT_EQUAL(2u, log.criticalCount());
  TEST_ASSERT_EQUAL(1u, log.warningCount());
}

// ── Sliding-window detection ──────────────────────────────────────────────────

static void test_repeated_auth_failures_detected() {
  SecurityAuditLog log;
  for (int i = 0; i < 5; ++i) log.logAuthFailure("ble", (uint64_t)i * 100);
  TEST_ASSERT_TRUE(log.hasRepeatedAuthFailures("ble", 5, 10000, 10000));
}

static void test_repeated_auth_failures_below_threshold() {
  SecurityAuditLog log;
  log.logAuthFailure("ble", 1000);
  TEST_ASSERT_FALSE(log.hasRepeatedAuthFailures("ble", 5, 10000, 5000));
}

static void test_auth_failures_outside_window_ignored() {
  SecurityAuditLog log;
  log.logAuthFailure("ble", 1000);  // old event
  TEST_ASSERT_FALSE(log.hasRepeatedAuthFailures("ble", 1, 100, 60000));
}

static void test_suspicious_config_changes_detected() {
  SecurityAuditLog log;
  for (int i = 0; i < 3; ++i)
    log.logConfigChange("ssid", "user", (uint64_t)i * 1000);
  TEST_ASSERT_TRUE(log.hasSuspiciousConfigChanges("ssid", 3, 10000, 10000));
}

static void test_suspicious_config_null_key_safe() {
  SecurityAuditLog log;
  TEST_ASSERT_FALSE(log.hasSuspiciousConfigChanges(nullptr, 1, 10000, 10000));
}

static void test_auth_null_source_safe() {
  SecurityAuditLog log;
  TEST_ASSERT_FALSE(log.hasRepeatedAuthFailures(nullptr, 1, 10000, 10000));
}

// ── Report / JSON ─────────────────────────────────────────────────────────────

static void test_generate_report_does_not_crash() {
  SecurityAuditLog log;
  log.logAuthFailure("wifi", 1000);
  log.generateReport(nullptr);  // should be a no-op
  char buf[512];
  FILE *f = fmemopen(buf, sizeof(buf), "w");
  TEST_ASSERT_NOT_NULL(f);
  log.generateReport(f);
  fclose(f);
  TEST_ASSERT_TRUE(buf[0] != '\0');
}

static void test_export_json_valid_format() {
  SecurityAuditLog log;
  log.logAuthFailure("ble", 1000);
  char buf[256];
  log.exportJson(buf, sizeof(buf));
  TEST_ASSERT_TRUE(buf[0] == '{');
  TEST_ASSERT_NOT_NULL(strstr(buf, "events"));
}

static void test_export_json_null_buf_safe() {
  SecurityAuditLog log;
  log.exportJson(nullptr, 0);  // must not crash
}

static void test_export_json_empty_log() {
  SecurityAuditLog log;
  char buf[64];
  log.exportJson(buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("{\"events\":[]}", buf);
}

static void test_critical_severity_logged_in_report() {
  SecurityAuditLog log;
  log.logEvent(SecurityCategory::PRIVILEGE_ESCALATION, SecuritySeverity::CRITICAL,
               "proc", "root escalation", 1000);
  char buf[512];
  FILE *f = fmemopen(buf, sizeof(buf), "w");
  TEST_ASSERT_NOT_NULL(f);
  log.generateReport(f);
  fclose(f);
  TEST_ASSERT_NOT_NULL(strstr(buf, "CRITICAL"));
}

static void test_zero_threshold_always_false() {
  SecurityAuditLog log;
  log.logAuthFailure("x", 1000);
  TEST_ASSERT_FALSE(log.hasRepeatedAuthFailures("x", 0, 10000, 10000));
  TEST_ASSERT_FALSE(log.hasSuspiciousConfigChanges("x", 0, 10000, 10000));
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_log_single_event);
  RUN_TEST(test_null_source_ignored);
  RUN_TEST(test_null_desc_ignored);
  RUN_TEST(test_event_fields_stored_correctly);

  RUN_TEST(test_log_auth_failure_helper);
  RUN_TEST(test_log_config_change_helper);
  RUN_TEST(test_log_module_access_helper);

  RUN_TEST(test_dedup_same_category_severity_source);
  RUN_TEST(test_no_dedup_different_source);
  RUN_TEST(test_no_dedup_different_category);

  RUN_TEST(test_ring_wraps_at_max_size);
  RUN_TEST(test_event_at_out_of_range_null);

  RUN_TEST(test_clear_resets_count);

  RUN_TEST(test_critical_count);

  RUN_TEST(test_repeated_auth_failures_detected);
  RUN_TEST(test_repeated_auth_failures_below_threshold);
  RUN_TEST(test_auth_failures_outside_window_ignored);
  RUN_TEST(test_suspicious_config_changes_detected);
  RUN_TEST(test_suspicious_config_null_key_safe);
  RUN_TEST(test_auth_null_source_safe);

  RUN_TEST(test_generate_report_does_not_crash);
  RUN_TEST(test_export_json_valid_format);
  RUN_TEST(test_export_json_null_buf_safe);
  RUN_TEST(test_export_json_empty_log);
  RUN_TEST(test_critical_severity_logged_in_report);
  RUN_TEST(test_zero_threshold_always_false);

  return UNITY_END();
}

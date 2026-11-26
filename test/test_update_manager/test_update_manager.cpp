// test_update_manager.cpp — Tests for UpdateManager (Phase 10).

#include "unity.h"
#include "core/UpdateManager.h"

void setUp() {}
void tearDown() {}

// ── Helpers ───────────────────────────────────────────────────────────────────

static UpdateManifest makeManifest(const char *ver, uint32_t size,
                                   uint32_t checksum,
                                   const char *minCompat = "",
                                   const char *board = "esp32") {
  UpdateManifest m = {};
  strncpy(m.version, ver, sizeof(m.version) - 1);
  m.expectedSize = size;
  m.expectedChecksum = checksum;
  strncpy(m.minCompatVersion, minCompat, sizeof(m.minCompatVersion) - 1);
  strncpy(m.targetBoard, board, sizeof(m.targetBoard) - 1);
  m.valid = true;
  return m;
}

// ── loadManifest ──────────────────────────────────────────────────────────────

static void test_load_valid_manifest() {
  UpdateManager um;
  UpdateManifest m = makeManifest("1.2.0", 4096, 0xDEADBEEF);
  TEST_ASSERT_TRUE(um.loadManifest(m));
  TEST_ASSERT_EQUAL((int)UpdateState::IDLE, (int)um.state());
}

static void test_load_invalid_manifest_fails() {
  UpdateManager um;
  UpdateManifest m = {};
  m.valid = false;
  TEST_ASSERT_FALSE(um.loadManifest(m));
  TEST_ASSERT_EQUAL((int)UpdateState::FAILED, (int)um.state());
  TEST_ASSERT_EQUAL((int)UpdateFailReason::INVALID_MANIFEST, (int)um.failReason());
}

static void test_load_manifest_empty_version_fails() {
  UpdateManager um;
  UpdateManifest m = {};
  m.valid = true;
  m.version[0] = '\0';
  TEST_ASSERT_FALSE(um.loadManifest(m));
}

// ── validateManifest / checkCompatibility ─────────────────────────────────────

static void test_validate_manifest_ok() {
  UpdateManager um;
  um.loadManifest(makeManifest("1.0.0", 4096, 0xABCD1234));
  TEST_ASSERT_TRUE(um.validateManifest());
}

static void test_check_compatibility_no_min_version() {
  UpdateManager um;
  um.loadManifest(makeManifest("1.0.0", 4096, 0, ""));
  TEST_ASSERT_TRUE(um.checkCompatibility("0.1.0"));
}

static void test_check_compatibility_satisfies_min() {
  UpdateManager um;
  um.loadManifest(makeManifest("2.0.0", 4096, 0, "1.0.0"));
  TEST_ASSERT_TRUE(um.checkCompatibility("1.5.0"));
}

static void test_check_compatibility_below_min_fails() {
  UpdateManager um;
  um.loadManifest(makeManifest("2.0.0", 4096, 0, "1.5.0"));
  TEST_ASSERT_FALSE(um.checkCompatibility("1.0.0"));
}

static void test_check_compatibility_null_version_fails() {
  UpdateManager um;
  um.loadManifest(makeManifest("2.0.0", 4096, 0, "1.0.0"));
  TEST_ASSERT_FALSE(um.checkCompatibility(nullptr));
}

// ── startUpdate ───────────────────────────────────────────────────────────────

static void test_start_update_transitions_to_downloading() {
  UpdateManager um;
  um.loadManifest(makeManifest("1.2.0", 4096, 0xDEAD));
  TEST_ASSERT_TRUE(um.startUpdate("1.1.0", 1000));
  TEST_ASSERT_EQUAL((int)UpdateState::DOWNLOADING, (int)um.state());
}

static void test_start_update_without_manifest_fails() {
  UpdateManager um;
  TEST_ASSERT_FALSE(um.startUpdate("1.0.0", 1000));
}

static void test_start_update_not_idle_fails() {
  UpdateManager um;
  um.loadManifest(makeManifest("1.2.0", 4096, 0xDEAD));
  um.startUpdate("1.0.0", 1000);
  TEST_ASSERT_FALSE(um.startUpdate("1.0.0", 2000));  // already DOWNLOADING
}

// ── setDownloadProgress ───────────────────────────────────────────────────────

static void test_progress_updates_correctly() {
  UpdateManager um;
  um.loadManifest(makeManifest("1.2.0", 4096, 0xDEAD));
  um.startUpdate("1.0.0", 1000);
  TEST_ASSERT_TRUE(um.setDownloadProgress(2048));
  TEST_ASSERT_EQUAL(50, (int)um.progress());
  TEST_ASSERT_EQUAL(2048u, um.bytesReceived());
}

static void test_progress_overflow_fails() {
  UpdateManager um;
  um.loadManifest(makeManifest("1.2.0", 4096, 0xDEAD));
  um.startUpdate("1.0.0", 1000);
  TEST_ASSERT_FALSE(um.setDownloadProgress(5000));
  TEST_ASSERT_EQUAL((int)UpdateState::FAILED, (int)um.state());
}

static void test_progress_not_in_downloading_fails() {
  UpdateManager um;
  um.loadManifest(makeManifest("1.2.0", 4096, 0xDEAD));
  TEST_ASSERT_FALSE(um.setDownloadProgress(100));
}

// ── completeDownload ──────────────────────────────────────────────────────────

static void test_complete_download_success() {
  UpdateManager um;
  um.loadManifest(makeManifest("1.2.0", 1024, 0xCAFE));
  um.startUpdate("1.0.0", 1000);
  um.setDownloadProgress(1024);
  TEST_ASSERT_TRUE(um.completeDownload(0xCAFE, 2000));
  TEST_ASSERT_EQUAL((int)UpdateState::DONE, (int)um.state());
  TEST_ASSERT_EQUAL(100, (int)um.progress());
}

static void test_complete_download_checksum_mismatch_fails() {
  UpdateManager um;
  um.loadManifest(makeManifest("1.2.0", 1024, 0xCAFE));
  um.startUpdate("1.0.0", 1000);
  um.setDownloadProgress(1024);
  TEST_ASSERT_FALSE(um.completeDownload(0xDEAD, 2000));
  TEST_ASSERT_EQUAL((int)UpdateState::FAILED, (int)um.state());
  TEST_ASSERT_EQUAL((int)UpdateFailReason::CHECKSUM_MISMATCH, (int)um.failReason());
}

static void test_complete_download_incomplete_fails() {
  UpdateManager um;
  um.loadManifest(makeManifest("1.2.0", 1024, 0xCAFE));
  um.startUpdate("1.0.0", 1000);
  um.setDownloadProgress(512);  // only half downloaded
  TEST_ASSERT_FALSE(um.completeDownload(0xCAFE, 2000));
  TEST_ASSERT_EQUAL((int)UpdateFailReason::INCOMPLETE_DOWNLOAD, (int)um.failReason());
}

// ── rollback ─────────────────────────────────────────────────────────────────

static void test_rollback_from_failed() {
  UpdateManager um;
  um.loadManifest(makeManifest("1.2.0", 1024, 0xCAFE));
  um.startUpdate("1.0.0", 1000);
  um.setDownloadProgress(1024);
  um.completeDownload(0xDEAD, 2000);  // force FAILED
  TEST_ASSERT_TRUE(um.rollback(3000));
  TEST_ASSERT_EQUAL((int)UpdateState::ROLLED_BACK, (int)um.state());
}

static void test_rollback_from_idle_fails() {
  UpdateManager um;
  um.loadManifest(makeManifest("1.2.0", 1024, 0xCAFE));
  TEST_ASSERT_FALSE(um.rollback(1000));
}

// ── simulatePowerLoss / canResume ─────────────────────────────────────────────

static void test_simulate_power_loss_during_download() {
  UpdateManager um;
  um.loadManifest(makeManifest("1.2.0", 1024, 0xCAFE));
  um.startUpdate("1.0.0", 1000);
  um.setDownloadProgress(512);
  um.simulatePowerLoss();
  TEST_ASSERT_EQUAL((int)UpdateState::FAILED, (int)um.state());
  TEST_ASSERT_EQUAL((int)UpdateFailReason::INTERRUPTED, (int)um.failReason());
  TEST_ASSERT_TRUE(um.canResume());
}

static void test_no_resume_if_no_bytes() {
  UpdateManager um;
  um.loadManifest(makeManifest("1.2.0", 1024, 0xCAFE));
  um.startUpdate("1.0.0", 1000);
  um.simulatePowerLoss();
  TEST_ASSERT_FALSE(um.canResume());
}

// ── reset ─────────────────────────────────────────────────────────────────────

static void test_reset_returns_to_idle() {
  UpdateManager um;
  um.loadManifest(makeManifest("1.2.0", 1024, 0xCAFE));
  um.startUpdate("1.0.0", 1000);
  um.reset();
  TEST_ASSERT_EQUAL((int)UpdateState::IDLE, (int)um.state());
  TEST_ASSERT_EQUAL(0u, um.bytesReceived());
  TEST_ASSERT_EQUAL(0, (int)um.progress());
}

// ── Transactions ──────────────────────────────────────────────────────────────

static void test_transaction_recorded_on_success() {
  UpdateManager um;
  um.loadManifest(makeManifest("1.2.0", 1024, 0xCAFE));
  um.startUpdate("1.1.0", 1000);
  um.setDownloadProgress(1024);
  um.completeDownload(0xCAFE, 2000);
  TEST_ASSERT_EQUAL(1, (int)um.transactionCount());
  const UpdateTransaction *tx = um.transactionAt(0);
  TEST_ASSERT_NOT_NULL(tx);
  TEST_ASSERT_TRUE(tx->success);
  TEST_ASSERT_EQUAL_STRING("1.2.0", tx->toVersion);
}

static void test_transaction_at_null_out_of_range() {
  UpdateManager um;
  TEST_ASSERT_NULL(um.transactionAt(0));
}

static void test_transaction_recorded_on_rollback() {
  UpdateManager um;
  um.loadManifest(makeManifest("1.2.0", 1024, 0xCAFE));
  um.startUpdate("1.0.0", 1000);
  um.setDownloadProgress(1024);
  um.completeDownload(0xBEEF, 2000);  // wrong checksum → FAILED
  um.rollback(3000);
  TEST_ASSERT_EQUAL(1, (int)um.transactionCount());
  TEST_ASSERT_FALSE(um.transactionAt(0)->success);
}

static void test_zero_expected_size_progress() {
  UpdateManager um;
  um.loadManifest(makeManifest("1.2.0", 0, 0xCAFE));  // expectedSize=0
  um.startUpdate("1.0.0", 1000);
  TEST_ASSERT_TRUE(um.setDownloadProgress(999));  // no overflow check when size==0
}

static void test_zero_expected_size_complete() {
  UpdateManager um;
  um.loadManifest(makeManifest("1.2.0", 0, 0xCAFE));  // expectedSize=0
  um.startUpdate("1.0.0", 1000);
  um.setDownloadProgress(0);
  TEST_ASSERT_TRUE(um.completeDownload(0xCAFE, 2000));  // no incomplete check
  TEST_ASSERT_EQUAL((int)UpdateState::DONE, (int)um.state());
}

static void test_transaction_log_rotation() {
  UpdateManager um;
  for (size_t i = 0; i <= PHANTOM_UM_MAX_TRANSACTIONS; ++i) {
    um.reset();
    um.loadManifest(makeManifest("1.2.0", 0, 0xCAFE));
    um.startUpdate("1.0.0", (uint64_t)i * 1000);
    um.completeDownload(0xCAFE, (uint64_t)(i + 1) * 1000);
  }
  TEST_ASSERT_EQUAL((int)PHANTOM_UM_MAX_TRANSACTIONS, (int)um.transactionCount());
}

static void test_null_from_version_in_start_update() {
  UpdateManager um;
  um.loadManifest(makeManifest("1.2.0", 0, 0xCAFE));
  TEST_ASSERT_TRUE(um.startUpdate(nullptr, 1000));
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_load_valid_manifest);
  RUN_TEST(test_load_invalid_manifest_fails);
  RUN_TEST(test_load_manifest_empty_version_fails);

  RUN_TEST(test_validate_manifest_ok);
  RUN_TEST(test_check_compatibility_no_min_version);
  RUN_TEST(test_check_compatibility_satisfies_min);
  RUN_TEST(test_check_compatibility_below_min_fails);
  RUN_TEST(test_check_compatibility_null_version_fails);

  RUN_TEST(test_start_update_transitions_to_downloading);
  RUN_TEST(test_start_update_without_manifest_fails);
  RUN_TEST(test_start_update_not_idle_fails);

  RUN_TEST(test_progress_updates_correctly);
  RUN_TEST(test_progress_overflow_fails);
  RUN_TEST(test_progress_not_in_downloading_fails);

  RUN_TEST(test_complete_download_success);
  RUN_TEST(test_complete_download_checksum_mismatch_fails);
  RUN_TEST(test_complete_download_incomplete_fails);

  RUN_TEST(test_rollback_from_failed);
  RUN_TEST(test_rollback_from_idle_fails);

  RUN_TEST(test_simulate_power_loss_during_download);
  RUN_TEST(test_no_resume_if_no_bytes);

  RUN_TEST(test_reset_returns_to_idle);

  RUN_TEST(test_transaction_recorded_on_success);
  RUN_TEST(test_transaction_at_null_out_of_range);
  RUN_TEST(test_transaction_recorded_on_rollback);
  RUN_TEST(test_zero_expected_size_progress);
  RUN_TEST(test_zero_expected_size_complete);
  RUN_TEST(test_transaction_log_rotation);
  RUN_TEST(test_null_from_version_in_start_update);

  return UNITY_END();
}

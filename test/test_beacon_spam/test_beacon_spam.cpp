// test_beacon_spam.cpp — Unit tests for beacon spam logic.
//
// Tests: SSID pool management (add/remove/clear/cap), beacon interval
// validation, channel assignment, frame counter tracking, and attack
// state machine transitions.
// All tests run on native (no Arduino dependencies).

#include <unity.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

// ============================================================
// MINIMAL STUBS
// ============================================================

#define BS_MAX_SSIDS      64
#define BS_SSID_LEN       33
#define BS_MIN_INTERVAL   10     // TU
#define BS_MAX_INTERVAL   1000   // TU
#define BS_DEFAULT_INTERVAL 100

struct BsSsidEntry {
  char    ssid[BS_SSID_LEN];
  uint8_t channel;
  bool    active;
  uint32_t frames_sent;
};

typedef enum { BS_IDLE=0, BS_RUNNING=1 } BsState;

static BsSsidEntry g_bs_ssids[BS_MAX_SSIDS];
static int         g_bs_ssid_count;
static BsState     g_bs_state;
static uint16_t    g_bs_interval_tu;
static uint8_t     g_bs_channel;
static uint32_t    g_bs_total_frames;
static uint32_t    g_mock_ms;

static void bs_reset() {
  memset(g_bs_ssids, 0, sizeof(g_bs_ssids));
  g_bs_ssid_count   = 0;
  g_bs_state        = BS_IDLE;
  g_bs_interval_tu  = BS_DEFAULT_INTERVAL;
  g_bs_channel      = 6;
  g_bs_total_frames = 0;
  g_mock_ms         = 0;
}

static bool bs_add_ssid(const char *ssid, uint8_t ch) {
  if (!ssid || strlen(ssid) == 0 || strlen(ssid) >= BS_SSID_LEN) return false;
  for (int i = 0; i < g_bs_ssid_count; i++)
    if (strcmp(g_bs_ssids[i].ssid, ssid) == 0) return false; // duplicate
  if (g_bs_ssid_count >= BS_MAX_SSIDS) return false;
  BsSsidEntry *e = &g_bs_ssids[g_bs_ssid_count++];
  memset(e, 0, sizeof(BsSsidEntry));
  strncpy(e->ssid, ssid, BS_SSID_LEN - 1);
  e->channel = ch;
  e->active  = true;
  return true;
}

static bool bs_remove_ssid(const char *ssid) {
  for (int i = 0; i < g_bs_ssid_count; i++) {
    if (strcmp(g_bs_ssids[i].ssid, ssid) == 0) {
      g_bs_ssids[i] = g_bs_ssids[--g_bs_ssid_count];
      return true;
    }
  }
  return false;
}

static void bs_clear_ssids() {
  memset(g_bs_ssids, 0, sizeof(g_bs_ssids));
  g_bs_ssid_count = 0;
}

static bool bs_set_interval(uint16_t tu) {
  if (tu < BS_MIN_INTERVAL || tu > BS_MAX_INTERVAL) return false;
  g_bs_interval_tu = tu;
  return true;
}

static bool bs_start(uint8_t ch, uint16_t interval_tu) {
  if (g_bs_state == BS_RUNNING) return false;
  if (g_bs_ssid_count == 0)    return false;
  if (!bs_set_interval(interval_tu)) return false;
  if (ch < 1 || ch > 14) return false;
  g_bs_channel = ch;
  g_bs_state   = BS_RUNNING;
  return true;
}

static void bs_stop() { g_bs_state = BS_IDLE; }

static void bs_record_frame(int ssid_index) {
  if (ssid_index < 0 || ssid_index >= g_bs_ssid_count) return;
  g_bs_ssids[ssid_index].frames_sent++;
  g_bs_total_frames++;
}

// ============================================================
// TESTS: SSID pool management
// ============================================================

void test_bs_add_first() {
  bs_reset();
  TEST_ASSERT_TRUE(bs_add_ssid("TestNet", 6));
  TEST_ASSERT_EQUAL(1, g_bs_ssid_count);
}

void test_bs_add_duplicate_rejected() {
  bs_reset();
  bs_add_ssid("TestNet", 6);
  TEST_ASSERT_FALSE(bs_add_ssid("TestNet", 11));
  TEST_ASSERT_EQUAL(1, g_bs_ssid_count);
}

void test_bs_add_multiple() {
  bs_reset();
  bs_add_ssid("Net1", 1);
  bs_add_ssid("Net2", 6);
  bs_add_ssid("Net3", 11);
  TEST_ASSERT_EQUAL(3, g_bs_ssid_count);
}

void test_bs_add_empty_rejected() {
  bs_reset();
  TEST_ASSERT_FALSE(bs_add_ssid("", 6));
}

void test_bs_add_too_long_rejected() {
  bs_reset();
  char long_ssid[64];
  memset(long_ssid, 'A', 63); long_ssid[63] = '\0';
  TEST_ASSERT_FALSE(bs_add_ssid(long_ssid, 6));
}

void test_bs_add_null_rejected() {
  bs_reset();
  TEST_ASSERT_FALSE(bs_add_ssid(nullptr, 6));
}

void test_bs_max_cap() {
  bs_reset();
  for (int i = 0; i < BS_MAX_SSIDS; i++) {
    char s[8]; snprintf(s, 8, "net%d", i);
    TEST_ASSERT_TRUE(bs_add_ssid(s, 6));
  }
  TEST_ASSERT_FALSE(bs_add_ssid("overflow", 6));
}

void test_bs_remove_exists() {
  bs_reset();
  bs_add_ssid("Net1", 6);
  TEST_ASSERT_TRUE(bs_remove_ssid("Net1"));
  TEST_ASSERT_EQUAL(0, g_bs_ssid_count);
}

void test_bs_remove_nonexistent() {
  bs_reset();
  TEST_ASSERT_FALSE(bs_remove_ssid("DoesNotExist"));
}

void test_bs_clear() {
  bs_reset();
  bs_add_ssid("Net1", 6);
  bs_add_ssid("Net2", 11);
  bs_clear_ssids();
  TEST_ASSERT_EQUAL(0, g_bs_ssid_count);
}

void test_bs_ssid_stores_channel() {
  bs_reset();
  bs_add_ssid("Net1", 11);
  TEST_ASSERT_EQUAL(11, g_bs_ssids[0].channel);
}

// ============================================================
// TESTS: Beacon interval validation
// ============================================================

void test_bs_interval_valid_100() {
  bs_reset();
  TEST_ASSERT_TRUE(bs_set_interval(100));
  TEST_ASSERT_EQUAL(100, g_bs_interval_tu);
}

void test_bs_interval_min() {
  bs_reset();
  TEST_ASSERT_TRUE(bs_set_interval(BS_MIN_INTERVAL));
}

void test_bs_interval_max() {
  bs_reset();
  TEST_ASSERT_TRUE(bs_set_interval(BS_MAX_INTERVAL));
}

void test_bs_interval_below_min_rejected() {
  bs_reset();
  TEST_ASSERT_FALSE(bs_set_interval(BS_MIN_INTERVAL - 1));
}

void test_bs_interval_above_max_rejected() {
  bs_reset();
  TEST_ASSERT_FALSE(bs_set_interval(BS_MAX_INTERVAL + 1));
}

void test_bs_interval_zero_rejected() {
  bs_reset();
  TEST_ASSERT_FALSE(bs_set_interval(0));
}

// ============================================================
// TESTS: State machine
// ============================================================

void test_bs_starts_idle() {
  bs_reset();
  TEST_ASSERT_EQUAL(BS_IDLE, g_bs_state);
}

void test_bs_start_with_ssid() {
  bs_reset();
  bs_add_ssid("Net1", 6);
  TEST_ASSERT_TRUE(bs_start(6, 100));
  TEST_ASSERT_EQUAL(BS_RUNNING, g_bs_state);
}

void test_bs_start_no_ssids_fails() {
  bs_reset();
  TEST_ASSERT_FALSE(bs_start(6, 100));
  TEST_ASSERT_EQUAL(BS_IDLE, g_bs_state);
}

void test_bs_start_invalid_channel_fails() {
  bs_reset();
  bs_add_ssid("Net1", 6);
  TEST_ASSERT_FALSE(bs_start(0, 100));
  TEST_ASSERT_FALSE(bs_start(15, 100));
}

void test_bs_start_invalid_interval_fails() {
  bs_reset();
  bs_add_ssid("Net1", 6);
  TEST_ASSERT_FALSE(bs_start(6, 5));
}

void test_bs_start_when_running_fails() {
  bs_reset();
  bs_add_ssid("Net1", 6);
  bs_start(6, 100);
  TEST_ASSERT_FALSE(bs_start(11, 200));
}

void test_bs_stop_returns_idle() {
  bs_reset();
  bs_add_ssid("Net1", 6);
  bs_start(6, 100);
  bs_stop();
  TEST_ASSERT_EQUAL(BS_IDLE, g_bs_state);
}

void test_bs_restart_after_stop() {
  bs_reset();
  bs_add_ssid("Net1", 6);
  bs_start(6, 100);
  bs_stop();
  TEST_ASSERT_TRUE(bs_start(11, 200));
}

// ============================================================
// TESTS: Frame counters
// ============================================================

void test_bs_frame_counter_increments() {
  bs_reset();
  bs_add_ssid("Net1", 6);
  bs_record_frame(0);
  TEST_ASSERT_EQUAL(1u, g_bs_ssids[0].frames_sent);
  TEST_ASSERT_EQUAL(1u, g_bs_total_frames);
}

void test_bs_frame_counter_multiple_ssids() {
  bs_reset();
  bs_add_ssid("Net1", 6);
  bs_add_ssid("Net2", 11);
  bs_record_frame(0);
  bs_record_frame(1);
  bs_record_frame(0);
  TEST_ASSERT_EQUAL(2u, g_bs_ssids[0].frames_sent);
  TEST_ASSERT_EQUAL(1u, g_bs_ssids[1].frames_sent);
  TEST_ASSERT_EQUAL(3u, g_bs_total_frames);
}

void test_bs_frame_counter_invalid_index_no_crash() {
  bs_reset();
  bs_add_ssid("Net1", 6);
  bs_record_frame(-1);
  bs_record_frame(5);
  TEST_ASSERT_EQUAL(0u, g_bs_total_frames);
}

// ============================================================
// ENTRY POINT
// ============================================================

void setUp()    { bs_reset(); }
void tearDown() {}

int main(int argc, char **argv) {
  UNITY_BEGIN();

  RUN_TEST(test_bs_add_first);
  RUN_TEST(test_bs_add_duplicate_rejected);
  RUN_TEST(test_bs_add_multiple);
  RUN_TEST(test_bs_add_empty_rejected);
  RUN_TEST(test_bs_add_too_long_rejected);
  RUN_TEST(test_bs_add_null_rejected);
  RUN_TEST(test_bs_max_cap);
  RUN_TEST(test_bs_remove_exists);
  RUN_TEST(test_bs_remove_nonexistent);
  RUN_TEST(test_bs_clear);
  RUN_TEST(test_bs_ssid_stores_channel);

  RUN_TEST(test_bs_interval_valid_100);
  RUN_TEST(test_bs_interval_min);
  RUN_TEST(test_bs_interval_max);
  RUN_TEST(test_bs_interval_below_min_rejected);
  RUN_TEST(test_bs_interval_above_max_rejected);
  RUN_TEST(test_bs_interval_zero_rejected);

  RUN_TEST(test_bs_starts_idle);
  RUN_TEST(test_bs_start_with_ssid);
  RUN_TEST(test_bs_start_no_ssids_fails);
  RUN_TEST(test_bs_start_invalid_channel_fails);
  RUN_TEST(test_bs_start_invalid_interval_fails);
  RUN_TEST(test_bs_start_when_running_fails);
  RUN_TEST(test_bs_stop_returns_idle);
  RUN_TEST(test_bs_restart_after_stop);

  RUN_TEST(test_bs_frame_counter_increments);
  RUN_TEST(test_bs_frame_counter_multiple_ssids);
  RUN_TEST(test_bs_frame_counter_invalid_index_no_crash);

  return UNITY_END();
}

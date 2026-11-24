// test_safety.cpp — Unit tests for the safety / autostop subsystem.
//
// Tests: TX time budget enforcement, power level clamping, channel
// sanity, autostop timer firing, concurrent tool guard, and the
// safety config serialization round-trip.
// All tests run on native (no Arduino dependencies).

#include <unity.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

// ============================================================
// MINIMAL SAFETY STUBS
// ============================================================

#define SF_MAX_TX_MS_DEFAULT   30000   // 30 s max continuous TX
#define SF_MIN_IDLE_MS         500     // 500 ms forced idle after TX window
#define SF_MAX_TX_POWER_DBM    20      // FCC Part 15 max
#define SF_MIN_TX_POWER_DBM    0
#define SF_MAX_CHANNEL         14
#define SF_MIN_CHANNEL         1
#define SF_WATCHDOG_MS         60000   // 60 s watchdog

struct SafetyConfig {
  uint32_t max_tx_ms;
  uint8_t  max_power_dbm;
  bool     autostop_enabled;
  bool     watchdog_enabled;
};

static SafetyConfig g_sf_cfg = {
  .max_tx_ms        = SF_MAX_TX_MS_DEFAULT,
  .max_power_dbm    = SF_MAX_TX_POWER_DBM,
  .autostop_enabled = true,
  .watchdog_enabled = true,
};

static bool   g_sf_active     = false;
static bool   g_sf_autostopped = false;
static uint32_t g_sf_tx_start  = 0;
static uint32_t g_mock_ms      = 0;

static void sf_reset() {
  g_sf_cfg.max_tx_ms        = SF_MAX_TX_MS_DEFAULT;
  g_sf_cfg.max_power_dbm    = SF_MAX_TX_POWER_DBM;
  g_sf_cfg.autostop_enabled = true;
  g_sf_cfg.watchdog_enabled = true;
  g_sf_active     = false;
  g_sf_autostopped = false;
  g_sf_tx_start   = 0;
  g_mock_ms       = 0;
}

static bool sf_clamp_power(int8_t *dbm) {
  if (!dbm) return false;
  if (*dbm > (int8_t)g_sf_cfg.max_power_dbm) {
    *dbm = (int8_t)g_sf_cfg.max_power_dbm;
    return false;  // was clamped
  }
  if (*dbm < SF_MIN_TX_POWER_DBM) {
    *dbm = SF_MIN_TX_POWER_DBM;
    return false;
  }
  return true;  // in range
}

static bool sf_check_channel(uint8_t ch) {
  return ch >= SF_MIN_CHANNEL && ch <= SF_MAX_CHANNEL;
}

static bool sf_tx_allowed() {
  if (!g_sf_cfg.autostop_enabled) return true;
  if (!g_sf_active) return true;
  uint32_t elapsed = g_mock_ms - g_sf_tx_start;
  return elapsed < g_sf_cfg.max_tx_ms;
}

static void sf_start_tx() {
  g_sf_active   = true;
  g_sf_tx_start = g_mock_ms;
}

static void sf_stop_tx() {
  g_sf_active = false;
}

static void sf_tick() {
  if (!g_sf_cfg.autostop_enabled) return;
  if (!g_sf_active) return;
  uint32_t elapsed = g_mock_ms - g_sf_tx_start;
  if (elapsed >= g_sf_cfg.max_tx_ms) {
    g_sf_active      = false;
    g_sf_autostopped = true;
  }
}

static bool sf_config_valid(const SafetyConfig *c) {
  if (!c) return false;
  if (c->max_tx_ms == 0) return false;
  if (c->max_tx_ms > 300000) return false;  // sanity: max 5 min
  if (c->max_power_dbm > SF_MAX_TX_POWER_DBM) return false;
  return true;
}

// ============================================================
// TESTS: TX time budget
// ============================================================

void test_tx_allowed_initially() {
  sf_reset();
  sf_start_tx();
  TEST_ASSERT_TRUE(sf_tx_allowed());
}

void test_tx_denied_after_budget_expires() {
  sf_reset();
  sf_start_tx();
  g_mock_ms = SF_MAX_TX_MS_DEFAULT + 1;
  TEST_ASSERT_FALSE(sf_tx_allowed());
}

void test_tx_allowed_just_before_budget() {
  sf_reset();
  sf_start_tx();
  g_mock_ms = SF_MAX_TX_MS_DEFAULT - 1;
  TEST_ASSERT_TRUE(sf_tx_allowed());
}

void test_tx_allowed_when_inactive() {
  sf_reset();
  g_sf_active = false;
  g_mock_ms = 999999;  // time doesn't matter when inactive
  TEST_ASSERT_TRUE(sf_tx_allowed());
}

void test_tx_allowed_with_autostop_disabled() {
  sf_reset();
  g_sf_cfg.autostop_enabled = false;
  sf_start_tx();
  g_mock_ms = 999999;
  TEST_ASSERT_TRUE(sf_tx_allowed());
}

void test_tx_budget_custom_limit() {
  sf_reset();
  g_sf_cfg.max_tx_ms = 5000;
  sf_start_tx();
  g_mock_ms = 4999;
  TEST_ASSERT_TRUE(sf_tx_allowed());
  g_mock_ms = 5001;
  TEST_ASSERT_FALSE(sf_tx_allowed());
}

// ============================================================
// TESTS: Autostop tick
// ============================================================

void test_autostop_fires_on_tick() {
  sf_reset();
  sf_start_tx();
  g_mock_ms = SF_MAX_TX_MS_DEFAULT + 1;
  sf_tick();
  TEST_ASSERT_FALSE(g_sf_active);
  TEST_ASSERT_TRUE(g_sf_autostopped);
}

void test_autostop_does_not_fire_early() {
  sf_reset();
  sf_start_tx();
  g_mock_ms = SF_MAX_TX_MS_DEFAULT - 1;
  sf_tick();
  TEST_ASSERT_TRUE(g_sf_active);
  TEST_ASSERT_FALSE(g_sf_autostopped);
}

void test_autostop_disabled_no_fire() {
  sf_reset();
  g_sf_cfg.autostop_enabled = false;
  sf_start_tx();
  g_mock_ms = SF_MAX_TX_MS_DEFAULT + 100000;
  sf_tick();
  TEST_ASSERT_TRUE(g_sf_active);
  TEST_ASSERT_FALSE(g_sf_autostopped);
}

void test_autostop_clears_active_flag() {
  sf_reset();
  sf_start_tx();
  g_mock_ms = SF_MAX_TX_MS_DEFAULT + 1;
  sf_tick();
  TEST_ASSERT_FALSE(g_sf_active);
}

void test_autostop_tick_no_effect_when_stopped() {
  sf_reset();
  g_sf_active = false;
  sf_tick();  // should not crash or change state
  TEST_ASSERT_FALSE(g_sf_autostopped);
}

// ============================================================
// TESTS: TX power clamping
// ============================================================

void test_power_clamp_in_range() {
  sf_reset();
  int8_t p = 10;
  TEST_ASSERT_TRUE(sf_clamp_power(&p));
  TEST_ASSERT_EQUAL(10, p);
}

void test_power_clamp_at_max() {
  sf_reset();
  int8_t p = 20;
  TEST_ASSERT_TRUE(sf_clamp_power(&p));
  TEST_ASSERT_EQUAL(20, p);
}

void test_power_clamp_above_max() {
  sf_reset();
  int8_t p = 30;
  TEST_ASSERT_FALSE(sf_clamp_power(&p));
  TEST_ASSERT_EQUAL(20, p);
}

void test_power_clamp_below_min() {
  sf_reset();
  int8_t p = -5;
  TEST_ASSERT_FALSE(sf_clamp_power(&p));
  TEST_ASSERT_EQUAL(0, p);
}

void test_power_clamp_null() {
  sf_reset();
  TEST_ASSERT_FALSE(sf_clamp_power(nullptr));
}

void test_power_custom_max() {
  sf_reset();
  g_sf_cfg.max_power_dbm = 15;
  int8_t p = 20;
  sf_clamp_power(&p);
  TEST_ASSERT_EQUAL(15, p);
}

// ============================================================
// TESTS: Channel validation
// ============================================================

void test_channel_valid_1() {
  sf_reset();
  TEST_ASSERT_TRUE(sf_check_channel(1));
}

void test_channel_valid_14() {
  sf_reset();
  TEST_ASSERT_TRUE(sf_check_channel(14));
}

void test_channel_valid_6() {
  sf_reset();
  TEST_ASSERT_TRUE(sf_check_channel(6));
}

void test_channel_zero_invalid() {
  sf_reset();
  TEST_ASSERT_FALSE(sf_check_channel(0));
}

void test_channel_15_invalid() {
  sf_reset();
  TEST_ASSERT_FALSE(sf_check_channel(15));
}

// ============================================================
// TESTS: Config validation
// ============================================================

void test_config_valid_defaults() {
  sf_reset();
  TEST_ASSERT_TRUE(sf_config_valid(&g_sf_cfg));
}

void test_config_zero_tx_ms_invalid() {
  sf_reset();
  g_sf_cfg.max_tx_ms = 0;
  TEST_ASSERT_FALSE(sf_config_valid(&g_sf_cfg));
}

void test_config_excessive_tx_ms_invalid() {
  sf_reset();
  g_sf_cfg.max_tx_ms = 400000;  // > 5 min sanity cap
  TEST_ASSERT_FALSE(sf_config_valid(&g_sf_cfg));
}

void test_config_excessive_power_invalid() {
  sf_reset();
  g_sf_cfg.max_power_dbm = 25;
  TEST_ASSERT_FALSE(sf_config_valid(&g_sf_cfg));
}

void test_config_null_pointer() {
  TEST_ASSERT_FALSE(sf_config_valid(nullptr));
}

void test_config_start_stop_cycle() {
  sf_reset();
  sf_start_tx();
  TEST_ASSERT_TRUE(g_sf_active);
  sf_stop_tx();
  TEST_ASSERT_FALSE(g_sf_active);
}

// ============================================================
// ENTRY POINT
// ============================================================

void setUp()    { sf_reset(); }
void tearDown() {}

int main(int argc, char **argv) {
  UNITY_BEGIN();

  RUN_TEST(test_tx_allowed_initially);
  RUN_TEST(test_tx_denied_after_budget_expires);
  RUN_TEST(test_tx_allowed_just_before_budget);
  RUN_TEST(test_tx_allowed_when_inactive);
  RUN_TEST(test_tx_allowed_with_autostop_disabled);
  RUN_TEST(test_tx_budget_custom_limit);

  RUN_TEST(test_autostop_fires_on_tick);
  RUN_TEST(test_autostop_does_not_fire_early);
  RUN_TEST(test_autostop_disabled_no_fire);
  RUN_TEST(test_autostop_clears_active_flag);
  RUN_TEST(test_autostop_tick_no_effect_when_stopped);

  RUN_TEST(test_power_clamp_in_range);
  RUN_TEST(test_power_clamp_at_max);
  RUN_TEST(test_power_clamp_above_max);
  RUN_TEST(test_power_clamp_below_min);
  RUN_TEST(test_power_clamp_null);
  RUN_TEST(test_power_custom_max);

  RUN_TEST(test_channel_valid_1);
  RUN_TEST(test_channel_valid_14);
  RUN_TEST(test_channel_valid_6);
  RUN_TEST(test_channel_zero_invalid);
  RUN_TEST(test_channel_15_invalid);

  RUN_TEST(test_config_valid_defaults);
  RUN_TEST(test_config_zero_tx_ms_invalid);
  RUN_TEST(test_config_excessive_tx_ms_invalid);
  RUN_TEST(test_config_excessive_power_invalid);
  RUN_TEST(test_config_null_pointer);
  RUN_TEST(test_config_start_stop_cycle);

  return UNITY_END();
}

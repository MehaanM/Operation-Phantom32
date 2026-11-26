// test_config_manager.cpp — Tests for ConfigManager (Phase 6).

#include "unity.h"
#include "core/ConfigManager.h"

void setUp() {}
void tearDown() {}

// ── define / fieldCount ────────────────────────────────────────────────────────

static void test_define_bool_field() {
  ConfigManager cm;
  TEST_ASSERT_TRUE(cm.define("debug", false));
  TEST_ASSERT_EQUAL(1, (int)cm.fieldCount());
}

static void test_define_int_field() {
  ConfigManager cm;
  TEST_ASSERT_TRUE(cm.define("channel", (int32_t)6));
  TEST_ASSERT_EQUAL(1, (int)cm.fieldCount());
}

static void test_define_float_field() {
  ConfigManager cm;
  TEST_ASSERT_TRUE(cm.define("threshold", 0.5f));
  TEST_ASSERT_EQUAL(1, (int)cm.fieldCount());
}

static void test_define_string_field() {
  ConfigManager cm;
  TEST_ASSERT_TRUE(cm.define("ssid", "phantom"));
  TEST_ASSERT_EQUAL(1, (int)cm.fieldCount());
}

static void test_define_null_key_rejected() {
  ConfigManager cm;
  TEST_ASSERT_FALSE(cm.define(nullptr, (int32_t)1));
}

static void test_redefine_updates_default() {
  ConfigManager cm;
  cm.define("x", (int32_t)10);
  cm.define("x", (int32_t)20);
  TEST_ASSERT_EQUAL(1, (int)cm.fieldCount());
  int32_t v = 0;
  cm.getInt("x", &v);
  TEST_ASSERT_EQUAL_INT32(20, v);
}

// ── get / set ─────────────────────────────────────────────────────────────────

static void test_set_and_get_bool() {
  ConfigManager cm;
  cm.define("dbg", false);
  TEST_ASSERT_TRUE(cm.setBool("dbg", true));
  bool b = false;
  TEST_ASSERT_TRUE(cm.getBool("dbg", &b));
  TEST_ASSERT_TRUE(b);
}

static void test_set_and_get_int() {
  ConfigManager cm;
  cm.define("ch", (int32_t)1);
  cm.setInt("ch", 11);
  int32_t v = 0;
  cm.getInt("ch", &v);
  TEST_ASSERT_EQUAL_INT32(11, v);
}

static void test_set_and_get_float() {
  ConfigManager cm;
  cm.define("gain", 1.0f);
  cm.setFloat("gain", 2.5f);
  float f = 0.0f;
  cm.getFloat("gain", &f);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.5f, f);
}

static void test_set_and_get_string() {
  ConfigManager cm;
  cm.define("name", "default");
  cm.setString("name", "phantom32");
  char buf[32];
  cm.getString("name", buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("phantom32", buf);
}

static void test_set_unknown_key_fails() {
  ConfigManager cm;
  TEST_ASSERT_FALSE(cm.setInt("missing", 5));
}

static void test_get_null_out_ptr_fails() {
  ConfigManager cm;
  cm.define("x", (int32_t)1);
  TEST_ASSERT_FALSE(cm.getInt("x", nullptr));
}

// ── Range validation ───────────────────────────────────────────────────────────

static void test_range_accepts_in_bounds() {
  ConfigManager cm;
  cm.define("ch", (int32_t)6);
  cm.setRange("ch", 1, 13);
  TEST_ASSERT_TRUE(cm.setInt("ch", 1));
  TEST_ASSERT_TRUE(cm.setInt("ch", 13));
}

static void test_range_rejects_out_of_bounds() {
  ConfigManager cm;
  cm.define("ch", (int32_t)6);
  cm.setRange("ch", 1, 13);
  TEST_ASSERT_FALSE(cm.setInt("ch", 0));
  TEST_ASSERT_FALSE(cm.setInt("ch", 14));
}

static void test_float_range_validation() {
  ConfigManager cm;
  cm.define("power", 0.5f);
  cm.setRangeF("power", 0.0f, 1.0f);
  TEST_ASSERT_TRUE(cm.setFloat("power", 0.0f));
  TEST_ASSERT_FALSE(cm.setFloat("power", 1.1f));
}

// ── Enum validation ────────────────────────────────────────────────────────────

static void test_enum_accepts_valid_value() {
  ConfigManager cm;
  cm.define("mode", (int32_t)1);
  const int32_t vals[] = {1, 2, 3};
  cm.setEnum("mode", vals, 3);
  TEST_ASSERT_TRUE(cm.setInt("mode", 2));
}

static void test_enum_rejects_invalid_value() {
  ConfigManager cm;
  cm.define("mode", (int32_t)1);
  const int32_t vals[] = {1, 2, 3};
  cm.setEnum("mode", vals, 3);
  TEST_ASSERT_FALSE(cm.setInt("mode", 99));
}

// ── Lock ──────────────────────────────────────────────────────────────────────

static void test_locked_field_rejects_writes() {
  ConfigManager cm;
  cm.define("board", (int32_t)1);
  cm.lock("board");
  TEST_ASSERT_FALSE(cm.setInt("board", 2));
  TEST_ASSERT_TRUE(cm.isLocked("board"));
}

static void test_lock_null_key_fails() {
  ConfigManager cm;
  TEST_ASSERT_FALSE(cm.lock(nullptr));
}

// ── Reset / profiles ──────────────────────────────────────────────────────────

static void test_reset_to_defaults() {
  ConfigManager cm;
  cm.define("x", (int32_t)5);
  cm.setInt("x", 99);
  cm.resetToDefaults();
  int32_t v = 0;
  cm.getInt("x", &v);
  TEST_ASSERT_EQUAL_INT32(5, v);
}

static void test_save_and_load_profile() {
  ConfigManager cm;
  cm.define("x", (int32_t)1);
  cm.setInt("x", 42);
  cm.saveProfile(0);
  cm.setInt("x", 99);
  cm.loadProfile(0);
  int32_t v = 0;
  cm.getInt("x", &v);
  TEST_ASSERT_EQUAL_INT32(42, v);
}

static void test_load_invalid_profile_fails() {
  ConfigManager cm;
  TEST_ASSERT_FALSE(cm.loadProfile(0));  // never saved
}

static void test_load_slot_out_of_range_fails() {
  ConfigManager cm;
  TEST_ASSERT_FALSE(cm.loadProfile(PHANTOM_CM_MAX_PROFILES));
}

// ── Checksum ──────────────────────────────────────────────────────────────────

static void test_checksum_changes_on_set() {
  ConfigManager cm;
  cm.define("x", (int32_t)1);
  uint32_t before = cm.checksum();
  cm.setInt("x", 2);
  TEST_ASSERT_NOT_EQUAL(before, cm.checksum());
}

static void test_checksum_stable_without_changes() {
  ConfigManager cm;
  cm.define("x", (int32_t)1);
  TEST_ASSERT_EQUAL_UINT32(cm.checksum(), cm.checksum());
}

// ── Dirty tracking ────────────────────────────────────────────────────────────

static void test_dirty_after_set() {
  ConfigManager cm;
  cm.define("x", (int32_t)1);
  cm.setInt("x", 2);
  TEST_ASSERT_TRUE(cm.isDirty("x"));
  TEST_ASSERT_EQUAL(1, (int)cm.dirtyCount());
}

static void test_clear_dirty() {
  ConfigManager cm;
  cm.define("x", (int32_t)1);
  cm.setInt("x", 2);
  cm.clearDirty();
  TEST_ASSERT_FALSE(cm.isDirty("x"));
  TEST_ASSERT_EQUAL(0, (int)cm.dirtyCount());
}

// ── Change listener ───────────────────────────────────────────────────────────

static int gListenerCount = 0;
static void onConfigChange(const char *, const ConfigValue &, const ConfigValue &) {
  ++gListenerCount;
}

static void test_listener_fires_on_change() {
  gListenerCount = 0;
  ConfigManager cm;
  cm.define("x", (int32_t)1);
  cm.addListener(onConfigChange);
  cm.setInt("x", 2);
  TEST_ASSERT_EQUAL(1, gListenerCount);
}

static void test_listener_null_rejected() {
  ConfigManager cm;
  TEST_ASSERT_FALSE(cm.addListener(nullptr));
}

// ── Cross-field validation ────────────────────────────────────────────────────

static bool crossRejectNegative(const char *, const ConfigValue &v, void *) {
  return v.type == ConfigType::INT32 ? v.raw.i >= 0 : true;
}

static void test_cross_validator_blocks_invalid() {
  ConfigManager cm;
  cm.define("count", (int32_t)5);
  cm.addCrossValidator(crossRejectNegative);
  TEST_ASSERT_FALSE(cm.setInt("count", -1));
}

static void test_cross_validator_allows_valid() {
  ConfigManager cm;
  cm.define("count", (int32_t)5);
  cm.addCrossValidator(crossRejectNegative);
  TEST_ASSERT_TRUE(cm.setInt("count", 10));
}

// ── validateAll ───────────────────────────────────────────────────────────────

static void test_validate_all_passes_clean_config() {
  ConfigManager cm;
  cm.define("ch", (int32_t)6);
  cm.setRange("ch", 1, 13);
  cm.define("dbg", false);
  TEST_ASSERT_TRUE(cm.validateAll());
}

// ── Registry full ─────────────────────────────────────────────────────────────

static void test_registry_full_returns_false() {
  ConfigManager cm;
  for (size_t i = 0; i < PHANTOM_CM_MAX_FIELDS; ++i) {
    char key[8];
    key[0] = (char)('a' + (i % 26));
    key[1] = (char)('0' + i);
    key[2] = '\0';
    cm.define(key, (int32_t)0);
  }
  TEST_ASSERT_FALSE(cm.define("overflow", (int32_t)1));
}

// ── fieldFor ─────────────────────────────────────────────────────────────────

static void test_field_for_returns_field() {
  ConfigManager cm;
  cm.define("x", (int32_t)7);
  const ConfigField *f = cm.fieldFor("x");
  TEST_ASSERT_NOT_NULL(f);
  TEST_ASSERT_EQUAL_INT32(7, f->currentVal.raw.i);
}

static void test_field_for_null_returns_null() {
  ConfigManager cm;
  TEST_ASSERT_NULL(cm.fieldFor(nullptr));
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_define_bool_field);
  RUN_TEST(test_define_int_field);
  RUN_TEST(test_define_float_field);
  RUN_TEST(test_define_string_field);
  RUN_TEST(test_define_null_key_rejected);
  RUN_TEST(test_redefine_updates_default);

  RUN_TEST(test_set_and_get_bool);
  RUN_TEST(test_set_and_get_int);
  RUN_TEST(test_set_and_get_float);
  RUN_TEST(test_set_and_get_string);
  RUN_TEST(test_set_unknown_key_fails);
  RUN_TEST(test_get_null_out_ptr_fails);

  RUN_TEST(test_range_accepts_in_bounds);
  RUN_TEST(test_range_rejects_out_of_bounds);
  RUN_TEST(test_float_range_validation);

  RUN_TEST(test_enum_accepts_valid_value);
  RUN_TEST(test_enum_rejects_invalid_value);

  RUN_TEST(test_locked_field_rejects_writes);
  RUN_TEST(test_lock_null_key_fails);

  RUN_TEST(test_reset_to_defaults);
  RUN_TEST(test_save_and_load_profile);
  RUN_TEST(test_load_invalid_profile_fails);
  RUN_TEST(test_load_slot_out_of_range_fails);

  RUN_TEST(test_checksum_changes_on_set);
  RUN_TEST(test_checksum_stable_without_changes);

  RUN_TEST(test_dirty_after_set);
  RUN_TEST(test_clear_dirty);

  RUN_TEST(test_listener_fires_on_change);
  RUN_TEST(test_listener_null_rejected);

  RUN_TEST(test_cross_validator_blocks_invalid);
  RUN_TEST(test_cross_validator_allows_valid);

  RUN_TEST(test_validate_all_passes_clean_config);
  RUN_TEST(test_registry_full_returns_false);
  RUN_TEST(test_field_for_returns_field);
  RUN_TEST(test_field_for_null_returns_null);

  return UNITY_END();
}

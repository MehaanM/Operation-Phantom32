// test_plugin_registry.cpp — Tests for PluginRegistry (Phase 15).

#include "unity.h"
#include "core/PluginRegistry.h"

void setUp() {}
void tearDown() {}

// ── Helpers ───────────────────────────────────────────────────────────────────

static PluginDescriptor makePlugin(const char *name, uint32_t apiVer = 1,
                                   uint32_t memBytes = 4096) {
  PluginDescriptor d = {};
  strncpy(d.name, name, sizeof(d.name) - 1);
  strncpy(d.version, "1.0.0", sizeof(d.version) - 1);
  strncpy(d.author, "test", sizeof(d.author) - 1);
  d.apiVersion = apiVer;
  d.memoryBudgetBytes = memBytes;
  d.cpuBudgetUs = 1000;
  d.sandboxed = true;
  d.valid = true;
  return d;
}

// ── registerPlugin ────────────────────────────────────────────────────────────

static void test_register_valid_plugin() {
  PluginRegistry pr;
  PluginDescriptor d = makePlugin("scanner");
  TEST_ASSERT_TRUE(pr.registerPlugin(d));
  TEST_ASSERT_EQUAL(1, (int)pr.pluginCount());
}

static void test_register_invalid_plugin_fails() {
  PluginRegistry pr;
  PluginDescriptor d = makePlugin("x");
  d.valid = false;
  TEST_ASSERT_FALSE(pr.registerPlugin(d));
}

static void test_register_empty_name_fails() {
  PluginRegistry pr;
  PluginDescriptor d = makePlugin("x");
  d.name[0] = '\0';
  TEST_ASSERT_FALSE(pr.registerPlugin(d));
}

static void test_register_incompatible_api_fails() {
  PluginRegistry pr;
  PluginDescriptor d = makePlugin("future_plugin", PHANTOM_PR_API_VERSION + 1);
  TEST_ASSERT_FALSE(pr.registerPlugin(d));
}

static void test_register_oversized_budget_fails() {
  PluginRegistry pr;
  PluginDescriptor d = makePlugin("bigmem", 1, PHANTOM_PR_MAX_MEMORY_BYTES + 1);
  TEST_ASSERT_FALSE(pr.registerPlugin(d));
}

static void test_register_updates_existing() {
  PluginRegistry pr;
  PluginDescriptor d = makePlugin("scanner");
  pr.registerPlugin(d);
  d.cpuBudgetUs = 2000;
  TEST_ASSERT_TRUE(pr.registerPlugin(d));
  TEST_ASSERT_EQUAL(1, (int)pr.pluginCount());
}

static void test_register_full_registry_fails() {
  PluginRegistry pr;
  for (size_t i = 0; i < PHANTOM_PR_MAX_PLUGINS; ++i) {
    char name[8];
    name[0] = (char)('a' + i);
    name[1] = '\0';
    pr.registerPlugin(makePlugin(name));
  }
  TEST_ASSERT_FALSE(pr.registerPlugin(makePlugin("overflow")));
}

// ── validate ──────────────────────────────────────────────────────────────────

static void test_validate_valid_plugin() {
  PluginRegistry pr;
  PluginDescriptor d = makePlugin("scanner");
  TEST_ASSERT_TRUE(pr.validate(d));
}

static void test_validate_invalid_plugin() {
  PluginRegistry pr;
  PluginDescriptor d = makePlugin("scanner");
  d.valid = false;
  TEST_ASSERT_FALSE(pr.validate(d));
}

static void test_validate_future_api_fails() {
  PluginRegistry pr;
  PluginDescriptor d = makePlugin("x", PHANTOM_PR_API_VERSION + 1);
  TEST_ASSERT_FALSE(pr.validate(d));
}

// ── activate / suspend ────────────────────────────────────────────────────────

static void test_activate_registered_plugin() {
  PluginRegistry pr;
  pr.registerPlugin(makePlugin("scanner"));
  TEST_ASSERT_TRUE(pr.activate("scanner"));
  TEST_ASSERT_EQUAL(1, (int)pr.activeCount());
  const PluginDescriptor *p = pr.descriptorFor("scanner");
  TEST_ASSERT_NOT_NULL(p);
  TEST_ASSERT_EQUAL((int)PluginState::ACTIVE, (int)p->state);
}

static void test_activate_unknown_fails() {
  PluginRegistry pr;
  TEST_ASSERT_FALSE(pr.activate("missing"));
}

static void test_suspend_active_plugin() {
  PluginRegistry pr;
  pr.registerPlugin(makePlugin("scanner"));
  pr.activate("scanner");
  TEST_ASSERT_TRUE(pr.suspend("scanner"));
  TEST_ASSERT_EQUAL(0, (int)pr.activeCount());
  TEST_ASSERT_EQUAL((int)PluginState::SUSPENDED, (int)pr.descriptorFor("scanner")->state);
}

static void test_suspend_non_active_fails() {
  PluginRegistry pr;
  pr.registerPlugin(makePlugin("scanner")); // REGISTERED, not ACTIVE
  TEST_ASSERT_FALSE(pr.suspend("scanner"));
}

static void test_suspend_unknown_fails() {
  PluginRegistry pr;
  TEST_ASSERT_FALSE(pr.suspend("missing"));
}

// ── permissions ───────────────────────────────────────────────────────────────

static void test_has_permission_false_initially() {
  PluginRegistry pr;
  pr.registerPlugin(makePlugin("scanner"));
  TEST_ASSERT_FALSE(pr.hasPermission("scanner", PluginPermission::NETWORK_WRITE));
}

static void test_grant_permission() {
  PluginRegistry pr;
  pr.registerPlugin(makePlugin("scanner"));
  TEST_ASSERT_TRUE(pr.grantPermission("scanner", PluginPermission::NETWORK_READ));
  TEST_ASSERT_TRUE(pr.hasPermission("scanner", PluginPermission::NETWORK_READ));
}

static void test_grant_duplicate_permission_ok() {
  PluginRegistry pr;
  pr.registerPlugin(makePlugin("scanner"));
  pr.grantPermission("scanner", PluginPermission::STORAGE_READ);
  TEST_ASSERT_TRUE(pr.grantPermission("scanner", PluginPermission::STORAGE_READ));
}

static void test_grant_permission_unknown_fails() {
  PluginRegistry pr;
  TEST_ASSERT_FALSE(pr.grantPermission("missing", PluginPermission::NETWORK_READ));
}

static void test_revoke_permission() {
  PluginRegistry pr;
  pr.registerPlugin(makePlugin("scanner"));
  pr.grantPermission("scanner", PluginPermission::HARDWARE_ACCESS);
  TEST_ASSERT_TRUE(pr.revokePermission("scanner", PluginPermission::HARDWARE_ACCESS));
  TEST_ASSERT_FALSE(pr.hasPermission("scanner", PluginPermission::HARDWARE_ACCESS));
}

static void test_revoke_non_held_permission_false() {
  PluginRegistry pr;
  pr.registerPlugin(makePlugin("scanner"));
  TEST_ASSERT_FALSE(pr.revokePermission("scanner", PluginPermission::DIAGNOSTIC_READ));
}

static void test_revoke_permission_unknown_plugin_false() {
  PluginRegistry pr;
  TEST_ASSERT_FALSE(pr.revokePermission("missing", PluginPermission::NETWORK_READ));
}

static void test_has_permission_null_plugin_false() {
  PluginRegistry pr;
  TEST_ASSERT_FALSE(pr.hasPermission(nullptr, PluginPermission::NETWORK_READ));
}

static void test_descriptor_for_null_returns_null() {
  PluginRegistry pr;
  TEST_ASSERT_NULL(pr.descriptorFor(nullptr));
}

// ── isCompatible ──────────────────────────────────────────────────────────────

static void test_is_compatible_valid() {
  PluginRegistry pr;
  PluginDescriptor d = makePlugin("x");
  TEST_ASSERT_TRUE(pr.isCompatible(d));
}

static void test_is_compatible_invalid_struct() {
  PluginRegistry pr;
  PluginDescriptor d = makePlugin("x");
  d.valid = false;
  TEST_ASSERT_FALSE(pr.isCompatible(d));
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_register_valid_plugin);
  RUN_TEST(test_register_invalid_plugin_fails);
  RUN_TEST(test_register_empty_name_fails);
  RUN_TEST(test_register_incompatible_api_fails);
  RUN_TEST(test_register_oversized_budget_fails);
  RUN_TEST(test_register_updates_existing);
  RUN_TEST(test_register_full_registry_fails);

  RUN_TEST(test_validate_valid_plugin);
  RUN_TEST(test_validate_invalid_plugin);
  RUN_TEST(test_validate_future_api_fails);

  RUN_TEST(test_activate_registered_plugin);
  RUN_TEST(test_activate_unknown_fails);
  RUN_TEST(test_suspend_active_plugin);
  RUN_TEST(test_suspend_non_active_fails);
  RUN_TEST(test_suspend_unknown_fails);

  RUN_TEST(test_has_permission_false_initially);
  RUN_TEST(test_grant_permission);
  RUN_TEST(test_grant_duplicate_permission_ok);
  RUN_TEST(test_grant_permission_unknown_fails);
  RUN_TEST(test_revoke_permission);
  RUN_TEST(test_revoke_non_held_permission_false);
  RUN_TEST(test_revoke_permission_unknown_plugin_false);
  RUN_TEST(test_has_permission_null_plugin_false);
  RUN_TEST(test_descriptor_for_null_returns_null);

  RUN_TEST(test_is_compatible_valid);
  RUN_TEST(test_is_compatible_invalid_struct);

  return UNITY_END();
}

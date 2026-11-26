// test_ui_framework.cpp — Tests for UIFramework (Phase 9).

#include "unity.h"
#include "ui/UIFramework.h"

void setUp() {}
void tearDown() {}

// ── UITheme ───────────────────────────────────────────────────────────────────

static void test_light_theme_mode() {
  UITheme t = UITheme::light();
  TEST_ASSERT_EQUAL((int)UIThemeMode::LIGHT, (int)t.mode);
}

static void test_dark_theme_mode() {
  UITheme t = UITheme::dark();
  TEST_ASSERT_EQUAL((int)UIThemeMode::DARK, (int)t.mode);
}

static void test_high_contrast_theme_mode() {
  UITheme t = UITheme::highContrast();
  TEST_ASSERT_EQUAL((int)UIThemeMode::HIGH_CONTRAST, (int)t.mode);
}

static void test_themes_have_distinct_backgrounds() {
  TEST_ASSERT_NOT_EQUAL(UITheme::light().colors.background,
                        UITheme::dark().colors.background);
}

// ── UIComponent ───────────────────────────────────────────────────────────────

static void test_component_default_state_idle() {
  UIComponent c;
  TEST_ASSERT_EQUAL((int)UIState::IDLE, (int)c.getState());
}

static void test_component_set_state() {
  UIComponent c;
  c.setState(UIState::LOADING);
  TEST_ASSERT_EQUAL((int)UIState::LOADING, (int)c.getState());
  TEST_ASSERT_TRUE(c.isLoading());
}

static void test_component_error_state() {
  UIComponent c;
  c.setState(UIState::ERROR);
  TEST_ASSERT_TRUE(c.isError());
  TEST_ASSERT_FALSE(c.isLoading());
}

static void test_component_success_state() {
  UIComponent c;
  c.setState(UIState::SUCCESS);
  TEST_ASSERT_TRUE(c.isSuccess());
}

static void test_component_visibility() {
  UIComponent c;
  TEST_ASSERT_TRUE(c.visible);
  c.hide();
  TEST_ASSERT_FALSE(c.visible);
  c.show();
  TEST_ASSERT_TRUE(c.visible);
}

static void test_component_enabled() {
  UIComponent c;
  TEST_ASSERT_TRUE(c.enabled);
  c.disable();
  TEST_ASSERT_FALSE(c.enabled);
  c.enable();
  TEST_ASSERT_TRUE(c.enabled);
}

static void test_component_set_content() {
  UIComponent c;
  c.setContent("Scanning...");
  TEST_ASSERT_EQUAL_STRING("Scanning...", c.content);
}

// ── UIScreen ──────────────────────────────────────────────────────────────────

static UIComponent makeComponent(const char *id) {
  UIComponent c;
  strncpy(c.id, id, sizeof(c.id) - 1);
  return c;
}

static void test_screen_add_component() {
  UIScreen s;
  TEST_ASSERT_TRUE(s.addComponent(makeComponent("btn1")));
  TEST_ASSERT_EQUAL(1, (int)s.componentCount());
}

static void test_screen_find_component() {
  UIScreen s;
  s.addComponent(makeComponent("label"));
  UIComponent *c = s.findComponent("label");
  TEST_ASSERT_NOT_NULL(c);
  TEST_ASSERT_EQUAL_STRING("label", c->id);
}

static void test_screen_find_missing_returns_null() {
  UIScreen s;
  TEST_ASSERT_NULL(s.findComponent("ghost"));
}

static void test_screen_find_null_id_safe() {
  UIScreen s;
  TEST_ASSERT_NULL(s.findComponent(nullptr));
}

static void test_screen_full_rejects_add() {
  UIScreen s;
  for (size_t i = 0; i < PHANTOM_UI_MAX_COMPONENTS; ++i) {
    char id[8];
    id[0] = (char)('a' + i % 26);
    id[1] = '\0';
    // Avoid re-using the same ID; suffix by index.
    id[1] = (char)('0' + i % 10);
    id[2] = '\0';
    s.addComponent(makeComponent(id));
  }
  TEST_ASSERT_FALSE(s.addComponent(makeComponent("overflow")));
}

static void test_screen_visible_count() {
  UIScreen s;
  UIComponent a = makeComponent("a");
  UIComponent b = makeComponent("b");
  b.hide();
  s.addComponent(a);
  s.addComponent(b);
  TEST_ASSERT_EQUAL(1, (int)s.visibleCount());
}

static void test_screen_count_in_state() {
  UIScreen s;
  UIComponent a = makeComponent("a");
  UIComponent b = makeComponent("b");
  a.setState(UIState::LOADING);
  b.setState(UIState::LOADING);
  s.addComponent(a);
  s.addComponent(b);
  TEST_ASSERT_EQUAL(2, (int)s.countInState(UIState::LOADING));
}

static void test_screen_has_error() {
  UIScreen s;
  UIComponent c = makeComponent("err");
  c.setState(UIState::ERROR);
  s.addComponent(c);
  TEST_ASSERT_TRUE(s.hasError());
}

static void test_screen_is_loading() {
  UIScreen s;
  UIComponent c = makeComponent("spinner");
  c.setState(UIState::LOADING);
  s.addComponent(c);
  TEST_ASSERT_TRUE(s.isLoading());
}

static void test_screen_set_theme() {
  UIScreen s;
  s.setTheme(UITheme::dark());
  TEST_ASSERT_EQUAL((int)UIThemeMode::DARK, (int)s.theme().mode);
}

// ── UINavigator ───────────────────────────────────────────────────────────────

static void test_navigator_push_screen() {
  UINavigator nav;
  UIScreen sc;
  TEST_ASSERT_TRUE(nav.pushScreen(&sc));
  TEST_ASSERT_EQUAL(1, (int)nav.depth());
}

static void test_navigator_current_screen() {
  UINavigator nav;
  UIScreen sc;
  nav.pushScreen(&sc);
  TEST_ASSERT_EQUAL_PTR(&sc, nav.currentScreen());
}

static void test_navigator_pop_screen() {
  UINavigator nav;
  UIScreen s1, s2;
  nav.pushScreen(&s1);
  nav.pushScreen(&s2);
  TEST_ASSERT_TRUE(nav.popScreen());
  TEST_ASSERT_EQUAL_PTR(&s1, nav.currentScreen());
  TEST_ASSERT_EQUAL(1, (int)nav.depth());
}

static void test_navigator_pop_empty_fails() {
  UINavigator nav;
  TEST_ASSERT_FALSE(nav.popScreen());
}

static void test_navigator_current_on_empty_null() {
  UINavigator nav;
  TEST_ASSERT_NULL(nav.currentScreen());
}

static void test_navigator_reset() {
  UINavigator nav;
  UIScreen s1, s2;
  nav.pushScreen(&s1);
  nav.pushScreen(&s2);
  nav.reset();
  TEST_ASSERT_EQUAL(0, (int)nav.depth());
  TEST_ASSERT_NULL(nav.currentScreen());
}

static void test_navigator_stack_full_rejects_push() {
  UINavigator nav;
  UIScreen screens[PHANTOM_UI_STACK_DEPTH + 1];
  for (size_t i = 0; i < PHANTOM_UI_STACK_DEPTH; ++i)
    nav.pushScreen(&screens[i]);
  TEST_ASSERT_FALSE(nav.pushScreen(&screens[PHANTOM_UI_STACK_DEPTH]));
}

static void test_navigator_push_null_fails() {
  UINavigator nav;
  TEST_ASSERT_FALSE(nav.pushScreen(nullptr));
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_light_theme_mode);
  RUN_TEST(test_dark_theme_mode);
  RUN_TEST(test_high_contrast_theme_mode);
  RUN_TEST(test_themes_have_distinct_backgrounds);

  RUN_TEST(test_component_default_state_idle);
  RUN_TEST(test_component_set_state);
  RUN_TEST(test_component_error_state);
  RUN_TEST(test_component_success_state);
  RUN_TEST(test_component_visibility);
  RUN_TEST(test_component_enabled);
  RUN_TEST(test_component_set_content);

  RUN_TEST(test_screen_add_component);
  RUN_TEST(test_screen_find_component);
  RUN_TEST(test_screen_find_missing_returns_null);
  RUN_TEST(test_screen_find_null_id_safe);
  RUN_TEST(test_screen_full_rejects_add);
  RUN_TEST(test_screen_visible_count);
  RUN_TEST(test_screen_count_in_state);
  RUN_TEST(test_screen_has_error);
  RUN_TEST(test_screen_is_loading);
  RUN_TEST(test_screen_set_theme);

  RUN_TEST(test_navigator_push_screen);
  RUN_TEST(test_navigator_current_screen);
  RUN_TEST(test_navigator_pop_screen);
  RUN_TEST(test_navigator_pop_empty_fails);
  RUN_TEST(test_navigator_current_on_empty_null);
  RUN_TEST(test_navigator_reset);
  RUN_TEST(test_navigator_stack_full_rejects_push);
  RUN_TEST(test_navigator_push_null_fails);

  return UNITY_END();
}

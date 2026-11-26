#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// UIFramework — testable UI state-machine framework (Phase 9).
//
// Provides a platform-agnostic component/screen/navigator model. All logic is
// pure state: no rendering takes place here — that is left to platform-specific
// adapters. The framework can therefore be fully tested in native environments.
//
// Layers:
//   UITheme        — colour tokens for light/dark/high-contrast modes
//   UIComponent    — smallest renderable unit; has a state, visibility, content
//   UIScreen       — ordered collection of components with a shared theme
//   UINavigator    — screen stack with push/pop navigation

static constexpr size_t PHANTOM_UI_MAX_COMPONENTS = 16;
static constexpr size_t PHANTOM_UI_STACK_DEPTH = 8;

// ── Theme ─────────────────────────────────────────────────────────────────────

enum class UIThemeMode : uint8_t { LIGHT, DARK, HIGH_CONTRAST };

struct UIThemeColors {
  uint32_t background;
  uint32_t foreground;
  uint32_t accent;
  uint32_t warning;
  uint32_t error;
};

struct UITheme {
  UIThemeMode mode;
  UIThemeColors colors;

  static UITheme light() {
    return {UIThemeMode::LIGHT, {0xFFFFFFu, 0x111111u, 0x0057FFu, 0xFFAA00u, 0xDD2222u}};
  }
  static UITheme dark() {
    return {UIThemeMode::DARK, {0x1A1A2Eu, 0xE0E0E0u, 0x4A9EFFu, 0xFFCC44u, 0xFF4444u}};
  }
  static UITheme highContrast() {
    return {UIThemeMode::HIGH_CONTRAST, {0x000000u, 0xFFFFFFu, 0xFFFF00u, 0xFF8800u, 0xFF0000u}};
  }
};

// ── Component state ───────────────────────────────────────────────────────────

enum class UIState : uint8_t { IDLE, LOADING, ERROR, SUCCESS, CONFIRMATION };

struct UIComponent {
  char id[24];
  UIState state;
  bool visible;
  bool enabled;
  char content[64];

  UIComponent() : state(UIState::IDLE), visible(true), enabled(true) {
    id[0] = '\0';
    content[0] = '\0';
  }

  void setState(UIState s) { state = s; }
  UIState getState() const { return state; }
  void show() { visible = true; }
  void hide() { visible = false; }
  void enable() { enabled = true; }
  void disable() { enabled = false; }
  void setContent(const char *s) {
    if (s)
      strncpy(content, s, sizeof(content) - 1);
  }

  bool isLoading() const { return state == UIState::LOADING; }
  bool isError() const { return state == UIState::ERROR; }
  bool isSuccess() const { return state == UIState::SUCCESS; }
};

// ── Screen ────────────────────────────────────────────────────────────────────

class UIScreen {
 public:
  UIScreen() : count_(0), theme_(UITheme::light()) {}

  bool addComponent(const UIComponent &c) {
    if (count_ >= PHANTOM_UI_MAX_COMPONENTS)
      return false;
    components_[count_++] = c;
    return true;
  }

  UIComponent *findComponent(const char *id) {
    if (!id)
      return nullptr;
    for (size_t i = 0; i < count_; ++i)
      if (strncmp(components_[i].id, id, sizeof(components_[i].id)) == 0)
        return &components_[i];
    return nullptr;
  }

  const UIComponent *findComponent(const char *id) const {
    if (!id)
      return nullptr;
    for (size_t i = 0; i < count_; ++i)
      if (strncmp(components_[i].id, id, sizeof(components_[i].id)) == 0)
        return &components_[i];
    return nullptr;
  }

  size_t componentCount() const { return count_; }

  void setTheme(const UITheme &t) { theme_ = t; }
  const UITheme &theme() const { return theme_; }

  // Count visible components.
  size_t visibleCount() const {
    size_t n = 0;
    for (size_t i = 0; i < count_; ++i)
      if (components_[i].visible)
        ++n;
    return n;
  }

  // Count components in a given state.
  size_t countInState(UIState s) const {
    size_t n = 0;
    for (size_t i = 0; i < count_; ++i)
      if (components_[i].state == s)
        ++n;
    return n;
  }

  // Returns true if any component is in ERROR state.
  bool hasError() const { return countInState(UIState::ERROR) > 0; }

  // Returns true if any component is in LOADING state.
  bool isLoading() const { return countInState(UIState::LOADING) > 0; }

 private:
  UIComponent components_[PHANTOM_UI_MAX_COMPONENTS] = {};
  size_t count_ = 0;
  UITheme theme_;
};

// ── Navigator ─────────────────────────────────────────────────────────────────

class UINavigator {
 public:
  UINavigator() : depth_(0) {}

  bool pushScreen(UIScreen *screen) {
    if (!screen || depth_ >= PHANTOM_UI_STACK_DEPTH)
      return false;
    stack_[depth_++] = screen;
    return true;
  }

  bool popScreen() {
    if (depth_ == 0)
      return false;
    --depth_;
    return true;
  }

  UIScreen *currentScreen() const { return depth_ > 0 ? stack_[depth_ - 1] : nullptr; }

  size_t depth() const { return depth_; }

  void reset() { depth_ = 0; }

 private:
  UIScreen *stack_[PHANTOM_UI_STACK_DEPTH] = {};
  size_t depth_ = 0;
};

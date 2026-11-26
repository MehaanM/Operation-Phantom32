#pragma once
#include <stdint.h>

// IModule — formal lifecycle interface for all Phantom32 subsystems.
//
// Design rules
//   - Modules are independently constructible without global state.
//   - initialize() must be idempotent: calling it twice is safe.
//   - shutdown() must release all resources; the destructor must be safe after it.
//   - update() is called from the main loop; it must never block indefinitely.
//   - Modules must not throw; they signal failure via return values and state_.
//
// Lifecycle
//   Uninitialized → initialize() → Running → update()... → shutdown() → Stopped
//                                             initialize() may also → Error

class IModule {
 public:
  enum class State : uint8_t {
    Uninitialized,
    Running,
    Stopped,
    Error,
  };

  virtual ~IModule() = default;

  // Core lifecycle — must be overridden.
  virtual bool initialize() = 0;
  virtual void update() = 0;
  virtual void shutdown() = 0;

  // Metadata — must be overridden; returns a string literal, never nullptr.
  virtual const char *name() const = 0;
  virtual const char *version() const = 0;
  virtual const char *description() const = 0;

  // Dependency declaration — optional.
  // ModuleLifecycle resolves initialization order so every declared dependency
  // is initialized before this module. Return the name of the i-th dependency.
  virtual size_t dependencyCount() const { return 0; }
  virtual const char *dependency(size_t /*i*/) const { return nullptr; }

  // Introspection.
  State state() const { return state_; }
  bool isRunning() const { return state_ == State::Running; }
  bool hasError() const { return state_ == State::Error; }

  // Called by ModuleLifecycle when a dependency cycle prevents initialization.
  void markError() { state_ = State::Error; }

 protected:
  // Subclasses transition state through these helpers.
  void setRunning() { state_ = State::Running; }
  void setStopped() { state_ = State::Stopped; }
  void setError() { state_ = State::Error; }

 private:
  State state_ = State::Uninitialized;
};

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
// State machine
//
//   Uninitialized ──beginInitializing()──► Initializing
//                                               │
//                               setRunning() ◄─┤─► setError()
//                                   │               │
//                                Running         Error ──markFailed()──► Failed
//                                   │               │                      │
//                       beginStopping()     beginInitializing()   beginInitializing()
//                                   │          (auto-restart)      (manual restart)
//                                Stopping           └──────────────────────┘
//                                   │
//                               setStopped()
//                                   │
//                                Stopped ──beginInitializing()──► Initializing
//
// Calling conventions
//   - setRunning(), setError(), setStopped() are called from within the module's
//     own initialize(), update(), and shutdown() implementations.
//   - beginInitializing(), beginStopping(), markFailed(), markError() are driven
//     by ModuleLifecycle to enforce framework-level transitions.
//   - Invalid transitions are silently ignored; state does not change.
//   - Failed is terminal for automatic restarts; only restartModule() may exit it.

class IModule {
 public:
  enum class State : uint8_t {
    Uninitialized,  // registered, not yet initialized
    Initializing,   // initialize() in progress
    Running,        // operational
    Stopping,       // shutdown() in progress
    Stopped,        // cleanly shut down
    Error,          // transient failure — autoRestart will retry
    Failed,         // permanent — circuit open, no automatic retries
  };

  // Bump when IModule has breaking interface changes.
  static constexpr uint8_t kApiVersion = 2;

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

  // Health check — override for deeper runtime validation beyond isRunning().
  // Does not change state.
  virtual bool healthCheck() { return isRunning(); }

  // API version query — override to declare the version the module was built against.
  virtual uint8_t apiVersion() const { return kApiVersion; }

  // Introspection.
  State state() const { return state_; }
  bool isRunning() const { return state_ == State::Running; }
  bool hasError() const { return state_ == State::Error; }
  bool isInitializing() const { return state_ == State::Initializing; }
  bool isStopping() const { return state_ == State::Stopping; }
  bool isStopped() const { return state_ == State::Stopped; }
  bool isFailed() const { return state_ == State::Failed; }

  // ── Framework interface ────────────────────────────────────────────────────
  // Called by ModuleLifecycle — do not call from module implementations.

  // Transition to Initializing. Valid from: Uninitialized, Stopped, Error, Failed.
  void beginInitializing() {
    if (state_ == State::Uninitialized || state_ == State::Stopped || state_ == State::Error ||
        state_ == State::Failed)
      state_ = State::Initializing;
  }

  // Transition to Stopping. Valid from: Running only.
  void beginStopping() {
    if (state_ == State::Running)
      state_ = State::Stopping;
  }

  // Transition to Failed (circuit open). Valid from: Error only.
  void markFailed() {
    if (state_ == State::Error)
      state_ = State::Failed;
  }

  // Force to Error — unrestricted; used by ModuleLifecycle for cycle detection
  // and cannot be blocked by state enforcement because cycle-involved modules
  // may never have called beginInitializing().
  void markError() {
    if (state_ != State::Failed)
      state_ = State::Error;
  }

 protected:
  // ── Subclass state transitions ─────────────────────────────────────────────
  // Called from within initialize(), update(), and shutdown() implementations.
  // Invalid transitions are silently ignored so a misbehaving module cannot
  // escape the Failed terminal state without an explicit restartModule() call.

  // Running. Valid from: Initializing or Uninitialized (backward-compat direct call).
  void setRunning() {
    if (state_ == State::Initializing || state_ == State::Uninitialized)
      state_ = State::Running;
  }

  // Stopped. Valid from: Stopping or Running (Running allowed for backward compat).
  void setStopped() {
    if (state_ == State::Stopping || state_ == State::Running)
      state_ = State::Stopped;
  }

  // Error. Cannot escape Failed without an explicit restart.
  void setError() {
    if (state_ != State::Failed)
      state_ = State::Error;
  }

 private:
  State state_ = State::Uninitialized;
};

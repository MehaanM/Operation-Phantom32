#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// SimulationEngine — event recording, replayable scenarios, and simulation snapshots (Phase 16).
// Records a timestamped event log; scenarios are named collections of events at relative
// timestamps that can be replayed from any start time for deterministic testing.

static constexpr size_t PHANTOM_SE_MAX_EVENTS = 128;
static constexpr size_t PHANTOM_SE_MAX_SCENARIOS = 8;
static constexpr size_t PHANTOM_SE_SCENARIO_EVENTS = 32;
static constexpr size_t PHANTOM_SE_MAX_SNAPSHOTS = 8;

enum class SimEventType : uint8_t {
  CLOCK_ADVANCE,
  HARDWARE_FAULT,
  HARDWARE_RECOVERY,
  CONFIG_CHANGE,
  POWER_LOSS,
  MEMORY_PRESSURE,
  MODULE_START,
  MODULE_STOP,
  NETWORK_SCAN,
  USER_INPUT
};

struct SimEvent {
  SimEventType type;
  uint64_t timestampMs;
  char target[32];
  char payload[48];
  bool processed;
};

struct SimScenario {
  char name[32];
  char description[64];
  SimEvent events[PHANTOM_SE_SCENARIO_EVENTS];
  size_t eventCount;
  uint64_t durationMs;
  bool valid;
};

struct SimSnapshot {
  char label[32];
  uint64_t timestampMs;
  bool valid;
};

class SimulationEngine {
 public:
  bool recordEvent(SimEventType type, const char *target, const char *payload, uint64_t nowMs) {
    if (evtCount_ >= PHANTOM_SE_MAX_EVENTS)
      return false;
    SimEvent &e = events_[evtCount_++];
    e.type = type;
    e.timestampMs = nowMs;
    if (target)
      strncpy(e.target, target, sizeof(e.target) - 1);
    if (payload)
      strncpy(e.payload, payload, sizeof(e.payload) - 1);
    e.processed = false;
    return true;
  }

  bool createScenario(const char *name, const char *description, uint64_t durationMs) {
    if (!name || !*name)
      return false;
    for (size_t i = 0; i < scenCount_; ++i) {
      if (strcmp(scenarios_[i].name, name) == 0) {
        scenarios_[i].durationMs = durationMs;
        if (description)
          strncpy(scenarios_[i].description, description, sizeof(scenarios_[i].description) - 1);
        return true;
      }
    }
    if (scenCount_ >= PHANTOM_SE_MAX_SCENARIOS)
      return false;
    SimScenario &s = scenarios_[scenCount_++];
    strncpy(s.name, name, sizeof(s.name) - 1);
    if (description)
      strncpy(s.description, description, sizeof(s.description) - 1);
    s.durationMs = durationMs;
    s.valid = true;
    return true;
  }

  bool addScenarioEvent(const char *scenarioName, SimEventType type, const char *target,
                        const char *payload, uint64_t relMs) {
    SimScenario *s = findScenario(scenarioName);
    if (!s || s->eventCount >= PHANTOM_SE_SCENARIO_EVENTS)
      return false;
    SimEvent &e = s->events[s->eventCount++];
    e.type = type;
    e.timestampMs = relMs;
    if (target)
      strncpy(e.target, target, sizeof(e.target) - 1);
    if (payload)
      strncpy(e.payload, payload, sizeof(e.payload) - 1);
    e.processed = false;
    return true;
  }

  bool validateScenario(const char *name) const {
    const SimScenario *s = findConstScenario(name);
    if (!s || !s->valid || s->eventCount == 0)
      return false;
    for (size_t i = 0; i < s->eventCount; ++i) {
      if (s->events[i].timestampMs > s->durationMs)
        return false;
    }
    return true;
  }

  size_t replayScenario(const char *name, uint64_t startMs) {
    SimScenario *s = findScenario(name);
    if (!s || !s->valid)
      return 0;
    size_t processed = 0;
    for (size_t i = 0; i < s->eventCount; ++i) {
      if (evtCount_ < PHANTOM_SE_MAX_EVENTS) {
        events_[evtCount_] = s->events[i];
        events_[evtCount_].timestampMs += startMs;
        events_[evtCount_].processed = true;
        ++evtCount_;
      }
      ++processed;
    }
    return processed;
  }

  size_t countEventsOfType(SimEventType type) const {
    size_t n = 0;
    for (size_t i = 0; i < evtCount_; ++i) {
      if (events_[i].type == type)
        ++n;
    }
    return n;
  }

  bool snapshot(const char *label, uint64_t nowMs) {
    if (!label || !*label || snapCount_ >= PHANTOM_SE_MAX_SNAPSHOTS)
      return false;
    for (size_t i = 0; i < snapCount_; ++i) {
      if (strcmp(snaps_[i].label, label) == 0) {
        snaps_[i].timestampMs = nowMs;
        return true;
      }
    }
    strncpy(snaps_[snapCount_].label, label, 31);
    snaps_[snapCount_].timestampMs = nowMs;
    snaps_[snapCount_].valid = true;
    ++snapCount_;
    return true;
  }

  void clearEvents() {
    evtCount_ = 0;
    for (size_t i = 0; i < PHANTOM_SE_MAX_EVENTS; ++i)
      events_[i] = {};
  }

  size_t eventCount() const { return evtCount_; }
  const SimEvent *eventAt(size_t i) const { return i < evtCount_ ? &events_[i] : nullptr; }
  size_t scenarioCount() const { return scenCount_; }
  const SimScenario *scenarioFor(const char *name) const { return findConstScenario(name); }
  size_t snapshotCount() const { return snapCount_; }

 private:
  SimEvent events_[PHANTOM_SE_MAX_EVENTS] = {};
  size_t evtCount_ = 0;
  SimScenario scenarios_[PHANTOM_SE_MAX_SCENARIOS] = {};
  size_t scenCount_ = 0;
  SimSnapshot snaps_[PHANTOM_SE_MAX_SNAPSHOTS] = {};
  size_t snapCount_ = 0;

  SimScenario *findScenario(const char *name) {
    if (!name)
      return nullptr;
    for (size_t i = 0; i < scenCount_; ++i) {
      if (strcmp(scenarios_[i].name, name) == 0)
        return &scenarios_[i];
    }
    return nullptr;
  }

  const SimScenario *findConstScenario(const char *name) const {
    if (!name)
      return nullptr;
    for (size_t i = 0; i < scenCount_; ++i) {
      if (strcmp(scenarios_[i].name, name) == 0)
        return &scenarios_[i];
    }
    return nullptr;
  }
};

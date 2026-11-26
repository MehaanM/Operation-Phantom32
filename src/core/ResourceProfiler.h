#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// ResourceProfiler — per-module RAM/CPU/storage profiling and regression detection (Phase 19).
// sampleSystem() populates real ESP32 heap metrics on firmware builds (guarded by
// PHANTOM32_NATIVE_TEST, which the native/native_asan/native_cov envs set via -D).

#ifndef PHANTOM32_NATIVE_TEST
#include <esp_heap_caps.h>
#include <esp_system.h>
#endif

static constexpr size_t PHANTOM_RP_MAX_MODULES = 16;
static constexpr size_t PHANTOM_RP_MAX_SNAPSHOTS = 8;

struct ModuleProfile {
  char name[32];
  uint32_t ramBytes;
  uint32_t cpuUs;
  uint32_t storageBytes;
  uint64_t sampleCount;
  bool valid;
};

struct ProfileSnapshot {
  char label[32];
  uint64_t timestampMs;
  uint32_t ramValues[PHANTOM_RP_MAX_MODULES];
  uint32_t cpuValues[PHANTOM_RP_MAX_MODULES];
  uint32_t storageValues[PHANTOM_RP_MAX_MODULES];
  size_t count;
  bool valid;
};

struct ResourceProfiler {
  bool registerModule(const char *name) {
    if (!name || !name[0])
      return false;
    for (size_t i = 0; i < modCount_; ++i) {
      if (strncmp(mods_[i].name, name, 31) == 0)
        return true;
    }
    if (modCount_ >= PHANTOM_RP_MAX_MODULES)
      return false;
    ModuleProfile &m = mods_[modCount_++];
    memset(&m, 0, sizeof(m));
    strncpy(m.name, name, 31);
    m.valid = true;
    return true;
  }

  bool recordRam(const char *name, uint32_t bytes) {
    ModuleProfile *m = findMod(name);
    if (!m)
      return false;
    m->ramBytes = bytes;
    ++m->sampleCount;
    return true;
  }

  bool recordCpu(const char *name, uint32_t microseconds) {
    ModuleProfile *m = findMod(name);
    if (!m)
      return false;
    m->cpuUs = microseconds;
    ++m->sampleCount;
    return true;
  }

  bool recordStorage(const char *name, uint32_t bytes) {
    ModuleProfile *m = findMod(name);
    if (!m)
      return false;
    m->storageBytes = bytes;
    ++m->sampleCount;
    return true;
  }

  uint32_t totalRam() const {
    uint32_t t = 0;
    for (size_t i = 0; i < modCount_; ++i)
      t += mods_[i].ramBytes;
    return t;
  }

  uint32_t totalCpu() const {
    uint32_t t = 0;
    for (size_t i = 0; i < modCount_; ++i)
      t += mods_[i].cpuUs;
    return t;
  }

  uint32_t totalStorage() const {
    uint32_t t = 0;
    for (size_t i = 0; i < modCount_; ++i)
      t += mods_[i].storageBytes;
    return t;
  }

  bool saveSnapshot(const char *label, uint64_t timestampMs) {
    if (!label || !label[0])
      return false;
    for (size_t i = 0; i < snapCount_; ++i) {
      if (strncmp(snaps_[i].label, label, 31) == 0) {
        fillSnap(snaps_[i], label, timestampMs);
        return true;
      }
    }
    if (snapCount_ >= PHANTOM_RP_MAX_SNAPSHOTS)
      return false;
    fillSnap(snaps_[snapCount_++], label, timestampMs);
    return true;
  }

  int32_t ramRegressionVs(const char *label, const char *moduleName) const {
    if (!label || !moduleName)
      return 0;
    const ProfileSnapshot *snap = snapshotFor(label);
    if (!snap)
      return 0;
    for (size_t i = 0; i < modCount_; ++i) {
      if (strncmp(mods_[i].name, moduleName, 31) == 0) {
        uint32_t baseline = (i < snap->count) ? snap->ramValues[i] : 0;
        return (int32_t)mods_[i].ramBytes - (int32_t)baseline;
      }
    }
    return 0;
  }

  const ModuleProfile *profileFor(const char *name) const {
    if (!name)
      return nullptr;
    for (size_t i = 0; i < modCount_; ++i)
      if (strncmp(mods_[i].name, name, 31) == 0)
        return &mods_[i];
    return nullptr;
  }

  size_t moduleCount() const { return modCount_; }

  // Samples the ESP32 system heap and updates the "System" module profile.
  // On native test builds (PHANTOM32_NATIVE_TEST defined) this is a no-op
  // so tests can call it safely without linking ESP32 SDK headers.
  void sampleSystem(uint64_t nowMs) {
    (void)nowMs;
#ifndef PHANTOM32_NATIVE_TEST
    uint32_t freeHeap = esp_get_free_heap_size();
    uint32_t totalHeap = (uint32_t)heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
    systemFreeRam_ = freeHeap;
    systemTotalRam_ = totalHeap;
    // Register "System" module if not yet present, then update its RAM.
    registerModule("System");
    for (size_t i = 0; i < modCount_; ++i) {
      if (strncmp(mods_[i].name, "System", 31) == 0) {
        mods_[i].ramBytes = freeHeap;
        ++mods_[i].sampleCount;
        break;
      }
    }
#endif
  }

  uint32_t systemFreeRam() const { return systemFreeRam_; }
  uint32_t systemTotalRam() const { return systemTotalRam_; }

  void generateReport(FILE *out) const {
    if (!out)
      return;
    fprintf(out, "Resource Profile\n");
    for (size_t i = 0; i < modCount_; ++i) {
      fprintf(out, "  %-20s RAM=%u CPU=%u STORAGE=%u\n", mods_[i].name, (unsigned)mods_[i].ramBytes,
              (unsigned)mods_[i].cpuUs, (unsigned)mods_[i].storageBytes);
    }
    fprintf(out, "  Total: RAM=%u CPU=%u STORAGE=%u\n", (unsigned)totalRam(), (unsigned)totalCpu(),
            (unsigned)totalStorage());
  }

 private:
  ModuleProfile mods_[PHANTOM_RP_MAX_MODULES] = {};
  ProfileSnapshot snaps_[PHANTOM_RP_MAX_SNAPSHOTS] = {};
  size_t modCount_ = 0;
  size_t snapCount_ = 0;
  uint32_t systemFreeRam_ = 0;
  uint32_t systemTotalRam_ = 0;

  ModuleProfile *findMod(const char *name) {
    if (!name)
      return nullptr;
    for (size_t i = 0; i < modCount_; ++i)
      if (strncmp(mods_[i].name, name, 31) == 0)
        return &mods_[i];
    return nullptr;
  }

  const ProfileSnapshot *snapshotFor(const char *label) const {
    if (!label)
      return nullptr;
    for (size_t i = 0; i < snapCount_; ++i)
      if (strncmp(snaps_[i].label, label, 31) == 0)
        return &snaps_[i];
    return nullptr;
  }

  void fillSnap(ProfileSnapshot &s, const char *label, uint64_t timestampMs) const {
    memset(&s, 0, sizeof(s));
    strncpy(s.label, label, 31);
    s.timestampMs = timestampMs;
    s.count = modCount_;
    for (size_t i = 0; i < modCount_; ++i) {
      s.ramValues[i] = mods_[i].ramBytes;
      s.cpuValues[i] = mods_[i].cpuUs;
      s.storageValues[i] = mods_[i].storageBytes;
    }
    s.valid = true;
  }
};

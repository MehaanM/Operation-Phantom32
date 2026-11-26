#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// ResourceBudget — per-module resource limits.
// A zero limit means unlimited.
struct ResourceBudget {
  size_t maxHeapBytes = 0;    // 0 = unlimited
  uint32_t maxCpuTimeMs = 0;  // 0 = unlimited (not enforced on bare-metal)
  uint16_t maxTaskCount = 0;  // 0 = unlimited
};

struct ModuleResources {
  char name[32] = {};
  size_t heapAllocated = 0;
  size_t peakHeapAllocated = 0;
  uint32_t allocCount = 0;
  uint32_t freeCount = 0;
  ResourceBudget budget = {};
};

static constexpr size_t PHANTOM_RM_MAX_MODULES = 16;

// ResourceManager — tracks heap allocations per module and enforces budgets.
//
// Allocation-failure injection:
//   rm.setFailAfter(2) causes the next 2 recordAlloc() calls to return false,
//   simulating an allocator that runs out of memory. Use this in tests to
//   verify error-handling paths without touching the real heap.
//
// Usage:
//   ResourceManager rm;
//   rm.registerModule("WiFiScanner", {.maxHeapBytes = 32768});
//   rm.recordAlloc("WiFiScanner", actualAlloc);
//   if (rm.isOverBudget("WiFiScanner")) { ... }
//   if (rm.hasMismatch("WiFiScanner")) { /* alloc/free count differs */ }
class ResourceManager {
 public:
  bool registerModule(const char *name, ResourceBudget budget = {}) {
    if (!name || count_ >= PHANTOM_RM_MAX_MODULES)
      return false;
    // Update budget if already registered.
    ModuleResources *e = find(name);
    if (e) {
      e->budget = budget;
      return true;
    }
    ModuleResources &slot = entries_[count_++];
    strncpy(slot.name, name, sizeof(slot.name) - 1);
    slot.name[sizeof(slot.name) - 1] = '\0';
    slot.heapAllocated = 0;
    slot.peakHeapAllocated = 0;
    slot.allocCount = 0;
    slot.freeCount = 0;
    slot.budget = budget;
    return true;
  }

  bool unregisterModule(const char *name) {
    for (size_t i = 0; i < count_; ++i) {
      if (strncmp(entries_[i].name, name, sizeof(entries_[i].name)) == 0) {
        for (size_t j = i; j + 1 < count_; ++j)
          entries_[j] = entries_[j + 1];
        --count_;
        return true;
      }
    }
    return false;
  }

  // Allocation-failure injection: the next `n` recordAlloc() calls will
  // return false without modifying state, as if the allocator failed.
  // Pass 0 to disable injection.
  void setFailAfter(uint32_t n) { failRemaining_ = n; }
  uint32_t failRemaining() const { return failRemaining_; }

  // Returns false if name not registered, if bytes would overflow size_t,
  // or if allocation-failure injection is active (failRemaining_ > 0).
  bool recordAlloc(const char *name, size_t bytes) {
    if (failRemaining_ > 0) {
      --failRemaining_;
      return false;
    }
    ModuleResources *e = find(name);
    if (!e)
      return false;
    // Overflow check: addition wraps if result < either operand.
    if (bytes > 0 && e->heapAllocated + bytes < e->heapAllocated)
      return false;
    e->heapAllocated += bytes;
    ++e->allocCount;
    if (e->heapAllocated > e->peakHeapAllocated)
      e->peakHeapAllocated = e->heapAllocated;
    return true;
  }

  bool recordFree(const char *name, size_t bytes) {
    ModuleResources *e = find(name);
    if (!e)
      return false;
    e->heapAllocated = (bytes > e->heapAllocated) ? 0 : (e->heapAllocated - bytes);
    ++e->freeCount;
    return true;
  }

  bool isOverBudget(const char *name) const {
    const ModuleResources *e = findConst(name);
    if (!e || e->budget.maxHeapBytes == 0)
      return false;
    return e->heapAllocated > e->budget.maxHeapBytes;
  }

  size_t heapUsed(const char *name) const {
    const ModuleResources *e = findConst(name);
    return e ? e->heapAllocated : 0;
  }

  size_t peakHeapUsed(const char *name) const {
    const ModuleResources *e = findConst(name);
    return e ? e->peakHeapAllocated : 0;
  }

  uint32_t allocCount(const char *name) const {
    const ModuleResources *e = findConst(name);
    return e ? e->allocCount : 0;
  }

  uint32_t freeCount(const char *name) const {
    const ModuleResources *e = findConst(name);
    return e ? e->freeCount : 0;
  }

  // Returns true if allocCount != freeCount (possible leak or double-free).
  bool hasMismatch(const char *name) const {
    const ModuleResources *e = findConst(name);
    if (!e)
      return false;
    return e->allocCount != e->freeCount;
  }

  // Reset all counters for a module without unregistering it.
  bool reset(const char *name) {
    ModuleResources *e = find(name);
    if (!e)
      return false;
    e->heapAllocated = 0;
    e->peakHeapAllocated = 0;
    e->allocCount = 0;
    e->freeCount = 0;
    return true;
  }

  size_t totalHeapTracked() const {
    size_t total = 0;
    for (size_t i = 0; i < count_; ++i)
      total += entries_[i].heapAllocated;
    return total;
  }

  size_t moduleCount() const { return count_; }

 private:
  ModuleResources *find(const char *name) {
    for (size_t i = 0; i < count_; ++i)
      if (strncmp(entries_[i].name, name, sizeof(entries_[i].name)) == 0)
        return &entries_[i];
    return nullptr;
  }

  const ModuleResources *findConst(const char *name) const {
    for (size_t i = 0; i < count_; ++i)
      if (strncmp(entries_[i].name, name, sizeof(entries_[i].name)) == 0)
        return &entries_[i];
    return nullptr;
  }

  ModuleResources entries_[PHANTOM_RM_MAX_MODULES] = {};
  size_t count_ = 0;
  uint32_t failRemaining_ = 0;
};

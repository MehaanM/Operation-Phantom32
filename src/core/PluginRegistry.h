#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// PluginRegistry — plugin interface, permission model, and lifecycle (Phase 15).
// Plugins declare a semver-matched API version, per-tick CPU budget, heap budget,
// and a capability permission set. Incompatible API versions or oversized budgets
// are rejected at registration time. Permissions can be granted/revoked at runtime.

static constexpr size_t PHANTOM_PR_MAX_PLUGINS = 8;
static constexpr size_t PHANTOM_PR_MAX_PERMS = 8;
static constexpr uint32_t PHANTOM_PR_API_VERSION = 1;
static constexpr uint32_t PHANTOM_PR_MAX_MEMORY_BYTES = 65536;  // 64 KB

enum class PluginPermission : uint8_t {
  NETWORK_READ,
  NETWORK_WRITE,
  STORAGE_READ,
  STORAGE_WRITE,
  CONFIG_READ,
  CONFIG_WRITE,
  DIAGNOSTIC_READ,
  HARDWARE_ACCESS
};

enum class PluginState : uint8_t { UNREGISTERED, REGISTERED, ACTIVE, SUSPENDED, REJECTED };

struct PluginDescriptor {
  char name[32];
  char version[16];
  char author[32];
  uint32_t apiVersion;
  uint32_t memoryBudgetBytes;
  uint32_t cpuBudgetUs;
  PluginPermission permissions[PHANTOM_PR_MAX_PERMS];
  uint8_t permissionCount;
  char minPlatformVersion[16];
  bool sandboxed;
  PluginState state;
  bool valid;
};

class PluginRegistry {
 public:
  bool registerPlugin(const PluginDescriptor &desc) {
    if (!desc.valid || !desc.name[0])
      return false;
    if (!isCompatible(desc))
      return false;
    for (size_t i = 0; i < count_; ++i) {
      if (strcmp(plugins_[i].name, desc.name) == 0) {
        plugins_[i] = desc;
        plugins_[i].state = PluginState::REGISTERED;
        return true;
      }
    }
    if (count_ >= PHANTOM_PR_MAX_PLUGINS)
      return false;
    plugins_[count_] = desc;
    plugins_[count_].state = PluginState::REGISTERED;
    ++count_;
    return true;
  }

  bool validate(const PluginDescriptor &desc) const {
    if (!desc.valid || !desc.name[0])
      return false;
    if (desc.apiVersion > PHANTOM_PR_API_VERSION)
      return false;
    if (desc.permissionCount > PHANTOM_PR_MAX_PERMS)
      return false;
    return true;
  }

  bool activate(const char *name) {
    PluginDescriptor *p = find(name);
    if (!p || p->state == PluginState::REJECTED)
      return false;
    p->state = PluginState::ACTIVE;
    return true;
  }

  bool suspend(const char *name) {
    PluginDescriptor *p = find(name);
    if (!p || p->state != PluginState::ACTIVE)
      return false;
    p->state = PluginState::SUSPENDED;
    return true;
  }

  bool hasPermission(const char *name, PluginPermission perm) const {
    const PluginDescriptor *p = findConst(name);
    if (!p)
      return false;
    for (uint8_t i = 0; i < p->permissionCount; ++i) {
      if (p->permissions[i] == perm)
        return true;
    }
    return false;
  }

  bool grantPermission(const char *name, PluginPermission perm) {
    PluginDescriptor *p = find(name);
    if (!p || p->permissionCount >= PHANTOM_PR_MAX_PERMS)
      return false;
    for (uint8_t i = 0; i < p->permissionCount; ++i) {
      if (p->permissions[i] == perm)
        return true;  // already granted
    }
    p->permissions[p->permissionCount++] = perm;
    return true;
  }

  bool revokePermission(const char *name, PluginPermission perm) {
    PluginDescriptor *p = find(name);
    if (!p)
      return false;
    for (uint8_t i = 0; i < p->permissionCount; ++i) {
      if (p->permissions[i] == perm) {
        for (uint8_t j = i; j < p->permissionCount - 1u; ++j) {
          p->permissions[j] = p->permissions[j + 1u];
        }
        --p->permissionCount;
        return true;
      }
    }
    return false;
  }

  bool isCompatible(const PluginDescriptor &desc) const {
    if (!desc.valid)
      return false;
    if (desc.apiVersion > PHANTOM_PR_API_VERSION)
      return false;
    if (desc.memoryBudgetBytes > PHANTOM_PR_MAX_MEMORY_BYTES)
      return false;
    return true;
  }

  size_t pluginCount() const { return count_; }
  size_t activeCount() const {
    size_t n = 0;
    for (size_t i = 0; i < count_; ++i) {
      if (plugins_[i].state == PluginState::ACTIVE)
        ++n;
    }
    return n;
  }
  const PluginDescriptor *descriptorFor(const char *name) const { return findConst(name); }

 private:
  PluginDescriptor plugins_[PHANTOM_PR_MAX_PLUGINS] = {};
  size_t count_ = 0;

  PluginDescriptor *find(const char *name) {
    if (!name)
      return nullptr;
    for (size_t i = 0; i < count_; ++i) {
      if (strcmp(plugins_[i].name, name) == 0)
        return &plugins_[i];
    }
    return nullptr;
  }

  const PluginDescriptor *findConst(const char *name) const {
    if (!name)
      return nullptr;
    for (size_t i = 0; i < count_; ++i) {
      if (strcmp(plugins_[i].name, name) == 0)
        return &plugins_[i];
    }
    return nullptr;
  }
};

#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// PeripheralRegistry — peripheral ownership tracking and conflict detection.
//
// Each peripheral has a numeric ID (PeripheralId), an optional GPIO pin, and
// an optional owner tag (the module name that claimed it). Claiming a peripheral
// that is already owned by a different module fails and reports a conflict.
//
// Pin-conflict detection:
//   registerPeripheral() rejects a registration if the requested GPIO pin is
//   already assigned to a different peripheral. Pass kNoPin (0xFF) when the
//   peripheral is not GPIO-mapped.
//
// Usage:
//   PeripheralRegistry reg;
//   reg.registerPeripheral(1, "SPI_BUS",   5);  // peripheral 1 on GPIO5
//   reg.registerPeripheral(2, "I2C_SCL",   5);  // fails: pin 5 already used
//   reg.claim(1, "DisplayModule");               // DisplayModule owns SPI_BUS
//   reg.claim(1, "WiFiScanner");                 // fails: already owned
//   reg.release(1, "DisplayModule");             // releases ownership
//
// All operations are O(N) where N = registered peripherals (≤ 32).

static constexpr size_t PERIPH_MAX = 32;
static constexpr uint8_t kNoPin = 0xFF;

struct PeripheralEntry {
  uint8_t id = 0;
  char name[24] = {};
  uint8_t pin = kNoPin;  // GPIO pin or kNoPin
  char owner[24] = {};   // owning module; empty = unowned
  bool registered = false;
};

class PeripheralRegistry {
 public:
  // Register a peripheral. Fails if:
  //   - id is already registered
  //   - pin != kNoPin and that pin is already assigned to another peripheral
  //   - name is null or the registry is full
  bool registerPeripheral(uint8_t id, const char *name, uint8_t pin = kNoPin) {
    if (!name || count_ >= PERIPH_MAX)
      return false;
    // Duplicate id check.
    if (findById(id))
      return false;
    // Pin-conflict check.
    if (pin != kNoPin) {
      for (size_t i = 0; i < count_; ++i) {
        if (entries_[i].pin == pin)
          return false;  // pin conflict
      }
    }
    PeripheralEntry &e = entries_[count_++];
    e.id = id;
    strncpy(e.name, name, sizeof(e.name) - 1);
    e.pin = pin;
    e.owner[0] = '\0';
    e.registered = true;
    return true;
  }

  // Claim ownership of a peripheral for the given module.
  // Fails if the peripheral is already owned by a different module,
  // the peripheral is not registered, or owner is null.
  bool claim(uint8_t id, const char *owner) {
    if (!owner)
      return false;
    PeripheralEntry *e = findById(id);
    if (!e)
      return false;
    if (e->owner[0] != '\0') {
      // Already owned: allow re-claim by same owner, reject others.
      return strncmp(e->owner, owner, sizeof(e->owner)) == 0;
    }
    strncpy(e->owner, owner, sizeof(e->owner) - 1);
    e->owner[sizeof(e->owner) - 1] = '\0';
    return true;
  }

  // Release a peripheral. The caller must be the current owner.
  // Returns false if not the owner, not registered, or owner is null.
  bool release(uint8_t id, const char *owner) {
    if (!owner)
      return false;
    PeripheralEntry *e = findById(id);
    if (!e)
      return false;
    if (strncmp(e->owner, owner, sizeof(e->owner)) != 0)
      return false;
    e->owner[0] = '\0';
    return true;
  }

  // Returns true if the peripheral is claimed by any module.
  bool isClaimed(uint8_t id) const {
    const PeripheralEntry *e = findByIdConst(id);
    return e && e->owner[0] != '\0';
  }

  // Returns the owner tag, or nullptr if unclaimed or not registered.
  const char *ownerOf(uint8_t id) const {
    const PeripheralEntry *e = findByIdConst(id);
    if (!e || e->owner[0] == '\0')
      return nullptr;
    return e->owner;
  }

  // Returns the entry for a given id, or nullptr if not registered.
  const PeripheralEntry *entryFor(uint8_t id) const { return findByIdConst(id); }

  // Detect all pin conflicts in the registry (should be zero after valid setup).
  // Returns the number of duplicate-pin registrations found (always 0 if
  // registerPeripheral() was used consistently — present for defensive audit).
  size_t pinConflictCount() const {
    size_t conflicts = 0;
    for (size_t i = 0; i < count_; ++i) {
      if (entries_[i].pin == kNoPin)
        continue;
      for (size_t j = i + 1; j < count_; ++j) {
        if (entries_[j].pin == entries_[i].pin)
          ++conflicts;
      }
    }
    return conflicts;
  }

  size_t count() const { return count_; }

 private:
  PeripheralEntry *findById(uint8_t id) {
    for (size_t i = 0; i < count_; ++i)
      if (entries_[i].id == id)
        return &entries_[i];
    return nullptr;
  }
  const PeripheralEntry *findByIdConst(uint8_t id) const {
    for (size_t i = 0; i < count_; ++i)
      if (entries_[i].id == id)
        return &entries_[i];
    return nullptr;
  }

  PeripheralEntry entries_[PERIPH_MAX] = {};
  size_t count_ = 0;
};

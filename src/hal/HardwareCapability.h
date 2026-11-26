#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// HardwareCapability — per-board capability declarations and profile validation.
//
// A HardwareProfile names a board and declares which capabilities it provides
// via a bitmask. Code queries hasCapability() before using optional hardware
// rather than hard-coding board IDs.
//
// Capability flags (extend as needed; keep under 32 bits):
//   DISPLAY      — attached display (TFT, OLED, …)
//   STORAGE      — persistent storage (NVS, LittleFS, SD)
//   WIFI         — 802.11 radio
//   BLE          — Bluetooth Low Energy
//   PSRAM        — external PSRAM
//   USB_OTG      — USB OTG controller
//   BATTERY_MON  — battery voltage/status monitoring
//   BUTTONS      — physical buttons beyond reset
//   LED          — onboard LED or NeoPixel
//   UART_BRIDGE  — hardware UART-to-USB bridge
//
// Usage:
//   HardwareProfile p;
//   p.set("doom_s3", HWCap::WIFI | HWCap::BLE | HWCap::PSRAM | HWCap::DISPLAY);
//   if (p.hasCapability(HWCap::PSRAM)) { /* enable PSRAM features */ }

struct HWCap {
  static constexpr uint32_t DISPLAY = 1u << 0;
  static constexpr uint32_t STORAGE = 1u << 1;
  static constexpr uint32_t WIFI = 1u << 2;
  static constexpr uint32_t BLE = 1u << 3;
  static constexpr uint32_t PSRAM = 1u << 4;
  static constexpr uint32_t USB_OTG = 1u << 5;
  static constexpr uint32_t BATTERY_MON = 1u << 6;
  static constexpr uint32_t BUTTONS = 1u << 7;
  static constexpr uint32_t LED = 1u << 8;
  static constexpr uint32_t UART_BRIDGE = 1u << 9;
};

// Validation status of a capability check.
enum class CapValidation : uint8_t {
  UNKNOWN,        // capability was never probed
  COMPILES,       // firmware compiles with this capability
  SIMULATED,      // simulated/native test passed
  HARDWARE,       // hardware test passed
  LONG_DURATION,  // passed a long-duration test
  VALIDATED,      // fully validated (highest)
};

struct CapabilityEntry {
  uint32_t flag = 0;
  CapValidation validation = CapValidation::UNKNOWN;
};

static constexpr size_t HW_MAX_CAPABILITIES = 16;
static constexpr size_t HW_BOARD_ID_LEN = 32;

struct HardwareProfile {
  char boardId[HW_BOARD_ID_LEN] = {};
  uint32_t capabilityMask = 0;
  CapabilityEntry capEntries[HW_MAX_CAPABILITIES] = {};
  size_t capEntryCount = 0;

  // Set board ID and capability mask in one call.
  void set(const char *id, uint32_t caps) {
    if (id)
      strncpy(boardId, id, sizeof(boardId) - 1);
    capabilityMask = caps;
  }

  bool hasCapability(uint32_t flag) const { return (capabilityMask & flag) != 0; }

  // Record a validation level for a specific capability flag.
  bool setValidation(uint32_t flag, CapValidation v) {
    // Update existing entry.
    for (size_t i = 0; i < capEntryCount; ++i) {
      if (capEntries[i].flag == flag) {
        capEntries[i].validation = v;
        return true;
      }
    }
    if (capEntryCount >= HW_MAX_CAPABILITIES)
      return false;
    capEntries[capEntryCount].flag = flag;
    capEntries[capEntryCount].validation = v;
    ++capEntryCount;
    return true;
  }

  CapValidation getValidation(uint32_t flag) const {
    for (size_t i = 0; i < capEntryCount; ++i)
      if (capEntries[i].flag == flag)
        return capEntries[i].validation;
    return CapValidation::UNKNOWN;
  }

  // Returns true if boardId is non-empty. A profile with an empty board ID
  // is considered unconfigured and should not be used for dispatch.
  bool isValid() const { return boardId[0] != '\0'; }

  // Validate that all declared capabilities have at least COMPILES status.
  // Returns the number of capabilities with UNKNOWN validation (0 = all good).
  size_t unvalidatedCount() const {
    size_t count = 0;
    uint32_t remaining = capabilityMask;
    while (remaining) {
      uint32_t bit = remaining & (-remaining);  // lowest set bit
      if (getValidation(bit) == CapValidation::UNKNOWN)
        ++count;
      remaining &= remaining - 1;
    }
    return count;
  }
};

// ── Hardware profile registry ──────────────────────────────────────────────
// Stores up to N board profiles and resolves the active one by board ID.

static constexpr size_t HW_MAX_PROFILES = 16;

class HardwareProfileRegistry {
 public:
  // Register or update a profile by board ID.
  bool addProfile(const HardwareProfile &p) {
    if (!p.isValid())
      return false;
    // Update existing.
    for (size_t i = 0; i < count_; ++i) {
      if (strncmp(profiles_[i].boardId, p.boardId, sizeof(p.boardId)) == 0) {
        profiles_[i] = p;
        return true;
      }
    }
    if (count_ >= HW_MAX_PROFILES)
      return false;
    profiles_[count_++] = p;
    return true;
  }

  const HardwareProfile *findProfile(const char *boardId) const {
    if (!boardId)
      return nullptr;
    for (size_t i = 0; i < count_; ++i)
      if (strncmp(profiles_[i].boardId, boardId, sizeof(profiles_[i].boardId)) == 0)
        return &profiles_[i];
    return nullptr;
  }

  size_t count() const { return count_; }

 private:
  HardwareProfile profiles_[HW_MAX_PROFILES] = {};
  size_t count_ = 0;
};

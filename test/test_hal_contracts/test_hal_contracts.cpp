// test_hal_contracts.cpp — Tests for HardwareCapability and PeripheralRegistry (Phase 5).

#include "unity.h"
#include "hal/HardwareCapability.h"
#include "hal/PeripheralRegistry.h"

void setUp() {}
void tearDown() {}

// ── HardwareProfile ────────────────────────────────────────────────────────

static void test_profile_valid_after_set() {
  HardwareProfile p;
  p.set("doom_s3", HWCap::WIFI | HWCap::PSRAM);
  TEST_ASSERT_TRUE(p.isValid());
  TEST_ASSERT_EQUAL_STRING("doom_s3", p.boardId);
}

static void test_profile_invalid_before_set() {
  HardwareProfile p;
  TEST_ASSERT_FALSE(p.isValid());
}

static void test_has_capability_set_flag() {
  HardwareProfile p;
  p.set("doom", HWCap::WIFI | HWCap::BLE);
  TEST_ASSERT_TRUE(p.hasCapability(HWCap::WIFI));
  TEST_ASSERT_TRUE(p.hasCapability(HWCap::BLE));
}

static void test_has_capability_missing_flag() {
  HardwareProfile p;
  p.set("doom", HWCap::WIFI);
  TEST_ASSERT_FALSE(p.hasCapability(HWCap::PSRAM));
  TEST_ASSERT_FALSE(p.hasCapability(HWCap::DISPLAY));
}

static void test_set_validation_and_get() {
  HardwareProfile p;
  p.set("doom_s3", HWCap::WIFI);
  TEST_ASSERT_TRUE(p.setValidation(HWCap::WIFI, CapValidation::SIMULATED));
  TEST_ASSERT_EQUAL(CapValidation::SIMULATED, p.getValidation(HWCap::WIFI));
}

static void test_get_validation_unknown_returns_unknown() {
  HardwareProfile p;
  p.set("doom", HWCap::WIFI);
  TEST_ASSERT_EQUAL(CapValidation::UNKNOWN, p.getValidation(HWCap::WIFI));
}

static void test_set_validation_update_existing() {
  HardwareProfile p;
  p.set("doom", HWCap::WIFI);
  p.setValidation(HWCap::WIFI, CapValidation::COMPILES);
  p.setValidation(HWCap::WIFI, CapValidation::HARDWARE);
  TEST_ASSERT_EQUAL(CapValidation::HARDWARE, p.getValidation(HWCap::WIFI));
  // Entry count should stay at 1.
  TEST_ASSERT_EQUAL(1, (int)p.capEntryCount);
}

static void test_unvalidated_count_all_unknown() {
  HardwareProfile p;
  p.set("doom", HWCap::WIFI | HWCap::BLE | HWCap::PSRAM);
  // No validations set → 3 unvalidated bits.
  TEST_ASSERT_EQUAL(3, (int)p.unvalidatedCount());
}

static void test_unvalidated_count_decrements_when_validated() {
  HardwareProfile p;
  p.set("doom", HWCap::WIFI | HWCap::BLE);
  p.setValidation(HWCap::WIFI, CapValidation::COMPILES);
  TEST_ASSERT_EQUAL(1, (int)p.unvalidatedCount());
}

static void test_unvalidated_count_zero_when_all_validated() {
  HardwareProfile p;
  p.set("doom", HWCap::WIFI | HWCap::STORAGE);
  p.setValidation(HWCap::WIFI,    CapValidation::VALIDATED);
  p.setValidation(HWCap::STORAGE, CapValidation::VALIDATED);
  TEST_ASSERT_EQUAL(0, (int)p.unvalidatedCount());
}

// ── HardwareProfileRegistry ────────────────────────────────────────────────

static void test_registry_add_valid_profile() {
  HardwareProfileRegistry reg;
  HardwareProfile p;
  p.set("doom_s3", HWCap::WIFI);
  TEST_ASSERT_TRUE(reg.addProfile(p));
  TEST_ASSERT_EQUAL(1, (int)reg.count());
}

static void test_registry_add_invalid_profile_rejected() {
  HardwareProfileRegistry reg;
  HardwareProfile p;  // isValid() == false
  TEST_ASSERT_FALSE(reg.addProfile(p));
}

static void test_registry_find_existing_profile() {
  HardwareProfileRegistry reg;
  HardwareProfile p;
  p.set("doom_s3", HWCap::WIFI | HWCap::PSRAM);
  reg.addProfile(p);
  const HardwareProfile *found = reg.findProfile("doom_s3");
  TEST_ASSERT_NOT_NULL(found);
  TEST_ASSERT_TRUE(found->hasCapability(HWCap::WIFI));
}

static void test_registry_find_missing_returns_null() {
  HardwareProfileRegistry reg;
  TEST_ASSERT_NULL(reg.findProfile("nonexistent"));
}

static void test_registry_find_null_returns_null() {
  HardwareProfileRegistry reg;
  TEST_ASSERT_NULL(reg.findProfile(nullptr));
}

static void test_registry_update_existing_profile() {
  HardwareProfileRegistry reg;
  HardwareProfile p;
  p.set("doom", HWCap::WIFI);
  reg.addProfile(p);
  // Update: add PSRAM.
  p.set("doom", HWCap::WIFI | HWCap::PSRAM);
  reg.addProfile(p);
  TEST_ASSERT_EQUAL(1, (int)reg.count());
  const HardwareProfile *found = reg.findProfile("doom");
  TEST_ASSERT_NOT_NULL(found);
  TEST_ASSERT_TRUE(found->hasCapability(HWCap::PSRAM));
}

// ── PeripheralRegistry ─────────────────────────────────────────────────────

static void test_register_peripheral_success() {
  PeripheralRegistry reg;
  TEST_ASSERT_TRUE(reg.registerPeripheral(1, "SPI_BUS", 5));
  TEST_ASSERT_EQUAL(1, (int)reg.count());
}

static void test_register_null_name_rejected() {
  PeripheralRegistry reg;
  TEST_ASSERT_FALSE(reg.registerPeripheral(1, nullptr, 5));
}

static void test_register_duplicate_id_rejected() {
  PeripheralRegistry reg;
  reg.registerPeripheral(1, "A", 5);
  TEST_ASSERT_FALSE(reg.registerPeripheral(1, "B", 6));
  TEST_ASSERT_EQUAL(1, (int)reg.count());
}

static void test_register_duplicate_pin_rejected() {
  PeripheralRegistry reg;
  reg.registerPeripheral(1, "A", 5);
  TEST_ASSERT_FALSE(reg.registerPeripheral(2, "B", 5));
  TEST_ASSERT_EQUAL(1, (int)reg.count());
}

static void test_register_no_pin_always_ok() {
  PeripheralRegistry reg;
  reg.registerPeripheral(1, "A", kNoPin);
  TEST_ASSERT_TRUE(reg.registerPeripheral(2, "B", kNoPin));
}

// ── Claim / release ────────────────────────────────────────────────────────

static void test_claim_unclaimed_peripheral_succeeds() {
  PeripheralRegistry reg;
  reg.registerPeripheral(1, "SPI", 5);
  TEST_ASSERT_TRUE(reg.claim(1, "Display"));
  TEST_ASSERT_TRUE(reg.isClaimed(1));
}

static void test_claim_by_same_owner_succeeds() {
  PeripheralRegistry reg;
  reg.registerPeripheral(1, "SPI", 5);
  reg.claim(1, "Display");
  TEST_ASSERT_TRUE(reg.claim(1, "Display"));  // idempotent
}

static void test_claim_by_different_owner_fails() {
  PeripheralRegistry reg;
  reg.registerPeripheral(1, "SPI", 5);
  reg.claim(1, "Display");
  TEST_ASSERT_FALSE(reg.claim(1, "WiFi"));
}

static void test_claim_null_owner_fails() {
  PeripheralRegistry reg;
  reg.registerPeripheral(1, "SPI", 5);
  TEST_ASSERT_FALSE(reg.claim(1, nullptr));
}

static void test_claim_unregistered_fails() {
  PeripheralRegistry reg;
  TEST_ASSERT_FALSE(reg.claim(99, "M"));
}

static void test_release_by_owner_succeeds() {
  PeripheralRegistry reg;
  reg.registerPeripheral(1, "SPI", 5);
  reg.claim(1, "Display");
  TEST_ASSERT_TRUE(reg.release(1, "Display"));
  TEST_ASSERT_FALSE(reg.isClaimed(1));
}

static void test_release_by_non_owner_fails() {
  PeripheralRegistry reg;
  reg.registerPeripheral(1, "SPI", 5);
  reg.claim(1, "Display");
  TEST_ASSERT_FALSE(reg.release(1, "WiFi"));
  TEST_ASSERT_TRUE(reg.isClaimed(1));
}

static void test_release_null_owner_fails() {
  PeripheralRegistry reg;
  reg.registerPeripheral(1, "SPI", 5);
  reg.claim(1, "Display");
  TEST_ASSERT_FALSE(reg.release(1, nullptr));
}

static void test_owner_of_unclaimed_returns_null() {
  PeripheralRegistry reg;
  reg.registerPeripheral(1, "SPI", 5);
  TEST_ASSERT_NULL(reg.ownerOf(1));
}

static void test_owner_of_claimed_returns_name() {
  PeripheralRegistry reg;
  reg.registerPeripheral(1, "SPI", 5);
  reg.claim(1, "Display");
  TEST_ASSERT_EQUAL_STRING("Display", reg.ownerOf(1));
}

static void test_owner_of_unregistered_returns_null() {
  PeripheralRegistry reg;
  TEST_ASSERT_NULL(reg.ownerOf(99));
}

// ── Pin conflict detection ─────────────────────────────────────────────────

static void test_no_pin_conflicts_initially() {
  PeripheralRegistry reg;
  reg.registerPeripheral(1, "A", 5);
  reg.registerPeripheral(2, "B", 6);
  TEST_ASSERT_EQUAL(0, (int)reg.pinConflictCount());
}

static void test_entry_for_registered_not_null() {
  PeripheralRegistry reg;
  reg.registerPeripheral(1, "SPI", 5);
  const PeripheralEntry *e = reg.entryFor(1);
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL_UINT8(5, e->pin);
  TEST_ASSERT_EQUAL_STRING("SPI", e->name);
}

static void test_entry_for_unregistered_returns_null() {
  PeripheralRegistry reg;
  TEST_ASSERT_NULL(reg.entryFor(99));
}

// ── Main ───────────────────────────────────────────────────────────────────

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_profile_valid_after_set);
  RUN_TEST(test_profile_invalid_before_set);
  RUN_TEST(test_has_capability_set_flag);
  RUN_TEST(test_has_capability_missing_flag);
  RUN_TEST(test_set_validation_and_get);
  RUN_TEST(test_get_validation_unknown_returns_unknown);
  RUN_TEST(test_set_validation_update_existing);
  RUN_TEST(test_unvalidated_count_all_unknown);
  RUN_TEST(test_unvalidated_count_decrements_when_validated);
  RUN_TEST(test_unvalidated_count_zero_when_all_validated);

  RUN_TEST(test_registry_add_valid_profile);
  RUN_TEST(test_registry_add_invalid_profile_rejected);
  RUN_TEST(test_registry_find_existing_profile);
  RUN_TEST(test_registry_find_missing_returns_null);
  RUN_TEST(test_registry_find_null_returns_null);
  RUN_TEST(test_registry_update_existing_profile);

  RUN_TEST(test_register_peripheral_success);
  RUN_TEST(test_register_null_name_rejected);
  RUN_TEST(test_register_duplicate_id_rejected);
  RUN_TEST(test_register_duplicate_pin_rejected);
  RUN_TEST(test_register_no_pin_always_ok);

  RUN_TEST(test_claim_unclaimed_peripheral_succeeds);
  RUN_TEST(test_claim_by_same_owner_succeeds);
  RUN_TEST(test_claim_by_different_owner_fails);
  RUN_TEST(test_claim_null_owner_fails);
  RUN_TEST(test_claim_unregistered_fails);

  RUN_TEST(test_release_by_owner_succeeds);
  RUN_TEST(test_release_by_non_owner_fails);
  RUN_TEST(test_release_null_owner_fails);
  RUN_TEST(test_owner_of_unclaimed_returns_null);
  RUN_TEST(test_owner_of_claimed_returns_name);
  RUN_TEST(test_owner_of_unregistered_returns_null);

  RUN_TEST(test_no_pin_conflicts_initially);
  RUN_TEST(test_entry_for_registered_not_null);
  RUN_TEST(test_entry_for_unregistered_returns_null);

  return UNITY_END();
}

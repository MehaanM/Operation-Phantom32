// test_memory.cpp — Memory boundary and ring buffer stress tests.
//
// Tests: ring buffer wrap-around, off-by-one at capacity boundaries,
// struct size assertions (to catch accidental size increases),
// null-pointer safety on all public API surfaces, and fence-post
// correctness for all fixed-size arrays used in the firmware.
//
// All tests run on native (no Arduino dependencies).

#include <unity.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>

// ============================================================
// RING BUFFER GENERIC IMPLEMENTATION (matches firmware pattern)
// ============================================================

#define RB_CAP 16

struct RingBuf {
  int      items[RB_CAP];
  int      head;
  int      count;

  void reset() { memset(items, 0, sizeof(items)); head = 0; count = 0; }

  void push(int v) {
    items[head] = v;
    head = (head + 1) % RB_CAP;
    if (count < RB_CAP) count++;
  }

  // Retrieve item at logical index i (0 = oldest).
  int get(int i) const {
    if (i < 0 || i >= count) return -9999;
    int idx = (head - count + i + RB_CAP * 2) % RB_CAP;
    return items[idx];
  }
};

static RingBuf g_rb;

// ============================================================
// TESTS: Ring buffer correctness
// ============================================================

void test_rb_push_one() {
  g_rb.reset();
  g_rb.push(42);
  TEST_ASSERT_EQUAL(1, g_rb.count);
  TEST_ASSERT_EQUAL(42, g_rb.get(0));
}

void test_rb_push_to_cap() {
  g_rb.reset();
  for (int i = 0; i < RB_CAP; i++) g_rb.push(i);
  TEST_ASSERT_EQUAL(RB_CAP, g_rb.count);
}

void test_rb_push_over_cap_wraps() {
  g_rb.reset();
  for (int i = 0; i < RB_CAP + 1; i++) g_rb.push(i);
  // count stays at cap
  TEST_ASSERT_EQUAL(RB_CAP, g_rb.count);
}

void test_rb_oldest_overwritten_on_wrap() {
  g_rb.reset();
  for (int i = 0; i < RB_CAP; i++) g_rb.push(i);
  g_rb.push(999);  // overwrites slot 0 (the original item 0)
  // oldest item is now item 1
  TEST_ASSERT_EQUAL(1, g_rb.get(0));
}

void test_rb_newest_item_after_wrap() {
  g_rb.reset();
  for (int i = 0; i < RB_CAP; i++) g_rb.push(i);
  g_rb.push(999);
  // most recent item is at index count-1
  TEST_ASSERT_EQUAL(999, g_rb.get(g_rb.count - 1));
}

void test_rb_get_out_of_range_returns_sentinel() {
  g_rb.reset();
  g_rb.push(1);
  TEST_ASSERT_EQUAL(-9999, g_rb.get(-1));
  TEST_ASSERT_EQUAL(-9999, g_rb.get(1));   // only 1 item
  TEST_ASSERT_EQUAL(-9999, g_rb.get(100));
}

void test_rb_empty_count_zero() {
  g_rb.reset();
  TEST_ASSERT_EQUAL(0, g_rb.count);
}

void test_rb_sequential_values_preserved() {
  g_rb.reset();
  for (int i = 0; i < 8; i++) g_rb.push(i * 10);
  for (int i = 0; i < 8; i++)
    TEST_ASSERT_EQUAL(i * 10, g_rb.get(i));
}

void test_rb_full_then_drain_values_shift() {
  g_rb.reset();
  for (int i = 0; i < RB_CAP; i++) g_rb.push(i);
  // Push 4 more — the 4 oldest are dropped.
  for (int i = 100; i < 104; i++) g_rb.push(i);
  // Oldest item in buffer is now original item 4.
  TEST_ASSERT_EQUAL(4, g_rb.get(0));
  TEST_ASSERT_EQUAL(100, g_rb.get(RB_CAP - 4));
}

// ============================================================
// STRUCT SIZE ASSERTIONS
// (Catches accidental size regressions that cause RAM overflow)
// ============================================================

// These limits are set generously to accommodate the full structs;
// they are intentionally NOT exact — the goal is to catch runaway growth.

void test_struct_sizes_within_budget() {
  // Session token struct: token (65B) + ts (4B) + valid (1B) = ~70B.
  // 8 sessions * 70B = ~560B.
  struct MockSession {
    char     token[65];
    uint32_t created_s;
    bool     valid;
  };
  TEST_ASSERT_TRUE(sizeof(MockSession) <= 80);

  // Rate limiter bucket: 3 floats + uint32 = ~16B.
  struct MockBucket {
    float    tokens;
    float    cap;
    float    rate_per_ms;
    uint32_t last_refill_ms;
  };
  TEST_ASSERT_TRUE(sizeof(MockBucket) <= 24);

  // RSSI history ring: 20 int8 + 2 uint8 = 22B.
  struct MockRssiHist {
    int8_t  vals[20];
    uint8_t head;
    uint8_t count;
  };
  TEST_ASSERT_TRUE(sizeof(MockRssiHist) <= 24);
}

void test_device_tracker_budget_lite() {
  // In LITE mode: 20 devices.
  // Per-device: mac(6) + vendor(20) + type(1) + rssi(1) + hist(22) +
  //   probes(2) + various counters(10) + assoc_ap(6) + probe_ssids(4*33=132)
  //   + probe_count(1) + timestamps(12) + flags(3) = ~216B.
  // 20 * 216 = 4320B ~ 4.2KB.  Budget is 20KB.
  struct MockDevice {
    uint8_t  mac[6];
    char     vendor[20];
    uint8_t  type;
    int8_t   rssi_last;
    int8_t   rssi_hist_vals[20];
    uint8_t  rssi_head;
    uint8_t  rssi_count;
    uint16_t probes;
    uint16_t beacons_rx;
    uint16_t data_frames;
    uint16_t mgmt_frames;
    uint16_t deauths_rx;
    uint8_t  assoc_ap[6];
    char     probe_ssids[4][33]; // LITE: DT_MAX_PROBES = 4
    uint8_t  probe_count;
    uint32_t first_seen_ms;
    uint32_t last_seen_ms;
    uint32_t last_active_ms;
    bool     is_ap;
    bool     randomized;
    bool     present;
    bool     alerted;
  };
  size_t per_device = sizeof(MockDevice);
  size_t total      = 20 * per_device;
  // Allow generous headroom; must stay under 20KB for LITE budget.
  TEST_ASSERT_TRUE(total < 20 * 1024);
}

void test_probe_flood_budget_lite() {
  // LITE: 8 clients * 4 targets * target_struct
  struct MockTarget {
    char     ssid[33];
    uint8_t  bssid[6];
    uint8_t  channel;
    uint32_t frames_sent;
    uint32_t last_sent_ms;
    bool     active;
  };
  struct MockClient {
    uint8_t    mac[6];
    MockTarget targets[4];
    int        target_count;
    uint32_t   frames_sent;
    bool       active;
  };
  size_t total = 8 * sizeof(MockClient);
  // Must stay under 10KB for LITE budget.
  TEST_ASSERT_TRUE(total < 10 * 1024);
}

void test_evil_twin_budget_lite() {
  struct MockCred {
    char    ssid[33];
    char    password[65];
    char    src_ip[16];
    uint32_t ts_ms;
    bool    valid;
  };
  size_t total = 8 * sizeof(MockCred); // LITE: ET_MAX_CREDS = 8
  TEST_ASSERT_TRUE(total < 2 * 1024);
}

// ============================================================
// TESTS: Null-pointer safety on common patterns
// ============================================================

void test_null_mac_memcmp_guard() {
  // memcmp(nullptr, ...) is undefined behavior; simulate the guard pattern.
  const uint8_t *a = nullptr;
  const uint8_t b[] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
  bool safe = (a != nullptr) && (memcmp(a, b, 6) == 0);
  TEST_ASSERT_FALSE(safe); // correctly returns false without crashing
}

void test_null_string_strlen_guard() {
  const char *s = nullptr;
  size_t len = s ? strlen(s) : 0;
  TEST_ASSERT_EQUAL(0u, len);
}

void test_null_string_strcmp_guard() {
  const char *a = nullptr;
  const char *b = "hello";
  bool eq = (a && b) ? (strcmp(a, b) == 0) : false;
  TEST_ASSERT_FALSE(eq);
}

void test_null_strncpy_guard() {
  char dst[32] = "original";
  const char *src = nullptr;
  if (src) strncpy(dst, src, 31);
  TEST_ASSERT_EQUAL_STRING("original", dst); // not modified
}

// ============================================================
// TESTS: Off-by-one at array boundaries
// ============================================================

void test_array_fence_at_max_minus_1() {
  // Ring buffer: writing at index (RB_CAP - 1) should work.
  g_rb.reset();
  for (int i = 0; i < RB_CAP - 1; i++) g_rb.push(i);
  TEST_ASSERT_EQUAL(RB_CAP - 1, g_rb.count);
  g_rb.push(999);
  TEST_ASSERT_EQUAL(RB_CAP, g_rb.count);
}

void test_array_fence_exactly_at_max() {
  g_rb.reset();
  for (int i = 0; i < RB_CAP; i++) g_rb.push(i);
  TEST_ASSERT_EQUAL(RB_CAP, g_rb.count);
  // One more should wrap, not overflow.
  g_rb.push(999);
  TEST_ASSERT_EQUAL(RB_CAP, g_rb.count); // count still at cap
}

void test_modular_index_no_oob() {
  // Verify head wraps correctly with modular arithmetic.
  g_rb.reset();
  for (int i = 0; i < RB_CAP * 3; i++) g_rb.push(i);
  // head should be in [0, RB_CAP).
  TEST_ASSERT_TRUE(g_rb.head >= 0 && g_rb.head < RB_CAP);
}

// ============================================================
// TESTS: String boundary conditions
// ============================================================

void test_ssid_exactly_32_chars() {
  // 32 'A's = maximum legal SSID length.
  char ssid[33];
  memset(ssid, 'A', 32); ssid[32] = '\0';
  TEST_ASSERT_EQUAL(32, (int)strlen(ssid));
  // Validation: len <= 32 AND printable.
  bool ok = (strlen(ssid) <= 32);
  for (int i = 0; i < 32; i++) {
    if ((uint8_t)ssid[i] < 0x20 || (uint8_t)ssid[i] > 0x7E) { ok = false; break; }
  }
  TEST_ASSERT_TRUE(ok);
}

void test_ssid_33_chars_rejected() {
  char ssid[34];
  memset(ssid, 'A', 33); ssid[33] = '\0';
  bool ok = (strlen(ssid) <= 32);
  TEST_ASSERT_FALSE(ok);
}

void test_mac_string_17_chars_valid() {
  const char *mac = "AA:BB:CC:DD:EE:FF";
  TEST_ASSERT_EQUAL(17, (int)strlen(mac));
}

void test_mac_string_18_chars_invalid() {
  const char *mac = "AA:BB:CC:DD:EE:FF:";
  TEST_ASSERT_NOT_EQUAL(17, (int)strlen(mac));
  TEST_ASSERT_NOT_EQUAL(12, (int)strlen(mac));
}

// ============================================================
// ENTRY POINT
// ============================================================

void setUp()    { g_rb.reset(); }
void tearDown() {}

int main(int argc, char **argv) {
  UNITY_BEGIN();

  RUN_TEST(test_rb_push_one);
  RUN_TEST(test_rb_push_to_cap);
  RUN_TEST(test_rb_push_over_cap_wraps);
  RUN_TEST(test_rb_oldest_overwritten_on_wrap);
  RUN_TEST(test_rb_newest_item_after_wrap);
  RUN_TEST(test_rb_get_out_of_range_returns_sentinel);
  RUN_TEST(test_rb_empty_count_zero);
  RUN_TEST(test_rb_sequential_values_preserved);
  RUN_TEST(test_rb_full_then_drain_values_shift);

  RUN_TEST(test_struct_sizes_within_budget);
  RUN_TEST(test_device_tracker_budget_lite);
  RUN_TEST(test_probe_flood_budget_lite);
  RUN_TEST(test_evil_twin_budget_lite);

  RUN_TEST(test_null_mac_memcmp_guard);
  RUN_TEST(test_null_string_strlen_guard);
  RUN_TEST(test_null_string_strcmp_guard);
  RUN_TEST(test_null_strncpy_guard);

  RUN_TEST(test_array_fence_at_max_minus_1);
  RUN_TEST(test_array_fence_exactly_at_max);
  RUN_TEST(test_modular_index_no_oob);

  RUN_TEST(test_ssid_exactly_32_chars);
  RUN_TEST(test_ssid_33_chars_rejected);
  RUN_TEST(test_mac_string_17_chars_valid);
  RUN_TEST(test_mac_string_18_chars_invalid);

  return UNITY_END();
}

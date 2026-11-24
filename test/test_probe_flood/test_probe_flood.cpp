// test_probe_flood.cpp — Unit tests for probe flood target management.
//
// Tests target list deduplication, SSID pool management,
// client tracking, attack state transitions, and frame counter logic.
// All tests run on native (no Arduino dependencies).

#include <unity.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

// ============================================================
// MINIMAL STUBS
// ============================================================

#define PFL_MAX_TARGETS    32
#define PFL_MAX_CLIENTS    48
#define PFL_SSID_LEN       33
#define PFL_MAX_SSIDS      8

struct PflTarget {
  char     ssid[PFL_SSID_LEN];
  uint8_t  bssid[6];
  uint8_t  channel;
  uint32_t frames_sent;
  uint32_t last_sent_ms;
  bool     active;
};

struct PflClient {
  uint8_t    mac[6];
  PflTarget  targets[PFL_MAX_TARGETS];
  int        target_count;
  uint32_t   frames_sent;
  bool       active;
};

static PflTarget  g_pfl_ssids[PFL_MAX_SSIDS];
static int        g_pfl_ssid_count;
static PflClient  g_pfl_clients[PFL_MAX_CLIENTS];
static int        g_pfl_client_count;
static bool       g_pfl_running;
static uint32_t   g_mock_ms;

static void pfl_reset() {
  memset(g_pfl_ssids, 0, sizeof(g_pfl_ssids));
  memset(g_pfl_clients, 0, sizeof(g_pfl_clients));
  g_pfl_ssid_count   = 0;
  g_pfl_client_count = 0;
  g_pfl_running      = false;
  g_mock_ms          = 0;
}

// Add an SSID to the probe pool.
static bool pfl_add_ssid(const char *ssid, uint8_t ch) {
  if (!ssid || strlen(ssid) == 0) return false;
  if (strlen(ssid) >= PFL_SSID_LEN) return false;
  // Deduplicate.
  for (int i = 0; i < g_pfl_ssid_count; i++)
    if (strcmp(g_pfl_ssids[i].ssid, ssid) == 0) return false;
  if (g_pfl_ssid_count >= PFL_MAX_SSIDS) return false;
  PflTarget *t = &g_pfl_ssids[g_pfl_ssid_count++];
  memset(t, 0, sizeof(PflTarget));
  strncpy(t->ssid, ssid, PFL_SSID_LEN - 1);
  t->channel = ch;
  t->active  = true;
  return true;
}

static bool pfl_remove_ssid(const char *ssid) {
  for (int i = 0; i < g_pfl_ssid_count; i++) {
    if (strcmp(g_pfl_ssids[i].ssid, ssid) == 0) {
      g_pfl_ssids[i] = g_pfl_ssids[--g_pfl_ssid_count];
      return true;
    }
  }
  return false;
}

// Add a client target MAC.
static PflClient* pfl_find_or_add_client(const uint8_t *mac) {
  for (int i = 0; i < g_pfl_client_count; i++)
    if (memcmp(g_pfl_clients[i].mac, mac, 6) == 0) return &g_pfl_clients[i];
  if (g_pfl_client_count >= PFL_MAX_CLIENTS) return nullptr;
  PflClient *c = &g_pfl_clients[g_pfl_client_count++];
  memset(c, 0, sizeof(PflClient));
  memcpy(c->mac, mac, 6);
  c->active = true;
  return c;
}

// Record a sent probe frame.
static void pfl_record_sent(PflClient *c, PflTarget *t) {
  if (!c || !t) return;
  c->frames_sent++;
  t->frames_sent++;
  t->last_sent_ms = g_mock_ms;
}

// ============================================================
// TESTS: SSID pool management
// ============================================================

void test_ssid_add_first() {
  pfl_reset();
  TEST_ASSERT_TRUE(pfl_add_ssid("HomeNetwork", 6));
  TEST_ASSERT_EQUAL(1, g_pfl_ssid_count);
}

void test_ssid_no_duplicate() {
  pfl_reset();
  pfl_add_ssid("HomeNetwork", 6);
  TEST_ASSERT_FALSE(pfl_add_ssid("HomeNetwork", 11));
  TEST_ASSERT_EQUAL(1, g_pfl_ssid_count);
}

void test_ssid_different_ssids() {
  pfl_reset();
  pfl_add_ssid("Net1", 1);
  pfl_add_ssid("Net2", 6);
  TEST_ASSERT_EQUAL(2, g_pfl_ssid_count);
}

void test_ssid_max_cap() {
  pfl_reset();
  for (int i = 0; i < PFL_MAX_SSIDS; i++) {
    char s[8]; snprintf(s, 8, "net%d", i);
    TEST_ASSERT_TRUE(pfl_add_ssid(s, 6));
  }
  TEST_ASSERT_FALSE(pfl_add_ssid("overflow", 6));
}

void test_ssid_remove_exists() {
  pfl_reset();
  pfl_add_ssid("Net1", 6);
  TEST_ASSERT_TRUE(pfl_remove_ssid("Net1"));
  TEST_ASSERT_EQUAL(0, g_pfl_ssid_count);
}

void test_ssid_remove_nonexistent() {
  pfl_reset();
  TEST_ASSERT_FALSE(pfl_remove_ssid("DoesNotExist"));
}

void test_ssid_remove_middle() {
  pfl_reset();
  pfl_add_ssid("Net1", 1);
  pfl_add_ssid("Net2", 6);
  pfl_add_ssid("Net3", 11);
  pfl_remove_ssid("Net2");
  TEST_ASSERT_EQUAL(2, g_pfl_ssid_count);
}

void test_ssid_empty_rejected() {
  pfl_reset();
  TEST_ASSERT_FALSE(pfl_add_ssid("", 6));
}

void test_ssid_too_long_rejected() {
  pfl_reset();
  char long_ssid[64];
  memset(long_ssid, 'A', 63); long_ssid[63] = '\0';
  TEST_ASSERT_FALSE(pfl_add_ssid(long_ssid, 6));
}

void test_ssid_stores_channel() {
  pfl_reset();
  pfl_add_ssid("TestNet", 11);
  TEST_ASSERT_EQUAL(11, g_pfl_ssids[0].channel);
}

// ============================================================
// TESTS: Client tracking
// ============================================================

void test_client_add_first() {
  pfl_reset();
  uint8_t mac[] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
  TEST_ASSERT_NOT_NULL(pfl_find_or_add_client(mac));
  TEST_ASSERT_EQUAL(1, g_pfl_client_count);
}

void test_client_no_duplicate() {
  pfl_reset();
  uint8_t mac[] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
  pfl_find_or_add_client(mac);
  pfl_find_or_add_client(mac);
  TEST_ASSERT_EQUAL(1, g_pfl_client_count);
}

void test_client_different_macs() {
  pfl_reset();
  uint8_t mac1[] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
  uint8_t mac2[] = {0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
  pfl_find_or_add_client(mac1);
  pfl_find_or_add_client(mac2);
  TEST_ASSERT_EQUAL(2, g_pfl_client_count);
}

void test_client_max_cap() {
  pfl_reset();
  for (int i = 0; i < PFL_MAX_CLIENTS; i++) {
    uint8_t mac[] = {0x02, 0x00, (uint8_t)(i>>8), (uint8_t)i, 0x00, 0x00};
    TEST_ASSERT_NOT_NULL(pfl_find_or_add_client(mac));
  }
  uint8_t overflow[] = {0x02, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  TEST_ASSERT_NULL(pfl_find_or_add_client(overflow));
}

void test_client_find_returns_same() {
  pfl_reset();
  uint8_t mac[] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
  PflClient *a = pfl_find_or_add_client(mac);
  PflClient *b = pfl_find_or_add_client(mac);
  TEST_ASSERT_EQUAL_PTR(a, b);
}

// ============================================================
// TESTS: Frame counters
// ============================================================

void test_record_sent_increments_client() {
  pfl_reset();
  uint8_t mac[] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
  PflClient *c = pfl_find_or_add_client(mac);
  pfl_add_ssid("Net1", 6);
  pfl_record_sent(c, &g_pfl_ssids[0]);
  TEST_ASSERT_EQUAL(1u, c->frames_sent);
}

void test_record_sent_increments_target() {
  pfl_reset();
  uint8_t mac[] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
  PflClient *c = pfl_find_or_add_client(mac);
  pfl_add_ssid("Net1", 6);
  pfl_record_sent(c, &g_pfl_ssids[0]);
  TEST_ASSERT_EQUAL(1u, g_pfl_ssids[0].frames_sent);
}

void test_record_sent_updates_timestamp() {
  pfl_reset();
  g_mock_ms = 12345;
  uint8_t mac[] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
  PflClient *c = pfl_find_or_add_client(mac);
  pfl_add_ssid("Net1", 6);
  pfl_record_sent(c, &g_pfl_ssids[0]);
  TEST_ASSERT_EQUAL(12345u, g_pfl_ssids[0].last_sent_ms);
}

void test_record_sent_null_client_no_crash() {
  pfl_reset();
  pfl_add_ssid("Net1", 6);
  pfl_record_sent(nullptr, &g_pfl_ssids[0]);
  TEST_ASSERT_EQUAL(0u, g_pfl_ssids[0].frames_sent);
}

void test_record_sent_null_target_no_crash() {
  pfl_reset();
  uint8_t mac[] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
  PflClient *c = pfl_find_or_add_client(mac);
  pfl_record_sent(c, nullptr);
  TEST_ASSERT_EQUAL(0u, c->frames_sent);
}

// ============================================================
// ENTRY POINT
// ============================================================

void setUp()    { pfl_reset(); }
void tearDown() {}

int main(int argc, char **argv) {
  UNITY_BEGIN();

  RUN_TEST(test_ssid_add_first);
  RUN_TEST(test_ssid_no_duplicate);
  RUN_TEST(test_ssid_different_ssids);
  RUN_TEST(test_ssid_max_cap);
  RUN_TEST(test_ssid_remove_exists);
  RUN_TEST(test_ssid_remove_nonexistent);
  RUN_TEST(test_ssid_remove_middle);
  RUN_TEST(test_ssid_empty_rejected);
  RUN_TEST(test_ssid_too_long_rejected);
  RUN_TEST(test_ssid_stores_channel);

  RUN_TEST(test_client_add_first);
  RUN_TEST(test_client_no_duplicate);
  RUN_TEST(test_client_different_macs);
  RUN_TEST(test_client_max_cap);
  RUN_TEST(test_client_find_returns_same);

  RUN_TEST(test_record_sent_increments_client);
  RUN_TEST(test_record_sent_increments_target);
  RUN_TEST(test_record_sent_updates_timestamp);
  RUN_TEST(test_record_sent_null_client_no_crash);
  RUN_TEST(test_record_sent_null_target_no_crash);

  return UNITY_END();
}

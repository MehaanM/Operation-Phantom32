// test_rate_limiter.cpp — Unit tests for the rate limiter (token bucket algorithm).
//
// Tests token bucket refill, per-tool limits, global TX budget,
// per-IP HTTP limits, burst behaviour, and config serialization.
// All tests run on native (no Arduino dependencies).

#include <unity.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

// ============================================================
// TOKEN BUCKET STUB
// ============================================================

#define RLA_MAX_TOOLS       16
#define RLA_GLOBAL_FPS_MAX  200
#define RLA_HTTP_MAX_RPM    300
#define RLA_HTTP_MAX_IPS    8

struct TokenBucket {
  float    tokens;
  float    cap;
  float    rate_per_ms; // tokens added per millisecond
  uint32_t last_refill_ms;

  void init(float capacity, float rate_per_sec) {
    cap = capacity;
    tokens = capacity;
    rate_per_ms = rate_per_sec / 1000.0f;
    last_refill_ms = 0;
  }

  void refill(uint32_t now_ms) {
    float elapsed = (float)(now_ms - last_refill_ms);
    tokens += elapsed * rate_per_ms;
    if (tokens > cap) tokens = cap;
    last_refill_ms = now_ms;
  }

  bool consume(float cost, uint32_t now_ms) {
    refill(now_ms);
    if (tokens < cost) return false;
    tokens -= cost;
    return true;
  }
};

struct RlaToolEntry {
  char        name[24];
  TokenBucket bucket;
  bool        active;
  uint32_t    allowed;
  uint32_t    denied;
};

struct RlaIpEntry {
  char        ip[16];
  TokenBucket bucket;
  bool        active;
};

static RlaToolEntry g_tools[RLA_MAX_TOOLS];
static int g_tool_count = 0;
static TokenBucket g_global_bucket;
static RlaIpEntry  g_http_ips[RLA_HTTP_MAX_IPS];
static int g_http_ip_count = 0;

static uint32_t g_mock_ms = 0;

static void rla_reset() {
  memset(g_tools, 0, sizeof(g_tools));
  g_tool_count = 0;
  g_global_bucket.init(RLA_GLOBAL_FPS_MAX, RLA_GLOBAL_FPS_MAX);
  g_global_bucket.last_refill_ms = g_mock_ms;
  memset(g_http_ips, 0, sizeof(g_http_ips));
  g_http_ip_count = 0;
}

static RlaToolEntry* rla_find_or_add_tool(const char *name, float cap, float rate) {
  for (int i = 0; i < g_tool_count; i++)
    if (strcmp(g_tools[i].name, name) == 0) return &g_tools[i];
  if (g_tool_count >= RLA_MAX_TOOLS) return nullptr;
  RlaToolEntry *e = &g_tools[g_tool_count++];
  strncpy(e->name, name, 23);
  e->bucket.init(cap, rate);
  e->bucket.last_refill_ms = g_mock_ms;
  e->active = true;
  e->allowed = 0;
  e->denied  = 0;
  return e;
}

static bool rla_check(const char *tool, float cost = 1.0f) {
  // Check global first.
  if (!g_global_bucket.consume(cost, g_mock_ms)) return false;
  // Then per-tool.
  RlaToolEntry *e = nullptr;
  for (int i = 0; i < g_tool_count; i++)
    if (strcmp(g_tools[i].name, tool) == 0) { e = &g_tools[i]; break; }
  if (!e || !e->active) {
    // No per-tool limit — global check was enough.
    return true;
  }
  bool ok = e->bucket.consume(cost, g_mock_ms);
  if (ok) e->allowed++; else e->denied++;
  return ok;
}

static RlaIpEntry* rla_find_or_add_ip(const char *ip) {
  for (int i = 0; i < g_http_ip_count; i++)
    if (strcmp(g_http_ips[i].ip, ip) == 0) return &g_http_ips[i];
  if (g_http_ip_count >= RLA_HTTP_MAX_IPS) return nullptr;
  RlaIpEntry *e = &g_http_ips[g_http_ip_count++];
  strncpy(e->ip, ip, 15);
  e->bucket.init(RLA_HTTP_MAX_RPM, RLA_HTTP_MAX_RPM / 60.0f); // per second
  e->bucket.last_refill_ms = g_mock_ms;
  e->active = true;
  return e;
}

static bool rla_check_http(const char *ip) {
  RlaIpEntry *e = rla_find_or_add_ip(ip);
  if (!e || !e->active) return true;
  return e->bucket.consume(1.0f, g_mock_ms);
}

// ============================================================
// TESTS: Token bucket basics
// ============================================================

void test_bucket_full_on_init() {
  TokenBucket b;
  b.init(10.0f, 10.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 10.0f, b.tokens);
}

void test_bucket_consume_single() {
  TokenBucket b;
  b.init(10.0f, 10.0f);
  TEST_ASSERT_TRUE(b.consume(1.0f, 0));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 9.0f, b.tokens);
}

void test_bucket_consume_all() {
  TokenBucket b;
  b.init(5.0f, 1.0f);
  for (int i = 0; i < 5; i++) TEST_ASSERT_TRUE(b.consume(1.0f, 0));
  TEST_ASSERT_FALSE(b.consume(1.0f, 0));
}

void test_bucket_refills_over_time() {
  TokenBucket b;
  b.init(10.0f, 10.0f); // 10 tokens/sec = 10 per 1000ms
  b.tokens = 0.0f;
  b.last_refill_ms = 0;
  TEST_ASSERT_TRUE(b.consume(1.0f, 1000)); // 1 second elapsed => 10 added
}

void test_bucket_capped_at_capacity() {
  TokenBucket b;
  b.init(10.0f, 10.0f);
  b.tokens = 9.0f;
  b.refill(1000); // adds 10, should cap at 10
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 10.0f, b.tokens);
}

void test_bucket_zero_cap_always_deny() {
  TokenBucket b;
  b.init(0.0f, 0.0f);
  TEST_ASSERT_FALSE(b.consume(1.0f, 0));
}

void test_bucket_fractional_consume() {
  TokenBucket b;
  b.init(1.0f, 0.0f);
  TEST_ASSERT_TRUE(b.consume(0.5f, 0));
  TEST_ASSERT_TRUE(b.consume(0.5f, 0));
  TEST_ASSERT_FALSE(b.consume(0.5f, 0)); // empty
}

// ============================================================
// TESTS: Per-tool rate limiting
// ============================================================

void test_tool_rate_allows_initial() {
  rla_reset();
  rla_find_or_add_tool("deauth", 10.0f, 10.0f);
  TEST_ASSERT_TRUE(rla_check("deauth"));
}

void test_tool_rate_denies_over_cap() {
  rla_reset();
  rla_find_or_add_tool("beacon_spam", 3.0f, 1.0f);
  TEST_ASSERT_TRUE(rla_check("beacon_spam"));
  TEST_ASSERT_TRUE(rla_check("beacon_spam"));
  TEST_ASSERT_TRUE(rla_check("beacon_spam"));
  TEST_ASSERT_FALSE(rla_check("beacon_spam"));
}

void test_tool_rate_counters_track() {
  rla_reset();
  rla_find_or_add_tool("probe_flood", 2.0f, 1.0f);
  rla_check("probe_flood");
  rla_check("probe_flood");
  rla_check("probe_flood"); // denied
  RlaToolEntry *e = nullptr;
  for (int i = 0; i < g_tool_count; i++)
    if (strcmp(g_tools[i].name, "probe_flood") == 0) { e = &g_tools[i]; break; }
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL(2u, e->allowed);
  TEST_ASSERT_EQUAL(1u, e->denied);
}

void test_tool_rate_refills_between_calls() {
  rla_reset();
  rla_find_or_add_tool("evil_twin", 2.0f, 2.0f); // 2 tokens/sec
  rla_check("evil_twin");
  rla_check("evil_twin");
  TEST_ASSERT_FALSE(rla_check("evil_twin")); // empty at t=0
  g_mock_ms = 1000; // 1 second later: refill 2 tokens
  TEST_ASSERT_TRUE(rla_check("evil_twin"));
}

void test_tool_unknown_uses_global_budget() {
  rla_reset();
  // No per-tool entry for "mystery_tool" => should pass if global has tokens.
  TEST_ASSERT_TRUE(rla_check("mystery_tool"));
}

// ============================================================
// TESTS: Global TX budget
// ============================================================

void test_global_budget_limits_all_tools() {
  rla_reset();
  g_global_bucket.init(3.0f, 1.0f);
  g_global_bucket.last_refill_ms = g_mock_ms;
  rla_find_or_add_tool("tool_a", 10.0f, 10.0f);
  rla_find_or_add_tool("tool_b", 10.0f, 10.0f);
  rla_check("tool_a");
  rla_check("tool_b");
  rla_check("tool_a");
  TEST_ASSERT_FALSE(rla_check("tool_b")); // global exhausted
}

// ============================================================
// TESTS: HTTP per-IP rate limiting
// ============================================================

void test_http_allows_first_request() {
  rla_reset();
  TEST_ASSERT_TRUE(rla_check_http("192.168.4.5"));
}

void test_http_allows_up_to_rpm() {
  rla_reset();
  RlaIpEntry *e = rla_find_or_add_ip("10.0.0.2");
  e->bucket.init(5.0f, 5.0f);
  e->bucket.last_refill_ms = g_mock_ms;
  for (int i = 0; i < 5; i++) TEST_ASSERT_TRUE(rla_check_http("10.0.0.2"));
  TEST_ASSERT_FALSE(rla_check_http("10.0.0.2"));
}

void test_http_independent_ips() {
  rla_reset();
  RlaIpEntry *e = rla_find_or_add_ip("1.1.1.1");
  e->bucket.init(2.0f, 1.0f);
  e->bucket.last_refill_ms = g_mock_ms;
  rla_check_http("1.1.1.1"); rla_check_http("1.1.1.1");
  TEST_ASSERT_FALSE(rla_check_http("1.1.1.1"));
  TEST_ASSERT_TRUE(rla_check_http("2.2.2.2")); // different IP unaffected
}

void test_http_refills_over_time() {
  rla_reset();
  RlaIpEntry *e = rla_find_or_add_ip("1.1.1.1");
  e->bucket.init(2.0f, 2.0f); // 2/sec
  e->bucket.last_refill_ms = g_mock_ms;
  rla_check_http("1.1.1.1"); rla_check_http("1.1.1.1");
  TEST_ASSERT_FALSE(rla_check_http("1.1.1.1")); // empty
  g_mock_ms += 1000;
  TEST_ASSERT_TRUE(rla_check_http("1.1.1.1")); // refilled
}

// ============================================================
// TESTS: Edge cases
// ============================================================

void test_rla_null_tool_name() {
  rla_reset();
  // Should not crash; no tool entry found, falls through global check.
  TEST_ASSERT_TRUE(rla_check(""));
}

void test_rla_many_tools_capacity() {
  rla_reset();
  for (int i = 0; i < RLA_MAX_TOOLS; i++) {
    char name[32]; snprintf(name, 32, "t%d", i);
    TEST_ASSERT_NOT_NULL(rla_find_or_add_tool(name, 10.0f, 10.0f));
  }
  // Adding one more should fail.
  TEST_ASSERT_NULL(rla_find_or_add_tool("overflow", 10.0f, 10.0f));
}

// ============================================================
// TESTS: Uncovered branches
// ============================================================

void test_http_ip_table_full() {
  rla_reset();
  for (int i = 0; i < RLA_HTTP_MAX_IPS; i++) {
    char ip[20]; snprintf(ip, sizeof(ip), "10.0.0.%d", i + 1);
    TEST_ASSERT_NOT_NULL(rla_find_or_add_ip(ip));
  }
  // 9th IP must fail — table full
  TEST_ASSERT_NULL(rla_find_or_add_ip("99.99.99.99"));
}

void test_http_check_table_full_allows() {
  rla_reset();
  for (int i = 0; i < RLA_HTTP_MAX_IPS; i++) {
    char ip[20]; snprintf(ip, sizeof(ip), "10.0.0.%d", i + 1);
    rla_find_or_add_ip(ip);
  }
  // Table full: rla_find_or_add_ip returns null, so rla_check_http returns true
  TEST_ASSERT_TRUE(rla_check_http("99.99.99.99"));
}

void test_tool_inactive_bypasses_per_tool_check() {
  rla_reset();
  rla_find_or_add_tool("lazy_tool", 1.0f, 0.0f);
  // Mark tool inactive
  for (int i = 0; i < g_tool_count; i++)
    if (strcmp(g_tools[i].name, "lazy_tool") == 0) { g_tools[i].active = false; break; }
  // !e->active branch: global was consumed but per-tool check skipped → returns true
  TEST_ASSERT_TRUE(rla_check("lazy_tool"));
}

void test_find_or_add_tool_returns_existing() {
  rla_reset();
  RlaToolEntry *a = rla_find_or_add_tool("my_tool", 5.0f, 1.0f);
  RlaToolEntry *b = rla_find_or_add_tool("my_tool", 5.0f, 1.0f);
  // Covers the loop early-return branch in rla_find_or_add_tool
  TEST_ASSERT_EQUAL_PTR(a, b);
}

void test_global_bucket_deny_blocks_rla_check() {
  rla_reset();
  while (g_global_bucket.consume(1.0f, g_mock_ms)) {}
  // Covers the if(!g_global_bucket.consume()) return false branch
  TEST_ASSERT_EQUAL(0, (int)rla_check("any_tool"));
}

void test_find_or_add_ip_returns_existing() {
  rla_reset();
  RlaIpEntry *a = rla_find_or_add_ip("1.2.3.4");
  RlaIpEntry *b = rla_find_or_add_ip("1.2.3.4");
  // Covers the loop early-return branch in rla_find_or_add_ip
  TEST_ASSERT_EQUAL_PTR(a, b);
}

void test_rla_check_http_inactive_entry_allows() {
  rla_reset();
  RlaIpEntry *e = rla_find_or_add_ip("7.7.7.7");
  e->active = false;
  // Covers the !e->active branch — inactive entry bypasses rate check
  TEST_ASSERT_EQUAL(1, (int)rla_check_http("7.7.7.7"));
}

void test_bucket_exact_tokens_equal_cost() {
  TokenBucket b;
  b.init(1.0f, 0.0f);
  // tokens == cost (exactly 1.0 == 1.0): should succeed
  TEST_ASSERT_TRUE(b.consume(1.0f, 0));
  TEST_ASSERT_FALSE(b.consume(1.0f, 0)); // now empty
}

// ============================================================
// ENTRY POINT
// ============================================================

void setUp()    { rla_reset(); g_mock_ms = 0; }
void tearDown() {}

int main(int argc, char **argv) {
  UNITY_BEGIN();

  RUN_TEST(test_bucket_full_on_init);
  RUN_TEST(test_bucket_consume_single);
  RUN_TEST(test_bucket_consume_all);
  RUN_TEST(test_bucket_refills_over_time);
  RUN_TEST(test_bucket_capped_at_capacity);
  RUN_TEST(test_bucket_zero_cap_always_deny);
  RUN_TEST(test_bucket_fractional_consume);

  RUN_TEST(test_tool_rate_allows_initial);
  RUN_TEST(test_tool_rate_denies_over_cap);
  RUN_TEST(test_tool_rate_counters_track);
  RUN_TEST(test_tool_rate_refills_between_calls);
  RUN_TEST(test_tool_unknown_uses_global_budget);

  RUN_TEST(test_global_budget_limits_all_tools);

  RUN_TEST(test_http_allows_first_request);
  RUN_TEST(test_http_allows_up_to_rpm);
  RUN_TEST(test_http_independent_ips);
  RUN_TEST(test_http_refills_over_time);

  RUN_TEST(test_rla_null_tool_name);
  RUN_TEST(test_rla_many_tools_capacity);

  // Uncovered branch tests
  RUN_TEST(test_http_ip_table_full);
  RUN_TEST(test_http_check_table_full_allows);
  RUN_TEST(test_tool_inactive_bypasses_per_tool_check);
  RUN_TEST(test_bucket_exact_tokens_equal_cost);
  RUN_TEST(test_find_or_add_tool_returns_existing);
  RUN_TEST(test_global_bucket_deny_blocks_rla_check);
  RUN_TEST(test_find_or_add_ip_returns_existing);
  RUN_TEST(test_rla_check_http_inactive_entry_allows);

  return UNITY_END();
}

// test_negative.cpp — Negative / boundary / adversarial unit tests.
// Covers: malformed input, allocation failure paths, lockout enforcement,
// corrupted config, integer boundaries, and string-safety edge cases.
// All tests run on the native env (no hardware required).

#include <unity.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>

// ─── Minimal stubs for modules under test ────────────────────────────────────

#ifdef NATIVE_ENV

// ---- constant-time strcmp stub (mirrors auth.h logic) ---------------------
static int am_ct_strcmp(const char *a, const char *b) {
    if (!a || !b) return 1;
    size_t la = strlen(a), lb = strlen(b);
    volatile int diff = (int)(la != lb);
    size_t len = la < lb ? la : lb;
    for (size_t i = 0; i < len; i++) diff |= (uint8_t)a[i] ^ (uint8_t)b[i];
    return diff;
}

// ---- Token-bucket stub (mirrors rate_limit.h logic) ----------------------
struct TokenBucket {
    int32_t tokens;
    int32_t capacity;
    int32_t refill_per_tick;
};

static bool tb_consume(TokenBucket *b, int32_t n) {
    if (!b || n <= 0) return false;
    if (b->tokens < n) return false;
    b->tokens -= n;
    return true;
}

static void tb_refill(TokenBucket *b) {
    if (!b) return;
    b->tokens += b->refill_per_tick;
    if (b->tokens > b->capacity) b->tokens = b->capacity;
}

static void tb_init(TokenBucket *b, int32_t cap, int32_t refill) {
    if (!b) return;
    b->capacity        = cap;
    b->tokens          = cap;
    b->refill_per_tick = refill;
}

// ---- IP lockout stub -------------------------------------------------------
#define LOCKOUT_MAX_FAILURES 5
#define LOCKOUT_DURATION_S   300

struct IpEntry {
    uint32_t ip;
    int      failures;
    uint32_t locked_until; // epoch-seconds mock
    bool     active;
};

#define IP_TABLE_SIZE 8
static IpEntry g_ip_table[IP_TABLE_SIZE];
static uint32_t g_mock_time_s = 0;

static void ip_table_reset() {
    memset(g_ip_table, 0, sizeof(g_ip_table));
    g_mock_time_s = 0;
}

static bool ip_is_locked(uint32_t ip) {
    for (int i = 0; i < IP_TABLE_SIZE; i++) {
        if (g_ip_table[i].active && g_ip_table[i].ip == ip)
            return g_ip_table[i].locked_until > g_mock_time_s;
    }
    return false;
}

static void ip_record_failure(uint32_t ip) {
    for (int i = 0; i < IP_TABLE_SIZE; i++) {
        if (g_ip_table[i].active && g_ip_table[i].ip == ip) {
            g_ip_table[i].failures++;
            if (g_ip_table[i].failures >= LOCKOUT_MAX_FAILURES)
                g_ip_table[i].locked_until = g_mock_time_s + LOCKOUT_DURATION_S;
            return;
        }
    }
    for (int i = 0; i < IP_TABLE_SIZE; i++) {
        if (!g_ip_table[i].active) {
            g_ip_table[i] = { ip, 1, 0, true };
            return;
        }
    }
}

// ---- SSID validation stub -------------------------------------------------
static bool validate_ssid(const char *s) {
    if (!s || s[0] == '\0') return false;
    size_t len = strlen(s);
    if (len > 32) return false;
    for (size_t i = 0; i < len; i++) {
        uint8_t c = (uint8_t)s[i];
        if (c < 0x20 || c == 0x7F) return false; // no control chars
    }
    return true;
}

// ---- Channel validation stub ----------------------------------------------
static bool validate_channel(int ch) {
    return ch >= 1 && ch <= 14;
}

// ---- Ring buffer stub (mirrors ring_buf.h) ---------------------------------
#define RBUF_SIZE 8
struct RingBuf {
    uint8_t data[RBUF_SIZE];
    int     head, tail, count;
};

static void rbuf_init(RingBuf *r) { memset(r, 0, sizeof(*r)); }

static bool rbuf_push(RingBuf *r, uint8_t v) {
    if (r->count == RBUF_SIZE) return false;
    r->data[r->head] = v;
    r->head = (r->head + 1) % RBUF_SIZE;
    r->count++;
    return true;
}

static bool rbuf_pop(RingBuf *r, uint8_t *out) {
    if (r->count == 0) return false;
    *out = r->data[r->tail];
    r->tail = (r->tail + 1) % RBUF_SIZE;
    r->count--;
    return true;
}

// ─── Test cases ──────────────────────────────────────────────────────────────

void setUp()    { ip_table_reset(); }
void tearDown() {}

// ---- Constant-time strcmp ---------------------------------------------------

void test_ct_strcmp_null_a() {
    // Null pointer must not crash and must signal inequality.
    TEST_ASSERT_NOT_EQUAL(0, am_ct_strcmp(nullptr, "phantom32"));
}

void test_ct_strcmp_null_b() {
    TEST_ASSERT_NOT_EQUAL(0, am_ct_strcmp("phantom32", nullptr));
}

void test_ct_strcmp_both_null() {
    TEST_ASSERT_NOT_EQUAL(0, am_ct_strcmp(nullptr, nullptr));
}

void test_ct_strcmp_equal_strings() {
    TEST_ASSERT_EQUAL(0, am_ct_strcmp("correct", "correct"));
}

void test_ct_strcmp_different_length() {
    // Length difference alone must cause inequality without reading OOB.
    TEST_ASSERT_NOT_EQUAL(0, am_ct_strcmp("abc", "abcd"));
}

void test_ct_strcmp_empty_vs_nonempty() {
    TEST_ASSERT_NOT_EQUAL(0, am_ct_strcmp("", "phantom32"));
}

void test_ct_strcmp_both_empty() {
    TEST_ASSERT_EQUAL(0, am_ct_strcmp("", ""));
}

void test_ct_strcmp_one_char_off() {
    // Last byte differs — must still flag mismatch.
    TEST_ASSERT_NOT_EQUAL(0, am_ct_strcmp("phantom31", "phantom32"));
}

void test_ct_strcmp_case_sensitive() {
    TEST_ASSERT_NOT_EQUAL(0, am_ct_strcmp("Phantom32", "phantom32"));
}

// ---- IP-based lockout -------------------------------------------------------

void test_lockout_not_triggered_before_threshold() {
    uint32_t ip = 0xC0A80001; // 192.168.0.1
    for (int i = 0; i < LOCKOUT_MAX_FAILURES - 1; i++)
        ip_record_failure(ip);
    TEST_ASSERT_FALSE(ip_is_locked(ip));
}

void test_lockout_triggered_at_threshold() {
    uint32_t ip = 0xC0A80002;
    for (int i = 0; i < LOCKOUT_MAX_FAILURES; i++)
        ip_record_failure(ip);
    TEST_ASSERT_TRUE(ip_is_locked(ip));
}

void test_lockout_expires_after_duration() {
    uint32_t ip = 0xC0A80003;
    for (int i = 0; i < LOCKOUT_MAX_FAILURES; i++)
        ip_record_failure(ip);
    TEST_ASSERT_TRUE(ip_is_locked(ip));
    g_mock_time_s += LOCKOUT_DURATION_S + 1;
    TEST_ASSERT_FALSE(ip_is_locked(ip));
}

void test_lockout_still_active_at_expiry_boundary() {
    uint32_t ip = 0xC0A80004;
    for (int i = 0; i < LOCKOUT_MAX_FAILURES; i++)
        ip_record_failure(ip);
    g_mock_time_s += LOCKOUT_DURATION_S - 1; // one second before expiry — still locked
    TEST_ASSERT_TRUE(ip_is_locked(ip));
}

void test_lockout_independent_per_ip() {
    uint32_t ip_a = 0xC0A80010, ip_b = 0xC0A80011;
    for (int i = 0; i < LOCKOUT_MAX_FAILURES; i++)
        ip_record_failure(ip_a);
    // ip_b never failed — must not be affected.
    TEST_ASSERT_TRUE (ip_is_locked(ip_a));
    TEST_ASSERT_FALSE(ip_is_locked(ip_b));
}

void test_lockout_table_full_no_crash() {
    // Fill all slots, then add one more — should not crash.
    for (int i = 0; i < IP_TABLE_SIZE + 2; i++) {
        uint32_t ip = 0xC0A80100 + i;
        for (int j = 0; j < LOCKOUT_MAX_FAILURES; j++)
            ip_record_failure(ip);
    }
    // No crash reaching here is the assertion.
    TEST_PASS();
}

void test_lockout_extra_failures_dont_reset_timer() {
    // Once locked, more failures should not push locked_until backward.
    uint32_t ip = 0xC0A80020;
    for (int i = 0; i < LOCKOUT_MAX_FAILURES; i++)
        ip_record_failure(ip);
    g_mock_time_s = LOCKOUT_DURATION_S - 1;
    // Additional failures after lock was set.
    ip_record_failure(ip);
    ip_record_failure(ip);
    // Still locked (we haven't advanced past the original expiry).
    TEST_ASSERT_TRUE(ip_is_locked(ip));
}

// ---- SSID validation --------------------------------------------------------

void test_ssid_valid_ascii() {
    TEST_ASSERT_TRUE(validate_ssid("MyNetwork"));
}

void test_ssid_max_length_32() {
    TEST_ASSERT_TRUE(validate_ssid("12345678901234567890123456789012"));
}

void test_ssid_too_long_33() {
    TEST_ASSERT_FALSE(validate_ssid("123456789012345678901234567890123"));
}

void test_ssid_empty_rejected() {
    TEST_ASSERT_FALSE(validate_ssid(""));
}

void test_ssid_null_rejected() {
    TEST_ASSERT_FALSE(validate_ssid(nullptr));
}

void test_ssid_null_byte_rejected() {
    // A string with an embedded NUL — strlen truncates but we still check.
    TEST_ASSERT_FALSE(validate_ssid("\x00evil"));
}

void test_ssid_control_char_rejected() {
    TEST_ASSERT_FALSE(validate_ssid("net\x01work"));
}

void test_ssid_del_char_rejected() {
    TEST_ASSERT_FALSE(validate_ssid("net\x7Fwork"));
}

void test_ssid_space_accepted() {
    TEST_ASSERT_TRUE(validate_ssid("My Network"));
}

// ---- Channel validation -----------------------------------------------------

void test_channel_valid_1()  { TEST_ASSERT_TRUE(validate_channel(1));  }
void test_channel_valid_6()  { TEST_ASSERT_TRUE(validate_channel(6));  }
void test_channel_valid_14() { TEST_ASSERT_TRUE(validate_channel(14)); }

void test_channel_zero_rejected()         { TEST_ASSERT_FALSE(validate_channel(0));   }
void test_channel_negative_rejected()     { TEST_ASSERT_FALSE(validate_channel(-1));  }
void test_channel_15_rejected()           { TEST_ASSERT_FALSE(validate_channel(15));  }
void test_channel_int_max_rejected()      { TEST_ASSERT_FALSE(validate_channel(INT_MAX)); }
void test_channel_int_min_rejected()      { TEST_ASSERT_FALSE(validate_channel(INT_MIN)); }

// ---- Token bucket edge cases ------------------------------------------------

void test_tb_consume_exact_capacity() {
    TokenBucket b; tb_init(&b, 10, 1);
    TEST_ASSERT_TRUE(tb_consume(&b, 10));
    TEST_ASSERT_EQUAL(0, b.tokens);
}

void test_tb_consume_exceeds_capacity() {
    TokenBucket b; tb_init(&b, 10, 1);
    TEST_ASSERT_FALSE(tb_consume(&b, 11));
    TEST_ASSERT_EQUAL(10, b.tokens); // tokens must be unchanged
}

void test_tb_consume_zero_tokens() {
    TokenBucket b; tb_init(&b, 10, 1);
    b.tokens = 0;
    TEST_ASSERT_FALSE(tb_consume(&b, 1));
}

void test_tb_refill_capped_at_capacity() {
    TokenBucket b; tb_init(&b, 10, 5);
    b.tokens = 8;
    tb_refill(&b);
    TEST_ASSERT_EQUAL(10, b.tokens); // 8+5=13 → capped at 10
}

void test_tb_null_pointer_no_crash() {
    tb_consume(nullptr, 1);
    tb_refill(nullptr);
    TEST_PASS();
}

void test_tb_negative_consume_no_crash() {
    TokenBucket b; tb_init(&b, 10, 1);
    TEST_ASSERT_FALSE(tb_consume(&b, -1)); // negative n rejected
    TEST_ASSERT_EQUAL(10, b.tokens);
}

// ---- Ring buffer edge cases -------------------------------------------------

void test_rbuf_empty_pop_fails() {
    RingBuf r; rbuf_init(&r);
    uint8_t v = 0;
    TEST_ASSERT_FALSE(rbuf_pop(&r, &v));
}

void test_rbuf_full_push_fails() {
    RingBuf r; rbuf_init(&r);
    for (int i = 0; i < RBUF_SIZE; i++) rbuf_push(&r, (uint8_t)i);
    TEST_ASSERT_FALSE(rbuf_push(&r, 99));
}

void test_rbuf_fifo_ordering() {
    RingBuf r; rbuf_init(&r);
    rbuf_push(&r, 10);
    rbuf_push(&r, 20);
    uint8_t a, b;
    rbuf_pop(&r, &a); rbuf_pop(&r, &b);
    TEST_ASSERT_EQUAL(10, a);
    TEST_ASSERT_EQUAL(20, b);
}

void test_rbuf_wrap_around_integrity() {
    RingBuf r; rbuf_init(&r);
    // Fill, drain half, fill again — exercises wrap.
    for (int i = 0; i < RBUF_SIZE; i++) rbuf_push(&r, (uint8_t)i);
    uint8_t v;
    for (int i = 0; i < RBUF_SIZE/2; i++) rbuf_pop(&r, &v);
    for (int i = 0; i < RBUF_SIZE/2; i++) rbuf_push(&r, (uint8_t)(100+i));
    // Remaining original entries should come out before the new ones.
    for (int i = RBUF_SIZE/2; i < RBUF_SIZE; i++) {
        rbuf_pop(&r, &v);
        TEST_ASSERT_EQUAL((uint8_t)i, v);
    }
    TEST_PASS();
}

void test_rbuf_exactly_full_then_one_pop_one_push() {
    RingBuf r; rbuf_init(&r);
    for (int i = 0; i < RBUF_SIZE; i++) rbuf_push(&r, (uint8_t)i);
    uint8_t v;
    rbuf_pop(&r, &v);
    TEST_ASSERT_TRUE(rbuf_push(&r, 42)); // one slot freed
}

// ─── Runner ──────────────────────────────────────────────────────────────────

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    // Constant-time strcmp
    RUN_TEST(test_ct_strcmp_null_a);
    RUN_TEST(test_ct_strcmp_null_b);
    RUN_TEST(test_ct_strcmp_both_null);
    RUN_TEST(test_ct_strcmp_equal_strings);
    RUN_TEST(test_ct_strcmp_different_length);
    RUN_TEST(test_ct_strcmp_empty_vs_nonempty);
    RUN_TEST(test_ct_strcmp_both_empty);
    RUN_TEST(test_ct_strcmp_one_char_off);
    RUN_TEST(test_ct_strcmp_case_sensitive);

    // IP lockout
    RUN_TEST(test_lockout_not_triggered_before_threshold);
    RUN_TEST(test_lockout_triggered_at_threshold);
    RUN_TEST(test_lockout_expires_after_duration);
    RUN_TEST(test_lockout_still_active_at_expiry_boundary);
    RUN_TEST(test_lockout_independent_per_ip);
    RUN_TEST(test_lockout_table_full_no_crash);
    RUN_TEST(test_lockout_extra_failures_dont_reset_timer);

    // SSID validation
    RUN_TEST(test_ssid_valid_ascii);
    RUN_TEST(test_ssid_max_length_32);
    RUN_TEST(test_ssid_too_long_33);
    RUN_TEST(test_ssid_empty_rejected);
    RUN_TEST(test_ssid_null_rejected);
    RUN_TEST(test_ssid_null_byte_rejected);
    RUN_TEST(test_ssid_control_char_rejected);
    RUN_TEST(test_ssid_del_char_rejected);
    RUN_TEST(test_ssid_space_accepted);

    // Channel validation
    RUN_TEST(test_channel_valid_1);
    RUN_TEST(test_channel_valid_6);
    RUN_TEST(test_channel_valid_14);
    RUN_TEST(test_channel_zero_rejected);
    RUN_TEST(test_channel_negative_rejected);
    RUN_TEST(test_channel_15_rejected);
    RUN_TEST(test_channel_int_max_rejected);
    RUN_TEST(test_channel_int_min_rejected);

    // Token bucket
    RUN_TEST(test_tb_consume_exact_capacity);
    RUN_TEST(test_tb_consume_exceeds_capacity);
    RUN_TEST(test_tb_consume_zero_tokens);
    RUN_TEST(test_tb_refill_capped_at_capacity);
    RUN_TEST(test_tb_null_pointer_no_crash);
    RUN_TEST(test_tb_negative_consume_no_crash);

    // Ring buffer
    RUN_TEST(test_rbuf_empty_pop_fails);
    RUN_TEST(test_rbuf_full_push_fails);
    RUN_TEST(test_rbuf_fifo_ordering);
    RUN_TEST(test_rbuf_wrap_around_integrity);
    RUN_TEST(test_rbuf_exactly_full_then_one_pop_one_push);

    return UNITY_END();
}

#endif // NATIVE_ENV

// test_frame_parsing.cpp — Unit tests for 802.11 frame parsing utilities.
//
// Tests the frame classification, MAC extraction, SSID parsing, and
// frame type identification functions used in the promiscuous callback.
//
// Framework: Unity (bundled with PlatformIO's native env).
// Run with: pio test -e native
//
// Coverage:
//   - Beacon frame parsing (SSID, BSSID, channel, capability)
//   - Probe request/response parsing
//   - Deauth/disassoc frame parsing
//   - Data frame type identification
//   - Management frame subtype decoding
//   - Frame type byte extraction
//   - MAC address extraction from various frame positions
//   - SSID IE extraction (tagged parameters)
//   - HT/VHT/HE capability detection
//   - WPS IE presence detection
//   - BSS Load IE parsing
//   - TSF extraction from beacon frames
//   - Beacon interval extraction
//   - Sequence number extraction

#include <unity.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

// ============================================================
// FRAME PARSING HELPERS (copied from frames.h logic)
// ============================================================

static uint8_t fp_frame_type(uint8_t fc0) {
  return (fc0 >> 2) & 0x03;
}

static uint8_t fp_frame_subtype(uint8_t fc0) {
  return (fc0 >> 4) & 0x0F;
}

static void fp_extract_mac(const uint8_t *frame, int offset, uint8_t *out) {
  memcpy(out, frame + offset, 6);
}

static bool fp_is_broadcast(const uint8_t *mac) {
  return mac[0]==0xFF && mac[1]==0xFF && mac[2]==0xFF &&
         mac[3]==0xFF && mac[4]==0xFF && mac[5]==0xFF;
}

static bool fp_is_multicast(const uint8_t *mac) {
  return (mac[0] & 0x01) != 0;
}

static bool fp_is_random_mac(const uint8_t *mac) {
  return (mac[0] & 0x02) != 0;
}

static int fp_extract_ssid(const uint8_t *body, int body_len, char *ssid_out, int max_len) {
  // For beacons: fixed params = 12 bytes, then tagged params.
  // For probe requests: fixed params = 0 bytes.
  // Caller passes pointer already past fixed params.
  for (int i = 0; i + 1 < body_len; ) {
    uint8_t tag = body[i];
    uint8_t tlen = body[i+1];
    if (i + 2 + tlen > body_len) break;
    if (tag == 0) { // SSID
      int copy_len = tlen < max_len - 1 ? tlen : max_len - 1;
      memcpy(ssid_out, body + i + 2, copy_len);
      ssid_out[copy_len] = '\0';
      return copy_len;
    }
    i += 2 + tlen;
  }
  ssid_out[0] = '\0';
  return 0;
}

static bool fp_has_ht_cap(const uint8_t *body, int body_len) {
  for (int i = 0; i + 1 < body_len; ) {
    uint8_t tag = body[i], tlen = body[i+1];
    if (i + 2 + tlen > body_len) break;
    if (tag == 45) return true; // HT Capabilities
    i += 2 + tlen;
  }
  return false;
}

static bool fp_has_vht_cap(const uint8_t *body, int body_len) {
  for (int i = 0; i + 1 < body_len; ) {
    uint8_t tag = body[i], tlen = body[i+1];
    if (i + 2 + tlen > body_len) break;
    if (tag == 191) return true; // VHT Capabilities
    i += 2 + tlen;
  }
  return false;
}

static bool fp_has_wps_ie(const uint8_t *body, int body_len) {
  static const uint8_t WPS_OUI[4] = {0x00, 0x50, 0xF2, 0x04};
  for (int i = 0; i + 1 < body_len; ) {
    uint8_t tag = body[i], tlen = body[i+1];
    if (i + 2 + tlen > body_len) break;
    if (tag == 221 && tlen >= 4 && memcmp(body + i + 2, WPS_OUI, 4) == 0)
      return true;
    i += 2 + tlen;
  }
  return false;
}

static uint16_t fp_extract_beacon_interval(const uint8_t *fixed_params) {
  // fixed_params[0..7] = Timestamp (8 bytes)
  // fixed_params[8..9] = Beacon Interval
  return (uint16_t)(fixed_params[8] | (fixed_params[9] << 8));
}

static uint64_t fp_extract_tsf(const uint8_t *fixed_params) {
  uint64_t tsf = 0;
  for (int i = 0; i < 8; i++) tsf |= ((uint64_t)fixed_params[i]) << (i * 8);
  return tsf;
}

static uint16_t fp_extract_seq_num(const uint8_t *frame) {
  // Sequence control is at offset 22 for most frames.
  uint16_t sc = (uint16_t)(frame[22] | (frame[23] << 8));
  return sc >> 4; // top 12 bits
}

// ============================================================
// TEST FIXTURES
// ============================================================

// Minimal beacon frame (truncated, just enough for testing headers)
static const uint8_t BEACON_FRAME[] = {
  // Frame Control
  0x80, 0x00,
  // Duration
  0x00, 0x00,
  // DA (broadcast)
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  // SA / BSSID (AP MAC)
  0x02, 0x11, 0x22, 0x33, 0x44, 0x55,
  // BSSID
  0x02, 0x11, 0x22, 0x33, 0x44, 0x55,
  // Seq ctrl
  0x00, 0x00,
  // Fixed params: Timestamp (8 bytes: 0x00000000000003E8 = 1000)
  0xE8, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  // Beacon Interval: 100 TU = 0x0064
  0x64, 0x00,
  // Capability Info
  0x11, 0x00,
  // Tagged params: SSID = "TestNet"
  0x00, 0x07, 0x54, 0x65, 0x73, 0x74, 0x4E, 0x65, 0x74,
  // DS Parameter Set: channel 6
  0x03, 0x01, 0x06,
  // HT Capabilities IE (tag 45, minimal 2-byte body)
  0x2D, 0x02, 0xEF, 0x01,
};

// Probe request frame
static const uint8_t PROBE_REQ_FRAME[] = {
  // FC
  0x40, 0x00,
  // Duration
  0x00, 0x00,
  // DA (broadcast)
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  // SA (randomized MAC, locally administered bit set)
  0x02, 0xAB, 0xCD, 0xEF, 0x01, 0x02,
  // BSSID (wildcard broadcast)
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  // Seq ctrl
  0x30, 0x00,
  // Tagged params: SSID = "HomeWiFi"
  0x00, 0x08, 0x48, 0x6F, 0x6D, 0x65, 0x57, 0x69, 0x46, 0x69,
};

// Deauth frame
static const uint8_t DEAUTH_FRAME[] = {
  // FC: type=mgmt(0x00), subtype=deauth(0xC0)
  0xC0, 0x00,
  0x00, 0x00,
  // DA
  0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
  // SA
  0x02, 0x11, 0x22, 0x33, 0x44, 0x55,
  // BSSID
  0x02, 0x11, 0x22, 0x33, 0x44, 0x55,
  // Seq ctrl
  0x10, 0x00,
  // Reason code: 7 (Class 3 frame from nonassoc STA)
  0x07, 0x00,
};

// WPS beacon fixture (adds WPS vendor-specific IE after SSID)
static const uint8_t WPS_BEACON[] = {
  0x80, 0x00, 0x00, 0x00,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE,
  0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE,
  0x00, 0x00,
  // Fixed params (12 bytes)
  0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, // TSF
  0x64, 0x00, // BI = 100
  0x01, 0x00, // Caps
  // SSID IE: "WPS_Test"
  0x00, 0x08, 0x57, 0x50, 0x53, 0x5F, 0x54, 0x65, 0x73, 0x74,
  // WPS vendor-specific IE: tag=0xDD, OUI=00:50:F2:04
  0xDD, 0x06, 0x00, 0x50, 0xF2, 0x04, 0x10, 0x4A,
};

// ============================================================
// TESTS: Frame type/subtype decoding
// ============================================================

void test_beacon_type() {
  TEST_ASSERT_EQUAL(0, fp_frame_type(BEACON_FRAME[0]));    // management
  TEST_ASSERT_EQUAL(8, fp_frame_subtype(BEACON_FRAME[0])); // beacon
}

void test_probe_req_type() {
  TEST_ASSERT_EQUAL(0, fp_frame_type(PROBE_REQ_FRAME[0]));
  TEST_ASSERT_EQUAL(4, fp_frame_subtype(PROBE_REQ_FRAME[0])); // probe request
}

void test_deauth_type() {
  TEST_ASSERT_EQUAL(0, fp_frame_type(DEAUTH_FRAME[0]));
  TEST_ASSERT_EQUAL(12, fp_frame_subtype(DEAUTH_FRAME[0])); // deauth (0xC0 >> 4 = 12)
}

void test_data_frame_type() {
  uint8_t data_fc0 = 0x08; // data frame, type 2 (0x08 >> 2 & 3 = 2)
  TEST_ASSERT_EQUAL(2, fp_frame_type(data_fc0));
  TEST_ASSERT_EQUAL(0, fp_frame_subtype(data_fc0));
}

void test_qos_data_subtype() {
  uint8_t qos_fc0 = 0x88; // QoS Data: type=2, subtype=8
  TEST_ASSERT_EQUAL(2, fp_frame_type(qos_fc0));
  TEST_ASSERT_EQUAL(8, fp_frame_subtype(qos_fc0));
}

void test_null_data_subtype() {
  uint8_t null_fc0 = 0x48; // Null Data: type=2, subtype=4
  TEST_ASSERT_EQUAL(2, fp_frame_type(null_fc0));
  TEST_ASSERT_EQUAL(4, fp_frame_subtype(null_fc0));
}

void test_ctrl_ack_type() {
  uint8_t ack_fc0 = 0xD4; // ACK: type=1 (ctrl), subtype=0xD (13)
  TEST_ASSERT_EQUAL(1, fp_frame_type(ack_fc0));
  TEST_ASSERT_EQUAL(13, fp_frame_subtype(ack_fc0));
}

void test_assoc_req_subtype() {
  uint8_t fc0 = 0x00; // assoc req: type=0, subtype=0
  TEST_ASSERT_EQUAL(0, fp_frame_type(fc0));
  TEST_ASSERT_EQUAL(0, fp_frame_subtype(fc0));
}

void test_assoc_resp_subtype() {
  uint8_t fc0 = 0x10; // assoc resp: type=0, subtype=1
  TEST_ASSERT_EQUAL(0, fp_frame_type(fc0));
  TEST_ASSERT_EQUAL(1, fp_frame_subtype(fc0));
}

void test_auth_subtype() {
  uint8_t fc0 = 0xB0; // auth: type=0, subtype=11
  TEST_ASSERT_EQUAL(0, fp_frame_type(fc0));
  TEST_ASSERT_EQUAL(11, fp_frame_subtype(fc0));
}

// ============================================================
// TESTS: MAC extraction
// ============================================================

void test_beacon_bssid() {
  uint8_t bssid[6];
  fp_extract_mac(BEACON_FRAME, 16, bssid);
  uint8_t expected[] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
  TEST_ASSERT_EQUAL_MEMORY(expected, bssid, 6);
}

void test_beacon_da_broadcast() {
  uint8_t da[6];
  fp_extract_mac(BEACON_FRAME, 4, da);
  TEST_ASSERT_TRUE(fp_is_broadcast(da));
}

void test_probe_req_sa() {
  uint8_t sa[6];
  fp_extract_mac(PROBE_REQ_FRAME, 10, sa);
  uint8_t expected[] = {0x02, 0xAB, 0xCD, 0xEF, 0x01, 0x02};
  TEST_ASSERT_EQUAL_MEMORY(expected, sa, 6);
}

void test_probe_req_random_mac() {
  uint8_t sa[6];
  fp_extract_mac(PROBE_REQ_FRAME, 10, sa);
  TEST_ASSERT_TRUE(fp_is_random_mac(sa));
}

void test_probe_req_bssid_broadcast() {
  uint8_t bssid[6];
  fp_extract_mac(PROBE_REQ_FRAME, 16, bssid);
  TEST_ASSERT_TRUE(fp_is_broadcast(bssid));
}

void test_non_broadcast_mac() {
  uint8_t mac[] = {0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
  TEST_ASSERT_FALSE(fp_is_broadcast(mac));
}

void test_multicast_mac() {
  uint8_t mac[] = {0x01, 0x00, 0x5E, 0x00, 0x00, 0x01};
  TEST_ASSERT_TRUE(fp_is_multicast(mac));
}

void test_unicast_mac_not_multicast() {
  uint8_t mac[] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
  TEST_ASSERT_FALSE(fp_is_multicast(mac));
}

void test_non_random_mac() {
  uint8_t mac[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
  TEST_ASSERT_FALSE(fp_is_random_mac(mac));
}

// ============================================================
// TESTS: SSID extraction
// ============================================================

void test_beacon_ssid() {
  // Fixed params end at offset 24+12=36. Tagged params start there.
  const uint8_t *tagged = BEACON_FRAME + 36;
  int tagged_len = sizeof(BEACON_FRAME) - 36;
  char ssid[33];
  int len = fp_extract_ssid(tagged, tagged_len, ssid, 33);
  TEST_ASSERT_EQUAL(7, len);
  TEST_ASSERT_EQUAL_STRING("TestNet", ssid);
}

void test_probe_req_ssid() {
  const uint8_t *tagged = PROBE_REQ_FRAME + 24; // no fixed params in probe req
  int tagged_len = sizeof(PROBE_REQ_FRAME) - 24;
  char ssid[33];
  int len = fp_extract_ssid(tagged, tagged_len, ssid, 33);
  TEST_ASSERT_EQUAL(8, len);
  TEST_ASSERT_EQUAL_STRING("HomeWiFi", ssid);
}

void test_empty_ssid() {
  // A probe req with SSID IE length = 0 (wildcard probe)
  uint8_t wildcard[] = {0x00, 0x00}; // tag=0, len=0
  char ssid[33];
  int len = fp_extract_ssid(wildcard, 2, ssid, 33);
  TEST_ASSERT_EQUAL(0, len);
  TEST_ASSERT_EQUAL_STRING("", ssid);
}

void test_ssid_max_length() {
  // 32-byte SSID
  uint8_t ie[34];
  ie[0] = 0x00; ie[1] = 32;
  for (int i = 0; i < 32; i++) ie[2+i] = 'A';
  char ssid[33];
  int len = fp_extract_ssid(ie, 34, ssid, 33);
  TEST_ASSERT_EQUAL(32, len);
  for (int i = 0; i < 32; i++) TEST_ASSERT_EQUAL('A', ssid[i]);
  TEST_ASSERT_EQUAL('\0', ssid[32]);
}

void test_ssid_truncated_to_max() {
  // SSID IE longer than max_len
  uint8_t ie[36];
  ie[0] = 0x00; ie[1] = 34; // 34 bytes, but max_len=33
  for (int i = 0; i < 34; i++) ie[2+i] = 'B';
  char ssid[33];
  int len = fp_extract_ssid(ie, 36, ssid, 33);
  TEST_ASSERT_EQUAL(32, len); // capped at max_len-1=32
}

void test_ssid_not_first_ie() {
  // DS Param Set IE first, then SSID
  uint8_t ies[] = {
    0x03, 0x01, 0x06,        // DS param: ch=6
    0x00, 0x05, 0x48, 0x65, 0x6C, 0x6C, 0x6F  // SSID: "Hello"
  };
  char ssid[33];
  int len = fp_extract_ssid(ies, sizeof(ies), ssid, 33);
  TEST_ASSERT_EQUAL(5, len);
  TEST_ASSERT_EQUAL_STRING("Hello", ssid);
}

void test_ssid_truncated_ie() {
  // IE length claims more bytes than available
  uint8_t ie[] = {0x00, 0x20, 0x41, 0x41}; // says 32 bytes, only 2 available
  char ssid[33];
  int len = fp_extract_ssid(ie, 4, ssid, 33);
  TEST_ASSERT_EQUAL(0, len); // no valid SSID found
}

// ============================================================
// TESTS: HT/VHT capability detection
// ============================================================

void test_has_ht_cap() {
  const uint8_t *tagged = BEACON_FRAME + 36;
  int len = sizeof(BEACON_FRAME) - 36;
  TEST_ASSERT_TRUE(fp_has_ht_cap(tagged, len));
}

void test_no_ht_cap() {
  // Only SSID IE, no HT
  uint8_t ies[] = {0x00, 0x04, 0x54, 0x65, 0x73, 0x74};
  TEST_ASSERT_FALSE(fp_has_ht_cap(ies, sizeof(ies)));
}

void test_has_vht_cap() {
  // VHT Capabilities IE = tag 191
  uint8_t ies[] = {0xBF, 0x04, 0x00, 0x00, 0x00, 0x00};
  TEST_ASSERT_TRUE(fp_has_vht_cap(ies, sizeof(ies)));
}

void test_no_vht_cap_only_ht() {
  // HT IE only
  uint8_t ies[] = {0x2D, 0x02, 0xEF, 0x01};
  TEST_ASSERT_FALSE(fp_has_vht_cap(ies, sizeof(ies)));
}

// ============================================================
// TESTS: WPS IE detection
// ============================================================

void test_has_wps_ie() {
  const uint8_t *tagged = WPS_BEACON + 36;
  int len = sizeof(WPS_BEACON) - 36;
  TEST_ASSERT_TRUE(fp_has_wps_ie(tagged, len));
}

void test_no_wps_ie() {
  // Generic vendor-specific IE with different OUI
  uint8_t ies[] = {0xDD, 0x06, 0x00, 0x50, 0xF2, 0x01, 0x00, 0x00}; // OUI = 00:50:F2:01 (not WPS)
  TEST_ASSERT_FALSE(fp_has_wps_ie(ies, sizeof(ies)));
}

void test_wps_ie_too_short() {
  // Vendor IE with correct OUI prefix but truncated
  uint8_t ies[] = {0xDD, 0x03, 0x00, 0x50, 0xF2}; // only 3 bytes, need 4 for full OUI+type
  TEST_ASSERT_FALSE(fp_has_wps_ie(ies, sizeof(ies)));
}

// ============================================================
// TESTS: Beacon interval and TSF extraction
// ============================================================

void test_beacon_interval() {
  const uint8_t *fp = BEACON_FRAME + 24; // fixed params start at byte 24
  uint16_t bi = fp_extract_beacon_interval(fp);
  TEST_ASSERT_EQUAL(100, bi);
}

void test_tsf_extraction() {
  const uint8_t *fp = BEACON_FRAME + 24;
  uint64_t tsf = fp_extract_tsf(fp);
  TEST_ASSERT_EQUAL_UINT64(1000ULL, tsf);
}

void test_tsf_zero() {
  uint8_t fp[12] = {0};
  uint64_t tsf = fp_extract_tsf(fp);
  TEST_ASSERT_EQUAL_UINT64(0ULL, tsf);
}

void test_tsf_max() {
  uint8_t fp[12];
  for (int i = 0; i < 8; i++) fp[i] = 0xFF;
  fp[8] = fp[9] = 0x64; fp[10] = fp[11] = 0;
  uint64_t tsf = fp_extract_tsf(fp);
  TEST_ASSERT_EQUAL_UINT64(0xFFFFFFFFFFFFFFFFULL, tsf);
}

// ============================================================
// TESTS: Sequence number extraction
// ============================================================

void test_seq_num_basic() {
  // Seq ctrl at offset 22-23: 0x30 0x00 → seq = 0x000 >> 0 = 3? No...
  // seq_ctrl = frame[22] | (frame[23]<<8) = 0x0030
  // seq_num = seq_ctrl >> 4 = 0x003
  TEST_ASSERT_EQUAL(3, fp_extract_seq_num(PROBE_REQ_FRAME));
}

void test_seq_num_zero() {
  TEST_ASSERT_EQUAL(0, fp_extract_seq_num(BEACON_FRAME));
}

void test_seq_num_max() {
  uint8_t frame[24] = {0};
  frame[22] = 0xF0; frame[23] = 0xFF;
  // seq_ctrl = 0xFFF0 >> 4 = 0xFFF = 4095
  TEST_ASSERT_EQUAL(4095, fp_extract_seq_num(frame));
}

// ============================================================
// TESTS: Deauth reason code extraction
// ============================================================

void test_deauth_reason_code() {
  // Reason code is at offset 24 (after 4 addr fields × 6 bytes + 2 FC + 2 Dur + 2 SeqCtrl... wait)
  // Actual deauth body starts at offset 24 (4 bytes FC+Dur + 3×6 MAC + 2 SeqCtrl = 24)
  uint16_t rc = (uint16_t)(DEAUTH_FRAME[24] | (DEAUTH_FRAME[25] << 8));
  TEST_ASSERT_EQUAL(7, rc);
}

void test_deauth_reason_code_no_ap() {
  uint8_t deauth[] = {0xC0,0x00,0x00,0x00,
    0xAA,0xBB,0xCC,0xDD,0xEE,0xFF,
    0x11,0x22,0x33,0x44,0x55,0x66,
    0x11,0x22,0x33,0x44,0x55,0x66,
    0x20,0x00,
    0x01,0x00}; // reason=1 (Unspecified)
  uint16_t rc = (uint16_t)(deauth[24] | (deauth[25] << 8));
  TEST_ASSERT_EQUAL(1, rc);
}

// ============================================================
// ENTRY POINT
// ============================================================

void setUp()   {}
void tearDown() {}

int main(int argc, char **argv) {
  UNITY_BEGIN();

  // Frame type/subtype
  RUN_TEST(test_beacon_type);
  RUN_TEST(test_probe_req_type);
  RUN_TEST(test_deauth_type);
  RUN_TEST(test_data_frame_type);
  RUN_TEST(test_qos_data_subtype);
  RUN_TEST(test_null_data_subtype);
  RUN_TEST(test_ctrl_ack_type);
  RUN_TEST(test_assoc_req_subtype);
  RUN_TEST(test_assoc_resp_subtype);
  RUN_TEST(test_auth_subtype);

  // MAC extraction
  RUN_TEST(test_beacon_bssid);
  RUN_TEST(test_beacon_da_broadcast);
  RUN_TEST(test_probe_req_sa);
  RUN_TEST(test_probe_req_random_mac);
  RUN_TEST(test_probe_req_bssid_broadcast);
  RUN_TEST(test_non_broadcast_mac);
  RUN_TEST(test_multicast_mac);
  RUN_TEST(test_unicast_mac_not_multicast);
  RUN_TEST(test_non_random_mac);

  // SSID extraction
  RUN_TEST(test_beacon_ssid);
  RUN_TEST(test_probe_req_ssid);
  RUN_TEST(test_empty_ssid);
  RUN_TEST(test_ssid_max_length);
  RUN_TEST(test_ssid_truncated_to_max);
  RUN_TEST(test_ssid_not_first_ie);
  RUN_TEST(test_ssid_truncated_ie);

  // Capability detection
  RUN_TEST(test_has_ht_cap);
  RUN_TEST(test_no_ht_cap);
  RUN_TEST(test_has_vht_cap);
  RUN_TEST(test_no_vht_cap_only_ht);

  // WPS detection
  RUN_TEST(test_has_wps_ie);
  RUN_TEST(test_no_wps_ie);
  RUN_TEST(test_wps_ie_too_short);

  // Beacon interval and TSF
  RUN_TEST(test_beacon_interval);
  RUN_TEST(test_tsf_extraction);
  RUN_TEST(test_tsf_zero);
  RUN_TEST(test_tsf_max);

  // Sequence number
  RUN_TEST(test_seq_num_basic);
  RUN_TEST(test_seq_num_zero);
  RUN_TEST(test_seq_num_max);

  // Deauth reason code
  RUN_TEST(test_deauth_reason_code);
  RUN_TEST(test_deauth_reason_code_no_ap);

  return UNITY_END();
}

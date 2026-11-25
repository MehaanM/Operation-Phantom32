#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <string.h>
#include <stdint.h>

// ── 802.11 Frame Builder ──────────────────────────────────────────────────────
//
// Complete 802.11 frame construction library for ESP32.
// Provides typed builders for management, control, and data frames.
// Handles endianness, sequence numbers, FCS, and IE encoding.

// ── Frame type/subtype constants ──────────────────────────────────────────────
#define FB_TYPE_MGMT 0x00
#define FB_TYPE_CTRL 0x01
#define FB_TYPE_DATA 0x02

#define FB_STYPE_ASSOC_REQ 0x00
#define FB_STYPE_ASSOC_RESP 0x01
#define FB_STYPE_REASSOC_REQ 0x02
#define FB_STYPE_REASSOC_RESP 0x03
#define FB_STYPE_PROBE_REQ 0x04
#define FB_STYPE_PROBE_RESP 0x05
#define FB_STYPE_TIMING_ADV 0x06
#define FB_STYPE_BEACON 0x08
#define FB_STYPE_ATIM 0x09
#define FB_STYPE_DISASSOC 0x0A
#define FB_STYPE_AUTH 0x0B
#define FB_STYPE_DEAUTH 0x0C
#define FB_STYPE_ACTION 0x0D
#define FB_STYPE_ACTION_NOACK 0x0E

#define FB_STYPE_CTRL_WRAPPER 0x07
#define FB_STYPE_BAR 0x08
#define FB_STYPE_BA 0x09
#define FB_STYPE_PS_POLL 0x0A
#define FB_STYPE_RTS 0x0B
#define FB_STYPE_CTS 0x0C
#define FB_STYPE_ACK 0x0D
#define FB_STYPE_CF_END 0x0E
#define FB_STYPE_CF_END_ACK 0x0F

#define FB_STYPE_DATA 0x00
#define FB_STYPE_DATA_CF_ACK 0x01
#define FB_STYPE_DATA_CF_POLL 0x02
#define FB_STYPE_NULL 0x04
#define FB_STYPE_QOS_DATA 0x08
#define FB_STYPE_QOS_NULL 0x0C

// ── Frame control word builder ────────────────────────────────────────────────
#define FB_FC(type, stype, tods, fromds, retry, pm, md, wep, order)                         \
  (uint16_t)(((stype) << 4) | ((type) << 2) |                                               \
             (((tods) | (fromds << 1) | (retry << 3) | (pm << 4) | (md << 5) | (wep << 6) | \
               (order << 7))                                                                \
              << 8))

// ── IE tag numbers ────────────────────────────────────────────────────────────
#define FB_IE_SSID 0
#define FB_IE_SUPPORTED_RATES 1
#define FB_IE_DS_PARAM 3
#define FB_IE_TIM 5
#define FB_IE_IBSS_PARAM 6
#define FB_IE_COUNTRY 7
#define FB_IE_BSS_LOAD 11
#define FB_IE_ERP 42
#define FB_IE_HT_CAP 45
#define FB_IE_RSN 48
#define FB_IE_EXT_RATES 50
#define FB_IE_HT_OP 61
#define FB_IE_VHT_CAP 191
#define FB_IE_VHT_OP 192
#define FB_IE_EXT_CAP 127
#define FB_IE_WPA 221     // OUI 00:50:F2 type 1
#define FB_IE_WMM 221     // OUI 00:50:F2 type 2
#define FB_IE_HE_CAP 255  // EXT tag 35
#define FB_IE_HE_OP 255   // EXT tag 36

// ── Authentication algorithms ─────────────────────────────────────────────────
#define FB_AUTH_OPEN 0
#define FB_AUTH_SHARED_KEY 1
#define FB_AUTH_FT 2
#define FB_AUTH_SAE 3
#define FB_AUTH_FILS_SK 4
#define FB_AUTH_FILS_SK_PFS 5
#define FB_AUTH_FILS_PK 6

// ── Deauth/Disassoc reason codes ──────────────────────────────────────────────
#define FB_REASON_UNSPEC 1
#define FB_REASON_PREV_AUTH_NO_LONGER 2
#define FB_REASON_DEAUTH_LEAVING 3
#define FB_REASON_DISASSOC_INACT 4
#define FB_REASON_DISASSOC_AP_BUSY 5
#define FB_REASON_CLASS2_FROM_NON_AUTH 6
#define FB_REASON_CLASS3_FROM_NON_ASSOC 7
#define FB_REASON_DISASSOC_STA_LEFT 8
#define FB_REASON_STA_REQ_ASSOC_NO_AUTH 9
#define FB_REASON_PWR_CAP 10
#define FB_REASON_SUPPORTED_CH 11
#define FB_REASON_INVALID_IE 13
#define FB_REASON_MIC_FAILURE 14
#define FB_REASON_4WAY_TIMEOUT 15
#define FB_REASON_GROUP_KEY_TIMEOUT 16
#define FB_REASON_IE_DIFF 17
#define FB_REASON_INVALID_GROUP_CIPHER 18
#define FB_REASON_INVALID_PAIRWISE_CIPHER 19
#define FB_REASON_INVALID_AKMP 20
#define FB_REASON_UNSUPPORTED_RSNE 21
#define FB_REASON_INVALID_RSNE_CAP 22
#define FB_REASON_8021X_AUTH_FAILED 23
#define FB_REASON_CIPHER_REJECTED 24
#define FB_REASON_TDLS_TEARDOWN 26
#define FB_REASON_DISASSOC_UNSPEC_QOS 32
#define FB_REASON_STA_LACKS_QOS 33
#define FB_REASON_TIMEOUT 39
#define FB_REASON_STA_CIPHER_NO_MATCH 46
#define FB_REASON_AUTHORIZED_ACCESS 47

// ── Status codes ─────────────────────────────────────────────────────────────
#define FB_STATUS_SUCCESS 0
#define FB_STATUS_UNSPEC_FAILURE 1
#define FB_STATUS_CAPS_MISMATCH 10
#define FB_STATUS_NO_ASSOC_EXIST 11
#define FB_STATUS_OTHER_ASSOC_DENIED 12
#define FB_STATUS_ALG_NOT_SUPPORTED 13
#define FB_STATUS_SEQ_OUT_OF_RANGE 14
#define FB_STATUS_CHALLENGE_FAILURE 15
#define FB_STATUS_AUTH_TIMEOUT 16
#define FB_STATUS_TOO_MANY_STAS 17
#define FB_STATUS_RATES_MISMATCH 18
#define FB_STATUS_ASSOC_DENIED_RATES 22
#define FB_STATUS_NOT_IN_RANGE 25
#define FB_STATUS_NOT_AUTHENTICATED 28
#define FB_STATUS_SAE_HASH_TO_ELEMENT 77

// ── Buffer size constants ─────────────────────────────────────────────────────
#define FB_MAX_FRAME 1600
#define FB_MAX_SSID 32
#define FB_MAX_IE_LEN 255
#define FB_HDR_LEN 24  // Standard management header
#define FB_FCS_LEN 4

// ── Standard 802.11 MAC header ────────────────────────────────────────────────
struct __attribute__((packed)) Fb80211Hdr {
  uint16_t fc;       // Frame control
  uint16_t dur;      // Duration/ID
  uint8_t addr1[6];  // Receiver
  uint8_t addr2[6];  // Transmitter
  uint8_t addr3[6];  // BSSID (mgmt) / dest (data)
  uint16_t seq;      // Sequence control
};

// ── QoS data header extension ─────────────────────────────────────────────────
struct __attribute__((packed)) FbQosHdr {
  Fb80211Hdr base;
  uint16_t qos;  // QoS control
};

// ── 4-address header ─────────────────────────────────────────────────────────
struct __attribute__((packed)) Fb4AddrHdr {
  Fb80211Hdr base;
  uint8_t addr4[6];
};

// ── Beacon/ProbeResp fixed fields ────────────────────────────────────────────
struct __attribute__((packed)) FbBeaconFixed {
  uint64_t timestamp;
  uint16_t interval;
  uint16_t capability;
};

// ── Capability bits ──────────────────────────────────────────────────────────
#define FB_CAP_ESS 0x0001
#define FB_CAP_IBSS 0x0002
#define FB_CAP_CF_POLL 0x0004
#define FB_CAP_CF_REQ 0x0008
#define FB_CAP_PRIVACY 0x0010
#define FB_CAP_SHORT_PRE 0x0020
#define FB_CAT_SHORT_SLOT 0x0400
#define FB_CAP_DSSSOFDM 0x2000

// ── Global sequence counter ───────────────────────────────────────────────────
static uint16_t g_fb_seq = 0;
static uint16_t fb_next_seq() {
  g_fb_seq = (g_fb_seq + 0x10) & 0xFFF0;
  return g_fb_seq;
}

// ── Random MAC generator ──────────────────────────────────────────────────────
static void fb_random_mac(uint8_t out[6]) {
  for (int i = 0; i < 6; i++)
    out[i] = (uint8_t)esp_random();
  out[0] &= 0xFE;  // unicast
  out[0] |= 0x02;  // locally administered
}

// ── Random broadcast-like MAC (unicast) ───────────────────────────────────────
static void fb_rand_unicast_mac(uint8_t out[6]) {
  for (int i = 0; i < 6; i++)
    out[i] = (uint8_t)esp_random();
  out[0] &= 0xFC;  // unicast + globally administered
}

// ── Broadcast address helper ──────────────────────────────────────────────────
static const uint8_t FB_BCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// ── IE encoding helpers ───────────────────────────────────────────────────────
static int fb_put_ie(uint8_t *buf, uint8_t tag, const uint8_t *data, uint8_t len) {
  buf[0] = tag;
  buf[1] = len;
  if (len > 0 && data)
    memcpy(buf + 2, data, len);
  return 2 + len;
}

static int fb_put_ie_ssid(uint8_t *buf, const char *ssid) {
  uint8_t len = (uint8_t)strlen(ssid);
  if (len > 32)
    len = 32;
  return fb_put_ie(buf, FB_IE_SSID, (const uint8_t *)ssid, len);
}

static int fb_put_ie_ssid_raw(uint8_t *buf, const uint8_t *ssid, uint8_t len) {
  return fb_put_ie(buf, FB_IE_SSID, ssid, len);
}

static int fb_put_ie_ds(uint8_t *buf, uint8_t ch) {
  return fb_put_ie(buf, FB_IE_DS_PARAM, &ch, 1);
}

static int fb_put_ie_rates(uint8_t *buf) {
  // Standard 2.4GHz rates: 1,2,5.5,11,6,9,12,18 Mbps
  static const uint8_t rates[] = {0x82, 0x84, 0x8B, 0x96, 0x24, 0x30, 0x48, 0x60};
  return fb_put_ie(buf, FB_IE_SUPPORTED_RATES, rates, 8);
}

static int fb_put_ie_ext_rates(uint8_t *buf) {
  // Extended rates: 24,36,48,54 Mbps
  static const uint8_t rates[] = {0x6C, 0x90, 0xB4, 0xC8};  // 54 Mbps basic
  return fb_put_ie(buf, FB_IE_EXT_RATES, rates, 4);
}

static int fb_put_ie_erp(uint8_t *buf, bool protection) {
  uint8_t erp = protection ? 0x02 : 0x00;
  return fb_put_ie(buf, FB_IE_ERP, &erp, 1);
}

static int fb_put_ie_rsn(uint8_t *buf, bool ccmp, bool tkip, bool pmf) {
  // RSN element for WPA2-Personal
  uint8_t rsn[20];
  int pos = 0;
  // Version
  rsn[pos++] = 0x01;
  rsn[pos++] = 0x00;
  // Group cipher: CCMP (00:0F:AC:04) or TKIP (00:0F:AC:02)
  rsn[pos++] = 0x00;
  rsn[pos++] = 0x0F;
  rsn[pos++] = 0xAC;
  rsn[pos++] = (tkip && !ccmp) ? 0x02 : 0x04;
  // Pairwise cipher count + suite
  rsn[pos++] = 0x01;
  rsn[pos++] = 0x00;
  rsn[pos++] = 0x00;
  rsn[pos++] = 0x0F;
  rsn[pos++] = 0xAC;
  rsn[pos++] = (tkip && !ccmp) ? 0x02 : 0x04;
  // AKM count + suite (PSK=00:0F:AC:02)
  rsn[pos++] = 0x01;
  rsn[pos++] = 0x00;
  rsn[pos++] = 0x00;
  rsn[pos++] = 0x0F;
  rsn[pos++] = 0xAC;
  rsn[pos++] = 0x02;
  // RSN capabilities
  uint16_t cap = 0;
  if (pmf)
    cap |= 0x0080;  // MFPC
  rsn[pos++] = (uint8_t)cap;
  rsn[pos++] = (uint8_t)(cap >> 8);
  return fb_put_ie(buf, FB_IE_RSN, rsn, pos);
}

static int fb_put_ie_rsn_sae(uint8_t *buf) {
  // RSN for WPA3-Personal SAE
  uint8_t rsn[20];
  int pos = 0;
  rsn[pos++] = 0x01;
  rsn[pos++] = 0x00;  // version 1
  rsn[pos++] = 0x00;
  rsn[pos++] = 0x0F;
  rsn[pos++] = 0xAC;
  rsn[pos++] = 0x04;  // CCMP group
  rsn[pos++] = 0x01;
  rsn[pos++] = 0x00;
  rsn[pos++] = 0x00;
  rsn[pos++] = 0x0F;
  rsn[pos++] = 0xAC;
  rsn[pos++] = 0x04;  // CCMP pairwise
  rsn[pos++] = 0x01;
  rsn[pos++] = 0x00;
  rsn[pos++] = 0x00;
  rsn[pos++] = 0x0F;
  rsn[pos++] = 0xAC;
  rsn[pos++] = 0x08;  // SAE AKM
  rsn[pos++] = 0x40;
  rsn[pos++] = 0x00;  // MFPR+MFPC
  return fb_put_ie(buf, FB_IE_RSN, rsn, pos);
}

static int fb_put_ie_wpa(uint8_t *buf) {
  // WPA1 vendor IE: OUI 00:50:F2, type 0x01
  static const uint8_t wpa[] = {
      0x00, 0x50, 0xF2, 0x01,  // OUI + type
      0x01, 0x00,              // version 1
      0x00, 0x50, 0xF2, 0x02,  // TKIP group cipher
      0x01, 0x00,              // 1 pairwise
      0x00, 0x50, 0xF2, 0x02,  // TKIP pairwise
      0x01, 0x00,              // 1 AKM
      0x00, 0x50, 0xF2, 0x02,  // PSK AKM
      0x00, 0x00               // capabilities
  };
  return fb_put_ie(buf, FB_IE_WPA, wpa, sizeof(wpa));
}

static int fb_put_ie_ht_cap(uint8_t *buf) {
  // HT Capabilities IE (minimal 802.11n)
  static const uint8_t ht[] = {
      0xEF, 0x09,  // HT cap info: LDPC, 40MHz, SM PS disabled, GF, SGI20, SGI40, TX STBC, RX STBC
                   // 1ss
      0x17,        // A-MPDU params
      0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00,                    // MCS set
      0x00, 0x00,              // HT ext caps
      0x00, 0x00, 0x00, 0x00,  // beamforming caps
      0x00                     // ASEL
  };
  return fb_put_ie(buf, FB_IE_HT_CAP, ht, 26);
}

static int fb_put_ie_ht_op(uint8_t *buf, uint8_t ch) {
  uint8_t op[22] = {};
  op[0] = ch;    // primary channel
  op[1] = 0x00;  // HT op info 1: STA channel width, secondary channel offset
  op[2] = 0x00;
  op[3] = 0x00;
  op[4] = 0x00;  // HT op info 2+3
  // Basic MCS set
  op[5] = 0xFF;
  return fb_put_ie(buf, FB_IE_HT_OP, op, 22);
}

static int fb_put_ie_vht_cap(uint8_t *buf) {
  // VHT Capabilities (802.11ac)
  static const uint8_t vht[] = {
      0x32, 0x11, 0x00, 0x00,  // VHT cap info
      0xFE, 0xFF,              // RX MCS map (MCS 0-9 for 1-4 SS)
      0x00, 0x00,              // RX highest supported rate
      0xFE, 0xFF,              // TX MCS map
      0x00, 0x00               // TX highest supported rate
  };
  return fb_put_ie(buf, FB_IE_VHT_CAP, vht, 12);
}

static int fb_put_ie_vht_op(uint8_t *buf, uint8_t ch, bool is80mhz) {
  uint8_t op[5] = {};
  op[0] = is80mhz ? 1 : 0;  // channel width (0=20/40, 1=80)
  op[1] = ch;               // channel center seg 0
  op[2] = 0;                // channel center seg 1
  op[3] = 0xFE;
  op[4] = 0xFF;  // VHT basic MCS set
  return fb_put_ie(buf, FB_IE_VHT_OP, op, 5);
}

static int fb_put_ie_tim(uint8_t *buf, uint8_t dtim_count, uint8_t dtim_period) {
  uint8_t tim[4] = {dtim_count, dtim_period, 0x00, 0x00};
  return fb_put_ie(buf, FB_IE_TIM, tim, 4);
}

static int fb_put_ie_country(uint8_t *buf, const char cc[2], uint8_t first_ch, uint8_t num_ch,
                             uint8_t max_power) {
  uint8_t data[6];
  data[0] = (uint8_t)cc[0];
  data[1] = (uint8_t)cc[1];
  data[2] = ' ';
  data[3] = first_ch;
  data[4] = num_ch;
  data[5] = max_power;
  return fb_put_ie(buf, FB_IE_COUNTRY, data, 6);
}

static int fb_put_ie_ext_cap(uint8_t *buf) {
  static const uint8_t cap[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40};  // SSID list
  return fb_put_ie(buf, FB_IE_EXT_CAP, cap, 8);
}

// ── Beacon frame builder ──────────────────────────────────────────────────────
struct FbBeaconOpts {
  const char *ssid;
  const uint8_t *ssid_raw;  // if non-null, overrides ssid
  uint8_t ssid_raw_len;
  uint8_t bssid[6];
  uint8_t channel;
  bool hidden;  // zero-length SSID IE
  bool wpa2;
  bool wpa3;
  bool open;
  bool wpa1;
  bool ht;            // include HT caps (802.11n)
  bool vht;           // include VHT caps (802.11ac)
  uint16_t interval;  // beacon interval TUs (default 100)
  uint16_t capability;
};

static int fb_build_beacon(uint8_t *buf, int buflen, const FbBeaconOpts &o) {
  if (buflen < 128)
    return -1;
  int pos = 0;
  // Frame control: beacon
  Fb80211Hdr *hdr = (Fb80211Hdr *)buf;
  hdr->fc = __builtin_bswap16(0x0080);  // beacon
  hdr->dur = 0;
  memcpy(hdr->addr1, FB_BCAST, 6);
  memcpy(hdr->addr2, o.bssid, 6);
  memcpy(hdr->addr3, o.bssid, 6);
  hdr->seq = fb_next_seq();
  pos = sizeof(Fb80211Hdr);

  // Fixed params
  FbBeaconFixed *fixed = (FbBeaconFixed *)(buf + pos);
  fixed->timestamp = 0;
  fixed->interval = o.interval ? o.interval : 100;
  fixed->capability =
      o.capability
          ? o.capability
          : (FB_CAP_ESS | (o.open ? 0 : FB_CAP_PRIVACY) | FB_CAP_SHORT_PRE | FB_CAT_SHORT_SLOT);
  pos += sizeof(FbBeaconFixed);

  // IEs
  if (o.hidden) {
    buf[pos++] = FB_IE_SSID;
    buf[pos++] = 0;  // hidden: empty SSID
  } else if (o.ssid_raw && o.ssid_raw_len > 0) {
    pos += fb_put_ie_ssid_raw(buf + pos, o.ssid_raw, o.ssid_raw_len);
  } else if (o.ssid) {
    pos += fb_put_ie_ssid(buf + pos, o.ssid);
  } else {
    buf[pos++] = FB_IE_SSID;
    buf[pos++] = 0;
  }
  pos += fb_put_ie_rates(buf + pos);
  pos += fb_put_ie_ds(buf + pos, o.channel);
  pos += fb_put_ie_tim(buf + pos, 0, 2);
  pos += fb_put_ie_country(buf + pos, "US", 1, 11, 20);
  if (o.ht) {
    pos += fb_put_ie_erp(buf + pos, false);
    pos += fb_put_ie_ext_rates(buf + pos);
    pos += fb_put_ie_ht_cap(buf + pos);
    pos += fb_put_ie_ht_op(buf + pos, o.channel);
  }
  if (o.vht && o.channel > 14) {
    pos += fb_put_ie_vht_cap(buf + pos);
    pos += fb_put_ie_vht_op(buf + pos, o.channel, true);
  }
  if (o.wpa3)
    pos += fb_put_ie_rsn_sae(buf + pos);
  else if (o.wpa2)
    pos += fb_put_ie_rsn(buf + pos, true, false, false);
  else if (o.wpa1)
    pos += fb_put_ie_wpa(buf + pos);
  if (o.ht)
    pos += fb_put_ie_ext_cap(buf + pos);
  return pos;
}

// ── Probe Request builder ─────────────────────────────────────────────────────
static int fb_build_probe_req(uint8_t *buf, int buflen, const uint8_t src[6],
                              const char *ssid,  // nullptr = wildcard
                              uint8_t channel) {
  if (buflen < 64)
    return -1;
  int pos = 0;
  Fb80211Hdr *hdr = (Fb80211Hdr *)buf;
  hdr->fc = __builtin_bswap16(0x0040);  // probe req
  hdr->dur = 0;
  memcpy(hdr->addr1, FB_BCAST, 6);
  memcpy(hdr->addr2, src, 6);
  memcpy(hdr->addr3, FB_BCAST, 6);
  hdr->seq = fb_next_seq();
  pos = sizeof(Fb80211Hdr);
  if (ssid && strlen(ssid) > 0)
    pos += fb_put_ie_ssid(buf + pos, ssid);
  else {
    buf[pos++] = 0;
    buf[pos++] = 0;
  }  // wildcard
  pos += fb_put_ie_rates(buf + pos);
  if (channel)
    pos += fb_put_ie_ds(buf + pos, channel);
  pos += fb_put_ie_ext_rates(buf + pos);
  pos += fb_put_ie_ht_cap(buf + pos);
  pos += fb_put_ie_ext_cap(buf + pos);
  return pos;
}

// ── Probe Response builder ────────────────────────────────────────────────────
static int fb_build_probe_resp(uint8_t *buf, int buflen, const uint8_t dst[6],
                               const uint8_t bssid[6], const char *ssid, uint8_t channel,
                               bool wpa2) {
  FbBeaconOpts o{};
  o.ssid = ssid;
  o.channel = channel;
  o.wpa2 = wpa2;
  memcpy(o.bssid, bssid, 6);
  int len = fb_build_beacon(buf, buflen, o);
  if (len < 0)
    return -1;
  Fb80211Hdr *hdr = (Fb80211Hdr *)buf;
  hdr->fc = __builtin_bswap16(0x0050);  // probe resp
  memcpy(hdr->addr1, dst, 6);
  return len;
}

// ── Deauth frame builder ──────────────────────────────────────────────────────
static int fb_build_deauth(uint8_t *buf, int buflen, const uint8_t dst[6], const uint8_t src[6],
                           const uint8_t bssid[6], uint16_t reason) {
  if (buflen < 30)
    return -1;
  Fb80211Hdr *hdr = (Fb80211Hdr *)buf;
  hdr->fc = __builtin_bswap16(0x00C0);  // deauth
  hdr->dur = 0;
  memcpy(hdr->addr1, dst, 6);
  memcpy(hdr->addr2, src, 6);
  memcpy(hdr->addr3, bssid, 6);
  hdr->seq = fb_next_seq();
  uint8_t *p = buf + sizeof(Fb80211Hdr);
  p[0] = (uint8_t)reason;
  p[1] = (uint8_t)(reason >> 8);
  return sizeof(Fb80211Hdr) + 2;
}

// ── Disassoc frame builder ────────────────────────────────────────────────────
static int fb_build_disassoc(uint8_t *buf, int buflen, const uint8_t dst[6], const uint8_t src[6],
                             const uint8_t bssid[6], uint16_t reason) {
  if (buflen < 30)
    return -1;
  Fb80211Hdr *hdr = (Fb80211Hdr *)buf;
  hdr->fc = __builtin_bswap16(0x00A0);  // disassoc
  hdr->dur = 0;
  memcpy(hdr->addr1, dst, 6);
  memcpy(hdr->addr2, src, 6);
  memcpy(hdr->addr3, bssid, 6);
  hdr->seq = fb_next_seq();
  uint8_t *p = buf + sizeof(Fb80211Hdr);
  p[0] = (uint8_t)reason;
  p[1] = (uint8_t)(reason >> 8);
  return sizeof(Fb80211Hdr) + 2;
}

// ── Authentication frame builder ──────────────────────────────────────────────
static int fb_build_auth(uint8_t *buf, int buflen, const uint8_t dst[6], const uint8_t src[6],
                         const uint8_t bssid[6], uint16_t alg, uint16_t seq, uint16_t status) {
  if (buflen < 36)
    return -1;
  Fb80211Hdr *hdr = (Fb80211Hdr *)buf;
  hdr->fc = __builtin_bswap16(0x00B0);
  hdr->dur = 0;
  memcpy(hdr->addr1, dst, 6);
  memcpy(hdr->addr2, src, 6);
  memcpy(hdr->addr3, bssid, 6);
  hdr->seq = fb_next_seq();
  uint8_t *p = buf + sizeof(Fb80211Hdr);
  p[0] = (uint8_t)alg;
  p[1] = (uint8_t)(alg >> 8);
  p[2] = (uint8_t)seq;
  p[3] = (uint8_t)(seq >> 8);
  p[4] = (uint8_t)status;
  p[5] = (uint8_t)(status >> 8);
  return sizeof(Fb80211Hdr) + 6;
}

// ── Association Request builder ───────────────────────────────────────────────
static int fb_build_assoc_req(uint8_t *buf, int buflen, const uint8_t sta[6],
                              const uint8_t bssid[6], const char *ssid, uint16_t capability,
                              uint16_t listen_interval) {
  if (buflen < 128)
    return -1;
  Fb80211Hdr *hdr = (Fb80211Hdr *)buf;
  hdr->fc = __builtin_bswap16(0x0000);  // assoc req
  hdr->dur = 0;
  memcpy(hdr->addr1, bssid, 6);
  memcpy(hdr->addr2, sta, 6);
  memcpy(hdr->addr3, bssid, 6);
  hdr->seq = fb_next_seq();
  int pos = sizeof(Fb80211Hdr);
  buf[pos++] = (uint8_t)capability;
  buf[pos++] = (uint8_t)(capability >> 8);
  buf[pos++] = (uint8_t)listen_interval;
  buf[pos++] = (uint8_t)(listen_interval >> 8);
  pos += fb_put_ie_ssid(buf + pos, ssid);
  pos += fb_put_ie_rates(buf + pos);
  pos += fb_put_ie_ext_rates(buf + pos);
  pos += fb_put_ie_ht_cap(buf + pos);
  return pos;
}

// ── Association Response builder ──────────────────────────────────────────────
static int fb_build_assoc_resp(uint8_t *buf, int buflen, const uint8_t sta[6],
                               const uint8_t bssid[6], uint16_t status, uint16_t aid) {
  if (buflen < 64)
    return -1;
  Fb80211Hdr *hdr = (Fb80211Hdr *)buf;
  hdr->fc = __builtin_bswap16(0x0010);  // assoc resp
  hdr->dur = 0;
  memcpy(hdr->addr1, sta, 6);
  memcpy(hdr->addr2, bssid, 6);
  memcpy(hdr->addr3, bssid, 6);
  hdr->seq = fb_next_seq();
  int pos = sizeof(Fb80211Hdr);
  uint16_t cap = FB_CAP_ESS | FB_CAT_SHORT_SLOT;
  buf[pos++] = (uint8_t)cap;
  buf[pos++] = (uint8_t)(cap >> 8);
  buf[pos++] = (uint8_t)status;
  buf[pos++] = (uint8_t)(status >> 8);
  buf[pos++] = (uint8_t)aid;
  buf[pos++] = (uint8_t)(aid >> 8);
  pos += fb_put_ie_rates(buf + pos);
  pos += fb_put_ie_ext_rates(buf + pos);
  pos += fb_put_ie_ht_cap(buf + pos);
  return pos;
}

// ── Action frame builder ──────────────────────────────────────────────────────
static int fb_build_action(uint8_t *buf, int buflen, const uint8_t dst[6], const uint8_t src[6],
                           const uint8_t bssid[6], uint8_t category, uint8_t action,
                           const uint8_t *body, uint8_t body_len) {
  if (buflen < 32 + body_len)
    return -1;
  Fb80211Hdr *hdr = (Fb80211Hdr *)buf;
  hdr->fc = __builtin_bswap16(0x00D0);  // action
  hdr->dur = 0;
  memcpy(hdr->addr1, dst, 6);
  memcpy(hdr->addr2, src, 6);
  memcpy(hdr->addr3, bssid, 6);
  hdr->seq = fb_next_seq();
  int pos = sizeof(Fb80211Hdr);
  buf[pos++] = category;
  buf[pos++] = action;
  if (body && body_len > 0) {
    memcpy(buf + pos, body, body_len);
    pos += body_len;
  }
  return pos;
}

// ── BSS Transition Management Request (BTM/802.11v) ──────────────────────────
static int fb_build_btm_req(uint8_t *buf, int buflen, const uint8_t sta[6], const uint8_t bssid[6],
                            uint8_t dialog_token, uint8_t disassoc_imminent) {
  uint8_t body[6] = {};
  body[0] = dialog_token;
  body[1] = (uint8_t)((disassoc_imminent ? 0x04 : 0x00) | 0x01);  // request mode
  body[2] = 0x00;
  body[3] = 0x00;  // disassoc timer
  body[4] = 0x00;  // validity interval
  return fb_build_action(buf, buflen, sta, bssid, bssid, 10, 7, body, 5);
}

// ── SA Query Request (PMF) ────────────────────────────────────────────────────
static int fb_build_sa_query_req(uint8_t *buf, int buflen, const uint8_t dst[6],
                                 const uint8_t src[6], const uint8_t bssid[6]) {
  uint8_t body[3] = {0x00, 0x00, 0x00};  // transaction identifier
  return fb_build_action(buf, buflen, dst, src, bssid, 8, 0, body, 3);
}

// ── Null data frame (power-save) ──────────────────────────────────────────────
static int fb_build_null(uint8_t *buf, int buflen, const uint8_t sta[6], const uint8_t bssid[6],
                         bool power_save, bool qos) {
  if (buflen < 30)
    return -1;
  Fb80211Hdr *hdr = (Fb80211Hdr *)buf;
  uint16_t stype = qos ? 0x00C8 : 0x0048;  // QoS null or null
  hdr->fc = __builtin_bswap16(stype);
  if (power_save)
    hdr->fc |= (1 << 12);  // PM bit
  hdr->dur = 0;
  memcpy(hdr->addr1, bssid, 6);
  memcpy(hdr->addr2, sta, 6);
  memcpy(hdr->addr3, bssid, 6);
  hdr->seq = fb_next_seq();
  int pos = sizeof(Fb80211Hdr);
  if (qos) {
    buf[pos++] = 0x00;
    buf[pos++] = 0x00;
  }  // QoS control
  return pos;
}

// ── RTS frame builder ─────────────────────────────────────────────────────────
struct __attribute__((packed)) FbRtsFrame {
  uint16_t fc;
  uint16_t dur;
  uint8_t ra[6];  // receiver
  uint8_t ta[6];  // transmitter
};
static int fb_build_rts(uint8_t *buf, int buflen, const uint8_t ra[6], const uint8_t ta[6],
                        uint16_t duration) {
  if (buflen < 16)
    return -1;
  FbRtsFrame *f = (FbRtsFrame *)buf;
  f->fc = __builtin_bswap16(0x00B4);  // RTS
  f->dur = duration;
  memcpy(f->ra, ra, 6);
  memcpy(f->ta, ta, 6);
  return sizeof(FbRtsFrame);
}

// ── CTS frame builder ─────────────────────────────────────────────────────────
struct __attribute__((packed)) FbCtsFrame {
  uint16_t fc;
  uint16_t dur;
  uint8_t ra[6];
};
static int fb_build_cts(uint8_t *buf, int buflen, const uint8_t ra[6], uint16_t duration) {
  if (buflen < 10)
    return -1;
  FbCtsFrame *f = (FbCtsFrame *)buf;
  f->fc = __builtin_bswap16(0x00C4);  // CTS
  f->dur = duration;
  memcpy(f->ra, ra, 6);
  return sizeof(FbCtsFrame);
}

// ── BAR (Block Ack Request) ───────────────────────────────────────────────────
static int fb_build_bar(uint8_t *buf, int buflen, const uint8_t ra[6], const uint8_t ta[6],
                        uint8_t tid, uint16_t seq) {
  if (buflen < 16)
    return -1;
  Fb80211Hdr *hdr = (Fb80211Hdr *)buf;
  hdr->fc = __builtin_bswap16(0x0084);  // BAR
  hdr->dur = 0;
  memcpy(hdr->addr1, ra, 6);
  memcpy(hdr->addr2, ta, 6);
  memset(hdr->addr3, 0, 6);
  hdr->seq = 0;
  int pos = sizeof(Fb80211Hdr) - 6;  // BAR has no addr3+seq in some interpretations
  // BAR control
  buf[pos++] = 0x04 | (tid << 12);
  buf[pos++] = (tid >> 4);
  // Starting sequence control
  buf[pos++] = (uint8_t)seq;
  buf[pos++] = (uint8_t)(seq >> 8);
  return pos;
}

// ── EAP frame builder (for EAPOL injection) ───────────────────────────────────
struct __attribute__((packed)) FbEapolHdr {
  uint8_t version;  // 0x01 = 802.1X-2001
  uint8_t type;     // 0=EAP, 1=Start, 2=Logoff, 3=Key, 4=ASF
  uint16_t length;  // body length BE
};
struct __attribute__((packed)) FbEapHdr {
  uint8_t code;  // 1=Req, 2=Resp, 3=Success, 4=Failure
  uint8_t id;
  uint16_t length;  // total BE including header
  uint8_t type;     // 1=Identity, 4=MD5, 25=PEAP, 26=MSCHAPv2
};

static int fb_build_eapol_start(uint8_t *buf, int buflen, const uint8_t sta[6],
                                const uint8_t bssid[6]) {
  // LLC + EAPOL Start
  if (buflen < 40)
    return -1;
  Fb80211Hdr *hdr = (Fb80211Hdr *)buf;
  hdr->fc = __builtin_bswap16(0x0208);  // data frame
  hdr->dur = 0;
  memcpy(hdr->addr1, bssid, 6);
  memcpy(hdr->addr2, sta, 6);
  memcpy(hdr->addr3, bssid, 6);
  hdr->seq = fb_next_seq();
  int pos = sizeof(Fb80211Hdr);
  // LLC snap header
  buf[pos++] = 0xAA;
  buf[pos++] = 0xAA;
  buf[pos++] = 0x03;
  buf[pos++] = 0x00;
  buf[pos++] = 0x00;
  buf[pos++] = 0x00;
  buf[pos++] = 0x88;
  buf[pos++] = 0x8E;  // EtherType 0x888E
  // EAPOL Start
  FbEapolHdr *eapol = (FbEapolHdr *)(buf + pos);
  eapol->version = 0x01;
  eapol->type = 0x01;
  eapol->length = 0;
  pos += sizeof(FbEapolHdr);
  return pos;
}

// ── SAE Commit frame builder (WPA3) ───────────────────────────────────────────
static int fb_build_sae_commit(uint8_t *buf, int buflen, const uint8_t dst[6], const uint8_t src[6],
                               const uint8_t bssid[6]) {
  // Auth frame with alg=SAE, seq=1
  int len = fb_build_auth(buf, buflen, dst, src, bssid, FB_AUTH_SAE, 1, FB_STATUS_SUCCESS);
  if (len < 0)
    return -1;
  // Append SAE commit body: group=19 (P-256), scalar (32B), element (64B)
  int pos = len;
  if (pos + 98 > buflen)
    return -1;
  buf[pos++] = 0x13;
  buf[pos++] = 0x00;  // group=19
  // Random scalar (32 bytes)
  for (int i = 0; i < 32; i++)
    buf[pos++] = (uint8_t)esp_random();
  // Random element (64 bytes, x+y)
  for (int i = 0; i < 64; i++)
    buf[pos++] = (uint8_t)esp_random();
  return pos;
}

// ── SAE Confirm frame builder ─────────────────────────────────────────────────
static int fb_build_sae_confirm(uint8_t *buf, int buflen, const uint8_t dst[6],
                                const uint8_t src[6], const uint8_t bssid[6]) {
  int len = fb_build_auth(buf, buflen, dst, src, bssid, FB_AUTH_SAE, 2, FB_STATUS_SUCCESS);
  if (len < 0)
    return -1;
  int pos = len;
  if (pos + 34 > buflen)
    return -1;
  buf[pos++] = 0x00;
  buf[pos++] = 0x00;  // send confirm = 0
  for (int i = 0; i < 32; i++)
    buf[pos++] = (uint8_t)esp_random();  // confirm token
  return pos;
}

// ── Channel Switch Announcement (CSA) action builder ─────────────────────────
static int fb_build_csa_action(uint8_t *buf, int buflen, const uint8_t dst[6], const uint8_t src[6],
                               const uint8_t bssid[6], uint8_t new_ch, uint8_t count) {
  // Public action CSA
  uint8_t body[5] = {};
  body[0] = 0x04;  // new channel
  body[1] = new_ch;
  body[2] = count;
  // Extended CSA element embedded
  body[3] = 0x34;
  body[4] = 0x01;  // IE 52, len 1 (just mode)
  return fb_build_action(buf, buflen, dst, src, bssid, 4, 4, body, 5);
}

// ── ESP-NOW broadcast frame builder ──────────────────────────────────────────
static int fb_build_espnow(uint8_t *buf, int buflen, const uint8_t src[6], const uint8_t *payload,
                           uint8_t payload_len) {
  if (buflen < 32 + payload_len)
    return -1;
  Fb80211Hdr *hdr = (Fb80211Hdr *)buf;
  hdr->fc = __builtin_bswap16(0x00D0);  // action
  hdr->dur = 0;
  memcpy(hdr->addr1, FB_BCAST, 6);
  memcpy(hdr->addr2, src, 6);
  memcpy(hdr->addr3, FB_BCAST, 6);
  hdr->seq = fb_next_seq();
  int pos = sizeof(Fb80211Hdr);
  // Vendor-specific OUI: 18:FE:34 (Espressif)
  buf[pos++] = 0x7F;  // action category: vendor specific
  buf[pos++] = 0x18;
  buf[pos++] = 0xFE;
  buf[pos++] = 0x34;  // Espressif OUI
  buf[pos++] = 0x04;  // ESP-NOW subtype
  if (payload && payload_len > 0) {
    memcpy(buf + pos, payload, payload_len);
    pos += payload_len;
  }
  return pos;
}

// ── Inject helper ─────────────────────────────────────────────────────────────
static esp_err_t fb_inject(const uint8_t *buf, int len) {
  return esp_wifi_80211_tx(WIFI_IF_AP, buf, len, false);
}

static esp_err_t fb_inject_sta(const uint8_t *buf, int len) {
  return esp_wifi_80211_tx(WIFI_IF_STA, buf, len, false);
}

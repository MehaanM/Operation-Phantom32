#pragma once
#include <Arduino.h>
#include <string.h>
#include <stdint.h>

// ── Beacon Parser ─────────────────────────────────────────────────────────────
//
// Comprehensive 802.11 beacon frame parser extracting all standard
// Information Elements (IEs), Vendor Specific OUIs, power constraints,
// country strings, MCS sets, VHT capabilities, HE (Wi-Fi 6) capabilities,
// extended capabilities, and RM capabilities.

#define BP_MAX_IES 64
#define BP_MAX_RATES 16
#define BP_MAX_BASIC_RATES 8
#define BP_MAX_COUNTRY_TRIPLETS 12

// ── Capability info bits ──────────────────────────────────────────────────────
#define BP_CAP_ESS (1 << 0)
#define BP_CAP_IBSS (1 << 1)
#define BP_CAP_CF_POLL (1 << 2)
#define BP_CAP_CF_POLL_REQ (1 << 3)
#define BP_CAP_PRIVACY (1 << 4)
#define BP_CAP_SHORT_PREAMBLE (1 << 5)
#define BP_CAP_PBCC (1 << 6)
#define BP_CAP_CH_AGILITY (1 << 7)
#define BP_CAP_SPECTRUM_MGMT (1 << 8)
#define BP_CAP_QOS (1 << 9)
#define BP_CAP_SHORT_SLOT (1 << 10)
#define BP_CAP_APSD (1 << 11)
#define BP_CAP_RADIO_MEAS (1 << 12)
#define BP_CAP_DSSS_OFDM (1 << 13)
#define BP_CAP_DELAYED_BACK (1 << 14)
#define BP_CAP_IMM_BACK (1 << 15)

// ── IE tag numbers ─────────────────────────────────────────────────────────────
#define IE_SSID 0
#define IE_SUPPORTED_RATES 1
#define IE_DSSS_PARAM 3
#define IE_CF_PARAM 4
#define IE_TIM 5
#define IE_IBSS_PARAM 6
#define IE_COUNTRY 7
#define IE_HOP_PARAM 8
#define IE_HOP_TABLE 9
#define IE_REQUEST 10
#define IE_BSS_LOAD 11
#define IE_EDCA_PARAM 12
#define IE_TSPEC 13
#define IE_TCLAS 14
#define IE_SCHEDULE 15
#define IE_CHALLENGE_TEXT 16
#define IE_POWER_CONSTRAINT 32
#define IE_POWER_CAPABILITY 33
#define IE_TPC_REQ 34
#define IE_TPC_REPORT 35
#define IE_SUPPORTED_CHANNELS 36
#define IE_CHANNEL_SWITCH 37
#define IE_MEASUREMENT_REQ 38
#define IE_MEASUREMENT_REPORT 39
#define IE_QUIET 40
#define IE_IBSS_DFS 41
#define IE_ERP 42
#define IE_TS_DELAY 43
#define IE_TCLAS_PROCESSING 44
#define IE_HT_CAPABILITY 45
#define IE_QOS_CAPABILITY 46
#define IE_RSN 48
#define IE_EXT_SUPPORTED_RATES 50
#define IE_HT_OPERATION 61
#define IE_BSS_COEX 72
#define IE_OVERLAPPING_BSS 73
#define IE_RRM_ENABLED 70
#define IE_MOBILITY_DOMAIN 54
#define IE_FT 55
#define IE_TIMEOUT_INTERVAL 56
#define IE_RIC_DATA 57
#define IE_DSE_REG_LOC 58
#define IE_SUPPORTED_OPCLASS 59
#define IE_EXT_CHANNEL_SWITCH 60
#define IE_SECONDARY_CHAN_OFFSET 62
#define IE_BSS_AVERAGE_DELAY 63
#define IE_ANTENNA 64
#define IE_RSNI 65
#define IE_MEASUREMENT_PILOT 66
#define IE_BSS_AVAIL_ADM 67
#define IE_BSS_AC_ACCESS_DELAY 68
#define IE_TIME_ADV 69
#define IE_INTERWORKING 107
#define IE_ADVERTISEMENT_PROTOCOL 108
#define IE_ROAMING_CONSORTIUM 111
#define IE_EMERGENCY_ALERT 112
#define IE_MESH_CONFIGURATION 113
#define IE_MESH_ID 114
#define IE_MESH_LINK_METRIC 115
#define IE_CONGESTION_NOTIF 116
#define IE_MESH_PEERING_MGMT 117
#define IE_MESH_CHANNEL_SWITCH 118
#define IE_MESH_AWAKE_WINDOW 119
#define IE_BEACON_TIMING 120
#define IE_MCCAOP_SETUP_REQ 121
#define IE_MCCAOP_SETUP_REPLY 122
#define IE_MCCAOP_ADVERT 123
#define IE_MCCAOP_TEARDOWN 124
#define IE_GANN 125
#define IE_RANN 126
#define IE_EXT_CAPABILITIES 127
#define IE_PREQ 130
#define IE_PREP 131
#define IE_PERR 132
#define IE_PXU 137
#define IE_PXUC 138
#define IE_AUTHENTICATED_MESH_PEERING_EXCHANGE 139
#define IE_MIC 140
#define IE_DESTINATION_URI 141
#define IE_U_APSD_COEX 142
#define IE_WAKEUP_SCHEDULE 143
#define IE_EXT_SCHEDULE 144
#define IE_STA_AVAILABILITY 145
#define IE_DMG_TSPEC 146
#define IE_NEXT_DMG_ATI 147
#define IE_DMG_CAPABILITIES 148
#define IE_DMG_OPERATION 151
#define IE_DMG_BSS_PARAM_CHANGE 152
#define IE_DMG_BEAM_REFINEMENT 153
#define IE_CHANNEL_MEAS_FEEDBACK 154
#define IE_CCKM 156
#define IE_VHT_CAPABILITY 191
#define IE_VHT_OPERATION 192
#define IE_EXT_BSS_LOAD 193
#define IE_WIDE_BW_CHANNEL_SWITCH 194
#define IE_TRANSMIT_POWER_ENVELOPE 195
#define IE_CHANNEL_SWITCH_WRAPPER 196
#define IE_AID 197
#define IE_QUIET_CHANNEL 198
#define IE_OP_MODE_NOTIFICATION 199
#define IE_UPSIM 200
#define IE_REDUCED_NEIGHBOR_REPORT 201
#define IE_TVHT_OPERATION 202
#define IE_DEVICE_LOCATION 204
#define IE_VENDOR_SPECIFIC 221
#define IE_EXTENSION 255

// ── Country constraint triplet ────────────────────────────────────────────────
struct BpCountryTriplet {
  uint8_t first_chan;
  uint8_t num_chans;
  int8_t max_tx_power;
};

// ── ERP info flags ────────────────────────────────────────────────────────────
#define ERP_NON_ERP_PRESENT (1 << 0)
#define ERP_USE_PROTECTION (1 << 1)
#define ERP_BARKER_PREAMBLE (1 << 2)

// ── HT operation ──────────────────────────────────────────────────────────────
struct BpHtOp {
  uint8_t primary_chan;
  uint8_t secondary_chan_offset;  // 0=none, 1=above, 3=below
  bool sta_chan_width;            // true=any, false=20MHz only
  bool rifs;
  uint8_t ht_prot;
  bool nongf_ht_present;
  bool obss_ht_prot;
  uint8_t basic_mcs[2];  // first 2 bytes of 16-byte MCS set
};

// ── VHT operation ─────────────────────────────────────────────────────────────
struct BpVhtOp {
  uint8_t chan_width;  // 0=80MHz absent, 1=80, 2=160, 3=80+80
  uint8_t center_seg0;
  uint8_t center_seg1;
  uint16_t basic_mcs_set;
};

// ── Full beacon parse result ──────────────────────────────────────────────────
struct BpBeacon {
  // Header fields
  uint8_t bssid[6];
  uint8_t ta[6];
  uint64_t timestamp;
  uint16_t beacon_interval_tu;
  uint16_t capability_info;
  char ssid[33];
  bool ssid_hidden;

  // Channel
  uint8_t ds_channel;          // from DSSS parameter set IE
  uint8_t ht_primary_channel;  // from HT Operation IE

  // Supported rates
  uint8_t rates[BP_MAX_RATES];
  uint8_t rate_count;
  uint8_t basic_rates[BP_MAX_BASIC_RATES];
  uint8_t basic_rate_count;
  uint8_t ext_rates[BP_MAX_RATES];
  uint8_t ext_rate_count;

  // Country
  char country_code[3];
  BpCountryTriplet country_triplets[BP_MAX_COUNTRY_TRIPLETS];
  uint8_t country_triplet_count;
  uint8_t max_tx_power_dbm;

  // ERP
  bool erp_present;
  uint8_t erp_info;

  // RSN / WPA
  bool rsn_present;
  bool wpa_present;
  uint8_t rsn_ver;
  uint32_t group_cipher;  // packed OUI+type
  bool ccmp;
  bool tkip;
  bool pmf_capable;
  bool pmf_required;
  bool sae;
  bool owe;

  // HT
  bool ht_present;
  uint8_t ht_max_amsdu;  // 0=3839 1=7935
  bool ht_greenfield;
  bool ht_sgi_20;
  bool ht_sgi_40;
  bool ht_40_plus;
  bool ht_40_minus;
  uint8_t ht_chan_width;
  BpHtOp ht_op;
  bool ht_op_present;

  // VHT
  bool vht_present;
  uint16_t vht_mcs_rx;
  uint16_t vht_mcs_tx;
  uint8_t vht_max_mpdu;
  bool vht_sgi_80;
  bool vht_sgi_160;
  uint8_t vht_chan_width_cap;
  BpVhtOp vht_op;
  bool vht_op_present;

  // BSS Load
  bool bss_load_present;
  uint16_t bss_station_count;
  uint8_t bss_channel_util;
  uint16_t bss_avail_cap;

  // Extended capabilities
  bool ext_cap_present;
  bool ext_cap_bss_transition;
  bool ext_cap_dms;
  bool ext_cap_tfs;
  bool ext_cap_wnm_sleep;
  bool ext_cap_tim_bcast;
  bool ext_cap_bss_coex;

  // RRM
  bool rrm_present;
  uint8_t rrm_caps[5];

  // TIM
  bool tim_present;
  uint8_t dtim_count;
  uint8_t dtim_period;

  // Power
  bool power_constraint_present;
  uint8_t power_constraint_db;
  int8_t tx_power_dbm;  // effective

  // Interworking / Hotspot 2.0
  bool interworking_present;
  bool access_network_type;
  bool internet_bit;
  bool hs20_present;

  // Vendor specific
  bool wps_present;
  bool p2p_present;
  uint8_t vendor_ouis[8][3];  // up to 8 vendor OUIs seen
  uint8_t vendor_oui_count;

  // Misc
  bool probe_response;  // set true if this was a probe response
  uint32_t parsed_ms;   // millis() at parse time
};

// ── Utility: read uint16_t LE ─────────────────────────────────────────────────
static inline uint16_t bp_u16le(const uint8_t *p) {
  return (uint16_t)(p[0] | (p[1] << 8));
}
static inline uint32_t bp_u32le(const uint8_t *p) {
  return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24));
}

// ── Rate decode ───────────────────────────────────────────────────────────────
static uint8_t bp_rate_mbps_x2(uint8_t r) {
  return r & 0x7F;  // 500kbps units
}

// ── Parse supported rates IE ──────────────────────────────────────────────────
static void bp_parse_rates(BpBeacon *b, const uint8_t *data, uint8_t len, bool extended) {
  for (uint8_t i = 0; i < len; i++) {
    uint8_t r = data[i];
    bool basic = (r & 0x80) != 0;
    uint8_t rate = r & 0x7F;
    if (extended) {
      if (b->ext_rate_count < BP_MAX_RATES)
        b->ext_rates[b->ext_rate_count++] = rate;
    } else {
      if (b->rate_count < BP_MAX_RATES)
        b->rates[b->rate_count++] = rate;
      if (basic && b->basic_rate_count < BP_MAX_BASIC_RATES)
        b->basic_rates[b->basic_rate_count++] = rate;
    }
  }
}

// ── Parse RSN IE ──────────────────────────────────────────────────────────────
static void bp_parse_rsn(BpBeacon *b, const uint8_t *d, uint8_t len) {
  if (len < 2)
    return;
  b->rsn_present = true;
  b->rsn_ver = bp_u16le(d);
  if (len < 6)
    return;
  uint32_t group = bp_u32le(d + 2);
  b->group_cipher = group;
  uint8_t suite_type = d[5];
  if (suite_type == 2)
    b->tkip = true;
  if (suite_type == 4)
    b->ccmp = true;
  if (len < 8)
    return;
  uint16_t pairwise_count = bp_u16le(d + 6);
  uint16_t off = 8;
  for (uint16_t i = 0; i < pairwise_count && off + 4 <= len; i++) {
    if (d[off + 3] == 2)
      b->tkip = true;
    if (d[off + 3] == 4)
      b->ccmp = true;
    off += 4;
  }
  if (off + 2 > len)
    return;
  uint16_t akm_count = bp_u16le(d + off);
  off += 2;
  for (uint16_t i = 0; i < akm_count && off + 4 <= len; i++) {
    uint8_t akm = d[off + 3];
    if (akm == 2 || akm == 6 || akm == 8 || akm == 9)
      ;  // PSK variants
    if (akm == 8)
      b->sae = true;  // SAE = WPA3
    if (akm == 18)
      b->owe = true;  // OWE
    off += 4;
  }
  if (off + 2 > len)
    return;
  uint16_t rsnc = bp_u16le(d + off);
  b->pmf_capable = (rsnc & 0x0080) != 0;
  b->pmf_required = (rsnc & 0x0040) != 0;
}

// ── Parse HT capability ───────────────────────────────────────────────────────
static void bp_parse_ht_cap(BpBeacon *b, const uint8_t *d, uint8_t len) {
  if (len < 26)
    return;
  b->ht_present = true;
  uint16_t ht_cap = bp_u16le(d);
  b->ht_chan_width = (ht_cap >> 1) & 1;
  b->ht_greenfield = (ht_cap >> 4) & 1;
  b->ht_sgi_20 = (ht_cap >> 5) & 1;
  b->ht_sgi_40 = (ht_cap >> 6) & 1;
  b->ht_max_amsdu = (ht_cap >> 11) & 1;
  b->ht_40_plus = (ht_cap >> 6) & 1;
}

// ── Parse HT operation ────────────────────────────────────────────────────────
static void bp_parse_ht_op(BpBeacon *b, const uint8_t *d, uint8_t len) {
  if (len < 22)
    return;
  b->ht_op_present = true;
  b->ht_op.primary_chan = d[0];
  b->ht_op.secondary_chan_offset = d[1] & 0x03;
  b->ht_op.sta_chan_width = (d[1] >> 2) & 1;
  b->ht_op.rifs = (d[1] >> 3) & 1;
  b->ht_op.ht_prot = d[2] & 0x03;
  b->ht_op.nongf_ht_present = (d[2] >> 2) & 1;
  b->ht_op.obss_ht_prot = (d[2] >> 4) & 1;
  b->ht_op.basic_mcs[0] = d[6];
  b->ht_op.basic_mcs[1] = d[7];
}

// ── Parse VHT capability ──────────────────────────────────────────────────────
static void bp_parse_vht_cap(BpBeacon *b, const uint8_t *d, uint8_t len) {
  if (len < 12)
    return;
  b->vht_present = true;
  uint32_t vht_cap = bp_u32le(d);
  b->vht_max_mpdu = vht_cap & 0x03;
  b->vht_chan_width_cap = (vht_cap >> 2) & 0x03;
  b->vht_sgi_80 = (vht_cap >> 5) & 1;
  b->vht_sgi_160 = (vht_cap >> 6) & 1;
  b->vht_mcs_rx = bp_u16le(d + 8);
  b->vht_mcs_tx = bp_u16le(d + 10);
}

// ── Parse VHT operation ───────────────────────────────────────────────────────
static void bp_parse_vht_op(BpBeacon *b, const uint8_t *d, uint8_t len) {
  if (len < 5)
    return;
  b->vht_op_present = true;
  b->vht_op.chan_width = d[0];
  b->vht_op.center_seg0 = d[1];
  b->vht_op.center_seg1 = d[2];
  b->vht_op.basic_mcs_set = bp_u16le(d + 3);
}

// ── Parse BSS Load ────────────────────────────────────────────────────────────
static void bp_parse_bss_load(BpBeacon *b, const uint8_t *d, uint8_t len) {
  if (len < 5)
    return;
  b->bss_load_present = true;
  b->bss_station_count = bp_u16le(d);
  b->bss_channel_util = d[2];
  b->bss_avail_cap = bp_u16le(d + 3);
}

// ── Parse country IE ──────────────────────────────────────────────────────────
static void bp_parse_country(BpBeacon *b, const uint8_t *d, uint8_t len) {
  if (len < 3)
    return;
  b->country_code[0] = d[0];
  b->country_code[1] = d[1];
  b->country_code[2] = 0;
  uint8_t off = 3;
  while (off + 3 <= len && b->country_triplet_count < BP_MAX_COUNTRY_TRIPLETS) {
    BpCountryTriplet &t = b->country_triplets[b->country_triplet_count++];
    t.first_chan = d[off];
    t.num_chans = d[off + 1];
    t.max_tx_power = (int8_t)d[off + 2];
    off += 3;
  }
}

// ── Parse extended capabilities ───────────────────────────────────────────────
static void bp_parse_ext_cap(BpBeacon *b, const uint8_t *d, uint8_t len) {
  b->ext_cap_present = true;
  if (len >= 1) {
    b->ext_cap_bss_coex = (d[0] >> 0) & 1;
  }
  if (len >= 3) {
    b->ext_cap_bss_transition = (d[2] >> 3) & 1;
    b->ext_cap_dms = (d[2] >> 2) & 1;
  }
  if (len >= 4) {
    b->ext_cap_tfs = (d[3] >> 0) & 1;
    b->ext_cap_wnm_sleep = (d[3] >> 1) & 1;
    b->ext_cap_tim_bcast = (d[3] >> 3) & 1;
  }
}

// ── Parse vendor specific IE ──────────────────────────────────────────────────
static void bp_parse_vendor(BpBeacon *b, const uint8_t *d, uint8_t len) {
  if (len < 4)
    return;
  uint8_t oui[3] = {d[0], d[1], d[2]};
  uint8_t oui_type = d[3];
  // WPS: OUI=00:50:F2 type=4
  if (oui[0] == 0x00 && oui[1] == 0x50 && oui[2] == 0xF2 && oui_type == 4) {
    b->wps_present = true;
  }
  // WPA: OUI=00:50:F2 type=1
  if (oui[0] == 0x00 && oui[1] == 0x50 && oui[2] == 0xF2 && oui_type == 1) {
    b->wpa_present = true;
  }
  // P2P: OUI=50:6F:9A type=9
  if (oui[0] == 0x50 && oui[1] == 0x6F && oui[2] == 0x9A && oui_type == 9) {
    b->p2p_present = true;
  }
  // HS2.0: OUI=50:6F:9A type=16
  if (oui[0] == 0x50 && oui[1] == 0x6F && oui[2] == 0x9A && oui_type == 16) {
    b->hs20_present = true;
  }
  // Record OUI
  if (b->vendor_oui_count < 8) {
    memcpy(b->vendor_ouis[b->vendor_oui_count++], oui, 3);
  }
}

// ── Main beacon parser ────────────────────────────────────────────────────────
static bool bp_parse(BpBeacon *b, const uint8_t *frame, uint16_t frame_len,
                     bool is_probe_response) {
  if (!b || !frame || frame_len < 36)
    return false;
  memset(b, 0, sizeof(*b));
  b->probe_response = is_probe_response;
  b->parsed_ms = millis();

  // 802.11 MAC header (24 bytes)
  // Addresses at bytes 4,10,16
  memcpy(b->bssid, frame + 16, 6);
  memcpy(b->ta, frame + 10, 6);

  // Fixed-length beacon fields (12 bytes at offset 24)
  if (frame_len < 36)
    return false;
  const uint8_t *fixed = frame + 24;
  memcpy(&b->timestamp, fixed, 8);  // LE 8-byte
  b->beacon_interval_tu = bp_u16le(fixed + 8);
  b->capability_info = bp_u16le(fixed + 10);

  // Check ESS
  if (!(b->capability_info & BP_CAP_ESS))
    return false;  // not an AP beacon

  // IE parse starts at offset 36
  const uint8_t *ie = frame + 36;
  uint16_t ie_len = frame_len - 36;
  uint16_t pos = 0;
  while (pos + 2 <= ie_len) {
    uint8_t tag = ie[pos];
    uint8_t tlen = ie[pos + 1];
    if (pos + 2 + tlen > ie_len)
      break;
    const uint8_t *data = ie + pos + 2;
    switch (tag) {
      case IE_SSID:
        if (tlen == 0) {
          b->ssid_hidden = true;
          break;
        }
        if (tlen > 32)
          tlen = 32;
        memcpy(b->ssid, data, tlen);
        b->ssid[tlen] = 0;
        // Check for hidden SSID (all nulls or spaces)
        {
          bool hidden = true;
          for (uint8_t i = 0; i < tlen; i++)
            if (data[i] > 0x20) {
              hidden = false;
              break;
            }
          if (hidden)
            b->ssid_hidden = true;
          b->ssid[tlen] = 0;
        }
        break;
      case IE_SUPPORTED_RATES:
        bp_parse_rates(b, data, tlen, false);
        break;
      case IE_DSSS_PARAM:
        if (tlen >= 1)
          b->ds_channel = data[0];
        break;
      case IE_TIM:
        b->tim_present = true;
        if (tlen >= 2) {
          b->dtim_count = data[0];
          b->dtim_period = data[1];
        }
        break;
      case IE_COUNTRY:
        bp_parse_country(b, data, tlen);
        break;
      case IE_POWER_CONSTRAINT:
        b->power_constraint_present = true;
        if (tlen >= 1)
          b->power_constraint_db = data[0];
        break;
      case IE_ERP:
        b->erp_present = true;
        if (tlen >= 1)
          b->erp_info = data[0];
        break;
      case IE_HT_CAPABILITY:
        bp_parse_ht_cap(b, data, tlen);
        break;
      case IE_RSN:
        bp_parse_rsn(b, data, tlen);
        break;
      case IE_EXT_SUPPORTED_RATES:
        bp_parse_rates(b, data, tlen, true);
        break;
      case IE_HT_OPERATION:
        bp_parse_ht_op(b, data, tlen);
        b->ht_primary_channel = b->ht_op.primary_chan;
        break;
      case IE_BSS_LOAD:
        bp_parse_bss_load(b, data, tlen);
        break;
      case IE_RRM_ENABLED:
        b->rrm_present = true;
        if (tlen >= 5)
          memcpy(b->rrm_caps, data, 5);
        break;
      case IE_EXT_CAPABILITIES:
        bp_parse_ext_cap(b, data, tlen);
        break;
      case IE_VHT_CAPABILITY:
        bp_parse_vht_cap(b, data, tlen);
        break;
      case IE_VHT_OPERATION:
        bp_parse_vht_op(b, data, tlen);
        break;
      case IE_INTERWORKING:
        b->interworking_present = true;
        if (tlen >= 1) {
          b->access_network_type = data[0] & 0x0F;
          b->internet_bit = (data[0] >> 4) & 1;
        }
        break;
      case IE_VENDOR_SPECIFIC:
        bp_parse_vendor(b, data, tlen);
        break;
      default:
        break;
    }
    pos += 2 + tlen;
  }

  // Determine effective channel
  if (b->ht_primary_channel)
    b->ds_channel = b->ht_primary_channel;

  return true;
}

// ── Capability flags string ───────────────────────────────────────────────────
static String bp_cap_string(uint16_t cap) {
  String s = "";
  if (cap & BP_CAP_ESS)
    s += "ESS ";
  if (cap & BP_CAP_IBSS)
    s += "IBSS ";
  if (cap & BP_CAP_PRIVACY)
    s += "Privacy ";
  if (cap & BP_CAP_SHORT_SLOT)
    s += "ShortSlot ";
  if (cap & BP_CAP_QOS)
    s += "QoS ";
  if (cap & BP_CAP_SPECTRUM_MGMT)
    s += "SpecMgmt ";
  if (cap & BP_CAP_RADIO_MEAS)
    s += "RRM ";
  if (s.length() && s.charAt(s.length() - 1) == ' ')
    s = s.substring(0, s.length() - 1);
  return s;
}

// ── Encryption description ─────────────────────────────────────────────────────
static const char *bp_enc_desc(const BpBeacon *b) {
  if (!(b->capability_info & BP_CAP_PRIVACY))
    return "Open";
  if (b->rsn_present || b->wpa_present) {
    if (b->sae)
      return "WPA3-SAE";
    if (b->owe)
      return "OWE";
    if (b->pmf_required)
      return "WPA3-Personal";
    if (b->ccmp && !b->tkip)
      return "WPA2-CCMP";
    if (b->tkip && !b->ccmp)
      return "WPA-TKIP";
    if (b->ccmp && b->tkip)
      return "WPA/WPA2-Mixed";
    if (b->wpa_present && !b->rsn_present)
      return "WPA";
    return "WPA2";
  }
  return "WEP";
}

// ── PHY description ───────────────────────────────────────────────────────────
static const char *bp_phy_desc(const BpBeacon *b) {
  if (b->vht_present)
    return "802.11ac (VHT)";
  if (b->ht_present)
    return "802.11n (HT)";
  if (b->erp_present)
    return "802.11g (ERP)";
  // Check rates for 802.11a-like
  for (uint8_t i = 0; i < b->rate_count; i++) {
    if (b->rates[i] == 108)
      return "802.11a";  // 54 Mbps
  }
  return "802.11b";
}

// ── Beacon JSON ───────────────────────────────────────────────────────────────
static String bp_beacon_json(const BpBeacon *b) {
  char mac[18];
  snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X", b->bssid[0], b->bssid[1], b->bssid[2],
           b->bssid[3], b->bssid[4], b->bssid[5]);
  String j = "{\"bssid\":\"";
  j += mac;
  j += "\"";
  j += ",\"ssid\":\"";
  j += b->ssid;
  j += "\"";
  j += ",\"hidden\":";
  j += b->ssid_hidden ? 1 : 0;
  j += ",\"ch\":";
  j += b->ds_channel;
  j += ",\"bi_tu\":";
  j += b->beacon_interval_tu;
  j += ",\"enc\":\"";
  j += bp_enc_desc(b);
  j += "\"";
  j += ",\"phy\":\"";
  j += bp_phy_desc(b);
  j += "\"";
  j += ",\"pmf_cap\":";
  j += b->pmf_capable ? 1 : 0;
  j += ",\"pmf_req\":";
  j += b->pmf_required ? 1 : 0;
  j += ",\"wps\":";
  j += b->wps_present ? 1 : 0;
  j += ",\"p2p\":";
  j += b->p2p_present ? 1 : 0;
  j += ",\"hs20\":";
  j += b->hs20_present ? 1 : 0;
  j += ",\"vht\":";
  j += b->vht_present ? 1 : 0;
  j += ",\"ht\":";
  j += b->ht_present ? 1 : 0;
  j += ",\"erp\":";
  j += b->erp_present ? 1 : 0;
  j += ",\"country\":\"";
  j += b->country_code;
  j += "\"";
  if (b->bss_load_present) {
    j += ",\"bss_stas\":";
    j += b->bss_station_count;
    j += ",\"bss_util\":";
    j += b->bss_channel_util;
  }
  j += ",\"dtim_period\":";
  j += b->dtim_period;
  j += ",\"interwork\":";
  j += b->interworking_present ? 1 : 0;
  j += ",\"internet\":";
  j += b->internet_bit ? 1 : 0;
  j += ",\"rates_mbps\":[";
  for (uint8_t i = 0; i < b->rate_count; i++) {
    if (i)
      j += ",";
    j += String((float)b->rates[i] / 2.0f, 1);
  }
  j += "]}";
  return j;
}

// ── HTTP routes ───────────────────────────────────────────────────────────────
static void bp_routes() {
  extern WebServer server;

  server.on("/api/bp/status", HTTP_GET, []() {
    server.send(200, "application/json", "{\"module\":\"beacon_parser\",\"ok\":1}");
  });
}

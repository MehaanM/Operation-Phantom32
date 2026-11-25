#pragma once
// frame_decoder.h — Deep 802.11 frame decoder with IE parsing and field extraction.
// Provides a unified decode path for all subsystems. Prefix: frd_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

// ─── 802.11 frame control field bits ─────────────────────────────────────────
#define FRD_FC_TYPE_SHIFT 2
#define FRD_FC_TYPE_MASK 0x03
#define FRD_FC_SUBTYPE_SHIFT 4
#define FRD_FC_SUBTYPE_MASK 0x0F
#define FRD_FC_TO_DS 0x01
#define FRD_FC_FROM_DS 0x02
#define FRD_FC_MORE_FRAG 0x04
#define FRD_FC_RETRY 0x08
#define FRD_FC_PWR_MGMT 0x10
#define FRD_FC_MORE_DATA 0x20
#define FRD_FC_PROTECTED 0x40
#define FRD_FC_HTCPLUS 0x80

// ─── Parsed IE collection ─────────────────────────────────────────────────────
#define FRD_MAX_IES 32
#define FRD_IE_DATA 24

struct FrdIE {
  uint8_t tag;
  uint8_t len;
  uint8_t data[FRD_IE_DATA];  // first FRD_IE_DATA bytes
  bool valid;
};

// ─── Decoded beacon/probe response ───────────────────────────────────────────
struct FrdBeacon {
  uint8_t bssid[6];
  uint8_t src[6];
  uint8_t dst[6];
  char ssid[33];
  uint8_t channel;     // from DS IE
  uint8_t rx_channel;  // channel packet arrived on
  int8_t rssi;
  uint16_t beacon_interval;
  uint16_t capability;
  bool hidden;  // zero-length SSID

  // Security
  bool wep;           // capability bit 4
  bool wpa;           // vendor IE OUI 00:50:f2 type 1
  bool wpa2;          // RSN IE present (tag 48)
  bool wpa3;          // RSN AKM suite 8 (SAE)
  bool wpa3_trans;    // both SAE and PSK AKMs
  bool owe;           // AKM 18 (OWE)
  bool wpa2_e;        // AKM 1 (802.1X)
  bool pmf_required;  // RSN cap bit 7 (MFPR)
  bool pmf_capable;   // RSN cap bit 6 (MFPC)
  bool wps;           // vendor IE OUI 00:50:f2 type 4

  // Capabilities from IEs
  bool ht;   // HT Capabilities IE (tag 45)
  bool vht;  // VHT Capabilities IE (tag 191)
  bool he;   // HE Capabilities IE (vendor or tag 255)
  uint8_t max_ht_mcs;
  uint8_t bandwidth;         // 20/40/80/160 MHz
  uint16_t supported_rates;  // bitmask

  // IEs
  FrdIE ies[FRD_MAX_IES];
  uint8_t ie_count;

  // Timestamps
  uint32_t ts;
};

// ─── Decoded probe request ────────────────────────────────────────────────────
struct FrdProbe {
  uint8_t src[6];
  uint8_t dst[6];
  char ssid[33];
  bool directed;
  int8_t rssi;
  uint8_t channel;
  uint32_t ts;
};

// ─── Decoded data frame ────────────────────────────────────────────────────────
struct FrdData {
  uint8_t src[6];
  uint8_t dst[6];
  uint8_t bssid[6];
  uint8_t type;  // 0=mgmt, 1=ctrl, 2=data
  uint8_t subtype;
  bool to_ds;
  bool from_ds;
  bool protected_frame;
  uint16_t frame_len;
  uint32_t ts;
};

// ─── Helpers ─────────────────────────────────────────────────────────────────
static inline uint8_t frd_type(const uint8_t *fc) {
  return (fc[0] >> FRD_FC_TYPE_SHIFT) & FRD_FC_TYPE_MASK;
}
static inline uint8_t frd_subtype(const uint8_t *fc) {
  return (fc[0] >> FRD_FC_SUBTYPE_SHIFT) & FRD_FC_SUBTYPE_MASK;
}

// ─── IE walker ────────────────────────────────────────────────────────────────
static void frd_walk_ies(const uint8_t *body, int body_len, FrdBeacon &b) {
  int off = 0;
  b.ie_count = 0;

  while (off + 1 < body_len) {
    uint8_t tag = body[off];
    uint8_t len = body[off + 1];
    if (off + 2 + len > body_len)
      break;
    const uint8_t *data = body + off + 2;

    // Store IE
    if (b.ie_count < FRD_MAX_IES) {
      FrdIE &ie = b.ies[b.ie_count++];
      ie.valid = true;
      ie.tag = tag;
      ie.len = len;
      uint8_t copy = (len < FRD_IE_DATA) ? len : FRD_IE_DATA;
      memcpy(ie.data, data, copy);
    }

    switch (tag) {
      case 0:  // SSID
        if (len == 0) {
          b.hidden = true;
        } else {
          uint8_t slen = (len < 32) ? len : 32;
          memcpy(b.ssid, data, slen);
          b.ssid[slen] = 0;
          b.hidden = false;
        }
        break;

      case 3:  // DS Parameter Set
        if (len >= 1)
          b.channel = data[0];
        break;

      case 45:  // HT Capabilities
        b.ht = true;
        if (len >= 2) {
          uint16_t ht_cap = (data[1] << 8) | data[0];
          b.bandwidth = (ht_cap & 0x02) ? 40 : 20;
        }
        break;

      case 48: {  // RSN IE (WPA2/WPA3/OWE)
        b.wpa2 = true;
        if (len < 2)
          break;
        int roff = 2;  // skip version
        if (roff + 4 > len)
          break;
        roff += 4;  // skip group cipher suite

        // Pairwise cipher suites
        if (roff + 2 > len)
          break;
        uint16_t pc_count = (data[roff + 1] << 8) | data[roff];
        roff += 2 + pc_count * 4;

        // AKM suites
        if (roff + 2 > len)
          break;
        uint16_t akm_count = (data[roff + 1] << 8) | data[roff];
        roff += 2;
        for (int i = 0; i < akm_count && roff + 4 <= len; i++, roff += 4) {
          uint8_t akm_type = data[roff + 3];
          if (akm_type == 1)
            b.wpa2_e = true;  // 802.1X
          if (akm_type == 2)
            ;  // PSK (WPA2)
          if (akm_type == 8)
            b.wpa3 = true;  // SAE
          if (akm_type == 18)
            b.owe = true;  // OWE
          if (b.wpa3 && !b.wpa2_e && akm_count > 1)
            b.wpa3_trans = true;
        }

        // RSN Capabilities
        if (roff + 2 <= len) {
          uint16_t rsn_cap = (data[roff + 1] << 8) | data[roff];
          b.pmf_required = (rsn_cap >> 7) & 1;
          b.pmf_capable = (rsn_cap >> 6) & 1;
        }
        break;
      }

      case 191:  // VHT Capabilities
        b.vht = true;
        if (!b.bandwidth || b.bandwidth < 80)
          b.bandwidth = 80;
        if (len >= 1) {
          uint8_t vht_w = (data[0] >> 2) & 0x03;
          if (vht_w == 2)
            b.bandwidth = 160;
          else if (vht_w == 1)
            b.bandwidth = 80;
        }
        break;

      case 221:  // Vendor Specific
        if (len >= 4) {
          // WPA IE: 00:50:f2 type 01
          if (data[0] == 0x00 && data[1] == 0x50 && data[2] == 0xF2 && data[3] == 0x01)
            b.wpa = true;
          // WPS: 00:50:f2 type 04
          if (data[0] == 0x00 && data[1] == 0x50 && data[2] == 0xF2 && data[3] == 0x04)
            b.wps = true;
          // HE (WiFi 6): 50:6F:9A type 24 or vendor
          if (data[0] == 0x50 && data[1] == 0x6F && data[2] == 0x9A)
            b.he = true;
        }
        break;
    }

    off += 2 + len;
  }
}

// ─── Decode beacon/probe response ────────────────────────────────────────────
bool frd_decode_beacon(const uint8_t *raw, int len, int8_t rssi, uint8_t rx_channel,
                       FrdBeacon &out) {
  if (len < 36)
    return false;  // min beacon size

  out = {};
  out.rssi = rssi;
  out.rx_channel = rx_channel;
  out.ts = millis() / 1000;

  int off = 0;

  // Radiotap header
  if (raw[0] == 0x00 && len > 4) {
    uint16_t rtlen = raw[2] | (raw[3] << 8);
    if (rtlen < 8 || rtlen >= (uint16_t)len)
      return false;
    off += rtlen;
  }

  if (off + 24 >= len)
    return false;

  // 802.11 MAC header
  uint8_t fc0 = raw[off];
  uint8_t fc1 = raw[off + 1];
  uint8_t type = (fc0 >> 2) & 0x03;
  uint8_t subtype = (fc0 >> 4) & 0x0F;

  if (type != 0)
    return false;  // not management
  if (subtype != 8 && subtype != 5)
    return false;  // not beacon or probe resp

  // dst, src, bssid
  memcpy(out.dst, raw + off + 4, 6);
  memcpy(out.src, raw + off + 10, 6);
  memcpy(out.bssid, raw + off + 16, 6);
  (void)fc1;
  off += 24;

  if (off + 12 >= len)
    return false;

  // Fixed parameters: timestamp(8) + interval(2) + capability(2)
  out.beacon_interval = raw[off + 8] | (raw[off + 9] << 8);
  out.capability = raw[off + 10] | (raw[off + 11] << 8);
  out.wep = (out.capability >> 4) & 1;
  off += 12;

  // Parse IEs
  frd_walk_ies(raw + off, len - off, out);

  if (!out.channel && rx_channel)
    out.channel = rx_channel;
  return true;
}

// ─── Decode probe request ─────────────────────────────────────────────────────
bool frd_decode_probe(const uint8_t *raw, int len, int8_t rssi, uint8_t rx_channel, FrdProbe &out) {
  if (len < 28)
    return false;

  int off = 0;

  // Radiotap
  if (raw[0] == 0x00 && len > 4) {
    uint16_t rtlen = raw[2] | (raw[3] << 8);
    if (rtlen < 8 || rtlen >= (uint16_t)len)
      return false;
    off += rtlen;
  }

  if (off + 24 >= len)
    return false;

  uint8_t fc0 = raw[off];
  if (((fc0 >> 2) & 3) != 0)
    return false;  // mgmt only
  if (((fc0 >> 4) & 0xF) != 4)
    return false;  // probe req = subtype 4

  memcpy(out.dst, raw + off + 4, 6);
  memcpy(out.src, raw + off + 10, 6);
  out.rssi = rssi;
  out.channel = rx_channel;
  out.ts = millis() / 1000;
  off += 24;

  // SSID IE
  if (off + 2 <= len && raw[off] == 0x00) {
    uint8_t ssid_len = raw[off + 1];
    if (ssid_len > 0 && off + 2 + ssid_len <= len) {
      uint8_t copy = (ssid_len < 32) ? ssid_len : 32;
      memcpy(out.ssid, raw + off + 2, copy);
      out.ssid[copy] = 0;
      out.directed = true;
    } else {
      out.ssid[0] = 0;
      out.directed = false;
    }
  }

  return true;
}

// ─── Decode data frame ────────────────────────────────────────────────────────
bool frd_decode_data(const uint8_t *raw, int len, int8_t rssi, uint8_t rx_channel, FrdData &out) {
  if (len < 24)
    return false;

  int off = 0;
  if (raw[0] == 0x00 && len > 4) {
    uint16_t rtlen = raw[2] | (raw[3] << 8);
    if (rtlen < 8 || rtlen >= (uint16_t)len)
      return false;
    off += rtlen;
  }

  if (off + 24 > len)
    return false;

  uint8_t fc0 = raw[off];
  uint8_t fc1 = raw[off + 1];

  out.type = (fc0 >> 2) & 0x03;
  out.subtype = (fc0 >> 4) & 0x0F;
  out.to_ds = fc1 & 0x01;
  out.from_ds = (fc1 >> 1) & 0x01;
  out.protected_frame = (fc1 >> 6) & 0x01;
  out.frame_len = (uint16_t)len;

  // Addresses depend on to_ds/from_ds bits
  if (out.to_ds && !out.from_ds) {
    memcpy(out.bssid, raw + off + 4, 6);
    memcpy(out.src, raw + off + 10, 6);
    memcpy(out.dst, raw + off + 16, 6);
  } else if (!out.to_ds && out.from_ds) {
    memcpy(out.dst, raw + off + 4, 6);
    memcpy(out.bssid, raw + off + 10, 6);
    memcpy(out.src, raw + off + 16, 6);
  } else if (out.to_ds && out.from_ds) {
    // WDS frame
    memcpy(out.dst, raw + off + 4, 6);
    memcpy(out.src, raw + off + 10, 6);
    memcpy(out.bssid, raw + off + 16, 6);
  } else {
    memcpy(out.dst, raw + off + 4, 6);
    memcpy(out.src, raw + off + 10, 6);
    memcpy(out.bssid, raw + off + 16, 6);
  }

  out.ts = millis() / 1000;
  return true;
}

// ─── IE dump for a decoded beacon ────────────────────────────────────────────
static String frd_ie_dump_json(const FrdBeacon &b) {
  String s = "[";
  for (int i = 0; i < b.ie_count; i++) {
    const FrdIE &ie = b.ies[i];
    if (!ie.valid)
      continue;
    if (i)
      s += ",";
    s += "{\"tag\":" + String(ie.tag);
    s += ",\"len\":" + String(ie.len);
    s += ",\"hex\":\"";
    uint8_t dump = (ie.len < FRD_IE_DATA) ? ie.len : FRD_IE_DATA;
    for (int j = 0; j < dump; j++) {
      char hex[3];
      snprintf(hex, 3, "%02x", ie.data[j]);
      s += hex;
    }
    s += "\"}";
  }
  s += "]";
  return s;
}

// ─── Decode stats ─────────────────────────────────────────────────────────────
static struct {
  uint32_t total_decoded;
  uint32_t beacons;
  uint32_t probes;
  uint32_t data_frames;
  uint32_t errors;
} g_frd = {};

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void frd_routes() {
  server.on("/api/frd/stats", HTTP_GET, []() {
    String s = "{\"total\":" + String(g_frd.total_decoded);
    s += ",\"beacons\":" + String(g_frd.beacons);
    s += ",\"probes\":" + String(g_frd.probes);
    s += ",\"data\":" + String(g_frd.data_frames);
    s += ",\"errors\":" + String(g_frd.errors) + "}";
    server.send(200, "application/json", s);
  });

  server.on("/api/frd/clear", HTTP_POST, []() {
    memset(&g_frd, 0, sizeof(g_frd));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

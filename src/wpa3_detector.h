#pragma once
// wpa3_detector.h — WPA3/SAE detector, SAE frame parser, OWE detection,
// WPA3-Enterprise (Suite B) identifier. Prefix: w3d_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define W3D_MAX_ENTRIES   64
#define W3D_SSID_LEN      33

// AKM Suite selectors (OUI 00:0F:AC)
#define W3D_AKM_8021X        1   // WPA2-Enterprise
#define W3D_AKM_PSK          2   // WPA2-PSK
#define W3D_AKM_FT_8021X     3
#define W3D_AKM_FT_PSK       4
#define W3D_AKM_8021X_SHA256 5
#define W3D_AKM_PSK_SHA256   6
#define W3D_AKM_TDLS         7
#define W3D_AKM_SAE          8   // WPA3-Personal (SAE)
#define W3D_AKM_FT_SAE       9
#define W3D_AKM_OWE         18   // OWE (Enhanced Open)
#define W3D_AKM_SUITE_B     11   // WPA3-Enterprise Suite B 128
#define W3D_AKM_SUITE_B_192 12   // WPA3-Enterprise Suite B 192
#define W3D_AKM_FT_8021X_SHA384 13

// Cipher suites
#define W3D_CIPHER_NONE      0
#define W3D_CIPHER_WEP40     1
#define W3D_CIPHER_TKIP      2
#define W3D_CIPHER_CCMP128   4
#define W3D_CIPHER_WEP104    5
#define W3D_CIPHER_GCMP128   8
#define W3D_CIPHER_GCMP256   9
#define W3D_CIPHER_CCMP256  10

// RSN Capabilities bits
#define W3D_RSNC_PREAUTH        0x0001
#define W3D_RSNC_NO_PAIRWISE    0x0002
#define W3D_RSNC_PTKSA_REPLAY   0x000C
#define W3D_RSNC_GTKSA_REPLAY   0x0030
#define W3D_RSNC_MFP_REQUIRED   0x0040  // 802.11w required
#define W3D_RSNC_MFP_CAPABLE    0x0080  // 802.11w capable
#define W3D_RSNC_PEERKEY        0x0200
#define W3D_RSNC_SPP_AMSDU_CAP  0x0400
#define W3D_RSNC_SPP_AMSDU_REQ  0x0800
#define W3D_RSNC_PBAC           0x1000
#define W3D_RSNC_EXTENDED_KEY   0x2000
#define W3D_RSNC_OCVC           0x4000

// WPA3 mode classification
enum W3dMode {
  W3D_MODE_UNKNOWN = 0,
  W3D_MODE_WPA2_ONLY,
  W3D_MODE_WPA2_WPA3_TRANSITION,
  W3D_MODE_WPA3_PERSONAL,          // SAE only
  W3D_MODE_WPA3_ENTERPRISE,        // Suite B
  W3D_MODE_OWE,                    // Enhanced Open
  W3D_MODE_WPA3_ENTERPRISE_192,    // Suite B 192
};

// ─── Entry ───────────────────────────────────────────────────────────────────
struct W3dEntry {
  bool    active;
  uint8_t bssid[6];
  char    ssid[W3D_SSID_LEN];
  uint8_t channel;
  int8_t  rssi;

  W3dMode mode;
  uint8_t akm_suites[8];
  uint8_t akm_count;
  uint8_t pairwise_ciphers[4];
  uint8_t pairwise_count;
  uint8_t group_cipher;
  uint16_t rsn_caps;

  bool    has_sae;
  bool    has_owe;
  bool    has_suite_b;
  bool    has_mfp_required;
  bool    has_mfp_capable;
  bool    has_ft_sae;
  bool    transition_mode; // WPA2+WPA3 coexist
  bool    has_gcmp;         // GCMP-256 suite B indicator
  bool    pmf_enabled;      // Protected Management Frames

  uint32_t first_seen;
  uint32_t last_seen;
  uint32_t beacon_count;

  // SAE commit frames captured
  uint32_t sae_commits;
  uint32_t sae_confirms;
};

// ─── State ────────────────────────────────────────────────────────────────────
static struct {
  W3dEntry entries[W3D_MAX_ENTRIES];
  uint8_t  count;
  uint32_t total_frames;
  uint32_t sae_total;
  uint32_t owe_total;
} g_w3d = {};

// ─── Helpers ─────────────────────────────────────────────────────────────────
static bool w3d_mac_eq(const uint8_t *a, const uint8_t *b) { return memcmp(a,b,6)==0; }

static W3dEntry* w3d_find(const uint8_t *bssid) {
  for (int i = 0; i < W3D_MAX_ENTRIES; i++) {
    if (g_w3d.entries[i].active && w3d_mac_eq(g_w3d.entries[i].bssid, bssid))
      return &g_w3d.entries[i];
  }
  return nullptr;
}

static W3dEntry* w3d_alloc(const uint8_t *bssid) {
  for (int i = 0; i < W3D_MAX_ENTRIES; i++) {
    if (!g_w3d.entries[i].active) {
      W3dEntry &e = g_w3d.entries[i];
      memset(&e, 0, sizeof(e));
      memcpy(e.bssid, bssid, 6);
      e.active = true;
      e.first_seen = millis() / 1000;
      g_w3d.count++;
      return &e;
    }
  }
  return nullptr;
}

// ─── Parse RSN IE ─────────────────────────────────────────────────────────────
static void w3d_parse_rsn(W3dEntry &e, const uint8_t *data, uint8_t len) {
  if (len < 4) return;
  int pos = 0;
  uint16_t version = data[pos] | (data[pos+1] << 8); pos += 2;
  if (version != 1) return;
  if (pos + 4 > len) return;

  // Group cipher
  uint32_t group_oui = (data[pos]<<16)|(data[pos+1]<<8)|data[pos+2];
  if (group_oui == 0x000FAC) e.group_cipher = data[pos+3];
  pos += 4;

  // Pairwise
  if (pos + 2 > len) return;
  uint16_t pw_cnt = data[pos] | (data[pos+1]<<8); pos += 2;
  e.pairwise_count = min((int)pw_cnt, 4);
  for (int i = 0; i < e.pairwise_count && pos + 4 <= len; i++) {
    uint32_t oui = (data[pos]<<16)|(data[pos+1]<<8)|data[pos+2];
    if (oui == 0x000FAC) {
      e.pairwise_ciphers[i] = data[pos+3];
      if (data[pos+3] == W3D_CIPHER_GCMP256 || data[pos+3] == W3D_CIPHER_GCMP128) e.has_gcmp = true;
    }
    pos += 4;
  }

  // AKM
  if (pos + 2 > len) return;
  uint16_t akm_cnt = data[pos] | (data[pos+1]<<8); pos += 2;
  e.akm_count = min((int)akm_cnt, 8);
  bool any_sae = false, any_wpa2 = false, any_owe = false, any_suite_b = false;
  for (int i = 0; i < e.akm_count && pos + 4 <= len; i++) {
    uint32_t oui = (data[pos]<<16)|(data[pos+1]<<8)|data[pos+2];
    if (oui == 0x000FAC) {
      e.akm_suites[i] = data[pos+3];
      switch (data[pos+3]) {
        case W3D_AKM_SAE:          any_sae     = true; e.has_sae     = true; break;
        case W3D_AKM_FT_SAE:       any_sae     = true; e.has_ft_sae  = true; break;
        case W3D_AKM_OWE:          any_owe     = true; e.has_owe     = true; break;
        case W3D_AKM_SUITE_B:
        case W3D_AKM_SUITE_B_192:  any_suite_b = true; e.has_suite_b = true; break;
        case W3D_AKM_PSK:
        case W3D_AKM_FT_PSK:
        case W3D_AKM_PSK_SHA256:   any_wpa2    = true; break;
      }
    }
    pos += 4;
  }

  // RSN caps
  if (pos + 2 <= len) {
    e.rsn_caps = data[pos] | (data[pos+1]<<8);
    e.has_mfp_capable  = (e.rsn_caps & W3D_RSNC_MFP_CAPABLE)  != 0;
    e.has_mfp_required = (e.rsn_caps & W3D_RSNC_MFP_REQUIRED) != 0;
    e.pmf_enabled = e.has_mfp_capable;
  }

  // Classify mode
  if (any_suite_b) {
    e.mode = any_wpa2 ? W3D_MODE_WPA3_ENTERPRISE : W3D_MODE_WPA3_ENTERPRISE_192;
  } else if (any_owe) {
    e.mode = W3D_MODE_OWE;
  } else if (any_sae && any_wpa2) {
    e.mode = W3D_MODE_WPA2_WPA3_TRANSITION;
    e.transition_mode = true;
  } else if (any_sae) {
    e.mode = W3D_MODE_WPA3_PERSONAL;
  } else {
    e.mode = W3D_MODE_WPA2_ONLY;
  }
}

// ─── Ingest 802.11 frame ─────────────────────────────────────────────────────
void w3d_ingest(const uint8_t *raw, int len, const uint8_t *bssid,
                const char *ssid, uint8_t type, uint8_t sub,
                int8_t rssi, uint8_t channel) {
  g_w3d.total_frames++;

  // Beacon or Probe Response
  if (type == 0 && (sub == 8 || sub == 5)) {
    int ie_off = 8 + 24 + 12;
    if (ie_off >= len) return;

    // Walk IEs looking for RSN (tag 48)
    int i = ie_off;
    while (i + 2 <= len) {
      uint8_t tag = raw[i], tlen = raw[i+1];
      if (i + 2 + tlen > len) break;
      if (tag == 48) { // RSN IE
        W3dEntry *e = w3d_find(bssid);
        if (!e) e = w3d_alloc(bssid);
        if (!e) return;
        if (ssid) strncpy(e->ssid, ssid, W3D_SSID_LEN - 1);
        e->channel = channel;
        e->rssi = rssi;
        e->beacon_count++;
        e->last_seen = millis() / 1000;
        w3d_parse_rsn(*e, raw + i + 2, tlen);
        if (e->has_sae) g_w3d.sae_total++;
        if (e->has_owe) g_w3d.owe_total++;
      }
      i += 2 + tlen;
    }
  }

  // SAE Authentication frames (type=0 sub=11)
  if (type == 0 && sub == 11) {
    int body_off = 8 + 24;
    if (body_off + 6 > len) return;
    // Auth algo: bytes 0-1 (3 = SAE)
    uint16_t algo = raw[body_off] | (raw[body_off+1] << 8);
    if (algo == 3) {
      uint16_t seq = raw[body_off+2] | (raw[body_off+3] << 8);
      W3dEntry *e = w3d_find(bssid);
      if (!e) e = w3d_alloc(bssid);
      if (e) {
        if (seq == 1) e->sae_commits++;
        if (seq == 2) e->sae_confirms++;
      }
    }
  }
}

// ─── JSON ─────────────────────────────────────────────────────────────────────
static const char* w3d_mode_str(W3dMode m) {
  switch(m) {
    case W3D_MODE_WPA2_ONLY:               return "WPA2";
    case W3D_MODE_WPA2_WPA3_TRANSITION:    return "WPA2/WPA3";
    case W3D_MODE_WPA3_PERSONAL:           return "WPA3-SAE";
    case W3D_MODE_WPA3_ENTERPRISE:         return "WPA3-Enterprise";
    case W3D_MODE_OWE:                     return "OWE";
    case W3D_MODE_WPA3_ENTERPRISE_192:     return "WPA3-Enterprise-192";
    default:                               return "Unknown";
  }
}

static void w3d_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String w3d_json_status() {
  String s = "{\"count\":" + String(g_w3d.count);
  s += ",\"sae_total\":" + String(g_w3d.sae_total);
  s += ",\"owe_total\":" + String(g_w3d.owe_total);
  s += ",\"entries\":[";
  char mac[18];
  bool first = true;
  for (int i = 0; i < W3D_MAX_ENTRIES; i++) {
    W3dEntry &e = g_w3d.entries[i];
    if (!e.active) continue;
    if (!first) s += ",";
    first = false;
    w3d_mac_str(e.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(e.ssid) + "\"";
    s += ",\"ch\":" + String(e.channel);
    s += ",\"rssi\":" + String(e.rssi);
    s += ",\"mode\":\"" + String(w3d_mode_str(e.mode)) + "\"";
    s += ",\"has_sae\":" + String(e.has_sae?"true":"false");
    s += ",\"has_owe\":" + String(e.has_owe?"true":"false");
    s += ",\"has_suite_b\":" + String(e.has_suite_b?"true":"false");
    s += ",\"mfp_required\":" + String(e.has_mfp_required?"true":"false");
    s += ",\"mfp_capable\":" + String(e.has_mfp_capable?"true":"false");
    s += ",\"transition\":" + String(e.transition_mode?"true":"false");
    s += ",\"gcmp\":" + String(e.has_gcmp?"true":"false");
    s += ",\"sae_commits\":" + String(e.sae_commits);
    s += ",\"sae_confirms\":" + String(e.sae_confirms);
    s += ",\"last_seen\":" + String(e.last_seen);
    s += "}";
  }
  s += "]}";
  return s;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void w3d_routes() {
  server.on("/api/w3d/status", HTTP_GET, []() {
    server.send(200, "application/json", w3d_json_status());
  });

  server.on("/api/w3d/clear", HTTP_POST, []() {
    memset(&g_w3d, 0, sizeof(g_w3d));
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/w3d/wpa3_only", HTTP_GET, []() {
    String s = "{\"entries\":[";
    char mac[18];
    bool first = true;
    for (int i = 0; i < W3D_MAX_ENTRIES; i++) {
      W3dEntry &e = g_w3d.entries[i];
      if (!e.active || e.mode == W3D_MODE_WPA2_ONLY || e.mode == W3D_MODE_UNKNOWN) continue;
      if (!first) s += ",";
      first = false;
      w3d_mac_str(e.bssid, mac);
      s += "{\"bssid\":\"" + String(mac) + "\",\"ssid\":\"" + String(e.ssid) + "\",\"mode\":\"" + String(w3d_mode_str(e.mode)) + "\"}";
    }
    s += "]}";
    server.send(200, "application/json", s);
  });
}

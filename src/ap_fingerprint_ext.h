#pragma once
// ap_fingerprint_ext.h — Extended AP fingerprinting using multiple signals.
// Combines IE patterns, vendor OUI, capabilities to identify AP model/firmware.
// Prefix: apf_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

#define APF_MAX_APS 48
#define APF_MAX_SIG_LEN 24

// ─── Known device signatures ─────────────────────────────────────────────────
struct ApfSignature {
  const char *vendor;
  const char *model;
  uint8_t oui[3];  // OUI prefix
  bool has_wps;
  bool has_11r;             // FT
  bool has_11k;             // RRM
  bool has_11v;             // BSS Transition
  uint8_t country_code[2];  // 0 = any
  uint8_t ht_caps_mask;     // key HT capability bits (approximate)
};

// Minimal signature table (for demonstration; a real one would be much larger)
static const ApfSignature APF_SIGS[] PROGMEM = {
    {{0x24, 0xA4, 0x3C},
     {0x24, 0xA4, 0x3C},
     {0x24, 0xA4, 0x3C},
     false,
     true,
     true,
     true,
     {0, 0},
     0},
};

// ─── Per-AP fingerprint ───────────────────────────────────────────────────────
struct ApfAP {
  bool active;
  uint8_t bssid[6];
  char ssid[33];
  int8_t rssi;
  uint8_t channel;

  // Observed capabilities
  bool has_wps;
  bool has_11r;  // Fast BSS Transition
  bool has_11k;  // Radio Resource Measurement
  bool has_11v;  // BSS Transition Management
  bool has_ht;   // 802.11n
  bool has_vht;  // 802.11ac
  bool has_he;   // 802.11ax (HE)
  bool has_wpa3;
  bool has_pmf;

  // RSN capabilities
  uint8_t pairwise_cipher;
  uint8_t group_cipher;
  uint8_t akm_primary;

  // IE-based identifiers
  char country_code[3];
  uint8_t ht_channel_width;   // 0=20MHz, 1=40MHz
  uint8_t vht_channel_width;  // 0=80, 1=160, 2=80+80
  uint8_t max_tx_power;

  // Guess at model
  char vendor_guess[20];
  char model_guess[20];
  uint8_t confidence;  // 0-100

  // Timing
  uint32_t first_ts;
  uint32_t last_ts;
  uint32_t beacon_count;
};

static struct {
  ApfAP aps[APF_MAX_APS];
  uint8_t ap_count;

  uint32_t total_fingerprinted;
  uint32_t with_11r;
  uint32_t with_11k;
  uint32_t with_he;  // WiFi 6
} g_apf = {};

static bool apf_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static ApfAP *apf_find_or_alloc(const uint8_t *bssid) {
  for (int i = 0; i < APF_MAX_APS; i++)
    if (g_apf.aps[i].active && apf_mac_eq(g_apf.aps[i].bssid, bssid))
      return &g_apf.aps[i];
  for (int i = 0; i < APF_MAX_APS; i++) {
    if (!g_apf.aps[i].active) {
      ApfAP &a = g_apf.aps[i];
      memset(&a, 0, sizeof(a));
      a.active = true;
      memcpy(a.bssid, bssid, 6);
      a.first_ts = millis() / 1000;
      g_apf.ap_count++;
      return &a;
    }
  }
  uint32_t oldest = UINT32_MAX;
  int idx = 0;
  for (int i = 0; i < APF_MAX_APS; i++)
    if (g_apf.aps[i].last_ts < oldest) {
      oldest = g_apf.aps[i].last_ts;
      idx = i;
    }
  memset(&g_apf.aps[idx], 0, sizeof(g_apf.aps[0]));
  g_apf.aps[idx].active = true;
  memcpy(g_apf.aps[idx].bssid, bssid, 6);
  g_apf.aps[idx].first_ts = millis() / 1000;
  return &g_apf.aps[idx];
}

static void apf_guess_vendor(ApfAP &ap) {
  // Simple OUI-based vendor guess
  static const struct {
    uint8_t oui[3];
    const char *vendor;
  } V[] = {
      {{0x24, 0xA4, 0x3C}, "Ubiquiti"}, {{0x00, 0x27, 0x22}, "Ubiquiti"},
      {{0xFC, 0xEC, 0xDA}, "Ubiquiti"}, {{0x00, 0x1A, 0x73}, "Cisco"},
      {{0x24, 0xBE, 0x05}, "Cisco"},    {{0x1C, 0xC1, 0xDE}, "TP-Link"},
      {{0x50, 0xC7, 0xBF}, "TP-Link"},  {{0xE8, 0x65, 0xD4}, "Netgear"},
      {{0xC4, 0x04, 0x15}, "Netgear"},  {{0x14, 0x91, 0x82}, "ASUS"},
      {{0x04, 0xBF, 0xA2}, "ASUS"},     {{0x94, 0x65, 0x9C}, "Google"},
      {{0xF4, 0xF5, 0xDB}, "Google"},   {{0x68, 0xD7, 0x9A}, "Ubiquiti UniFi"},
      {{0x44, 0xD9, 0xE7}, "MikroTik"}, {{0xE4, 0x8D, 0x8C}, "MikroTik"},
  };
  for (unsigned i = 0; i < sizeof(V) / sizeof(V[0]); i++) {
    if (ap.bssid[0] == V[i].oui[0] && ap.bssid[1] == V[i].oui[1] && ap.bssid[2] == V[i].oui[2]) {
      strncpy(ap.vendor_guess, V[i].vendor, 19);
      ap.confidence = 60;
      return;
    }
  }
  strncpy(ap.vendor_guess, "Unknown", 19);
  ap.confidence = 0;
}

static void apf_boost_confidence(ApfAP &ap) {
  if (ap.has_11r && ap.has_11k && ap.has_11v)
    ap.confidence = (ap.confidence < 90) ? ap.confidence + 20 : 90;
  if (ap.has_he)
    ap.confidence = (ap.confidence < 85) ? ap.confidence + 10 : 85;
  if (ap.has_wpa3 && ap.has_pmf)
    ap.confidence = (ap.confidence < 80) ? ap.confidence + 5 : 80;
}

void apf_ingest(const uint8_t *bssid, const char *ssid, int8_t rssi, uint8_t channel, bool wps,
                bool ft, bool rrm, bool bss_tm, bool ht, bool vht, bool he, bool wpa3, bool pmf,
                const char *cc) {
  ApfAP *ap = apf_find_or_alloc(bssid);
  if (!ap)
    return;

  if (ssid && ssid[0])
    strncpy(ap->ssid, ssid, 32);
  ap->rssi = rssi;
  ap->channel = channel;
  ap->last_ts = millis() / 1000;
  ap->beacon_count++;

  ap->has_wps = wps;
  ap->has_11r = ft;
  ap->has_11k = rrm;
  ap->has_11v = bss_tm;
  ap->has_ht = ht;
  ap->has_vht = vht;
  ap->has_he = he;
  ap->has_wpa3 = wpa3;
  ap->has_pmf = pmf;

  if (cc && cc[0]) {
    ap->country_code[0] = cc[0];
    ap->country_code[1] = cc[1];
  }

  apf_guess_vendor(*ap);
  apf_boost_confidence(*ap);

  g_apf.total_fingerprinted++;
  if (ft)
    g_apf.with_11r++;
  if (rrm)
    g_apf.with_11k++;
  if (he)
    g_apf.with_he++;
}

static void apf_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String apf_json_all() {
  String s = "{\"ap_count\":" + String(g_apf.ap_count);
  s += ",\"with_11r\":" + String(g_apf.with_11r);
  s += ",\"with_11k\":" + String(g_apf.with_11k);
  s += ",\"with_he\":" + String(g_apf.with_he);
  s += ",\"aps\":[";
  char mac[18];
  bool first = true;
  for (int i = 0; i < APF_MAX_APS; i++) {
    ApfAP &ap = g_apf.aps[i];
    if (!ap.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    apf_mac_str(ap.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(ap.ssid) + "\"";
    s += ",\"ch\":" + String(ap.channel);
    s += ",\"rssi\":" + String(ap.rssi);
    s += ",\"vendor\":\"" + String(ap.vendor_guess) + "\"";
    s += ",\"conf\":" + String(ap.confidence);
    s += ",\"caps\":{";
    s += "\"ht\":" + String(ap.has_ht ? "true" : "false");
    s += ",\"vht\":" + String(ap.has_vht ? "true" : "false");
    s += ",\"he\":" + String(ap.has_he ? "true" : "false");
    s += ",\"wps\":" + String(ap.has_wps ? "true" : "false");
    s += ",\"ft\":" + String(ap.has_11r ? "true" : "false");
    s += ",\"rrm\":" + String(ap.has_11k ? "true" : "false");
    s += ",\"bss_tm\":" + String(ap.has_11v ? "true" : "false");
    s += ",\"wpa3\":" + String(ap.has_wpa3 ? "true" : "false");
    s += ",\"pmf\":" + String(ap.has_pmf ? "true" : "false");
    s += "},\"cc\":\"" + String(ap.country_code) + "\"";
    s += ",\"beacons\":" + String(ap.beacon_count);
    s += "}";
  }
  s += "]}";
  return s;
}

inline void apf_routes() {
  server.on("/api/apf/all", HTTP_GET,
            []() { server.send(200, "application/json", apf_json_all()); });
  server.on("/api/apf/clear", HTTP_POST, []() {
    memset(&g_apf, 0, sizeof(g_apf));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

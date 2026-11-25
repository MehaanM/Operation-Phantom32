#pragma once
// pmf_detector.h — 802.11w Protected Management Frame (PMF) detection.
// Parses RSN Information Element for PMF capability / requirement.
// Prefix: pmf_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define PMF_MAX_APS 64

// PMF modes
enum PmfMode : uint8_t {
  PMF_NONE = 0,      // RSN IE not present or no PMF capability
  PMF_CAPABLE = 1,   // MFPC=1, MFPR=0 (optional)
  PMF_REQUIRED = 2,  // MFPC=1, MFPR=1 (mandatory)
};

// 802.11w RSN Capabilities bit definitions
#define RSN_CAP_MFPC (1 << 7)  // Management Frame Protection Capable
#define RSN_CAP_MFPR (1 << 6)  // Management Frame Protection Required

struct PmfAP {
  bool active;
  uint8_t bssid[6];
  char ssid[33];
  uint8_t channel;
  int8_t rssi;

  PmfMode pmf_mode;
  bool wpa3;     // SAE detected in AKM suite list
  bool owe;      // OWE detected
  bool wpa2;     // WPA2-PSK / WPA2-Enterprise
  bool has_rsn;  // RSN IE present

  // RSN details
  uint8_t rsn_ver;
  uint8_t group_cipher;  // 0=WEP40, 2=TKIP, 4=CCMP, ...
  uint8_t pairwise_cipher;
  uint8_t akm_suite;  // 1=PSK, 2=802.1X, 8=SAE, 18=OWE

  uint32_t beacon_count;
  uint32_t first_ts;
  uint32_t last_ts;
};

static struct {
  PmfAP aps[PMF_MAX_APS];
  uint8_t ap_count;
  uint32_t total_beacons;
  uint32_t pmf_capable_count;
  uint32_t pmf_required_count;
  uint32_t wpa3_count;
  uint32_t unprotected_count;
} g_pmf = {};

static bool pmf_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static PmfAP *pmf_find_or_alloc(const uint8_t *bssid) {
  for (int i = 0; i < PMF_MAX_APS; i++)
    if (g_pmf.aps[i].active && pmf_mac_eq(g_pmf.aps[i].bssid, bssid))
      return &g_pmf.aps[i];
  for (int i = 0; i < PMF_MAX_APS; i++) {
    if (!g_pmf.aps[i].active) {
      PmfAP &a = g_pmf.aps[i];
      memset(&a, 0, sizeof(a));
      a.active = true;
      memcpy(a.bssid, bssid, 6);
      a.first_ts = millis() / 1000;
      g_pmf.ap_count++;
      return &a;
    }
  }
  uint32_t oldest = UINT32_MAX;
  int idx = 0;
  for (int i = 0; i < PMF_MAX_APS; i++)
    if (g_pmf.aps[i].last_ts < oldest) {
      oldest = g_pmf.aps[i].last_ts;
      idx = i;
    }
  memset(&g_pmf.aps[idx], 0, sizeof(g_pmf.aps[0]));
  g_pmf.aps[idx].active = true;
  memcpy(g_pmf.aps[idx].bssid, bssid, 6);
  g_pmf.aps[idx].first_ts = millis() / 1000;
  return &g_pmf.aps[idx];
}

// Parse RSN IE (tag 48) starting at the data byte (after tag+len)
static void pmf_parse_rsn(PmfAP &a, const uint8_t *rsn, uint8_t len) {
  if (len < 2)
    return;
  a.has_rsn = true;
  a.rsn_ver = rsn[0] | (rsn[1] << 8);
  uint8_t off = 2;

  // Group cipher suite (4 bytes)
  if (off + 4 > len)
    return;
  a.group_cipher = rsn[off + 3];  // cipher type
  off += 4;

  // Pairwise cipher suite count
  if (off + 2 > len)
    return;
  uint16_t pw_cnt = rsn[off] | (rsn[off + 1] << 8);
  off += 2;
  if (pw_cnt && off + pw_cnt * 4 <= len) {
    a.pairwise_cipher = rsn[off + 3];
    off += pw_cnt * 4;
  }

  // AKM suite count
  if (off + 2 > len)
    return;
  uint16_t akm_cnt = rsn[off] | (rsn[off + 1] << 8);
  off += 2;
  for (uint16_t k = 0; k < akm_cnt && off + 4 <= len; k++, off += 4) {
    uint8_t suite = rsn[off + 3];
    if (suite == 1 || suite == 2)
      a.wpa2 = true;
    if (suite == 8 || suite == 24) {
      a.wpa3 = true;
      a.akm_suite = suite;
    }
    if (suite == 18) {
      a.owe = true;
      a.akm_suite = suite;
    }
    if (!a.akm_suite)
      a.akm_suite = suite;
  }

  // RSN Capabilities (2 bytes)
  if (off + 2 > len)
    return;
  uint16_t caps = rsn[off] | (rsn[off + 1] << 8);
  bool mfpc = (caps >> 7) & 1;
  bool mfpr = (caps >> 6) & 1;
  if (mfpr)
    a.pmf_mode = PMF_REQUIRED;
  else if (mfpc)
    a.pmf_mode = PMF_CAPABLE;
  else
    a.pmf_mode = PMF_NONE;
}

void pmf_ingest_beacon(const uint8_t *bssid, const char *ssid, uint8_t channel, int8_t rssi,
                       const uint8_t *rsn_ie, uint8_t rsn_len) {
  g_pmf.total_beacons++;
  PmfAP *a = pmf_find_or_alloc(bssid);
  if (!a)
    return;

  if (ssid && ssid[0])
    strncpy(a->ssid, ssid, 32);
  a->channel = channel;
  if (rssi < 0)
    a->rssi = rssi;
  a->beacon_count++;
  a->last_ts = millis() / 1000;

  // Update RSN / PMF state
  bool prev_pmf_cap = (a->pmf_mode == PMF_CAPABLE);
  bool prev_pmf_req = (a->pmf_mode == PMF_REQUIRED);
  bool prev_wpa3 = a->wpa3;
  bool prev_unprotected = (!a->has_rsn);

  if (rsn_ie && rsn_len) {
    pmf_parse_rsn(*a, rsn_ie, rsn_len);
  } else {
    a->has_rsn = false;
    a->pmf_mode = PMF_NONE;
  }

  // Update global counters on first beacon
  if (a->beacon_count == 1) {
    if (a->pmf_mode == PMF_CAPABLE)
      g_pmf.pmf_capable_count++;
    if (a->pmf_mode == PMF_REQUIRED)
      g_pmf.pmf_required_count++;
    if (a->wpa3)
      g_pmf.wpa3_count++;
    if (!a->has_rsn)
      g_pmf.unprotected_count++;
  }
}

static const char *pmf_cipher_name(uint8_t c) {
  switch (c) {
    case 0:
      return "WEP-40";
    case 1:
      return "WEP-40";
    case 2:
      return "TKIP";
    case 4:
      return "CCMP";
    case 5:
      return "WEP-104";
    case 6:
      return "BIP-CMAC";
    case 8:
      return "GCMP";
    default:
      return "Unknown";
  }
}

static const char *pmf_akm_name(uint8_t a) {
  switch (a) {
    case 1:
      return "PSK";
    case 2:
      return "802.1X";
    case 3:
      return "FT-802.1X";
    case 4:
      return "FT-PSK";
    case 5:
      return "802.1X-SHA256";
    case 6:
      return "PSK-SHA256";
    case 8:
      return "SAE";
    case 11:
      return "802.1X-Suite-B";
    case 18:
      return "OWE";
    case 24:
      return "SAE-EXT";
    default:
      return "Other";
  }
}

static void pmf_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String pmf_json_all() {
  String s = "{\"ap_count\":" + String(g_pmf.ap_count);
  s += ",\"total_beacons\":" + String(g_pmf.total_beacons);
  s += ",\"pmf_capable\":" + String(g_pmf.pmf_capable_count);
  s += ",\"pmf_required\":" + String(g_pmf.pmf_required_count);
  s += ",\"wpa3_count\":" + String(g_pmf.wpa3_count);
  s += ",\"unprotected\":" + String(g_pmf.unprotected_count);
  s += ",\"aps\":[";
  char mac[18];
  bool first = true;
  for (int i = 0; i < PMF_MAX_APS; i++) {
    PmfAP &a = g_pmf.aps[i];
    if (!a.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    pmf_mac_str(a.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(a.ssid) + "\"";
    s += ",\"ch\":" + String(a.channel);
    s += ",\"rssi\":" + String(a.rssi);
    s += ",\"pmf_mode\":" + String(a.pmf_mode);
    s += ",\"wpa3\":" + String(a.wpa3 ? "true" : "false");
    s += ",\"owe\":" + String(a.owe ? "true" : "false");
    s += ",\"wpa2\":" + String(a.wpa2 ? "true" : "false");
    s += ",\"has_rsn\":" + String(a.has_rsn ? "true" : "false");
    s += ",\"group_cipher\":\"" + String(pmf_cipher_name(a.group_cipher)) + "\"";
    s += ",\"pairwise_cipher\":\"" + String(pmf_cipher_name(a.pairwise_cipher)) + "\"";
    s += ",\"akm\":\"" + String(pmf_akm_name(a.akm_suite)) + "\"";
    s += ",\"beacons\":" + String(a.beacon_count);
    s += "}";
  }
  s += "]}";
  return s;
}

inline void pmf_routes() {
  server.on("/api/pmf/all", HTTP_GET,
            []() { server.send(200, "application/json", pmf_json_all()); });
  server.on("/api/pmf/clear", HTTP_POST, []() {
    memset(&g_pmf, 0, sizeof(g_pmf));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

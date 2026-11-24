#pragma once
// ap_fingerprint.h — Advanced 802.11 AP fingerprinting.
// Identifies AP firmware/model from beacon IE patterns, supported rates,
// HT/VHT/HE capabilities, and vendor-specific information elements.
// Prefix: apf_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define APF_MAX_APS     48
#define APF_HIST_LEN    30

// Fingerprint confidence
enum ApfConf : uint8_t {
  APF_CONF_GUESS = 0,
  APF_CONF_LIKELY = 1,
  APF_CONF_PROBABLE = 2,
  APF_CONF_HIGH = 3
};

// Chip/platform categories
enum ApfChip : uint8_t {
  APF_CHIP_UNKNOWN = 0,
  APF_CHIP_BROADCOM = 1,
  APF_CHIP_QUALCOMM = 2,
  APF_CHIP_REALTEK  = 3,
  APF_CHIP_MEDIATEK = 4,
  APF_CHIP_MARVELL  = 5,
  APF_CHIP_INTEL    = 6,
  APF_CHIP_ESPRESSIF = 7
};

static const char* APF_CHIP_NAMES[] = {
  "Unknown", "Broadcom", "Qualcomm/Atheros", "Realtek",
  "MediaTek", "Marvell", "Intel", "Espressif"
};

// Known vendor-specific OUI prefixes in beacon IEs
static const struct {
  uint8_t oui[3];
  const char *vendor;
  ApfChip chip;
} APF_IE_VENDORS[] = {
  {{0x00,0x50,0xF2}, "Microsoft WMM",   APF_CHIP_UNKNOWN},  // WMM/WPA
  {{0x00,0x0B,0x86}, "Aruba",           APF_CHIP_QUALCOMM},
  {{0x00,0x17,0xF2}, "Apple",           APF_CHIP_BROADCOM},
  {{0x00,0x11,0x22}, "Marvell IE",      APF_CHIP_MARVELL},
  {{0x00,0x50,0x43}, "3Com",            APF_CHIP_BROADCOM},
  {{0x28,0xCF,0xDA}, "OpenWRT",         APF_CHIP_UNKNOWN},
  {{0x00,0x0E,0x8E}, "Cisco Meraki",    APF_CHIP_QUALCOMM},
  {{0x00,0x0C,0xE7}, "Cisco CCKM",     APF_CHIP_QUALCOMM},
  {{0x00,0x50,0xF2}, "Microsoft WPS",   APF_CHIP_UNKNOWN},
  {{0x00,0x00,0x00}, nullptr,           APF_CHIP_UNKNOWN}
};

// AP fingerprint capability flags
struct ApfCapFlags {
  bool ht;             // 802.11n HT
  bool vht;            // 802.11ac VHT
  bool he;             // 802.11ax HE (Wi-Fi 6)
  bool eht;            // 802.11be EHT (Wi-Fi 7)
  bool wpa2;
  bool wpa3;
  bool pmf;            // Protected Management Frames
  bool mfp_required;   // PMF required (not just capable)
  bool fast_bss_trans; // 802.11r FT
  bool rrm;            // 802.11k Radio Resource Management
  bool btm;            // 802.11v BSS Transition Management
  bool fils;           // Fast Initial Link Setup
  bool ocv;            // Operating Channel Validation
  bool twt;            // Target Wake Time (HE)
  bool mu_mimo;        // MU-MIMO
  bool su_beamformer;  // SU beamforming
  bool tdls;           // Tunnel Direct Link Setup
  bool cf_pollable;
  bool short_slot;
  bool short_preamble;
  bool dsss_ofdm;
};

struct ApfAP {
  bool    active;
  uint8_t bssid[6];
  char    ssid[33];
  uint8_t ch;
  int8_t  rssi;
  uint8_t oui[3];

  // Fingerprint result
  char    vendor[32];
  char    model[48];
  char    firmware[32];
  ApfChip chip;
  ApfConf confidence;

  // Capability profile
  ApfCapFlags caps;
  uint8_t  max_spatial_streams;  // from HT/VHT/HE caps
  uint16_t max_rate_mbps;
  uint8_t  supported_rates[16];  // in 500Kbps units
  uint8_t  rate_count;

  // IE presence flags (vendor-specific OUIs seen)
  uint8_t  vs_ie_count;
  uint8_t  vs_ie_ouis[8][3];

  // Operational parameters
  uint16_t beacon_interval;
  uint8_t  dtim_period;
  uint16_t capability_info;

  // Country/regulatory
  char     country_code[3];
  bool     has_country_ie;

  // Activity
  uint32_t beacons_seen;
  uint32_t last_ts;
  uint32_t first_ts;
};

static struct {
  ApfAP   aps[APF_MAX_APS];
  uint8_t count;
} g_apf = {};

static bool apf_mac_eq(const uint8_t *a, const uint8_t *b) { return memcmp(a,b,6)==0; }

static ApfAP* apf_find(const uint8_t *bssid) {
  for (int i = 0; i < APF_MAX_APS; i++)
    if (g_apf.aps[i].active && apf_mac_eq(g_apf.aps[i].bssid, bssid))
      return &g_apf.aps[i];
  return nullptr;
}

static ApfAP* apf_alloc(const uint8_t *bssid) {
  for (int i = 0; i < APF_MAX_APS; i++) {
    if (!g_apf.aps[i].active) {
      ApfAP &a = g_apf.aps[i];
      memset(&a, 0, sizeof(a));
      a.active = true;
      memcpy(a.bssid, bssid, 6);
      memcpy(a.oui, bssid, 3);
      a.first_ts = millis()/1000;
      g_apf.count++;
      return &a;
    }
  }
  uint32_t oldest = UINT32_MAX; int idx = 0;
  for (int i = 0; i < APF_MAX_APS; i++)
    if (g_apf.aps[i].last_ts < oldest) { oldest = g_apf.aps[i].last_ts; idx = i; }
  memset(&g_apf.aps[idx], 0, sizeof(ApfAP));
  g_apf.aps[idx].active = true;
  memcpy(g_apf.aps[idx].bssid, bssid, 6);
  memcpy(g_apf.aps[idx].oui, bssid, 3);
  g_apf.aps[idx].first_ts = millis()/1000;
  return &g_apf.aps[idx];
}

// Heuristic model detection based on capabilities + OUI
static void apf_fingerprint(ApfAP &a) {
  // Try to match vendor from OUI
  for (int i = 0; APF_IE_VENDORS[i].vendor; i++) {
    for (int j = 0; j < a.vs_ie_count; j++) {
      if (memcmp(APF_IE_VENDORS[i].oui, a.vs_ie_ouis[j], 3) == 0) {
        const char *vn = APF_IE_VENDORS[i].vendor;
        if (vn && !strstr(a.vendor, vn)) {
          if (a.vendor[0]) strncat(a.vendor, "/", 2);
          strncat(a.vendor, vn, 31 - strlen(a.vendor));
          if (a.chip == APF_CHIP_UNKNOWN) a.chip = APF_IE_VENDORS[i].chip;
        }
      }
    }
  }

  // Capability-based model hinting
  if (a.caps.he) {
    if (a.max_spatial_streams >= 4) strncpy(a.model, "Wi-Fi 6 Enterprise", 47);
    else strncpy(a.model, "Wi-Fi 6 (802.11ax)", 47);
    a.confidence = APF_CONF_HIGH;
  } else if (a.caps.vht) {
    strncpy(a.model, "Wi-Fi 5 (802.11ac)", 47);
    a.confidence = APF_CONF_LIKELY;
  } else if (a.caps.ht) {
    strncpy(a.model, "Wi-Fi 4 (802.11n)", 47);
    a.confidence = APF_CONF_GUESS;
  }

  // Security hinting
  if (a.caps.wpa3) {
    if (a.caps.pmf) a.confidence = (ApfConf)(a.confidence < APF_CONF_LIKELY ? APF_CONF_LIKELY : a.confidence);
    strncat(a.model, " WPA3", 47 - strlen(a.model));
  }
  if (a.caps.rrm && a.caps.btm && a.caps.fast_bss_trans)
    a.confidence = (ApfConf)(a.confidence < APF_CONF_PROBABLE ? APF_CONF_PROBABLE : a.confidence);
}

void apf_ingest(const uint8_t *bssid, const char *ssid,
                uint8_t ch, int8_t rssi,
                bool ht, bool vht, bool he, bool eht,
                bool wpa2, bool wpa3, bool pmf, bool pmf_req,
                bool ft, bool rrm, bool btm, bool tdls,
                bool mu_mimo, bool su_bf,
                uint8_t streams, uint16_t max_rate,
                const uint8_t *rates, uint8_t rate_cnt,
                uint16_t bcn_int, uint8_t dtim,
                uint16_t cap_info, const char *country) {
  ApfAP *a = apf_find(bssid);
  if (!a) a = apf_alloc(bssid);
  if (!a) return;

  if (ssid && ssid[0]) strncpy(a->ssid, ssid, 32);
  a->ch = ch; a->rssi = rssi;
  a->caps.ht = ht; a->caps.vht = vht; a->caps.he = he; a->caps.eht = eht;
  a->caps.wpa2 = wpa2; a->caps.wpa3 = wpa3;
  a->caps.pmf = pmf; a->caps.mfp_required = pmf_req;
  a->caps.fast_bss_trans = ft; a->caps.rrm = rrm; a->caps.btm = btm; a->caps.tdls = tdls;
  a->caps.mu_mimo = mu_mimo; a->caps.su_beamformer = su_bf;
  a->max_spatial_streams = streams;
  a->max_rate_mbps = max_rate;
  a->beacon_interval = bcn_int;
  a->dtim_period = dtim;
  a->capability_info = cap_info;
  if (country && country[0]) { a->country_code[0] = country[0]; a->country_code[1] = country[1]; a->has_country_ie = true; }
  if (rates && rate_cnt) {
    uint8_t cnt = rate_cnt < 16 ? rate_cnt : 16;
    memcpy(a->supported_rates, rates, cnt);
    a->rate_count = cnt;
  }
  a->beacons_seen++;
  a->last_ts = millis()/1000;

  if (a->beacons_seen == 1 || a->beacons_seen % 20 == 0)
    apf_fingerprint(*a);
}

void apf_add_vs_ie(const uint8_t *bssid, const uint8_t *oui) {
  ApfAP *a = apf_find(bssid);
  if (!a) return;
  for (int i = 0; i < a->vs_ie_count; i++)
    if (memcmp(a->vs_ie_ouis[i], oui, 3) == 0) return;
  if (a->vs_ie_count < 8) {
    memcpy(a->vs_ie_ouis[a->vs_ie_count++], oui, 3);
    apf_fingerprint(*a);
  }
}

static const char* apf_conf_str(ApfConf c) {
  switch (c) {
    case APF_CONF_GUESS:    return "Guess";
    case APF_CONF_LIKELY:   return "Likely";
    case APF_CONF_PROBABLE: return "Probable";
    case APF_CONF_HIGH:     return "High";
  }
  return "Unknown";
}

static void apf_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String apf_cap_str(const ApfCapFlags &c) {
  String s = "[";
  if (c.ht)  s += "\"HT\",";  if (c.vht) s += "\"VHT\",";
  if (c.he)  s += "\"HE\",";  if (c.eht) s += "\"EHT\",";
  if (c.wpa3) s += "\"WPA3\","; else if (c.wpa2) s += "\"WPA2\",";
  if (c.pmf) s += "\"PMF\","; if (c.rrm) s += "\"RRM\",";
  if (c.btm) s += "\"BTM\","; if (c.fast_bss_trans) s += "\"FT\",";
  if (c.mu_mimo) s += "\"MU-MIMO\","; if (c.su_beamformer) s += "\"SU-BF\",";
  if (c.tdls) s += "\"TDLS\",";
  if (s.endsWith(",")) s.remove(s.length()-1);
  s += "]";
  return s;
}

static String apf_json_all() {
  String s = "{\"count\":" + String(g_apf.count) + ",\"aps\":[";
  char mac[18]; bool first = true;
  for (int i = 0; i < APF_MAX_APS; i++) {
    ApfAP &a = g_apf.aps[i];
    if (!a.active) continue;
    if (!first) s += ","; first = false;
    apf_mac_str(a.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(a.ssid) + "\"";
    s += ",\"ch\":" + String(a.ch);
    s += ",\"rssi\":" + String(a.rssi);
    s += ",\"vendor\":\"" + String(a.vendor) + "\"";
    s += ",\"model\":\"" + String(a.model) + "\"";
    s += ",\"chip\":\"" + String(APF_CHIP_NAMES[a.chip]) + "\"";
    s += ",\"confidence\":\"" + String(apf_conf_str(a.confidence)) + "\"";
    s += ",\"streams\":" + String(a.max_spatial_streams);
    s += ",\"max_rate\":" + String(a.max_rate_mbps);
    s += ",\"bcn_interval\":" + String(a.beacon_interval);
    s += ",\"dtim\":" + String(a.dtim_period);
    s += ",\"country\":\"" + String(a.country_code[0] ? a.country_code : "—") + "\"";
    s += ",\"beacons\":" + String(a.beacons_seen);
    s += ",\"caps\":" + apf_cap_str(a.caps) + "}";
  }
  s += "]}";
  return s;
}

inline void apf_routes() {
  server.on("/api/apf/all", HTTP_GET, []() {
    server.send(200, "application/json", apf_json_all());
  });
  server.on("/api/apf/clear", HTTP_POST, []() {
    memset(&g_apf, 0, sizeof(g_apf));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

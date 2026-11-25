#pragma once
// scan_result_cache.h — Active and passive scan result cache with aging.
// Maintains a deduplicated view of observed BSSIDs with signal history,
// security profile, supported rates, and channel assignment.
// Prefix: src_

#include <Arduino.h>
#include <WebServer.h>
#include "psram_attr.h"

extern WebServer server;

#define SRC_MAX_ENTRIES  80
#define SRC_HIST_LEN     30
#define SRC_MAX_RATES     8

// Supported rate flags
#define SRC_RATE_1M   (1 << 0)
#define SRC_RATE_2M   (1 << 1)
#define SRC_RATE_5M   (1 << 2)
#define SRC_RATE_6M   (1 << 3)
#define SRC_RATE_9M   (1 << 4)
#define SRC_RATE_11M  (1 << 5)
#define SRC_RATE_12M  (1 << 6)
#define SRC_RATE_18M  (1 << 7)
#define SRC_RATE_24M  (1 << 8)
#define SRC_RATE_36M  (1 << 9)
#define SRC_RATE_48M  (1 << 10)
#define SRC_RATE_54M  (1 << 11)

// Security flags
#define SRC_SEC_OPEN    0x00
#define SRC_SEC_WEP     0x01
#define SRC_SEC_WPA     0x02
#define SRC_SEC_WPA2    0x04
#define SRC_SEC_WPA3    0x08
#define SRC_SEC_OWE     0x10
#define SRC_SEC_PMF_REQ 0x20
#define SRC_SEC_PMF_CAP 0x40

// Band
enum SrcBand : uint8_t {
  SRC_BAND_2G = 0,
  SRC_BAND_5G = 1,
  SRC_BAND_6G = 2,
};

// Network type
enum SrcNetType : uint8_t {
  SRC_NET_INFRA  = 0,   // ESS infrastructure
  SRC_NET_IBSS   = 1,   // IBSS ad-hoc
  SRC_NET_MESH   = 2,   // 802.11s mesh
};

struct SrcEntry {
  bool    active;
  uint8_t bssid[6];
  char    ssid[33];
  bool    hidden;
  uint8_t ch;
  SrcBand band;
  SrcNetType net_type;

  // Signal
  int8_t  rssi;
  int8_t  rssi_min;
  int8_t  rssi_max;
  int16_t rssi_sum;     // for running avg
  uint16_t rssi_count;
  int8_t   rssi_hist[SRC_HIST_LEN];
  uint8_t  hist_head;

  // Security
  uint8_t  sec_flags;
  char     akm[12];     // AKM suite: "PSK", "SAE", "OWE", "EAP", etc.
  char     cipher[12];  // pairwise cipher: "CCMP", "TKIP", "GCMP", etc.
  uint8_t  rsn_ver;     // RSN IE version

  // Capabilities
  uint16_t rate_flags;    // supported rates bitmask
  uint8_t  max_rate_mbps; // highest rate seen
  uint16_t beacon_interval_tu;
  uint8_t  dtim_period;
  bool     has_ht;        // 802.11n
  bool     has_vht;       // 802.11ac
  bool     has_he;        // 802.11ax (Wi-Fi 6)
  uint8_t  spatial_streams;
  uint16_t channel_width_mhz;

  // Probe request count (how many clients probed for this SSID)
  uint16_t probe_req_count;
  uint16_t probe_resp_count;

  // Aging
  uint32_t first_seen_ts;
  uint32_t last_seen_ts;
  uint32_t beacon_count;
  uint16_t age_s;        // seconds since last seen

  // Flags
  bool     is_captive;   // Captive portal detected
  bool     is_hidden;    // SSID not broadcast
};

typedef struct {
  SrcEntry entries[SRC_MAX_ENTRIES];
  uint8_t  count;
  uint32_t total_beacons_seen;
  uint32_t last_age_tick;
} G_SrcState;
#ifdef BOARD_HAS_PSRAM
static G_SrcState* _g_src_raw;
#define g_src (*_g_src_raw)
#else
static G_SrcState g_src;
#endif

static bool src_mac_eq(const uint8_t *a, const uint8_t *b) { return memcmp(a,b,6)==0; }

static SrcEntry* src_find(const uint8_t *bssid) {
  for (int i = 0; i < SRC_MAX_ENTRIES; i++)
    if (g_src.entries[i].active && src_mac_eq(g_src.entries[i].bssid, bssid))
      return &g_src.entries[i];
  return nullptr;
}

static SrcEntry* src_alloc(const uint8_t *bssid) {
  for (int i = 0; i < SRC_MAX_ENTRIES; i++) {
    if (!g_src.entries[i].active) {
      SrcEntry &e = g_src.entries[i];
      memset(&e, 0, sizeof(e));
      e.active = true;
      memcpy(e.bssid, bssid, 6);
      e.rssi_min = 0;
      e.rssi_max = -100;
      e.first_seen_ts = millis()/1000;
      g_src.count++;
      return &e;
    }
  }
  // LRU eviction: evict oldest
  uint32_t oldest = UINT32_MAX; int idx = 0;
  for (int i = 0; i < SRC_MAX_ENTRIES; i++)
    if (g_src.entries[i].last_seen_ts < oldest) { oldest = g_src.entries[i].last_seen_ts; idx = i; }
  memset(&g_src.entries[idx], 0, sizeof(SrcEntry));
  g_src.entries[idx].active = true;
  memcpy(g_src.entries[idx].bssid, bssid, 6);
  g_src.entries[idx].rssi_min = 0;
  g_src.entries[idx].rssi_max = -100;
  g_src.entries[idx].first_seen_ts = millis()/1000;
  return &g_src.entries[idx];
}

static uint8_t src_max_rate(uint16_t flags) {
  if (flags & SRC_RATE_54M) return 54;
  if (flags & SRC_RATE_48M) return 48;
  if (flags & SRC_RATE_36M) return 36;
  if (flags & SRC_RATE_24M) return 24;
  if (flags & SRC_RATE_18M) return 18;
  if (flags & SRC_RATE_12M) return 12;
  if (flags & SRC_RATE_11M) return 11;
  if (flags & SRC_RATE_9M)  return 9;
  if (flags & SRC_RATE_6M)  return 6;
  if (flags & SRC_RATE_5M)  return 5;
  if (flags & SRC_RATE_2M)  return 2;
  if (flags & SRC_RATE_1M)  return 1;
  return 0;
}

static void src_update_hist(SrcEntry &e, int8_t rssi) {
  e.rssi_hist[e.hist_head % SRC_HIST_LEN] = rssi;
  e.hist_head = (e.hist_head + 1) % SRC_HIST_LEN;
}

void src_ingest_beacon(const uint8_t *bssid, const char *ssid, uint8_t ch, int8_t rssi,
                        uint8_t sec_flags, const char *akm, const char *cipher,
                        uint8_t rsn_ver, bool hidden,
                        uint16_t rate_flags, uint16_t beacon_interval,
                        uint8_t dtim_period, bool has_ht, bool has_vht, bool has_he,
                        uint8_t ss, uint16_t bw, bool captive) {
  SrcEntry *e = src_find(bssid);
  if (!e) e = src_alloc(bssid);
  if (!e) return;

  if (ssid && ssid[0] && !e->ssid[0]) strncpy(e->ssid, ssid, 32);
  e->hidden = hidden;
  e->ch = ch;
  e->band = (ch > 14) ? SRC_BAND_5G : SRC_BAND_2G;
  e->rssi = rssi;
  if (rssi > e->rssi_max) e->rssi_max = rssi;
  if (rssi < e->rssi_min || e->rssi_count == 0) e->rssi_min = rssi;
  e->rssi_sum += rssi; e->rssi_count++;
  src_update_hist(*e, rssi);

  e->sec_flags = sec_flags;
  if (akm)    strncpy(e->akm, akm, 11);
  if (cipher) strncpy(e->cipher, cipher, 11);
  e->rsn_ver = rsn_ver;
  if (rate_flags) {
    e->rate_flags = rate_flags;
    e->max_rate_mbps = src_max_rate(rate_flags);
  }
  if (beacon_interval) e->beacon_interval_tu = beacon_interval;
  if (dtim_period)     e->dtim_period = dtim_period;
  if (has_ht)  e->has_ht  = true;
  if (has_vht) e->has_vht = true;
  if (has_he)  e->has_he  = true;
  if (ss > e->spatial_streams) e->spatial_streams = ss;
  if (bw > e->channel_width_mhz) e->channel_width_mhz = bw;
  e->is_captive = captive;
  e->beacon_count++;
  g_src.total_beacons_seen++;
  e->last_seen_ts = millis()/1000;
}

void src_ingest_probe(const uint8_t *bssid, bool is_response) {
  SrcEntry *e = src_find(bssid);
  if (!e) return;
  if (is_response) e->probe_resp_count++;
  else             e->probe_req_count++;
}

void src_age_entries() {
  uint32_t now_s = millis()/1000;
  if (now_s == g_src.last_age_tick) return;
  g_src.last_age_tick = now_s;
  for (int i = 0; i < SRC_MAX_ENTRIES; i++) {
    SrcEntry &e = g_src.entries[i];
    if (!e.active) continue;
    e.age_s = (uint16_t)(now_s - e.last_seen_ts);
    if (e.age_s > 300) {   // expire after 5 minutes of silence
      e.active = false;
      if (g_src.count > 0) g_src.count--;
    }
  }
}

static void src_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String src_hist_json(const int8_t *h, int len, int head) {
  String s = "[";
  for (int i = 0; i < len; i++) {
    if (i) s += ",";
    s += String((int)h[(head+i)%len]);
  }
  return s + "]";
}

static const char* src_wifi_gen(SrcEntry &e) {
  if (e.has_he)  return "Wi-Fi 6";
  if (e.has_vht) return "Wi-Fi 5";
  if (e.has_ht)  return "Wi-Fi 4";
  return "Legacy";
}

static String src_sec_str(uint8_t f) {
  if (f == SRC_SEC_OPEN) return "Open";
  if (f & SRC_SEC_WPA3)  return "WPA3";
  if (f & SRC_SEC_WPA2)  return "WPA2";
  if (f & SRC_SEC_WPA)   return "WPA";
  if (f & SRC_SEC_WEP)   return "WEP";
  if (f & SRC_SEC_OWE)   return "OWE";
  return "Unknown";
}

static String src_json_all() {
  src_age_entries();
  char mac[18];
  String s = "{\"count\":" + String(g_src.count);
  s += ",\"total_beacons\":" + String(g_src.total_beacons_seen);
  s += ",\"entries\":[";
  bool first = true;
  for (int i = 0; i < SRC_MAX_ENTRIES; i++) {
    SrcEntry &e = g_src.entries[i];
    if (!e.active) continue;
    if (!first) s += ","; first = false;
    src_mac_str(e.bssid, mac);
    int avg = e.rssi_count ? (int)(e.rssi_sum / e.rssi_count) : e.rssi;
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(e.ssid) + "\"";
    s += ",\"hidden\":" + String(e.hidden ? "true" : "false");
    s += ",\"ch\":" + String(e.ch);
    s += ",\"rssi\":" + String(e.rssi);
    s += ",\"rssi_avg\":" + String(avg);
    s += ",\"rssi_min\":" + String(e.rssi_min);
    s += ",\"rssi_max\":" + String(e.rssi_max);
    s += ",\"sec\":\"" + src_sec_str(e.sec_flags) + "\"";
    s += ",\"akm\":\"" + String(e.akm) + "\"";
    s += ",\"cipher\":\"" + String(e.cipher) + "\"";
    s += ",\"gen\":\"" + String(src_wifi_gen(e)) + "\"";
    s += ",\"max_rate\":" + String(e.max_rate_mbps);
    s += ",\"bw\":" + String(e.channel_width_mhz);
    s += ",\"ss\":" + String(e.spatial_streams);
    s += ",\"pmf\":" + String((e.sec_flags & SRC_SEC_PMF_REQ) ? "\"Required\"" : (e.sec_flags & SRC_SEC_PMF_CAP) ? "\"Capable\"" : "\"None\"");
    s += ",\"beacons\":" + String(e.beacon_count);
    s += ",\"age\":" + String(e.age_s);
    s += ",\"captive\":" + String(e.is_captive ? "true" : "false");
    s += ",\"rssi_hist\":" + src_hist_json(e.rssi_hist, SRC_HIST_LEN, e.hist_head) + "}";
  }
  s += "]}";
  return s;
}

inline void src_routes() {
  server.on("/api/src/all", HTTP_GET, []() {
    server.send(200, "application/json", src_json_all());
  });
  server.on("/api/src/clear", HTTP_POST, []() {
    memset(&g_src, 0, sizeof(g_src));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

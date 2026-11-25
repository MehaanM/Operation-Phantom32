#pragma once
// coex_detector.h — 2.4/5GHz coexistence and interference detector.
// Tracks 20/40MHz coexistence events, channel switching, and
// interference from non-Wi-Fi sources (Bluetooth, Zigbee, etc.).
// Prefix: cxd_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define CXD_MAX_APS 32
#define CXD_MAX_CH_SWITCH 48  // channel switch event log
#define CXD_HIST_LEN 60

// Interference type classification
enum CxdIntfType : uint8_t {
  CXD_INTF_UNKNOWN = 0,
  CXD_INTF_BT = 1,         // Bluetooth (2.4GHz AFH patterns)
  CXD_INTF_ZIGBEE = 2,     // Zigbee (802.15.4)
  CXD_INTF_MICROWAVE = 3,  // Microwave oven
  CXD_INTF_DECT = 4,       // DECT phone
  CXD_INTF_RADAR = 5,      // 5GHz radar (DFS trigger)
  CXD_INTF_ADJACENT = 6,   // Adjacent channel Wi-Fi
};

// 20/40 MHz coexistence event
enum CxdCoexEvent : uint8_t {
  CXD_COEX_NONE = 0,
  CXD_COEX_40_TO_20 = 1,    // AP dropped from 40MHz to 20MHz
  CXD_COEX_20_TO_40 = 2,    // AP upgraded to 40MHz
  CXD_COEX_INTOLERANT = 3,  // 40MHz intolerant bit set by neighbor
  CXD_COEX_OBSS = 4,        // Overlapping BSS detection
};

// Channel switch announcement record
struct CxdChSwitch {
  uint8_t bssid[6];
  char ssid[33];
  uint8_t from_ch;
  uint8_t to_ch;
  uint8_t mode;   // 0=immediate, 1=no TX before switch
  uint8_t count;  // TBTTs remaining
  CxdIntfType trigger;
  uint32_t ts;
};

struct CxdAP {
  bool active;
  uint8_t bssid[6];
  char ssid[33];
  uint8_t ch;
  int8_t rssi;

  // Bandwidth state
  uint8_t bw_mhz;  // 20/40/80/160
  uint8_t bw_mhz_prev;
  uint16_t bw_changes;

  // Coexistence flags
  bool intolerant_40;  // AP/neighbor flagged 40MHz intolerant
  bool obss_active;    // OBSS scan active
  bool dfs_channel;    // On DFS channel
  bool has_csa;        // Sent Channel Switch Announcement

  // Coexistence events
  uint32_t coex_events;
  uint32_t csa_count;

  // Channel switch history index (into global table)
  uint8_t csa_ref;

  // Interference score (0-100)
  uint8_t intf_score;
  CxdIntfType intf_type;

  // History
  uint8_t intf_hist[CXD_HIST_LEN];
  uint8_t hist_head;
  uint32_t last_tick_s;
  uint32_t cur_intf_events;

  uint32_t first_ts;
  uint32_t last_ts;
};

static struct {
  CxdAP aps[CXD_MAX_APS];
  uint8_t count;
  CxdChSwitch switches[CXD_MAX_CH_SWITCH];
  uint8_t sw_head;
  uint8_t sw_count;
  uint32_t total_csa;
  uint32_t total_coex;
} g_cxd = {};

static bool cxd_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static CxdAP *cxd_find(const uint8_t *bssid) {
  for (int i = 0; i < CXD_MAX_APS; i++)
    if (g_cxd.aps[i].active && cxd_mac_eq(g_cxd.aps[i].bssid, bssid))
      return &g_cxd.aps[i];
  return nullptr;
}

static CxdAP *cxd_alloc(const uint8_t *bssid) {
  for (int i = 0; i < CXD_MAX_APS; i++) {
    if (!g_cxd.aps[i].active) {
      CxdAP &a = g_cxd.aps[i];
      memset(&a, 0, sizeof(a));
      a.active = true;
      memcpy(a.bssid, bssid, 6);
      a.first_ts = millis() / 1000;
      g_cxd.count++;
      return &a;
    }
  }
  uint32_t oldest = UINT32_MAX;
  int idx = 0;
  for (int i = 0; i < CXD_MAX_APS; i++)
    if (g_cxd.aps[i].last_ts < oldest) {
      oldest = g_cxd.aps[i].last_ts;
      idx = i;
    }
  memset(&g_cxd.aps[idx], 0, sizeof(CxdAP));
  g_cxd.aps[idx].active = true;
  memcpy(g_cxd.aps[idx].bssid, bssid, 6);
  g_cxd.aps[idx].first_ts = millis() / 1000;
  return &g_cxd.aps[idx];
}

static void cxd_tick(CxdAP &a) {
  uint32_t now_s = millis() / 1000;
  if (now_s == a.last_tick_s)
    return;
  a.intf_hist[a.hist_head % CXD_HIST_LEN] = a.intf_score;
  a.hist_head = (a.hist_head + 1) % CXD_HIST_LEN;
  a.last_tick_s = now_s;
  a.cur_intf_events = 0;
}

void cxd_ingest_beacon(const uint8_t *bssid, const char *ssid, uint8_t ch, int8_t rssi,
                       uint8_t bw_mhz, bool intolerant_40, bool obss, bool dfs,
                       bool ht_protection_active) {
  CxdAP *a = cxd_find(bssid);
  if (!a)
    a = cxd_alloc(bssid);
  if (!a)
    return;

  cxd_tick(*a);
  a->ch = ch;
  a->rssi = rssi;
  if (ssid)
    strncpy(a->ssid, ssid, 32);

  // Bandwidth change detection
  if (a->bw_mhz > 0 && bw_mhz != a->bw_mhz) {
    a->bw_mhz_prev = a->bw_mhz;
    a->bw_changes++;
    a->coex_events++;
    g_cxd.total_coex++;
  }
  a->bw_mhz = bw_mhz;

  a->intolerant_40 = intolerant_40;
  a->obss_active = obss;
  a->dfs_channel = dfs;

  // Update interference score heuristic
  uint8_t score = 0;
  if (intolerant_40)
    score += 20;
  if (obss)
    score += 25;
  if (dfs)
    score += 10;
  if (ht_protection_active)
    score += 20;
  if (a->bw_changes > 2)
    score += 15;
  a->intf_score = score > 100 ? 100 : score;
  a->last_ts = millis() / 1000;
}

void cxd_ingest_csa(const uint8_t *bssid, const char *ssid, uint8_t from_ch, uint8_t to_ch,
                    uint8_t mode, uint8_t count, CxdIntfType trigger) {
  CxdAP *a = cxd_find(bssid);
  if (!a)
    a = cxd_alloc(bssid);
  if (!a)
    return;

  a->has_csa = true;
  a->csa_count++;
  g_cxd.total_csa++;

  CxdChSwitch &sw = g_cxd.switches[g_cxd.sw_head % CXD_MAX_CH_SWITCH];
  memset(&sw, 0, sizeof(sw));
  memcpy(sw.bssid, bssid, 6);
  if (ssid)
    strncpy(sw.ssid, ssid, 32);
  sw.from_ch = from_ch;
  sw.to_ch = to_ch;
  sw.mode = mode;
  sw.count = count;
  sw.trigger = trigger;
  sw.ts = millis() / 1000;
  g_cxd.sw_head = (g_cxd.sw_head + 1) % CXD_MAX_CH_SWITCH;
  if (g_cxd.sw_count < CXD_MAX_CH_SWITCH)
    g_cxd.sw_count++;

  a->last_ts = millis() / 1000;
}

static const char *cxd_intf_str(CxdIntfType t) {
  switch (t) {
    case CXD_INTF_BT:
      return "Bluetooth";
    case CXD_INTF_ZIGBEE:
      return "Zigbee";
    case CXD_INTF_MICROWAVE:
      return "Microwave";
    case CXD_INTF_DECT:
      return "DECT";
    case CXD_INTF_RADAR:
      return "Radar";
    case CXD_INTF_ADJACENT:
      return "Adjacent";
    default:
      return "Unknown";
  }
}

static void cxd_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String cxd_hist_json(const uint8_t *hist, int len, int head) {
  String s = "[";
  for (int i = 0; i < len; i++) {
    if (i)
      s += ",";
    s += String(hist[(head + i) % len]);
  }
  return s + "]";
}

static String cxd_json_all() {
  String s = "{\"count\":" + String(g_cxd.count);
  s += ",\"total_csa\":" + String(g_cxd.total_csa);
  s += ",\"total_coex\":" + String(g_cxd.total_coex);

  // Channel switch log
  s += ",\"switches\":[";
  char mac[18];
  bool first = true;
  for (int i = 0; i < g_cxd.sw_count; i++) {
    int idx = (g_cxd.sw_head + ROM_MAX_EVENTS - 1 - i) % CXD_MAX_CH_SWITCH;
    // just iterate last N
    int idx2 = (g_cxd.sw_head + g_cxd.sw_count - 1 - i) % CXD_MAX_CH_SWITCH;
    CxdChSwitch &sw = g_cxd.switches[idx2];
    if (!sw.ts)
      continue;
    if (!first)
      s += ",";
    first = false;
    cxd_mac_str(sw.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(sw.ssid) + "\"";
    s += ",\"from\":" + String(sw.from_ch);
    s += ",\"to\":" + String(sw.to_ch);
    s += ",\"mode\":" + String(sw.mode);
    s += ",\"count\":" + String(sw.count);
    s += ",\"trigger\":\"" + String(cxd_intf_str(sw.trigger)) + "\"";
    s += ",\"ts\":" + String(sw.ts) + "}";
  }

  s += "],\"aps\":[";
  first = true;
  for (int i = 0; i < CXD_MAX_APS; i++) {
    CxdAP &a = g_cxd.aps[i];
    if (!a.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    cxd_mac_str(a.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(a.ssid) + "\"";
    s += ",\"ch\":" + String(a.ch);
    s += ",\"rssi\":" + String(a.rssi);
    s += ",\"bw\":" + String(a.bw_mhz);
    s += ",\"bw_prev\":" + String(a.bw_mhz_prev);
    s += ",\"bw_changes\":" + String(a.bw_changes);
    s += ",\"intol40\":" + String(a.intolerant_40 ? "true" : "false");
    s += ",\"obss\":" + String(a.obss_active ? "true" : "false");
    s += ",\"dfs\":" + String(a.dfs_channel ? "true" : "false");
    s += ",\"csa_count\":" + String(a.csa_count);
    s += ",\"coex_events\":" + String(a.coex_events);
    s += ",\"intf_score\":" + String(a.intf_score);
    s += ",\"intf_type\":\"" + String(cxd_intf_str(a.intf_type)) + "\"";
    s += ",\"intf_hist\":" + cxd_hist_json(a.intf_hist, CXD_HIST_LEN, a.hist_head) + "}";
  }
  s += "]}";
  return s;
}

inline void cxd_routes() {
  server.on("/api/cxd/all", HTTP_GET,
            []() { server.send(200, "application/json", cxd_json_all()); });
  server.on("/api/cxd/clear", HTTP_POST, []() {
    memset(&g_cxd, 0, sizeof(g_cxd));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

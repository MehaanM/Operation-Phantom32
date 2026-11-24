#pragma once
// bss_load_history.h — BSS Load IE history tracker.
// Tracks the 802.11e BSS Load element (channel utilization, station count,
// available admission capacity) over time and detects congestion patterns.
// Prefix: bsl_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define BSL_MAX_APS     32
#define BSL_HIST_LEN    30   // history samples per AP
#define BSL_MAX_EVENTS  40

// Congestion state derived from channel utilization
enum BslCongState : uint8_t {
  BSL_CONG_IDLE    = 0,  // util < 25%
  BSL_CONG_LIGHT   = 1,  // util 25-50%
  BSL_CONG_MODERATE= 2,  // util 50-75%
  BSL_CONG_HEAVY   = 3,  // util > 75%
};

static const char* bsl_cong_str(BslCongState s) {
  switch(s) {
    case BSL_CONG_IDLE:     return "Idle";
    case BSL_CONG_LIGHT:    return "Light";
    case BSL_CONG_MODERATE: return "Moderate";
    case BSL_CONG_HEAVY:    return "Heavy";
  }
  return "?";
}

// Channel utilization history sample
struct BslSample {
  uint8_t  util_pct;      // 0-100%
  uint8_t  sta_count;     // connected stations
  uint16_t avail_cap;     // available admission capacity (units of 32 µs / 1s)
  uint32_t ts;
};

struct BslAP {
  bool    active;
  uint8_t bssid[6];
  char    ssid[33];
  uint8_t ch;
  int8_t  rssi;

  // Current BSS Load
  uint8_t  cur_util_pct;
  uint8_t  cur_sta_count;
  uint16_t cur_avail_cap;
  BslCongState cur_state;

  // History
  BslSample hist[BSL_HIST_LEN];
  uint8_t   hist_head;
  uint8_t   hist_count;

  // Statistics
  uint8_t  util_max;
  uint8_t  util_min;
  uint16_t util_sum;     // for running mean (wraps after ~4000 samples)
  uint16_t sample_count;
  uint8_t  util_mean;
  uint8_t  sta_max;

  // Congestion events
  uint16_t heavy_events;
  uint32_t last_heavy_ts;
  uint16_t beacon_count;

  uint32_t first_ts;
  uint32_t last_ts;
};

struct BslEvent {
  bool    active;
  char    ssid[33];
  uint8_t bssid[6];
  uint8_t util_pct;
  uint8_t sta_count;
  BslCongState state;
  uint32_t ts;
};

static struct {
  BslAP    aps[BSL_MAX_APS];
  BslEvent events[BSL_MAX_EVENTS];
  uint8_t  ap_count;
  uint8_t  ev_head;
  uint16_t total_heavy_events;
  uint8_t  busiest_ch;
  uint8_t  busiest_util;
} g_bsl = {};

static bool bsl_mac_eq(const uint8_t *a, const uint8_t *b) { return memcmp(a,b,6)==0; }

static BslCongState bsl_classify(uint8_t util) {
  if (util < 25)  return BSL_CONG_IDLE;
  if (util < 50)  return BSL_CONG_LIGHT;
  if (util < 75)  return BSL_CONG_MODERATE;
  return BSL_CONG_HEAVY;
}

static BslAP* bsl_find(const uint8_t *bssid) {
  for (int i = 0; i < BSL_MAX_APS; i++)
    if (g_bsl.aps[i].active && bsl_mac_eq(g_bsl.aps[i].bssid, bssid))
      return &g_bsl.aps[i];
  return nullptr;
}

static BslAP* bsl_alloc(const uint8_t *bssid) {
  for (int i = 0; i < BSL_MAX_APS; i++) {
    if (!g_bsl.aps[i].active) {
      BslAP &a = g_bsl.aps[i];
      memset(&a, 0, sizeof(a));
      a.active = true;
      memcpy(a.bssid, bssid, 6);
      a.util_min = 100;
      a.first_ts = millis()/1000;
      g_bsl.ap_count++;
      return &a;
    }
  }
  uint32_t oldest = UINT32_MAX; int idx = 0;
  for (int i = 0; i < BSL_MAX_APS; i++)
    if (g_bsl.aps[i].last_ts < oldest) { oldest = g_bsl.aps[i].last_ts; idx = i; }
  memset(&g_bsl.aps[idx], 0, sizeof(BslAP));
  g_bsl.aps[idx].active = true;
  memcpy(g_bsl.aps[idx].bssid, bssid, 6);
  g_bsl.aps[idx].util_min = 100;
  g_bsl.aps[idx].first_ts = millis()/1000;
  return &g_bsl.aps[idx];
}

static void bsl_add_event(BslAP &a) {
  BslEvent &ev = g_bsl.events[g_bsl.ev_head % BSL_MAX_EVENTS];
  memset(&ev, 0, sizeof(ev));
  ev.active = true;
  strncpy(ev.ssid, a.ssid, 32);
  memcpy(ev.bssid, a.bssid, 6);
  ev.util_pct  = a.cur_util_pct;
  ev.sta_count = a.cur_sta_count;
  ev.state = a.cur_state;
  ev.ts = millis()/1000;
  g_bsl.ev_head = (g_bsl.ev_head + 1) % BSL_MAX_EVENTS;
}

void bsl_ingest(const uint8_t *bssid, const char *ssid, uint8_t ch, int8_t rssi,
                uint8_t util_255, uint8_t sta_count, uint16_t avail_cap) {
  BslAP *a = bsl_find(bssid);
  if (!a) a = bsl_alloc(bssid);
  if (!a) return;

  if (ssid) strncpy(a->ssid, ssid, 32);
  a->ch = ch; a->rssi = rssi;

  // Convert raw util (0-255) to percent
  uint8_t util_pct = (uint8_t)((uint16_t)util_255 * 100 / 255);

  a->cur_util_pct  = util_pct;
  a->cur_sta_count = sta_count;
  a->cur_avail_cap = avail_cap;
  a->cur_state = bsl_classify(util_pct);

  // Record sample
  BslSample &s = a->hist[a->hist_head % BSL_HIST_LEN];
  s.util_pct  = util_pct;
  s.sta_count = sta_count;
  s.avail_cap = avail_cap;
  s.ts = millis()/1000;
  a->hist_head = (a->hist_head + 1) % BSL_HIST_LEN;
  if (a->hist_count < BSL_HIST_LEN) a->hist_count++;

  // Update stats
  if (util_pct > a->util_max) a->util_max = util_pct;
  if (util_pct < a->util_min) a->util_min = util_pct;
  a->util_sum += util_pct; a->sample_count++;
  if (a->sample_count > 0) a->util_mean = (uint8_t)(a->util_sum / a->sample_count);
  if (sta_count > a->sta_max) a->sta_max = sta_count;
  a->beacon_count++;
  a->last_ts = millis()/1000;

  // Heavy congestion event
  if (a->cur_state == BSL_CONG_HEAVY) {
    if (a->heavy_events == 0 || (millis()/1000 - a->last_heavy_ts > 10)) {
      a->heavy_events++;
      a->last_heavy_ts = millis()/1000;
      g_bsl.total_heavy_events++;
      bsl_add_event(*a);
    }
  }

  // Track busiest channel
  if (util_pct > g_bsl.busiest_util) {
    g_bsl.busiest_util = util_pct;
    g_bsl.busiest_ch = ch;
  }
}

static void bsl_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String bsl_json_all() {
  char mac[18];
  String s = "{\"ap_count\":" + String(g_bsl.ap_count);
  s += ",\"total_heavy\":" + String(g_bsl.total_heavy_events);
  s += ",\"busiest_ch\":" + String(g_bsl.busiest_ch);
  s += ",\"busiest_util\":" + String(g_bsl.busiest_util);
  s += ",\"aps\":[";
  bool first = true;
  for (int i = 0; i < BSL_MAX_APS; i++) {
    BslAP &a = g_bsl.aps[i];
    if (!a.active) continue;
    if (!first) s += ","; first = false;
    bsl_mac_str(a.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\",\"ssid\":\"" + String(a.ssid) + "\"";
    s += ",\"ch\":" + String(a.ch) + ",\"rssi\":" + String(a.rssi);
    s += ",\"util\":" + String(a.cur_util_pct);
    s += ",\"sta\":" + String(a.cur_sta_count);
    s += ",\"avail\":" + String(a.cur_avail_cap);
    s += ",\"state\":\"" + String(bsl_cong_str(a.cur_state)) + "\"";
    s += ",\"util_max\":" + String(a.util_max);
    s += ",\"util_min\":" + String(a.util_min);
    s += ",\"util_mean\":" + String(a.util_mean);
    s += ",\"sta_max\":" + String(a.sta_max);
    s += ",\"heavy_events\":" + String(a.heavy_events);
    s += ",\"hist\":[";
    for (int j = 0; j < a.hist_count; j++) {
      if (j) s += ",";
      int idx = (a.hist_head - a.hist_count + j + BSL_HIST_LEN) % BSL_HIST_LEN;
      s += "{\"u\":" + String(a.hist[idx].util_pct);
      s += ",\"s\":" + String(a.hist[idx].sta_count) + "}";
    }
    s += "]}";
  }
  s += "],\"events\":[";
  first = true;
  for (int i = 0; i < BSL_MAX_EVENTS; i++) {
    BslEvent &ev = g_bsl.events[i];
    if (!ev.active) continue;
    if (!first) s += ","; first = false;
    bsl_mac_str(ev.bssid, mac);
    s += "{\"ssid\":\"" + String(ev.ssid) + "\",\"bssid\":\"" + String(mac) + "\"";
    s += ",\"util\":" + String(ev.util_pct);
    s += ",\"sta\":" + String(ev.sta_count);
    s += ",\"state\":\"" + String(bsl_cong_str(ev.state)) + "\"";
    s += ",\"ts\":" + String(ev.ts) + "}";
  }
  s += "]}";
  return s;
}

inline void bsl_routes() {
  server.on("/api/bsl/all", HTTP_GET, []() {
    server.send(200, "application/json", bsl_json_all());
  });
  server.on("/api/bsl/clear", HTTP_POST, []() {
    memset(&g_bsl, 0, sizeof(g_bsl));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

#pragma once
// load_balance_monitor.h — AP load balancing and BSS load monitor.
// Tracks BSS Load IE (channel utilization, station count, available admission
// capacity) and 802.11v BSS Transition Management load-balancing events.
// Prefix: lbm_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define LBM_MAX_APS     32
#define LBM_HIST_LEN    30

// BSS Load IE fields
struct LbmBssLoad {
  uint16_t station_count;      // Number of associated stations
  uint8_t  channel_util;       // 0-255 (0=0%, 255=100%)
  uint16_t avail_admission;    // Available admission capacity (32µs units)
};

// BSS Transition Management (11v) event
enum LbmBtmType : uint8_t {
  LBM_BTM_REQ    = 0,  // AP sent BTM Request
  LBM_BTM_RSP    = 1,  // Client sent BTM Response
  LBM_BTM_QUERY  = 2,  // Client sent BTM Query
  LBM_BTM_NOTIFY = 3,  // AP sent BTM Notification
};

struct LbmBtmEvent {
  uint8_t  client_mac[6];
  uint8_t  ap_bssid[6];
  LbmBtmType type;
  uint8_t  status;       // 0=Accept, 1=Reject (various reasons), etc.
  uint8_t  mode;         // mode flags (bit0=preferred list, bit1=abridged, etc.)
  uint16_t disassoc_timer; // TBTTs
  uint8_t  candidate_bssid[6]; // suggested target AP
  int8_t   candidate_rssi;
  uint32_t ts;
};

#define LBM_MAX_BTM_EVENTS 32

struct LbmAP {
  bool    active;
  uint8_t bssid[6];
  char    ssid[33];
  uint8_t ch;
  int8_t  rssi;

  // Current BSS Load
  LbmBssLoad current_load;
  LbmBssLoad peak_load;

  // Load history
  uint8_t  util_hist[LBM_HIST_LEN];     // channel utilization per tick
  uint16_t sta_hist[LBM_HIST_LEN];      // station count per tick
  uint8_t  hist_head;
  uint32_t last_tick_s;

  // Load statistics
  uint32_t load_updates;
  uint32_t btm_requests_sent;    // BTM requests this AP sent to clients
  uint32_t btm_accepts;          // clients that accepted BTM
  uint32_t btm_rejects;          // clients that rejected BTM

  // Congestion flags
  bool    congested;             // util > 75%
  bool    saturated;             // avail_admission == 0
  uint32_t congestion_events;

  // Rate class limits
  bool    has_bss_load_ie;

  uint32_t first_ts;
  uint32_t last_ts;
};

static struct {
  LbmAP   aps[LBM_MAX_APS];
  uint8_t count;
  LbmBtmEvent events[LBM_MAX_BTM_EVENTS];
  uint8_t  evt_head;
  uint8_t  evt_count;
  uint32_t total_btm;
} g_lbm = {};

static bool lbm_mac_eq(const uint8_t *a, const uint8_t *b) { return memcmp(a,b,6)==0; }

static LbmAP* lbm_find(const uint8_t *bssid) {
  for (int i = 0; i < LBM_MAX_APS; i++)
    if (g_lbm.aps[i].active && lbm_mac_eq(g_lbm.aps[i].bssid, bssid))
      return &g_lbm.aps[i];
  return nullptr;
}

static LbmAP* lbm_alloc(const uint8_t *bssid) {
  for (int i = 0; i < LBM_MAX_APS; i++) {
    if (!g_lbm.aps[i].active) {
      LbmAP &a = g_lbm.aps[i];
      memset(&a, 0, sizeof(a));
      a.active = true;
      memcpy(a.bssid, bssid, 6);
      a.first_ts = millis()/1000;
      g_lbm.count++;
      return &a;
    }
  }
  uint32_t oldest = UINT32_MAX; int idx = 0;
  for (int i = 0; i < LBM_MAX_APS; i++)
    if (g_lbm.aps[i].last_ts < oldest) { oldest = g_lbm.aps[i].last_ts; idx = i; }
  memset(&g_lbm.aps[idx], 0, sizeof(LbmAP));
  g_lbm.aps[idx].active = true;
  memcpy(g_lbm.aps[idx].bssid, bssid, 6);
  g_lbm.aps[idx].first_ts = millis()/1000;
  return &g_lbm.aps[idx];
}

static void lbm_tick(LbmAP &a) {
  uint32_t now_s = millis()/1000;
  if (now_s == a.last_tick_s) return;
  int h = a.hist_head % LBM_HIST_LEN;
  a.util_hist[h] = a.current_load.channel_util;
  a.sta_hist[h]  = a.current_load.station_count;
  a.hist_head = (a.hist_head + 1) % LBM_HIST_LEN;
  a.last_tick_s = now_s;
}

void lbm_ingest_bss_load(const uint8_t *bssid, const char *ssid, uint8_t ch, int8_t rssi,
                          uint16_t station_count, uint8_t channel_util,
                          uint16_t avail_admission) {
  LbmAP *a = lbm_find(bssid);
  if (!a) a = lbm_alloc(bssid);
  if (!a) return;

  lbm_tick(*a);
  if (ssid) strncpy(a->ssid, ssid, 32);
  a->ch = ch; a->rssi = rssi;
  a->has_bss_load_ie = true;
  a->load_updates++;

  a->current_load.station_count   = station_count;
  a->current_load.channel_util    = channel_util;
  a->current_load.avail_admission = avail_admission;

  // Update peaks
  if (station_count > a->peak_load.station_count)
    a->peak_load.station_count = station_count;
  if (channel_util > a->peak_load.channel_util)
    a->peak_load.channel_util = channel_util;

  // Check congestion
  bool was_congested = a->congested;
  a->congested  = (channel_util > 192);  // > 75%
  a->saturated  = (avail_admission == 0);
  if (a->congested && !was_congested) a->congestion_events++;

  a->last_ts = millis()/1000;
}

void lbm_ingest_btm(const uint8_t *client, const uint8_t *ap,
                    LbmBtmType type, uint8_t status, uint8_t mode,
                    uint16_t disassoc_timer, const uint8_t *candidate,
                    int8_t candidate_rssi) {
  LbmAP *a = lbm_find(ap);
  if (!a) a = lbm_alloc(ap);
  if (!a) return;

  LbmBtmEvent &e = g_lbm.events[g_lbm.evt_head % LBM_MAX_BTM_EVENTS];
  memset(&e, 0, sizeof(e));
  memcpy(e.client_mac, client, 6);
  memcpy(e.ap_bssid, ap, 6);
  e.type = type;
  e.status = status;
  e.mode = mode;
  e.disassoc_timer = disassoc_timer;
  if (candidate) memcpy(e.candidate_bssid, candidate, 6);
  e.candidate_rssi = candidate_rssi;
  e.ts = millis()/1000;
  g_lbm.evt_head = (g_lbm.evt_head + 1) % LBM_MAX_BTM_EVENTS;
  if (g_lbm.evt_count < LBM_MAX_BTM_EVENTS) g_lbm.evt_count++;
  g_lbm.total_btm++;

  if (type == LBM_BTM_REQ) a->btm_requests_sent++;
  if (type == LBM_BTM_RSP) {
    if (status == 0) a->btm_accepts++;
    else             a->btm_rejects++;
  }
  a->last_ts = millis()/1000;
}

static void lbm_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String lbm_hist_u8(const uint8_t *h, int len, int head) {
  String s = "[";
  for (int i = 0; i < len; i++) {
    if (i) s += ",";
    s += String(h[(head+i)%len]);
  }
  return s + "]";
}

static String lbm_hist_u16(const uint16_t *h, int len, int head) {
  String s = "[";
  for (int i = 0; i < len; i++) {
    if (i) s += ",";
    s += String(h[(head+i)%len]);
  }
  return s + "]";
}

static const char* lbm_btm_type_str(LbmBtmType t) {
  switch (t) {
    case LBM_BTM_REQ:    return "Request";
    case LBM_BTM_RSP:    return "Response";
    case LBM_BTM_QUERY:  return "Query";
    case LBM_BTM_NOTIFY: return "Notify";
  }
  return "Unknown";
}

static String lbm_json_all() {
  String s = "{\"count\":" + String(g_lbm.count);
  s += ",\"total_btm\":" + String(g_lbm.total_btm);

  // Events
  s += ",\"events\":[";
  char mac[18], ap[18], cand[18]; bool first = true;
  for (int i = 0; i < g_lbm.evt_count; i++) {
    int idx = (g_lbm.evt_head + LBM_MAX_BTM_EVENTS - i - 1) % LBM_MAX_BTM_EVENTS;
    LbmBtmEvent &e = g_lbm.events[idx];
    if (!e.ts) continue;
    if (!first) s += ","; first = false;
    lbm_mac_str(e.client_mac, mac);
    lbm_mac_str(e.ap_bssid, ap);
    lbm_mac_str(e.candidate_bssid, cand);
    s += "{\"mac\":\"" + String(mac) + "\"";
    s += ",\"ap\":\"" + String(ap) + "\"";
    s += ",\"type\":\"" + String(lbm_btm_type_str(e.type)) + "\"";
    s += ",\"status\":" + String(e.status);
    s += ",\"disassoc_timer\":" + String(e.disassoc_timer);
    s += ",\"candidate\":\"" + String(cand) + "\"";
    s += ",\"cand_rssi\":" + String(e.candidate_rssi);
    s += ",\"ts\":" + String(e.ts) + "}";
  }

  s += "],\"aps\":[";
  first = true;
  for (int i = 0; i < LBM_MAX_APS; i++) {
    LbmAP &a = g_lbm.aps[i];
    if (!a.active) continue;
    if (!first) s += ","; first = false;
    lbm_mac_str(a.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(a.ssid) + "\"";
    s += ",\"ch\":" + String(a.ch);
    s += ",\"rssi\":" + String(a.rssi);
    s += ",\"station_count\":" + String(a.current_load.station_count);
    s += ",\"peak_stations\":" + String(a.peak_load.station_count);
    s += ",\"ch_util\":" + String(a.current_load.channel_util);
    s += ",\"peak_util\":" + String(a.peak_load.channel_util);
    s += ",\"avail_adm\":" + String(a.current_load.avail_admission);
    s += ",\"congested\":" + String(a.congested ? "true" : "false");
    s += ",\"saturated\":" + String(a.saturated ? "true" : "false");
    s += ",\"congestion_events\":" + String(a.congestion_events);
    s += ",\"btm_req\":" + String(a.btm_requests_sent);
    s += ",\"btm_accept\":" + String(a.btm_accepts);
    s += ",\"btm_reject\":" + String(a.btm_rejects);
    s += ",\"has_bss_load\":" + String(a.has_bss_load_ie ? "true" : "false");
    s += ",\"util_hist\":" + lbm_hist_u8(a.util_hist, LBM_HIST_LEN, a.hist_head);
    s += ",\"sta_hist\":" + lbm_hist_u16(a.sta_hist, LBM_HIST_LEN, a.hist_head) + "}";
  }
  s += "]}";
  return s;
}

inline void lbm_routes() {
  server.on("/api/lbm/all", HTTP_GET, []() {
    server.send(200, "application/json", lbm_json_all());
  });
  server.on("/api/lbm/clear", HTTP_POST, []() {
    memset(&g_lbm, 0, sizeof(g_lbm));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

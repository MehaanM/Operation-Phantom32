#pragma once
// rrm_tracker.h — 802.11k Radio Resource Management tracking.
// Parses Neighbor Report Request/Response and Channel Load frames.
// Prefix: rrm_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define RRM_MAX_APS 32
#define RRM_MAX_NEIGHBORS 64
#define RRM_MAX_EVENTS 128
#define RRM_CHAN_LOAD_HIST 30

// 802.11k Action frame categories
#define RRM_ACTION_CAT 5
#define RRM_ACTION_NEIGHBOR_REQ 4
#define RRM_ACTION_NEIGHBOR_RSP 5
#define RRM_ACTION_CHAN_LOAD_REQ 3
#define RRM_ACTION_CHAN_LOAD_RSP 4
#define RRM_ACTION_LINK_MEAS_REQ 6
#define RRM_ACTION_LINK_MEAS_RSP 7

struct RrmNeighborEntry {
  bool active;
  uint8_t bssid[6];
  char ssid[33];
  uint32_t bssid_info;  // 32-bit BSSID Info field
  uint8_t op_class;
  uint8_t channel;
  uint8_t phy_type;
  int8_t rssi;
  uint32_t last_ts;
  uint32_t report_count;
};

struct RrmAP {
  bool active;
  uint8_t bssid[6];
  char ssid[33];
  uint8_t channel;
  int8_t rssi;

  // Channel load history (% utilization)
  uint8_t chan_load_hist[RRM_CHAN_LOAD_HIST];
  uint8_t chan_load_head;
  uint8_t current_load;

  // RRM capability flags (from Capability IE)
  bool rrm_capable;
  bool neighbor_report_capable;
  bool link_meas_capable;
  bool beacon_meas_capable;

  uint32_t neighbor_req_count;
  uint32_t neighbor_rsp_count;
  uint32_t chan_load_req_count;
  uint32_t chan_load_rsp_count;
  uint32_t link_meas_count;

  uint32_t first_ts;
  uint32_t last_ts;
};

struct RrmEvent {
  uint32_t ts;
  uint8_t ap[6];
  uint8_t client[6];
  uint8_t action;  // RRM action code
  uint8_t channel;
  uint8_t op_class;
  int8_t rssi;
  char detail[48];
};

static struct {
  RrmAP aps[RRM_MAX_APS];
  uint8_t ap_count;
  RrmNeighborEntry neighbors[RRM_MAX_NEIGHBORS];
  uint8_t neighbor_count;
  RrmEvent events[RRM_MAX_EVENTS];
  uint8_t event_head;
  uint32_t event_count;

  uint32_t total_neighbor_reqs;
  uint32_t total_neighbor_rsps;
  uint32_t total_chan_loads;
  uint32_t total_link_meas;
} g_rrm = {};

static bool rrm_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static RrmAP *rrm_find_or_alloc_ap(const uint8_t *bssid) {
  for (int i = 0; i < RRM_MAX_APS; i++)
    if (g_rrm.aps[i].active && rrm_mac_eq(g_rrm.aps[i].bssid, bssid))
      return &g_rrm.aps[i];
  for (int i = 0; i < RRM_MAX_APS; i++) {
    if (!g_rrm.aps[i].active) {
      RrmAP &a = g_rrm.aps[i];
      memset(&a, 0, sizeof(a));
      a.active = true;
      memcpy(a.bssid, bssid, 6);
      a.first_ts = millis() / 1000;
      g_rrm.ap_count++;
      return &a;
    }
  }
  uint32_t oldest = UINT32_MAX;
  int idx = 0;
  for (int i = 0; i < RRM_MAX_APS; i++)
    if (g_rrm.aps[i].last_ts < oldest) {
      oldest = g_rrm.aps[i].last_ts;
      idx = i;
    }
  memset(&g_rrm.aps[idx], 0, sizeof(g_rrm.aps[0]));
  g_rrm.aps[idx].active = true;
  memcpy(g_rrm.aps[idx].bssid, bssid, 6);
  g_rrm.aps[idx].first_ts = millis() / 1000;
  return &g_rrm.aps[idx];
}

static void rrm_add_event(const uint8_t *ap, const uint8_t *client, uint8_t action, uint8_t ch,
                          uint8_t op_class, int8_t rssi, const char *detail) {
  RrmEvent &e = g_rrm.events[g_rrm.event_head % RRM_MAX_EVENTS];
  e.ts = millis() / 1000;
  if (ap)
    memcpy(e.ap, ap, 6);
  else
    memset(e.ap, 0, 6);
  if (client)
    memcpy(e.client, client, 6);
  else
    memset(e.client, 0, 6);
  e.action = action;
  e.channel = ch;
  e.op_class = op_class;
  e.rssi = rssi;
  strncpy(e.detail, detail, 47);
  g_rrm.event_head = (g_rrm.event_head + 1) % RRM_MAX_EVENTS;
  g_rrm.event_count++;
}

void rrm_ingest_ap(const uint8_t *bssid, const char *ssid, uint8_t channel, int8_t rssi,
                   bool rrm_cap) {
  RrmAP *a = rrm_find_or_alloc_ap(bssid);
  if (!a)
    return;
  if (ssid && ssid[0])
    strncpy(a->ssid, ssid, 32);
  a->channel = channel;
  if (rssi < 0)
    a->rssi = rssi;
  a->rrm_capable = rrm_cap;
  a->last_ts = millis() / 1000;
}

void rrm_ingest_neighbor_req(const uint8_t *ap, const uint8_t *client, uint8_t channel,
                             int8_t rssi) {
  RrmAP *a = rrm_find_or_alloc_ap(ap);
  if (a) {
    a->neighbor_req_count++;
    a->last_ts = millis() / 1000;
  }
  g_rrm.total_neighbor_reqs++;
  rrm_add_event(ap, client, RRM_ACTION_NEIGHBOR_REQ, channel, 0, rssi, "Neighbor Report Req");
}

void rrm_ingest_neighbor_rsp(const uint8_t *ap, const uint8_t *client, uint8_t channel, int8_t rssi,
                             const uint8_t *neighbor_bssid, uint8_t nb_channel,
                             uint32_t bssid_info) {
  RrmAP *a = rrm_find_or_alloc_ap(ap);
  if (a) {
    a->neighbor_rsp_count++;
    a->neighbor_report_capable = true;
    a->last_ts = millis() / 1000;
  }
  g_rrm.total_neighbor_rsps++;

  if (neighbor_bssid) {
    bool found = false;
    for (int i = 0; i < RRM_MAX_NEIGHBORS; i++) {
      if (g_rrm.neighbors[i].active && rrm_mac_eq(g_rrm.neighbors[i].bssid, neighbor_bssid)) {
        g_rrm.neighbors[i].rssi = rssi;
        g_rrm.neighbors[i].last_ts = millis() / 1000;
        g_rrm.neighbors[i].report_count++;
        found = true;
        break;
      }
    }
    if (!found) {
      for (int i = 0; i < RRM_MAX_NEIGHBORS; i++) {
        if (!g_rrm.neighbors[i].active) {
          RrmNeighborEntry &n = g_rrm.neighbors[i];
          memset(&n, 0, sizeof(n));
          n.active = true;
          memcpy(n.bssid, neighbor_bssid, 6);
          n.channel = nb_channel;
          n.bssid_info = bssid_info;
          n.rssi = rssi;
          n.report_count = 1;
          n.last_ts = millis() / 1000;
          g_rrm.neighbor_count++;
          break;
        }
      }
    }
  }

  rrm_add_event(ap, client, RRM_ACTION_NEIGHBOR_RSP, channel, 0, rssi, "Neighbor Report Rsp");
}

void rrm_ingest_chan_load(const uint8_t *ap, uint8_t channel, uint8_t op_class, uint8_t load_pct) {
  RrmAP *a = rrm_find_or_alloc_ap(ap);
  if (a) {
    a->chan_load_rsp_count++;
    a->current_load = load_pct;
    a->chan_load_hist[a->chan_load_head] = load_pct;
    a->chan_load_head = (a->chan_load_head + 1) % RRM_CHAN_LOAD_HIST;
    a->last_ts = millis() / 1000;
  }
  g_rrm.total_chan_loads++;
  char det[48];
  snprintf(det, 47, "Chan Load %d%% ch%d", load_pct, channel);
  rrm_add_event(ap, nullptr, RRM_ACTION_CHAN_LOAD_RSP, channel, op_class, 0, det);
}

static void rrm_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String rrm_json_all() {
  String s = "{\"ap_count\":" + String(g_rrm.ap_count);
  s += ",\"neighbor_count\":" + String(g_rrm.neighbor_count);
  s += ",\"event_count\":" + String(g_rrm.event_count);
  s += ",\"total_neighbor_reqs\":" + String(g_rrm.total_neighbor_reqs);
  s += ",\"total_neighbor_rsps\":" + String(g_rrm.total_neighbor_rsps);
  s += ",\"total_chan_loads\":" + String(g_rrm.total_chan_loads);
  s += ",\"aps\":[";
  char mac[18];
  bool first = true;
  for (int i = 0; i < RRM_MAX_APS; i++) {
    RrmAP &a = g_rrm.aps[i];
    if (!a.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    rrm_mac_str(a.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(a.ssid) + "\"";
    s += ",\"ch\":" + String(a.channel);
    s += ",\"rssi\":" + String(a.rssi);
    s += ",\"rrm_cap\":" + String(a.rrm_capable ? "true" : "false");
    s += ",\"nb_cap\":" + String(a.neighbor_report_capable ? "true" : "false");
    s += ",\"load\":" + String(a.current_load);
    s += ",\"nb_reqs\":" + String(a.neighbor_req_count);
    s += ",\"nb_rsps\":" + String(a.neighbor_rsp_count);
    s += ",\"load_rsps\":" + String(a.chan_load_rsp_count);
    s += ",\"load_hist\":[";
    for (int j = 0; j < RRM_CHAN_LOAD_HIST; j++) {
      int pos = (a.chan_load_head + j) % RRM_CHAN_LOAD_HIST;
      if (j)
        s += ",";
      s += String(a.chan_load_hist[pos]);
    }
    s += "]}";
  }
  s += "],\"neighbors\":[";
  first = true;
  for (int i = 0; i < RRM_MAX_NEIGHBORS; i++) {
    RrmNeighborEntry &n = g_rrm.neighbors[i];
    if (!n.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    rrm_mac_str(n.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(n.ssid) + "\"";
    s += ",\"ch\":" + String(n.channel);
    s += ",\"rssi\":" + String(n.rssi);
    s += ",\"reports\":" + String(n.report_count);
    s += ",\"bssid_info\":" + String(n.bssid_info);
    s += "}";
  }
  s += "]}";
  return s;
}

static String rrm_json_events() {
  String s = "[";
  char ap[18], cl[18];
  bool first = true;
  uint32_t cnt = g_rrm.event_count;
  uint32_t start = (cnt > RRM_MAX_EVENTS) ? (g_rrm.event_head) : 0;
  uint32_t num = min((uint32_t)RRM_MAX_EVENTS, cnt);
  for (uint32_t i = 0; i < num; i++) {
    uint32_t idx = (start + i) % RRM_MAX_EVENTS;
    RrmEvent &e = g_rrm.events[idx];
    if (!e.ts)
      continue;
    if (!first)
      s += ",";
    first = false;
    rrm_mac_str(e.ap, ap);
    rrm_mac_str(e.client, cl);
    s += "{\"ts\":" + String(e.ts);
    s += ",\"ap\":\"" + String(ap) + "\"";
    s += ",\"client\":\"" + String(cl) + "\"";
    s += ",\"action\":" + String(e.action);
    s += ",\"ch\":" + String(e.channel);
    s += ",\"rssi\":" + String(e.rssi);
    s += ",\"detail\":\"" + String(e.detail) + "\"";
    s += "}";
  }
  s += "]";
  return s;
}

inline void rrm_routes() {
  server.on("/api/rrm/all", HTTP_GET,
            []() { server.send(200, "application/json", rrm_json_all()); });
  server.on("/api/rrm/events", HTTP_GET,
            []() { server.send(200, "application/json", rrm_json_events()); });
  server.on("/api/rrm/clear", HTTP_POST, []() {
    memset(&g_rrm, 0, sizeof(g_rrm));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

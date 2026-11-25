#pragma once
// client_activity.h — Per-client activity profiling and session tracking.
// Tracks client frame types, activity windows, idle periods, roaming events.
// Prefix: cac_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define CAC_MAX_CLIENTS 64
#define CAC_HIST_LEN 60      // 60-second activity history
#define CAC_MAX_APS_ASSOC 4  // APs a client has been seen associated to

// Activity state
enum CacState : uint8_t {
  CAC_IDLE = 0,
  CAC_ACTIVE = 1,
  CAC_ROAMING = 2,
  CAC_PROBING = 3,
  CAC_DEAUTHED = 4,
};

struct CacAssoc {
  uint8_t bssid[6];
  char ssid[33];
  uint32_t first_seen;
  uint32_t last_seen;
  uint32_t frame_count;
  int8_t avg_rssi;
};

struct CacClient {
  bool active;
  uint8_t mac[6];
  char vendor[24];
  char category[16];

  CacState state;
  uint8_t channel;
  int8_t rssi;

  // Associations (roaming history)
  CacAssoc assocs[CAC_MAX_APS_ASSOC];
  uint8_t assoc_count;
  uint8_t roam_count;

  // Frame type counters
  uint32_t mgmt_frames;
  uint32_t data_frames;
  uint32_t ctrl_frames;
  uint32_t probe_requests;
  uint32_t assoc_requests;
  uint32_t deauth_frames;
  uint32_t total_frames;
  uint32_t total_bytes;

  // Activity rate ring (frames per second, 60-second history)
  uint8_t activity_hist[CAC_HIST_LEN];
  uint8_t activity_head;
  uint32_t activity_tick_ts;
  uint8_t activity_accum;

  // Idle tracking
  uint32_t last_active_ts;
  uint32_t idle_secs;

  // Session
  uint32_t first_ts;
  uint32_t last_ts;
  uint32_t session_start;
  uint32_t session_duration;
};

static struct {
  CacClient clients[CAC_MAX_CLIENTS];
  uint8_t client_count;
  uint32_t total_frames;
  uint32_t total_roams;
  uint32_t total_probes;
} g_cac = {};

static bool cac_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}
static bool cac_is_bcast(const uint8_t *m) {
  return m[0] & 0x01;
}

static CacClient *cac_find_or_alloc(const uint8_t *mac) {
  for (int i = 0; i < CAC_MAX_CLIENTS; i++)
    if (g_cac.clients[i].active && cac_mac_eq(g_cac.clients[i].mac, mac))
      return &g_cac.clients[i];
  for (int i = 0; i < CAC_MAX_CLIENTS; i++) {
    if (!g_cac.clients[i].active) {
      CacClient &c = g_cac.clients[i];
      memset(&c, 0, sizeof(c));
      c.active = true;
      memcpy(c.mac, mac, 6);
      c.first_ts = millis() / 1000;
      c.session_start = c.first_ts;
      g_cac.client_count++;
      return &c;
    }
  }
  uint32_t oldest = UINT32_MAX;
  int idx = 0;
  for (int i = 0; i < CAC_MAX_CLIENTS; i++)
    if (g_cac.clients[i].last_ts < oldest) {
      oldest = g_cac.clients[i].last_ts;
      idx = i;
    }
  memset(&g_cac.clients[idx], 0, sizeof(g_cac.clients[0]));
  g_cac.clients[idx].active = true;
  memcpy(g_cac.clients[idx].mac, mac, 6);
  g_cac.clients[idx].first_ts = millis() / 1000;
  g_cac.clients[idx].session_start = g_cac.clients[idx].first_ts;
  return &g_cac.clients[idx];
}

static void cac_tick_activity(CacClient &c, uint32_t now_s) {
  if (c.activity_tick_ts == now_s)
    return;
  c.activity_hist[c.activity_head] = c.activity_accum;
  c.activity_head = (c.activity_head + 1) % CAC_HIST_LEN;
  c.activity_accum = 0;
  c.activity_tick_ts = now_s;
}

static void cac_update_assoc(CacClient &c, const uint8_t *bssid, const char *ssid, int8_t rssi,
                             uint32_t now_s) {
  for (int i = 0; i < CAC_MAX_APS_ASSOC; i++) {
    if (cac_mac_eq(c.assocs[i].bssid, bssid)) {
      c.assocs[i].last_seen = now_s;
      c.assocs[i].frame_count++;
      c.assocs[i].avg_rssi = (int8_t)((c.assocs[i].avg_rssi * 3 + rssi) / 4);
      return;
    }
  }
  // New AP
  if (c.assoc_count > 0) {
    c.roam_count++;
    g_cac.total_roams++;
    c.state = CAC_ROAMING;
  }
  int slot = c.assoc_count < CAC_MAX_APS_ASSOC ? c.assoc_count : CAC_MAX_APS_ASSOC - 1;
  if (c.assoc_count < CAC_MAX_APS_ASSOC)
    c.assoc_count++;
  memcpy(c.assocs[slot].bssid, bssid, 6);
  if (ssid)
    strncpy(c.assocs[slot].ssid, ssid, 32);
  c.assocs[slot].first_seen = now_s;
  c.assocs[slot].last_seen = now_s;
  c.assocs[slot].frame_count = 1;
  c.assocs[slot].avg_rssi = rssi;
}

void cac_ingest_data(const uint8_t *ta, const uint8_t *bssid, const char *ssid, uint8_t channel,
                     int8_t rssi, uint16_t len) {
  if (!ta || cac_is_bcast(ta))
    return;
  uint32_t now_s = millis() / 1000;
  CacClient *c = cac_find_or_alloc(ta);
  if (!c)
    return;

  cac_tick_activity(*c, now_s);
  c->channel = channel;
  if (rssi < 0)
    c->rssi = rssi;
  c->data_frames++;
  c->total_frames++;
  c->total_bytes += len;
  c->activity_accum++;
  c->last_ts = now_s;
  c->last_active_ts = now_s;
  c->idle_secs = 0;
  c->state = CAC_ACTIVE;
  c->session_duration = now_s - c->session_start;
  g_cac.total_frames++;

  if (bssid)
    cac_update_assoc(*c, bssid, ssid, rssi, now_s);
}

void cac_ingest_probe(const uint8_t *ta, uint8_t channel, int8_t rssi) {
  if (!ta || cac_is_bcast(ta))
    return;
  uint32_t now_s = millis() / 1000;
  CacClient *c = cac_find_or_alloc(ta);
  if (!c)
    return;

  cac_tick_activity(*c, now_s);
  c->channel = channel;
  if (rssi < 0)
    c->rssi = rssi;
  c->mgmt_frames++;
  c->probe_requests++;
  c->total_frames++;
  c->activity_accum++;
  c->last_ts = now_s;
  c->last_active_ts = now_s;
  if (c->state == CAC_IDLE)
    c->state = CAC_PROBING;
  g_cac.total_probes++;
  g_cac.total_frames++;
}

void cac_ingest_deauth(const uint8_t *ta, uint8_t channel, int8_t rssi) {
  if (!ta || cac_is_bcast(ta))
    return;
  uint32_t now_s = millis() / 1000;
  CacClient *c = cac_find_or_alloc(ta);
  if (!c)
    return;

  c->deauth_frames++;
  c->mgmt_frames++;
  c->total_frames++;
  c->state = CAC_DEAUTHED;
  c->last_ts = now_s;
  g_cac.total_frames++;
}

static void cac_update_idle(uint32_t now_s) {
  for (int i = 0; i < CAC_MAX_CLIENTS; i++) {
    CacClient &c = g_cac.clients[i];
    if (!c.active)
      continue;
    if (c.last_active_ts && now_s > c.last_active_ts) {
      c.idle_secs = now_s - c.last_active_ts;
      if (c.idle_secs > 30 && c.state == CAC_ACTIVE)
        c.state = CAC_IDLE;
    }
  }
}

static void cac_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String cac_json_all() {
  cac_update_idle(millis() / 1000);
  String s = "{\"client_count\":" + String(g_cac.client_count);
  s += ",\"total_frames\":" + String(g_cac.total_frames);
  s += ",\"total_roams\":" + String(g_cac.total_roams);
  s += ",\"total_probes\":" + String(g_cac.total_probes);
  s += ",\"clients\":[";
  char mac[18];
  bool first = true;
  static const char *const state_str[] = {"idle", "active", "roaming", "probing", "deauthed"};
  for (int i = 0; i < CAC_MAX_CLIENTS; i++) {
    CacClient &c = g_cac.clients[i];
    if (!c.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    cac_mac_str(c.mac, mac);
    s += "{\"mac\":\"" + String(mac) + "\"";
    s += ",\"ch\":" + String(c.channel);
    s += ",\"rssi\":" + String(c.rssi);
    s += ",\"state\":\"" + String(state_str[c.state]) + "\"";
    s += ",\"total\":" + String(c.total_frames);
    s += ",\"data\":" + String(c.data_frames);
    s += ",\"mgmt\":" + String(c.mgmt_frames);
    s += ",\"probes\":" + String(c.probe_requests);
    s += ",\"deauths\":" + String(c.deauth_frames);
    s += ",\"bytes\":" + String(c.total_bytes);
    s += ",\"roams\":" + String(c.roam_count);
    s += ",\"idle\":" + String(c.idle_secs);
    s += ",\"duration\":" + String(c.session_duration);
    s += ",\"activity\":[";
    for (int j = 0; j < CAC_HIST_LEN; j++) {
      int pos = (c.activity_head + j) % CAC_HIST_LEN;
      if (j)
        s += ",";
      s += String(c.activity_hist[pos]);
    }
    s += "],\"assocs\":[";
    bool fa = true;
    for (int j = 0; j < c.assoc_count && j < CAC_MAX_APS_ASSOC; j++) {
      char ap[18];
      cac_mac_str(c.assocs[j].bssid, ap);
      if (!fa)
        s += ",";
      fa = false;
      s += "{\"bssid\":\"" + String(ap) + "\"";
      s += ",\"ssid\":\"" + String(c.assocs[j].ssid) + "\"";
      s += ",\"frames\":" + String(c.assocs[j].frame_count);
      s += ",\"rssi\":" + String(c.assocs[j].avg_rssi);
      s += "}";
    }
    s += "]}";
  }
  s += "]}";
  return s;
}

inline void cac_routes() {
  server.on("/api/cac/all", HTTP_GET,
            []() { server.send(200, "application/json", cac_json_all()); });
  server.on("/api/cac/clear", HTTP_POST, []() {
    memset(&g_cac, 0, sizeof(g_cac));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

#pragma once
// probe_flood.h — Probe request flood detector.
// Tracks probe request rates per client and per SSID target.
// High probe rates from a single client indicate scanning tools (e.g. Kismet,
// airodump-ng) or automated device discovery. Wildcard probes (empty SSID)
// often indicate PNL (Preferred Network List) enumeration attempts.
// Prefix: pfl_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define PFL_MAX_CLIENTS  48
#define PFL_MAX_TARGETS  32   // unique SSIDs probed per client
#define PFL_HIST_LEN     30
#define PFL_FLOOD_THRESH  8   // probes/sec threshold

struct PflTarget {
  char    ssid[33];
  uint16_t count;
  uint32_t last_ts;
};

struct PflClient {
  bool    active;
  uint8_t mac[6];

  uint32_t total_probes;
  uint32_t wildcard_probes;  // empty SSID (PNL scan)
  uint32_t directed_probes;  // specific SSID

  // Rolling 30s rate
  uint16_t hist[PFL_HIST_LEN];
  uint8_t  hist_head;
  uint16_t cur_count;
  uint32_t last_tick_s;

  bool     flood_active;
  uint32_t flood_start_ts;

  // Top SSID targets
  PflTarget targets[PFL_MAX_TARGETS];
  uint8_t   target_count;

  int8_t   rssi;
  uint32_t first_ts;
  uint32_t last_ts;
};

static struct {
  PflClient clients[PFL_MAX_CLIENTS];
  uint8_t   count;
  uint32_t  total_probes;
} g_pfl = {};

static bool pfl_mac_eq(const uint8_t *a, const uint8_t *b) { return memcmp(a,b,6)==0; }

static PflClient* pfl_find(const uint8_t *mac) {
  for (int i = 0; i < PFL_MAX_CLIENTS; i++)
    if (g_pfl.clients[i].active && pfl_mac_eq(g_pfl.clients[i].mac, mac))
      return &g_pfl.clients[i];
  return nullptr;
}

static PflClient* pfl_alloc(const uint8_t *mac) {
  for (int i = 0; i < PFL_MAX_CLIENTS; i++) {
    if (!g_pfl.clients[i].active) {
      PflClient &c = g_pfl.clients[i];
      memset(&c, 0, sizeof(c));
      c.active = true;
      memcpy(c.mac, mac, 6);
      c.first_ts = millis()/1000;
      g_pfl.count++;
      return &c;
    }
  }
  uint32_t oldest = UINT32_MAX; int idx = 0;
  for (int i = 0; i < PFL_MAX_CLIENTS; i++)
    if (g_pfl.clients[i].last_ts < oldest) { oldest = g_pfl.clients[i].last_ts; idx = i; }
  memset(&g_pfl.clients[idx], 0, sizeof(g_pfl.clients[0]));
  g_pfl.clients[idx].active = true;
  memcpy(g_pfl.clients[idx].mac, mac, 6);
  g_pfl.clients[idx].first_ts = millis()/1000;
  return &g_pfl.clients[idx];
}

static void pfl_tick(PflClient &c) {
  uint32_t now_s = millis()/1000;
  if (now_s == c.last_tick_s) return;
  uint32_t elapsed = now_s - c.last_tick_s;
  if (elapsed > PFL_HIST_LEN) elapsed = PFL_HIST_LEN;
  for (uint32_t i = 0; i < elapsed; i++) {
    c.hist[c.hist_head % PFL_HIST_LEN] = (i == 0) ? c.cur_count : 0;
    c.hist_head = (c.hist_head + 1) % PFL_HIST_LEN;
  }
  c.last_tick_s = now_s;
  c.cur_count = 0;
}

void pfl_ingest(const uint8_t *client_mac, const char *ssid, int8_t rssi) {
  PflClient *c = pfl_find(client_mac);
  if (!c) c = pfl_alloc(client_mac);
  if (!c) return;

  c->last_ts = millis()/1000;
  if (rssi < 0) c->rssi = rssi;
  c->total_probes++;
  g_pfl.total_probes++;

  pfl_tick(*c);
  c->cur_count++;

  bool wildcard = (!ssid || ssid[0] == 0);
  if (wildcard) {
    c->wildcard_probes++;
  } else {
    c->directed_probes++;
    // Update target list
    bool found = false;
    for (int i = 0; i < c->target_count; i++) {
      if (strncmp(c->targets[i].ssid, ssid, 32) == 0) {
        c->targets[i].count++;
        c->targets[i].last_ts = c->last_ts;
        found = true;
        break;
      }
    }
    if (!found) {
      if (c->target_count < PFL_MAX_TARGETS) {
        PflTarget &t = c->targets[c->target_count++];
        strncpy(t.ssid, ssid, 32);
        t.count = 1;
        t.last_ts = c->last_ts;
      } else {
        // Replace oldest
        uint32_t oldest = UINT32_MAX; int idx = 0;
        for (int i = 0; i < PFL_MAX_TARGETS; i++)
          if (c->targets[i].last_ts < oldest) { oldest = c->targets[i].last_ts; idx = i; }
        strncpy(c->targets[idx].ssid, ssid, 32);
        c->targets[idx].count = 1;
        c->targets[idx].last_ts = c->last_ts;
      }
    }
  }

  // Check flood
  uint32_t ws = 0;
  for (int i = 0; i < PFL_HIST_LEN; i++) ws += c->hist[i];
  float rate = (float)ws / PFL_HIST_LEN;
  if (rate >= PFL_FLOOD_THRESH) {
    if (!c->flood_active) {
      c->flood_active = true;
      c->flood_start_ts = c->last_ts;
    }
  } else {
    c->flood_active = false;
  }
}

static void pfl_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String pfl_json_all() {
  String s = "{\"count\":" + String(g_pfl.count);
  s += ",\"total_probes\":" + String(g_pfl.total_probes);
  int floods = 0;
  for (int i = 0; i < PFL_MAX_CLIENTS; i++)
    if (g_pfl.clients[i].active && g_pfl.clients[i].flood_active) floods++;
  s += ",\"active_floods\":" + String(floods);
  s += ",\"clients\":[";
  char mac[18]; bool first = true;
  for (int i = 0; i < PFL_MAX_CLIENTS; i++) {
    PflClient &c = g_pfl.clients[i];
    if (!c.active) continue;
    if (!first) s += ","; first = false;
    pfl_mac_str(c.mac, mac);
    uint32_t ws = 0;
    for (int j = 0; j < PFL_HIST_LEN; j++) ws += c.hist[j];
    float rate = (float)ws / PFL_HIST_LEN;
    s += "{\"mac\":\"" + String(mac) + "\"";
    s += ",\"rssi\":" + String(c.rssi);
    s += ",\"total\":" + String(c.total_probes);
    s += ",\"wildcard\":" + String(c.wildcard_probes);
    s += ",\"directed\":" + String(c.directed_probes);
    s += ",\"rate\":" + String((int)(rate * 10));
    s += ",\"flood\":" + String(c.flood_active ? "true" : "false");
    s += ",\"targets\":[";
    bool ft = true;
    for (int j = 0; j < c.target_count; j++) {
      if (!ft) s += ","; ft = false;
      // Escape SSID
      s += "{\"ssid\":\"";
      for (int k = 0; k < 32 && c.targets[j].ssid[k]; k++) {
        char ch = c.targets[j].ssid[k];
        if (ch == '"' || ch == '\\') s += '\\';
        s += ch;
      }
      s += "\",\"count\":" + String(c.targets[j].count) + "}";
    }
    s += "],\"hist\":[";
    for (int j = 0; j < PFL_HIST_LEN; j++) {
      if (j) s += ",";
      int idx = (c.hist_head + j) % PFL_HIST_LEN;
      s += String(c.hist[idx]);
    }
    s += "]}";
  }
  s += "]}";
  return s;
}

inline void pfl_routes() {
  server.on("/api/pfl/all", HTTP_GET, []() {
    server.send(200, "application/json", pfl_json_all());
  });
  server.on("/api/pfl/clear", HTTP_POST, []() {
    memset(&g_pfl, 0, sizeof(g_pfl));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

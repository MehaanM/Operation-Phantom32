#pragma once
// ap_load_balancer.h — Multi-AP load balancing detector.
// Detects BSS-TM steering (802.11v), BSS-TM neighbor reports,
// per-AP client counts, and load-balancing patterns across SSIDs.
// Prefix: alb_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define ALB_MAX_APS 32
#define ALB_MAX_SSID_GRP 8  // groups of APs sharing same SSID
#define ALB_HIST_LEN 60

struct AlbAP {
  bool active;
  uint8_t bssid[6];
  char ssid[33];
  uint8_t ch;
  int8_t rssi;
  uint8_t oui[3];

  // Client load
  uint16_t client_count;  // assoc clients from SSID scan
  uint16_t peak_clients;
  uint16_t total_assocs;

  // 802.11v BSS Transition Management
  bool has_btm;               // AP supports BSS Transition Management
  uint16_t btm_req_sent;      // BTM requests observed going to clients
  uint16_t btm_rsp_accepted;  // clients accepted the transition

  // Load history (client count per second)
  uint8_t load_hist[ALB_HIST_LEN];
  uint8_t hist_head;
  uint8_t cur_load;
  uint32_t last_tick_s;

  uint32_t first_ts;
  uint32_t last_ts;
};

struct AlbSsidGroup {
  bool active;
  char ssid[33];
  uint8_t ap_count;
  uint8_t ap_bssids[ALB_MAX_APS][6];  // BSSIDs in this group

  // Load distribution stats
  uint16_t total_clients;
  uint8_t imbalance_pct;  // max - min client count / max * 100
};

static struct {
  AlbAP aps[ALB_MAX_APS];
  AlbSsidGroup groups[ALB_MAX_SSID_GRP];
  uint8_t ap_count;
  uint8_t group_count;
} g_alb = {};

static bool alb_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static AlbAP *alb_find(const uint8_t *bssid) {
  for (int i = 0; i < ALB_MAX_APS; i++)
    if (g_alb.aps[i].active && alb_mac_eq(g_alb.aps[i].bssid, bssid))
      return &g_alb.aps[i];
  return nullptr;
}

static AlbAP *alb_alloc(const uint8_t *bssid) {
  for (int i = 0; i < ALB_MAX_APS; i++) {
    if (!g_alb.aps[i].active) {
      AlbAP &a = g_alb.aps[i];
      memset(&a, 0, sizeof(a));
      a.active = true;
      memcpy(a.bssid, bssid, 6);
      memcpy(a.oui, bssid, 3);
      a.first_ts = millis() / 1000;
      g_alb.ap_count++;
      return &a;
    }
  }
  uint32_t oldest = UINT32_MAX;
  int idx = 0;
  for (int i = 0; i < ALB_MAX_APS; i++)
    if (!g_alb.aps[i].has_btm && g_alb.aps[i].last_ts < oldest) {
      oldest = g_alb.aps[i].last_ts;
      idx = i;
    }
  memset(&g_alb.aps[idx], 0, sizeof(g_alb.aps[0]));
  g_alb.aps[idx].active = true;
  memcpy(g_alb.aps[idx].bssid, bssid, 6);
  memcpy(g_alb.aps[idx].oui, bssid, 3);
  g_alb.aps[idx].first_ts = millis() / 1000;
  return &g_alb.aps[idx];
}

static void alb_tick(AlbAP &a) {
  uint32_t now_s = millis() / 1000;
  if (now_s == a.last_tick_s)
    return;
  a.load_hist[a.hist_head % ALB_HIST_LEN] = a.cur_load;
  a.hist_head = (a.hist_head + 1) % ALB_HIST_LEN;
  a.last_tick_s = now_s;
  a.cur_load = 0;
}

static AlbSsidGroup *alb_find_group(const char *ssid) {
  for (int i = 0; i < ALB_MAX_SSID_GRP; i++)
    if (g_alb.groups[i].active && strncmp(g_alb.groups[i].ssid, ssid, 32) == 0)
      return &g_alb.groups[i];
  return nullptr;
}

static void alb_add_to_group(const char *ssid, const uint8_t *bssid) {
  AlbSsidGroup *g = alb_find_group(ssid);
  if (!g) {
    for (int i = 0; i < ALB_MAX_SSID_GRP; i++) {
      if (!g_alb.groups[i].active) {
        g = &g_alb.groups[i];
        g->active = true;
        strncpy(g->ssid, ssid, 32);
        g_alb.group_count++;
        break;
      }
    }
  }
  if (!g)
    return;
  for (int i = 0; i < g->ap_count; i++)
    if (alb_mac_eq(g->ap_bssids[i], bssid))
      return;
  if (g->ap_count < ALB_MAX_APS)
    memcpy(g->ap_bssids[g->ap_count++], bssid, 6);
}

static void alb_recompute_group(AlbSsidGroup &g) {
  uint16_t min_c = 65535, max_c = 0, total = 0;
  for (int i = 0; i < g.ap_count; i++) {
    AlbAP *a = alb_find(g.ap_bssids[i]);
    if (!a)
      continue;
    if (a->client_count < min_c)
      min_c = a->client_count;
    if (a->client_count > max_c)
      max_c = a->client_count;
    total += a->client_count;
  }
  g.total_clients = total;
  g.imbalance_pct = (max_c > 0) ? (uint8_t)(((max_c - min_c) * 100) / max_c) : 0;
}

void alb_ingest_ap(const uint8_t *bssid, const char *ssid, uint8_t ch, int8_t rssi, bool has_btm,
                   uint16_t client_count) {
  AlbAP *a = alb_find(bssid);
  if (!a)
    a = alb_alloc(bssid);
  if (!a)
    return;

  if (ssid && ssid[0])
    strncpy(a->ssid, ssid, 32);
  a->ch = ch;
  if (rssi < 0)
    a->rssi = rssi;
  a->has_btm = has_btm;
  a->last_ts = millis() / 1000;
  alb_tick(*a);

  if (client_count != a->client_count) {
    a->total_assocs += (client_count > a->client_count) ? (client_count - a->client_count) : 0;
    a->client_count = client_count;
    a->cur_load = client_count;
    if (client_count > a->peak_clients)
      a->peak_clients = client_count;
  }

  if (ssid && ssid[0])
    alb_add_to_group(ssid, bssid);
}

void alb_ingest_btm_req(const uint8_t *ap_bssid, const uint8_t *client_mac) {
  AlbAP *a = alb_find(ap_bssid);
  if (!a)
    return;
  a->btm_req_sent++;
}

void alb_ingest_btm_rsp(const uint8_t *ap_bssid, bool accepted) {
  AlbAP *a = alb_find(ap_bssid);
  if (!a)
    return;
  if (accepted)
    a->btm_rsp_accepted++;
}

static void alb_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String alb_hist_json(const AlbAP &a) {
  String s = "[";
  for (int i = 0; i < ALB_HIST_LEN; i++) {
    int idx = (a.hist_head + i) % ALB_HIST_LEN;
    if (i)
      s += ",";
    s += String(a.load_hist[idx]);
  }
  return s + "]";
}

static String alb_json_all() {
  // Recompute groups
  for (int i = 0; i < ALB_MAX_SSID_GRP; i++)
    if (g_alb.groups[i].active)
      alb_recompute_group(g_alb.groups[i]);

  String s = "{\"ap_count\":" + String(g_alb.ap_count);
  s += ",\"group_count\":" + String(g_alb.group_count);
  s += ",\"groups\":[";
  bool fg = true;
  for (int i = 0; i < ALB_MAX_SSID_GRP; i++) {
    AlbSsidGroup &g = g_alb.groups[i];
    if (!g.active)
      continue;
    if (!fg)
      s += ",";
    fg = false;
    s += "{\"ssid\":\"" + String(g.ssid) + "\"";
    s += ",\"ap_count\":" + String(g.ap_count);
    s += ",\"total_clients\":" + String(g.total_clients);
    s += ",\"imbalance_pct\":" + String(g.imbalance_pct) + "}";
  }
  s += "],\"aps\":[";
  char mac[18];
  bool first = true;
  for (int i = 0; i < ALB_MAX_APS; i++) {
    AlbAP &a = g_alb.aps[i];
    if (!a.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    alb_mac_str(a.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(a.ssid) + "\"";
    s += ",\"ch\":" + String(a.ch);
    s += ",\"rssi\":" + String(a.rssi);
    s += ",\"clients\":" + String(a.client_count);
    s += ",\"peak_clients\":" + String(a.peak_clients);
    s += ",\"has_btm\":" + String(a.has_btm ? "true" : "false");
    s += ",\"btm_req\":" + String(a.btm_req_sent);
    s += ",\"btm_ok\":" + String(a.btm_rsp_accepted);
    s += ",\"hist\":" + alb_hist_json(a) + "}";
  }
  s += "]}";
  return s;
}

inline void alb_routes() {
  server.on("/api/alb/all", HTTP_GET,
            []() { server.send(200, "application/json", alb_json_all()); });
  server.on("/api/alb/clear", HTTP_POST, []() {
    memset(&g_alb, 0, sizeof(g_alb));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

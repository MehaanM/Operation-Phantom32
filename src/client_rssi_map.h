#pragma once
// client_rssi_map.h — Per-client RSSI heatmap and location proximity estimation.
// Uses multi-AP RSSI to estimate relative client positions.
// Prefix: crm_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define CRM_MAX_CLIENTS  48
#define CRM_MAX_APS      16
#define CRM_HIST_LEN     30   // 30-sample RSSI history per AP

struct CrmRssiSample {
  int8_t  rssi;
  uint8_t ch;
  uint32_t ts;
};

struct CrmClientAP {
  bool     active;
  uint8_t  ap_bssid[6];
  char     ap_ssid[17];
  int8_t   rssi;
  uint8_t  ch;
  uint32_t last_ts;
  uint32_t frame_count;
  // Rolling RSSI history from this AP's perspective
  CrmRssiSample hist[CRM_HIST_LEN];
  uint8_t  hist_head;
  uint8_t  hist_count;
  int8_t   rssi_avg;
  int8_t   rssi_min;
  int8_t   rssi_max;
};

struct CrmClient {
  bool     active;
  uint8_t  mac[6];
  uint32_t first_ts;
  uint32_t last_ts;
  uint32_t total_frames;

  CrmClientAP ap_views[CRM_MAX_APS];  // RSSI seen from each AP
  uint8_t  ap_count;

  // Computed proximity: best-rssi AP is "anchor"
  uint8_t  best_ap_bssid[6];
  int8_t   best_rssi;
  int8_t   rssi_spread;  // max - min across APs (indicates position estimate quality)
};

static struct {
  CrmClient clients[CRM_MAX_CLIENTS];
  uint8_t   count;
  uint32_t  total_updates;
} g_crm = {};

static bool crm_mac_eq(const uint8_t *a, const uint8_t *b) { return memcmp(a,b,6)==0; }
static bool crm_is_bcast(const uint8_t *m) { return m[0]&0x01; }

static CrmClient* crm_find(const uint8_t *mac) {
  for (int i = 0; i < CRM_MAX_CLIENTS; i++)
    if (g_crm.clients[i].active && crm_mac_eq(g_crm.clients[i].mac, mac))
      return &g_crm.clients[i];
  return nullptr;
}

static CrmClient* crm_alloc(const uint8_t *mac) {
  for (int i = 0; i < CRM_MAX_CLIENTS; i++) {
    if (!g_crm.clients[i].active) {
      CrmClient &c = g_crm.clients[i];
      memset(&c, 0, sizeof(c));
      c.active = true;
      memcpy(c.mac, mac, 6);
      c.best_rssi = -127;
      c.first_ts = millis()/1000;
      g_crm.count++;
      return &c;
    }
  }
  uint32_t oldest = UINT32_MAX; int idx = 0;
  for (int i = 0; i < CRM_MAX_CLIENTS; i++)
    if (g_crm.clients[i].last_ts < oldest) { oldest = g_crm.clients[i].last_ts; idx = i; }
  memset(&g_crm.clients[idx], 0, sizeof(g_crm.clients[0]));
  g_crm.clients[idx].active = true;
  memcpy(g_crm.clients[idx].mac, mac, 6);
  g_crm.clients[idx].best_rssi = -127;
  g_crm.clients[idx].first_ts = millis()/1000;
  return &g_crm.clients[idx];
}

static CrmClientAP* crm_find_ap(CrmClient &c, const uint8_t *ap_bssid) {
  for (int i = 0; i < CRM_MAX_APS; i++)
    if (c.ap_views[i].active && crm_mac_eq(c.ap_views[i].ap_bssid, ap_bssid))
      return &c.ap_views[i];
  return nullptr;
}

static CrmClientAP* crm_alloc_ap(CrmClient &c, const uint8_t *ap_bssid) {
  for (int i = 0; i < CRM_MAX_APS; i++) {
    if (!c.ap_views[i].active) {
      CrmClientAP &a = c.ap_views[i];
      memset(&a, 0, sizeof(a));
      a.active = true;
      memcpy(a.ap_bssid, ap_bssid, 6);
      a.rssi_min = 0;
      c.ap_count++;
      return &a;
    }
  }
  uint32_t oldest = UINT32_MAX; int idx = 0;
  for (int i = 0; i < CRM_MAX_APS; i++)
    if (c.ap_views[i].last_ts < oldest) { oldest = c.ap_views[i].last_ts; idx = i; }
  memset(&c.ap_views[idx], 0, sizeof(c.ap_views[0]));
  c.ap_views[idx].active = true;
  memcpy(c.ap_views[idx].ap_bssid, ap_bssid, 6);
  c.ap_views[idx].rssi_min = 0;
  return &c.ap_views[idx];
}

static void crm_update_ap_stats(CrmClientAP &av) {
  int8_t mn = 0, mx = -127; int16_t sum = 0;
  for (int i = 0; i < av.hist_count; i++) {
    int8_t v = av.hist[i].rssi;
    if (v < mn || mn == 0) mn = v;
    if (v > mx) mx = v;
    sum += v;
  }
  av.rssi_avg = av.hist_count ? sum / av.hist_count : av.rssi;
  av.rssi_min = mn;
  av.rssi_max = mx;
}

static void crm_update_client_best(CrmClient &c) {
  c.best_rssi = -127;
  int8_t min_rssi = 0;
  for (int i = 0; i < CRM_MAX_APS; i++) {
    if (!c.ap_views[i].active) continue;
    if (c.ap_views[i].rssi > c.best_rssi) {
      c.best_rssi = c.ap_views[i].rssi;
      memcpy(c.best_ap_bssid, c.ap_views[i].ap_bssid, 6);
    }
    if (c.ap_views[i].rssi < min_rssi || min_rssi == 0) min_rssi = c.ap_views[i].rssi;
  }
  c.rssi_spread = c.best_rssi - min_rssi;
}

void crm_ingest(const uint8_t *client_mac, const uint8_t *ap_bssid,
                const char *ap_ssid, uint8_t ch, int8_t rssi) {
  if (!client_mac || !ap_bssid || crm_is_bcast(client_mac)) return;
  if (rssi >= 0) return;
  uint32_t now_s = millis()/1000;
  g_crm.total_updates++;

  CrmClient *c = crm_find(client_mac);
  if (!c) c = crm_alloc(client_mac);
  if (!c) return;

  c->total_frames++;
  c->last_ts = now_s;

  CrmClientAP *av = crm_find_ap(*c, ap_bssid);
  if (!av) av = crm_alloc_ap(*c, ap_bssid);
  if (!av) return;

  av->rssi = rssi;
  av->ch = ch;
  av->last_ts = now_s;
  av->frame_count++;
  if (ap_ssid) strncpy(av->ap_ssid, ap_ssid, 16);

  CrmRssiSample &s = av->hist[av->hist_head % CRM_HIST_LEN];
  s.rssi = rssi; s.ch = ch; s.ts = now_s;
  av->hist_head = (av->hist_head + 1) % CRM_HIST_LEN;
  if (av->hist_count < CRM_HIST_LEN) av->hist_count++;

  if (av->frame_count % 5 == 0) crm_update_ap_stats(*av);
  crm_update_client_best(*c);
}

static void crm_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String crm_json_all() {
  String s = "{\"count\":" + String(g_crm.count);
  s += ",\"total_updates\":" + String(g_crm.total_updates);
  s += ",\"clients\":[";
  char mac[18]; bool first = true;
  for (int i = 0; i < CRM_MAX_CLIENTS; i++) {
    CrmClient &c = g_crm.clients[i];
    if (!c.active) continue;
    if (!first) s += ","; first = false;
    crm_mac_str(c.mac, mac);
    s += "{\"mac\":\"" + String(mac) + "\"";
    s += ",\"frames\":" + String(c.total_frames);
    s += ",\"ap_count\":" + String(c.ap_count);
    s += ",\"best_rssi\":" + String(c.best_rssi);
    s += ",\"spread\":" + String(c.rssi_spread);
    char bap[18]; crm_mac_str(c.best_ap_bssid, bap);
    s += ",\"best_ap\":\"" + String(bap) + "\"";
    s += ",\"aps\":[";
    bool fa = true;
    for (int j = 0; j < CRM_MAX_APS; j++) {
      CrmClientAP &av = c.ap_views[j];
      if (!av.active) continue;
      if (!fa) s += ","; fa = false;
      char abssid[18]; crm_mac_str(av.ap_bssid, abssid);
      s += "{\"bssid\":\"" + String(abssid) + "\"";
      s += ",\"ssid\":\"" + String(av.ap_ssid) + "\"";
      s += ",\"rssi\":" + String(av.rssi);
      s += ",\"avg\":" + String(av.rssi_avg);
      s += ",\"min\":" + String(av.rssi_min);
      s += ",\"max\":" + String(av.rssi_max);
      s += ",\"ch\":" + String(av.ch);
      s += ",\"frames\":" + String(av.frame_count);
      s += ",\"hist\":[";
      for (int k = 0; k < av.hist_count; k++) {
        int idx = ((int)av.hist_head - av.hist_count + k + CRM_HIST_LEN) % CRM_HIST_LEN;
        if (k) s += ",";
        s += String(av.hist[idx].rssi);
      }
      s += "]}";
    }
    s += "]}";
  }
  s += "]}";
  return s;
}

inline void crm_routes() {
  server.on("/api/crm/all", HTTP_GET, []() {
    server.send(200, "application/json", crm_json_all());
  });
  server.on("/api/crm/clear", HTTP_POST, []() {
    memset(&g_crm, 0, sizeof(g_crm));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

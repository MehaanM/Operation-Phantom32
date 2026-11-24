#pragma once
// client_assoc_tracker.h — Client association history tracker.
// Tracks which APs each client has associated with over time,
// including roaming events, re-associations, and time between events.
// Prefix: cat_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define CAT_MAX_CLIENTS  48
#define CAT_MAX_ASSOC_HIST 16  // associations per client
#define CAT_ROAM_THRESH_MS 500 // sub-500ms AP change = roam

struct CatAssocEvent {
  uint8_t  ap_bssid[6];
  char     ap_ssid[33];
  uint8_t  ch;
  int8_t   rssi;
  uint32_t ts;
  bool     is_reassoc;
};

struct CatClient {
  bool    active;
  uint8_t mac[6];

  // Current association
  bool    is_associated;
  uint8_t cur_ap[6];
  char    cur_ssid[33];
  uint8_t cur_ch;
  int8_t  cur_rssi;
  uint32_t assoc_ts;

  // History
  CatAssocEvent history[CAT_MAX_ASSOC_HIST];
  uint8_t  hist_head;
  uint8_t  hist_count;

  // Computed stats
  uint16_t roam_count;        // AP changes detected
  uint16_t reassoc_count;     // re-assoc to same AP
  uint32_t total_on_air_s;    // cumulative associated time
  uint32_t last_deauth_ts;    // last deauth/disassoc
  uint32_t last_assoc_ts;     // last association

  uint32_t first_ts;
  uint32_t last_ts;
};

static struct {
  CatClient clients[CAT_MAX_CLIENTS];
  uint8_t   count;
} g_cat = {};

static bool cat_mac_eq(const uint8_t *a, const uint8_t *b) { return memcmp(a,b,6)==0; }
static bool cat_is_bcast(const uint8_t *m) {
  return m[0]==0xFF&&m[1]==0xFF&&m[2]==0xFF&&m[3]==0xFF&&m[4]==0xFF&&m[5]==0xFF;
}

static CatClient* cat_find(const uint8_t *mac) {
  for (int i = 0; i < CAT_MAX_CLIENTS; i++)
    if (g_cat.clients[i].active && cat_mac_eq(g_cat.clients[i].mac, mac))
      return &g_cat.clients[i];
  return nullptr;
}

static CatClient* cat_alloc(const uint8_t *mac) {
  for (int i = 0; i < CAT_MAX_CLIENTS; i++) {
    if (!g_cat.clients[i].active) {
      CatClient &c = g_cat.clients[i];
      memset(&c, 0, sizeof(c));
      c.active   = true;
      memcpy(c.mac, mac, 6);
      c.first_ts = millis()/1000;
      g_cat.count++;
      return &c;
    }
  }
  uint32_t oldest = UINT32_MAX; int idx = 0;
  for (int i = 0; i < CAT_MAX_CLIENTS; i++)
    if (!g_cat.clients[i].is_associated && g_cat.clients[i].last_ts < oldest)
      { oldest = g_cat.clients[i].last_ts; idx = i; }
  memset(&g_cat.clients[idx], 0, sizeof(g_cat.clients[0]));
  g_cat.clients[idx].active = true;
  memcpy(g_cat.clients[idx].mac, mac, 6);
  g_cat.clients[idx].first_ts = millis()/1000;
  return &g_cat.clients[idx];
}

static void cat_push_history(CatClient &c, const uint8_t *ap, const char *ssid,
                               uint8_t ch, int8_t rssi, bool reassoc) {
  CatAssocEvent &e = c.history[c.hist_head % CAT_MAX_ASSOC_HIST];
  memcpy(e.ap_bssid, ap, 6);
  if (ssid) strncpy(e.ap_ssid, ssid, 32);
  e.ch    = ch;
  e.rssi  = rssi;
  e.ts    = millis()/1000;
  e.is_reassoc = reassoc;
  c.hist_head = (c.hist_head + 1) % CAT_MAX_ASSOC_HIST;
  if (c.hist_count < CAT_MAX_ASSOC_HIST) c.hist_count++;
}

void cat_ingest_assoc(const uint8_t *client_mac, const uint8_t *ap_bssid,
                       const char *ssid, uint8_t ch, int8_t rssi, bool is_reassoc) {
  if (cat_is_bcast(client_mac)) return;
  CatClient *c = cat_find(client_mac);
  if (!c) c = cat_alloc(client_mac);
  if (!c) return;

  c->last_ts = millis()/1000;
  bool same_ap = cat_mac_eq(c->cur_ap, ap_bssid);

  if (c->is_associated && !same_ap) {
    c->roam_count++;
    // Accumulate on-air time
    if (c->assoc_ts > 0)
      c->total_on_air_s += (c->last_ts - c->assoc_ts);
  } else if (same_ap && is_reassoc) {
    c->reassoc_count++;
  }

  memcpy(c->cur_ap, ap_bssid, 6);
  if (ssid && ssid[0]) strncpy(c->cur_ssid, ssid, 32);
  c->cur_ch   = ch;
  c->cur_rssi = rssi;
  c->is_associated = true;
  c->assoc_ts = c->last_ts;
  c->last_assoc_ts = c->last_ts;

  cat_push_history(*c, ap_bssid, ssid, ch, rssi, is_reassoc);
}

inline void cat_ingest_deauth(const uint8_t *client_mac, const uint8_t *ap_bssid) {
  if (cat_is_bcast(client_mac)) return;
  CatClient *c = cat_find(client_mac);
  if (!c) return;
  if (cat_mac_eq(c->cur_ap, ap_bssid) && c->is_associated) {
    c->last_deauth_ts = millis()/1000;
    c->total_on_air_s += (c->last_deauth_ts - c->assoc_ts);
    c->is_associated  = false;
    memset(c->cur_ap, 0, 6);
    c->last_ts = c->last_deauth_ts;
  }
}

static void cat_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String cat_json_all() {
  String s = "{\"count\":" + String(g_cat.count) + ",\"clients\":[";
  char mac[18], apmac[18]; bool first = true;
  for (int i = 0; i < CAT_MAX_CLIENTS; i++) {
    CatClient &c = g_cat.clients[i];
    if (!c.active) continue;
    if (!first) s += ","; first = false;
    cat_mac_str(c.mac, mac);
    cat_mac_str(c.cur_ap, apmac);
    s += "{\"mac\":\"" + String(mac) + "\"";
    s += ",\"assoc\":" + String(c.is_associated ? "true" : "false");
    s += ",\"cur_ap\":\"" + String(apmac) + "\"";
    s += ",\"cur_ssid\":\"" + String(c.cur_ssid) + "\"";
    s += ",\"cur_ch\":" + String(c.cur_ch);
    s += ",\"cur_rssi\":" + String(c.cur_rssi);
    s += ",\"roams\":" + String(c.roam_count);
    s += ",\"reassocs\":" + String(c.reassoc_count);
    s += ",\"on_air_s\":" + String(c.total_on_air_s);
    s += ",\"last_assoc\":" + String(c.last_assoc_ts);
    s += ",\"last_deauth\":" + String(c.last_deauth_ts);
    s += ",\"history\":[";
    bool fh = true;
    for (int j = 0; j < c.hist_count; j++) {
      int idx = (c.hist_head - c.hist_count + j + CAT_MAX_ASSOC_HIST) % CAT_MAX_ASSOC_HIST;
      CatAssocEvent &e = c.history[idx];
      if (!fh) s += ","; fh = false;
      cat_mac_str(e.ap_bssid, apmac);
      s += "{\"ap\":\"" + String(apmac) + "\"";
      s += ",\"ssid\":\"" + String(e.ap_ssid) + "\"";
      s += ",\"ch\":" + String(e.ch);
      s += ",\"rssi\":" + String(e.rssi);
      s += ",\"ts\":" + String(e.ts);
      s += ",\"reassoc\":" + String(e.is_reassoc ? "true" : "false") + "}";
    }
    s += "]}";
  }
  s += "]}";
  return s;
}

inline void cat_routes() {
  server.on("/api/cat/all", HTTP_GET, []() {
    server.send(200, "application/json", cat_json_all());
  });
  server.on("/api/cat/clear", HTTP_POST, []() {
    memset(&g_cat, 0, sizeof(g_cat));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

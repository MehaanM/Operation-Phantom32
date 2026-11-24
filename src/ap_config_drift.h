#pragma once
// ap_config_drift.h — AP configuration change detector.
// Tracks channel, security mode, and capability changes over time.
// Prefix: acd_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define ACD_MAX_APS       64
#define ACD_CHANGE_BUF    96
#define ACD_MAX_HIST      16  // config snapshots per AP

enum AcdChangeType : uint8_t {
  ACD_CHG_CHANNEL    = 0,
  ACD_CHG_SECURITY   = 1,
  ACD_CHG_PMF        = 2,
  ACD_CHG_WPS        = 3,
  ACD_CHG_SSID       = 4,
  ACD_CHG_BEACON_INT = 5,
  ACD_CHG_CAPABILITY = 6,
};

struct AcdSnapshot {
  uint32_t ts;
  uint8_t  ch;
  uint8_t  security;
  bool     has_pmf;
  bool     pmf_req;
  bool     has_wps;
  bool     has_ht;    // 802.11n
  bool     has_vht;   // 802.11ac
  bool     has_he;    // 802.11ax
  uint16_t beacon_int_tu;
  char     ssid[33];
};

struct AcdChange {
  bool     active;
  uint32_t ts;
  uint8_t  bssid[6];
  char     ssid[33];
  AcdChangeType type;
  char     old_val[24];
  char     new_val[24];
};

struct AcdAP {
  bool     active;
  uint8_t  bssid[6];
  char     ssid[33];
  uint8_t  ch;
  int8_t   rssi;
  uint32_t first_ts;
  uint32_t last_ts;
  uint32_t change_count;
  uint32_t beacon_count;

  // Rolling history
  AcdSnapshot hist[ACD_MAX_HIST];
  uint8_t  hist_head;
  uint8_t  hist_count;

  // Current state
  AcdSnapshot current;
  bool     initialized;
};

static struct {
  AcdAP    aps[ACD_MAX_APS];
  uint8_t  count;
  AcdChange changes[ACD_CHANGE_BUF];
  uint8_t  change_head;
  uint32_t total_changes;
} g_acd = {};

static bool acd_mac_eq(const uint8_t *a, const uint8_t *b) { return memcmp(a,b,6)==0; }

static AcdAP* acd_find(const uint8_t *bssid) {
  for (int i = 0; i < ACD_MAX_APS; i++)
    if (g_acd.aps[i].active && acd_mac_eq(g_acd.aps[i].bssid, bssid))
      return &g_acd.aps[i];
  return nullptr;
}

static AcdAP* acd_alloc(const uint8_t *bssid) {
  for (int i = 0; i < ACD_MAX_APS; i++) {
    if (!g_acd.aps[i].active) {
      AcdAP &a = g_acd.aps[i];
      memset(&a, 0, sizeof(a));
      a.active = true;
      memcpy(a.bssid, bssid, 6);
      a.first_ts = millis()/1000;
      g_acd.count++;
      return &a;
    }
  }
  uint32_t oldest = UINT32_MAX; int idx = 0;
  for (int i = 0; i < ACD_MAX_APS; i++)
    if (g_acd.aps[i].last_ts < oldest) { oldest = g_acd.aps[i].last_ts; idx = i; }
  memset(&g_acd.aps[idx], 0, sizeof(g_acd.aps[0]));
  g_acd.aps[idx].active = true;
  memcpy(g_acd.aps[idx].bssid, bssid, 6);
  g_acd.aps[idx].first_ts = millis()/1000;
  return &g_acd.aps[idx];
}

static void acd_log_change(AcdAP &a, AcdChangeType type,
                            const char *old_v, const char *new_v) {
  AcdChange &c = g_acd.changes[g_acd.change_head % ACD_CHANGE_BUF];
  c.active = true;
  c.ts = millis()/1000;
  memcpy(c.bssid, a.bssid, 6);
  strncpy(c.ssid, a.ssid, 32);
  c.type = type;
  if (old_v) strncpy(c.old_val, old_v, 23);
  if (new_v) strncpy(c.new_val, new_v, 23);
  g_acd.change_head = (g_acd.change_head + 1) % ACD_CHANGE_BUF;
  g_acd.total_changes++;
  a.change_count++;
}

static void acd_compare(AcdAP &a, const AcdSnapshot &snap) {
  AcdSnapshot &cur = a.current;

  if (snap.ch != cur.ch) {
    char ov[8], nv[8];
    snprintf(ov, 8, "CH%d", cur.ch);
    snprintf(nv, 8, "CH%d", snap.ch);
    acd_log_change(a, ACD_CHG_CHANNEL, ov, nv);
  }
  if (snap.security != cur.security) {
    const char *sec[] = {"Open","WEP","WPA","WPA2","WPA3"};
    acd_log_change(a, ACD_CHG_SECURITY,
      sec[min(cur.security,(uint8_t)4)],
      sec[min(snap.security,(uint8_t)4)]);
  }
  if (snap.has_pmf != cur.has_pmf || snap.pmf_req != cur.pmf_req) {
    char ov[12], nv[12];
    snprintf(ov, 12, "%s/%s", cur.has_pmf?"PMF":"noPMF", cur.pmf_req?"req":"opt");
    snprintf(nv, 12, "%s/%s", snap.has_pmf?"PMF":"noPMF", snap.pmf_req?"req":"opt");
    acd_log_change(a, ACD_CHG_PMF, ov, nv);
  }
  if (snap.has_wps != cur.has_wps) {
    acd_log_change(a, ACD_CHG_WPS, cur.has_wps?"WPS=on":"WPS=off",
                                    snap.has_wps?"WPS=on":"WPS=off");
  }
  if (strncmp(snap.ssid, cur.ssid, 32) != 0) {
    acd_log_change(a, ACD_CHG_SSID, cur.ssid, snap.ssid);
  }
  if (snap.beacon_int_tu != cur.beacon_int_tu && cur.beacon_int_tu != 0) {
    char ov[12], nv[12];
    snprintf(ov, 12, "%dTU", cur.beacon_int_tu);
    snprintf(nv, 12, "%dTU", snap.beacon_int_tu);
    acd_log_change(a, ACD_CHG_BEACON_INT, ov, nv);
  }
  uint8_t cap_old = (cur.has_ht?4:0)|(cur.has_vht?2:0)|(cur.has_he?1:0);
  uint8_t cap_new = (snap.has_ht?4:0)|(snap.has_vht?2:0)|(snap.has_he?1:0);
  if (cap_old != cap_new) {
    char ov[16], nv[16];
    snprintf(ov, 16, "%s%s%s", cur.has_ht?"n":"", cur.has_vht?"ac":"", cur.has_he?"ax":"");
    snprintf(nv, 16, "%s%s%s", snap.has_ht?"n":"", snap.has_vht?"ac":"", snap.has_he?"ax":"");
    acd_log_change(a, ACD_CHG_CAPABILITY, ov, nv);
  }
}

void acd_ingest(const uint8_t *bssid, const char *ssid, uint8_t ch, int8_t rssi,
                uint8_t security, bool pmf, bool pmf_req, bool wps,
                bool has_ht, bool has_vht, bool has_he, uint16_t beacon_int_tu) {
  AcdAP *a = acd_find(bssid);
  if (!a) a = acd_alloc(bssid);
  if (!a) return;

  a->last_ts = millis()/1000;
  a->beacon_count++;
  if (rssi < 0) a->rssi = rssi;
  if (ssid && ssid[0]) strncpy(a->ssid, ssid, 32);
  a->ch = ch;

  AcdSnapshot snap = {};
  snap.ts = a->last_ts;
  snap.ch = ch;
  snap.security = security;
  snap.has_pmf = pmf;
  snap.pmf_req = pmf_req;
  snap.has_wps = wps;
  snap.has_ht = has_ht;
  snap.has_vht = has_vht;
  snap.has_he = has_he;
  snap.beacon_int_tu = beacon_int_tu;
  if (ssid) strncpy(snap.ssid, ssid, 32);

  if (!a->initialized) {
    a->current = snap;
    a->initialized = true;
  } else {
    acd_compare(*a, snap);
    // Store snapshot every 60 beacons
    if (a->beacon_count % 60 == 0) {
      a->hist[a->hist_head % ACD_MAX_HIST] = snap;
      a->hist_head = (a->hist_head + 1) % ACD_MAX_HIST;
      if (a->hist_count < ACD_MAX_HIST) a->hist_count++;
    }
    a->current = snap;
  }
}

static const char* acd_type_str(AcdChangeType t) {
  switch(t) {
    case ACD_CHG_CHANNEL:    return "Channel";
    case ACD_CHG_SECURITY:   return "Security";
    case ACD_CHG_PMF:        return "PMF";
    case ACD_CHG_WPS:        return "WPS";
    case ACD_CHG_SSID:       return "SSID";
    case ACD_CHG_BEACON_INT: return "BeaconInt";
    case ACD_CHG_CAPABILITY: return "PHYCaps";
    default:                 return "Unknown";
  }
}

static void acd_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String acd_json_all() {
  String s = "{\"count\":" + String(g_acd.count);
  s += ",\"total_changes\":" + String(g_acd.total_changes);
  s += ",\"aps\":[";
  char mac[18]; bool first = true;
  for (int i = 0; i < ACD_MAX_APS; i++) {
    AcdAP &a = g_acd.aps[i];
    if (!a.active) continue;
    if (!first) s += ","; first = false;
    acd_mac_str(a.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(a.ssid) + "\"";
    s += ",\"ch\":" + String(a.ch);
    s += ",\"rssi\":" + String(a.rssi);
    s += ",\"changes\":" + String(a.change_count);
    s += ",\"beacons\":" + String(a.beacon_count);
    s += ",\"cur\":{\"sec\":" + String(a.current.security);
    s += ",\"pmf\":" + String(a.current.has_pmf?"true":"false");
    s += ",\"pmf_req\":" + String(a.current.pmf_req?"true":"false");
    s += ",\"wps\":" + String(a.current.has_wps?"true":"false");
    s += ",\"ht\":" + String(a.current.has_ht?"true":"false");
    s += ",\"vht\":" + String(a.current.has_vht?"true":"false");
    s += ",\"he\":" + String(a.current.has_he?"true":"false");
    s += ",\"bint\":" + String(a.current.beacon_int_tu) + "}}";
  }
  s += "],\"recent_changes\":[";
  uint32_t cnt = g_acd.total_changes;
  uint32_t num = min((uint32_t)ACD_CHANGE_BUF, cnt);
  uint32_t start = (cnt > ACD_CHANGE_BUF) ? g_acd.change_head : 0;
  first = true;
  for (uint32_t i = 0; i < num; i++) {
    uint32_t idx = (start + i) % ACD_CHANGE_BUF;
    AcdChange &c = g_acd.changes[idx];
    if (!c.active) continue;
    if (!first) s += ","; first = false;
    acd_mac_str(c.bssid, mac);
    s += "{\"ts\":" + String(c.ts);
    s += ",\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(c.ssid) + "\"";
    s += ",\"type\":\"" + String(acd_type_str(c.type)) + "\"";
    s += ",\"type_code\":" + String(c.type);
    s += ",\"old\":\"" + String(c.old_val) + "\"";
    s += ",\"new\":\"" + String(c.new_val) + "\"";
    s += "}";
  }
  s += "]}";
  return s;
}

inline void acd_routes() {
  server.on("/api/acd/all", HTTP_GET, []() {
    server.send(200, "application/json", acd_json_all());
  });
  server.on("/api/acd/clear", HTTP_POST, []() {
    memset(&g_acd, 0, sizeof(g_acd));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

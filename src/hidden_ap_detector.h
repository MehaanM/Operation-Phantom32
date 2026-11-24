#pragma once
// hidden_ap_detector.h — Hidden SSID detection from probe responses.
// Tracks APs that beacon with empty SSID and reveals them via probe responses.
// Prefix: had_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define HAD_MAX_APS     32
#define HAD_MAX_ALIASES  8  // per AP

struct HadAP {
  bool    active;
  uint8_t bssid[6];
  char    ssid[33];         // current known SSID (may be empty)
  bool    is_hidden;        // beacons with empty/null SSID
  bool    revealed;         // at least one real SSID discovered

  char    aliases[HAD_MAX_ALIASES][33];  // SSIDs seen in probe responses
  uint8_t alias_count;

  uint8_t channel;
  int8_t  rssi;

  uint32_t beacon_count;
  uint32_t probe_resp_count;
  uint32_t discovery_ts;    // when first real SSID was seen
  uint32_t first_ts;
  uint32_t last_ts;

  uint8_t clients_probing;  // clients actively probing this BSSID
};

static struct {
  HadAP    aps[HAD_MAX_APS];
  uint8_t  ap_count;
  uint8_t  hidden_count;
  uint8_t  revealed_count;
  uint32_t total_beacons;
  uint32_t total_probe_resps;
} g_had = {};

static bool had_mac_eq(const uint8_t *a, const uint8_t *b) { return memcmp(a, b, 6) == 0; }

static HadAP* had_find_or_alloc(const uint8_t *bssid) {
  for (int i = 0; i < HAD_MAX_APS; i++)
    if (g_had.aps[i].active && had_mac_eq(g_had.aps[i].bssid, bssid))
      return &g_had.aps[i];
  for (int i = 0; i < HAD_MAX_APS; i++) {
    if (!g_had.aps[i].active) {
      HadAP &a = g_had.aps[i];
      memset(&a, 0, sizeof(a));
      a.active  = true;
      memcpy(a.bssid, bssid, 6);
      a.rssi    = -100;
      a.first_ts = millis()/1000;
      g_had.ap_count++;
      return &a;
    }
  }
  uint32_t oldest = UINT32_MAX; int idx = 0;
  for (int i = 0; i < HAD_MAX_APS; i++)
    if (g_had.aps[i].last_ts < oldest) { oldest = g_had.aps[i].last_ts; idx = i; }
  memset(&g_had.aps[idx], 0, sizeof(g_had.aps[0]));
  g_had.aps[idx].active  = true;
  memcpy(g_had.aps[idx].bssid, bssid, 6);
  g_had.aps[idx].first_ts = millis()/1000;
  return &g_had.aps[idx];
}

static bool had_add_alias(HadAP &ap, const char *ssid) {
  if (!ssid || !ssid[0]) return false;
  for (int i = 0; i < ap.alias_count; i++)
    if (strcmp(ap.aliases[i], ssid) == 0) return false; // already known
  if (ap.alias_count >= HAD_MAX_ALIASES) return false;
  strncpy(ap.aliases[ap.alias_count], ssid, 32);
  ap.alias_count++;
  return true;
}

// Called for every beacon frame
void had_ingest_beacon(const uint8_t *bssid, const char *ssid,
                       uint8_t channel, int8_t rssi) {
  g_had.total_beacons++;
  HadAP *ap = had_find_or_alloc(bssid);
  if (!ap) return;

  ap->beacon_count++;
  ap->channel  = channel;
  ap->rssi     = rssi;
  ap->last_ts  = millis()/1000;

  bool currently_hidden = !ssid || !ssid[0];

  if (currently_hidden && !ap->is_hidden) {
    ap->is_hidden = true;
    g_had.hidden_count++;
  }

  if (!currently_hidden) {
    strncpy(ap->ssid, ssid, 32);
    ap->is_hidden = false;
    if (ap->alias_count == 0 || strcmp(ap->aliases[0], ssid) != 0) {
      had_add_alias(*ap, ssid);
    }
  }
}

// Called for every probe response frame
void had_ingest_probe_resp(const uint8_t *bssid, const char *ssid,
                            uint8_t channel, int8_t rssi) {
  g_had.total_probe_resps++;
  HadAP *ap = had_find_or_alloc(bssid);
  if (!ap) return;

  ap->probe_resp_count++;
  if (rssi > ap->rssi) ap->rssi = rssi;
  ap->last_ts = millis()/1000;

  if (ssid && ssid[0]) {
    bool is_new = had_add_alias(*ap, ssid);
    if (ap->is_hidden && is_new) {
      // First SSID revelation
      strncpy(ap->ssid, ssid, 32);
      if (!ap->revealed) {
        ap->revealed = true;
        ap->discovery_ts = millis()/1000;
        g_had.revealed_count++;
      }
    }
  }
}

static void had_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String had_json_all() {
  String s = "{\"ap_count\":" + String(g_had.ap_count);
  s += ",\"hidden_count\":" + String(g_had.hidden_count);
  s += ",\"revealed_count\":" + String(g_had.revealed_count);
  s += ",\"total_beacons\":" + String(g_had.total_beacons);
  s += ",\"total_probe_resps\":" + String(g_had.total_probe_resps);
  s += ",\"aps\":[";
  char mac[18];
  bool first = true;
  for (int i = 0; i < HAD_MAX_APS; i++) {
    HadAP &ap = g_had.aps[i];
    if (!ap.active) continue;
    if (!first) s += ",";
    first = false;
    had_mac_str(ap.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(ap.ssid) + "\"";
    s += ",\"hidden\":" + String(ap.is_hidden ? "true":"false");
    s += ",\"revealed\":" + String(ap.revealed ? "true":"false");
    s += ",\"ch\":" + String(ap.channel);
    s += ",\"rssi\":" + String(ap.rssi);
    s += ",\"beacons\":" + String(ap.beacon_count);
    s += ",\"probe_resps\":" + String(ap.probe_resp_count);
    s += ",\"discovery_ts\":" + String(ap.discovery_ts);
    s += ",\"aliases\":[";
    for (int j = 0; j < ap.alias_count; j++) {
      if (j) s += ",";
      s += "\"" + String(ap.aliases[j]) + "\"";
    }
    s += "]}";
  }
  s += "]}";
  return s;
}

inline void had_routes() {
  server.on("/api/had/all", HTTP_GET, []() {
    server.send(200, "application/json", had_json_all());
  });
  server.on("/api/had/clear", HTTP_POST, []() {
    memset(&g_had, 0, sizeof(g_had));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

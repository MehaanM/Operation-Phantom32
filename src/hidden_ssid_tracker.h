#pragma once
// hidden_ssid_tracker.h — Hidden SSID uncovering via probe response correlation.
// When a client probes for an SSID and an AP responds, we learn the BSSID→SSID mapping.
// Prefix: hst_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define HST_MAX_HIDDEN    48
#define HST_MAX_CLIENTS   16  // clients that revealed the SSID

struct HstReveal {
  uint8_t  client_mac[6];
  uint32_t ts;
};

struct HstHidden {
  bool    active;
  uint8_t bssid[6];
  char    ssid[33];           // learned SSID (empty if not yet revealed)
  bool    revealed;
  uint8_t ch;
  int8_t  rssi;
  uint32_t first_seen;
  uint32_t last_seen;
  uint32_t beacon_count;      // how many hidden beacons seen

  HstReveal reveals[HST_MAX_CLIENTS];
  uint8_t  reveal_count;
};

static struct {
  HstHidden entries[HST_MAX_HIDDEN];
  uint8_t   count;
  uint8_t   revealed_count;
} g_hst = {};

static bool hst_mac_eq(const uint8_t *a, const uint8_t *b) { return memcmp(a,b,6)==0; }

static HstHidden* hst_find(const uint8_t *bssid) {
  for (int i = 0; i < HST_MAX_HIDDEN; i++)
    if (g_hst.entries[i].active && hst_mac_eq(g_hst.entries[i].bssid, bssid))
      return &g_hst.entries[i];
  return nullptr;
}

static HstHidden* hst_alloc(const uint8_t *bssid) {
  for (int i = 0; i < HST_MAX_HIDDEN; i++) {
    if (!g_hst.entries[i].active) {
      HstHidden &h = g_hst.entries[i];
      memset(&h, 0, sizeof(h));
      h.active = true;
      memcpy(h.bssid, bssid, 6);
      h.first_seen = millis()/1000;
      g_hst.count++;
      return &h;
    }
  }
  uint32_t oldest = UINT32_MAX; int idx = 0;
  for (int i = 0; i < HST_MAX_HIDDEN; i++)
    if (g_hst.entries[i].last_seen < oldest) { oldest = g_hst.entries[i].last_seen; idx = i; }
  if (g_hst.entries[idx].revealed) g_hst.revealed_count--;
  memset(&g_hst.entries[idx], 0, sizeof(g_hst.entries[0]));
  g_hst.entries[idx].active = true;
  memcpy(g_hst.entries[idx].bssid, bssid, 6);
  g_hst.entries[idx].first_seen = millis()/1000;
  return &g_hst.entries[idx];
}

void hst_ingest_hidden_beacon(const uint8_t *bssid, uint8_t ch, int8_t rssi) {
  HstHidden *h = hst_find(bssid);
  if (!h) h = hst_alloc(bssid);
  if (!h) return;

  h->ch = ch;
  if (rssi < 0) h->rssi = rssi;
  h->last_seen = millis()/1000;
  h->beacon_count++;
}

void hst_ingest_probe_response(const uint8_t *ap_bssid, const uint8_t *client_mac,
                                const char *ssid, uint8_t ch, int8_t rssi) {
  if (!ssid || !ssid[0]) return;

  HstHidden *h = hst_find(ap_bssid);
  if (!h) {
    h = hst_alloc(ap_bssid);
    if (!h) return;
  }

  h->ch = ch;
  if (rssi < 0) h->rssi = rssi;
  h->last_seen = millis()/1000;

  if (!h->revealed) {
    strncpy(h->ssid, ssid, 32);
    h->revealed = true;
    g_hst.revealed_count++;
  }

  // Track which client triggered the reveal
  if (client_mac) {
    for (int i = 0; i < h->reveal_count; i++)
      if (hst_mac_eq(h->reveals[i].client_mac, client_mac)) return;
    if (h->reveal_count < HST_MAX_CLIENTS) {
      memcpy(h->reveals[h->reveal_count].client_mac, client_mac, 6);
      h->reveals[h->reveal_count].ts = h->last_seen;
      h->reveal_count++;
    }
  }
}

static void hst_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String hst_json_all() {
  String s = "{\"count\":" + String(g_hst.count);
  s += ",\"revealed\":" + String(g_hst.revealed_count);
  s += ",\"entries\":[";
  char mac[18]; bool first = true;
  for (int i = 0; i < HST_MAX_HIDDEN; i++) {
    HstHidden &h = g_hst.entries[i];
    if (!h.active) continue;
    if (!first) s += ","; first = false;
    hst_mac_str(h.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(h.ssid) + "\"";
    s += ",\"revealed\":" + String(h.revealed?"true":"false");
    s += ",\"ch\":" + String(h.ch);
    s += ",\"rssi\":" + String(h.rssi);
    s += ",\"beacons\":" + String(h.beacon_count);
    s += ",\"first_seen\":" + String(h.first_seen);
    s += ",\"last_seen\":" + String(h.last_seen);
    s += ",\"reveal_count\":" + String(h.reveal_count);
    s += ",\"revealers\":[";
    bool fr = true;
    for (int j = 0; j < h.reveal_count; j++) {
      if (!fr) s += ","; fr = false;
      hst_mac_str(h.reveals[j].client_mac, mac);
      s += "{\"mac\":\"" + String(mac) + "\",\"ts\":" + String(h.reveals[j].ts) + "}";
    }
    s += "]}";
  }
  s += "]}";
  return s;
}

inline void hst_routes() {
  server.on("/api/hst/all", HTTP_GET, []() {
    server.send(200, "application/json", hst_json_all());
  });
  server.on("/api/hst/clear", HTTP_POST, []() {
    memset(&g_hst, 0, sizeof(g_hst));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

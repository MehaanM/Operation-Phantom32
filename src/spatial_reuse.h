#pragma once
// spatial_reuse.h — 802.11ax BSS coloring and spatial reuse analysis.
// Tracks BSS Color IE values and estimates co-channel interference benefit.
// Prefix: sru_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define SRU_MAX_APS    48
#define SRU_MAX_COLORS 64  // BSS Color is 6-bit, 1-63 valid

struct SruAP {
  bool    active;
  uint8_t bssid[6];
  char    ssid[33];
  uint8_t channel;
  int8_t  rssi;

  uint8_t bss_color;        // 0 = not set, 1-63 = BSS color
  bool    he_capable;       // supports 802.11ax/Wi-Fi 6
  bool    color_disabled;   // BSS Color Disabled bit
  bool    color_change;     // BSS Color Change Announcement seen

  uint32_t frame_count;
  uint32_t last_ts;
  uint32_t first_ts;
};

struct SruColorStat {
  uint8_t  color;
  uint8_t  ap_count;        // how many APs share this color
  bool     collision;       // >1 AP on same channel with same color
  int8_t   strongest_rssi;  // strongest AP with this color on same channel
};

static struct {
  SruAP         aps[SRU_MAX_APS];
  uint8_t       ap_count;
  SruColorStat  colors[SRU_MAX_COLORS];

  uint8_t  he_ap_count;
  uint8_t  color_collision_count;
  uint8_t  disabled_count;
  bool     spatial_reuse_active;  // any HE AP with valid non-conflicting color
} g_sru = {};

static bool sru_mac_eq(const uint8_t *a, const uint8_t *b) { return memcmp(a, b, 6) == 0; }

static SruAP* sru_find_or_alloc(const uint8_t *bssid) {
  for (int i = 0; i < SRU_MAX_APS; i++)
    if (g_sru.aps[i].active && sru_mac_eq(g_sru.aps[i].bssid, bssid))
      return &g_sru.aps[i];
  for (int i = 0; i < SRU_MAX_APS; i++) {
    if (!g_sru.aps[i].active) {
      SruAP &a = g_sru.aps[i];
      memset(&a, 0, sizeof(a));
      a.active  = true;
      memcpy(a.bssid, bssid, 6);
      a.rssi    = -100;
      a.first_ts = millis()/1000;
      g_sru.ap_count++;
      return &a;
    }
  }
  uint32_t oldest = UINT32_MAX; int idx = 0;
  for (int i = 0; i < SRU_MAX_APS; i++)
    if (g_sru.aps[i].last_ts < oldest) { oldest = g_sru.aps[i].last_ts; idx = i; }
  memset(&g_sru.aps[idx], 0, sizeof(g_sru.aps[0]));
  g_sru.aps[idx].active = true;
  memcpy(g_sru.aps[idx].bssid, bssid, 6);
  g_sru.aps[idx].first_ts = millis()/1000;
  return &g_sru.aps[idx];
}

static void sru_recompute_color_stats() {
  memset(g_sru.colors, 0, sizeof(g_sru.colors));
  g_sru.color_collision_count = 0;

  for (int i = 0; i < SRU_MAX_APS; i++) {
    SruAP &a = g_sru.aps[i];
    if (!a.active || !a.bss_color) continue;
    int cidx = a.bss_color;
    if (cidx >= SRU_MAX_COLORS) continue;

    SruColorStat &c = g_sru.colors[cidx];
    c.color = a.bss_color;

    // Check if another AP on the same channel already has this color
    for (int j = 0; j < SRU_MAX_APS; j++) {
      if (j == i) continue;
      SruAP &b = g_sru.aps[j];
      if (!b.active || b.bss_color != a.bss_color) continue;
      if (b.channel == a.channel) {
        c.collision = true;
      }
    }

    c.ap_count++;
    if (a.rssi > c.strongest_rssi) c.strongest_rssi = a.rssi;
    if (c.collision) g_sru.color_collision_count++;
  }

  g_sru.spatial_reuse_active = (g_sru.he_ap_count > 0 &&
                                 g_sru.color_collision_count == 0 &&
                                 g_sru.disabled_count == 0);
}

void sru_ingest(const uint8_t *bssid, const char *ssid,
                uint8_t channel, int8_t rssi,
                bool he_capable, uint8_t bss_color,
                bool color_disabled, bool color_change) {
  SruAP *ap = sru_find_or_alloc(bssid);
  if (!ap) return;

  if (ssid && ssid[0]) strncpy(ap->ssid, ssid, 32);
  ap->channel       = channel;
  ap->rssi          = rssi;
  ap->last_ts       = millis()/1000;

  bool was_he = ap->he_capable;
  ap->he_capable    = he_capable;
  ap->bss_color     = bss_color;
  ap->color_disabled = color_disabled;
  ap->color_change  = color_change;
  ap->frame_count++;

  if (!was_he && he_capable) g_sru.he_ap_count++;
  if (color_disabled) g_sru.disabled_count++;

  sru_recompute_color_stats();
}

static void sru_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String sru_json_all() {
  String s = "{\"ap_count\":" + String(g_sru.ap_count);
  s += ",\"he_count\":" + String(g_sru.he_ap_count);
  s += ",\"collisions\":" + String(g_sru.color_collision_count);
  s += ",\"disabled\":" + String(g_sru.disabled_count);
  s += ",\"spatial_reuse\":" + String(g_sru.spatial_reuse_active?"true":"false");
  s += ",\"aps\":[";
  char mac[18];
  bool first = true;
  for (int i = 0; i < SRU_MAX_APS; i++) {
    SruAP &a = g_sru.aps[i];
    if (!a.active) continue;
    if (!first) s += ",";
    first = false;
    sru_mac_str(a.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(a.ssid) + "\"";
    s += ",\"ch\":" + String(a.channel);
    s += ",\"rssi\":" + String(a.rssi);
    s += ",\"he\":" + String(a.he_capable?"true":"false");
    s += ",\"color\":" + String(a.bss_color);
    s += ",\"disabled\":" + String(a.color_disabled?"true":"false");
    s += ",\"change\":" + String(a.color_change?"true":"false");
    s += "}";
  }
  s += "],\"color_stats\":[";
  first = true;
  for (int i = 1; i < SRU_MAX_COLORS; i++) {
    if (!g_sru.colors[i].ap_count) continue;
    if (!first) s += ",";
    first = false;
    SruColorStat &c = g_sru.colors[i];
    s += "{\"color\":" + String(i);
    s += ",\"aps\":" + String(c.ap_count);
    s += ",\"collision\":" + String(c.collision?"true":"false");
    s += "}";
  }
  s += "]}";
  return s;
}

inline void sru_routes() {
  server.on("/api/sru/all", HTTP_GET, []() {
    server.send(200, "application/json", sru_json_all());
  });
  server.on("/api/sru/clear", HTTP_POST, []() {
    memset(&g_sru, 0, sizeof(g_sru));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

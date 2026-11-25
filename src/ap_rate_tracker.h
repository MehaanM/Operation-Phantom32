#pragma once
// ap_rate_tracker.h — Per-AP packet and beacon rate tracker.
// Tracks pps, bps, and beacon interval per AP.
// Prefix: art_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define ART_MAX_APS 64
#define ART_WINDOW_S 10  // rolling 10-second window

struct ArtWindow {
  uint32_t pkt_count;
  uint32_t byte_count;
  uint32_t beacon_count;
  uint32_t ts;
};

struct ArtAP {
  bool active;
  uint8_t bssid[6];
  char ssid[33];
  int8_t rssi;
  uint8_t channel;

  ArtWindow win[ART_WINDOW_S];
  uint8_t win_head;

  // Current rates
  float pkt_rate;     // packets/s
  float byte_rate;    // bytes/s
  float beacon_rate;  // beacons/s

  // Anomaly flags
  bool burst_detected;  // pkt_rate > 200/s
  bool beacon_flood;    // beacon_rate > 50/s
  bool silent;          // no frames >30s

  uint32_t total_pkts;
  uint32_t total_bytes;
  uint32_t last_ts;
  uint32_t first_ts;
};

static struct {
  ArtAP aps[ART_MAX_APS];
  uint8_t ap_count;
  uint32_t tick_ts;

  uint32_t global_pkt_rate;  // sum of all AP rates
  uint32_t burst_count;      // APs in burst state
} g_art = {};

static bool art_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static ArtAP *art_find_or_alloc(const uint8_t *bssid) {
  for (int i = 0; i < ART_MAX_APS; i++)
    if (g_art.aps[i].active && art_mac_eq(g_art.aps[i].bssid, bssid))
      return &g_art.aps[i];
  for (int i = 0; i < ART_MAX_APS; i++) {
    if (!g_art.aps[i].active) {
      ArtAP &ap = g_art.aps[i];
      memset(&ap, 0, sizeof(ap));
      ap.active = true;
      memcpy(ap.bssid, bssid, 6);
      ap.first_ts = millis() / 1000;
      g_art.ap_count++;
      return &ap;
    }
  }
  uint32_t oldest = UINT32_MAX;
  int idx = 0;
  for (int i = 0; i < ART_MAX_APS; i++)
    if (g_art.aps[i].last_ts < oldest) {
      oldest = g_art.aps[i].last_ts;
      idx = i;
    }
  memset(&g_art.aps[idx], 0, sizeof(g_art.aps[0]));
  g_art.aps[idx].active = true;
  memcpy(g_art.aps[idx].bssid, bssid, 6);
  g_art.aps[idx].first_ts = millis() / 1000;
  return &g_art.aps[idx];
}

void art_tick() {
  uint32_t now = millis() / 1000;
  if (now == g_art.tick_ts)
    return;
  g_art.tick_ts = now;

  g_art.global_pkt_rate = 0;
  g_art.burst_count = 0;

  for (int i = 0; i < ART_MAX_APS; i++) {
    ArtAP &ap = g_art.aps[i];
    if (!ap.active)
      continue;

    // Advance window
    ap.win_head = (ap.win_head + 1) % ART_WINDOW_S;
    memset(&ap.win[ap.win_head], 0, sizeof(ArtWindow));
    ap.win[ap.win_head].ts = now;

    // Compute rolling rates
    uint32_t sum_pkt = 0, sum_bytes = 0, sum_beacons = 0;
    for (int j = 0; j < ART_WINDOW_S; j++) {
      sum_pkt += ap.win[j].pkt_count;
      sum_bytes += ap.win[j].byte_count;
      sum_beacons += ap.win[j].beacon_count;
    }
    ap.pkt_rate = (float)sum_pkt / ART_WINDOW_S;
    ap.byte_rate = (float)sum_bytes / ART_WINDOW_S;
    ap.beacon_rate = (float)sum_beacons / ART_WINDOW_S;

    ap.burst_detected = (ap.pkt_rate > 200.0f);
    ap.beacon_flood = (ap.beacon_rate > 50.0f);
    ap.silent = (now - ap.last_ts > 30);

    if (ap.burst_detected)
      g_art.burst_count++;
    g_art.global_pkt_rate += (uint32_t)ap.pkt_rate;
  }
}

void art_ingest(const uint8_t *bssid, const char *ssid, int8_t rssi, uint8_t channel, uint16_t len,
                bool is_beacon) {
  art_tick();

  ArtAP *ap = art_find_or_alloc(bssid);
  if (!ap)
    return;

  if (ssid && ssid[0])
    strncpy(ap->ssid, ssid, 32);
  ap->rssi = rssi;
  ap->channel = channel;
  ap->last_ts = millis() / 1000;

  ap->win[ap->win_head].pkt_count++;
  ap->win[ap->win_head].byte_count += len;
  if (is_beacon)
    ap->win[ap->win_head].beacon_count++;

  ap->total_pkts++;
  ap->total_bytes += len;
}

static void art_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String art_json_all() {
  String s = "{\"ap_count\":" + String(g_art.ap_count);
  s += ",\"global_pps\":" + String(g_art.global_pkt_rate);
  s += ",\"burst_count\":" + String(g_art.burst_count);
  s += ",\"aps\":[";
  char mac[18];
  bool first = true;
  for (int i = 0; i < ART_MAX_APS; i++) {
    ArtAP &ap = g_art.aps[i];
    if (!ap.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    art_mac_str(ap.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(ap.ssid) + "\"";
    s += ",\"ch\":" + String(ap.channel);
    s += ",\"rssi\":" + String(ap.rssi);
    s += ",\"pps\":" + String((int)ap.pkt_rate);
    s += ",\"bps\":" + String((int)ap.byte_rate);
    s += ",\"beacons\":" + String((int)ap.beacon_rate);
    s += ",\"burst\":" + String(ap.burst_detected ? "true" : "false");
    s += ",\"bflood\":" + String(ap.beacon_flood ? "true" : "false");
    s += ",\"silent\":" + String(ap.silent ? "true" : "false");
    s += ",\"total_pkts\":" + String(ap.total_pkts);
    s += "}";
  }
  s += "]}";
  return s;
}

inline void art_routes() {
  server.on("/api/art/all", HTTP_GET, []() {
    art_tick();
    server.send(200, "application/json", art_json_all());
  });
  server.on("/api/art/clear", HTTP_POST, []() {
    memset(&g_art, 0, sizeof(g_art));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

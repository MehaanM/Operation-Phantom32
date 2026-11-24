#pragma once
// ap_load_monitor.h — AP load monitoring via QoS/data frame analysis.
// Estimates per-AP channel utilization and congestion index.
// Prefix: alm_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define ALM_MAX_APS       48
#define ALM_WINDOW_S       5   // rolling window for load calculation
#define ALM_WINDOW_FRAMES 300  // expected max frames per AP in window

struct AlmAP {
  bool    active;
  uint8_t bssid[6];
  char    ssid[33];
  int8_t  rssi;
  uint8_t channel;

  // Rolling counters (ALM_WINDOW_S second buckets)
  uint16_t frames_per_s[ALM_WINDOW_S];
  uint16_t bytes_per_s[ALM_WINDOW_S];
  uint8_t  bucket_idx;
  uint32_t last_bucket_ts;

  // Totals
  uint32_t total_frames;
  uint32_t total_bytes;

  // Derived metrics
  uint8_t  load_pct;        // 0-100% utilization estimate
  uint8_t  congestion;      // 0-100 congestion index
  bool     overloaded;      // load > 80%
  uint32_t last_ts;
};

static struct {
  AlmAP   aps[ALM_MAX_APS];
  uint8_t ap_count;
  uint32_t total_frames;
} g_alm = {};

static bool alm_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static AlmAP* alm_find_or_alloc(const uint8_t *bssid) {
  for (int i = 0; i < ALM_MAX_APS; i++)
    if (g_alm.aps[i].active && alm_mac_eq(g_alm.aps[i].bssid, bssid))
      return &g_alm.aps[i];
  for (int i = 0; i < ALM_MAX_APS; i++)
    if (!g_alm.aps[i].active) {
      memset(&g_alm.aps[i], 0, sizeof(g_alm.aps[i]));
      g_alm.aps[i].active = true;
      memcpy(g_alm.aps[i].bssid, bssid, 6);
      g_alm.ap_count++;
      return &g_alm.aps[i];
    }
  // Evict least recently seen
  uint32_t oldest = UINT32_MAX;
  int idx = 0;
  for (int i = 0; i < ALM_MAX_APS; i++)
    if (g_alm.aps[i].last_ts < oldest) { oldest = g_alm.aps[i].last_ts; idx = i; }
  memset(&g_alm.aps[idx], 0, sizeof(g_alm.aps[0]));
  g_alm.aps[idx].active = true;
  memcpy(g_alm.aps[idx].bssid, bssid, 6);
  return &g_alm.aps[idx];
}

static void alm_update_load(AlmAP &ap) {
  uint32_t total_f = 0, total_b = 0;
  for (int i = 0; i < ALM_WINDOW_S; i++) {
    total_f += ap.frames_per_s[i];
    total_b += ap.bytes_per_s[i];
  }
  uint32_t avg_fps = total_f / ALM_WINDOW_S;

  // Load: frame rate relative to expected max (300 fps = busy AP on 20MHz)
  ap.load_pct = (uint8_t)((avg_fps * 100) / ALM_WINDOW_FRAMES);
  if (ap.load_pct > 100) ap.load_pct = 100;

  // Congestion index: combines load and byte rate
  uint32_t avg_bps = (total_b / ALM_WINDOW_S) * 8; // bits per sec
  uint8_t byte_load = (uint8_t)(avg_bps / 54000); // relative to 54 Mbps
  if (byte_load > 100) byte_load = 100;
  ap.congestion = (uint8_t)((ap.load_pct * 60 + byte_load * 40) / 100);
  ap.overloaded = ap.load_pct > 80;
}

void alm_ingest(const uint8_t *bssid, const char *ssid, int8_t rssi,
                uint8_t channel, uint16_t frame_len) {
  AlmAP *ap = alm_find_or_alloc(bssid);
  if (!ap) return;

  uint32_t now_s = millis() / 1000;
  if (ssid && ssid[0]) strncpy(ap->ssid, ssid, 32);
  ap->rssi    = rssi;
  ap->channel = channel;
  ap->last_ts = now_s;
  ap->total_frames++;
  ap->total_bytes += frame_len;
  g_alm.total_frames++;

  // Advance bucket if second changed
  if (now_s != ap->last_bucket_ts) {
    ap->last_bucket_ts = now_s;
    ap->bucket_idx = (ap->bucket_idx + 1) % ALM_WINDOW_S;
    ap->frames_per_s[ap->bucket_idx] = 0;
    ap->bytes_per_s[ap->bucket_idx]  = 0;
  }

  ap->frames_per_s[ap->bucket_idx]++;
  ap->bytes_per_s[ap->bucket_idx] += frame_len;

  alm_update_load(*ap);
}

static void alm_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String alm_json_all() {
  char mac[18];
  String s = "{\"count\":" + String(g_alm.ap_count);
  s += ",\"total_frames\":" + String(g_alm.total_frames) + ",\"aps\":[";
  bool first = true;
  for (int i = 0; i < ALM_MAX_APS; i++) {
    AlmAP &ap = g_alm.aps[i];
    if (!ap.active) continue;
    if (!first) s += ",";
    first = false;
    alm_mac_str(ap.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(ap.ssid) + "\"";
    s += ",\"ch\":" + String(ap.channel);
    s += ",\"rssi\":" + String(ap.rssi);
    s += ",\"load\":" + String(ap.load_pct);
    s += ",\"congestion\":" + String(ap.congestion);
    s += ",\"overloaded\":" + String(ap.overloaded?"true":"false");
    s += ",\"total_frames\":" + String(ap.total_frames) + "}";
  }
  s += "]}";
  return s;
}

static String alm_json_overloaded() {
  char mac[18];
  String s = "[";
  bool first = true;
  for (int i = 0; i < ALM_MAX_APS; i++) {
    AlmAP &ap = g_alm.aps[i];
    if (!ap.active || !ap.overloaded) continue;
    if (!first) s += ",";
    first = false;
    alm_mac_str(ap.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(ap.ssid) + "\"";
    s += ",\"load\":" + String(ap.load_pct) + "}";
  }
  s += "]";
  return s;
}

inline void alm_routes() {
  server.on("/api/alm/all", HTTP_GET, []() {
    server.send(200, "application/json", alm_json_all());
  });

  server.on("/api/alm/overloaded", HTTP_GET, []() {
    server.send(200, "application/json", alm_json_overloaded());
  });

  server.on("/api/alm/clear", HTTP_POST, []() {
    memset(&g_alm, 0, sizeof(g_alm));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

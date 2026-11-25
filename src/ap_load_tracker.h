#pragma once
// ap_load_tracker.h — AP channel utilization and load monitoring.
// Tracks per-AP frame rate, client count, and load estimates.
// Prefix: alt_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define ALT_MAX_APS 48
#define ALT_HIST_LEN 30

struct AltAP {
  bool active;
  uint8_t bssid[6];
  char ssid[33];
  uint8_t channel;
  int8_t rssi;

  uint8_t client_count;  // unique clients seen associating
  uint8_t max_clients;   // historical peak

  // Frame rates
  uint32_t frames_per_sec;  // current
  uint32_t bytes_per_sec;
  uint32_t peak_fps;
  uint32_t peak_bps;

  // Accumulator
  uint32_t acc_frames;
  uint32_t acc_bytes;
  uint32_t acc_ts;

  // Load history (0-100%)
  uint8_t load_hist[ALT_HIST_LEN];
  uint8_t load_head;

  // Computed load score (0-100)
  uint8_t load_score;

  uint32_t total_frames;
  uint64_t total_bytes;
  uint32_t last_ts;
  uint32_t first_ts;

  // Associated client MACs
  uint8_t clients[8][6];
  uint8_t client_arr_len;
};

static struct {
  AltAP aps[ALT_MAX_APS];
  uint8_t ap_count;

  uint32_t global_fps;
  uint32_t global_bps;
  uint32_t global_peak_fps;
} g_alt = {};

static bool alt_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static AltAP *alt_find_or_alloc(const uint8_t *bssid) {
  for (int i = 0; i < ALT_MAX_APS; i++)
    if (g_alt.aps[i].active && alt_mac_eq(g_alt.aps[i].bssid, bssid))
      return &g_alt.aps[i];
  for (int i = 0; i < ALT_MAX_APS; i++) {
    if (!g_alt.aps[i].active) {
      AltAP &a = g_alt.aps[i];
      memset(&a, 0, sizeof(a));
      a.active = true;
      memcpy(a.bssid, bssid, 6);
      a.rssi = -100;
      a.first_ts = millis() / 1000;
      a.acc_ts = a.first_ts;
      g_alt.ap_count++;
      return &a;
    }
  }
  uint32_t oldest = UINT32_MAX;
  int idx = 0;
  for (int i = 0; i < ALT_MAX_APS; i++)
    if (g_alt.aps[i].last_ts < oldest) {
      oldest = g_alt.aps[i].last_ts;
      idx = i;
    }
  memset(&g_alt.aps[idx], 0, sizeof(g_alt.aps[0]));
  g_alt.aps[idx].active = true;
  memcpy(g_alt.aps[idx].bssid, bssid, 6);
  g_alt.aps[idx].first_ts = millis() / 1000;
  g_alt.aps[idx].acc_ts = g_alt.aps[idx].first_ts;
  return &g_alt.aps[idx];
}

static uint8_t alt_compute_load(AltAP &a) {
  int score = 0;
  score += min(50, (int)(a.frames_per_sec * 100 / 500));  // fps contributes up to 50
  score += min(30, (int)(a.client_count * 10));           // clients up to 30
  score += min(20, (int)(a.bytes_per_sec / 50000));       // throughput up to 20
  return (uint8_t)min(100, score);
}

static void alt_tick(AltAP &a, uint32_t now) {
  if (now == a.acc_ts)
    return;
  a.acc_ts = now;

  a.frames_per_sec = a.acc_frames;
  a.bytes_per_sec = a.acc_bytes;

  if (a.frames_per_sec > a.peak_fps)
    a.peak_fps = a.frames_per_sec;
  if (a.bytes_per_sec > a.peak_bps)
    a.peak_bps = a.bytes_per_sec;

  a.load_score = alt_compute_load(a);
  a.load_hist[a.load_head] = a.load_score;
  a.load_head = (a.load_head + 1) % ALT_HIST_LEN;

  if (a.client_count > a.max_clients)
    a.max_clients = a.client_count;

  a.acc_frames = 0;
  a.acc_bytes = 0;
}

void alt_ingest_frame(const uint8_t *bssid, const char *ssid, uint8_t channel, int8_t rssi,
                      uint16_t len) {
  uint32_t now = millis() / 1000;
  AltAP *a = alt_find_or_alloc(bssid);
  if (!a)
    return;
  alt_tick(*a, now);

  if (ssid && ssid[0])
    strncpy(a->ssid, ssid, 32);
  a->channel = channel;
  if (rssi < 0)
    a->rssi = rssi;
  a->acc_frames++;
  a->acc_bytes += len;
  a->total_frames++;
  a->total_bytes += len;
  a->last_ts = now;
}

void alt_add_client(const uint8_t *bssid, const uint8_t *client_mac) {
  AltAP *a = alt_find_or_alloc(bssid);
  if (!a)
    return;

  // Check if already tracked
  for (int i = 0; i < a->client_arr_len; i++)
    if (alt_mac_eq(a->clients[i], client_mac))
      return;

  if (a->client_arr_len < 8) {
    memcpy(a->clients[a->client_arr_len], client_mac, 6);
    a->client_arr_len++;
  }
  a->client_count = a->client_arr_len;
}

static void alt_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String alt_json_all() {
  String s = "{\"ap_count\":" + String(g_alt.ap_count);
  s += ",\"aps\":[";
  char mac[18];
  bool first = true;
  for (int i = 0; i < ALT_MAX_APS; i++) {
    AltAP &a = g_alt.aps[i];
    if (!a.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    alt_mac_str(a.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(a.ssid) + "\"";
    s += ",\"ch\":" + String(a.channel);
    s += ",\"rssi\":" + String(a.rssi);
    s += ",\"fps\":" + String(a.frames_per_sec);
    s += ",\"bps\":" + String(a.bytes_per_sec);
    s += ",\"peak_fps\":" + String(a.peak_fps);
    s += ",\"clients\":" + String(a.client_count);
    s += ",\"max_clients\":" + String(a.max_clients);
    s += ",\"load\":" + String(a.load_score);
    s += ",\"total_frames\":" + String(a.total_frames);
    s += ",\"load_hist\":[";
    for (int j = 0; j < ALT_HIST_LEN; j++) {
      int pos = (a.load_head + j) % ALT_HIST_LEN;
      if (j)
        s += ",";
      s += String(a.load_hist[pos]);
    }
    s += "]}";
  }
  s += "]}";
  return s;
}

inline void alt_routes() {
  server.on("/api/alt/all", HTTP_GET,
            []() { server.send(200, "application/json", alt_json_all()); });
  server.on("/api/alt/clear", HTTP_POST, []() {
    memset(&g_alt, 0, sizeof(g_alt));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

#pragma once
// pkt_loss_tracker.h — 802.11 packet loss and retry rate tracker.
// Uses retry bit in 802.11 frame control to estimate air loss.
// Prefix: plt_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define PLT_MAX_APS 64
#define PLT_HIST_LEN 60  // 60-second rolling window

struct PltBucket {
  uint32_t total;
  uint32_t retry;
  uint32_t mcast;
  uint32_t bytes;
};

struct PltAP {
  bool active;
  uint8_t bssid[6];
  char ssid[33];
  uint8_t ch;
  int8_t rssi;
  uint32_t first_ts;
  uint32_t last_ts;

  PltBucket hist[PLT_HIST_LEN];
  uint8_t hist_head;

  PltBucket total;

  uint16_t retry_rate_x100;  // 0-10000 (percentage * 100)
  uint32_t last_tick_s;
  PltBucket cur;
};

static struct {
  PltAP aps[PLT_MAX_APS];
  uint8_t count;
  uint32_t global_total;
  uint32_t global_retry;
} g_plt = {};

static bool plt_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static PltAP *plt_find(const uint8_t *bssid) {
  for (int i = 0; i < PLT_MAX_APS; i++)
    if (g_plt.aps[i].active && plt_mac_eq(g_plt.aps[i].bssid, bssid))
      return &g_plt.aps[i];
  return nullptr;
}

static PltAP *plt_alloc(const uint8_t *bssid) {
  for (int i = 0; i < PLT_MAX_APS; i++) {
    if (!g_plt.aps[i].active) {
      PltAP &a = g_plt.aps[i];
      memset(&a, 0, sizeof(a));
      a.active = true;
      memcpy(a.bssid, bssid, 6);
      a.first_ts = millis() / 1000;
      g_plt.count++;
      return &a;
    }
  }
  uint32_t oldest = UINT32_MAX;
  int idx = 0;
  for (int i = 0; i < PLT_MAX_APS; i++)
    if (g_plt.aps[i].last_ts < oldest) {
      oldest = g_plt.aps[i].last_ts;
      idx = i;
    }
  memset(&g_plt.aps[idx], 0, sizeof(g_plt.aps[0]));
  g_plt.aps[idx].active = true;
  memcpy(g_plt.aps[idx].bssid, bssid, 6);
  g_plt.aps[idx].first_ts = millis() / 1000;
  return &g_plt.aps[idx];
}

static void plt_tick_ap(PltAP &a) {
  uint32_t now_s = millis() / 1000;
  if (now_s == a.last_tick_s)
    return;
  a.last_tick_s = now_s;
  a.hist[a.hist_head % PLT_HIST_LEN] = a.cur;
  a.hist_head = (a.hist_head + 1) % PLT_HIST_LEN;
  a.total.total += a.cur.total;
  a.total.retry += a.cur.retry;
  a.total.bytes += a.cur.bytes;
  if (a.cur.total > 0)
    a.retry_rate_x100 = (uint16_t)((a.cur.retry * 10000UL) / a.cur.total);
  memset(&a.cur, 0, sizeof(a.cur));
}

void plt_ingest(const uint8_t *bssid, const char *ssid, uint8_t ch, int8_t rssi, bool is_retry,
                bool is_mcast, uint16_t len) {
  PltAP *a = plt_find(bssid);
  if (!a)
    a = plt_alloc(bssid);
  if (!a)
    return;

  if (ssid && ssid[0])
    strncpy(a->ssid, ssid, 32);
  a->ch = ch;
  if (rssi < 0)
    a->rssi = rssi;
  a->last_ts = millis() / 1000;

  plt_tick_ap(*a);
  a->cur.total++;
  a->cur.bytes += len;
  if (is_retry) {
    a->cur.retry++;
    g_plt.global_retry++;
  }
  if (is_mcast)
    a->cur.mcast++;
  g_plt.global_total++;
}

static void plt_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String plt_hist_json(const PltAP &a, bool retry_mode) {
  String s = "[";
  for (int i = 0; i < PLT_HIST_LEN; i++) {
    int idx = (a.hist_head + i) % PLT_HIST_LEN;
    if (i)
      s += ",";
    const PltBucket &b = a.hist[idx];
    if (retry_mode)
      s += String(b.total > 0 ? (b.retry * 100 / b.total) : 0);
    else
      s += String(b.total);
  }
  return s + "]";
}

static String plt_json_all() {
  uint32_t global_rate =
      g_plt.global_total > 0 ? (g_plt.global_retry * 100 / g_plt.global_total) : 0;
  String s = "{\"ap_count\":" + String(g_plt.count);
  s += ",\"global_total\":" + String(g_plt.global_total);
  s += ",\"global_retry\":" + String(g_plt.global_retry);
  s += ",\"global_retry_pct\":" + String(global_rate);
  s += ",\"aps\":[";
  char mac[18];
  bool first = true;
  for (int i = 0; i < PLT_MAX_APS; i++) {
    PltAP &a = g_plt.aps[i];
    if (!a.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    plt_mac_str(a.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(a.ssid) + "\"";
    s += ",\"ch\":" + String(a.ch);
    s += ",\"rssi\":" + String(a.rssi);
    s += ",\"total\":" + String(a.total.total);
    s += ",\"retry\":" + String(a.total.retry);
    s += ",\"retry_rate\":" + String(a.retry_rate_x100);
    s += ",\"bytes\":" + String(a.total.bytes);
    s += ",\"hist_total\":" + plt_hist_json(a, false);
    s += ",\"hist_retry\":" + plt_hist_json(a, true);
    s += "}";
  }
  s += "]}";
  return s;
}

inline void plt_routes() {
  server.on("/api/plt/all", HTTP_GET, []() {
    for (int i = 0; i < PLT_MAX_APS; i++)
      if (g_plt.aps[i].active)
        plt_tick_ap(g_plt.aps[i]);
    server.send(200, "application/json", plt_json_all());
  });
  server.on("/api/plt/clear", HTTP_POST, []() {
    memset(&g_plt, 0, sizeof(g_plt));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

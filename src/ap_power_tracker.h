#pragma once
// ap_power_tracker.h — AP transmit power estimation via received RSSI and path-loss model.
// Estimates TX power using free-space path-loss model: FSPL = 20log10(d) + 20log10(f) + 20log10(4π/c)
// At 2.4GHz, FSPL(1m) ≈ 40 dBm. RSSI = TXPower - FSPL(d). We invert at known distances.
// Prefix: app_ (AP Power)

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define APP_MAX_APS      64
#define APP_HIST_LEN     60
#define APP_FREQ_MHZ     2437  // mid-band 2.4GHz default

struct AppAP {
  bool    active;
  uint8_t bssid[6];
  char    ssid[33];
  uint8_t ch;
  int8_t  rssi;
  uint32_t first_ts;
  uint32_t last_ts;
  uint32_t beacon_count;

  // RSSI history for smoothing
  int8_t   rssi_hist[APP_HIST_LEN];
  uint8_t  rssi_head;
  uint8_t  rssi_count;

  // Computed
  int8_t   rssi_avg;
  int8_t   rssi_min;
  int8_t   rssi_max;
  int8_t   rssi_sigma;    // standard deviation * 2 (approximate, stored as int8)

  // Power estimation (assumes unknown distance, relative comparison)
  // relative_power: normalized 0-100 within the scan set
  uint8_t  relative_power;

  // Trend
  int8_t   trend_delta;   // last_avg - prev_avg (positive = getting stronger)
};

static struct {
  AppAP   aps[APP_MAX_APS];
  uint8_t count;
  uint32_t total_beacons;
} g_app = {};

static bool app_mac_eq(const uint8_t *a, const uint8_t *b) { return memcmp(a,b,6)==0; }

static AppAP* app_find(const uint8_t *bssid) {
  for (int i = 0; i < APP_MAX_APS; i++)
    if (g_app.aps[i].active && app_mac_eq(g_app.aps[i].bssid, bssid))
      return &g_app.aps[i];
  return nullptr;
}

static AppAP* app_alloc(const uint8_t *bssid) {
  for (int i = 0; i < APP_MAX_APS; i++) {
    if (!g_app.aps[i].active) {
      AppAP &a = g_app.aps[i];
      memset(&a, 0, sizeof(a));
      a.active = true;
      memcpy(a.bssid, bssid, 6);
      a.rssi_min = 0;
      a.first_ts = millis()/1000;
      g_app.count++;
      return &a;
    }
  }
  uint32_t oldest = UINT32_MAX; int idx = 0;
  for (int i = 0; i < APP_MAX_APS; i++)
    if (g_app.aps[i].last_ts < oldest) { oldest = g_app.aps[i].last_ts; idx = i; }
  memset(&g_app.aps[idx], 0, sizeof(g_app.aps[0]));
  g_app.aps[idx].active = true;
  memcpy(g_app.aps[idx].bssid, bssid, 6);
  g_app.aps[idx].rssi_min = 0;
  g_app.aps[idx].first_ts = millis()/1000;
  return &g_app.aps[idx];
}

static void app_compute_stats(AppAP &a) {
  uint8_t cnt = a.rssi_count;
  if (!cnt) return;
  int16_t sum = 0; int8_t mn = 0, mx = -127;
  for (int i = 0; i < cnt; i++) {
    int8_t v = a.rssi_hist[i];
    sum += v;
    if (v < mn || mn == 0) mn = v;
    if (v > mx) mx = v;
  }
  int8_t prev_avg = a.rssi_avg;
  a.rssi_avg = (int8_t)(sum / cnt);
  a.rssi_min = mn;
  a.rssi_max = mx;
  a.trend_delta = a.rssi_avg - prev_avg;

  // Approximate sigma (mean absolute deviation * 1.25)
  int16_t mad = 0;
  for (int i = 0; i < cnt; i++)
    mad += abs(a.rssi_hist[i] - a.rssi_avg);
  a.rssi_sigma = (int8_t)((mad * 5 / 4) / cnt);
}

static void app_update_relative(void) {
  int8_t best = -127, worst = 0;
  for (int i = 0; i < APP_MAX_APS; i++) {
    if (!g_app.aps[i].active) continue;
    if (g_app.aps[i].rssi_avg > best) best = g_app.aps[i].rssi_avg;
    if (g_app.aps[i].rssi_avg < worst || worst == 0) worst = g_app.aps[i].rssi_avg;
  }
  int8_t span = best - worst;
  if (span < 1) span = 1;
  for (int i = 0; i < APP_MAX_APS; i++) {
    if (!g_app.aps[i].active) continue;
    int pct = ((int)(g_app.aps[i].rssi_avg - worst) * 100) / span;
    g_app.aps[i].relative_power = (uint8_t)max(0, min(100, pct));
  }
}

void app_ingest_beacon(const uint8_t *bssid, const char *ssid, uint8_t ch, int8_t rssi) {
  if (rssi >= 0) return;
  AppAP *a = app_find(bssid);
  if (!a) a = app_alloc(bssid);
  if (!a) return;

  if (ssid && ssid[0]) strncpy(a->ssid, ssid, 32);
  a->ch = ch;
  a->rssi = rssi;
  a->last_ts = millis()/1000;
  a->beacon_count++;
  g_app.total_beacons++;

  a->rssi_hist[a->rssi_head % APP_HIST_LEN] = rssi;
  a->rssi_head = (a->rssi_head + 1) % APP_HIST_LEN;
  if (a->rssi_count < APP_HIST_LEN) a->rssi_count++;

  if (a->beacon_count % 5 == 0) {
    app_compute_stats(*a);
    app_update_relative();
  }
}

static void app_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String app_hist_json(const AppAP &a) {
  String s = "[";
  uint8_t cnt = a.rssi_count;
  for (int i = 0; i < cnt; i++) {
    int idx = ((int)a.rssi_head - cnt + i + APP_HIST_LEN) % APP_HIST_LEN;
    if (i) s += ",";
    s += String(a.rssi_hist[idx]);
  }
  return s + "]";
}

static String app_json_all() {
  String s = "{\"count\":" + String(g_app.count);
  s += ",\"total_beacons\":" + String(g_app.total_beacons);
  s += ",\"aps\":[";
  char mac[18]; bool first = true;
  for (int i = 0; i < APP_MAX_APS; i++) {
    AppAP &a = g_app.aps[i];
    if (!a.active) continue;
    if (!first) s += ","; first = false;
    app_mac_str(a.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(a.ssid) + "\"";
    s += ",\"ch\":" + String(a.ch);
    s += ",\"rssi\":" + String(a.rssi);
    s += ",\"rssi_avg\":" + String(a.rssi_avg);
    s += ",\"rssi_min\":" + String(a.rssi_min);
    s += ",\"rssi_max\":" + String(a.rssi_max);
    s += ",\"rssi_sigma\":" + String(a.rssi_sigma);
    s += ",\"trend\":" + String(a.trend_delta);
    s += ",\"relative_power\":" + String(a.relative_power);
    s += ",\"beacons\":" + String(a.beacon_count);
    s += ",\"samples\":" + String(a.rssi_count);
    s += ",\"hist\":" + app_hist_json(a);
    s += "}";
  }
  s += "]}";
  return s;
}

inline void app_routes() {
  server.on("/api/app/all", HTTP_GET, []() {
    server.send(200, "application/json", app_json_all());
  });
  server.on("/api/app/clear", HTTP_POST, []() {
    memset(&g_app, 0, sizeof(g_app));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

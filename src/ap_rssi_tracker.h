#pragma once
// ap_rssi_tracker.h — Detailed RSSI tracking per AP with trend analysis.
// Maintains RSSI history, trend direction, stability scoring.
// Prefix: art_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define ART_MAX_APS    64
#define ART_HIST_LEN   120  // 120-second RSSI history (EMA-smoothed)
#define ART_TREND_WIN  10   // window for trend detection

// Trend direction
enum ArtTrend : uint8_t {
  ART_STABLE   = 0,
  ART_RISING   = 1,  // RSSI getting stronger
  ART_FALLING  = 2,  // RSSI getting weaker
  ART_UNKNOWN  = 3,
};

struct ArtAP {
  bool     active;
  uint8_t  bssid[6];
  char     ssid[33];
  uint8_t  channel;

  // Raw and smoothed RSSI
  int8_t   rssi_raw;
  int8_t   rssi_smooth;      // EMA-smoothed (alpha=0.3)
  int8_t   rssi_min;
  int8_t   rssi_max;
  int16_t  rssi_sum;
  uint16_t rssi_count;
  int8_t   rssi_avg;

  // 120-second history ring (smoothed values)
  int8_t   hist[ART_HIST_LEN];
  uint8_t  hist_head;
  uint32_t hist_tick_ts;     // last tick second

  // Trend analysis
  ArtTrend trend;
  int8_t   trend_delta;      // delta over last trend window (dBm)
  uint8_t  stability;        // 0-100 (100 = rock stable)
  uint8_t  jitter;           // sigma estimate (dBm)
  uint16_t above_threshold;  // samples above -70 dBm
  uint16_t below_threshold;  // samples below -80 dBm

  uint32_t sample_count;
  uint32_t first_ts;
  uint32_t last_ts;
};

static struct {
  ArtAP   aps[ART_MAX_APS];
  uint8_t ap_count;
  uint32_t total_samples;
} g_art = {};

static bool art_mac_eq(const uint8_t *a, const uint8_t *b) { return memcmp(a, b, 6) == 0; }

static ArtAP* art_find_or_alloc(const uint8_t *bssid) {
  for (int i = 0; i < ART_MAX_APS; i++)
    if (g_art.aps[i].active && art_mac_eq(g_art.aps[i].bssid, bssid))
      return &g_art.aps[i];
  for (int i = 0; i < ART_MAX_APS; i++) {
    if (!g_art.aps[i].active) {
      ArtAP &a = g_art.aps[i];
      memset(&a, 0, sizeof(a));
      a.active  = true;
      memcpy(a.bssid, bssid, 6);
      a.rssi_min = -10;
      a.rssi_max = -100;
      a.first_ts = millis()/1000;
      g_art.ap_count++;
      return &a;
    }
  }
  uint32_t oldest = UINT32_MAX; int idx = 0;
  for (int i = 0; i < ART_MAX_APS; i++)
    if (g_art.aps[i].last_ts < oldest) { oldest = g_art.aps[i].last_ts; idx = i; }
  memset(&g_art.aps[idx], 0, sizeof(g_art.aps[0]));
  g_art.aps[idx].active  = true;
  memcpy(g_art.aps[idx].bssid, bssid, 6);
  g_art.aps[idx].rssi_min = -10;
  g_art.aps[idx].rssi_max = -100;
  g_art.aps[idx].first_ts = millis()/1000;
  return &g_art.aps[idx];
}

static ArtTrend art_compute_trend(const int8_t *hist, uint8_t head, int *delta_out) {
  // Compare mean of last 5 samples vs prior 5 samples
  int32_t recent = 0, older = 0, cnt_r = 0, cnt_o = 0;
  for (int i = 0; i < ART_TREND_WIN; i++) {
    int pos_r = ((int)head - 1 - i + ART_HIST_LEN) % ART_HIST_LEN;
    int pos_o = ((int)head - 1 - i - ART_TREND_WIN/2 + ART_HIST_LEN) % ART_HIST_LEN;
    if (hist[pos_r]) { recent += hist[pos_r]; cnt_r++; }
    if (hist[pos_o]) { older  += hist[pos_o]; cnt_o++; }
  }
  if (!cnt_r || !cnt_o) { *delta_out = 0; return ART_UNKNOWN; }
  int delta = (int)(recent / cnt_r) - (int)(older / cnt_o);
  *delta_out = delta;
  if (delta >  2) return ART_RISING;
  if (delta < -2) return ART_FALLING;
  return ART_STABLE;
}

static uint8_t art_compute_stability(const int8_t *hist, uint8_t head, uint8_t *jitter_out) {
  int32_t sum = 0; int cnt = 0;
  for (int i = 0; i < 30 && i < ART_HIST_LEN; i++) {
    int pos = ((int)head - 1 - i + ART_HIST_LEN) % ART_HIST_LEN;
    if (hist[pos]) { sum += hist[pos]; cnt++; }
  }
  if (!cnt) { *jitter_out = 0; return 100; }
  int mean = sum / cnt;
  int32_t var = 0;
  for (int i = 0; i < 30 && i < ART_HIST_LEN; i++) {
    int pos = ((int)head - 1 - i + ART_HIST_LEN) % ART_HIST_LEN;
    if (hist[pos]) { int d = hist[pos] - mean; var += d*d; }
  }
  int sigma = (int)sqrt((float)var / cnt);
  *jitter_out = (uint8_t)min(sigma, 20);
  return (uint8_t)max(0, 100 - sigma * 8);
}

void art_ingest(const uint8_t *bssid, const char *ssid,
                uint8_t channel, int8_t rssi) {
  if (rssi >= 0 || rssi < -110) return;

  uint32_t now_s = millis() / 1000;
  ArtAP *a = art_find_or_alloc(bssid);
  if (!a) return;

  if (ssid && ssid[0] && !a->ssid[0]) strncpy(a->ssid, ssid, 32);
  a->channel = channel;
  a->rssi_raw = rssi;

  // EMA smooth
  if (!a->rssi_smooth) a->rssi_smooth = rssi;
  a->rssi_smooth = (int8_t)((a->rssi_smooth * 7 + rssi) / 8);

  // Min/max/avg
  if (rssi > a->rssi_min) a->rssi_min = rssi;
  if (rssi < a->rssi_max) a->rssi_max = rssi;
  a->rssi_sum += rssi;
  a->rssi_count++;
  if (a->rssi_count) a->rssi_avg = (int8_t)(a->rssi_sum / a->rssi_count);

  // Threshold counters
  if (rssi >= -70) a->above_threshold++;
  if (rssi <= -80) a->below_threshold++;

  // History tick
  if (a->hist_tick_ts != now_s) {
    a->hist[a->hist_head] = a->rssi_smooth;
    a->hist_head = (a->hist_head + 1) % ART_HIST_LEN;
    a->hist_tick_ts = now_s;

    // Recompute trend and stability periodically
    int delta = 0;
    a->trend = art_compute_trend(a->hist, a->hist_head, &delta);
    a->trend_delta = (int8_t)delta;
    uint8_t jitter = 0;
    a->stability = art_compute_stability(a->hist, a->hist_head, &jitter);
    a->jitter = jitter;
  }

  a->sample_count++;
  a->last_ts = now_s;
  g_art.total_samples++;
}

static void art_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String art_json_all() {
  String s = "{\"ap_count\":" + String(g_art.ap_count);
  s += ",\"total_samples\":" + String(g_art.total_samples);
  s += ",\"aps\":[";
  char mac[18]; bool first = true;
  static const char* const trend_str[] = {"stable","rising","falling","unknown"};
  for (int i = 0; i < ART_MAX_APS; i++) {
    ArtAP &a = g_art.aps[i];
    if (!a.active) continue;
    if (!first) s += ","; first = false;
    art_mac_str(a.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(a.ssid) + "\"";
    s += ",\"ch\":" + String(a.channel);
    s += ",\"rssi\":" + String(a.rssi_smooth);
    s += ",\"rssi_raw\":" + String(a.rssi_raw);
    s += ",\"rssi_min\":" + String(a.rssi_max);
    s += ",\"rssi_max\":" + String(a.rssi_min);
    s += ",\"rssi_avg\":" + String(a.rssi_avg);
    s += ",\"trend\":\"" + String(trend_str[a.trend]) + "\"";
    s += ",\"trend_delta\":" + String(a.trend_delta);
    s += ",\"stability\":" + String(a.stability);
    s += ",\"jitter\":" + String(a.jitter);
    s += ",\"samples\":" + String(a.sample_count);
    s += ",\"above70\":" + String(a.above_threshold);
    s += ",\"below80\":" + String(a.below_threshold);
    s += ",\"hist\":[";
    for (int j = 0; j < ART_HIST_LEN; j++) {
      int pos = (a.hist_head + j) % ART_HIST_LEN;
      if (j) s += ",";
      s += String(a.hist[pos]);
    }
    s += "]}";
  }
  s += "]}";
  return s;
}

inline void art_routes() {
  server.on("/api/art/all", HTTP_GET, []() {
    server.send(200, "application/json", art_json_all());
  });
  server.on("/api/art/clear", HTTP_POST, []() {
    memset(&g_art, 0, sizeof(g_art));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

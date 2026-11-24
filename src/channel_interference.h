#pragma once
// channel_interference.h — Per-channel interference score tracker.
// Models overlapping channel energy for 2.4 GHz 20 MHz channels (1-13).
// Prefix: chi_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

// 2.4 GHz channel overlap matrix (20 MHz BW). Channels within 5 of each other
// share significant spectral overlap. The penalty is modeled as 100% for direct
// neighbors and decreasing linearly.
// chi[i][j] = overlap weight (0-100) between channels i+1 and j+1
static const uint8_t CHI_OVERLAP[13][13] = {
//  1    2    3    4    5    6    7    8    9   10   11   12   13
  {100, 80,  60,  40,  20,   0,   0,   0,   0,   0,   0,   0,   0}, // ch1
  { 80,100,  80,  60,  40,  20,   0,   0,   0,   0,   0,   0,   0}, // ch2
  { 60, 80, 100,  80,  60,  40,  20,   0,   0,   0,   0,   0,   0}, // ch3
  { 40, 60,  80, 100,  80,  60,  40,  20,   0,   0,   0,   0,   0}, // ch4
  { 20, 40,  60,  80, 100,  80,  60,  40,  20,   0,   0,   0,   0}, // ch5
  {  0, 20,  40,  60,  80, 100,  80,  60,  40,  20,   0,   0,   0}, // ch6
  {  0,  0,  20,  40,  60,  80, 100,  80,  60,  40,  20,   0,   0}, // ch7
  {  0,  0,   0,  20,  40,  60,  80, 100,  80,  60,  40,  20,   0}, // ch8
  {  0,  0,   0,   0,  20,  40,  60,  80, 100,  80,  60,  40,  20}, // ch9
  {  0,  0,   0,   0,   0,  20,  40,  60,  80, 100,  80,  60,  40}, // ch10
  {  0,  0,   0,   0,   0,   0,  20,  40,  60,  80, 100,  80,  60}, // ch11
  {  0,  0,   0,   0,   0,   0,   0,  20,  40,  60,  80, 100,  80}, // ch12
  {  0,  0,   0,   0,   0,   0,   0,   0,  20,  40,  60,  80, 100}, // ch13
};

#define CHI_MAX_CHANNELS  13
#define CHI_HIST_LEN      30

struct ChiChannel {
  uint8_t  channel;
  uint8_t  ap_count;        // APs seen on this channel
  int8_t   strongest_rssi;  // strongest AP on this channel
  uint8_t  utilization;     // 0-100 estimated utilization
  uint8_t  interference;    // 0-100 weighted interference from other channels
  uint8_t  score;           // 0-100 overall channel quality (higher = better)

  uint32_t frame_count;
  uint32_t retry_count;
  uint8_t  retry_pct;       // retry percentage (quality indicator)

  uint8_t  hist[CHI_HIST_LEN]; // interference history ring
  uint8_t  hist_head;

  uint32_t last_update;
};

static struct {
  ChiChannel channels[CHI_MAX_CHANNELS];
  uint8_t    best_channel;
  uint8_t    best_score;
  uint32_t   last_full_scan;
} g_chi = {};

static void chi_init() {
  for (int i = 0; i < CHI_MAX_CHANNELS; i++) {
    g_chi.channels[i].channel = i + 1;
    g_chi.channels[i].strongest_rssi = -100;
    g_chi.channels[i].score  = 100; // start optimistic
  }
}

static void chi_recompute_scores() {
  g_chi.best_score   = 0;
  g_chi.best_channel = 1;

  for (int i = 0; i < CHI_MAX_CHANNELS; i++) {
    ChiChannel &c = g_chi.channels[i];

    // Interference score: weighted sum from all other channels
    uint32_t weighted_interference = 0;
    uint32_t total_weight          = 0;

    for (int j = 0; j < CHI_MAX_CHANNELS; j++) {
      if (i == j) continue;
      uint8_t overlap = CHI_OVERLAP[i][j];
      if (!overlap) continue;
      // Interference contribution from channel j
      uint32_t contrib = (uint32_t)c.utilization * overlap / 100;
      weighted_interference += contrib;
      total_weight += overlap;
    }

    c.interference = (uint8_t)min((uint32_t)100, weighted_interference / max(total_weight, (uint32_t)1));

    // Channel quality score (higher = better)
    // Penalty: AP count (congestion), retry rate, interference
    int score = 100;
    score -= c.ap_count * 5;         // each AP costs 5 points
    score -= (int)c.retry_pct;       // retry percentage as direct penalty
    score -= c.interference / 2;     // half the interference score
    if (score < 0) score = 0;
    c.score = (uint8_t)score;

    // History ring
    c.hist[c.hist_head] = c.interference;
    c.hist_head = (c.hist_head + 1) % CHI_HIST_LEN;

    if (c.score > g_chi.best_score) {
      g_chi.best_score   = c.score;
      g_chi.best_channel = c.channel;
    }
  }
}

void chi_ingest_ap(uint8_t channel, int8_t rssi) {
  if (channel < 1 || channel > CHI_MAX_CHANNELS) return;
  ChiChannel &c = g_chi.channels[channel - 1];
  c.ap_count++;
  if (rssi > c.strongest_rssi) c.strongest_rssi = rssi;

  // Estimate utilization from RSSI (simple model)
  // RSSI of -50 → 90% util, -70 → 50%, -90 → 20%
  int util = 0;
  if      (rssi > -50) util = 90;
  else if (rssi > -60) util = 70;
  else if (rssi > -70) util = 50;
  else if (rssi > -80) util = 30;
  else                 util = 20;

  c.utilization = (uint8_t)min(100, (int)c.utilization + util / max(1, (int)c.ap_count));
  c.last_update = millis() / 1000;
}

void chi_ingest_frame(uint8_t channel, bool retry) {
  if (channel < 1 || channel > CHI_MAX_CHANNELS) return;
  ChiChannel &c = g_chi.channels[channel - 1];
  c.frame_count++;
  if (retry) c.retry_count++;
  if (c.frame_count > 100)
    c.retry_pct = (uint8_t)(c.retry_count * 100 / c.frame_count);
  chi_recompute_scores();
}

static String chi_json_all() {
  String s = "{\"best_channel\":" + String(g_chi.best_channel);
  s += ",\"best_score\":" + String(g_chi.best_score);
  s += ",\"channels\":[";
  for (int i = 0; i < CHI_MAX_CHANNELS; i++) {
    ChiChannel &c = g_chi.channels[i];
    if (i) s += ",";
    s += "{\"ch\":" + String(c.channel);
    s += ",\"aps\":" + String(c.ap_count);
    s += ",\"rssi\":" + String(c.strongest_rssi);
    s += ",\"util\":" + String(c.utilization);
    s += ",\"interference\":" + String(c.interference);
    s += ",\"score\":" + String(c.score);
    s += ",\"frames\":" + String(c.frame_count);
    s += ",\"retry_pct\":" + String(c.retry_pct);
    s += ",\"hist\":[";
    for (int j = 0; j < CHI_HIST_LEN; j++) {
      int pos = (c.hist_head + j) % CHI_HIST_LEN;
      if (j) s += ",";
      s += String(c.hist[pos]);
    }
    s += "]}";
  }
  s += "]}";
  return s;
}

inline void chi_routes() {
  server.on("/api/chi/all", HTTP_GET, []() {
    server.send(200, "application/json", chi_json_all());
  });
  server.on("/api/chi/reset", HTTP_POST, []() {
    memset(&g_chi, 0, sizeof(g_chi));
    chi_init();
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

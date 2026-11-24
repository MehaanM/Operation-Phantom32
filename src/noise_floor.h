#pragma once
// noise_floor.h — Per-channel noise floor estimation from RSSI distribution.
// Uses the lower percentile (10th pct) of RSSI samples as noise floor proxy.
// Low RSSI frames near the noise floor reveal channel congestion and
// interference sources.
// Prefix: nfl_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define NFL_MAX_CHANNELS 16
#define NFL_RSSI_BUCKETS 50   // -100 to -50 dBm, 1dBm/bucket
#define NFL_HIST_LEN     60

// Supported channel numbers (2.4GHz 1-14, 5GHz spot channels)
static const uint8_t NFL_CH_TABLE[] = {
  1,2,3,4,5,6,7,8,9,10,11,12,13,14,36,40
};

struct NflChannel {
  bool    active;
  uint8_t ch;

  // RSSI histogram: index 0 = -100dBm, index 49 = -51dBm
  uint32_t rssi_hist[NFL_RSSI_BUCKETS];
  uint32_t total_samples;

  // Computed stats (updated every 10 samples)
  int8_t  noise_floor;    // 10th percentile RSSI
  int8_t  median_rssi;    // 50th percentile RSSI
  int8_t  peak_rssi;      // best seen RSSI (90th pct)
  uint8_t congestion_pct; // % of samples above -70dBm (strong signals = busy ch)

  // 60s rolling sample counts
  uint16_t sample_hist[NFL_HIST_LEN];
  uint8_t  hist_head;
  uint16_t cur_samples;
  uint32_t last_tick_s;

  // Frame type breakdown
  uint32_t mgmt_frames;
  uint32_t data_frames;
  uint32_t ctrl_frames;

  uint32_t last_ts;
};

static struct {
  NflChannel channels[NFL_MAX_CHANNELS];
  uint8_t    count;
} g_nfl = {};

static NflChannel* nfl_find(uint8_t ch) {
  for (int i = 0; i < NFL_MAX_CHANNELS; i++)
    if (g_nfl.channels[i].active && g_nfl.channels[i].ch == ch)
      return &g_nfl.channels[i];
  return nullptr;
}

static NflChannel* nfl_alloc(uint8_t ch) {
  for (int i = 0; i < NFL_MAX_CHANNELS; i++) {
    if (!g_nfl.channels[i].active) {
      NflChannel &n = g_nfl.channels[i];
      memset(&n, 0, sizeof(n));
      n.active = true;
      n.ch = ch;
      n.noise_floor = -95;
      n.median_rssi = -75;
      n.peak_rssi   = -60;
      g_nfl.count++;
      return &n;
    }
  }
  return nullptr; // no eviction — channels are fixed
}

static void nfl_compute_stats(NflChannel &n) {
  if (n.total_samples == 0) return;
  uint32_t cum = 0;
  int8_t pct10 = -100, pct50 = -100, pct90 = -100;
  uint32_t above70 = 0;

  for (int i = 0; i < NFL_RSSI_BUCKETS; i++) {
    uint32_t prev = cum;
    cum += n.rssi_hist[i];
    int8_t dbm = -100 + i;
    if (prev * 10 < n.total_samples * 1 && cum * 10 >= n.total_samples * 1) pct10 = dbm;
    if (prev * 2  < n.total_samples     && cum * 2  >= n.total_samples)     pct50 = dbm;
    if (prev * 10 < n.total_samples * 9 && cum * 10 >= n.total_samples * 9) pct90 = dbm;
    if (dbm >= -70) above70 += n.rssi_hist[i];
  }
  n.noise_floor    = pct10;
  n.median_rssi    = pct50;
  n.peak_rssi      = pct90;
  n.congestion_pct = (uint8_t)((above70 * 100) / n.total_samples);
}

static void nfl_tick(NflChannel &n) {
  uint32_t now_s = millis()/1000;
  if (now_s == n.last_tick_s) return;
  uint32_t elapsed = now_s - n.last_tick_s;
  if (elapsed > NFL_HIST_LEN) elapsed = NFL_HIST_LEN;
  for (uint32_t i = 0; i < elapsed; i++) {
    n.sample_hist[n.hist_head % NFL_HIST_LEN] = (i == 0) ? n.cur_samples : 0;
    n.hist_head = (n.hist_head + 1) % NFL_HIST_LEN;
  }
  n.last_tick_s = now_s;
  n.cur_samples = 0;
}

void nfl_ingest(uint8_t ch, int8_t rssi, uint8_t frame_type) {
  // frame_type: 0=mgmt, 1=ctrl, 2=data
  if (rssi >= 0 || rssi < -100) return; // invalid
  NflChannel *n = nfl_find(ch);
  if (!n) n = nfl_alloc(ch);
  if (!n) return;

  n->last_ts = millis()/1000;
  nfl_tick(*n);
  n->cur_samples++;

  int bucket = rssi + 100; // -100dBm → 0, -51dBm → 49
  if (bucket < 0) bucket = 0;
  if (bucket >= NFL_RSSI_BUCKETS) bucket = NFL_RSSI_BUCKETS - 1;
  n->rssi_hist[bucket]++;
  n->total_samples++;

  switch (frame_type) {
    case 0: n->mgmt_frames++; break;
    case 1: n->ctrl_frames++; break;
    case 2: n->data_frames++; break;
  }

  // Recompute stats every 32 samples
  if ((n->total_samples & 31) == 0) nfl_compute_stats(*n);
}

static String nfl_hist_json(const NflChannel &n) {
  String s = "[";
  for (int i = 0; i < NFL_HIST_LEN; i++) {
    int idx = (n.hist_head + i) % NFL_HIST_LEN;
    if (i) s += ",";
    s += String(n.sample_hist[idx]);
  }
  return s + "]";
}

static String nfl_rssi_hist_json(const NflChannel &n) {
  // Return downsampled histogram: group into 5dBm buckets
  String s = "[";
  for (int i = 0; i < 10; i++) { // 10 × 5dBm groups from -100 to -50
    uint32_t sum = 0;
    for (int j = 0; j < 5; j++) sum += n.rssi_hist[i*5+j];
    if (i) s += ",";
    s += String(sum);
  }
  return s + "]";
}

static String nfl_json_all() {
  String s = "{\"count\":" + String(g_nfl.count) + ",\"channels\":[";
  bool first = true;
  for (int i = 0; i < NFL_MAX_CHANNELS; i++) {
    NflChannel &n = g_nfl.channels[i];
    if (!n.active) continue;
    if (!first) s += ","; first = false;
    s += "{\"ch\":" + String(n.ch);
    s += ",\"noise_floor\":" + String(n.noise_floor);
    s += ",\"median_rssi\":" + String(n.median_rssi);
    s += ",\"peak_rssi\":" + String(n.peak_rssi);
    s += ",\"congestion_pct\":" + String(n.congestion_pct);
    s += ",\"total_samples\":" + String(n.total_samples);
    s += ",\"mgmt\":" + String(n.mgmt_frames);
    s += ",\"ctrl\":" + String(n.ctrl_frames);
    s += ",\"data\":" + String(n.data_frames);
    s += ",\"sample_hist\":" + nfl_hist_json(n);
    s += ",\"rssi_dist\":" + nfl_rssi_hist_json(n);
    s += "}";
  }
  s += "]}";
  return s;
}

inline void nfl_routes() {
  server.on("/api/nfl/all", HTTP_GET, []() {
    server.send(200, "application/json", nfl_json_all());
  });
  server.on("/api/nfl/clear", HTTP_POST, []() {
    memset(&g_nfl, 0, sizeof(g_nfl));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

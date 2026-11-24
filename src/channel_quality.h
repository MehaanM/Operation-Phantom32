#pragma once
// channel_quality.h — Channel quality scoring with airtime utilization and SNR.
// Combines frame density, RSSI variance, and noise floor. Prefix: cq_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define CQ_CHANNELS   14
#define CQ_WINDOW_S    5  // scoring window

// ─── Channel quality record ───────────────────────────────────────────────────
struct CqChannel {
  bool    valid;
  uint8_t channel;

  // Frame counters (5s window)
  uint32_t frame_count;
  uint32_t frame_count_prev;

  // RSSI
  int32_t  rssi_sum;
  uint32_t rssi_count;
  int8_t   avg_rssi;
  int8_t   noise_floor;
  float    snr_db;

  // 5s rate
  float    rate_per_sec;
  float    airtime_pct;   // estimated utilization

  // Computed quality score 0-100
  uint8_t  quality;
  uint8_t  congestion;    // 0-100 (100 = very congested)

  uint32_t window_start;
  uint32_t last_update;
};

// ─── State ────────────────────────────────────────────────────────────────────
static struct {
  CqChannel chans[CQ_CHANNELS];
  uint32_t  total_frames;
  uint32_t  last_tick_ms;
} g_cq = {};

// ─── Helpers ─────────────────────────────────────────────────────────────────
static int cq_idx(uint8_t ch) {
  return (ch >= 1 && ch <= 14) ? ch-1 : -1;
}

// ─── Ingest sample ───────────────────────────────────────────────────────────
void cq_ingest(uint8_t channel, int8_t rssi) {
  int idx = cq_idx(channel);
  if (idx < 0) return;

  CqChannel &c = g_cq.chans[idx];
  c.valid   = true;
  c.channel = channel;
  c.frame_count++;
  c.rssi_sum += rssi;
  c.rssi_count++;
  g_cq.total_frames++;
}

// ─── Set noise floor from nfl module ─────────────────────────────────────────
void cq_set_noise_floor(uint8_t channel, int8_t nf) {
  int idx = cq_idx(channel);
  if (idx >= 0) g_cq.chans[idx].noise_floor = nf;
}

// ─── Compute quality scores ───────────────────────────────────────────────────
static void cq_compute(CqChannel &c) {
  if (!c.valid || c.rssi_count == 0) return;

  c.avg_rssi = (int8_t)(c.rssi_sum / (int32_t)c.rssi_count);

  // SNR
  if (c.noise_floor < -50) {
    c.snr_db = (float)(c.avg_rssi - c.noise_floor);
  } else {
    c.snr_db = (float)(c.avg_rssi + 95); // assume -95 noise floor if unknown
  }

  // Rate per second
  uint32_t delta_frames = c.frame_count - c.frame_count_prev;
  c.rate_per_sec = (float)delta_frames / CQ_WINDOW_S;
  c.frame_count_prev = c.frame_count;

  // Airtime estimation: assume 500 frames/s = 100% at 2.4 GHz typical traffic
  c.airtime_pct = (c.rate_per_sec / 500.0f) * 100.0f;
  if (c.airtime_pct > 100.0f) c.airtime_pct = 100.0f;

  // Quality score: SNR contribution (60%) + low airtime (40%)
  float snr_norm = (c.snr_db < 0) ? 0.0f : (c.snr_db > 40.0f) ? 40.0f : c.snr_db;
  float snr_score = (snr_norm / 40.0f) * 60.0f;
  float airtime_score = (1.0f - c.airtime_pct / 100.0f) * 40.0f;
  c.quality = (uint8_t)(snr_score + airtime_score);

  // Congestion: inverse of quality
  c.congestion = (uint8_t)(c.airtime_pct);

  c.last_update = millis() / 1000;
  c.rssi_sum = 0;
  c.rssi_count = 0;
}

// ─── Tick (every 5s) ─────────────────────────────────────────────────────────
void cq_tick() {
  uint32_t now = millis();
  if (now - g_cq.last_tick_ms < CQ_WINDOW_S * 1000) return;
  g_cq.last_tick_ms = now;
  for (int i = 0; i < CQ_CHANNELS; i++) {
    if (g_cq.chans[i].valid) cq_compute(g_cq.chans[i]);
  }
}

// ─── Best channel ─────────────────────────────────────────────────────────────
uint8_t cq_best_channel() {
  // Check non-overlapping channels first (1,6,11)
  static const uint8_t preferred[] = {1,6,11,2,3,4,5,7,8,9,10,12,13,14};
  uint8_t best_ch  = 1;
  uint8_t best_q   = 0;
  for (uint8_t pch : preferred) {
    int idx = cq_idx(pch);
    if (idx < 0) continue;
    CqChannel &c = g_cq.chans[idx];
    if (!c.valid) return pch; // unused channel is best
    if (c.quality > best_q) { best_q = c.quality; best_ch = pch; }
  }
  return best_ch;
}

// ─── JSON ─────────────────────────────────────────────────────────────────────
static String cq_json_all() {
  String s = "{\"best_channel\":" + String(cq_best_channel());
  s += ",\"total_frames\":" + String(g_cq.total_frames);
  s += ",\"channels\":[";
  bool first = true;
  for (int i = 0; i < CQ_CHANNELS; i++) {
    CqChannel &c = g_cq.chans[i];
    if (!c.valid) continue;
    if (!first) s += ",";
    first = false;
    s += "{\"ch\":" + String(c.channel);
    s += ",\"quality\":" + String(c.quality);
    s += ",\"congestion\":" + String(c.congestion);
    s += ",\"rate\":" + String(c.rate_per_sec, 1);
    s += ",\"airtime\":" + String(c.airtime_pct, 1);
    s += ",\"avg_rssi\":" + String(c.avg_rssi);
    s += ",\"noise_floor\":" + String(c.noise_floor);
    s += ",\"snr\":" + String(c.snr_db, 1) + "}";
  }
  s += "]}";
  return s;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void cq_routes() {
  server.on("/api/cq/all", HTTP_GET, []() {
    server.send(200, "application/json", cq_json_all());
  });

  server.on("/api/cq/best", HTTP_GET, []() {
    server.send(200, "application/json",
      "{\"best_channel\":" + String(cq_best_channel()) + "}");
  });

  server.on("/api/cq/clear", HTTP_POST, []() {
    memset(&g_cq, 0, sizeof(g_cq));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

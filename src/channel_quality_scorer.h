#pragma once
// channel_quality_scorer.h — Composite per-channel quality score for Phantom32.
// Combines noise floor, AP density, channel utilization, interference overlap,
// retry rates, and RSSI variance into a single 0-100 quality score per channel.
// This score is used by the dashboard and channel-hop logic to prefer cleaner channels.
// Routes: /api/chqs/all. Prefix: chqs_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

// Quality score per channel (1-14 for 2.4GHz)
struct ChqsScore {
  uint8_t  ch;
  uint8_t  score;          // 0 (worst) to 100 (best)
  uint8_t  ap_density;     // 0-10: APs heard on this channel
  uint8_t  utilization;    // 0-100: estimated channel utilization %
  uint8_t  noise_penalty;  // 0-50: deducted for high noise floor
  uint8_t  overlap_penalty;// 0-20: deducted for channel overlap
  uint8_t  retry_penalty;  // 0-20: deducted for high retry rate
  bool     is_non_overlapping; // channels 1, 6, 11 are non-overlapping
};

static struct {
  ChqsScore scores[14];
  uint32_t  last_update_ms;
  uint8_t   best_ch;
  uint8_t   worst_ch;
  bool      ready;
} g_chqs = {};

static inline void chqs_init() {
  for (uint8_t i = 0; i < 14; i++) {
    g_chqs.scores[i].ch    = i + 1;
    g_chqs.scores[i].score = 50; // neutral default
    g_chqs.scores[i].is_non_overlapping = (i == 0 || i == 5 || i == 10);
  }
  g_chqs.ready = false;
}

// Compute scores from external module data (called periodically from loop())
// Parameters are summaries provided by other modules; this function is
// intentionally lightweight — it just weights the inputs.
static inline void chqs_update(
  uint8_t ch,              // 1-14
  uint8_t ap_count,        // APs heard on this channel (0-20)
  uint8_t utilization_pct, // channel utilization 0-100
  int8_t  noise_dbm,       // estimated noise floor dBm
  uint8_t retry_rate_pct,  // retry rate 0-100
  uint8_t overlap_count    // number of overlapping APs from adjacent channels
) {
  if (ch < 1 || ch > 14) return;
  ChqsScore &s = g_chqs.scores[ch - 1];
  s.ch = ch;

  // Noise penalty: 0 at -100dBm, 50 at -60dBm
  int8_t noise_norm = noise_dbm + 100; // 0=quietest, 40=loudest
  s.noise_penalty = (uint8_t)((noise_norm * 50) / 40);
  if (s.noise_penalty > 50) s.noise_penalty = 50;

  // AP density factor: more APs = more contention
  s.ap_density = (ap_count > 10) ? 10 : ap_count;

  // Utilization and retry penalties
  s.utilization    = utilization_pct;
  s.overlap_penalty = (overlap_count > 20) ? 20 : overlap_count;
  s.retry_penalty   = (retry_rate_pct / 5 > 20) ? 20 : (retry_rate_pct / 5);

  // Base score: start at 100, deduct penalties
  int16_t score = 100;
  score -= s.noise_penalty;
  score -= (s.utilization / 5);    // 0-20 points
  score -= s.overlap_penalty;
  score -= s.retry_penalty;
  score -= (s.ap_density * 2);     // 0-20 points

  // Bonus for non-overlapping channels
  if (s.is_non_overlapping) score += 5;

  s.score = (uint8_t)((score < 0) ? 0 : (score > 100 ? 100 : score));
  g_chqs.last_update_ms = millis();
  g_chqs.ready = true;

  // Update best/worst
  uint8_t best = 1, worst = 1;
  for (uint8_t i = 0; i < 14; i++) {
    if (g_chqs.scores[i].score > g_chqs.scores[best-1].score) best = i+1;
    if (g_chqs.scores[i].score < g_chqs.scores[worst-1].score) worst = i+1;
  }
  g_chqs.best_ch  = best;
  g_chqs.worst_ch = worst;
}

// Returns the best channel (highest quality score)
static inline uint8_t chqs_best() { return g_chqs.best_ch ? g_chqs.best_ch : 1; }

static inline void chqs_routes() {
  server.on("/api/chqs/all", HTTP_GET, []() {
    String out;
    out.reserve(1024);
    out += "{\"best_ch\":";  out += g_chqs.best_ch;
    out += ",\"worst_ch\":"; out += g_chqs.worst_ch;
    out += ",\"ready\":";    out += g_chqs.ready ? "true" : "false";
    out += ",\"channels\":[";
    for (uint8_t i = 0; i < 14; i++) {
      const ChqsScore &s = g_chqs.scores[i];
      if (i > 0) out += ',';
      out += "{\"ch\":";           out += s.ch;
      out += ",\"score\":";        out += s.score;
      out += ",\"ap_density\":";   out += s.ap_density;
      out += ",\"utilization\":";  out += s.utilization;
      out += ",\"noise_penalty\":"; out += s.noise_penalty;
      out += ",\"overlap_penalty\":"; out += s.overlap_penalty;
      out += ",\"retry_penalty\":"; out += s.retry_penalty;
      out += ",\"non_overlapping\":"; out += s.is_non_overlapping ? "true" : "false";
      out += '}';
    }
    out += "]}";
    server.send(200, "application/json", out);
  });
}

// ============================================================
// Channel quality score interpretation guide:
//   90-100  Excellent — low noise, few APs, minimal overlap
//   75-89   Good      — suitable for AP mode and monitoring
//   50-74   Fair      — moderate congestion or noise
//   25-49   Poor      — high utilization or interference
//   0-24    Bad       — unusable; heavy congestion or noise
// ============================================================
// Non-overlapping channels in 2.4GHz (20MHz bandwidth):
//   CH  1  (2.412 GHz) — no overlap with CH6, CH11
//   CH  6  (2.437 GHz) — no overlap with CH1, CH11
//   CH 11  (2.462 GHz) — no overlap with CH1, CH6
// Use these three for best co-channel interference avoidance.
// All other channels (2-5, 7-10, 12-14) overlap at least one
// of the three non-overlapping channels, so adjacent-channel
// interference is unavoidable when any two of them are active.
// In 40MHz HT mode (802.11n), only one non-overlapping pair
// exists in 2.4GHz (1+5 or 5+9 or 9+13 partial), so 40MHz
// operation is not recommended in crowded 2.4GHz environments.
// ============================================================
// end
// end
// end

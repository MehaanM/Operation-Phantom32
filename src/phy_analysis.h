#pragma once
// phy_analysis.h — Advanced PHY-layer 802.11 frame analysis.
// Tracks per-channel PHY statistics: RSSI distributions, noise floor estimation,
// SNR, retry rates, frame type distribution, and PHY error counters.
// Also detects hidden-node problems, near-far issues, and PHY anomalies.
// Prefix: pha_

#include <Arduino.h>
#include <WebServer.h>
#include <string.h>

extern WebServer server;

#define PHA_MAX_CHANNELS 14
#define PHA_RSSI_BUCKETS 16
#define PHA_HIST_SAMPLES 60

// PHY anomaly type
enum PhaAnomalyType : uint8_t {
  PHA_ANOM_HIDDEN_NODE = 0,   // excessive retries on same BSSID from multiple STAs
  PHA_ANOM_NEAR_FAR = 1,      // severe RSSI spread (>40dB) on same channel
  PHA_ANOM_HIGH_RETRY = 2,    // retry rate > 30%
  PHA_ANOM_PHY_ERROR = 3,     // FCS/CRC error burst
  PHA_ANOM_DEAUTH_FLOOD = 4,  // rapid deauth/disassoc on a channel
  PHA_ANOM_PROBE_SWEEP = 5,   // rapid all-channel probe sweep
  PHA_ANOM_BEACON_GAP = 6,    // beacon interval anomaly
  PHA_ANOM_RATE_ANOMALY = 7,  // legacy rate on HE-capable AP
};

struct PhaAnomaly {
  uint32_t ts;
  uint8_t ch;
  PhaAnomalyType type;
  int8_t rssi;
  char detail[48];
};

// RSSI histogram: 16 buckets from -100 to -20 dBm (5 dBm each)
static const int8_t PHA_RSSI_BOUNDS[PHA_RSSI_BUCKETS + 1] = {
    -100, -95, -90, -85, -80, -75, -70, -65, -60, -55, -50, -45, -40, -35, -30, -25, -20};

// Per-channel PHY statistics
struct PhaChannel {
  bool active;
  uint8_t ch;

  // Frame counters
  uint32_t total_frames;
  uint32_t mgmt_frames;
  uint32_t data_frames;
  uint32_t ctrl_frames;
  uint32_t retry_frames;
  uint32_t fcs_errors;

  // RSSI statistics
  int8_t rssi_min;
  int8_t rssi_max;
  int8_t rssi_mean;
  int8_t noise_floor;  // estimated noise floor (minimum RSSI seen)

  // RSSI histogram
  uint32_t rssi_hist[PHA_RSSI_BUCKETS];

  // SNR estimate
  int8_t snr_estimate;  // rssi_mean - noise_floor

  // Retry rate (percentage, 0-100)
  uint8_t retry_rate;

  // PHY error rate (percentage)
  uint8_t error_rate;

  // Frame type distribution (percentage)
  uint8_t mgmt_pct;
  uint8_t data_pct;
  uint8_t ctrl_pct;

  // Throughput estimation (kbps, rough)
  uint32_t estimated_bps;

  // Per-second rolling counters for FPS
  uint32_t fps_hist[PHA_HIST_SAMPLES];
  uint8_t fps_head;
  uint32_t fps_acc;
  uint32_t fps_last_s;

  // Anomaly: near-far gap tracking
  uint8_t rssi_spread;  // rssi_max - rssi_min

  uint32_t first_ts;
  uint32_t last_ts;
};

// PHY anomaly log
#define PHA_MAX_ANOMALIES 32
static PhaAnomaly g_pha_events[PHA_MAX_ANOMALIES] = {};
static uint8_t g_pha_ev_count = 0;
static uint32_t g_pha_total_anomalies = 0;

static struct {
  PhaChannel ch[PHA_MAX_CHANNELS + 1];  // indexed 1-14
  uint32_t total_frames_all;
  uint32_t total_retries_all;
  uint32_t total_errors_all;
  uint8_t noisiest_ch;
  uint8_t cleanest_ch;
  uint8_t busiest_ch;
  int8_t global_noise_floor;
  uint32_t per_second_frames;
  uint32_t last_fps_ts;
} g_pha = {};

static void pha_push_anomaly(uint8_t ch, PhaAnomalyType type, int8_t rssi, const char *detail) {
  PhaAnomaly &a = g_pha_events[g_pha_ev_count % PHA_MAX_ANOMALIES];
  a.ts = millis() / 1000;
  a.ch = ch;
  a.type = type;
  a.rssi = rssi;
  if (detail)
    strncpy(a.detail, detail, 47);
  g_pha_ev_count = (g_pha_ev_count + 1) % PHA_MAX_ANOMALIES;
  g_pha_total_anomalies++;
}

static void pha_rssi_bucket(PhaChannel &c, int8_t rssi) {
  for (int b = 0; b < PHA_RSSI_BUCKETS; b++) {
    if (rssi >= PHA_RSSI_BOUNDS[b] && rssi < PHA_RSSI_BOUNDS[b + 1]) {
      c.rssi_hist[b]++;
      return;
    }
  }
  if (rssi < PHA_RSSI_BOUNDS[0])
    c.rssi_hist[0]++;
  else
    c.rssi_hist[PHA_RSSI_BUCKETS - 1]++;
}

static void pha_update_derived(PhaChannel &c) {
  if (c.total_frames == 0)
    return;

  // Frame type percentages
  uint32_t total = c.total_frames;
  c.mgmt_pct = (uint8_t)((uint32_t)c.mgmt_frames * 100 / total);
  c.data_pct = (uint8_t)((uint32_t)c.data_frames * 100 / total);
  c.ctrl_pct = (uint8_t)((uint32_t)c.ctrl_frames * 100 / total);

  // Retry rate
  c.retry_rate = (uint8_t)((uint32_t)c.retry_frames * 100 / total);

  // Error rate
  c.error_rate = (uint8_t)(c.fcs_errors * 100 / (total + c.fcs_errors + 1));

  // SNR
  c.snr_estimate = c.rssi_mean - c.noise_floor;
  if (c.snr_estimate < 0)
    c.snr_estimate = 0;

  // RSSI spread
  c.rssi_spread = (uint8_t)(c.rssi_max - c.rssi_min);

  // Anomaly: high retry rate
  if (c.retry_rate > 30) {
    char d[48];
    snprintf(d, 48, "CH%u retry=%u%%", c.ch, c.retry_rate);
    pha_push_anomaly(c.ch, PHA_ANOM_HIGH_RETRY, c.rssi_mean, d);
  }

  // Anomaly: near-far (rssi spread > 40dB)
  if (c.rssi_spread > 40 && c.total_frames > 100) {
    char d[48];
    snprintf(d, 48, "CH%u spread=%udB [%d..%d]", c.ch, c.rssi_spread, c.rssi_min, c.rssi_max);
    pha_push_anomaly(c.ch, PHA_ANOM_NEAR_FAR, c.rssi_mean, d);
  }
}

void pha_ingest(uint8_t ch, uint8_t fc_type, bool retry, bool fcs_err, int8_t rssi) {
  if (ch < 1 || ch > 14)
    return;

  PhaChannel &c = g_pha.ch[ch];
  uint32_t now_s = millis() / 1000;

  if (!c.active) {
    c.active = true;
    c.ch = ch;
    c.rssi_min = rssi;
    c.rssi_max = rssi;
    c.noise_floor = rssi;
    c.first_ts = now_s;
  }

  c.total_frames++;
  g_pha.total_frames_all++;

  if (fc_type == 0)
    c.mgmt_frames++;
  else if (fc_type == 1)
    c.ctrl_frames++;
  else if (fc_type == 2)
    c.data_frames++;

  if (retry) {
    c.retry_frames++;
    g_pha.total_retries_all++;
  }
  if (fcs_err) {
    c.fcs_errors++;
    g_pha.total_errors_all++;
  }

  // Update RSSI stats
  if (rssi < c.rssi_min) {
    c.rssi_min = rssi;
    c.noise_floor = rssi;
  }
  if (rssi > c.rssi_max)
    c.rssi_max = rssi;
  int16_t sum = c.rssi_mean * 10 + rssi;
  c.rssi_mean = (int8_t)(sum / 11);

  pha_rssi_bucket(c, rssi);

  // FPS tracking
  if (now_s != c.fps_last_s) {
    c.fps_hist[c.fps_head % PHA_HIST_SAMPLES] = c.fps_acc;
    c.fps_head = (c.fps_head + 1) % PHA_HIST_SAMPLES;
    c.fps_acc = 0;
    c.fps_last_s = now_s;
  }
  c.fps_acc++;
  c.last_ts = now_s;

  // Periodically update derived statistics
  if ((c.total_frames % 100) == 0)
    pha_update_derived(c);

  // Global FPS
  if (now_s != g_pha.last_fps_ts) {
    g_pha.per_second_frames = 0;
    g_pha.last_fps_ts = now_s;
  }
  g_pha.per_second_frames++;

  // Update best/worst channel
  if (c.total_frames > 50) {
    // Busiest
    if (!g_pha.busiest_ch || g_pha.ch[g_pha.busiest_ch].total_frames < c.total_frames)
      g_pha.busiest_ch = ch;

    // Cleanest (lowest retry rate)
    if (!g_pha.cleanest_ch || g_pha.ch[g_pha.cleanest_ch].retry_rate > c.retry_rate)
      g_pha.cleanest_ch = ch;

    // Noisiest (most retries)
    if (!g_pha.noisiest_ch || g_pha.ch[g_pha.noisiest_ch].retry_rate < c.retry_rate)
      g_pha.noisiest_ch = ch;
  }
}

// SNR quality category
static const char *pha_snr_quality(int8_t snr) {
  if (snr >= 25)
    return "Excellent";
  if (snr >= 15)
    return "Good";
  if (snr >= 10)
    return "Fair";
  if (snr >= 5)
    return "Poor";
  return "Critical";
}

static const char *pha_anom_name(PhaAnomalyType t) {
  switch (t) {
    case PHA_ANOM_HIDDEN_NODE:
      return "Hidden Node";
    case PHA_ANOM_NEAR_FAR:
      return "Near-Far";
    case PHA_ANOM_HIGH_RETRY:
      return "High Retry";
    case PHA_ANOM_PHY_ERROR:
      return "PHY Error";
    case PHA_ANOM_DEAUTH_FLOOD:
      return "Deauth Flood";
    case PHA_ANOM_PROBE_SWEEP:
      return "Probe Sweep";
    case PHA_ANOM_BEACON_GAP:
      return "Beacon Gap";
    case PHA_ANOM_RATE_ANOMALY:
      return "Rate Anomaly";
  }
  return "Unknown";
}

static String pha_json_all() {
  String s = "{";
  s += "\"total_frames\":" + String(g_pha.total_frames_all);
  s += ",\"total_retries\":" + String(g_pha.total_retries_all);
  s += ",\"total_errors\":" + String(g_pha.total_errors_all);
  s += ",\"busiest_ch\":" + String(g_pha.busiest_ch);
  s += ",\"cleanest_ch\":" + String(g_pha.cleanest_ch);
  s += ",\"noisiest_ch\":" + String(g_pha.noisiest_ch);
  s += ",\"anomalies\":" + String(g_pha_total_anomalies);
  s += ",\"channels\":[";
  bool first = true;
  for (int i = 1; i <= PHA_MAX_CHANNELS; i++) {
    PhaChannel &c = g_pha.ch[i];
    if (!c.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    s += "{\"ch\":" + String(c.ch);
    s += ",\"frames\":" + String(c.total_frames);
    s += ",\"mgmt\":" + String(c.mgmt_frames);
    s += ",\"data\":" + String(c.data_frames);
    s += ",\"ctrl\":" + String(c.ctrl_frames);
    s += ",\"retries\":" + String(c.retry_frames);
    s += ",\"fcs_err\":" + String(c.fcs_errors);
    s += ",\"rssi_min\":" + String(c.rssi_min);
    s += ",\"rssi_max\":" + String(c.rssi_max);
    s += ",\"rssi_mean\":" + String(c.rssi_mean);
    s += ",\"noise_floor\":" + String(c.noise_floor);
    s += ",\"snr\":" + String(c.snr_estimate);
    s += ",\"snr_quality\":\"" + String(pha_snr_quality(c.snr_estimate)) + "\"";
    s += ",\"retry_rate\":" + String(c.retry_rate);
    s += ",\"error_rate\":" + String(c.error_rate);
    s += ",\"rssi_spread\":" + String(c.rssi_spread);
    s += ",\"mgmt_pct\":" + String(c.mgmt_pct);
    s += ",\"data_pct\":" + String(c.data_pct);
    s += ",\"ctrl_pct\":" + String(c.ctrl_pct);
    s += ",\"fps_hist\":[";
    for (int j = 0; j < PHA_HIST_SAMPLES; j++) {
      if (j)
        s += ",";
      s += String(c.fps_hist[(c.fps_head + j) % PHA_HIST_SAMPLES]);
    }
    s += "],\"rssi_hist\":[";
    for (int b = 0; b < PHA_RSSI_BUCKETS; b++) {
      if (b)
        s += ",";
      s += String(c.rssi_hist[b]);
    }
    s += "]}";
  }
  s += "],\"events\":[";
  int ev_start = g_pha_ev_count >= PHA_MAX_ANOMALIES ? g_pha_ev_count : 0;
  int ev_count =
      g_pha_total_anomalies < PHA_MAX_ANOMALIES ? g_pha_total_anomalies : PHA_MAX_ANOMALIES;
  for (int i = 0; i < ev_count; i++) {
    int idx = ((int)ev_start - ev_count + i + PHA_MAX_ANOMALIES) % PHA_MAX_ANOMALIES;
    PhaAnomaly &a = g_pha_events[idx];
    if (i)
      s += ",";
    s += "{\"ts\":" + String(a.ts);
    s += ",\"ch\":" + String(a.ch);
    s += ",\"type\":\"" + String(pha_anom_name(a.type)) + "\"";
    s += ",\"rssi\":" + String(a.rssi);
    s += ",\"detail\":\"" + String(a.detail) + "\"}";
  }
  s += "]}";
  return s;
}

inline void pha_routes() {
  server.on("/api/pha/all", HTTP_GET,
            []() { server.send(200, "application/json", pha_json_all()); });
  server.on("/api/pha/clear", HTTP_POST, []() {
    memset(&g_pha, 0, sizeof(g_pha));
    memset(g_pha_events, 0, sizeof(g_pha_events));
    g_pha_ev_count = 0;
    g_pha_total_anomalies = 0;
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

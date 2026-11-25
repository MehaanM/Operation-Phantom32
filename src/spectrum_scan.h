#pragma once
// spectrum_scan.h — Pseudo-spectrum analyzer via RSS distribution.
// Builds per-channel "spectrum" from observed RSSI vs. time, with
// occupancy scoring, interference pattern detection, and CCA busy time
// estimation using the ESP32's internal RSSI + frame observations.
// Prefix: sps_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define SPS_MAX_CHANNELS 16
#define SPS_TIME_BINS 60      // 60 seconds of spectrum history
#define SPS_RSSI_BINS 20      // -100 to -50 in 2.5dBm steps
#define SPS_INTER_THRESH -80  // dBm threshold for interference signals

struct SpsSample {
  uint32_t ts;
  int8_t rssi;
  uint8_t frame_type;  // 0=mgmt, 1=ctrl, 2=data
};

struct SpsChannel {
  bool active;
  uint8_t ch;
  uint16_t freq_mhz;  // center frequency

  // RSSI distribution over time
  uint32_t rssi_bins[SPS_RSSI_BINS];
  uint32_t total_frames;

  // 60-second busy history (% of time with frames)
  uint8_t busy_hist[SPS_TIME_BINS];
  uint8_t hist_head;
  uint16_t cur_frames;
  uint32_t last_tick_s;

  // Interference indicators
  uint8_t cca_busy_pct;   // CCA busy time estimate
  bool has_non_wifi;      // non-WiFi interference suspected (erratic RSSI)
  uint8_t occupancy_pct;  // fraction of time channel has any observable signal

  // Frequency-domain statistics
  int8_t peak_rssi;    // max RSSI seen
  int8_t noise_floor;  // 10th percentile RSSI
  int8_t avg_rssi;     // EMA average

  // Channel band info
  uint8_t bandwidth_mhz;  // 20/40/80 (inferred from VHT/HT)
  bool is_5ghz;

  uint32_t last_ts;
};

static struct {
  SpsChannel channels[SPS_MAX_CHANNELS];
  uint8_t count;
} g_sps = {};

static uint16_t sps_ch_to_freq(uint8_t ch) {
  if (ch <= 14)
    return 2412 + (ch - 1) * 5;  // 2.4GHz
  if (ch == 36)
    return 5180;
  if (ch == 40)
    return 5200;
  if (ch == 44)
    return 5220;
  if (ch == 48)
    return 5240;
  if (ch == 52)
    return 5260;
  if (ch == 56)
    return 5280;
  if (ch == 60)
    return 5300;
  if (ch == 64)
    return 5320;
  if (ch == 100)
    return 5500;
  if (ch == 104)
    return 5520;
  if (ch == 108)
    return 5540;
  if (ch == 112)
    return 5560;
  if (ch == 116)
    return 5580;
  if (ch == 120)
    return 5600;
  if (ch == 124)
    return 5620;
  if (ch == 128)
    return 5640;
  if (ch == 132)
    return 5660;
  if (ch == 136)
    return 5680;
  if (ch == 140)
    return 5700;
  if (ch == 149)
    return 5745;
  if (ch == 153)
    return 5765;
  if (ch == 157)
    return 5785;
  if (ch == 161)
    return 5805;
  if (ch == 165)
    return 5825;
  return 0;
}

static SpsChannel *sps_find(uint8_t ch) {
  for (int i = 0; i < SPS_MAX_CHANNELS; i++)
    if (g_sps.channels[i].active && g_sps.channels[i].ch == ch)
      return &g_sps.channels[i];
  return nullptr;
}

static SpsChannel *sps_alloc(uint8_t ch) {
  for (int i = 0; i < SPS_MAX_CHANNELS; i++) {
    if (!g_sps.channels[i].active) {
      SpsChannel &s = g_sps.channels[i];
      memset(&s, 0, sizeof(s));
      s.active = true;
      s.ch = ch;
      s.freq_mhz = sps_ch_to_freq(ch);
      s.is_5ghz = (ch > 14);
      s.noise_floor = -95;
      s.avg_rssi = -75;
      s.peak_rssi = -100;
      g_sps.count++;
      return &s;
    }
  }
  return nullptr;
}

static void sps_tick(SpsChannel &s) {
  uint32_t now_s = millis() / 1000;
  if (now_s == s.last_tick_s)
    return;
  uint32_t elapsed = now_s - s.last_tick_s;
  if (elapsed > SPS_TIME_BINS)
    elapsed = SPS_TIME_BINS;
  for (uint32_t i = 0; i < elapsed; i++) {
    uint8_t busy = (s.cur_frames > 0) ? (uint8_t)min(100u, (uint32_t)s.cur_frames * 5) : 0;
    s.busy_hist[s.hist_head % SPS_TIME_BINS] = (i == 0) ? busy : 0;
    s.hist_head = (s.hist_head + 1) % SPS_TIME_BINS;
  }
  s.last_tick_s = now_s;
  s.cur_frames = 0;
}

static void sps_recompute(SpsChannel &s) {
  if (s.total_frames == 0)
    return;

  // noise floor: 10th percentile
  uint32_t cum = 0, thresh1 = s.total_frames / 10;
  uint32_t thresh5 = s.total_frames / 2;
  bool set1 = false, set5 = false;
  for (int i = 0; i < SPS_RSSI_BINS; i++) {
    cum += s.rssi_bins[i];
    int8_t dbm = -100 + i * 5 / 2;
    if (!set1 && cum >= thresh1) {
      s.noise_floor = dbm;
      set1 = true;
    }
    if (!set5 && cum >= thresh5) {
      set5 = true;
    }
  }

  // occupancy = frames that exceed noise floor by 10dBm
  uint32_t above = 0;
  for (int i = SPS_RSSI_BINS / 4; i < SPS_RSSI_BINS; i++)
    above += s.rssi_bins[i];
  s.occupancy_pct = (uint8_t)((above * 100) / s.total_frames);

  // CCA busy: estimate from busy_hist
  uint32_t sum = 0;
  for (int i = 0; i < SPS_TIME_BINS; i++)
    sum += s.busy_hist[i];
  s.cca_busy_pct = (uint8_t)(sum / SPS_TIME_BINS);
}

void sps_ingest(uint8_t ch, int8_t rssi, uint8_t frame_type, uint8_t bandwidth_mhz) {
  if (rssi >= 0 || rssi < -100)
    return;
  SpsChannel *s = sps_find(ch);
  if (!s)
    s = sps_alloc(ch);
  if (!s)
    return;

  s->last_ts = millis() / 1000;
  if (bandwidth_mhz > s->bandwidth_mhz)
    s->bandwidth_mhz = bandwidth_mhz;

  sps_tick(*s);
  s->cur_frames++;
  s->total_frames++;

  // Update RSSI bin
  int bin = (rssi + 100) * 2 / 5;  // 2.5dBm/bin
  if (bin < 0)
    bin = 0;
  if (bin >= SPS_RSSI_BINS)
    bin = SPS_RSSI_BINS - 1;
  s->rssi_bins[bin]++;

  // EMA avg RSSI
  if (s->avg_rssi == -75 && s->total_frames == 1)
    s->avg_rssi = rssi;
  else
    s->avg_rssi = (int8_t)((s->avg_rssi * 7 + rssi) / 8);

  if (rssi > s->peak_rssi)
    s->peak_rssi = rssi;

  // Detect possible non-WiFi: RSSI below -90 but lots of frames (jammers, Zigbee)
  if (rssi < -90)
    s->has_non_wifi = (s->total_frames > 20);

  if ((s->total_frames & 15) == 0)
    sps_recompute(*s);
}

static String sps_hist_json(const SpsChannel &s) {
  String out = "[";
  for (int i = 0; i < SPS_TIME_BINS; i++) {
    int idx = (s.hist_head + i) % SPS_TIME_BINS;
    if (i)
      out += ",";
    out += String(s.busy_hist[idx]);
  }
  return out + "]";
}

static String sps_rssi_json(const SpsChannel &s) {
  String out = "[";
  for (int i = 0; i < SPS_RSSI_BINS; i++) {
    if (i)
      out += ",";
    out += String(s.rssi_bins[i]);
  }
  return out + "]";
}

static String sps_json_all() {
  String s = "{\"count\":" + String(g_sps.count) + ",\"channels\":[";
  bool first = true;
  for (int i = 0; i < SPS_MAX_CHANNELS; i++) {
    SpsChannel &c = g_sps.channels[i];
    if (!c.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    s += "{\"ch\":" + String(c.ch);
    s += ",\"freq\":" + String(c.freq_mhz);
    s += ",\"is5\":" + String(c.is_5ghz ? "true" : "false");
    s += ",\"bw\":" + String(c.bandwidth_mhz ? c.bandwidth_mhz : 20);
    s += ",\"frames\":" + String(c.total_frames);
    s += ",\"peak\":" + String(c.peak_rssi);
    s += ",\"noise\":" + String(c.noise_floor);
    s += ",\"avg\":" + String(c.avg_rssi);
    s += ",\"cca\":" + String(c.cca_busy_pct);
    s += ",\"occ\":" + String(c.occupancy_pct);
    s += ",\"non_wifi\":" + String(c.has_non_wifi ? "true" : "false");
    s += ",\"busy_hist\":" + sps_hist_json(c);
    s += ",\"rssi_dist\":" + sps_rssi_json(c);
    s += "}";
  }
  s += "]}";
  return s;
}

inline void sps_routes() {
  server.on("/api/sps/all", HTTP_GET,
            []() { server.send(200, "application/json", sps_json_all()); });
  server.on("/api/sps/clear", HTTP_POST, []() {
    memset(&g_sps, 0, sizeof(g_sps));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

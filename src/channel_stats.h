#pragma once
// channel_stats.h — Per-channel 802.11 utilization and interference statistics.
// Tracks frame density, RSSI distribution, AP count, and noise. Prefix: chs_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define CHS_MAX_CHANNELS   14    // 2.4GHz channels 1-14
#define CHS_HIST_LEN       60    // 60-second history

// ─── Channel record ───────────────────────────────────────────────────────────
struct ChsChannel {
  uint8_t  ch;
  uint32_t frames_total;
  uint32_t frames_mgmt;
  uint32_t frames_data;
  uint32_t frames_ctrl;
  uint32_t frames_beacon;
  uint32_t frames_probe;
  uint32_t frames_deauth;
  uint32_t ap_count;
  uint32_t sta_count;
  int8_t   rssi_min;
  int8_t   rssi_max;
  int8_t   rssi_avg;    // exponential average
  uint32_t bytes_total;
  // Interference score (0-100): high frame density + low RSSI = interference
  uint8_t  interference;
  // Utilization: frames/sec in last window
  uint32_t rate_1s;
  uint32_t rate_30s;
  // Rolling history
  uint16_t hist[CHS_HIST_LEN];
  uint8_t  hist_idx;
};

// ─── State ────────────────────────────────────────────────────────────────────
static struct {
  ChsChannel channels[CHS_MAX_CHANNELS];
  uint8_t    current_channel;
  uint32_t   total_frames;
  uint32_t   last_tick_ts;
  uint32_t   window_frames[CHS_MAX_CHANNELS];
} g_chs = {};

// ─── Init ─────────────────────────────────────────────────────────────────────
inline void chs_init() {
  for (int i = 0; i < CHS_MAX_CHANNELS; i++) {
    g_chs.channels[i].ch = i + 1;
    g_chs.channels[i].rssi_min = 0;
    g_chs.channels[i].rssi_max = -127;
    g_chs.channels[i].rssi_avg = -80;
  }
}

// ─── Ingest ───────────────────────────────────────────────────────────────────
void chs_ingest(int len, uint8_t type, uint8_t sub, int8_t rssi,
                uint8_t channel, bool is_new_ap, bool is_new_sta) {
  if (channel < 1 || channel > CHS_MAX_CHANNELS) return;
  int idx = channel - 1;
  ChsChannel &c = g_chs.channels[idx];

  c.frames_total++;
  c.bytes_total += len;
  g_chs.total_frames++;
  g_chs.window_frames[idx]++;

  if      (type == 0) { c.frames_mgmt++; }
  else if (type == 1) { c.frames_ctrl++; }
  else if (type == 2) { c.frames_data++; }

  if (type == 0 && sub == 8) c.frames_beacon++;
  if (type == 0 && sub == 4) c.frames_probe++;
  if (type == 0 && sub == 12) c.frames_deauth++;

  if (rssi < c.rssi_min || c.rssi_min == 0) c.rssi_min = rssi;
  if (rssi > c.rssi_max) c.rssi_max = rssi;
  c.rssi_avg = (int8_t)((c.rssi_avg * 7 + rssi) / 8);

  if (is_new_ap)  c.ap_count++;
  if (is_new_sta) c.sta_count++;

  // Interference score
  uint8_t noise = (uint8_t)min(100, (int)(c.frames_total / 1000));
  uint8_t rssi_score = (uint8_t)max(0, min(50, (-c.rssi_avg - 50)));
  c.interference = min((uint8_t)100, (uint8_t)(noise/2 + rssi_score/2));
}

// ─── Tick ─────────────────────────────────────────────────────────────────────
void chs_tick() {
  uint32_t now = millis() / 1000;
  if (now == g_chs.last_tick_ts) return;
  g_chs.last_tick_ts = now;

  for (int i = 0; i < CHS_MAX_CHANNELS; i++) {
    ChsChannel &c = g_chs.channels[i];
    uint16_t rate = (uint16_t)min((uint32_t)65535, g_chs.window_frames[i]);
    c.hist[c.hist_idx % CHS_HIST_LEN] = rate;
    c.hist_idx++;
    c.rate_1s = rate;

    uint32_t sum = 0;
    for (int j = 0; j < CHS_HIST_LEN; j++) sum += c.hist[j];
    c.rate_30s = sum / CHS_HIST_LEN;

    g_chs.window_frames[i] = 0;
  }
}

// ─── Best channel recommendation ─────────────────────────────────────────────
static uint8_t chs_best_channel() {
  // Among channels 1, 6, 11 (non-overlapping 2.4GHz)
  uint8_t candidates[] = {1, 6, 11};
  uint8_t best = 6;
  uint32_t min_frames = UINT32_MAX;
  for (int i = 0; i < 3; i++) {
    uint8_t ch = candidates[i];
    if (ch < 1 || ch > CHS_MAX_CHANNELS) continue;
    uint32_t f = g_chs.channels[ch-1].frames_total;
    if (f < min_frames) { min_frames = f; best = ch; }
  }
  return best;
}

// ─── JSON ─────────────────────────────────────────────────────────────────────
static String chs_json_all() {
  String s = "{\"total_frames\":" + String(g_chs.total_frames);
  s += ",\"best_channel\":" + String(chs_best_channel());
  s += ",\"channels\":[";
  for (int i = 0; i < CHS_MAX_CHANNELS; i++) {
    if (i) s += ",";
    ChsChannel &c = g_chs.channels[i];
    s += "{\"ch\":" + String(c.ch);
    s += ",\"frames\":" + String(c.frames_total);
    s += ",\"data\":" + String(c.frames_data);
    s += ",\"mgmt\":" + String(c.frames_mgmt);
    s += ",\"beacon\":" + String(c.frames_beacon);
    s += ",\"deauth\":" + String(c.frames_deauth);
    s += ",\"ap_count\":" + String(c.ap_count);
    s += ",\"sta_count\":" + String(c.sta_count);
    s += ",\"rssi_avg\":" + String(c.rssi_avg);
    s += ",\"rssi_min\":" + String(c.rssi_min);
    s += ",\"rssi_max\":" + String(c.rssi_max);
    s += ",\"rate_1s\":" + String(c.rate_1s);
    s += ",\"rate_30s\":" + String(c.rate_30s);
    s += ",\"interference\":" + String(c.interference);
    s += ",\"bytes\":" + String(c.bytes_total);
    s += ",\"hist\":[";
    for (int j = 0; j < CHS_HIST_LEN; j++) {
      if (j) s += ",";
      int idx = (c.hist_idx - CHS_HIST_LEN + j + CHS_HIST_LEN) % CHS_HIST_LEN;
      s += String(c.hist[idx]);
    }
    s += "]}";
  }
  s += "]}";
  return s;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void chs_routes() {
  server.on("/api/chs/all", HTTP_GET, []() {
    server.send(200, "application/json", chs_json_all());
  });

  server.on("/api/chs/channel", HTTP_GET, []() {
    if (!server.hasArg("ch")) {
      server.send(400, "application/json", "{\"error\":\"missing ch\"}"); return;
    }
    int ch = server.arg("ch").toInt();
    if (ch < 1 || ch > CHS_MAX_CHANNELS) {
      server.send(400, "application/json", "{\"error\":\"invalid ch\"}"); return;
    }
    ChsChannel &c = g_chs.channels[ch-1];
    String s = "{\"ch\":" + String(c.ch);
    s += ",\"frames\":" + String(c.frames_total);
    s += ",\"rate_1s\":" + String(c.rate_1s);
    s += ",\"interference\":" + String(c.interference);
    s += ",\"rssi_avg\":" + String(c.rssi_avg);
    s += "}";
    server.send(200, "application/json", s);
  });

  server.on("/api/chs/best", HTTP_GET, []() {
    server.send(200, "application/json",
      "{\"best_channel\":" + String(chs_best_channel()) + "}");
  });

  server.on("/api/chs/clear", HTTP_POST, []() {
    for (int i = 0; i < CHS_MAX_CHANNELS; i++) {
      ChsChannel &c = g_chs.channels[i];
      c.frames_total = c.frames_mgmt = c.frames_data = c.frames_ctrl = 0;
      c.frames_beacon = c.frames_probe = c.frames_deauth = 0;
      c.ap_count = c.sta_count = 0;
      memset(c.hist, 0, sizeof(c.hist));
    }
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

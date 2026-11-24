#pragma once
// channel_utilization.h — Per-channel utilization and airtime tracker.
// Estimates CCA (Clear Channel Assessment) busy time, protocol overhead,
// and actual data airtime per channel from observed frame statistics.
// Prefix: cut_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define CUT_MAX_CHANNELS  20
#define CUT_HIST_LEN      60

// Airtime components
struct CutAirtime {
  uint32_t data_us;      // estimated data frame airtime (µs)
  uint32_t mgmt_us;      // management frames
  uint32_t ctrl_us;      // control frames (ACK, RTS, CTS)
  uint32_t overhead_us;  // inter-frame spacing, backoff
};

struct CutChannel {
  bool    active;
  uint8_t ch;
  uint32_t freq_mhz;

  // Frame counters
  uint32_t data_frames;
  uint32_t mgmt_frames;
  uint32_t ctrl_frames;
  uint32_t retry_frames;
  uint32_t broadcast_frames;

  // Byte counters
  uint32_t data_bytes;
  uint32_t mgmt_bytes;

  // Utilization estimation
  // CCA busy percentage (0-100): estimated from frame density
  uint8_t  cca_busy_pct;
  uint8_t  data_airtime_pct;
  uint8_t  mgmt_overhead_pct;
  uint8_t  ctrl_overhead_pct;

  // AP/client count
  uint8_t  ap_count;
  uint8_t  client_count;

  // Rolling history
  uint8_t  util_hist[CUT_HIST_LEN];   // 0-100%
  uint32_t pkt_hist[CUT_HIST_LEN];
  uint8_t  hist_head;
  uint32_t cur_pkts;
  uint32_t cur_bytes;
  uint32_t last_tick_s;

  // Peak utilization
  uint8_t  peak_util_pct;
  uint32_t peak_ts;
};

// Known AP BSSIDs per channel for counting
static const uint8_t CUT_CHANNEL_FREQS[] = {
  2412,2417,2422,2427,2432,2437,2442,2447,2452,2457,2462,2467,2472,2484, // 1-14
  5180,5200,5220,5240,5260,5280                                            // 36-56
};

static struct {
  CutChannel channels[CUT_MAX_CHANNELS];
  uint8_t    count;
  uint32_t   total_frames;
} g_cut = {};

static int cut_ch_to_idx(uint8_t ch) {
  if (ch >= 1 && ch <= 14) return ch - 1;
  if (ch == 36) return 14;
  if (ch == 40) return 15;
  if (ch == 44) return 16;
  if (ch == 48) return 17;
  if (ch == 52) return 18;
  if (ch == 56) return 19;
  return -1;
}

static void cut_tick(CutChannel &c) {
  uint32_t now_s = millis()/1000;
  if (now_s == c.last_tick_s) return;

  // Estimate utilization from frame rate (heuristic)
  // ~1 frame per ms = fully loaded; actual airtime depends on data rate
  uint32_t frames_per_s = c.cur_pkts;
  uint32_t bytes_per_s  = c.cur_bytes;

  // Very rough estimate: 1500-byte frame at 54Mbps ≈ 222µs
  // At 11Mbps ≈ 1090µs. Normalize to seconds.
  uint32_t est_busy_us = (bytes_per_s * 8) / 54; // µs at 54Mbps
  c.cca_busy_pct = (uint8_t)(est_busy_us > 1000000 ? 100 : est_busy_us / 10000);
  if (c.cca_busy_pct > c.peak_util_pct) {
    c.peak_util_pct = c.cca_busy_pct;
    c.peak_ts = millis()/1000;
  }

  c.util_hist[c.hist_head % CUT_HIST_LEN] = c.cca_busy_pct;
  c.pkt_hist[c.hist_head  % CUT_HIST_LEN] = c.cur_pkts;
  c.hist_head = (c.hist_head + 1) % CUT_HIST_LEN;
  c.last_tick_s = now_s;
  c.cur_pkts  = 0;
  c.cur_bytes = 0;
}

void cut_ingest(uint8_t ch, uint8_t frame_type, bool is_broadcast,
                bool is_retry, uint16_t len) {
  int idx = cut_ch_to_idx(ch);
  if (idx < 0) return;

  CutChannel &c = g_cut.channels[idx];
  if (!c.active) {
    c.active = true;
    c.ch = ch;
    if (idx < 20) c.freq_mhz = CUT_CHANNEL_FREQS[idx];
    g_cut.count++;
  }

  cut_tick(c);
  c.cur_pkts++;
  c.cur_bytes += len;
  g_cut.total_frames++;

  switch (frame_type) {
    case 0: c.mgmt_frames++; c.mgmt_bytes += len; break;
    case 1: c.ctrl_frames++; break;
    case 2: c.data_frames++; c.data_bytes += len; break;
  }

  if (is_broadcast) c.broadcast_frames++;
  if (is_retry)     c.retry_frames++;

  // Recompute airtime percentages
  uint32_t total = c.data_frames + c.mgmt_frames + c.ctrl_frames;
  if (total > 0) {
    c.data_airtime_pct  = (uint8_t)((uint32_t)c.data_frames  * 100 / total);
    c.mgmt_overhead_pct = (uint8_t)((uint32_t)c.mgmt_frames  * 100 / total);
    c.ctrl_overhead_pct = (uint8_t)((uint32_t)c.ctrl_frames  * 100 / total);
  }
}

void cut_update_ap_count(uint8_t ch, uint8_t ap_cnt, uint8_t cli_cnt) {
  int idx = cut_ch_to_idx(ch);
  if (idx < 0) return;
  CutChannel &c = g_cut.channels[idx];
  c.ap_count     = ap_cnt;
  c.client_count = cli_cnt;
}

static String cut_hist_json(const uint8_t *hist, int len, int head) {
  String s = "[";
  for (int i = 0; i < len; i++) {
    int idx = (head + i) % len;
    if (i) s += ",";
    s += String(hist[idx]);
  }
  return s + "]";
}

static String cut_pkt_hist_json(const uint32_t *hist, int len, int head) {
  String s = "[";
  for (int i = 0; i < len; i++) {
    int idx = (head + i) % len;
    if (i) s += ",";
    s += String(hist[idx]);
  }
  return s + "]";
}

static String cut_json_all() {
  String s = "{\"count\":" + String(g_cut.count);
  s += ",\"total_frames\":" + String(g_cut.total_frames);
  s += ",\"channels\":[";
  bool first = true;
  for (int i = 0; i < CUT_MAX_CHANNELS; i++) {
    CutChannel &c = g_cut.channels[i];
    if (!c.active) continue;
    if (!first) s += ","; first = false;
    uint32_t total = c.data_frames + c.mgmt_frames + c.ctrl_frames;
    s += "{\"ch\":" + String(c.ch);
    s += ",\"freq\":" + String(c.freq_mhz);
    s += ",\"cca_busy\":" + String(c.cca_busy_pct);
    s += ",\"peak_util\":" + String(c.peak_util_pct);
    s += ",\"data_pct\":" + String(c.data_airtime_pct);
    s += ",\"mgmt_pct\":" + String(c.mgmt_overhead_pct);
    s += ",\"ctrl_pct\":" + String(c.ctrl_overhead_pct);
    s += ",\"data\":" + String(c.data_frames);
    s += ",\"mgmt\":" + String(c.mgmt_frames);
    s += ",\"ctrl\":" + String(c.ctrl_frames);
    s += ",\"retry\":" + String(c.retry_frames);
    s += ",\"bcast\":" + String(c.broadcast_frames);
    s += ",\"total\":" + String(total);
    s += ",\"data_bytes\":" + String(c.data_bytes);
    s += ",\"ap_count\":" + String(c.ap_count);
    s += ",\"client_count\":" + String(c.client_count);
    s += ",\"util_hist\":" + cut_hist_json(c.util_hist, CUT_HIST_LEN, c.hist_head);
    s += ",\"pkt_hist\":" + cut_pkt_hist_json(c.pkt_hist, CUT_HIST_LEN, c.hist_head) + "}";
  }
  s += "]}";
  return s;
}

inline void cut_routes() {
  server.on("/api/cut/all", HTTP_GET, []() {
    server.send(200, "application/json", cut_json_all());
  });
  server.on("/api/cut/clear", HTTP_POST, []() {
    memset(&g_cut, 0, sizeof(g_cut));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

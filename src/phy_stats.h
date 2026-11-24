#pragma once
// phy_stats.h — PHY layer statistics tracker.
// Tracks per-channel MCS (Modulation and Coding Scheme) usage, guard intervals,
// channel bandwidth, spatial streams, and frame retry statistics.
// Prefix: phy_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define PHY_MAX_CHANNELS  14
#define PHY_HIST_LEN      60

// MCS index groups (0-7 HT, 0-9 VHT/HE, with legacy fallback)
#define PHY_MCS_BUCKETS   10

// Guard interval types
enum PhyGI : uint8_t {
  PHY_GI_LONG   = 0,  // 800ns
  PHY_GI_SHORT  = 1,  // 400ns
  PHY_GI_HE_1X6 = 2,  // HE 1600ns OFDM
  PHY_GI_HE_3X2 = 3   // HE 3200ns OFDM
};

// Channel width
enum PhyBW : uint8_t {
  PHY_BW_20   = 0,
  PHY_BW_40   = 1,
  PHY_BW_80   = 2,
  PHY_BW_160  = 3,
  PHY_BW_80_80 = 4
};

// PHY generation type
enum PhyGen : uint8_t {
  PHY_GEN_LEGACY = 0, // 802.11a/b/g
  PHY_GEN_HT     = 1, // 802.11n (HT)
  PHY_GEN_VHT    = 2, // 802.11ac (VHT)
  PHY_GEN_HE     = 3  // 802.11ax (HE)
};

static const char* PHY_GEN_NAMES[] = {"Legacy", "HT (n)", "VHT (ac)", "HE (ax)"};
static const char* PHY_BW_NAMES[]  = {"20MHz","40MHz","80MHz","160MHz","80+80MHz"};

struct PhyChannelStats {
  bool    active;
  uint8_t ch;
  uint32_t frames_total;
  uint32_t frames_retry;
  uint32_t frames_frag;

  // Frame rates by generation
  uint32_t gen_count[4];   // legacy/HT/VHT/HE frame counts

  // MCS index histogram (0-9)
  uint32_t mcs_hist[PHY_MCS_BUCKETS];

  // Bandwidth usage
  uint32_t bw_hist[5];     // 20/40/80/160/80+80

  // Guard interval usage
  uint32_t gi_hist[4];     // long/short/HE1.6/HE3.2

  // Spatial stream histogram (1-8 streams)
  uint32_t ss_hist[8];

  // Rolling frame rate
  uint32_t pkt_hist[PHY_HIST_LEN];
  uint32_t byte_hist[PHY_HIST_LEN];
  uint8_t  hist_head;
  uint32_t cur_pkts;
  uint32_t cur_bytes;
  uint32_t last_tick_s;

  // Efficiency metrics
  uint32_t retry_rate_ema;  // EMA * 1000 (0-1000 = 0-100%)
  uint32_t avg_mcs_ema;     // EMA of MCS index * 10
};

// Global aggregated stats
struct PhyGlobalStats {
  uint64_t total_frames;
  uint64_t total_bytes;
  uint32_t total_retries;
  uint32_t total_fragments;
  uint32_t gen_total[4];
  uint32_t mcs_total[PHY_MCS_BUCKETS];
  uint32_t bw_total[5];
  uint32_t ss_total[8];
};

static struct {
  PhyChannelStats channels[PHY_MAX_CHANNELS];
  PhyGlobalStats  global;
  uint8_t         active_ch_count;
} g_phy = {};

static int phy_ch_to_idx(uint8_t ch) {
  if (ch >= 1 && ch <= 14) return ch - 1;
  return -1;
}

static void phy_tick(PhyChannelStats &c) {
  uint32_t now_s = millis()/1000;
  if (now_s == c.last_tick_s) return;
  c.pkt_hist[c.hist_head % PHY_HIST_LEN] = c.cur_pkts;
  c.byte_hist[c.hist_head % PHY_HIST_LEN] = c.cur_bytes;
  c.hist_head = (c.hist_head + 1) % PHY_HIST_LEN;
  c.last_tick_s = now_s;
  c.cur_pkts = c.cur_bytes = 0;
}

void phy_ingest(uint8_t ch, PhyGen gen, uint8_t mcs, PhyBW bw,
                uint8_t streams, PhyGI gi,
                bool retry, bool frag, uint16_t len) {
  int idx = phy_ch_to_idx(ch);
  if (idx < 0) {
    // Handle 5GHz channels (36,40,44,48,52,56,60,64)
    if (ch == 36) idx = 0;  // map to slot 0 for 5GHz (simplified)
    else return;
  }

  PhyChannelStats &c = g_phy.channels[idx];
  if (!c.active) {
    c.active = true;
    c.ch = ch;
    g_phy.active_ch_count++;
  }

  phy_tick(c);
  c.frames_total++;
  c.cur_pkts++;
  c.cur_bytes += len;

  if (retry) { c.frames_retry++;   g_phy.total_retries++; }
  if (frag)  { c.frames_frag++;    g_phy.total_fragments++; }

  if (gen < 4) { c.gen_count[gen]++; g_phy.gen_total[gen]++; }

  uint8_t mcs_idx = mcs < PHY_MCS_BUCKETS ? mcs : PHY_MCS_BUCKETS-1;
  c.mcs_hist[mcs_idx]++;
  g_phy.mcs_total[mcs_idx]++;

  if (bw <= PHY_BW_80_80) { c.bw_hist[bw]++; g_phy.bw_total[bw]++; }

  if (gi < 4) c.gi_hist[gi]++;

  uint8_t ss_idx = (streams > 0 && streams <= 8) ? streams-1 : 0;
  c.ss_hist[ss_idx]++;
  g_phy.ss_total[ss_idx]++;

  g_phy.global.total_frames++;
  g_phy.global.total_bytes += len;

  // Update EMA retry rate
  uint32_t retry_inst = retry ? 1000 : 0;
  if (c.retry_rate_ema == 0) c.retry_rate_ema = retry_inst;
  else c.retry_rate_ema = (c.retry_rate_ema * 15 + retry_inst) / 16;

  // Update EMA MCS
  uint32_t mcs_v = mcs_idx * 10;
  if (c.avg_mcs_ema == 0) c.avg_mcs_ema = mcs_v;
  else c.avg_mcs_ema = (c.avg_mcs_ema * 7 + mcs_v) / 8;
}

static String phy_hist_json(const uint32_t *hist, int len, int head) {
  String s = "[";
  for (int i = 0; i < len; i++) {
    int idx = (head + i) % len;
    if (i) s += ",";
    s += String(hist[idx]);
  }
  return s + "]";
}

static String phy_arr_json(const uint32_t *arr, int len) {
  String s = "[";
  for (int i = 0; i < len; i++) {
    if (i) s += ",";
    s += String(arr[i]);
  }
  return s + "]";
}

static String phy_json_all() {
  PhyGlobalStats &g = g_phy.global;
  String s = "{\"active_channels\":" + String(g_phy.active_ch_count);
  s += ",\"total_frames\":" + String((uint32_t)g.total_frames);
  s += ",\"total_bytes\":" + String((uint32_t)g.total_bytes);
  s += ",\"total_retries\":" + String(g.total_retries);
  s += ",\"gen_total\":" + phy_arr_json(g.gen_total, 4);
  s += ",\"mcs_total\":" + phy_arr_json(g.mcs_total, PHY_MCS_BUCKETS);
  s += ",\"bw_total\":" + phy_arr_json(g.bw_total, 5);
  s += ",\"ss_total\":" + phy_arr_json(g.ss_total, 8);
  s += ",\"channels\":[";
  bool first = true;
  for (int i = 0; i < PHY_MAX_CHANNELS; i++) {
    PhyChannelStats &c = g_phy.channels[i];
    if (!c.active) continue;
    if (!first) s += ","; first = false;
    s += "{\"ch\":" + String(c.ch);
    s += ",\"frames\":" + String(c.frames_total);
    s += ",\"retries\":" + String(c.frames_retry);
    s += ",\"retry_pct\":" + String(c.retry_rate_ema / 10);
    s += ",\"avg_mcs\":" + String(c.avg_mcs_ema / 10);
    s += ",\"gen\":" + phy_arr_json(c.gen_count, 4);
    s += ",\"mcs\":" + phy_arr_json(c.mcs_hist, PHY_MCS_BUCKETS);
    s += ",\"bw\":" + phy_arr_json(c.bw_hist, 5);
    s += ",\"ss\":" + phy_arr_json(c.ss_hist, 8);
    s += ",\"gi\":" + phy_arr_json(c.gi_hist, 4);
    s += ",\"pkt_hist\":" + phy_hist_json(c.pkt_hist, PHY_HIST_LEN, c.hist_head) + "}";
  }
  s += "]}";
  return s;
}

inline void phy_routes() {
  server.on("/api/phy/all", HTTP_GET, []() {
    server.send(200, "application/json", phy_json_all());
  });
  server.on("/api/phy/clear", HTTP_POST, []() {
    memset(&g_phy, 0, sizeof(g_phy));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

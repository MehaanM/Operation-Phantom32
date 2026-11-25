#pragma once
// channel_analyzer.h — Detailed 802.11 channel and spectrum analyzer.
// Tracks per-channel AP density, interference overlap, channel bonding
// (HT40/VHT80/VHT160), BSS coexistence, and optimal channel suggestions.
// Prefix: caz_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define CAZ_MAX_CHANNELS 14
#define CAZ_MAX_APS_PER_CH 20
#define CAZ_HIST_SAMPLES 40

// Channel width in MHz
enum CazBW : uint8_t {
  CAZ_BW_20 = 0,
  CAZ_BW_40 = 1,
  CAZ_BW_80 = 2,
  CAZ_BW_160 = 3,
};

static uint8_t caz_bw_mhz(CazBW bw) {
  switch (bw) {
    case CAZ_BW_20:
      return 20;
    case CAZ_BW_40:
      return 40;
    case CAZ_BW_80:
      return 80;
    case CAZ_BW_160:
      return 160;
  }
  return 20;
}

// 2.4GHz channel overlap map: channels overlap if |ch1-ch2| <= 4
static inline bool caz_channels_overlap(uint8_t a, uint8_t b) {
  if (a > 14 || b > 14)
    return false;
  int diff = a - b;
  if (diff < 0)
    diff = -diff;
  return diff <= 4;
}

// AP record for channel density analysis
struct CazApRec {
  bool active;
  uint8_t bssid[6];
  char ssid[33];
  int8_t rssi;
  CazBW bw;
  uint8_t sec_ch;  // secondary channel for HT40
  bool is_ht;      // HT (802.11n)
  bool is_vht;     // VHT (802.11ac)
  bool is_he;      // HE (802.11ax)
  uint32_t last_ts;
};

// Per-channel analysis state
struct CazChannel {
  bool has_data;
  uint8_t ch;

  // AP density
  CazApRec aps[CAZ_MAX_APS_PER_CH];
  uint8_t ap_count;
  uint8_t ht_count;   // 802.11n APs
  uint8_t vht_count;  // 802.11ac APs
  uint8_t he_count;   // 802.11ax APs

  // RSSI
  int8_t rssi_min;
  int8_t rssi_max;
  int8_t rssi_mean;
  uint32_t frame_count;

  // Rolling RSSI history for sparkline
  int8_t rssi_hist[CAZ_HIST_SAMPLES];
  uint8_t hist_head;

  // Interference: APs from overlapping channels
  uint8_t overlap_count;
  uint8_t overlap_channels[8];  // which channels overlap here

  // Computed interference score (0=none, 100=maximum)
  uint8_t interference_score;

  // Optimal: is this a "clean" non-overlapping channel?
  bool is_preferred;  // ch 1, 6, or 11 for 2.4GHz

  uint32_t first_ts;
  uint32_t last_ts;
};

static struct {
  CazChannel ch[CAZ_MAX_CHANNELS + 1];
  uint8_t best_ch_2g;  // recommended 2.4GHz channel
  uint8_t best_score;  // score of best channel (lower = better)
  uint32_t total_frames;
} g_caz = {};

static bool caz_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static CazApRec *caz_find_ap(CazChannel &c, const uint8_t *bssid) {
  for (int i = 0; i < CAZ_MAX_APS_PER_CH; i++)
    if (c.aps[i].active && caz_mac_eq(c.aps[i].bssid, bssid))
      return &c.aps[i];
  return nullptr;
}

static CazApRec *caz_alloc_ap(CazChannel &c, const uint8_t *bssid) {
  for (int i = 0; i < CAZ_MAX_APS_PER_CH; i++) {
    if (!c.aps[i].active) {
      memset(&c.aps[i], 0, sizeof(CazApRec));
      c.aps[i].active = true;
      memcpy(c.aps[i].bssid, bssid, 6);
      c.ap_count++;
      return &c.aps[i];
    }
  }
  uint32_t oldest = UINT32_MAX;
  int idx = 0;
  for (int i = 0; i < CAZ_MAX_APS_PER_CH; i++)
    if (c.aps[i].last_ts < oldest) {
      oldest = c.aps[i].last_ts;
      idx = i;
    }
  memset(&c.aps[idx], 0, sizeof(CazApRec));
  c.aps[idx].active = true;
  memcpy(c.aps[idx].bssid, bssid, 6);
  return &c.aps[idx];
}

static void caz_compute_interference() {
  // For each 2.4GHz channel, count APs on overlapping channels
  for (int i = 1; i <= 13; i++) {
    CazChannel &c = g_caz.ch[i];
    if (!c.has_data)
      continue;
    c.overlap_count = 0;
    uint8_t overlap_ap_count = 0;
    for (int j = 1; j <= 13; j++) {
      if (j == i)
        continue;
      if (!g_caz.ch[j].has_data)
        continue;
      if (caz_channels_overlap(i, j)) {
        if (c.overlap_count < 8)
          c.overlap_channels[c.overlap_count++] = j;
        overlap_ap_count += g_caz.ch[j].ap_count;
      }
    }
    // Interference score: own APs + weighted overlapping APs (halved since they're partial)
    uint8_t score = c.ap_count + (overlap_ap_count / 2);
    c.interference_score = score > 100 ? 100 : score;
    c.is_preferred = (i == 1 || i == 6 || i == 11);
  }

  // Find best 2.4GHz channel (lowest interference, prefer 1/6/11)
  uint8_t best = 1;
  uint8_t best_sc = 255;
  for (int i = 1; i <= 11; i++) {
    if (!g_caz.ch[i].has_data) {
      if (g_caz.ch[i].is_preferred || i == 1 || i == 6 || i == 11) {
        if (best_sc > 0) {
          best = i;
          best_sc = 0;
        }
      }
      continue;
    }
    CazChannel &c = g_caz.ch[i];
    uint8_t effective = c.interference_score + (c.is_preferred ? 0 : 20);
    if (effective < best_sc) {
      best = i;
      best_sc = effective;
    }
  }
  g_caz.best_ch_2g = best;
  g_caz.best_score = best_sc;
}

void caz_ingest(uint8_t ch, int8_t rssi, const uint8_t *bssid, const char *ssid, bool is_ht,
                bool is_vht, bool is_he, CazBW bw, uint8_t sec_ch) {
  if (ch < 1 || ch > 14)
    return;
  CazChannel &c = g_caz.ch[ch];
  if (!c.has_data) {
    c.has_data = true;
    c.ch = ch;
    c.rssi_min = rssi;
    c.rssi_max = rssi;
    c.first_ts = millis() / 1000;
    c.is_preferred = (ch == 1 || ch == 6 || ch == 11);
  }

  c.frame_count++;
  g_caz.total_frames++;
  if (rssi < c.rssi_min)
    c.rssi_min = rssi;
  if (rssi > c.rssi_max)
    c.rssi_max = rssi;
  int16_t sum = c.rssi_mean * 10 + rssi;  // moving average
  c.rssi_mean = (int8_t)(sum / 11);

  // Rolling RSSI history (1 per 2 seconds)
  uint32_t now = millis() / 1000;
  if (c.last_ts == 0 || (now - c.last_ts >= 2)) {
    c.rssi_hist[c.hist_head % CAZ_HIST_SAMPLES] = c.rssi_mean;
    c.hist_head = (c.hist_head + 1) % CAZ_HIST_SAMPLES;
    c.last_ts = now;
  }

  // AP record
  if (bssid) {
    CazApRec *ap = caz_find_ap(c, bssid);
    if (!ap)
      ap = caz_alloc_ap(c, bssid);
    if (ap) {
      if (ssid)
        strncpy(ap->ssid, ssid, 32);
      ap->rssi = rssi;
      ap->bw = bw;
      ap->sec_ch = sec_ch;
      bool was_ht = ap->is_ht, was_vht = ap->is_vht, was_he = ap->is_he;
      ap->is_ht = is_ht;
      ap->is_vht = is_vht;
      ap->is_he = is_he;
      ap->last_ts = now;
      if (!was_ht && is_ht)
        c.ht_count++;
      if (!was_vht && is_vht)
        c.vht_count++;
      if (!was_he && is_he)
        c.he_count++;
    }
  }

  // Recompute interference periodically
  if ((g_caz.total_frames % 50) == 0)
    caz_compute_interference();
}

static void caz_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String caz_json_all() {
  String s = "{\"best_ch\":" + String(g_caz.best_ch_2g);
  s += ",\"best_score\":" + String(g_caz.best_score);
  s += ",\"total_frames\":" + String(g_caz.total_frames);
  s += ",\"channels\":[";
  bool first = true;
  for (int i = 1; i <= CAZ_MAX_CHANNELS; i++) {
    CazChannel &c = g_caz.ch[i];
    if (!c.has_data)
      continue;
    if (!first)
      s += ",";
    first = false;
    s += "{\"ch\":" + String(c.ch);
    s += ",\"frames\":" + String(c.frame_count);
    s += ",\"ap_count\":" + String(c.ap_count);
    s += ",\"ht\":" + String(c.ht_count);
    s += ",\"vht\":" + String(c.vht_count);
    s += ",\"he\":" + String(c.he_count);
    s += ",\"rssi_min\":" + String(c.rssi_min);
    s += ",\"rssi_max\":" + String(c.rssi_max);
    s += ",\"rssi_mean\":" + String(c.rssi_mean);
    s += ",\"interference\":" + String(c.interference_score);
    s += ",\"overlap_count\":" + String(c.overlap_count);
    s += ",\"preferred\":" + String(c.is_preferred ? "true" : "false");
    s += ",\"best\":" + String(i == g_caz.best_ch_2g ? "true" : "false");
    s += ",\"hist\":[";
    for (int j = 0; j < CAZ_HIST_SAMPLES; j++) {
      if (j)
        s += ",";
      s += String(c.rssi_hist[(c.hist_head + j) % CAZ_HIST_SAMPLES]);
    }
    s += "],\"aps\":[";
    char mac[18];
    bool afirst = true;
    for (int j = 0; j < CAZ_MAX_APS_PER_CH; j++) {
      if (!c.aps[j].active)
        continue;
      if (!afirst)
        s += ",";
      afirst = false;
      caz_mac_str(c.aps[j].bssid, mac);
      s += "{\"bssid\":\"" + String(mac) + "\",\"ssid\":\"" + String(c.aps[j].ssid) + "\"";
      s += ",\"rssi\":" + String(c.aps[j].rssi);
      s += ",\"bw\":" + String(caz_bw_mhz(c.aps[j].bw));
      s += ",\"ht\":" + String(c.aps[j].is_ht ? "true" : "false");
      s += ",\"vht\":" + String(c.aps[j].is_vht ? "true" : "false");
      s += ",\"he\":" + String(c.aps[j].is_he ? "true" : "false") + "}";
    }
    s += "]}";
  }
  s += "]}";
  return s;
}

inline void caz_routes() {
  server.on("/api/caz/all", HTTP_GET,
            []() { server.send(200, "application/json", caz_json_all()); });
  server.on("/api/caz/clear", HTTP_POST, []() {
    memset(&g_caz, 0, sizeof(g_caz));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

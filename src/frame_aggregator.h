#pragma once
// frame_aggregator.h — 802.11 A-MPDU and A-MSDU frame aggregation monitor.
// Tracks per-AP aggregation statistics: average A-MPDU size, subframe count,
// A-MSDU usage, and aggregation efficiency estimates.
// Prefix: fag_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define FAG_MAX_APS 48
#define FAG_HIST_LEN 60
#define FAG_MAX_SUBFRAME 64  // max subframes per A-MPDU

struct FagAP {
  bool active;
  uint8_t bssid[6];
  char ssid[33];
  uint8_t ch;
  int8_t rssi;

  // A-MPDU statistics
  uint32_t ampdu_count;      // A-MPDUs observed
  uint32_t ampdu_subframes;  // total subframe count in A-MPDUs
  uint32_t ampdu_max_subf;   // largest A-MPDU seen
  uint32_t ampdu_bytes;      // total bytes in A-MPDUs

  // A-MSDU statistics
  uint32_t amsdu_count;
  uint32_t amsdu_subframes;
  uint32_t amsdu_bytes;

  // Frame type counts
  uint32_t data_frames;
  uint32_t mgmt_frames;
  uint32_t ctrl_frames;

  // Aggregation efficiency: bytes/subframe (higher = more efficient packing)
  uint32_t avg_bytes_per_subframe_ema;  // EMA * 10 to avoid float

  // Rolling throughput
  uint32_t bytes_hist[FAG_HIST_LEN];
  uint8_t hist_head;
  uint32_t cur_bytes;
  uint32_t last_tick_s;

  // Subframe size histogram (8 buckets: 0-128, 128-256, ... >1024)
  uint32_t subframe_size_hist[8];

  uint32_t first_ts;
  uint32_t last_ts;
};

static struct {
  FagAP aps[FAG_MAX_APS];
  uint8_t count;
} g_fag = {};

static bool fag_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static FagAP *fag_find(const uint8_t *bssid) {
  for (int i = 0; i < FAG_MAX_APS; i++)
    if (g_fag.aps[i].active && fag_mac_eq(g_fag.aps[i].bssid, bssid))
      return &g_fag.aps[i];
  return nullptr;
}

static FagAP *fag_alloc(const uint8_t *bssid) {
  for (int i = 0; i < FAG_MAX_APS; i++) {
    if (!g_fag.aps[i].active) {
      FagAP &a = g_fag.aps[i];
      memset(&a, 0, sizeof(a));
      a.active = true;
      memcpy(a.bssid, bssid, 6);
      a.first_ts = millis() / 1000;
      g_fag.count++;
      return &a;
    }
  }
  uint32_t oldest = UINT32_MAX;
  int idx = 0;
  for (int i = 0; i < FAG_MAX_APS; i++)
    if (g_fag.aps[i].last_ts < oldest) {
      oldest = g_fag.aps[i].last_ts;
      idx = i;
    }
  memset(&g_fag.aps[idx], 0, sizeof(g_fag.aps[0]));
  g_fag.aps[idx].active = true;
  memcpy(g_fag.aps[idx].bssid, bssid, 6);
  g_fag.aps[idx].first_ts = millis() / 1000;
  return &g_fag.aps[idx];
}

static void fag_tick(FagAP &a) {
  uint32_t now_s = millis() / 1000;
  if (now_s == a.last_tick_s)
    return;
  a.bytes_hist[a.hist_head % FAG_HIST_LEN] = a.cur_bytes;
  a.hist_head = (a.hist_head + 1) % FAG_HIST_LEN;
  a.last_tick_s = now_s;
  a.cur_bytes = 0;
}

static int fag_size_bucket(uint16_t sz) {
  if (sz < 128)
    return 0;
  if (sz < 256)
    return 1;
  if (sz < 384)
    return 2;
  if (sz < 512)
    return 3;
  if (sz < 768)
    return 4;
  if (sz < 1024)
    return 5;
  if (sz < 2048)
    return 6;
  return 7;
}

void fag_ingest_ampdu(const uint8_t *ap_bssid, const char *ssid, uint8_t ch, int8_t rssi,
                      uint8_t subframe_count, uint16_t total_len, uint8_t frame_type) {
  FagAP *a = fag_find(ap_bssid);
  if (!a)
    a = fag_alloc(ap_bssid);
  if (!a)
    return;

  if (ssid && ssid[0])
    strncpy(a->ssid, ssid, 32);
  a->ch = ch;
  if (rssi < 0)
    a->rssi = rssi;
  a->last_ts = millis() / 1000;

  fag_tick(*a);
  a->cur_bytes += total_len;

  a->ampdu_count++;
  a->ampdu_subframes += subframe_count;
  a->ampdu_bytes += total_len;
  if (subframe_count > a->ampdu_max_subf)
    a->ampdu_max_subf = subframe_count;

  if (subframe_count > 0) {
    uint16_t avg_sf = total_len / subframe_count;
    a->subframe_size_hist[fag_size_bucket(avg_sf)]++;
    // EMA of bytes/subframe
    uint32_t bps = (uint32_t)avg_sf * 10;
    if (a->avg_bytes_per_subframe_ema == 0)
      a->avg_bytes_per_subframe_ema = bps;
    else
      a->avg_bytes_per_subframe_ema = (a->avg_bytes_per_subframe_ema * 7 + bps) / 8;
  }

  switch (frame_type) {
    case 0:
      a->mgmt_frames++;
      break;
    case 1:
      a->ctrl_frames++;
      break;
    case 2:
      a->data_frames++;
      break;
  }
}

void fag_ingest_amsdu(const uint8_t *ap_bssid, const char *ssid, uint8_t ch, int8_t rssi,
                      uint8_t subframe_count, uint16_t total_len) {
  FagAP *a = fag_find(ap_bssid);
  if (!a)
    a = fag_alloc(ap_bssid);
  if (!a)
    return;

  if (ssid && ssid[0])
    strncpy(a->ssid, ssid, 32);
  a->ch = ch;
  if (rssi < 0)
    a->rssi = rssi;
  a->last_ts = millis() / 1000;

  fag_tick(*a);
  a->cur_bytes += total_len;
  a->amsdu_count++;
  a->amsdu_subframes += subframe_count;
  a->amsdu_bytes += total_len;
}

static String fag_hist_json(const FagAP &a) {
  String s = "[";
  for (int i = 0; i < FAG_HIST_LEN; i++) {
    int idx = (a.hist_head + i) % FAG_HIST_LEN;
    if (i)
      s += ",";
    s += String(a.bytes_hist[idx]);
  }
  return s + "]";
}

static String fag_sf_hist_json(const FagAP &a) {
  String s = "[";
  for (int i = 0; i < 8; i++) {
    if (i)
      s += ",";
    s += String(a.subframe_size_hist[i]);
  }
  return s + "]";
}

static void fag_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String fag_json_all() {
  String s = "{\"count\":" + String(g_fag.count) + ",\"aps\":[";
  char mac[18];
  bool first = true;
  for (int i = 0; i < FAG_MAX_APS; i++) {
    FagAP &a = g_fag.aps[i];
    if (!a.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    fag_mac_str(a.bssid, mac);
    uint32_t avg_sf_cnt = a.ampdu_count > 0 ? a.ampdu_subframes / a.ampdu_count : 0;
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(a.ssid) + "\"";
    s += ",\"ch\":" + String(a.ch);
    s += ",\"rssi\":" + String(a.rssi);
    s += ",\"ampdu_count\":" + String(a.ampdu_count);
    s += ",\"avg_subframes\":" + String(avg_sf_cnt);
    s += ",\"max_subframes\":" + String(a.ampdu_max_subf);
    s += ",\"ampdu_bytes\":" + String(a.ampdu_bytes);
    s += ",\"amsdu_count\":" + String(a.amsdu_count);
    s += ",\"amsdu_bytes\":" + String(a.amsdu_bytes);
    s += ",\"avg_bps\":" + String(a.avg_bytes_per_subframe_ema / 10);
    s += ",\"data\":" + String(a.data_frames);
    s += ",\"mgmt\":" + String(a.mgmt_frames);
    s += ",\"ctrl\":" + String(a.ctrl_frames);
    s += ",\"bytes_hist\":" + fag_hist_json(a);
    s += ",\"sf_size_hist\":" + fag_sf_hist_json(a) + "}";
  }
  s += "]}";
  return s;
}

inline void fag_routes() {
  server.on("/api/fag/all", HTTP_GET,
            []() { server.send(200, "application/json", fag_json_all()); });
  server.on("/api/fag/clear", HTTP_POST, []() {
    memset(&g_fag, 0, sizeof(g_fag));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

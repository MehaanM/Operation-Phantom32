#pragma once
// data_rate_tracker.h — 802.11 data rate tracking from radiotap MCS/VHT/HE fields.
// Tracks per-AP effective data rates seen on-air.
// Prefix: drt_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define DRT_MAX_APS    64
#define DRT_HIST_LEN   60
#define DRT_MAX_RATES  16   // distinct rates seen per AP

struct DrtRateSample {
  uint16_t rate_mbps;    // 1 = 1Mbps, 540 = 54.0Mbps, etc (tenths)
  uint32_t count;
};

struct DrtAP {
  bool    active;
  uint8_t bssid[6];
  char    ssid[33];
  uint8_t ch;
  int8_t  rssi;
  uint32_t first_ts;
  uint32_t last_ts;
  uint32_t frame_count;

  // Per-rate counts
  DrtRateSample rates[DRT_MAX_RATES];
  uint8_t  rate_count;

  // Rolling throughput estimate (Mbps * 10 per second)
  uint32_t hist_rate[DRT_HIST_LEN];   // rate in Kbps per second
  uint8_t  hist_head;

  // Summary stats
  uint16_t min_rate_mbps10;  // tenths of Mbps
  uint16_t max_rate_mbps10;
  uint16_t avg_rate_mbps10;
  uint16_t last_rate_mbps10;

  uint32_t cur_bytes;
  uint32_t last_tick_s;
};

static struct {
  DrtAP   aps[DRT_MAX_APS];
  uint8_t count;
} g_drt = {};

static bool drt_mac_eq(const uint8_t *a, const uint8_t *b) { return memcmp(a,b,6)==0; }

static DrtAP* drt_find(const uint8_t *bssid) {
  for (int i = 0; i < DRT_MAX_APS; i++)
    if (g_drt.aps[i].active && drt_mac_eq(g_drt.aps[i].bssid, bssid))
      return &g_drt.aps[i];
  return nullptr;
}

static DrtAP* drt_alloc(const uint8_t *bssid) {
  for (int i = 0; i < DRT_MAX_APS; i++) {
    if (!g_drt.aps[i].active) {
      DrtAP &a = g_drt.aps[i];
      memset(&a, 0, sizeof(a));
      a.active = true;
      memcpy(a.bssid, bssid, 6);
      a.min_rate_mbps10 = UINT16_MAX;
      a.first_ts = millis()/1000;
      g_drt.count++;
      return &a;
    }
  }
  uint32_t oldest = UINT32_MAX; int idx = 0;
  for (int i = 0; i < DRT_MAX_APS; i++)
    if (g_drt.aps[i].last_ts < oldest) { oldest = g_drt.aps[i].last_ts; idx = i; }
  memset(&g_drt.aps[idx], 0, sizeof(g_drt.aps[0]));
  g_drt.aps[idx].active = true;
  memcpy(g_drt.aps[idx].bssid, bssid, 6);
  g_drt.aps[idx].min_rate_mbps10 = UINT16_MAX;
  g_drt.aps[idx].first_ts = millis()/1000;
  return &g_drt.aps[idx];
}

static void drt_tick(DrtAP &a) {
  uint32_t now_s = millis()/1000;
  if (now_s == a.last_tick_s) return;
  a.last_tick_s = now_s;
  // bytes * 8 / 1000 = Kbps
  uint32_t kbps = (a.cur_bytes * 8) / 1000;
  a.hist_rate[a.hist_head % DRT_HIST_LEN] = kbps;
  a.hist_head = (a.hist_head + 1) % DRT_HIST_LEN;
  a.cur_bytes = 0;
}

static void drt_update_rate_table(DrtAP &a, uint16_t rate_mbps10) {
  for (int i = 0; i < DRT_MAX_RATES; i++) {
    if (a.rates[i].rate_mbps == rate_mbps10) {
      a.rates[i].count++;
      return;
    }
    if (!a.rates[i].count) {
      a.rates[i].rate_mbps = rate_mbps10;
      a.rates[i].count = 1;
      if (a.rate_count < DRT_MAX_RATES) a.rate_count++;
      return;
    }
  }
  // Replace least common
  uint32_t min_cnt = UINT32_MAX; int idx = 0;
  for (int i = 0; i < DRT_MAX_RATES; i++)
    if (a.rates[i].count < min_cnt) { min_cnt = a.rates[i].count; idx = i; }
  a.rates[idx].rate_mbps = rate_mbps10;
  a.rates[idx].count = 1;
}

void drt_ingest(const uint8_t *bssid, const char *ssid, uint8_t ch, int8_t rssi,
                uint16_t rate_mbps10, uint16_t len) {
  DrtAP *a = drt_find(bssid);
  if (!a) a = drt_alloc(bssid);
  if (!a) return;

  if (ssid && ssid[0]) strncpy(a->ssid, ssid, 32);
  a->ch = ch;
  if (rssi < 0) a->rssi = rssi;
  a->last_ts = millis()/1000;
  a->frame_count++;
  a->last_rate_mbps10 = rate_mbps10;

  drt_tick(*a);
  a->cur_bytes += len;

  if (rate_mbps10 > 0) {
    if (rate_mbps10 < a->min_rate_mbps10) a->min_rate_mbps10 = rate_mbps10;
    if (rate_mbps10 > a->max_rate_mbps10) a->max_rate_mbps10 = rate_mbps10;
    drt_update_rate_table(*a, rate_mbps10);

    // Rolling avg
    if (a->avg_rate_mbps10 == 0) a->avg_rate_mbps10 = rate_mbps10;
    else a->avg_rate_mbps10 = (uint16_t)((a->avg_rate_mbps10 * 7 + rate_mbps10) / 8);
  }
}

static void drt_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String drt_hist_json(const DrtAP &a) {
  String s = "[";
  for (int i = 0; i < DRT_HIST_LEN; i++) {
    int idx = (a.hist_head + i) % DRT_HIST_LEN;
    if (i) s += ",";
    s += String(a.hist_rate[idx]);
  }
  return s + "]";
}

static String drt_json_all() {
  String s = "{\"count\":" + String(g_drt.count);
  s += ",\"aps\":[";
  char mac[18]; bool first = true;
  for (int i = 0; i < DRT_MAX_APS; i++) {
    DrtAP &a = g_drt.aps[i];
    if (!a.active) continue;
    if (!first) s += ","; first = false;
    drt_mac_str(a.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(a.ssid) + "\"";
    s += ",\"ch\":" + String(a.ch);
    s += ",\"rssi\":" + String(a.rssi);
    s += ",\"frames\":" + String(a.frame_count);
    s += ",\"last_rate\":" + String(a.last_rate_mbps10);
    s += ",\"min_rate\":" + String(a.min_rate_mbps10 == UINT16_MAX ? 0 : a.min_rate_mbps10);
    s += ",\"max_rate\":" + String(a.max_rate_mbps10);
    s += ",\"avg_rate\":" + String(a.avg_rate_mbps10);
    s += ",\"hist\":" + drt_hist_json(a);
    s += ",\"rate_table\":[";
    bool fr = true;
    for (int j = 0; j < DRT_MAX_RATES; j++) {
      if (!a.rates[j].count) continue;
      if (!fr) s += ","; fr = false;
      s += "{\"r\":" + String(a.rates[j].rate_mbps);
      s += ",\"n\":" + String(a.rates[j].count) + "}";
    }
    s += "]}";
  }
  s += "]}";
  return s;
}

inline void drt_routes() {
  server.on("/api/drt/all", HTTP_GET, []() {
    for (int i = 0; i < DRT_MAX_APS; i++)
      if (g_drt.aps[i].active) drt_tick(g_drt.aps[i]);
    server.send(200, "application/json", drt_json_all());
  });
  server.on("/api/drt/clear", HTTP_POST, []() {
    memset(&g_drt, 0, sizeof(g_drt));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

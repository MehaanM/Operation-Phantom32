#pragma once
// beacon_rate_tracker.h — Per-AP beacon rate and interval anomaly tracker.
// Detects beacon flooding, irregular intervals, and beacon suppression.
// Prefix: brt_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define BRT_MAX_APS 40
#define BRT_HIST_LEN 60

// Anomaly flags (bitmask)
#define BRT_ANOM_FLOOD 0x01        // beacons per second > threshold
#define BRT_ANOM_IRREGULAR 0x02    // interval jitter too high
#define BRT_ANOM_SILENT 0x04       // missed beacons
#define BRT_ANOM_INTERVAL_CH 0x08  // beacon interval changed
#define BRT_ANOM_SSID_CH 0x10      // SSID changed between beacons

struct BrtAP {
  bool active;
  uint8_t bssid[6];
  char ssid[33];
  uint8_t ch;
  int8_t rssi;

  uint16_t beacon_interval;       // advertised interval in TU
  uint16_t prev_beacon_interval;  // prior advertised interval

  uint32_t total_beacons;
  uint32_t missed_beacons;  // estimated from gaps
  uint32_t interval_changes;

  // Timing
  uint32_t first_ts;
  uint32_t last_ts;
  uint32_t last_beacon_ms;  // millis() of last beacon

  // Gap statistics (ms)
  uint32_t avg_gap_ms;  // EMA of observed inter-beacon gap
  uint32_t min_gap_ms;
  uint32_t max_gap_ms;
  uint32_t jitter_ema;  // EMA of |gap - expected|

  // Beacon rate history (beacons/second * 100)
  uint8_t rate_hist[BRT_HIST_LEN];  // beacons per sec (whole)
  uint8_t hist_head;
  uint32_t cur_count;
  uint32_t last_tick_s;

  // Anomalies
  uint8_t anomaly_flags;
  uint32_t flood_count;  // times flood threshold exceeded
  uint32_t anomaly_ts;

  // Expected gap in ms (derived from beacon_interval in TU)
  uint32_t expected_gap_ms;  // beacon_interval * 1.024
};

static struct {
  BrtAP aps[BRT_MAX_APS];
  uint8_t count;
  uint32_t total_beacons;
  uint32_t flood_events;
} g_brt = {};

#define BRT_FLOOD_THRESHOLD_PER_SEC 20
#define BRT_JITTER_THRESHOLD_MS 50
#define BRT_SILENCE_MULTIPLIER 3  // missed if gap > interval * 3

static bool brt_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static BrtAP *brt_find(const uint8_t *bssid) {
  for (int i = 0; i < BRT_MAX_APS; i++)
    if (g_brt.aps[i].active && brt_mac_eq(g_brt.aps[i].bssid, bssid))
      return &g_brt.aps[i];
  return nullptr;
}

static BrtAP *brt_alloc(const uint8_t *bssid) {
  for (int i = 0; i < BRT_MAX_APS; i++) {
    if (!g_brt.aps[i].active) {
      BrtAP &a = g_brt.aps[i];
      memset(&a, 0, sizeof(a));
      a.active = true;
      memcpy(a.bssid, bssid, 6);
      a.min_gap_ms = UINT32_MAX;
      a.first_ts = millis() / 1000;
      g_brt.count++;
      return &a;
    }
  }
  // Evict oldest
  uint32_t oldest = UINT32_MAX;
  int idx = 0;
  for (int i = 0; i < BRT_MAX_APS; i++)
    if (g_brt.aps[i].last_ts < oldest) {
      oldest = g_brt.aps[i].last_ts;
      idx = i;
    }
  memset(&g_brt.aps[idx], 0, sizeof(BrtAP));
  g_brt.aps[idx].active = true;
  g_brt.aps[idx].min_gap_ms = UINT32_MAX;
  memcpy(g_brt.aps[idx].bssid, bssid, 6);
  g_brt.aps[idx].first_ts = millis() / 1000;
  return &g_brt.aps[idx];
}

static void brt_tick(BrtAP &a) {
  uint32_t now_s = millis() / 1000;
  if (now_s == a.last_tick_s)
    return;
  uint32_t rate = a.cur_count;
  a.rate_hist[a.hist_head % BRT_HIST_LEN] = (uint8_t)(rate > 255 ? 255 : rate);
  a.hist_head = (a.hist_head + 1) % BRT_HIST_LEN;
  if (rate > BRT_FLOOD_THRESHOLD_PER_SEC) {
    a.anomaly_flags |= BRT_ANOM_FLOOD;
    a.flood_count++;
    g_brt.flood_events++;
  } else {
    a.anomaly_flags &= ~BRT_ANOM_FLOOD;
  }
  a.cur_count = 0;
  a.last_tick_s = now_s;
}

void brt_ingest(const uint8_t *bssid, const char *ssid, uint8_t ch, int8_t rssi,
                uint16_t beacon_interval) {
  BrtAP *a = brt_find(bssid);
  if (!a)
    a = brt_alloc(bssid);
  if (!a)
    return;

  brt_tick(*a);
  a->ch = ch;
  a->rssi = rssi;
  a->total_beacons++;
  g_brt.total_beacons++;
  a->cur_count++;

  // SSID change detection
  if (ssid && ssid[0] && strncmp(a->ssid, ssid, 32) != 0) {
    if (a->ssid[0] != '\0')
      a->anomaly_flags |= BRT_ANOM_SSID_CH;
    strncpy(a->ssid, ssid, 32);
  }

  // Beacon interval change detection
  if (a->beacon_interval > 0 && beacon_interval != a->beacon_interval) {
    a->prev_beacon_interval = a->beacon_interval;
    a->interval_changes++;
    a->anomaly_flags |= BRT_ANOM_INTERVAL_CH;
  }
  a->beacon_interval = beacon_interval;
  a->expected_gap_ms = (uint32_t)beacon_interval * 1024 / 1000;

  // Gap tracking
  uint32_t now_ms = millis();
  if (a->last_beacon_ms > 0) {
    uint32_t gap = now_ms - a->last_beacon_ms;
    if (gap < a->min_gap_ms)
      a->min_gap_ms = gap;
    if (gap > a->max_gap_ms)
      a->max_gap_ms = gap;

    if (a->avg_gap_ms == 0)
      a->avg_gap_ms = gap;
    else
      a->avg_gap_ms = (a->avg_gap_ms * 15 + gap) / 16;

    // Jitter
    uint32_t exp = a->expected_gap_ms;
    uint32_t dev = gap > exp ? gap - exp : exp - gap;
    if (a->jitter_ema == 0)
      a->jitter_ema = dev;
    else
      a->jitter_ema = (a->jitter_ema * 7 + dev) / 8;

    if (a->jitter_ema > BRT_JITTER_THRESHOLD_MS)
      a->anomaly_flags |= BRT_ANOM_IRREGULAR;
    else
      a->anomaly_flags &= ~BRT_ANOM_IRREGULAR;

    // Missed beacon estimation
    if (exp > 0 && gap > exp * BRT_SILENCE_MULTIPLIER) {
      a->missed_beacons += (gap / exp) - 1;
      a->anomaly_flags |= BRT_ANOM_SILENT;
    }
  }
  a->last_beacon_ms = now_ms;
  a->last_ts = millis() / 1000;
}

static void brt_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String brt_hist_json(const uint8_t *hist, int len, int head) {
  String s = "[";
  for (int i = 0; i < len; i++) {
    if (i)
      s += ",";
    s += String(hist[(head + i) % len]);
  }
  return s + "]";
}

static String brt_json_all() {
  String s = "{\"count\":" + String(g_brt.count);
  s += ",\"total_beacons\":" + String(g_brt.total_beacons);
  s += ",\"flood_events\":" + String(g_brt.flood_events);
  s += ",\"aps\":[";
  char mac[18];
  bool first = true;
  for (int i = 0; i < BRT_MAX_APS; i++) {
    BrtAP &a = g_brt.aps[i];
    if (!a.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    brt_mac_str(a.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(a.ssid) + "\"";
    s += ",\"ch\":" + String(a.ch);
    s += ",\"rssi\":" + String(a.rssi);
    s += ",\"bcn_int\":" + String(a.beacon_interval);
    s += ",\"prev_int\":" + String(a.prev_beacon_interval);
    s += ",\"total\":" + String(a.total_beacons);
    s += ",\"missed\":" + String(a.missed_beacons);
    s += ",\"int_changes\":" + String(a.interval_changes);
    s += ",\"avg_gap\":" + String(a.avg_gap_ms);
    s += ",\"min_gap\":" + String(a.min_gap_ms == UINT32_MAX ? 0 : a.min_gap_ms);
    s += ",\"max_gap\":" + String(a.max_gap_ms);
    s += ",\"jitter\":" + String(a.jitter_ema);
    s += ",\"expected_gap\":" + String(a.expected_gap_ms);
    s += ",\"anomaly\":" + String(a.anomaly_flags);
    s += ",\"flood_count\":" + String(a.flood_count);
    s += ",\"rate_hist\":" + brt_hist_json(a.rate_hist, BRT_HIST_LEN, a.hist_head) + "}";
  }
  s += "]}";
  return s;
}

inline void brt_routes() {
  server.on("/api/brt/all", HTTP_GET,
            []() { server.send(200, "application/json", brt_json_all()); });
  server.on("/api/brt/clear", HTTP_POST, []() {
    memset(&g_brt, 0, sizeof(g_brt));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

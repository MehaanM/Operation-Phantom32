#pragma once
// beacon_quality.h — Beacon frame quality and consistency tracker.
// Detects beacon anomalies: timestamp jumps, interval drift, SSID changes.
// Prefix: bcq_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define BCQ_MAX_APS     48
#define BCQ_HIST_LEN    30

struct BcqAP {
  bool    active;
  uint8_t bssid[6];
  char    ssid[33];
  uint8_t channel;
  int8_t  rssi;

  // Beacon interval tracking (nominal in TUs, observed in ms)
  uint16_t nominal_interval_tu;    // from beacon Interval field
  uint32_t last_beacon_ms;         // last beacon timestamp (millis)
  uint32_t acc_interval_ms;        // running sum of observed intervals
  uint32_t interval_count;
  uint32_t avg_interval_ms;        // rolling average observed interval
  uint32_t min_interval_ms;
  uint32_t max_interval_ms;

  // Interval jitter (deviation from expected)
  uint32_t jitter_max_ms;
  uint32_t jitter_avg_ms;

  // Quality score (0-100)
  uint8_t  quality;

  // Per-second rate ring
  uint8_t  rate_hist[BCQ_HIST_LEN]; // beacons per 2 seconds
  uint8_t  rate_head;
  uint8_t  acc_beacons;
  uint32_t acc_ts;

  // Anomaly flags
  bool     ssid_changed;
  bool     interval_changed;
  bool     timestamp_jump;
  bool     rate_spike;

  uint32_t total_beacons;
  uint32_t anomaly_count;
  uint32_t first_ts;
  uint32_t last_ts;

  // Last few SSIDs (for SSID change detection)
  char    prev_ssid[33];
  bool    prev_ssid_valid;
};

static struct {
  BcqAP   aps[BCQ_MAX_APS];
  uint8_t ap_count;
  uint32_t total_beacons;
  uint32_t total_anomalies;
} g_bcq = {};

static bool bcq_mac_eq(const uint8_t *a, const uint8_t *b) { return memcmp(a, b, 6) == 0; }

static BcqAP* bcq_find_or_alloc(const uint8_t *bssid) {
  for (int i = 0; i < BCQ_MAX_APS; i++)
    if (g_bcq.aps[i].active && bcq_mac_eq(g_bcq.aps[i].bssid, bssid))
      return &g_bcq.aps[i];
  for (int i = 0; i < BCQ_MAX_APS; i++) {
    if (!g_bcq.aps[i].active) {
      BcqAP &a = g_bcq.aps[i];
      memset(&a, 0, sizeof(a));
      a.active  = true;
      memcpy(a.bssid, bssid, 6);
      a.rssi    = -100;
      a.min_interval_ms = UINT32_MAX;
      a.first_ts = millis()/1000;
      g_bcq.ap_count++;
      return &a;
    }
  }
  uint32_t oldest = UINT32_MAX; int idx = 0;
  for (int i = 0; i < BCQ_MAX_APS; i++)
    if (g_bcq.aps[i].last_ts < oldest) { oldest = g_bcq.aps[i].last_ts; idx = i; }
  memset(&g_bcq.aps[idx], 0, sizeof(g_bcq.aps[0]));
  g_bcq.aps[idx].active  = true;
  memcpy(g_bcq.aps[idx].bssid, bssid, 6);
  g_bcq.aps[idx].min_interval_ms = UINT32_MAX;
  g_bcq.aps[idx].first_ts = millis()/1000;
  return &g_bcq.aps[idx];
}

static uint8_t bcq_compute_quality(BcqAP &a) {
  int q = 100;
  // Jitter penalty: each 20ms of average jitter costs 5 points
  if (a.avg_interval_ms && a.nominal_interval_tu) {
    uint32_t expected_ms = (uint32_t)a.nominal_interval_tu * 1024 / 1000;
    int drift = abs((int)a.avg_interval_ms - (int)expected_ms);
    q -= drift / 4;
  }
  if (a.ssid_changed)   q -= 20;
  if (a.timestamp_jump) q -= 15;
  if (a.rate_spike)     q -= 10;
  if (a.interval_changed) q -= 10;
  return (uint8_t)max(0, min(100, q));
}

void bcq_ingest(const uint8_t *bssid, const char *ssid,
                uint8_t channel, int8_t rssi,
                uint16_t interval_tu) {
  g_bcq.total_beacons++;
  uint32_t now_ms = millis();
  uint32_t now_s  = now_ms / 1000;

  BcqAP *a = bcq_find_or_alloc(bssid);
  if (!a) return;

  a->channel = channel;
  if (rssi < 0) a->rssi = rssi;
  a->last_ts = now_s;
  a->total_beacons++;

  // Rate accumulator
  if (now_s / 2 != a->acc_ts / 2) {
    a->rate_hist[a->rate_head] = a->acc_beacons;
    a->rate_head = (a->rate_head + 1) % BCQ_HIST_LEN;
    a->rate_spike = (a->acc_beacons > 20); // >10/s average over window
    a->acc_beacons = 0;
    a->acc_ts = now_s;
  }
  a->acc_beacons++;

  // Nominal interval check
  if (interval_tu && a->nominal_interval_tu && interval_tu != a->nominal_interval_tu) {
    a->interval_changed = true;
    a->anomaly_count++;
    g_bcq.total_anomalies++;
  }
  if (interval_tu) a->nominal_interval_tu = interval_tu;

  // SSID change detection
  if (ssid && ssid[0]) {
    if (a->ssid[0] && strcmp(a->ssid, ssid) != 0) {
      strncpy(a->prev_ssid, a->ssid, 32);
      a->prev_ssid_valid = true;
      a->ssid_changed    = true;
      a->anomaly_count++;
      g_bcq.total_anomalies++;
    }
    strncpy(a->ssid, ssid, 32);
  }

  // Interval measurement
  if (a->last_beacon_ms && now_ms > a->last_beacon_ms) {
    uint32_t delta = now_ms - a->last_beacon_ms;
    if (delta > 10 && delta < 500) { // plausible range
      a->acc_interval_ms += delta;
      a->interval_count++;

      if (delta < a->min_interval_ms) a->min_interval_ms = delta;
      if (delta > a->max_interval_ms) a->max_interval_ms = delta;

      a->avg_interval_ms = a->acc_interval_ms / a->interval_count;

      // Jitter
      uint32_t expected = (uint32_t)a->nominal_interval_tu * 1024 / 1000;
      if (expected) {
        uint32_t diff = (delta > expected) ? (delta - expected) : (expected - delta);
        if (diff > a->jitter_max_ms) a->jitter_max_ms = diff;
        a->jitter_avg_ms = (a->jitter_avg_ms * 7 + diff) / 8;
      }
    }
  }
  a->last_beacon_ms = now_ms;
  a->quality = bcq_compute_quality(*a);
}

static void bcq_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String bcq_json_all() {
  String s = "{\"ap_count\":" + String(g_bcq.ap_count);
  s += ",\"total_beacons\":" + String(g_bcq.total_beacons);
  s += ",\"total_anomalies\":" + String(g_bcq.total_anomalies);
  s += ",\"aps\":[";
  char mac[18];
  bool first = true;
  for (int i = 0; i < BCQ_MAX_APS; i++) {
    BcqAP &a = g_bcq.aps[i];
    if (!a.active) continue;
    if (!first) s += ",";
    first = false;
    bcq_mac_str(a.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(a.ssid) + "\"";
    s += ",\"ch\":" + String(a.channel);
    s += ",\"rssi\":" + String(a.rssi);
    s += ",\"interval_tu\":" + String(a.nominal_interval_tu);
    s += ",\"avg_ms\":" + String(a.avg_interval_ms);
    s += ",\"jitter_avg\":" + String(a.jitter_avg_ms);
    s += ",\"jitter_max\":" + String(a.jitter_max_ms);
    s += ",\"quality\":" + String(a.quality);
    s += ",\"beacons\":" + String(a.total_beacons);
    s += ",\"anomalies\":" + String(a.anomaly_count);
    s += ",\"ssid_changed\":" + String(a.ssid_changed?"true":"false");
    s += ",\"interval_changed\":" + String(a.interval_changed?"true":"false");
    s += ",\"rate_spike\":" + String(a.rate_spike?"true":"false");
    if (a.prev_ssid_valid)
      s += ",\"prev_ssid\":\"" + String(a.prev_ssid) + "\"";
    s += ",\"rate_hist\":[";
    for (int j = 0; j < BCQ_HIST_LEN; j++) {
      int pos = (a.rate_head + j) % BCQ_HIST_LEN;
      if (j) s += ",";
      s += String(a.rate_hist[pos]);
    }
    s += "]}";
  }
  s += "]}";
  return s;
}

inline void bcq_routes() {
  server.on("/api/bcq/all", HTTP_GET, []() {
    server.send(200, "application/json", bcq_json_all());
  });
  server.on("/api/bcq/clear", HTTP_POST, []() {
    memset(&g_bcq, 0, sizeof(g_bcq));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

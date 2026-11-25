#pragma once
// tsf_tracker.h — TSF (Timing Synchronization Function) tracker.
// Estimates AP uptime, clock skew between APs, TSF rollover detection,
// and infrastructure timing synchronization quality assessment.
// Prefix: tsf_

#include <Arduino.h>
#include <WebServer.h>
#include "psram_attr.h"

extern WebServer server;

#define TSF_MAX_APS 32
#define TSF_HIST_LEN 20

// TSF state
enum TsfState : uint8_t {
  TSF_STATE_INIT = 0,      // First beacon, no history
  TSF_STATE_TRACKING = 1,  // Normal tracking
  TSF_STATE_SKEWED = 2,    // Clock skew detected
  TSF_STATE_JUMP = 3,      // TSF discontinuity
  TSF_STATE_ROLLOVER = 4,  // Near or past 64-bit rollover
};

static const char *tsf_state_str(TsfState s) {
  switch (s) {
    case TSF_STATE_INIT:
      return "Init";
    case TSF_STATE_TRACKING:
      return "Tracking";
    case TSF_STATE_SKEWED:
      return "Skewed";
    case TSF_STATE_JUMP:
      return "Jump";
    case TSF_STATE_ROLLOVER:
      return "Rollover";
  }
  return "Unknown";
}

struct TsfAP {
  bool active;
  uint8_t bssid[6];
  char ssid[33];
  uint8_t ch;
  int8_t rssi;

  TsfState state;

  // TSF measurements
  uint64_t first_tsf;        // first TSF seen
  uint64_t last_tsf;         // most recent TSF
  uint64_t tsf_at_arrival;   // TSF value when last beacon arrived
  uint32_t local_ms_at_tsf;  // local millis() at that arrival

  // Uptime estimation
  uint64_t tsf_uptime_us;       // TSF-derived uptime (microseconds)
  uint32_t estimated_uptime_s;  // human-readable uptime seconds
  uint32_t estimated_days;
  uint8_t estimated_hours;
  uint8_t estimated_mins;
  uint8_t estimated_secs;

  // Clock quality
  int32_t drift_ppm_hist[TSF_HIST_LEN];  // ppm measurements
  uint8_t drift_hist_head;
  int32_t drift_ppm_avg;  // running average drift
  int32_t drift_ppm_max;  // worst drift seen
  bool drift_ok;          // within ±100ppm spec

  // Continuity
  uint16_t beacon_count;
  uint16_t missed_beacons;  // estimated from TSF gaps
  uint16_t jump_events;
  uint32_t last_tick_s;

  // Interval tracking
  uint16_t nominal_interval_tu;
  uint32_t nominal_interval_us;

  uint32_t first_seen_ts;
  uint32_t last_seen_ts;
};

typedef struct {
  TsfAP aps[TSF_MAX_APS];
  uint8_t count;
} G_TsfState;
#ifdef BOARD_HAS_PSRAM
static G_TsfState *_g_tsf_raw;
#define g_tsf (*_g_tsf_raw)
#else
static G_TsfState g_tsf;
#endif

static bool tsf_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static TsfAP *tsf_find(const uint8_t *bssid) {
  for (int i = 0; i < TSF_MAX_APS; i++)
    if (g_tsf.aps[i].active && tsf_mac_eq(g_tsf.aps[i].bssid, bssid))
      return &g_tsf.aps[i];
  return nullptr;
}

static TsfAP *tsf_alloc(const uint8_t *bssid) {
  for (int i = 0; i < TSF_MAX_APS; i++) {
    if (!g_tsf.aps[i].active) {
      TsfAP &a = g_tsf.aps[i];
      memset(&a, 0, sizeof(a));
      a.active = true;
      memcpy(a.bssid, bssid, 6);
      a.state = TSF_STATE_INIT;
      a.drift_ppm_max = 0;
      a.drift_ok = true;
      a.first_seen_ts = millis() / 1000;
      g_tsf.count++;
      return &a;
    }
  }
  uint32_t oldest = UINT32_MAX;
  int idx = 0;
  for (int i = 0; i < TSF_MAX_APS; i++)
    if (g_tsf.aps[i].last_seen_ts < oldest) {
      oldest = g_tsf.aps[i].last_seen_ts;
      idx = i;
    }
  memset(&g_tsf.aps[idx], 0, sizeof(TsfAP));
  g_tsf.aps[idx].active = true;
  memcpy(g_tsf.aps[idx].bssid, bssid, 6);
  g_tsf.aps[idx].state = TSF_STATE_INIT;
  g_tsf.aps[idx].drift_ok = true;
  g_tsf.aps[idx].first_seen_ts = millis() / 1000;
  return &g_tsf.aps[idx];
}

static void tsf_format_uptime(TsfAP &a) {
  uint32_t s = a.estimated_uptime_s;
  a.estimated_days = s / 86400;
  a.estimated_hours = (s % 86400) / 3600;
  a.estimated_mins = (s % 3600) / 60;
  a.estimated_secs = s % 60;
}

static void tsf_update_drift_avg(TsfAP &a, int32_t ppm) {
  a.drift_ppm_hist[a.drift_hist_head % TSF_HIST_LEN] = ppm;
  a.drift_hist_head = (a.drift_hist_head + 1) % TSF_HIST_LEN;

  int32_t sum = 0;
  int cnt = 0;
  for (int i = 0; i < TSF_HIST_LEN; i++) {
    if (a.drift_ppm_hist[i] != 0) {
      sum += a.drift_ppm_hist[i];
      cnt++;
    }
  }
  if (cnt > 0)
    a.drift_ppm_avg = sum / cnt;

  int32_t abs_ppm = ppm < 0 ? -ppm : ppm;
  if (abs_ppm > a.drift_ppm_max)
    a.drift_ppm_max = abs_ppm;
  a.drift_ok = (abs_ppm <= 100);
}

void tsf_ingest(const uint8_t *bssid, const char *ssid, uint8_t ch, int8_t rssi, uint64_t tsf,
                uint16_t beacon_interval) {
  TsfAP *a = tsf_find(bssid);
  if (!a)
    a = tsf_alloc(bssid);
  if (!a)
    return;

  if (ssid)
    strncpy(a->ssid, ssid, 32);
  a->ch = ch;
  a->rssi = rssi;
  if (beacon_interval && a->nominal_interval_tu == 0) {
    a->nominal_interval_tu = beacon_interval;
    a->nominal_interval_us = (uint32_t)beacon_interval * 1024;
  }

  uint32_t now_ms = millis();

  if (a->state == TSF_STATE_INIT) {
    // First beacon: record as baseline
    a->first_tsf = tsf;
    a->last_tsf = tsf;
    a->local_ms_at_tsf = now_ms;
    a->beacon_count = 1;
    a->state = TSF_STATE_TRACKING;
  } else {
    // Subsequent beacons: compute drift
    uint64_t tsf_delta_us = tsf - a->last_tsf;                      // change in TSF counter
    uint32_t wall_delta_us = (now_ms - a->local_ms_at_tsf) * 1000;  // local time delta

    if (wall_delta_us > 0 && tsf_delta_us > 0) {
      // drift ppm = (tsf_delta - wall_delta) / wall_delta * 1e6
      int64_t diff = (int64_t)tsf_delta_us - (int64_t)wall_delta_us;
      int32_t ppm = (int32_t)(diff * 1000000 / (int64_t)wall_delta_us);
      tsf_update_drift_avg(*a, ppm);

      // Check for TSF jump (unexpected discontinuity)
      if (a->nominal_interval_us > 0) {
        uint64_t expected_delta = a->nominal_interval_us;
        uint64_t ratio = tsf_delta_us / expected_delta;
        if (ratio > 2) {
          // TSF jumped more than 2 expected intervals
          a->jump_events++;
          a->missed_beacons += (uint16_t)(ratio - 1);
          if (a->state != TSF_STATE_JUMP)
            a->state = TSF_STATE_JUMP;
        } else if (!a->drift_ok) {
          a->state = TSF_STATE_SKEWED;
        } else {
          a->state = TSF_STATE_TRACKING;
        }
      }
    }

    // TSF-based uptime: TSF counts µs since AP boot
    a->tsf_uptime_us = tsf;
    a->estimated_uptime_s = (uint32_t)(tsf / 1000000);
    tsf_format_uptime(*a);

    // Rollover check: > 2^63 µs ≈ 292000 years, practically never
    if (tsf > 0xFFFFFFFF00000000ULL)
      a->state = TSF_STATE_ROLLOVER;

    a->last_tsf = tsf;
    a->local_ms_at_tsf = now_ms;
    a->beacon_count++;
  }

  a->last_seen_ts = millis() / 1000;
}

static void tsf_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String tsf_json_all() {
  char mac[18];
  String s = "{\"count\":" + String(g_tsf.count);
  s += ",\"aps\":[";
  bool first = true;
  for (int i = 0; i < TSF_MAX_APS; i++) {
    TsfAP &a = g_tsf.aps[i];
    if (!a.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    tsf_mac_str(a.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(a.ssid) + "\"";
    s += ",\"ch\":" + String(a.ch);
    s += ",\"rssi\":" + String(a.rssi);
    s += ",\"state\":\"" + String(tsf_state_str(a.state)) + "\"";
    s += ",\"uptime_s\":" + String(a.estimated_uptime_s);
    s += ",\"days\":" + String(a.estimated_days);
    s += ",\"hours\":" + String(a.estimated_hours);
    s += ",\"mins\":" + String(a.estimated_mins);
    s += ",\"secs\":" + String(a.estimated_secs);
    s += ",\"drift_ppm\":" + String(a.drift_ppm_avg);
    s += ",\"drift_max\":" + String(a.drift_ppm_max);
    s += ",\"drift_ok\":" + String(a.drift_ok ? "true" : "false");
    s += ",\"beacons\":" + String(a.beacon_count);
    s += ",\"missed\":" + String(a.missed_beacons);
    s += ",\"jumps\":" + String(a.jump_events) + "}";
  }
  s += "]}";
  return s;
}

inline void tsf_routes() {
  server.on("/api/tsf/all", HTTP_GET,
            []() { server.send(200, "application/json", tsf_json_all()); });
  server.on("/api/tsf/clear", HTTP_POST, []() {
    memset(&g_tsf, 0, sizeof(g_tsf));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

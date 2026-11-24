#pragma once
// timing_attack_detector.h — 802.11 timing anomaly and attack detector.
// Detects suspicious timing patterns: beacon jitter, auth sequence timing,
// fast probe sweep (scanners), coordinated deauth timing, and TSF drift.
// Prefix: tad_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define TAD_MAX_APS       32
#define TAD_MAX_ANOMALIES 48
#define TAD_HIST_LEN      20

// Timing anomaly types
enum TadAnomalyType : uint8_t {
  TAD_ANOM_NONE         = 0,
  TAD_ANOM_BEACON_JITTER= 1,   // Beacon interval variation > 10% of nominal
  TAD_ANOM_TSF_JUMP     = 2,   // TSF counter jumped unexpectedly
  TAD_ANOM_FAST_PROBE   = 3,   // Probe sweep: < 50ms between channels
  TAD_ANOM_AUTH_FLOOD   = 4,   // > 20 auth frames in 1 second
  TAD_ANOM_DEAUTH_BURST = 5,   // Multiple deauths from same source < 100ms
  TAD_ANOM_FRAME_GAP    = 6,   // Frame gap anomaly (unexpected silence)
  TAD_ANOM_CLOCK_SKEW   = 7,   // TSF-based clock skew > 500ppm
};

static const char* tad_anom_str(TadAnomalyType t) {
  switch (t) {
    case TAD_ANOM_BEACON_JITTER: return "BeaconJitter";
    case TAD_ANOM_TSF_JUMP:      return "TSFJump";
    case TAD_ANOM_FAST_PROBE:    return "FastProbe";
    case TAD_ANOM_AUTH_FLOOD:    return "AuthFlood";
    case TAD_ANOM_DEAUTH_BURST:  return "DeauthBurst";
    case TAD_ANOM_FRAME_GAP:     return "FrameGap";
    case TAD_ANOM_CLOCK_SKEW:    return "ClockSkew";
    default: return "Unknown";
  }
}

struct TadAnomalyEvent {
  TadAnomalyType type;
  uint8_t  source_mac[6];
  uint8_t  ch;
  int32_t  delta_ms;         // timing delta that triggered detection
  uint32_t ts;
};

struct TadBeaconTrack {
  bool    active;
  uint8_t bssid[6];
  char    ssid[33];
  uint8_t ch;

  // Beacon timing
  uint32_t last_beacon_ts_ms;
  uint16_t nominal_interval_tu;    // TUs (1 TU = 1024µs)
  uint32_t nominal_interval_ms;    // in ms
  int32_t  last_jitter_ms;
  int32_t  max_jitter_ms;
  uint32_t jitter_events;

  // TSF tracking
  uint64_t last_tsf;
  int32_t  tsf_drift_ppm;    // parts per million clock skew
  uint32_t tsf_jump_events;

  // Auth flood tracking
  uint32_t auth_count_1s;
  uint32_t auth_window_start;
  uint32_t auth_flood_events;

  // Deauth burst tracking
  uint32_t last_deauth_ms;
  uint32_t deauth_in_burst;
  uint32_t deauth_burst_events;

  // Per-second beacon count
  uint8_t  beacons_1s;
  uint32_t beacons_window_start;

  // History of jitter (abs ms)
  uint8_t  jitter_hist[TAD_HIST_LEN];
  uint8_t  hist_head;

  uint32_t first_ts;
  uint32_t last_ts;
};

static struct {
  TadBeaconTrack  aps[TAD_MAX_APS];
  uint8_t         ap_count;
  TadAnomalyEvent anomalies[TAD_MAX_ANOMALIES];
  uint8_t         anom_head;
  uint8_t         anom_count;

  // Global probe sweep tracking
  struct {
    uint8_t  mac[6];
    uint32_t last_ms;
    uint8_t  channels_hit;
    uint16_t sweep_count;
  } probe_sweep;

  uint32_t total_anomalies;
} g_tad = {};

static bool tad_mac_eq(const uint8_t *a, const uint8_t *b) { return memcmp(a,b,6)==0; }

static void tad_push_anom(TadAnomalyType type, const uint8_t *mac, uint8_t ch, int32_t delta) {
  TadAnomalyEvent &e = g_tad.anomalies[g_tad.anom_head % TAD_MAX_ANOMALIES];
  memset(&e, 0, sizeof(e));
  e.type = type;
  if (mac) memcpy(e.source_mac, mac, 6);
  e.ch = ch;
  e.delta_ms = delta;
  e.ts = millis()/1000;
  g_tad.anom_head = (g_tad.anom_head + 1) % TAD_MAX_ANOMALIES;
  if (g_tad.anom_count < TAD_MAX_ANOMALIES) g_tad.anom_count++;
  g_tad.total_anomalies++;
}

static TadBeaconTrack* tad_find_ap(const uint8_t *bssid) {
  for (int i = 0; i < TAD_MAX_APS; i++)
    if (g_tad.aps[i].active && tad_mac_eq(g_tad.aps[i].bssid, bssid))
      return &g_tad.aps[i];
  return nullptr;
}

static TadBeaconTrack* tad_alloc_ap(const uint8_t *bssid) {
  for (int i = 0; i < TAD_MAX_APS; i++) {
    if (!g_tad.aps[i].active) {
      TadBeaconTrack &a = g_tad.aps[i];
      memset(&a, 0, sizeof(a));
      a.active = true;
      memcpy(a.bssid, bssid, 6);
      a.first_ts = millis()/1000;
      g_tad.ap_count++;
      return &a;
    }
  }
  uint32_t oldest = UINT32_MAX; int idx = 0;
  for (int i = 0; i < TAD_MAX_APS; i++)
    if (g_tad.aps[i].last_ts < oldest) { oldest = g_tad.aps[i].last_ts; idx = i; }
  memset(&g_tad.aps[idx], 0, sizeof(TadBeaconTrack));
  g_tad.aps[idx].active = true;
  memcpy(g_tad.aps[idx].bssid, bssid, 6);
  g_tad.aps[idx].first_ts = millis()/1000;
  return &g_tad.aps[idx];
}

void tad_ingest_beacon(const uint8_t *bssid, const char *ssid, uint8_t ch,
                        uint16_t beacon_interval, uint64_t tsf) {
  TadBeaconTrack *a = tad_find_ap(bssid);
  if (!a) a = tad_alloc_ap(bssid);
  if (!a) return;

  if (ssid) strncpy(a->ssid, ssid, 32);
  a->ch = ch;

  uint32_t now_ms = millis();

  // Set nominal interval from first few beacons
  if (a->nominal_interval_tu == 0 && beacon_interval > 0) {
    a->nominal_interval_tu = beacon_interval;
    a->nominal_interval_ms = (uint32_t)beacon_interval * 1024 / 1000; // TU to ms
  }

  // Check beacon jitter
  if (a->last_beacon_ts_ms > 0 && a->nominal_interval_ms > 0) {
    int32_t actual_gap = (int32_t)(now_ms - a->last_beacon_ts_ms);
    int32_t jitter = actual_gap - (int32_t)a->nominal_interval_ms;
    if (jitter < 0) jitter = -jitter;

    a->last_jitter_ms = jitter;
    if (jitter > a->max_jitter_ms) a->max_jitter_ms = jitter;

    // 10% threshold
    uint32_t thresh = a->nominal_interval_ms / 10;
    if (jitter > (int32_t)thresh && thresh > 5) {
      a->jitter_events++;
      tad_push_anom(TAD_ANOM_BEACON_JITTER, bssid, ch, jitter);
    }

    // Store in history (capped to 255ms)
    uint8_t hist_val = (jitter > 255) ? 255 : (uint8_t)jitter;
    a->jitter_hist[a->hist_head % TAD_HIST_LEN] = hist_val;
    a->hist_head = (a->hist_head + 1) % TAD_HIST_LEN;
  }

  // TSF drift detection
  if (a->last_tsf > 0 && tsf > 0 && a->last_beacon_ts_ms > 0) {
    uint64_t tsf_delta_us = tsf - a->last_tsf;  // expected: interval * 1024
    uint32_t wall_delta_us = (now_ms - a->last_beacon_ts_ms) * 1000;
    if (wall_delta_us > 0 && tsf_delta_us > 0) {
      // Drift in ppm: (tsf - wall) / wall * 1e6
      int64_t drift = ((int64_t)tsf_delta_us - (int64_t)wall_delta_us) * 1000000 / (int64_t)wall_delta_us;
      if (drift < 0) drift = -drift;
      a->tsf_drift_ppm = (int32_t)(drift > 0x7FFFFFFF ? 0x7FFFFFFF : drift);

      if (a->tsf_drift_ppm > 500) {
        a->tsf_jump_events++;
        tad_push_anom(TAD_ANOM_TSF_JUMP, bssid, ch, a->tsf_drift_ppm);
      }
    }
  }

  a->last_tsf = tsf;
  a->last_beacon_ts_ms = now_ms;
  a->last_ts = now_ms / 1000;
}

void tad_ingest_auth(const uint8_t *ap_bssid, uint8_t ch) {
  TadBeaconTrack *a = tad_find_ap(ap_bssid);
  if (!a) a = tad_alloc_ap(ap_bssid);
  if (!a) return;

  uint32_t now_s = millis()/1000;
  if (now_s != a->auth_window_start) {
    a->auth_count_1s = 0;
    a->auth_window_start = now_s;
  }
  a->auth_count_1s++;
  if (a->auth_count_1s > 20) {
    a->auth_flood_events++;
    tad_push_anom(TAD_ANOM_AUTH_FLOOD, ap_bssid, ch, (int32_t)a->auth_count_1s);
  }
}

void tad_ingest_deauth(const uint8_t *src_mac, uint8_t ch) {
  // Find or create a placeholder for the source
  TadBeaconTrack *a = tad_find_ap(src_mac);
  if (!a) a = tad_alloc_ap(src_mac);
  if (!a) return;

  uint32_t now_ms = millis();
  if (a->last_deauth_ms > 0 && (now_ms - a->last_deauth_ms) < 100) {
    a->deauth_in_burst++;
    if (a->deauth_in_burst >= 3) {
      a->deauth_burst_events++;
      tad_push_anom(TAD_ANOM_DEAUTH_BURST, src_mac, ch, (int32_t)(now_ms - a->last_deauth_ms));
    }
  } else {
    a->deauth_in_burst = 1;
  }
  a->last_deauth_ms = now_ms;
  a->last_ts = now_ms / 1000;
}

void tad_ingest_probe(const uint8_t *src_mac, uint8_t ch) {
  uint32_t now_ms = millis();
  if (tad_mac_eq(g_tad.probe_sweep.mac, src_mac)) {
    uint32_t gap = now_ms - g_tad.probe_sweep.last_ms;
    if (gap < 50) {   // Less than 50ms between probe requests on different channels
      g_tad.probe_sweep.channels_hit++;
      if (g_tad.probe_sweep.channels_hit >= 6) {
        g_tad.probe_sweep.sweep_count++;
        tad_push_anom(TAD_ANOM_FAST_PROBE, src_mac, ch, (int32_t)gap);
        g_tad.probe_sweep.channels_hit = 0;
      }
    } else {
      g_tad.probe_sweep.channels_hit = 1;
    }
  } else {
    memcpy(g_tad.probe_sweep.mac, src_mac, 6);
    g_tad.probe_sweep.channels_hit = 1;
  }
  g_tad.probe_sweep.last_ms = now_ms;
}

static void tad_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String tad_hist_json(const uint8_t *h, int len, int head) {
  String s = "[";
  for (int i = 0; i < len; i++) { if (i) s += ","; s += String(h[(head+i)%len]); }
  return s + "]";
}

static String tad_json_all() {
  char mac[18];
  String s = "{\"ap_count\":" + String(g_tad.ap_count);
  s += ",\"total_anomalies\":" + String(g_tad.total_anomalies);
  s += ",\"probe_sweeps\":" + String(g_tad.probe_sweep.sweep_count);

  s += ",\"anomalies\":[";
  bool first = true;
  for (int i = 0; i < g_tad.anom_count; i++) {
    int idx = (g_tad.anom_head + TAD_MAX_ANOMALIES - i - 1) % TAD_MAX_ANOMALIES;
    TadAnomalyEvent &e = g_tad.anomalies[idx];
    if (!e.ts) continue;
    if (!first) s += ","; first = false;
    tad_mac_str(e.source_mac, mac);
    s += "{\"type\":\"" + String(tad_anom_str(e.type)) + "\"";
    s += ",\"mac\":\"" + String(mac) + "\"";
    s += ",\"ch\":" + String(e.ch);
    s += ",\"delta\":" + String(e.delta_ms);
    s += ",\"ts\":" + String(e.ts) + "}";
  }

  s += "],\"aps\":[";
  first = true;
  for (int i = 0; i < TAD_MAX_APS; i++) {
    TadBeaconTrack &a = g_tad.aps[i];
    if (!a.active) continue;
    if (!first) s += ","; first = false;
    tad_mac_str(a.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(a.ssid) + "\"";
    s += ",\"ch\":" + String(a.ch);
    s += ",\"interval_ms\":" + String(a.nominal_interval_ms);
    s += ",\"last_jitter\":" + String(a.last_jitter_ms);
    s += ",\"max_jitter\":" + String(a.max_jitter_ms);
    s += ",\"jitter_events\":" + String(a.jitter_events);
    s += ",\"tsf_drift_ppm\":" + String(a.tsf_drift_ppm);
    s += ",\"tsf_jumps\":" + String(a.tsf_jump_events);
    s += ",\"auth_flood\":" + String(a.auth_flood_events);
    s += ",\"deauth_burst\":" + String(a.deauth_burst_events);
    s += ",\"jitter_hist\":" + tad_hist_json(a.jitter_hist, TAD_HIST_LEN, a.hist_head) + "}";
  }
  s += "]}";
  return s;
}

inline void tad_routes() {
  server.on("/api/tad/all", HTTP_GET, []() {
    server.send(200, "application/json", tad_json_all());
  });
  server.on("/api/tad/clear", HTTP_POST, []() {
    memset(&g_tad, 0, sizeof(g_tad));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

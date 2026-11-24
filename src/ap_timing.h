#pragma once
// ap_timing.h — AP beacon interval and timing analysis.
// Measures actual beacon intervals and detects timing anomalies (jitter, drift, gaps).
// Prefix: apt_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define APT_MAX_APS       64
#define APT_HIST_LEN      60   // 60-sample interval history
#define APT_MIN_SAMPLES   5    // need at least this many before stats are meaningful

struct AptAP {
  bool    active;
  uint8_t bssid[6];
  char    ssid[33];
  uint8_t channel;
  int8_t  rssi;

  uint32_t last_ts_ms;      // millis() of last beacon
  uint32_t first_ts;        // seconds
  uint32_t last_ts;         // seconds
  uint32_t total_beacons;

  uint16_t nominal_interval_ms;  // from beacon IE (in TUs * 1024/1000)

  // Rolling interval measurements
  uint16_t intervals[APT_HIST_LEN];  // last N inter-beacon intervals in ms
  uint8_t  int_head;
  uint8_t  int_count;

  // Computed stats (updated every 5 beacons)
  uint32_t avg_interval_ms;
  uint32_t min_interval_ms;
  uint32_t max_interval_ms;
  uint32_t jitter_ms;         // standard deviation of intervals
  int32_t  drift_ppm;         // parts-per-million vs nominal

  uint32_t missed_beacons;    // estimated from gaps > 2x nominal
  uint32_t anomaly_count;     // intervals deviating > 20% from nominal
};

static struct {
  AptAP   aps[APT_MAX_APS];
  uint8_t ap_count;
  uint32_t total_beacons_seen;
} g_apt = {};

static bool apt_mac_eq(const uint8_t *a, const uint8_t *b) { return memcmp(a,b,6)==0; }

static AptAP* apt_find(const uint8_t *bssid) {
  for (int i = 0; i < APT_MAX_APS; i++)
    if (g_apt.aps[i].active && apt_mac_eq(g_apt.aps[i].bssid, bssid))
      return &g_apt.aps[i];
  return nullptr;
}

static AptAP* apt_alloc(const uint8_t *bssid) {
  for (int i = 0; i < APT_MAX_APS; i++) {
    if (!g_apt.aps[i].active) {
      AptAP &a = g_apt.aps[i];
      memset(&a, 0, sizeof(a));
      a.active = true;
      memcpy(a.bssid, bssid, 6);
      a.min_interval_ms = UINT32_MAX;
      a.first_ts = millis()/1000;
      g_apt.ap_count++;
      return &a;
    }
  }
  uint32_t oldest = UINT32_MAX; int idx = 0;
  for (int i = 0; i < APT_MAX_APS; i++)
    if (g_apt.aps[i].last_ts < oldest) { oldest = g_apt.aps[i].last_ts; idx = i; }
  memset(&g_apt.aps[idx], 0, sizeof(g_apt.aps[0]));
  g_apt.aps[idx].active = true;
  memcpy(g_apt.aps[idx].bssid, bssid, 6);
  g_apt.aps[idx].min_interval_ms = UINT32_MAX;
  g_apt.aps[idx].first_ts = millis()/1000;
  return &g_apt.aps[idx];
}

static void apt_compute_stats(AptAP &a) {
  uint8_t cnt = a.int_count;
  if (cnt < APT_MIN_SAMPLES) return;

  uint32_t sum = 0, mn = UINT32_MAX, mx = 0;
  for (int i = 0; i < cnt; i++) {
    uint16_t v = a.intervals[i];
    sum += v;
    if (v < mn) mn = v;
    if (v > mx) mx = v;
  }
  a.avg_interval_ms = sum / cnt;
  a.min_interval_ms = mn;
  a.max_interval_ms = mx;

  // Jitter = max deviation from mean
  uint32_t jit = 0;
  for (int i = 0; i < cnt; i++) {
    uint32_t dev = abs((int)a.intervals[i] - (int)a.avg_interval_ms);
    if (dev > jit) jit = dev;
  }
  a.jitter_ms = jit;

  // Drift in PPM vs nominal
  if (a.nominal_interval_ms > 0 && a.avg_interval_ms > 0) {
    int32_t diff = (int32_t)a.avg_interval_ms - (int32_t)a.nominal_interval_ms;
    a.drift_ppm = (diff * 1000000L) / (int32_t)a.nominal_interval_ms;
  }
}

void apt_ingest_beacon(const uint8_t *bssid, const char *ssid,
                       uint8_t ch, int8_t rssi, uint16_t beacon_interval_tu) {
  uint32_t now_ms = millis();
  g_apt.total_beacons_seen++;

  AptAP *a = apt_find(bssid);
  if (!a) a = apt_alloc(bssid);
  if (!a) return;

  if (ssid && ssid[0]) strncpy(a->ssid, ssid, 32);
  a->channel = ch;
  if (rssi < 0) a->rssi = rssi;
  a->last_ts = now_ms / 1000;

  if (beacon_interval_tu > 0) {
    // Convert TUs (1024 µs) to ms
    a->nominal_interval_ms = (uint32_t)beacon_interval_tu * 1024 / 1000;
  }

  if (a->last_ts_ms > 0 && now_ms > a->last_ts_ms) {
    uint32_t iv = now_ms - a->last_ts_ms;
    // Check for missed beacons
    if (a->nominal_interval_ms > 0 && iv > a->nominal_interval_ms * 2) {
      a->missed_beacons += (iv / a->nominal_interval_ms) - 1;
    }
    // Check anomaly
    if (a->nominal_interval_ms > 0) {
      uint32_t dev_pct = abs((int)iv - (int)a->nominal_interval_ms) * 100 / a->nominal_interval_ms;
      if (dev_pct > 20) a->anomaly_count++;
    }
    if (iv < 5000) {  // discard huge gaps from channel hops
      a->intervals[a->int_head % APT_HIST_LEN] = (uint16_t)min(iv, (uint32_t)65535);
      a->int_head = (a->int_head + 1) % APT_HIST_LEN;
      if (a->int_count < APT_HIST_LEN) a->int_count++;
    }
    if (a->total_beacons % 5 == 0) apt_compute_stats(*a);
  }

  a->last_ts_ms = now_ms;
  a->total_beacons++;
}

static void apt_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String apt_hist_json(const AptAP &a) {
  String s = "[";
  uint8_t cnt = a.int_count;
  uint8_t head = a.int_head;
  for (int i = 0; i < cnt; i++) {
    int idx = ((int)head - cnt + i + APT_HIST_LEN) % APT_HIST_LEN;
    if (i) s += ",";
    s += String(a.intervals[idx]);
  }
  return s + "]";
}

static String apt_json_all() {
  String s = "{\"ap_count\":" + String(g_apt.ap_count);
  s += ",\"total_beacons\":" + String(g_apt.total_beacons_seen);
  s += ",\"aps\":[";
  char mac[18]; bool first = true;
  for (int i = 0; i < APT_MAX_APS; i++) {
    AptAP &a = g_apt.aps[i];
    if (!a.active) continue;
    if (!first) s += ","; first = false;
    apt_mac_str(a.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(a.ssid) + "\"";
    s += ",\"ch\":" + String(a.channel);
    s += ",\"rssi\":" + String(a.rssi);
    s += ",\"nominal_ms\":" + String(a.nominal_interval_ms);
    s += ",\"avg_ms\":" + String(a.avg_interval_ms);
    s += ",\"min_ms\":" + String(a.min_interval_ms == UINT32_MAX ? 0 : a.min_interval_ms);
    s += ",\"max_ms\":" + String(a.max_interval_ms);
    s += ",\"jitter_ms\":" + String(a.jitter_ms);
    s += ",\"drift_ppm\":" + String(a.drift_ppm);
    s += ",\"total\":" + String(a.total_beacons);
    s += ",\"missed\":" + String(a.missed_beacons);
    s += ",\"anomalies\":" + String(a.anomaly_count);
    s += ",\"samples\":" + String(a.int_count);
    s += ",\"hist\":" + apt_hist_json(a);
    s += "}";
  }
  s += "]}";
  return s;
}

inline void apt_routes() {
  server.on("/api/apt/all", HTTP_GET, []() {
    server.send(200, "application/json", apt_json_all());
  });
  server.on("/api/apt/clear", HTTP_POST, []() {
    memset(&g_apt, 0, sizeof(g_apt));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

#pragma once
// beacon_rate.h — Per-AP beacon rate tracker with anomaly detection.
// Flags APs with abnormal beacon intervals or burst transmissions. Prefix: bcr_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define BCR_MAX_APS 64
#define BCR_HIST_LEN 30  // 30-second history
#define BCR_WINDOW_S 5   // analysis window

// ─── AP beacon record ─────────────────────────────────────────────────────────
struct BcrAP {
  bool active;
  uint8_t bssid[6];
  char ssid[33];
  uint8_t channel;

  // Beacon rate history (beacons per second, 30s)
  uint8_t rate_hist[BCR_HIST_LEN];
  uint8_t hist_idx;
  uint32_t last_hist_ts;

  // Counts
  uint32_t total_beacons;
  uint32_t window_beacons;
  uint32_t window_start;

  // Stats
  float avg_rate;        // beacons/sec average
  float expected_rate;   // from beacon interval IE
  uint16_t interval_tu;  // advertised interval (TUs)
  int8_t rssi;

  // Anomaly
  bool rate_anomaly;    // actual rate differs >50% from expected
  bool burst_detected;  // >20 beacons in 1s
  bool rate_zero;       // no beacons in last 10s (AP left?)
  uint32_t last_beacon_ts;
  uint32_t anomaly_since;
};

// ─── State ────────────────────────────────────────────────────────────────────
static struct {
  BcrAP aps[BCR_MAX_APS];
  uint32_t count;
  uint32_t total_beacons;
  uint32_t anomalies;
  uint32_t last_tick_ts;
} g_bcr = {};

// ─── Helpers ─────────────────────────────────────────────────────────────────
static bool bcr_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static BcrAP *bcr_find(const uint8_t *bssid) {
  for (int i = 0; i < BCR_MAX_APS; i++) {
    if (g_bcr.aps[i].active && bcr_mac_eq(g_bcr.aps[i].bssid, bssid))
      return &g_bcr.aps[i];
  }
  return nullptr;
}

static BcrAP *bcr_alloc(const uint8_t *bssid) {
  for (int i = 0; i < BCR_MAX_APS; i++) {
    if (!g_bcr.aps[i].active) {
      memset(&g_bcr.aps[i], 0, sizeof(g_bcr.aps[i]));
      memcpy(g_bcr.aps[i].bssid, bssid, 6);
      g_bcr.aps[i].active = true;
      g_bcr.count++;
      return &g_bcr.aps[i];
    }
  }
  return nullptr;
}

// ─── Ingest beacon ───────────────────────────────────────────────────────────
void bcr_ingest(const uint8_t *bssid, const char *ssid, int8_t rssi, uint8_t channel,
                uint16_t interval_tu) {
  if (!bssid)
    return;

  BcrAP *ap = bcr_find(bssid);
  if (!ap) {
    ap = bcr_alloc(bssid);
    if (!ap)
      return;
  }

  uint32_t now = millis() / 1000;
  if (ssid && ssid[0])
    strncpy(ap->ssid, ssid, 32);
  ap->channel = channel;
  ap->rssi = rssi;
  if (interval_tu > 0)
    ap->interval_tu = interval_tu;
  ap->total_beacons++;
  ap->last_beacon_ts = now;
  g_bcr.total_beacons++;

  // Window counting
  if (now - ap->window_start >= BCR_WINDOW_S) {
    float window_rate = (float)ap->window_beacons / BCR_WINDOW_S;
    ap->avg_rate = window_rate;

    // Expected rate = 1024000 / (interval_tu * 1000) ≈ 1024/interval per sec
    if (ap->interval_tu > 0) {
      ap->expected_rate = 1024.0f / ap->interval_tu;
    } else {
      ap->expected_rate = 10.0f;  // default 100ms
    }

    // Rate anomaly check
    if (ap->expected_rate > 0.1f) {
      float ratio = window_rate / ap->expected_rate;
      bool was_anom = ap->rate_anomaly;
      ap->rate_anomaly = (ratio > 1.5f || ratio < 0.5f) && ap->window_beacons > 2;
      if (!was_anom && ap->rate_anomaly) {
        ap->anomaly_since = now;
        g_bcr.anomalies++;
      }
    }

    // Burst: >20 in one second (compare to window average)
    ap->burst_detected = (ap->window_beacons > (uint32_t)(BCR_WINDOW_S * 20));

    ap->window_beacons = 0;
    ap->window_start = now;
  }
  ap->window_beacons++;

  // History (1-second buckets)
  if (now != ap->last_hist_ts) {
    ap->last_hist_ts = now;
    ap->rate_hist[ap->hist_idx % BCR_HIST_LEN] = (uint8_t)ap->window_beacons;
    ap->hist_idx = (ap->hist_idx + 1) % BCR_HIST_LEN;
  }
}

// ─── Tick — mark absent APs ───────────────────────────────────────────────────
void bcr_tick() {
  uint32_t now = millis() / 1000;
  if (now == g_bcr.last_tick_ts)
    return;
  g_bcr.last_tick_ts = now;

  for (int i = 0; i < BCR_MAX_APS; i++) {
    BcrAP &ap = g_bcr.aps[i];
    if (!ap.active)
      continue;
    ap.rate_zero = (now - ap.last_beacon_ts > 10);
  }
}

// ─── JSON ─────────────────────────────────────────────────────────────────────
static void bcr_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String bcr_json_all() {
  String s = "{\"count\":" + String(g_bcr.count);
  s += ",\"total_beacons\":" + String(g_bcr.total_beacons);
  s += ",\"anomalies\":" + String(g_bcr.anomalies);
  s += ",\"aps\":[";
  char mac[18];
  bool first = true;
  for (int i = 0; i < BCR_MAX_APS; i++) {
    BcrAP &ap = g_bcr.aps[i];
    if (!ap.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    bcr_mac_str(ap.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(ap.ssid) + "\"";
    s += ",\"ch\":" + String(ap.channel);
    s += ",\"rssi\":" + String(ap.rssi);
    s += ",\"avg_rate\":" + String(ap.avg_rate, 2);
    s += ",\"expected_rate\":" + String(ap.expected_rate, 2);
    s += ",\"interval_tu\":" + String(ap.interval_tu);
    s += ",\"total_beacons\":" + String(ap.total_beacons);
    s += ",\"rate_anomaly\":" + String(ap.rate_anomaly ? "true" : "false");
    s += ",\"burst\":" + String(ap.burst_detected ? "true" : "false");
    s += ",\"gone\":" + String(ap.rate_zero ? "true" : "false") + "}";
  }
  s += "]}";
  return s;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void bcr_routes() {
  server.on("/api/bcr/all", HTTP_GET,
            []() { server.send(200, "application/json", bcr_json_all()); });

  server.on("/api/bcr/anomalies", HTTP_GET, []() {
    String s = "[";
    char mac[18];
    bool first = true;
    for (int i = 0; i < BCR_MAX_APS; i++) {
      BcrAP &ap = g_bcr.aps[i];
      if (!ap.active || (!ap.rate_anomaly && !ap.burst_detected))
        continue;
      if (!first)
        s += ",";
      first = false;
      bcr_mac_str(ap.bssid, mac);
      s += "{\"bssid\":\"" + String(mac) + "\"";
      s += ",\"ssid\":\"" + String(ap.ssid) + "\"";
      s += ",\"rate_anomaly\":" + String(ap.rate_anomaly ? "true" : "false");
      s += ",\"burst\":" + String(ap.burst_detected ? "true" : "false");
      s += ",\"avg_rate\":" + String(ap.avg_rate, 2);
      s += ",\"expected_rate\":" + String(ap.expected_rate, 2) + "}";
    }
    s += "]";
    server.send(200, "application/json", s);
  });

  server.on("/api/bcr/clear", HTTP_POST, []() {
    memset(&g_bcr, 0, sizeof(g_bcr));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

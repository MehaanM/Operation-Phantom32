#pragma once
// rssi_trilateration.h — RSSI-based device localization and trilateration.
// Uses multiple known AP positions + RSSI readings to estimate device location.
// Also provides RSSI path-loss model, floor-plan annotation, and tracking.
// Prefix: tri_

#include <Arduino.h>
#include <WebServer.h>
#include <math.h>

extern WebServer server;

#define TRI_MAX_ANCHORS 8    // known AP positions
#define TRI_MAX_TARGETS 16   // devices being tracked
#define TRI_HISTORY 30       // position history per target
#define TRI_MAX_READINGS 32  // RSSI readings per estimate

// ─── Path-loss model ──────────────────────────────────────────────────────────
// FSPL: d = 10^((TxPower - RSSI - 20*log10(f_MHz) + 27.55) / 20)
// Two-ray: d^4 model for indoor environments
#define TRI_FREQ_MHZ_24 2437.0f  // CH6 center
#define TRI_FREQ_MHZ_50 5180.0f  // CH36 center
#define TRI_PATH_LOSS_EXP 2.7f   // indoor path loss exponent

// ─── Structs ──────────────────────────────────────────────────────────────────
struct TriAnchor {
  bool active;
  uint8_t bssid[6];
  char ssid[33];
  float x;  // meters from origin
  float y;
  float z;          // floor height (optional)
  int8_t tx_power;  // dBm (default -20)
  float freq_mhz;
  uint32_t last_rssi_ts;
  int8_t last_rssi;
  int8_t rssi_history[16];
  uint8_t rssi_hist_idx;
};

struct TriPosition {
  float x;
  float y;
  float z;
  float accuracy_m;  // estimated accuracy in meters
  uint32_t ts;
};

struct TriTarget {
  bool active;
  uint8_t mac[6];
  char name[32];
  TriPosition pos;
  TriPosition history[TRI_HISTORY];
  uint8_t hist_idx;
  uint32_t fix_count;
  int8_t rssi_to_anchors[TRI_MAX_ANCHORS];
  float dist_to_anchors[TRI_MAX_ANCHORS];
  float dx;  // velocity m/s X
  float dy;  // velocity m/s Y
  uint32_t last_fix_ts;
};

static struct {
  TriAnchor anchors[TRI_MAX_ANCHORS];
  uint8_t anchor_count;
  TriTarget targets[TRI_MAX_TARGETS];
  uint32_t total_fixes;
  float path_loss_exp;
  int8_t default_tx_power;
} g_tri = {};

// ─── RSSI to distance ─────────────────────────────────────────────────────────
static float tri_rssi_to_dist(int8_t rssi, int8_t tx_power, float freq_mhz, float n) {
  // Free-space + indoor: d = 10^((TxPower - RSSI - 20*log10(f) - 20*log10(4π/c)) / (10*n))
  // Simplified: d = 10^((TxPower - RSSI - 20*log10(f_MHz) + 27.55) / 20) for n=2
  float numerator = (float)tx_power - (float)rssi - 20.0f * log10f(freq_mhz) + 27.55f;
  if (n == 2.0f) {
    return powf(10.0f, numerator / 20.0f);
  }
  // For non-2 path-loss exponent, use: d = 10^((TxPower - RSSI) / (10*n)) * d_ref
  // where d_ref = 1m reference distance
  float d_ref_exponent = numerator / 20.0f;  // at n=2
  float d_ref = powf(10.0f, d_ref_exponent);
  return d_ref * powf(10.0f, (d_ref_exponent - (float)(rssi - tx_power) / (10.0f * n)));
}

// Average recent RSSI readings for an anchor
static int8_t tri_anchor_avg_rssi(const TriAnchor &a) {
  if (a.rssi_hist_idx == 0)
    return a.last_rssi;
  int sum = 0;
  int cnt = min((int)a.rssi_hist_idx, 16);
  for (int i = 0; i < cnt; i++)
    sum += a.rssi_history[i];
  return (int8_t)(sum / cnt);
}

// ─── Weighted centroid trilateration ─────────────────────────────────────────
// Uses N anchor positions + distances to compute estimated position
static bool tri_compute(TriTarget &t) {
  float xs[TRI_MAX_ANCHORS], ys[TRI_MAX_ANCHORS], ws[TRI_MAX_ANCHORS];
  int valid = 0;

  for (int i = 0; i < g_tri.anchor_count; i++) {
    TriAnchor &a = g_tri.anchors[i];
    if (!a.active)
      continue;
    if (t.rssi_to_anchors[i] == 0)
      continue;  // no reading

    float d = tri_rssi_to_dist(t.rssi_to_anchors[i], a.tx_power, a.freq_mhz, g_tri.path_loss_exp);
    if (d < 0.1f)
      d = 0.1f;
    if (d > 300.0f)
      continue;  // too far, unreliable

    t.dist_to_anchors[i] = d;
    xs[valid] = a.x;
    ys[valid] = a.y;
    ws[valid] = 1.0f / (d * d);  // weight = 1/d^2
    valid++;
  }

  if (valid < 1)
    return false;

  // Weighted centroid
  float sum_w = 0;
  float wx = 0, wy = 0;
  for (int i = 0; i < valid; i++) {
    sum_w += ws[i];
    wx += ws[i] * xs[i];
    wy += ws[i] * ys[i];
  }
  if (sum_w == 0)
    return false;

  float est_x = wx / sum_w;
  float est_y = wy / sum_w;

  // Accuracy estimate: average residual
  float residual = 0;
  for (int i = 0; i < valid; i++) {
    float dx = est_x - xs[i];
    float dy = est_y - ys[i];
    float dist_to_anchor = sqrtf(dx * dx + dy * dy);
    residual += fabsf(dist_to_anchor - t.dist_to_anchors[i]);
  }
  float accuracy = valid > 0 ? residual / valid : 99.0f;

  // Clamp accuracy
  if (accuracy > 50.0f)
    accuracy = 50.0f;

  // Update position
  TriPosition prev = t.pos;
  t.pos.x = est_x;
  t.pos.y = est_y;
  t.pos.z = 0;
  t.pos.accuracy_m = accuracy;
  t.pos.ts = millis() / 1000;

  // Store history
  t.history[t.hist_idx % TRI_HISTORY] = t.pos;
  t.hist_idx++;

  // Velocity estimate
  if (t.fix_count > 0 && prev.ts > 0) {
    float dt = (float)(t.pos.ts - prev.ts);
    if (dt > 0) {
      t.dx = (t.pos.x - prev.x) / dt;
      t.dy = (t.pos.y - prev.y) / dt;
    }
  }

  t.fix_count++;
  t.last_fix_ts = t.pos.ts;
  g_tri.total_fixes++;
  return true;
}

// ─── RSSI ingestion ───────────────────────────────────────────────────────────
void tri_ingest_rssi(const uint8_t *target_mac, const uint8_t *anchor_bssid, int8_t rssi) {
  // Find or create anchor index
  int anchor_idx = -1;
  for (int i = 0; i < TRI_MAX_ANCHORS; i++) {
    if (g_tri.anchors[i].active && memcmp(g_tri.anchors[i].bssid, anchor_bssid, 6) == 0) {
      anchor_idx = i;
      break;
    }
  }
  if (anchor_idx < 0)
    return;

  // Update anchor RSSI history
  TriAnchor &a = g_tri.anchors[anchor_idx];
  a.last_rssi = rssi;
  a.last_rssi_ts = millis() / 1000;
  a.rssi_history[a.rssi_hist_idx % 16] = rssi;
  a.rssi_hist_idx++;

  // Find or create target
  TriTarget *t = nullptr;
  for (int i = 0; i < TRI_MAX_TARGETS; i++) {
    if (g_tri.targets[i].active && memcmp(g_tri.targets[i].mac, target_mac, 6) == 0) {
      t = &g_tri.targets[i];
      break;
    }
  }
  if (!t) {
    for (int i = 0; i < TRI_MAX_TARGETS; i++) {
      if (!g_tri.targets[i].active) {
        t = &g_tri.targets[i];
        memset(t, 0, sizeof(*t));
        memcpy(t->mac, target_mac, 6);
        t->active = true;
        break;
      }
    }
  }
  if (!t)
    return;

  t->rssi_to_anchors[anchor_idx] = rssi;
  tri_compute(*t);
}

// ─── Add anchor ───────────────────────────────────────────────────────────────
inline void tri_add_anchor(const uint8_t *bssid, const char *ssid, float x, float y, float z = 0,
                           int8_t tx_power = -20, float freq_mhz = TRI_FREQ_MHZ_24) {
  if (g_tri.anchor_count >= TRI_MAX_ANCHORS)
    return;
  TriAnchor &a = g_tri.anchors[g_tri.anchor_count];
  memset(&a, 0, sizeof(a));
  if (bssid)
    memcpy(a.bssid, bssid, 6);
  if (ssid)
    strncpy(a.ssid, ssid, 32);
  a.x = x;
  a.y = y;
  a.z = z;
  a.tx_power = tx_power;
  a.freq_mhz = freq_mhz > 0 ? freq_mhz : TRI_FREQ_MHZ_24;
  a.active = true;
  g_tri.anchor_count++;
}

// ─── JSON ─────────────────────────────────────────────────────────────────────
static void tri_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String tri_json_status() {
  String s = "{\"anchors\":[";
  char mac[18];
  for (int i = 0; i < g_tri.anchor_count; i++) {
    TriAnchor &a = g_tri.anchors[i];
    if (!a.active)
      continue;
    if (i)
      s += ",";
    tri_mac_str(a.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(a.ssid) + "\"";
    s += ",\"x\":" + String(a.x, 1);
    s += ",\"y\":" + String(a.y, 1);
    s += ",\"z\":" + String(a.z, 1);
    s += ",\"tx_power\":" + String(a.tx_power);
    s += ",\"last_rssi\":" + String(a.last_rssi);
    s += "}";
  }
  s += "],\"targets\":[";
  bool first = true;
  for (int i = 0; i < TRI_MAX_TARGETS; i++) {
    TriTarget &t = g_tri.targets[i];
    if (!t.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    tri_mac_str(t.mac, mac);
    s += "{\"mac\":\"" + String(mac) + "\"";
    s += ",\"name\":\"" + String(t.name) + "\"";
    s += ",\"x\":" + String(t.pos.x, 2);
    s += ",\"y\":" + String(t.pos.y, 2);
    s += ",\"accuracy\":" + String(t.pos.accuracy_m, 2);
    s += ",\"ts\":" + String(t.pos.ts);
    s += ",\"fixes\":" + String(t.fix_count);
    s += ",\"vx\":" + String(t.dx, 3);
    s += ",\"vy\":" + String(t.dy, 3);
    s += "}";
  }
  s += "],\"total_fixes\":" + String(g_tri.total_fixes);
  s += ",\"path_loss_exp\":" + String(g_tri.path_loss_exp, 2);
  s += "}";
  return s;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void tri_routes() {
  server.on("/api/tri/status", HTTP_GET,
            []() { server.send(200, "application/json", tri_json_status()); });

  server.on("/api/tri/anchor/add", HTTP_POST, []() {
    if (!server.hasArg("bssid") || !server.hasArg("x") || !server.hasArg("y")) {
      server.send(400, "application/json", "{\"error\":\"missing params\"}");
      return;
    }
    uint8_t mac[6];
    sscanf(server.arg("bssid").c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &mac[0], &mac[1], &mac[2],
           &mac[3], &mac[4], &mac[5]);
    float x = server.arg("x").toFloat();
    float y = server.arg("y").toFloat();
    float z = server.hasArg("z") ? server.arg("z").toFloat() : 0;
    int8_t txp = server.hasArg("tx_power") ? server.arg("tx_power").toInt() : -20;
    const char *ssid = server.hasArg("ssid") ? server.arg("ssid").c_str() : "";
    tri_add_anchor(mac, ssid, x, y, z, txp);
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/tri/anchor/clear", HTTP_POST, []() {
    memset(&g_tri.anchors, 0, sizeof(g_tri.anchors));
    g_tri.anchor_count = 0;
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/tri/targets/clear", HTTP_POST, []() {
    memset(&g_tri.targets, 0, sizeof(g_tri.targets));
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/tri/config", HTTP_POST, []() {
    if (server.hasArg("n"))
      g_tri.path_loss_exp = server.arg("n").toFloat();
    if (server.hasArg("tx_power"))
      g_tri.default_tx_power = server.arg("tx_power").toInt();
    server.send(200, "application/json", "{\"ok\":true}");
  });

  // Distance estimation endpoint
  server.on("/api/tri/distance", HTTP_GET, []() {
    if (!server.hasArg("rssi")) {
      server.send(400, "application/json", "{\"error\":\"missing rssi\"}");
      return;
    }
    int8_t rssi = server.arg("rssi").toInt();
    int8_t txp = server.hasArg("tx_power") ? (int8_t)server.arg("tx_power").toInt() : -20;
    float freq = server.hasArg("freq") ? server.arg("freq").toFloat() : TRI_FREQ_MHZ_24;
    float n = server.hasArg("n") ? server.arg("n").toFloat() : g_tri.path_loss_exp;
    float d = tri_rssi_to_dist(rssi, txp, freq, n);
    String s = "{\"rssi\":" + String(rssi);
    s += ",\"tx_power\":" + String(txp);
    s += ",\"freq_mhz\":" + String(freq, 0);
    s += ",\"n\":" + String(n, 2);
    s += ",\"distance_m\":" + String(d, 2);
    s += "}";
    server.send(200, "application/json", s);
  });
}

#pragma once
// signal_mapper.h — RSSI signal strength mapper with heatmap grid.
// Divides the scan environment into a logical grid, records RSSI per cell,
// generates gradient heatmap data for the web UI. Prefix: sgm_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define SGM_GRID_X        20    // 20 horizontal cells
#define SGM_GRID_Y        15    // 15 vertical cells
#define SGM_MAX_BSSIDS    32
#define SGM_SSID_LEN      33
#define SGM_HISTORY_LEN   10

// ─── Grid cell ────────────────────────────────────────────────────────────────
struct SgmCell {
  int8_t  rssi;           // current RSSI reading
  int8_t  rssi_max;       // max ever seen in this cell
  int8_t  rssi_avg;       // exponential average
  uint32_t sample_count;
  uint32_t last_update_ts;
};

// ─── Per-BSSID tracker ────────────────────────────────────────────────────────
struct SgmBssid {
  bool    active;
  uint8_t bssid[6];
  char    ssid[SGM_SSID_LEN];
  uint8_t channel;
  SgmCell grid[SGM_GRID_X][SGM_GRID_Y];
  int8_t  rssi_history[SGM_HISTORY_LEN];
  uint8_t hist_idx;
  uint32_t total_samples;
  // Position estimate (weighted centroid of grid)
  float    est_x;        // 0.0-1.0 normalized
  float    est_y;
  float    confidence;
  int8_t   peak_rssi;
  uint8_t  peak_x;
  uint8_t  peak_y;
};

// ─── State ────────────────────────────────────────────────────────────────────
static struct {
  SgmBssid bssids[SGM_MAX_BSSIDS];
  uint8_t  count;
  uint8_t  current_x;    // current estimated X position (0–SGM_GRID_X-1)
  uint8_t  current_y;    // current estimated Y position
  bool     position_set;
  uint32_t total_samples;
} g_sgm = {};

// ─── Helpers ─────────────────────────────────────────────────────────────────
static bool sgm_mac_eq(const uint8_t *a, const uint8_t *b) { return memcmp(a,b,6)==0; }

static SgmBssid* sgm_find(const uint8_t *bssid) {
  for (int i = 0; i < SGM_MAX_BSSIDS; i++) {
    if (g_sgm.bssids[i].active && sgm_mac_eq(g_sgm.bssids[i].bssid, bssid))
      return &g_sgm.bssids[i];
  }
  return nullptr;
}

static SgmBssid* sgm_alloc(const uint8_t *bssid, const char *ssid, uint8_t ch) {
  for (int i = 0; i < SGM_MAX_BSSIDS; i++) {
    if (!g_sgm.bssids[i].active) {
      SgmBssid &b = g_sgm.bssids[i];
      memset(&b, 0, sizeof(b));
      memcpy(b.bssid, bssid, 6);
      if (ssid) strncpy(b.ssid, ssid, SGM_SSID_LEN - 1);
      b.channel = ch;
      b.active = true;
      b.peak_rssi = -100;
      // Init grid
      for (int x = 0; x < SGM_GRID_X; x++)
        for (int y = 0; y < SGM_GRID_Y; y++) {
          b.grid[x][y].rssi = -100;
          b.grid[x][y].rssi_max = -100;
          b.grid[x][y].rssi_avg = -100;
        }
      g_sgm.count++;
      return &b;
    }
  }
  return nullptr;
}

// ─── Set surveyor position ────────────────────────────────────────────────────
inline void sgm_set_position(uint8_t x, uint8_t y) {
  if (x >= SGM_GRID_X) x = SGM_GRID_X - 1;
  if (y >= SGM_GRID_Y) y = SGM_GRID_Y - 1;
  g_sgm.current_x = x;
  g_sgm.current_y = y;
  g_sgm.position_set = true;
}

// ─── Ingest RSSI sample ───────────────────────────────────────────────────────
void sgm_ingest(const uint8_t *bssid, const char *ssid,
                int8_t rssi, uint8_t channel) {
  if (!g_sgm.position_set) return;

  SgmBssid *b = sgm_find(bssid);
  if (!b) b = sgm_alloc(bssid, ssid, channel);
  if (!b) return;

  g_sgm.total_samples++;
  b->total_samples++;

  uint8_t x = g_sgm.current_x;
  uint8_t y = g_sgm.current_y;
  SgmCell &cell = b->grid[x][y];

  cell.rssi = rssi;
  if (rssi > cell.rssi_max) cell.rssi_max = rssi;
  if (cell.rssi_avg == -100) cell.rssi_avg = rssi;
  else cell.rssi_avg = (int8_t)((cell.rssi_avg * 7 + rssi) / 8);
  cell.sample_count++;
  cell.last_update_ts = millis() / 1000;

  if (rssi > b->peak_rssi) {
    b->peak_rssi = rssi;
    b->peak_x = x;
    b->peak_y = y;
  }

  // History
  b->rssi_history[b->hist_idx % SGM_HISTORY_LEN] = rssi;
  b->hist_idx++;

  // Position estimate: weighted centroid of non-empty cells
  float wx = 0, wy = 0, wsum = 0;
  for (int gx = 0; gx < SGM_GRID_X; gx++) {
    for (int gy = 0; gy < SGM_GRID_Y; gy++) {
      int8_t r = b->grid[gx][gy].rssi_avg;
      if (r > -95 && r < 0) {
        float w = 1.0f / (float)((-r) * (-r));
        wx += gx * w;
        wy += gy * w;
        wsum += w;
      }
    }
  }
  if (wsum > 0) {
    b->est_x = wx / wsum / (SGM_GRID_X - 1);
    b->est_y = wy / wsum / (SGM_GRID_Y - 1);
    b->confidence = min(1.0f, b->total_samples / 50.0f);
  }
}

// ─── JSON export ─────────────────────────────────────────────────────────────
static void sgm_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String sgm_json_grid(const SgmBssid &b) {
  String s = "[";
  for (int y = 0; y < SGM_GRID_Y; y++) {
    if (y) s += ",";
    s += "[";
    for (int x = 0; x < SGM_GRID_X; x++) {
      if (x) s += ",";
      s += String(b.grid[x][y].rssi_avg);
    }
    s += "]";
  }
  s += "]";
  return s;
}

static String sgm_json_status() {
  String s = "{\"count\":" + String(g_sgm.count);
  s += ",\"total_samples\":" + String(g_sgm.total_samples);
  s += ",\"pos_x\":" + String(g_sgm.current_x);
  s += ",\"pos_y\":" + String(g_sgm.current_y);
  s += ",\"grid_w\":" + String(SGM_GRID_X);
  s += ",\"grid_h\":" + String(SGM_GRID_Y);
  s += ",\"bssids\":[";
  char mac[18];
  bool first = true;
  for (int i = 0; i < SGM_MAX_BSSIDS; i++) {
    SgmBssid &b = g_sgm.bssids[i];
    if (!b.active) continue;
    if (!first) s += ",";
    first = false;
    sgm_mac_str(b.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(b.ssid) + "\"";
    s += ",\"ch\":" + String(b.channel);
    s += ",\"peak_rssi\":" + String(b.peak_rssi);
    s += ",\"peak_x\":" + String(b.peak_x);
    s += ",\"peak_y\":" + String(b.peak_y);
    s += ",\"est_x\":" + String(b.est_x, 3);
    s += ",\"est_y\":" + String(b.est_y, 3);
    s += ",\"confidence\":" + String(b.confidence, 3);
    s += ",\"samples\":" + String(b.total_samples);
    s += "}";
  }
  s += "]}";
  return s;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void sgm_routes() {
  server.on("/api/sgm/status", HTTP_GET, []() {
    server.send(200, "application/json", sgm_json_status());
  });

  server.on("/api/sgm/position", HTTP_POST, []() {
    int x = server.hasArg("x") ? server.arg("x").toInt() : 0;
    int y = server.hasArg("y") ? server.arg("y").toInt() : 0;
    sgm_set_position((uint8_t)x, (uint8_t)y);
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/sgm/grid", HTTP_GET, []() {
    if (!server.hasArg("bssid")) {
      server.send(400, "application/json", "{\"error\":\"missing bssid\"}"); return;
    }
    uint8_t bssid[6];
    sscanf(server.arg("bssid").c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
           &bssid[0],&bssid[1],&bssid[2],&bssid[3],&bssid[4],&bssid[5]);
    SgmBssid *b = sgm_find(bssid);
    if (!b) { server.send(404, "application/json", "{\"error\":\"not found\"}"); return; }
    server.send(200, "application/json", "{\"grid\":" + sgm_json_grid(*b) + "}");
  });

  server.on("/api/sgm/clear", HTTP_POST, []() {
    memset(&g_sgm, 0, sizeof(g_sgm));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

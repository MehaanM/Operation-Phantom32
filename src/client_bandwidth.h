#pragma once
// client_bandwidth.h — Per-client bandwidth estimation from frame sizes.
// Estimates TX/RX rates for associated stations. Prefix: cbw_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define CBW_MAX_CLIENTS 64
#define CBW_HIST_LEN 30  // 30s of per-second samples
#define CBW_WINDOW_S 5

// ─── Client bandwidth record ──────────────────────────────────────────────────
struct CbwClient {
  bool active;
  uint8_t mac[6];
  uint8_t bssid[6];  // associated AP

  // Bytes per direction
  uint32_t tx_bytes;  // STA → DS
  uint32_t rx_bytes;  // DS → STA
  uint32_t tx_pkts;
  uint32_t rx_pkts;

  // 5s window
  uint32_t win_tx_bytes;
  uint32_t win_rx_bytes;
  uint32_t win_start;

  // Rates (bytes/s)
  float tx_rate;
  float rx_rate;
  float total_rate;

  // History (tx+rx bytes/s last 30s)
  uint16_t rate_hist[CBW_HIST_LEN];
  uint8_t hist_idx;
  uint32_t last_hist_ts;

  // Peak
  float peak_tx;
  float peak_rx;

  // Timestamps
  uint32_t first_seen;
  uint32_t last_seen;
  int8_t rssi;
};

// ─── State ────────────────────────────────────────────────────────────────────
static struct {
  CbwClient clients[CBW_MAX_CLIENTS];
  uint32_t count;
  uint64_t total_bytes;
  uint32_t last_tick_ms;
} g_cbw = {};

// ─── Helpers ─────────────────────────────────────────────────────────────────
static bool cbw_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static CbwClient *cbw_find(const uint8_t *mac) {
  for (int i = 0; i < CBW_MAX_CLIENTS; i++) {
    if (g_cbw.clients[i].active && cbw_mac_eq(g_cbw.clients[i].mac, mac))
      return &g_cbw.clients[i];
  }
  return nullptr;
}

static CbwClient *cbw_alloc(const uint8_t *mac) {
  // LRU: evict oldest inactive
  uint32_t oldest_ts = UINT32_MAX;
  int slot = -1;
  for (int i = 0; i < CBW_MAX_CLIENTS; i++) {
    if (!g_cbw.clients[i].active) {
      slot = i;
      break;
    }
    if (g_cbw.clients[i].last_seen < oldest_ts) {
      oldest_ts = g_cbw.clients[i].last_seen;
      slot = i;
    }
  }
  if (slot < 0)
    return nullptr;

  CbwClient &c = g_cbw.clients[slot];
  if (c.active && slot >= 0)
    g_cbw.count--;
  memset(&c, 0, sizeof(c));
  memcpy(c.mac, mac, 6);
  c.active = true;
  c.first_seen = millis() / 1000;
  g_cbw.count++;
  return &c;
}

// ─── Ingest data frame ────────────────────────────────────────────────────────
void cbw_ingest(const uint8_t *src, const uint8_t *dst, const uint8_t *bssid, uint16_t frame_len,
                int8_t rssi, bool is_to_ds) {
  if (!src || !bssid || frame_len < 10)
    return;

  // Determine client MAC
  const uint8_t *client_mac = is_to_ds ? src : dst;
  if (!client_mac)
    return;

  // Skip broadcast/multicast
  if (client_mac[0] & 0x01)
    return;

  CbwClient *c = cbw_find(client_mac);
  if (!c) {
    c = cbw_alloc(client_mac);
    if (!c)
      return;
    memcpy(c->bssid, bssid, 6);
  }

  uint32_t now_s = millis() / 1000;
  c->last_seen = now_s;
  c->rssi = rssi;
  g_cbw.total_bytes += frame_len;

  // Window reset
  if (now_s - c->win_start >= CBW_WINDOW_S) {
    c->tx_rate = (float)c->win_tx_bytes / CBW_WINDOW_S;
    c->rx_rate = (float)c->win_rx_bytes / CBW_WINDOW_S;
    c->total_rate = c->tx_rate + c->rx_rate;
    if (c->tx_rate > c->peak_tx)
      c->peak_tx = c->tx_rate;
    if (c->rx_rate > c->peak_rx)
      c->peak_rx = c->rx_rate;
    c->win_tx_bytes = 0;
    c->win_rx_bytes = 0;
    c->win_start = now_s;
  }

  if (is_to_ds) {
    c->tx_bytes += frame_len;
    c->tx_pkts++;
    c->win_tx_bytes += frame_len;
  } else {
    c->rx_bytes += frame_len;
    c->rx_pkts++;
    c->win_rx_bytes += frame_len;
  }

  // Rate history
  if (now_s != c->last_hist_ts) {
    c->last_hist_ts = now_s;
    uint16_t rate_kbps = (uint16_t)((c->total_rate * 8) / 1000);
    c->rate_hist[c->hist_idx % CBW_HIST_LEN] = rate_kbps;
    c->hist_idx = (c->hist_idx + 1) % CBW_HIST_LEN;
  }
}

// ─── JSON ─────────────────────────────────────────────────────────────────────
static void cbw_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String cbw_json_client(const CbwClient &c) {
  char mac[18], bssid[18];
  cbw_mac_str(c.mac, mac);
  cbw_mac_str(c.bssid, bssid);
  String s = "{\"mac\":\"" + String(mac) + "\"";
  s += ",\"bssid\":\"" + String(bssid) + "\"";
  s += ",\"tx_rate\":" + String(c.tx_rate, 0);
  s += ",\"rx_rate\":" + String(c.rx_rate, 0);
  s += ",\"total_rate\":" + String(c.total_rate, 0);
  s += ",\"peak_tx\":" + String(c.peak_tx, 0);
  s += ",\"peak_rx\":" + String(c.peak_rx, 0);
  s += ",\"tx_bytes\":" + String(c.tx_bytes);
  s += ",\"rx_bytes\":" + String(c.rx_bytes);
  s += ",\"tx_pkts\":" + String(c.tx_pkts);
  s += ",\"rx_pkts\":" + String(c.rx_pkts);
  s += ",\"rssi\":" + String(c.rssi);
  s += ",\"last_seen\":" + String(c.last_seen) + "}";
  return s;
}

static String cbw_json_all() {
  String s = "{\"count\":" + String(g_cbw.count);
  s += ",\"total_bytes\":" + String((uint32_t)g_cbw.total_bytes);
  s += ",\"clients\":[";
  bool first = true;
  for (int i = 0; i < CBW_MAX_CLIENTS; i++) {
    if (!g_cbw.clients[i].active)
      continue;
    if (!first)
      s += ",";
    first = false;
    s += cbw_json_client(g_cbw.clients[i]);
  }
  s += "]}";
  return s;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void cbw_routes() {
  server.on("/api/cbw/all", HTTP_GET,
            []() { server.send(200, "application/json", cbw_json_all()); });

  server.on("/api/cbw/top", HTTP_GET, []() {
    // Top 10 by total rate
    CbwClient *sorted[CBW_MAX_CLIENTS];
    int n = 0;
    for (int i = 0; i < CBW_MAX_CLIENTS; i++) {
      if (g_cbw.clients[i].active)
        sorted[n++] = &g_cbw.clients[i];
    }
    for (int i = 0; i < n - 1; i++)
      for (int j = i + 1; j < n; j++)
        if (sorted[j]->total_rate > sorted[i]->total_rate) {
          CbwClient *tmp = sorted[i];
          sorted[i] = sorted[j];
          sorted[j] = tmp;
        }
    String s = "[";
    for (int i = 0; i < n && i < 10; i++) {
      if (i)
        s += ",";
      s += cbw_json_client(*sorted[i]);
    }
    s += "]";
    server.send(200, "application/json", s);
  });

  server.on("/api/cbw/clear", HTTP_POST, []() {
    memset(&g_cbw, 0, sizeof(g_cbw));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

#pragma once
// bandwidth_estimator.h — Per-AP and per-client bandwidth estimation.
// Uses frame length, rate, and timing to estimate throughput.
// Prefix: bwe_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define BWE_MAX_APS     48
#define BWE_MAX_CLIENTS 48
#define BWE_WIN_SECS    10   // 10-second rolling window

struct BweNode {
  bool    active;
  uint8_t mac[6];
  char    ssid[33];

  uint8_t  channel;
  int8_t   rssi;

  // Rolling window (1 bucket per second for BWE_WIN_SECS seconds)
  uint32_t bytes_per_sec[BWE_WIN_SECS];
  uint32_t pkts_per_sec[BWE_WIN_SECS];
  uint8_t  win_head;
  uint32_t win_ts;

  // Accumulator for current second
  uint32_t acc_bytes;
  uint32_t acc_pkts;
  uint32_t acc_ts;

  // Rolling stats
  uint32_t avg_bps;    // EMA in bytes/sec
  uint32_t peak_bps;
  uint32_t cur_bps;

  uint64_t total_bytes;
  uint64_t total_pkts;
  uint32_t first_ts;
  uint32_t last_ts;
};

static struct {
  BweNode aps[BWE_MAX_APS];
  uint8_t ap_count;
  BweNode clients[BWE_MAX_CLIENTS];
  uint8_t client_count;

  uint64_t global_bytes;
  uint32_t global_cur_bps;
  uint32_t global_peak_bps;
  uint32_t global_acc_bytes;
  uint32_t global_acc_ts;
} g_bwe = {};

static bool bwe_mac_eq(const uint8_t *a, const uint8_t *b) { return memcmp(a, b, 6) == 0; }

static BweNode* bwe_find_or_alloc(BweNode *tbl, uint8_t &count, int max_sz, const uint8_t *mac) {
  for (int i = 0; i < max_sz; i++)
    if (tbl[i].active && bwe_mac_eq(tbl[i].mac, mac))
      return &tbl[i];
  for (int i = 0; i < max_sz; i++) {
    if (!tbl[i].active) {
      BweNode &n = tbl[i];
      memset(&n, 0, sizeof(n));
      n.active   = true;
      memcpy(n.mac, mac, 6);
      n.rssi     = -100;
      n.first_ts = millis()/1000;
      n.acc_ts   = n.first_ts;
      n.win_ts   = n.first_ts;
      count++;
      return &n;
    }
  }
  uint32_t oldest = UINT32_MAX; int idx = 0;
  for (int i = 0; i < max_sz; i++)
    if (tbl[i].last_ts < oldest) { oldest = tbl[i].last_ts; idx = i; }
  memset(&tbl[idx], 0, sizeof(tbl[0]));
  tbl[idx].active  = true;
  memcpy(tbl[idx].mac, mac, 6);
  tbl[idx].first_ts = millis()/1000;
  tbl[idx].acc_ts   = tbl[idx].first_ts;
  tbl[idx].win_ts   = tbl[idx].first_ts;
  return &tbl[idx];
}

static void bwe_tick_node(BweNode &n, uint32_t now) {
  while (n.acc_ts < now) {
    n.acc_ts++;
    // Store accumulated bytes/pkts in current window bucket
    n.bytes_per_sec[n.win_head] = n.acc_bytes;
    n.pkts_per_sec[n.win_head]  = n.acc_pkts;
    n.win_head = (n.win_head + 1) % BWE_WIN_SECS;

    n.cur_bps = n.acc_bytes;
    if (n.cur_bps > n.peak_bps) n.peak_bps = n.cur_bps;

    // EMA (alpha ≈ 0.3)
    n.avg_bps = (n.avg_bps * 7 + n.cur_bps * 3) / 10;

    n.acc_bytes = 0;
    n.acc_pkts  = 0;
  }
}

static void bwe_update_node(BweNode &n, uint16_t len, uint8_t ch, int8_t rssi) {
  uint32_t now = millis()/1000;
  bwe_tick_node(n, now);
  n.acc_bytes  += len;
  n.acc_pkts++;
  n.total_bytes += len;
  n.total_pkts++;
  n.channel    = ch;
  if (rssi < 0) n.rssi = rssi;
  n.last_ts = now;
}

void bwe_ingest(const uint8_t *ta, const uint8_t *bssid,
                uint8_t channel, int8_t rssi, uint16_t len) {
  uint32_t now = millis()/1000;

  g_bwe.global_bytes += len;
  if (now != g_bwe.global_acc_ts) {
    g_bwe.global_cur_bps = g_bwe.global_acc_bytes;
    if (g_bwe.global_cur_bps > g_bwe.global_peak_bps)
      g_bwe.global_peak_bps = g_bwe.global_cur_bps;
    g_bwe.global_acc_bytes = 0;
    g_bwe.global_acc_ts    = now;
  }
  g_bwe.global_acc_bytes += len;

  // AP node (by BSSID)
  if (bssid && !(bssid[0] & 0x01)) {
    BweNode *n = bwe_find_or_alloc(g_bwe.aps, g_bwe.ap_count, BWE_MAX_APS, bssid);
    if (n) bwe_update_node(*n, len, channel, rssi);
  }

  // Client node (by TA)
  if (ta && !(ta[0] & 0x01)) {
    BweNode *n = bwe_find_or_alloc(g_bwe.clients, g_bwe.client_count, BWE_MAX_CLIENTS, ta);
    if (n) bwe_update_node(*n, len, channel, rssi);
  }
}

static void bwe_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String bwe_node_json(const BweNode &n) {
  char mac[18];
  bwe_mac_str(n.mac, mac);
  String s = "{\"mac\":\"" + String(mac) + "\"";
  s += ",\"ssid\":\"" + String(n.ssid) + "\"";
  s += ",\"ch\":" + String(n.channel);
  s += ",\"rssi\":" + String(n.rssi);
  s += ",\"cur_bps\":" + String(n.cur_bps);
  s += ",\"avg_bps\":" + String(n.avg_bps);
  s += ",\"peak_bps\":" + String(n.peak_bps);
  s += ",\"total_bytes\":" + String((uint32_t)n.total_bytes);
  s += ",\"total_pkts\":" + String((uint32_t)n.total_pkts);
  s += ",\"win\":[";
  for (int i = 0; i < BWE_WIN_SECS; i++) {
    int pos = (n.win_head + i) % BWE_WIN_SECS;
    if (i) s += ",";
    s += String(n.bytes_per_sec[pos]);
  }
  s += "]}";
  return s;
}

static String bwe_json_all() {
  String s = "{\"global_bytes\":" + String((uint32_t)g_bwe.global_bytes);
  s += ",\"global_cur_bps\":" + String(g_bwe.global_cur_bps);
  s += ",\"global_peak_bps\":" + String(g_bwe.global_peak_bps);
  s += ",\"ap_count\":" + String(g_bwe.ap_count);
  s += ",\"client_count\":" + String(g_bwe.client_count);
  s += ",\"aps\":[";
  bool first = true;
  for (int i = 0; i < BWE_MAX_APS; i++) {
    if (!g_bwe.aps[i].active) continue;
    if (!first) s += ",";
    first = false;
    s += bwe_node_json(g_bwe.aps[i]);
  }
  s += "],\"clients\":[";
  first = true;
  for (int i = 0; i < BWE_MAX_CLIENTS; i++) {
    if (!g_bwe.clients[i].active) continue;
    if (!first) s += ",";
    first = false;
    s += bwe_node_json(g_bwe.clients[i]);
  }
  s += "]}";
  return s;
}

inline void bwe_routes() {
  server.on("/api/bwe/all", HTTP_GET, []() {
    server.send(200, "application/json", bwe_json_all());
  });
  server.on("/api/bwe/clear", HTTP_POST, []() {
    memset(&g_bwe, 0, sizeof(g_bwe));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

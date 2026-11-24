#pragma once
// retransmission_analyzer.h — 802.11 retry/retransmission analysis.
// Tracks per-AP and per-client retry rates, burst detection, and link quality.
// Prefix: rta_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define RTA_MAX_APS     48
#define RTA_MAX_CLIENTS 48
#define RTA_HIST_LEN    30

struct RtaNode {
  bool    active;
  uint8_t mac[6];
  char    ssid[33];     // only for APs

  uint32_t total_frames;
  uint32_t retry_frames;
  uint32_t retry_bursts;    // consecutive retries > 5 in a row

  uint8_t  retry_pct;       // 0-100
  uint8_t  cur_burst;       // current consecutive retry count
  uint8_t  max_burst;       // max ever seen

  // Rolling 30-second retry % history
  uint8_t  hist[RTA_HIST_LEN];
  uint8_t  hist_head;

  // Per-second accumulator
  uint32_t acc_frames;
  uint32_t acc_retries;
  uint32_t acc_ts;

  uint8_t  channel;
  int8_t   rssi;
  uint32_t last_ts;
};

static struct {
  RtaNode aps[RTA_MAX_APS];
  uint8_t ap_count;
  RtaNode clients[RTA_MAX_CLIENTS];
  uint8_t client_count;

  uint64_t global_frames;
  uint64_t global_retries;
  uint8_t  global_retry_pct;

  uint32_t acc_frames;
  uint32_t acc_retries;
  uint32_t acc_ts;
} g_rta = {};

static bool rta_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static bool rta_is_broadcast(const uint8_t *m) {
  return m[0] == 0xFF;
}

static RtaNode* rta_find_or_alloc(RtaNode *tbl, uint8_t &count, int max_sz, const uint8_t *mac) {
  for (int i = 0; i < max_sz; i++)
    if (tbl[i].active && rta_mac_eq(tbl[i].mac, mac))
      return &tbl[i];
  for (int i = 0; i < max_sz; i++) {
    if (!tbl[i].active) {
      memset(&tbl[i], 0, sizeof(tbl[0]));
      tbl[i].active = true;
      memcpy(tbl[i].mac, mac, 6);
      count++;
      return &tbl[i];
    }
  }
  uint32_t oldest = UINT32_MAX; int idx = 0;
  for (int i = 0; i < max_sz; i++)
    if (tbl[i].last_ts < oldest) { oldest = tbl[i].last_ts; idx = i; }
  memset(&tbl[idx], 0, sizeof(tbl[0]));
  tbl[idx].active = true;
  memcpy(tbl[idx].mac, mac, 6);
  return &tbl[idx];
}

static void rta_tick_node(RtaNode &n, uint32_t now) {
  if (now == n.acc_ts) return;
  n.acc_ts = now;
  if (n.total_frames > 10)
    n.retry_pct = (uint8_t)(n.retry_frames * 100 / n.total_frames);
  n.hist[n.hist_head] = n.retry_pct;
  n.hist_head = (n.hist_head + 1) % RTA_HIST_LEN;
  n.acc_frames  = 0;
  n.acc_retries = 0;
}

static void rta_update_node(RtaNode &n, bool retry, uint8_t ch, int8_t rssi) {
  uint32_t now = millis()/1000;
  rta_tick_node(n, now);

  n.total_frames++;
  n.acc_frames++;
  n.channel = ch;
  if (rssi < 0 && rssi > -120) n.rssi = rssi;
  n.last_ts = now;

  if (retry) {
    n.retry_frames++;
    n.acc_retries++;
    n.cur_burst++;
    if (n.cur_burst > n.max_burst) n.max_burst = n.cur_burst;
    if (n.cur_burst >= 5) {
      n.retry_bursts++;
      n.cur_burst = 0; // reset burst counter after recording
    }
  } else {
    n.cur_burst = 0;
  }
}

void rta_ingest(const uint8_t *ta, const uint8_t *ra, const uint8_t *bssid,
                uint8_t channel, int8_t rssi, bool retry) {
  uint32_t now = millis()/1000;
  g_rta.global_frames++;
  if (retry) g_rta.global_retries++;

  if (now != g_rta.acc_ts) {
    g_rta.acc_ts = now;
    if (g_rta.global_frames > 10)
      g_rta.global_retry_pct = (uint8_t)(g_rta.global_retries * 100 / g_rta.global_frames);
    g_rta.acc_frames = g_rta.acc_retries = 0;
  }

  // AP node (by BSSID)
  if (!rta_is_broadcast(bssid)) {
    RtaNode *ap = rta_find_or_alloc(g_rta.aps, g_rta.ap_count, RTA_MAX_APS, bssid);
    if (ap) rta_update_node(*ap, retry, channel, rssi);
  }

  // Client node (by TA = transmitter address)
  if (!rta_is_broadcast(ta)) {
    RtaNode *cl = rta_find_or_alloc(g_rta.clients, g_rta.client_count, RTA_MAX_CLIENTS, ta);
    if (cl) rta_update_node(*cl, retry, channel, rssi);
  }
}

static void rta_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String rta_node_json(const RtaNode &n) {
  char mac[18];
  rta_mac_str(n.mac, mac);
  String s = "{\"mac\":\"" + String(mac) + "\"";
  s += ",\"frames\":" + String(n.total_frames);
  s += ",\"retries\":" + String(n.retry_frames);
  s += ",\"retry_pct\":" + String(n.retry_pct);
  s += ",\"bursts\":" + String(n.retry_bursts);
  s += ",\"max_burst\":" + String(n.max_burst);
  s += ",\"ch\":" + String(n.channel);
  s += ",\"rssi\":" + String(n.rssi);
  s += ",\"hist\":[";
  for (int i = 0; i < RTA_HIST_LEN; i++) {
    int pos = (n.hist_head + i) % RTA_HIST_LEN;
    if (i) s += ",";
    s += String(n.hist[pos]);
  }
  s += "]}";
  return s;
}

static String rta_json_summary() {
  String s = "{\"global_frames\":" + String((uint32_t)g_rta.global_frames);
  s += ",\"global_retries\":" + String((uint32_t)g_rta.global_retries);
  s += ",\"global_retry_pct\":" + String(g_rta.global_retry_pct);
  s += ",\"ap_count\":" + String(g_rta.ap_count);
  s += ",\"client_count\":" + String(g_rta.client_count);
  s += ",\"aps\":[";
  bool first = true;
  for (int i = 0; i < RTA_MAX_APS; i++) {
    if (!g_rta.aps[i].active) continue;
    if (!first) s += ",";
    first = false;
    s += rta_node_json(g_rta.aps[i]);
  }
  s += "],\"clients\":[";
  first = true;
  for (int i = 0; i < RTA_MAX_CLIENTS; i++) {
    if (!g_rta.clients[i].active) continue;
    if (!first) s += ",";
    first = false;
    s += rta_node_json(g_rta.clients[i]);
  }
  s += "]}";
  return s;
}

inline void rta_routes() {
  server.on("/api/rta/summary", HTTP_GET, []() {
    server.send(200, "application/json", rta_json_summary());
  });
  server.on("/api/rta/clear", HTTP_POST, []() {
    memset(&g_rta, 0, sizeof(g_rta));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

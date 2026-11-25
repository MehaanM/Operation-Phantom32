#pragma once
// auth_tracker.h — Authentication event tracker.
// Monitors 802.11 authentication exchanges and detects anomalies. Prefix: ath_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define ATH_MAX_EVENTS 128
#define ATH_MAX_PAIRS 32
#define ATH_SSID_LEN 33

// ─── Auth algorithm ───────────────────────────────────────────────────────────
enum AthAlgo {
  ATH_ALGO_OPEN = 0,
  ATH_ALGO_SHARED = 1,
  ATH_ALGO_FT = 2,
  ATH_ALGO_SAE = 3,  // WPA3
  ATH_ALGO_FILS = 4,
  ATH_ALGO_OWE = 18,
  ATH_ALGO_OTHER = 255
};

// ─── Auth event ───────────────────────────────────────────────────────────────
struct AthEvent {
  bool used;
  uint32_t ts;
  uint8_t client[6];
  uint8_t bssid[6];
  uint8_t channel;
  int8_t rssi;
  AthAlgo algo;
  uint16_t seq;      // transaction sequence number (1,2,3,4)
  uint16_t status;   // 802.11 status code
  bool is_response;  // false=request from STA, true=response from AP
  bool success;      // seq==2 and status==0
  bool fail;         // seq==2 and status!=0
};

// ─── Auth pair (tracks exchange between one STA and one AP) ───────────────────
struct AthPair {
  bool active;
  uint8_t client[6];
  uint8_t bssid[6];
  uint8_t channel;
  AthAlgo algo;
  uint32_t req_ts;
  uint32_t resp_ts;
  uint16_t status;
  bool complete;
  bool success;
  uint8_t retries;  // number of duplicate req/resp
  uint32_t total_time_ms;
};

// ─── State ────────────────────────────────────────────────────────────────────
static struct {
  AthEvent events[ATH_MAX_EVENTS];
  uint8_t event_head;
  AthPair pairs[ATH_MAX_PAIRS];
  uint8_t pair_count;

  uint32_t total_events;
  uint32_t success_count;
  uint32_t fail_count;
  uint32_t sae_count;
  uint32_t open_count;
  uint32_t shared_count;
  uint32_t ft_count;

  // Burst detection
  uint32_t burst_window_start;
  uint8_t burst_count;
  bool burst_detected;
} g_ath = {};

// ─── Helpers ─────────────────────────────────────────────────────────────────
static bool ath_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static AthPair *ath_find_pair(const uint8_t *client, const uint8_t *bssid) {
  for (int i = 0; i < ATH_MAX_PAIRS; i++) {
    if (g_ath.pairs[i].active && ath_mac_eq(g_ath.pairs[i].client, client) &&
        ath_mac_eq(g_ath.pairs[i].bssid, bssid))
      return &g_ath.pairs[i];
  }
  return nullptr;
}

static AthPair *ath_alloc_pair(const uint8_t *client, const uint8_t *bssid) {
  for (int i = 0; i < ATH_MAX_PAIRS; i++) {
    if (!g_ath.pairs[i].active) {
      AthPair &p = g_ath.pairs[i];
      memset(&p, 0, sizeof(p));
      memcpy(p.client, client, 6);
      memcpy(p.bssid, bssid, 6);
      p.active = true;
      g_ath.pair_count++;
      return &p;
    }
  }
  // Evict oldest completed pair
  for (int i = 0; i < ATH_MAX_PAIRS; i++) {
    if (g_ath.pairs[i].complete) {
      AthPair &p = g_ath.pairs[i];
      memset(&p, 0, sizeof(p));
      memcpy(p.client, client, 6);
      memcpy(p.bssid, bssid, 6);
      p.active = true;
      return &p;
    }
  }
  return nullptr;
}

// ─── Log event ────────────────────────────────────────────────────────────────
static void ath_log(const uint8_t *client, const uint8_t *bssid, uint8_t channel, int8_t rssi,
                    AthAlgo algo, uint16_t seq, uint16_t status, bool is_resp) {
  AthEvent &e = g_ath.events[g_ath.event_head % ATH_MAX_EVENTS];
  e.used = true;
  e.ts = millis() / 1000;
  if (client)
    memcpy(e.client, client, 6);
  if (bssid)
    memcpy(e.bssid, bssid, 6);
  e.channel = channel;
  e.rssi = rssi;
  e.algo = algo;
  e.seq = seq;
  e.status = status;
  e.is_response = is_resp;
  e.success = (seq == 2 && status == 0);
  e.fail = (seq == 2 && status != 0);
  g_ath.event_head++;
  g_ath.total_events++;

  if (e.success)
    g_ath.success_count++;
  if (e.fail)
    g_ath.fail_count++;

  switch (algo) {
    case ATH_ALGO_SAE:
      g_ath.sae_count++;
      break;
    case ATH_ALGO_OPEN:
      g_ath.open_count++;
      break;
    case ATH_ALGO_SHARED:
      g_ath.shared_count++;
      break;
    case ATH_ALGO_FT:
      g_ath.ft_count++;
      break;
    default:
      break;
  }
}

// ─── Ingest 802.11 auth frame ─────────────────────────────────────────────────
void ath_ingest(const uint8_t *raw, int len, const uint8_t *src, const uint8_t *dst,
                const uint8_t *bssid, int8_t rssi, uint8_t channel) {
  if (len < 30)
    return;

  // Auth frame fixed fields: algo(2) + seq(2) + status(2) starting at MAC header end
  int body_off = 24;  // MAC header
  if (body_off + 6 > len)
    return;

  uint16_t algo = raw[body_off] | (raw[body_off + 1] << 8);
  uint16_t seq = raw[body_off + 2] | (raw[body_off + 3] << 8);
  uint16_t status = raw[body_off + 4] | (raw[body_off + 5] << 8);

  AthAlgo a;
  switch (algo) {
    case 0:
      a = ATH_ALGO_OPEN;
      break;
    case 1:
      a = ATH_ALGO_SHARED;
      break;
    case 2:
      a = ATH_ALGO_FT;
      break;
    case 3:
      a = ATH_ALGO_SAE;
      break;
    case 4:
      a = ATH_ALGO_FILS;
      break;
    case 18:
      a = ATH_ALGO_OWE;
      break;
    default:
      a = ATH_ALGO_OTHER;
      break;
  }

  // Determine client vs AP direction
  const uint8_t *client = src;
  bool is_resp = (seq == 2 || seq == 4);  // even seq = AP response

  ath_log(client, bssid, channel, rssi, a, seq, status, is_resp);

  // Track pair
  AthPair *pair = ath_find_pair(client, bssid);
  if (!pair)
    pair = ath_alloc_pair(client, bssid);
  if (!pair)
    return;

  pair->algo = a;
  pair->channel = channel;
  if (seq == 1) {
    pair->req_ts = millis();
    pair->retries = pair->complete ? pair->retries + 1 : 0;
    pair->complete = false;
  } else if (seq == 2) {
    pair->resp_ts = millis();
    pair->status = status;
    pair->success = (status == 0);
    pair->complete = true;
    if (pair->req_ts > 0)
      pair->total_time_ms = millis() - pair->req_ts;
  }

  // Burst detection: >20 auth requests in 1 second
  uint32_t now = millis() / 1000;
  if (now != g_ath.burst_window_start) {
    g_ath.burst_window_start = now;
    g_ath.burst_count = 0;
    g_ath.burst_detected = false;
  }
  if (seq == 1) {
    g_ath.burst_count++;
    if (g_ath.burst_count > 20)
      g_ath.burst_detected = true;
  }
}

// ─── JSON ─────────────────────────────────────────────────────────────────────
static void ath_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static const char *ath_algo_str(AthAlgo a) {
  switch (a) {
    case ATH_ALGO_OPEN:
      return "open";
    case ATH_ALGO_SHARED:
      return "shared";
    case ATH_ALGO_FT:
      return "ft";
    case ATH_ALGO_SAE:
      return "sae";
    case ATH_ALGO_FILS:
      return "fils";
    case ATH_ALGO_OWE:
      return "owe";
    default:
      return "other";
  }
}

static String ath_json_status() {
  String s = "{\"total_events\":" + String(g_ath.total_events);
  s += ",\"success\":" + String(g_ath.success_count);
  s += ",\"fail\":" + String(g_ath.fail_count);
  s += ",\"sae\":" + String(g_ath.sae_count);
  s += ",\"open\":" + String(g_ath.open_count);
  s += ",\"shared\":" + String(g_ath.shared_count);
  s += ",\"ft\":" + String(g_ath.ft_count);
  s += ",\"burst_detected\":" + String(g_ath.burst_detected ? "true" : "false");
  s += ",\"burst_count\":" + String(g_ath.burst_count);
  s += ",\"pairs\":[";
  char mac1[18], mac2[18];
  bool first = true;
  for (int i = 0; i < ATH_MAX_PAIRS; i++) {
    AthPair &p = g_ath.pairs[i];
    if (!p.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    ath_mac_str(p.client, mac1);
    ath_mac_str(p.bssid, mac2);
    s += "{\"client\":\"" + String(mac1) + "\"";
    s += ",\"bssid\":\"" + String(mac2) + "\"";
    s += ",\"algo\":\"" + String(ath_algo_str(p.algo)) + "\"";
    s += ",\"status\":" + String(p.status);
    s += ",\"success\":" + String(p.success ? "true" : "false");
    s += ",\"complete\":" + String(p.complete ? "true" : "false");
    s += ",\"retries\":" + String(p.retries);
    s += ",\"time_ms\":" + String(p.total_time_ms);
    s += "}";
  }
  s += "],\"events\":[";
  first = true;
  for (int i = 0; i < ATH_MAX_EVENTS; i++) {
    int idx = (g_ath.event_head - ATH_MAX_EVENTS + i + ATH_MAX_EVENTS) % ATH_MAX_EVENTS;
    AthEvent &e = g_ath.events[idx];
    if (!e.used)
      continue;
    if (!first)
      s += ",";
    first = false;
    ath_mac_str(e.client, mac1);
    ath_mac_str(e.bssid, mac2);
    s += "{\"ts\":" + String(e.ts);
    s += ",\"client\":\"" + String(mac1) + "\"";
    s += ",\"bssid\":\"" + String(mac2) + "\"";
    s += ",\"algo\":\"" + String(ath_algo_str(e.algo)) + "\"";
    s += ",\"seq\":" + String(e.seq);
    s += ",\"status\":" + String(e.status);
    s += ",\"ok\":" + String(e.success ? "true" : "false");
    s += "}";
  }
  s += "]}";
  return s;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void ath_routes() {
  server.on("/api/ath/status", HTTP_GET,
            []() { server.send(200, "application/json", ath_json_status()); });

  server.on("/api/ath/clear", HTTP_POST, []() {
    memset(&g_ath, 0, sizeof(g_ath));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

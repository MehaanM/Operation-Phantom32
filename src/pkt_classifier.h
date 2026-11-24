#pragma once
// pkt_classifier.h — 802.11 frame type classification and statistics engine.
// Classifies every frame and maintains rolling counters. Prefix: pkc_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define PKC_HIST_LEN    30    // 30-second rolling window
#define PKC_SUBTYPE_N   16

// ─── Frame class ──────────────────────────────────────────────────────────────
enum PkcClass {
  PKC_MGMT_PROBE_REQ = 0,
  PKC_MGMT_PROBE_RESP,
  PKC_MGMT_BEACON,
  PKC_MGMT_ASSOC_REQ,
  PKC_MGMT_ASSOC_RESP,
  PKC_MGMT_REASSOC_REQ,
  PKC_MGMT_REASSOC_RESP,
  PKC_MGMT_DISASSOC,
  PKC_MGMT_AUTH,
  PKC_MGMT_DEAUTH,
  PKC_MGMT_ACTION,
  PKC_MGMT_OTHER,
  PKC_CTRL_RTS,
  PKC_CTRL_CTS,
  PKC_CTRL_ACK,
  PKC_CTRL_BLOCK_ACK,
  PKC_CTRL_OTHER,
  PKC_DATA,
  PKC_DATA_NULL,
  PKC_DATA_QOS,
  PKC_DATA_OTHER,
  PKC_TOTAL,          // total frames
  PKC_CLASS_COUNT
};

static const char* PKC_NAMES[] = {
  "probe_req","probe_resp","beacon","assoc_req","assoc_resp",
  "reassoc_req","reassoc_resp","disassoc","auth","deauth","action","mgmt_other",
  "rts","cts","ack","block_ack","ctrl_other",
  "data","null_data","qos_data","data_other","total"
};

// ─── State ────────────────────────────────────────────────────────────────────
static struct {
  uint64_t counters[PKC_CLASS_COUNT];
  uint32_t hist[PKC_CLASS_COUNT][PKC_HIST_LEN]; // per-second counts
  uint8_t  hist_idx;
  uint32_t last_tick_ts;
  uint32_t last_window_counts[PKC_CLASS_COUNT]; // counts at start of current second
  uint16_t rate_1s[PKC_CLASS_COUNT];            // frames/sec in last second
  uint32_t rate_30s[PKC_CLASS_COUNT];           // sum over 30 seconds

  // Size distribution buckets (<64, 64-127, 128-255, 256-511, 512-1023, >=1024)
  uint32_t size_buckets[6];

  // Top talkers (by src MAC)
  struct { uint8_t mac[6]; uint32_t frames; bool active; } top[8];
} g_pkc = {};

// ─── Classify ─────────────────────────────────────────────────────────────────
static PkcClass pkc_classify(uint8_t type, uint8_t sub) {
  if (type == 0) { // management
    switch (sub) {
      case 0:  return PKC_MGMT_ASSOC_REQ;
      case 1:  return PKC_MGMT_ASSOC_RESP;
      case 2:  return PKC_MGMT_REASSOC_REQ;
      case 3:  return PKC_MGMT_REASSOC_RESP;
      case 4:  return PKC_MGMT_PROBE_REQ;
      case 5:  return PKC_MGMT_PROBE_RESP;
      case 8:  return PKC_MGMT_BEACON;
      case 10: return PKC_MGMT_DISASSOC;
      case 11: return PKC_MGMT_AUTH;
      case 12: return PKC_MGMT_DEAUTH;
      case 13: return PKC_MGMT_ACTION;
      default: return PKC_MGMT_OTHER;
    }
  }
  if (type == 1) { // control
    switch (sub) {
      case 11: return PKC_CTRL_RTS;
      case 12: return PKC_CTRL_CTS;
      case 13: return PKC_CTRL_ACK;
      case 8: case 9: return PKC_CTRL_BLOCK_ACK;
      default: return PKC_CTRL_OTHER;
    }
  }
  if (type == 2) { // data
    if (sub == 4 || sub == 12) return PKC_DATA_NULL;
    if (sub >= 8) return PKC_DATA_QOS;
    if (sub == 0 || sub == 1 || sub == 2 || sub == 3) return PKC_DATA;
    return PKC_DATA_OTHER;
  }
  return PKC_DATA_OTHER;
}

// ─── Top talker update ────────────────────────────────────────────────────────
static void pkc_update_top(const uint8_t *src) {
  for (int i = 0; i < 8; i++) {
    if (g_pkc.top[i].active && memcmp(g_pkc.top[i].mac, src, 6) == 0) {
      g_pkc.top[i].frames++;
      return;
    }
  }
  // Find slot
  for (int i = 0; i < 8; i++) {
    if (!g_pkc.top[i].active) {
      memcpy(g_pkc.top[i].mac, src, 6);
      g_pkc.top[i].frames = 1;
      g_pkc.top[i].active = true;
      return;
    }
  }
  // Evict lowest
  int min_i = 0;
  for (int i = 1; i < 8; i++) if (g_pkc.top[i].frames < g_pkc.top[min_i].frames) min_i = i;
  memcpy(g_pkc.top[min_i].mac, src, 6);
  g_pkc.top[min_i].frames = 1;
}

// ─── Ingest ───────────────────────────────────────────────────────────────────
void pkc_ingest(const uint8_t *raw, int len, const uint8_t *src,
                uint8_t type, uint8_t sub) {
  PkcClass cls = pkc_classify(type, sub);
  g_pkc.counters[cls]++;
  g_pkc.counters[PKC_TOTAL]++;

  // Size bucket
  if      (len < 64)   g_pkc.size_buckets[0]++;
  else if (len < 128)  g_pkc.size_buckets[1]++;
  else if (len < 256)  g_pkc.size_buckets[2]++;
  else if (len < 512)  g_pkc.size_buckets[3]++;
  else if (len < 1024) g_pkc.size_buckets[4]++;
  else                 g_pkc.size_buckets[5]++;

  if (src && !(src[0]&0x01)) pkc_update_top(src);
}

// ─── Tick (call every 1 second) ───────────────────────────────────────────────
void pkc_tick() {
  uint32_t now = millis() / 1000;
  if (now == g_pkc.last_tick_ts) return;
  g_pkc.last_tick_ts = now;

  uint8_t idx = g_pkc.hist_idx % PKC_HIST_LEN;
  for (int i = 0; i < PKC_CLASS_COUNT; i++) {
    uint32_t delta = (uint32_t)(g_pkc.counters[i] - g_pkc.last_window_counts[i]);
    g_pkc.hist[i][idx] = delta;
    g_pkc.last_window_counts[i] = (uint32_t)g_pkc.counters[i];
    g_pkc.rate_1s[i] = (uint16_t)min((uint32_t)65535, delta);

    // 30-second sum
    uint32_t sum = 0;
    for (int j = 0; j < PKC_HIST_LEN; j++) sum += g_pkc.hist[i][j];
    g_pkc.rate_30s[i] = sum;
  }
  g_pkc.hist_idx++;
}

// ─── JSON ─────────────────────────────────────────────────────────────────────
static void pkc_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String pkc_json_summary() {
  String s = "{\"total\":" + String((uint32_t)g_pkc.counters[PKC_TOTAL]);
  s += ",\"counters\":{";
  for (int i = 0; i < PKC_CLASS_COUNT; i++) {
    if (i) s += ",";
    s += "\"" + String(PKC_NAMES[i]) + "\":" + String((uint32_t)g_pkc.counters[i]);
  }
  s += "},\"rate_1s\":{";
  for (int i = 0; i < PKC_CLASS_COUNT; i++) {
    if (i) s += ",";
    s += "\"" + String(PKC_NAMES[i]) + "\":" + String(g_pkc.rate_1s[i]);
  }
  s += "},\"size_buckets\":[";
  for (int i = 0; i < 6; i++) {
    if (i) s += ",";
    s += String(g_pkc.size_buckets[i]);
  }
  s += "],\"top_talkers\":[";
  char mac[18];
  bool first = true;
  for (int i = 0; i < 8; i++) {
    if (!g_pkc.top[i].active) continue;
    if (!first) s += ",";
    first = false;
    pkc_mac_str(g_pkc.top[i].mac, mac);
    s += "{\"mac\":\"" + String(mac) + "\",\"frames\":" + String(g_pkc.top[i].frames) + "}";
  }
  s += "]}";
  return s;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void pkc_routes() {
  server.on("/api/pkc/summary", HTTP_GET, []() {
    server.send(200, "application/json", pkc_json_summary());
  });

  server.on("/api/pkc/hist", HTTP_GET, []() {
    if (!server.hasArg("class")) {
      server.send(400, "application/json", "{\"error\":\"missing class\"}"); return;
    }
    int cls = server.arg("class").toInt();
    if (cls < 0 || cls >= PKC_CLASS_COUNT) {
      server.send(400, "application/json", "{\"error\":\"invalid class\"}"); return;
    }
    String s = "[";
    for (int i = 0; i < PKC_HIST_LEN; i++) {
      if (i) s += ",";
      int idx = (g_pkc.hist_idx - PKC_HIST_LEN + i + PKC_HIST_LEN) % PKC_HIST_LEN;
      s += String(g_pkc.hist[cls][idx]);
    }
    s += "]";
    server.send(200, "application/json", s);
  });

  server.on("/api/pkc/clear", HTTP_POST, []() {
    memset(&g_pkc, 0, sizeof(g_pkc));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

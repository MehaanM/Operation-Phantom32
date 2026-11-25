#pragma once
// traffic_profiler.h — Deep per-flow traffic profiling and protocol distribution.
// Tracks frame type/subtype distribution, QoS, retries, fragmentation. Prefix: trp_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

// ─── Frame type/subtype distribution ──────────────────────────────────────────
#define TRP_MGMT_SUBTYPES 16
#define TRP_DATA_SUBTYPES 16
#define TRP_CTRL_SUBTYPES 16
#define TRP_MAX_FLOWS 64
#define TRP_WINDOW_S 5

struct TrpFlowKey {
  uint8_t src[6];
  uint8_t dst[6];
  uint8_t channel;
};

struct TrpFlow {
  bool active;
  TrpFlowKey key;

  uint32_t pkt_count;
  uint32_t byte_count;
  uint32_t retry_count;
  uint32_t frag_count;
  uint32_t prot_count;  // protected (encrypted) frames
  uint32_t qos_count;

  uint8_t type_dist[3];  // 0=mgmt 1=ctrl 2=data counts (pct)
  uint8_t avg_rssi_neg;  // stored as positive (negate to get dBm)

  // Window-based rate
  uint32_t window_pkts;
  uint32_t window_bytes;
  uint32_t window_start_ts;

  uint32_t pkt_rate;   // pkt/s over last window
  uint32_t byte_rate;  // bytes/s over last window

  uint32_t first_ts;
  uint32_t last_ts;
};

// ─── Global distribution counters ─────────────────────────────────────────────
static struct {
  // 802.11 frame type distribution
  uint32_t mgmt_total;
  uint32_t ctrl_total;
  uint32_t data_total;

  // Management subtypes
  uint32_t mgmt_sub[TRP_MGMT_SUBTYPES];

  // Data subtypes (QoS, CF-Ack, etc.)
  uint32_t data_sub[TRP_DATA_SUBTYPES];

  // Control subtypes (RTS, CTS, ACK, etc.)
  uint32_t ctrl_sub[TRP_CTRL_SUBTYPES];

  // Quality metrics
  uint32_t retry_total;
  uint32_t frag_total;
  uint32_t prot_total;
  uint32_t qos_total;

  // Channel distribution
  uint32_t ch_dist[14];

  // Rate tracking
  uint32_t total_frames;
  uint32_t total_bytes;
  uint32_t frames_last_s;
  uint32_t bytes_last_s;
  uint32_t last_rate_ts;

  // Per-flow table
  TrpFlow flows[TRP_MAX_FLOWS];
  uint8_t flow_count;
} g_trp = {};

// ─── Helpers ─────────────────────────────────────────────────────────────────
static bool trp_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static bool trp_is_bcast(const uint8_t *m) {
  return m[0] == 0xFF && m[1] == 0xFF && m[2] == 0xFF && m[3] == 0xFF && m[4] == 0xFF &&
         m[5] == 0xFF;
}

static TrpFlow *trp_find_or_alloc(const uint8_t *src, const uint8_t *dst, uint8_t ch) {
  for (int i = 0; i < TRP_MAX_FLOWS; i++) {
    if (!g_trp.flows[i].active)
      continue;
    if (trp_mac_eq(g_trp.flows[i].key.src, src) && trp_mac_eq(g_trp.flows[i].key.dst, dst) &&
        g_trp.flows[i].key.channel == ch)
      return &g_trp.flows[i];
  }
  for (int i = 0; i < TRP_MAX_FLOWS; i++) {
    if (!g_trp.flows[i].active) {
      TrpFlow &f = g_trp.flows[i];
      memset(&f, 0, sizeof(f));
      f.active = true;
      memcpy(f.key.src, src, 6);
      memcpy(f.key.dst, dst, 6);
      f.key.channel = ch;
      f.first_ts = millis() / 1000;
      f.window_start_ts = f.first_ts;
      g_trp.flow_count++;
      return &f;
    }
  }
  // Evict oldest
  uint32_t oldest = UINT32_MAX;
  int idx = 0;
  for (int i = 0; i < TRP_MAX_FLOWS; i++)
    if (g_trp.flows[i].last_ts < oldest) {
      oldest = g_trp.flows[i].last_ts;
      idx = i;
    }
  TrpFlow &f = g_trp.flows[idx];
  memset(&f, 0, sizeof(f));
  f.active = true;
  memcpy(f.key.src, src, 6);
  memcpy(f.key.dst, dst, 6);
  f.key.channel = ch;
  f.first_ts = millis() / 1000;
  f.window_start_ts = f.first_ts;
  return &f;
}

// ─── Ingest ───────────────────────────────────────────────────────────────────
void trp_ingest(const uint8_t *src, const uint8_t *dst, uint8_t type, uint8_t subtype, bool retry,
                bool frag, bool prot, bool qos, uint8_t channel, uint16_t frame_len, int8_t rssi) {
  if (!src || !dst)
    return;

  uint32_t now = millis() / 1000;
  g_trp.total_frames++;
  g_trp.total_bytes += frame_len;

  // Type distribution
  if (type == 0) {
    g_trp.mgmt_total++;
    if (subtype < TRP_MGMT_SUBTYPES)
      g_trp.mgmt_sub[subtype]++;
  } else if (type == 1) {
    g_trp.ctrl_total++;
    if (subtype < TRP_CTRL_SUBTYPES)
      g_trp.ctrl_sub[subtype]++;
  } else {
    g_trp.data_total++;
    if (subtype < TRP_DATA_SUBTYPES)
      g_trp.data_sub[subtype]++;
  }

  // Quality metrics
  if (retry)
    g_trp.retry_total++;
  if (frag)
    g_trp.frag_total++;
  if (prot)
    g_trp.prot_total++;
  if (qos)
    g_trp.qos_total++;

  // Channel
  if (channel >= 1 && channel <= 14)
    g_trp.ch_dist[channel - 1]++;

  // Rate window (1s)
  if (now != g_trp.last_rate_ts) {
    g_trp.frames_last_s = g_trp.total_frames;
    g_trp.bytes_last_s = g_trp.total_bytes;
    g_trp.last_rate_ts = now;
  }

  // Per-flow
  if (!trp_is_bcast(dst)) {
    TrpFlow *f = trp_find_or_alloc(src, dst, channel);
    if (f) {
      f->pkt_count++;
      f->byte_count += frame_len;
      if (retry)
        f->retry_count++;
      if (frag)
        f->frag_count++;
      if (prot)
        f->prot_count++;
      if (qos)
        f->qos_count++;
      f->avg_rssi_neg = (uint8_t)((-rssi * 1 + f->avg_rssi_neg * 7) / 8);
      f->last_ts = now;

      // Window rate
      if (now - f->window_start_ts >= TRP_WINDOW_S) {
        f->pkt_rate = f->window_pkts / TRP_WINDOW_S;
        f->byte_rate = f->window_bytes / TRP_WINDOW_S;
        f->window_pkts = 0;
        f->window_bytes = 0;
        f->window_start_ts = now;
      }
      f->window_pkts++;
      f->window_bytes += frame_len;
    }
  }
}

// ─── Tick ─────────────────────────────────────────────────────────────────────
void trp_tick() {
  // Expire flows silent for 120s
  uint32_t now = millis() / 1000;
  for (int i = 0; i < TRP_MAX_FLOWS; i++) {
    if (!g_trp.flows[i].active)
      continue;
    if (now - g_trp.flows[i].last_ts > 120) {
      memset(&g_trp.flows[i], 0, sizeof(g_trp.flows[i]));
      if (g_trp.flow_count > 0)
        g_trp.flow_count--;
    }
  }
}

// ─── JSON builders ────────────────────────────────────────────────────────────
static void trp_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String trp_json_distribution() {
  uint32_t total = g_trp.mgmt_total + g_trp.ctrl_total + g_trp.data_total;
  String s = "{\"total_frames\":" + String(total);
  s += ",\"total_bytes\":" + String(g_trp.total_bytes);
  s += ",\"mgmt\":" + String(g_trp.mgmt_total);
  s += ",\"ctrl\":" + String(g_trp.ctrl_total);
  s += ",\"data\":" + String(g_trp.data_total);
  s += ",\"retry\":" + String(g_trp.retry_total);
  s += ",\"frag\":" + String(g_trp.frag_total);
  s += ",\"prot\":" + String(g_trp.prot_total);
  s += ",\"qos\":" + String(g_trp.qos_total);
  s += ",\"mgmt_sub\":[";
  for (int i = 0; i < TRP_MGMT_SUBTYPES; i++) {
    if (i)
      s += ",";
    s += String(g_trp.mgmt_sub[i]);
  }
  s += "],\"data_sub\":[";
  for (int i = 0; i < TRP_DATA_SUBTYPES; i++) {
    if (i)
      s += ",";
    s += String(g_trp.data_sub[i]);
  }
  s += "],\"ctrl_sub\":[";
  for (int i = 0; i < TRP_CTRL_SUBTYPES; i++) {
    if (i)
      s += ",";
    s += String(g_trp.ctrl_sub[i]);
  }
  s += "],\"ch_dist\":[";
  for (int i = 0; i < 14; i++) {
    if (i)
      s += ",";
    s += String(g_trp.ch_dist[i]);
  }
  s += "]}";
  return s;
}

static String trp_json_flows() {
  char mac_s[18], mac_d[18];
  String s = "{\"count\":" + String(g_trp.flow_count) + ",\"flows\":[";
  bool first = true;
  for (int i = 0; i < TRP_MAX_FLOWS; i++) {
    TrpFlow &f = g_trp.flows[i];
    if (!f.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    trp_mac_str(f.key.src, mac_s);
    trp_mac_str(f.key.dst, mac_d);
    s += "{\"src\":\"" + String(mac_s) + "\"";
    s += ",\"dst\":\"" + String(mac_d) + "\"";
    s += ",\"ch\":" + String(f.key.channel);
    s += ",\"pkts\":" + String(f.pkt_count);
    s += ",\"bytes\":" + String(f.byte_count);
    s += ",\"retries\":" + String(f.retry_count);
    s += ",\"pkt_rate\":" + String(f.pkt_rate);
    s += ",\"byte_rate\":" + String(f.byte_rate);
    s += ",\"rssi\":-" + String(f.avg_rssi_neg);
    s += ",\"dur\":" + String(f.last_ts - f.first_ts) + "}";
  }
  s += "]}";
  return s;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void trp_routes() {
  server.on("/api/trp/dist", HTTP_GET,
            []() { server.send(200, "application/json", trp_json_distribution()); });

  server.on("/api/trp/flows", HTTP_GET,
            []() { server.send(200, "application/json", trp_json_flows()); });

  server.on("/api/trp/clear", HTTP_POST, []() {
    memset(&g_trp, 0, sizeof(g_trp));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

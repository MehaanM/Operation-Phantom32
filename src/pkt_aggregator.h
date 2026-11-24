#pragma once
// pkt_aggregator.h — Packet-level aggregation engine: per-BSSID/STA/channel
// rolling counters with 1-second resolution and 60-second history ring.
// Prefix: pag_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define PAG_HISTORY_BUCKETS 60   // 60 × 1s = 1 minute of history
#define PAG_MAX_BSSIDS      48
#define PAG_MAX_STAS        48
#define PAG_CHANNELS        14

// ─── Per-BSSID bucket ────────────────────────────────────────────────────────
struct PagBssidBucket {
  uint32_t mgmt;
  uint32_t data;
  uint32_t ctrl;
  uint32_t bytes;
};

struct PagBssid {
  bool    active;
  uint8_t bssid[6];
  PagBssidBucket ring[PAG_HISTORY_BUCKETS];
  uint8_t  head;
  uint32_t total_frames;
  uint32_t last_ts;
};

// ─── Per-STA bucket ──────────────────────────────────────────────────────────
struct PagStaBucket {
  uint32_t tx;   // to_ds frames
  uint32_t rx;   // from_ds frames
  uint32_t bytes;
};

struct PagSta {
  bool    active;
  uint8_t mac[6];
  PagStaBucket ring[PAG_HISTORY_BUCKETS];
  uint8_t  head;
  uint32_t total_frames;
  int8_t   last_rssi;
  uint32_t last_ts;
};

// ─── Per-channel bucket ───────────────────────────────────────────────────────
struct PagChBucket {
  uint32_t frames;
  uint32_t bytes;
};

struct PagChannel {
  PagChBucket ring[PAG_HISTORY_BUCKETS];
  uint8_t  head;
  uint32_t total_frames;
};

// ─── State ────────────────────────────────────────────────────────────────────
static struct {
  PagBssid   bssids[PAG_MAX_BSSIDS];
  uint8_t    bssid_count;

  PagSta     stas[PAG_MAX_STAS];
  uint8_t    sta_count;

  PagChannel channels[PAG_CHANNELS]; // index 0 = ch1

  uint32_t   last_tick_ts;
  uint32_t   total_frames;
  uint32_t   total_bytes;
} g_pag = {};

// ─── Helpers ─────────────────────────────────────────────────────────────────
static bool pag_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static PagBssid* pag_find_or_alloc_bssid(const uint8_t *bssid) {
  for (int i = 0; i < PAG_MAX_BSSIDS; i++)
    if (g_pag.bssids[i].active && pag_mac_eq(g_pag.bssids[i].bssid, bssid))
      return &g_pag.bssids[i];
  for (int i = 0; i < PAG_MAX_BSSIDS; i++)
    if (!g_pag.bssids[i].active) {
      memset(&g_pag.bssids[i], 0, sizeof(g_pag.bssids[i]));
      g_pag.bssids[i].active = true;
      memcpy(g_pag.bssids[i].bssid, bssid, 6);
      g_pag.bssid_count++;
      return &g_pag.bssids[i];
    }
  // Evict oldest
  uint32_t oldest = UINT32_MAX;
  int idx = 0;
  for (int i = 0; i < PAG_MAX_BSSIDS; i++)
    if (g_pag.bssids[i].last_ts < oldest) { oldest = g_pag.bssids[i].last_ts; idx = i; }
  memset(&g_pag.bssids[idx], 0, sizeof(g_pag.bssids[0]));
  g_pag.bssids[idx].active = true;
  memcpy(g_pag.bssids[idx].bssid, bssid, 6);
  return &g_pag.bssids[idx];
}

static PagSta* pag_find_or_alloc_sta(const uint8_t *mac) {
  for (int i = 0; i < PAG_MAX_STAS; i++)
    if (g_pag.stas[i].active && pag_mac_eq(g_pag.stas[i].mac, mac))
      return &g_pag.stas[i];
  for (int i = 0; i < PAG_MAX_STAS; i++)
    if (!g_pag.stas[i].active) {
      memset(&g_pag.stas[i], 0, sizeof(g_pag.stas[i]));
      g_pag.stas[i].active = true;
      memcpy(g_pag.stas[i].mac, mac, 6);
      g_pag.sta_count++;
      return &g_pag.stas[i];
    }
  uint32_t oldest = UINT32_MAX;
  int idx = 0;
  for (int i = 0; i < PAG_MAX_STAS; i++)
    if (g_pag.stas[i].last_ts < oldest) { oldest = g_pag.stas[i].last_ts; idx = i; }
  memset(&g_pag.stas[idx], 0, sizeof(g_pag.stas[0]));
  g_pag.stas[idx].active = true;
  memcpy(g_pag.stas[idx].mac, mac, 6);
  return &g_pag.stas[idx];
}

// ─── Tick — advance ring buffers every second ─────────────────────────────────
void pag_tick() {
  uint32_t now_s = millis() / 1000;
  if (now_s == g_pag.last_tick_ts) return;
  g_pag.last_tick_ts = now_s;

  for (int i = 0; i < PAG_MAX_BSSIDS; i++) {
    if (!g_pag.bssids[i].active) continue;
    g_pag.bssids[i].head = (g_pag.bssids[i].head + 1) % PAG_HISTORY_BUCKETS;
    memset(&g_pag.bssids[i].ring[g_pag.bssids[i].head], 0,
           sizeof(PagBssidBucket));
  }
  for (int i = 0; i < PAG_MAX_STAS; i++) {
    if (!g_pag.stas[i].active) continue;
    g_pag.stas[i].head = (g_pag.stas[i].head + 1) % PAG_HISTORY_BUCKETS;
    memset(&g_pag.stas[i].ring[g_pag.stas[i].head], 0,
           sizeof(PagStaBucket));
  }
  for (int c = 0; c < PAG_CHANNELS; c++) {
    g_pag.channels[c].head = (g_pag.channels[c].head + 1) % PAG_HISTORY_BUCKETS;
    memset(&g_pag.channels[c].ring[g_pag.channels[c].head], 0,
           sizeof(PagChBucket));
  }
}

// ─── Ingest a frame ───────────────────────────────────────────────────────────
void pag_ingest(const uint8_t *bssid, const uint8_t *src,
                uint8_t type, bool to_ds, uint16_t frame_len,
                uint8_t channel, int8_t rssi) {
  g_pag.total_frames++;
  g_pag.total_bytes += frame_len;

  // Channel aggregation
  if (channel >= 1 && channel <= 14) {
    PagChannel &ch = g_pag.channels[channel-1];
    ch.ring[ch.head].frames++;
    ch.ring[ch.head].bytes += frame_len;
    ch.total_frames++;
  }

  // BSSID aggregation
  if (bssid) {
    PagBssid *pb = pag_find_or_alloc_bssid(bssid);
    if (pb) {
      PagBssidBucket &bkt = pb->ring[pb->head];
      if (type == 0) bkt.mgmt++;
      else if (type == 1) bkt.ctrl++;
      else bkt.data++;
      bkt.bytes += frame_len;
      pb->total_frames++;
      pb->last_ts = millis()/1000;
    }
  }

  // STA aggregation
  if (src) {
    PagSta *ps = pag_find_or_alloc_sta(src);
    if (ps) {
      PagStaBucket &bkt = ps->ring[ps->head];
      if (to_ds) bkt.tx++;
      else bkt.rx++;
      bkt.bytes += frame_len;
      ps->total_frames++;
      ps->last_rssi = rssi;
      ps->last_ts = millis()/1000;
    }
  }
}

// ─── Summary helpers ─────────────────────────────────────────────────────────
static uint32_t pag_bssid_rate(const PagBssid &b, int window_s) {
  uint32_t total = 0;
  int max_w = window_s < PAG_HISTORY_BUCKETS ? window_s : PAG_HISTORY_BUCKETS;
  for (int i = 0; i < max_w; i++) {
    int idx = (b.head - i + PAG_HISTORY_BUCKETS) % PAG_HISTORY_BUCKETS;
    total += b.ring[idx].mgmt + b.ring[idx].data + b.ring[idx].ctrl;
  }
  return max_w > 0 ? total / max_w : 0;
}

static uint32_t pag_ch_rate(int ch_idx, int window_s) {
  if (ch_idx < 0 || ch_idx >= PAG_CHANNELS) return 0;
  const PagChannel &ch = g_pag.channels[ch_idx];
  uint32_t total = 0;
  int max_w = window_s < PAG_HISTORY_BUCKETS ? window_s : PAG_HISTORY_BUCKETS;
  for (int i = 0; i < max_w; i++) {
    int idx = (ch.head - i + PAG_HISTORY_BUCKETS) % PAG_HISTORY_BUCKETS;
    total += ch.ring[idx].frames;
  }
  return max_w > 0 ? total / max_w : 0;
}

// ─── JSON ─────────────────────────────────────────────────────────────────────
static void pag_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String pag_json_channels() {
  String s = "{\"channels\":[";
  for (int c = 0; c < PAG_CHANNELS; c++) {
    if (c) s += ",";
    uint32_t rate5 = pag_ch_rate(c, 5);
    s += "{\"ch\":" + String(c+1);
    s += ",\"fps\":" + String(rate5);
    s += ",\"total\":" + String(g_pag.channels[c].total_frames) + "}";
  }
  s += "]}";
  return s;
}

static String pag_json_bssids() {
  char mac[18];
  String s = "{\"count\":" + String(g_pag.bssid_count) + ",\"bssids\":[";
  bool first = true;
  for (int i = 0; i < PAG_MAX_BSSIDS; i++) {
    PagBssid &b = g_pag.bssids[i];
    if (!b.active) continue;
    if (!first) s += ",";
    first = false;
    pag_mac_str(b.bssid, mac);
    uint32_t rate = pag_bssid_rate(b, 10);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"fps\":" + String(rate);
    s += ",\"total\":" + String(b.total_frames) + "}";
  }
  s += "]}";
  return s;
}

static String pag_json_summary() {
  String s = "{\"total_frames\":" + String(g_pag.total_frames);
  s += ",\"total_bytes\":" + String(g_pag.total_bytes);
  s += ",\"bssid_count\":" + String(g_pag.bssid_count);
  s += ",\"sta_count\":" + String(g_pag.sta_count) + "}";
  return s;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void pag_routes() {
  server.on("/api/pag/summary", HTTP_GET, []() {
    server.send(200, "application/json", pag_json_summary());
  });

  server.on("/api/pag/channels", HTTP_GET, []() {
    server.send(200, "application/json", pag_json_channels());
  });

  server.on("/api/pag/bssids", HTTP_GET, []() {
    server.send(200, "application/json", pag_json_bssids());
  });

  server.on("/api/pag/clear", HTTP_POST, []() {
    memset(&g_pag, 0, sizeof(g_pag));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

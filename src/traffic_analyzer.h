#pragma once
// traffic_analyzer.h — Per-flow traffic statistics and protocol analysis.
// Tracks active flows (src/dst MAC pairs), byte/frame counters per protocol,
// rolling 10-second rate windows, and top-talkers leaderboard. Prefix: taf_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define TAF_MAX_FLOWS 128
#define TAF_MAX_PROTOCOLS 32
#define TAF_WINDOW_SLOTS 10  // 1-second buckets for rate calculation
#define TAF_TOP_N 16

// ─── 802.11 protocol classification ──────────────────────────────────────────
enum TafProto {
  TAF_PROTO_MGMT_BEACON = 0,
  TAF_PROTO_MGMT_PROBE_REQ,
  TAF_PROTO_MGMT_PROBE_RESP,
  TAF_PROTO_MGMT_ASSOC,
  TAF_PROTO_MGMT_AUTH,
  TAF_PROTO_MGMT_DEAUTH,
  TAF_PROTO_MGMT_DISASSOC,
  TAF_PROTO_MGMT_ACTION,
  TAF_PROTO_CTRL_RTS,
  TAF_PROTO_CTRL_CTS,
  TAF_PROTO_CTRL_ACK,
  TAF_PROTO_CTRL_BLOCK_ACK,
  TAF_PROTO_DATA,
  TAF_PROTO_DATA_NULL,
  TAF_PROTO_EAPOL,
  TAF_PROTO_ARP,
  TAF_PROTO_DHCP,
  TAF_PROTO_DNS,
  TAF_PROTO_HTTP,
  TAF_PROTO_ICMP,
  TAF_PROTO_TCP,
  TAF_PROTO_UDP,
  TAF_PROTO_OTHER,
  TAF_PROTO_COUNT,
};

static const char *TAF_PROTO_NAMES[] = {
    "Beacon", "ProbeReq", "ProbeResp", "Assoc",    "Auth", "Deauth", "Disassoc", "Action",
    "RTS",    "CTS",      "ACK",       "BlockACK", "Data", "Null",   "EAPOL",    "ARP",
    "DHCP",   "DNS",      "HTTP",      "ICMP",     "TCP",  "UDP",    "Other"};

// ─── Flow record ─────────────────────────────────────────────────────────────
struct TafFlow {
  bool active;
  uint8_t src[6];
  uint8_t dst[6];
  uint8_t channel;
  uint32_t frames;
  uint32_t bytes;
  uint32_t frames_per_proto[TAF_PROTO_COUNT];
  uint32_t first_seen;
  uint32_t last_seen;
  int8_t avg_rssi;
  int32_t rssi_sum;
  uint32_t rssi_count;
  // rolling rate window
  uint32_t rate_window_frames[TAF_WINDOW_SLOTS];
  uint32_t rate_window_bytes[TAF_WINDOW_SLOTS];
  uint8_t rate_slot;
  uint32_t rate_slot_ts;
};

// ─── Global state ─────────────────────────────────────────────────────────────
static struct {
  TafFlow flows[TAF_MAX_FLOWS];
  uint32_t proto_total[TAF_PROTO_COUNT];
  uint32_t total_frames;
  uint32_t total_bytes;
  uint32_t frames_per_channel[14];
  uint32_t bytes_per_channel[14];
  uint32_t start_ts;
  uint32_t last_tick_ts;
  // aggregate rate
  uint32_t rate_frames_1s;
  uint32_t rate_bytes_1s;
  uint32_t prev_rate_ts;
  uint32_t prev_total_frames;
} g_taf = {};

// ─── Helpers ─────────────────────────────────────────────────────────────────
static bool taf_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static TafFlow *taf_find_flow(const uint8_t *src, const uint8_t *dst) {
  for (int i = 0; i < TAF_MAX_FLOWS; i++) {
    if (!g_taf.flows[i].active)
      continue;
    if (taf_mac_eq(g_taf.flows[i].src, src) && taf_mac_eq(g_taf.flows[i].dst, dst))
      return &g_taf.flows[i];
  }
  return nullptr;
}

static TafFlow *taf_alloc_flow(const uint8_t *src, const uint8_t *dst, uint8_t ch) {
  // Try to evict oldest inactive
  TafFlow *oldest = nullptr;
  uint32_t oldest_ts = 0xFFFFFFFF;
  for (int i = 0; i < TAF_MAX_FLOWS; i++) {
    if (!g_taf.flows[i].active) {
      oldest = &g_taf.flows[i];
      break;
    }
    if (g_taf.flows[i].last_seen < oldest_ts) {
      oldest_ts = g_taf.flows[i].last_seen;
      oldest = &g_taf.flows[i];
    }
  }
  if (!oldest)
    return nullptr;
  memset(oldest, 0, sizeof(TafFlow));
  memcpy(oldest->src, src, 6);
  memcpy(oldest->dst, dst, 6);
  oldest->channel = ch;
  oldest->active = true;
  oldest->first_seen = millis() / 1000;
  oldest->rate_slot_ts = millis();
  return oldest;
}

// ─── Protocol classification ─────────────────────────────────────────────────
static TafProto taf_classify(const uint8_t *raw, int len) {
  if (len < 10)
    return TAF_PROTO_OTHER;
  uint8_t fc_type = (raw[8] >> 2) & 0x03;
  uint8_t fc_sub = (raw[8] >> 4) & 0x0F;

  if (fc_type == 0) {  // Management
    switch (fc_sub) {
      case 8:
        return TAF_PROTO_MGMT_BEACON;
      case 4:
        return TAF_PROTO_MGMT_PROBE_REQ;
      case 5:
        return TAF_PROTO_MGMT_PROBE_RESP;
      case 0:
      case 1:
        return TAF_PROTO_MGMT_ASSOC;
      case 11:
        return TAF_PROTO_MGMT_AUTH;
      case 12:
        return TAF_PROTO_MGMT_DEAUTH;
      case 10:
        return TAF_PROTO_MGMT_DISASSOC;
      case 13:
        return TAF_PROTO_MGMT_ACTION;
      default:
        return TAF_PROTO_OTHER;
    }
  }
  if (fc_type == 1) {  // Control
    switch (fc_sub) {
      case 11:
        return TAF_PROTO_CTRL_RTS;
      case 12:
        return TAF_PROTO_CTRL_CTS;
      case 13:
        return TAF_PROTO_CTRL_ACK;
      case 9:
      case 8:
        return TAF_PROTO_CTRL_BLOCK_ACK;
      default:
        return TAF_PROTO_OTHER;
    }
  }
  if (fc_type == 2) {  // Data
    if (fc_sub >= 4 && fc_sub <= 7)
      return TAF_PROTO_DATA_NULL;
    if (len < 36)
      return TAF_PROTO_DATA;
    // Check EAPOL (Ethertype 0x888E at SNAP offset)
    if (len > 44 && raw[32] == 0xAA && raw[33] == 0xAA && raw[34] == 0x03) {
      if (raw[38] == 0x88 && raw[39] == 0x8E)
        return TAF_PROTO_EAPOL;
      if (raw[38] == 0x08 && raw[39] == 0x06)
        return TAF_PROTO_ARP;
      if (raw[38] == 0x08 && raw[39] == 0x00 && len > 60) {
        uint8_t proto_ip = raw[49];
        if (proto_ip == 0x01)
          return TAF_PROTO_ICMP;
        if (proto_ip == 0x06) {
          uint16_t dport = ((uint16_t)raw[64] << 8) | raw[65];
          if (dport == 80 || dport == 8080)
            return TAF_PROTO_HTTP;
          return TAF_PROTO_TCP;
        }
        if (proto_ip == 0x11) {
          uint16_t dport = ((uint16_t)raw[64] << 8) | raw[65];
          if (dport == 53)
            return TAF_PROTO_DNS;
          if (dport == 67 || dport == 68)
            return TAF_PROTO_DHCP;
          return TAF_PROTO_UDP;
        }
      }
    }
    return TAF_PROTO_DATA;
  }
  return TAF_PROTO_OTHER;
}

// ─── Ingest packet ───────────────────────────────────────────────────────────
void taf_ingest(const uint8_t *raw, int len, const uint8_t *src, const uint8_t *dst,
                uint8_t channel, int8_t rssi) {
  if (!raw || len < 4)
    return;
  TafProto proto = taf_classify(raw, len);

  g_taf.total_frames++;
  g_taf.total_bytes += len;
  g_taf.proto_total[proto]++;
  if (channel >= 1 && channel <= 14) {
    g_taf.frames_per_channel[channel - 1]++;
    g_taf.bytes_per_channel[channel - 1] += len;
  }

  TafFlow *f = taf_find_flow(src, dst);
  if (!f)
    f = taf_alloc_flow(src, dst, channel);
  if (!f)
    return;

  f->frames++;
  f->bytes += len;
  f->last_seen = millis() / 1000;
  f->frames_per_proto[proto]++;
  f->rssi_sum += rssi;
  f->rssi_count++;
  f->avg_rssi = (int8_t)(f->rssi_sum / (int32_t)f->rssi_count);

  // Rate window
  uint32_t now_ms = millis();
  uint32_t elapsed = now_ms - f->rate_slot_ts;
  if (elapsed >= 1000) {
    f->rate_slot = (f->rate_slot + 1) % TAF_WINDOW_SLOTS;
    f->rate_window_frames[f->rate_slot] = 0;
    f->rate_window_bytes[f->rate_slot] = 0;
    f->rate_slot_ts = now_ms;
  }
  f->rate_window_frames[f->rate_slot]++;
  f->rate_window_bytes[f->rate_slot] += len;
}

// ─── Rate calculation ─────────────────────────────────────────────────────────
inline void taf_tick() {
  uint32_t now = millis();
  if (now - g_taf.prev_rate_ts >= 1000) {
    g_taf.rate_frames_1s = g_taf.total_frames - g_taf.prev_total_frames;
    g_taf.prev_total_frames = g_taf.total_frames;
    g_taf.prev_rate_ts = now;
  }
}

// ─── Top-talker sort ─────────────────────────────────────────────────────────
static void taf_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static uint32_t taf_flow_rate(const TafFlow &f) {
  uint32_t sum = 0;
  for (int i = 0; i < TAF_WINDOW_SLOTS; i++)
    sum += f.rate_window_frames[i];
  return sum / TAF_WINDOW_SLOTS;
}

// ─── JSON output ─────────────────────────────────────────────────────────────
static String taf_json_summary() {
  String s = "{\"total_frames\":" + String(g_taf.total_frames);
  s += ",\"total_bytes\":" + String(g_taf.total_bytes);
  s += ",\"rate_fps\":" + String(g_taf.rate_frames_1s);
  s += ",\"active_flows\":" + String((int)[] {
         int c = 0;
         for (int i = 0; i < TAF_MAX_FLOWS; i++)
           if (g_taf.flows[i].active)
             c++;
         return c;
       }());

  s += ",\"proto\":{";
  for (int i = 0; i < TAF_PROTO_COUNT; i++) {
    if (i)
      s += ",";
    s += "\"" + String(TAF_PROTO_NAMES[i]) + "\":" + String(g_taf.proto_total[i]);
  }
  s += "},\"channels\":[";
  for (int i = 0; i < 14; i++) {
    if (i)
      s += ",";
    s += "{\"ch\":" + String(i + 1) + ",\"frames\":" + String(g_taf.frames_per_channel[i]) +
         ",\"bytes\":" + String(g_taf.bytes_per_channel[i]) + "}";
  }
  s += "]}";
  return s;
}

static String taf_json_flows(int limit = 32) {
  // Collect active flows, sorted by frames desc
  struct Ref {
    TafFlow *f;
    uint32_t frames;
  };
  Ref refs[TAF_MAX_FLOWS];
  int cnt = 0;
  for (int i = 0; i < TAF_MAX_FLOWS; i++) {
    if (g_taf.flows[i].active)
      refs[cnt++] = {&g_taf.flows[i], g_taf.flows[i].frames};
  }
  // Bubble sort top-N
  int n = min(cnt, limit);
  for (int i = 0; i < n; i++) {
    for (int j = i + 1; j < cnt; j++) {
      if (refs[j].frames > refs[i].frames) {
        Ref tmp = refs[i];
        refs[i] = refs[j];
        refs[j] = tmp;
      }
    }
  }

  String s = "{\"flows\":[";
  char mac1[18], mac2[18];
  for (int i = 0; i < n && refs[i].f; i++) {
    if (i)
      s += ",";
    TafFlow &f = *refs[i].f;
    taf_mac_str(f.src, mac1);
    taf_mac_str(f.dst, mac2);
    s += "{\"src\":\"" + String(mac1) + "\"";
    s += ",\"dst\":\"" + String(mac2) + "\"";
    s += ",\"ch\":" + String(f.channel);
    s += ",\"frames\":" + String(f.frames);
    s += ",\"bytes\":" + String(f.bytes);
    s += ",\"rssi\":" + String(f.avg_rssi);
    s += ",\"rate\":" + String(taf_flow_rate(f));
    s += ",\"last_seen\":" + String(f.last_seen);
    s += "}";
  }
  s += "]}";
  return s;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void taf_routes() {
  server.on("/api/taf/summary", HTTP_GET, []() {
    taf_tick();
    server.send(200, "application/json", taf_json_summary());
  });

  server.on("/api/taf/flows", HTTP_GET, []() {
    int lim = server.hasArg("limit") ? server.arg("limit").toInt() : 32;
    if (lim > TAF_MAX_FLOWS)
      lim = TAF_MAX_FLOWS;
    server.send(200, "application/json", taf_json_flows(lim));
  });

  server.on("/api/taf/clear", HTTP_POST, []() {
    memset(&g_taf, 0, sizeof(g_taf));
    g_taf.start_ts = millis() / 1000;
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/taf/proto", HTTP_GET, []() {
    String s = "{\"proto\":[";
    for (int i = 0; i < TAF_PROTO_COUNT; i++) {
      if (i)
        s += ",";
      s += "{\"name\":\"" + String(TAF_PROTO_NAMES[i]) +
           "\",\"count\":" + String(g_taf.proto_total[i]) + "}";
    }
    s += "]}";
    server.send(200, "application/json", s);
  });
}

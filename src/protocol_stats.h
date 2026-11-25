#pragma once
// protocol_stats.h — 802.11 protocol frame statistics engine.
// Tracks per-channel and global frame type/subtype counters.
// Prefix: prs_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

// ─── 802.11 frame type constants ─────────────────────────────────────────────
#define PRS_TYPE_MGMT 0
#define PRS_TYPE_CTRL 1
#define PRS_TYPE_DATA 2
#define PRS_MAX_SUBTYPE 16
#define PRS_MAX_CH 14

// ─── Management frame subtypes ───────────────────────────────────────────────
static const char *PRS_MGMT_NAMES[PRS_MAX_SUBTYPE] = {
    "Assoc-Req",  "Assoc-Resp", "Reassoc-Req",  "Reassoc-Resp", "Probe-Req", "Probe-Resp",
    "Timing-Adv", "Reserved",   "Beacon",       "ATIM",         "Disassoc",  "Auth",
    "Deauth",     "Action",     "Action-NoAck", "Reserved"};

static const char *PRS_CTRL_NAMES[PRS_MAX_SUBTYPE] = {
    "Reserved", "Reserved", "Reserved", "Reserved",  "BF-Rprt-Poll", "VHT/HE-NDP",
    "Ctrl-Ext", "Wrapper",  "BAR",      "BA",        "PS-Poll",      "RTS",
    "CTS",      "ACK",      "CF-End",   "CF-End+ACK"};

static const char *PRS_DATA_NAMES[PRS_MAX_SUBTYPE] = {
    "Data",           "Data+CF-ACK",     "Data+CF-Poll", "Data+CF-ACK+Poll", "Null",
    "CF-ACK",         "CF-Poll",         "CF-ACK+Poll",  "QoS-Data",         "QoS+CF-ACK",
    "QoS+CF-Poll",    "QoS+CF-ACK+Poll", "QoS-Null",     "Reserved",         "QoS+CF-Poll(no-data)",
    "QoS+CF-ACK+Poll"};

// ─── Per-channel stats ────────────────────────────────────────────────────────
struct PrsChannel {
  uint32_t type_count[3];  // mgmt / ctrl / data total
  uint32_t subtype_count[3][PRS_MAX_SUBTYPE];
  uint32_t retry_count;
  uint32_t frag_count;
  uint32_t qos_count;
  uint32_t protected_count;  // encrypted data frames
  uint32_t byte_total;
  uint32_t last_ts;
};

// ─── Global stats ─────────────────────────────────────────────────────────────
struct PrsGlobal {
  uint32_t type_count[3];
  uint32_t subtype_count[3][PRS_MAX_SUBTYPE];
  uint32_t retry_count;
  uint32_t frag_count;
  uint32_t qos_count;
  uint32_t protected_count;
  uint32_t byte_total;
  uint32_t frame_total;
};

static struct {
  PrsChannel ch[PRS_MAX_CH];
  PrsGlobal global;
  uint32_t start_ts;
} g_prs = {};

// ─── Ingest ───────────────────────────────────────────────────────────────────
void prs_ingest(uint8_t type, uint8_t subtype, uint8_t channel, bool retry, bool frag, bool qos,
                bool prot, uint16_t len) {
  if (type > 2)
    return;
  if (channel == 0 || channel > PRS_MAX_CH)
    channel = 1;
  uint8_t ch = channel - 1;
  uint8_t st = subtype & 0x0F;

  PrsChannel &c = g_prs.ch[ch];
  c.type_count[type]++;
  c.subtype_count[type][st]++;
  if (retry)
    c.retry_count++;
  if (frag)
    c.frag_count++;
  if (qos)
    c.qos_count++;
  if (prot)
    c.protected_count++;
  c.byte_total += len;
  c.last_ts = millis() / 1000;

  PrsGlobal &gl = g_prs.global;
  gl.type_count[type]++;
  gl.subtype_count[type][st]++;
  if (retry)
    gl.retry_count++;
  if (frag)
    gl.frag_count++;
  if (qos)
    gl.qos_count++;
  if (prot)
    gl.protected_count++;
  gl.byte_total += len;
  gl.frame_total++;
}

// ─── JSON builders ────────────────────────────────────────────────────────────
static String prs_json_global() {
  uint32_t total = g_prs.global.frame_total;
  if (!total)
    total = 1;

  String s = "{\"frames\":" + String(g_prs.global.frame_total);
  s += ",\"bytes\":" + String(g_prs.global.byte_total);
  s += ",\"retry\":" + String(g_prs.global.retry_count);
  s += ",\"frag\":" + String(g_prs.global.frag_count);
  s += ",\"qos\":" + String(g_prs.global.qos_count);
  s += ",\"prot\":" + String(g_prs.global.protected_count);

  s += ",\"types\":[";
  s += String(g_prs.global.type_count[0]) + ",";
  s += String(g_prs.global.type_count[1]) + ",";
  s += String(g_prs.global.type_count[2]);
  s += "]";

  // Top mgmt subtypes
  s += ",\"top_mgmt\":[";
  bool first = true;
  for (int st = 0; st < PRS_MAX_SUBTYPE; st++) {
    uint32_t v = g_prs.global.subtype_count[PRS_TYPE_MGMT][st];
    if (!v)
      continue;
    if (!first)
      s += ",";
    first = false;
    s += "{\"name\":\"" + String(PRS_MGMT_NAMES[st]) + "\"";
    s += ",\"count\":" + String(v) + "}";
  }
  s += "],\"top_data\":[";
  first = true;
  for (int st = 0; st < PRS_MAX_SUBTYPE; st++) {
    uint32_t v = g_prs.global.subtype_count[PRS_TYPE_DATA][st];
    if (!v)
      continue;
    if (!first)
      s += ",";
    first = false;
    s += "{\"name\":\"" + String(PRS_DATA_NAMES[st]) + "\"";
    s += ",\"count\":" + String(v) + "}";
  }
  s += "]}";
  return s;
}

static String prs_json_channels() {
  String s = "[";
  bool first = true;
  for (int i = 0; i < PRS_MAX_CH; i++) {
    PrsChannel &c = g_prs.ch[i];
    uint32_t total = c.type_count[0] + c.type_count[1] + c.type_count[2];
    if (!total)
      continue;
    if (!first)
      s += ",";
    first = false;
    s += "{\"ch\":" + String(i + 1);
    s += ",\"frames\":" + String(total);
    s += ",\"mgmt\":" + String(c.type_count[0]);
    s += ",\"ctrl\":" + String(c.type_count[1]);
    s += ",\"data\":" + String(c.type_count[2]);
    s += ",\"bytes\":" + String(c.byte_total);
    s += ",\"retry\":" + String(c.retry_count);
    s += "}";
  }
  s += "]";
  return s;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void prs_routes() {
  server.on("/api/prs/global", HTTP_GET,
            []() { server.send(200, "application/json", prs_json_global()); });
  server.on("/api/prs/channels", HTTP_GET,
            []() { server.send(200, "application/json", prs_json_channels()); });
  server.on("/api/prs/clear", HTTP_POST, []() {
    memset(&g_prs, 0, sizeof(g_prs));
    g_prs.start_ts = millis() / 1000;
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

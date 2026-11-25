#pragma once
// frame_inject_log.h — Injected frame activity log and statistics.
// Tracks all frames transmitted by this device via esp_wifi_80211_tx().
// Records frame type, target, channel, outcome, and transmit rate.
// Prefix: fil_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define FIL_MAX_LOG 128
#define FIL_MAX_TARGETS 32

enum FilFrameType : uint8_t {
  FIL_MGMT_PROBE_REQ = 0,
  FIL_MGMT_PROBE_RSP = 1,
  FIL_MGMT_AUTH = 2,
  FIL_MGMT_ASSOC_REQ = 3,
  FIL_MGMT_ASSOC_RSP = 4,
  FIL_MGMT_DEAUTH = 5,
  FIL_MGMT_DISASSOC = 6,
  FIL_MGMT_BEACON = 7,
  FIL_DATA_NULL = 8,
  FIL_DATA_QOS = 9,
  FIL_CTRL_RTS = 10,
  FIL_CTRL_CTS = 11,
  FIL_OTHER = 15,
};

static const char *fil_type_name(FilFrameType t) {
  switch (t) {
    case FIL_MGMT_PROBE_REQ:
      return "Probe Req";
    case FIL_MGMT_PROBE_RSP:
      return "Probe Rsp";
    case FIL_MGMT_AUTH:
      return "Auth";
    case FIL_MGMT_ASSOC_REQ:
      return "Assoc Req";
    case FIL_MGMT_ASSOC_RSP:
      return "Assoc Rsp";
    case FIL_MGMT_DEAUTH:
      return "Deauth";
    case FIL_MGMT_DISASSOC:
      return "Disassoc";
    case FIL_MGMT_BEACON:
      return "Beacon";
    case FIL_DATA_NULL:
      return "Null Data";
    case FIL_DATA_QOS:
      return "QoS Data";
    case FIL_CTRL_RTS:
      return "RTS";
    case FIL_CTRL_CTS:
      return "CTS";
    default:
      return "Other";
  }
}

struct FilEntry {
  uint32_t ts;
  FilFrameType type;
  uint8_t dst_mac[6];
  uint8_t src_mac[6];
  uint8_t bssid[6];
  uint8_t ch;
  uint16_t length;
  bool success;   // esp_wifi_80211_tx return == ESP_OK
  char note[24];  // e.g. "reason=7", "ssid=net", etc.
};

struct FilTarget {
  uint8_t mac[6];
  uint32_t frame_count;
  uint32_t last_ts;
};

static struct {
  FilEntry log[FIL_MAX_LOG];
  uint8_t log_head;
  uint32_t total_sent;
  uint32_t total_ok;
  uint32_t total_fail;

  // Per-type counters
  uint32_t type_counts[16];

  // Target list (unique dst MACs)
  FilTarget targets[FIL_MAX_TARGETS];
  uint8_t target_count;

  // Per-second rate
  uint16_t cur_rate;
  uint16_t last_rate;
  uint32_t last_tick_s;
} g_fil = {};

static bool fil_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static void fil_tick() {
  uint32_t now_s = millis() / 1000;
  if (now_s != g_fil.last_tick_s) {
    g_fil.last_rate = g_fil.cur_rate;
    g_fil.cur_rate = 0;
    g_fil.last_tick_s = now_s;
  }
}

static void fil_track_target(const uint8_t *mac) {
  for (int i = 0; i < g_fil.target_count; i++) {
    if (fil_mac_eq(g_fil.targets[i].mac, mac)) {
      g_fil.targets[i].frame_count++;
      g_fil.targets[i].last_ts = millis() / 1000;
      return;
    }
  }
  if (g_fil.target_count < FIL_MAX_TARGETS) {
    FilTarget &t = g_fil.targets[g_fil.target_count++];
    memcpy(t.mac, mac, 6);
    t.frame_count = 1;
    t.last_ts = millis() / 1000;
  }
}

void fil_log(FilFrameType type, const uint8_t *dst_mac, const uint8_t *src_mac,
             const uint8_t *bssid, uint8_t ch, uint16_t length, bool success, const char *note) {
  fil_tick();

  FilEntry &e = g_fil.log[g_fil.log_head % FIL_MAX_LOG];
  e.ts = millis() / 1000;
  e.type = type;
  memcpy(e.dst_mac, dst_mac, 6);
  memcpy(e.src_mac, src_mac, 6);
  memcpy(e.bssid, bssid, 6);
  e.ch = ch;
  e.length = length;
  e.success = success;
  if (note)
    strncpy(e.note, note, 23);
  else
    e.note[0] = 0;
  g_fil.log_head = (g_fil.log_head + 1) % FIL_MAX_LOG;

  g_fil.total_sent++;
  if (success)
    g_fil.total_ok++;
  else
    g_fil.total_fail++;
  g_fil.cur_rate++;

  if ((uint8_t)type < 16)
    g_fil.type_counts[(uint8_t)type]++;
  fil_track_target(dst_mac);
}

static void fil_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String fil_json_all() {
  String s = "{\"total_sent\":" + String(g_fil.total_sent);
  s += ",\"total_ok\":" + String(g_fil.total_ok);
  s += ",\"total_fail\":" + String(g_fil.total_fail);
  s += ",\"rate_per_sec\":" + String(g_fil.last_rate);

  s += ",\"by_type\":[";
  for (int i = 0; i < 16; i++) {
    if (i)
      s += ",";
    s += "{\"type\":\"" + String(fil_type_name((FilFrameType)i)) + "\"";
    s += ",\"count\":" + String(g_fil.type_counts[i]) + "}";
  }
  s += "]";

  s += ",\"targets\":[";
  char mac[18];
  bool first = true;
  for (int i = 0; i < g_fil.target_count; i++) {
    if (!first)
      s += ",";
    first = false;
    fil_mac_str(g_fil.targets[i].mac, mac);
    s += "{\"mac\":\"" + String(mac) + "\"";
    s += ",\"count\":" + String(g_fil.targets[i].frame_count) + "}";
  }
  s += "]";

  s += ",\"log\":[";
  bool fl = true;
  for (int i = 0; i < FIL_MAX_LOG; i++) {
    int idx = (g_fil.log_head - 1 - i + FIL_MAX_LOG) % FIL_MAX_LOG;
    FilEntry &e = g_fil.log[idx];
    if (e.ts == 0)
      continue;
    if (!fl)
      s += ",";
    fl = false;
    char dmac[18], smac[18], bmac[18];
    fil_mac_str(e.dst_mac, dmac);
    fil_mac_str(e.src_mac, smac);
    fil_mac_str(e.bssid, bmac);
    s += "{\"ts\":" + String(e.ts);
    s += ",\"type\":\"" + String(fil_type_name(e.type)) + "\"";
    s += ",\"dst\":\"" + String(dmac) + "\"";
    s += ",\"src\":\"" + String(smac) + "\"";
    s += ",\"bssid\":\"" + String(bmac) + "\"";
    s += ",\"ch\":" + String(e.ch);
    s += ",\"len\":" + String(e.length);
    s += ",\"ok\":" + String(e.success ? "true" : "false");
    s += ",\"note\":\"" + String(e.note) + "\"}";
    if (i >= 63)
      break;  // limit to 64 entries in JSON
  }
  s += "]}";
  return s;
}

inline void fil_routes() {
  server.on("/api/fil/all", HTTP_GET,
            []() { server.send(200, "application/json", fil_json_all()); });
  server.on("/api/fil/clear", HTTP_POST, []() {
    memset(&g_fil, 0, sizeof(g_fil));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

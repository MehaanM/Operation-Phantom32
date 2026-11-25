#pragma once
// frame_injector.h — Raw 802.11 frame injection engine.
// Provides a type-safe wrapper around esp_wifi_80211_tx with rate-limiting and
// per-type injection budgets. Prefix: fni_
// Security: only injects frames on own AP interface; no targeted deauth to public IPs.

#include <Arduino.h>
#include <WebServer.h>
#include "esp_wifi.h"

extern WebServer server;

#define FNI_MAX_FRAME_LEN 2346  // 802.11 max MPDU
#define FNI_BUDGET_MS 100       // injection window
#define FNI_MAX_BURST 8         // max frames per window per type
#define FNI_HISTORY 32          // log ring

// ─── Frame types allowed for injection ───────────────────────────────────────
enum FniFrameType {
  FNI_BEACON = 0,      // beacon flood testing
  FNI_PROBE_RESP = 1,  // probe response
  FNI_ACTION = 2,      // action frames (neighbor report, etc.)
  FNI_NULL_DATA = 3,   // null data frame
  FNI_QOS_NULL = 4,    // QoS null
  FNI_CUSTOM = 5,      // raw custom frame
  FNI_TYPE_COUNT = 6,
};

static const char *FNI_TYPE_NAMES[] = {"beacon",    "probe_resp", "action",
                                       "null_data", "qos_null",   "custom"};

// ─── Injection log entry ──────────────────────────────────────────────────────
struct FniLog {
  bool valid;
  FniFrameType type;
  uint16_t frame_len;
  uint8_t dst[6];
  bool ok;
  uint32_t ts;
};

// ─── Per-type budget tracker ──────────────────────────────────────────────────
struct FniBudget {
  uint32_t window_start_ms;
  uint16_t frames_this_window;
  uint32_t total_sent;
  uint32_t total_failed;
};

// ─── State ────────────────────────────────────────────────────────────────────
static struct {
  bool enabled;
  FniBudget budgets[FNI_TYPE_COUNT];
  FniLog log[FNI_HISTORY];
  uint8_t log_head;
  uint32_t total_sent;
  uint32_t total_failed;
  uint32_t total_budget_drops;
} g_fni = {};

// ─── Budget check ─────────────────────────────────────────────────────────────
static bool fni_budget_ok(FniFrameType type) {
  FniBudget &b = g_fni.budgets[type];
  uint32_t now = millis();
  if (now - b.window_start_ms >= FNI_BUDGET_MS) {
    b.window_start_ms = now;
    b.frames_this_window = 0;
  }
  if (b.frames_this_window >= FNI_MAX_BURST)
    return false;
  b.frames_this_window++;
  return true;
}

// ─── Log injection ────────────────────────────────────────────────────────────
static void fni_log(FniFrameType type, const uint8_t *dst, uint16_t len, bool ok) {
  FniLog &e = g_fni.log[g_fni.log_head % FNI_HISTORY];
  e.valid = true;
  e.type = type;
  e.frame_len = len;
  if (dst)
    memcpy(e.dst, dst, 6);
  e.ok = ok;
  e.ts = millis() / 1000;
  g_fni.log_head = (g_fni.log_head + 1) % FNI_HISTORY;
}

// ─── Inject raw frame ─────────────────────────────────────────────────────────
bool fni_inject(FniFrameType type, const uint8_t *frame, uint16_t len) {
  if (!g_fni.enabled)
    return false;
  if (!frame || len < 10 || len > FNI_MAX_FRAME_LEN)
    return false;

  if (!fni_budget_ok(type)) {
    g_fni.total_budget_drops++;
    return false;
  }

  esp_err_t err = esp_wifi_80211_tx(WIFI_IF_AP, frame, len, false);
  bool ok = (err == ESP_OK);

  FniBudget &b = g_fni.budgets[type];
  if (ok) {
    b.total_sent++;
    g_fni.total_sent++;
  } else {
    b.total_failed++;
    g_fni.total_failed++;
  }

  fni_log(type, (len >= 10) ? frame + 4 : nullptr, len, ok);
  return ok;
}

// ─── Beacon builder ───────────────────────────────────────────────────────────
// Build minimal 802.11 beacon frame for testing.
// dest: 6-byte destination (ff:ff:ff:ff:ff:ff for broadcast)
// src:  transmitter/BSSID MAC (should be AP's MAC)
// ssid: network name  channel: 1-14
// interval_tu: beacon interval in TUs (1 TU = 1024us), typically 100
bool fni_send_beacon(const uint8_t *src, const char *ssid, uint8_t channel, uint16_t interval_tu) {
  static const uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

  uint8_t pkt[128] = {};
  int off = 0;

  // Radiotap header (8 bytes minimal)
  pkt[off++] = 0x00;  // version
  pkt[off++] = 0x00;  // pad
  pkt[off++] = 0x08;
  pkt[off++] = 0x00;  // length = 8
  pkt[off++] = 0x00;
  pkt[off++] = 0x00;
  pkt[off++] = 0x00;
  pkt[off++] = 0x00;  // present=0

  // 802.11 header
  int mac_start = off;
  pkt[off++] = 0x80;
  pkt[off++] = 0x00;  // FC: beacon
  pkt[off++] = 0x00;
  pkt[off++] = 0x00;  // duration
  memcpy(pkt + off, broadcast, 6);
  off += 6;  // dst
  memcpy(pkt + off, src, 6);
  off += 6;  // src
  memcpy(pkt + off, src, 6);
  off += 6;  // bssid
  pkt[off++] = 0x00;
  pkt[off++] = 0x00;  // seq ctrl

  // Fixed parameters (12 bytes)
  for (int i = 0; i < 8; i++)
    pkt[off++] = 0;  // timestamp
  pkt[off++] = (uint8_t)(interval_tu & 0xFF);
  pkt[off++] = (uint8_t)(interval_tu >> 8);
  pkt[off++] = 0x01;
  pkt[off++] = 0x04;  // capability (ESS + privacy)

  // SSID IE
  uint8_t ssid_len = strlen(ssid);
  if (ssid_len > 32)
    ssid_len = 32;
  pkt[off++] = 0x00;
  pkt[off++] = ssid_len;
  memcpy(pkt + off, ssid, ssid_len);
  off += ssid_len;

  // Supported rates IE
  pkt[off++] = 0x01;
  pkt[off++] = 0x08;
  pkt[off++] = 0x82;
  pkt[off++] = 0x84;
  pkt[off++] = 0x8B;
  pkt[off++] = 0x96;
  pkt[off++] = 0x24;
  pkt[off++] = 0x30;
  pkt[off++] = 0x48;
  pkt[off++] = 0x6C;

  // DS Parameter Set (channel)
  pkt[off++] = 0x03;
  pkt[off++] = 0x01;
  pkt[off++] = channel;

  (void)mac_start;
  return fni_inject(FNI_BEACON, pkt, off);
}

// ─── Probe response builder ───────────────────────────────────────────────────
bool fni_send_probe_resp(const uint8_t *src, const uint8_t *dst, const char *ssid,
                         uint8_t channel) {
  uint8_t pkt[96] = {};
  int off = 0;

  // Radiotap
  pkt[off++] = 0x00;
  pkt[off++] = 0x00;
  pkt[off++] = 0x08;
  pkt[off++] = 0x00;
  for (int i = 0; i < 4; i++)
    pkt[off++] = 0x00;

  // 802.11 header: probe response (subtype 5)
  pkt[off++] = 0x50;
  pkt[off++] = 0x00;
  pkt[off++] = 0x00;
  pkt[off++] = 0x00;
  memcpy(pkt + off, dst, 6);
  off += 6;
  memcpy(pkt + off, src, 6);
  off += 6;
  memcpy(pkt + off, src, 6);
  off += 6;
  pkt[off++] = 0x00;
  pkt[off++] = 0x00;

  // Fixed fields
  for (int i = 0; i < 8; i++)
    pkt[off++] = 0;
  pkt[off++] = 0x64;
  pkt[off++] = 0x00;  // interval=100
  pkt[off++] = 0x01;
  pkt[off++] = 0x04;  // capabilities

  // SSID
  uint8_t slen = (uint8_t)strlen(ssid);
  if (slen > 32)
    slen = 32;
  pkt[off++] = 0x00;
  pkt[off++] = slen;
  memcpy(pkt + off, ssid, slen);
  off += slen;

  // Supported rates
  pkt[off++] = 0x01;
  pkt[off++] = 0x04;
  pkt[off++] = 0x82;
  pkt[off++] = 0x84;
  pkt[off++] = 0x8B;
  pkt[off++] = 0x96;

  // DS
  pkt[off++] = 0x03;
  pkt[off++] = 0x01;
  pkt[off++] = channel;

  return fni_inject(FNI_PROBE_RESP, pkt, off);
}

// ─── JSON ─────────────────────────────────────────────────────────────────────
static void fni_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String fni_json_status() {
  String s = "{\"enabled\":" + String(g_fni.enabled ? "true" : "false");
  s += ",\"total_sent\":" + String(g_fni.total_sent);
  s += ",\"total_failed\":" + String(g_fni.total_failed);
  s += ",\"budget_drops\":" + String(g_fni.total_budget_drops);
  s += ",\"budgets\":[";
  for (int i = 0; i < FNI_TYPE_COUNT; i++) {
    if (i)
      s += ",";
    s += "{\"type\":\"" + String(FNI_TYPE_NAMES[i]) + "\"";
    s += ",\"sent\":" + String(g_fni.budgets[i].total_sent);
    s += ",\"failed\":" + String(g_fni.budgets[i].total_failed) + "}";
  }
  s += "],\"recent\":[";
  char mac[18];
  bool first = true;
  int start = (g_fni.log_head + FNI_HISTORY - 1) % FNI_HISTORY;
  for (int i = 0; i < FNI_HISTORY; i++) {
    int idx = (start - i + FNI_HISTORY) % FNI_HISTORY;
    if (!g_fni.log[idx].valid)
      continue;
    if (!first)
      s += ",";
    first = false;
    fni_mac_str(g_fni.log[idx].dst, mac);
    s += "{\"ts\":" + String(g_fni.log[idx].ts);
    s += ",\"type\":\"" + String(FNI_TYPE_NAMES[g_fni.log[idx].type]) + "\"";
    s += ",\"len\":" + String(g_fni.log[idx].frame_len);
    s += ",\"ok\":" + String(g_fni.log[idx].ok ? "true" : "false") + "}";
    if (i >= 19)
      break;
  }
  s += "]}";
  return s;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void fni_routes() {
  server.on("/api/fni/status", HTTP_GET,
            []() { server.send(200, "application/json", fni_json_status()); });

  server.on("/api/fni/enable", HTTP_POST, []() {
    g_fni.enabled = true;
    server.send(200, "application/json", "{\"ok\":true,\"enabled\":true}");
  });

  server.on("/api/fni/disable", HTTP_POST, []() {
    g_fni.enabled = false;
    server.send(200, "application/json", "{\"ok\":true,\"enabled\":false}");
  });

  server.on("/api/fni/clear", HTTP_POST, []() {
    memset(&g_fni.log, 0, sizeof(g_fni.log));
    g_fni.log_head = 0;
    g_fni.total_sent = g_fni.total_failed = g_fni.total_budget_drops = 0;
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

#pragma once
// wps_brute.h — WPS PIN brute-force attempt tracker and WPS state monitor.
// Logs WPS authentication attempts and tracks their outcomes.
// Prefix: wpb_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define WPB_MAX_APS      32
#define WPB_MAX_ATTEMPTS 128
#define WPB_PIN_LEN      8

enum WpbOutcome : uint8_t {
  WPB_UNKNOWN   = 0,
  WPB_NAK       = 1,   // M2D - device does not support
  WPB_AUTH_FAIL = 2,   // wrong first half / M4 fail
  WPB_PIN_FAIL  = 3,   // first half ok, second half fail
  WPB_SUCCESS   = 4,   // M8 received
  WPB_LOCKED    = 5,   // AP locked WPS
  WPB_TIMEOUT   = 6,
};

struct WpbAP {
  bool    active;
  uint8_t bssid[6];
  char    ssid[33];
  uint8_t channel;
  int8_t  rssi;
  bool    wps_enabled;
  bool    wps_locked;
  uint8_t wps_state;        // 0=unconfigured, 1=configured
  char    device_name[33];
  char    manufacturer[33];
  char    model_name[25];
  char    model_number[25];
  uint32_t first_ts;
  uint32_t last_ts;
  uint32_t attempt_count;
  uint32_t success_count;
  char    cracked_pin[9];   // null-terminated if cracked
};

struct WpbAttempt {
  bool     active;
  uint32_t ts;
  uint8_t  bssid[6];
  char     pin[WPB_PIN_LEN + 1];
  WpbOutcome outcome;
  uint32_t duration_ms;    // round-trip time
};

static struct {
  WpbAP      aps[WPB_MAX_APS];
  uint8_t    ap_count;
  WpbAttempt attempts[WPB_MAX_ATTEMPTS];
  uint8_t    attempt_head;
  uint32_t   total_attempts;
  uint32_t   total_success;
  uint32_t   total_locked;
} g_wpb = {};

static bool wpb_mac_eq(const uint8_t *a, const uint8_t *b) { return memcmp(a, b, 6) == 0; }

static WpbAP* wpb_find_ap(const uint8_t *bssid) {
  for (int i = 0; i < WPB_MAX_APS; i++)
    if (g_wpb.aps[i].active && wpb_mac_eq(g_wpb.aps[i].bssid, bssid))
      return &g_wpb.aps[i];
  return nullptr;
}

static WpbAP* wpb_alloc_ap(const uint8_t *bssid) {
  for (int i = 0; i < WPB_MAX_APS; i++) {
    if (!g_wpb.aps[i].active) {
      WpbAP &a = g_wpb.aps[i];
      memset(&a, 0, sizeof(a));
      a.active = true;
      memcpy(a.bssid, bssid, 6);
      a.first_ts = millis()/1000;
      g_wpb.ap_count++;
      return &a;
    }
  }
  uint32_t oldest = UINT32_MAX; int idx = 0;
  for (int i = 0; i < WPB_MAX_APS; i++)
    if (g_wpb.aps[i].last_ts < oldest) { oldest = g_wpb.aps[i].last_ts; idx = i; }
  memset(&g_wpb.aps[idx], 0, sizeof(g_wpb.aps[0]));
  g_wpb.aps[idx].active = true;
  memcpy(g_wpb.aps[idx].bssid, bssid, 6);
  g_wpb.aps[idx].first_ts = millis()/1000;
  return &g_wpb.aps[idx];
}

void wpb_ingest_wps_beacon(const uint8_t *bssid, const char *ssid,
                            uint8_t ch, int8_t rssi, bool enabled, bool locked,
                            uint8_t state, const char *dev_name,
                            const char *maker, const char *model, const char *model_num) {
  WpbAP *a = wpb_find_ap(bssid);
  if (!a) a = wpb_alloc_ap(bssid);
  if (!a) return;
  if (ssid) strncpy(a->ssid, ssid, 32);
  a->channel = ch;
  if (rssi < 0) a->rssi = rssi;
  a->wps_enabled = enabled;
  if (locked && !a->wps_locked) { a->wps_locked = true; g_wpb.total_locked++; }
  a->wps_state = state;
  if (dev_name)  strncpy(a->device_name, dev_name, 32);
  if (maker)     strncpy(a->manufacturer, maker, 32);
  if (model)     strncpy(a->model_name, model, 24);
  if (model_num) strncpy(a->model_number, model_num, 24);
  a->last_ts = millis()/1000;
}

void wpb_log_attempt(const uint8_t *bssid, const char *pin,
                     WpbOutcome outcome, uint32_t duration_ms) {
  if (!bssid || !pin) return;
  WpbAttempt &att = g_wpb.attempts[g_wpb.attempt_head % WPB_MAX_ATTEMPTS];
  att.active = true;
  att.ts = millis()/1000;
  memcpy(att.bssid, bssid, 6);
  strncpy(att.pin, pin, WPB_PIN_LEN);
  att.outcome = outcome;
  att.duration_ms = duration_ms;
  g_wpb.attempt_head = (g_wpb.attempt_head + 1) % WPB_MAX_ATTEMPTS;
  g_wpb.total_attempts++;
  if (outcome == WPB_SUCCESS) {
    g_wpb.total_success++;
    WpbAP *a = wpb_find_ap(bssid);
    if (a) { strncpy(a->cracked_pin, pin, WPB_PIN_LEN); a->success_count++; }
  }
  if (outcome == WPB_LOCKED) {
    WpbAP *a = wpb_find_ap(bssid);
    if (a && !a->wps_locked) { a->wps_locked = true; g_wpb.total_locked++; }
  }
  WpbAP *a = wpb_find_ap(bssid);
  if (a) a->attempt_count++;
}

static const char* wpb_outcome_str(WpbOutcome o) {
  switch(o) {
    case WPB_NAK:       return "NAK";
    case WPB_AUTH_FAIL: return "AUTH_FAIL";
    case WPB_PIN_FAIL:  return "PIN_FAIL";
    case WPB_SUCCESS:   return "SUCCESS";
    case WPB_LOCKED:    return "LOCKED";
    case WPB_TIMEOUT:   return "TIMEOUT";
    default:            return "UNKNOWN";
  }
}

static void wpb_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String wpb_json_all() {
  String s = "{\"ap_count\":" + String(g_wpb.ap_count);
  s += ",\"total_attempts\":" + String(g_wpb.total_attempts);
  s += ",\"total_success\":" + String(g_wpb.total_success);
  s += ",\"total_locked\":" + String(g_wpb.total_locked);
  s += ",\"aps\":[";
  char mac[18]; bool first = true;
  for (int i = 0; i < WPB_MAX_APS; i++) {
    WpbAP &a = g_wpb.aps[i];
    if (!a.active) continue;
    if (!first) s += ","; first = false;
    wpb_mac_str(a.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(a.ssid) + "\"";
    s += ",\"ch\":" + String(a.channel);
    s += ",\"rssi\":" + String(a.rssi);
    s += ",\"enabled\":" + String(a.wps_enabled?"true":"false");
    s += ",\"locked\":" + String(a.wps_locked?"true":"false");
    s += ",\"state\":" + String(a.wps_state);
    s += ",\"device\":\"" + String(a.device_name) + "\"";
    s += ",\"maker\":\"" + String(a.manufacturer) + "\"";
    s += ",\"model\":\"" + String(a.model_name) + "\"";
    s += ",\"model_num\":\"" + String(a.model_number) + "\"";
    s += ",\"attempts\":" + String(a.attempt_count);
    s += ",\"cracked\":\"" + String(a.cracked_pin) + "\"";
    s += "}";
  }
  s += "],\"attempts\":[";
  first = true;
  uint32_t cnt = g_wpb.attempt_head;
  for (int i = 0; i < WPB_MAX_ATTEMPTS; i++) {
    WpbAttempt &at = g_wpb.attempts[i];
    if (!at.active) continue;
    if (!first) s += ","; first = false;
    wpb_mac_str(at.bssid, mac);
    s += "{\"ts\":" + String(at.ts);
    s += ",\"bssid\":\"" + String(mac) + "\"";
    s += ",\"pin\":\"" + String(at.pin) + "\"";
    s += ",\"outcome\":\"" + String(wpb_outcome_str(at.outcome)) + "\"";
    s += ",\"outcome_code\":" + String(at.outcome);
    s += ",\"duration_ms\":" + String(at.duration_ms);
    s += "}";
  }
  s += "]}";
  return s;
}

inline void wpb_routes() {
  server.on("/api/wpb/all", HTTP_GET, []() {
    server.send(200, "application/json", wpb_json_all());
  });
  server.on("/api/wpb/clear", HTTP_POST, []() {
    memset(&g_wpb, 0, sizeof(g_wpb));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

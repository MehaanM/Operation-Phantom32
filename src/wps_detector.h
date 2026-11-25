#pragma once
// wps_detector.h — Wi-Fi Protected Setup (WPS) detector and analyzer.
// Detects WPS-enabled APs, tracks WPS pushbutton and PIN sessions,
// and flags WPS lock states and Pixie Dust attack indicators.
// Prefix: wps_

#include <Arduino.h>
#include <WebServer.h>
#include "psram_attr.h"

extern WebServer server;

#define WPS_MAX_APS 40
#define WPS_MAX_SESSIONS 24
#define WPS_HIST_LEN 20

// WPS state
enum WpsState : uint8_t {
  WPS_STATE_UNCONFIGURED = 0,
  WPS_STATE_CONFIGURED = 1,
};

// WPS configuration method flags
enum WpsMethod : uint8_t {
  WPS_METHOD_NONE = 0,
  WPS_METHOD_PBC = 1,  // Pushbutton (broadcast)
  WPS_METHOD_PIN = 2,  // PIN method
  WPS_METHOD_NFC = 4,  // NFC
  WPS_METHOD_USB = 8,  // USB
};

// WPS version
enum WpsVersion : uint8_t {
  WPS_VER_UNKNOWN = 0,
  WPS_VER_1_0 = 10,
  WPS_VER_2_0 = 20,
};

// Known vulnerable device models by OUI+device string
struct WpsVulnPattern {
  const char *vendor;
  const char *device;
  bool pixie_vulnerable;
};

static const WpsVulnPattern WPS_VULN_PATTERNS[] = {
    {"Ralink", "", true},       {"Realtek", "", true}, {"Broadcom", "BCM", false},
    {"TP-Link", "TL-WR", true}, {"D-Link", "", true},  {"Netgear", "WNR", true},
    {"Belkin", "", false},      {"Linksys", "", true},
};

struct WpsAP {
  bool active;
  uint8_t bssid[6];
  char ssid[33];
  uint8_t ch;
  int8_t rssi;

  // WPS attributes
  WpsState state;
  WpsMethod methods;
  WpsVersion version;
  char device_name[33];
  char manufacturer[33];
  char model_name[33];
  char model_number[17];
  char serial_number[17];
  uint8_t uuid[16];

  // Security assessment
  bool pbc_active;          // PBC session in progress
  bool pin_locked;          // AP locked due to failed PIN attempts
  bool pixie_suspect;       // Chip vulnerable to Pixie Dust
  uint8_t failed_attempts;  // PIN failures seen
  bool ap_setup_locked;     // AP lockout flag in IE

  // Activity counters
  uint16_t beacon_count;
  uint16_t probe_resp_count;
  uint16_t pbc_session_count;

  // History: beacons per tick
  uint8_t hist[WPS_HIST_LEN];
  uint8_t hist_head;
  uint32_t last_tick_s;
  uint16_t cur_beacons;

  uint32_t first_ts;
  uint32_t last_ts;
};

struct WpsSession {
  bool active;
  uint8_t client_mac[6];
  uint8_t ap_bssid[6];
  char ap_ssid[33];
  WpsMethod method;
  uint8_t msg_num;  // M1-M8 exchange progress
  bool completed;
  bool failed;
  uint16_t duration_ms;
  uint32_t start_ts;
  uint32_t last_ts;
};

typedef struct {
  WpsAP aps[WPS_MAX_APS];
  uint8_t ap_count;
  WpsSession sessions[WPS_MAX_SESSIONS];
  uint8_t sess_count;
  uint32_t total_pbc_sessions;
  uint32_t total_pin_sessions;
  uint32_t total_locked_aps;
} G_WpsDetState;
#ifdef BOARD_HAS_PSRAM
static G_WpsDetState *_g_wps_raw;
#define g_wps (*_g_wps_raw)
#else
static G_WpsDetState g_wps;
#endif

static bool wps_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static WpsAP *wps_find_ap(const uint8_t *bssid) {
  for (int i = 0; i < WPS_MAX_APS; i++)
    if (g_wps.aps[i].active && wps_mac_eq(g_wps.aps[i].bssid, bssid))
      return &g_wps.aps[i];
  return nullptr;
}

static WpsAP *wps_alloc_ap(const uint8_t *bssid) {
  for (int i = 0; i < WPS_MAX_APS; i++) {
    if (!g_wps.aps[i].active) {
      WpsAP &a = g_wps.aps[i];
      memset(&a, 0, sizeof(a));
      a.active = true;
      memcpy(a.bssid, bssid, 6);
      a.first_ts = millis() / 1000;
      g_wps.ap_count++;
      return &a;
    }
  }
  uint32_t oldest = UINT32_MAX;
  int idx = 0;
  for (int i = 0; i < WPS_MAX_APS; i++)
    if (g_wps.aps[i].last_ts < oldest) {
      oldest = g_wps.aps[i].last_ts;
      idx = i;
    }
  memset(&g_wps.aps[idx], 0, sizeof(WpsAP));
  g_wps.aps[idx].active = true;
  memcpy(g_wps.aps[idx].bssid, bssid, 6);
  g_wps.aps[idx].first_ts = millis() / 1000;
  return &g_wps.aps[idx];
}

static void wps_check_pixie(WpsAP &a) {
  for (auto &p : WPS_VULN_PATTERNS) {
    if (strstr(a.manufacturer, p.vendor) || strstr(a.device_name, p.vendor)) {
      if (p.pixie_vulnerable) {
        a.pixie_suspect = true;
        break;
      }
    }
  }
}

static void wps_tick(WpsAP &a) {
  uint32_t now_s = millis() / 1000;
  if (now_s == a.last_tick_s)
    return;
  a.hist[a.hist_head % WPS_HIST_LEN] = (uint8_t)(a.cur_beacons > 255 ? 255 : a.cur_beacons);
  a.hist_head = (a.hist_head + 1) % WPS_HIST_LEN;
  a.cur_beacons = 0;
  a.last_tick_s = now_s;
}

void wps_ingest_beacon(const uint8_t *bssid, const char *ssid, uint8_t ch, int8_t rssi,
                       WpsState state, WpsMethod methods, WpsVersion ver, const char *device_name,
                       const char *manufacturer, const char *model_name, const char *model_num,
                       bool pbc_active, bool ap_setup_locked) {
  WpsAP *a = wps_find_ap(bssid);
  if (!a)
    a = wps_alloc_ap(bssid);
  if (!a)
    return;

  wps_tick(*a);
  if (ssid)
    strncpy(a->ssid, ssid, 32);
  a->ch = ch;
  a->rssi = rssi;
  a->state = state;
  a->methods = methods;
  a->version = ver;
  if (device_name && device_name[0])
    strncpy(a->device_name, device_name, 32);
  if (manufacturer && manufacturer[0])
    strncpy(a->manufacturer, manufacturer, 32);
  if (model_name && model_name[0])
    strncpy(a->model_name, model_name, 32);
  if (model_num && model_num[0])
    strncpy(a->model_number, model_num, 16);

  bool was_pbc = a->pbc_active;
  a->pbc_active = pbc_active;
  if (pbc_active && !was_pbc) {
    a->pbc_session_count++;
    g_wps.total_pbc_sessions++;
  }

  bool was_locked = a->ap_setup_locked;
  a->ap_setup_locked = ap_setup_locked;
  if (ap_setup_locked && !was_locked)
    g_wps.total_locked_aps++;

  a->beacon_count++;
  a->cur_beacons++;
  wps_check_pixie(*a);
  a->last_ts = millis() / 1000;
}

void wps_ingest_eap_msg(const uint8_t *client, const uint8_t *ap, const char *ssid,
                        WpsMethod method, uint8_t msg_num, bool completed, bool failed) {
  WpsSession *s = nullptr;
  for (int i = 0; i < WPS_MAX_SESSIONS; i++) {
    if (g_wps.sessions[i].active && wps_mac_eq(g_wps.sessions[i].client_mac, client) &&
        wps_mac_eq(g_wps.sessions[i].ap_bssid, ap)) {
      s = &g_wps.sessions[i];
      break;
    }
  }
  if (!s) {
    for (int i = 0; i < WPS_MAX_SESSIONS; i++) {
      if (!g_wps.sessions[i].active) {
        s = &g_wps.sessions[i];
        memset(s, 0, sizeof(WpsSession));
        s->active = true;
        memcpy(s->client_mac, client, 6);
        memcpy(s->ap_bssid, ap, 6);
        if (ssid)
          strncpy(s->ap_ssid, ssid, 32);
        s->method = method;
        s->start_ts = millis() / 1000;
        g_wps.sess_count++;
        if (method == WPS_METHOD_PIN)
          g_wps.total_pin_sessions++;
        break;
      }
    }
  }
  if (!s)
    return;

  s->msg_num = msg_num;
  s->completed = completed;
  s->failed = failed;
  s->duration_ms = (millis() - s->start_ts * 1000);
  s->last_ts = millis() / 1000;

  // Update AP PIN lock status
  if (failed) {
    WpsAP *a = wps_find_ap(ap);
    if (a) {
      a->failed_attempts++;
      if (a->failed_attempts >= 3)
        a->pin_locked = true;
    }
  }
}

static void wps_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String wps_hist_json(const uint8_t *h, int len, int head) {
  String s = "[";
  for (int i = 0; i < len; i++) {
    if (i)
      s += ",";
    s += String(h[(head + i) % len]);
  }
  return s + "]";
}

static const char *wps_ver_str(WpsVersion v) {
  if (v == WPS_VER_1_0)
    return "1.0";
  if (v == WPS_VER_2_0)
    return "2.0";
  return "?";
}

static String wps_json_all() {
  char mac[18];
  String s = "{\"ap_count\":" + String(g_wps.ap_count);
  s += ",\"sess_count\":" + String(g_wps.sess_count);
  s += ",\"total_pbc\":" + String(g_wps.total_pbc_sessions);
  s += ",\"total_pin\":" + String(g_wps.total_pin_sessions);
  s += ",\"total_locked\":" + String(g_wps.total_locked_aps);

  s += ",\"aps\":[";
  bool first = true;
  for (int i = 0; i < WPS_MAX_APS; i++) {
    WpsAP &a = g_wps.aps[i];
    if (!a.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    wps_mac_str(a.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(a.ssid) + "\"";
    s += ",\"ch\":" + String(a.ch);
    s += ",\"rssi\":" + String(a.rssi);
    s += ",\"state\":" + String(a.state);
    s += ",\"version\":\"" + String(wps_ver_str(a.version)) + "\"";
    s += ",\"methods\":" + String(a.methods);
    s += ",\"pbc\":" + String(a.pbc_active ? "true" : "false");
    s += ",\"locked\":" + String(a.ap_setup_locked ? "true" : "false");
    s += ",\"pin_locked\":" + String(a.pin_locked ? "true" : "false");
    s += ",\"pixie\":" + String(a.pixie_suspect ? "true" : "false");
    s += ",\"device\":\"" + String(a.device_name) + "\"";
    s += ",\"mfr\":\"" + String(a.manufacturer) + "\"";
    s += ",\"model\":\"" + String(a.model_name) + "\"";
    s += ",\"hist\":" + wps_hist_json(a.hist, WPS_HIST_LEN, a.hist_head) + "}";
  }

  s += "],\"sessions\":[";
  first = true;
  for (int i = 0; i < WPS_MAX_SESSIONS; i++) {
    WpsSession &ss = g_wps.sessions[i];
    if (!ss.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    wps_mac_str(ss.client_mac, mac);
    s += "{\"mac\":\"" + String(mac) + "\"";
    wps_mac_str(ss.ap_bssid, mac);
    s += ",\"ap\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(ss.ap_ssid) + "\"";
    s += ",\"method\":" + String(ss.method);
    s += ",\"msg\":" + String(ss.msg_num);
    s += ",\"done\":" + String(ss.completed ? "true" : "false");
    s += ",\"failed\":" + String(ss.failed ? "true" : "false");
    s += ",\"dur_ms\":" + String(ss.duration_ms) + "}";
  }
  s += "]}";
  return s;
}

inline void wps_routes() {
  server.on("/api/wps/all", HTTP_GET,
            []() { server.send(200, "application/json", wps_json_all()); });
  server.on("/api/wps/clear", HTTP_POST, []() {
    memset(&g_wps, 0, sizeof(g_wps));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

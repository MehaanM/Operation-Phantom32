#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <string.h>
#include <stdint.h>

// ── UI Dashboard API ───────────────────────────────────────────────────────────
//
// Aggregated REST API endpoints for the dashboard. Combines data from
// multiple modules into unified responses, provides system status,
// module health, and cross-module search.

#define UD_MAX_EVENTS 64
#define UD_MAX_ALERTS 32

// ── Event types ───────────────────────────────────────────────────────────────
enum UdEventType : uint8_t {
  UD_EV_INFO = 0,
  UD_EV_WARN = 1,
  UD_EV_ALERT = 2,
  UD_EV_CRITICAL = 3,
};

static const char *ud_ev_type_name(UdEventType t) {
  switch (t) {
    case UD_EV_INFO:
      return "info";
    case UD_EV_WARN:
      return "warn";
    case UD_EV_ALERT:
      return "alert";
    case UD_EV_CRITICAL:
      return "critical";
    default:
      return "unknown";
  }
}

// ── Event entry ───────────────────────────────────────────────────────────────
struct UdEvent {
  uint32_t ts_ms;
  UdEventType type;
  char source[16];
  char message[64];
};

// ── Module health entry ───────────────────────────────────────────────────────
struct UdModuleHealth {
  char name[16];
  bool enabled;
  uint32_t last_active_ms;
  uint32_t frame_count;
  uint8_t error_count;
};

// ── System counters ───────────────────────────────────────────────────────────
struct UdSystemInfo {
  uint32_t uptime_ms;
  uint32_t free_heap;
  uint32_t min_free_heap;
  uint8_t rssi;
  uint8_t cpu_freq_mhz;
  uint32_t flash_size;
  uint32_t sketch_size;
  uint32_t free_sketch_space;
  char mac_ap[18];
  char ip_ap[16];
  char ssid_ap[33];
  uint8_t clients_connected;
  uint8_t wifi_channel;
};

// ── Dashboard state ───────────────────────────────────────────────────────────
struct UdState {
  UdEvent events[UD_MAX_EVENTS];
  uint8_t ev_head;
  uint8_t ev_count;
  UdModuleHealth modules[16];
  uint8_t module_count;
  uint32_t total_frames;
  uint32_t frames_per_sec;
  uint32_t last_fps_ms;
  uint32_t fps_frame_count;
};

static UdState g_ud = {};

// ── Post event ────────────────────────────────────────────────────────────────
static void ud_event(UdEventType type, const char *source, const char *msg) {
  UdEvent *e = &g_ud.events[g_ud.ev_head];
  e->ts_ms = millis();
  e->type = type;
  strncpy(e->source, source ? source : "", 15);
  e->source[15] = 0;
  strncpy(e->message, msg ? msg : "", 63);
  e->message[63] = 0;
  g_ud.ev_head = (g_ud.ev_head + 1) % UD_MAX_EVENTS;
  if (g_ud.ev_count < UD_MAX_EVENTS)
    g_ud.ev_count++;
}

// ── Register module ───────────────────────────────────────────────────────────
static void ud_register_module(const char *name, bool enabled) {
  if (g_ud.module_count >= 16)
    return;
  UdModuleHealth &m = g_ud.modules[g_ud.module_count++];
  strncpy(m.name, name, 15);
  m.name[15] = 0;
  m.enabled = enabled;
  m.last_active_ms = millis();
  m.frame_count = 0;
  m.error_count = 0;
}

// ── Update module activity ────────────────────────────────────────────────────
static void ud_module_activity(const char *name, uint32_t frames) {
  for (uint8_t i = 0; i < g_ud.module_count; i++) {
    if (strcmp(g_ud.modules[i].name, name) == 0) {
      g_ud.modules[i].last_active_ms = millis();
      g_ud.modules[i].frame_count += frames;
      return;
    }
  }
}

// ── FPS update ────────────────────────────────────────────────────────────────
static void ud_fps_tick(uint32_t frames) {
  g_ud.total_frames += frames;
  g_ud.fps_frame_count += frames;
  uint32_t now = millis();
  uint32_t elapsed = now - g_ud.last_fps_ms;
  if (elapsed >= 1000) {
    g_ud.frames_per_sec = (g_ud.fps_frame_count * 1000) / elapsed;
    g_ud.fps_frame_count = 0;
    g_ud.last_fps_ms = now;
  }
}

// ── System info collect ───────────────────────────────────────────────────────
static UdSystemInfo ud_collect_sysinfo() {
  UdSystemInfo s;
  memset(&s, 0, sizeof(s));
  s.uptime_ms = millis();
  s.free_heap = ESP.getFreeHeap();
  s.min_free_heap = ESP.getMinFreeHeap();
  s.cpu_freq_mhz = (uint8_t)ESP.getCpuFreqMHz();
  s.flash_size = ESP.getFlashChipSize();
  s.sketch_size = ESP.getSketchSize();
  s.free_sketch_space = ESP.getFreeSketchSpace();
  // AP MAC
  uint8_t mac[6];
  WiFi.softAPmacAddress(mac);
  snprintf(s.mac_ap, sizeof(s.mac_ap), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2],
           mac[3], mac[4], mac[5]);
  IPAddress ip = WiFi.softAPIP();
  snprintf(s.ip_ap, sizeof(s.ip_ap), "%s", ip.toString().c_str());
  strncpy(s.ssid_ap, "Phantom32", 32);
  s.clients_connected = (uint8_t)WiFi.softAPgetStationNum();
  wifi_second_chan_t sec;
  esp_wifi_get_channel(&s.wifi_channel, &sec);
  return s;
}

// ── System info JSON ──────────────────────────────────────────────────────────
static String ud_sysinfo_json() {
  UdSystemInfo s = ud_collect_sysinfo();
  String j = "{";
  j += "\"uptime_ms\":" + String(s.uptime_ms);
  j += ",\"uptime_s\":" + String(s.uptime_ms / 1000);
  j += ",\"free_heap\":" + String(s.free_heap);
  j += ",\"min_free_heap\":" + String(s.min_free_heap);
  j += ",\"cpu_mhz\":" + String((int)s.cpu_freq_mhz);
  j += ",\"flash_size\":" + String(s.flash_size);
  j += ",\"sketch_size\":" + String(s.sketch_size);
  j += ",\"free_sketch\":" + String(s.free_sketch_space);
  j += ",\"ap_mac\":\"";
  j += s.mac_ap;
  j += "\"";
  j += ",\"ap_ip\":\"";
  j += s.ip_ap;
  j += "\"";
  j += ",\"ap_ssid\":\"Phantom32\"";
  j += ",\"clients\":" + String((int)s.clients_connected);
  j += ",\"channel\":" + String((int)s.wifi_channel);
  j += ",\"total_frames\":" + String(g_ud.total_frames);
  j += ",\"fps\":" + String(g_ud.frames_per_sec);
  j += "}";
  return j;
}

// ── Events JSON ───────────────────────────────────────────────────────────────
static String ud_events_json(int max_n) {
  if (max_n < 1 || max_n > UD_MAX_EVENTS)
    max_n = UD_MAX_EVENTS;
  String j = "{\"count\":" + String(g_ud.ev_count) + ",\"events\":[";
  int start = (g_ud.ev_head + UD_MAX_EVENTS - g_ud.ev_count) % UD_MAX_EVENTS;
  int shown = 0;
  for (int i = g_ud.ev_count - 1; i >= 0 && shown < max_n; i--) {
    if (shown)
      j += ",";
    int idx = (start + i) % UD_MAX_EVENTS;
    UdEvent &e = g_ud.events[idx];
    j += "{\"ts\":";
    j += e.ts_ms;
    j += ",\"type\":\"";
    j += ud_ev_type_name(e.type);
    j += "\"";
    j += ",\"source\":\"";
    j += e.source;
    j += "\"";
    j += ",\"msg\":\"";
    j += e.message;
    j += "\"";
    j += "}";
    shown++;
  }
  j += "]}";
  return j;
}

// ── Modules health JSON ───────────────────────────────────────────────────────
static String ud_modules_json() {
  String j = "{\"modules\":[";
  for (uint8_t i = 0; i < g_ud.module_count; i++) {
    if (i)
      j += ",";
    UdModuleHealth &m = g_ud.modules[i];
    j += "{\"name\":\"";
    j += m.name;
    j += "\"";
    j += ",\"enabled\":";
    j += m.enabled ? 1 : 0;
    j += ",\"frames\":";
    j += m.frame_count;
    j += ",\"errors\":";
    j += m.error_count;
    uint32_t age = (millis() - m.last_active_ms) / 1000;
    j += ",\"idle_s\":";
    j += age;
    j += "}";
  }
  j += "]}";
  return j;
}

// ── Aggregate summary ─────────────────────────────────────────────────────────
static String ud_summary_json() {
  String j = "{";
  j += ud_sysinfo_json().substring(1, ud_sysinfo_json().length() - 1);
  j += ",\"events\":" + String(g_ud.ev_count);
  j += ",\"modules\":" + String(g_ud.module_count);
  j += "}";
  return j;
}

// ── Initialize with standard module list ──────────────────────────────────────
static void ud_init() {
  g_ud.last_fps_ms = millis();
  ud_register_module("analytics", true);
  ud_register_module("rf_monitor", true);
  ud_register_module("threat_intel", true);
  ud_register_module("vuln_scan", true);
  ud_register_module("fingerprint", true);
  ud_register_module("network_map", true);
  ud_register_module("eap_handler", true);
  ud_register_module("pcap_writer", false);
  ud_register_module("port_scanner", false);
  ud_event(UD_EV_INFO, "system", "Phantom32 started");
}

// ── Ticker (call every second) ────────────────────────────────────────────────
static void ud_tick() {
  ud_fps_tick(0);  // zero frames this tick if no caller incremented
}

// ── HTTP routes ───────────────────────────────────────────────────────────────
static void ud_routes() {
  extern WebServer server;

  server.on("/api/sys/info", HTTP_GET,
            []() { server.send(200, "application/json", ud_sysinfo_json()); });

  server.on("/api/sys/events", HTTP_GET, []() {
    int n = server.hasArg("n") ? server.arg("n").toInt() : 20;
    server.send(200, "application/json", ud_events_json(n));
  });

  server.on("/api/sys/modules", HTTP_GET,
            []() { server.send(200, "application/json", ud_modules_json()); });

  server.on("/api/sys/summary", HTTP_GET,
            []() { server.send(200, "application/json", ud_summary_json()); });

  server.on("/api/sys/heap", HTTP_GET, []() {
    char buf[64];
    snprintf(buf, sizeof(buf), "{\"free\":%u,\"min_free\":%u}", (unsigned)ESP.getFreeHeap(),
             (unsigned)ESP.getMinFreeHeap());
    server.send(200, "application/json", buf);
  });

  server.on("/api/sys/reset", HTTP_POST, []() {
    server.send(200, "application/json", "{\"ok\":1}");
    delay(200);
    ESP.restart();
  });

  server.on("/api/sys/event", HTTP_POST, []() {
    if (!server.hasArg("msg")) {
      server.send(400, "application/json", "{\"error\":\"msg required\"}");
      return;
    }
    int type_int = server.hasArg("type") ? server.arg("type").toInt() : 0;
    UdEventType type = (UdEventType)(type_int > 3 ? 0 : type_int);
    String src = server.hasArg("src") ? server.arg("src") : "api";
    ud_event(type, src.c_str(), server.arg("msg").c_str());
    server.send(200, "application/json", "{\"ok\":1}");
  });

  server.on("/api/sys/channels", HTTP_GET, []() {
    // Return current channel and available channels
    uint8_t ch = 0;
    wifi_second_chan_t sec;
    esp_wifi_get_channel(&ch, &sec);
    String j = "{\"current\":" + String((int)ch);
    j += ",\"available\":[1,2,3,4,5,6,7,8,9,10,11,12,13]}";
    server.send(200, "application/json", j);
  });

  server.on("/api/sys/set_channel", HTTP_POST, []() {
    if (!server.hasArg("ch")) {
      server.send(400, "application/json", "{\"error\":\"ch required\"}");
      return;
    }
    uint8_t ch = (uint8_t)server.arg("ch").toInt();
    if (ch < 1 || ch > 14) {
      server.send(400, "application/json", "{\"error\":\"ch out of range\"}");
      return;
    }
    esp_err_t r = esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    server.send(200, "application/json", r == ESP_OK ? "{\"ok\":1}" : "{\"error\":\"set failed\"}");
  });

  server.on("/api/sys/scan_start", HTTP_POST, []() {
    // Trigger WiFi scan
    WiFi.scanNetworks(true, true);
    server.send(200, "application/json", "{\"ok\":1,\"async\":1}");
  });

  server.on("/api/sys/scan_results", HTTP_GET, []() {
    int16_t n = WiFi.scanComplete();
    if (n < 0) {
      server.send(200, "application/json", "{\"status\":\"scanning\"}");
      return;
    }
    String j = "{\"count\":" + String((int)n) + ",\"aps\":[";
    for (int16_t i = 0; i < n; i++) {
      if (i)
        j += ",";
      j += "{\"ssid\":\"";
      j += WiFi.SSID(i);
      j += "\"";
      j += ",\"bssid\":\"";
      j += WiFi.BSSIDstr(i);
      j += "\"";
      j += ",\"rssi\":";
      j += WiFi.RSSI(i);
      j += ",\"ch\":";
      j += WiFi.channel(i);
      j += ",\"enc\":";
      j += WiFi.encryptionType(i);
      j += "}";
    }
    j += "]}";
    server.send(200, "application/json", j);
  });

  server.on("/api/sys/promiscuous", HTTP_POST, []() {
    bool en = server.hasArg("enable") && server.arg("enable") != "0";
    esp_wifi_set_promiscuous(en);
    server.send(200, "application/json",
                en ? "{\"ok\":1,\"mode\":\"promiscuous\"}" : "{\"ok\":1,\"mode\":\"normal\"}");
  });

  server.on("/api/sys/firmware", HTTP_GET, []() {
    char buf[256];
    snprintf(buf, sizeof(buf),
             "{\"chip_model\":\"%s\",\"chip_rev\":%d,\"sdk_ver\":\"%s\","
             "\"flash_size\":%u,\"cpu_mhz\":%u}",
             ESP.getChipModel(), (int)ESP.getChipRevision(), ESP.getSdkVersion(),
             (unsigned)ESP.getFlashChipSize(), (unsigned)ESP.getCpuFreqMHz());
    server.send(200, "application/json", buf);
  });
}

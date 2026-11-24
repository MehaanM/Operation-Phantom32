#pragma once
// session_tracker.h — Global session and uptime tracker.
// Tracks AP operating time, restarts, usage statistics. Prefix: ses_

#include <Arduino.h>
#include <WebServer.h>
#include <LittleFS.h>

extern WebServer server;

#define SES_PATH       "/session.json"
#define SES_MAX_EVENTS  32

// ─── Session event ────────────────────────────────────────────────────────────
struct SesEvent {
  bool    valid;
  char    type[24];  // boot, channel_change, module_start, etc.
  char    detail[48];
  uint32_t ts;
};

// ─── Session state ────────────────────────────────────────────────────────────
struct SesState {
  uint32_t boot_ts;
  uint32_t session_id;
  uint8_t  boot_count;
  uint32_t total_frames;
  uint32_t total_aps;
  uint32_t total_stas;
  uint32_t total_alerts;
  uint32_t last_save_ts;
  char     ap_ssid[33];
  char     ap_ip[16];
  uint8_t  ap_channel;
  char     fw_version[16];

  // Runtime counters
  uint32_t uptime_s;
  uint32_t frames_per_min;
  uint32_t last_frame_ts;
};

// ─── State ────────────────────────────────────────────────────────────────────
static struct {
  SesState  state;
  SesEvent  events[SES_MAX_EVENTS];
  uint8_t   ev_head;
} g_ses = {};

// ─── Init ─────────────────────────────────────────────────────────────────────
void ses_init(const char *ssid, const char *ip, uint8_t channel) {
  g_ses.state.boot_ts = millis() / 1000;
  if (ssid) strncpy(g_ses.state.ap_ssid, ssid, 32);
  if (ip)   strncpy(g_ses.state.ap_ip,   ip,   15);
  g_ses.state.ap_channel = channel;
  strncpy(g_ses.state.fw_version, "1.0.0", 15);

  // Load persistent state
  if (LittleFS.exists(SES_PATH)) {
    File f = LittleFS.open(SES_PATH, "r");
    if (f) {
      // Parse boot_count and session_id from JSON (simple scan)
      String json = f.readString();
      f.close();
      int bc_pos = json.indexOf("\"boot_count\":");
      if (bc_pos >= 0) {
        g_ses.state.boot_count = json.substring(bc_pos+13).toInt();
      }
      int sid_pos = json.indexOf("\"session_id\":");
      if (sid_pos >= 0) {
        g_ses.state.session_id = json.substring(sid_pos+13).toInt();
      }
    }
  }
  g_ses.state.boot_count++;
  g_ses.state.session_id++;
}

// ─── Log event ───────────────────────────────────────────────────────────────
void ses_log(const char *type, const char *detail) {
  SesEvent &e = g_ses.events[g_ses.ev_head % SES_MAX_EVENTS];
  e.valid = true;
  strncpy(e.type,   type,   23);
  strncpy(e.detail, detail, 47);
  e.ts = millis() / 1000;
  g_ses.ev_head = (g_ses.ev_head + 1) % SES_MAX_EVENTS;
}

// ─── Tick ─────────────────────────────────────────────────────────────────────
void ses_tick() {
  uint32_t now_s = millis() / 1000;
  g_ses.state.uptime_s = now_s - g_ses.state.boot_ts;

  // Auto-save every 60s
  if (now_s - g_ses.state.last_save_ts > 60) {
    g_ses.state.last_save_ts = now_s;
    File f = LittleFS.open(SES_PATH, "w");
    if (f) {
      f.printf("{\"boot_count\":%d,\"session_id\":%u,\"uptime\":%u}",
               g_ses.state.boot_count, g_ses.state.session_id, g_ses.state.uptime_s);
      f.close();
    }
  }
}

// ─── Update counters ──────────────────────────────────────────────────────────
void ses_frame(uint32_t total_frames_seen) {
  g_ses.state.total_frames = total_frames_seen;
  g_ses.state.last_frame_ts = millis() / 1000;
}

// ─── JSON ─────────────────────────────────────────────────────────────────────
static String ses_json_status() {
  SesState &s = g_ses.state;
  uint32_t now = millis() / 1000;
  s.uptime_s = now - s.boot_ts;

  String js = "{\"session_id\":" + String(s.session_id);
  js += ",\"boot_count\":" + String(s.boot_count);
  js += ",\"uptime_s\":" + String(s.uptime_s);
  js += ",\"ap_ssid\":\"" + String(s.ap_ssid) + "\"";
  js += ",\"ap_ip\":\"" + String(s.ap_ip) + "\"";
  js += ",\"ap_channel\":" + String(s.ap_channel);
  js += ",\"fw_version\":\"" + String(s.fw_version) + "\"";
  js += ",\"total_frames\":" + String(s.total_frames);
  js += ",\"heap_free\":" + String(ESP.getFreeHeap());
  js += ",\"heap_min\":" + String(ESP.getMinFreeHeap());
  js += ",\"chip_model\":\"ESP32\"";
  js += ",\"cpu_freq\":" + String(ESP.getCpuFreqMHz());
  js += ",\"flash_size\":" + String(ESP.getFlashChipSize());
  js += ",\"events\":[";
  bool first = true;
  int start = (g_ses.ev_head + SES_MAX_EVENTS - 1) % SES_MAX_EVENTS;
  for (int i = 0; i < SES_MAX_EVENTS; i++) {
    int idx = (start - i + SES_MAX_EVENTS) % SES_MAX_EVENTS;
    SesEvent &e = g_ses.events[idx];
    if (!e.valid) continue;
    if (!first) js += ",";
    first = false;
    js += "{\"ts\":" + String(e.ts);
    js += ",\"type\":\"" + String(e.type) + "\"";
    js += ",\"detail\":\"" + String(e.detail) + "\"}";
    if (i >= 19) break;
  }
  js += "]}";
  return js;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void ses_routes() {
  server.on("/api/ses/status", HTTP_GET, []() {
    server.send(200, "application/json", ses_json_status());
  });

  server.on("/api/ses/log", HTTP_POST, []() {
    const char *type   = server.hasArg("type")   ? server.arg("type").c_str()   : "user";
    const char *detail = server.hasArg("detail")  ? server.arg("detail").c_str() : "";
    ses_log(type, detail);
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/ses/clear_events", HTTP_POST, []() {
    memset(g_ses.events, 0, sizeof(g_ses.events));
    g_ses.ev_head = 0;
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

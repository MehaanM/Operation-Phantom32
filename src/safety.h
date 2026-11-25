#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <esp_timer.h>
#include "auth_manager.h"

struct SafetyLimits {
  uint16_t deauth_per_sec;
  uint16_t beacon_per_sec;
  uint16_t probe_per_sec;
  uint16_t auto_stop_sec;
  char blocked_bssids[384];
};

static SafetyLimits g_safety = {100, 200, 300, 0, ""};
static uint32_t g_deauth_window = 0, g_beacon_window = 0, g_probe_window = 0;
static uint32_t g_deauth_start_us = 0;
static void (*safety_autostop_hook)() = nullptr;

inline void safety_load() {
  Preferences p;
  if (!p.begin("safety", true))
    return;
  g_safety.deauth_per_sec = p.getUShort("dps", 100);
  g_safety.beacon_per_sec = p.getUShort("bps", 200);
  g_safety.probe_per_sec = p.getUShort("pps", 300);
  g_safety.auto_stop_sec = p.getUShort("ast", 0);
  p.getString("blk", "").toCharArray(g_safety.blocked_bssids, sizeof(g_safety.blocked_bssids));
  p.end();
}
inline void safety_save() {
  Preferences p;
  if (!p.begin("safety", false))
    return;
  p.putUShort("dps", g_safety.deauth_per_sec);
  p.putUShort("bps", g_safety.beacon_per_sec);
  p.putUShort("pps", g_safety.probe_per_sec);
  p.putUShort("ast", g_safety.auto_stop_sec);
  p.putString("blk", g_safety.blocked_bssids);
  p.end();
}

inline bool safety_bssid_blocked(const uint8_t *b) {
  if (!g_safety.blocked_bssids[0])
    return false;
  char buf[18];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", b[0], b[1], b[2], b[3], b[4], b[5]);
  return strstr(g_safety.blocked_bssids, buf) != nullptr;
}

inline void safety_blocklist_add(const String &mac) {
  String cur = g_safety.blocked_bssids;
  if (cur.indexOf(mac) >= 0)
    return;
  if (cur.length())
    cur += ",";
  cur += mac;
  cur.toCharArray(g_safety.blocked_bssids, sizeof(g_safety.blocked_bssids));
  safety_save();
}
inline void safety_blocklist_clear() {
  g_safety.blocked_bssids[0] = 0;
  safety_save();
}

// kind: 1=deauth, 2=beacon, 3=probe. Returns true if under the per-second limit.
inline bool safety_rate_ok(uint8_t kind) {
  uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
  uint32_t now_s = now_ms / 1000;
  uint32_t *window = (kind == 1)   ? &g_deauth_window
                     : (kind == 2) ? &g_beacon_window
                                   : &g_probe_window;
  static uint32_t last_s[4] = {0, 0, 0, 0};
  if (now_s != last_s[kind]) {
    last_s[kind] = now_s;
    *window = 0;
  }
  uint16_t limit = (kind == 1)   ? g_safety.deauth_per_sec
                   : (kind == 2) ? g_safety.beacon_per_sec
                                 : g_safety.probe_per_sec;
  if (*window >= limit)
    return false;
  (*window)++;
  return true;
}

inline void safety_on_start(const char *slug) {
  g_deauth_start_us = (uint32_t)(esp_timer_get_time() / 1000000);
  if (g_safety.auto_stop_sec > 0) {
    Serial.printf("[safety] %s will auto-stop after %us\n", slug, g_safety.auto_stop_sec);
  }
}

inline void safety_tick() {
  if (!g_active || g_safety.auto_stop_sec == 0)
    return;
  uint32_t now_s = (uint32_t)(esp_timer_get_time() / 1000000);
  if (now_s - g_deauth_start_us >= g_safety.auto_stop_sec) {
    Serial.printf("[safety] auto-stop: %s ran %us\n", g_active, g_safety.auto_stop_sec);
    if (safety_autostop_hook)
      safety_autostop_hook();
  }
}

inline void safety_routes() {
  extern WebServer server;
  server.on("/api/safety/get", HTTP_GET, []() {
    if (!auth_check()) {
      auth_reject();
      return;
    }
    String j = "{\"dps\":" + String(g_safety.deauth_per_sec) +
               ",\"bps\":" + String(g_safety.beacon_per_sec) +
               ",\"pps\":" + String(g_safety.probe_per_sec) +
               ",\"ast\":" + String(g_safety.auto_stop_sec) + ",\"blk\":\"" +
               String(g_safety.blocked_bssids) + "\"}";
    server.send(200, "application/json", j);
  });
  server.on("/api/safety/set", HTTP_POST, []() {
    if (!auth_check()) {
      auth_reject();
      return;
    }
    if (server.hasArg("dps"))
      g_safety.deauth_per_sec = server.arg("dps").toInt();
    if (server.hasArg("bps"))
      g_safety.beacon_per_sec = server.arg("bps").toInt();
    if (server.hasArg("pps"))
      g_safety.probe_per_sec = server.arg("pps").toInt();
    if (server.hasArg("ast"))
      g_safety.auto_stop_sec = server.arg("ast").toInt();
    safety_save();
    server.send(200, "application/json", "{\"ok\":1}");
  });
  server.on("/api/safety/block", HTTP_POST, []() {
    if (!auth_check()) {
      auth_reject();
      return;
    }
    if (!server.hasArg("mac"))
      return server.send(400, "application/json", "{\"ok\":0}");
    safety_blocklist_add(server.arg("mac"));
    server.send(200, "application/json", "{\"ok\":1}");
  });
  server.on("/api/safety/unblock", HTTP_POST, []() {
    if (!auth_check()) {
      auth_reject();
      return;
    }
    safety_blocklist_clear();
    server.send(200, "application/json", "{\"ok\":1}");
  });
}
static bool safety_bssid_blocked_from_str(const String &s) {
  if (!g_safety.blocked_bssids[0])
    return false;
  return strstr(g_safety.blocked_bssids, s.c_str()) != nullptr;
}

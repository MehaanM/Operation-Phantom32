#pragma once
// rate_limiter_adv.h — Centralized per-endpoint, per-IP, per-tool rate limiter.
//
// All active radio transmission operations (deauth, beacon spam, probe flood,
// etc.) go through rla_check() before transmitting. This provides a single
// enforcement boundary that cannot be bypassed regardless of which tool
// initiated the transmission.
//
// Three independent rate axes:
//   1. Per-tool TX rate (frames/second)
//   2. Per-IP HTTP request rate (requests/minute, for web UI spam prevention)
//   3. Global TX rate budget (all tools combined)
//
// Limits are stored in NVS Preferences and are configurable at runtime via
// the /api/rla/* endpoints.

#include <Arduino.h>
#include <WebServer.h>
#include <Preferences.h>
#include <string.h>

extern WebServer server;

// ============================================================
// CONFIGURATION
// ============================================================

#define RLA_MAX_TOOL_SLOTS   32    // maximum tool entries
#define RLA_MAX_IP_SLOTS     16    // IPs to rate-track
#define RLA_WINDOW_S         1     // rate window in seconds (sliding)
#define RLA_IP_WINDOW_S      60    // HTTP request window in seconds

#define RLA_DEFAULT_TX_FPS   20    // default max TX frames per second per tool
#define RLA_DEFAULT_BURST    50    // burst capacity (frames above steady rate)
#define RLA_GLOBAL_FPS       200   // combined max TX frames/second across all tools
#define RLA_HTTP_REQ_PM      300   // max HTTP requests per minute per IP

#define RLA_NVS_NS           "phantom32"
#define RLA_NVS_GLOBAL_FPS   "rla_gfps"
#define RLA_NVS_HTTP_RPM     "rla_hrpm"

// ============================================================
// DATA STRUCTURES
// ============================================================

struct RlaToolSlot {
  char     name[24];         // tool name (matches ToolModule name)
  bool     active;
  uint32_t max_fps;          // configured max frames/second
  uint32_t burst_capacity;   // token bucket burst capacity
  float    tokens;           // current token count (token bucket)
  uint32_t last_refill_us;   // last token refill timestamp (esp_timer_get_time)
  uint32_t total_allowed;    // lifetime frames allowed
  uint32_t total_blocked;    // lifetime frames blocked
  uint32_t recent_fps;       // rolling 1s average
  uint32_t recent_count;     // frames in current second window
  uint32_t window_start_us;  // start of current 1s window
};

struct RlaIpSlot {
  char     ip[16];
  uint32_t req_count;        // requests in current window
  uint32_t window_start_s;   // start of current window (seconds)
  uint32_t total_blocked;    // lifetime blocked requests from this IP
};

struct RlaGlobalBucket {
  float    tokens;
  uint32_t last_refill_us;
  uint32_t max_fps;
  uint32_t total_allowed;
  uint32_t total_blocked;
};

// ============================================================
// STATE
// ============================================================

static RlaToolSlot   g_rla_tools[RLA_MAX_TOOL_SLOTS] = {};
static RlaIpSlot     g_rla_ips[RLA_MAX_IP_SLOTS] = {};
static RlaGlobalBucket g_rla_global = {
  .tokens = RLA_GLOBAL_FPS,
  .last_refill_us = 0,
  .max_fps = RLA_GLOBAL_FPS,
  .total_allowed = 0,
  .total_blocked = 0
};
static uint32_t g_rla_http_rpm = RLA_HTTP_REQ_PM;

// ============================================================
// HELPERS
// ============================================================

static RlaToolSlot* rla_find_tool(const char *name) {
  for (int i = 0; i < RLA_MAX_TOOL_SLOTS; i++) {
    if (g_rla_tools[i].active && strcmp(g_rla_tools[i].name, name) == 0)
      return &g_rla_tools[i];
  }
  return nullptr;
}

static RlaToolSlot* rla_alloc_tool(const char *name) {
  for (int i = 0; i < RLA_MAX_TOOL_SLOTS; i++) {
    if (!g_rla_tools[i].active) {
      RlaToolSlot &s = g_rla_tools[i];
      memset(&s, 0, sizeof(s));
      strncpy(s.name, name, 23);
      s.active = true;
      s.max_fps = RLA_DEFAULT_TX_FPS;
      s.burst_capacity = RLA_DEFAULT_BURST;
      s.tokens = RLA_DEFAULT_TX_FPS;
      s.last_refill_us = esp_timer_get_time();
      return &s;
    }
  }
  return nullptr;
}

static RlaToolSlot* rla_get_or_create(const char *name) {
  RlaToolSlot *s = rla_find_tool(name);
  if (s) return s;
  return rla_alloc_tool(name);
}

static void rla_refill_tool(RlaToolSlot *s) {
  uint64_t now_us = esp_timer_get_time();
  float elapsed_s = (float)(now_us - s->last_refill_us) / 1e6f;
  if (elapsed_s <= 0) return;
  s->tokens += elapsed_s * s->max_fps;
  if (s->tokens > s->burst_capacity) s->tokens = s->burst_capacity;
  s->last_refill_us = now_us;

  // Rolling 1s fps counter
  uint32_t now_us32 = (uint32_t)now_us;
  if ((now_us32 - s->window_start_us) >= 1000000u) {
    s->recent_fps = s->recent_count;
    s->recent_count = 0;
    s->window_start_us = now_us32;
  }
}

static void rla_refill_global() {
  uint64_t now_us = esp_timer_get_time();
  float elapsed_s = (float)(now_us - g_rla_global.last_refill_us) / 1e6f;
  if (elapsed_s <= 0) return;
  g_rla_global.tokens += elapsed_s * g_rla_global.max_fps;
  if (g_rla_global.tokens > g_rla_global.max_fps * 2)
    g_rla_global.tokens = g_rla_global.max_fps * 2;
  g_rla_global.last_refill_us = now_us;
}

// ============================================================
// CORE TX RATE CHECK
// ============================================================

// Returns true if the frame is allowed; false if rate-limited.
// Call before every esp_wifi_80211_tx() call.
static bool rla_check(const char *tool_name) {
  // 1. Global token bucket
  rla_refill_global();
  if (g_rla_global.tokens < 1.0f) {
    g_rla_global.total_blocked++;
    return false;
  }

  // 2. Per-tool token bucket
  RlaToolSlot *s = rla_get_or_create(tool_name);
  if (!s) {
    g_rla_global.total_blocked++;
    return false;
  }
  rla_refill_tool(s);
  if (s->tokens < 1.0f) {
    s->total_blocked++;
    g_rla_global.total_blocked++;
    return false;
  }

  // Consume one token from each bucket.
  s->tokens -= 1.0f;
  g_rla_global.tokens -= 1.0f;
  s->total_allowed++;
  s->recent_count++;
  g_rla_global.total_allowed++;
  return true;
}

// ============================================================
// HTTP REQUEST RATE CHECK
// ============================================================

static RlaIpSlot* rla_ip_find_or_add(const char *ip) {
  uint32_t now_s = millis() / 1000;
  for (int i = 0; i < RLA_MAX_IP_SLOTS; i++) {
    if (strcmp(g_rla_ips[i].ip, ip) == 0) return &g_rla_ips[i];
  }
  for (int i = 0; i < RLA_MAX_IP_SLOTS; i++) {
    if (!g_rla_ips[i].ip[0]) {
      strncpy(g_rla_ips[i].ip, ip, 15);
      g_rla_ips[i].req_count = 0;
      g_rla_ips[i].window_start_s = now_s;
      return &g_rla_ips[i];
    }
  }
  // Evict the oldest.
  uint32_t oldest = UINT32_MAX; int idx = 0;
  for (int i = 0; i < RLA_MAX_IP_SLOTS; i++) {
    if (g_rla_ips[i].window_start_s < oldest) {
      oldest = g_rla_ips[i].window_start_s;
      idx = i;
    }
  }
  memset(&g_rla_ips[idx], 0, sizeof(RlaIpSlot));
  strncpy(g_rla_ips[idx].ip, ip, 15);
  g_rla_ips[idx].window_start_s = now_s;
  return &g_rla_ips[idx];
}

static bool rla_check_http(const char *ip) {
  RlaIpSlot *s = rla_ip_find_or_add(ip);
  if (!s) return true;
  uint32_t now_s = millis() / 1000;
  if ((now_s - s->window_start_s) >= RLA_IP_WINDOW_S) {
    s->req_count = 0;
    s->window_start_s = now_s;
  }
  s->req_count++;
  if (s->req_count > g_rla_http_rpm) {
    s->total_blocked++;
    return false;
  }
  return true;
}

// ============================================================
// CONFIGURATION PERSISTENCE
// ============================================================

static void rla_load_config() {
  Preferences prefs;
  prefs.begin(RLA_NVS_NS, true);
  g_rla_global.max_fps = prefs.getUInt(RLA_NVS_GLOBAL_FPS, RLA_GLOBAL_FPS);
  g_rla_global.tokens  = g_rla_global.max_fps;
  g_rla_http_rpm       = prefs.getUInt(RLA_NVS_HTTP_RPM, RLA_HTTP_REQ_PM);
  prefs.end();
}

static void rla_save_config() {
  Preferences prefs;
  prefs.begin(RLA_NVS_NS, false);
  prefs.putUInt(RLA_NVS_GLOBAL_FPS, g_rla_global.max_fps);
  prefs.putUInt(RLA_NVS_HTTP_RPM,   g_rla_http_rpm);
  prefs.end();
}

// ============================================================
// ROUTE HANDLERS
// ============================================================

static void rla_handle_status() {
  String s = "{\"global\":{";
  s += "\"max_fps\":" + String(g_rla_global.max_fps);
  s += ",\"tokens\":" + String((int)g_rla_global.tokens);
  s += ",\"total_allowed\":" + String(g_rla_global.total_allowed);
  s += ",\"total_blocked\":" + String(g_rla_global.total_blocked);
  s += ",\"http_rpm\":" + String(g_rla_http_rpm);
  s += "},\"tools\":[";
  bool first = true;
  for (int i = 0; i < RLA_MAX_TOOL_SLOTS; i++) {
    if (!g_rla_tools[i].active) continue;
    if (!first) s += ",";
    first = false;
    s += "{\"name\":\"" + String(g_rla_tools[i].name) + "\"";
    s += ",\"max_fps\":" + String(g_rla_tools[i].max_fps);
    s += ",\"burst\":" + String(g_rla_tools[i].burst_capacity);
    s += ",\"tokens\":" + String((int)g_rla_tools[i].tokens);
    s += ",\"fps\":" + String(g_rla_tools[i].recent_fps);
    s += ",\"allowed\":" + String(g_rla_tools[i].total_allowed);
    s += ",\"blocked\":" + String(g_rla_tools[i].total_blocked) + "}";
  }
  s += "]}";
  server.send(200, "application/json", s);
}

static void rla_handle_set_global() {
  if (server.hasArg("max_fps")) {
    uint32_t fps = (uint32_t)server.arg("max_fps").toInt();
    if (fps > 0 && fps <= 1000) {
      g_rla_global.max_fps = fps;
      rla_save_config();
    }
  }
  if (server.hasArg("http_rpm")) {
    uint32_t rpm = (uint32_t)server.arg("http_rpm").toInt();
    if (rpm > 0 && rpm <= 10000) {
      g_rla_http_rpm = rpm;
      rla_save_config();
    }
  }
  server.send(200, "application/json", "{\"ok\":true}");
}

static void rla_handle_set_tool() {
  if (!server.hasArg("name")) {
    server.send(400, "application/json", "{\"error\":\"name required\"}");
    return;
  }
  String name = server.arg("name");
  RlaToolSlot *s = rla_get_or_create(name.c_str());
  if (!s) {
    server.send(503, "application/json", "{\"error\":\"table full\"}");
    return;
  }
  if (server.hasArg("max_fps")) {
    uint32_t fps = (uint32_t)server.arg("max_fps").toInt();
    if (fps > 0 && fps <= 500) s->max_fps = fps;
  }
  if (server.hasArg("burst")) {
    uint32_t burst = (uint32_t)server.arg("burst").toInt();
    if (burst > 0 && burst <= 1000) s->burst_capacity = burst;
  }
  server.send(200, "application/json", "{\"ok\":true}");
}

static void rla_handle_reset_tool() {
  if (!server.hasArg("name")) {
    server.send(400, "application/json", "{\"error\":\"name required\"}");
    return;
  }
  String name = server.arg("name");
  for (int i = 0; i < RLA_MAX_TOOL_SLOTS; i++) {
    if (g_rla_tools[i].active && strcmp(g_rla_tools[i].name, name.c_str()) == 0) {
      g_rla_tools[i].total_allowed = 0;
      g_rla_tools[i].total_blocked = 0;
      g_rla_tools[i].recent_fps = 0;
      g_rla_tools[i].tokens = g_rla_tools[i].max_fps;
      server.send(200, "application/json", "{\"ok\":true}");
      return;
    }
  }
  server.send(404, "application/json", "{\"error\":\"tool not found\"}");
}

// ============================================================
// PUBLIC API
// ============================================================

inline void rla_init() {
  rla_load_config();
  g_rla_global.last_refill_us = esp_timer_get_time();
  Serial.printf("[RLA] Rate limiter init. Global max: %u fps, HTTP: %u rpm\n",
                g_rla_global.max_fps, g_rla_http_rpm);
}

inline void rla_routes() {
  server.on("/api/rla/status",    HTTP_GET,  rla_handle_status);
  server.on("/api/rla/global",    HTTP_POST, rla_handle_set_global);
  server.on("/api/rla/tool",      HTTP_POST, rla_handle_set_tool);
  server.on("/api/rla/tool/reset",HTTP_POST, rla_handle_reset_tool);
}

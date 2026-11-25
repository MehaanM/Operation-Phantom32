#pragma once
// event_logger.h — Persistent event logging with LittleFS rotation.
// Stores timestamped events (alerts, discoveries, actions) with severity,
// category, and optional detail payload. HTTP query interface. Prefix: evl_

#include <Arduino.h>
#include <WebServer.h>
#include <LittleFS.h>

extern WebServer server;

#define EVL_MAX_EVENTS 256
#define EVL_DETAIL_LEN 96
#define EVL_CAT_LEN 20
#define EVL_LOG_FILE "/events.log"
#define EVL_LOG_MAX_BYTES (64 * 1024)  // 64 KB before rotation

// ─── Event category ───────────────────────────────────────────────────────────
enum EvlCategory {
  EVL_CAT_SCAN = 0,
  EVL_CAT_DISCOVERY,   // new AP/STA/BLE
  EVL_CAT_ALERT,       // threat / anomaly
  EVL_CAT_ATTACK,      // active attack module event
  EVL_CAT_CAPTURE,     // PMKID / handshake
  EVL_CAT_SYSTEM,      // boot, reboot, error
  EVL_CAT_AUTOMATION,  // automation trigger fired
  EVL_CAT_USER,        // manual user action
  EVL_CAT_NETWORK,     // network-level event
  EVL_CAT_BLE,         // BLE event
};

enum EvlSeverity {
  EVL_SEV_DEBUG = 0,
  EVL_SEV_INFO,
  EVL_SEV_WARN,
  EVL_SEV_ERROR,
  EVL_SEV_CRITICAL,
};

// ─── Event struct ─────────────────────────────────────────────────────────────
struct EvlEvent {
  uint32_t ts;  // millis()/1000
  EvlCategory cat;
  EvlSeverity sev;
  char source[16];  // module prefix
  char detail[EVL_DETAIL_LEN];
  uint32_t context_int;  // optional integer context
};

// ─── State ────────────────────────────────────────────────────────────────────
static struct {
  EvlEvent ring[EVL_MAX_EVENTS];
  uint16_t head;
  uint16_t total;
  bool persist_enabled;
  uint32_t bytes_written;
  uint32_t rotations;
  uint32_t event_count_by_cat[10];
  uint32_t event_count_by_sev[5];
} g_evl = {};

// ─── Helpers ──────────────────────────────────────────────────────────────────
static const char *evl_cat_str(EvlCategory c) {
  switch (c) {
    case EVL_CAT_SCAN:
      return "scan";
    case EVL_CAT_DISCOVERY:
      return "discovery";
    case EVL_CAT_ALERT:
      return "alert";
    case EVL_CAT_ATTACK:
      return "attack";
    case EVL_CAT_CAPTURE:
      return "capture";
    case EVL_CAT_SYSTEM:
      return "system";
    case EVL_CAT_AUTOMATION:
      return "automation";
    case EVL_CAT_USER:
      return "user";
    case EVL_CAT_NETWORK:
      return "network";
    case EVL_CAT_BLE:
      return "ble";
    default:
      return "unknown";
  }
}

static const char *evl_sev_str(EvlSeverity s) {
  switch (s) {
    case EVL_SEV_DEBUG:
      return "debug";
    case EVL_SEV_INFO:
      return "info";
    case EVL_SEV_WARN:
      return "warn";
    case EVL_SEV_ERROR:
      return "error";
    case EVL_SEV_CRITICAL:
      return "critical";
    default:
      return "info";
  }
}

// ─── Log event ────────────────────────────────────────────────────────────────
void evl_log(EvlCategory cat, EvlSeverity sev, const char *source, const char *detail,
             uint32_t ctx = 0) {
  EvlEvent &e = g_evl.ring[g_evl.head % EVL_MAX_EVENTS];
  e.ts = millis() / 1000;
  e.cat = cat;
  e.sev = sev;
  e.context_int = ctx;
  strncpy(e.source, source ? source : "", 15);
  strncpy(e.detail, detail ? detail : "", EVL_DETAIL_LEN - 1);
  e.source[15] = '\0';
  e.detail[EVL_DETAIL_LEN - 1] = '\0';
  g_evl.head++;
  g_evl.total++;

  if (cat < 10)
    g_evl.event_count_by_cat[(int)cat]++;
  if (sev < 5)
    g_evl.event_count_by_sev[(int)sev]++;

  // Persist to flash
  if (g_evl.persist_enabled) {
    // Check rotation
    if (g_evl.bytes_written > EVL_LOG_MAX_BYTES) {
      // Rotate: rename .log to .log.1, clear current
      LittleFS.remove("/events.log.1");
      LittleFS.rename(EVL_LOG_FILE, "/events.log.1");
      g_evl.bytes_written = 0;
      g_evl.rotations++;
    }
    File f = LittleFS.open(EVL_LOG_FILE, "a");
    if (f) {
      char line[160];
      int n = snprintf(line, sizeof(line), "%lu|%s|%s|%s|%s|%lu\n", (unsigned long)e.ts,
                       evl_cat_str(cat), evl_sev_str(sev), e.source, e.detail, (unsigned long)ctx);
      f.write((uint8_t *)line, n);
      g_evl.bytes_written += n;
      f.close();
    }
  }
}

// Convenience macros
#define EVL_DEBUG(src, msg) evl_log(EVL_CAT_SYSTEM, EVL_SEV_DEBUG, src, msg)
#define EVL_INFO(src, msg) evl_log(EVL_CAT_SYSTEM, EVL_SEV_INFO, src, msg)
#define EVL_WARN(src, msg) evl_log(EVL_CAT_ALERT, EVL_SEV_WARN, src, msg)
#define EVL_ERROR(src, msg) evl_log(EVL_CAT_SYSTEM, EVL_SEV_ERROR, src, msg)
#define EVL_CRITICAL(src, msg) evl_log(EVL_CAT_ALERT, EVL_SEV_CRITICAL, src, msg)
#define EVL_DISCOVERY(src, msg) evl_log(EVL_CAT_DISCOVERY, EVL_SEV_INFO, src, msg)
#define EVL_CAPTURE(src, msg) evl_log(EVL_CAT_CAPTURE, EVL_SEV_INFO, src, msg)

// ─── Init ────────────────────────────────────────────────────────────────────
inline void evl_init(bool persist = true) {
  memset(&g_evl, 0, sizeof(g_evl));
  g_evl.persist_enabled = persist;
  evl_log(EVL_CAT_SYSTEM, EVL_SEV_INFO, "evl", "Event logger initialized");
}

// ─── Query/filter ─────────────────────────────────────────────────────────────
static String evl_json_query(int limit, int sev_min, int cat_filter, uint32_t since_ts) {
  String s = "{\"total\":" + String(g_evl.total);
  s += ",\"events\":[";

  int shown = 0;
  int total_in_ring = min((int)g_evl.total, EVL_MAX_EVENTS);
  bool first = true;

  for (int i = 0; i < total_in_ring && shown < limit; i++) {
    // Most recent first
    int idx = ((int)g_evl.head - 1 - i + EVL_MAX_EVENTS) % EVL_MAX_EVENTS;
    EvlEvent &e = g_evl.ring[idx];
    if (e.ts == 0)
      continue;
    if (sev_min >= 0 && (int)e.sev < sev_min)
      continue;
    if (cat_filter >= 0 && (int)e.cat != cat_filter)
      continue;
    if (since_ts > 0 && e.ts < since_ts)
      continue;

    if (!first)
      s += ",";
    first = false;
    shown++;

    s += "{\"ts\":" + String(e.ts);
    s += ",\"cat\":\"" + String(evl_cat_str(e.cat)) + "\"";
    s += ",\"sev\":\"" + String(evl_sev_str(e.sev)) + "\"";
    s += ",\"src\":\"" + String(e.source) + "\"";
    s += ",\"detail\":\"";
    // JSON-escape detail
    for (int j = 0; e.detail[j]; j++) {
      char c = e.detail[j];
      if (c == '"')
        s += "\\\"";
      else if (c == '\\')
        s += "\\\\";
      else if (c == '\n')
        s += "\\n";
      else
        s += c;
    }
    s += "\"";
    s += ",\"ctx\":" + String(e.context_int);
    s += "}";
  }
  s += "],\"stats\":{";
  s += "\"by_cat\":{";
  for (int i = 0; i < 10; i++) {
    if (i)
      s += ",";
    s += "\"" + String(evl_cat_str((EvlCategory)i)) + "\":" + String(g_evl.event_count_by_cat[i]);
  }
  s += "},\"by_sev\":{";
  for (int i = 0; i < 5; i++) {
    if (i)
      s += ",";
    s += "\"" + String(evl_sev_str((EvlSeverity)i)) + "\":" + String(g_evl.event_count_by_sev[i]);
  }
  s += "},\"persist\":" + String(g_evl.persist_enabled ? "true" : "false");
  s += ",\"bytes_written\":" + String(g_evl.bytes_written);
  s += ",\"rotations\":" + String(g_evl.rotations);
  s += "}}";
  return s;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void evl_routes() {
  server.on("/api/evl/events", HTTP_GET, []() {
    int limit = server.hasArg("limit") ? server.arg("limit").toInt() : 50;
    if (limit > 200)
      limit = 200;
    int sev = server.hasArg("sev") ? server.arg("sev").toInt() : -1;
    int cat = server.hasArg("cat") ? server.arg("cat").toInt() : -1;
    uint32_t since = server.hasArg("since") ? (uint32_t)server.arg("since").toInt() : 0;
    server.send(200, "application/json", evl_json_query(limit, sev, cat, since));
  });

  server.on("/api/evl/clear", HTTP_POST, []() {
    memset(&g_evl.ring, 0, sizeof(g_evl.ring));
    g_evl.head = 0;
    g_evl.total = 0;
    memset(g_evl.event_count_by_cat, 0, sizeof(g_evl.event_count_by_cat));
    memset(g_evl.event_count_by_sev, 0, sizeof(g_evl.event_count_by_sev));
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/evl/persist/enable", HTTP_POST, []() {
    g_evl.persist_enabled = true;
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/evl/persist/disable", HTTP_POST, []() {
    g_evl.persist_enabled = false;
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/evl/download", HTTP_GET, []() {
    if (!LittleFS.exists(EVL_LOG_FILE)) {
      server.send(404, "text/plain", "No log file");
      return;
    }
    File f = LittleFS.open(EVL_LOG_FILE, "r");
    server.sendHeader("Content-Disposition", "attachment; filename=phantom32_events.log");
    server.streamFile(f, "text/plain");
    f.close();
  });

  server.on("/api/evl/rotate", HTTP_POST, []() {
    LittleFS.remove("/events.log.1");
    LittleFS.rename(EVL_LOG_FILE, "/events.log.1");
    g_evl.bytes_written = 0;
    g_evl.rotations++;
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

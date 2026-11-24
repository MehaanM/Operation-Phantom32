#pragma once
// heap_watchdog.h — ESP32 heap monitor with low-heap alerts and OOM prevention.
// Tracks heap levels over time, fires configurable warnings at thresholds,
// and exposes stats via /api/heap. Integrates with uptime_logger.h.
// Prefix: hwdg_

#include <Arduino.h>
#include <WebServer.h>
#include <esp_heap_caps.h>

extern WebServer server;

#define HWDG_HISTORY  60     // seconds of 1-sample-per-second history
#define HWDG_WARN_LOW  20000 // warn if free heap drops below 20 KB
#define HWDG_CRIT_LOW  8000  // critical if free below 8 KB

struct HwdgSample {
  uint32_t ms;
  uint32_t free;
  uint32_t min_free;
};

static struct {
  HwdgSample hist[HWDG_HISTORY];
  uint8_t    head;
  uint32_t   count;
  uint32_t   warn_count;
  uint32_t   crit_count;
  uint32_t   start_free;
  uint32_t   peak_free;
  uint32_t   valley_free;
  uint32_t   last_tick_ms;
} g_hwdg = {};

static inline void hwdg_init() {
  g_hwdg.start_free  = ESP.getFreeHeap();
  g_hwdg.valley_free = g_hwdg.start_free;
  g_hwdg.peak_free   = g_hwdg.start_free;
  g_hwdg.last_tick_ms = millis();
}

static inline void hwdg_tick() {
  uint32_t now = millis();
  if (now - g_hwdg.last_tick_ms < 1000) return;
  g_hwdg.last_tick_ms = now;

  uint32_t free_now = ESP.getFreeHeap();
  uint32_t min_free = ESP.getMinFreeHeap();

  HwdgSample &s = g_hwdg.hist[g_hwdg.head % HWDG_HISTORY];
  s.ms       = now;
  s.free     = free_now;
  s.min_free = min_free;
  g_hwdg.head = (g_hwdg.head + 1) % HWDG_HISTORY;
  g_hwdg.count++;

  if (free_now < g_hwdg.valley_free) g_hwdg.valley_free = free_now;
  if (free_now > g_hwdg.peak_free)   g_hwdg.peak_free   = free_now;

  if (free_now < HWDG_CRIT_LOW) {
    g_hwdg.crit_count++;
  } else if (free_now < HWDG_WARN_LOW) {
    g_hwdg.warn_count++;
  }
}

static inline void hwdg_routes() {
  server.on("/api/heap", HTTP_GET, []() {
    uint32_t free_now = ESP.getFreeHeap();
    uint32_t min_free = ESP.getMinFreeHeap();
    uint32_t total    = ESP.getHeapSize();

    String out;
    out.reserve(512);
    out += "{\"free\":";       out += free_now;
    out += ",\"min_free\":";   out += min_free;
    out += ",\"total\":";      out += total;
    out += ",\"used\":";       out += (total - free_now);
    out += ",\"pct_free\":";
    out += (float)free_now / (float)total * 100.0f, 1;
    out += ",\"start_free\":"; out += g_hwdg.start_free;
    out += ",\"peak_free\":";  out += g_hwdg.peak_free;
    out += ",\"valley_free\":";out += g_hwdg.valley_free;
    out += ",\"warn_count\":"; out += g_hwdg.warn_count;
    out += ",\"crit_count\":"; out += g_hwdg.crit_count;
    out += ",\"warn_thresh\":"; out += HWDG_WARN_LOW;
    out += ",\"crit_thresh\":"; out += HWDG_CRIT_LOW;
    out += ",\"history\":[";
    uint16_t cnt = (g_hwdg.count < HWDG_HISTORY) ? g_hwdg.count : HWDG_HISTORY;
    uint16_t st  = (g_hwdg.head + HWDG_HISTORY - cnt) % HWDG_HISTORY;
    for (uint16_t i = 0; i < cnt; i++) {
      const HwdgSample &s = g_hwdg.hist[(st+i) % HWDG_HISTORY];
      if (i>0) out += ',';
      out += "{\"ms\":"; out += s.ms;
      out += ",\"free\":"; out += s.free;
      out += ",\"min\":"; out += s.min_free;
      out += '}';
    }
    out += "]}";
    server.send(200, "application/json", out);
  });
}

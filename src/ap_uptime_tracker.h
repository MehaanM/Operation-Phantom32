#pragma once
// ap_uptime_tracker.h — AP uptime estimation via TSF (Timing Synchronization Function).
// TSF is a 64-bit microsecond counter reset at each AP boot.
// Prefix: aut_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define AUT_MAX_APS 64
#define AUT_REBOOT_HIST 8  // number of reboot events to store

struct AutReboot {
  uint32_t ts_wall;     // wall-clock millis()/1000 when reboot was detected
  uint64_t tsf_before;  // last TSF before apparent reboot
  uint64_t tsf_after;   // first TSF seen after reboot
};

struct AutAP {
  bool active;
  uint8_t bssid[6];
  char ssid[33];
  uint8_t ch;
  int8_t rssi;

  // TSF tracking
  uint64_t first_tsf;   // first TSF ever recorded
  uint64_t last_tsf;    // most recent TSF
  uint32_t first_wall;  // wall time of first_tsf
  uint32_t last_wall;   // wall time of last_tsf

  // Uptime estimate (seconds since last AP reboot, inferred from TSF)
  uint32_t uptime_s;

  // Reboot history
  AutReboot reboots[AUT_REBOOT_HIST];
  uint8_t reboot_count;
  uint8_t reboot_head;

  // Drift correction: EMA of (observed TSF delta / expected TSF delta)
  int32_t drift_ppb;  // parts-per-billion drift, positive = fast
  uint8_t sample_count;

  uint32_t first_ts;
  uint32_t last_ts;
};

static struct {
  AutAP aps[AUT_MAX_APS];
  uint8_t count;
} g_aut = {};

static bool aut_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static AutAP *aut_find(const uint8_t *bssid) {
  for (int i = 0; i < AUT_MAX_APS; i++)
    if (g_aut.aps[i].active && aut_mac_eq(g_aut.aps[i].bssid, bssid))
      return &g_aut.aps[i];
  return nullptr;
}

static AutAP *aut_alloc(const uint8_t *bssid) {
  for (int i = 0; i < AUT_MAX_APS; i++) {
    if (!g_aut.aps[i].active) {
      AutAP &a = g_aut.aps[i];
      memset(&a, 0, sizeof(a));
      a.active = true;
      memcpy(a.bssid, bssid, 6);
      a.first_ts = millis() / 1000;
      g_aut.count++;
      return &a;
    }
  }
  // LRU eviction
  uint32_t oldest = UINT32_MAX;
  int idx = 0;
  for (int i = 0; i < AUT_MAX_APS; i++)
    if (g_aut.aps[i].last_ts < oldest) {
      oldest = g_aut.aps[i].last_ts;
      idx = i;
    }
  memset(&g_aut.aps[idx], 0, sizeof(g_aut.aps[0]));
  g_aut.aps[idx].active = true;
  memcpy(g_aut.aps[idx].bssid, bssid, 6);
  g_aut.aps[idx].first_ts = millis() / 1000;
  return &g_aut.aps[idx];
}

void aut_ingest_beacon(const uint8_t *bssid, const char *ssid, uint8_t ch, int8_t rssi,
                       uint32_t tsf_lo, uint32_t tsf_hi) {
  AutAP *a = aut_find(bssid);
  if (!a)
    a = aut_alloc(bssid);
  if (!a)
    return;

  if (ssid && ssid[0])
    strncpy(a->ssid, ssid, 32);
  a->ch = ch;
  if (rssi < 0)
    a->rssi = rssi;
  a->last_ts = millis() / 1000;

  uint64_t tsf = ((uint64_t)tsf_hi << 32) | tsf_lo;
  uint32_t wall = millis() / 1000;

  if (a->first_tsf == 0) {
    a->first_tsf = tsf;
    a->first_wall = wall;
    a->last_tsf = tsf;
    a->last_wall = wall;
    // Uptime = TSF / 1,000,000 (TSF is in microseconds)
    a->uptime_s = (uint32_t)(tsf / 1000000ULL);
    return;
  }

  // Detect reboot: TSF went backward significantly (>10 seconds)
  if (tsf < a->last_tsf && (a->last_tsf - tsf) > 10000000ULL) {
    // AP rebooted
    AutReboot &rb = a->reboots[a->reboot_head % AUT_REBOOT_HIST];
    rb.ts_wall = wall;
    rb.tsf_before = a->last_tsf;
    rb.tsf_after = tsf;
    a->reboot_head = (a->reboot_head + 1) % AUT_REBOOT_HIST;
    if (a->reboot_count < AUT_REBOOT_HIST)
      a->reboot_count++;
    // Reset baseline
    a->first_tsf = tsf;
    a->first_wall = wall;
  } else if (tsf > a->last_tsf) {
    // Normal advance: compute drift if we have wall-clock delta
    uint32_t wall_dt = wall - a->last_wall;
    if (wall_dt > 0 && a->sample_count < 200) {
      uint64_t tsf_dt = tsf - a->last_tsf;
      // Expected TSF delta for wall_dt seconds: wall_dt * 1,000,000
      uint64_t expected = (uint64_t)wall_dt * 1000000ULL;
      if (expected > 0) {
        int64_t diff = (int64_t)tsf_dt - (int64_t)expected;
        int32_t ppb = (int32_t)((diff * 1000000000LL) / (int64_t)expected);
        // EMA update
        if (a->sample_count == 0)
          a->drift_ppb = ppb;
        else
          a->drift_ppb = (a->drift_ppb * 7 + ppb) / 8;
        a->sample_count++;
      }
    }
  }

  a->last_tsf = tsf;
  a->last_wall = wall;
  // Uptime = time since first_tsf (accounting for drift roughly)
  a->uptime_s = (uint32_t)((tsf - a->first_tsf) / 1000000ULL) + 0;
}

static void aut_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String aut_uptime_pretty(uint32_t s) {
  char buf[32];
  if (s < 60) {
    snprintf(buf, sizeof(buf), "%us", s);
    return buf;
  }
  if (s < 3600) {
    snprintf(buf, sizeof(buf), "%um%02us", s / 60, s % 60);
    return buf;
  }
  if (s < 86400) {
    snprintf(buf, sizeof(buf), "%uh%02um", s / 3600, (s % 3600) / 60);
    return buf;
  }
  snprintf(buf, sizeof(buf), "%ud%02uh", s / 86400, (s % 86400) / 3600);
  return buf;
}

static String aut_json_all() {
  String s = "{\"count\":" + String(g_aut.count) + ",\"aps\":[";
  char mac[18];
  bool first = true;
  for (int i = 0; i < AUT_MAX_APS; i++) {
    AutAP &a = g_aut.aps[i];
    if (!a.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    aut_mac_str(a.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(a.ssid) + "\"";
    s += ",\"ch\":" + String(a.ch);
    s += ",\"rssi\":" + String(a.rssi);
    s += ",\"uptime_s\":" + String(a.uptime_s);
    s += ",\"uptime_str\":\"" + aut_uptime_pretty(a.uptime_s) + "\"";
    s += ",\"drift_ppb\":" + String(a.drift_ppb);
    s += ",\"reboots\":" + String(a.reboot_count);
    s += ",\"reboot_log\":[";
    bool fr = true;
    for (int j = 0; j < a.reboot_count; j++) {
      int idx = (a.reboot_head - a.reboot_count + j + AUT_REBOOT_HIST) % AUT_REBOOT_HIST;
      AutReboot &rb = a.reboots[idx];
      if (!fr)
        s += ",";
      fr = false;
      s += "{\"ts\":" + String(rb.ts_wall) + "}";
    }
    s += "]}";
  }
  s += "]}";
  return s;
}

inline void aut_routes() {
  server.on("/api/aut/all", HTTP_GET,
            []() { server.send(200, "application/json", aut_json_all()); });
  server.on("/api/aut/clear", HTTP_POST, []() {
    memset(&g_aut, 0, sizeof(g_aut));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

#pragma once
// rssi_smoother.h — Per-MAC RSSI smoothing using exponential moving average.
// Eliminates multipath jitter for distance estimation inputs. Prefix: rss_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define RSS_MAX_ENTRIES 128
#define RSS_ALPHA_NUM 3  // EMA alpha = 3/10
#define RSS_ALPHA_DEN 10

struct RssEntry {
  bool active;
  uint8_t mac[6];
  int16_t raw_sum;  // rolling raw samples (last 4)
  uint8_t raw_count;
  int8_t ema;  // smoothed RSSI (EMA)
  int8_t min_seen;
  int8_t max_seen;
  uint32_t last_ts;
};

static struct {
  RssEntry entries[RSS_MAX_ENTRIES];
  uint16_t count;
} g_rss = {};

static bool rss_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

int8_t rss_update(const uint8_t *mac, int8_t raw_rssi) {
  RssEntry *e = nullptr;
  for (int i = 0; i < RSS_MAX_ENTRIES; i++) {
    if (g_rss.entries[i].active && rss_mac_eq(g_rss.entries[i].mac, mac)) {
      e = &g_rss.entries[i];
      break;
    }
  }
  if (!e) {
    for (int i = 0; i < RSS_MAX_ENTRIES; i++) {
      if (!g_rss.entries[i].active) {
        e = &g_rss.entries[i];
        memset(e, 0, sizeof(*e));
        e->active = true;
        memcpy(e->mac, mac, 6);
        e->ema = raw_rssi;
        e->min_seen = raw_rssi;
        e->max_seen = raw_rssi;
        g_rss.count++;
        break;
      }
    }
    if (!e)
      return raw_rssi;  // table full — return raw
  }

  // EMA: ema_new = alpha * raw + (1 - alpha) * ema_old
  e->ema = (int8_t)((RSS_ALPHA_NUM * raw_rssi + (RSS_ALPHA_DEN - RSS_ALPHA_NUM) * e->ema) /
                    RSS_ALPHA_DEN);
  if (raw_rssi < e->min_seen)
    e->min_seen = raw_rssi;
  if (raw_rssi > e->max_seen)
    e->max_seen = raw_rssi;
  e->last_ts = millis() / 1000;
  return e->ema;
}

int8_t rss_get(const uint8_t *mac) {
  for (int i = 0; i < RSS_MAX_ENTRIES; i++)
    if (g_rss.entries[i].active && rss_mac_eq(g_rss.entries[i].mac, mac))
      return g_rss.entries[i].ema;
  return -100;
}

static void rss_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

inline void rss_routes() {
  server.on("/api/rss/all", HTTP_GET, []() {
    char mac[18];
    String s = "[";
    bool first = true;
    for (int i = 0; i < RSS_MAX_ENTRIES; i++) {
      RssEntry &e = g_rss.entries[i];
      if (!e.active)
        continue;
      if (!first)
        s += ",";
      first = false;
      rss_mac_str(e.mac, mac);
      s += "{\"mac\":\"" + String(mac) + "\"";
      s += ",\"ema\":" + String(e.ema);
      s += ",\"min\":" + String(e.min_seen);
      s += ",\"max\":" + String(e.max_seen) + "}";
    }
    s += "]";
    server.send(200, "application/json", s);
  });

  server.on("/api/rss/clear", HTTP_POST, []() {
    memset(&g_rss, 0, sizeof(g_rss));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

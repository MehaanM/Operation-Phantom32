#pragma once
// station_behavior.h — STA behavioral profiling: probe patterns, roaming, activity cycles.
// Prefix: stb_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define STB_MAX_CLIENTS 80
#define STB_MAX_PROBED 12  // SSIDs probed per client
#define STB_HIST_LEN 60    // seconds of activity history

struct StbProbedSSID {
  char ssid[33];
  uint16_t count;
  uint32_t last_ts;
};

// Activity histogram: 60 one-second buckets
struct StbActHist {
  uint8_t frames[STB_HIST_LEN];  // frame count per second (capped at 255)
  uint8_t head;
  uint32_t last_tick_s;
  uint8_t cur_frames;
};

struct StbClient {
  bool active;
  uint8_t mac[6];
  uint32_t first_ts;
  uint32_t last_ts;
  uint32_t frame_count;
  uint32_t probe_count;
  uint32_t data_count;
  uint32_t mgmt_count;

  // Probe behavior
  StbProbedSSID probed[STB_MAX_PROBED];
  uint8_t probed_count;
  uint8_t wildcard_probes;  // null SSID broadcasts

  // Current state
  int8_t rssi;
  uint8_t ch;
  bool is_associated;
  uint8_t assoc_bssid[6];
  char assoc_ssid[33];

  // Activity histogram
  StbActHist hist;

  // Timing derived
  uint32_t duty_cycle_pct;  // % of time with activity
  uint16_t inter_probe_ms;  // EMA inter-probe interval
  uint32_t last_probe_ts_ms;

  // Roaming
  uint8_t roam_count;  // number of AP changes detected
  uint8_t prev_bssid[6];
};

static struct {
  StbClient clients[STB_MAX_CLIENTS];
  uint8_t count;
} g_stb = {};

static bool stb_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static bool stb_is_bcast(const uint8_t *m) {
  return m[0] == 0xFF && m[1] == 0xFF && m[2] == 0xFF && m[3] == 0xFF && m[4] == 0xFF &&
         m[5] == 0xFF;
}
static bool stb_is_mcast(const uint8_t *m) {
  return m[0] & 0x01;
}

static StbClient *stb_find(const uint8_t *mac) {
  for (int i = 0; i < STB_MAX_CLIENTS; i++)
    if (g_stb.clients[i].active && stb_mac_eq(g_stb.clients[i].mac, mac))
      return &g_stb.clients[i];
  return nullptr;
}

static StbClient *stb_alloc(const uint8_t *mac) {
  for (int i = 0; i < STB_MAX_CLIENTS; i++) {
    if (!g_stb.clients[i].active) {
      StbClient &c = g_stb.clients[i];
      memset(&c, 0, sizeof(c));
      c.active = true;
      memcpy(c.mac, mac, 6);
      c.first_ts = millis() / 1000;
      g_stb.count++;
      return &c;
    }
  }
  uint32_t oldest = UINT32_MAX;
  int idx = 0;
  for (int i = 0; i < STB_MAX_CLIENTS; i++)
    if (g_stb.clients[i].last_ts < oldest) {
      oldest = g_stb.clients[i].last_ts;
      idx = i;
    }
  memset(&g_stb.clients[idx], 0, sizeof(g_stb.clients[0]));
  g_stb.clients[idx].active = true;
  memcpy(g_stb.clients[idx].mac, mac, 6);
  g_stb.clients[idx].first_ts = millis() / 1000;
  return &g_stb.clients[idx];
}

static void stb_tick_hist(StbClient &c) {
  uint32_t now_s = millis() / 1000;
  if (now_s == c.hist.last_tick_s) {
    if (c.hist.cur_frames < 255)
      c.hist.cur_frames++;
    return;
  }
  // Store accumulated bucket
  c.hist.frames[c.hist.head % STB_HIST_LEN] = c.hist.cur_frames;
  c.hist.head = (c.hist.head + 1) % STB_HIST_LEN;
  c.hist.cur_frames = 1;
  c.hist.last_tick_s = now_s;

  // Compute duty cycle: fraction of non-zero buckets
  uint8_t active_buckets = 0;
  for (int i = 0; i < STB_HIST_LEN; i++)
    if (c.hist.frames[i] > 0)
      active_buckets++;
  c.duty_cycle_pct = (active_buckets * 100) / STB_HIST_LEN;
}

void stb_ingest_probe(const uint8_t *client_mac, const char *ssid, uint8_t ch, int8_t rssi) {
  if (stb_is_bcast(client_mac) || stb_is_mcast(client_mac))
    return;

  StbClient *c = stb_find(client_mac);
  if (!c)
    c = stb_alloc(client_mac);
  if (!c)
    return;

  c->last_ts = millis() / 1000;
  c->frame_count++;
  c->probe_count++;
  c->mgmt_count++;
  if (ch)
    c->ch = ch;
  if (rssi < 0)
    c->rssi = rssi;
  stb_tick_hist(*c);

  // Inter-probe interval EMA
  uint32_t now_ms = millis();
  if (c->last_probe_ts_ms) {
    uint32_t delta = now_ms - c->last_probe_ts_ms;
    if (c->inter_probe_ms == 0)
      c->inter_probe_ms = (uint16_t)min((uint32_t)65535, delta);
    else
      c->inter_probe_ms =
          (uint16_t)min((uint32_t)65535, (uint32_t)((c->inter_probe_ms * 7 + delta) / 8));
  }
  c->last_probe_ts_ms = now_ms;

  // Log probed SSID
  bool wild = (!ssid || !ssid[0]);
  if (wild) {
    c->wildcard_probes++;
    return;
  }
  for (int i = 0; i < c->probed_count; i++) {
    if (strcmp(c->probed[i].ssid, ssid) == 0) {
      c->probed[i].count++;
      c->probed[i].last_ts = c->last_ts;
      return;
    }
  }
  if (c->probed_count < STB_MAX_PROBED) {
    StbProbedSSID &p = c->probed[c->probed_count++];
    strncpy(p.ssid, ssid, 32);
    p.count = 1;
    p.last_ts = c->last_ts;
  } else {
    uint16_t min_cnt = UINT16_MAX;
    int idx = 0;
    for (int i = 0; i < STB_MAX_PROBED; i++)
      if (c->probed[i].count < min_cnt) {
        min_cnt = c->probed[i].count;
        idx = i;
      }
    strncpy(c->probed[idx].ssid, ssid, 32);
    c->probed[idx].count = 1;
    c->probed[idx].last_ts = c->last_ts;
  }
}

void stb_ingest_data(const uint8_t *client_mac, const uint8_t *ap_bssid, const char *ssid,
                     uint8_t ch, int8_t rssi) {
  if (stb_is_bcast(client_mac) || stb_is_mcast(client_mac))
    return;

  StbClient *c = stb_find(client_mac);
  if (!c)
    c = stb_alloc(client_mac);
  if (!c)
    return;

  c->last_ts = millis() / 1000;
  c->frame_count++;
  c->data_count++;
  if (ch)
    c->ch = ch;
  if (rssi < 0)
    c->rssi = rssi;
  stb_tick_hist(*c);

  // Check for roaming
  if (ap_bssid && !stb_mac_eq(c->assoc_bssid, ap_bssid)) {
    if (c->is_associated) {
      c->roam_count++;
      memcpy(c->prev_bssid, c->assoc_bssid, 6);
    }
    memcpy(c->assoc_bssid, ap_bssid, 6);
    c->is_associated = true;
    if (ssid && ssid[0])
      strncpy(c->assoc_ssid, ssid, 32);
  }
}

void stb_ingest_mgmt(const uint8_t *client_mac, uint8_t ch, int8_t rssi) {
  if (stb_is_bcast(client_mac) || stb_is_mcast(client_mac))
    return;

  StbClient *c = stb_find(client_mac);
  if (!c)
    c = stb_alloc(client_mac);
  if (!c)
    return;

  c->last_ts = millis() / 1000;
  c->frame_count++;
  c->mgmt_count++;
  if (ch)
    c->ch = ch;
  if (rssi < 0)
    c->rssi = rssi;
  stb_tick_hist(*c);
}

static void stb_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String stb_hist_json(const StbClient &c) {
  String s = "[";
  for (int i = 0; i < STB_HIST_LEN; i++) {
    int idx = (c.hist.head + i) % STB_HIST_LEN;
    if (i)
      s += ",";
    s += String(c.hist.frames[idx]);
  }
  return s + "]";
}

static String stb_json_all() {
  String s = "{\"count\":" + String(g_stb.count);
  s += ",\"clients\":[";
  char mac[18], apmac[18];
  bool first = true;
  for (int i = 0; i < STB_MAX_CLIENTS; i++) {
    StbClient &c = g_stb.clients[i];
    if (!c.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    stb_mac_str(c.mac, mac);
    stb_mac_str(c.assoc_bssid, apmac);
    s += "{\"mac\":\"" + String(mac) + "\"";
    s += ",\"rssi\":" + String(c.rssi);
    s += ",\"ch\":" + String(c.ch);
    s += ",\"frames\":" + String(c.frame_count);
    s += ",\"probes\":" + String(c.probe_count);
    s += ",\"wildcard_probes\":" + String(c.wildcard_probes);
    s += ",\"data\":" + String(c.data_count);
    s += ",\"mgmt\":" + String(c.mgmt_count);
    s += ",\"duty_cycle\":" + String(c.duty_cycle_pct);
    s += ",\"inter_probe_ms\":" + String(c.inter_probe_ms);
    s += ",\"roams\":" + String(c.roam_count);
    s += ",\"assoc\":" + String(c.is_associated ? "true" : "false");
    s += ",\"assoc_bssid\":\"" + String(apmac) + "\"";
    s += ",\"assoc_ssid\":\"" + String(c.assoc_ssid) + "\"";
    s += ",\"hist\":" + stb_hist_json(c);
    s += ",\"probed_ssids\":[";
    bool fp = true;
    for (int j = 0; j < c.probed_count; j++) {
      if (!fp)
        s += ",";
      fp = false;
      s += "{\"ssid\":\"" + String(c.probed[j].ssid) + "\"";
      s += ",\"n\":" + String(c.probed[j].count) + "}";
    }
    s += "]}";
  }
  s += "]}";
  return s;
}

inline void stb_routes() {
  server.on("/api/stb/all", HTTP_GET,
            []() { server.send(200, "application/json", stb_json_all()); });
  server.on("/api/stb/clear", HTTP_POST, []() {
    memset(&g_stb, 0, sizeof(g_stb));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

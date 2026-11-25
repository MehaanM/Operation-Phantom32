#pragma once
// beacon_anomaly.h — Beacon anomaly detector.
// Detects unexpected beacon characteristics: TSF rollover, sudden SSID changes,
// impossible beacon intervals, and malformed IE sequences.
// Prefix: ban_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define BAN_MAX_APS 64
#define BAN_ALERT_BUF 64
#define BAN_TSF_HIST 8  // last N TSF values

enum BanAnomalyType : uint8_t {
  BAN_TSF_JUMP = 0,       // TSF jumped backward or forward impossibly far
  BAN_SSID_CHANGE = 1,    // SSID changed mid-stream
  BAN_BINT_ANOMALY = 2,   // beacon interval changed or out of range
  BAN_RAPID_BEACON = 3,   // beacons arriving too fast (< 10ms apart)
  BAN_MISSING_BURST = 4,  // missed beacon burst (> 10x interval gap)
  BAN_MULTI_SSID = 5,     // same BSSID different SSIDs
};

struct BanAlert {
  bool active;
  uint32_t ts;
  uint8_t bssid[6];
  char ssid[33];
  BanAnomalyType type;
  char detail[80];
};

struct BanAP {
  bool active;
  uint8_t bssid[6];
  char ssid[33];
  uint8_t ch;
  int8_t rssi;
  uint32_t first_ts;
  uint32_t last_ts;
  uint32_t beacon_count;
  uint32_t anomaly_count;

  // TSF tracking (lower 32 bits for ESP simplicity)
  uint32_t tsf_hist[BAN_TSF_HIST];
  uint8_t tsf_head;
  uint8_t tsf_count;
  bool tsf_monotonic;

  // Beacon interval tracking
  uint16_t beacon_int_tu;
  uint16_t last_beacon_int;

  // Timing
  uint32_t last_rx_ms;
  uint32_t min_inter_beacon_ms;
  uint32_t avg_inter_beacon_ms;

  // SSID history (detect changes)
  char ssid_hist[4][33];
  uint8_t ssid_count;
};

static struct {
  BanAP aps[BAN_MAX_APS];
  uint8_t count;
  BanAlert alerts[BAN_ALERT_BUF];
  uint8_t alert_head;
  uint32_t total_anomalies;
} g_ban = {};

static bool ban_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static BanAP *ban_find(const uint8_t *bssid) {
  for (int i = 0; i < BAN_MAX_APS; i++)
    if (g_ban.aps[i].active && ban_mac_eq(g_ban.aps[i].bssid, bssid))
      return &g_ban.aps[i];
  return nullptr;
}

static BanAP *ban_alloc(const uint8_t *bssid) {
  for (int i = 0; i < BAN_MAX_APS; i++) {
    if (!g_ban.aps[i].active) {
      BanAP &a = g_ban.aps[i];
      memset(&a, 0, sizeof(a));
      a.active = true;
      memcpy(a.bssid, bssid, 6);
      a.tsf_monotonic = true;
      a.min_inter_beacon_ms = UINT32_MAX;
      a.first_ts = millis() / 1000;
      g_ban.count++;
      return &a;
    }
  }
  uint32_t oldest = UINT32_MAX;
  int idx = 0;
  for (int i = 0; i < BAN_MAX_APS; i++)
    if (g_ban.aps[i].last_ts < oldest) {
      oldest = g_ban.aps[i].last_ts;
      idx = i;
    }
  memset(&g_ban.aps[idx], 0, sizeof(g_ban.aps[0]));
  g_ban.aps[idx].active = true;
  memcpy(g_ban.aps[idx].bssid, bssid, 6);
  g_ban.aps[idx].tsf_monotonic = true;
  g_ban.aps[idx].min_inter_beacon_ms = UINT32_MAX;
  g_ban.aps[idx].first_ts = millis() / 1000;
  return &g_ban.aps[idx];
}

static void ban_alert(BanAP &a, BanAnomalyType type, const char *detail) {
  BanAlert &al = g_ban.alerts[g_ban.alert_head % BAN_ALERT_BUF];
  al.active = true;
  al.ts = millis() / 1000;
  memcpy(al.bssid, a.bssid, 6);
  strncpy(al.ssid, a.ssid, 32);
  al.type = type;
  if (detail)
    strncpy(al.detail, detail, 79);
  g_ban.alert_head = (g_ban.alert_head + 1) % BAN_ALERT_BUF;
  g_ban.total_anomalies++;
  a.anomaly_count++;
}

void ban_ingest_beacon(const uint8_t *bssid, const char *ssid, uint8_t ch, int8_t rssi,
                       uint32_t tsf_lo, uint16_t beacon_int_tu) {
  BanAP *a = ban_find(bssid);
  if (!a)
    a = ban_alloc(bssid);
  if (!a)
    return;

  uint32_t now_ms = millis();
  a->last_ts = now_ms / 1000;
  a->beacon_count++;
  a->ch = ch;
  if (rssi < 0)
    a->rssi = rssi;

  // --- SSID change detection ---
  if (ssid && ssid[0]) {
    bool ssid_known = false;
    for (int i = 0; i < a->ssid_count; i++)
      if (strcmp(a->ssid_hist[i], ssid) == 0) {
        ssid_known = true;
        break;
      }

    if (!ssid_known) {
      if (a->ssid_count > 0) {
        char detail[80];
        snprintf(detail, 80, "Was '%s', now '%s'", a->ssid, ssid);
        ban_alert(*a, BAN_SSID_CHANGE, detail);
      }
      if (a->ssid_count < 4)
        strncpy(a->ssid_hist[a->ssid_count++], ssid, 32);
      strncpy(a->ssid, ssid, 32);
    }
  }

  // --- TSF monotonicity check ---
  if (a->tsf_count > 0) {
    uint32_t prev_tsf = a->tsf_hist[(a->tsf_head - 1 + BAN_TSF_HIST) % BAN_TSF_HIST];
    if (tsf_lo < prev_tsf) {
      // Allow wraparound, but flag huge jumps backward
      if (prev_tsf - tsf_lo > 0x10000000) {
        ban_alert(*a, BAN_TSF_JUMP, "TSF jumped backward without wrap");
        a->tsf_monotonic = false;
      }
    } else {
      uint32_t tsf_delta = tsf_lo - prev_tsf;
      // TSF increments at 1 MHz; beacon at ~100ms = 100,000 us delta typically
      if (tsf_delta > 1000000000UL) {  // > 1000s jump
        ban_alert(*a, BAN_TSF_JUMP, "TSF jumped forward impossibly far");
      }
    }
  }
  a->tsf_hist[a->tsf_head % BAN_TSF_HIST] = tsf_lo;
  a->tsf_head = (a->tsf_head + 1) % BAN_TSF_HIST;
  if (a->tsf_count < BAN_TSF_HIST)
    a->tsf_count++;

  // --- Beacon interval anomaly ---
  if (beacon_int_tu > 0) {
    if (a->beacon_int_tu != 0 && abs((int)beacon_int_tu - (int)a->beacon_int_tu) > 20) {
      char detail[80];
      snprintf(detail, 80, "Interval was %d TU, now %d TU", a->beacon_int_tu, beacon_int_tu);
      ban_alert(*a, BAN_BINT_ANOMALY, detail);
    }
    if (beacon_int_tu < 15 || beacon_int_tu > 1024) {
      char detail[80];
      snprintf(detail, 80, "Interval %d TU is out of valid range [15,1024]", beacon_int_tu);
      ban_alert(*a, BAN_BINT_ANOMALY, detail);
    }
    a->beacon_int_tu = beacon_int_tu;
  }

  // --- Inter-beacon timing ---
  if (a->last_rx_ms > 0) {
    uint32_t delta_ms = now_ms - a->last_rx_ms;

    if (delta_ms < 10 && delta_ms > 0) {
      ban_alert(*a, BAN_RAPID_BEACON, "Beacons arriving < 10ms apart — injection suspected");
    }

    uint32_t nominal_ms = (uint32_t)a->beacon_int_tu * 1024 / 1000;
    if (nominal_ms > 0 && delta_ms > nominal_ms * 10) {
      ban_alert(*a, BAN_MISSING_BURST, "Beacon gap > 10x nominal interval");
    }

    if (delta_ms < a->min_inter_beacon_ms)
      a->min_inter_beacon_ms = delta_ms;
    if (a->avg_inter_beacon_ms == 0)
      a->avg_inter_beacon_ms = delta_ms;
    else
      a->avg_inter_beacon_ms = (uint32_t)((a->avg_inter_beacon_ms * 7 + delta_ms) / 8);
  }
  a->last_rx_ms = now_ms;
}

static const char *ban_type_str(BanAnomalyType t) {
  switch (t) {
    case BAN_TSF_JUMP:
      return "TSF Jump";
    case BAN_SSID_CHANGE:
      return "SSID Change";
    case BAN_BINT_ANOMALY:
      return "Beacon Interval";
    case BAN_RAPID_BEACON:
      return "Rapid Beacon";
    case BAN_MISSING_BURST:
      return "Missing Burst";
    case BAN_MULTI_SSID:
      return "Multi SSID";
    default:
      return "Unknown";
  }
}

static void ban_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String ban_json_all() {
  String s = "{\"count\":" + String(g_ban.count);
  s += ",\"total_anomalies\":" + String(g_ban.total_anomalies);
  s += ",\"aps\":[";
  char mac[18];
  bool first = true;
  for (int i = 0; i < BAN_MAX_APS; i++) {
    BanAP &a = g_ban.aps[i];
    if (!a.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    ban_mac_str(a.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(a.ssid) + "\"";
    s += ",\"ch\":" + String(a.ch);
    s += ",\"rssi\":" + String(a.rssi);
    s += ",\"beacons\":" + String(a.beacon_count);
    s += ",\"anomalies\":" + String(a.anomaly_count);
    s += ",\"tsf_ok\":" + String(a.tsf_monotonic ? "true" : "false");
    s += ",\"bint\":" + String(a.beacon_int_tu);
    s += ",\"avg_inter_ms\":" + String(a.avg_inter_beacon_ms);
    s += ",\"min_inter_ms\":" +
         String(a.min_inter_beacon_ms == UINT32_MAX ? 0 : a.min_inter_beacon_ms);
    s += ",\"ssid_count\":" + String(a.ssid_count);
    s += "}";
  }
  s += "],\"alerts\":[";
  uint32_t cnt = g_ban.total_anomalies;
  uint32_t num = min((uint32_t)BAN_ALERT_BUF, cnt);
  uint32_t start = (cnt > BAN_ALERT_BUF) ? g_ban.alert_head : 0;
  first = true;
  for (uint32_t i = 0; i < num; i++) {
    uint32_t idx = (start + i) % BAN_ALERT_BUF;
    BanAlert &al = g_ban.alerts[idx];
    if (!al.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    ban_mac_str(al.bssid, mac);
    s += "{\"ts\":" + String(al.ts);
    s += ",\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(al.ssid) + "\"";
    s += ",\"type\":\"" + String(ban_type_str(al.type)) + "\"";
    s += ",\"type_code\":" + String(al.type);
    s += ",\"detail\":\"" + String(al.detail) + "\"";
    s += "}";
  }
  s += "]}";
  return s;
}

inline void ban_routes() {
  server.on("/api/ban/all", HTTP_GET,
            []() { server.send(200, "application/json", ban_json_all()); });
  server.on("/api/ban/clear", HTTP_POST, []() {
    memset(&g_ban, 0, sizeof(g_ban));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

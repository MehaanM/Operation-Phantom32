#pragma once
// ssid_collision.h — SSID collision detector: finds APs sharing identical SSIDs.
// Key indicator for evil twin attacks, misconfigured networks, or SSID spoofing.
// Prefix: ssc_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define SSC_MAX_SSIDS 64
#define SSC_MAX_APS_PER 16  // APs per SSID bucket
#define SSC_ALERT_BUF 48

enum SscRisk : uint8_t {
  SSC_RISK_NONE = 0,    // single AP, no collision
  SSC_RISK_LOW = 1,     // same SSID, same BSSID prefix (same vendor) — may be mesh
  SSC_RISK_MEDIUM = 2,  // same SSID, different OUI, same channel
  SSC_RISK_HIGH = 3,    // same SSID, different OUI, different channel
  SSC_RISK_CRIT = 4,    // same SSID, different security levels
};

struct SscEntry {
  uint8_t bssid[6];
  uint8_t ch;
  int8_t rssi;
  uint8_t security;  // 0=open,1=WEP,2=WPA,3=WPA2,4=WPA3
  uint32_t first_ts;
  uint32_t last_ts;
};

struct SscBucket {
  bool active;
  char ssid[33];
  SscEntry aps[SSC_MAX_APS_PER];
  uint8_t ap_count;
  SscRisk risk;
  uint32_t last_ts;
  uint32_t collision_count;  // number of times we re-evaluated risk
};

struct SscAlert {
  bool active;
  uint32_t ts;
  char ssid[33];
  uint8_t bssid_a[6];
  uint8_t bssid_b[6];
  SscRisk risk;
  char reason[64];
};

static struct {
  SscBucket buckets[SSC_MAX_SSIDS];
  uint8_t count;
  SscAlert alerts[SSC_ALERT_BUF];
  uint8_t alert_head;
  uint32_t total_alerts;
} g_ssc = {};

static bool ssc_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}
static bool ssc_oui_eq(const uint8_t *a, const uint8_t *b) {
  return a[0] == b[0] && a[1] == b[1] && a[2] == b[2];
}

static SscBucket *ssc_find_ssid(const char *ssid) {
  for (int i = 0; i < SSC_MAX_SSIDS; i++)
    if (g_ssc.buckets[i].active && strcmp(g_ssc.buckets[i].ssid, ssid) == 0)
      return &g_ssc.buckets[i];
  return nullptr;
}

static SscBucket *ssc_alloc_ssid(const char *ssid) {
  for (int i = 0; i < SSC_MAX_SSIDS; i++) {
    if (!g_ssc.buckets[i].active) {
      SscBucket &b = g_ssc.buckets[i];
      memset(&b, 0, sizeof(b));
      b.active = true;
      strncpy(b.ssid, ssid, 32);
      g_ssc.count++;
      return &b;
    }
  }
  uint32_t oldest = UINT32_MAX;
  int idx = 0;
  for (int i = 0; i < SSC_MAX_SSIDS; i++)
    if (g_ssc.buckets[i].last_ts < oldest) {
      oldest = g_ssc.buckets[i].last_ts;
      idx = i;
    }
  memset(&g_ssc.buckets[idx], 0, sizeof(g_ssc.buckets[0]));
  g_ssc.buckets[idx].active = true;
  strncpy(g_ssc.buckets[idx].ssid, ssid, 32);
  return &g_ssc.buckets[idx];
}

static void ssc_add_alert(const char *ssid, const uint8_t *ba, const uint8_t *bb, SscRisk risk,
                          const char *reason) {
  SscAlert &a = g_ssc.alerts[g_ssc.alert_head % SSC_ALERT_BUF];
  a.active = true;
  a.ts = millis() / 1000;
  strncpy(a.ssid, ssid, 32);
  if (ba)
    memcpy(a.bssid_a, ba, 6);
  if (bb)
    memcpy(a.bssid_b, bb, 6);
  a.risk = risk;
  strncpy(a.reason, reason, 63);
  g_ssc.alert_head = (g_ssc.alert_head + 1) % SSC_ALERT_BUF;
  g_ssc.total_alerts++;
}

static void ssc_evaluate_risk(SscBucket &b) {
  if (b.ap_count < 2) {
    b.risk = SSC_RISK_NONE;
    return;
  }
  SscRisk worst = SSC_RISK_NONE;
  for (int i = 0; i < b.ap_count; i++) {
    for (int j = i + 1; j < b.ap_count; j++) {
      SscEntry &a = b.aps[i];
      SscEntry &c = b.aps[j];
      SscRisk r = SSC_RISK_LOW;

      if (a.security != c.security) {
        r = SSC_RISK_CRIT;
        if (r > worst) {
          worst = r;
          ssc_add_alert(b.ssid, a.bssid, c.bssid, r,
                        "Different security modes on same SSID — evil twin suspected");
        }
      } else if (!ssc_oui_eq(a.bssid, c.bssid)) {
        if (a.ch != c.ch) {
          r = SSC_RISK_HIGH;
          if (r > worst) {
            worst = r;
            ssc_add_alert(b.ssid, a.bssid, c.bssid, r,
                          "Different OUI and channel — unauthorized AP?");
          }
        } else {
          r = SSC_RISK_MEDIUM;
          if (r > worst) {
            worst = r;
            ssc_add_alert(b.ssid, a.bssid, c.bssid, r,
                          "Different OUI, same channel — possible rogue AP");
          }
        }
      }
    }
  }
  b.risk = worst;
  b.collision_count++;
}

void ssc_ingest(const uint8_t *bssid, const char *ssid, uint8_t ch, int8_t rssi, uint8_t security) {
  if (!ssid || !ssid[0])
    return;

  SscBucket *b = ssc_find_ssid(ssid);
  if (!b)
    b = ssc_alloc_ssid(ssid);
  if (!b)
    return;

  b->last_ts = millis() / 1000;

  // Find or add AP entry
  for (int i = 0; i < b->ap_count; i++) {
    if (ssc_mac_eq(b->aps[i].bssid, bssid)) {
      b->aps[i].ch = ch;
      b->aps[i].rssi = rssi;
      b->aps[i].security = security;
      b->aps[i].last_ts = millis() / 1000;
      ssc_evaluate_risk(*b);
      return;
    }
  }
  if (b->ap_count < SSC_MAX_APS_PER) {
    SscEntry &e = b->aps[b->ap_count++];
    memcpy(e.bssid, bssid, 6);
    e.ch = ch;
    e.rssi = rssi;
    e.security = security;
    e.first_ts = e.last_ts = millis() / 1000;
    ssc_evaluate_risk(*b);
  }
}

static const char *ssc_risk_str(SscRisk r) {
  switch (r) {
    case SSC_RISK_NONE:
      return "NONE";
    case SSC_RISK_LOW:
      return "LOW";
    case SSC_RISK_MEDIUM:
      return "MEDIUM";
    case SSC_RISK_HIGH:
      return "HIGH";
    case SSC_RISK_CRIT:
      return "CRITICAL";
    default:
      return "UNKNOWN";
  }
}

static void ssc_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String ssc_json_all() {
  String s = "{\"count\":" + String(g_ssc.count);
  s += ",\"total_alerts\":" + String(g_ssc.total_alerts);
  s += ",\"ssids\":[";
  char mac[18];
  bool first = true;
  for (int i = 0; i < SSC_MAX_SSIDS; i++) {
    SscBucket &b = g_ssc.buckets[i];
    if (!b.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    s += "{\"ssid\":\"" + String(b.ssid) + "\"";
    s += ",\"ap_count\":" + String(b.ap_count);
    s += ",\"risk\":\"" + String(ssc_risk_str(b.risk)) + "\"";
    s += ",\"risk_code\":" + String(b.risk);
    s += ",\"collisions\":" + String(b.collision_count);
    s += ",\"aps\":[";
    bool fa = true;
    for (int j = 0; j < b.ap_count; j++) {
      SscEntry &e = b.aps[j];
      if (!fa)
        s += ",";
      fa = false;
      ssc_mac_str(e.bssid, mac);
      s += "{\"bssid\":\"" + String(mac) + "\"";
      s += ",\"ch\":" + String(e.ch);
      s += ",\"rssi\":" + String(e.rssi);
      s += ",\"sec\":" + String(e.security) + "}";
    }
    s += "]}";
  }
  s += "],\"alerts\":[";
  uint32_t cnt = g_ssc.total_alerts;
  uint32_t num = min((uint32_t)SSC_ALERT_BUF, cnt);
  uint32_t start = (cnt > SSC_ALERT_BUF) ? g_ssc.alert_head : 0;
  first = true;
  for (uint32_t i = 0; i < num; i++) {
    uint32_t idx = (start + i) % SSC_ALERT_BUF;
    SscAlert &a = g_ssc.alerts[idx];
    if (!a.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    char ma[18], mb[18];
    ssc_mac_str(a.bssid_a, ma);
    ssc_mac_str(a.bssid_b, mb);
    s += "{\"ts\":" + String(a.ts);
    s += ",\"ssid\":\"" + String(a.ssid) + "\"";
    s += ",\"bssid_a\":\"" + String(ma) + "\"";
    s += ",\"bssid_b\":\"" + String(mb) + "\"";
    s += ",\"risk\":\"" + String(ssc_risk_str(a.risk)) + "\"";
    s += ",\"risk_code\":" + String(a.risk);
    s += ",\"reason\":\"" + String(a.reason) + "\"";
    s += "}";
  }
  s += "]}";
  return s;
}

inline void ssc_routes() {
  server.on("/api/ssc/all", HTTP_GET,
            []() { server.send(200, "application/json", ssc_json_all()); });
  server.on("/api/ssc/clear", HTTP_POST, []() {
    memset(&g_ssc, 0, sizeof(g_ssc));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

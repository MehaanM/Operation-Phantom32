#pragma once
// rogue_ap_detector.h — Rogue/unauthorized AP detection.
// Detects APs that share an SSID with known-good APs but differ in
// BSSID OUI, security mode, channel, or capabilities — classic evil-twin
// fingerprinting beyond the basic SSID-collision check.
// Prefix: rgd_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define RGD_MAX_KNOWN 32
#define RGD_MAX_ROGUE 32
#define RGD_MAX_ALERTS 64

enum RgdRisk : uint8_t {
  RGD_OK = 0,
  RGD_SUSPICIOUS = 1,
  RGD_PROBABLE = 2,
  RGD_CONFIRMED = 3,
};

static const char *rgd_risk_name(RgdRisk r) {
  switch (r) {
    case RGD_OK:
      return "OK";
    case RGD_SUSPICIOUS:
      return "Suspicious";
    case RGD_PROBABLE:
      return "Probable";
    case RGD_CONFIRMED:
      return "Confirmed";
    default:
      return "Unknown";
  }
}

struct RgdApProfile {
  bool active;
  uint8_t bssid[6];
  char ssid[33];
  uint8_t ch;
  int8_t rssi;
  uint8_t oui[3];
  uint8_t security;  // 0=open,1=WEP,2=WPA,3=WPA2,4=WPA3
  bool pmf;
  bool has_wps;
  bool has_ht;
  bool has_vht;
  bool has_he;
  uint16_t beacon_int_tu;
  uint32_t last_ts;
  uint32_t first_ts;
  bool is_known;  // explicitly marked as legitimate
};

struct RgdAlert {
  uint32_t ts;
  uint8_t rogue_bssid[6];
  uint8_t legit_bssid[6];
  char ssid[33];
  RgdRisk risk;
  char reason[64];  // human-readable detection reason
};

static struct {
  RgdApProfile aps[RGD_MAX_KNOWN + RGD_MAX_ROGUE];
  uint8_t ap_count;
  RgdAlert alerts[RGD_MAX_ALERTS];
  uint8_t alert_head;
  uint16_t total_alerts;
} g_rgd = {};

static bool rgd_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static RgdApProfile *rgd_find(const uint8_t *bssid) {
  for (int i = 0; i < RGD_MAX_KNOWN + RGD_MAX_ROGUE; i++)
    if (g_rgd.aps[i].active && rgd_mac_eq(g_rgd.aps[i].bssid, bssid))
      return &g_rgd.aps[i];
  return nullptr;
}

static RgdApProfile *rgd_alloc(const uint8_t *bssid) {
  for (int i = 0; i < RGD_MAX_KNOWN + RGD_MAX_ROGUE; i++) {
    if (!g_rgd.aps[i].active) {
      RgdApProfile &a = g_rgd.aps[i];
      memset(&a, 0, sizeof(a));
      a.active = true;
      memcpy(a.bssid, bssid, 6);
      memcpy(a.oui, bssid, 3);
      a.first_ts = millis() / 1000;
      g_rgd.ap_count++;
      return &a;
    }
  }
  uint32_t oldest = UINT32_MAX;
  int idx = 0;
  for (int i = 0; i < RGD_MAX_KNOWN + RGD_MAX_ROGUE; i++)
    if (g_rgd.aps[i].last_ts < oldest && !g_rgd.aps[i].is_known) {
      oldest = g_rgd.aps[i].last_ts;
      idx = i;
    }
  memset(&g_rgd.aps[idx], 0, sizeof(g_rgd.aps[0]));
  g_rgd.aps[idx].active = true;
  memcpy(g_rgd.aps[idx].bssid, bssid, 6);
  memcpy(g_rgd.aps[idx].oui, bssid, 3);
  g_rgd.aps[idx].first_ts = millis() / 1000;
  return &g_rgd.aps[idx];
}

static void rgd_add_alert(const uint8_t *rogue, const uint8_t *legit, const char *ssid,
                          RgdRisk risk, const char *reason) {
  RgdAlert &al = g_rgd.alerts[g_rgd.alert_head % RGD_MAX_ALERTS];
  al.ts = millis() / 1000;
  memcpy(al.rogue_bssid, rogue, 6);
  memcpy(al.legit_bssid, legit, 6);
  strncpy(al.ssid, ssid, 32);
  al.risk = risk;
  strncpy(al.reason, reason, 63);
  g_rgd.alert_head = (g_rgd.alert_head + 1) % RGD_MAX_ALERTS;
  g_rgd.total_alerts++;
}

static void rgd_compare(RgdApProfile &candidate) {
  for (int i = 0; i < RGD_MAX_KNOWN + RGD_MAX_ROGUE; i++) {
    RgdApProfile &ref = g_rgd.aps[i];
    if (!ref.active)
      continue;
    if (!ref.is_known)
      continue;
    if (rgd_mac_eq(ref.bssid, candidate.bssid))
      continue;
    if (strncmp(ref.ssid, candidate.ssid, 32) != 0)
      continue;

    // Same SSID, different BSSID — examine differences
    char reason[64] = "";
    RgdRisk risk = RGD_SUSPICIOUS;

    // Different OUI (vendor) — most common rogue indicator
    if (memcmp(ref.oui, candidate.oui, 3) != 0) {
      risk = RGD_PROBABLE;
      snprintf(reason, 63, "OUI mismatch: %02X:%02X:%02X vs %02X:%02X:%02X", ref.oui[0], ref.oui[1],
               ref.oui[2], candidate.oui[0], candidate.oui[1], candidate.oui[2]);
    }

    // Open security when legit is WPA2/3 — confirmed rogue
    if (candidate.security < ref.security && ref.security >= 3) {
      risk = RGD_CONFIRMED;
      snprintf(reason, 63, "Downgraded security: %d->%d", ref.security, candidate.security);
    }

    // PMF disabled when legit has PMF
    if (ref.pmf && !candidate.pmf && risk < RGD_PROBABLE) {
      risk = RGD_PROBABLE;
      snprintf(reason, 63, "PMF stripped");
    }

    // Higher TX power (much closer RSSI than ref)
    if (candidate.rssi > ref.rssi + 15 && risk == RGD_SUSPICIOUS) {
      snprintf(reason, 63, "Unusually strong signal: %d vs %d dBm", candidate.rssi, ref.rssi);
    }

    if (risk > RGD_OK) {
      rgd_add_alert(candidate.bssid, ref.bssid, candidate.ssid, risk, reason);
    }
  }
}

void rgd_ingest(const uint8_t *bssid, const char *ssid, uint8_t ch, int8_t rssi, uint8_t security,
                bool pmf, bool wps, bool ht, bool vht, bool he, uint16_t beacon_int) {
  RgdApProfile *a = rgd_find(bssid);
  bool is_new = (a == nullptr);
  if (!a)
    a = rgd_alloc(bssid);
  if (!a)
    return;

  if (ssid && ssid[0])
    strncpy(a->ssid, ssid, 32);
  a->ch = ch;
  if (rssi < 0)
    a->rssi = rssi;
  a->security = security;
  a->pmf = pmf;
  a->has_wps = wps;
  a->has_ht = ht;
  a->has_vht = vht;
  a->has_he = he;
  a->beacon_int_tu = beacon_int;
  a->last_ts = millis() / 1000;

  if (is_new && ssid && ssid[0]) {
    rgd_compare(*a);
  }
}

void rgd_mark_known(const uint8_t *bssid) {
  RgdApProfile *a = rgd_find(bssid);
  if (!a)
    return;
  a->is_known = true;
}

static void rgd_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String rgd_json_all() {
  String s = "{\"ap_count\":" + String(g_rgd.ap_count);
  s += ",\"total_alerts\":" + String(g_rgd.total_alerts);

  int confirmed = 0, probable = 0;
  for (int i = 0; i < RGD_MAX_ALERTS; i++) {
    if (g_rgd.alerts[i].ts == 0)
      continue;
    if (g_rgd.alerts[i].risk == RGD_CONFIRMED)
      confirmed++;
    else if (g_rgd.alerts[i].risk == RGD_PROBABLE)
      probable++;
  }
  s += ",\"confirmed\":" + String(confirmed);
  s += ",\"probable\":" + String(probable);

  s += ",\"aps\":[";
  char mac[18];
  bool first = true;
  for (int i = 0; i < RGD_MAX_KNOWN + RGD_MAX_ROGUE; i++) {
    RgdApProfile &a = g_rgd.aps[i];
    if (!a.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    rgd_mac_str(a.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(a.ssid) + "\"";
    s += ",\"ch\":" + String(a.ch);
    s += ",\"rssi\":" + String(a.rssi);
    s += ",\"security\":" + String(a.security);
    s += ",\"pmf\":" + String(a.pmf ? "true" : "false");
    s += ",\"wps\":" + String(a.has_wps ? "true" : "false");
    s += ",\"ht\":" + String(a.has_ht ? "true" : "false");
    s += ",\"vht\":" + String(a.has_vht ? "true" : "false");
    s += ",\"he\":" + String(a.has_he ? "true" : "false");
    s += ",\"bint\":" + String(a.beacon_int_tu);
    s += ",\"known\":" + String(a.is_known ? "true" : "false") + "}";
  }
  s += "],\"alerts\":[";
  bool fa = true;
  for (int i = 0; i < RGD_MAX_ALERTS; i++) {
    int idx = (g_rgd.alert_head - 1 - i + RGD_MAX_ALERTS) % RGD_MAX_ALERTS;
    RgdAlert &al = g_rgd.alerts[idx];
    if (al.ts == 0)
      continue;
    if (!fa)
      s += ",";
    fa = false;
    char rmac[18], lmac[18];
    rgd_mac_str(al.rogue_bssid, rmac);
    rgd_mac_str(al.legit_bssid, lmac);
    s += "{\"ts\":" + String(al.ts);
    s += ",\"rogue\":\"" + String(rmac) + "\"";
    s += ",\"legit\":\"" + String(lmac) + "\"";
    s += ",\"ssid\":\"" + String(al.ssid) + "\"";
    s += ",\"risk\":\"" + String(rgd_risk_name(al.risk)) + "\"";
    s += ",\"reason\":\"" + String(al.reason) + "\"}";
    if (i >= 31)
      break;
  }
  s += "]}";
  return s;
}

inline void rgd_routes() {
  server.on("/api/rgd/all", HTTP_GET,
            []() { server.send(200, "application/json", rgd_json_all()); });
  server.on("/api/rgd/clear", HTTP_POST, []() {
    memset(&g_rgd, 0, sizeof(g_rgd));
    server.send(200, "application/json", "{\"ok\":true}");
  });
  server.on("/api/rgd/mark_known", HTTP_POST, []() {
    // ?bssid=XX:XX:XX:XX:XX:XX
    String bssid_str = server.arg("bssid");
    if (bssid_str.length() < 17) {
      server.send(400, "application/json", "{\"error\":\"bad bssid\"}");
      return;
    }
    uint8_t b[6];
    if (sscanf(bssid_str.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &b[0], &b[1], &b[2], &b[3],
               &b[4], &b[5]) == 6) {
      rgd_mark_known(b);
      server.send(200, "application/json", "{\"ok\":true}");
    } else {
      server.send(400, "application/json", "{\"error\":\"parse fail\"}");
    }
  });
}

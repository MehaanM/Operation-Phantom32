#pragma once
// ap_anomaly.h — AP behavioral anomaly detection engine.
// Detects rogue APs, SSID spoofing, channel switching attacks, etc.
// Prefix: apa_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define APA_MAX_APS 64
#define APA_MAX_ALERTS 64
#define APA_SSID_TABLE 32  // known good SSIDs

enum ApaAnomalyType {
  APA_NONE = 0,
  APA_SSID_SPOOF = 1,     // same SSID different OUI
  APA_BSSID_CLONE = 2,    // same BSSID different channel
  APA_RAPID_CHANNEL = 3,  // AP changes channel frequently
  APA_SUDDEN_OPEN = 4,    // was WPA2, now open
  APA_SUDDEN_WPA3 = 5,    // very rapid upgrade (suspicious)
  APA_BEACON_FLOOD = 6,   // >50 beacons/s
  APA_MGMT_FLOOD = 7,     // excessive mgmt frames
  APA_EVIL_TWIN = 8,      // same SSID, same channel, diff BSSID
  APA_KARMA_LIKELY = 9,   // responds to all probe requests
};

static const char *APA_ANOM_NAMES[] = {
    "None",        "SSID Spoof",  "BSSID Clone",  "Rapid Channel Change",
    "Sudden Open", "Sudden WPA3", "Beacon Flood", "Mgmt Flood",
    "Evil Twin",   "KARMA Attack"};

struct ApaAlert {
  bool active;
  ApaAnomalyType type;
  uint8_t bssid[6];
  char ssid[33];
  uint8_t channel;
  int8_t rssi;
  char detail[64];
  uint32_t ts;
  uint32_t count;  // how many times triggered
};

struct ApaAP {
  bool active;
  uint8_t bssid[6];
  char ssid[33];
  int8_t rssi;
  uint8_t channel;
  uint8_t last_channel;  // previous channel (for rapid-change detection)
  uint8_t channel_changes;
  uint32_t channel_change_ts;

  uint8_t security;  // 0=open, 1=wep, 2=wpa, 3=wpa2, 4=wpa3
  uint8_t last_security;

  uint32_t beacon_count;
  uint32_t mgmt_count;
  uint32_t last_beacon_ts;
  uint32_t beacon_rate;  // beacons per 10s window

  uint32_t first_ts;
  uint32_t last_ts;

  ApaAnomalyType anomaly;  // most recent
};

// Known-good SSID table (populated from first observation)
struct ApaKnownSSID {
  bool active;
  char ssid[33];
  uint8_t oui[3];  // first-seen OUI
  uint8_t security;
  uint8_t channel;
};

static struct {
  ApaAP aps[APA_MAX_APS];
  uint8_t ap_count;
  ApaAlert alerts[APA_MAX_ALERTS];
  uint8_t alert_count;
  ApaKnownSSID known[APA_SSID_TABLE];
  uint8_t known_count;

  uint32_t total_anomalies;
} g_apa = {};

static bool apa_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static ApaAP *apa_find_or_alloc(const uint8_t *bssid) {
  for (int i = 0; i < APA_MAX_APS; i++)
    if (g_apa.aps[i].active && apa_mac_eq(g_apa.aps[i].bssid, bssid))
      return &g_apa.aps[i];
  for (int i = 0; i < APA_MAX_APS; i++) {
    if (!g_apa.aps[i].active) {
      ApaAP &a = g_apa.aps[i];
      memset(&a, 0, sizeof(a));
      a.active = true;
      memcpy(a.bssid, bssid, 6);
      a.first_ts = millis() / 1000;
      g_apa.ap_count++;
      return &a;
    }
  }
  uint32_t oldest = UINT32_MAX;
  int idx = 0;
  for (int i = 0; i < APA_MAX_APS; i++)
    if (g_apa.aps[i].last_ts < oldest) {
      oldest = g_apa.aps[i].last_ts;
      idx = i;
    }
  memset(&g_apa.aps[idx], 0, sizeof(g_apa.aps[0]));
  g_apa.aps[idx].active = true;
  memcpy(g_apa.aps[idx].bssid, bssid, 6);
  g_apa.aps[idx].first_ts = millis() / 1000;
  return &g_apa.aps[idx];
}

static void apa_add_alert(ApaAnomalyType type, const uint8_t *bssid, const char *ssid, uint8_t ch,
                          int8_t rssi, const char *detail) {
  // Check for existing alert (increment count)
  for (int i = 0; i < APA_MAX_ALERTS; i++) {
    ApaAlert &a = g_apa.alerts[i];
    if (a.active && a.type == type && apa_mac_eq(a.bssid, bssid)) {
      a.count++;
      a.ts = millis() / 1000;
      return;
    }
  }
  // New alert
  for (int i = 0; i < APA_MAX_ALERTS; i++) {
    if (!g_apa.alerts[i].active) {
      ApaAlert &a = g_apa.alerts[i];
      a.active = true;
      a.type = type;
      memcpy(a.bssid, bssid, 6);
      strncpy(a.ssid, ssid ? ssid : "", 32);
      a.channel = ch;
      a.rssi = rssi;
      strncpy(a.detail, detail ? detail : "", 63);
      a.ts = millis() / 1000;
      a.count = 1;
      g_apa.alert_count++;
      g_apa.total_anomalies++;
      return;
    }
  }
  // Overwrite oldest
  uint32_t oldest = UINT32_MAX;
  int idx = 0;
  for (int i = 0; i < APA_MAX_ALERTS; i++)
    if (g_apa.alerts[i].ts < oldest) {
      oldest = g_apa.alerts[i].ts;
      idx = i;
    }
  g_apa.alerts[idx].type = type;
  memcpy(g_apa.alerts[idx].bssid, bssid, 6);
  strncpy(g_apa.alerts[idx].ssid, ssid ? ssid : "", 32);
  g_apa.alerts[idx].ts = millis() / 1000;
  g_apa.alerts[idx].count = 1;
  strncpy(g_apa.alerts[idx].detail, detail ? detail : "", 63);
  g_apa.total_anomalies++;
}

static ApaKnownSSID *apa_find_known(const char *ssid) {
  for (int i = 0; i < APA_SSID_TABLE; i++)
    if (g_apa.known[i].active && strcmp(g_apa.known[i].ssid, ssid) == 0)
      return &g_apa.known[i];
  return nullptr;
}

static void apa_register_known(const char *ssid, const uint8_t *bssid, uint8_t sec, uint8_t ch) {
  for (int i = 0; i < APA_SSID_TABLE; i++) {
    if (!g_apa.known[i].active) {
      g_apa.known[i].active = true;
      strncpy(g_apa.known[i].ssid, ssid, 32);
      memcpy(g_apa.known[i].oui, bssid, 3);
      g_apa.known[i].security = sec;
      g_apa.known[i].channel = ch;
      g_apa.known_count++;
      return;
    }
  }
}

void apa_ingest(const uint8_t *bssid, const char *ssid, int8_t rssi, uint8_t channel,
                uint8_t security, uint32_t beacon_rate_10s) {
  if (!ssid || !ssid[0])
    return;

  uint32_t now = millis() / 1000;
  ApaAP *ap = apa_find_or_alloc(bssid);
  if (!ap)
    return;

  if (ssid[0])
    strncpy(ap->ssid, ssid, 32);
  ap->rssi = rssi;
  ap->last_channel = ap->channel;
  ap->channel = channel;
  ap->last_security = ap->security;
  ap->security = security;
  ap->beacon_count++;
  ap->beacon_rate = beacon_rate_10s;
  ap->last_ts = now;
  ap->anomaly = APA_NONE;

  // ─── Checks ────────────────────────────────────────────────────────────────

  // Beacon flood
  if (beacon_rate_10s > 500) {
    ap->anomaly = APA_BEACON_FLOOD;
    apa_add_alert(APA_BEACON_FLOOD, bssid, ssid, channel, rssi, "Beacon rate >50/s");
  }

  // Rapid channel change
  if (ap->last_channel && ap->channel != ap->last_channel) {
    ap->channel_changes++;
    ap->channel_change_ts = now;
    if (ap->channel_changes >= 3) {
      ap->anomaly = APA_RAPID_CHANNEL;
      apa_add_alert(APA_RAPID_CHANNEL, bssid, ssid, channel, rssi,
                    "Multiple channel changes observed");
    }
  }

  // Sudden security downgrade
  if (ap->last_security > 0 && security < ap->last_security && security == 0) {
    ap->anomaly = APA_SUDDEN_OPEN;
    apa_add_alert(APA_SUDDEN_OPEN, bssid, ssid, channel, rssi, "AP changed from encrypted to open");
  }

  // SSID spoof / evil twin detection via known table
  ApaKnownSSID *known = apa_find_known(ssid);
  if (known) {
    // Same SSID, different OUI
    if (known->oui[0] != bssid[0] || known->oui[1] != bssid[1] || known->oui[2] != bssid[2]) {
      ap->anomaly = APA_SSID_SPOOF;
      apa_add_alert(APA_SSID_SPOOF, bssid, ssid, channel, rssi,
                    "SSID seen from different OUI than first observation");
    }
    // Evil twin: same SSID, same channel, different BSSID
    if (known->channel == channel) {
      ap->anomaly = APA_EVIL_TWIN;
      apa_add_alert(APA_EVIL_TWIN, bssid, ssid, channel, rssi,
                    "Same SSID+channel from multiple BSSIDs");
    }
  } else {
    apa_register_known(ssid, bssid, security, channel);
  }
}

static void apa_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String apa_json_alerts() {
  String s = "{\"alert_count\":" + String(g_apa.alert_count);
  s += ",\"total_anomalies\":" + String(g_apa.total_anomalies);
  s += ",\"alerts\":[";
  char mac[18];
  bool first = true;
  for (int i = 0; i < APA_MAX_ALERTS; i++) {
    ApaAlert &a = g_apa.alerts[i];
    if (!a.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    apa_mac_str(a.bssid, mac);
    s += "{\"type\":\"" + String(APA_ANOM_NAMES[a.type]) + "\"";
    s += ",\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(a.ssid) + "\"";
    s += ",\"ch\":" + String(a.channel);
    s += ",\"rssi\":" + String(a.rssi);
    s += ",\"detail\":\"" + String(a.detail) + "\"";
    s += ",\"count\":" + String(a.count);
    s += ",\"ts\":" + String(a.ts) + "}";
  }
  s += "]}";
  return s;
}

inline void apa_routes() {
  server.on("/api/apa/alerts", HTTP_GET,
            []() { server.send(200, "application/json", apa_json_alerts()); });
  server.on("/api/apa/clear", HTTP_POST, []() {
    memset(&g_apa, 0, sizeof(g_apa));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

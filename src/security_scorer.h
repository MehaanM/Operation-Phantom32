#pragma once
// security_scorer.h — AP security posture scorer with weighted risk analysis.
// Scores each AP from 0 (worst) to 100 (best) across multiple dimensions. Prefix: scs_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define SCS_MAX_APS      96

// ─── Score dimensions ─────────────────────────────────────────────────────────
struct ScsScore {
  uint8_t overall;       // 0–100
  uint8_t encryption;    // 0–100 (open=0, WEP=20, WPA=50, WPA2=75, WPA3=100)
  uint8_t authentication;// 0–100 (PSK=50, Enterprise=90, SAE=100)
  uint8_t management;    // 0–100 (PMF required=100, capable=50, none=0)
  uint8_t protocol;      // 0–100 (11ax=100, 11ac=80, 11n=60, legacy=20)
  uint8_t anonymity;     // 0–100 (hidden SSID bonus, no beacon info)
  uint8_t anomaly_pen;   // 0–100 penalty from anomalies (subtracted)
};

static const char* SCS_GRADE[] = {"F","F","D","D","C","C","B","B","A","A","A+"};

// ─── AP score record ──────────────────────────────────────────────────────────
struct ScsAP {
  bool    valid;
  uint8_t bssid[6];
  char    ssid[33];
  ScsScore score;
  uint8_t  issues_count;
  char     issues[4][48];  // top 4 textual issues
  uint32_t scored_at;
};

// ─── State ────────────────────────────────────────────────────────────────────
static struct {
  ScsAP    aps[SCS_MAX_APS];
  uint32_t count;
  uint32_t last_run_ts;
} g_scs = {};

// ─── Helpers ─────────────────────────────────────────────────────────────────
static bool scs_mac_eq(const uint8_t *a, const uint8_t *b) { return memcmp(a,b,6)==0; }

static ScsAP* scs_find(const uint8_t *bssid) {
  for (int i = 0; i < SCS_MAX_APS; i++) {
    if (g_scs.aps[i].valid && scs_mac_eq(g_scs.aps[i].bssid, bssid))
      return &g_scs.aps[i];
  }
  return nullptr;
}

static ScsAP* scs_alloc(const uint8_t *bssid) {
  for (int i = 0; i < SCS_MAX_APS; i++) {
    if (!g_scs.aps[i].valid) {
      memset(&g_scs.aps[i], 0, sizeof(g_scs.aps[i]));
      memcpy(g_scs.aps[i].bssid, bssid, 6);
      g_scs.aps[i].valid = true;
      g_scs.count++;
      return &g_scs.aps[i];
    }
  }
  return nullptr;
}

static void scs_add_issue(ScsAP &ap, const char *issue) {
  if (ap.issues_count < 4)
    strncpy(ap.issues[ap.issues_count++], issue, 47);
}

// ─── Encryption score ─────────────────────────────────────────────────────────
static uint8_t scs_enc_score(uint8_t sec) {
  switch (sec) {
    case 0:  return 0;   // open
    case 1:  return 15;  // WEP
    case 2:  return 50;  // WPA-TKIP
    case 3:  return 75;  // WPA2-CCMP
    case 4:  return 70;  // WPA2-Enterprise
    case 5:  return 100; // WPA3-SAE
    case 6:  return 85;  // WPA3 Transition
    case 7:  return 90;  // OWE
    default: return 10;
  }
}

// ─── Score single AP ─────────────────────────────────────────────────────────
void scs_score_ap(const uint8_t *bssid, const char *ssid,
                  uint8_t sec, bool ht, bool vht, bool he,
                  bool pmf_required, bool pmf_capable,
                  bool wps_enabled, bool hidden,
                  uint8_t anomaly_score) {
  ScsAP *ap = scs_find(bssid);
  if (!ap) ap = scs_alloc(bssid);
  if (!ap) return;

  if (ssid && ssid[0]) strncpy(ap->ssid, ssid, 32);
  ap->issues_count = 0;
  ap->scored_at = millis() / 1000;

  ScsScore &s = ap->score;

  // Encryption dimension
  s.encryption = scs_enc_score(sec);
  if (sec == 0) scs_add_issue(*ap, "Open network — no encryption");
  if (sec == 1) scs_add_issue(*ap, "WEP encryption — trivially crackable");
  if (sec == 2) scs_add_issue(*ap, "WPA/TKIP — deprecated, known weaknesses");

  // Authentication
  s.authentication = (sec == 4) ? 90 : (sec == 5) ? 100 : (sec >= 3) ? 60 : 0;

  // Management frame protection
  s.management = pmf_required ? 100 : pmf_capable ? 50 : 0;
  if (!pmf_capable) scs_add_issue(*ap, "No PMF — vulnerable to deauth attacks");

  // Protocol generation
  s.protocol = he ? 100 : vht ? 80 : ht ? 55 : 20;

  // Anonymity
  s.anonymity = hidden ? 20 : 0;

  // Anomaly penalty
  s.anomaly_pen = (uint8_t)((anomaly_score * 30) / 100);

  // WPS
  if (wps_enabled) {
    s.encryption = (uint8_t)((s.encryption * 7) / 10);
    scs_add_issue(*ap, "WPS enabled — PIN brute-force risk");
  }

  // Overall: weighted average minus anomaly penalty
  uint32_t weighted = (uint32_t)s.encryption * 40 +
                      (uint32_t)s.authentication * 20 +
                      (uint32_t)s.management * 25 +
                      (uint32_t)s.protocol * 15;
  uint8_t base = (uint8_t)(weighted / 100);
  s.overall = (uint8_t)((base > s.anomaly_pen) ? (base - s.anomaly_pen) : 0);
}

// ─── Grade helper ─────────────────────────────────────────────────────────────
static const char* scs_grade(uint8_t score) {
  int idx = score / 10;
  if (idx > 10) idx = 10;
  return SCS_GRADE[idx];
}

// ─── JSON ─────────────────────────────────────────────────────────────────────
static void scs_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String scs_json_ap(const ScsAP &ap) {
  char mac[18];
  scs_mac_str(ap.bssid, mac);
  String s = "{\"bssid\":\"" + String(mac) + "\"";
  s += ",\"ssid\":\"" + String(ap.ssid) + "\"";
  s += ",\"grade\":\"" + String(scs_grade(ap.score.overall)) + "\"";
  s += ",\"overall\":" + String(ap.score.overall);
  s += ",\"encryption\":" + String(ap.score.encryption);
  s += ",\"authentication\":" + String(ap.score.authentication);
  s += ",\"management\":" + String(ap.score.management);
  s += ",\"protocol\":" + String(ap.score.protocol);
  s += ",\"issues\":[";
  for (int i = 0; i < ap.issues_count; i++) {
    if (i) s += ",";
    s += "\"" + String(ap.issues[i]) + "\"";
  }
  s += "],\"scored_at\":" + String(ap.scored_at) + "}";
  return s;
}

static String scs_json_all() {
  String s = "{\"count\":" + String(g_scs.count);
  s += ",\"last_run\":" + String(g_scs.last_run_ts);
  s += ",\"aps\":[";
  bool first = true;
  for (int i = 0; i < SCS_MAX_APS; i++) {
    if (!g_scs.aps[i].valid) continue;
    if (!first) s += ",";
    first = false;
    s += scs_json_ap(g_scs.aps[i]);
  }
  s += "]}";
  return s;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void scs_routes() {
  server.on("/api/scs/all", HTTP_GET, []() {
    server.send(200, "application/json", scs_json_all());
  });

  server.on("/api/scs/ap", HTTP_GET, []() {
    if (!server.hasArg("bssid")) {
      server.send(400, "application/json", "{\"error\":\"missing bssid\"}"); return;
    }
    uint8_t bssid[6];
    sscanf(server.arg("bssid").c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
           &bssid[0],&bssid[1],&bssid[2],&bssid[3],&bssid[4],&bssid[5]);
    ScsAP *ap = scs_find(bssid);
    if (!ap) { server.send(404, "application/json", "{\"error\":\"not found\"}"); return; }
    server.send(200, "application/json", scs_json_ap(*ap));
  });

  server.on("/api/scs/worst", HTTP_GET, []() {
    // Return 10 worst-scored APs
    ScsAP* sorted[SCS_MAX_APS];
    int n = 0;
    for (int i = 0; i < SCS_MAX_APS; i++) {
      if (g_scs.aps[i].valid) sorted[n++] = &g_scs.aps[i];
    }
    for (int i = 0; i < n-1; i++)
      for (int j = i+1; j < n; j++)
        if (sorted[j]->score.overall < sorted[i]->score.overall) {
          ScsAP *tmp = sorted[i]; sorted[i]=sorted[j]; sorted[j]=tmp;
        }
    String s = "[";
    for (int i = 0; i < n && i < 10; i++) {
      if (i) s += ",";
      s += scs_json_ap(*sorted[i]);
    }
    s += "]";
    server.send(200, "application/json", s);
  });

  server.on("/api/scs/clear", HTTP_POST, []() {
    memset(&g_scs, 0, sizeof(g_scs));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

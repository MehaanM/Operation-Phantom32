#pragma once
// security_audit.h — Network-wide security audit engine.
// Scores every discovered AP across multiple security dimensions.
// Prefix: sau_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define SAU_MAX_APS 64
#define SAU_MAX_FINDINGS 128  // ring buffer of audit findings

enum SauSeverity : uint8_t {
  SAU_INFO = 0,
  SAU_LOW = 1,
  SAU_MEDIUM = 2,
  SAU_HIGH = 3,
  SAU_CRITICAL = 4,
};

enum SauCategory : uint8_t {
  SAU_CAT_AUTH = 0,    // authentication/cipher
  SAU_CAT_MGMT = 1,    // management frame protection
  SAU_CAT_PROTO = 2,   // protocol version
  SAU_CAT_CONFIG = 3,  // configuration issues
  SAU_CAT_WPS = 4,     // WPS vulnerabilities
  SAU_CAT_ROGUE = 5,   // rogue AP / evil twin
};

struct SauFinding {
  bool active;
  uint32_t ts;
  uint8_t bssid[6];
  char ssid[33];
  SauSeverity severity;
  SauCategory category;
  char title[48];
  char detail[80];
};

struct SauAP {
  bool active;
  uint8_t bssid[6];
  char ssid[33];
  uint8_t ch;
  int8_t rssi;
  uint32_t last_ts;

  // Inputs
  uint8_t security;  // 0=open,1=WEP,2=WPA,3=WPA2,4=WPA3
  bool has_pmf;
  bool pmf_required;
  bool has_wps;
  bool wps_locked;
  uint8_t generation;  // WiFi gen 3-7
  bool has_ssid;       // false = hidden
  bool has_he;         // 802.11ax

  // Score breakdown (0-100 each)
  uint8_t score_auth;    // authentication/encryption
  uint8_t score_mgmt;    // management frame protection
  uint8_t score_proto;   // protocol/cipher modernity
  uint8_t score_config;  // configuration
  uint8_t score_total;   // overall (weighted average)

  uint8_t finding_count;
};

static struct {
  SauAP aps[SAU_MAX_APS];
  uint8_t count;
  SauFinding findings[SAU_MAX_FINDINGS];
  uint8_t finding_head;
  uint32_t finding_count;
} g_sau = {};

static bool sau_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static SauAP *sau_find(const uint8_t *bssid) {
  for (int i = 0; i < SAU_MAX_APS; i++)
    if (g_sau.aps[i].active && sau_mac_eq(g_sau.aps[i].bssid, bssid))
      return &g_sau.aps[i];
  return nullptr;
}

static SauAP *sau_alloc(const uint8_t *bssid) {
  for (int i = 0; i < SAU_MAX_APS; i++) {
    if (!g_sau.aps[i].active) {
      SauAP &a = g_sau.aps[i];
      memset(&a, 0, sizeof(a));
      a.active = true;
      memcpy(a.bssid, bssid, 6);
      g_sau.count++;
      return &a;
    }
  }
  uint32_t oldest = UINT32_MAX;
  int idx = 0;
  for (int i = 0; i < SAU_MAX_APS; i++)
    if (g_sau.aps[i].last_ts < oldest) {
      oldest = g_sau.aps[i].last_ts;
      idx = i;
    }
  memset(&g_sau.aps[idx], 0, sizeof(g_sau.aps[0]));
  g_sau.aps[idx].active = true;
  memcpy(g_sau.aps[idx].bssid, bssid, 6);
  return &g_sau.aps[idx];
}

static void sau_add_finding(const uint8_t *bssid, const char *ssid, SauSeverity sev,
                            SauCategory cat, const char *title, const char *detail) {
  SauFinding &f = g_sau.findings[g_sau.finding_head % SAU_MAX_FINDINGS];
  f.active = true;
  f.ts = millis() / 1000;
  if (bssid)
    memcpy(f.bssid, bssid, 6);
  if (ssid)
    strncpy(f.ssid, ssid, 32);
  f.severity = sev;
  f.category = cat;
  if (title)
    strncpy(f.title, title, 47);
  if (detail)
    strncpy(f.detail, detail, 79);
  g_sau.finding_head = (g_sau.finding_head + 1) % SAU_MAX_FINDINGS;
  g_sau.finding_count++;
}

static void sau_audit(SauAP &a) {
  a.finding_count = 0;

  // --- Authentication score ---
  int s_auth = 0;
  if (a.security == 0) {
    s_auth = 0;
    sau_add_finding(a.bssid, a.ssid, SAU_CRITICAL, SAU_CAT_AUTH, "Open Network",
                    "No encryption — all traffic is plaintext");
    a.finding_count++;
  } else if (a.security == 1) {
    s_auth = 10;
    sau_add_finding(a.bssid, a.ssid, SAU_HIGH, SAU_CAT_AUTH, "WEP Encryption",
                    "WEP is broken and crackable in minutes");
    a.finding_count++;
  } else if (a.security == 2) {
    s_auth = 40;
    sau_add_finding(a.bssid, a.ssid, SAU_MEDIUM, SAU_CAT_AUTH, "WPA-TKIP",
                    "WPA/TKIP has known vulnerabilities; upgrade to WPA2-CCMP");
    a.finding_count++;
  } else if (a.security == 3) {
    s_auth = 75;
  } else if (a.security == 4) {
    s_auth = 100;
  }
  a.score_auth = (uint8_t)s_auth;

  // --- Management frame protection ---
  int s_mgmt = 0;
  if (a.security >= 3) {
    if (a.pmf_required) {
      s_mgmt = 100;
    } else if (a.has_pmf) {
      s_mgmt = 65;
      sau_add_finding(a.bssid, a.ssid, SAU_LOW, SAU_CAT_MGMT, "PMF Optional",
                      "PMF capable but not required — downgrade possible");
      a.finding_count++;
    } else {
      s_mgmt = 0;
      if (a.security == 3) {
        sau_add_finding(a.bssid, a.ssid, SAU_MEDIUM, SAU_CAT_MGMT, "No PMF",
                        "WPA2 without PMF — vulnerable to deauth attacks");
        a.finding_count++;
      }
    }
  } else {
    s_mgmt = 0;
  }
  a.score_mgmt = (uint8_t)s_mgmt;

  // --- Protocol modernity ---
  int s_proto = 0;
  if (a.generation >= 6)
    s_proto = 100;
  else if (a.generation == 5)
    s_proto = 75;
  else if (a.generation == 4)
    s_proto = 50;
  else
    s_proto = 20;
  a.score_proto = (uint8_t)s_proto;

  // --- Configuration ---
  int s_config = 80;
  if (a.has_wps && !a.wps_locked) {
    s_config -= 30;
    sau_add_finding(a.bssid, a.ssid, SAU_HIGH, SAU_CAT_WPS, "WPS Enabled",
                    "WPS PIN brute-force attacks possible");
    a.finding_count++;
  }
  if (a.has_wps && a.wps_locked) {
    s_config -= 10;
    sau_add_finding(a.bssid, a.ssid, SAU_LOW, SAU_CAT_WPS, "WPS Locked",
                    "WPS locked after failed attempts");
    a.finding_count++;
  }
  if (!a.has_ssid) {
    // Hidden SSID is security-through-obscurity, not real security
    sau_add_finding(a.bssid, a.ssid, SAU_INFO, SAU_CAT_CONFIG, "Hidden SSID",
                    "SSID not broadcast — easily discovered by passive scanning");
    a.finding_count++;
  }
  if (s_config < 0)
    s_config = 0;
  a.score_config = (uint8_t)s_config;

  // --- Total weighted score: auth=40%, mgmt=30%, proto=15%, config=15% ---
  a.score_total =
      (uint8_t)((a.score_auth * 40 + a.score_mgmt * 30 + a.score_proto * 15 + a.score_config * 15) /
                100);
}

void sau_ingest(const uint8_t *bssid, const char *ssid, uint8_t ch, int8_t rssi, uint8_t sec,
                bool pmf, bool pmf_req, bool wps, bool wps_locked, uint8_t gen, bool hidden) {
  SauAP *a = sau_find(bssid);
  if (!a)
    a = sau_alloc(bssid);
  if (!a)
    return;

  if (ssid && ssid[0])
    strncpy(a->ssid, ssid, 32);
  a->ch = ch;
  if (rssi < 0)
    a->rssi = rssi;
  a->last_ts = millis() / 1000;
  a->security = sec;
  a->has_pmf = pmf;
  a->pmf_required = pmf_req;
  a->has_wps = wps;
  a->wps_locked = wps_locked;
  a->generation = gen;
  a->has_ssid = !hidden;
  sau_audit(*a);
}

static const char *sau_sev_str(SauSeverity s) {
  switch (s) {
    case SAU_INFO:
      return "INFO";
    case SAU_LOW:
      return "LOW";
    case SAU_MEDIUM:
      return "MEDIUM";
    case SAU_HIGH:
      return "HIGH";
    case SAU_CRITICAL:
      return "CRITICAL";
    default:
      return "UNKNOWN";
  }
}

static const char *sau_cat_str(SauCategory c) {
  switch (c) {
    case SAU_CAT_AUTH:
      return "Auth";
    case SAU_CAT_MGMT:
      return "MgmtProt";
    case SAU_CAT_PROTO:
      return "Protocol";
    case SAU_CAT_CONFIG:
      return "Config";
    case SAU_CAT_WPS:
      return "WPS";
    case SAU_CAT_ROGUE:
      return "Rogue";
    default:
      return "Other";
  }
}

static void sau_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String sau_json_all() {
  String s = "{\"count\":" + String(g_sau.count);
  s += ",\"finding_count\":" + String(g_sau.finding_count);
  s += ",\"aps\":[";
  char mac[18];
  bool first = true;
  for (int i = 0; i < SAU_MAX_APS; i++) {
    SauAP &a = g_sau.aps[i];
    if (!a.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    sau_mac_str(a.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(a.ssid) + "\"";
    s += ",\"ch\":" + String(a.ch);
    s += ",\"rssi\":" + String(a.rssi);
    s += ",\"sec\":" + String(a.security);
    s += ",\"pmf\":" + String(a.has_pmf ? "true" : "false");
    s += ",\"pmf_req\":" + String(a.pmf_required ? "true" : "false");
    s += ",\"wps\":" + String(a.has_wps ? "true" : "false");
    s += ",\"gen\":" + String(a.generation);
    s += ",\"score_auth\":" + String(a.score_auth);
    s += ",\"score_mgmt\":" + String(a.score_mgmt);
    s += ",\"score_proto\":" + String(a.score_proto);
    s += ",\"score_config\":" + String(a.score_config);
    s += ",\"score_total\":" + String(a.score_total);
    s += ",\"findings\":" + String(a.finding_count);
    s += "}";
  }
  s += "],\"recent_findings\":[";
  first = true;
  uint32_t cnt = g_sau.finding_count;
  uint32_t num = min((uint32_t)SAU_MAX_FINDINGS, cnt);
  uint32_t start = (cnt > SAU_MAX_FINDINGS) ? g_sau.finding_head : 0;
  for (uint32_t i = 0; i < num; i++) {
    uint32_t idx = (start + i) % SAU_MAX_FINDINGS;
    SauFinding &f = g_sau.findings[idx];
    if (!f.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    sau_mac_str(f.bssid, mac);
    s += "{\"ts\":" + String(f.ts);
    s += ",\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(f.ssid) + "\"";
    s += ",\"sev\":\"" + String(sau_sev_str(f.severity)) + "\"";
    s += ",\"sev_code\":" + String(f.severity);
    s += ",\"cat\":\"" + String(sau_cat_str(f.category)) + "\"";
    s += ",\"title\":\"" + String(f.title) + "\"";
    s += ",\"detail\":\"" + String(f.detail) + "\"";
    s += "}";
  }
  s += "]}";
  return s;
}

inline void sau_routes() {
  server.on("/api/sau/all", HTTP_GET,
            []() { server.send(200, "application/json", sau_json_all()); });
  server.on("/api/sau/clear", HTTP_POST, []() {
    memset(&g_sau, 0, sizeof(g_sau));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

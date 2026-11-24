#pragma once
// evil_twin_detector.h — Evil twin / rogue AP detection engine.
// Detects APs that share an SSID with known APs but differ in BSSID/channel/cipher.
// Prefix: etd_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define ETD_MAX_LEGIT    64
#define ETD_MAX_ROGUES   32
#define ETD_MAX_EVENTS   64

enum EtdAlertLevel : uint8_t {
  ETD_NONE     = 0,
  ETD_SUSPECT  = 1,  // SSID matches, different BSSID (possible evil twin)
  ETD_PROBABLE = 2,  // SSID match + weaker security than known AP
  ETD_CRITICAL = 3,  // SSID match + open/weaker auth + same OUI prefix
};

struct EtdLegitAP {
  bool    active;
  uint8_t bssid[6];
  char    ssid[33];
  uint8_t channel;
  int8_t  rssi;
  uint8_t security_level; // 0=open, 1=WEP, 2=WPA, 3=WPA2, 4=WPA3
  bool    has_pmf;
  uint32_t first_ts;
  uint32_t last_ts;
};

struct EtdRogueAP {
  bool    active;
  uint8_t bssid[6];
  char    ssid[33];
  uint8_t channel;
  int8_t  rssi;
  uint8_t security_level;
  bool    has_pmf;

  uint8_t legit_bssid[6];     // which legit AP this is a twin of
  EtdAlertLevel alert;
  char    alert_reason[64];

  uint32_t first_ts;
  uint32_t last_ts;
  uint32_t detection_count;
};

struct EtdEvent {
  uint32_t ts;
  uint8_t  rogue[6];
  uint8_t  legit[6];
  char     ssid[33];
  EtdAlertLevel level;
  char     reason[64];
};

static struct {
  EtdLegitAP legit_aps[ETD_MAX_LEGIT];
  uint8_t    legit_count;
  EtdRogueAP rogues[ETD_MAX_ROGUES];
  uint8_t    rogue_count;
  EtdEvent   events[ETD_MAX_EVENTS];
  uint8_t    event_head;
  uint32_t   event_count;
  uint32_t   total_detections;
} g_etd = {};

static bool etd_mac_eq(const uint8_t *a, const uint8_t *b) { return memcmp(a, b, 6) == 0; }

static EtdLegitAP* etd_find_legit(const uint8_t *bssid) {
  for (int i = 0; i < ETD_MAX_LEGIT; i++)
    if (g_etd.legit_aps[i].active && etd_mac_eq(g_etd.legit_aps[i].bssid, bssid))
      return &g_etd.legit_aps[i];
  return nullptr;
}

static EtdLegitAP* etd_find_legit_by_ssid(const char *ssid, const uint8_t *exclude_bssid) {
  for (int i = 0; i < ETD_MAX_LEGIT; i++) {
    if (!g_etd.legit_aps[i].active) continue;
    if (strcmp(g_etd.legit_aps[i].ssid, ssid) == 0 &&
        !etd_mac_eq(g_etd.legit_aps[i].bssid, exclude_bssid))
      return &g_etd.legit_aps[i];
  }
  return nullptr;
}

static EtdRogueAP* etd_find_rogue(const uint8_t *bssid) {
  for (int i = 0; i < ETD_MAX_ROGUES; i++)
    if (g_etd.rogues[i].active && etd_mac_eq(g_etd.rogues[i].bssid, bssid))
      return &g_etd.rogues[i];
  return nullptr;
}

static void etd_add_event(const uint8_t *rogue, const uint8_t *legit,
                           const char *ssid, EtdAlertLevel level, const char *reason) {
  EtdEvent &e = g_etd.events[g_etd.event_head % ETD_MAX_EVENTS];
  e.ts = millis()/1000;
  if (rogue) memcpy(e.rogue, rogue, 6); else memset(e.rogue, 0, 6);
  if (legit) memcpy(e.legit, legit, 6); else memset(e.legit, 0, 6);
  if (ssid)  strncpy(e.ssid, ssid, 32);
  e.level = level;
  if (reason) strncpy(e.reason, reason, 63);
  g_etd.event_head = (g_etd.event_head + 1) % ETD_MAX_EVENTS;
  g_etd.event_count++;
}

static EtdAlertLevel etd_assess(const EtdLegitAP &legit, const uint8_t *rogue_bssid,
                                  uint8_t rogue_sec, bool rogue_pmf, char *reason_out) {
  uint8_t score = 1; // at least SUSPECT if same SSID, different BSSID
  snprintf(reason_out, 63, "Same SSID '%.*s', different BSSID", 20, legit.ssid);

  // If lower security, raise alert
  if (rogue_sec < legit.security_level) {
    score = 2;
    snprintf(reason_out, 63, "Downgraded security: %d vs %d", rogue_sec, legit.security_level);
  }

  // If open (no security), raise to critical
  if (rogue_sec == 0) {
    score = 3;
    strncpy(reason_out, "Open network same SSID as secured AP", 63);
  }

  // OUI match (same manufacturer, more suspicious)
  if (rogue_bssid[0] == legit.bssid[0] &&
      rogue_bssid[1] == legit.bssid[1] &&
      rogue_bssid[2] == legit.bssid[2]) {
    score = max(score, (uint8_t)2);
    strncat(reason_out, " + same OUI", 63 - strlen(reason_out));
  }

  return (EtdAlertLevel)min((int)score, 3);
}

void etd_add_trusted(const uint8_t *bssid, const char *ssid,
                     uint8_t channel, uint8_t sec_level, bool pmf) {
  EtdLegitAP *a = etd_find_legit(bssid);
  if (!a) {
    for (int i = 0; i < ETD_MAX_LEGIT; i++) {
      if (!g_etd.legit_aps[i].active) {
        a = &g_etd.legit_aps[i];
        memset(a, 0, sizeof(*a));
        a->active = true;
        memcpy(a->bssid, bssid, 6);
        a->first_ts = millis()/1000;
        g_etd.legit_count++;
        break;
      }
    }
  }
  if (!a) return;
  if (ssid) strncpy(a->ssid, ssid, 32);
  a->channel = channel;
  a->security_level = sec_level;
  a->has_pmf = pmf;
  a->last_ts = millis()/1000;
}

void etd_ingest_beacon(const uint8_t *bssid, const char *ssid,
                       uint8_t channel, int8_t rssi,
                       uint8_t sec_level, bool pmf) {
  if (!ssid || !ssid[0]) return;

  // Check if this is a known legit AP
  EtdLegitAP *legit = etd_find_legit(bssid);
  if (legit) {
    legit->last_ts = millis()/1000;
    if (rssi < 0) legit->rssi = rssi;
    return;
  }

  // Check if another legit AP has the same SSID
  EtdLegitAP *twin_of = etd_find_legit_by_ssid(ssid, bssid);
  if (!twin_of) return;

  char reason[64] = {};
  EtdAlertLevel level = etd_assess(*twin_of, bssid, sec_level, pmf, reason);
  if (level == ETD_NONE) return;

  // Update or create rogue entry
  EtdRogueAP *rogue = etd_find_rogue(bssid);
  if (!rogue) {
    for (int i = 0; i < ETD_MAX_ROGUES; i++) {
      if (!g_etd.rogues[i].active) {
        rogue = &g_etd.rogues[i];
        memset(rogue, 0, sizeof(*rogue));
        rogue->active = true;
        memcpy(rogue->bssid, bssid, 6);
        rogue->first_ts = millis()/1000;
        g_etd.rogue_count++;
        break;
      }
    }
  }
  if (!rogue) return;

  strncpy(rogue->ssid, ssid, 32);
  rogue->channel = channel;
  if (rssi < 0) rogue->rssi = rssi;
  rogue->security_level = sec_level;
  rogue->has_pmf = pmf;
  memcpy(rogue->legit_bssid, twin_of->bssid, 6);
  rogue->alert = level;
  strncpy(rogue->alert_reason, reason, 63);
  rogue->detection_count++;
  rogue->last_ts = millis()/1000;
  g_etd.total_detections++;

  if (rogue->detection_count == 1) {
    etd_add_event(bssid, twin_of->bssid, ssid, level, reason);
  }
}

static const char* etd_level_str(EtdAlertLevel l) {
  switch(l) {
    case ETD_SUSPECT:  return "SUSPECT";
    case ETD_PROBABLE: return "PROBABLE";
    case ETD_CRITICAL: return "CRITICAL";
    default:           return "none";
  }
}

static void etd_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String etd_json_all() {
  String s = "{\"legit_count\":" + String(g_etd.legit_count);
  s += ",\"rogue_count\":" + String(g_etd.rogue_count);
  s += ",\"total_detections\":" + String(g_etd.total_detections);
  s += ",\"rogues\":[";
  char mac[18], lmac[18]; bool first = true;
  for (int i = 0; i < ETD_MAX_ROGUES; i++) {
    EtdRogueAP &r = g_etd.rogues[i];
    if (!r.active) continue;
    if (!first) s += ","; first = false;
    etd_mac_str(r.bssid, mac);
    etd_mac_str(r.legit_bssid, lmac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(r.ssid) + "\"";
    s += ",\"ch\":" + String(r.channel);
    s += ",\"rssi\":" + String(r.rssi);
    s += ",\"sec\":" + String(r.security_level);
    s += ",\"pmf\":" + String(r.has_pmf?"true":"false");
    s += ",\"legit_bssid\":\"" + String(lmac) + "\"";
    s += ",\"alert\":\"" + String(etd_level_str(r.alert)) + "\"";
    s += ",\"alert_code\":" + String(r.alert);
    s += ",\"reason\":\"" + String(r.alert_reason) + "\"";
    s += ",\"detections\":" + String(r.detection_count);
    s += "}";
  }
  s += "],\"events\":[";
  first = true;
  uint32_t cnt = g_etd.event_count;
  uint32_t num = min((uint32_t)ETD_MAX_EVENTS, cnt);
  uint32_t start = (cnt > ETD_MAX_EVENTS) ? g_etd.event_head : 0;
  for (uint32_t i = 0; i < num; i++) {
    uint32_t idx = (start + i) % ETD_MAX_EVENTS;
    EtdEvent &e  = g_etd.events[idx];
    if (!e.ts) continue;
    if (!first) s += ","; first = false;
    char r2[18], l2[18];
    etd_mac_str(e.rogue, r2); etd_mac_str(e.legit, l2);
    s += "{\"ts\":" + String(e.ts);
    s += ",\"rogue\":\"" + String(r2) + "\"";
    s += ",\"legit\":\"" + String(l2) + "\"";
    s += ",\"ssid\":\"" + String(e.ssid) + "\"";
    s += ",\"level\":\"" + String(etd_level_str(e.level)) + "\"";
    s += ",\"code\":" + String(e.level);
    s += ",\"reason\":\"" + String(e.reason) + "\"";
    s += "}";
  }
  s += "]}";
  return s;
}

inline void etd_routes() {
  server.on("/api/etd/all", HTTP_GET, []() {
    server.send(200, "application/json", etd_json_all());
  });
  server.on("/api/etd/add_trusted", HTTP_POST, []() {
    // Add a trusted SSID/BSSID for monitoring
    if (server.hasArg("bssid") && server.hasArg("ssid")) {
      uint8_t mac[6] = {};
      sscanf(server.arg("bssid").c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
             &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
      uint8_t sec = server.hasArg("sec") ? server.arg("sec").toInt() : 3;
      bool pmf    = server.hasArg("pmf");
      etd_add_trusted(mac, server.arg("ssid").c_str(),
                      server.hasArg("ch") ? server.arg("ch").toInt() : 0,
                      sec, pmf);
      server.send(200, "application/json", "{\"ok\":true}");
    } else {
      server.send(400, "application/json", "{\"error\":\"missing bssid/ssid\"}");
    }
  });
  server.on("/api/etd/clear", HTTP_POST, []() {
    memset(&g_etd, 0, sizeof(g_etd));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

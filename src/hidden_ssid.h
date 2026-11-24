#pragma once
// hidden_ssid.h — Hidden SSID discovery.
// Collects hidden AP beacons (empty SSID), then tries to match them
// with probe responses, association requests, and directed probes
// to reveal the actual SSID.
// Prefix: hss_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define HSS_MAX_HIDDEN   24
#define HSS_MAX_GUESSES   8   // known SSIDs guessed per AP
#define HSS_HIST_LEN     60

// Match confidence for revealed SSID
enum HssRevealConf : uint8_t {
  HSS_REV_UNRESOLVED = 0,
  HSS_REV_PARTIAL    = 1,  // length known, SSID unknown
  HSS_REV_PROBABLE   = 2,  // seen in nearby probe response
  HSS_REV_CONFIRMED  = 3   // seen in assoc request to this AP
};

struct HssGuess {
  char ssid[33];
  uint32_t seen_ts;
  uint8_t  evidence;   // 1=probe_req, 2=probe_rsp, 3=assoc
};

struct HssHiddenAP {
  bool    active;
  uint8_t bssid[6];
  uint8_t ch;
  int8_t  rssi;
  uint8_t ssid_len;         // from beacon (may be 0 if truly zero-length)
  bool    zero_length;      // beacon uses zero-length vs whitespace SSID

  HssRevealConf reveal_conf;
  char    revealed_ssid[33]; // best-guess actual SSID
  uint8_t reveal_evidence;   // 1=probe_req, 2=probe_rsp, 3=assoc, 4=direct

  HssGuess guesses[HSS_MAX_GUESSES];
  uint8_t  guess_count;

  // Statistics
  uint32_t beacon_count;
  uint32_t probe_rsp_count;  // probe responses from this AP
  uint32_t assoc_seen;       // association requests targeting this BSSID

  uint32_t first_ts;
  uint32_t last_ts;

  // Beacon interval stats
  uint16_t beacon_interval;
  uint32_t last_beacon_ts;
  uint32_t avg_beacon_gap_ms;
};

static struct {
  HssHiddenAP aps[HSS_MAX_HIDDEN];
  uint8_t     count;
  uint16_t    total_resolved;
} g_hss = {};

static bool hss_mac_eq(const uint8_t *a, const uint8_t *b) { return memcmp(a,b,6)==0; }

static HssHiddenAP* hss_find(const uint8_t *bssid) {
  for (int i = 0; i < HSS_MAX_HIDDEN; i++)
    if (g_hss.aps[i].active && hss_mac_eq(g_hss.aps[i].bssid, bssid))
      return &g_hss.aps[i];
  return nullptr;
}

static HssHiddenAP* hss_alloc(const uint8_t *bssid) {
  for (int i = 0; i < HSS_MAX_HIDDEN; i++) {
    if (!g_hss.aps[i].active) {
      HssHiddenAP &a = g_hss.aps[i];
      memset(&a, 0, sizeof(a));
      a.active = true;
      memcpy(a.bssid, bssid, 6);
      a.first_ts = millis()/1000;
      g_hss.count++;
      return &a;
    }
  }
  uint32_t oldest = UINT32_MAX; int idx = 0;
  for (int i = 0; i < HSS_MAX_HIDDEN; i++)
    if (g_hss.aps[i].reveal_conf < HSS_REV_CONFIRMED && g_hss.aps[i].last_ts < oldest)
      { oldest = g_hss.aps[i].last_ts; idx = i; }
  memset(&g_hss.aps[idx], 0, sizeof(HssHiddenAP));
  g_hss.aps[idx].active = true;
  memcpy(g_hss.aps[idx].bssid, bssid, 6);
  g_hss.aps[idx].first_ts = millis()/1000;
  return &g_hss.aps[idx];
}

static void hss_add_guess(HssHiddenAP &a, const char *ssid, uint8_t evidence) {
  for (int i = 0; i < a.guess_count; i++)
    if (strncmp(a.guesses[i].ssid, ssid, 32) == 0) {
      if (evidence > a.guesses[i].evidence) a.guesses[i].evidence = evidence;
      return;
    }
  if (a.guess_count >= HSS_MAX_GUESSES) return;
  HssGuess &g = a.guesses[a.guess_count++];
  strncpy(g.ssid, ssid, 32);
  g.seen_ts  = millis()/1000;
  g.evidence = evidence;
}

// Ingest a hidden SSID beacon
void hss_ingest_hidden_beacon(const uint8_t *bssid, uint8_t ch, int8_t rssi,
                                uint8_t ssid_len, uint16_t beacon_interval) {
  HssHiddenAP *a = hss_find(bssid);
  if (!a) a = hss_alloc(bssid);
  if (!a) return;

  a->ch = ch; a->rssi = rssi;
  a->ssid_len = ssid_len;
  a->zero_length = (ssid_len == 0);
  a->beacon_interval = beacon_interval;
  a->beacon_count++;

  // Track beacon gap for timing analysis
  if (a->last_beacon_ts > 0) {
    uint32_t gap = millis() - a->last_beacon_ts;
    if (a->avg_beacon_gap_ms == 0) a->avg_beacon_gap_ms = gap;
    else a->avg_beacon_gap_ms = (a->avg_beacon_gap_ms * 7 + gap) / 8;
  }
  a->last_beacon_ts = millis();
  a->last_ts = millis()/1000;
}

// Ingest a probe response from this AP (reveals SSID)
void hss_ingest_probe_rsp(const uint8_t *ap_bssid, const char *ssid,
                           uint8_t ch, int8_t rssi) {
  if (!ssid || !ssid[0]) return;
  HssHiddenAP *a = hss_find(ap_bssid);
  if (!a) a = hss_alloc(ap_bssid);
  if (!a) return;

  a->ch = ch; a->rssi = rssi;
  a->probe_rsp_count++;
  hss_add_guess(*a, ssid, 2);

  if (a->reveal_conf < HSS_REV_PROBABLE) {
    a->reveal_conf = HSS_REV_PROBABLE;
    strncpy(a->revealed_ssid, ssid, 32);
    a->reveal_evidence = 2;
  }
  a->last_ts = millis()/1000;
}

// Ingest an association request to this AP (most reliable reveal)
void hss_ingest_assoc_req(const uint8_t *ap_bssid, const uint8_t *client_mac,
                           const char *ssid) {
  if (!ssid || !ssid[0]) return;
  HssHiddenAP *a = hss_find(ap_bssid);
  if (!a) a = hss_alloc(ap_bssid);
  if (!a) return;

  a->assoc_seen++;
  hss_add_guess(*a, ssid, 3);

  if (a->reveal_conf < HSS_REV_CONFIRMED) {
    a->reveal_conf = HSS_REV_CONFIRMED;
    strncpy(a->revealed_ssid, ssid, 32);
    a->reveal_evidence = 3;
    g_hss.total_resolved++;
  }
  a->last_ts = millis()/1000;
}

// Ingest a directed probe request (may hint at SSID in area)
void hss_ingest_directed_probe(const uint8_t *client_mac, const char *ssid,
                                uint8_t ch) {
  if (!ssid || !ssid[0]) return;
  // Try to match to a hidden AP on this channel
  for (int i = 0; i < HSS_MAX_HIDDEN; i++) {
    HssHiddenAP &a = g_hss.aps[i];
    if (!a.active || a.ch != ch) continue;
    if (a.reveal_conf >= HSS_REV_PROBABLE) continue;
    hss_add_guess(a, ssid, 1);
    if (a.reveal_conf == HSS_REV_UNRESOLVED && a.guess_count == 1) {
      a.reveal_conf = HSS_REV_PARTIAL;
      strncpy(a.revealed_ssid, ssid, 32);
      a.reveal_evidence = 1;
    }
  }
}

static const char* hss_conf_str(HssRevealConf c) {
  switch (c) {
    case HSS_REV_UNRESOLVED: return "Hidden";
    case HSS_REV_PARTIAL:    return "Partial";
    case HSS_REV_PROBABLE:   return "Probable";
    case HSS_REV_CONFIRMED:  return "Confirmed";
  }
  return "Unknown";
}

static void hss_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String hss_json_all() {
  String s = "{\"count\":" + String(g_hss.count);
  s += ",\"total_resolved\":" + String(g_hss.total_resolved);
  s += ",\"aps\":[";
  char mac[18]; bool first = true;
  for (int i = 0; i < HSS_MAX_HIDDEN; i++) {
    HssHiddenAP &a = g_hss.aps[i];
    if (!a.active) continue;
    if (!first) s += ","; first = false;
    hss_mac_str(a.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ch\":" + String(a.ch);
    s += ",\"rssi\":" + String(a.rssi);
    s += ",\"ssid_len\":" + String(a.ssid_len);
    s += ",\"zero_length\":" + String(a.zero_length ? "true" : "false");
    s += ",\"reveal\":\"" + String(hss_conf_str(a.reveal_conf)) + "\"";
    s += ",\"revealed_ssid\":\"" + String(a.revealed_ssid) + "\"";
    s += ",\"beacons\":" + String(a.beacon_count);
    s += ",\"probe_rsps\":" + String(a.probe_rsp_count);
    s += ",\"assocs\":" + String(a.assoc_seen);
    s += ",\"bcn_interval\":" + String(a.beacon_interval);
    s += ",\"avg_gap_ms\":" + String(a.avg_beacon_gap_ms);
    s += ",\"guesses\":[";
    for (int j = 0; j < a.guess_count; j++) {
      if (j) s += ",";
      s += "{\"ssid\":\"" + String(a.guesses[j].ssid) + "\"";
      s += ",\"ev\":" + String(a.guesses[j].evidence) + "}";
    }
    s += "]}";
  }
  s += "]}";
  return s;
}

inline void hss_routes() {
  server.on("/api/hss/all", HTTP_GET, []() {
    server.send(200, "application/json", hss_json_all());
  });
  server.on("/api/hss/clear", HTTP_POST, []() {
    memset(&g_hss, 0, sizeof(g_hss));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

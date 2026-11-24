#pragma once
// network_classifier.h — Network type classifier and device role inference.
// Classifies APs and STAs based on observed behavior and OUI/capability patterns.
// Prefix: ncl_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

// ─── Network type classification ─────────────────────────────────────────────
enum NclNetType {
  NCL_NET_UNKNOWN    = 0,
  NCL_NET_HOME       = 1,  // residential AP (simple WPA2, single SSID)
  NCL_NET_ENTERPRISE = 2,  // 802.1X, CCMP, PMF required
  NCL_NET_PUBLIC     = 3,  // open or captive portal (single BSSID)
  NCL_NET_IOT        = 4,  // IoT hub: short beacon interval, low data rate rates
  NCL_NET_HOTSPOT    = 5,  // mobile hotspot (vendor OUI pattern)
  NCL_NET_REPEATER   = 6,  // WDS / mesh / two APs same SSID adjacent channels
  NCL_NET_GUEST      = 7,  // second SSID from same vendor (OUI matched)
  NCL_NET_ROGUE      = 8,  // open with identical SSID to known WPA2 AP
  NCL_NET_ENTERPRISE_GUEST = 9, // 802.1X + open same BSSID (dual SSID infra)
};

static const char* NCL_NET_TYPE_NAMES[] = {
  "unknown","home","enterprise","public","iot","hotspot",
  "repeater","guest","rogue","enterprise_guest"
};

// ─── Device role for a STA ────────────────────────────────────────────────────
enum NclDevRole {
  NCL_ROLE_UNKNOWN  = 0,
  NCL_ROLE_LAPTOP   = 1,  // varied probe history + HT caps
  NCL_ROLE_PHONE    = 2,  // randomized MAC + frequent reassoc
  NCL_ROLE_TABLET   = 3,  // low tx rate + frequent power save
  NCL_ROLE_IOT      = 4,  // fixed MAC + single SSID probes
  NCL_ROLE_CAMERA   = 5,  // near-continuous tx, no probes
  NCL_ROLE_PRINTER  = 6,  // vendor OUI match (Xerox/HP/Brother)
  NCL_ROLE_TV       = 7,  // Samsung/LG/Sony OUI, low probe rate
  NCL_ROLE_ROUTER   = 8,  // acts as AP + STA simultaneously (WDS)
  NCL_ROLE_SCANNER  = 9,  // very high probe rate, many SSIDs
};

static const char* NCL_ROLE_NAMES[] = {
  "unknown","laptop","phone","tablet","iot","camera","printer","tv","router","scanner"
};

// ─── Classified AP record ────────────────────────────────────────────────────
#define NCL_MAX_APS  64

struct NclAP {
  bool        active;
  uint8_t     bssid[6];
  char        ssid[33];
  NclNetType  net_type;
  uint8_t     confidence;    // 0-100
  uint16_t    beacon_iv;     // beacon interval TUs
  int8_t      rssi;
  uint8_t     channel;
  bool        wpa2;
  bool        wpa3;
  bool        open;
  bool        wps;
  bool        pmf;
  bool        enterprise;
  bool        ht;
  bool        vht;
  uint32_t    first_ts;
  uint32_t    last_ts;
};

// ─── Classified STA record ───────────────────────────────────────────────────
#define NCL_MAX_STAS 64

struct NclSTA {
  bool       active;
  uint8_t    mac[6];
  NclDevRole role;
  uint8_t    confidence;
  uint8_t    probe_count;   // distinct SSIDs probed
  bool       randomized;    // locally administered bit set
  int8_t     best_rssi;
  uint32_t   frame_count;
  uint32_t   last_ts;
};

// ─── State ────────────────────────────────────────────────────────────────────
static struct {
  NclAP   aps[NCL_MAX_APS];
  uint8_t ap_count;

  NclSTA  stas[NCL_MAX_STAS];
  uint8_t sta_count;

  uint32_t classified_aps;
  uint32_t classified_stas;
} g_ncl = {};

// ─── Helpers ─────────────────────────────────────────────────────────────────
static bool ncl_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static bool ncl_is_local(const uint8_t *mac) {
  return (mac[0] & 0x02) != 0; // locally administered bit
}

static NclAP* ncl_find_or_alloc_ap(const uint8_t *bssid) {
  for (int i = 0; i < NCL_MAX_APS; i++)
    if (g_ncl.aps[i].active && ncl_mac_eq(g_ncl.aps[i].bssid, bssid))
      return &g_ncl.aps[i];
  for (int i = 0; i < NCL_MAX_APS; i++)
    if (!g_ncl.aps[i].active) {
      memset(&g_ncl.aps[i], 0, sizeof(g_ncl.aps[i]));
      g_ncl.aps[i].active = true;
      memcpy(g_ncl.aps[i].bssid, bssid, 6);
      g_ncl.aps[i].first_ts = millis()/1000;
      g_ncl.ap_count++;
      return &g_ncl.aps[i];
    }
  // LRU: evict oldest
  uint32_t oldest_ts = UINT32_MAX;
  int oldest_idx = 0;
  for (int i = 0; i < NCL_MAX_APS; i++)
    if (g_ncl.aps[i].last_ts < oldest_ts) { oldest_ts = g_ncl.aps[i].last_ts; oldest_idx = i; }
  memset(&g_ncl.aps[oldest_idx], 0, sizeof(g_ncl.aps[0]));
  g_ncl.aps[oldest_idx].active = true;
  memcpy(g_ncl.aps[oldest_idx].bssid, bssid, 6);
  g_ncl.aps[oldest_idx].first_ts = millis()/1000;
  return &g_ncl.aps[oldest_idx];
}

// ─── AP classification logic ─────────────────────────────────────────────────
static void ncl_classify_ap(NclAP &ap) {
  uint8_t score[10] = {};

  // Open network → public or rogue
  if (ap.open) {
    score[NCL_NET_PUBLIC] += 60;
    score[NCL_NET_ROGUE]  += 10; // bump by rogue check elsewhere
  }

  // Enterprise (802.1X)
  if (ap.enterprise) {
    score[NCL_NET_ENTERPRISE] += 70;
    if (ap.pmf) score[NCL_NET_ENTERPRISE] += 15;
  }

  // WPA3 → enterprise or home
  if (ap.wpa3) {
    score[NCL_NET_ENTERPRISE] += 20;
    score[NCL_NET_HOME]       += 30;
  }

  // WPA2 personal, no enterprise, no WPA3 → home
  if (ap.wpa2 && !ap.enterprise && !ap.wpa3) {
    score[NCL_NET_HOME] += 50;
  }

  // Short beacon interval → IoT
  if (ap.beacon_iv > 0 && ap.beacon_iv < 80) {
    score[NCL_NET_IOT] += 40;
  }

  // WPS → home or IoT
  if (ap.wps) {
    score[NCL_NET_HOME] += 15;
    score[NCL_NET_IOT]  += 10;
  }

  // VHT (5GHz / 80MHz) → enterprise or home high-end
  if (ap.vht) {
    score[NCL_NET_ENTERPRISE] += 10;
    score[NCL_NET_HOME]       += 10;
  }

  // Find best score
  uint8_t best = NCL_NET_UNKNOWN;
  uint8_t best_score = 20; // threshold
  for (int t = 1; t < 10; t++) {
    if (score[t] > best_score) { best_score = score[t]; best = t; }
  }

  ap.net_type   = (NclNetType)best;
  ap.confidence = best_score > 100 ? 100 : best_score;
  g_ncl.classified_aps++;
}

// ─── STA classification logic ─────────────────────────────────────────────────
static void ncl_classify_sta(NclSTA &sta) {
  uint8_t score[10] = {};

  // Randomized MAC → phone
  if (sta.randomized) {
    score[NCL_ROLE_PHONE] += 50;
    score[NCL_ROLE_LAPTOP] += 15;
  } else {
    score[NCL_ROLE_IOT] += 15;
  }

  // High probe count → scanner or laptop
  if (sta.probe_count >= 10) {
    score[NCL_ROLE_SCANNER] += 60;
  } else if (sta.probe_count >= 3) {
    score[NCL_ROLE_LAPTOP] += 30;
    score[NCL_ROLE_PHONE]  += 20;
  } else if (sta.probe_count == 1) {
    score[NCL_ROLE_IOT]    += 20;
    score[NCL_ROLE_CAMERA] += 15;
  } else if (sta.probe_count == 0) {
    score[NCL_ROLE_CAMERA] += 25;
    score[NCL_ROLE_IOT]    += 20;
  }

  // High frame count relative to time → camera or IoT
  if (sta.frame_count > 1000) {
    score[NCL_ROLE_CAMERA] += 20;
  }

  // Find best
  uint8_t best = NCL_ROLE_UNKNOWN;
  uint8_t best_score = 15;
  for (int r = 1; r < 10; r++) {
    if (score[r] > best_score) { best_score = score[r]; best = r; }
  }

  sta.role       = (NclDevRole)best;
  sta.confidence = best_score > 100 ? 100 : best_score;
  g_ncl.classified_stas++;
}

// ─── Public ingest APIs ───────────────────────────────────────────────────────
void ncl_ingest_ap(const uint8_t *bssid, const char *ssid, int8_t rssi,
                   uint8_t channel, uint16_t beacon_iv,
                   bool wpa2, bool wpa3, bool open, bool wps, bool pmf,
                   bool enterprise, bool ht, bool vht) {
  NclAP *ap = ncl_find_or_alloc_ap(bssid);
  if (!ap) return;

  if (ssid && ssid[0]) strncpy(ap->ssid, ssid, 32);
  ap->rssi       = rssi;
  ap->channel    = channel;
  ap->beacon_iv  = beacon_iv;
  ap->wpa2       = wpa2;
  ap->wpa3       = wpa3;
  ap->open       = open;
  ap->wps        = wps;
  ap->pmf        = pmf;
  ap->enterprise = enterprise;
  ap->ht         = ht;
  ap->vht        = vht;
  ap->last_ts    = millis()/1000;

  ncl_classify_ap(*ap);
}

void ncl_ingest_sta(const uint8_t *mac, int8_t rssi, uint8_t probe_count) {
  NclSTA *sta = nullptr;
  for (int i = 0; i < NCL_MAX_STAS; i++)
    if (g_ncl.stas[i].active && ncl_mac_eq(g_ncl.stas[i].mac, mac))
      { sta = &g_ncl.stas[i]; break; }

  if (!sta) {
    for (int i = 0; i < NCL_MAX_STAS; i++)
      if (!g_ncl.stas[i].active) {
        sta = &g_ncl.stas[i];
        memset(sta, 0, sizeof(*sta));
        sta->active = true;
        memcpy(sta->mac, mac, 6);
        g_ncl.sta_count++;
        break;
      }
  }
  if (!sta) return;

  sta->probe_count = probe_count;
  sta->randomized  = ncl_is_local(mac);
  sta->best_rssi   = rssi;
  sta->frame_count++;
  sta->last_ts     = millis()/1000;

  ncl_classify_sta(*sta);
}

// ─── JSON ─────────────────────────────────────────────────────────────────────
static void ncl_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String ncl_json_aps() {
  char mac[18];
  String s = "{\"count\":" + String(g_ncl.ap_count) + ",\"aps\":[";
  bool first = true;
  for (int i = 0; i < NCL_MAX_APS; i++) {
    NclAP &ap = g_ncl.aps[i];
    if (!ap.active) continue;
    if (!first) s += ",";
    first = false;
    ncl_mac_str(ap.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(ap.ssid) + "\"";
    s += ",\"type\":\"" + String(NCL_NET_TYPE_NAMES[ap.net_type]) + "\"";
    s += ",\"conf\":" + String(ap.confidence);
    s += ",\"ch\":" + String(ap.channel);
    s += ",\"rssi\":" + String(ap.rssi);
    s += ",\"ts\":" + String(ap.last_ts) + "}";
  }
  s += "]}";
  return s;
}

static String ncl_json_stas() {
  char mac[18];
  String s = "{\"count\":" + String(g_ncl.sta_count) + ",\"stas\":[";
  bool first = true;
  for (int i = 0; i < NCL_MAX_STAS; i++) {
    NclSTA &sta = g_ncl.stas[i];
    if (!sta.active) continue;
    if (!first) s += ",";
    first = false;
    ncl_mac_str(sta.mac, mac);
    s += "{\"mac\":\"" + String(mac) + "\"";
    s += ",\"role\":\"" + String(NCL_ROLE_NAMES[sta.role]) + "\"";
    s += ",\"conf\":" + String(sta.confidence);
    s += ",\"rand\":" + String(sta.randomized?"true":"false");
    s += ",\"probes\":" + String(sta.probe_count);
    s += ",\"rssi\":" + String(sta.best_rssi);
    s += ",\"ts\":" + String(sta.last_ts) + "}";
  }
  s += "]}";
  return s;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void ncl_routes() {
  server.on("/api/ncl/aps", HTTP_GET, []() {
    server.send(200, "application/json", ncl_json_aps());
  });

  server.on("/api/ncl/stas", HTTP_GET, []() {
    server.send(200, "application/json", ncl_json_stas());
  });

  server.on("/api/ncl/stats", HTTP_GET, []() {
    String s = "{\"ap_count\":" + String(g_ncl.ap_count);
    s += ",\"sta_count\":" + String(g_ncl.sta_count);
    s += ",\"classified_aps\":" + String(g_ncl.classified_aps);
    s += ",\"classified_stas\":" + String(g_ncl.classified_stas) + "}";
    server.send(200, "application/json", s);
  });

  server.on("/api/ncl/clear", HTTP_POST, []() {
    memset(&g_ncl, 0, sizeof(g_ncl));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

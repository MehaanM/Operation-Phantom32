#pragma once
// ap_database.h — Central AP database with full profile storage.
// Single source of truth for all discovered access points. Prefix: apdb_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define APDB_MAX_APS      96
#define APDB_SSID_LEN     33
#define APDB_VENDOR_LEN   24
#define APDB_RSSI_HIST    20

// ─── Security type ────────────────────────────────────────────────────────────
enum ApdbSec {
  APDB_SEC_OPEN    = 0,
  APDB_SEC_WEP     = 1,
  APDB_SEC_WPA     = 2,
  APDB_SEC_WPA2    = 3,
  APDB_SEC_WPA2_E  = 4,
  APDB_SEC_WPA3    = 5,
  APDB_SEC_WPA3_T  = 6, // WPA2/WPA3 transition
  APDB_SEC_OWE     = 7,
  APDB_SEC_UNKNOWN = 8,
};

// ─── AP record ────────────────────────────────────────────────────────────────
struct ApdbAP {
  bool    active;
  uint8_t bssid[6];
  char    ssid[APDB_SSID_LEN];
  char    vendor[APDB_VENDOR_LEN];
  uint8_t channel;
  uint8_t ds_channel;    // from DS IE
  int8_t  rssi;
  int8_t  rssi_hist[APDB_RSSI_HIST];
  uint8_t rssi_hist_idx;
  ApdbSec security;
  uint16_t capability;
  uint16_t beacon_interval;
  bool    hidden;        // zero-length SSID
  bool    wps;
  bool    ht;            // 802.11n
  bool    vht;           // 802.11ac
  bool    he;            // 802.11ax (WiFi 6)
  bool    pmf_required;
  uint8_t max_data_rate; // advertised Mbps
  uint8_t bandwidth;     // 20/40/80/160 MHz
  // Client count (associated stations observed)
  uint8_t  client_count;
  uint8_t  client_macs[8][6];
  // Anomaly flags
  bool    anomaly_ssid_changed;
  bool    anomaly_channel_mismatch;
  bool    anomaly_malformed;
  uint8_t anomaly_score;
  // Timestamps
  uint32_t first_seen;
  uint32_t last_seen;
  uint32_t beacon_count;
  uint32_t active_seconds;
  // Distance estimate (meters, from RSSI path loss)
  float   est_distance_m;
  // LittleFS persisted
  bool    saved;
};

// ─── State ────────────────────────────────────────────────────────────────────
static struct {
  ApdbAP   aps[APDB_MAX_APS];
  uint8_t  count;
  uint32_t total_beacons;
  uint32_t total_frames;
  uint32_t last_tick_ts;
} g_apdb = {};

// ─── Helpers ─────────────────────────────────────────────────────────────────
static bool apdb_mac_eq(const uint8_t *a, const uint8_t *b) { return memcmp(a,b,6)==0; }

static ApdbAP* apdb_find(const uint8_t *bssid) {
  for (int i = 0; i < APDB_MAX_APS; i++) {
    if (g_apdb.aps[i].active && apdb_mac_eq(g_apdb.aps[i].bssid, bssid))
      return &g_apdb.aps[i];
  }
  return nullptr;
}

static ApdbAP* apdb_alloc(const uint8_t *bssid) {
  for (int i = 0; i < APDB_MAX_APS; i++) {
    if (!g_apdb.aps[i].active) {
      ApdbAP &ap = g_apdb.aps[i];
      memset(&ap, 0, sizeof(ap));
      memcpy(ap.bssid, bssid, 6);
      ap.active = true;
      ap.first_seen = millis() / 1000;
      ap.rssi = -100;
      g_apdb.count++;
      return &ap;
    }
  }
  return nullptr;
}

// ─── RSSI to distance ─────────────────────────────────────────────────────────
static float apdb_rssi_to_dist(int8_t rssi, uint8_t channel) {
  float freq_mhz = (channel <= 13) ? (2412 + (channel-1)*5) : 5180;
  float txpower = 20.0f;
  float exp = (txpower - (float)rssi - 20.0f * log10f(freq_mhz) + 27.55f) / 20.0f;
  return powf(10.0f, exp);
}

// ─── Upsert ───────────────────────────────────────────────────────────────────
ApdbAP* apdb_upsert(const uint8_t *bssid, const char *ssid, int8_t rssi,
                     uint8_t channel, ApdbSec sec, bool ht, bool vht, bool he,
                     bool wps, bool pmf, uint16_t interval, uint16_t caps) {
  ApdbAP *ap = apdb_find(bssid);
  bool is_new = (ap == nullptr);
  if (is_new) {
    ap = apdb_alloc(bssid);
    if (!ap) return nullptr;
  }

  if (ssid && ssid[0]) {
    if (strcmp(ap->ssid, ssid) != 0 && ap->beacon_count > 1)
      ap->anomaly_ssid_changed = true;
    strncpy(ap->ssid, ssid, APDB_SSID_LEN - 1);
    ap->hidden = false;
  } else {
    ap->hidden = true;
  }

  ap->rssi = rssi;
  ap->rssi_hist[ap->rssi_hist_idx % APDB_RSSI_HIST] = rssi;
  ap->rssi_hist_idx++;

  ap->channel = channel;
  ap->security = sec;
  ap->ht = ht;
  ap->vht = vht;
  ap->he = he;
  ap->wps = wps;
  ap->pmf_required = pmf;
  ap->beacon_interval = interval;
  ap->capability = caps;
  ap->last_seen = millis() / 1000;
  ap->beacon_count++;
  ap->est_distance_m = apdb_rssi_to_dist(rssi, channel);
  g_apdb.total_beacons++;

  return ap;
}

// ─── Associate station ────────────────────────────────────────────────────────
void apdb_assoc_client(const uint8_t *bssid, const uint8_t *client) {
  ApdbAP *ap = apdb_find(bssid);
  if (!ap) return;
  for (int i = 0; i < ap->client_count; i++) {
    if (apdb_mac_eq(ap->client_macs[i], client)) return;
  }
  if (ap->client_count < 8) {
    memcpy(ap->client_macs[ap->client_count++], client, 6);
  }
}

// ─── Tick ─────────────────────────────────────────────────────────────────────
void apdb_tick() {
  uint32_t now = millis() / 1000;
  if (now == g_apdb.last_tick_ts) return;
  g_apdb.last_tick_ts = now;
  for (int i = 0; i < APDB_MAX_APS; i++) {
    ApdbAP &ap = g_apdb.aps[i];
    if (!ap.active) continue;
    if (now - ap.last_seen < 300) ap.active_seconds++;
  }
}

// ─── JSON ─────────────────────────────────────────────────────────────────────
static void apdb_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0],m[1],m[2],m[3],m[4],m[5]);
}

static const char* apdb_sec_str(ApdbSec s) {
  switch(s) {
    case APDB_SEC_OPEN:   return "open";
    case APDB_SEC_WEP:    return "wep";
    case APDB_SEC_WPA:    return "wpa";
    case APDB_SEC_WPA2:   return "wpa2";
    case APDB_SEC_WPA2_E: return "wpa2_e";
    case APDB_SEC_WPA3:   return "wpa3";
    case APDB_SEC_WPA3_T: return "wpa3_t";
    case APDB_SEC_OWE:    return "owe";
    default:               return "unknown";
  }
}

static String apdb_json_ap(const ApdbAP &ap) {
  char mac[18];
  apdb_mac_str(ap.bssid, mac);
  String s = "{\"bssid\":\"" + String(mac) + "\"";
  s += ",\"ssid\":\"" + String(ap.ssid) + "\"";
  s += ",\"vendor\":\"" + String(ap.vendor) + "\"";
  s += ",\"ch\":" + String(ap.channel);
  s += ",\"rssi\":" + String(ap.rssi);
  s += ",\"sec\":\"" + String(apdb_sec_str(ap.security)) + "\"";
  s += ",\"ht\":" + String(ap.ht?"true":"false");
  s += ",\"vht\":" + String(ap.vht?"true":"false");
  s += ",\"he\":" + String(ap.he?"true":"false");
  s += ",\"wps\":" + String(ap.wps?"true":"false");
  s += ",\"pmf\":" + String(ap.pmf_required?"true":"false");
  s += ",\"hidden\":" + String(ap.hidden?"true":"false");
  s += ",\"beacons\":" + String(ap.beacon_count);
  s += ",\"clients\":" + String(ap.client_count);
  s += ",\"dist_m\":" + String(ap.est_distance_m, 1);
  s += ",\"active_s\":" + String(ap.active_seconds);
  s += ",\"first_seen\":" + String(ap.first_seen);
  s += ",\"last_seen\":" + String(ap.last_seen);
  s += ",\"anomaly\":" + String(ap.anomaly_score);
  s += "}";
  return s;
}

static String apdb_json_all() {
  String s = "{\"count\":" + String(g_apdb.count);
  s += ",\"total_beacons\":" + String(g_apdb.total_beacons);
  s += ",\"aps\":[";
  bool first = true;
  for (int i = 0; i < APDB_MAX_APS; i++) {
    if (!g_apdb.aps[i].active) continue;
    if (!first) s += ",";
    first = false;
    s += apdb_json_ap(g_apdb.aps[i]);
  }
  s += "]}";
  return s;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void apdb_routes() {
  server.on("/api/apdb/all", HTTP_GET, []() {
    server.send(200, "application/json", apdb_json_all());
  });

  server.on("/api/apdb/ap", HTTP_GET, []() {
    if (!server.hasArg("bssid")) {
      server.send(400, "application/json", "{\"error\":\"missing bssid\"}"); return;
    }
    uint8_t bssid[6];
    sscanf(server.arg("bssid").c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
           &bssid[0],&bssid[1],&bssid[2],&bssid[3],&bssid[4],&bssid[5]);
    ApdbAP *ap = apdb_find(bssid);
    if (!ap) { server.send(404, "application/json", "{\"error\":\"not found\"}"); return; }
    server.send(200, "application/json", apdb_json_ap(*ap));
  });

  server.on("/api/apdb/count", HTTP_GET, []() {
    server.send(200, "application/json",
      "{\"count\":" + String(g_apdb.count) + ",\"beacons\":" + String(g_apdb.total_beacons) + "}");
  });

  server.on("/api/apdb/clear", HTTP_POST, []() {
    memset(&g_apdb, 0, sizeof(g_apdb));
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/apdb/open", HTTP_GET, []() {
    // List all open APs
    String s = "[";
    bool first = true;
    for (int i = 0; i < APDB_MAX_APS; i++) {
      ApdbAP &ap = g_apdb.aps[i];
      if (!ap.active || ap.security != APDB_SEC_OPEN) continue;
      if (!first) s += ",";
      first = false;
      s += apdb_json_ap(ap);
    }
    s += "]";
    server.send(200, "application/json", s);
  });
}

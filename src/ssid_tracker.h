#pragma once
// ssid_tracker.h — SSID observation history database with change detection.
// Tracks when SSIDs appear/disappear and BSSID-to-SSID mapping changes. Prefix: sst_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define SST_MAX_SSIDS 96
#define SST_MAX_BSSIDS 8  // per SSID
#define SST_SSID_LEN 33
#define SST_HISTORY_LEN 16  // channel history per SSID

// ─── SSID record ──────────────────────────────────────────────────────────────
struct SstSSID {
  bool valid;
  char ssid[SST_SSID_LEN];
  uint8_t hash;  // simple hash for fast compare

  // Associated BSSIDs
  uint8_t bssids[SST_MAX_BSSIDS][6];
  uint8_t bssid_count;

  // Activity stats
  uint32_t first_seen;
  uint32_t last_seen;
  uint32_t beacon_count;
  uint32_t probe_response_count;

  // Channel history
  uint8_t channels[SST_HISTORY_LEN];
  uint8_t ch_count;
  uint8_t primary_channel;

  // RSSI
  int8_t best_rssi;
  int8_t current_rssi;

  // Security changes
  uint8_t sec_seen;  // bitmask of security types ever observed
  bool sec_changed;  // security type changed during observation

  // Anomaly flags
  bool ssid_conflict;    // same SSID on different BSSIDs with different security
  bool hidden_ap_match;  // non-hidden SSID matches a known hidden AP BSSID
  bool recently_seen;    // seen in last 60s
  bool was_gone;         // disappeared and came back
};

// ─── State ────────────────────────────────────────────────────────────────────
static struct {
  SstSSID ssids[SST_MAX_SSIDS];
  uint32_t count;
  uint32_t total_beacons;
  uint32_t changes_detected;
  uint32_t last_cleanup_ts;
} g_sst = {};

// ─── Helpers ─────────────────────────────────────────────────────────────────
static uint8_t sst_hash(const char *s) {
  uint8_t h = 0;
  while (*s)
    h ^= (uint8_t)*s++;
  return h;
}

static bool sst_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static SstSSID *sst_find(const char *ssid) {
  uint8_t h = sst_hash(ssid);
  for (int i = 0; i < SST_MAX_SSIDS; i++) {
    if (g_sst.ssids[i].valid && g_sst.ssids[i].hash == h && strcmp(g_sst.ssids[i].ssid, ssid) == 0)
      return &g_sst.ssids[i];
  }
  return nullptr;
}

static SstSSID *sst_alloc(const char *ssid) {
  for (int i = 0; i < SST_MAX_SSIDS; i++) {
    if (!g_sst.ssids[i].valid) {
      SstSSID &s = g_sst.ssids[i];
      memset(&s, 0, sizeof(s));
      strncpy(s.ssid, ssid, SST_SSID_LEN - 1);
      s.hash = sst_hash(ssid);
      s.valid = true;
      s.best_rssi = -100;
      g_sst.count++;
      return &s;
    }
  }
  return nullptr;
}

static bool sst_has_bssid(SstSSID &s, const uint8_t *bssid) {
  for (int i = 0; i < s.bssid_count; i++) {
    if (sst_mac_eq(s.bssids[i], bssid))
      return true;
  }
  return false;
}

static bool sst_has_channel(SstSSID &s, uint8_t ch) {
  for (int i = 0; i < s.ch_count; i++) {
    if (s.channels[i] == ch)
      return true;
  }
  return false;
}

// ─── Ingest beacon/probe response ────────────────────────────────────────────
void sst_ingest(const char *ssid, const uint8_t *bssid, int8_t rssi, uint8_t channel,
                uint8_t security_type, bool is_probe_resp) {
  if (!ssid || !ssid[0] || !bssid)
    return;  // skip hidden

  uint32_t now = millis() / 1000;

  SstSSID *s = sst_find(ssid);
  bool is_new = (s == nullptr);
  if (is_new) {
    s = sst_alloc(ssid);
    if (!s)
      return;
    s->first_seen = now;
  }

  // Update timestamps and counts
  if (!s->recently_seen) {
    s->was_gone = !is_new;  // returned after being gone
    if (s->was_gone)
      g_sst.changes_detected++;
  }
  s->last_seen = now;
  s->recently_seen = true;
  g_sst.total_beacons++;

  if (is_probe_resp)
    s->probe_response_count++;
  else
    s->beacon_count++;

  // RSSI tracking
  if (rssi > s->best_rssi)
    s->best_rssi = rssi;
  s->current_rssi = rssi;

  // BSSID tracking
  if (!sst_has_bssid(*s, bssid) && s->bssid_count < SST_MAX_BSSIDS) {
    memcpy(s->bssids[s->bssid_count++], bssid, 6);
    if (!is_new)
      g_sst.changes_detected++;
  }

  // Channel tracking
  if (!sst_has_channel(*s, channel) && s->ch_count < SST_HISTORY_LEN) {
    s->channels[s->ch_count++] = channel;
  }
  s->primary_channel = channel;

  // Security type bitmask (bit 0=open, 1=wep, 2=wpa, 3=wpa2, 4=wpa3, etc.)
  uint8_t old_sec = s->sec_seen;
  if (security_type < 8)
    s->sec_seen |= (1 << security_type);
  if (!is_new && s->sec_seen != old_sec) {
    s->sec_changed = true;
    g_sst.changes_detected++;
  }

  // Conflict detection: >1 BSSID with different security
  if (s->bssid_count > 1 && __builtin_popcount(s->sec_seen) > 1) {
    s->ssid_conflict = true;
  }
}

// ─── Tick — mark stale entries ────────────────────────────────────────────────
void sst_tick() {
  uint32_t now = millis() / 1000;
  if (now - g_sst.last_cleanup_ts < 30)
    return;
  g_sst.last_cleanup_ts = now;

  for (int i = 0; i < SST_MAX_SSIDS; i++) {
    SstSSID &s = g_sst.ssids[i];
    if (!s.valid)
      continue;
    if (now - s.last_seen > 60)
      s.recently_seen = false;
  }
}

// ─── JSON ─────────────────────────────────────────────────────────────────────
static void sst_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String sst_json_all(bool active_only = false) {
  String s = "{\"count\":" + String(g_sst.count);
  s += ",\"total_beacons\":" + String(g_sst.total_beacons);
  s += ",\"changes\":" + String(g_sst.changes_detected);
  s += ",\"ssids\":[";
  bool first = true;
  for (int i = 0; i < SST_MAX_SSIDS; i++) {
    SstSSID &e = g_sst.ssids[i];
    if (!e.valid)
      continue;
    if (active_only && !e.recently_seen)
      continue;
    if (!first)
      s += ",";
    first = false;
    char mac[18];
    s += "{\"ssid\":\"" + String(e.ssid) + "\"";
    s += ",\"bssid_count\":" + String(e.bssid_count);
    s += ",\"channel\":" + String(e.primary_channel);
    s += ",\"rssi\":" + String(e.current_rssi);
    s += ",\"best_rssi\":" + String(e.best_rssi);
    s += ",\"beacons\":" + String(e.beacon_count);
    s += ",\"recent\":" + String(e.recently_seen ? "true" : "false");
    s += ",\"conflict\":" + String(e.ssid_conflict ? "true" : "false");
    s += ",\"sec_changed\":" + String(e.sec_changed ? "true" : "false");
    s += ",\"bssids\":[";
    for (int j = 0; j < e.bssid_count; j++) {
      if (j)
        s += ",";
      sst_mac_str(e.bssids[j], mac);
      s += "\"" + String(mac) + "\"";
    }
    s += "],\"first_seen\":" + String(e.first_seen);
    s += ",\"last_seen\":" + String(e.last_seen) + "}";
  }
  s += "]}";
  return s;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void sst_routes() {
  server.on("/api/sst/all", HTTP_GET, []() {
    bool active_only = server.hasArg("active");
    server.send(200, "application/json", sst_json_all(active_only));
  });

  server.on("/api/sst/conflicts", HTTP_GET, []() {
    String s = "[";
    bool first = true;
    for (int i = 0; i < SST_MAX_SSIDS; i++) {
      SstSSID &e = g_sst.ssids[i];
      if (!e.valid || !e.ssid_conflict)
        continue;
      if (!first)
        s += ",";
      first = false;
      s += "{\"ssid\":\"" + String(e.ssid) + "\"";
      s += ",\"bssid_count\":" + String(e.bssid_count) + "}";
    }
    s += "]";
    server.send(200, "application/json", s);
  });

  server.on("/api/sst/clear", HTTP_POST, []() {
    memset(&g_sst, 0, sizeof(g_sst));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

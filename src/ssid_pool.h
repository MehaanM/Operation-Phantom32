#pragma once
// ssid_pool.h — SSID observation pool and popularity tracker.
// Tracks which SSIDs appear most frequently, along with per-SSID AP count,
// security profile, and geographic spread across channels.
// Prefix: spl_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define SPL_MAX_SSIDS    64
#define SPL_MAX_BSSIDS    8  // per SSID
#define SPL_HIST_LEN     30

// Security profile flags (bitmask)
#define SPL_SEC_OPEN    0x00
#define SPL_SEC_WEP     0x01
#define SPL_SEC_WPA     0x02
#define SPL_SEC_WPA2    0x04
#define SPL_SEC_WPA3    0x08
#define SPL_SEC_OWE     0x10
#define SPL_SEC_MIXED   0x20

struct SplBssid {
  uint8_t mac[6];
  uint8_t ch;
  int8_t  rssi;
  uint8_t sec_flags;
  uint32_t last_ts;
};

struct SplSsid {
  bool    active;
  char    ssid[33];
  uint8_t ssid_len;

  // AP instances
  SplBssid bssids[SPL_MAX_BSSIDS];
  uint8_t  bssid_count;
  uint32_t total_beacons;    // total beacon frames seen

  // Channel spread
  uint32_t ch_bitmap;        // channels where this SSID appears (bitmask bit=ch)

  // Security profile (union of all APs)
  uint8_t  sec_union;
  bool     has_mixed_sec;    // different security on different APs (honeypot risk)
  bool     has_open;
  bool     has_wpa3;
  bool     is_hidden;

  // Probe interest
  uint32_t probe_requests;   // times probed by clients

  // Activity history (beacons per 10s bucket)
  uint8_t  beacon_hist[SPL_HIST_LEN];
  uint8_t  hist_head;
  uint32_t cur_beacons;
  uint32_t last_tick_s;

  // Channel spread diversity
  uint8_t  ch_count;

  uint32_t first_ts;
  uint32_t last_ts;
};

static struct {
  SplSsid  ssids[SPL_MAX_SSIDS];
  uint8_t  count;
  uint32_t total_beacons;
} g_spl = {};

static bool spl_mac_eq(const uint8_t *a, const uint8_t *b) { return memcmp(a,b,6)==0; }

static SplSsid* spl_find(const char *ssid) {
  for (int i = 0; i < SPL_MAX_SSIDS; i++)
    if (g_spl.ssids[i].active && strncmp(g_spl.ssids[i].ssid, ssid, 32) == 0)
      return &g_spl.ssids[i];
  return nullptr;
}

static SplSsid* spl_alloc(const char *ssid) {
  for (int i = 0; i < SPL_MAX_SSIDS; i++) {
    if (!g_spl.ssids[i].active) {
      SplSsid &s = g_spl.ssids[i];
      memset(&s, 0, sizeof(s));
      s.active = true;
      strncpy(s.ssid, ssid, 32);
      s.ssid_len = strlen(ssid);
      s.first_ts = millis()/1000;
      g_spl.count++;
      return &s;
    }
  }
  // Evict least-beaconed SSID
  uint32_t least = UINT32_MAX; int idx = 0;
  for (int i = 0; i < SPL_MAX_SSIDS; i++)
    if (g_spl.ssids[i].total_beacons < least) { least = g_spl.ssids[i].total_beacons; idx = i; }
  memset(&g_spl.ssids[idx], 0, sizeof(SplSsid));
  g_spl.ssids[idx].active = true;
  strncpy(g_spl.ssids[idx].ssid, ssid, 32);
  g_spl.ssids[idx].ssid_len = strlen(ssid);
  g_spl.ssids[idx].first_ts = millis()/1000;
  return &g_spl.ssids[idx];
}

static void spl_tick(SplSsid &s) {
  uint32_t now_s = millis()/1000;
  if (now_s == s.last_tick_s) return;
  s.beacon_hist[s.hist_head % SPL_HIST_LEN] = (uint8_t)(s.cur_beacons > 255 ? 255 : s.cur_beacons);
  s.hist_head = (s.hist_head + 1) % SPL_HIST_LEN;
  s.cur_beacons = 0;
  s.last_tick_s = now_s;
}

void spl_ingest_beacon(const uint8_t *bssid, const char *ssid, uint8_t ch, int8_t rssi,
                        uint8_t sec_flags, bool hidden) {
  if (!ssid || (!ssid[0] && !hidden)) return;

  const char *key = (ssid && ssid[0]) ? ssid : "[Hidden]";
  SplSsid *s = spl_find(key);
  if (!s) s = spl_alloc(key);
  if (!s) return;

  spl_tick(*s);
  s->total_beacons++;
  g_spl.total_beacons++;
  s->cur_beacons++;
  s->is_hidden = hidden;

  // Update channel bitmap
  if (ch >= 1 && ch <= 32) {
    if (!(s->ch_bitmap & (1UL << ch))) {
      s->ch_bitmap |= (1UL << ch);
      s->ch_count++;
    }
  }

  // Update security profile
  if (s->sec_union > 0 && s->sec_union != sec_flags) {
    s->has_mixed_sec = true;
    s->sec_union |= sec_flags;
  } else {
    s->sec_union = sec_flags;
  }

  if (sec_flags == SPL_SEC_OPEN) s->has_open = true;
  if (sec_flags & SPL_SEC_WPA3)  s->has_wpa3 = true;

  // Update or add BSSID entry
  for (int i = 0; i < s->bssid_count; i++) {
    if (spl_mac_eq(s->bssids[i].mac, bssid)) {
      s->bssids[i].ch = ch; s->bssids[i].rssi = rssi;
      s->bssids[i].sec_flags = sec_flags;
      s->bssids[i].last_ts = millis()/1000;
      s->last_ts = millis()/1000;
      return;
    }
  }
  if (s->bssid_count < SPL_MAX_BSSIDS) {
    SplBssid &b = s->bssids[s->bssid_count++];
    memcpy(b.mac, bssid, 6);
    b.ch = ch; b.rssi = rssi; b.sec_flags = sec_flags;
    b.last_ts = millis()/1000;
  }
  s->last_ts = millis()/1000;
}

void spl_ingest_probe_req(const char *ssid) {
  if (!ssid || !ssid[0]) return;
  SplSsid *s = spl_find(ssid);
  if (s) s->probe_requests++;
}

static String spl_hist_json(const uint8_t *hist, int len, int head) {
  String s = "[";
  for (int i = 0; i < len; i++) {
    if (i) s += ",";
    s += String(hist[(head+i)%len]);
  }
  return s + "]";
}

static const char* spl_sec_str(uint8_t flags) {
  if (flags == 0)         return "Open";
  if (flags & SPL_SEC_WPA3) return "WPA3";
  if (flags & SPL_SEC_WPA2) return "WPA2";
  if (flags & SPL_SEC_WPA)  return "WPA";
  if (flags & SPL_SEC_WEP)  return "WEP";
  return "Unknown";
}

static void spl_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String spl_json_all() {
  String s = "{\"count\":" + String(g_spl.count);
  s += ",\"total_beacons\":" + String(g_spl.total_beacons);
  s += ",\"ssids\":[";
  char mac[18]; bool first = true;
  for (int i = 0; i < SPL_MAX_SSIDS; i++) {
    SplSsid &ss = g_spl.ssids[i];
    if (!ss.active) continue;
    if (!first) s += ","; first = false;
    s += "{\"ssid\":\"" + String(ss.ssid) + "\"";
    s += ",\"len\":" + String(ss.ssid_len);
    s += ",\"bssid_count\":" + String(ss.bssid_count);
    s += ",\"total_beacons\":" + String(ss.total_beacons);
    s += ",\"probe_req\":" + String(ss.probe_requests);
    s += ",\"ch_count\":" + String(ss.ch_count);
    s += ",\"sec\":\"" + String(spl_sec_str(ss.sec_union)) + "\"";
    s += ",\"mixed_sec\":" + String(ss.has_mixed_sec ? "true" : "false");
    s += ",\"has_open\":" + String(ss.has_open ? "true" : "false");
    s += ",\"has_wpa3\":" + String(ss.has_wpa3 ? "true" : "false");
    s += ",\"hidden\":" + String(ss.is_hidden ? "true" : "false");
    s += ",\"beacon_hist\":" + spl_hist_json(ss.beacon_hist, SPL_HIST_LEN, ss.hist_head);
    s += ",\"bssids\":[";
    bool bf = true;
    for (int j = 0; j < ss.bssid_count; j++) {
      SplBssid &b = ss.bssids[j];
      if (!bf) s += ","; bf = false;
      spl_mac_str(b.mac, mac);
      s += "{\"bssid\":\"" + String(mac) + "\"";
      s += ",\"ch\":" + String(b.ch);
      s += ",\"rssi\":" + String(b.rssi);
      s += ",\"sec\":\"" + String(spl_sec_str(b.sec_flags)) + "\"}";
    }
    s += "]}";
  }
  s += "]}";
  return s;
}

inline void spl_routes() {
  server.on("/api/spl/all", HTTP_GET, []() {
    server.send(200, "application/json", spl_json_all());
  });
  server.on("/api/spl/clear", HTTP_POST, []() {
    memset(&g_spl, 0, sizeof(g_spl));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

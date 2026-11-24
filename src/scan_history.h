#pragma once
// scan_history.h — Persistent scan history with LittleFS storage.
// Saves AP snapshot records to /scans/ directory on LittleFS.
// Prefix: sch_

#include <Arduino.h>
#include <WebServer.h>
#include <LittleFS.h>

extern WebServer server;

#define SCH_MAX_RECORDS  100
#define SCH_DIR          "/scans"
#define SCH_RECORD_LEN   96   // bytes per packed record

struct SchRecord {
  uint8_t  bssid[6];
  char     ssid[33];
  int8_t   rssi;
  uint8_t  channel;
  uint8_t  security;  // 0=open, 1=wep, 2=wpa, 3=wpa2, 4=wpa3
  bool     wps;
  bool     pmf;
  uint32_t ts;        // unix-like epoch (millis/1000)
  uint32_t seq;       // global sequence number
};

static struct {
  SchRecord ring[SCH_MAX_RECORDS];
  uint16_t  head;
  uint16_t  count;
  uint32_t  total_saved;
  uint32_t  seq;
  bool      lfs_ok;
} g_sch = {};

// ─── LittleFS helpers ────────────────────────────────────────────────────────
static bool sch_lfs_init() {
  if (!LittleFS.begin(true)) return false;
  if (!LittleFS.exists(SCH_DIR)) LittleFS.mkdir(SCH_DIR);
  g_sch.lfs_ok = true;
  return true;
}

static String sch_path(uint32_t seq) {
  char buf[32];
  snprintf(buf, sizeof(buf), "%s/%08X.bin", SCH_DIR, seq);
  return String(buf);
}

static void sch_save_lfs(const SchRecord &r) {
  if (!g_sch.lfs_ok) return;
  String path = sch_path(r.seq);
  File f = LittleFS.open(path, "w");
  if (!f) return;
  f.write((const uint8_t*)&r, sizeof(r));
  f.close();
}

static bool sch_load_lfs(uint32_t seq, SchRecord &out) {
  String path = sch_path(seq);
  if (!LittleFS.exists(path)) return false;
  File f = LittleFS.open(path, "r");
  if (!f) return false;
  f.read((uint8_t*)&out, sizeof(out));
  f.close();
  return true;
}

// ─── Public API ──────────────────────────────────────────────────────────────
void sch_init() {
  sch_lfs_init();
}

void sch_add(const uint8_t *bssid, const char *ssid,
             int8_t rssi, uint8_t channel, uint8_t security,
             bool wps, bool pmf) {
  SchRecord &r = g_sch.ring[g_sch.head];
  memset(&r, 0, sizeof(r));
  memcpy(r.bssid, bssid, 6);
  strncpy(r.ssid, ssid ? ssid : "", 32);
  r.rssi     = rssi;
  r.channel  = channel;
  r.security = security;
  r.wps      = wps;
  r.pmf      = pmf;
  r.ts       = millis()/1000;
  r.seq      = g_sch.seq++;

  g_sch.head = (g_sch.head + 1) % SCH_MAX_RECORDS;
  if (g_sch.count < SCH_MAX_RECORDS) g_sch.count++;
  g_sch.total_saved++;

  sch_save_lfs(r);
}

// ─── JSON ─────────────────────────────────────────────────────────────────────
static void sch_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String sch_sec_name(uint8_t s) {
  switch (s) {
    case 0: return "Open";
    case 1: return "WEP";
    case 2: return "WPA";
    case 3: return "WPA2";
    case 4: return "WPA3";
  }
  return "?";
}

static String sch_json_recent(int limit) {
  if (limit <= 0 || limit > (int)g_sch.count) limit = (int)g_sch.count;
  String s = "[";
  char mac[18];
  bool first = true;
  int pos = ((int)g_sch.head - 1 + SCH_MAX_RECORDS) % SCH_MAX_RECORDS;
  for (int i = 0; i < limit; i++) {
    SchRecord &r = g_sch.ring[pos];
    if (!first) s += ",";
    first = false;
    sch_mac_str(r.bssid, mac);
    s += "{\"seq\":" + String(r.seq);
    s += ",\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(r.ssid) + "\"";
    s += ",\"rssi\":" + String(r.rssi);
    s += ",\"ch\":" + String(r.channel);
    s += ",\"sec\":\"" + sch_sec_name(r.security) + "\"";
    s += ",\"wps\":" + String(r.wps?"true":"false");
    s += ",\"pmf\":" + String(r.pmf?"true":"false");
    s += ",\"ts\":" + String(r.ts) + "}";
    pos = (pos - 1 + SCH_MAX_RECORDS) % SCH_MAX_RECORDS;
  }
  s += "]";
  return s;
}

static String sch_json_stats() {
  // Count per-security
  uint32_t sec_count[5] = {0,0,0,0,0};
  for (int i = 0; i < (int)g_sch.count; i++) {
    uint8_t s = g_sch.ring[i].security;
    if (s < 5) sec_count[s]++;
  }
  String s = "{\"count\":" + String(g_sch.count);
  s += ",\"total_saved\":" + String(g_sch.total_saved);
  s += ",\"lfs_ok\":" + String(g_sch.lfs_ok?"true":"false");
  s += ",\"open\":" + String(sec_count[0]);
  s += ",\"wep\":" + String(sec_count[1]);
  s += ",\"wpa\":" + String(sec_count[2]);
  s += ",\"wpa2\":" + String(sec_count[3]);
  s += ",\"wpa3\":" + String(sec_count[4]);
  s += "}";
  return s;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void sch_routes() {
  server.on("/api/sch/recent", HTTP_GET, []() {
    int limit = 50;
    if (server.hasArg("limit")) limit = server.arg("limit").toInt();
    server.send(200, "application/json", sch_json_recent(limit));
  });
  server.on("/api/sch/stats", HTTP_GET, []() {
    server.send(200, "application/json", sch_json_stats());
  });
  server.on("/api/sch/clear", HTTP_POST, []() {
    // Clear in-memory ring
    memset(g_sch.ring, 0, sizeof(g_sch.ring));
    g_sch.head = 0;
    g_sch.count = 0;
    // Remove all files from LittleFS
    if (g_sch.lfs_ok) {
      File dir = LittleFS.open(SCH_DIR);
      if (dir) {
        File f = dir.openNextFile();
        while (f) {
          String name = String(SCH_DIR) + "/" + f.name();
          f.close();
          LittleFS.remove(name);
          f = dir.openNextFile();
        }
        dir.close();
      }
    }
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

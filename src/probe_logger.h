#pragma once
// probe_logger.h — Probe request logger with SSID frequency analysis.
// Captures and indexes all probe requests from stations. Prefix: plog_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define PLOG_MAX_ENTRIES  256
#define PLOG_MAX_SSIDS     64
#define PLOG_SSID_LEN      33
#define PLOG_MAX_STAS      64

// ─── Probe log entry ─────────────────────────────────────────────────────────
struct PlogEntry {
  bool    valid;
  uint8_t src[6];
  char    ssid[PLOG_SSID_LEN];
  bool    directed;    // false = wildcard probe
  int8_t  rssi;
  uint8_t channel;
  uint32_t ts;
};

// ─── Per-SSID aggregate ───────────────────────────────────────────────────────
struct PlogSSID {
  bool   valid;
  char   ssid[PLOG_SSID_LEN];
  uint32_t count;
  uint32_t last_ts;
  int8_t   best_rssi;
  uint8_t  unique_stas;
};

// ─── Per-station probe history ────────────────────────────────────────────────
struct PlogSta {
  bool    valid;
  uint8_t mac[6];
  uint16_t probe_count;
  uint16_t directed_count;
  uint16_t wildcard_count;
  uint32_t last_ts;
  int8_t   rssi;
  uint8_t  ssid_count;
  char     ssids[8][PLOG_SSID_LEN];
};

// ─── State ────────────────────────────────────────────────────────────────────
static struct {
  PlogEntry entries[PLOG_MAX_ENTRIES];
  uint16_t  head;    // ring buffer head
  uint32_t  total;

  PlogSSID  ssids[PLOG_MAX_SSIDS];
  uint16_t  ssid_count;

  PlogSta   stas[PLOG_MAX_STAS];
  uint16_t  sta_count;
} g_plog = {};

// ─── Helpers ─────────────────────────────────────────────────────────────────
static bool plog_mac_eq(const uint8_t *a, const uint8_t *b) { return memcmp(a,b,6)==0; }

static PlogSta* plog_find_sta(const uint8_t *mac) {
  for (int i = 0; i < PLOG_MAX_STAS; i++) {
    if (g_plog.stas[i].valid && plog_mac_eq(g_plog.stas[i].mac, mac))
      return &g_plog.stas[i];
  }
  return nullptr;
}

static PlogSta* plog_alloc_sta(const uint8_t *mac) {
  for (int i = 0; i < PLOG_MAX_STAS; i++) {
    if (!g_plog.stas[i].valid) {
      memset(&g_plog.stas[i], 0, sizeof(g_plog.stas[i]));
      memcpy(g_plog.stas[i].mac, mac, 6);
      g_plog.stas[i].valid = true;
      g_plog.sta_count++;
      return &g_plog.stas[i];
    }
  }
  return nullptr;
}

static PlogSSID* plog_find_ssid(const char *ssid) {
  for (int i = 0; i < PLOG_MAX_SSIDS; i++) {
    if (g_plog.ssids[i].valid && strcmp(g_plog.ssids[i].ssid, ssid) == 0)
      return &g_plog.ssids[i];
  }
  return nullptr;
}

static PlogSSID* plog_alloc_ssid(const char *ssid) {
  for (int i = 0; i < PLOG_MAX_SSIDS; i++) {
    if (!g_plog.ssids[i].valid) {
      memset(&g_plog.ssids[i], 0, sizeof(g_plog.ssids[i]));
      strncpy(g_plog.ssids[i].ssid, ssid, PLOG_SSID_LEN - 1);
      g_plog.ssids[i].valid = true;
      g_plog.ssids[i].best_rssi = -100;
      g_plog.ssid_count++;
      return &g_plog.ssids[i];
    }
  }
  return nullptr;
}

// ─── Ingest probe request ─────────────────────────────────────────────────────
void plog_ingest(const uint8_t *src, const char *ssid, int8_t rssi, uint8_t channel) {
  if (!src) return;

  // Ring buffer entry
  PlogEntry &e = g_plog.entries[g_plog.head % PLOG_MAX_ENTRIES];
  e.valid = true;
  memcpy(e.src, src, 6);
  strncpy(e.ssid, ssid ? ssid : "", PLOG_SSID_LEN - 1);
  e.directed = (ssid && ssid[0]);
  e.rssi = rssi;
  e.channel = channel;
  e.ts = millis() / 1000;
  g_plog.head = (g_plog.head + 1) % PLOG_MAX_ENTRIES;
  g_plog.total++;

  // Per-STA tracking
  PlogSta *sta = plog_find_sta(src);
  if (!sta) sta = plog_alloc_sta(src);
  if (sta) {
    sta->probe_count++;
    sta->last_ts = e.ts;
    sta->rssi = rssi;
    if (e.directed) {
      sta->directed_count++;
      // Add to sta SSID list
      bool found = false;
      for (int i = 0; i < sta->ssid_count; i++) {
        if (strcmp(sta->ssids[i], ssid) == 0) { found = true; break; }
      }
      if (!found && sta->ssid_count < 8)
        strncpy(sta->ssids[sta->ssid_count++], ssid, PLOG_SSID_LEN-1);
    } else {
      sta->wildcard_count++;
    }
  }

  // Per-SSID aggregate
  if (ssid && ssid[0]) {
    PlogSSID *ps = plog_find_ssid(ssid);
    if (!ps) ps = plog_alloc_ssid(ssid);
    if (ps) {
      ps->count++;
      ps->last_ts = e.ts;
      if (rssi > ps->best_rssi) ps->best_rssi = rssi;
    }
  }
}

// ─── JSON ─────────────────────────────────────────────────────────────────────
static void plog_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String plog_json_recent(int limit=50) {
  String s = "{\"total\":" + String(g_plog.total);
  s += ",\"ssid_count\":" + String(g_plog.ssid_count);
  s += ",\"sta_count\":" + String(g_plog.sta_count);
  s += ",\"recent\":[";
  char mac[18];
  int count = 0;
  bool first = true;
  // Walk backwards from head
  int start = (g_plog.head + PLOG_MAX_ENTRIES - 1) % PLOG_MAX_ENTRIES;
  for (int i = 0; i < PLOG_MAX_ENTRIES && count < limit; i++) {
    int idx = (start - i + PLOG_MAX_ENTRIES) % PLOG_MAX_ENTRIES;
    PlogEntry &e = g_plog.entries[idx];
    if (!e.valid) continue;
    if (!first) s += ",";
    first = false;
    plog_mac_str(e.src, mac);
    s += "{\"ts\":" + String(e.ts);
    s += ",\"src\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(e.ssid) + "\"";
    s += ",\"directed\":" + String(e.directed?"true":"false");
    s += ",\"rssi\":" + String(e.rssi);
    s += ",\"ch\":" + String(e.channel) + "}";
    count++;
  }
  s += "],\"top_ssids\":[";
  first = true;
  // Sort top 20 SSIDs by count (simple selection)
  PlogSSID sorted[20];
  int ns = 0;
  for (int i = 0; i < PLOG_MAX_SSIDS && ns < 20; i++) {
    if (g_plog.ssids[i].valid) sorted[ns++] = g_plog.ssids[i];
  }
  // Bubble sort top 20
  for (int i = 0; i < ns-1; i++)
    for (int j = i+1; j < ns; j++)
      if (sorted[j].count > sorted[i].count) {
        PlogSSID tmp = sorted[i]; sorted[i]=sorted[j]; sorted[j]=tmp;
      }
  for (int i = 0; i < ns; i++) {
    if (!first) s += ",";
    first = false;
    s += "{\"ssid\":\"" + String(sorted[i].ssid) + "\"";
    s += ",\"count\":" + String(sorted[i].count);
    s += ",\"rssi\":" + String(sorted[i].best_rssi) + "}";
  }
  s += "]}";
  return s;
}

static String plog_json_stas() {
  String s = "[";
  char mac[18];
  bool first = true;
  for (int i = 0; i < PLOG_MAX_STAS; i++) {
    PlogSta &sta = g_plog.stas[i];
    if (!sta.valid) continue;
    if (!first) s += ",";
    first = false;
    plog_mac_str(sta.mac, mac);
    s += "{\"mac\":\"" + String(mac) + "\"";
    s += ",\"total\":" + String(sta.probe_count);
    s += ",\"directed\":" + String(sta.directed_count);
    s += ",\"wildcard\":" + String(sta.wildcard_count);
    s += ",\"rssi\":" + String(sta.rssi);
    s += ",\"last_ts\":" + String(sta.last_ts);
    s += ",\"ssids\":[";
    for (int j = 0; j < sta.ssid_count; j++) {
      if (j) s += ",";
      s += "\"" + String(sta.ssids[j]) + "\"";
    }
    s += "]}";
  }
  s += "]";
  return s;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void plog_routes() {
  server.on("/api/plog/recent", HTTP_GET, []() {
    int limit = server.hasArg("limit") ? server.arg("limit").toInt() : 50;
    if (limit < 1 || limit > 256) limit = 50;
    server.send(200, "application/json", plog_json_recent(limit));
  });

  server.on("/api/plog/stas", HTTP_GET, []() {
    server.send(200, "application/json", plog_json_stas());
  });

  server.on("/api/plog/clear", HTTP_POST, []() {
    memset(&g_plog, 0, sizeof(g_plog));
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/plog/stats", HTTP_GET, []() {
    String s = "{\"total\":" + String(g_plog.total);
    s += ",\"ssid_count\":" + String(g_plog.ssid_count);
    s += ",\"sta_count\":" + String(g_plog.sta_count) + "}";
    server.send(200, "application/json", s);
  });
}

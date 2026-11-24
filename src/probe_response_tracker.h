#pragma once
// probe_response_tracker.h — Probe request/response correlation engine.
// Tracks which APs respond to which probe SSIDs and correlates timing.
// Prefix: prt_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define PRT_MAX_PROBES   128
#define PRT_MAX_APS       64
#define PRT_MAX_CORR     256  // correlation table entries

struct PrtProbe {
  bool    active;
  uint8_t src[6];       // client MAC
  char    ssid[33];     // requested SSID (empty = wildcard)
  uint8_t channel;
  int8_t  rssi;
  uint32_t ts_ms;
  bool    is_directed;  // has specific SSID vs wildcard
  uint32_t resp_count;  // number of probe responses observed
};

struct PrtResponse {
  bool     active;
  uint8_t  ap_bssid[6];
  char     ssid[33];
  uint8_t  channel;
  int8_t   rssi;
  uint32_t ts_ms;
  uint32_t latency_ms;  // response latency from matched probe
};

struct PrtCorrelation {
  bool     active;
  uint8_t  client[6];
  uint8_t  ap[6];
  char     ssid[33];
  uint32_t probe_ts;
  uint32_t resp_ts;
  uint32_t latency_ms;
  uint8_t  channel;
};

struct PrtAPStat {
  bool    active;
  uint8_t bssid[6];
  char    ssid[33];
  uint32_t responses_sent;
  uint32_t wildcard_responses;
  uint32_t directed_responses;
  uint32_t avg_latency_ms;
  bool     karma_suspect;    // responds to ALL probes including wildcard
};

static struct {
  PrtProbe   probes[PRT_MAX_PROBES];
  uint8_t    probe_head;
  uint32_t   probe_count;

  PrtCorrelation corr[PRT_MAX_CORR];
  uint8_t    corr_head;
  uint32_t   corr_count;

  PrtAPStat  ap_stats[PRT_MAX_APS];
  uint8_t    ap_count;

  uint32_t   total_probes;
  uint32_t   total_responses;
  uint32_t   total_correlations;
  uint32_t   karma_suspects;
} g_prt = {};

static bool prt_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static PrtAPStat* prt_find_or_alloc_ap(const uint8_t *bssid, const char *ssid) {
  for (int i = 0; i < PRT_MAX_APS; i++)
    if (g_prt.ap_stats[i].active && prt_mac_eq(g_prt.ap_stats[i].bssid, bssid))
      return &g_prt.ap_stats[i];
  for (int i = 0; i < PRT_MAX_APS; i++) {
    if (!g_prt.ap_stats[i].active) {
      PrtAPStat &a = g_prt.ap_stats[i];
      memset(&a, 0, sizeof(a));
      a.active = true;
      memcpy(a.bssid, bssid, 6);
      if (ssid) strncpy(a.ssid, ssid, 32);
      g_prt.ap_count++;
      return &a;
    }
  }
  // LRU evict – just use first slot
  memset(&g_prt.ap_stats[0], 0, sizeof(g_prt.ap_stats[0]));
  g_prt.ap_stats[0].active = true;
  memcpy(g_prt.ap_stats[0].bssid, bssid, 6);
  if (ssid) strncpy(g_prt.ap_stats[0].ssid, ssid, 32);
  return &g_prt.ap_stats[0];
}

void prt_ingest_probe(const uint8_t *src, const char *ssid,
                      uint8_t channel, int8_t rssi) {
  g_prt.total_probes++;
  PrtProbe &p = g_prt.probes[g_prt.probe_head];
  memset(&p, 0, sizeof(p));
  p.active = true;
  memcpy(p.src, src, 6);
  if (ssid) strncpy(p.ssid, ssid, 32);
  p.channel     = channel;
  p.rssi        = rssi;
  p.ts_ms       = millis();
  p.is_directed = ssid && ssid[0];
  g_prt.probe_head = (g_prt.probe_head + 1) % PRT_MAX_PROBES;
  g_prt.probe_count = min((uint32_t)PRT_MAX_PROBES, g_prt.probe_count + 1);
}

void prt_ingest_response(const uint8_t *ap_bssid, const char *ssid,
                         uint8_t channel, int8_t rssi) {
  g_prt.total_responses++;
  uint32_t now = millis();

  // Look for a recent matching probe within 500ms
  uint32_t best_latency = UINT32_MAX;
  uint8_t  best_client[6] = {};
  bool     found = false;
  bool     was_wildcard = false;

  for (int i = 0; i < PRT_MAX_PROBES; i++) {
    PrtProbe &pr = g_prt.probes[i];
    if (!pr.active) continue;
    uint32_t age = now - pr.ts_ms;
    if (age > 500) continue;
    // Match: same SSID or wildcard probe
    bool match = !pr.is_directed ||
                 (ssid && strcmp(pr.ssid, ssid) == 0);
    if (!match) continue;
    if (age < best_latency) {
      best_latency = age;
      memcpy(best_client, pr.src, 6);
      was_wildcard = !pr.is_directed;
      found = true;
      pr.resp_count++;
    }
  }

  // Update AP stat
  PrtAPStat *ap = prt_find_or_alloc_ap(ap_bssid, ssid);
  if (ap) {
    ap->responses_sent++;
    if (was_wildcard) ap->wildcard_responses++;
    else              ap->directed_responses++;
    // EMA latency
    if (found && best_latency != UINT32_MAX)
      ap->avg_latency_ms = (ap->avg_latency_ms * 7 + best_latency) / 8;
    // KARMA suspect: responds to wildcard probes
    if (ap->wildcard_responses > 3) {
      ap->karma_suspect = true;
      g_prt.karma_suspects = 0;
      for (int i = 0; i < PRT_MAX_APS; i++)
        if (g_prt.ap_stats[i].active && g_prt.ap_stats[i].karma_suspect)
          g_prt.karma_suspects++;
    }
  }

  // Record correlation
  if (found) {
    g_prt.total_correlations++;
    PrtCorrelation &c = g_prt.corr[g_prt.corr_head];
    memset(&c, 0, sizeof(c));
    c.active = true;
    memcpy(c.client, best_client, 6);
    memcpy(c.ap,     ap_bssid, 6);
    if (ssid) strncpy(c.ssid, ssid, 32);
    c.resp_ts     = now;
    c.latency_ms  = best_latency;
    c.channel     = channel;
    g_prt.corr_head = (g_prt.corr_head + 1) % PRT_MAX_CORR;
    g_prt.corr_count = min((uint32_t)PRT_MAX_CORR, g_prt.corr_count + 1);
  }
}

static void prt_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String prt_json_stats() {
  String s = "{\"total_probes\":" + String(g_prt.total_probes);
  s += ",\"total_responses\":" + String(g_prt.total_responses);
  s += ",\"total_correlations\":" + String(g_prt.total_correlations);
  s += ",\"karma_suspects\":" + String(g_prt.karma_suspects);
  s += ",\"ap_count\":" + String(g_prt.ap_count);
  s += ",\"aps\":[";
  char mac[18];
  bool first = true;
  for (int i = 0; i < PRT_MAX_APS; i++) {
    PrtAPStat &a = g_prt.ap_stats[i];
    if (!a.active) continue;
    if (!first) s += ",";
    first = false;
    prt_mac_str(a.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(a.ssid) + "\"";
    s += ",\"responses\":" + String(a.responses_sent);
    s += ",\"wildcard\":" + String(a.wildcard_responses);
    s += ",\"directed\":" + String(a.directed_responses);
    s += ",\"avg_latency\":" + String(a.avg_latency_ms);
    s += ",\"karma\":" + String(a.karma_suspect ? "true" : "false");
    s += "}";
  }
  s += "]}";
  return s;
}

static String prt_json_correlations(int limit = 50) {
  String s = "[";
  char mac1[18], mac2[18];
  int shown = 0;
  // Walk backwards from most recent
  for (int i = 0; i < PRT_MAX_CORR && shown < limit; i++) {
    int idx = ((int)g_prt.corr_head - 1 - i + PRT_MAX_CORR) % PRT_MAX_CORR;
    PrtCorrelation &c = g_prt.corr[idx];
    if (!c.active) continue;
    if (shown) s += ",";
    prt_mac_str(c.client, mac1);
    prt_mac_str(c.ap, mac2);
    s += "{\"client\":\"" + String(mac1) + "\"";
    s += ",\"ap\":\"" + String(mac2) + "\"";
    s += ",\"ssid\":\"" + String(c.ssid) + "\"";
    s += ",\"ch\":" + String(c.channel);
    s += ",\"latency\":" + String(c.latency_ms);
    s += ",\"ts\":" + String(c.resp_ts);
    s += "}";
    shown++;
  }
  s += "]";
  return s;
}

inline void prt_routes() {
  server.on("/api/prt/stats", HTTP_GET, []() {
    server.send(200, "application/json", prt_json_stats());
  });
  server.on("/api/prt/correlations", HTTP_GET, []() {
    server.send(200, "application/json", prt_json_correlations(50));
  });
  server.on("/api/prt/clear", HTTP_POST, []() {
    memset(&g_prt, 0, sizeof(g_prt));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

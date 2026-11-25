#pragma once
// probe_flood_detector.h — 802.11 probe request flood and scanner detector.
// Detects MAC randomization, rapid channel sweeps, targeted network scanners,
// and passive vs active scanning strategies.
// Prefix: pfd_

#include <Arduino.h>
#include <WebServer.h>
#include "psram_attr.h"

extern WebServer server;

#define PFD_MAX_SCANNERS  32
#define PFD_MAX_EVENTS    48
#define PFD_HIST_LEN      20

// Scanner classification
enum PfdScannerType : uint8_t {
  PFD_SCAN_UNKNOWN    = 0,
  PFD_SCAN_PASSIVE    = 1,   // No probe requests (passive scan only)
  PFD_SCAN_ACTIVE     = 2,   // Active scanning (sends probes)
  PFD_SCAN_DIRECTED   = 3,   // Directed probes to specific SSIDs
  PFD_SCAN_FLOOD      = 4,   // Probe flood (> 20/sec)
  PFD_SCAN_SWEEP      = 5,   // Multi-channel sweep (all channels < 100ms each)
  PFD_SCAN_WLAN_TOOL  = 6,   // Pattern matches known scanner tools
};

static const char* pfd_type_str(PfdScannerType t) {
  switch (t) {
    case PFD_SCAN_PASSIVE:   return "Passive";
    case PFD_SCAN_ACTIVE:    return "Active";
    case PFD_SCAN_DIRECTED:  return "Directed";
    case PFD_SCAN_FLOOD:     return "Flood";
    case PFD_SCAN_SWEEP:     return "Sweep";
    case PFD_SCAN_WLAN_TOOL: return "Scanner";
    default:                 return "Unknown";
  }
}

struct PfdProbeEntry {
  char    ssid[33];
  uint32_t count;
  uint32_t last_ts;
};

struct PfdScanner {
  bool    active;
  uint8_t mac[6];
  bool    is_random_mac;    // locally-administered bit
  PfdScannerType type;

  // Probe stats
  uint32_t probe_count;
  uint32_t wildcard_count;  // null/empty SSID probes
  uint32_t directed_count;  // probes to specific SSIDs

  // Rate tracking
  uint32_t probes_per_sec;
  uint32_t peak_rate;
  uint8_t  rate_hist[PFD_HIST_LEN];
  uint8_t  hist_head;
  uint32_t cur_probes;
  uint32_t last_tick_s;

  // Channel tracking
  uint8_t  channels_hit;         // bitmask of 2.4GHz channels
  uint8_t  channels_hit_5g;      // bitmask of 5GHz categories
  uint8_t  unique_channels;
  uint32_t last_ch_ms;
  uint16_t min_ch_gap_ms;        // fastest channel switch observed

  // SSID target list
  PfdProbeEntry targets[8];
  uint8_t  target_count;

  // Event counters
  uint32_t flood_events;
  uint32_t sweep_events;

  // RSSI range
  int8_t   rssi_min;
  int8_t   rssi_max;

  uint32_t first_ts;
  uint32_t last_ts;
};

struct PfdEvent {
  uint8_t       mac[6];
  PfdScannerType type;
  char          ssid[33];
  uint8_t       ch;
  uint32_t      rate;
  uint32_t      ts;
};

typedef struct {
  PfdScanner  scanners[PFD_MAX_SCANNERS];
  uint8_t     count;
  PfdEvent    events[PFD_MAX_EVENTS];
  uint8_t     evt_head;
  uint8_t     evt_count;
  uint32_t    total_probes;
  uint32_t    total_flood_events;
  uint32_t    total_sweep_events;
} G_PfdState;
#ifdef BOARD_HAS_PSRAM
static G_PfdState* _g_pfd_raw;
#define g_pfd (*_g_pfd_raw)
#else
static G_PfdState g_pfd;
#endif

static bool pfd_mac_eq(const uint8_t *a, const uint8_t *b) { return memcmp(a,b,6)==0; }
static bool pfd_is_random(const uint8_t *m) { return (m[0] & 0x02) != 0; }

static PfdScanner* pfd_find(const uint8_t *mac) {
  for (int i = 0; i < PFD_MAX_SCANNERS; i++)
    if (g_pfd.scanners[i].active && pfd_mac_eq(g_pfd.scanners[i].mac, mac))
      return &g_pfd.scanners[i];
  return nullptr;
}

static PfdScanner* pfd_alloc(const uint8_t *mac) {
  for (int i = 0; i < PFD_MAX_SCANNERS; i++) {
    if (!g_pfd.scanners[i].active) {
      PfdScanner &s = g_pfd.scanners[i];
      memset(&s, 0, sizeof(s));
      s.active = true;
      memcpy(s.mac, mac, 6);
      s.is_random_mac = pfd_is_random(mac);
      s.first_ts = millis()/1000;
      s.rssi_min = 0; s.rssi_max = -100;
      s.min_ch_gap_ms = 0xFFFF;
      g_pfd.count++;
      return &s;
    }
  }
  uint32_t oldest = UINT32_MAX; int idx = 0;
  for (int i = 0; i < PFD_MAX_SCANNERS; i++)
    if (g_pfd.scanners[i].last_ts < oldest) { oldest = g_pfd.scanners[i].last_ts; idx = i; }
  memset(&g_pfd.scanners[idx], 0, sizeof(PfdScanner));
  g_pfd.scanners[idx].active = true;
  memcpy(g_pfd.scanners[idx].mac, mac, 6);
  g_pfd.scanners[idx].is_random_mac = pfd_is_random(mac);
  g_pfd.scanners[idx].first_ts = millis()/1000;
  g_pfd.scanners[idx].rssi_min = 0; g_pfd.scanners[idx].rssi_max = -100;
  g_pfd.scanners[idx].min_ch_gap_ms = 0xFFFF;
  return &g_pfd.scanners[idx];
}

static void pfd_tick(PfdScanner &s) {
  uint32_t now_s = millis()/1000;
  if (now_s == s.last_tick_s) return;
  s.probes_per_sec = s.cur_probes;
  if (s.cur_probes > s.peak_rate) s.peak_rate = s.cur_probes;
  s.rate_hist[s.hist_head % PFD_HIST_LEN] = (uint8_t)(s.cur_probes > 255 ? 255 : s.cur_probes);
  s.hist_head = (s.hist_head + 1) % PFD_HIST_LEN;
  s.cur_probes = 0;
  s.last_tick_s = now_s;
}

static void pfd_add_target(PfdScanner &s, const char *ssid) {
  for (int i = 0; i < s.target_count; i++) {
    if (strcmp(s.targets[i].ssid, ssid) == 0) {
      s.targets[i].count++;
      s.targets[i].last_ts = millis()/1000;
      return;
    }
  }
  if (s.target_count < 8) {
    strncpy(s.targets[s.target_count].ssid, ssid, 32);
    s.targets[s.target_count].count = 1;
    s.targets[s.target_count].last_ts = millis()/1000;
    s.target_count++;
  }
}

static void pfd_push_event(const uint8_t *mac, PfdScannerType type,
                            const char *ssid, uint8_t ch, uint32_t rate) {
  PfdEvent &e = g_pfd.events[g_pfd.evt_head % PFD_MAX_EVENTS];
  memset(&e, 0, sizeof(e));
  memcpy(e.mac, mac, 6);
  e.type = type;
  if (ssid) strncpy(e.ssid, ssid, 32);
  e.ch = ch; e.rate = rate;
  e.ts = millis()/1000;
  g_pfd.evt_head = (g_pfd.evt_head + 1) % PFD_MAX_EVENTS;
  if (g_pfd.evt_count < PFD_MAX_EVENTS) g_pfd.evt_count++;
}

void pfd_ingest(const uint8_t *mac, const char *ssid, uint8_t ch, int8_t rssi) {
  PfdScanner *s = pfd_find(mac);
  if (!s) s = pfd_alloc(mac);
  if (!s) return;

  pfd_tick(*s);
  s->probe_count++;
  s->cur_probes++;
  g_pfd.total_probes++;

  if (rssi > s->rssi_max) s->rssi_max = rssi;
  if (rssi < s->rssi_min || s->probe_count == 1) s->rssi_min = rssi;

  bool is_wildcard = (!ssid || ssid[0] == 0);
  if (is_wildcard) s->wildcard_count++;
  else { s->directed_count++; pfd_add_target(*s, ssid); }

  // Channel sweep detection
  uint32_t now_ms = millis();
  if (s->last_ch_ms > 0) {
    uint32_t gap_ms = now_ms - s->last_ch_ms;
    if (gap_ms < s->min_ch_gap_ms && gap_ms > 0) s->min_ch_gap_ms = (uint16_t)gap_ms;
    if (gap_ms < 100) {
      s->channels_hit |= (ch < 14) ? (1 << (ch % 8)) : 0;
      uint8_t unique = __builtin_popcount(s->channels_hit);
      if (unique >= 6) {
        s->sweep_events++;
        g_pfd.total_sweep_events++;
        s->type = PFD_SCAN_SWEEP;
        pfd_push_event(mac, PFD_SCAN_SWEEP, ssid, ch, s->probes_per_sec);
        s->channels_hit = 0;
      }
    }
  }
  s->last_ch_ms = now_ms;

  // Flood detection
  if (s->probes_per_sec >= 20) {
    s->flood_events++;
    g_pfd.total_flood_events++;
    if (s->type < PFD_SCAN_FLOOD) s->type = PFD_SCAN_FLOOD;
    pfd_push_event(mac, PFD_SCAN_FLOOD, ssid, ch, s->probes_per_sec);
  } else if (s->type == PFD_SCAN_UNKNOWN) {
    s->type = is_wildcard ? PFD_SCAN_ACTIVE : PFD_SCAN_DIRECTED;
  }

  s->last_ts = millis()/1000;
}

static void pfd_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String pfd_hist_json(const uint8_t *h, int len, int head) {
  String s = "[";
  for (int i = 0; i < len; i++) { if (i) s+=","; s+=String(h[(head+i)%len]); }
  return s + "]";
}

static String pfd_json_all() {
  char mac[18];
  String s = "{\"count\":" + String(g_pfd.count);
  s += ",\"total_probes\":" + String(g_pfd.total_probes);
  s += ",\"total_flood\":" + String(g_pfd.total_flood_events);
  s += ",\"total_sweep\":" + String(g_pfd.total_sweep_events);

  s += ",\"events\":[";
  bool first = true;
  for (int i = 0; i < g_pfd.evt_count; i++) {
    int idx = (g_pfd.evt_head + PFD_MAX_EVENTS - i - 1) % PFD_MAX_EVENTS;
    PfdEvent &e = g_pfd.events[idx];
    if (!e.ts) continue;
    if (!first) s += ","; first = false;
    pfd_mac_str(e.mac, mac);
    s += "{\"mac\":\"" + String(mac) + "\"";
    s += ",\"type\":\"" + String(pfd_type_str(e.type)) + "\"";
    s += ",\"ssid\":\"" + String(e.ssid) + "\"";
    s += ",\"ch\":" + String(e.ch);
    s += ",\"rate\":" + String(e.rate);
    s += ",\"ts\":" + String(e.ts) + "}";
  }

  s += "],\"scanners\":[";
  first = true;
  for (int i = 0; i < PFD_MAX_SCANNERS; i++) {
    PfdScanner &sc = g_pfd.scanners[i];
    if (!sc.active) continue;
    if (!first) s += ","; first = false;
    pfd_mac_str(sc.mac, mac);
    s += "{\"mac\":\"" + String(mac) + "\"";
    s += ",\"rand\":" + String(sc.is_random_mac ? "true" : "false");
    s += ",\"type\":\"" + String(pfd_type_str(sc.type)) + "\"";
    s += ",\"total\":" + String(sc.probe_count);
    s += ",\"wildcard\":" + String(sc.wildcard_count);
    s += ",\"directed\":" + String(sc.directed_count);
    s += ",\"rate\":" + String(sc.probes_per_sec);
    s += ",\"peak\":" + String(sc.peak_rate);
    s += ",\"flood_ev\":" + String(sc.flood_events);
    s += ",\"sweep_ev\":" + String(sc.sweep_events);
    s += ",\"min_ch_gap\":" + String(sc.min_ch_gap_ms);
    s += ",\"rssi_min\":" + String(sc.rssi_min);
    s += ",\"rssi_max\":" + String(sc.rssi_max);
    s += ",\"targets\":" + String(sc.target_count);
    // Top target SSID
    if (sc.target_count > 0) s += ",\"top_target\":\"" + String(sc.targets[0].ssid) + "\"";
    s += ",\"rate_hist\":" + pfd_hist_json(sc.rate_hist, PFD_HIST_LEN, sc.hist_head) + "}";
  }
  s += "]}";
  return s;
}

inline void pfd_routes() {
  server.on("/api/pfd/all", HTTP_GET, []() {
    server.send(200, "application/json", pfd_json_all());
  });
  server.on("/api/pfd/clear", HTTP_POST, []() {
    memset(&g_pfd, 0, sizeof(g_pfd));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

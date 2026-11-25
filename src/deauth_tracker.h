#pragma once
// deauth_tracker.h — Deauthentication / disassociation storm tracker.
// Detects rogue deauth floods and identifies attacking MACs. Prefix: dat_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define DAT_MAX_ATTACKERS 32
#define DAT_MAX_TARGETS 32
#define DAT_MAX_EVENTS 128
#define DAT_STORM_THRESH 5  // deauths/s = storm
#define DAT_WINDOW_S 3

// ─── Reason codes ─────────────────────────────────────────────────────────────
static const char *DAT_REASON_NAMES[] = {"?",
                                         "Unspecified",
                                         "Prev Auth Invalid",
                                         "STA Leaving",
                                         "Inactivity",
                                         "AP Full",
                                         "Cls2 Non-Auth",
                                         "Cls3 Non-Assoc",
                                         "STA Left",
                                         "Not Authed",
                                         "PMK Expired",
                                         "Mesh Peering Canceled",
                                         "?",
                                         "?",
                                         "?",
                                         "?",
                                         "Invalid IE",
                                         "MIC Fail",
                                         "4WHS Timeout",
                                         "GTK Handshake Timeout",
                                         "?",
                                         "?",
                                         "?",
                                         "IEEE 802.1X Auth Fail",
                                         "?",
                                         "?",
                                         "?",
                                         "?",
                                         "?",
                                         "?",
                                         "?",
                                         "?"};
#define DAT_REASON_MAX 32

// ─── Deauth event ─────────────────────────────────────────────────────────────
struct DatEvent {
  bool valid;
  uint8_t src[6];  // sender (AP or STA)
  uint8_t dst[6];  // target (STA or broadcast)
  uint8_t bssid[6];
  uint8_t reason;
  bool is_disassoc;  // true = disassoc subtype
  int8_t rssi;
  uint8_t channel;
  uint32_t ts;
};

// ─── Per-attacker tracking ────────────────────────────────────────────────────
struct DatAttacker {
  bool active;
  uint8_t mac[6];
  uint8_t bssid[6];  // target AP BSSID (if identifiable)
  uint32_t count_total;
  uint32_t count_window;  // count in last DAT_WINDOW_S seconds
  uint32_t window_start;
  uint8_t peak_rate;  // highest deauths/s seen
  bool is_storm;
  bool spoofed_ap;                      // src looks like an AP (OUI check)
  uint8_t reason_hist[DAT_REASON_MAX];  // reason code distribution
  uint32_t first_ts;
  uint32_t last_ts;
};

// ─── Per-target tracking ─────────────────────────────────────────────────────
struct DatTarget {
  bool active;
  uint8_t mac[6];
  uint32_t deauth_count;
  uint32_t disassoc_count;
  uint8_t last_reason;
  uint32_t last_ts;
};

// ─── State ────────────────────────────────────────────────────────────────────
static struct {
  DatEvent events[DAT_MAX_EVENTS];
  uint8_t ev_head;
  uint32_t total_deauths;
  uint32_t total_disassocs;
  uint32_t storm_count;

  DatAttacker attackers[DAT_MAX_ATTACKERS];
  uint8_t attacker_count;

  DatTarget targets[DAT_MAX_TARGETS];
  uint8_t target_count;
} g_dat = {};

// ─── Helpers ─────────────────────────────────────────────────────────────────
static bool dat_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}
static bool dat_is_bcast(const uint8_t *m) {
  return m[0] == 0xFF && m[1] == 0xFF;
}

static DatAttacker *dat_find_attacker(const uint8_t *mac) {
  for (int i = 0; i < DAT_MAX_ATTACKERS; i++)
    if (g_dat.attackers[i].active && dat_mac_eq(g_dat.attackers[i].mac, mac))
      return &g_dat.attackers[i];
  return nullptr;
}

static DatAttacker *dat_alloc_attacker(const uint8_t *mac) {
  for (int i = 0; i < DAT_MAX_ATTACKERS; i++) {
    if (!g_dat.attackers[i].active) {
      DatAttacker &a = g_dat.attackers[i];
      memset(&a, 0, sizeof(a));
      a.active = true;
      memcpy(a.mac, mac, 6);
      a.first_ts = millis() / 1000;
      g_dat.attacker_count++;
      return &a;
    }
  }
  return nullptr;
}

static DatTarget *dat_find_or_alloc_target(const uint8_t *mac) {
  for (int i = 0; i < DAT_MAX_TARGETS; i++)
    if (g_dat.targets[i].active && dat_mac_eq(g_dat.targets[i].mac, mac))
      return &g_dat.targets[i];
  for (int i = 0; i < DAT_MAX_TARGETS; i++) {
    if (!g_dat.targets[i].active) {
      DatTarget &t = g_dat.targets[i];
      memset(&t, 0, sizeof(t));
      t.active = true;
      memcpy(t.mac, mac, 6);
      g_dat.target_count++;
      return &t;
    }
  }
  return nullptr;
}

// ─── Ingest ───────────────────────────────────────────────────────────────────
void dat_ingest(const uint8_t *src, const uint8_t *dst, const uint8_t *bssid, uint8_t reason,
                bool is_disassoc, int8_t rssi, uint8_t channel) {
  if (!src || !dst)
    return;

  uint32_t now = millis() / 1000;

  // Ring buffer event
  DatEvent &ev = g_dat.events[g_dat.ev_head % DAT_MAX_EVENTS];
  ev.valid = true;
  memcpy(ev.src, src, 6);
  memcpy(ev.dst, dst, 6);
  if (bssid)
    memcpy(ev.bssid, bssid, 6);
  ev.reason = reason;
  ev.is_disassoc = is_disassoc;
  ev.rssi = rssi;
  ev.channel = channel;
  ev.ts = now;
  g_dat.ev_head = (g_dat.ev_head + 1) % DAT_MAX_EVENTS;

  if (is_disassoc)
    g_dat.total_disassocs++;
  else
    g_dat.total_deauths++;

  // Attacker tracking
  DatAttacker *a = dat_find_attacker(src);
  if (!a)
    a = dat_alloc_attacker(src);
  if (a) {
    a->count_total++;
    a->last_ts = now;
    if (bssid)
      memcpy(a->bssid, bssid, 6);
    if (reason < DAT_REASON_MAX)
      a->reason_hist[reason]++;

    // Window rate
    if (now - a->window_start >= DAT_WINDOW_S) {
      uint8_t rate = (uint8_t)(a->count_window / DAT_WINDOW_S);
      if (rate > a->peak_rate)
        a->peak_rate = rate;
      if (rate >= DAT_STORM_THRESH && !a->is_storm) {
        a->is_storm = true;
        g_dat.storm_count++;
      } else if (rate < DAT_STORM_THRESH) {
        a->is_storm = false;
      }
      a->count_window = 0;
      a->window_start = now;
    }
    a->count_window++;
  }

  // Target tracking
  if (!dat_is_bcast(dst)) {
    DatTarget *t = dat_find_or_alloc_target(dst);
    if (t) {
      if (is_disassoc)
        t->disassoc_count++;
      else
        t->deauth_count++;
      t->last_reason = reason;
      t->last_ts = now;
    }
  }
}

// ─── Tick ─────────────────────────────────────────────────────────────────────
void dat_tick() {
  uint32_t now = millis() / 1000;
  for (int i = 0; i < DAT_MAX_ATTACKERS; i++) {
    if (!g_dat.attackers[i].active)
      continue;
    // Reset storm flag if silent >10s
    if (now - g_dat.attackers[i].last_ts > 10)
      g_dat.attackers[i].is_storm = false;
    // Expire if silent >300s
    if (now - g_dat.attackers[i].last_ts > 300) {
      memset(&g_dat.attackers[i], 0, sizeof(g_dat.attackers[i]));
      if (g_dat.attacker_count)
        g_dat.attacker_count--;
    }
  }
}

// ─── JSON ─────────────────────────────────────────────────────────────────────
static void dat_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String dat_json_status() {
  String s = "{\"total_deauths\":" + String(g_dat.total_deauths);
  s += ",\"total_disassocs\":" + String(g_dat.total_disassocs);
  s += ",\"storm_count\":" + String(g_dat.storm_count);
  s += ",\"attacker_count\":" + String(g_dat.attacker_count);
  s += ",\"target_count\":" + String(g_dat.target_count);

  // Active storms
  s += ",\"active_storms\":[";
  char mac[18];
  bool first = true;
  for (int i = 0; i < DAT_MAX_ATTACKERS; i++) {
    DatAttacker &a = g_dat.attackers[i];
    if (!a.active || !a.is_storm)
      continue;
    if (!first)
      s += ",";
    first = false;
    dat_mac_str(a.mac, mac);
    s += "{\"mac\":\"" + String(mac) + "\"";
    s += ",\"peak_rate\":" + String(a.peak_rate);
    s += ",\"total\":" + String(a.count_total) + "}";
  }
  s += "],\"attackers\":[";
  first = true;
  for (int i = 0; i < DAT_MAX_ATTACKERS; i++) {
    DatAttacker &a = g_dat.attackers[i];
    if (!a.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    dat_mac_str(a.mac, mac);
    s += "{\"mac\":\"" + String(mac) + "\"";
    s += ",\"total\":" + String(a.count_total);
    s += ",\"peak\":" + String(a.peak_rate);
    s += ",\"storm\":" + String(a.is_storm ? "true" : "false");
    s += ",\"ts\":" + String(a.last_ts) + "}";
  }
  s += "]}";
  return s;
}

static String dat_json_events(int limit) {
  char mac_s[18], mac_d[18];
  String s = "[";
  bool first = true;
  int shown = 0;
  int start = (g_dat.ev_head + DAT_MAX_EVENTS - 1) % DAT_MAX_EVENTS;
  for (int i = 0; i < DAT_MAX_EVENTS && shown < limit; i++) {
    int idx = (start - i + DAT_MAX_EVENTS) % DAT_MAX_EVENTS;
    DatEvent &e = g_dat.events[idx];
    if (!e.valid)
      continue;
    if (!first)
      s += ",";
    first = false;
    dat_mac_str(e.src, mac_s);
    dat_mac_str(e.dst, mac_d);
    s += "{\"ts\":" + String(e.ts);
    s += ",\"src\":\"" + String(mac_s) + "\"";
    s += ",\"dst\":\"" + String(mac_d) + "\"";
    s += ",\"reason\":" + String(e.reason);
    s += ",\"dis\":" + String(e.is_disassoc ? "true" : "false");
    s += ",\"rssi\":" + String(e.rssi);
    s += ",\"ch\":" + String(e.channel) + "}";
    shown++;
  }
  s += "]";
  return s;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void dat_routes() {
  server.on("/api/dat/status", HTTP_GET,
            []() { server.send(200, "application/json", dat_json_status()); });

  server.on("/api/dat/events", HTTP_GET, []() {
    int limit = server.hasArg("limit") ? server.arg("limit").toInt() : 50;
    if (limit <= 0 || limit > DAT_MAX_EVENTS)
      limit = 50;
    server.send(200, "application/json", dat_json_events(limit));
  });

  server.on("/api/dat/clear", HTTP_POST, []() {
    memset(&g_dat, 0, sizeof(g_dat));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

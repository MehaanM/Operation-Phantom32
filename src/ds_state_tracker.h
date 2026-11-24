#pragma once
// ds_state_tracker.h — 802.11 Distribution System (DS) state tracker.
// Tracks per-client DS association state: auth/assoc state machine,
// AID assignment, capability negotiation, and state anomalies.
// Prefix: dst_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define DST_MAX_ENTRIES  48
#define DST_HIST_LEN     30

// 802.11 state machine states
enum DstDsState : uint8_t {
  DST_STATE_1 = 0,  // Unauthenticated, Unassociated
  DST_STATE_2 = 1,  // Authenticated, Unassociated
  DST_STATE_3 = 2,  // Authenticated, Associated
  DST_STATE_ERR = 3,// Invalid state transition
};

// Deauthentication/Disassociation reason codes (key ones)
static const struct { uint16_t code; const char *reason; } DST_REASON_CODES[] = {
  {1,  "Unspecified"},
  {2,  "Previous auth not valid"},
  {3,  "Left BSS or went dormant"},
  {4,  "Inactivity timeout"},
  {5,  "AP full"},
  {6,  "Class 2 from non-auth"},
  {7,  "Class 3 from non-assoc"},
  {8,  "Disassoc: left BSS"},
  {9,  "Not authenticated"},
  {13, "Invalid IE"},
  {14, "MIC failure"},
  {15, "4-way HS timeout"},
  {16, "Group key HS timeout"},
  {17, "IE mismatch"},
  {18, "Invalid group cipher"},
  {19, "Invalid pairwise cipher"},
  {20, "Invalid AKMP"},
  {23, "IEEE 802.1X failure"},
  {34, "TDLS teardown"},
  {45, "Bad source address"},
};

static const char* dst_reason_str(uint16_t code) {
  for (auto &r : DST_REASON_CODES)
    if (r.code == code) return r.reason;
  return "Reserved";
}

struct DstClient {
  bool    active;
  uint8_t mac[6];
  uint8_t ap_bssid[6];
  char    ap_ssid[33];
  uint8_t ch;
  int8_t  rssi;

  DstDsState state;
  DstDsState prev_state;

  // Association details
  uint16_t aid;              // Association ID
  uint16_t cap_info;         // Capability info word
  uint16_t listen_interval;  // Listen interval TUs

  // State transition counters
  uint16_t auth_success;
  uint16_t auth_fail;
  uint16_t assoc_success;
  uint16_t assoc_fail;
  uint16_t deauth_rx;
  uint16_t deauth_tx;        // deauths this client sent (or saw with addr as source)
  uint16_t disassoc_rx;
  uint16_t disassoc_tx;
  uint16_t reassoc_count;

  // Last reason code
  uint16_t last_deauth_reason;
  uint16_t last_disassoc_reason;
  uint32_t last_deauth_ts;

  // State anomaly detection
  bool    class2_violation;  // class-2 frame while unauthenticated
  bool    class3_violation;  // class-3 frame while unassociated
  bool    deauth_storm;      // multiple deauths in short window
  uint32_t deauth_storm_count;

  // Capability flags
  bool    cap_ess;
  bool    cap_ibss;
  bool    cap_cf_poll;
  bool    cap_privacy;
  bool    cap_short_preamble;
  bool    cap_pbcc;
  bool    cap_channel_agility;
  bool    cap_spectrum_mgmt;
  bool    cap_qos;
  bool    cap_short_slot;
  bool    cap_apsd;
  bool    cap_radio_meas;

  // Timing
  uint32_t state_enter_ts;   // when entered current state
  uint32_t time_in_state_s;
  uint32_t first_ts;
  uint32_t last_ts;
};

static struct {
  DstClient entries[DST_MAX_ENTRIES];
  uint8_t   count;
  uint32_t  total_deauths;
  uint32_t  total_assocs;
  uint32_t  anomalies;
} g_dst = {};

static bool dst_mac_eq(const uint8_t *a, const uint8_t *b) { return memcmp(a,b,6)==0; }

static DstClient* dst_find(const uint8_t *mac, const uint8_t *ap) {
  for (int i = 0; i < DST_MAX_ENTRIES; i++) {
    DstClient &c = g_dst.entries[i];
    if (c.active && dst_mac_eq(c.mac, mac) &&
        (ap == nullptr || dst_mac_eq(c.ap_bssid, ap)))
      return &c;
  }
  return nullptr;
}

static DstClient* dst_alloc(const uint8_t *mac, const uint8_t *ap) {
  for (int i = 0; i < DST_MAX_ENTRIES; i++) {
    if (!g_dst.entries[i].active) {
      DstClient &c = g_dst.entries[i];
      memset(&c, 0, sizeof(c));
      c.active = true;
      memcpy(c.mac, mac, 6);
      if (ap) memcpy(c.ap_bssid, ap, 6);
      c.state = DST_STATE_1;
      c.first_ts = millis()/1000;
      c.state_enter_ts = millis()/1000;
      g_dst.count++;
      return &c;
    }
  }
  uint32_t oldest = UINT32_MAX; int idx = 0;
  for (int i = 0; i < DST_MAX_ENTRIES; i++)
    if (g_dst.entries[i].last_ts < oldest) { oldest = g_dst.entries[i].last_ts; idx = i; }
  memset(&g_dst.entries[idx], 0, sizeof(DstClient));
  g_dst.entries[idx].active = true;
  memcpy(g_dst.entries[idx].mac, mac, 6);
  if (ap) memcpy(g_dst.entries[idx].ap_bssid, ap, 6);
  g_dst.entries[idx].state = DST_STATE_1;
  g_dst.entries[idx].first_ts = millis()/1000;
  g_dst.entries[idx].state_enter_ts = millis()/1000;
  return &g_dst.entries[idx];
}

static void dst_transition(DstClient &c, DstDsState next) {
  c.prev_state = c.state;
  c.state = next;
  c.time_in_state_s = millis()/1000 - c.state_enter_ts;
  c.state_enter_ts = millis()/1000;
}

void dst_ingest_auth(const uint8_t *client, const uint8_t *ap, bool success,
                     uint16_t status_code) {
  DstClient *c = dst_find(client, ap);
  if (!c) c = dst_alloc(client, ap);
  if (!c) return;

  if (success) {
    c->auth_success++;
    dst_transition(*c, DST_STATE_2);
  } else {
    c->auth_fail++;
  }
  c->last_ts = millis()/1000;
}

void dst_ingest_assoc(const uint8_t *client, const uint8_t *ap, const char *ssid,
                       bool success, uint16_t status_code, uint16_t aid,
                       uint16_t cap_info, uint16_t listen_interval, bool reassoc) {
  DstClient *c = dst_find(client, ap);
  if (!c) c = dst_alloc(client, ap);
  if (!c) return;

  if (ssid) strncpy(c->ap_ssid, ssid, 32);
  c->cap_info        = cap_info;
  c->listen_interval = listen_interval;
  c->cap_ess         = (cap_info >> 0) & 1;
  c->cap_ibss        = (cap_info >> 1) & 1;
  c->cap_cf_poll     = (cap_info >> 2) & 1;
  c->cap_privacy     = (cap_info >> 4) & 1;
  c->cap_short_preamble = (cap_info >> 5) & 1;
  c->cap_spectrum_mgmt  = (cap_info >> 8) & 1;
  c->cap_qos         = (cap_info >> 9) & 1;
  c->cap_short_slot  = (cap_info >> 10) & 1;
  c->cap_apsd        = (cap_info >> 11) & 1;
  c->cap_radio_meas  = (cap_info >> 12) & 1;

  if (success) {
    c->aid = aid;
    c->assoc_success++;
    if (reassoc) c->reassoc_count++;
    dst_transition(*c, DST_STATE_3);
    g_dst.total_assocs++;
  } else {
    c->assoc_fail++;
    // Check for class 2 violation: assoc attempt while unauthenticated
    if (c->state == DST_STATE_1) {
      c->class2_violation = true;
      g_dst.anomalies++;
    }
  }
  c->last_ts = millis()/1000;
}

void dst_ingest_deauth(const uint8_t *client, const uint8_t *ap,
                        uint16_t reason, bool from_ap) {
  DstClient *c = dst_find(client, ap);
  if (!c) c = dst_alloc(client, ap);
  if (!c) return;

  c->last_deauth_reason = reason;
  c->last_deauth_ts = millis()/1000;
  g_dst.total_deauths++;

  if (from_ap) {
    c->deauth_rx++;
  } else {
    c->deauth_tx++;
    // Flood detection: many deauths from same client in short window
    static uint32_t last_check[DST_MAX_ENTRIES] = {};
    int idx = c - g_dst.entries;
    if (millis()/1000 - last_check[idx] < 5) {
      if (c->deauth_tx > 5) {
        c->deauth_storm = true;
        c->deauth_storm_count++;
        g_dst.anomalies++;
      }
    } else {
      last_check[idx] = millis()/1000;
    }
  }

  dst_transition(*c, DST_STATE_1);
  c->last_ts = millis()/1000;
}

void dst_ingest_disassoc(const uint8_t *client, const uint8_t *ap,
                          uint16_t reason, bool from_ap) {
  DstClient *c = dst_find(client, ap);
  if (!c) return;

  c->last_disassoc_reason = reason;
  if (from_ap) c->disassoc_rx++;
  else         c->disassoc_tx++;
  dst_transition(*c, DST_STATE_2);
  c->last_ts = millis()/1000;
}

void dst_ingest_class3(const uint8_t *client, const uint8_t *ap) {
  DstClient *c = dst_find(client, ap);
  if (!c) return;
  if (c->state < DST_STATE_3) {
    c->class3_violation = true;
    g_dst.anomalies++;
  }
}

static const char* dst_state_str(DstDsState s) {
  switch (s) {
    case DST_STATE_1: return "Unauth";
    case DST_STATE_2: return "Auth";
    case DST_STATE_3: return "Assoc";
    case DST_STATE_ERR: return "Error";
  }
  return "Unknown";
}

static void dst_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String dst_json_all() {
  String s = "{\"count\":" + String(g_dst.count);
  s += ",\"total_deauths\":" + String(g_dst.total_deauths);
  s += ",\"total_assocs\":" + String(g_dst.total_assocs);
  s += ",\"anomalies\":" + String(g_dst.anomalies);
  s += ",\"entries\":[";
  char mac[18], ap[18]; bool first = true;
  for (int i = 0; i < DST_MAX_ENTRIES; i++) {
    DstClient &c = g_dst.entries[i];
    if (!c.active) continue;
    if (!first) s += ","; first = false;
    dst_mac_str(c.mac, mac);
    dst_mac_str(c.ap_bssid, ap);
    s += "{\"mac\":\"" + String(mac) + "\"";
    s += ",\"ap\":\"" + String(ap) + "\"";
    s += ",\"ssid\":\"" + String(c.ap_ssid) + "\"";
    s += ",\"state\":\"" + String(dst_state_str(c.state)) + "\"";
    s += ",\"aid\":" + String(c.aid);
    s += ",\"auth_ok\":" + String(c.auth_success);
    s += ",\"auth_fail\":" + String(c.auth_fail);
    s += ",\"assoc_ok\":" + String(c.assoc_success);
    s += ",\"assoc_fail\":" + String(c.assoc_fail);
    s += ",\"deauth_rx\":" + String(c.deauth_rx);
    s += ",\"deauth_tx\":" + String(c.deauth_tx);
    s += ",\"disassoc_rx\":" + String(c.disassoc_rx);
    s += ",\"reassoc\":" + String(c.reassoc_count);
    s += ",\"deauth_reason\":" + String(c.last_deauth_reason);
    s += ",\"deauth_reason_str\":\"" + String(dst_reason_str(c.last_deauth_reason)) + "\"";
    s += ",\"class2_viol\":" + String(c.class2_violation ? "true" : "false");
    s += ",\"class3_viol\":" + String(c.class3_violation ? "true" : "false");
    s += ",\"deauth_storm\":" + String(c.deauth_storm ? "true" : "false");
    s += ",\"deauth_storm_cnt\":" + String(c.deauth_storm_count);
    s += ",\"cap_qos\":" + String(c.cap_qos ? "true" : "false");
    s += ",\"cap_privacy\":" + String(c.cap_privacy ? "true" : "false");
    s += ",\"cap_apsd\":" + String(c.cap_apsd ? "true" : "false");
    s += ",\"cap_rm\":" + String(c.cap_radio_meas ? "true" : "false") + "}";
  }
  s += "]}";
  return s;
}

inline void dst_routes() {
  server.on("/api/dst/all", HTTP_GET, []() {
    server.send(200, "application/json", dst_json_all());
  });
  server.on("/api/dst/clear", HTTP_POST, []() {
    memset(&g_dst, 0, sizeof(g_dst));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

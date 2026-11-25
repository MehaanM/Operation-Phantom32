#pragma once
// association_tracker.h — 802.11 association/reassociation/disassociation event tracker.
// Tracks client join/leave events and calculates session durations. Prefix: asc_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define ASC_MAX_EVENTS 128
#define ASC_MAX_SESSIONS 64

// ─── Event types ─────────────────────────────────────────────────────────────
enum AscEventType {
  ASC_EV_ASSOC = 0,
  ASC_EV_REASSOC = 1,
  ASC_EV_DISASSOC = 2,
  ASC_EV_DEAUTH = 3,
  ASC_EV_AUTH_OK = 4,
  ASC_EV_AUTH_FAIL = 5,
};

static const char *ASC_EV_NAMES[] = {"assoc",  "reassoc", "disassoc",
                                     "deauth", "auth_ok", "auth_fail"};

// ─── Event record ─────────────────────────────────────────────────────────────
struct AscEvent {
  bool valid;
  AscEventType type;
  uint8_t client[6];
  uint8_t bssid[6];
  uint16_t reason;  // for disassoc/deauth
  uint16_t status;  // for auth/assoc
  int8_t rssi;
  uint8_t channel;
  uint32_t ts;
};

// ─── Client session ───────────────────────────────────────────────────────────
struct AscSession {
  bool valid;
  bool active;
  uint8_t client[6];
  uint8_t bssid[6];
  char ssid[33];
  uint32_t assoc_ts;
  uint32_t disassoc_ts;
  uint32_t duration_s;
  uint16_t disassoc_reason;
  bool was_reassoc;
  int8_t rssi_assoc;
  int8_t rssi_last;
  uint32_t frames_during;  // frames seen while associated
};

// ─── State ────────────────────────────────────────────────────────────────────
static struct {
  AscEvent events[ASC_MAX_EVENTS];
  uint16_t ev_head;
  uint32_t total_events;

  AscSession sessions[ASC_MAX_SESSIONS];
  uint16_t sess_count;

  uint32_t total_assocs;
  uint32_t total_disassocs;
  uint32_t total_deauths;
  uint32_t active_clients;
} g_asc = {};

// ─── Helpers ─────────────────────────────────────────────────────────────────
static bool asc_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static AscSession *asc_find_session(const uint8_t *client, const uint8_t *bssid) {
  for (int i = 0; i < ASC_MAX_SESSIONS; i++) {
    if (g_asc.sessions[i].valid && asc_mac_eq(g_asc.sessions[i].client, client) &&
        asc_mac_eq(g_asc.sessions[i].bssid, bssid))
      return &g_asc.sessions[i];
  }
  return nullptr;
}

static AscSession *asc_alloc_session(const uint8_t *client, const uint8_t *bssid) {
  // Reuse oldest inactive session if full
  int slot = -1;
  uint32_t oldest = UINT32_MAX;
  for (int i = 0; i < ASC_MAX_SESSIONS; i++) {
    if (!g_asc.sessions[i].valid) {
      slot = i;
      break;
    }
    if (!g_asc.sessions[i].active && g_asc.sessions[i].assoc_ts < oldest) {
      oldest = g_asc.sessions[i].assoc_ts;
      slot = i;
    }
  }
  if (slot < 0)
    return nullptr;

  AscSession &s = g_asc.sessions[slot];
  if (s.valid && s.active)
    g_asc.active_clients--;
  memset(&s, 0, sizeof(s));
  memcpy(s.client, client, 6);
  memcpy(s.bssid, bssid, 6);
  s.valid = true;
  s.active = true;
  g_asc.active_clients++;
  g_asc.sess_count++;
  return &s;
}

// ─── Log event ───────────────────────────────────────────────────────────────
static void asc_log_event(AscEventType type, const uint8_t *client, const uint8_t *bssid,
                          uint16_t reason, uint16_t status, int8_t rssi, uint8_t channel) {
  AscEvent &e = g_asc.events[g_asc.ev_head % ASC_MAX_EVENTS];
  e.valid = true;
  e.type = type;
  memcpy(e.client, client, 6);
  memcpy(e.bssid, bssid, 6);
  e.reason = reason;
  e.status = status;
  e.rssi = rssi;
  e.channel = channel;
  e.ts = millis() / 1000;
  g_asc.ev_head = (g_asc.ev_head + 1) % ASC_MAX_EVENTS;
  g_asc.total_events++;
}

// ─── Ingest 802.11 frame ─────────────────────────────────────────────────────
void asc_ingest(uint8_t type, uint8_t subtype, const uint8_t *src, const uint8_t *dst,
                const uint8_t *bssid, int8_t rssi, uint8_t channel, const char *ssid,
                uint16_t reason, uint16_t status) {
  if (!src || !bssid)
    return;

  // Type 0 (management) frames only
  if (type != 0)
    return;

  switch (subtype) {
    case 0:    // Association request (STA → AP)
    case 2: {  // Reassociation request
      AscEventType et = (subtype == 2) ? ASC_EV_REASSOC : ASC_EV_ASSOC;
      asc_log_event(et, src, bssid, 0, status, rssi, channel);

      AscSession *sess = asc_find_session(src, bssid);
      if (!sess)
        sess = asc_alloc_session(src, bssid);
      if (sess) {
        if (!sess->active) {
          sess->active = true;
          g_asc.active_clients++;
        }
        sess->assoc_ts = millis() / 1000;
        sess->rssi_assoc = rssi;
        sess->rssi_last = rssi;
        sess->was_reassoc = (subtype == 2);
        if (ssid && ssid[0])
          strncpy(sess->ssid, ssid, 32);
      }
      g_asc.total_assocs++;
      break;
    }
    case 10: {  // Disassociation
      asc_log_event(ASC_EV_DISASSOC, src, bssid, reason, 0, rssi, channel);

      AscSession *sess = asc_find_session(src, bssid);
      if (!sess)
        sess = asc_find_session(dst, bssid);
      if (sess && sess->active) {
        uint32_t now = millis() / 1000;
        sess->active = false;
        sess->disassoc_ts = now;
        sess->disassoc_reason = reason;
        sess->duration_s = now - sess->assoc_ts;
        g_asc.active_clients--;
      }
      g_asc.total_disassocs++;
      break;
    }
    case 12: {  // Deauthentication
      asc_log_event(ASC_EV_DEAUTH, src, bssid, reason, 0, rssi, channel);
      AscSession *sess = asc_find_session(src, bssid);
      if (!sess)
        sess = asc_find_session(dst, bssid);
      if (sess && sess->active) {
        uint32_t now = millis() / 1000;
        sess->active = false;
        sess->disassoc_ts = now;
        sess->disassoc_reason = reason;
        sess->duration_s = now - sess->assoc_ts;
        g_asc.active_clients--;
      }
      g_asc.total_deauths++;
      break;
    }
    case 11: {  // Authentication
      AscEventType et = (status == 0) ? ASC_EV_AUTH_OK : ASC_EV_AUTH_FAIL;
      asc_log_event(et, src, bssid, 0, status, rssi, channel);
      break;
    }
  }
}

// ─── JSON ─────────────────────────────────────────────────────────────────────
static void asc_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String asc_json_status() {
  String s = "{\"total_events\":" + String(g_asc.total_events);
  s += ",\"total_assocs\":" + String(g_asc.total_assocs);
  s += ",\"total_disassocs\":" + String(g_asc.total_disassocs);
  s += ",\"total_deauths\":" + String(g_asc.total_deauths);
  s += ",\"active_clients\":" + String(g_asc.active_clients);
  s += ",\"session_count\":" + String(g_asc.sess_count);
  s += ",\"recent_events\":[";

  char c1[18], c2[18];
  bool first = true;
  int start = (g_asc.ev_head + ASC_MAX_EVENTS - 1) % ASC_MAX_EVENTS;
  int shown = 0;
  for (int i = 0; i < ASC_MAX_EVENTS && shown < 50; i++) {
    int idx = (start - i + ASC_MAX_EVENTS) % ASC_MAX_EVENTS;
    AscEvent &e = g_asc.events[idx];
    if (!e.valid)
      continue;
    if (!first)
      s += ",";
    first = false;
    asc_mac_str(e.client, c1);
    asc_mac_str(e.bssid, c2);
    s += "{\"ts\":" + String(e.ts);
    s += ",\"type\":\"" + String(ASC_EV_NAMES[e.type]) + "\"";
    s += ",\"client\":\"" + String(c1) + "\"";
    s += ",\"bssid\":\"" + String(c2) + "\"";
    s += ",\"reason\":" + String(e.reason);
    s += ",\"rssi\":" + String(e.rssi);
    s += ",\"ch\":" + String(e.channel) + "}";
    shown++;
  }
  s += "],\"active_sessions\":[";
  first = true;
  for (int i = 0; i < ASC_MAX_SESSIONS; i++) {
    AscSession &sess = g_asc.sessions[i];
    if (!sess.valid || !sess.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    asc_mac_str(sess.client, c1);
    asc_mac_str(sess.bssid, c2);
    uint32_t dur = millis() / 1000 - sess.assoc_ts;
    s += "{\"client\":\"" + String(c1) + "\"";
    s += ",\"bssid\":\"" + String(c2) + "\"";
    s += ",\"ssid\":\"" + String(sess.ssid) + "\"";
    s += ",\"duration_s\":" + String(dur);
    s += ",\"rssi\":" + String(sess.rssi_last) + "}";
  }
  s += "]}";
  return s;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void asc_routes() {
  server.on("/api/asc/status", HTTP_GET,
            []() { server.send(200, "application/json", asc_json_status()); });

  server.on("/api/asc/clear", HTTP_POST, []() {
    memset(&g_asc, 0, sizeof(g_asc));
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/asc/sessions", HTTP_GET, []() {
    String s = "[";
    char c1[18], c2[18];
    bool first = true;
    for (int i = 0; i < ASC_MAX_SESSIONS; i++) {
      AscSession &sess = g_asc.sessions[i];
      if (!sess.valid)
        continue;
      if (!first)
        s += ",";
      first = false;
      asc_mac_str(sess.client, c1);
      asc_mac_str(sess.bssid, c2);
      s += "{\"client\":\"" + String(c1) + "\"";
      s += ",\"bssid\":\"" + String(c2) + "\"";
      s += ",\"ssid\":\"" + String(sess.ssid) + "\"";
      s += ",\"active\":" + String(sess.active ? "true" : "false");
      s += ",\"assoc_ts\":" + String(sess.assoc_ts);
      s += ",\"duration_s\":" + String(sess.duration_s);
      s += ",\"reason\":" + String(sess.disassoc_reason) + "}";
    }
    s += "]";
    server.send(200, "application/json", s);
  });
}

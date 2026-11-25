#pragma once
// wps_state_machine.h — WPS exchange state tracker and PIN brute-force detector.
// Passive — only observes management frames, never transmits. Prefix: wsm_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define WSM_MAX_SESSIONS 32

// ─── WPS state ────────────────────────────────────────────────────────────────
enum WsmState {
  WSM_STATE_IDLE = 0,
  WSM_STATE_M1_SEEN = 1,  // AP sent M1 EAP-Request/Identity
  WSM_STATE_M2_SEEN = 2,  // STA sent M2
  WSM_STATE_M3_SEEN = 3,
  WSM_STATE_M4_SEEN = 4,
  WSM_STATE_M5_SEEN = 5,
  WSM_STATE_M6_SEEN = 6,
  WSM_STATE_M7_SEEN = 7,
  WSM_STATE_M8_SEEN = 8,
  WSM_STATE_DONE = 9,   // WSC_Done
  WSM_STATE_NACK = 10,  // WSC_Nack (failure — PIN mismatch or brute force detected)
};

static const char *WSM_STATE_NAMES[] = {"idle", "M1", "M2", "M3",   "M4",  "M5",
                                        "M6",   "M7", "M8", "done", "nack"};

struct WsmSession {
  bool active;
  uint8_t ap_bssid[6];
  uint8_t sta_mac[6];
  WsmState state;
  uint8_t nack_count;  // consecutive NACKs → brute force indicator
  bool brute_force_suspect;
  uint32_t first_ts;
  uint32_t last_ts;
  uint32_t duration_s;
};

static struct {
  WsmSession sessions[WSM_MAX_SESSIONS];
  uint8_t session_count;

  uint32_t total_sessions;
  uint32_t total_nacks;
  uint32_t brute_force_suspects;
} g_wsm = {};

static bool wsm_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static WsmSession *wsm_find(const uint8_t *ap, const uint8_t *sta) {
  for (int i = 0; i < WSM_MAX_SESSIONS; i++) {
    WsmSession &s = g_wsm.sessions[i];
    if (!s.active)
      continue;
    if (wsm_mac_eq(s.ap_bssid, ap) && wsm_mac_eq(s.sta_mac, sta))
      return &s;
  }
  return nullptr;
}

static WsmSession *wsm_alloc(const uint8_t *ap, const uint8_t *sta) {
  for (int i = 0; i < WSM_MAX_SESSIONS; i++)
    if (!g_wsm.sessions[i].active) {
      WsmSession &s = g_wsm.sessions[i];
      memset(&s, 0, sizeof(s));
      s.active = true;
      memcpy(s.ap_bssid, ap, 6);
      memcpy(s.sta_mac, sta, 6);
      s.state = WSM_STATE_IDLE;
      s.first_ts = millis() / 1000;
      g_wsm.session_count++;
      g_wsm.total_sessions++;
      return &s;
    }
  // Reuse oldest completed
  uint32_t oldest = UINT32_MAX;
  int idx = 0;
  for (int i = 0; i < WSM_MAX_SESSIONS; i++)
    if (g_wsm.sessions[i].state >= WSM_STATE_DONE && g_wsm.sessions[i].last_ts < oldest) {
      oldest = g_wsm.sessions[i].last_ts;
      idx = i;
    }
  WsmSession &s = g_wsm.sessions[idx];
  memset(&s, 0, sizeof(s));
  s.active = true;
  memcpy(s.ap_bssid, ap, 6);
  memcpy(s.sta_mac, sta, 6);
  s.first_ts = millis() / 1000;
  g_wsm.total_sessions++;
  return &s;
}

void wsm_ingest(const uint8_t *ap, const uint8_t *sta, uint8_t wps_msg_type) {
  if (!ap || !sta)
    return;

  WsmSession *s = wsm_find(ap, sta);
  if (!s)
    s = wsm_alloc(ap, sta);
  if (!s)
    return;

  s->last_ts = millis() / 1000;
  s->duration_s = s->last_ts - s->first_ts;

  switch (wps_msg_type) {
    case 0x04:
      s->state = WSM_STATE_M1_SEEN;
      break;
    case 0x05:
      s->state = WSM_STATE_M2_SEEN;
      break;
    case 0x07:
      s->state = WSM_STATE_M3_SEEN;
      break;
    case 0x08:
      s->state = WSM_STATE_M4_SEEN;
      break;
    case 0x09:
      s->state = WSM_STATE_M5_SEEN;
      break;
    case 0x0A:
      s->state = WSM_STATE_M6_SEEN;
      break;
    case 0x0B:
      s->state = WSM_STATE_M7_SEEN;
      break;
    case 0x0C:
      s->state = WSM_STATE_M8_SEEN;
      break;
    case 0x0F:  // WSC_Done
      s->state = WSM_STATE_DONE;
      break;
    case 0x0E:  // WSC_Nack
      s->state = WSM_STATE_NACK;
      s->nack_count++;
      g_wsm.total_nacks++;
      if (s->nack_count >= 3 && !s->brute_force_suspect) {
        s->brute_force_suspect = true;
        g_wsm.brute_force_suspects++;
      }
      break;
  }
}

static void wsm_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String wsm_json_all() {
  char mac_ap[18], mac_sta[18];
  String s = "{\"session_count\":" + String(g_wsm.session_count);
  s += ",\"total_sessions\":" + String(g_wsm.total_sessions);
  s += ",\"total_nacks\":" + String(g_wsm.total_nacks);
  s += ",\"brute_force_suspects\":" + String(g_wsm.brute_force_suspects);
  s += ",\"sessions\":[";
  bool first = true;
  for (int i = 0; i < WSM_MAX_SESSIONS; i++) {
    WsmSession &ss = g_wsm.sessions[i];
    if (!ss.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    wsm_mac_str(ss.ap_bssid, mac_ap);
    wsm_mac_str(ss.sta_mac, mac_sta);
    s += "{\"ap\":\"" + String(mac_ap) + "\"";
    s += ",\"sta\":\"" + String(mac_sta) + "\"";
    s += ",\"state\":\"" + String(WSM_STATE_NAMES[ss.state]) + "\"";
    s += ",\"nacks\":" + String(ss.nack_count);
    s += ",\"bf\":" + String(ss.brute_force_suspect ? "true" : "false");
    s += ",\"dur\":" + String(ss.duration_s) + "}";
  }
  s += "]}";
  return s;
}

inline void wsm_routes() {
  server.on("/api/wsm/all", HTTP_GET,
            []() { server.send(200, "application/json", wsm_json_all()); });

  server.on("/api/wsm/clear", HTTP_POST, []() {
    memset(&g_wsm, 0, sizeof(g_wsm));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

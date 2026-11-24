#pragma once
// eapol_monitor.h — 802.1X / WPA2 EAPOL frame monitor.
// Tracks 4-way handshake progress, PMKID extraction, MIC failures,
// and WPA2/WPA3 authentication exchange anomalies.
// Prefix: eap_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define EAP_MAX_SESSIONS  32
#define EAP_MAX_EVENTS    64

// EAPOL message types (4-way handshake)
enum EapMsg : uint8_t {
  EAP_MSG_UNKNOWN = 0,
  EAP_MSG_1_OF_4  = 1,  // AP → STA: ANonce
  EAP_MSG_2_OF_4  = 2,  // STA → AP: SNonce + MIC
  EAP_MSG_3_OF_4  = 3,  // AP → STA: GTK + MIC
  EAP_MSG_4_OF_4  = 4,  // STA → AP: ACK
  EAP_MSG_GTK_1   = 5,  // Group key handshake 1
  EAP_MSG_GTK_2   = 6,  // Group key handshake 2
  EAP_MSG_EAP_REQ = 7,  // 802.1X EAP Request
  EAP_MSG_EAP_RSP = 8,  // 802.1X EAP Response
};

// Session anomalies
enum EapAnom : uint8_t {
  EAP_ANOM_NONE         = 0,
  EAP_ANOM_MIC_FAIL     = 1,   // MIC verification failure
  EAP_ANOM_REPLAY       = 2,   // Replay counter went backward
  EAP_ANOM_TIMEOUT      = 4,   // Handshake timed out (> 5 seconds no progress)
  EAP_ANOM_DEAUTH_MID   = 8,   // Deauth mid-handshake
  EAP_ANOM_PMKID        = 16,  // PMKID seen in M1 (PMKID attack risk)
  EAP_ANOM_RAPID_RETRY  = 32,  // > 5 HS attempts in 10 seconds
};

struct EapSession {
  bool    active;
  uint8_t client_mac[6];
  uint8_t ap_bssid[6];
  char    ap_ssid[33];
  uint8_t ch;

  // Handshake state
  uint8_t last_msg;        // last message number seen (1-4)
  bool    handshake_done;
  bool    group_rekey_seen;
  uint32_t hs_start_ts;
  uint32_t hs_end_ts;
  uint32_t hs_duration_ms;

  // Counters
  uint16_t total_eapol;
  uint16_t total_hs;       // completed handshakes
  uint16_t total_failed;   // incomplete / failed
  uint16_t retry_count;    // retransmission counter
  uint32_t last_replay_ctr;
  bool     replay_ctr_ok;

  // PMKID (16 bytes, from M1 or PMKID frame)
  bool    has_pmkid;
  uint8_t pmkid[16];

  // Anomaly flags
  uint8_t anomalies;       // bitmask of EapAnom

  // Rate tracking
  uint16_t hs_per_min;
  uint32_t hs_last_min_ts;
  uint16_t hs_in_window;

  uint32_t first_ts;
  uint32_t last_ts;
};

struct EapEvent {
  uint8_t  client_mac[6];
  uint8_t  ap_bssid[6];
  EapMsg   msg_type;
  uint8_t  anomalies;
  uint32_t ts;
};

static struct {
  EapSession sessions[EAP_MAX_SESSIONS];
  uint8_t    count;
  EapEvent   events[EAP_MAX_EVENTS];
  uint8_t    evt_head;
  uint8_t    evt_count;

  uint32_t   total_eapol;
  uint32_t   total_complete_hs;
  uint32_t   total_failed_hs;
  uint32_t   total_pmkid;
  uint32_t   total_mic_fail;
} g_eap = {};

static bool eap_mac_eq(const uint8_t *a, const uint8_t *b) { return memcmp(a,b,6)==0; }

static EapSession* eap_find(const uint8_t *client, const uint8_t *ap) {
  for (int i = 0; i < EAP_MAX_SESSIONS; i++) {
    EapSession &s = g_eap.sessions[i];
    if (s.active && eap_mac_eq(s.client_mac, client) &&
        (ap == nullptr || eap_mac_eq(s.ap_bssid, ap)))
      return &s;
  }
  return nullptr;
}

static EapSession* eap_alloc(const uint8_t *client, const uint8_t *ap) {
  for (int i = 0; i < EAP_MAX_SESSIONS; i++) {
    if (!g_eap.sessions[i].active) {
      EapSession &s = g_eap.sessions[i];
      memset(&s, 0, sizeof(s));
      s.active = true;
      memcpy(s.client_mac, client, 6);
      if (ap) memcpy(s.ap_bssid, ap, 6);
      s.first_ts = millis()/1000;
      s.replay_ctr_ok = true;
      g_eap.count++;
      return &s;
    }
  }
  uint32_t oldest = UINT32_MAX; int idx = 0;
  for (int i = 0; i < EAP_MAX_SESSIONS; i++)
    if (g_eap.sessions[i].last_ts < oldest) { oldest = g_eap.sessions[i].last_ts; idx = i; }
  memset(&g_eap.sessions[idx], 0, sizeof(EapSession));
  g_eap.sessions[idx].active = true;
  memcpy(g_eap.sessions[idx].client_mac, client, 6);
  if (ap) memcpy(g_eap.sessions[idx].ap_bssid, ap, 6);
  g_eap.sessions[idx].first_ts = millis()/1000;
  g_eap.sessions[idx].replay_ctr_ok = true;
  return &g_eap.sessions[idx];
}

static void eap_push_event(const uint8_t *client, const uint8_t *ap,
                            EapMsg msg, uint8_t anoms) {
  EapEvent &e = g_eap.events[g_eap.evt_head % EAP_MAX_EVENTS];
  memset(&e, 0, sizeof(e));
  memcpy(e.client_mac, client, 6);
  if (ap) memcpy(e.ap_bssid, ap, 6);
  e.msg_type  = msg;
  e.anomalies = anoms;
  e.ts = millis()/1000;
  g_eap.evt_head = (g_eap.evt_head + 1) % EAP_MAX_EVENTS;
  if (g_eap.evt_count < EAP_MAX_EVENTS) g_eap.evt_count++;
}

void eap_ingest(const uint8_t *client, const uint8_t *ap, const char *ssid,
                uint8_t ch, EapMsg msg, bool mic_fail, bool has_pmkid,
                const uint8_t *pmkid_data, uint32_t replay_ctr) {
  EapSession *s = eap_find(client, ap);
  if (!s) s = eap_alloc(client, ap);
  if (!s) return;

  if (ssid) strncpy(s->ap_ssid, ssid, 32);
  s->ch = ch;
  s->total_eapol++;
  g_eap.total_eapol++;
  uint8_t new_anoms = 0;

  // Replay counter check
  if (replay_ctr > 0 && replay_ctr < s->last_replay_ctr) {
    s->anomalies |= EAP_ANOM_REPLAY;
    s->replay_ctr_ok = false;
    new_anoms |= EAP_ANOM_REPLAY;
  }
  if (replay_ctr > 0) s->last_replay_ctr = replay_ctr;

  if (mic_fail) {
    s->anomalies |= EAP_ANOM_MIC_FAIL;
    new_anoms |= EAP_ANOM_MIC_FAIL;
    g_eap.total_mic_fail++;
  }

  if (has_pmkid && pmkid_data) {
    if (!s->has_pmkid) {
      s->has_pmkid = true;
      memcpy(s->pmkid, pmkid_data, 16);
      s->anomalies |= EAP_ANOM_PMKID;
      g_eap.total_pmkid++;
    }
    new_anoms |= EAP_ANOM_PMKID;
  }

  // State machine
  uint32_t now_ms = millis();
  switch (msg) {
    case EAP_MSG_1_OF_4:
      s->last_msg = 1;
      s->handshake_done = false;
      s->hs_start_ts = now_ms;
      break;
    case EAP_MSG_2_OF_4:
      if (s->last_msg == 1) s->last_msg = 2;
      else { s->retry_count++; }
      break;
    case EAP_MSG_3_OF_4:
      if (s->last_msg == 2) s->last_msg = 3;
      break;
    case EAP_MSG_4_OF_4:
      if (s->last_msg == 3) {
        s->last_msg = 4;
        s->handshake_done = true;
        s->hs_end_ts = now_ms;
        s->hs_duration_ms = now_ms - s->hs_start_ts;
        s->total_hs++;
        g_eap.total_complete_hs++;
        // Rapid retry detection
        uint32_t now_s = now_ms/1000;
        if (now_s - s->hs_last_min_ts < 10) {
          s->hs_in_window++;
          if (s->hs_in_window > 5) { s->anomalies |= EAP_ANOM_RAPID_RETRY; new_anoms |= EAP_ANOM_RAPID_RETRY; }
        } else {
          s->hs_last_min_ts = now_s;
          s->hs_in_window = 1;
        }
      }
      break;
    case EAP_MSG_GTK_1:
    case EAP_MSG_GTK_2:
      s->group_rekey_seen = true;
      break;
    default: break;
  }

  // Handshake timeout: message stuck for > 5000ms
  if (!s->handshake_done && s->hs_start_ts > 0 &&
      (now_ms - s->hs_start_ts) > 5000 && s->last_msg < 4) {
    if (!(s->anomalies & EAP_ANOM_TIMEOUT)) {
      s->anomalies |= EAP_ANOM_TIMEOUT;
      s->total_failed++;
      g_eap.total_failed_hs++;
      new_anoms |= EAP_ANOM_TIMEOUT;
    }
  }

  eap_push_event(client, ap, msg, new_anoms);
  s->last_ts = millis()/1000;
}

void eap_ingest_deauth_mid_hs(const uint8_t *client, const uint8_t *ap) {
  EapSession *s = eap_find(client, ap);
  if (!s) return;
  if (!s->handshake_done && s->hs_start_ts > 0) {
    s->anomalies |= EAP_ANOM_DEAUTH_MID;
    s->total_failed++;
    g_eap.total_failed_hs++;
    eap_push_event(client, ap, EAP_MSG_UNKNOWN, EAP_ANOM_DEAUTH_MID);
  }
}

static const char* eap_msg_str(EapMsg m) {
  switch (m) {
    case EAP_MSG_1_OF_4:  return "M1";
    case EAP_MSG_2_OF_4:  return "M2";
    case EAP_MSG_3_OF_4:  return "M3";
    case EAP_MSG_4_OF_4:  return "M4";
    case EAP_MSG_GTK_1:   return "GTK1";
    case EAP_MSG_GTK_2:   return "GTK2";
    case EAP_MSG_EAP_REQ: return "EAP_REQ";
    case EAP_MSG_EAP_RSP: return "EAP_RSP";
    default:              return "Unknown";
  }
}

static void eap_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String eap_pmkid_hex(const uint8_t *p) {
  char buf[33]; buf[0] = 0;
  for (int i = 0; i < 16; i++) snprintf(buf+i*2, 3, "%02X", p[i]);
  return String(buf);
}

static String eap_json_all() {
  String s = "{\"count\":" + String(g_eap.count);
  s += ",\"total_eapol\":" + String(g_eap.total_eapol);
  s += ",\"total_hs\":" + String(g_eap.total_complete_hs);
  s += ",\"total_failed\":" + String(g_eap.total_failed_hs);
  s += ",\"total_pmkid\":" + String(g_eap.total_pmkid);
  s += ",\"total_mic_fail\":" + String(g_eap.total_mic_fail);

  s += ",\"events\":[";
  char mac[18], ap[18]; bool first = true;
  for (int i = 0; i < g_eap.evt_count; i++) {
    int idx = (g_eap.evt_head + EAP_MAX_EVENTS - i - 1) % EAP_MAX_EVENTS;
    EapEvent &e = g_eap.events[idx];
    if (!e.ts) continue;
    if (!first) s += ","; first = false;
    eap_mac_str(e.client_mac, mac);
    eap_mac_str(e.ap_bssid, ap);
    s += "{\"mac\":\"" + String(mac) + "\",\"ap\":\"" + String(ap) + "\"";
    s += ",\"msg\":\"" + String(eap_msg_str(e.msg_type)) + "\"";
    s += ",\"anoms\":" + String(e.anomalies);
    s += ",\"ts\":" + String(e.ts) + "}";
  }

  s += "],\"sessions\":[";
  first = true;
  for (int i = 0; i < EAP_MAX_SESSIONS; i++) {
    EapSession &ss = g_eap.sessions[i];
    if (!ss.active) continue;
    if (!first) s += ","; first = false;
    eap_mac_str(ss.client_mac, mac);
    eap_mac_str(ss.ap_bssid, ap);
    s += "{\"mac\":\"" + String(mac) + "\",\"ap\":\"" + String(ap) + "\"";
    s += ",\"ssid\":\"" + String(ss.ap_ssid) + "\"";
    s += ",\"total\":" + String(ss.total_eapol);
    s += ",\"hs_ok\":" + String(ss.total_hs);
    s += ",\"hs_fail\":" + String(ss.total_failed);
    s += ",\"last_msg\":" + String(ss.last_msg);
    s += ",\"done\":" + String(ss.handshake_done ? "true" : "false");
    s += ",\"hs_ms\":" + String(ss.hs_duration_ms);
    s += ",\"has_pmkid\":" + String(ss.has_pmkid ? "true" : "false");
    if (ss.has_pmkid) s += ",\"pmkid\":\"" + eap_pmkid_hex(ss.pmkid) + "\"";
    s += ",\"anoms\":" + String(ss.anomalies) + "}";
  }
  s += "]}";
  return s;
}

inline void eap_routes() {
  server.on("/api/eap/all", HTTP_GET, []() {
    server.send(200, "application/json", eap_json_all());
  });
  server.on("/api/eap/clear", HTTP_POST, []() {
    memset(&g_eap, 0, sizeof(g_eap));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

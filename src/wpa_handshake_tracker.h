#pragma once
// wpa_handshake_tracker.h — WPA2 4-way handshake tracker.
// Passively tracks EAPOL message exchange state to detect:
//  - Full 4-way handshakes (potential for hashcat capture)
//  - Incomplete handshakes (possible DoS or failed auth)
//  - MIC failure retries (WPA-MIC failure)
//  - Grouped EAPOL key messages (GTK rotation)
// Prefix: wht_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define WHT_MAX_SESSIONS 48
#define WHT_TIMEOUT_S 30  // seconds before incomplete session expires

enum WhtState : uint8_t {
  WHT_IDLE = 0,
  WHT_MSG1 = 1,  // AP → STA: ANonce
  WHT_MSG2 = 2,  // STA → AP: SNonce + MIC
  WHT_MSG3 = 3,  // AP → STA: GTK + MIC
  WHT_MSG4 = 4,  // STA → AP: ACK = complete
  WHT_FAIL = 5,  // MIC failure or timeout
};

static const char *wht_state_name(WhtState s) {
  switch (s) {
    case WHT_IDLE:
      return "Idle";
    case WHT_MSG1:
      return "M1";
    case WHT_MSG2:
      return "M2";
    case WHT_MSG3:
      return "M3";
    case WHT_MSG4:
      return "Complete";
    case WHT_FAIL:
      return "Failed";
    default:
      return "?";
  }
}

struct WhtSession {
  bool active;
  uint8_t ap_bssid[6];
  uint8_t client_mac[6];
  char ssid[33];
  uint8_t ch;
  int8_t rssi;

  WhtState state;
  uint32_t msg_ts[4];  // timestamps of M1..M4
  bool has_mic;        // M2 had a MIC field
  bool complete;       // M4 received = full handshake
  bool mic_failed;     // server sent M1 again after M2 (retransmit → MIC fail)

  uint8_t key_ver;      // from M2: 1=WPA-TKIP,2=WPA2-CCMP,3=WPA2-GCMP
  uint32_t replay_ctr;  // last replay counter

  uint8_t anonce[32];  // from M1
  uint8_t snonce[32];  // from M2

  // PMKID from M1 RSN IE (if present)
  bool has_pmkid;
  uint8_t pmkid[16];

  uint32_t first_ts;
  uint32_t last_ts;
};

static struct {
  WhtSession sessions[WHT_MAX_SESSIONS];
  uint8_t count;
  uint32_t total_complete;
  uint32_t total_failed;
  uint32_t total_seen;
} g_wht = {};

static bool wht_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static WhtSession *wht_find(const uint8_t *ap, const uint8_t *cli) {
  for (int i = 0; i < WHT_MAX_SESSIONS; i++) {
    if (!g_wht.sessions[i].active)
      continue;
    if (wht_mac_eq(g_wht.sessions[i].ap_bssid, ap) && wht_mac_eq(g_wht.sessions[i].client_mac, cli))
      return &g_wht.sessions[i];
  }
  return nullptr;
}

static WhtSession *wht_alloc(const uint8_t *ap, const uint8_t *cli) {
  for (int i = 0; i < WHT_MAX_SESSIONS; i++) {
    if (!g_wht.sessions[i].active) {
      WhtSession &s = g_wht.sessions[i];
      memset(&s, 0, sizeof(s));
      s.active = true;
      memcpy(s.ap_bssid, ap, 6);
      memcpy(s.client_mac, cli, 6);
      s.first_ts = millis() / 1000;
      g_wht.count++;
      g_wht.total_seen++;
      return &s;
    }
  }
  // Evict oldest completed or failed
  uint32_t oldest = UINT32_MAX;
  int idx = 0;
  for (int i = 0; i < WHT_MAX_SESSIONS; i++)
    if ((g_wht.sessions[i].complete || g_wht.sessions[i].mic_failed || !g_wht.sessions[i].active) &&
        g_wht.sessions[i].last_ts < oldest) {
      oldest = g_wht.sessions[i].last_ts;
      idx = i;
    }
  memset(&g_wht.sessions[idx], 0, sizeof(g_wht.sessions[0]));
  g_wht.sessions[idx].active = true;
  memcpy(g_wht.sessions[idx].ap_bssid, ap, 6);
  memcpy(g_wht.sessions[idx].client_mac, cli, 6);
  g_wht.sessions[idx].first_ts = millis() / 1000;
  g_wht.total_seen++;
  return &g_wht.sessions[idx];
}

void wht_ingest_eapol(const uint8_t *ap_bssid, const uint8_t *client_mac, const char *ssid,
                      uint8_t ch, int8_t rssi,
                      uint8_t msg_num,  // 1,2,3,4
                      uint8_t key_ver, bool has_mic,
                      const uint8_t *nonce,  // 32 bytes
                      bool has_pmkid,
                      const uint8_t *pmkid,  // 16 bytes, may be null
                      uint32_t replay_ctr) {
  WhtSession *s = wht_find(ap_bssid, client_mac);
  if (!s)
    s = wht_alloc(ap_bssid, client_mac);
  if (!s)
    return;

  if (ssid && ssid[0])
    strncpy(s->ssid, ssid, 32);
  s->ch = ch;
  if (rssi < 0)
    s->rssi = rssi;
  s->last_ts = millis() / 1000;
  s->key_ver = key_ver;
  s->replay_ctr = replay_ctr;

  uint8_t idx = msg_num - 1;
  if (idx < 4)
    s->msg_ts[idx] = s->last_ts;

  switch (msg_num) {
    case 1:
      // AP → STA: M1 starts (or retransmits) the handshake
      if (s->state == WHT_MSG2) {
        s->mic_failed = true;  // AP re-sent M1 after receiving M2 → MIC failure
        s->state = WHT_FAIL;
        g_wht.total_failed++;
        break;
      }
      s->state = WHT_MSG1;
      if (nonce)
        memcpy(s->anonce, nonce, 32);
      if (has_pmkid && pmkid) {
        s->has_pmkid = true;
        memcpy(s->pmkid, pmkid, 16);
      }
      break;
    case 2:
      if (s->state == WHT_MSG1) {
        s->state = WHT_MSG2;
        s->has_mic = has_mic;
        if (nonce)
          memcpy(s->snonce, nonce, 32);
      }
      break;
    case 3:
      if (s->state == WHT_MSG2)
        s->state = WHT_MSG3;
      break;
    case 4:
      if (s->state == WHT_MSG3) {
        s->state = WHT_MSG4;
        s->complete = true;
        g_wht.total_complete++;
      }
      break;
  }
}

static void wht_expire_old() {
  uint32_t now = millis() / 1000;
  for (int i = 0; i < WHT_MAX_SESSIONS; i++) {
    WhtSession &s = g_wht.sessions[i];
    if (!s.active || s.complete)
      continue;
    if (now - s.last_ts > WHT_TIMEOUT_S) {
      s.state = WHT_FAIL;
      g_wht.total_failed++;
    }
  }
}

static void wht_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String wht_json_all() {
  wht_expire_old();
  String s = "{\"count\":" + String(g_wht.count);
  s += ",\"total_seen\":" + String(g_wht.total_seen);
  s += ",\"total_complete\":" + String(g_wht.total_complete);
  s += ",\"total_failed\":" + String(g_wht.total_failed);
  s += ",\"sessions\":[";
  char mac[18], cmac[18];
  bool first = true;
  for (int i = 0; i < WHT_MAX_SESSIONS; i++) {
    WhtSession &ss = g_wht.sessions[i];
    if (!ss.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    wht_mac_str(ss.ap_bssid, mac);
    wht_mac_str(ss.client_mac, cmac);
    s += "{\"ap\":\"" + String(mac) + "\"";
    s += ",\"client\":\"" + String(cmac) + "\"";
    s += ",\"ssid\":\"" + String(ss.ssid) + "\"";
    s += ",\"ch\":" + String(ss.ch);
    s += ",\"rssi\":" + String(ss.rssi);
    s += ",\"state\":\"" + String(wht_state_name(ss.state)) + "\"";
    s += ",\"complete\":" + String(ss.complete ? "true" : "false");
    s += ",\"mic_failed\":" + String(ss.mic_failed ? "true" : "false");
    s += ",\"has_pmkid\":" + String(ss.has_pmkid ? "true" : "false");
    s += ",\"key_ver\":" + String(ss.key_ver);
    s += ",\"first_ts\":" + String(ss.first_ts);
    s += ",\"last_ts\":" + String(ss.last_ts);
    if (ss.has_pmkid) {
      char hex[33];
      for (int j = 0; j < 16; j++)
        snprintf(hex + j * 2, 3, "%02X", ss.pmkid[j]);
      s += ",\"pmkid\":\"" + String(hex) + "\"";
    }
    s += "}";
  }
  s += "]}";
  return s;
}

inline void wht_routes() {
  server.on("/api/wht/all", HTTP_GET,
            []() { server.send(200, "application/json", wht_json_all()); });
  server.on("/api/wht/clear", HTTP_POST, []() {
    memset(&g_wht, 0, sizeof(g_wht));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

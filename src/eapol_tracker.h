#pragma once
// eapol_tracker.h — EAPOL 4-way handshake tracker for WPA2/WPA3.
// Captures MIC, ANONCE, SNONCE, and reconstructs Hashcat 22000 format.
// Prefix: eap2_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define EAP2_MAX_SESSIONS 16
#define EAP2_SSID_LEN 33
#define EAP2_TIMEOUT_S 10

// ─── EAPOL key type ───────────────────────────────────────────────────────────
struct Eap2Key {
  bool valid;
  uint8_t timestamp;
  uint8_t key_info[2];  // key info field
  uint16_t key_len;
  uint64_t replay_counter;
  uint8_t nonce[32];  // ANonce or SNonce
  uint8_t mic[16];    // MIC field
  uint32_t data_len;
  uint8_t data[128];  // key data (for GTK/PTK)
};

// ─── Handshake session ────────────────────────────────────────────────────────
struct Eap2Session {
  bool active;
  uint8_t ap_mac[6];
  uint8_t client_mac[6];
  char ssid[EAP2_SSID_LEN];
  uint8_t channel;
  int8_t rssi;

  Eap2Key msg1;  // AP → STA: ANonce
  Eap2Key msg2;  // STA → AP: SNonce + MIC
  Eap2Key msg3;  // AP → STA: encrypted GTK
  Eap2Key msg4;  // STA → AP: acknowledgment

  bool has_pmkid;
  uint8_t pmkid[16];
  bool complete;   // msgs 1+2 or 2+3 captured
  bool crackable;  // has MIC+nonces needed for Hashcat

  uint32_t first_ts;
  uint32_t last_ts;

  // Hashcat 22000 export string (pre-built)
  char hc22000[256];
};

// ─── State ────────────────────────────────────────────────────────────────────
static struct {
  Eap2Session sessions[EAP2_MAX_SESSIONS];
  uint8_t count;
  uint32_t total_frames;
  uint32_t complete_sessions;
  uint32_t crackable_sessions;
} g_eap2 = {};

// ─── Helpers ─────────────────────────────────────────────────────────────────
static bool eap2_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static Eap2Session *eap2_find(const uint8_t *ap, const uint8_t *client) {
  for (int i = 0; i < EAP2_MAX_SESSIONS; i++) {
    if (g_eap2.sessions[i].active && eap2_mac_eq(g_eap2.sessions[i].ap_mac, ap) &&
        eap2_mac_eq(g_eap2.sessions[i].client_mac, client))
      return &g_eap2.sessions[i];
  }
  return nullptr;
}

static Eap2Session *eap2_alloc(const uint8_t *ap, const uint8_t *client, uint8_t ch) {
  // Evict timed-out session first
  uint32_t now = millis() / 1000;
  for (int i = 0; i < EAP2_MAX_SESSIONS; i++) {
    Eap2Session &s = g_eap2.sessions[i];
    if (s.active && now - s.last_ts > EAP2_TIMEOUT_S) {
      memset(&s, 0, sizeof(s));
      g_eap2.count--;
      break;
    }
  }
  for (int i = 0; i < EAP2_MAX_SESSIONS; i++) {
    if (!g_eap2.sessions[i].active) {
      Eap2Session &s = g_eap2.sessions[i];
      memset(&s, 0, sizeof(s));
      memcpy(s.ap_mac, ap, 6);
      memcpy(s.client_mac, client, 6);
      s.channel = ch;
      s.active = true;
      s.first_ts = now;
      s.last_ts = now;
      g_eap2.count++;
      return &s;
    }
  }
  return nullptr;
}

// ─── Build Hashcat 22000 format ───────────────────────────────────────────────
static void eap2_build_hc22000(Eap2Session *s) {
  if (!s->msg2.valid || !s->msg1.valid)
    return;

  // WPA*02*MIC*AP_MAC*CLIENT_MAC*ESSID_HEX*ANONCE*EAPOL*KEYVER
  char ap_mac[13], cli_mac[13], ssid_hex[65];
  snprintf(ap_mac, sizeof(ap_mac), "%02x%02x%02x%02x%02x%02x", s->ap_mac[0], s->ap_mac[1],
           s->ap_mac[2], s->ap_mac[3], s->ap_mac[4], s->ap_mac[5]);
  snprintf(cli_mac, sizeof(cli_mac), "%02x%02x%02x%02x%02x%02x", s->client_mac[0], s->client_mac[1],
           s->client_mac[2], s->client_mac[3], s->client_mac[4], s->client_mac[5]);

  int ssid_len = strlen(s->ssid);
  for (int i = 0; i < ssid_len && i < 32; i++)
    snprintf(ssid_hex + i * 2, 3, "%02x", (uint8_t)s->ssid[i]);

  char mic_hex[33];
  for (int i = 0; i < 16; i++)
    snprintf(mic_hex + i * 2, 3, "%02x", s->msg2.mic[i]);

  char anonce_hex[65];
  for (int i = 0; i < 32; i++)
    snprintf(anonce_hex + i * 2, 3, "%02x", s->msg1.nonce[i]);

  snprintf(s->hc22000, sizeof(s->hc22000), "WPA*02*%s*%s*%s*%s*%s**", mic_hex, ap_mac, cli_mac,
           ssid_hex, anonce_hex);

  s->crackable = true;
  g_eap2.crackable_sessions++;
}

// ─── Ingest EAPOL frame ───────────────────────────────────────────────────────
void eap2_ingest(const uint8_t *raw, int len, const uint8_t *src, const uint8_t *dst,
                 const uint8_t *bssid, int8_t rssi, uint8_t channel, const char *ssid) {
  if (len < 99)
    return;  // too short for EAPOL

  // 802.11 MAC header is 24 bytes, then QoS or LLC...
  // We look for EAPOL after LLC/SNAP: AA AA 03 00 00 00 88 8E
  int snap_off = -1;
  for (int i = 24; i < len - 8; i++) {
    if (raw[i] == 0xAA && raw[i + 1] == 0xAA && raw[i + 2] == 0x03 && raw[i + 3] == 0x00 &&
        raw[i + 4] == 0x00 && raw[i + 5] == 0x00 && raw[i + 6] == 0x88 && raw[i + 7] == 0x8E) {
      snap_off = i + 8;
      break;
    }
  }
  if (snap_off < 0 || snap_off + 4 >= len)
    return;

  // EAPOL header: version(1) + type(1) + length(2)
  // EAPOL-Key: type=3
  if (raw[snap_off + 1] != 0x03)
    return;  // not EAPOL-Key

  int key_off = snap_off + 4;  // past EAPOL header
  if (key_off + 95 >= len)
    return;

  // Key info: bytes key_off+1,key_off+2 (after descriptor type at key_off)
  uint8_t desc_type = raw[key_off];
  uint16_t key_info = (raw[key_off + 1] << 8) | raw[key_off + 2];
  bool install = (key_info >> 6) & 1;
  bool ack = (key_info >> 7) & 1;
  bool mic = (key_info >> 8) & 1;
  bool secure = (key_info >> 9) & 1;
  uint8_t key_ver = key_info & 0x07;

  // Nonce at offset key_off + 17
  const uint8_t *nonce = raw + key_off + 17;

  // MIC at offset key_off + 77
  const uint8_t *mic_field = raw + key_off + 77;

  // Determine message number: ANonce(msg1), SNonce+MIC(msg2), msg3, msg4
  uint8_t msg_num = 0;
  if (ack && !mic && !install && !secure)
    msg_num = 1;  // AP → STA
  if (!ack && mic && !install && !secure)
    msg_num = 2;  // STA → AP
  if (ack && mic && install && secure)
    msg_num = 3;  // AP → STA
  if (!ack && mic && !install && secure)
    msg_num = 4;  // STA → AP

  if (msg_num == 0)
    return;

  // Identify AP/client direction
  const uint8_t *ap_mac = (msg_num == 1 || msg_num == 3) ? src : dst;
  const uint8_t *client_mac = (msg_num == 1 || msg_num == 3) ? dst : src;

  if (!ap_mac || !client_mac)
    return;

  Eap2Session *sess = eap2_find(ap_mac, client_mac);
  if (!sess)
    sess = eap2_alloc(ap_mac, client_mac, channel);
  if (!sess)
    return;

  g_eap2.total_frames++;
  sess->last_ts = millis() / 1000;
  sess->rssi = rssi;
  if (ssid && ssid[0])
    strncpy(sess->ssid, ssid, EAP2_SSID_LEN - 1);

  Eap2Key *key_slot = (msg_num == 1)   ? &sess->msg1
                      : (msg_num == 2) ? &sess->msg2
                      : (msg_num == 3) ? &sess->msg3
                                       : &sess->msg4;
  key_slot->valid = true;
  key_slot->key_info[0] = key_info >> 8;
  key_slot->key_info[1] = key_info & 0xFF;
  if (key_off + 17 + 32 <= len)
    memcpy(key_slot->nonce, nonce, 32);
  if (key_off + 77 + 16 <= len)
    memcpy(key_slot->mic, mic_field, 16);

  // Try to build Hashcat string
  if (sess->msg1.valid && sess->msg2.valid && !sess->crackable) {
    eap2_build_hc22000(sess);
  }

  // Check for PMKID in msg1 key data (last 16 bytes of key data can contain PMKID)
  if (msg_num == 1 && key_off + 95 + 16 <= len) {
    // RSN PMKID in key data: OUI 00:0F:AC, PMKID list
    int kd_len_off = key_off + 93;
    if (kd_len_off + 2 <= len) {
      uint16_t kd_len = (raw[kd_len_off] << 8) | raw[kd_len_off + 1];
      int kd_off = kd_len_off + 2;
      if (kd_len >= 22 && kd_off + 22 <= len) {
        // RSN IE element (tag 48 or PMKID list): look for 00 0F AC 04 (PMKID list)
        if (raw[kd_off] == 0x00 && raw[kd_off + 1] == 0x0F && raw[kd_off + 2] == 0xAC) {
          memcpy(sess->pmkid, raw + kd_off + 6, 16);
          sess->has_pmkid = true;
        }
      }
    }
  }

  if (sess->msg1.valid && sess->msg2.valid) {
    sess->complete = true;
    g_eap2.complete_sessions++;
  }
}

// ─── JSON ─────────────────────────────────────────────────────────────────────
static void eap2_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String eap2_json_all() {
  String s = "{\"count\":" + String(g_eap2.count);
  s += ",\"total_frames\":" + String(g_eap2.total_frames);
  s += ",\"complete\":" + String(g_eap2.complete_sessions);
  s += ",\"crackable\":" + String(g_eap2.crackable_sessions);
  s += ",\"sessions\":[";
  char mac1[18], mac2[18];
  bool first = true;
  for (int i = 0; i < EAP2_MAX_SESSIONS; i++) {
    Eap2Session &sess = g_eap2.sessions[i];
    if (!sess.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    eap2_mac_str(sess.ap_mac, mac1);
    eap2_mac_str(sess.client_mac, mac2);
    s += "{\"ap\":\"" + String(mac1) + "\"";
    s += ",\"client\":\"" + String(mac2) + "\"";
    s += ",\"ssid\":\"" + String(sess.ssid) + "\"";
    s += ",\"ch\":" + String(sess.channel);
    s += ",\"rssi\":" + String(sess.rssi);
    s += ",\"has_msg1\":" + String(sess.msg1.valid ? "true" : "false");
    s += ",\"has_msg2\":" + String(sess.msg2.valid ? "true" : "false");
    s += ",\"has_msg3\":" + String(sess.msg3.valid ? "true" : "false");
    s += ",\"has_msg4\":" + String(sess.msg4.valid ? "true" : "false");
    s += ",\"complete\":" + String(sess.complete ? "true" : "false");
    s += ",\"crackable\":" + String(sess.crackable ? "true" : "false");
    s += ",\"has_pmkid\":" + String(sess.has_pmkid ? "true" : "false");
    if (sess.crackable) {
      s += ",\"hc22000\":\"" + String(sess.hc22000) + "\"";
    }
    s += "}";
  }
  s += "]}";
  return s;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void eap2_routes() {
  server.on("/api/eap2/status", HTTP_GET,
            []() { server.send(200, "application/json", eap2_json_all()); });

  server.on("/api/eap2/export", HTTP_GET, []() {
    String out;
    for (int i = 0; i < EAP2_MAX_SESSIONS; i++) {
      Eap2Session &s = g_eap2.sessions[i];
      if (!s.active || !s.crackable)
        continue;
      out += String(s.hc22000) + "\n";
    }
    server.send(200, "text/plain", out.length() ? out : "# No crackable sessions");
  });

  server.on("/api/eap2/clear", HTTP_POST, []() {
    memset(&g_eap2, 0, sizeof(g_eap2));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

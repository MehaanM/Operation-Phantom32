#pragma once
#include <Arduino.h>
#include <string.h>
#include <stdint.h>

// ── EAP / EAPOL Handler ───────────────────────────────────────────────────────
//
// Parses 802.1X EAP (Extensible Authentication Protocol) and EAPOL
// frames. Extracts WPA2 4-way handshake components, tracks PMKID from
// EAPOL Message 1, reconstructs handshake state machines per BSSID-STA pair.

#define EAP_MAX_SESSIONS 32
#define EAP_MAX_LOG 48
#define EAPOL_ETH_TYPE 0x888E
#define EAP_EAPOL_VERSION 1

// ── EAPOL packet types ────────────────────────────────────────────────────────
#define EAPOL_EAP_PACKET 0x00
#define EAPOL_START 0x01
#define EAPOL_LOGOFF 0x02
#define EAPOL_KEY 0x03
#define EAPOL_ENCAP_ASF_ALERT 0x04

// ── EAPOL key descriptor types ────────────────────────────────────────────────
#define EAPOL_KEY_DESC_RC4 0x01
#define EAPOL_KEY_DESC_RSN 0x02
#define EAPOL_KEY_DESC_WPA 0xFE

// ── Key information field bits ────────────────────────────────────────────────
#define EAPOL_KI_KEY_DESC_VERSION_MASK 0x0007
#define EAPOL_KI_KEY_TYPE 0x0008
#define EAPOL_KI_KEY_INDEX 0x0030
#define EAPOL_KI_INSTALL 0x0040
#define EAPOL_KI_KEY_ACK 0x0080
#define EAPOL_KI_KEY_MIC 0x0100
#define EAPOL_KI_SECURE 0x0200
#define EAPOL_KI_ERROR 0x0400
#define EAPOL_KI_REQUEST 0x0800
#define EAPOL_KI_ENCRYPTED_KEY_DATA 0x1000
#define EAPOL_KI_SMK_MESSAGE 0x2000

// ── EAP codes ─────────────────────────────────────────────────────────────────
#define EAP_CODE_REQUEST 1
#define EAP_CODE_RESPONSE 2
#define EAP_CODE_SUCCESS 3
#define EAP_CODE_FAILURE 4

// ── EAP types ─────────────────────────────────────────────────────────────────
#define EAP_TYPE_IDENTITY 1
#define EAP_TYPE_NOTIFICATION 2
#define EAP_TYPE_NAK 3
#define EAP_TYPE_MD5 4
#define EAP_TYPE_OTP 5
#define EAP_TYPE_GENERIC_TOKEN 6
#define EAP_TYPE_TLS 13
#define EAP_TYPE_LEAP 17
#define EAP_TYPE_SIM 18
#define EAP_TYPE_TTLS 21
#define EAP_TYPE_PEAP 25
#define EAP_TYPE_MSCHAPV2 26
#define EAP_TYPE_AKA 23
#define EAP_TYPE_FAST 43
#define EAP_TYPE_PWD 52
#define EAP_TYPE_EXPANDED 254

static const char *eap_type_name(uint8_t t) {
  switch (t) {
    case EAP_TYPE_IDENTITY:
      return "Identity";
    case EAP_TYPE_NOTIFICATION:
      return "Notification";
    case EAP_TYPE_NAK:
      return "NAK";
    case EAP_TYPE_MD5:
      return "MD5-Challenge";
    case EAP_TYPE_OTP:
      return "OTP";
    case EAP_TYPE_TLS:
      return "EAP-TLS";
    case EAP_TYPE_LEAP:
      return "LEAP";
    case EAP_TYPE_SIM:
      return "EAP-SIM";
    case EAP_TYPE_TTLS:
      return "EAP-TTLS";
    case EAP_TYPE_PEAP:
      return "PEAP";
    case EAP_TYPE_MSCHAPV2:
      return "MSCHAPv2";
    case EAP_TYPE_AKA:
      return "EAP-AKA";
    case EAP_TYPE_FAST:
      return "EAP-FAST";
    case EAP_TYPE_PWD:
      return "EAP-PWD";
    case EAP_TYPE_EXPANDED:
      return "Expanded";
    default:
      return "Unknown";
  }
}

// ── EAPOL key frame (packed, little-endian fields handled manually) ───────────
struct __attribute__((packed)) EapKeyHdr {
  uint8_t version;
  uint8_t type;  // EAPOL_KEY = 0x03
  uint16_t length;
  uint8_t key_desc_type;
  uint16_t key_info;
  uint16_t key_len;
  uint8_t replay_counter[8];
  uint8_t nonce[32];
  uint8_t key_iv[16];
  uint8_t key_rsc[8];
  uint8_t key_id[8];
  uint8_t mic[16];
  uint16_t key_data_len;
};

// ── EAPOL message numbers ──────────────────────────────────────────────────────
enum EapMsgNum : uint8_t {
  EAP_MSG_NONE = 0,
  EAP_MSG_1 = 1,  // M1: AP → STA: ANonce, ACK set
  EAP_MSG_2 = 2,  // M2: STA → AP: SNonce, MIC set
  EAP_MSG_3 = 3,  // M3: AP → STA: encrypted GTK, Secure+MIC+Install+Ack
  EAP_MSG_4 = 4,  // M4: STA → AP: Secure+MIC
};

// ── Handshake session ─────────────────────────────────────────────────────────
struct EapSession {
  uint8_t bssid[6];
  uint8_t sta_mac[6];
  uint8_t anonce[32];
  uint8_t snonce[32];
  uint8_t mic[16];
  uint8_t pmkid[16];
  uint8_t eapol_m2[256];
  uint16_t eapol_m2_len;
  EapMsgNum last_msg;
  bool has_anonce;
  bool has_snonce;
  bool has_mic;
  bool has_pmkid;
  bool complete;  // M1+M2+M3+M4 all captured
  uint32_t first_seen_ms;
  uint32_t last_seen_ms;
  uint32_t msg_count;
  char ssid[33];
};

// ── Log entry ─────────────────────────────────────────────────────────────────
struct EapLog {
  uint32_t ts_ms;
  uint8_t bssid[6];
  uint8_t sta_mac[6];
  EapMsgNum msg;
  char note[32];
};

// ── EAP handler state ─────────────────────────────────────────────────────────
struct EapState {
  EapSession sessions[EAP_MAX_SESSIONS];
  uint8_t session_count;
  EapLog log[EAP_MAX_LOG];
  uint8_t log_head;
  uint8_t log_count;
  uint32_t total_eapol;
  uint32_t total_complete;
  uint32_t total_pmkid;
};

static EapState g_eap = {};

// ── Find or create session ────────────────────────────────────────────────────
static EapSession *eap_session(const uint8_t bssid[6], const uint8_t sta[6]) {
  for (uint8_t i = 0; i < g_eap.session_count; i++) {
    if (memcmp(g_eap.sessions[i].bssid, bssid, 6) == 0 &&
        memcmp(g_eap.sessions[i].sta_mac, sta, 6) == 0)
      return &g_eap.sessions[i];
  }
  if (g_eap.session_count >= EAP_MAX_SESSIONS) {
    uint32_t oldest = g_eap.sessions[0].last_seen_ms;
    uint8_t oi = 0;
    for (uint8_t i = 1; i < g_eap.session_count; i++) {
      if (g_eap.sessions[i].last_seen_ms < oldest) {
        oldest = g_eap.sessions[i].last_seen_ms;
        oi = i;
      }
    }
    memset(&g_eap.sessions[oi], 0, sizeof(EapSession));
    memcpy(g_eap.sessions[oi].bssid, bssid, 6);
    memcpy(g_eap.sessions[oi].sta_mac, sta, 6);
    g_eap.sessions[oi].first_seen_ms = millis();
    return &g_eap.sessions[oi];
  }
  EapSession *s = &g_eap.sessions[g_eap.session_count++];
  memset(s, 0, sizeof(*s));
  memcpy(s->bssid, bssid, 6);
  memcpy(s->sta_mac, sta, 6);
  s->first_seen_ms = millis();
  return s;
}

// ── Log event ─────────────────────────────────────────────────────────────────
static void eap_log_event(const uint8_t bssid[6], const uint8_t sta[6], EapMsgNum msg,
                          const char *note) {
  EapLog *l = &g_eap.log[g_eap.log_head];
  l->ts_ms = millis();
  if (bssid)
    memcpy(l->bssid, bssid, 6);
  else
    memset(l->bssid, 0, 6);
  if (sta)
    memcpy(l->sta_mac, sta, 6);
  else
    memset(l->sta_mac, 0, 6);
  l->msg = msg;
  strncpy(l->note, note ? note : "", 31);
  l->note[31] = 0;
  g_eap.log_head = (g_eap.log_head + 1) % EAP_MAX_LOG;
  if (g_eap.log_count < EAP_MAX_LOG)
    g_eap.log_count++;
}

// ── Determine which EAPOL handshake message this is ───────────────────────────
static EapMsgNum eap_classify_msg(const EapKeyHdr *k) {
  uint16_t ki;
  memcpy(&ki, &k->key_info, 2);
  ki = __builtin_bswap16(ki);
  bool ack = (ki & EAPOL_KI_KEY_ACK) != 0;
  bool mic_set = (ki & EAPOL_KI_KEY_MIC) != 0;
  bool secure = (ki & EAPOL_KI_SECURE) != 0;
  bool install = (ki & EAPOL_KI_INSTALL) != 0;
  uint16_t kdlen;
  memcpy(&kdlen, &k->key_data_len, 2);
  kdlen = __builtin_bswap16(kdlen);

  if (ack && !mic_set && !secure)
    return EAP_MSG_1;
  if (!ack && mic_set && !secure && kdlen > 0)
    return EAP_MSG_2;
  if (ack && mic_set && secure && install && kdlen > 0)
    return EAP_MSG_3;
  if (!ack && mic_set && secure && kdlen == 0)
    return EAP_MSG_4;
  return EAP_MSG_NONE;
}

// ── Extract PMKID from M1 key data ───────────────────────────────────────────
static bool eap_extract_pmkid(const uint8_t *key_data, uint16_t kd_len, uint8_t pmkid_out[16]) {
  if (!key_data || kd_len < 2)
    return false;
  uint16_t i = 0;
  while (i + 2 <= kd_len) {
    uint8_t id = key_data[i];
    uint8_t len = key_data[i + 1];
    if (id == 0xDD) {
      // Vendor OUI check: 00:0F:AC = IEEE 802.11i
      if (len >= 22 && i + 2 + len <= kd_len) {
        const uint8_t *v = key_data + i + 2;
        if (v[0] == 0x00 && v[1] == 0x0F && v[2] == 0xAC && v[3] == 4) {
          memcpy(pmkid_out, v + 4, 16);
          return true;
        }
      }
    }
    if (len == 0)
      break;
    i += 2 + len;
  }
  return false;
}

// ── Ingest EAPOL frame ────────────────────────────────────────────────────────
static void eap_ingest(const uint8_t *src_mac, const uint8_t *dst_mac, const uint8_t *payload,
                       uint16_t len) {
  if (!payload || len < 4)
    return;
  if (payload[0] != EAP_EAPOL_VERSION)
    return;
  if (payload[1] != EAPOL_KEY)
    return;  // Only handle key frames here
  if (len < sizeof(EapKeyHdr) + 4)
    return;

  g_eap.total_eapol++;

  const EapKeyHdr *k = (const EapKeyHdr *)(payload);
  EapMsgNum msg = eap_classify_msg(k);
  if (msg == EAP_MSG_NONE)
    return;

  // For M1/M3: src=BSSID, dst=STA
  // For M2/M4: src=STA, dst=BSSID
  const uint8_t *bssid = (msg == EAP_MSG_1 || msg == EAP_MSG_3) ? src_mac : dst_mac;
  const uint8_t *sta = (msg == EAP_MSG_1 || msg == EAP_MSG_3) ? dst_mac : src_mac;

  EapSession *s = eap_session(bssid, sta);
  s->last_seen_ms = millis();
  s->msg_count++;
  s->last_msg = msg;

  uint16_t kdlen;
  memcpy(&kdlen, &k->key_data_len, 2);
  kdlen = __builtin_bswap16(kdlen);

  switch (msg) {
    case EAP_MSG_1:
      memcpy(s->anonce, k->nonce, 32);
      s->has_anonce = true;
      // Try PMKID extraction from key data
      if (kdlen > 0 && len >= sizeof(*k) + kdlen) {
        const uint8_t *kd = (const uint8_t *)(k + 1);
        if (eap_extract_pmkid(kd, kdlen, s->pmkid)) {
          s->has_pmkid = true;
          g_eap.total_pmkid++;
          eap_log_event(bssid, sta, msg, "M1 PMKID captured");
        } else {
          eap_log_event(bssid, sta, msg, "M1 ANonce");
        }
      } else {
        eap_log_event(bssid, sta, msg, "M1 ANonce");
      }
      break;

    case EAP_MSG_2:
      memcpy(s->snonce, k->nonce, 32);
      memcpy(s->mic, k->mic, 16);
      s->has_snonce = true;
      s->has_mic = true;
      // Store raw M2 for offline cracking
      {
        uint16_t copy = len < 256 ? len : 256;
        memcpy(s->eapol_m2, payload, copy);
        s->eapol_m2_len = copy;
      }
      eap_log_event(bssid, sta, msg, "M2 SNonce+MIC");
      break;

    case EAP_MSG_3:
      eap_log_event(bssid, sta, msg, "M3 GTK");
      break;

    case EAP_MSG_4:
      s->complete = s->has_anonce && s->has_snonce && s->has_mic;
      if (s->complete)
        g_eap.total_complete++;
      eap_log_event(bssid, sta, msg, s->complete ? "Complete handshake" : "M4");
      break;

    default:
      break;
  }
}

// ── Sessions JSON ──────────────────────────────────────────────────────────────
static String eap_sessions_json() {
  String j = "{\"total_eapol\":" + String(g_eap.total_eapol);
  j += ",\"total_complete\":" + String(g_eap.total_complete);
  j += ",\"total_pmkid\":" + String(g_eap.total_pmkid);
  j += ",\"sessions\":[";
  for (uint8_t i = 0; i < g_eap.session_count; i++) {
    if (i)
      j += ",";
    EapSession &s = g_eap.sessions[i];
    char bssid[18], sta[18];
    snprintf(bssid, sizeof(bssid), "%02X:%02X:%02X:%02X:%02X:%02X", s.bssid[0], s.bssid[1],
             s.bssid[2], s.bssid[3], s.bssid[4], s.bssid[5]);
    snprintf(sta, sizeof(sta), "%02X:%02X:%02X:%02X:%02X:%02X", s.sta_mac[0], s.sta_mac[1],
             s.sta_mac[2], s.sta_mac[3], s.sta_mac[4], s.sta_mac[5]);
    j += "{\"bssid\":\"";
    j += bssid;
    j += "\"";
    j += ",\"sta\":\"";
    j += sta;
    j += "\"";
    j += ",\"last_msg\":";
    j += s.last_msg;
    j += ",\"has_anonce\":";
    j += s.has_anonce ? 1 : 0;
    j += ",\"has_snonce\":";
    j += s.has_snonce ? 1 : 0;
    j += ",\"has_mic\":";
    j += s.has_mic ? 1 : 0;
    j += ",\"has_pmkid\":";
    j += s.has_pmkid ? 1 : 0;
    j += ",\"complete\":";
    j += s.complete ? 1 : 0;
    if (s.has_pmkid) {
      char hex[33];
      for (int k = 0; k < 16; k++)
        snprintf(hex + k * 2, 3, "%02X", s.pmkid[k]);
      hex[32] = 0;
      j += ",\"pmkid\":\"";
      j += hex;
      j += "\"";
    }
    j += ",\"msgs\":";
    j += s.msg_count;
    j += ",\"ssid\":\"";
    j += s.ssid;
    j += "\"";
    j += "}";
  }
  j += "]}";
  return j;
}

// ── Log JSON ──────────────────────────────────────────────────────────────────
static String eap_log_json() {
  String j = "{\"count\":" + String(g_eap.log_count) + ",\"log\":[";
  int start = (g_eap.log_head + EAP_MAX_LOG - g_eap.log_count) % EAP_MAX_LOG;
  for (int i = g_eap.log_count - 1; i >= 0; i--) {
    if (i < (int)g_eap.log_count - 1)
      j += ",";
    int idx = (start + i) % EAP_MAX_LOG;
    EapLog &l = g_eap.log[idx];
    char bssid[18];
    snprintf(bssid, sizeof(bssid), "%02X:%02X:%02X:%02X:%02X:%02X", l.bssid[0], l.bssid[1],
             l.bssid[2], l.bssid[3], l.bssid[4], l.bssid[5]);
    j += "{\"ts\":";
    j += l.ts_ms;
    j += ",\"bssid\":\"";
    j += bssid;
    j += "\"";
    j += ",\"msg\":";
    j += l.msg;
    j += ",\"note\":\"";
    j += l.note;
    j += "\"";
    j += "}";
  }
  j += "]}";
  return j;
}

// ── HTTP routes ───────────────────────────────────────────────────────────────
static void eaph_routes() {
  extern WebServer server;

  server.on("/api/eap/sessions", HTTP_GET,
            []() { server.send(200, "application/json", eap_sessions_json()); });

  server.on("/api/eap/log", HTTP_GET,
            []() { server.send(200, "application/json", eap_log_json()); });

  server.on("/api/eap/clear", HTTP_POST, []() {
    memset(&g_eap, 0, sizeof(g_eap));
    server.send(200, "application/json", "{\"ok\":1}");
  });

  server.on("/api/eap/summary", HTTP_GET, []() {
    char buf[128];
    snprintf(buf, sizeof(buf), "{\"sessions\":%u,\"complete\":%u,\"pmkid\":%u,\"total_eapol\":%u}",
             (unsigned)g_eap.session_count, (unsigned)g_eap.total_complete,
             (unsigned)g_eap.total_pmkid, (unsigned)g_eap.total_eapol);
    server.send(200, "application/json", buf);
  });
}

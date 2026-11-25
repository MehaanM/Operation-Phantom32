#pragma once
// pmkid_harvester.h — Passive PMKID harvester from EAPOL frame 1/4.
// Captures RSN Information Element from beacons and PMKID from EAPOL key messages.
// Prefix: pmk_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define PMK_MAX_RECORDS 32
#define PMK_PMKID_LEN 16

struct PmkRecord {
  bool active;
  uint8_t ap_bssid[6];
  uint8_t client_mac[6];
  char ssid[33];
  uint8_t ch;
  int8_t rssi;
  uint32_t ts;
  uint8_t pmkid[PMK_PMKID_LEN];
  bool has_pmkid;

  // RSN parameters
  uint8_t group_cipher;  // 0=None,1=WEP40,2=TKIP,3=WRAP,4=CCMP,5=WEP104
  uint8_t pairwise_cipher;
  uint8_t akm_suite;  // 1=802.1X,2=PSK,3=FT-IEEE,4=FT-PSK,6=PSK-SHA256,8=SAE,9=FT-SAE
  uint16_t rsn_caps;
  uint8_t pmk_gen;  // 2=WPA2,3=WPA3

  // EAPOL info
  uint8_t eapol_key_ver;
  uint32_t eapol_replay_counter_lo;
};

static struct {
  PmkRecord records[PMK_MAX_RECORDS];
  uint8_t count;
  uint32_t total_seen;
} g_pmk = {};

static bool pmk_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static PmkRecord *pmk_find(const uint8_t *ap_bssid, const uint8_t *client_mac) {
  for (int i = 0; i < PMK_MAX_RECORDS; i++) {
    if (!g_pmk.records[i].active)
      continue;
    if (pmk_mac_eq(g_pmk.records[i].ap_bssid, ap_bssid) &&
        pmk_mac_eq(g_pmk.records[i].client_mac, client_mac))
      return &g_pmk.records[i];
  }
  return nullptr;
}

static PmkRecord *pmk_alloc(const uint8_t *ap_bssid, const uint8_t *client_mac) {
  for (int i = 0; i < PMK_MAX_RECORDS; i++) {
    if (!g_pmk.records[i].active) {
      PmkRecord &r = g_pmk.records[i];
      memset(&r, 0, sizeof(r));
      r.active = true;
      memcpy(r.ap_bssid, ap_bssid, 6);
      memcpy(r.client_mac, client_mac, 6);
      g_pmk.count++;
      return &r;
    }
  }
  uint32_t oldest = UINT32_MAX;
  int idx = 0;
  for (int i = 0; i < PMK_MAX_RECORDS; i++)
    if (g_pmk.records[i].ts < oldest) {
      oldest = g_pmk.records[i].ts;
      idx = i;
    }
  memset(&g_pmk.records[idx], 0, sizeof(g_pmk.records[0]));
  g_pmk.records[idx].active = true;
  memcpy(g_pmk.records[idx].ap_bssid, ap_bssid, 6);
  memcpy(g_pmk.records[idx].client_mac, client_mac, 6);
  return &g_pmk.records[idx];
}

void pmk_ingest_rsn(const uint8_t *ap_bssid, const char *ssid, uint8_t ch, int8_t rssi,
                    uint8_t group_cipher, uint8_t pairwise_cipher, uint8_t akm, uint16_t rsn_caps,
                    uint8_t pmk_gen) {
  // RSN info from AP beacon; store against AP with blank client for reference
  for (int i = 0; i < PMK_MAX_RECORDS; i++) {
    PmkRecord &r = g_pmk.records[i];
    if (!r.active)
      continue;
    if (!pmk_mac_eq(r.ap_bssid, ap_bssid))
      continue;
    r.group_cipher = group_cipher;
    r.pairwise_cipher = pairwise_cipher;
    r.akm_suite = akm;
    r.rsn_caps = rsn_caps;
    r.pmk_gen = pmk_gen;
    r.ch = ch;
    if (rssi < 0)
      r.rssi = rssi;
    if (ssid && ssid[0])
      strncpy(r.ssid, ssid, 32);
  }
}

void pmk_ingest_eapol(const uint8_t *ap_bssid, const uint8_t *client_mac, const char *ssid,
                      uint8_t ch, int8_t rssi, uint8_t key_ver, const uint8_t *pmkid_data,
                      bool has_pmkid, uint32_t replay_ctr_lo) {
  PmkRecord *r = pmk_find(ap_bssid, client_mac);
  if (!r)
    r = pmk_alloc(ap_bssid, client_mac);
  if (!r)
    return;

  r->ts = millis() / 1000;
  r->ch = ch;
  if (rssi < 0)
    r->rssi = rssi;
  if (ssid && ssid[0])
    strncpy(r->ssid, ssid, 32);
  r->eapol_key_ver = key_ver;
  r->eapol_replay_counter_lo = replay_ctr_lo;

  if (has_pmkid && pmkid_data && !r->has_pmkid) {
    memcpy(r->pmkid, pmkid_data, PMK_PMKID_LEN);
    r->has_pmkid = true;
  }
  g_pmk.total_seen++;
}

static void pmk_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static void pmk_hex_str(const uint8_t *d, int len, char *out) {
  for (int i = 0; i < len; i++)
    snprintf(out + i * 2, 3, "%02X", d[i]);
  out[len * 2] = 0;
}

static const char *pmk_cipher_name(uint8_t c) {
  switch (c) {
    case 0:
      return "None";
    case 1:
      return "WEP40";
    case 2:
      return "TKIP";
    case 4:
      return "CCMP";
    case 5:
      return "WEP104";
    case 6:
      return "CMAC";
    case 8:
      return "GCMP";
    default:
      return "?";
  }
}

static const char *pmk_akm_name(uint8_t a) {
  switch (a) {
    case 1:
      return "802.1X";
    case 2:
      return "PSK";
    case 3:
      return "FT-802.1X";
    case 4:
      return "FT-PSK";
    case 6:
      return "PSK-SHA256";
    case 8:
      return "SAE";
    case 9:
      return "FT-SAE";
    case 11:
      return "SAE-EXT";
    default:
      return "?";
  }
}

static String pmk_json_all() {
  String s = "{\"count\":" + String(g_pmk.count);
  s += ",\"total_seen\":" + String(g_pmk.total_seen);
  s += ",\"records\":[";
  char mac[18], cmac[18];
  bool first = true;
  for (int i = 0; i < PMK_MAX_RECORDS; i++) {
    PmkRecord &r = g_pmk.records[i];
    if (!r.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    pmk_mac_str(r.ap_bssid, mac);
    pmk_mac_str(r.client_mac, cmac);
    s += "{\"ap\":\"" + String(mac) + "\"";
    s += ",\"client\":\"" + String(cmac) + "\"";
    s += ",\"ssid\":\"" + String(r.ssid) + "\"";
    s += ",\"ch\":" + String(r.ch);
    s += ",\"rssi\":" + String(r.rssi);
    s += ",\"ts\":" + String(r.ts);
    s += ",\"has_pmkid\":" + String(r.has_pmkid ? "true" : "false");
    if (r.has_pmkid) {
      char hex[PMK_PMKID_LEN * 2 + 1];
      pmk_hex_str(r.pmkid, PMK_PMKID_LEN, hex);
      s += ",\"pmkid\":\"" + String(hex) + "\"";
    }
    s += ",\"group_cipher\":\"" + String(pmk_cipher_name(r.group_cipher)) + "\"";
    s += ",\"pairwise_cipher\":\"" + String(pmk_cipher_name(r.pairwise_cipher)) + "\"";
    s += ",\"akm\":\"" + String(pmk_akm_name(r.akm_suite)) + "\"";
    s += ",\"rsn_caps\":" + String(r.rsn_caps);
    s += ",\"pmk_gen\":" + String(r.pmk_gen);
    s += ",\"eapol_key_ver\":" + String(r.eapol_key_ver);
    s += "}";
  }
  s += "]}";
  return s;
}

inline void pmk_routes() {
  server.on("/api/pmk/all", HTTP_GET,
            []() { server.send(200, "application/json", pmk_json_all()); });
  server.on("/api/pmk/clear", HTTP_POST, []() {
    memset(&g_pmk, 0, sizeof(g_pmk));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

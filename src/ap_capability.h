#pragma once
// ap_capability.h — AP 802.11 capability decoding and feature matrix.
// Parses HT/VHT/HE capability IEs to characterize AP hardware features.
// Prefix: apc_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define APC_MAX_APS    64

// HT Capability bits
#define HT_CAP_LDPC       (1<<0)
#define HT_CAP_40MHZ      (1<<1)
#define HT_CAP_SM_PS_MASK (3<<2)
#define HT_CAP_GF         (1<<4)
#define HT_CAP_SGI20      (1<<5)
#define HT_CAP_SGI40      (1<<6)
#define HT_CAP_TXSTBC     (1<<7)
#define HT_CAP_RXSTBC_MASK (3<<8)
#define HT_CAP_DELAYED_BA (1<<10)
#define HT_CAP_MAXAMSDU   (1<<11)
#define HT_CAP_DSSSCCK40  (1<<12)

// VHT Capability bits
#define VHT_CAP_80MHZ     (0<<2)
#define VHT_CAP_160MHZ    (1<<2)
#define VHT_CAP_80P80     (2<<2)
#define VHT_CAP_SGI80     (1<<5)
#define VHT_CAP_SGI160    (1<<6)
#define VHT_CAP_TXSTBC    (1<<7)
#define VHT_CAP_RXSTBC    (7<<8)
#define VHT_CAP_BFEE      (1<<19)
#define VHT_CAP_BFER      (1<<11)
#define VHT_CAP_MU_BFEE   (1<<20)
#define VHT_CAP_MU_BFER   (1<<19)

struct ApcAP {
  bool     active;
  uint8_t  bssid[6];
  char     ssid[33];
  uint8_t  channel;
  int8_t   rssi;

  // 802.11 generation detection
  bool     has_ht;      // 802.11n
  bool     has_vht;     // 802.11ac
  bool     has_he;      // 802.11ax (HE)
  bool     has_eht;     // 802.11be (EHT, WiFi 7)

  // HT Capabilities (802.11n)
  uint16_t ht_cap_info;
  uint8_t  ht_mcs_set[16]; // supported MCS indices
  uint8_t  ht_max_streams;
  bool     ht_sgi20;
  bool     ht_sgi40;
  bool     ht_40mhz;
  bool     ht_ldpc;
  bool     ht_greenfield;
  bool     ht_txstbc;

  // VHT Capabilities (802.11ac)
  uint32_t vht_cap_info;
  uint16_t vht_mcs_map;
  uint8_t  vht_max_streams;
  bool     vht_160mhz;
  bool     vht_sgi80;
  bool     vht_sgi160;
  bool     vht_bfee;
  bool     vht_mu_bfee;
  bool     vht_txstbc;

  // HE Capabilities (802.11ax)
  bool     he_80mhz;
  bool     he_160mhz;
  bool     he_bss_color;
  bool     he_twt;      // Target Wake Time
  bool     he_ofdma;
  bool     he_bsr;      // Buffer Status Report

  // Derived fields
  uint8_t  generation;   // 4=n, 5=ac, 6=ax, 7=be
  uint16_t max_data_rate; // estimated Mbps
  uint8_t  max_spatial_streams;
  char     generation_str[8]; // "WiFi 4" etc.

  uint32_t beacon_count;
  uint32_t first_ts;
  uint32_t last_ts;
};

static struct {
  ApcAP   aps[APC_MAX_APS];
  uint8_t ap_count;
  uint32_t total_beacons;
  uint32_t wifi4_count;
  uint32_t wifi5_count;
  uint32_t wifi6_count;
  uint32_t legacy_count;
} g_apc = {};

static bool apc_mac_eq(const uint8_t *a, const uint8_t *b) { return memcmp(a, b, 6) == 0; }

static ApcAP* apc_find_or_alloc(const uint8_t *bssid) {
  for (int i = 0; i < APC_MAX_APS; i++)
    if (g_apc.aps[i].active && apc_mac_eq(g_apc.aps[i].bssid, bssid))
      return &g_apc.aps[i];
  for (int i = 0; i < APC_MAX_APS; i++) {
    if (!g_apc.aps[i].active) {
      ApcAP &a = g_apc.aps[i];
      memset(&a, 0, sizeof(a));
      a.active = true;
      memcpy(a.bssid, bssid, 6);
      a.first_ts = millis()/1000;
      g_apc.ap_count++;
      return &a;
    }
  }
  uint32_t oldest = UINT32_MAX; int idx = 0;
  for (int i = 0; i < APC_MAX_APS; i++)
    if (g_apc.aps[i].last_ts < oldest) { oldest = g_apc.aps[i].last_ts; idx = i; }
  memset(&g_apc.aps[idx], 0, sizeof(g_apc.aps[0]));
  g_apc.aps[idx].active = true;
  memcpy(g_apc.aps[idx].bssid, bssid, 6);
  g_apc.aps[idx].first_ts = millis()/1000;
  return &g_apc.aps[idx];
}

static void apc_parse_ht(ApcAP &a, const uint8_t *data, uint8_t len) {
  if (len < 2) return;
  a.has_ht = true;
  a.ht_cap_info = data[0] | (data[1] << 8);
  a.ht_ldpc      = (a.ht_cap_info >> 0) & 1;
  a.ht_40mhz     = (a.ht_cap_info >> 1) & 1;
  a.ht_greenfield= (a.ht_cap_info >> 4) & 1;
  a.ht_sgi20     = (a.ht_cap_info >> 5) & 1;
  a.ht_sgi40     = (a.ht_cap_info >> 6) & 1;
  a.ht_txstbc    = (a.ht_cap_info >> 7) & 1;

  // MCS set starts at byte 3 (after AMPDU parameters)
  if (len >= 20) {
    memcpy(a.ht_mcs_set, data + 3, 10);
    // Count spatial streams from MCS set
    uint8_t nss = 0;
    for (int i = 0; i < 4; i++)
      if (a.ht_mcs_set[i]) nss = i + 1;
    a.ht_max_streams = nss;
    a.max_spatial_streams = max(a.max_spatial_streams, nss);
  }
}

static void apc_parse_vht(ApcAP &a, const uint8_t *data, uint8_t len) {
  if (len < 4) return;
  a.has_vht = true;
  a.vht_cap_info = data[0] | (data[1]<<8) | (data[2]<<16) | (data[3]<<24);
  uint8_t chw = (a.vht_cap_info >> 2) & 3;
  a.vht_160mhz   = (chw >= 1);
  a.vht_sgi80    = (a.vht_cap_info >> 5) & 1;
  a.vht_sgi160   = (a.vht_cap_info >> 6) & 1;
  a.vht_txstbc   = (a.vht_cap_info >> 7) & 1;
  a.vht_bfee     = (a.vht_cap_info >> 19) & 1;
  a.vht_mu_bfee  = (a.vht_cap_info >> 20) & 1;
  if (len >= 8) {
    a.vht_mcs_map = data[4] | (data[5] << 8);
    uint8_t nss = 0;
    for (int i = 0; i < 8; i++) {
      uint8_t mcs = (a.vht_mcs_map >> (i*2)) & 3;
      if (mcs != 3) nss = i + 1;
    }
    a.vht_max_streams = nss;
    a.max_spatial_streams = max(a.max_spatial_streams, nss);
  }
}

static void apc_parse_he(ApcAP &a, const uint8_t *data, uint8_t len) {
  if (len < 1) return;
  a.has_he = true;
  if (len >= 1) {
    a.he_twt  = (data[0] >> 1) & 1;
    a.he_bsr  = (data[0] >> 3) & 1;
  }
  if (len >= 2) {
    a.he_ofdma = (data[1] >> 1) & 1;
  }
}

static void apc_derive(ApcAP &a) {
  if (a.has_he || a.has_eht) {
    a.generation = a.has_eht ? 7 : 6;
    strncpy(a.generation_str, a.has_eht ? "WiFi 7" : "WiFi 6", 7);
    a.max_data_rate = a.has_eht ? 46000 : 9608;
  } else if (a.has_vht) {
    a.generation = 5;
    strncpy(a.generation_str, "WiFi 5", 7);
    a.max_data_rate = a.vht_max_streams * (a.vht_160mhz ? 867 : 433);
  } else if (a.has_ht) {
    a.generation = 4;
    strncpy(a.generation_str, "WiFi 4", 7);
    a.max_data_rate = a.ht_max_streams * (a.ht_40mhz ? 150 : 72);
  } else {
    a.generation = 3;
    strncpy(a.generation_str, "WiFi 3", 7);
    a.max_data_rate = 54;
  }
}

void apc_ingest_beacon(const uint8_t *bssid, const char *ssid, uint8_t ch, int8_t rssi,
                       const uint8_t *ht_ie, uint8_t ht_len,
                       const uint8_t *vht_ie, uint8_t vht_len,
                       const uint8_t *he_ie, uint8_t he_len) {
  g_apc.total_beacons++;
  ApcAP *a = apc_find_or_alloc(bssid);
  if (!a) return;

  if (ssid && ssid[0]) strncpy(a->ssid, ssid, 32);
  a->channel = ch;
  if (rssi < 0) a->rssi = rssi;
  a->beacon_count++;
  a->last_ts = millis()/1000;

  if (ht_ie && ht_len)   apc_parse_ht(*a, ht_ie, ht_len);
  if (vht_ie && vht_len) apc_parse_vht(*a, vht_ie, vht_len);
  if (he_ie && he_len)   apc_parse_he(*a, he_ie, he_len);
  apc_derive(*a);

  if (a->beacon_count == 1) {
    if (a->generation >= 6) g_apc.wifi6_count++;
    else if (a->generation == 5) g_apc.wifi5_count++;
    else if (a->generation == 4) g_apc.wifi4_count++;
    else g_apc.legacy_count++;
  }
}

static void apc_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String apc_json_all() {
  String s = "{\"ap_count\":" + String(g_apc.ap_count);
  s += ",\"wifi4\":" + String(g_apc.wifi4_count);
  s += ",\"wifi5\":" + String(g_apc.wifi5_count);
  s += ",\"wifi6\":" + String(g_apc.wifi6_count);
  s += ",\"legacy\":" + String(g_apc.legacy_count);
  s += ",\"aps\":[";
  char mac[18]; bool first = true;
  for (int i = 0; i < APC_MAX_APS; i++) {
    ApcAP &a = g_apc.aps[i];
    if (!a.active) continue;
    if (!first) s += ","; first = false;
    apc_mac_str(a.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(a.ssid) + "\"";
    s += ",\"ch\":" + String(a.channel);
    s += ",\"rssi\":" + String(a.rssi);
    s += ",\"gen\":" + String(a.generation);
    s += ",\"gen_str\":\"" + String(a.generation_str) + "\"";
    s += ",\"max_rate\":" + String(a.max_data_rate);
    s += ",\"streams\":" + String(a.max_spatial_streams);
    s += ",\"has_ht\":" + String(a.has_ht?"true":"false");
    s += ",\"has_vht\":" + String(a.has_vht?"true":"false");
    s += ",\"has_he\":" + String(a.has_he?"true":"false");
    s += ",\"ht_sgi20\":" + String(a.ht_sgi20?"true":"false");
    s += ",\"ht_sgi40\":" + String(a.ht_sgi40?"true":"false");
    s += ",\"ht_40mhz\":" + String(a.ht_40mhz?"true":"false");
    s += ",\"ht_ldpc\":" + String(a.ht_ldpc?"true":"false");
    s += ",\"vht_160\":" + String(a.vht_160mhz?"true":"false");
    s += ",\"vht_sgi80\":" + String(a.vht_sgi80?"true":"false");
    s += ",\"vht_bfee\":" + String(a.vht_bfee?"true":"false");
    s += ",\"he_twt\":" + String(a.he_twt?"true":"false");
    s += ",\"he_ofdma\":" + String(a.he_ofdma?"true":"false");
    s += ",\"beacons\":" + String(a.beacon_count);
    s += "}";
  }
  s += "]}";
  return s;
}

inline void apc_routes() {
  server.on("/api/apc/all", HTTP_GET, []() {
    server.send(200, "application/json", apc_json_all());
  });
  server.on("/api/apc/clear", HTTP_POST, []() {
    memset(&g_apc, 0, sizeof(g_apc));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

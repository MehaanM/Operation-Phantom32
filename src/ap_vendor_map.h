#pragma once
// ap_vendor_map.h — AP-to-vendor extended mapping with device type classification.
// Cross-references OUI with device categories (router, IoT, mobile AP, etc.)
// Prefix: avm_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define AVM_MAX_APS     64
#define AVM_MAX_VENDORS 24   // unique vendor OUI prefix table

enum AvmDeviceType : uint8_t {
  AVM_TYPE_UNKNOWN   = 0,
  AVM_TYPE_ROUTER    = 1,   // Consumer WiFi router
  AVM_TYPE_ENTERPRISE = 2,  // Enterprise/commercial AP
  AVM_TYPE_HOTSPOT   = 3,   // Mobile WiFi hotspot
  AVM_TYPE_REPEATER  = 4,   // WiFi extender/repeater
  AVM_TYPE_IOT       = 5,   // IoT gateway/smart home AP
  AVM_TYPE_MOBILE    = 6,   // Phone tethering
  AVM_TYPE_VIRTUAL   = 7,   // Virtualized / software AP
};

struct AvmVendorRecord {
  uint8_t oui[3];
  char    vendor[24];
  char    product[24];
  AvmDeviceType type;
};

// Compact OUI→vendor lookup table (subset of common APs)
static const AvmVendorRecord AVM_OUI_TABLE[] PROGMEM = {
  {{0x00,0x03,0x7f}, "Atheros",          "802.11 NIC",         AVM_TYPE_ROUTER},
  {{0x00,0x0c,0xe7}, "Linksys",          "WRT Series",         AVM_TYPE_ROUTER},
  {{0x00,0x14,0xbf}, "Linksys",          "WRT54G",             AVM_TYPE_ROUTER},
  {{0x00,0x18,0x01}, "Cisco",            "Aironet",            AVM_TYPE_ENTERPRISE},
  {{0x00,0x1a,0xa0}, "D-Link",           "DIR Series",         AVM_TYPE_ROUTER},
  {{0x00,0x1f,0x33}, "NETGEAR",          "WNR Series",         AVM_TYPE_ROUTER},
  {{0x00,0x24,0x01}, "ASUS",             "RT-N Series",        AVM_TYPE_ROUTER},
  {{0x00,0x26,0x44}, "Cisco",            "Catalyst AP",        AVM_TYPE_ENTERPRISE},
  {{0x00,0x50,0xf2}, "Microsoft",        "802.11 OUI",         AVM_TYPE_VIRTUAL},
  {{0x00,0x90,0x4c}, "Epigram",          "Broadcom WiFi",      AVM_TYPE_ROUTER},
  {{0x04,0x18,0xd6}, "NETGEAR",          "Orbi",               AVM_TYPE_ROUTER},
  {{0x04,0x92,0x26}, "Hewlett Packard",  "AP Series",          AVM_TYPE_ENTERPRISE},
  {{0x08,0x60,0x6e}, "NETGEAR",          "Nighthawk",          AVM_TYPE_ROUTER},
  {{0x10,0x62,0xeb}, "Routerboard",      "MikroTik",           AVM_TYPE_ENTERPRISE},
  {{0x14,0xf6,0x5a}, "Ruckus Wireless",  "ZoneFlex",           AVM_TYPE_ENTERPRISE},
  {{0x18,0x64,0x72}, "Apple",            "AirPort",            AVM_TYPE_ROUTER},
  {{0x18,0xa6,0xf7}, "D-Link",           "DSL/DIR",            AVM_TYPE_ROUTER},
  {{0x1c,0x1d,0x86}, "TP-Link",          "TL-WR Series",       AVM_TYPE_ROUTER},
  {{0x20,0x0d,0xb0}, "Technicolor",      "DSL Gateway",        AVM_TYPE_ROUTER},
  {{0x24,0xa2,0xe1}, "Ubiquiti",         "UniFi AP",           AVM_TYPE_ENTERPRISE},
  {{0x28,0x28,0x5d}, "Cisco",            "WAP Series",         AVM_TYPE_ENTERPRISE},
  {{0x2c,0x27,0xd7}, "NETGEAR",          "WNDR Series",        AVM_TYPE_ROUTER},
  {{0x30,0x46,0x9a}, "NETGEAR",          "AC1750",             AVM_TYPE_ROUTER},
  {{0x34,0x97,0xf6}, "NETGEAR",          "R7000",              AVM_TYPE_ROUTER},
};
static const uint8_t AVM_OUI_COUNT = sizeof(AVM_OUI_TABLE) / sizeof(AVM_OUI_TABLE[0]);

struct AvmAP {
  bool    active;
  uint8_t bssid[6];
  char    ssid[33];
  uint8_t ch;
  int8_t  rssi;
  uint32_t last_ts;
  uint32_t first_ts;
  uint32_t beacon_count;

  // Resolved vendor info
  char vendor[24];
  char product[24];
  AvmDeviceType type;

  // Capability flags from probing
  uint8_t generation;    // 3=legacy, 4=N, 5=AC, 6=AX
  bool    has_wps;
  bool    has_pmf;
  uint8_t security;      // 0-4

  // Scoring
  uint8_t trust_score;   // 0-100 (based on vendor, security, PMF)
};

static struct {
  AvmAP  aps[AVM_MAX_APS];
  uint8_t count;
  uint32_t total_beacons;
} g_avm = {};

static bool avm_mac_eq(const uint8_t *a, const uint8_t *b) { return memcmp(a,b,6)==0; }

static void avm_lookup_vendor(AvmAP &a) {
  for (int i = 0; i < AVM_OUI_COUNT; i++) {
    AvmVendorRecord rec;
    memcpy_P(&rec, &AVM_OUI_TABLE[i], sizeof(AvmVendorRecord));
    if (a.bssid[0] == rec.oui[0] && a.bssid[1] == rec.oui[1] && a.bssid[2] == rec.oui[2]) {
      strncpy(a.vendor, rec.vendor, 23);
      strncpy(a.product, rec.product, 23);
      a.type = rec.type;
      return;
    }
  }
  // Check for locally-administered bit (virtual/software AP)
  if (a.bssid[0] & 0x02) {
    strncpy(a.vendor, "Virtual/Random", 23);
    a.type = AVM_TYPE_VIRTUAL;
    return;
  }
  strncpy(a.vendor, "Unknown", 23);
  a.type = AVM_TYPE_UNKNOWN;
}

static void avm_compute_trust(AvmAP &a) {
  int score = 0;
  if (a.security >= 3) score += 30;  // WPA2+
  else if (a.security >= 2) score += 15;
  if (a.has_pmf) score += 20;
  if (a.generation >= 5) score += 15;  // WiFi 5+
  if (a.type == AVM_TYPE_ENTERPRISE) score += 20;
  else if (a.type == AVM_TYPE_ROUTER) score += 10;
  if (a.type == AVM_TYPE_VIRTUAL) score -= 20;
  if (a.security == 0) score = 0;
  a.trust_score = (uint8_t)max(0, min(100, score));
}

static AvmAP* avm_find(const uint8_t *bssid) {
  for (int i = 0; i < AVM_MAX_APS; i++)
    if (g_avm.aps[i].active && avm_mac_eq(g_avm.aps[i].bssid, bssid))
      return &g_avm.aps[i];
  return nullptr;
}

static AvmAP* avm_alloc(const uint8_t *bssid) {
  for (int i = 0; i < AVM_MAX_APS; i++) {
    if (!g_avm.aps[i].active) {
      AvmAP &a = g_avm.aps[i];
      memset(&a, 0, sizeof(a));
      a.active = true;
      memcpy(a.bssid, bssid, 6);
      a.first_ts = millis()/1000;
      g_avm.count++;
      return &a;
    }
  }
  uint32_t oldest = UINT32_MAX; int idx = 0;
  for (int i = 0; i < AVM_MAX_APS; i++)
    if (g_avm.aps[i].last_ts < oldest) { oldest = g_avm.aps[i].last_ts; idx = i; }
  memset(&g_avm.aps[idx], 0, sizeof(g_avm.aps[0]));
  g_avm.aps[idx].active = true;
  memcpy(g_avm.aps[idx].bssid, bssid, 6);
  g_avm.aps[idx].first_ts = millis()/1000;
  return &g_avm.aps[idx];
}

void avm_ingest(const uint8_t *bssid, const char *ssid, uint8_t ch, int8_t rssi,
                uint8_t gen, bool wps, bool pmf, uint8_t sec) {
  AvmAP *a = avm_find(bssid);
  if (!a) {
    a = avm_alloc(bssid);
    if (!a) return;
    avm_lookup_vendor(*a);
  }
  if (ssid && ssid[0]) strncpy(a->ssid, ssid, 32);
  a->ch = ch;
  if (rssi < 0) a->rssi = rssi;
  a->generation = gen;
  a->has_wps = wps;
  a->has_pmf = pmf;
  a->security = sec;
  a->last_ts = millis()/1000;
  a->beacon_count++;
  g_avm.total_beacons++;
  avm_compute_trust(*a);
}

static const char* avm_type_str(AvmDeviceType t) {
  switch(t) {
    case AVM_TYPE_ROUTER:     return "Router";
    case AVM_TYPE_ENTERPRISE: return "Enterprise";
    case AVM_TYPE_HOTSPOT:    return "Hotspot";
    case AVM_TYPE_REPEATER:   return "Repeater";
    case AVM_TYPE_IOT:        return "IoT";
    case AVM_TYPE_MOBILE:     return "Mobile";
    case AVM_TYPE_VIRTUAL:    return "Virtual";
    default:                  return "Unknown";
  }
}

static void avm_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String avm_json_all() {
  String s = "{\"count\":" + String(g_avm.count);
  s += ",\"total_beacons\":" + String(g_avm.total_beacons);
  s += ",\"aps\":[";
  char mac[18]; bool first = true;
  for (int i = 0; i < AVM_MAX_APS; i++) {
    AvmAP &a = g_avm.aps[i];
    if (!a.active) continue;
    if (!first) s += ","; first = false;
    avm_mac_str(a.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(a.ssid) + "\"";
    s += ",\"ch\":" + String(a.ch);
    s += ",\"rssi\":" + String(a.rssi);
    s += ",\"vendor\":\"" + String(a.vendor) + "\"";
    s += ",\"product\":\"" + String(a.product) + "\"";
    s += ",\"type\":\"" + String(avm_type_str(a.type)) + "\"";
    s += ",\"type_code\":" + String(a.type);
    s += ",\"gen\":" + String(a.generation);
    s += ",\"wps\":" + String(a.has_wps?"true":"false");
    s += ",\"pmf\":" + String(a.has_pmf?"true":"false");
    s += ",\"sec\":" + String(a.security);
    s += ",\"trust\":" + String(a.trust_score);
    s += ",\"beacons\":" + String(a.beacon_count);
    s += "}";
  }
  s += "]}";
  return s;
}

inline void avm_routes() {
  server.on("/api/avm/all", HTTP_GET, []() {
    server.send(200, "application/json", avm_json_all());
  });
  server.on("/api/avm/clear", HTTP_POST, []() {
    memset(&g_avm, 0, sizeof(g_avm));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

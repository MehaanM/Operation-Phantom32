#pragma once
// captive_portal.h — Captive portal detector.
// Identifies APs running captive portals by analyzing beacon information
// elements, SSID patterns, and client behavior. Tracks redirect patterns.
// Prefix: cpd_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define CPD_MAX_APS      32
#define CPD_MAX_CLIENTS  48
#define CPD_HIST_LEN     60

// Portal detection confidence levels
enum CpdConfidence : uint8_t {
  CPD_CONF_NONE        = 0,
  CPD_CONF_LOW         = 1,  // suspect SSID pattern
  CPD_CONF_MEDIUM      = 2,  // known vendor + open security
  CPD_CONF_HIGH        = 3,  // WLAN-specific IEs detected
  CPD_CONF_CONFIRMED   = 4   // client redirect observed
};

// Portal types
enum CpdPortalType : uint8_t {
  CPD_TYPE_UNKNOWN   = 0,
  CPD_TYPE_HOTEL     = 1,
  CPD_TYPE_CAFE      = 2,
  CPD_TYPE_AIRPORT   = 3,
  CPD_TYPE_CORPORATE = 4,
  CPD_TYPE_CELLULAR  = 5,
  CPD_TYPE_PAID      = 6,
  CPD_TYPE_GENERIC   = 7
};

static const char* CPD_TYPE_NAMES[] = {
  "Unknown", "Hotel", "Cafe", "Airport",
  "Corporate", "Cellular", "Paid", "Generic"
};

// Known portal vendor OUIs (access points)
static const struct { uint8_t oui[3]; const char *vendor; CpdPortalType type; }
CPD_PORTAL_VENDORS[] = {
  {{0x00,0x0E,0x8E}, "Cisco Meraki",   CPD_TYPE_CORPORATE},
  {{0x00,0x18,0x0A}, "Cisco Meraki",   CPD_TYPE_CORPORATE},
  {{0x34,0x97,0xF6}, "Cisco Meraki",   CPD_TYPE_CORPORATE},
  {{0x60,0x38,0xE0}, "Aruba/HPE",      CPD_TYPE_CORPORATE},
  {{0x00,0x24,0x6C}, "Aruba/HPE",      CPD_TYPE_CORPORATE},
  {{0x00,0x0B,0x86}, "Aruba/HPE",      CPD_TYPE_CORPORATE},
  {{0x68,0xC4,0x4D}, "Ruckus",         CPD_TYPE_CORPORATE},
  {{0x20,0xA6,0xCD}, "Ruckus",         CPD_TYPE_CORPORATE},
  {{0x38,0xFF,0x36}, "Cambium",        CPD_TYPE_GENERIC},
  {{0x00,0x00,0x00}, nullptr,          CPD_TYPE_UNKNOWN}
};

// Suspect SSID keywords that often indicate captive portals
static const char* CPD_SSID_KEYWORDS[] = {
  "guest", "free", "wifi", "hotel", "lobby", "public",
  "airport", "cafe", "coffee", "hotspot", "portal",
  "internet", "access", "connect", "open", nullptr
};

struct CpdAP {
  bool    active;
  uint8_t bssid[6];
  char    ssid[33];
  uint8_t ch;
  int8_t  rssi;
  uint8_t oui[3];
  bool    open_security;
  bool    has_wpa3;
  bool    has_bss_load;
  bool    has_cisco_ccx;  // Cisco CCX = often captive portal
  bool    has_passpoint;  // Hotspot 2.0 = likely captive portal

  CpdConfidence confidence;
  CpdPortalType portal_type;
  char    vendor[24];

  uint16_t client_count;
  uint16_t redirect_count;  // DNS redirects observed from this AP

  // Traffic patterns suggesting portal (ARP, DNS high, HTTP before HTTPS)
  uint32_t dns_frames;
  uint32_t arp_frames;
  uint32_t http_frames;
  uint32_t dhcp_frames;

  // Client redirected history (clients that got redirected)
  uint8_t  redirected_clients;

  uint32_t first_ts;
  uint32_t last_ts;
};

struct CpdClient {
  bool    active;
  uint8_t mac[6];
  uint8_t ap_bssid[6];
  char    ap_ssid[33];
  uint8_t ch;
  int8_t  rssi;
  bool    redirected;      // HTTP redirect observed
  bool    authenticated;   // POST to portal confirmed
  uint16_t dns_queries;
  uint16_t http_requests;
  uint32_t first_ts;
  uint32_t last_ts;
};

static struct {
  CpdAP     aps[CPD_MAX_APS];
  CpdClient clients[CPD_MAX_CLIENTS];
  uint8_t   ap_count;
  uint8_t   client_count;
  uint16_t  total_portals;
} g_cpd = {};

static bool cpd_mac_eq(const uint8_t *a, const uint8_t *b) { return memcmp(a,b,6)==0; }

static bool cpd_ssid_suspicious(const char *ssid) {
  if (!ssid || !ssid[0]) return false;
  char lower[33]; int i = 0;
  for (; ssid[i] && i < 32; i++) lower[i] = ssid[i] | 0x20;
  lower[i] = 0;
  for (int k = 0; CPD_SSID_KEYWORDS[k]; k++)
    if (strstr(lower, CPD_SSID_KEYWORDS[k])) return true;
  return false;
}

static const char* cpd_vendor_for_oui(const uint8_t *oui, CpdPortalType *type_out) {
  for (int i = 0; CPD_PORTAL_VENDORS[i].vendor; i++) {
    if (memcmp(CPD_PORTAL_VENDORS[i].oui, oui, 3) == 0) {
      if (type_out) *type_out = CPD_PORTAL_VENDORS[i].type;
      return CPD_PORTAL_VENDORS[i].vendor;
    }
  }
  return nullptr;
}

static CpdAP* cpd_find_ap(const uint8_t *bssid) {
  for (int i = 0; i < CPD_MAX_APS; i++)
    if (g_cpd.aps[i].active && cpd_mac_eq(g_cpd.aps[i].bssid, bssid))
      return &g_cpd.aps[i];
  return nullptr;
}

static CpdAP* cpd_alloc_ap(const uint8_t *bssid) {
  for (int i = 0; i < CPD_MAX_APS; i++) {
    if (!g_cpd.aps[i].active) {
      CpdAP &a = g_cpd.aps[i];
      memset(&a, 0, sizeof(a));
      a.active = true;
      memcpy(a.bssid, bssid, 6);
      memcpy(a.oui, bssid, 3);
      a.first_ts = millis()/1000;
      g_cpd.ap_count++;
      return &a;
    }
  }
  uint32_t oldest = UINT32_MAX; int idx = 0;
  for (int i = 0; i < CPD_MAX_APS; i++)
    if (g_cpd.aps[i].confidence < CPD_CONF_HIGH && g_cpd.aps[i].last_ts < oldest)
      { oldest = g_cpd.aps[i].last_ts; idx = i; }
  memset(&g_cpd.aps[idx], 0, sizeof(CpdAP));
  g_cpd.aps[idx].active = true;
  memcpy(g_cpd.aps[idx].bssid, bssid, 6);
  memcpy(g_cpd.aps[idx].oui, bssid, 3);
  g_cpd.aps[idx].first_ts = millis()/1000;
  return &g_cpd.aps[idx];
}

static void cpd_recompute_confidence(CpdAP &a) {
  CpdConfidence c = CPD_CONF_NONE;
  if (cpd_ssid_suspicious(a.ssid)) c = CPD_CONF_LOW;
  if (a.open_security) c = (CpdConfidence)(c < CPD_CONF_MEDIUM ? CPD_CONF_MEDIUM : c);
  if (a.has_cisco_ccx || a.has_passpoint) c = CPD_CONF_HIGH;
  if (a.vendor[0]) c = (CpdConfidence)(c < CPD_CONF_MEDIUM ? CPD_CONF_MEDIUM : c);
  if (a.redirect_count > 0 || a.redirected_clients > 0) c = CPD_CONF_CONFIRMED;
  a.confidence = c;
}

void cpd_ingest_beacon(const uint8_t *bssid, const char *ssid,
                        uint8_t ch, int8_t rssi,
                        bool open_sec, bool has_wpa3,
                        bool has_ccx, bool has_passpoint, bool has_bss_load) {
  CpdAP *a = cpd_find_ap(bssid);
  if (!a) a = cpd_alloc_ap(bssid);
  if (!a) return;

  if (ssid && ssid[0]) strncpy(a->ssid, ssid, 32);
  a->ch = ch; a->rssi = rssi;
  a->open_security = open_sec;
  a->has_wpa3     = has_wpa3;
  a->has_cisco_ccx = has_ccx;
  a->has_passpoint = has_passpoint;
  a->has_bss_load  = has_bss_load;
  a->last_ts = millis()/1000;

  CpdPortalType ptype = CPD_TYPE_UNKNOWN;
  const char *vendor = cpd_vendor_for_oui(a->oui, &ptype);
  if (vendor) { strncpy(a->vendor, vendor, 23); a->portal_type = ptype; }

  cpd_recompute_confidence(*a);
}

void cpd_ingest_client_dns(const uint8_t *client_mac, const uint8_t *ap_bssid) {
  CpdAP *a = cpd_find_ap(ap_bssid);
  if (a) { a->dns_frames++; cpd_recompute_confidence(*a); }
  for (int i = 0; i < CPD_MAX_CLIENTS; i++) {
    if (g_cpd.clients[i].active && cpd_mac_eq(g_cpd.clients[i].mac, client_mac)) {
      g_cpd.clients[i].dns_queries++;
      g_cpd.clients[i].last_ts = millis()/1000;
      return;
    }
  }
}

void cpd_ingest_redirect(const uint8_t *client_mac, const uint8_t *ap_bssid) {
  CpdAP *a = cpd_find_ap(ap_bssid);
  if (a) { a->redirect_count++; a->redirected_clients++; cpd_recompute_confidence(*a); }
  for (int i = 0; i < CPD_MAX_CLIENTS; i++) {
    if (g_cpd.clients[i].active && cpd_mac_eq(g_cpd.clients[i].mac, client_mac)) {
      g_cpd.clients[i].redirected = true;
      g_cpd.clients[i].last_ts   = millis()/1000;
      return;
    }
  }
}

static void cpd_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           m[0],m[1],m[2],m[3],m[4],m[5]);
}

static const char* cpd_conf_str(CpdConfidence c) {
  switch (c) {
    case CPD_CONF_NONE:      return "None";
    case CPD_CONF_LOW:       return "Low";
    case CPD_CONF_MEDIUM:    return "Medium";
    case CPD_CONF_HIGH:      return "High";
    case CPD_CONF_CONFIRMED: return "Confirmed";
  }
  return "Unknown";
}

static String cpd_json_all() {
  String s = "{\"ap_count\":" + String(g_cpd.ap_count);
  s += ",\"aps\":[";
  char mac[18]; bool first = true;
  for (int i = 0; i < CPD_MAX_APS; i++) {
    CpdAP &a = g_cpd.aps[i];
    if (!a.active) continue;
    if (!first) s += ","; first = false;
    cpd_mac_str(a.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(a.ssid) + "\"";
    s += ",\"ch\":" + String(a.ch);
    s += ",\"rssi\":" + String(a.rssi);
    s += ",\"confidence\":\"" + String(cpd_conf_str(a.confidence)) + "\"";
    s += ",\"portal_type\":\"" + String(CPD_TYPE_NAMES[a.portal_type]) + "\"";
    s += ",\"vendor\":\"" + String(a.vendor) + "\"";
    s += ",\"open\":" + String(a.open_security ? "true" : "false");
    s += ",\"ccx\":" + String(a.has_cisco_ccx ? "true" : "false");
    s += ",\"passpoint\":" + String(a.has_passpoint ? "true" : "false");
    s += ",\"dns\":" + String(a.dns_frames);
    s += ",\"redirects\":" + String(a.redirect_count);
    s += ",\"last_ts\":" + String(a.last_ts) + "}";
  }
  s += "]}";
  return s;
}

inline void cpd_routes() {
  server.on("/api/cpd/all", HTTP_GET, []() {
    server.send(200, "application/json", cpd_json_all());
  });
  server.on("/api/cpd/clear", HTTP_POST, []() {
    memset(&g_cpd, 0, sizeof(g_cpd));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

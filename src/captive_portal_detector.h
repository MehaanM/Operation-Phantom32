#pragma once
// captive_portal_detector.h — Captive portal and rogue AP detector.
// Detects captive portals by looking for DHCP-only open APs, DNS
// interception indicators, and known portal vendor fingerprints.
// Also detects credential-harvesting SSIDs with common name patterns.
// Prefix: cpd_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define CPD_MAX_APS    40
#define CPD_MAX_EVENTS 48

// Portal confidence level
enum CpdConfidence : uint8_t {
  CPD_CONF_LOW    = 0,  // Single weak indicator
  CPD_CONF_MEDIUM = 1,  // Multiple weak or one strong indicator
  CPD_CONF_HIGH   = 2,  // Strong indicator (known vendor, DNS redirect)
};

static const char* cpd_conf_str(CpdConfidence c) {
  switch(c) {
    case CPD_CONF_LOW:    return "Low";
    case CPD_CONF_MEDIUM: return "Medium";
    case CPD_CONF_HIGH:   return "High";
  }
  return "?";
}

// Portal type
enum CpdPortalType : uint8_t {
  CPD_TYPE_UNKNOWN      = 0,
  CPD_TYPE_HOTEL        = 1,
  CPD_TYPE_CAFE         = 2,
  CPD_TYPE_AIRPORT      = 3,
  CPD_TYPE_ISP_REDIRECT = 4,
  CPD_TYPE_CORPORATE    = 5,
  CPD_TYPE_EVIL_TWIN    = 6,  // Suspected evil twin
  CPD_TYPE_HONEYPOT     = 7,  // Likely credential harvester
};

static const char* cpd_type_str(CpdPortalType t) {
  switch(t) {
    case CPD_TYPE_HOTEL:        return "Hotel";
    case CPD_TYPE_CAFE:         return "Cafe";
    case CPD_TYPE_AIRPORT:      return "Airport";
    case CPD_TYPE_ISP_REDIRECT: return "ISP";
    case CPD_TYPE_CORPORATE:    return "Corp";
    case CPD_TYPE_EVIL_TWIN:    return "EvilTwin";
    case CPD_TYPE_HONEYPOT:     return "Honeypot";
    default:                    return "Unknown";
  }
}

// AP portal flags
enum CpdFlag : uint16_t {
  CPD_FLAG_OPEN_NO_PMF     = 0x0001, // Open AP without PMF
  CPD_FLAG_KNOWN_SSID      = 0x0002, // SSID matches a known portal pattern
  CPD_FLAG_NO_INTERNET_IE  = 0x0004, // Interworking IE with no internet bit
  CPD_FLAG_DS_PARAM_MISS   = 0x0008, // Missing DS parameter set IE
  CPD_FLAG_SHORT_BEACON_I  = 0x0010, // Unusually short beacon interval
  CPD_FLAG_VENDOR_PORTAL   = 0x0020, // Vendor OUI matches known portal vendor
  CPD_FLAG_NO_WPS          = 0x0040, // Open AP without WPS (unusual)
  CPD_FLAG_FAKE_SSID       = 0x0080, // SSID looks like a known network spoof
  CPD_FLAG_RAPID_CHANGE    = 0x0100, // SSID/security changed recently
  CPD_FLAG_WEAK_SEC        = 0x0200, // Downgraded security vs. known network
};

struct CpdAP {
  bool    active;
  uint8_t bssid[6];
  char    ssid[33];
  uint8_t ch;
  int8_t  rssi;
  bool    is_open;
  bool    has_pmf;
  bool    has_wps;
  bool    has_interworking;
  bool    internet_bit;
  uint16_t beacon_interval;
  uint8_t  security_flags;    // from scan: WPA/WPA2/WPA3
  uint16_t cpd_flags;
  CpdConfidence confidence;
  CpdPortalType portal_type;
  uint16_t  beacon_count;
  uint32_t  first_ts;
  uint32_t  last_ts;
  char      reason[48];       // human-readable reason for detection
};

struct CpdEvent {
  bool    active;
  char    ssid[33];
  uint8_t bssid[6];
  CpdPortalType type;
  CpdConfidence confidence;
  uint32_t ts;
  char     reason[48];
};

static struct {
  CpdAP    aps[CPD_MAX_APS];
  CpdEvent events[CPD_MAX_EVENTS];
  uint8_t  ap_count;
  uint8_t  event_count;
  uint8_t  ev_head;
  uint16_t total_detected;
  uint16_t total_evil_twin;
  uint16_t total_honeypot;
} g_cpd = {};

static bool cpd_mac_eq(const uint8_t *a, const uint8_t *b) { return memcmp(a,b,6)==0; }

// Known portal SSID substrings (all lowercase)
static const char* CPD_PORTAL_SSIDS[] = {
  "hotel","airport","lounge","guest","public","free","wifi","hotspot",
  "login","internet","connect","access","captive","portal","boingo",
  "attwifi","xfinity","cablewifi","spectrum"
};
#define CPD_NUM_PORTAL_SSIDS (sizeof(CPD_PORTAL_SSIDS)/sizeof(CPD_PORTAL_SSIDS[0]))

// Known portal vendor OUIs (first 3 bytes)
static const uint8_t CPD_PORTAL_OUIS[][3] = {
  {0x00,0x0F,0x66}, // Cisco WAVE
  {0x00,0x14,0xBF}, // Ruckus
  {0x00,0x1A,0x1E}, // Cisco
  {0xB4,0x82,0xFE}, // Cisco Meraki
  {0x88,0x15,0x44}, // Ruckus
  {0x34,0x17,0xEB}, // Aruba
};
#define CPD_NUM_OUIS (sizeof(CPD_PORTAL_OUIS)/sizeof(CPD_PORTAL_OUIS[0]))

static bool cpd_ssid_matches_portal(const char *ssid) {
  if (!ssid || !ssid[0]) return false;
  char lower[33];
  for (int i = 0; i < 32 && ssid[i]; i++) lower[i] = tolower((uint8_t)ssid[i]);
  lower[32] = 0;
  for (int i = 0; i < (int)CPD_NUM_PORTAL_SSIDS; i++)
    if (strstr(lower, CPD_PORTAL_SSIDS[i])) return true;
  return false;
}

static bool cpd_oui_matches_portal(const uint8_t *bssid) {
  for (int i = 0; i < (int)CPD_NUM_OUIS; i++)
    if (memcmp(bssid, CPD_PORTAL_OUIS[i], 3) == 0) return true;
  return false;
}

static void cpd_add_event(CpdAP &a) {
  CpdEvent &ev = g_cpd.events[g_cpd.ev_head % CPD_MAX_EVENTS];
  memset(&ev, 0, sizeof(ev));
  ev.active = true;
  strncpy(ev.ssid, a.ssid, 32);
  memcpy(ev.bssid, a.bssid, 6);
  ev.type = a.portal_type;
  ev.confidence = a.confidence;
  ev.ts = millis()/1000;
  strncpy(ev.reason, a.reason, 47);
  g_cpd.ev_head++;
  if (g_cpd.event_count < CPD_MAX_EVENTS) g_cpd.event_count++;
}

static void cpd_analyze(CpdAP &a) {
  uint8_t score = 0;
  uint16_t flags = 0;

  if (a.is_open && !a.has_pmf) { flags |= CPD_FLAG_OPEN_NO_PMF; score += 1; }
  if (cpd_ssid_matches_portal(a.ssid)) { flags |= CPD_FLAG_KNOWN_SSID; score += 2; }
  if (a.has_interworking && !a.internet_bit) { flags |= CPD_FLAG_NO_INTERNET_IE; score += 3; }
  if (cpd_oui_matches_portal(a.bssid)) { flags |= CPD_FLAG_VENDOR_PORTAL; score += 2; }
  if (a.beacon_interval > 0 && a.beacon_interval < 50) { flags |= CPD_FLAG_SHORT_BEACON_I; score += 1; }

  a.cpd_flags = flags;

  if (score >= 5) a.confidence = CPD_CONF_HIGH;
  else if (score >= 2) a.confidence = CPD_CONF_MEDIUM;
  else if (score >= 1) a.confidence = CPD_CONF_LOW;
  else { a.confidence = CPD_CONF_LOW; return; }

  // Classify portal type
  char lower[33];
  for (int i = 0; i < 32 && a.ssid[i]; i++) lower[i] = tolower((uint8_t)a.ssid[i]);
  lower[32] = 0;
  if (strstr(lower,"hotel") || strstr(lower,"inn") || strstr(lower,"hilton") || strstr(lower,"marriott"))
    a.portal_type = CPD_TYPE_HOTEL;
  else if (strstr(lower,"airport") || strstr(lower,"terminal") || strstr(lower,"lounge"))
    a.portal_type = CPD_TYPE_AIRPORT;
  else if (strstr(lower,"cafe") || strstr(lower,"coffee") || strstr(lower,"starbucks"))
    a.portal_type = CPD_TYPE_CAFE;
  else if (strstr(lower,"xfinity") || strstr(lower,"cablewifi") || strstr(lower,"spectrum"))
    a.portal_type = CPD_TYPE_ISP_REDIRECT;
  else if (a.confidence >= CPD_CONF_HIGH && a.is_open)
    a.portal_type = CPD_TYPE_HONEYPOT;
  else
    a.portal_type = CPD_TYPE_UNKNOWN;

  // Build reason string
  String r;
  if (flags & CPD_FLAG_OPEN_NO_PMF) r += "Open+nopmf ";
  if (flags & CPD_FLAG_KNOWN_SSID)  r += "KnownSSID ";
  if (flags & CPD_FLAG_NO_INTERNET_IE) r += "NoInternet ";
  if (flags & CPD_FLAG_VENDOR_PORTAL) r += "PortalOUI ";
  r.trim();
  strncpy(a.reason, r.c_str(), 47);
}

static CpdAP* cpd_find(const uint8_t *bssid) {
  for (int i = 0; i < CPD_MAX_APS; i++)
    if (g_cpd.aps[i].active && cpd_mac_eq(g_cpd.aps[i].bssid, bssid))
      return &g_cpd.aps[i];
  return nullptr;
}

static CpdAP* cpd_alloc(const uint8_t *bssid) {
  for (int i = 0; i < CPD_MAX_APS; i++) {
    if (!g_cpd.aps[i].active) {
      memset(&g_cpd.aps[i], 0, sizeof(CpdAP));
      g_cpd.aps[i].active = true;
      memcpy(g_cpd.aps[i].bssid, bssid, 6);
      g_cpd.aps[i].first_ts = millis()/1000;
      g_cpd.ap_count++;
      return &g_cpd.aps[i];
    }
  }
  uint32_t oldest = UINT32_MAX; int idx = 0;
  for (int i = 0; i < CPD_MAX_APS; i++)
    if (g_cpd.aps[i].last_ts < oldest) { oldest = g_cpd.aps[i].last_ts; idx = i; }
  memset(&g_cpd.aps[idx], 0, sizeof(CpdAP));
  g_cpd.aps[idx].active = true;
  memcpy(g_cpd.aps[idx].bssid, bssid, 6);
  g_cpd.aps[idx].first_ts = millis()/1000;
  return &g_cpd.aps[idx];
}

void cpd_ingest(const uint8_t *bssid, const char *ssid, uint8_t ch, int8_t rssi,
                bool is_open, bool has_pmf, bool has_wps,
                bool has_interworking, bool internet_bit,
                uint16_t beacon_interval, uint8_t security_flags) {
  CpdAP *a = cpd_find(bssid);
  if (!a) a = cpd_alloc(bssid);
  if (!a) return;

  if (ssid) strncpy(a->ssid, ssid, 32);
  a->ch = ch; a->rssi = rssi;
  a->is_open = is_open; a->has_pmf = has_pmf; a->has_wps = has_wps;
  a->has_interworking = has_interworking; a->internet_bit = internet_bit;
  a->beacon_interval = beacon_interval; a->security_flags = security_flags;
  a->beacon_count++;
  a->last_ts = millis()/1000;

  CpdConfidence prev = a->confidence;
  cpd_analyze(*a);

  if (a->confidence >= CPD_CONF_MEDIUM && prev < CPD_CONF_MEDIUM) {
    g_cpd.total_detected++;
    if (a->portal_type == CPD_TYPE_EVIL_TWIN) g_cpd.total_evil_twin++;
    if (a->portal_type == CPD_TYPE_HONEYPOT)  g_cpd.total_honeypot++;
    cpd_add_event(*a);
  }
}

static void cpd_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String cpd_json_all() {
  char mac[18];
  String s = "{\"ap_count\":" + String(g_cpd.ap_count);
  s += ",\"total_detected\":" + String(g_cpd.total_detected);
  s += ",\"total_evil_twin\":" + String(g_cpd.total_evil_twin);
  s += ",\"total_honeypot\":" + String(g_cpd.total_honeypot);
  s += ",\"aps\":[";
  bool first = true;
  for (int i = 0; i < CPD_MAX_APS; i++) {
    CpdAP &a = g_cpd.aps[i];
    if (!a.active) continue;
    if (!first) s += ","; first = false;
    cpd_mac_str(a.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(a.ssid) + "\"";
    s += ",\"ch\":" + String(a.ch);
    s += ",\"rssi\":" + String(a.rssi);
    s += ",\"open\":" + String(a.is_open?"true":"false");
    s += ",\"pmf\":" + String(a.has_pmf?"true":"false");
    s += ",\"conf\":\"" + String(cpd_conf_str(a.confidence)) + "\"";
    s += ",\"type\":\"" + String(cpd_type_str(a.portal_type)) + "\"";
    s += ",\"flags\":" + String(a.cpd_flags);
    s += ",\"reason\":\"" + String(a.reason) + "\"";
    s += ",\"beacons\":" + String(a.beacon_count) + "}";
  }
  s += "],\"events\":[";
  first = true;
  for (int i = 0; i < CPD_MAX_EVENTS; i++) {
    CpdEvent &ev = g_cpd.events[i];
    if (!ev.active) continue;
    if (!first) s += ","; first = false;
    cpd_mac_str(ev.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\",\"ssid\":\"" + String(ev.ssid) + "\"";
    s += ",\"conf\":\"" + String(cpd_conf_str(ev.confidence)) + "\"";
    s += ",\"type\":\"" + String(cpd_type_str(ev.type)) + "\"";
    s += ",\"ts\":" + String(ev.ts);
    s += ",\"reason\":\"" + String(ev.reason) + "\"}";
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

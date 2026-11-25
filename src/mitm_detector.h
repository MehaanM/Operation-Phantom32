#pragma once
// mitm_detector.h — Man-in-the-middle attack detector.
// Detects ARP spoofing, rogue DHCP servers, SSL stripping indicators,
// and evil-twin SSID conflicts. Prefix: mitm_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define MITM_MAX_ARP_CACHE 64
#define MITM_MAX_DHCP_SERVERS 8
#define MITM_MAX_SSID_CONF 32
#define MITM_MAX_ALERTS 64
#define MITM_SSID_LEN 33

// ─── Alert types ──────────────────────────────────────────────────────────────
enum MitmAlertType {
  MITM_ARP_SPOOF = 0,    // IP→MAC mapping changed
  MITM_ROGUE_DHCP,       // multiple DHCP servers on network
  MITM_EVIL_TWIN,        // same SSID, different BSSID with stronger RSSI
  MITM_DEAUTH_REDIRECT,  // deauth + new AP with same SSID
  MITM_BSSID_CHANGE,     // AP SSID moved to new BSSID
  MITM_IP_CONFLICT,      // same IP, different MAC
};

// ─── ARP cache entry ──────────────────────────────────────────────────────────
struct MitmArpEntry {
  bool active;
  uint8_t ip[4];
  uint8_t mac[6];
  uint32_t first_seen;
  uint32_t last_seen;
  uint32_t updates;
  uint8_t prev_mac[6];  // previous MAC if changed
  bool spoofed;
};

// ─── DHCP server record ───────────────────────────────────────────────────────
struct MitmDhcpServer {
  bool active;
  uint8_t server_mac[6];
  uint8_t server_ip[4];
  uint8_t gateway_ip[4];
  uint8_t subnet[4];
  uint8_t dns[4];
  uint32_t first_seen;
  uint32_t last_seen;
  uint32_t offers_seen;
  bool rogue;  // multiple servers → one of them is rogue
};

// ─── SSID conflict ────────────────────────────────────────────────────────────
struct MitmSsidConflict {
  bool active;
  char ssid[MITM_SSID_LEN];
  uint8_t bssid_a[6];
  uint8_t bssid_b[6];
  int8_t rssi_a;
  int8_t rssi_b;
  uint8_t ch_a;
  uint8_t ch_b;
  uint32_t detected_ts;
  bool likely_evil_twin;
};

// ─── Alert ────────────────────────────────────────────────────────────────────
struct MitmAlert {
  uint32_t ts;
  MitmAlertType type;
  char detail[80];
  uint8_t mac_a[6];
  uint8_t mac_b[6];
  uint8_t ip[4];
  uint8_t severity;  // 1-5
};

// ─── State ────────────────────────────────────────────────────────────────────
static struct {
  MitmArpEntry arp_cache[MITM_MAX_ARP_CACHE];
  MitmDhcpServer dhcp_servers[MITM_MAX_DHCP_SERVERS];
  MitmSsidConflict ssid_conflicts[MITM_MAX_SSID_CONF];
  MitmAlert alerts[MITM_MAX_ALERTS];
  uint8_t alert_head;
  uint32_t total_alerts;
  uint32_t arp_spoofs;
  uint32_t rogue_dhcp_count;
  uint32_t evil_twins;
} g_mitm = {};

// ─── Helpers ─────────────────────────────────────────────────────────────────
static bool mitm_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}
static bool mitm_ip_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 4) == 0;
}

static void mitm_push_alert(MitmAlertType type, const char *detail, const uint8_t *mac_a,
                            const uint8_t *mac_b, const uint8_t *ip, uint8_t severity) {
  MitmAlert &a = g_mitm.alerts[g_mitm.alert_head % MITM_MAX_ALERTS];
  a.ts = millis() / 1000;
  a.type = type;
  strncpy(a.detail, detail, 79);
  if (mac_a)
    memcpy(a.mac_a, mac_a, 6);
  if (mac_b)
    memcpy(a.mac_b, mac_b, 6);
  if (ip)
    memcpy(a.ip, ip, 4);
  a.severity = severity;
  g_mitm.alert_head++;
  g_mitm.total_alerts++;
}

// ─── ARP ingestion ───────────────────────────────────────────────────────────
// Called when an ARP packet is seen; sender_ip/sender_mac from ARP payload
void mitm_ingest_arp(const uint8_t *sender_mac, const uint8_t *sender_ip, uint16_t arp_op) {
  // Skip broadcast/multicast
  if (sender_mac[0] & 0x01)
    return;
  uint8_t zero_ip[4] = {0, 0, 0, 0};
  if (mitm_ip_eq(sender_ip, zero_ip))
    return;

  // Find in cache
  for (int i = 0; i < MITM_MAX_ARP_CACHE; i++) {
    MitmArpEntry &e = g_mitm.arp_cache[i];
    if (!e.active)
      continue;
    if (mitm_ip_eq(e.ip, sender_ip)) {
      e.last_seen = millis() / 1000;
      e.updates++;
      if (!mitm_mac_eq(e.mac, sender_mac)) {
        // MAC changed for same IP = potential ARP spoof
        memcpy(e.prev_mac, e.mac, 6);
        memcpy(e.mac, sender_mac, 6);
        e.spoofed = true;
        g_mitm.arp_spoofs++;
        char detail[80];
        snprintf(detail, 80, "ARP: IP %u.%u.%u.%u MAC changed", sender_ip[0], sender_ip[1],
                 sender_ip[2], sender_ip[3]);
        mitm_push_alert(MITM_ARP_SPOOF, detail, e.prev_mac, sender_mac, sender_ip, 4);
      }
      return;
    }
  }

  // New entry
  for (int i = 0; i < MITM_MAX_ARP_CACHE; i++) {
    if (!g_mitm.arp_cache[i].active) {
      MitmArpEntry &e = g_mitm.arp_cache[i];
      memset(&e, 0, sizeof(e));
      memcpy(e.ip, sender_ip, 4);
      memcpy(e.mac, sender_mac, 6);
      e.active = true;
      e.first_seen = millis() / 1000;
      e.last_seen = e.first_seen;
      return;
    }
  }
}

// ─── DHCP ingestion ──────────────────────────────────────────────────────────
// Called when a DHCP Offer is seen
void mitm_ingest_dhcp_offer(const uint8_t *server_mac, const uint8_t *server_ip,
                            const uint8_t *gateway_ip, const uint8_t *offered_ip) {
  // Check if we already know this server
  for (int i = 0; i < MITM_MAX_DHCP_SERVERS; i++) {
    if (!g_mitm.dhcp_servers[i].active)
      continue;
    if (mitm_mac_eq(g_mitm.dhcp_servers[i].server_mac, server_mac)) {
      g_mitm.dhcp_servers[i].last_seen = millis() / 1000;
      g_mitm.dhcp_servers[i].offers_seen++;
      return;
    }
  }

  // New DHCP server
  for (int i = 0; i < MITM_MAX_DHCP_SERVERS; i++) {
    if (!g_mitm.dhcp_servers[i].active) {
      MitmDhcpServer &s = g_mitm.dhcp_servers[i];
      memset(&s, 0, sizeof(s));
      memcpy(s.server_mac, server_mac, 6);
      if (server_ip)
        memcpy(s.server_ip, server_ip, 4);
      if (gateway_ip)
        memcpy(s.gateway_ip, gateway_ip, 4);
      s.active = true;
      s.first_seen = millis() / 1000;
      s.last_seen = s.first_seen;
      s.offers_seen = 1;

      // Count active servers
      int active_count = 0;
      for (int j = 0; j < MITM_MAX_DHCP_SERVERS; j++) {
        if (g_mitm.dhcp_servers[j].active)
          active_count++;
      }
      if (active_count > 1) {
        s.rogue = true;
        g_mitm.rogue_dhcp_count++;
        char detail[80];
        snprintf(detail, 80, "Rogue DHCP server: %u.%u.%u.%u", server_ip ? server_ip[0] : 0,
                 server_ip ? server_ip[1] : 0, server_ip ? server_ip[2] : 0,
                 server_ip ? server_ip[3] : 0);
        mitm_push_alert(MITM_ROGUE_DHCP, detail, server_mac, nullptr, server_ip, 4);
        // Mark all existing as potentially rogue
        for (int j = 0; j < MITM_MAX_DHCP_SERVERS; j++) {
          if (g_mitm.dhcp_servers[j].active)
            g_mitm.dhcp_servers[j].rogue = true;
        }
      }
      return;
    }
  }
}

// ─── SSID conflict detection ─────────────────────────────────────────────────
void mitm_ingest_ap(const uint8_t *bssid, const char *ssid, int8_t rssi, uint8_t channel) {
  if (!ssid || strlen(ssid) == 0)
    return;

  // Look for existing entry with same SSID but different BSSID
  for (int i = 0; i < MITM_MAX_SSID_CONF; i++) {
    MitmSsidConflict &c = g_mitm.ssid_conflicts[i];
    if (!c.active)
      continue;
    if (strcmp(c.ssid, ssid) != 0)
      continue;
    if (mitm_mac_eq(c.bssid_a, bssid) || mitm_mac_eq(c.bssid_b, bssid)) {
      // Update RSSI
      if (mitm_mac_eq(c.bssid_a, bssid))
        c.rssi_a = rssi;
      else
        c.rssi_b = rssi;
      return;
    }
    // New BSSID for same SSID = conflict
    if (!c.likely_evil_twin) {
      memcpy(c.bssid_b, bssid, 6);
      c.rssi_b = rssi;
      c.ch_b = channel;
      c.likely_evil_twin = (rssi > c.rssi_a);  // stronger signal = likely attacker
      g_mitm.evil_twins++;
      char detail[80];
      snprintf(detail, 80, "Evil twin: SSID '%s' ch%u", ssid, channel);
      mitm_push_alert(MITM_EVIL_TWIN, detail, c.bssid_a, bssid, nullptr, 5);
    }
    return;
  }

  // New SSID entry
  for (int i = 0; i < MITM_MAX_SSID_CONF; i++) {
    if (!g_mitm.ssid_conflicts[i].active) {
      MitmSsidConflict &c = g_mitm.ssid_conflicts[i];
      memset(&c, 0, sizeof(c));
      strncpy(c.ssid, ssid, MITM_SSID_LEN - 1);
      memcpy(c.bssid_a, bssid, 6);
      c.rssi_a = rssi;
      c.ch_a = channel;
      c.active = true;
      c.detected_ts = millis() / 1000;
      return;
    }
  }
}

// ─── JSON ─────────────────────────────────────────────────────────────────────
static const char *mitm_alert_type_str(MitmAlertType t) {
  switch (t) {
    case MITM_ARP_SPOOF:
      return "arp_spoof";
    case MITM_ROGUE_DHCP:
      return "rogue_dhcp";
    case MITM_EVIL_TWIN:
      return "evil_twin";
    case MITM_DEAUTH_REDIRECT:
      return "deauth_redirect";
    case MITM_BSSID_CHANGE:
      return "bssid_change";
    case MITM_IP_CONFLICT:
      return "ip_conflict";
    default:
      return "unknown";
  }
}

static void mitm_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String mitm_json_status() {
  String s = "{\"total_alerts\":" + String(g_mitm.total_alerts);
  s += ",\"arp_spoofs\":" + String(g_mitm.arp_spoofs);
  s += ",\"rogue_dhcp\":" + String(g_mitm.rogue_dhcp_count);
  s += ",\"evil_twins\":" + String(g_mitm.evil_twins);

  s += ",\"alerts\":[";
  char mac1[18], mac2[18];
  int atotal = min((int)g_mitm.total_alerts, MITM_MAX_ALERTS);
  for (int i = 0; i < atotal; i++) {
    int idx = ((int)g_mitm.alert_head - 1 - i + MITM_MAX_ALERTS) % MITM_MAX_ALERTS;
    MitmAlert &a = g_mitm.alerts[idx];
    if (a.ts == 0)
      continue;
    if (i)
      s += ",";
    mitm_mac_str(a.mac_a, mac1);
    mitm_mac_str(a.mac_b, mac2);
    s += "{\"ts\":" + String(a.ts);
    s += ",\"type\":\"" + String(mitm_alert_type_str(a.type)) + "\"";
    s += ",\"detail\":\"" + String(a.detail) + "\"";
    s += ",\"sev\":" + String(a.severity);
    s += ",\"mac_a\":\"" + String(mac1) + "\"";
    s += ",\"mac_b\":\"" + String(mac2) + "\"";
    s += ",\"ip\":\"" + String(a.ip[0]) + "." + String(a.ip[1]) + "." + String(a.ip[2]) + "." +
         String(a.ip[3]) + "\"";
    s += "}";
  }
  s += "],\"evil_twin_list\":[";
  bool first = true;
  for (int i = 0; i < MITM_MAX_SSID_CONF; i++) {
    MitmSsidConflict &c = g_mitm.ssid_conflicts[i];
    if (!c.active || !c.likely_evil_twin)
      continue;
    if (!first)
      s += ",";
    first = false;
    mitm_mac_str(c.bssid_a, mac1);
    mitm_mac_str(c.bssid_b, mac2);
    s += "{\"ssid\":\"" + String(c.ssid) + "\"";
    s += ",\"bssid_a\":\"" + String(mac1) + "\"";
    s += ",\"bssid_b\":\"" + String(mac2) + "\"";
    s += ",\"rssi_a\":" + String(c.rssi_a);
    s += ",\"rssi_b\":" + String(c.rssi_b);
    s += "}";
  }
  s += "]}";
  return s;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void mitm_routes() {
  server.on("/api/mitm/status", HTTP_GET,
            []() { server.send(200, "application/json", mitm_json_status()); });

  server.on("/api/mitm/clear", HTTP_POST, []() {
    memset(&g_mitm, 0, sizeof(g_mitm));
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/mitm/arp_cache", HTTP_GET, []() {
    String s = "{\"entries\":[";
    char mac1[18], mac2[18];
    bool first = true;
    for (int i = 0; i < MITM_MAX_ARP_CACHE; i++) {
      MitmArpEntry &e = g_mitm.arp_cache[i];
      if (!e.active)
        continue;
      if (!first)
        s += ",";
      first = false;
      mitm_mac_str(e.mac, mac1);
      mitm_mac_str(e.prev_mac, mac2);
      s += "{\"ip\":\"" + String(e.ip[0]) + "." + String(e.ip[1]) + "." + String(e.ip[2]) + "." +
           String(e.ip[3]) + "\"";
      s += ",\"mac\":\"" + String(mac1) + "\"";
      s += ",\"spoofed\":" + String(e.spoofed ? "true" : "false");
      if (e.spoofed)
        s += ",\"prev_mac\":\"" + String(mac2) + "\"";
      s += ",\"last_seen\":" + String(e.last_seen);
      s += "}";
    }
    s += "]}";
    server.send(200, "application/json", s);
  });
}

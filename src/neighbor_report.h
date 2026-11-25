#pragma once
// neighbor_report.h — 802.11k neighbor report (RRM neighbor list) parser and UI backend.
// Builds the network graph of advertised neighbor relationships.
// Prefix: nbr_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define NBR_MAX_APS 48
#define NBR_MAX_NEIGHBORS 16  // neighbors per AP

struct NbrEntry {
  uint8_t bssid[6];
  uint32_t bssid_info;  // capability bits from neighbor report
  uint8_t op_class;
  uint8_t ch;
  uint8_t phy_type;
  // bssid_info bit meanings
  bool admits_spectrum_mgmt;  // bit 0
  bool is_qos;                // bit 1
  bool is_apsd;               // bit 2
  bool is_rm;                 // bit 3 (RRM)
  bool is_delayed_ba;         // bit 4
  bool is_imm_ba;             // bit 5
};

struct NbrAP {
  bool active;
  uint8_t bssid[6];
  char ssid[33];
  uint8_t ch;
  int8_t rssi;
  uint32_t first_ts;
  uint32_t last_ts;
  uint32_t report_count;  // number of neighbor reports heard

  // Neighbors this AP advertised
  NbrEntry neighbors[NBR_MAX_NEIGHBORS];
  uint8_t neighbor_count;

  // ESS information
  bool has_rrm;
  bool has_bss_trans;  // 802.11v BSS Transition Management
};

static struct {
  NbrAP aps[NBR_MAX_APS];
  uint8_t count;
  uint32_t total_reports;
} g_nbr = {};

static bool nbr_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static NbrAP *nbr_find(const uint8_t *bssid) {
  for (int i = 0; i < NBR_MAX_APS; i++)
    if (g_nbr.aps[i].active && nbr_mac_eq(g_nbr.aps[i].bssid, bssid))
      return &g_nbr.aps[i];
  return nullptr;
}

static NbrAP *nbr_alloc(const uint8_t *bssid) {
  for (int i = 0; i < NBR_MAX_APS; i++) {
    if (!g_nbr.aps[i].active) {
      NbrAP &a = g_nbr.aps[i];
      memset(&a, 0, sizeof(a));
      a.active = true;
      memcpy(a.bssid, bssid, 6);
      a.first_ts = millis() / 1000;
      g_nbr.count++;
      return &a;
    }
  }
  uint32_t oldest = UINT32_MAX;
  int idx = 0;
  for (int i = 0; i < NBR_MAX_APS; i++)
    if (g_nbr.aps[i].last_ts < oldest) {
      oldest = g_nbr.aps[i].last_ts;
      idx = i;
    }
  memset(&g_nbr.aps[idx], 0, sizeof(g_nbr.aps[0]));
  g_nbr.aps[idx].active = true;
  memcpy(g_nbr.aps[idx].bssid, bssid, 6);
  g_nbr.aps[idx].first_ts = millis() / 1000;
  return &g_nbr.aps[idx];
}

void nbr_ingest_ap(const uint8_t *bssid, const char *ssid, uint8_t ch, int8_t rssi, bool has_rrm,
                   bool has_bss_trans) {
  NbrAP *a = nbr_find(bssid);
  if (!a)
    a = nbr_alloc(bssid);
  if (!a)
    return;

  if (ssid && ssid[0])
    strncpy(a->ssid, ssid, 32);
  a->ch = ch;
  if (rssi < 0)
    a->rssi = rssi;
  a->last_ts = millis() / 1000;
  a->has_rrm = has_rrm;
  a->has_bss_trans = has_bss_trans;
}

void nbr_ingest_neighbor(const uint8_t *reporting_ap, const uint8_t *neighbor_bssid,
                         uint32_t bssid_info, uint8_t op_class, uint8_t ch, uint8_t phy_type) {
  NbrAP *a = nbr_find(reporting_ap);
  if (!a)
    return;

  a->last_ts = millis() / 1000;
  a->report_count++;
  g_nbr.total_reports++;

  // Update or add neighbor entry
  for (int i = 0; i < a->neighbor_count; i++) {
    if (nbr_mac_eq(a->neighbors[i].bssid, neighbor_bssid)) {
      a->neighbors[i].bssid_info = bssid_info;
      a->neighbors[i].op_class = op_class;
      a->neighbors[i].ch = ch;
      a->neighbors[i].phy_type = phy_type;
      return;
    }
  }
  if (a->neighbor_count < NBR_MAX_NEIGHBORS) {
    NbrEntry &e = a->neighbors[a->neighbor_count++];
    memcpy(e.bssid, neighbor_bssid, 6);
    e.bssid_info = bssid_info;
    e.op_class = op_class;
    e.ch = ch;
    e.phy_type = phy_type;
    e.admits_spectrum_mgmt = (bssid_info >> 0) & 1;
    e.is_qos = (bssid_info >> 1) & 1;
    e.is_apsd = (bssid_info >> 2) & 1;
    e.is_rm = (bssid_info >> 3) & 1;
    e.is_delayed_ba = (bssid_info >> 4) & 1;
    e.is_imm_ba = (bssid_info >> 5) & 1;
  }
}

static void nbr_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String nbr_json_all() {
  String s = "{\"count\":" + String(g_nbr.count);
  s += ",\"total_reports\":" + String(g_nbr.total_reports);
  s += ",\"aps\":[";
  char mac[18];
  bool first = true;
  for (int i = 0; i < NBR_MAX_APS; i++) {
    NbrAP &a = g_nbr.aps[i];
    if (!a.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    nbr_mac_str(a.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(a.ssid) + "\"";
    s += ",\"ch\":" + String(a.ch);
    s += ",\"rssi\":" + String(a.rssi);
    s += ",\"rrm\":" + String(a.has_rrm ? "true" : "false");
    s += ",\"btm\":" + String(a.has_bss_trans ? "true" : "false");
    s += ",\"reports\":" + String(a.report_count);
    s += ",\"neighbors\":[";
    bool fn = true;
    for (int j = 0; j < a.neighbor_count; j++) {
      NbrEntry &e = a.neighbors[j];
      if (!fn)
        s += ",";
      fn = false;
      nbr_mac_str(e.bssid, mac);
      s += "{\"bssid\":\"" + String(mac) + "\"";
      s += ",\"ch\":" + String(e.ch);
      s += ",\"op_class\":" + String(e.op_class);
      s += ",\"phy\":" + String(e.phy_type);
      s += ",\"bssid_info\":" + String(e.bssid_info);
      s += ",\"qos\":" + String(e.is_qos ? "true" : "false");
      s += ",\"rrm\":" + String(e.is_rm ? "true" : "false");
      s += "}";
    }
    s += "]}";
  }
  s += "]}";
  return s;
}

inline void nbr_routes() {
  server.on("/api/nbr/all", HTTP_GET,
            []() { server.send(200, "application/json", nbr_json_all()); });
  server.on("/api/nbr/clear", HTTP_POST, []() {
    memset(&g_nbr, 0, sizeof(g_nbr));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

#pragma once
// deauth_storm.h — Deauthentication / disassociation storm detector.
// Tracks deauth rates per AP and per client. High rates indicate
// ongoing deauth attacks, misconfigurations, or WPA2-MIC failures.
// Prefix: das_

#include <Arduino.h>
#include <WebServer.h>
#include "psram_attr.h"

extern WebServer server;

#define DAS_MAX_APS 32
#define DAS_MAX_CLIENTS 48
#define DAS_HIST_LEN 30     // 30-second window
#define DAS_STORM_THRESH 5  // deauths/sec to flag a storm

// Reason codes from IEEE 802.11-2020 Table 9-45
static const char *das_reason_name(uint16_t rc) {
  switch (rc) {
    case 1:
      return "Unspec";
    case 2:
      return "Prev-auth expired";
    case 3:
      return "Leaving";
    case 4:
      return "Inactivity";
    case 5:
      return "AP-busy";
    case 6:
      return "Class2";
    case 7:
      return "Class3";
    case 8:
      return "Dis-assoc";
    case 9:
      return "Not-authed";
    case 15:
      return "4-way fail";
    case 16:
      return "GTK fail";
    case 23:
      return "IEEE-802.1X fail";
    case 34:
      return "TDLS-teardn";
    default:
      return "Other";
  }
}

struct DasEvent {
  uint8_t ap_bssid[6];
  uint8_t client_mac[6];
  uint16_t reason;
  bool is_disassoc;
  bool is_broadcast;
  uint32_t ts;
};

#define DAS_EVENT_RING 128

struct DasAP {
  bool active;
  uint8_t bssid[6];
  char ssid[33];
  uint8_t ch;
  int8_t rssi;

  uint32_t total_deauths;
  uint32_t total_disassoc;
  uint32_t broadcast_deauths;

  // 30-second rolling count
  uint16_t hist[DAS_HIST_LEN];
  uint8_t hist_head;
  uint16_t cur_count;
  uint32_t last_tick_s;

  bool storm_active;
  uint32_t storm_start_ts;
  uint32_t last_ts;
  uint32_t first_ts;

  // Most common reason code for this AP
  uint16_t top_reason;
  uint16_t top_reason_count;
};

struct DasClient {
  bool active;
  uint8_t mac[6];
  uint8_t last_ap[6];
  uint32_t deauth_count;
  uint32_t disassoc_count;
  uint32_t last_ts;
};

typedef struct {
  DasAP aps[DAS_MAX_APS];
  DasClient clients[DAS_MAX_CLIENTS];
  DasEvent events[DAS_EVENT_RING];
  uint8_t event_head;
  uint16_t total_events;
  uint8_t ap_count;
  uint8_t cli_count;
} G_DasState;
#ifdef BOARD_HAS_PSRAM
static G_DasState *_g_das_raw;
#define g_das (*_g_das_raw)
#else
static G_DasState g_das;
#endif

static bool das_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static DasAP *das_find_ap(const uint8_t *bssid) {
  for (int i = 0; i < DAS_MAX_APS; i++)
    if (g_das.aps[i].active && das_mac_eq(g_das.aps[i].bssid, bssid))
      return &g_das.aps[i];
  return nullptr;
}

static DasAP *das_alloc_ap(const uint8_t *bssid) {
  for (int i = 0; i < DAS_MAX_APS; i++) {
    if (!g_das.aps[i].active) {
      DasAP &a = g_das.aps[i];
      memset(&a, 0, sizeof(a));
      a.active = true;
      memcpy(a.bssid, bssid, 6);
      a.first_ts = millis() / 1000;
      g_das.ap_count++;
      return &a;
    }
  }
  uint32_t oldest = UINT32_MAX;
  int idx = 0;
  for (int i = 0; i < DAS_MAX_APS; i++)
    if (g_das.aps[i].last_ts < oldest) {
      oldest = g_das.aps[i].last_ts;
      idx = i;
    }
  memset(&g_das.aps[idx], 0, sizeof(g_das.aps[0]));
  g_das.aps[idx].active = true;
  memcpy(g_das.aps[idx].bssid, bssid, 6);
  g_das.aps[idx].first_ts = millis() / 1000;
  return &g_das.aps[idx];
}

static DasClient *das_find_client(const uint8_t *mac) {
  for (int i = 0; i < DAS_MAX_CLIENTS; i++)
    if (g_das.clients[i].active && das_mac_eq(g_das.clients[i].mac, mac))
      return &g_das.clients[i];
  return nullptr;
}

static DasClient *das_alloc_client(const uint8_t *mac) {
  for (int i = 0; i < DAS_MAX_CLIENTS; i++) {
    if (!g_das.clients[i].active) {
      DasClient &c = g_das.clients[i];
      memset(&c, 0, sizeof(c));
      c.active = true;
      memcpy(c.mac, mac, 6);
      g_das.cli_count++;
      return &c;
    }
  }
  uint32_t oldest = UINT32_MAX;
  int idx = 0;
  for (int i = 0; i < DAS_MAX_CLIENTS; i++)
    if (g_das.clients[i].last_ts < oldest) {
      oldest = g_das.clients[i].last_ts;
      idx = i;
    }
  memset(&g_das.clients[idx], 0, sizeof(g_das.clients[0]));
  g_das.clients[idx].active = true;
  memcpy(g_das.clients[idx].mac, mac, 6);
  return &g_das.clients[idx];
}

static void das_tick_ap(DasAP &a) {
  uint32_t now_s = millis() / 1000;
  if (now_s == a.last_tick_s)
    return;
  uint32_t elapsed = now_s - a.last_tick_s;
  if (elapsed > DAS_HIST_LEN)
    elapsed = DAS_HIST_LEN;
  for (uint32_t i = 0; i < elapsed; i++) {
    a.hist[a.hist_head % DAS_HIST_LEN] = (i == 0) ? a.cur_count : 0;
    a.hist_head = (a.hist_head + 1) % DAS_HIST_LEN;
  }
  a.last_tick_s = now_s;
  a.cur_count = 0;
}

static void das_check_storm(DasAP &a) {
  uint32_t window_sum = 0;
  for (int i = 0; i < DAS_HIST_LEN; i++)
    window_sum += a.hist[i];
  float rate = (float)window_sum / DAS_HIST_LEN;  // per second average
  if (rate >= DAS_STORM_THRESH) {
    if (!a.storm_active) {
      a.storm_active = true;
      a.storm_start_ts = millis() / 1000;
    }
  } else {
    a.storm_active = false;
  }
}

void das_ingest(const uint8_t *ap_bssid, const uint8_t *client_mac, const char *ssid, uint8_t ch,
                int8_t rssi, uint16_t reason, bool is_disassoc, bool is_broadcast) {
  DasAP *a = das_find_ap(ap_bssid);
  if (!a)
    a = das_alloc_ap(ap_bssid);
  if (!a)
    return;

  if (ssid && ssid[0])
    strncpy(a->ssid, ssid, 32);
  a->ch = ch;
  if (rssi < 0)
    a->rssi = rssi;
  a->last_ts = millis() / 1000;

  das_tick_ap(*a);
  a->cur_count++;

  if (is_disassoc)
    a->total_disassoc++;
  else
    a->total_deauths++;
  if (is_broadcast)
    a->broadcast_deauths++;

  // Track top reason
  if (reason == a->top_reason) {
    a->top_reason_count++;
  } else if (a->top_reason_count == 0) {
    a->top_reason = reason;
    a->top_reason_count = 1;
  }

  das_check_storm(*a);

  // Update client record
  if (!is_broadcast) {
    DasClient *c = das_find_client(client_mac);
    if (!c)
      c = das_alloc_client(client_mac);
    if (c) {
      memcpy(c->last_ap, ap_bssid, 6);
      c->last_ts = a->last_ts;
      if (is_disassoc)
        c->disassoc_count++;
      else
        c->deauth_count++;
    }
  }

  // Append to event ring
  DasEvent &ev = g_das.events[g_das.event_head % DAS_EVENT_RING];
  memcpy(ev.ap_bssid, ap_bssid, 6);
  memcpy(ev.client_mac, client_mac, 6);
  ev.reason = reason;
  ev.is_disassoc = is_disassoc;
  ev.is_broadcast = is_broadcast;
  ev.ts = a->last_ts;
  g_das.event_head = (g_das.event_head + 1) % DAS_EVENT_RING;
  g_das.total_events++;
}

static void das_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[3], m[5]);
  // corrected:
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String das_json_all() {
  String s = "{\"ap_count\":" + String(g_das.ap_count);
  s += ",\"total_events\":" + String(g_das.total_events);

  // Count storms
  int storms = 0;
  for (int i = 0; i < DAS_MAX_APS; i++)
    if (g_das.aps[i].active && g_das.aps[i].storm_active)
      storms++;
  s += ",\"active_storms\":" + String(storms);

  s += ",\"aps\":[";
  char mac[18];
  bool first = true;
  for (int i = 0; i < DAS_MAX_APS; i++) {
    DasAP &a = g_das.aps[i];
    if (!a.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    das_mac_str(a.bssid, mac);
    // compute window average
    uint32_t ws = 0;
    for (int j = 0; j < DAS_HIST_LEN; j++)
      ws += a.hist[j];
    float rate = (float)ws / DAS_HIST_LEN;
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(a.ssid) + "\"";
    s += ",\"ch\":" + String(a.ch);
    s += ",\"rssi\":" + String(a.rssi);
    s += ",\"total_deauths\":" + String(a.total_deauths);
    s += ",\"total_disassoc\":" + String(a.total_disassoc);
    s += ",\"bcast_deauths\":" + String(a.broadcast_deauths);
    s += ",\"rate\":" + String((int)(rate * 10));  // *10 to avoid float in JSON
    s += ",\"storm\":" + String(a.storm_active ? "true" : "false");
    s += ",\"top_reason_code\":" + String(a.top_reason);
    s += ",\"top_reason\":\"" + String(das_reason_name(a.top_reason)) + "\"";
    s += ",\"hist\":[";
    for (int j = 0; j < DAS_HIST_LEN; j++) {
      if (j)
        s += ",";
      int idx = (a.hist_head + j) % DAS_HIST_LEN;
      s += String(a.hist[idx]);
    }
    s += "]}";
  }
  s += "],\"events\":[";
  // Last 32 events (most recent first)
  bool fe = true;
  for (int i = 0; i < DAS_EVENT_RING; i++) {
    int idx = (g_das.event_head - 1 - i + DAS_EVENT_RING) % DAS_EVENT_RING;
    DasEvent &ev = g_das.events[idx];
    if (ev.ts == 0)
      continue;
    if (!fe)
      s += ",";
    fe = false;
    das_mac_str(ev.ap_bssid, mac);
    char cmac[18];
    das_mac_str(ev.client_mac, cmac);
    s += "{\"ap\":\"" + String(mac) + "\"";
    s += ",\"client\":\"" + String(cmac) + "\"";
    s += ",\"reason\":" + String(ev.reason);
    s += ",\"reason_name\":\"" + String(das_reason_name(ev.reason)) + "\"";
    s += ",\"type\":\"" + String(ev.is_disassoc ? "Disassoc" : "Deauth") + "\"";
    s += ",\"bcast\":" + String(ev.is_broadcast ? "true" : "false");
    s += ",\"ts\":" + String(ev.ts) + "}";
    if (i >= 31)
      break;
  }
  s += "]}";
  return s;
}

inline void das_routes() {
  server.on("/api/das/all", HTTP_GET,
            []() { server.send(200, "application/json", das_json_all()); });
  server.on("/api/das/clear", HTTP_POST, []() {
    memset(&g_das, 0, sizeof(g_das));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

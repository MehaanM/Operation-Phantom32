#pragma once
// association_history.h — Full association/disassociation event log.
// Tracks every association/disassociation/deauth event with timing.
// Prefix: ash_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define ASH_MAX_EVENTS  256
#define ASH_MAX_CLIENTS  64

enum AshEventType {
  ASH_ASSOC_REQ  = 0,
  ASH_ASSOC_RESP = 1,
  ASH_REASSOC    = 2,
  ASH_DISASSOC   = 3,
  ASH_DEAUTH     = 4,
  ASH_AUTH       = 5,
};

static const char* ASH_EVENT_NAMES[] = {
  "Assoc Req","Assoc Resp","Reassoc","Disassoc","Deauth","Auth"
};

struct AshEvent {
  bool      active;
  AshEventType type;
  uint8_t   client[6];
  uint8_t   ap[6];
  char      ssid[33];
  uint8_t   channel;
  int8_t    rssi;
  uint16_t  reason_code;  // for disassoc/deauth
  uint16_t  status_code;  // for assoc resp
  uint32_t  ts;           // millis()/1000
};

struct AshClient {
  bool    active;
  uint8_t mac[6];
  uint8_t current_ap[6];
  char    ssid[33];
  uint8_t channel;

  uint32_t assoc_count;
  uint32_t disassoc_count;
  uint32_t deauth_count;
  uint32_t last_assoc_ts;
  uint32_t last_deauth_ts;

  uint32_t first_seen;
  uint32_t last_seen;

  bool     is_associated;
};

static struct {
  AshEvent   events[ASH_MAX_EVENTS];
  uint8_t    event_head;
  uint32_t   event_count;

  AshClient  clients[ASH_MAX_CLIENTS];
  uint8_t    client_count;

  uint32_t   total_assocs;
  uint32_t   total_deauths;
  uint32_t   total_disassocs;
} g_ash = {};

static bool ash_mac_eq(const uint8_t *a, const uint8_t *b) { return memcmp(a, b, 6) == 0; }

static AshClient* ash_find_or_alloc_client(const uint8_t *mac) {
  for (int i = 0; i < ASH_MAX_CLIENTS; i++)
    if (g_ash.clients[i].active && ash_mac_eq(g_ash.clients[i].mac, mac))
      return &g_ash.clients[i];
  for (int i = 0; i < ASH_MAX_CLIENTS; i++) {
    if (!g_ash.clients[i].active) {
      AshClient &c = g_ash.clients[i];
      memset(&c, 0, sizeof(c));
      c.active  = true;
      memcpy(c.mac, mac, 6);
      c.first_seen = millis()/1000;
      g_ash.client_count++;
      return &c;
    }
  }
  uint32_t oldest = UINT32_MAX; int idx = 0;
  for (int i = 0; i < ASH_MAX_CLIENTS; i++)
    if (g_ash.clients[i].last_seen < oldest) { oldest = g_ash.clients[i].last_seen; idx = i; }
  memset(&g_ash.clients[idx], 0, sizeof(g_ash.clients[0]));
  g_ash.clients[idx].active = true;
  memcpy(g_ash.clients[idx].mac, mac, 6);
  g_ash.clients[idx].first_seen = millis()/1000;
  return &g_ash.clients[idx];
}

static void ash_push_event(AshEventType type,
                            const uint8_t *client, const uint8_t *ap,
                            const char *ssid, uint8_t ch, int8_t rssi,
                            uint16_t reason, uint16_t status) {
  AshEvent &e = g_ash.events[g_ash.event_head];
  memset(&e, 0, sizeof(e));
  e.active      = true;
  e.type        = type;
  e.channel     = ch;
  e.rssi        = rssi;
  e.reason_code = reason;
  e.status_code = status;
  e.ts          = millis()/1000;
  memcpy(e.client, client, 6);
  memcpy(e.ap,     ap,     6);
  if (ssid) strncpy(e.ssid, ssid, 32);

  g_ash.event_head = (g_ash.event_head + 1) % ASH_MAX_EVENTS;
  g_ash.event_count = min((uint32_t)ASH_MAX_EVENTS, g_ash.event_count + 1);
}

void ash_ingest(AshEventType type,
                const uint8_t *client, const uint8_t *ap,
                const char *ssid, uint8_t channel, int8_t rssi,
                uint16_t reason_code, uint16_t status_code) {
  uint32_t now = millis()/1000;

  ash_push_event(type, client, ap, ssid, channel, rssi, reason_code, status_code);

  AshClient *c = ash_find_or_alloc_client(client);
  if (!c) return;

  c->last_seen = now;
  c->channel   = channel;
  if (ssid && ssid[0]) strncpy(c->ssid, ssid, 32);

  switch (type) {
    case ASH_ASSOC_REQ:
    case ASH_ASSOC_RESP:
    case ASH_REASSOC:
      c->assoc_count++;
      c->last_assoc_ts = now;
      c->is_associated = true;
      memcpy(c->current_ap, ap, 6);
      g_ash.total_assocs++;
      break;
    case ASH_DISASSOC:
      c->disassoc_count++;
      c->is_associated = false;
      g_ash.total_disassocs++;
      break;
    case ASH_DEAUTH:
      c->deauth_count++;
      c->last_deauth_ts = now;
      c->is_associated  = false;
      g_ash.total_deauths++;
      break;
    case ASH_AUTH:
      break;
  }
}

static void ash_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String ash_json_events(int limit = 50) {
  String s = "[";
  char c1[18], c2[18];
  int shown = 0;
  for (int i = 0; i < ASH_MAX_EVENTS && shown < limit; i++) {
    int idx = ((int)g_ash.event_head - 1 - i + ASH_MAX_EVENTS) % ASH_MAX_EVENTS;
    AshEvent &e = g_ash.events[idx];
    if (!e.active) continue;
    if (shown) s += ",";
    ash_mac_str(e.client, c1);
    ash_mac_str(e.ap, c2);
    s += "{\"type\":\"" + String(ASH_EVENT_NAMES[e.type]) + "\"";
    s += ",\"client\":\"" + String(c1) + "\"";
    s += ",\"ap\":\"" + String(c2) + "\"";
    s += ",\"ssid\":\"" + String(e.ssid) + "\"";
    s += ",\"ch\":" + String(e.channel);
    s += ",\"rssi\":" + String(e.rssi);
    s += ",\"reason\":" + String(e.reason_code);
    s += ",\"status\":" + String(e.status_code);
    s += ",\"ts\":" + String(e.ts) + "}";
    shown++;
  }
  s += "]";
  return s;
}

static String ash_json_summary() {
  String s = "{\"total_assocs\":" + String(g_ash.total_assocs);
  s += ",\"total_deauths\":" + String(g_ash.total_deauths);
  s += ",\"total_disassocs\":" + String(g_ash.total_disassocs);
  s += ",\"client_count\":" + String(g_ash.client_count);
  s += ",\"event_count\":" + String(g_ash.event_count);
  s += "}";
  return s;
}

static String ash_json_clients() {
  String s = "[";
  char mac[18], ap_mac[18];
  bool first = true;
  for (int i = 0; i < ASH_MAX_CLIENTS; i++) {
    AshClient &c = g_ash.clients[i];
    if (!c.active) continue;
    if (!first) s += ",";
    first = false;
    ash_mac_str(c.mac, mac);
    ash_mac_str(c.current_ap, ap_mac);
    s += "{\"mac\":\"" + String(mac) + "\"";
    s += ",\"ap\":\"" + String(ap_mac) + "\"";
    s += ",\"ssid\":\"" + String(c.ssid) + "\"";
    s += ",\"ch\":" + String(c.channel);
    s += ",\"assocs\":" + String(c.assoc_count);
    s += ",\"deauths\":" + String(c.deauth_count);
    s += ",\"disassocs\":" + String(c.disassoc_count);
    s += ",\"assoc\":" + String(c.is_associated?"true":"false");
    s += ",\"last_ts\":" + String(c.last_seen) + "}";
  }
  s += "]";
  return s;
}

inline void ash_routes() {
  server.on("/api/ash/events", HTTP_GET, []() {
    server.send(200, "application/json", ash_json_events(50));
  });
  server.on("/api/ash/summary", HTTP_GET, []() {
    server.send(200, "application/json", ash_json_summary());
  });
  server.on("/api/ash/clients", HTTP_GET, []() {
    server.send(200, "application/json", ash_json_clients());
  });
  server.on("/api/ash/clear", HTTP_POST, []() {
    memset(&g_ash, 0, sizeof(g_ash));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

#pragma once
// network_topology.h — 802.11 network topology mapper.
// Builds AP→client association maps, detects infrastructure/ad-hoc, IBSS.
// Prefix: nto_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define NTO_MAX_APS       48
#define NTO_MAX_CLIENTS   128
#define NTO_MAX_LINKS     192  // AP-client association pairs

enum NtoNodeType : uint8_t {
  NTO_AP     = 0,
  NTO_CLIENT = 1,
  NTO_IBSS   = 2,  // ad-hoc
};

enum NtoLinkState : uint8_t {
  NTO_LINK_ASSOC    = 0,  // confirmed association
  NTO_LINK_PROBE    = 1,  // only probes seen
  NTO_LINK_DATA     = 2,  // data frames seen (inferred)
};

struct NtoAP {
  bool     active;
  uint8_t  bssid[6];
  char     ssid[33];
  uint8_t  channel;
  int8_t   rssi;
  uint8_t  client_count;    // currently associated
  uint32_t first_ts;
  uint32_t last_ts;
  bool     hidden;
  bool     infrastructure;  // false = IBSS/ad-hoc
};

struct NtoClient {
  bool     active;
  uint8_t  mac[6];
  uint8_t  assoc_bssid[6]; // current/last associated AP
  char     assoc_ssid[33];
  uint8_t  channel;
  int8_t   rssi;
  uint32_t first_ts;
  uint32_t last_ts;
  uint32_t frame_count;
  bool     has_assoc;
};

struct NtoLink {
  bool     active;
  uint8_t  ap_bssid[6];
  uint8_t  client_mac[6];
  NtoLinkState state;
  int8_t   ap_rssi;
  int8_t   cl_rssi;
  uint32_t frame_count;
  uint32_t first_ts;
  uint32_t last_ts;
  uint32_t last_assoc_ts;
};

static struct {
  NtoAP     aps[NTO_MAX_APS];
  uint8_t   ap_count;
  NtoClient clients[NTO_MAX_CLIENTS];
  uint8_t   client_count;
  NtoLink   links[NTO_MAX_LINKS];
  uint8_t   link_count;
  uint32_t  total_assocs;
  uint32_t  total_probes;
} g_nto = {};

static bool nto_mac_eq(const uint8_t *a, const uint8_t *b) { return memcmp(a, b, 6) == 0; }
static bool nto_is_bcast(const uint8_t *m) { return m[0] & 0x01; }
static bool nto_is_zero(const uint8_t *m) {
  return m[0]==0 && m[1]==0 && m[2]==0 && m[3]==0 && m[4]==0 && m[5]==0;
}

static NtoAP* nto_find_ap(const uint8_t *bssid) {
  for (int i = 0; i < NTO_MAX_APS; i++)
    if (g_nto.aps[i].active && nto_mac_eq(g_nto.aps[i].bssid, bssid))
      return &g_nto.aps[i];
  return nullptr;
}

static NtoAP* nto_alloc_ap(const uint8_t *bssid) {
  for (int i = 0; i < NTO_MAX_APS; i++) {
    if (!g_nto.aps[i].active) {
      NtoAP &a = g_nto.aps[i];
      memset(&a, 0, sizeof(a));
      a.active = true;
      memcpy(a.bssid, bssid, 6);
      a.infrastructure = true;
      a.first_ts = millis()/1000;
      g_nto.ap_count++;
      return &a;
    }
  }
  uint32_t oldest = UINT32_MAX; int idx = 0;
  for (int i = 0; i < NTO_MAX_APS; i++)
    if (g_nto.aps[i].last_ts < oldest) { oldest = g_nto.aps[i].last_ts; idx = i; }
  memset(&g_nto.aps[idx], 0, sizeof(g_nto.aps[0]));
  g_nto.aps[idx].active = true;
  memcpy(g_nto.aps[idx].bssid, bssid, 6);
  g_nto.aps[idx].first_ts = millis()/1000;
  return &g_nto.aps[idx];
}

static NtoClient* nto_find_client(const uint8_t *mac) {
  for (int i = 0; i < NTO_MAX_CLIENTS; i++)
    if (g_nto.clients[i].active && nto_mac_eq(g_nto.clients[i].mac, mac))
      return &g_nto.clients[i];
  return nullptr;
}

static NtoClient* nto_alloc_client(const uint8_t *mac) {
  for (int i = 0; i < NTO_MAX_CLIENTS; i++) {
    if (!g_nto.clients[i].active) {
      NtoClient &c = g_nto.clients[i];
      memset(&c, 0, sizeof(c));
      c.active = true;
      memcpy(c.mac, mac, 6);
      c.first_ts = millis()/1000;
      g_nto.client_count++;
      return &c;
    }
  }
  uint32_t oldest = UINT32_MAX; int idx = 0;
  for (int i = 0; i < NTO_MAX_CLIENTS; i++)
    if (g_nto.clients[i].last_ts < oldest) { oldest = g_nto.clients[i].last_ts; idx = i; }
  memset(&g_nto.clients[idx], 0, sizeof(g_nto.clients[0]));
  g_nto.clients[idx].active = true;
  memcpy(g_nto.clients[idx].mac, mac, 6);
  g_nto.clients[idx].first_ts = millis()/1000;
  return &g_nto.clients[idx];
}

static NtoLink* nto_find_link(const uint8_t *ap, const uint8_t *cl) {
  for (int i = 0; i < NTO_MAX_LINKS; i++)
    if (g_nto.links[i].active &&
        nto_mac_eq(g_nto.links[i].ap_bssid, ap) &&
        nto_mac_eq(g_nto.links[i].client_mac, cl))
      return &g_nto.links[i];
  return nullptr;
}

static NtoLink* nto_alloc_link(const uint8_t *ap, const uint8_t *cl) {
  for (int i = 0; i < NTO_MAX_LINKS; i++) {
    if (!g_nto.links[i].active) {
      NtoLink &l = g_nto.links[i];
      memset(&l, 0, sizeof(l));
      l.active = true;
      memcpy(l.ap_bssid, ap, 6);
      memcpy(l.client_mac, cl, 6);
      l.first_ts = millis()/1000;
      g_nto.link_count++;
      return &l;
    }
  }
  uint32_t oldest = UINT32_MAX; int idx = 0;
  for (int i = 0; i < NTO_MAX_LINKS; i++)
    if (g_nto.links[i].last_ts < oldest) { oldest = g_nto.links[i].last_ts; idx = i; }
  memset(&g_nto.links[idx], 0, sizeof(g_nto.links[0]));
  g_nto.links[idx].active = true;
  memcpy(g_nto.links[idx].ap_bssid, ap, 6);
  memcpy(g_nto.links[idx].client_mac, cl, 6);
  g_nto.links[idx].first_ts = millis()/1000;
  return &g_nto.links[idx];
}

void nto_ingest_beacon(const uint8_t *bssid, const char *ssid,
                       uint8_t ch, int8_t rssi, bool hidden) {
  NtoAP *a = nto_find_ap(bssid);
  if (!a) a = nto_alloc_ap(bssid);
  if (!a) return;
  if (ssid && ssid[0]) strncpy(a->ssid, ssid, 32);
  a->channel = ch;
  if (rssi < 0) a->rssi = rssi;
  a->hidden = hidden;
  a->last_ts = millis()/1000;
}

void nto_ingest_assoc(const uint8_t *client, const uint8_t *bssid,
                      const char *ssid, uint8_t ch, int8_t rssi) {
  if (!client || !bssid || nto_is_bcast(client)) return;
  uint32_t now_s = millis()/1000;
  g_nto.total_assocs++;

  NtoAP *a = nto_find_ap(bssid);
  if (!a) { a = nto_alloc_ap(bssid); if (a && ssid) strncpy(a->ssid, ssid, 32); }
  if (a) { a->last_ts = now_s; a->channel = ch; }

  NtoClient *c = nto_find_client(client);
  if (!c) c = nto_alloc_client(client);
  if (c) {
    memcpy(c->assoc_bssid, bssid, 6);
    if (ssid) strncpy(c->assoc_ssid, ssid, 32);
    c->channel = ch;
    if (rssi < 0) c->rssi = rssi;
    c->has_assoc = true;
    c->last_ts = now_s;
  }

  NtoLink *l = nto_find_link(bssid, client);
  if (!l) l = nto_alloc_link(bssid, client);
  if (l) {
    l->state = NTO_LINK_ASSOC;
    l->last_assoc_ts = now_s;
    l->last_ts = now_s;
    l->frame_count++;
    if (rssi < 0) l->cl_rssi = rssi;
  }
}

void nto_ingest_data(const uint8_t *ta, const uint8_t *bssid,
                     uint8_t ch, int8_t rssi, uint16_t len) {
  if (!ta || !bssid || nto_is_bcast(ta) || nto_is_zero(bssid)) return;
  if (nto_mac_eq(ta, bssid)) return; // AP-to-AP

  uint32_t now_s = millis()/1000;

  NtoClient *c = nto_find_client(ta);
  if (!c) c = nto_alloc_client(ta);
  if (c) {
    if (!c->has_assoc) { memcpy(c->assoc_bssid, bssid, 6); }
    c->channel = ch;
    if (rssi < 0) c->rssi = rssi;
    c->frame_count++;
    c->last_ts = now_s;
  }

  NtoLink *l = nto_find_link(bssid, ta);
  if (!l) l = nto_alloc_link(bssid, ta);
  if (l) {
    if (l->state == NTO_LINK_PROBE) l->state = NTO_LINK_DATA;
    l->last_ts = now_s;
    l->frame_count++;
    if (rssi < 0) l->cl_rssi = rssi;
  }
}

static void nto_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String nto_json_topology() {
  String s = "{\"ap_count\":" + String(g_nto.ap_count);
  s += ",\"client_count\":" + String(g_nto.client_count);
  s += ",\"link_count\":" + String(g_nto.link_count);
  s += ",\"total_assocs\":" + String(g_nto.total_assocs);
  s += ",\"aps\":[";
  char mac[18]; bool first = true;
  for (int i = 0; i < NTO_MAX_APS; i++) {
    NtoAP &a = g_nto.aps[i];
    if (!a.active) continue;
    if (!first) s += ","; first = false;
    nto_mac_str(a.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(a.ssid) + "\"";
    s += ",\"ch\":" + String(a.channel);
    s += ",\"rssi\":" + String(a.rssi);
    s += ",\"hidden\":" + String(a.hidden?"true":"false");
    s += ",\"clients\":" + String(a.client_count);
    s += "}";
  }
  s += "],\"clients\":[";
  first = true;
  for (int i = 0; i < NTO_MAX_CLIENTS; i++) {
    NtoClient &c = g_nto.clients[i];
    if (!c.active) continue;
    if (!first) s += ","; first = false;
    nto_mac_str(c.mac, mac);
    s += "{\"mac\":\"" + String(mac) + "\"";
    char ap[18]; nto_mac_str(c.assoc_bssid, ap);
    s += ",\"ap\":\"" + String(ap) + "\"";
    s += ",\"ssid\":\"" + String(c.assoc_ssid) + "\"";
    s += ",\"ch\":" + String(c.channel);
    s += ",\"rssi\":" + String(c.rssi);
    s += ",\"frames\":" + String(c.frame_count);
    s += ",\"has_assoc\":" + String(c.has_assoc?"true":"false");
    s += "}";
  }
  s += "],\"links\":[";
  first = true;
  for (int i = 0; i < NTO_MAX_LINKS; i++) {
    NtoLink &l = g_nto.links[i];
    if (!l.active) continue;
    if (!first) s += ","; first = false;
    char ap[18], cl[18];
    nto_mac_str(l.ap_bssid, ap);
    nto_mac_str(l.client_mac, cl);
    s += "{\"ap\":\"" + String(ap) + "\"";
    s += ",\"client\":\"" + String(cl) + "\"";
    s += ",\"state\":" + String(l.state);
    s += ",\"frames\":" + String(l.frame_count);
    s += ",\"rssi\":" + String(l.cl_rssi);
    s += "}";
  }
  s += "]}";
  return s;
}

inline void nto_routes() {
  server.on("/api/nto/topology", HTTP_GET, []() {
    server.send(200, "application/json", nto_json_topology());
  });
  server.on("/api/nto/clear", HTTP_POST, []() {
    memset(&g_nto, 0, sizeof(g_nto));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

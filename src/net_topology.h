#pragma once
// net_topology.h — Network topology tracker.
// Builds and maintains a graph of observed AP-client associations,
// inter-AP relationships (same ESS/ESSID), roaming edges, and
// client multi-homing detection.
// Prefix: ntt_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define NTT_MAX_APS       32
#define NTT_MAX_CLIENTS   48
#define NTT_MAX_EDGES    128
#define NTT_MAX_ESS       16

// Edge type between nodes
enum NttEdgeType : uint8_t {
  NTT_EDGE_ASSOC     = 0,  // Client associated with AP
  NTT_EDGE_PROBE     = 1,  // Client sent directed probe to AP
  NTT_EDGE_ROAM      = 2,  // Client roamed from one AP to another
  NTT_EDGE_SAME_ESS  = 3,  // Two APs share same SSID (same ESS)
  NTT_EDGE_ADJACENT  = 4,  // Two APs on adjacent channels (interference)
};

static const char* ntt_edge_str(NttEdgeType t) {
  switch (t) {
    case NTT_EDGE_ASSOC:    return "assoc";
    case NTT_EDGE_PROBE:    return "probe";
    case NTT_EDGE_ROAM:     return "roam";
    case NTT_EDGE_SAME_ESS: return "same_ess";
    case NTT_EDGE_ADJACENT: return "adjacent";
  }
  return "unknown";
}

// Node type
enum NttNodeType : uint8_t {
  NTT_NODE_AP     = 0,
  NTT_NODE_CLIENT = 1,
};

struct NttNode {
  bool    active;
  NttNodeType type;
  uint8_t mac[6];
  char    ssid[33];         // For APs: SSID; for clients: last associated SSID
  uint8_t ch;
  int8_t  rssi;
  bool    is_random_mac;    // Client uses randomized MAC
  uint32_t first_ts;
  uint32_t last_ts;
  uint16_t frame_count;
  // AP-only: ESS ID (index into g_ntt.ess_names)
  uint8_t  ess_id;
  bool     has_ess;
};

struct NttEdge {
  bool    active;
  uint8_t src_mac[6];   // AP or client MAC
  uint8_t dst_mac[6];   // AP or client MAC
  NttEdgeType type;
  uint32_t weight;      // frame count or event count
  int8_t   rssi;
  uint32_t first_ts;
  uint32_t last_ts;
};

struct NttEss {
  bool    active;
  char    ssid[33];
  uint8_t ap_macs[8][6];  // Up to 8 BSSIDs for this SSID
  uint8_t ap_count;
  uint32_t first_ts;
  uint32_t last_ts;
};

static struct {
  NttNode  nodes[NTT_MAX_APS + NTT_MAX_CLIENTS];
  NttEdge  edges[NTT_MAX_EDGES];
  NttEss   ess[NTT_MAX_ESS];
  uint8_t  node_count;
  uint8_t  edge_count;
  uint8_t  ess_count;
  uint32_t total_assoc_events;
  uint32_t total_roam_events;
} g_ntt = {};

static bool ntt_mac_eq(const uint8_t *a, const uint8_t *b) { return memcmp(a,b,6)==0; }
static bool ntt_is_random(const uint8_t *m) { return (m[0] & 0x02) != 0; }

static NttNode* ntt_find_node(const uint8_t *mac) {
  for (int i = 0; i < NTT_MAX_APS + NTT_MAX_CLIENTS; i++)
    if (g_ntt.nodes[i].active && ntt_mac_eq(g_ntt.nodes[i].mac, mac))
      return &g_ntt.nodes[i];
  return nullptr;
}

static NttNode* ntt_alloc_node(const uint8_t *mac, NttNodeType type) {
  for (int i = 0; i < NTT_MAX_APS + NTT_MAX_CLIENTS; i++) {
    if (!g_ntt.nodes[i].active) {
      NttNode &n = g_ntt.nodes[i];
      memset(&n, 0, sizeof(n));
      n.active = true;
      n.type = type;
      memcpy(n.mac, mac, 6);
      n.is_random_mac = ntt_is_random(mac);
      n.first_ts = millis()/1000;
      g_ntt.node_count++;
      return &n;
    }
  }
  // Evict oldest client node
  uint32_t oldest = UINT32_MAX; int idx = 0;
  for (int i = 0; i < NTT_MAX_APS + NTT_MAX_CLIENTS; i++) {
    if (g_ntt.nodes[i].active && g_ntt.nodes[i].type == NTT_NODE_CLIENT &&
        g_ntt.nodes[i].last_ts < oldest) {
      oldest = g_ntt.nodes[i].last_ts; idx = i;
    }
  }
  memset(&g_ntt.nodes[idx], 0, sizeof(NttNode));
  g_ntt.nodes[idx].active = true;
  g_ntt.nodes[idx].type = type;
  memcpy(g_ntt.nodes[idx].mac, mac, 6);
  g_ntt.nodes[idx].is_random_mac = ntt_is_random(mac);
  g_ntt.nodes[idx].first_ts = millis()/1000;
  return &g_ntt.nodes[idx];
}

static NttEdge* ntt_find_edge(const uint8_t *src, const uint8_t *dst, NttEdgeType type) {
  for (int i = 0; i < NTT_MAX_EDGES; i++) {
    NttEdge &e = g_ntt.edges[i];
    if (e.active && e.type == type &&
        ntt_mac_eq(e.src_mac, src) && ntt_mac_eq(e.dst_mac, dst))
      return &e;
  }
  return nullptr;
}

static NttEdge* ntt_alloc_edge(const uint8_t *src, const uint8_t *dst, NttEdgeType type) {
  for (int i = 0; i < NTT_MAX_EDGES; i++) {
    if (!g_ntt.edges[i].active) {
      NttEdge &e = g_ntt.edges[i];
      memset(&e, 0, sizeof(e));
      e.active = true;
      memcpy(e.src_mac, src, 6);
      memcpy(e.dst_mac, dst, 6);
      e.type = type;
      e.weight = 1;
      e.first_ts = millis()/1000;
      g_ntt.edge_count++;
      return &e;
    }
  }
  uint32_t oldest = UINT32_MAX; int idx = 0;
  for (int i = 0; i < NTT_MAX_EDGES; i++)
    if (g_ntt.edges[i].last_ts < oldest) { oldest = g_ntt.edges[i].last_ts; idx = i; }
  memset(&g_ntt.edges[idx], 0, sizeof(NttEdge));
  g_ntt.edges[idx].active = true;
  memcpy(g_ntt.edges[idx].src_mac, src, 6);
  memcpy(g_ntt.edges[idx].dst_mac, dst, 6);
  g_ntt.edges[idx].type = type;
  g_ntt.edges[idx].weight = 1;
  g_ntt.edges[idx].first_ts = millis()/1000;
  return &g_ntt.edges[idx];
}

static void ntt_add_edge(const uint8_t *src, const uint8_t *dst, NttEdgeType type, int8_t rssi) {
  NttEdge *e = ntt_find_edge(src, dst, type);
  if (!e) e = ntt_alloc_edge(src, dst, type);
  if (!e) return;
  e->weight++;
  e->rssi = rssi;
  e->last_ts = millis()/1000;
}

static uint8_t ntt_get_or_create_ess(const char *ssid) {
  for (int i = 0; i < NTT_MAX_ESS; i++) {
    if (g_ntt.ess[i].active && strcmp(g_ntt.ess[i].ssid, ssid) == 0)
      return i;
  }
  for (int i = 0; i < NTT_MAX_ESS; i++) {
    if (!g_ntt.ess[i].active) {
      NttEss &es = g_ntt.ess[i];
      memset(&es, 0, sizeof(es));
      es.active = true;
      strncpy(es.ssid, ssid, 32);
      es.first_ts = millis()/1000;
      g_ntt.ess_count++;
      return i;
    }
  }
  return 0xFF;
}

static void ntt_add_ap_to_ess(uint8_t ess_id, const uint8_t *bssid) {
  if (ess_id == 0xFF || ess_id >= NTT_MAX_ESS) return;
  NttEss &es = g_ntt.ess[ess_id];
  for (int i = 0; i < es.ap_count; i++)
    if (ntt_mac_eq(es.ap_macs[i], bssid)) return;
  if (es.ap_count < 8) {
    memcpy(es.ap_macs[es.ap_count++], bssid, 6);
    es.last_ts = millis()/1000;
    // If more than one AP with same SSID, create SAME_ESS edges
    if (es.ap_count > 1) {
      for (int i = 0; i < es.ap_count - 1; i++) {
        ntt_add_edge(bssid, es.ap_macs[i], NTT_EDGE_SAME_ESS, 0);
        ntt_add_edge(es.ap_macs[i], bssid, NTT_EDGE_SAME_ESS, 0);
      }
    }
  }
}

void ntt_ingest_ap(const uint8_t *bssid, const char *ssid, uint8_t ch, int8_t rssi) {
  NttNode *n = ntt_find_node(bssid);
  if (!n) n = ntt_alloc_node(bssid, NTT_NODE_AP);
  if (!n) return;
  if (ssid && ssid[0]) strncpy(n->ssid, ssid, 32);
  n->ch = ch; n->rssi = rssi;
  n->frame_count++;
  n->last_ts = millis()/1000;

  if (ssid && ssid[0]) {
    uint8_t ess_id = ntt_get_or_create_ess(ssid);
    if (ess_id != 0xFF) {
      n->ess_id = ess_id;
      n->has_ess = true;
      ntt_add_ap_to_ess(ess_id, bssid);
    }
  }
}

void ntt_ingest_client(const uint8_t *client_mac, const uint8_t *ap_bssid,
                        const char *ssid, uint8_t ch, int8_t rssi) {
  NttNode *cn = ntt_find_node(client_mac);
  if (!cn) cn = ntt_alloc_node(client_mac, NTT_NODE_CLIENT);
  if (!cn) return;
  if (ssid) strncpy(cn->ssid, ssid, 32);
  cn->ch = ch; cn->rssi = rssi;
  cn->frame_count++;
  cn->last_ts = millis()/1000;

  if (ap_bssid) {
    NttNode *an = ntt_find_node(ap_bssid);
    if (!an) an = ntt_alloc_node(ap_bssid, NTT_NODE_AP);
    if (an) { an->ch = ch; an->last_ts = millis()/1000; }
    ntt_add_edge(client_mac, ap_bssid, NTT_EDGE_ASSOC, rssi);
    g_ntt.total_assoc_events++;
  }
}

void ntt_ingest_probe(const uint8_t *client_mac, const uint8_t *ap_bssid,
                       uint8_t ch, int8_t rssi) {
  NttNode *cn = ntt_find_node(client_mac);
  if (!cn) cn = ntt_alloc_node(client_mac, NTT_NODE_CLIENT);
  if (!cn) return;
  cn->last_ts = millis()/1000;
  if (ap_bssid) ntt_add_edge(client_mac, ap_bssid, NTT_EDGE_PROBE, rssi);
}

void ntt_ingest_roam(const uint8_t *client_mac, const uint8_t *old_ap,
                      const uint8_t *new_ap, int8_t rssi) {
  if (!old_ap || !new_ap) return;
  ntt_add_edge(old_ap, new_ap, NTT_EDGE_ROAM, rssi);
  ntt_add_edge(client_mac, new_ap, NTT_EDGE_ASSOC, rssi);
  g_ntt.total_roam_events++;
}

static void ntt_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String ntt_json_all() {
  char mac[18], mac2[18];
  String s = "{\"node_count\":" + String(g_ntt.node_count);
  s += ",\"edge_count\":" + String(g_ntt.edge_count);
  s += ",\"ess_count\":" + String(g_ntt.ess_count);
  s += ",\"total_assoc\":" + String(g_ntt.total_assoc_events);
  s += ",\"total_roam\":" + String(g_ntt.total_roam_events);

  s += ",\"nodes\":[";
  bool first = true;
  for (int i = 0; i < NTT_MAX_APS + NTT_MAX_CLIENTS; i++) {
    NttNode &n = g_ntt.nodes[i];
    if (!n.active) continue;
    if (!first) s += ","; first = false;
    ntt_mac_str(n.mac, mac);
    s += "{\"mac\":\"" + String(mac) + "\"";
    s += ",\"type\":" + String(n.type);
    s += ",\"ssid\":\"" + String(n.ssid) + "\"";
    s += ",\"ch\":" + String(n.ch);
    s += ",\"rssi\":" + String(n.rssi);
    s += ",\"rand\":" + String(n.is_random_mac ? "true" : "false");
    s += ",\"frames\":" + String(n.frame_count);
    s += ",\"last\":" + String(n.last_ts) + "}";
  }

  s += "],\"edges\":[";
  first = true;
  for (int i = 0; i < NTT_MAX_EDGES; i++) {
    NttEdge &e = g_ntt.edges[i];
    if (!e.active) continue;
    if (!first) s += ","; first = false;
    ntt_mac_str(e.src_mac, mac);
    ntt_mac_str(e.dst_mac, mac2);
    s += "{\"src\":\"" + String(mac) + "\",\"dst\":\"" + String(mac2) + "\"";
    s += ",\"type\":\"" + String(ntt_edge_str(e.type)) + "\"";
    s += ",\"w\":" + String(e.weight);
    s += ",\"rssi\":" + String(e.rssi) + "}";
  }

  s += "],\"ess\":[";
  first = true;
  for (int i = 0; i < NTT_MAX_ESS; i++) {
    NttEss &es = g_ntt.ess[i];
    if (!es.active) continue;
    if (!first) s += ","; first = false;
    s += "{\"ssid\":\"" + String(es.ssid) + "\",\"aps\":" + String(es.ap_count) + "}";
  }
  s += "]}";
  return s;
}

inline void ntt_routes() {
  server.on("/api/ntt/all", HTTP_GET, []() {
    server.send(200, "application/json", ntt_json_all());
  });
  server.on("/api/ntt/clear", HTTP_POST, []() {
    memset(&g_ntt, 0, sizeof(g_ntt));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

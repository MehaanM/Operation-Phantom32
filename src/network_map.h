#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <string.h>
#include <stdint.h>

// ── Network Map ───────────────────────────────────────────────────────────────
//
// Passive 802.11 topology mapper: tracks BSS-STA associations,
// builds an adjacency graph, and provides JSON output for
// visual network map rendering.

#define NM_MAX_NODES 128
#define NM_MAX_EDGES 256
#define NM_MAX_SSIDS 48

// ── Node type ─────────────────────────────────────────────────────────────────
enum NmNodeType : uint8_t {
  NM_NODE_AP = 0,
  NM_NODE_STA = 1,
  NM_NODE_MESH = 2,
  NM_NODE_ROGUE = 3,
};

static const char *nm_node_type_name(NmNodeType t) {
  switch (t) {
    case NM_NODE_AP:
      return "ap";
    case NM_NODE_STA:
      return "sta";
    case NM_NODE_MESH:
      return "mesh";
    case NM_NODE_ROGUE:
      return "rogue";
    default:
      return "unknown";
  }
}

// ── Node ─────────────────────────────────────────────────────────────────────
struct NmNode {
  uint8_t mac[6];
  NmNodeType type;
  char label[33];  // SSID for AP, vendor for STA
  uint8_t channel;
  int8_t rssi;
  uint8_t enc;  // CsEnc or WpEncType
  bool pmf;
  uint32_t first_seen_ms;
  uint32_t last_seen_ms;
  uint32_t frame_count;
  bool active;
  uint8_t vendor_cat;  // OufCat
};

// ── Edge (association or observed communication) ──────────────────────────────
struct NmEdge {
  uint8_t src[6];
  uint8_t dst[6];
  uint32_t frame_count;
  uint32_t last_seen_ms;
  bool is_assoc;  // confirmed association vs inferred
  int8_t rssi;
};

// ── SSID record ───────────────────────────────────────────────────────────────
struct NmSsid {
  char ssid[33];
  uint8_t ap_macs[8][6];
  uint8_t ap_count;
  uint8_t channels_bitmask;  // channels 1-13 as bits
};

// ── Map state ─────────────────────────────────────────────────────────────────
struct NmState {
  NmNode nodes[NM_MAX_NODES];
  uint8_t node_count;
  NmEdge edges[NM_MAX_EDGES];
  uint16_t edge_count;
  NmSsid ssids[NM_MAX_SSIDS];
  uint8_t ssid_count;
  uint32_t total_frames;
  uint32_t last_update_ms;
};

static NmState g_nm = {};

// ── Node upsert ───────────────────────────────────────────────────────────────
static NmNode *nm_node_upsert(const uint8_t mac[6], NmNodeType type) {
  for (uint8_t i = 0; i < g_nm.node_count; i++) {
    if (memcmp(g_nm.nodes[i].mac, mac, 6) == 0)
      return &g_nm.nodes[i];
  }
  if (g_nm.node_count >= NM_MAX_NODES) {
    // Evict least recently seen inactive node
    uint32_t oldest = g_nm.nodes[0].last_seen_ms;
    uint8_t oi = 0;
    for (uint8_t i = 1; i < g_nm.node_count; i++) {
      if (!g_nm.nodes[i].active && g_nm.nodes[i].last_seen_ms < oldest) {
        oldest = g_nm.nodes[i].last_seen_ms;
        oi = i;
      }
    }
    memset(&g_nm.nodes[oi], 0, sizeof(NmNode));
    memcpy(g_nm.nodes[oi].mac, mac, 6);
    g_nm.nodes[oi].type = type;
    g_nm.nodes[oi].first_seen_ms = millis();
    return &g_nm.nodes[oi];
  }
  NmNode *n = &g_nm.nodes[g_nm.node_count++];
  memset(n, 0, sizeof(*n));
  memcpy(n->mac, mac, 6);
  n->type = type;
  n->first_seen_ms = millis();
  n->active = true;
  return n;
}

// ── Edge upsert ───────────────────────────────────────────────────────────────
static NmEdge *nm_edge_upsert(const uint8_t src[6], const uint8_t dst[6]) {
  for (uint16_t i = 0; i < g_nm.edge_count; i++) {
    if (memcmp(g_nm.edges[i].src, src, 6) == 0 && memcmp(g_nm.edges[i].dst, dst, 6) == 0)
      return &g_nm.edges[i];
  }
  if (g_nm.edge_count >= NM_MAX_EDGES) {
    // Evict oldest edge
    uint32_t oldest = g_nm.edges[0].last_seen_ms;
    uint16_t oi = 0;
    for (uint16_t i = 1; i < g_nm.edge_count; i++) {
      if (g_nm.edges[i].last_seen_ms < oldest) {
        oldest = g_nm.edges[i].last_seen_ms;
        oi = i;
      }
    }
    memset(&g_nm.edges[oi], 0, sizeof(NmEdge));
    memcpy(g_nm.edges[oi].src, src, 6);
    memcpy(g_nm.edges[oi].dst, dst, 6);
    return &g_nm.edges[oi];
  }
  NmEdge *e = &g_nm.edges[g_nm.edge_count++];
  memset(e, 0, sizeof(*e));
  memcpy(e->src, src, 6);
  memcpy(e->dst, dst, 6);
  return e;
}

// ── SSID tracking ─────────────────────────────────────────────────────────────
static void nm_track_ssid(const char *ssid, const uint8_t bssid[6], uint8_t ch) {
  if (!ssid || !ssid[0])
    return;
  for (uint8_t i = 0; i < g_nm.ssid_count; i++) {
    if (strcmp(g_nm.ssids[i].ssid, ssid) == 0) {
      NmSsid &s = g_nm.ssids[i];
      bool found = false;
      for (uint8_t j = 0; j < s.ap_count; j++) {
        if (memcmp(s.ap_macs[j], bssid, 6) == 0) {
          found = true;
          break;
        }
      }
      if (!found && s.ap_count < 8)
        memcpy(s.ap_macs[s.ap_count++], bssid, 6);
      if (ch >= 1 && ch <= 13)
        s.channels_bitmask |= (1 << (ch - 1));
      return;
    }
  }
  if (g_nm.ssid_count < NM_MAX_SSIDS) {
    NmSsid &s = g_nm.ssids[g_nm.ssid_count++];
    strncpy(s.ssid, ssid, 32);
    s.ssid[32] = 0;
    memcpy(s.ap_macs[0], bssid, 6);
    s.ap_count = 1;
    if (ch >= 1 && ch <= 13)
      s.channels_bitmask = (1 << (ch - 1));
  }
}

// ── Ingest frame ─────────────────────────────────────────────────────────────
static void nm_ingest(const uint8_t *src, const uint8_t *dst, const uint8_t *bssid,
                      const char *ssid, uint8_t type, uint8_t stype, uint8_t channel, int8_t rssi) {
  g_nm.total_frames++;
  g_nm.last_update_ms = millis();

  bool is_bcast_src = src && (src[0] & 1);
  bool is_bcast_dst = dst && (dst[0] & 1);

  if (type == 0) {
    if (stype == 8) {  // Beacon
      if (bssid) {
        NmNode *n = nm_node_upsert(bssid, NM_NODE_AP);
        n->channel = channel;
        n->rssi = rssi;
        n->frame_count++;
        n->last_seen_ms = millis();
        if (ssid && ssid[0]) {
          strncpy(n->label, ssid, 32);
          n->label[32] = 0;
        }
        nm_track_ssid(ssid, bssid, channel);
      }
    }
    if (stype == 4) {  // Probe request
      if (src && !is_bcast_src) {
        NmNode *n = nm_node_upsert(src, NM_NODE_STA);
        n->rssi = rssi;
        n->frame_count++;
        n->last_seen_ms = millis();
      }
    }
    if (stype == 0 || stype == 2) {  // Assoc req/resp
      if (src && dst && !is_bcast_src && !is_bcast_dst) {
        NmNode *sn = nm_node_upsert(src, NM_NODE_STA);
        NmNode *dn = nm_node_upsert(dst, NM_NODE_AP);
        sn->last_seen_ms = millis();
        dn->last_seen_ms = millis();
        NmEdge *e = nm_edge_upsert(src, dst);
        e->frame_count++;
        e->last_seen_ms = millis();
        e->is_assoc = true;
        e->rssi = rssi;
      }
    }
    if (stype == 10 || stype == 12) {  // Disassoc / Deauth
      if (src && dst && bssid) {
        // Mark edge as potentially severed (don't remove, just track)
        NmEdge *e = nm_edge_upsert(src, dst);
        e->frame_count++;
        e->last_seen_ms = millis();
        e->rssi = rssi;
      }
    }
  }
  if (type == 2) {  // Data
    if (src && !is_bcast_src && bssid) {
      NmNode *sn = nm_node_upsert(src, NM_NODE_STA);
      sn->rssi = rssi;
      sn->frame_count++;
      sn->last_seen_ms = millis();
      sn->channel = channel;
      NmEdge *e = nm_edge_upsert(src, bssid);
      e->frame_count++;
      e->last_seen_ms = millis();
      e->rssi = rssi;
    }
  }
}

// ── Active node/edge aging ────────────────────────────────────────────────────
static void nm_age(uint32_t stale_ms) {
  uint32_t now = millis();
  for (uint8_t i = 0; i < g_nm.node_count; i++) {
    if (g_nm.nodes[i].active && now - g_nm.nodes[i].last_seen_ms > stale_ms) {
      g_nm.nodes[i].active = false;
    }
  }
}

// ── Node count by type ────────────────────────────────────────────────────────
static uint8_t nm_count_type(NmNodeType type) {
  uint8_t c = 0;
  for (uint8_t i = 0; i < g_nm.node_count; i++) {
    if (g_nm.nodes[i].type == type)
      c++;
  }
  return c;
}

// ── Nodes JSON ────────────────────────────────────────────────────────────────
static String nm_nodes_json(int page, int per) {
  if (per < 1 || per > 50)
    per = 25;
  int start = page * per, end = start + per;
  if (end > (int)g_nm.node_count)
    end = g_nm.node_count;
  String j = "{\"total\":" + String(g_nm.node_count);
  j += ",\"page\":" + String(page) + ",\"per\":" + String(per);
  j += ",\"nodes\":[";
  for (int i = start; i < end; i++) {
    if (i > start)
      j += ",";
    NmNode &n = g_nm.nodes[i];
    char mac[18];
    snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X", n.mac[0], n.mac[1], n.mac[2],
             n.mac[3], n.mac[4], n.mac[5]);
    j += "{\"mac\":\"";
    j += mac;
    j += "\"";
    j += ",\"type\":\"";
    j += nm_node_type_name(n.type);
    j += "\"";
    j += ",\"label\":\"";
    j += n.label;
    j += "\"";
    j += ",\"ch\":";
    j += n.channel;
    j += ",\"rssi\":";
    j += n.rssi;
    j += ",\"frames\":";
    j += n.frame_count;
    j += ",\"active\":";
    j += n.active ? 1 : 0;
    uint32_t age = (millis() - n.last_seen_ms) / 1000;
    j += ",\"age_s\":";
    j += age;
    j += "}";
  }
  j += "]}";
  return j;
}

// ── Edges JSON ────────────────────────────────────────────────────────────────
static String nm_edges_json(int page, int per) {
  if (per < 1 || per > 100)
    per = 50;
  int start = page * per, end = start + per;
  if (end > (int)g_nm.edge_count)
    end = g_nm.edge_count;
  String j = "{\"total\":" + String(g_nm.edge_count);
  j += ",\"edges\":[";
  for (int i = start; i < end; i++) {
    if (i > start)
      j += ",";
    NmEdge &e = g_nm.edges[i];
    char s[18], d[18];
    snprintf(s, sizeof(s), "%02X:%02X:%02X:%02X:%02X:%02X", e.src[0], e.src[1], e.src[2], e.src[3],
             e.src[4], e.src[5]);
    snprintf(d, sizeof(d), "%02X:%02X:%02X:%02X:%02X:%02X", e.dst[0], e.dst[1], e.dst[2], e.dst[3],
             e.dst[4], e.dst[5]);
    j += "{\"src\":\"";
    j += s;
    j += "\"";
    j += ",\"dst\":\"";
    j += d;
    j += "\"";
    j += ",\"frames\":";
    j += e.frame_count;
    j += ",\"assoc\":";
    j += e.is_assoc ? 1 : 0;
    j += ",\"rssi\":";
    j += e.rssi;
    j += "}";
  }
  j += "]}";
  return j;
}

// ── SSID list JSON ────────────────────────────────────────────────────────────
static String nm_ssids_json() {
  String j = "{\"count\":" + String(g_nm.ssid_count) + ",\"ssids\":[";
  for (uint8_t i = 0; i < g_nm.ssid_count; i++) {
    if (i)
      j += ",";
    NmSsid &s = g_nm.ssids[i];
    j += "{\"ssid\":\"";
    j += s.ssid;
    j += "\"";
    j += ",\"ap_count\":";
    j += s.ap_count;
    j += ",\"chans\":";
    j += s.channels_bitmask;
    j += "}";
  }
  j += "]}";
  return j;
}

// ── Summary JSON ──────────────────────────────────────────────────────────────
static String nm_summary_json() {
  String j = "{\"nodes\":" + String(g_nm.node_count);
  j += ",\"edges\":" + String(g_nm.edge_count);
  j += ",\"ssids\":" + String(g_nm.ssid_count);
  j += ",\"aps\":" + String(nm_count_type(NM_NODE_AP));
  j += ",\"stas\":" + String(nm_count_type(NM_NODE_STA));
  j += ",\"frames\":" + String(g_nm.total_frames);
  j += "}";
  return j;
}

// ── HTTP routes ───────────────────────────────────────────────────────────────
static void nm_routes() {
  extern WebServer server;

  server.on("/api/nm/summary", HTTP_GET,
            []() { server.send(200, "application/json", nm_summary_json()); });

  server.on("/api/nm/nodes", HTTP_GET, []() {
    int page = server.hasArg("page") ? server.arg("page").toInt() : 0;
    int per = server.hasArg("per") ? server.arg("per").toInt() : 25;
    server.send(200, "application/json", nm_nodes_json(page, per));
  });

  server.on("/api/nm/edges", HTTP_GET, []() {
    int page = server.hasArg("page") ? server.arg("page").toInt() : 0;
    int per = server.hasArg("per") ? server.arg("per").toInt() : 50;
    server.send(200, "application/json", nm_edges_json(page, per));
  });

  server.on("/api/nm/ssids", HTTP_GET,
            []() { server.send(200, "application/json", nm_ssids_json()); });

  server.on("/api/nm/age", HTTP_POST, []() {
    uint32_t ms = server.hasArg("ms") ? server.arg("ms").toInt() : 30000;
    nm_age(ms);
    server.send(200, "application/json", "{\"ok\":1}");
  });

  server.on("/api/nm/clear", HTTP_POST, []() {
    memset(&g_nm, 0, sizeof(g_nm));
    server.send(200, "application/json", "{\"ok\":1}");
  });
}

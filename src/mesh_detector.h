#pragma once
// mesh_detector.h — 802.11s mesh network detector.
// Identifies mesh network nodes via MBSS/MESH IEs, 802.11s action frames,
// peering state, and mesh paths. Builds network topology from observed frames.
// Prefix: msh_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define MSH_MAX_NODES 32
#define MSH_MAX_LINKS 64
#define MSH_MAX_PATHS 32
#define MSH_HIST_LEN 60

// Mesh peering states (from IEEE 802.11-2020 s11.3.3)
enum MshPeerState : uint8_t {
  MSH_PEER_IDLE = 0,
  MSH_PEER_OPN_SNT = 1,   // Open sent
  MSH_PEER_CNF_RCVD = 2,  // Confirm received
  MSH_PEER_OPN_RCVD = 3,  // Open received
  MSH_PEER_ESTAB = 4,     // Established
  MSH_PEER_HOLDING = 5    // Holding
};

// 802.11s mesh action subtypes
enum MshActionType : uint8_t {
  MSH_ACT_LINK_MET_REP = 0,  // Link Metric Report
  MSH_ACT_HWMP_PREQ = 1,     // HWMP Path Request
  MSH_ACT_HWMP_PREP = 2,     // HWMP Path Reply
  MSH_ACT_HWMP_PERR = 3,     // HWMP Path Error
  MSH_ACT_HWMP_RANN = 4,     // Root Announcement
  MSH_ACT_GATE_ANN = 5       // Gate Announcement
};

struct MshLink {
  bool active;
  uint8_t node_a[6];
  uint8_t node_b[6];
  MshPeerState state;
  uint16_t metric;  // 802.11s airtime link metric
  int8_t rssi;
  uint32_t established_ts;
  uint32_t last_ts;
  uint16_t preq_count;
  uint16_t prep_count;
  uint16_t perr_count;
  uint16_t rann_count;
};

struct MshPath {
  bool active;
  uint8_t src[6];
  uint8_t dst[6];
  uint8_t nexthop[6];
  uint8_t hop_count;
  uint32_t metric;
  uint32_t seq;
  uint32_t last_ts;
  bool to_root;
};

struct MshNode {
  bool active;
  uint8_t addr[6];
  char ssid[33];  // mesh ID (SSID equivalent)
  uint8_t ch;
  int8_t rssi;
  bool is_root;      // announced as mesh root (gate)
  bool is_gate;      // has portal to external network
  uint8_t ttl;       // current TTL from frames
  uint16_t seq_num;  // HWMP sequence number
  MshPeerState peer_state;
  uint8_t link_count;  // number of active links
  uint8_t path_count;

  // Capability flags
  bool accepts_peer;
  bool mcca_support;
  bool forwarding;
  uint8_t num_peerings;

  // Traffic stats
  uint32_t data_frames;
  uint32_t mgmt_frames;
  uint32_t action_frames;

  // Activity history
  uint32_t pkt_hist[MSH_HIST_LEN];
  uint8_t hist_head;
  uint32_t cur_pkts;
  uint32_t last_tick_s;

  uint32_t first_ts;
  uint32_t last_ts;
};

static struct {
  MshNode nodes[MSH_MAX_NODES];
  MshLink links[MSH_MAX_LINKS];
  MshPath paths[MSH_MAX_PATHS];
  uint8_t node_count;
  uint8_t link_count;
  uint8_t path_count;
} g_msh = {};

static bool msh_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static MshNode *msh_find_node(const uint8_t *addr) {
  for (int i = 0; i < MSH_MAX_NODES; i++)
    if (g_msh.nodes[i].active && msh_mac_eq(g_msh.nodes[i].addr, addr))
      return &g_msh.nodes[i];
  return nullptr;
}

static MshNode *msh_alloc_node(const uint8_t *addr) {
  for (int i = 0; i < MSH_MAX_NODES; i++) {
    if (!g_msh.nodes[i].active) {
      MshNode &n = g_msh.nodes[i];
      memset(&n, 0, sizeof(n));
      n.active = true;
      memcpy(n.addr, addr, 6);
      n.forwarding = true;
      n.first_ts = millis() / 1000;
      g_msh.node_count++;
      return &n;
    }
  }
  uint32_t oldest = UINT32_MAX;
  int idx = 0;
  for (int i = 0; i < MSH_MAX_NODES; i++)
    if (!g_msh.nodes[i].is_root && g_msh.nodes[i].last_ts < oldest) {
      oldest = g_msh.nodes[i].last_ts;
      idx = i;
    }
  memset(&g_msh.nodes[idx], 0, sizeof(MshNode));
  g_msh.nodes[idx].active = true;
  memcpy(g_msh.nodes[idx].addr, addr, 6);
  g_msh.nodes[idx].first_ts = millis() / 1000;
  return &g_msh.nodes[idx];
}

static void msh_node_tick(MshNode &n) {
  uint32_t now_s = millis() / 1000;
  if (now_s == n.last_tick_s)
    return;
  n.pkt_hist[n.hist_head % MSH_HIST_LEN] = n.cur_pkts;
  n.hist_head = (n.hist_head + 1) % MSH_HIST_LEN;
  n.last_tick_s = now_s;
  n.cur_pkts = 0;
}

static MshLink *msh_find_link(const uint8_t *a, const uint8_t *b) {
  for (int i = 0; i < MSH_MAX_LINKS; i++) {
    MshLink &l = g_msh.links[i];
    if (!l.active)
      continue;
    if ((msh_mac_eq(l.node_a, a) && msh_mac_eq(l.node_b, b)) ||
        (msh_mac_eq(l.node_a, b) && msh_mac_eq(l.node_b, a)))
      return &l;
  }
  return nullptr;
}

static MshLink *msh_alloc_link(const uint8_t *a, const uint8_t *b) {
  for (int i = 0; i < MSH_MAX_LINKS; i++) {
    if (!g_msh.links[i].active) {
      MshLink &l = g_msh.links[i];
      memset(&l, 0, sizeof(l));
      l.active = true;
      memcpy(l.node_a, a, 6);
      memcpy(l.node_b, b, 6);
      l.state = MSH_PEER_OPN_SNT;
      l.established_ts = millis() / 1000;
      g_msh.link_count++;
      return &l;
    }
  }
  uint32_t oldest = UINT32_MAX;
  int idx = 0;
  for (int i = 0; i < MSH_MAX_LINKS; i++)
    if (g_msh.links[i].state != MSH_PEER_ESTAB && g_msh.links[i].last_ts < oldest) {
      oldest = g_msh.links[i].last_ts;
      idx = i;
    }
  memset(&g_msh.links[idx], 0, sizeof(MshLink));
  g_msh.links[idx].active = true;
  memcpy(g_msh.links[idx].node_a, a, 6);
  memcpy(g_msh.links[idx].node_b, b, 6);
  g_msh.links[idx].established_ts = millis() / 1000;
  return &g_msh.links[idx];
}

// Ingest a mesh beacon or mesh action frame
void msh_ingest_node(const uint8_t *addr, const char *mesh_id, uint8_t ch, int8_t rssi,
                     bool is_root, bool is_gate, bool accepts_peer, bool forwarding,
                     uint8_t num_peers) {
  MshNode *n = msh_find_node(addr);
  if (!n)
    n = msh_alloc_node(addr);
  if (!n)
    return;

  if (mesh_id && mesh_id[0])
    strncpy(n->ssid, mesh_id, 32);
  n->ch = ch;
  n->rssi = rssi;
  n->is_root = is_root;
  n->is_gate = is_gate;
  n->accepts_peer = accepts_peer;
  n->forwarding = forwarding;
  n->num_peerings = num_peers;
  n->last_ts = millis() / 1000;
  n->mgmt_frames++;
  msh_node_tick(*n);
  n->cur_pkts++;
}

// Ingest a mesh peering action frame
void msh_ingest_peer(const uint8_t *src, const uint8_t *dst, MshPeerState state, uint16_t metric,
                     int8_t rssi) {
  MshNode *ns = msh_find_node(src);
  if (!ns)
    ns = msh_alloc_node(src);
  MshNode *nd = msh_find_node(dst);
  if (!nd)
    nd = msh_alloc_node(dst);

  if (ns) {
    ns->peer_state = state;
    ns->last_ts = millis() / 1000;
  }
  if (nd) {
    nd->last_ts = millis() / 1000;
  }

  MshLink *l = msh_find_link(src, dst);
  if (!l)
    l = msh_alloc_link(src, dst);
  if (l) {
    l->state = state;
    l->metric = metric;
    l->rssi = rssi;
    l->last_ts = millis() / 1000;
  }
}

// Ingest HWMP path frames (PREQ/PREP/PERR/RANN)
void msh_ingest_hwmp(MshActionType act, const uint8_t *src, const uint8_t *target, uint8_t hops,
                     uint32_t metric, uint32_t seq) {
  MshNode *n = msh_find_node(src);
  if (!n)
    n = msh_alloc_node(src);
  if (n) {
    n->action_frames++;
    n->last_ts = millis() / 1000;
  }

  if (act == MSH_ACT_HWMP_RANN || act == MSH_ACT_GATE_ANN) {
    if (n) {
      n->is_root = true;
      n->seq_num = seq & 0xFFFF;
    }
    return;
  }

  if (target && (act == MSH_ACT_HWMP_PREP)) {
    MshPath *p = nullptr;
    for (int i = 0; i < MSH_MAX_PATHS; i++) {
      MshPath &mp = g_msh.paths[i];
      if (mp.active && msh_mac_eq(mp.src, src) && msh_mac_eq(mp.dst, target)) {
        p = &mp;
        break;
      }
    }
    if (!p) {
      for (int i = 0; i < MSH_MAX_PATHS; i++) {
        if (!g_msh.paths[i].active) {
          p = &g_msh.paths[i];
          memset(p, 0, sizeof(MshPath));
          p->active = true;
          memcpy(p->src, src, 6);
          memcpy(p->dst, target, 6);
          g_msh.path_count++;
          break;
        }
      }
    }
    if (p) {
      p->hop_count = hops;
      p->metric = metric;
      p->seq = seq;
      p->last_ts = millis() / 1000;
    }
  }
}

static const char *msh_peer_state_str(MshPeerState s) {
  switch (s) {
    case MSH_PEER_IDLE:
      return "Idle";
    case MSH_PEER_OPN_SNT:
      return "Open Sent";
    case MSH_PEER_CNF_RCVD:
      return "Confirm";
    case MSH_PEER_OPN_RCVD:
      return "Open Rcvd";
    case MSH_PEER_ESTAB:
      return "Established";
    case MSH_PEER_HOLDING:
      return "Holding";
  }
  return "Unknown";
}

static void msh_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String msh_hist_json(const MshNode &n) {
  String s = "[";
  for (int i = 0; i < MSH_HIST_LEN; i++) {
    int idx = (n.hist_head + i) % MSH_HIST_LEN;
    if (i)
      s += ",";
    s += String(n.pkt_hist[idx]);
  }
  return s + "]";
}

static String msh_json_all() {
  String s = "{\"node_count\":" + String(g_msh.node_count);
  s += ",\"link_count\":" + String(g_msh.link_count);
  s += ",\"path_count\":" + String(g_msh.path_count);
  s += ",\"nodes\":[";
  char mac[18];
  bool first = true;
  for (int i = 0; i < MSH_MAX_NODES; i++) {
    MshNode &n = g_msh.nodes[i];
    if (!n.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    msh_mac_str(n.addr, mac);
    s += "{\"addr\":\"" + String(mac) + "\"";
    s += ",\"mesh_id\":\"" + String(n.ssid) + "\"";
    s += ",\"ch\":" + String(n.ch);
    s += ",\"rssi\":" + String(n.rssi);
    s += ",\"is_root\":" + String(n.is_root ? "true" : "false");
    s += ",\"is_gate\":" + String(n.is_gate ? "true" : "false");
    s += ",\"forwarding\":" + String(n.forwarding ? "true" : "false");
    s += ",\"num_peerings\":" + String(n.num_peerings);
    s += ",\"peer_state\":\"" + String(msh_peer_state_str(n.peer_state)) + "\"";
    s += ",\"data\":" + String(n.data_frames);
    s += ",\"mgmt\":" + String(n.mgmt_frames);
    s += ",\"actions\":" + String(n.action_frames);
    s += ",\"hist\":" + msh_hist_json(n) + "}";
  }
  s += "],\"links\":[";
  char mac2[18];
  first = true;
  for (int i = 0; i < MSH_MAX_LINKS; i++) {
    MshLink &l = g_msh.links[i];
    if (!l.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    msh_mac_str(l.node_a, mac);
    msh_mac_str(l.node_b, mac2);
    s += "{\"a\":\"" + String(mac) + "\",\"b\":\"" + String(mac2) + "\"";
    s += ",\"state\":\"" + String(msh_peer_state_str(l.state)) + "\"";
    s += ",\"metric\":" + String(l.metric);
    s += ",\"rssi\":" + String(l.rssi) + "}";
  }
  s += "]}";
  return s;
}

inline void msh_routes() {
  server.on("/api/msh/all", HTTP_GET,
            []() { server.send(200, "application/json", msh_json_all()); });
  server.on("/api/msh/clear", HTTP_POST, []() {
    memset(&g_msh, 0, sizeof(g_msh));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

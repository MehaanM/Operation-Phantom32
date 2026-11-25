#pragma once
// traffic_classifier.h — 802.11 frame traffic classifier.
// Breaks down observed frames by type, subtype, source, destination,
// and infers application protocol from patterns (DNS, DHCP, ARP, HTTP, etc.).
// Prefix: tfc_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define TFC_MAX_FLOWS 64
#define TFC_HIST_LEN 60
#define TFC_MAX_PROTO 16

// Traffic classification categories
enum TfcProto : uint8_t {
  TFC_PROTO_UNKNOWN = 0,
  TFC_PROTO_DNS = 1,
  TFC_PROTO_DHCP = 2,
  TFC_PROTO_ARP = 3,
  TFC_PROTO_HTTP = 4,
  TFC_PROTO_HTTPS = 5,
  TFC_PROTO_MDNS = 6,
  TFC_PROTO_NTP = 7,
  TFC_PROTO_IGMP = 8,
  TFC_PROTO_EAPOL = 9,
  TFC_PROTO_MULTICAST = 10,
  TFC_PROTO_BROADCAST = 11,
  TFC_PROTO_DATA = 12,
  TFC_PROTO_MGMT = 13,
  TFC_PROTO_CTRL = 14,
  TFC_PROTO_QOS = 15
};

static const char *TFC_PROTO_NAMES[] = {
    "Unknown", "DNS",   "DHCP",      "ARP",       "HTTP", "HTTPS", "mDNS", "NTP",
    "IGMP",    "EAPOL", "Multicast", "Broadcast", "Data", "Mgmt",  "Ctrl", "QoS"};

// Per-protocol stats
struct TfcProtoStat {
  uint32_t packets;
  uint32_t bytes;
  uint32_t last_ts;
};

// Per-flow record (src MAC → dst MAC + proto)
struct TfcFlow {
  bool active;
  uint8_t src[6];
  uint8_t dst[6];
  TfcProto proto;
  uint8_t ch;
  int8_t rssi;
  uint32_t packets;
  uint32_t bytes;
  uint32_t first_ts;
  uint32_t last_ts;
};

// Frame type breakdown counters
struct TfcFrameBreakdown {
  uint32_t mgmt_total;
  uint32_t mgmt_beacon;
  uint32_t mgmt_probe_req;
  uint32_t mgmt_probe_rsp;
  uint32_t mgmt_auth;
  uint32_t mgmt_assoc;
  uint32_t mgmt_deauth;
  uint32_t mgmt_action;
  uint32_t ctrl_total;
  uint32_t ctrl_ack;
  uint32_t ctrl_rts;
  uint32_t ctrl_cts;
  uint32_t ctrl_block_ack;
  uint32_t data_total;
  uint32_t data_null;
  uint32_t data_qos;
  uint32_t data_eapol;
  uint32_t data_arp;
  uint32_t data_dhcp;
  uint32_t data_dns;
  uint32_t data_mdns;
  uint32_t data_ntp;
  uint32_t data_igmp;
  uint32_t data_multicast;
  uint32_t data_broadcast;
  uint32_t data_unicast;
  uint64_t total_bytes;
  uint32_t total_packets;
};

static struct {
  TfcFlow flows[TFC_MAX_FLOWS];
  uint16_t flow_count;
  TfcProtoStat protos[TFC_MAX_PROTO];
  TfcFrameBreakdown breakdown;
  uint32_t pkt_hist[TFC_HIST_LEN];
  uint32_t byt_hist[TFC_HIST_LEN];
  uint8_t hist_head;
  uint32_t cur_pkts;
  uint32_t cur_bytes;
  uint32_t last_tick_s;
} g_tfc = {};

static bool tfc_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static void tfc_tick() {
  uint32_t now_s = millis() / 1000;
  if (now_s == g_tfc.last_tick_s)
    return;
  g_tfc.pkt_hist[g_tfc.hist_head % TFC_HIST_LEN] = g_tfc.cur_pkts;
  g_tfc.byt_hist[g_tfc.hist_head % TFC_HIST_LEN] = g_tfc.cur_bytes;
  g_tfc.hist_head = (g_tfc.hist_head + 1) % TFC_HIST_LEN;
  g_tfc.last_tick_s = now_s;
  g_tfc.cur_pkts = g_tfc.cur_bytes = 0;
}

static TfcFlow *tfc_find_flow(const uint8_t *src, const uint8_t *dst, TfcProto proto) {
  for (int i = 0; i < TFC_MAX_FLOWS; i++) {
    TfcFlow &f = g_tfc.flows[i];
    if (f.active && tfc_mac_eq(f.src, src) && tfc_mac_eq(f.dst, dst) && f.proto == proto)
      return &f;
  }
  return nullptr;
}

static TfcFlow *tfc_alloc_flow(const uint8_t *src, const uint8_t *dst, TfcProto proto) {
  for (int i = 0; i < TFC_MAX_FLOWS; i++) {
    if (!g_tfc.flows[i].active) {
      TfcFlow &f = g_tfc.flows[i];
      memset(&f, 0, sizeof(f));
      f.active = true;
      memcpy(f.src, src, 6);
      memcpy(f.dst, dst, 6);
      f.proto = proto;
      f.first_ts = millis() / 1000;
      g_tfc.flow_count++;
      return &f;
    }
  }
  // LRU evict
  uint32_t oldest = UINT32_MAX;
  int idx = 0;
  for (int i = 0; i < TFC_MAX_FLOWS; i++)
    if (g_tfc.flows[i].last_ts < oldest) {
      oldest = g_tfc.flows[i].last_ts;
      idx = i;
    }
  memset(&g_tfc.flows[idx], 0, sizeof(TfcFlow));
  g_tfc.flows[idx].active = true;
  memcpy(g_tfc.flows[idx].src, src, 6);
  memcpy(g_tfc.flows[idx].dst, dst, 6);
  g_tfc.flows[idx].proto = proto;
  g_tfc.flows[idx].first_ts = millis() / 1000;
  return &g_tfc.flows[idx];
}

// Ingest a management frame
void tfc_ingest_mgmt(uint8_t subtype, const uint8_t *src, const uint8_t *dst, uint8_t ch,
                     int8_t rssi, uint16_t len) {
  tfc_tick();
  g_tfc.breakdown.mgmt_total++;
  g_tfc.breakdown.total_packets++;
  g_tfc.breakdown.total_bytes += len;
  g_tfc.cur_pkts++;
  g_tfc.cur_bytes += len;
  g_tfc.protos[TFC_PROTO_MGMT].packets++;
  g_tfc.protos[TFC_PROTO_MGMT].bytes += len;
  g_tfc.protos[TFC_PROTO_MGMT].last_ts = millis() / 1000;

  switch (subtype & 0x0F) {
    case 0x08:
      g_tfc.breakdown.mgmt_beacon++;
      break;
    case 0x04:
      g_tfc.breakdown.mgmt_probe_req++;
      break;
    case 0x05:
      g_tfc.breakdown.mgmt_probe_rsp++;
      break;
    case 0x0B:
      g_tfc.breakdown.mgmt_auth++;
      break;
    case 0x00:
      g_tfc.breakdown.mgmt_assoc++;
      break;
    case 0x0C:
      g_tfc.breakdown.mgmt_deauth++;
      break;
    case 0x0D:
      g_tfc.breakdown.mgmt_action++;
      break;
  }
}

// Ingest a control frame
void tfc_ingest_ctrl(uint8_t subtype, uint8_t ch, int8_t rssi, uint16_t len) {
  tfc_tick();
  g_tfc.breakdown.ctrl_total++;
  g_tfc.breakdown.total_packets++;
  g_tfc.breakdown.total_bytes += len;
  g_tfc.cur_pkts++;
  g_tfc.cur_bytes += len;
  g_tfc.protos[TFC_PROTO_CTRL].packets++;
  g_tfc.protos[TFC_PROTO_CTRL].bytes += len;

  switch (subtype & 0x0F) {
    case 0x0D:
      g_tfc.breakdown.ctrl_ack++;
      break;
    case 0x0B:
      g_tfc.breakdown.ctrl_rts++;
      break;
    case 0x0C:
      g_tfc.breakdown.ctrl_cts++;
      break;
    case 0x09:
      g_tfc.breakdown.ctrl_block_ack++;
      break;
  }
}

// Ingest a data frame with protocol classification
void tfc_ingest_data(TfcProto proto, const uint8_t *src, const uint8_t *dst, uint8_t ch,
                     int8_t rssi, uint16_t len, bool qos) {
  tfc_tick();
  g_tfc.breakdown.data_total++;
  g_tfc.breakdown.total_packets++;
  g_tfc.breakdown.total_bytes += len;
  g_tfc.cur_pkts++;
  g_tfc.cur_bytes += len;
  if (qos)
    g_tfc.breakdown.data_qos++;

  switch (proto) {
    case TFC_PROTO_ARP:
      g_tfc.breakdown.data_arp++;
      break;
    case TFC_PROTO_DHCP:
      g_tfc.breakdown.data_dhcp++;
      break;
    case TFC_PROTO_DNS:
      g_tfc.breakdown.data_dns++;
      break;
    case TFC_PROTO_MDNS:
      g_tfc.breakdown.data_mdns++;
      break;
    case TFC_PROTO_NTP:
      g_tfc.breakdown.data_ntp++;
      break;
    case TFC_PROTO_IGMP:
      g_tfc.breakdown.data_igmp++;
      break;
    case TFC_PROTO_EAPOL:
      g_tfc.breakdown.data_eapol++;
      break;
    case TFC_PROTO_MULTICAST:
      g_tfc.breakdown.data_multicast++;
      break;
    case TFC_PROTO_BROADCAST:
      g_tfc.breakdown.data_broadcast++;
      break;
    default:
      g_tfc.breakdown.data_unicast++;
      break;
  }

  if (proto < TFC_MAX_PROTO) {
    g_tfc.protos[proto].packets++;
    g_tfc.protos[proto].bytes += len;
    g_tfc.protos[proto].last_ts = millis() / 1000;
  }

  TfcFlow *f = tfc_find_flow(src, dst, proto);
  if (!f)
    f = tfc_alloc_flow(src, dst, proto);
  if (f) {
    f->ch = ch;
    f->rssi = rssi;
    f->packets++;
    f->bytes += len;
    f->last_ts = millis() / 1000;
  }
}

static void tfc_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String tfc_hist_json(const uint32_t *hist, int len, int head) {
  String s = "[";
  for (int i = 0; i < len; i++) {
    int idx = (head + i) % len;
    if (i)
      s += ",";
    s += String(hist[idx]);
  }
  return s + "]";
}

static String tfc_json_all() {
  TfcFrameBreakdown &b = g_tfc.breakdown;
  String s = "{";
  s += "\"total_packets\":" + String(b.total_packets);
  s += ",\"total_bytes\":" + String((uint32_t)b.total_bytes);
  s += ",\"mgmt_total\":" + String(b.mgmt_total);
  s += ",\"mgmt_beacon\":" + String(b.mgmt_beacon);
  s += ",\"mgmt_probe_req\":" + String(b.mgmt_probe_req);
  s += ",\"mgmt_probe_rsp\":" + String(b.mgmt_probe_rsp);
  s += ",\"mgmt_auth\":" + String(b.mgmt_auth);
  s += ",\"mgmt_assoc\":" + String(b.mgmt_assoc);
  s += ",\"mgmt_deauth\":" + String(b.mgmt_deauth);
  s += ",\"ctrl_total\":" + String(b.ctrl_total);
  s += ",\"ctrl_ack\":" + String(b.ctrl_ack);
  s += ",\"ctrl_rts\":" + String(b.ctrl_rts);
  s += ",\"ctrl_cts\":" + String(b.ctrl_cts);
  s += ",\"ctrl_block_ack\":" + String(b.ctrl_block_ack);
  s += ",\"data_total\":" + String(b.data_total);
  s += ",\"data_null\":" + String(b.data_null);
  s += ",\"data_qos\":" + String(b.data_qos);
  s += ",\"data_eapol\":" + String(b.data_eapol);
  s += ",\"data_arp\":" + String(b.data_arp);
  s += ",\"data_dhcp\":" + String(b.data_dhcp);
  s += ",\"data_dns\":" + String(b.data_dns);
  s += ",\"data_mdns\":" + String(b.data_mdns);
  s += ",\"data_multicast\":" + String(b.data_multicast);
  s += ",\"data_broadcast\":" + String(b.data_broadcast);
  s += ",\"data_unicast\":" + String(b.data_unicast);
  s += ",\"pkt_hist\":" + tfc_hist_json(g_tfc.pkt_hist, TFC_HIST_LEN, g_tfc.hist_head);
  s += ",\"byt_hist\":" + tfc_hist_json(g_tfc.byt_hist, TFC_HIST_LEN, g_tfc.hist_head);
  s += ",\"protos\":[";
  bool fp = true;
  for (int i = 0; i < TFC_MAX_PROTO; i++) {
    if (g_tfc.protos[i].packets == 0)
      continue;
    if (!fp)
      s += ",";
    fp = false;
    s += "{\"name\":\"" + String(TFC_PROTO_NAMES[i]) + "\"";
    s += ",\"packets\":" + String(g_tfc.protos[i].packets);
    s += ",\"bytes\":" + String(g_tfc.protos[i].bytes) + "}";
  }
  s += "],\"flows\":[";
  char mac[18];
  bool ff = true;
  for (int i = 0; i < TFC_MAX_FLOWS; i++) {
    TfcFlow &f = g_tfc.flows[i];
    if (!f.active)
      continue;
    if (!ff)
      s += ",";
    ff = false;
    tfc_mac_str(f.src, mac);
    s += "{\"src\":\"" + String(mac) + "\"";
    tfc_mac_str(f.dst, mac);
    s += ",\"dst\":\"" + String(mac) + "\"";
    s += ",\"proto\":\"" + String(f.proto < TFC_MAX_PROTO ? TFC_PROTO_NAMES[f.proto] : "?") + "\"";
    s += ",\"ch\":" + String(f.ch);
    s += ",\"rssi\":" + String(f.rssi);
    s += ",\"packets\":" + String(f.packets);
    s += ",\"bytes\":" + String(f.bytes) + "}";
  }
  s += "]}";
  return s;
}

inline void tfc_routes() {
  server.on("/api/tfc/all", HTTP_GET,
            []() { server.send(200, "application/json", tfc_json_all()); });
  server.on("/api/tfc/clear", HTTP_POST, []() {
    memset(&g_tfc, 0, sizeof(g_tfc));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

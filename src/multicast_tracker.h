#pragma once
// multicast_tracker.h — 802.11 multicast and broadcast frame tracker.
// Monitors multicast groups (IGMP/MLD), broadcast storms, multicast-to-unicast
// conversion, and mDNS/UPnP/NTP multicast traffic patterns.
// Prefix: mct_

#include <Arduino.h>
#include <WebServer.h>
#include "psram_attr.h"

extern WebServer server;

#define MCT_MAX_GROUPS  32
#define MCT_MAX_SOURCES  8
#define MCT_HIST_LEN    30

// Multicast group type
enum MctGroupType : uint8_t {
  MCT_GRP_UNKNOWN  = 0,
  MCT_GRP_MDNS     = 1,  // 224.0.0.251 / ff02::fb
  MCT_GRP_SSDP     = 2,  // 239.255.255.250 (UPnP)
  MCT_GRP_NTP      = 3,  // 224.0.1.1 (NTP broadcasts)
  MCT_GRP_DHCP_SRV = 4,  // 224.0.0.0/8 DHCP server
  MCT_GRP_IGMP     = 5,  // Generic IGMP multicast
  MCT_GRP_BROADCAST= 6,  // 802.11 broadcast (ff:ff:ff:ff:ff:ff)
  MCT_GRP_ARP      = 7,  // ARP broadcast
  MCT_GRP_EAPOL    = 8,  // EAPOL multicast frames
  MCT_GRP_IPV6     = 9,  // IPv6 multicast (ff02::/16)
};

// Source of multicast frames
struct MctSource {
  uint8_t  mac[6];
  uint32_t frame_count;
  uint32_t byte_count;
  uint32_t last_ts;
};

struct MctGroup {
  bool    active;
  uint8_t addr[6];           // multicast/broadcast MAC
  MctGroupType type;
  char    type_name[24];     // human-readable type

  // Frame counters
  uint32_t total_frames;
  uint32_t total_bytes;
  uint32_t retry_frames;

  // Rate tracking
  uint32_t frames_per_sec;    // current rate
  uint32_t peak_rate;
  uint8_t  rate_hist[MCT_HIST_LEN];
  uint8_t  hist_head;
  uint32_t cur_frames;
  uint32_t last_tick_s;

  // Source tracking
  MctSource sources[MCT_MAX_SOURCES];
  uint8_t   source_count;
  uint8_t   unique_sources;  // ever seen

  // Channel where observed
  uint8_t  channels_seen;    // bitmask of channels 1-8

  // Storm detection
  bool     is_storm;
  uint32_t storm_events;
  uint32_t last_storm_ts;

  uint32_t first_ts;
  uint32_t last_ts;
};

// Global broadcast counters
struct MctGlobalStats {
  uint64_t total_broadcast;
  uint64_t total_multicast;
  uint32_t broadcast_rate;    // per second
  uint32_t multicast_rate;
  uint32_t arp_requests;
  uint32_t arp_replies;
};

typedef struct {
  MctGroup      groups[MCT_MAX_GROUPS];
  uint8_t       count;
  MctGlobalStats global;
  uint32_t      last_tick_s;
  uint32_t      cur_bc;
  uint32_t      cur_mc;
} G_MctState;
#ifdef BOARD_HAS_PSRAM
static G_MctState* _g_mct_raw;
#define g_mct (*_g_mct_raw)
#else
static G_MctState g_mct;
#endif

static bool mct_mac_eq(const uint8_t *a, const uint8_t *b) { return memcmp(a,b,6)==0; }

static MctGroupType mct_classify(const uint8_t *addr) {
  if (addr[0]==0xff && addr[1]==0xff) return MCT_GRP_BROADCAST;
  if (addr[0]==0x01 && addr[1]==0x80 && addr[2]==0xC2) return MCT_GRP_IGMP; // STP/slow proto
  if (addr[0]==0x01 && addr[1]==0x00 && addr[2]==0x5E) {
    // IPv4 multicast
    if (addr[3]==0x00 && addr[4]==0x00 && addr[5]==0xFB) return MCT_GRP_MDNS;
    if (addr[3]==0xFF && addr[4]==0xFF && addr[5]==0xFA) return MCT_GRP_SSDP;
    if (addr[3]==0x00 && addr[4]==0x01 && addr[5]==0x01) return MCT_GRP_NTP;
    return MCT_GRP_IGMP;
  }
  if (addr[0]==0x33 && addr[1]==0x33) return MCT_GRP_IPV6; // IPv6 multicast
  return MCT_GRP_UNKNOWN;
}

static const char* mct_type_name(MctGroupType t) {
  switch (t) {
    case MCT_GRP_MDNS:     return "mDNS";
    case MCT_GRP_SSDP:     return "SSDP/UPnP";
    case MCT_GRP_NTP:      return "NTP";
    case MCT_GRP_DHCP_SRV: return "DHCP";
    case MCT_GRP_IGMP:     return "IGMP";
    case MCT_GRP_BROADCAST:return "Broadcast";
    case MCT_GRP_ARP:      return "ARP";
    case MCT_GRP_EAPOL:    return "EAPOL";
    case MCT_GRP_IPV6:     return "IPv6 MC";
    default:               return "Unknown";
  }
}

static MctGroup* mct_find(const uint8_t *addr) {
  for (int i = 0; i < MCT_MAX_GROUPS; i++)
    if (g_mct.groups[i].active && mct_mac_eq(g_mct.groups[i].addr, addr))
      return &g_mct.groups[i];
  return nullptr;
}

static MctGroup* mct_alloc(const uint8_t *addr) {
  for (int i = 0; i < MCT_MAX_GROUPS; i++) {
    if (!g_mct.groups[i].active) {
      MctGroup &g = g_mct.groups[i];
      memset(&g, 0, sizeof(g));
      g.active = true;
      memcpy(g.addr, addr, 6);
      g.type = mct_classify(addr);
      strncpy(g.type_name, mct_type_name(g.type), 23);
      g.first_ts = millis()/1000;
      g_mct.count++;
      return &g;
    }
  }
  uint32_t oldest = UINT32_MAX; int idx = 0;
  for (int i = 0; i < MCT_MAX_GROUPS; i++)
    if (g_mct.groups[i].last_ts < oldest) { oldest = g_mct.groups[i].last_ts; idx = i; }
  memset(&g_mct.groups[idx], 0, sizeof(MctGroup));
  g_mct.groups[idx].active = true;
  memcpy(g_mct.groups[idx].addr, addr, 6);
  g_mct.groups[idx].type = mct_classify(addr);
  strncpy(g_mct.groups[idx].type_name, mct_type_name(g_mct.groups[idx].type), 23);
  g_mct.groups[idx].first_ts = millis()/1000;
  return &g_mct.groups[idx];
}

static void mct_tick_global() {
  uint32_t now_s = millis()/1000;
  if (now_s == g_mct.last_tick_s) return;
  g_mct.global.broadcast_rate = g_mct.cur_bc;
  g_mct.global.multicast_rate = g_mct.cur_mc;
  g_mct.cur_bc = g_mct.cur_mc = 0;
  g_mct.last_tick_s = now_s;
}

static void mct_tick_group(MctGroup &gr) {
  uint32_t now_s = millis()/1000;
  if (now_s == gr.last_tick_s) return;
  gr.frames_per_sec = gr.cur_frames;
  if (gr.cur_frames > gr.peak_rate) gr.peak_rate = gr.cur_frames;

  #define MCT_STORM_THRESHOLD 100
  bool was_storm = gr.is_storm;
  gr.is_storm = (gr.cur_frames >= MCT_STORM_THRESHOLD);
  if (gr.is_storm && !was_storm) { gr.storm_events++; gr.last_storm_ts = millis()/1000; }

  gr.rate_hist[gr.hist_head % MCT_HIST_LEN] = (uint8_t)(gr.cur_frames > 255 ? 255 : gr.cur_frames);
  gr.hist_head = (gr.hist_head + 1) % MCT_HIST_LEN;
  gr.cur_frames = 0;
  gr.last_tick_s = now_s;
}

void mct_ingest(const uint8_t *dst_addr, const uint8_t *src_mac, uint8_t ch,
                bool retry, uint16_t len) {
  mct_tick_global();
  bool is_bcast = (dst_addr[0] == 0xFF);
  if (is_bcast) { g_mct.cur_bc++; g_mct.global.total_broadcast++; }
  else          { g_mct.cur_mc++; g_mct.global.total_multicast++; }

  MctGroup *gr = mct_find(dst_addr);
  if (!gr) gr = mct_alloc(dst_addr);
  if (!gr) return;

  mct_tick_group(*gr);
  gr->total_frames++;
  gr->total_bytes += len;
  if (retry) gr->retry_frames++;
  gr->cur_frames++;
  if (ch >= 1 && ch <= 8) gr->channels_seen |= (1 << (ch-1));
  gr->last_ts = millis()/1000;

  // Update source tracking
  if (src_mac) {
    bool found = false;
    for (int i = 0; i < gr->source_count; i++) {
      if (mct_mac_eq(gr->sources[i].mac, src_mac)) {
        gr->sources[i].frame_count++;
        gr->sources[i].byte_count += len;
        gr->sources[i].last_ts = millis()/1000;
        found = true;
        break;
      }
    }
    if (!found && gr->source_count < MCT_MAX_SOURCES) {
      MctSource &s = gr->sources[gr->source_count++];
      memcpy(s.mac, src_mac, 6);
      s.frame_count = 1;
      s.byte_count = len;
      s.last_ts = millis()/1000;
      gr->unique_sources++;
    }
  }
}

void mct_ingest_arp(bool is_request) {
  if (is_request) g_mct.global.arp_requests++;
  else            g_mct.global.arp_replies++;
}

static void mct_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String mct_hist_json(const uint8_t *hist, int len, int head) {
  String s = "[";
  for (int i = 0; i < len; i++) {
    if (i) s += ",";
    s += String(hist[(head+i)%len]);
  }
  return s + "]";
}

static String mct_json_all() {
  String s = "{\"count\":" + String(g_mct.count);
  s += ",\"total_broadcast\":" + String((uint32_t)g_mct.global.total_broadcast);
  s += ",\"total_multicast\":" + String((uint32_t)g_mct.global.total_multicast);
  s += ",\"broadcast_rate\":" + String(g_mct.global.broadcast_rate);
  s += ",\"multicast_rate\":" + String(g_mct.global.multicast_rate);
  s += ",\"arp_req\":" + String(g_mct.global.arp_requests);
  s += ",\"arp_rep\":" + String(g_mct.global.arp_replies);
  s += ",\"groups\":[";
  char mac[18]; bool first = true;
  for (int i = 0; i < MCT_MAX_GROUPS; i++) {
    MctGroup &gr = g_mct.groups[i];
    if (!gr.active) continue;
    if (!first) s += ","; first = false;
    mct_mac_str(gr.addr, mac);
    s += "{\"addr\":\"" + String(mac) + "\"";
    s += ",\"type\":\"" + String(gr.type_name) + "\"";
    s += ",\"total\":" + String(gr.total_frames);
    s += ",\"bytes\":" + String(gr.total_bytes);
    s += ",\"rate\":" + String(gr.frames_per_sec);
    s += ",\"peak\":" + String(gr.peak_rate);
    s += ",\"sources\":" + String(gr.unique_sources);
    s += ",\"storm\":" + String(gr.is_storm ? "true" : "false");
    s += ",\"storm_events\":" + String(gr.storm_events);
    s += ",\"rate_hist\":" + mct_hist_json(gr.rate_hist, MCT_HIST_LEN, gr.hist_head) + "}";
  }
  s += "]}";
  return s;
}

inline void mct_routes() {
  server.on("/api/mct/all", HTTP_GET, []() {
    server.send(200, "application/json", mct_json_all());
  });
  server.on("/api/mct/clear", HTTP_POST, []() {
    memset(&g_mct, 0, sizeof(g_mct));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

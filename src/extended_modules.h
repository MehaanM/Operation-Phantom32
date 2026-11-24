#pragma once
// extended_modules.h — Aggregator for all advanced analytical modules.
// Include this ONCE in main.cpp after all other module headers.
// Registers routes, initializes state, and provides unified API endpoints
// that aggregate data from multiple modules into a single response.
// This file pulls in ALL analytical/detection modules added in sessions 2+.

// ============================================================
// Module headers (session 2 — analytics)
// ============================================================
#include "multicast_tracker.h"
#include "eapol_monitor.h"
#include "wps_detector.h"
#include "timing_attack_detector.h"
#include "scan_result_cache.h"
#include "probe_flood_detector.h"
#include "tsf_tracker.h"
#include "bss_color.h"

// ============================================================
// Module headers (session 3 — topology & spectrum)
// ============================================================
#include "net_topology.h"
#include "signal_heatmap.h"
#include "captive_portal_detector.h"
#include "bss_load_history.h"
#include "channel_analyzer.h"

// ============================================================
// Web UI headers (session 2)
// ============================================================
#include "webui_multicast.h"
#include "webui_eapol.h"
#include "webui_wps.h"
#include "webui_timing.h"
#include "webui_scan_cache.h"
#include "webui_probe_flood.h"
#include "webui_tsf.h"
#include "webui_bss_color.h"

// ============================================================
// Web UI headers (session 3)
// ============================================================
#include "webui_net_topology.h"
#include "webui_signal_heatmap.h"
#include "webui_captive.h"
#include "webui_bss_load.h"
#include "webui_channel_analyzer.h"

// ============================================================
// Combined status endpoint: /api/status/all
// Returns a compact summary of all module states in one JSON.
// Used by the dashboard for a unified health view.
// ============================================================

static String ext_status_json() {
  String s = "{";
  s += "\"uptime_ms\":" + String(millis());
  s += ",\"free_heap\":" + String(ESP.getFreeHeap());
  s += ",\"min_heap\":" + String(ESP.getMinFreeHeap());

  // Multicast tracker
  extern struct { uint32_t total_unicast; uint32_t total_multicast; uint32_t total_broadcast;
                  uint32_t total_arp; uint32_t total_mdns; uint32_t total_ssdp;
                  uint32_t total_ntp; uint32_t storm_events; } g_mct;
  s += ",\"mct\":{";
  s += "\"bcast\":" + String(g_mct.total_broadcast);
  s += ",\"mcast\":" + String(g_mct.total_multicast);
  s += ",\"arp\":" + String(g_mct.total_arp);
  s += ",\"storms\":" + String(g_mct.storm_events);
  s += "}";

  // EAPOL monitor
  extern struct { uint8_t sess_count; uint8_t event_count; uint16_t total_sessions;
                  uint16_t total_complete; uint16_t total_pmkid; uint16_t total_mic_fail;
                  uint16_t total_replay; uint16_t total_timeout; } g_eap;
  s += ",\"eap\":{";
  s += "\"sessions\":" + String(g_eap.sess_count);
  s += ",\"pmkid\":" + String(g_eap.total_pmkid);
  s += ",\"mic_fail\":" + String(g_eap.total_mic_fail);
  s += "}";

  // WPS detector
  extern struct { uint8_t ap_count; uint8_t sess_count; uint16_t total_pixie;
                  uint16_t total_locked; } g_wps;
  s += ",\"wps\":{";
  s += "\"aps\":" + String(g_wps.ap_count);
  s += ",\"pixie\":" + String(g_wps.total_pixie);
  s += ",\"locked\":" + String(g_wps.total_locked);
  s += "}";

  // Timing attack detector
  extern struct { uint8_t ap_count; uint8_t event_count; uint16_t total_anomalies;
                  uint16_t total_auth_flood; uint16_t total_deauth_burst;
                  uint16_t total_probe_sweep; } g_tad;
  s += ",\"tad\":{";
  s += "\"anomalies\":" + String(g_tad.total_anomalies);
  s += ",\"auth_flood\":" + String(g_tad.total_auth_flood);
  s += ",\"deauth\":" + String(g_tad.total_deauth_burst);
  s += "}";

  // Scan cache
  extern struct { uint8_t count; uint32_t total_ingested; uint32_t total_aged;
                  uint32_t total_updated; } g_src;
  s += ",\"src\":{";
  s += "\"count\":" + String(g_src.count);
  s += ",\"total\":" + String(g_src.total_ingested);
  s += "}";

  // Probe flood
  extern struct { uint8_t device_count; uint16_t total_floods; uint16_t total_sweeps;
                  uint32_t total_probes; } g_pfd;
  s += ",\"pfd\":{";
  s += "\"devices\":" + String(g_pfd.device_count);
  s += ",\"floods\":" + String(g_pfd.total_floods);
  s += ",\"probes\":" + String(g_pfd.total_probes);
  s += "}";

  // TSF tracker
  extern struct { TsfAP aps[32]; uint8_t count; } g_tsf;
  s += ",\"tsf\":{\"aps\":" + String(g_tsf.count) + "}";

  // BSS Color
  extern struct { BscAP aps[32]; uint8_t count; uint8_t color_map[64];
                  uint16_t total_collisions; uint16_t total_color_changes; } g_bsc;
  s += ",\"bsc\":{";
  s += "\"aps\":" + String(g_bsc.count);
  s += ",\"collisions\":" + String(g_bsc.total_collisions);
  s += "}";

  // Network topology
  extern struct { NttNode nodes[80]; NttEdge edges[128]; NttEss ess[16];
                  uint8_t node_count; uint8_t edge_count; uint8_t ess_count;
                  uint32_t total_assoc_events; uint32_t total_roam_events; } g_ntt;
  s += ",\"ntt\":{";
  s += "\"nodes\":" + String(g_ntt.node_count);
  s += ",\"edges\":" + String(g_ntt.edge_count);
  s += ",\"roams\":" + String(g_ntt.total_roam_events);
  s += "}";

  // Signal heatmap
  extern struct { uint32_t total_frames_all; } g_shm;
  s += ",\"shm\":{\"frames\":" + String(g_shm.total_frames_all) + "}";

  // Captive portal
  extern struct { uint8_t ap_count; uint16_t total_detected; uint16_t total_evil_twin;
                  uint16_t total_honeypot; } g_cpd;
  s += ",\"cpd\":{";
  s += "\"detected\":" + String(g_cpd.total_detected);
  s += ",\"evil_twin\":" + String(g_cpd.total_evil_twin);
  s += "}";

  // BSS Load
  extern struct { uint8_t ap_count; uint16_t total_heavy_events;
                  uint8_t busiest_ch; uint8_t busiest_util; } g_bsl;
  s += ",\"bsl\":{";
  s += "\"aps\":" + String(g_bsl.ap_count);
  s += ",\"heavy\":" + String(g_bsl.total_heavy_events);
  s += ",\"busiest_ch\":" + String(g_bsl.busiest_ch);
  s += "}";

  // Channel analyzer
  extern struct { uint8_t best_ch_2g; uint8_t best_score; uint32_t total_frames; } g_caz;
  s += ",\"caz\":{";
  s += "\"best_ch\":" + String(g_caz.best_ch_2g);
  s += ",\"frames\":" + String(g_caz.total_frames);
  s += "}";

  s += "}";
  return s;
}

// ============================================================
// Module navigation links for the web UI
// Used by the dashboard and nav bar.
// ============================================================

static const char EXT_NAV_LINKS[] PROGMEM =
  "<a href='/'>Home</a>"
  "<a href='/dashboard'>Dashboard</a>"
  "<a href='/channel-analyzer'>Channels</a>"
  "<a href='/signal-heatmap'>Signal Map</a>"
  "<a href='/topology'>Topology</a>"
  "<a href='/scan-cache'>Scan Cache</a>"
  "<a href='/probe-flood'>Probe Flood</a>"
  "<a href='/multicast'>Multicast</a>"
  "<a href='/eapol'>EAPOL</a>"
  "<a href='/wps'>WPS</a>"
  "<a href='/timing'>Timing</a>"
  "<a href='/tsf'>TSF</a>"
  "<a href='/bss-color'>BSS Color</a>"
  "<a href='/bss-load'>BSS Load</a>"
  "<a href='/captive'>Captive</a>";

// ============================================================
// Register ALL extended module routes.
// Call ext_register_routes() once from setup() AFTER WiFi is up.
// ============================================================

inline void ext_register_routes() {
  // Backend API routes
  mct_routes();
  eap_routes();
  wps_routes();
  tad_routes();
  src_routes();
  pfd_routes();
  tsf_routes();
  bsc_routes();
  ntt_routes();
  shm_routes();
  cpd_routes();
  bsl_routes();
  caz_routes();

  // Web UI page routes
  wumct_routes();
  wueap_routes();
  wuwps_routes();
  wutad_routes();
  wusrc_routes();
  wupfd_routes();
  wutsf_routes();
  wubsc_routes();
  wuntt_routes();
  wushm_routes();
  wucpd_routes();
  wubsl_routes();
  wucaz_routes();

  // Combined status
  server.on("/api/status/all", HTTP_GET, []() {
    server.send(200, "application/json", ext_status_json());
  });
}

// ============================================================
// Unified promiscuous-mode frame dispatcher.
// In main.cpp, call ext_dispatch_frame() from the promisc callback
// to route each frame to all relevant analytical modules.
// ============================================================

struct ExtFrameInfo {
  uint8_t type;        // 802.11 frame type (mgmt=0, ctrl=1, data=2)
  uint8_t subtype;     // 802.11 subtype
  uint8_t ch;          // channel
  int8_t  rssi;        // RSSI in dBm
  uint8_t src[6];      // source MAC
  uint8_t dst[6];      // destination MAC
  uint8_t bssid[6];    // BSSID
  char    ssid[33];    // SSID (if beacon/probe)
  uint16_t len;        // frame length
  uint32_t seq;        // sequence number
  uint64_t tsf;        // TSF counter (from beacon)
  uint16_t beacon_int; // beacon interval (from beacon)
  bool    retry;       // retry bit

  // Capability info (from beacon)
  bool    is_ap;
  bool    is_open;
  bool    has_pmf;
  bool    has_wps;
  bool    has_ht;
  bool    has_vht;
  bool    has_he;
  uint8_t ch_bw;       // channel width: 20/40/80/160
  uint8_t sec_ch;      // secondary channel (HT40)
  uint8_t sec_flags;   // security flags
  uint8_t bss_color;   // 802.11ax BSS Color
  bool    he_capable;
  bool    color_disabled;

  // BSS Load IE
  bool     has_bss_load;
  uint8_t  bss_util;    // 0-255
  uint8_t  bss_sta;
  uint16_t bss_avail;

  // WPS info
  bool    has_wps_ie;

  // Interworking
  bool    has_interworking;
  bool    internet_bit;

  // Multicast / broadcast
  bool    is_mcast;
  bool    is_bcast;
  bool    is_arp;
  bool    arp_is_request;
};

inline void ext_dispatch_frame(const ExtFrameInfo &f) {
  // Signal heatmap: every frame
  shm_ingest(f.ch, f.rssi, f.bssid, f.ssid);

  // Channel analyzer: every frame
  caz_ingest(f.ch, f.rssi, f.bssid, f.ssid, f.has_ht, f.has_vht, f.has_he,
             (CazBW)0, f.sec_ch);

  if (f.type == 0) {  // management frames
    switch (f.subtype) {
      case 8:  // beacon
        // TSF tracker
        tsf_ingest(f.bssid, f.ssid, f.ch, f.rssi, f.tsf, f.beacon_int);
        // Timing anomaly detector
        tad_ingest_beacon(f.bssid, f.ssid, f.ch, f.beacon_int, f.tsf);
        // Scan result cache
        src_ingest_beacon(f.bssid, f.ssid, f.ch, f.rssi, f.sec_flags, "", "",
                          0, f.ssid[0]==0, 0, f.beacon_int, 0,
                          f.has_ht, f.has_vht, f.has_he, 0, 20, false);
        // Network topology
        ntt_ingest_ap(f.bssid, f.ssid, f.ch, f.rssi);
        // BSS Color
        if (f.he_capable) {
          bsc_ingest(f.bssid, f.ssid, f.ch, f.rssi, true, f.bss_color,
                     false, f.color_disabled, 0, -82, -62, false);
        }
        // BSS Load
        if (f.has_bss_load) {
          bsl_ingest(f.bssid, f.ssid, f.ch, f.rssi,
                     f.bss_util, f.bss_sta, f.bss_avail);
        }
        // Captive portal
        cpd_ingest(f.bssid, f.ssid, f.ch, f.rssi, f.is_open, f.has_pmf,
                   f.has_wps, f.has_interworking, f.internet_bit,
                   f.beacon_int, f.sec_flags);
        // WPS detector
        if (f.has_wps_ie) {
          wps_ingest_beacon(f.bssid, f.ssid, f.ch, f.rssi,
                            WPS_STATE_CONFIGURED, WPS_METHOD_PBC, WPS_VER_20,
                            "", "", "", "", false, false);
        }
        break;

      case 4:  // probe request
        tad_ingest_probe(f.src, f.ch);
        pfd_ingest(f.src, f.ssid, f.ch, f.rssi);
        ntt_ingest_probe(f.src, nullptr, f.ch, f.rssi);
        break;

      case 5:  // probe response
        src_ingest_probe(f.bssid, true);
        ntt_ingest_probe(nullptr, f.bssid, f.ch, f.rssi);
        break;

      case 11: // auth
        tad_ingest_auth(f.bssid, f.ch);
        break;

      case 12: // deauth
        tad_ingest_deauth(f.src, f.ch);
        eap_ingest_deauth_mid_hs(f.dst, f.src);
        break;

      case 10: // disassoc
        tad_ingest_deauth(f.src, f.ch);
        break;
    }
  }

  // Data frames: multicast/broadcast tracking
  if (f.type == 2) {
    if (f.is_mcast || f.is_bcast) {
      mct_ingest(f.dst, f.src, f.ch, f.retry, f.len);
    }
    if (f.is_arp) {
      mct_ingest_arp(f.arp_is_request);
    }
    // Association tracking
    if (!f.is_bcast) {
      ntt_ingest_client(f.src, f.bssid, f.ssid, f.ch, f.rssi);
    }
  }
}

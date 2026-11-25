#pragma once
// extended_modules.h — Aggregator for all advanced analytical modules.
// Include this ONCE in main.cpp after all other module headers.
//
// PHANTOM32_LITE: defined for non-PSRAM boards (WROOM-32D, NodeMCU-32S, etc.).
//   All heavy analytics are compiled out; only ExtFrameInfo struct and
//   no-op stubs remain so main.cpp compiles unchanged.
//
// Full analytics (all 13 modules) require PSRAM (WROVER, S3, S3-DEVKIT).

#include "psram_attr.h"

// ===========================================================================
// ExtFrameInfo — always present so main.cpp promisc_cb can build it
// ===========================================================================

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

#ifndef PHANTOM32_LITE
// ===========================================================================
// FULL BUILD — all analytical modules included (requires PSRAM)
// ===========================================================================

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
// ============================================================

static String ext_status_json() {
  String s = "{";
  s += "\"uptime_ms\":"  + String(millis());
  s += ",\"free_heap\":" + String(ESP.getFreeHeap());
  s += ",\"min_heap\":"  + String(ESP.getMinFreeHeap());
  s += ",\"total_heap\":" + String(ESP.getHeapSize());
  s += "}";
  return s;
}

// ============================================================
// Module navigation links for the web UI
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
  eapm_routes();
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
// ============================================================

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
                            WPS_STATE_CONFIGURED, WPS_METHOD_PBC, WPS_VER_2_0,
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

// ============================================================
// PSRAM heap allocation for all large analytics state.
// Call once at the very start of setup() on PSRAM builds.
// On LITE builds this is a no-op.
// ============================================================
static inline bool psram_alloc_all() {
#ifdef BOARD_HAS_PSRAM
  _g_mct_raw    = (G_MctState*)   ps_calloc(1, sizeof(G_MctState));
  _g_eap_raw    = (G_EapState*)   ps_calloc(1, sizeof(G_EapState));
  _g_wps_raw    = (G_WpsDetState*)ps_calloc(1, sizeof(G_WpsDetState));
  _g_tad_raw    = (G_TadState*)   ps_calloc(1, sizeof(G_TadState));
  _g_src_raw    = (G_SrcState*)   ps_calloc(1, sizeof(G_SrcState));
  _g_pfd_raw    = (G_PfdState*)   ps_calloc(1, sizeof(G_PfdState));
  _g_tsf_raw    = (G_TsfState*)   ps_calloc(1, sizeof(G_TsfState));
  _g_bsc_raw    = (G_BscState*)   ps_calloc(1, sizeof(G_BscState));
  _g_ntt_raw    = (G_NttState*)   ps_calloc(1, sizeof(G_NttState));
  _g_shm_raw    = (G_ShmState*)   ps_calloc(1, sizeof(G_ShmState));
  _g_cpd_raw    = (G_CpdState*)   ps_calloc(1, sizeof(G_CpdState));
  _g_bsl_raw    = (G_BslState*)   ps_calloc(1, sizeof(G_BslState));
  // Headers included directly in main.cpp before extended_modules.h:
  _g_pfl_raw    = (G_PflState*)   ps_calloc(1, sizeof(G_PflState));
  _g_das_raw    = (G_DasState*)   ps_calloc(1, sizeof(G_DasState));
  _g_et_raw     = (EtStatus*)     ps_calloc(1, sizeof(EtStatus));
  prh_entries   = (PrhEntry*)     ps_calloc(PRH_MAX_ENTRIES, sizeof(PrhEntry));
  g_dt_devs     = (DtDevice*)     ps_calloc(DT_MAX_DEVICES, sizeof(DtDevice));
  g_dt_aps      = (DtAP*)         ps_calloc(DT_MAX_APS, sizeof(DtAP));
  g_dt_alerts   = (DtAlert*)      ps_calloc(DT_MAX_ALERTS, sizeof(DtAlert));
  g_cli_scripts = (CliScript*)    ps_calloc(CLI_SCRIPT_MAX, sizeof(CliScript));
  return _g_mct_raw && _g_eap_raw && _g_wps_raw && _g_tad_raw && _g_src_raw &&
         _g_pfd_raw && _g_tsf_raw && _g_bsc_raw && _g_ntt_raw && _g_shm_raw &&
         _g_cpd_raw && _g_bsl_raw && _g_pfl_raw && _g_das_raw && _g_et_raw &&
         prh_entries && g_dt_devs && g_dt_aps && g_dt_alerts && g_cli_scripts;
#else
  return true;
#endif
}

#else // PHANTOM32_LITE
// ===========================================================================
// LITE BUILD — analytics disabled to fit in 320 KB DRAM (no PSRAM boards)
// Only the ExtFrameInfo struct (above) and no-op stubs are compiled in.
// Core attack tools (deauth, beacon spam, probe flood, etc.) work normally.
// ===========================================================================

static const char EXT_NAV_LINKS[] PROGMEM = "<a href='/'>Home</a>";

static String ext_status_json() {
  String s = "{";
  s += "\"uptime_ms\":"  + String(millis());
  s += ",\"free_heap\":" + String(ESP.getFreeHeap());
  s += ",\"min_heap\":"  + String(ESP.getMinFreeHeap());
  s += ",\"total_heap\":" + String(ESP.getHeapSize());
  s += ",\"lite_mode\":true";
  s += "}";
  return s;
}

inline void ext_register_routes() {
  server.on("/api/status/all", HTTP_GET, []() {
    server.send(200, "application/json", ext_status_json());
  });
}

inline void ext_dispatch_frame(const ExtFrameInfo &) {}

static inline bool psram_alloc_all() { return true; }

#endif // PHANTOM32_LITE

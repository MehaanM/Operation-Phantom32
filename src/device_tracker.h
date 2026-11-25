#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <string.h>
#include "module.h"
#include "psram_attr.h"

// ── Device Tracker ────────────────────────────────────────────────────────────
//
// Passive 802.11 device fingerprinting engine.
// Tracks devices by MAC address, observed frame types, probe SSIDs,
// RSSI history, activity patterns, vendor OUI, and presence/absence.
//
// Provides:
//   - /api/devtrack/devices    — full device list JSON
//   - /api/devtrack/seen       — recently-seen MAC list
//   - /api/devtrack/status     — running stats
//   - /api/devtrack/alerts     — new/disappeared device alerts
//   - /api/devtrack/ap_clients — AP → client association map

// ── OUI table (top 60 vendor prefixes) ───────────────────────────────────────
struct OuiEntry { uint8_t oui[3]; const char* vendor; };
static const OuiEntry DT_OUI_TABLE[] = {
  {{0x00,0x0C,0xE7},"Apple"},   {{0x00,0x50,0x56},"VMware"},
  {{0x00,0x1A,0x11},"Google"},  {{0xAC,0x91,0xA1},"Apple"},
  {{0x3C,0x07,0x54},"Apple"},   {{0xD0,0xC5,0xF3},"Apple"},
  {{0xF8,0xFF,0xC2},"Apple"},   {{0x48,0xD7,0x05},"Apple"},
  {{0x00,0x50,0xF2},"Microsoft"},{{0x00,0x17,0xFA},"Microsoft"},
  {{0x60,0x45,0xCB},"Huawei"},  {{0x00,0x0F,0xBB},"Samsung"},
  {{0x00,0x24,0x54},"Samsung"},  {{0x78,0x59,0x5E},"Samsung"},
  {{0x94,0x65,0x2D},"Samsung"}, {{0xB4,0xAE,0x2B},"Samsung"},
  {{0x44,0x65,0x0D},"Samsung"}, {{0x28,0xCC,0x9E},"Samsung"},
  {{0x00,0x11,0x22},"Ciena"},   {{0x00,0x1B,0x63},"Apple"},
  {{0x00,0x03,0x93},"Apple"},   {{0x00,0x0A,0x27},"Apple"},
  {{0x00,0x50,0xE4},"Cisco"},   {{0x00,0x0D,0x28},"Cisco"},
  {{0x00,0x11,0xBC},"Cisco"},   {{0x00,0x14,0x1B},"Cisco"},
  {{0x00,0x1A,0xA0},"Cisco"},   {{0x00,0x1C,0x57},"Cisco"},
  {{0x00,0x21,0x1C},"Cisco"},   {{0x00,0x0F,0x24},"Cisco"},
  {{0x18,0xF7,0x69},"Dell"},    {{0x14,0xFE,0xB5},"Dell"},
  {{0x00,0x21,0x9B},"Dell"},    {{0xF8,0x0D,0xAC},"Intel"},
  {{0xF4,0x96,0x34},"Intel"},   {{0x60,0x6D,0x3C},"Intel"},
  {{0xA4,0x34,0xD9},"Intel"},   {{0x8C,0xEC,0x4B},"Intel"},
  {{0x00,0x15,0x17},"Intel"},   {{0x00,0x1F,0xE2},"Intel"},
  {{0xB4,0x37,0xD1},"Ralink"},  {{0x00,0x11,0x50},"Ralink"},
  {{0xAC,0x81,0x12},"Xiaomi"},  {{0x28,0xE3,0x1F},"Xiaomi"},
  {{0x64,0xB4,0x73},"OnePlus"}, {{0x94,0x87,0xE0},"OnePlus"},
  {{0x34,0xAB,0x37},"Netgear"}, {{0x00,0x9A,0xCD},"Huawei"},
  {{0x00,0x18,0xE7},"Huawei"},  {{0xAC,0x61,0x75},"ASUS"},
  {{0x00,0x0C,0x6E},"ASUS"},    {{0x00,0x50,0xFC},"ASUS"},
  {{0xB0,0x6E,0xBF},"TP-Link"}, {{0x50,0xBD,0x5F},"TP-Link"},
  {{0xC0,0x4A,0x00},"TP-Link"}, {{0x00,0x1F,0x33},"Ubiquiti"},
  {{0x04,0x18,0xD6},"Ubiquiti"},{{0x24,0xA4,0x3C},"Ubiquiti"},
  {{0xDC,0x9F,0xDB},"Ubiquiti"},{{0xE0,0x63,0xDA},"Ubiquiti"},
};
#define DT_OUI_COUNT (sizeof(DT_OUI_TABLE)/sizeof(DT_OUI_TABLE[0]))

static const char* dt_oui_lookup(const uint8_t* mac) {
  for (size_t i = 0; i < DT_OUI_COUNT; i++) {
    if (mac[0]==DT_OUI_TABLE[i].oui[0] &&
        mac[1]==DT_OUI_TABLE[i].oui[1] &&
        mac[2]==DT_OUI_TABLE[i].oui[2]) return DT_OUI_TABLE[i].vendor;
  }
  // Locally administered / random MAC
  if (mac[0] & 0x02) return "Randomized";
  return "Unknown";
}

// ── Device types (heuristic from vendor + frame patterns) ─────────────────────
enum DtDevType {
  DT_TYPE_UNKNOWN=0, DT_TYPE_PHONE, DT_TYPE_LAPTOP, DT_TYPE_IOT,
  DT_TYPE_AP, DT_TYPE_TABLET, DT_TYPE_WEARABLE, DT_TYPE_CAMERA
};
static const char* const DT_TYPE_NAMES[] = {
  "unknown","phone","laptop","iot","ap","tablet","wearable","camera"
};
static const char* const DT_TYPE_ICONS[] = {
  "📱","📱","💻","📡","🌐","📱","⌚","📷"
};

static DtDevType dt_classify(const char* vendor, uint16_t probes,
                              uint16_t beacons, uint16_t data) {
  if (beacons > 10 && probes < 2) return DT_TYPE_AP;
  String v(vendor); v.toLowerCase();
  if (v.indexOf("apple") >= 0 || v.indexOf("samsung") >= 0 ||
      v.indexOf("xiaomi") >= 0 || v.indexOf("oneplus") >= 0) {
    return probes > 5 ? DT_TYPE_PHONE : DT_TYPE_TABLET;
  }
  if (v.indexOf("intel") >= 0 || v.indexOf("dell") >= 0) return DT_TYPE_LAPTOP;
  if (v.indexOf("ubiquiti") >= 0 || v.indexOf("netgear") >= 0 ||
      v.indexOf("tp-link") >= 0 || v.indexOf("asus") >= 0 ||
      v.indexOf("cisco") >= 0)   return DT_TYPE_AP;
  if (data == 0 && probes < 3)   return DT_TYPE_IOT;
  if (probes > 10)                return DT_TYPE_PHONE;
  return DT_TYPE_UNKNOWN;
}

// ── RSSI ring buffer ──────────────────────────────────────────────────────────
#define DT_RSSI_HIST 8
struct DtRssiHist {
  int8_t  vals[DT_RSSI_HIST];
  uint8_t head;
  uint8_t count;
  void push(int8_t v) {
    vals[head] = v;
    head = (head+1) % DT_RSSI_HIST;
    if (count < DT_RSSI_HIST) count++;
  }
  float avg() const {
    if (!count) return 0.0f;
    int32_t s = 0;
    for (int i = 0; i < count; i++) s += vals[i];
    return (float)s / count;
  }
  int8_t min_v() const {
    if (!count) return 0;
    int8_t m = vals[0];
    for (int i = 1; i < count; i++) if (vals[i] < m) m = vals[i];
    return m;
  }
  int8_t max_v() const {
    if (!count) return 0;
    int8_t m = vals[0];
    for (int i = 1; i < count; i++) if (vals[i] > m) m = vals[i];
    return m;
  }
};

// ── Probe history (up to 12 SSIDs per device) ────────────────────────────────
#ifdef PHANTOM32_LITE
#define DT_MAX_PROBES 4
#define DT_MAX_DEVICES 20
#define DT_MAX_APS     8
#define DT_MAX_ALERTS  8
#else
#define DT_MAX_PROBES 12
#define DT_MAX_DEVICES 96
#endif
#define DT_SSID_LEN   33

// ── Device record ─────────────────────────────────────────────────────────────
#ifndef DT_MAX_DEVICES
#define DT_MAX_DEVICES 96
#endif
struct DtDevice {
  uint8_t    mac[6];
  char       vendor[20];
  DtDevType  type;
  int8_t     rssi_last;
  DtRssiHist rssi_hist;
  uint16_t   probes;
  uint16_t   beacons_rx;
  uint16_t   data_frames;
  uint16_t   mgmt_frames;
  uint16_t   deauths_rx;
  uint8_t    assoc_ap[6];     // last seen AP BSSID (from data frame)
  char       probe_ssids[DT_MAX_PROBES][DT_SSID_LEN];
  uint8_t    probe_count;
  uint32_t   first_seen_ms;
  uint32_t   last_seen_ms;
  uint32_t   last_active_ms; // last data/probe frame
  bool       is_ap;
  bool       randomized;
  bool       present;         // seen in last 60s
  bool       alerted;         // new-device alert sent
};

// ── AP → client association map ───────────────────────────────────────────────
#ifndef DT_MAX_APS
#define DT_MAX_APS 32
#endif
struct DtAP {
  uint8_t  bssid[6];
  char     ssid[33];
  uint8_t  ch;
  int8_t   rssi;
  uint16_t beacons;
  uint16_t clients;
  uint8_t  client_macs[8][6]; // up to 8 known clients
  uint32_t last_seen_ms;
};

// ── Alert record ──────────────────────────────────────────────────────────────
#ifndef DT_MAX_ALERTS
#define DT_MAX_ALERTS 24
#endif
struct DtAlert {
  uint8_t  mac[6];
  char     vendor[20];
  int8_t   rssi;
  bool     appeared;    // true=new device, false=disappeared
  uint32_t ts_ms;
  DtDevType type;
};

// ── State ─────────────────────────────────────────────────────────────────────
#ifdef BOARD_HAS_PSRAM
static DtDevice* g_dt_devs;
#else
static DtDevice g_dt_devs[DT_MAX_DEVICES];
#endif
static volatile int g_dt_dev_count = 0;
#ifdef BOARD_HAS_PSRAM
static DtAP*     g_dt_aps;
#else
static DtAP     g_dt_aps[DT_MAX_APS];
#endif
static volatile int g_dt_ap_count = 0;
#ifdef BOARD_HAS_PSRAM
static DtAlert*  g_dt_alerts;
#else
static DtAlert  g_dt_alerts[DT_MAX_ALERTS];
#endif
static volatile int g_dt_alert_count = 0;
static volatile int g_dt_alert_head = 0;
static volatile uint32_t g_dt_rx_total = 0;
static volatile uint32_t g_dt_new_devices = 0;
static bool     g_dt_running = false;
static TaskHandle_t g_dt_task = nullptr;
static SemaphoreHandle_t g_dt_mutex = nullptr;

// ── Helper ────────────────────────────────────────────────────────────────────
static void dt_mac2str(const uint8_t* m, char* out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",m[0],m[1],m[2],m[3],m[4],m[5]);
}

static void dt_add_alert(const uint8_t* mac, const char* vendor, int8_t rssi, bool appeared, DtDevType type) {
  int idx = g_dt_alert_head;
  memcpy(g_dt_alerts[idx].mac, mac, 6);
  strncpy(g_dt_alerts[idx].vendor, vendor, 19);
  g_dt_alerts[idx].vendor[19] = 0;
  g_dt_alerts[idx].rssi     = rssi;
  g_dt_alerts[idx].appeared = appeared;
  g_dt_alerts[idx].ts_ms    = millis();
  g_dt_alerts[idx].type     = type;
  g_dt_alert_head = (g_dt_alert_head + 1) % DT_MAX_ALERTS;
  if (g_dt_alert_count < DT_MAX_ALERTS) g_dt_alert_count++;
}

static bool dt_probe_exists(DtDevice* d, const char* ssid) {
  for (int i = 0; i < d->probe_count; i++)
    if (strcmp(d->probe_ssids[i], ssid) == 0) return true;
  return false;
}
static void dt_add_probe(DtDevice* d, const char* ssid) {
  if (!ssid || !ssid[0]) return;
  if (dt_probe_exists(d, ssid)) return;
  if (d->probe_count < DT_MAX_PROBES) {
    strncpy(d->probe_ssids[d->probe_count], ssid, DT_SSID_LEN-1);
    d->probe_ssids[d->probe_count][DT_SSID_LEN-1] = 0;
    d->probe_count++;
  }
}

static DtDevice* dt_find_or_add(const uint8_t* mac, int8_t rssi, bool is_probe) {
  for (int i = 0; i < g_dt_dev_count; i++) {
    if (memcmp(g_dt_devs[i].mac, mac, 6) == 0) {
      g_dt_devs[i].rssi_last   = rssi;
      g_dt_devs[i].rssi_hist.push(rssi);
      g_dt_devs[i].last_seen_ms = millis();
      g_dt_devs[i].present     = true;
      if (is_probe) g_dt_devs[i].last_active_ms = millis();
      return &g_dt_devs[i];
    }
  }
  if (g_dt_dev_count >= DT_MAX_DEVICES) return nullptr;
  DtDevice* d = &g_dt_devs[g_dt_dev_count];
  memset(d, 0, sizeof(DtDevice));
  memcpy(d->mac, mac, 6);
  d->rssi_last     = rssi;
  d->rssi_hist.push(rssi);
  d->first_seen_ms = millis();
  d->last_seen_ms  = millis();
  d->last_active_ms = millis();
  d->present       = true;
  d->alerted       = false;
  d->randomized    = !!(mac[0] & 0x02);
  strncpy(d->vendor, dt_oui_lookup(mac), 19);
  d->vendor[19] = 0;
  d->type = DT_TYPE_UNKNOWN;
  g_dt_dev_count++;
  g_dt_new_devices++;
  return d;
}

static DtAP* dt_find_or_add_ap(const uint8_t* bssid, int8_t rssi, uint8_t ch) {
  for (int i = 0; i < g_dt_ap_count; i++) {
    if (memcmp(g_dt_aps[i].bssid, bssid, 6) == 0) {
      g_dt_aps[i].rssi = rssi;
      g_dt_aps[i].ch   = ch;
      g_dt_aps[i].beacons++;
      g_dt_aps[i].last_seen_ms = millis();
      return &g_dt_aps[i];
    }
  }
  if (g_dt_ap_count >= DT_MAX_APS) return nullptr;
  DtAP* a = &g_dt_aps[g_dt_ap_count];
  memset(a, 0, sizeof(DtAP));
  memcpy(a->bssid, bssid, 6);
  a->rssi = rssi; a->ch = ch; a->beacons = 1;
  a->last_seen_ms = millis();
  g_dt_ap_count++;
  return a;
}

static void dt_ap_add_client(DtAP* ap, const uint8_t* client) {
  if (!ap || !client) return;
  for (int i = 0; i < (int)ap->clients && i < 8; i++)
    if (memcmp(ap->client_macs[i], client, 6) == 0) return;
  if (ap->clients < 8) {
    memcpy(ap->client_macs[ap->clients], client, 6);
    ap->clients++;
  }
}

// ── Promiscuous callback ───────────────────────────────────────────────────────
static void dt_rx(void* buf, wifi_promiscuous_pkt_type_t type) {
  const wifi_promiscuous_pkt_t* p = (const wifi_promiscuous_pkt_t*)buf;
  const uint8_t* f = p->payload;
  uint32_t len = p->rx_ctrl.sig_len;
  int8_t   rssi = p->rx_ctrl.rssi;
  uint8_t  ch   = p->rx_ctrl.channel;

  if (len < 24) return;
  g_dt_rx_total++;

  if (g_dt_mutex) xSemaphoreTake(g_dt_mutex, portMAX_DELAY);

  uint8_t ft = f[0] & 0xFC;
  const uint8_t* dst = f + 4;
  const uint8_t* src = f + 10;
  const uint8_t* bss = f + 16;

  if (type == WIFI_PKT_MGMT) {
    if (ft == 0x80) { // beacon
      DtDevice* d = dt_find_or_add(src, rssi, false);
      if (d) { d->beacons_rx++; d->is_ap = true; d->mgmt_frames++; }
      DtAP* a = dt_find_or_add_ap(src, rssi, ch);
      if (a && len >= 38) {
        // Extract SSID from beacon body
        const uint8_t* ie = f + 36;
        int rem = (int)len - 36;
        while (rem >= 2) {
          if (ie[0] == 0 && ie[1] < 33) {
            strncpy(a->ssid, (const char*)ie+2, ie[1]);
            a->ssid[ie[1]] = 0;
            break;
          }
          rem -= 2 + ie[1]; ie += 2 + ie[1];
        }
      }
    } else if (ft == 0x40) { // probe request
      DtDevice* d = dt_find_or_add(src, rssi, true);
      if (d) {
        d->probes++;
        d->mgmt_frames++;
        // Extract probed SSID
        if (len >= 26) {
          const uint8_t* ie = f + 24;
          int rem = (int)len - 24;
          if (rem >= 2 && ie[0] == 0 && ie[1] > 0 && ie[1] < 33) {
            char ssid[33]; memcpy(ssid, ie+2, ie[1]); ssid[ie[1]] = 0;
            dt_add_probe(d, ssid);
          }
        }
      }
    } else if (ft == 0xC0 || ft == 0xA0) { // deauth or disassoc
      DtDevice* d = dt_find_or_add(src, rssi, false);
      if (d) { d->deauths_rx++; d->mgmt_frames++; }
    } else {
      DtDevice* d = dt_find_or_add(src, rssi, false);
      if (d) d->mgmt_frames++;
    }
  } else if (type == WIFI_PKT_DATA) {
    static const uint8_t BCAST[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    bool is_bc = memcmp(dst, BCAST, 6) == 0;
    DtDevice* ds = dt_find_or_add(src, rssi, true);
    if (ds) {
      ds->data_frames++;
      memcpy(ds->assoc_ap, bss, 6);
      // Reclassify type with more data
      if (ds->type == DT_TYPE_UNKNOWN || (ds->data_frames % 20 == 0))
        ds->type = dt_classify(ds->vendor, ds->probes, ds->beacons_rx, ds->data_frames);
    }
    // Track AP associations
    DtAP* a = dt_find_or_add_ap(bss, rssi, ch);
    if (a && !is_bc) dt_ap_add_client(a, src);
  }

  if (g_dt_mutex) xSemaphoreGive(g_dt_mutex);
}

// ── Background sweep: mark devices as absent ──────────────────────────────────
static void dt_run_task(void*) {
  while (g_dt_running) {
    vTaskDelay(pdMS_TO_TICKS(5000));
    uint32_t now = millis();
    for (int i = 0; i < g_dt_dev_count; i++) {
      DtDevice* d = &g_dt_devs[i];
      bool was_present = d->present;
      d->present = (now - d->last_seen_ms) < 60000;
      // Alert on new devices
      if (!d->alerted) {
        d->alerted = true;
        dt_add_alert(d->mac, d->vendor, d->rssi_last, true, d->type);
      }
      // Alert on disappeared
      if (was_present && !d->present) {
        dt_add_alert(d->mac, d->vendor, d->rssi_last, false, d->type);
      }
      // Update type classification periodically
      if (d->data_frames > 0 && (d->data_frames % 50 == 0)) {
        d->type = dt_classify(d->vendor, d->probes, d->beacons_rx, d->data_frames);
      }
    }
  }
  g_dt_task = nullptr;
  vTaskDelete(nullptr);
}

// ── Start/Stop ────────────────────────────────────────────────────────────────
void aegisSniff(void*, wifi_promiscuous_pkt_type_t);
static bool devt_start(const String&) {
  if (g_dt_running) return false;
  g_dt_dev_count = 0; g_dt_ap_count = 0;
  g_dt_alert_count = 0; g_dt_alert_head = 0;
  g_dt_rx_total = 0; g_dt_new_devices = 0;
  memset(g_dt_devs, 0, sizeof(g_dt_devs));
  memset(g_dt_aps, 0, sizeof(g_dt_aps));
  memset(g_dt_alerts, 0, sizeof(g_dt_alerts));
  if (!g_dt_mutex) g_dt_mutex = xSemaphoreCreateMutex();
  wifi_promiscuous_filter_t flt = {};
  flt.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA;
  esp_wifi_set_promiscuous_filter(&flt);
  esp_wifi_set_promiscuous_rx_cb(dt_rx);
  esp_wifi_set_promiscuous(true);
  g_dt_running = true;
  xTaskCreate(dt_run_task, "devtrack", 4096, nullptr, 2, &g_dt_task);
  return true;
}
static void devt_stop() {
  g_dt_running = false;
  if (g_dt_task) { vTaskDelay(pdMS_TO_TICKS(100)); g_dt_task = nullptr; }
  esp_wifi_set_promiscuous_rx_cb(aegisSniff);
  wifi_promiscuous_filter_t flt = {};
  flt.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA;
  esp_wifi_set_promiscuous_filter(&flt);
  esp_wifi_set_promiscuous(true);
}

// ── Routes ────────────────────────────────────────────────────────────────────
static void devt_routes() {
  extern WebServer server;

  server.on("/api/devtrack/start", HTTP_POST, [](){
    bool ok = tool_start("devtrack", "");
    server.send(200,"application/json",ok?"{\"ok\":1}":"{\"ok\":0,\"err\":\"busy\"}");
  });
  server.on("/api/devtrack/stop", HTTP_POST, [](){
    tool_stop("devtrack");
    server.send(200,"application/json","{\"ok\":1}");
  });

  server.on("/api/devtrack/status", HTTP_GET, [](){
    String j = "{";
    j += "\"running\":"    + String(g_dt_running?1:0);
    j += ",\"devices\":"   + String(g_dt_dev_count);
    j += ",\"aps\":"       + String(g_dt_ap_count);
    j += ",\"rx_total\":"  + String(g_dt_rx_total);
    j += ",\"new_devs\":"  + String(g_dt_new_devices);
    j += ",\"alerts\":"    + String(g_dt_alert_count);
    j += "}";
    server.send(200,"application/json",j);
  });

  server.on("/api/devtrack/devices", HTTP_GET, [](){
    // Sort by last_seen descending before serializing
    int limit = 50;
    if (server.hasArg("limit")) limit = server.arg("limit").toInt();
    if (limit > 96) limit = 96;
    String j = "[";
    bool first = true;
    char mac_str[18];
    for (int i = 0; i < g_dt_dev_count && limit-- > 0; i++) {
      DtDevice* d = &g_dt_devs[i];
      if (first) first = false; else j += ",";
      dt_mac2str(d->mac, mac_str);
      j += "{\"mac\":\"" + String(mac_str) + "\"";
      j += ",\"vendor\":\"" + String(d->vendor) + "\"";
      j += ",\"type\":\"" + String(DT_TYPE_NAMES[d->type]) + "\"";
      j += ",\"icon\":\"" + String(DT_TYPE_ICONS[d->type]) + "\"";
      j += ",\"rssi\":" + String(d->rssi_last);
      j += ",\"rssi_avg\":" + String((int)d->rssi_hist.avg());
      j += ",\"rssi_min\":" + String(d->rssi_hist.min_v());
      j += ",\"rssi_max\":" + String(d->rssi_hist.max_v());
      j += ",\"probes\":" + String(d->probes);
      j += ",\"data\":" + String(d->data_frames);
      j += ",\"mgmt\":" + String(d->mgmt_frames);
      j += ",\"deauths\":" + String(d->deauths_rx);
      j += ",\"present\":" + String(d->present?1:0);
      j += ",\"rand\":" + String(d->randomized?1:0);
      j += ",\"is_ap\":" + String(d->is_ap?1:0);
      j += ",\"first_ms\":" + String(d->first_seen_ms);
      j += ",\"last_ms\":" + String(d->last_seen_ms);
      // Probe SSIDs
      j += ",\"probe_ssids\":[";
      for (int k = 0; k < d->probe_count; k++) {
        if (k) j += ",";
        j += "\""; j += d->probe_ssids[k]; j += "\"";
      }
      j += "]";
      // AP association
      if (d->assoc_ap[0] || d->assoc_ap[1]) {
        dt_mac2str(d->assoc_ap, mac_str);
        j += ",\"ap\":\"" + String(mac_str) + "\"";
      }
      j += "}";
    }
    j += "]";
    server.send(200,"application/json",j);
  });

  server.on("/api/devtrack/seen", HTTP_GET, [](){
    String j = "[";
    bool first = true;
    char mac_str[18];
    uint32_t now = millis();
    for (int i = 0; i < g_dt_dev_count; i++) {
      if (now - g_dt_devs[i].last_seen_ms > 30000) continue;
      if (first) first = false; else j += ",";
      dt_mac2str(g_dt_devs[i].mac, mac_str);
      j += "{\"mac\":\"" + String(mac_str) + "\"";
      j += ",\"rssi\":" + String(g_dt_devs[i].rssi_last);
      j += ",\"vendor\":\"" + String(g_dt_devs[i].vendor) + "\"";
      j += ",\"ago\":" + String((now-g_dt_devs[i].last_seen_ms)/1000) + "}";
    }
    j += "]";
    server.send(200,"application/json",j);
  });

  server.on("/api/devtrack/alerts", HTTP_GET, [](){
    String j = "[";
    bool first = true;
    char mac_str[18];
    for (int i = 0; i < g_dt_alert_count; i++) {
      int idx = ((g_dt_alert_head - 1 - i) + DT_MAX_ALERTS) % DT_MAX_ALERTS;
      if (first) first = false; else j += ",";
      dt_mac2str(g_dt_alerts[idx].mac, mac_str);
      j += "{\"mac\":\"" + String(mac_str) + "\"";
      j += ",\"vendor\":\"" + String(g_dt_alerts[idx].vendor) + "\"";
      j += ",\"rssi\":" + String(g_dt_alerts[idx].rssi);
      j += ",\"appeared\":" + String(g_dt_alerts[idx].appeared?1:0);
      j += ",\"ts\":" + String(g_dt_alerts[idx].ts_ms);
      j += ",\"type\":\"" + String(DT_TYPE_NAMES[g_dt_alerts[idx].type]) + "\"}";
    }
    j += "]";
    server.send(200,"application/json",j);
  });

  server.on("/api/devtrack/ap_clients", HTTP_GET, [](){
    String j = "[";
    bool first = true;
    char mac_str[18];
    for (int i = 0; i < g_dt_ap_count; i++) {
      DtAP* a = &g_dt_aps[i];
      if (first) first = false; else j += ",";
      dt_mac2str(a->bssid, mac_str);
      j += "{\"bssid\":\"" + String(mac_str) + "\"";
      j += ",\"ssid\":\"" + String(a->ssid) + "\"";
      j += ",\"ch\":" + String(a->ch);
      j += ",\"rssi\":" + String(a->rssi);
      j += ",\"beacons\":" + String(a->beacons);
      j += ",\"clients\":" + String(a->clients);
      j += ",\"client_macs\":[";
      for (int k = 0; k < (int)a->clients && k < 8; k++) {
        if (k) j += ",";
        dt_mac2str(a->client_macs[k], mac_str);
        j += "\"" + String(mac_str) + "\"";
      }
      j += "]}";
    }
    j += "]";
    server.send(200,"application/json",j);
  });
}

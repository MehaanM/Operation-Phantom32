#pragma once
// freq_scanner.h — 2.4GHz and 5GHz frequency scanning with signal strength
// mapping, channel utilization tracking, interference detection, and
// occupancy scoring across all 802.11 channels. Prefix: fsc_

#include <Arduino.h>
#include <WebServer.h>
#include <esp_wifi.h>

extern WebServer server;

#define FSC_MAX_APS_PER_CH 24
#define FSC_MAX_TOTAL_APS 256
#define FSC_HISTORY_SLOTS 60  // 60 seconds of history
#define FSC_SSID_LEN 33

// 5GHz channel list (non-DFS + DFS)
static const uint8_t FSC_5GHZ[] = {36,  40,  44,  48,  52,  56,  60,  64,  100, 104, 108, 112,
                                   116, 120, 124, 128, 132, 136, 140, 149, 153, 157, 161, 165};
#define FSC_5GHZ_COUNT 24

// 2.4GHz channel list (1-14)
#define FSC_24GHZ_COUNT 14

// ─── AP snapshot ─────────────────────────────────────────────────────────────
struct FscAP {
  bool active;
  uint8_t bssid[6];
  char ssid[FSC_SSID_LEN];
  uint8_t channel;
  int8_t rssi;
  int8_t rssi_min;
  int8_t rssi_max;
  uint8_t security;
  uint32_t first_seen;
  uint32_t last_seen;
  uint32_t beacon_count;
  bool is_5ghz;
  uint8_t bandwidth;  // 20, 40, 80, 160 MHz
  bool he_capable;    // 802.11ax
  bool vht_capable;   // 802.11ac
  bool ht_capable;    // 802.11n
};

// ─── Per-channel state ────────────────────────────────────────────────────────
struct FscChannel {
  uint8_t number;
  bool is_5ghz;
  int8_t noise_floor;                          // estimated noise floor dBm
  int8_t peak_rssi;                            // strongest AP seen
  uint8_t ap_count;                            // APs on this channel
  uint32_t frames_total;                       // frames seen on this channel
  uint32_t frames_history[FSC_HISTORY_SLOTS];  // per-second counts
  uint8_t history_slot;
  uint32_t history_slot_ts;
  float utilization;            // 0.0-1.0 estimated utilization
  uint32_t interference_count;  // non-802.11 interference events
  int8_t avg_rssi;
  uint32_t dwell_ms;     // total time spent on this channel
  uint32_t dwell_count;  // how many times we hopped here
};

// ─── State ────────────────────────────────────────────────────────────────────
static struct {
  FscAP aps[FSC_MAX_TOTAL_APS];
  FscChannel ch24[FSC_24GHZ_COUNT];  // ch24[0] = channel 1
  FscChannel ch5[FSC_5GHZ_COUNT];
  uint16_t ap_count;
  bool scanning;
  uint32_t scan_start_ts;
  uint32_t scan_complete_ts;
  uint32_t total_frames;
  uint8_t scan_pass;
  // Currently dwell channel
  uint8_t current_channel;
  uint32_t current_dwell_start;
  TaskHandle_t task;
  // Scan config
  uint32_t dwell_ms_per_ch;
  bool include_5ghz;
  bool passive_only;
} g_fsc = {};

// ─── Channel → index ─────────────────────────────────────────────────────────
static FscChannel *fsc_get_ch(uint8_t ch) {
  if (ch >= 1 && ch <= 14)
    return &g_fsc.ch24[ch - 1];
  for (int i = 0; i < FSC_5GHZ_COUNT; i++) {
    if (FSC_5GHZ[i] == ch)
      return &g_fsc.ch5[i];
  }
  return nullptr;
}

static bool fsc_is_5ghz(uint8_t ch) {
  for (int i = 0; i < FSC_5GHZ_COUNT; i++)
    if (FSC_5GHZ[i] == ch)
      return true;
  return false;
}

// ─── Helpers ─────────────────────────────────────────────────────────────────
static bool fsc_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static FscAP *fsc_find_ap(const uint8_t *bssid) {
  for (int i = 0; i < FSC_MAX_TOTAL_APS; i++) {
    if (g_fsc.aps[i].active && fsc_mac_eq(g_fsc.aps[i].bssid, bssid))
      return &g_fsc.aps[i];
  }
  return nullptr;
}

static FscAP *fsc_alloc_ap(const uint8_t *bssid) {
  for (int i = 0; i < FSC_MAX_TOTAL_APS; i++) {
    if (!g_fsc.aps[i].active) {
      FscAP &a = g_fsc.aps[i];
      memset(&a, 0, sizeof(a));
      memcpy(a.bssid, bssid, 6);
      a.active = true;
      a.first_seen = millis() / 1000;
      a.rssi_min = -100;
      a.rssi_max = -100;
      g_fsc.ap_count++;
      return &a;
    }
  }
  return nullptr;
}

// ─── Build active scan probe ──────────────────────────────────────────────────
static void fsc_send_probe(uint8_t channel) {
  static const uint8_t probe[] = {
      // Radiotap
      0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
      // MAC header - Probe Request
      0x40, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // DA
      0x02, 0x00, 0xFA, 0xFB, 0xFC, 0xFD,                          // SA (fake LA)
      0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,                          // BSSID (broadcast)
      0x00, 0x00,                                                  // seq ctrl
      0x00, 0x00,                                                  // SSID IE: empty (wildcard)
      0x01, 0x08, 0x82, 0x84, 0x8B, 0x96, 0x24, 0x30, 0x48, 0x6C,  // rates
      0x03, 0x01, 0x00  // DS param (channel filled below)
  };
  uint8_t pkt[80];
  memcpy(pkt, probe, sizeof(probe));
  pkt[sizeof(probe) - 1] = channel;
  esp_wifi_80211_tx(WIFI_IF_AP, pkt, sizeof(probe), false);
}

// ─── Channel tick ─────────────────────────────────────────────────────────────
static void fsc_tick_channel(FscChannel &ch) {
  uint32_t now_ms = millis();
  if (now_ms - ch.history_slot_ts >= 1000) {
    ch.history_slot = (ch.history_slot + 1) % FSC_HISTORY_SLOTS;
    ch.frames_history[ch.history_slot] = 0;
    ch.history_slot_ts = now_ms;
  }

  // Estimate utilization from frames per second
  uint32_t sum = 0;
  for (int i = 0; i < FSC_HISTORY_SLOTS; i++)
    sum += ch.frames_history[i];
  float fps = (float)sum / FSC_HISTORY_SLOTS;
  // Rough estimate: 100 fps = fully utilized for 2.4GHz AP
  ch.utilization = min(1.0f, fps / 100.0f);
}

// ─── Scan task ────────────────────────────────────────────────────────────────
static void fsc_task(void *arg) {
  int total_channels = g_fsc.include_5ghz ? FSC_24GHZ_COUNT + FSC_5GHZ_COUNT : FSC_24GHZ_COUNT;
  g_fsc.scan_pass = 0;

  while (g_fsc.scanning) {
    // Hop through channels
    for (int ci = 0; ci < total_channels && g_fsc.scanning; ci++) {
      uint8_t ch;
      if (ci < FSC_24GHZ_COUNT)
        ch = ci + 1;
      else
        ch = FSC_5GHZ[ci - FSC_24GHZ_COUNT];

      g_fsc.current_channel = ch;
      g_fsc.current_dwell_start = millis();

      wifi_second_chan_t sec = WIFI_SECOND_CHAN_NONE;
      esp_wifi_set_channel(ch, sec);

      if (!g_fsc.passive_only)
        fsc_send_probe(ch);

      vTaskDelay(pdMS_TO_TICKS(g_fsc.dwell_ms_per_ch));

      FscChannel *fch = fsc_get_ch(ch);
      if (fch) {
        fch->dwell_ms += millis() - g_fsc.current_dwell_start;
        fch->dwell_count++;
        fsc_tick_channel(*fch);
      }
    }
    g_fsc.scan_pass++;
    g_fsc.scan_complete_ts = millis() / 1000;
  }

  g_fsc.scanning = false;
  g_fsc.task = nullptr;
  vTaskDelete(nullptr);
}

// ─── Ingest from promiscuous callback ────────────────────────────────────────
void fsc_ingest(const uint8_t *raw, int len, const uint8_t *bssid, const char *ssid, uint8_t type,
                uint8_t sub, int8_t rssi, uint8_t channel) {
  g_fsc.total_frames++;

  FscChannel *ch = fsc_get_ch(channel);
  if (ch) {
    ch->frames_total++;
    ch->frames_history[ch->history_slot]++;
    if (rssi > ch->peak_rssi)
      ch->peak_rssi = rssi;
    // Running average RSSI
    ch->avg_rssi = (ch->avg_rssi == 0) ? rssi : (ch->avg_rssi * 7 + rssi) / 8;
  }

  // Only process beacons/probe responses for AP tracking
  if (type != 0 || (sub != 8 && sub != 5))
    return;
  if (!bssid)
    return;

  FscAP *ap = fsc_find_ap(bssid);
  if (!ap) {
    ap = fsc_alloc_ap(bssid);
    if (ch)
      ch->ap_count++;
  }
  if (!ap)
    return;

  ap->last_seen = millis() / 1000;
  ap->beacon_count++;
  ap->rssi = rssi;
  if (rssi < ap->rssi_min)
    ap->rssi_min = rssi;
  if (rssi > ap->rssi_max)
    ap->rssi_max = rssi;
  ap->channel = channel;
  ap->is_5ghz = fsc_is_5ghz(channel);
  if (ssid && strlen(ssid) > 0 && strlen(ssid) < FSC_SSID_LEN)
    strncpy(ap->ssid, ssid, FSC_SSID_LEN - 1);

  // Parse HT/VHT/HE caps from IEs
  if (raw && len > 44) {
    int ie_off = 8 + 24 + 12;
    int i = ie_off;
    while (i + 2 <= len) {
      uint8_t tag = raw[i], tlen = raw[i + 1];
      if (i + 2 + tlen > len)
        break;
      if (tag == 45)
        ap->ht_capable = true;  // HT Caps
      if (tag == 191)
        ap->vht_capable = true;  // VHT Caps
      if (tag == 255 && tlen > 0 && raw[i + 2] == 35)
        ap->he_capable = true;  // HE Caps ext
      i += 2 + tlen;
    }
  }
}

// ─── Control ─────────────────────────────────────────────────────────────────
inline void fsc_start(uint32_t dwell_ms = 150, bool inc_5ghz = false, bool passive = true) {
  if (g_fsc.scanning)
    return;
  g_fsc.dwell_ms_per_ch = dwell_ms < 50 ? 50 : dwell_ms;
  g_fsc.include_5ghz = inc_5ghz;
  g_fsc.passive_only = passive;
  g_fsc.scanning = true;
  g_fsc.scan_start_ts = millis() / 1000;

  // Init channels
  for (int i = 0; i < FSC_24GHZ_COUNT; i++) {
    g_fsc.ch24[i].number = i + 1;
    g_fsc.ch24[i].is_5ghz = false;
    g_fsc.ch24[i].noise_floor = -95;
    if (g_fsc.ch24[i].history_slot_ts == 0)
      g_fsc.ch24[i].history_slot_ts = millis();
  }
  for (int i = 0; i < FSC_5GHZ_COUNT; i++) {
    g_fsc.ch5[i].number = FSC_5GHZ[i];
    g_fsc.ch5[i].is_5ghz = true;
    g_fsc.ch5[i].noise_floor = -95;
    if (g_fsc.ch5[i].history_slot_ts == 0)
      g_fsc.ch5[i].history_slot_ts = millis();
  }

  xTaskCreate(fsc_task, "fsc_scan", 2048, nullptr, 5, &g_fsc.task);
}

inline void fsc_stop() {
  g_fsc.scanning = false;
  uint32_t deadline = millis() + 1000;
  while (g_fsc.task && millis() < deadline)
    vTaskDelay(50);
}

// ─── JSON ─────────────────────────────────────────────────────────────────────
static void fsc_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String fsc_json_channels(bool ghz5) {
  String s = "[";
  int cnt = ghz5 ? FSC_5GHZ_COUNT : FSC_24GHZ_COUNT;
  FscChannel *chs = ghz5 ? g_fsc.ch5 : g_fsc.ch24;
  for (int i = 0; i < cnt; i++) {
    if (i)
      s += ",";
    FscChannel &ch = chs[i];
    s += "{\"ch\":" + String(ch.number);
    s += ",\"ap_count\":" + String(ch.ap_count);
    s += ",\"frames\":" + String(ch.frames_total);
    s += ",\"utilization\":" + String(ch.utilization, 3);
    s += ",\"peak_rssi\":" + String(ch.peak_rssi);
    s += ",\"avg_rssi\":" + String(ch.avg_rssi);
    s += ",\"noise_floor\":" + String(ch.noise_floor);
    s += ",\"dwell_ms\":" + String(ch.dwell_ms);
    s += "}";
  }
  s += "]";
  return s;
}

static String fsc_json_aps(int limit = 50) {
  String s = "{\"count\":" + String(g_fsc.ap_count) + ",\"aps\":[";
  char mac[18];
  int shown = 0;
  bool first = true;
  for (int i = 0; i < FSC_MAX_TOTAL_APS && shown < limit; i++) {
    FscAP &a = g_fsc.aps[i];
    if (!a.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    shown++;
    fsc_mac_str(a.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(a.ssid) + "\"";
    s += ",\"ch\":" + String(a.channel);
    s += ",\"rssi\":" + String(a.rssi);
    s += ",\"rssi_min\":" + String(a.rssi_min);
    s += ",\"rssi_max\":" + String(a.rssi_max);
    s += ",\"is_5ghz\":" + String(a.is_5ghz ? "true" : "false");
    s += ",\"ht\":" + String(a.ht_capable ? "true" : "false");
    s += ",\"vht\":" + String(a.vht_capable ? "true" : "false");
    s += ",\"he\":" + String(a.he_capable ? "true" : "false");
    s += ",\"beacons\":" + String(a.beacon_count);
    s += ",\"last_seen\":" + String(a.last_seen);
    s += "}";
  }
  s += "]}";
  return s;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void fsc_routes() {
  server.on("/api/fsc/status", HTTP_GET, []() {
    String s = "{\"scanning\":" + String(g_fsc.scanning ? "true" : "false");
    s += ",\"total_frames\":" + String(g_fsc.total_frames);
    s += ",\"ap_count\":" + String(g_fsc.ap_count);
    s += ",\"scan_pass\":" + String(g_fsc.scan_pass);
    s += ",\"current_ch\":" + String(g_fsc.current_channel);
    s += ",\"ch24\":" + fsc_json_channels(false);
    s += ",\"ch5\":" + fsc_json_channels(true);
    s += "}";
    server.send(200, "application/json", s);
  });

  server.on("/api/fsc/aps", HTTP_GET, []() {
    int lim = server.hasArg("limit") ? server.arg("limit").toInt() : 50;
    server.send(200, "application/json", fsc_json_aps(lim));
  });

  server.on("/api/fsc/start", HTTP_POST, []() {
    int dwell = server.hasArg("dwell") ? server.arg("dwell").toInt() : 150;
    bool inc5 = server.hasArg("5ghz");
    bool passive = !server.hasArg("active");
    fsc_start(dwell, inc5, passive);
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/fsc/stop", HTTP_POST, []() {
    fsc_stop();
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/fsc/clear", HTTP_POST, []() {
    bool sc = g_fsc.scanning;
    memset(g_fsc.aps, 0, sizeof(g_fsc.aps));
    memset(g_fsc.ch24, 0, sizeof(g_fsc.ch24));
    memset(g_fsc.ch5, 0, sizeof(g_fsc.ch5));
    g_fsc.ap_count = 0;
    g_fsc.total_frames = 0;
    g_fsc.scanning = sc;
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

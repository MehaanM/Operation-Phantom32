#pragma once
// hidden_ssid_finder.h — Hidden SSID uncloaking via probe response correlation.
// Tracks zero-length SSID beacons, listens for probe responses that reveal
// the real SSID, correlates by BSSID. Prefix: hsf_

#include <Arduino.h>
#include <WebServer.h>
#include <esp_wifi.h>

extern WebServer server;

#define HSF_MAX_HIDDEN 32
#define HSF_SSID_LEN 33
#define HSF_MAX_LOG 64

// ─── Hidden AP record ─────────────────────────────────────────────────────────
struct HsfEntry {
  bool active;
  uint8_t bssid[6];
  uint8_t channel;
  int8_t rssi;
  char revealed_ssid[HSF_SSID_LEN];
  bool ssid_known;
  uint32_t first_seen;
  uint32_t last_seen;
  uint32_t beacon_count;
  uint32_t probe_resp_count;
  // Probe requests captured targeting this AP
  uint32_t probe_req_count;
  uint8_t security;  // 0=open, 1=WPA, 2=WPA2, 3=WPA3
};

struct HsfLog {
  uint32_t ts;
  uint8_t bssid[6];
  char ssid[HSF_SSID_LEN];
  uint8_t source;  // 0=probe resp, 1=assoc resp, 2=reassoc
};

// ─── State ────────────────────────────────────────────────────────────────────
static struct {
  HsfEntry entries[HSF_MAX_HIDDEN];
  HsfLog log[HSF_MAX_LOG];
  uint8_t entry_count;
  uint8_t log_head;
  uint32_t total_reveals;
  uint32_t total_hidden_seen;
  // Active probing state
  bool active_probe;
  uint8_t probe_target[6];
  uint32_t probe_sent;
  uint32_t probe_last_ts;
  uint32_t probe_interval_ms;
  TaskHandle_t probe_task;
} g_hsf = {};

// ─── Helpers ─────────────────────────────────────────────────────────────────
static bool hsf_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static HsfEntry *hsf_find(const uint8_t *bssid) {
  for (int i = 0; i < HSF_MAX_HIDDEN; i++) {
    if (g_hsf.entries[i].active && hsf_mac_eq(g_hsf.entries[i].bssid, bssid))
      return &g_hsf.entries[i];
  }
  return nullptr;
}

static HsfEntry *hsf_alloc(const uint8_t *bssid) {
  for (int i = 0; i < HSF_MAX_HIDDEN; i++) {
    if (!g_hsf.entries[i].active) {
      memset(&g_hsf.entries[i], 0, sizeof(g_hsf.entries[i]));
      memcpy(g_hsf.entries[i].bssid, bssid, 6);
      g_hsf.entries[i].active = true;
      g_hsf.entries[i].first_seen = millis() / 1000;
      g_hsf.entry_count++;
      return &g_hsf.entries[i];
    }
  }
  return nullptr;
}

static void hsf_log_reveal(const uint8_t *bssid, const char *ssid, uint8_t source) {
  HsfLog &l = g_hsf.log[g_hsf.log_head % HSF_MAX_LOG];
  l.ts = millis() / 1000;
  memcpy(l.bssid, bssid, 6);
  strncpy(l.ssid, ssid, HSF_SSID_LEN - 1);
  l.source = source;
  g_hsf.log_head++;
  g_hsf.total_reveals++;
}

// ─── Parse IE to get SSID ────────────────────────────────────────────────────
static bool hsf_extract_ssid(const uint8_t *ie_start, int ie_len, char *ssid_out) {
  int i = 0;
  while (i + 2 <= ie_len) {
    uint8_t tag = ie_start[i], tlen = ie_start[i + 1];
    if (i + 2 + tlen > ie_len)
      break;
    if (tag == 0 && tlen > 0 && tlen < HSF_SSID_LEN) {
      memcpy(ssid_out, ie_start + i + 2, tlen);
      ssid_out[tlen] = '\0';
      // Check it's printable
      for (int j = 0; j < tlen; j++) {
        if (ssid_out[j] < 0x20) {
          ssid_out[0] = '\0';
          return false;
        }
      }
      return true;
    }
    i += 2 + tlen;
  }
  return false;
}

// ─── Build directed probe request ────────────────────────────────────────────
static uint16_t hsf_build_probe_req(uint8_t *buf, int max_len, const uint8_t *target_bssid,
                                    const uint8_t *our_mac, uint8_t channel) {
  if (max_len < 64)
    return 0;
  int pos = 0;

  // Radiotap
  buf[pos++] = 0x00;
  buf[pos++] = 0x00;
  buf[pos++] = 0x08;
  buf[pos++] = 0x00;
  buf[pos++] = 0x00;
  buf[pos++] = 0x00;
  buf[pos++] = 0x00;
  buf[pos++] = 0x00;

  // MAC header - Probe Request (type=0, sub=4)
  buf[pos++] = 0x40;
  buf[pos++] = 0x00;
  buf[pos++] = 0x00;
  buf[pos++] = 0x00;
  memcpy(buf + pos, target_bssid, 6);
  pos += 6;  // DA = target BSSID
  memcpy(buf + pos, our_mac, 6);
  pos += 6;
  memcpy(buf + pos, target_bssid, 6);
  pos += 6;
  buf[pos++] = 0x00;
  buf[pos++] = 0x00;

  // SSID IE: empty (wildcard)
  buf[pos++] = 0x00;
  buf[pos++] = 0x00;

  // Supported rates
  buf[pos++] = 0x01;
  buf[pos++] = 0x08;
  uint8_t rates[] = {0x82, 0x84, 0x8B, 0x96, 0x24, 0x30, 0x48, 0x6C};
  memcpy(buf + pos, rates, 8);
  pos += 8;

  // DS param
  buf[pos++] = 0x03;
  buf[pos++] = 0x01;
  buf[pos++] = channel;

  return (uint16_t)pos;
}

// ─── Active probe task ────────────────────────────────────────────────────────
static void hsf_probe_task(void *arg) {
  uint8_t our_mac[6] = {0x02, 0x00, 0xDE, 0xAD, 0xBE, 0xEF};
  esp_read_mac(our_mac, ESP_MAC_WIFI_SOFTAP);

  HsfEntry *target = hsf_find(g_hsf.probe_target);

  while (g_hsf.active_probe) {
    uint8_t buf[128];
    uint8_t ch = target ? target->channel : 1;
    uint16_t len = hsf_build_probe_req(buf, sizeof(buf), g_hsf.probe_target, our_mac, ch);
    if (len > 0) {
      esp_wifi_80211_tx(WIFI_IF_AP, buf, len, false);
      g_hsf.probe_sent++;
    }
    vTaskDelay(pdMS_TO_TICKS(g_hsf.probe_interval_ms));
  }
  g_hsf.probe_task = nullptr;
  vTaskDelete(nullptr);
}

// ─── Ingest 802.11 frame ─────────────────────────────────────────────────────
void hsf_ingest(const uint8_t *raw, int len, const uint8_t *src, const uint8_t *bssid, uint8_t type,
                uint8_t sub, int8_t rssi, uint8_t channel) {
  // Beacon with zero-length or zero-content SSID = hidden
  if (type == 0 && sub == 8) {
    int ie_off = 8 + 24 + 12;
    if (ie_off + 2 > len)
      return;
    uint8_t ssid_tag = raw[ie_off];
    uint8_t ssid_len = raw[ie_off + 1];
    if (ssid_tag == 0 && ssid_len == 0) {
      g_hsf.total_hidden_seen++;
      HsfEntry *e = hsf_find(bssid);
      if (!e)
        e = hsf_alloc(bssid);
      if (!e)
        return;
      e->beacon_count++;
      e->last_seen = millis() / 1000;
      e->rssi = rssi;
      e->channel = channel;
    }
    return;
  }

  // Probe Response → may reveal SSID
  if (type == 0 && sub == 5) {
    int ie_off = 8 + 24 + 12;
    if (ie_off >= len)
      return;

    char ssid[HSF_SSID_LEN] = "";
    if (!hsf_extract_ssid(raw + ie_off, len - ie_off, ssid))
      return;
    if (strlen(ssid) == 0)
      return;

    HsfEntry *e = hsf_find(bssid);
    if (!e) {
      // New reveal from a previously unknown hidden AP
      e = hsf_alloc(bssid);
      if (!e)
        return;
      e->channel = channel;
    }
    e->probe_resp_count++;
    e->last_seen = millis() / 1000;
    e->rssi = rssi;

    if (!e->ssid_known) {
      strncpy(e->revealed_ssid, ssid, HSF_SSID_LEN - 1);
      e->ssid_known = true;
      hsf_log_reveal(bssid, ssid, 0);
    }
  }

  // Association Response (sub=1) → reveals SSID
  if (type == 0 && (sub == 1 || sub == 3)) {
    int ie_off = 8 + 24 + 6;  // assoc resp: caps(2) + status(2) + aid(2)
    if (ie_off >= len)
      return;
    char ssid[HSF_SSID_LEN] = "";
    if (!hsf_extract_ssid(raw + ie_off, len - ie_off, ssid))
      return;
    if (strlen(ssid) == 0)
      return;
    HsfEntry *e = hsf_find(bssid);
    if (e && !e->ssid_known) {
      strncpy(e->revealed_ssid, ssid, HSF_SSID_LEN - 1);
      e->ssid_known = true;
      hsf_log_reveal(bssid, ssid, 1);
    }
  }
}

// ─── JSON ─────────────────────────────────────────────────────────────────────
static void hsf_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String hsf_json_status() {
  String s = "{\"total_hidden\":" + String(g_hsf.total_hidden_seen);
  s += ",\"total_reveals\":" + String(g_hsf.total_reveals);
  s += ",\"active_probe\":" + String(g_hsf.active_probe ? "true" : "false");
  s += ",\"probe_sent\":" + String(g_hsf.probe_sent);
  s += ",\"entries\":[";
  char mac[18];
  bool first = true;
  for (int i = 0; i < HSF_MAX_HIDDEN; i++) {
    HsfEntry &e = g_hsf.entries[i];
    if (!e.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    hsf_mac_str(e.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ch\":" + String(e.channel);
    s += ",\"rssi\":" + String(e.rssi);
    s += ",\"ssid_known\":" + String(e.ssid_known ? "true" : "false");
    s += ",\"ssid\":\"" + String(e.revealed_ssid) + "\"";
    s += ",\"beacons\":" + String(e.beacon_count);
    s += ",\"probe_resps\":" + String(e.probe_resp_count);
    s += ",\"last_seen\":" + String(e.last_seen);
    s += "}";
  }
  s += "],\"log\":[";
  int log_total = min((int)g_hsf.total_reveals, HSF_MAX_LOG);
  for (int i = 0; i < log_total; i++) {
    int idx = ((int)g_hsf.log_head - 1 - i + HSF_MAX_LOG) % HSF_MAX_LOG;
    HsfLog &l = g_hsf.log[idx];
    if (l.ts == 0)
      continue;
    if (i)
      s += ",";
    hsf_mac_str(l.bssid, mac);
    s += "{\"ts\":" + String(l.ts);
    s += ",\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(l.ssid) + "\"";
    s += ",\"source\":" + String(l.source);
    s += "}";
  }
  s += "]}";
  return s;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void hsf_routes() {
  server.on("/api/hsf/status", HTTP_GET,
            []() { server.send(200, "application/json", hsf_json_status()); });

  server.on("/api/hsf/probe/start", HTTP_POST, []() {
    if (!server.hasArg("bssid")) {
      server.send(400, "application/json", "{\"error\":\"missing bssid\"}");
      return;
    }
    sscanf(server.arg("bssid").c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &g_hsf.probe_target[0],
           &g_hsf.probe_target[1], &g_hsf.probe_target[2], &g_hsf.probe_target[3],
           &g_hsf.probe_target[4], &g_hsf.probe_target[5]);
    g_hsf.probe_interval_ms = server.hasArg("interval") ? server.arg("interval").toInt() : 500;
    if (g_hsf.probe_interval_ms < 100)
      g_hsf.probe_interval_ms = 100;
    if (!g_hsf.active_probe) {
      g_hsf.active_probe = true;
      xTaskCreate(hsf_probe_task, "hsf_probe", 2048, nullptr, 5, &g_hsf.probe_task);
    }
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/hsf/probe/stop", HTTP_POST, []() {
    g_hsf.active_probe = false;
    uint32_t deadline = millis() + 500;
    while (g_hsf.probe_task && millis() < deadline)
      vTaskDelay(10);
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/hsf/clear", HTTP_POST, []() {
    bool ap = g_hsf.active_probe;
    memset(&g_hsf, 0, sizeof(g_hsf));
    g_hsf.active_probe = ap;
    g_hsf.probe_interval_ms = 500;
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

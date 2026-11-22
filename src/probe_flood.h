#pragma once
#include <WiFi.h>
#include <esp_wifi.h>
#include "frames.h"
#include "module.h"
#include "safety.h"
static bool pf_running=false;static TaskHandle_t pf_task=nullptr;
static String pf_ssid="Free WiFi";static uint32_t pf_count=500,pf_sent=0;
static void pf_run(void*){
  uint8_t f[128];uint8_t sta[6];
  while(pf_running){
    for(uint8_t ch=1;ch<=13&&pf_running;ch++){
      esp_wifi_set_channel(ch,WIFI_SECOND_CHAN_NONE);
      for(int i=0;i<10&&pf_running;i++){
        for(int k=0;k<6;k++)sta[k]=esp_random()&0xFF;
        sta[0]=(sta[0]&0xFC)|0x02;
        if(!safety_rate_ok(3)){vTaskDelay(pdMS_TO_TICKS(20));i--;continue;}
        size_t n=f_probeReq(f,sta,pf_ssid.c_str());
        esp_wifi_80211_tx(WIFI_IF_AP,f,n,false);pf_sent++;
        if(pf_sent>=pf_count){pf_running=false;break;}
        vTaskDelay(pdMS_TO_TICKS(1));
      }
    }
  }
  pf_running=false;
  g_active=nullptr;
  if(tool_wifi_resume) tool_wifi_resume();
  pf_task=nullptr;vTaskDelete(nullptr);
}
static bool pf_start(const String&q){int i;
  if((i=q.indexOf("ssid="))>=0){int e=q.indexOf('&',i);pf_ssid=q.substring(i+5,e<0?q.length():e);}
  if((i=q.indexOf("count="))>=0){int e=q.indexOf('&',i);pf_count=q.substring(i+6,e<0?q.length():e).toInt();}
  if(!pf_count)pf_count=500;pf_sent=0;pf_running=true;xTaskCreate(pf_run,"probe",4096,nullptr,5,&pf_task);return true;
}
static void pf_stop(){pf_running=false;}
static void pf_routes(){extern WebServer server;
  server.on("/api/probe/start",HTTP_POST,[](){String q;for(int i=0;i<server.args();i++){if(i)q+='&';q+=server.argName(i)+'='+server.arg(i);}bool ok=tool_start("probe",q);server.send(200,"application/json",ok?"{\"ok\":1}":"{\"ok\":0,\"err\":\"busy\"}");});
  server.on("/api/probe/stop",HTTP_POST,[](){tool_stop("probe");server.send(200,"application/json","{\"ok\":1}");});
  server.on("/api/probe/status",HTTP_GET,[](){server.send(200,"application/json","{\"running\":"+String(pf_running?1:0)+",\"sent\":"+String(pf_sent)+",\"msg\":\"sent "+String(pf_sent)+"\"}");});}

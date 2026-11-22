#pragma once
#include <WiFi.h>
#include <esp_wifi.h>
#include "frames.h"
#include "module.h"
#include "safety.h"
static bool bs_running=false;static TaskHandle_t bs_task=nullptr;
static String bs_ssid="Free WiFi";static uint32_t bs_count=200,bs_sent=0;static bool bs_random=true;
static void bs_run(void*){
  uint8_t f[128];uint8_t bssid[6];
  while(bs_running){
    for(uint8_t ch=1;ch<=13&&bs_running;ch++){
      esp_wifi_set_channel(ch,WIFI_SECOND_CHAN_NONE);
      for(int i=0;i<8&&bs_running;i++){
        for(int k=0;k<6;k++)bssid[k]=esp_random()&0xFF;
        bssid[0]=(bssid[0]&0xFC)|0x02;
        if(!bs_random){static const uint8_t fx[6]={0x02,0x00,0x00,0x00,0x00,0x01};memcpy(bssid,fx,6);}
        if(!safety_rate_ok(2)){vTaskDelay(pdMS_TO_TICKS(20));i--;continue;}
        size_t n=f_beacon(f,bssid,bs_ssid.c_str(),ch,true,0);
        esp_wifi_80211_tx(WIFI_IF_AP,f,n,false);bs_sent++;
        if(bs_sent>=bs_count){bs_running=false;break;}
        vTaskDelay(pdMS_TO_TICKS(2));
      }
    }
  }
  bs_running=false;
  g_active=nullptr;
  if(tool_wifi_resume) tool_wifi_resume();
  bs_task=nullptr;vTaskDelete(nullptr);
}
static bool bs_start(const String&q){int i;
  if((i=q.indexOf("ssid="))>=0){int e=q.indexOf('&',i);bs_ssid=q.substring(i+5,e<0?q.length():e);}
  if((i=q.indexOf("count="))>=0){int e=q.indexOf('&',i);bs_count=q.substring(i+6,e<0?q.length():e).toInt();}
  if((i=q.indexOf("random="))>=0)bs_random=q.substring(i+7).toInt()!=0;
  if(!bs_count)bs_count=200;bs_sent=0;bs_running=true;xTaskCreate(bs_run,"beacon",4096,nullptr,5,&bs_task);return true;
}
static void bs_stop(){bs_running=false;}
static void bs_routes(){extern WebServer server;
  server.on("/api/beacon/start",HTTP_POST,[](){String q;for(int i=0;i<server.args();i++){if(i)q+='&';q+=server.argName(i)+'='+server.arg(i);}bool ok=tool_start("beacon",q);server.send(200,"application/json",ok?"{\"ok\":1}":"{\"ok\":0,\"err\":\"busy\"}");});
  server.on("/api/beacon/stop",HTTP_POST,[](){tool_stop("beacon");server.send(200,"application/json","{\"ok\":1}");});
  server.on("/api/beacon/status",HTTP_GET,[](){server.send(200,"application/json","{\"running\":"+String(bs_running?1:0)+",\"sent\":"+String(bs_sent)+",\"msg\":\"sent "+String(bs_sent)+"\"}");});}

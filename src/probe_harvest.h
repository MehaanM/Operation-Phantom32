#pragma once
#include <WiFi.h>
#include <esp_wifi.h>
#include "module.h"
#include "safety.h"

// Probe Request Harvester — passive recon.
// Sniffs probe requests and logs the STA MAC + SSID being probed for
// (reveals the device's saved network history). No frames transmitted.
#define PRH_MAX_ENTRIES 128
struct PrhEntry {
  uint8_t sta[6];
  char    ssid[33];
  int8_t  rssi;
  uint32_t last_seen; // millis()
};
static PrhEntry prh_entries[PRH_MAX_ENTRIES];
static int prh_count = 0;
static bool prh_running = false;
static volatile uint32_t prh_frames = 0;

static void prh_sniff(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT) return;
  const wifi_promiscuous_pkt_t* p = (const wifi_promiscuous_pkt_t*)buf;
  const uint8_t* pl = p->payload;
  if (pl[0] != 0x40) return; // probe request subtype 4
  if (p->rx_ctrl.sig_len < 28) return;
  const uint8_t* sta = pl + 10;
  uint8_t ie_id  = pl[24];
  uint8_t ie_len = pl[25];
  if (ie_id != 0 || ie_len == 0 || ie_len > 32) return; // skip wildcard probes
  if (p->rx_ctrl.sig_len < (uint32_t)(26 + ie_len)) return;
  char ssid[33]; memcpy(ssid, pl + 26, ie_len); ssid[ie_len] = 0;
  for (int i = 0; i < prh_count; i++) {
    if (memcmp(prh_entries[i].sta, sta, 6) == 0 && strcmp(prh_entries[i].ssid, ssid) == 0) {
      prh_entries[i].rssi      = p->rx_ctrl.rssi;
      prh_entries[i].last_seen = millis();
      prh_frames++;
      return;
    }
  }
  if (prh_count >= PRH_MAX_ENTRIES) return;
  memcpy(prh_entries[prh_count].sta, sta, 6);
  strncpy(prh_entries[prh_count].ssid, ssid, 32);
  prh_entries[prh_count].ssid[32] = 0;
  prh_entries[prh_count].rssi      = p->rx_ctrl.rssi;
  prh_entries[prh_count].last_seen = millis();
  prh_count++;
  prh_frames++;
}

void aegisSniff(void*, wifi_promiscuous_pkt_type_t);
static bool prh_start(const String&) {
  prh_count = 0; prh_frames = 0;
  wifi_promiscuous_filter_t f = {}; f.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT;
  esp_wifi_set_promiscuous_filter(&f);
  esp_wifi_set_promiscuous_rx_cb(prh_sniff);
  esp_wifi_set_promiscuous(true);
  prh_running = true;
  return true;
}
static void prh_stop() {
  prh_running = false;
  esp_wifi_set_promiscuous_rx_cb(aegisSniff);
  wifi_promiscuous_filter_t f = {};
  f.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA;
  esp_wifi_set_promiscuous_filter(&f);
  esp_wifi_set_promiscuous(true);
}
static void prh_routes() {
  extern WebServer server;
  server.on("/api/probeharvest/start", HTTP_POST, [](){
    String q; for(int i=0;i<server.args();i++){if(i)q+='&';q+=server.argName(i)+'='+server.arg(i);}
    bool ok = tool_start("probeharvest", q);
    server.send(200,"application/json",ok?"{\"ok\":1}":"{\"ok\":0,\"err\":\"busy\"}");
  });
  server.on("/api/probeharvest/stop", HTTP_POST, [](){
    tool_stop("probeharvest"); server.send(200,"application/json","{\"ok\":1}");
  });
  server.on("/api/probeharvest/status", HTTP_GET, [](){
    server.send(200,"application/json",
      "{\"running\":"+String(prh_running?1:0)+
      ",\"sent\":0,\"count\":"+String(prh_count)+
      ",\"msg\":\""+String(prh_count)+" STAs, "+String(prh_frames)+" probes\"}");
  });
  server.on("/api/probeharvest/results", HTTP_GET, [](){
    String j = "[";
    for (int i = 0; i < prh_count; i++) {
      if (i) j += ",";
      char mac[18]; snprintf(mac,18,"%02x:%02x:%02x:%02x:%02x:%02x",
        prh_entries[i].sta[0],prh_entries[i].sta[1],prh_entries[i].sta[2],
        prh_entries[i].sta[3],prh_entries[i].sta[4],prh_entries[i].sta[5]);
      j += "{\"sta\":\""; j += mac;
      j += "\",\"ssid\":\""; j += prh_entries[i].ssid;
      j += "\",\"rssi\":"; j += prh_entries[i].rssi;
      j += ",\"age\":"; j += (millis()-prh_entries[i].last_seen)/1000; j += "}";
    }
    j += "]"; server.send(200,"application/json",j);
  });
}

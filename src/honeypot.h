#pragma once
#include <WiFi.h>
#include "module.h"
#define HP_PORTS 3
static const uint16_t hp_ports[HP_PORTS] = {22, 80, 2323};
static WiFiServer *hp_srv[HP_PORTS] = {nullptr, nullptr, nullptr};
static WiFiClient hp_cli[HP_PORTS];
static bool hp_running = false;
static String hp_log;
static void hp_alloc() {
  for (int i = 0; i < HP_PORTS; i++) {
    if (!hp_srv[i]) {
      hp_srv[i] = new WiFiServer(hp_ports[i], 2);
      hp_srv[i]->begin();
    }
  }
}
static bool hp_start(const String &) {
  hp_alloc();
  hp_running = true;
  return true;
}
static void hp_stop() {
  for (int i = 0; i < HP_PORTS; i++) {
    if (hp_srv[i]) {
      hp_srv[i]->stop();
      delete hp_srv[i];
      hp_srv[i] = nullptr;
    }
  }
  hp_running = false;
}
static void hp_loop() {
  if (!hp_running)
    return;
  for (int i = 0; i < HP_PORTS; i++) {
    if (!hp_srv[i] || !hp_srv[i]->hasClient())
      continue;
    hp_cli[i] = hp_srv[i]->accept();
    if (!hp_cli[i])
      continue;
    String peer = hp_cli[i].remoteIP().toString();
    hp_log += peer + " :" + String(hp_ports[i]) + " @" + String(millis() / 1000) + "\n";
    if (hp_log.length() > 4096)
      hp_log = hp_log.substring(hp_log.length() - 2048);
    switch (hp_ports[i]) {
      case 22:
        hp_cli[i].print("SSH-2.0-OpenSSH_8.9\r\n");
        break;
      case 80:
        hp_cli[i].print("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK");
        break;
      case 2323:
        hp_cli[i].print("Welcome\r\nlogin: ");
        break;
    }
    delay(50);
    hp_cli[i].stop();
  }
}
static void hp_routes() {
  extern WebServer server;
  server.on("/api/honeypot/start", HTTP_POST, []() {
    bool ok = tool_start("honeypot", "");
    server.send(200, "application/json", ok ? "{\"ok\":1}" : "{\"ok\":0,\"err\":\"busy\"}");
  });
  server.on("/api/honeypot/stop", HTTP_POST, []() {
    tool_stop("honeypot");
    server.send(200, "application/json", "{\"ok\":1}");
  });
  server.on("/api/honeypot/status", HTTP_GET, []() {
    server.send(200, "application/json",
                "{\"running\":" + String(hp_running ? 1 : 0) + ",\"msg\":\"" +
                    String(hp_log.length()) + " log bytes\"}");
  });
  server.on("/api/honeypot/log", HTTP_GET, []() {
    String j = "{\"log\":\"";
    j += hp_log;
    j.replace("\n", "\\n");
    j += "\"}";
    server.send(200, "application/json", j);
  });
}

#pragma once
#include <esp_wifi.h>
#include <WebServer.h>
void aegisSniff(void *, wifi_promiscuous_pkt_type_t);
inline void stationSetup() {
  esp_wifi_set_promiscuous_rx_cb(aegisSniff);
}
inline void stationLoop() {}
inline String stationStats() {
  return "\"sta\":\"\"";
}
inline void handleLan() {
  extern WebServer server;
  server.send(200, "application/json", "[]");
}
inline void handleWifiCfg() {
  extern WebServer server;
  server.send(200, "application/json", "{}");
}
inline void handleCast() {
  extern WebServer server;
  server.send(400, "application/json", "{\"ok\":0}");
}
inline void handleYouTube() {
  extern WebServer server;
  server.send(400, "application/json", "{\"ok\":0}");
}
inline void handlePower() {
  extern WebServer server;
  server.send(400, "application/json", "{\"ok\":0}");
}

#pragma once
#include "module.h"
static bool spi_running = false;
static int spi_clk = 18, spi_mosi = 23, spi_cs = 5;
static volatile uint8_t spi_ring[256];
static volatile uint16_t spi_head = 0, spi_tail = 0;
static volatile uint8_t spi_byte = 0;
static volatile int spi_bit = 0;
static void IRAM_ATTR spi_on_clk() {
  if (digitalRead(spi_cs) != LOW)
    return;
  spi_byte = (spi_byte << 1) | digitalRead(spi_mosi);
  if (++spi_bit == 8) {
    uint16_t nh = (spi_head + 1) & 0xFF;
    if (nh != spi_tail) {
      spi_ring[spi_head] = spi_byte;
      spi_head = nh;
    }
    spi_bit = 0;
    spi_byte = 0;
  }
}
static String spi_log;
static void spi_drain() {
  while (spi_tail != spi_head) {
    char b[4];
    snprintf(b, sizeof(b), "%02X ", spi_ring[spi_tail]);
    spi_log += b;
    spi_tail = (spi_tail + 1) & 0xFF;
    if (spi_log.length() > 4096)
      spi_log = spi_log.substring(spi_log.length() - 2048);
  }
}
static bool spi_start(const String &q) {
  int i;
  if ((i = q.indexOf("clk=")) >= 0)
    spi_clk = q.substring(i + 4).toInt();
  if ((i = q.indexOf("mosi=")) >= 0)
    spi_mosi = q.substring(i + 5).toInt();
  if ((i = q.indexOf("cs=")) >= 0)
    spi_cs = q.substring(i + 3).toInt();
  pinMode(spi_clk, INPUT_PULLUP);
  pinMode(spi_mosi, INPUT_PULLUP);
  pinMode(spi_cs, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(spi_clk), spi_on_clk, RISING);
  spi_running = true;
  return true;
}
static void spi_stop() {
  detachInterrupt(digitalPinToInterrupt(spi_clk));
  spi_running = false;
}
static void spi_routes() {
  extern WebServer server;
  server.on("/api/spi/start", HTTP_POST, []() {
    String q;
    for (int i = 0; i < server.args(); i++) {
      if (i)
        q += '&';
      q += server.argName(i) + '=' + server.arg(i);
    }
    bool ok = tool_start("spi", q);
    server.send(200, "application/json", ok ? "{\"ok\":1}" : "{\"ok\":0,\"err\":\"busy\"}");
  });
  server.on("/api/spi/stop", HTTP_POST, []() {
    tool_stop("spi");
    server.send(200, "application/json", "{\"ok\":1}");
  });
  server.on("/api/spi/status", HTTP_GET, []() {
    spi_drain();
    server.send(200, "application/json",
                "{\"running\":" + String(spi_running ? 1 : 0) + ",\"msg\":\"" +
                    String(spi_log.length()) + " bytes\"}");
  });
  server.on("/api/spi/log", HTTP_GET, []() {
    spi_drain();
    String j = "{\"log\":\"";
    j += spi_log;
    j.replace("\n", "\\n");
    j += "\"}";
    server.send(200, "application/json", j);
  });
}

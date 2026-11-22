#pragma once
#include <Arduino.h>
#include "cli.h"
#include <WiFi.h>

struct OUIEntry { uint32_t oui; const char* vendor; };
static const OUIEntry oui_table[] = {
  {0x000C29,"VMware"},{0x005056,"VMware"},{0x001C42,"Parallels"},
  {0x080027,"VirtualBox"},{0x525400,"QEMU"},{0xACDE48,"Private"},
  {0x001A11,"Google"},{0x3C5AB4,"Google"},{0xF4F5D8,"Google"},{0x546009,"Google"},
  {0x001B63,"Apple"},{0x002500,"Apple"},{0x3C0754,"Apple"},{0xA45E60,"Apple"},
  {0xBC926B,"Apple"},{0xF0DBF8,"Apple"},{0xF81EDF,"Apple"},{0xACBC32,"Apple"},
  {0x001DD1,"Samsung"},{0x5CE8EB,"Samsung"},{0x2CAE2B,"Samsung"},{0xBC1485,"Samsung"},
  {0x0018E7,"Cisco"},{0x001A2F,"Cisco"},{0x0026CB,"Cisco"},
  {0x000D93,"Aruba"},{0x6CF37F,"Aruba"},{0x24DEC6,"Aruba"},
  {0x001A79,"TP-Link"},{0x14CC20,"TP-Link"},{0x50C7BF,"TP-Link"},{0xA42BB0,"TP-Link"},
  {0x000C43,"Ralink"},{0x00904C,"Xircom"},
  {0x0024B2,"Netgear"},{0x20E52A,"Netgear"},{0x4494FC,"Netgear"},
  {0x001349,"ZyXEL"},{0x5CE0C5,"MikroTik"},{0x48A98A,"MikroTik"},{0x744D28,"Routerboard"},
  {0xF48CEB,"Amazon"},{0x74C246,"Amazon"},{0x40B4CD,"Amazon"},
  {0x18B430,"Nest"},{0x641666,"Ring"},{0xD073D5,"Ring"},
  {0x0017EE,"Ubiquiti"},{0x24A43C,"Ubiquiti"},{0x74ACB9,"Ubiquiti"},{0x802AA8,"Ubiquiti"},
  {0x705681,"Espressif"},{0x246F28,"Espressif"},{0x7C9EBD,"Espressif"},{0xAC67B2,"Espressif"},
  {0x001A11,"Raspberry Pi"},{0xB827EB,"Raspberry Pi"},{0xDCA632,"Raspberry Pi"},{0xE45F01,"Raspberry Pi"},
  {0x001CB3,"Huawei"},{0x0C37DC,"Huawei"},{0x48DB50,"Huawei"},{0x781DBA,"Huawei"},
  {0x001E75,"Intel"},{0x88B111,"Intel"},{0xA0369F,"Intel"},
  {0x0017A4,"Xiaomi"},{0x286C07,"Xiaomi"},{0x781103,"Xiaomi"},{0x9C99A0,"Xiaomi"},
  {0x0018DD,"Sonos"},{0x5CAAFD,"Sonos"},{0xB8E937,"Sonos"},
  {0x90DD5D,"Apple"},{0x5CD2E4,"Intel"},{0xD0577B,"Intel"},
};

static String diag_vendor(const uint8_t* mac){
  uint32_t oui = ((uint32_t)mac[0]<<16) | ((uint32_t)mac[1]<<8) | mac[2];
  for (size_t i=0;i<sizeof(oui_table)/sizeof(oui_table[0]);i++)
    if (oui_table[i].oui == oui) return oui_table[i].vendor;
  if ((mac[0] & 0x02)) return "Randomized";
  return "Unknown";
}
static String diag_mac(const uint8_t* m){
  char b[18];
  snprintf(b,sizeof(b),"%02X:%02X:%02X:%02X:%02X:%02X",m[0],m[1],m[2],m[3],m[4],m[5]);
  return String(b);
}

// Channel utilization — derived from Aegis's chanCount[]
static String diag_channel(){
  extern volatile uint32_t chanCount[15];
  uint32_t total = 0;
  for (int c=1;c<=14;c++) total += chanCount[c];
  String s;
  for (int c=1;c<=14;c++){
    uint32_t n = chanCount[c];
    int pct = total ? (int)((n*100)/total) : 0;
    s += "ch" + String(c) + ": " + String(n) + " (" + String(pct) + "%)\n";
  }
  s += "total: " + String(total) + " frames";
  return s;
}

// RSSI histogram — buckets every 10 dBm from -100 to 0
static String diag_heatmap(){
  extern int apCount;
  extern String scanJson;
  // derive from WiFi.scanNetworks recent
  int buckets[11] = {0};
  int n = WiFi.scanComplete();
  if (n < 0){
    WiFi.scanNetworks(true);
    return "scan in progress, retry";
  }
  for (int i=0;i<n;i++){
    int r = WiFi.RSSI(i);
    int b = (r + 100) / 10;
    if (b < 0) b = 0;
    if (b > 10) b = 10;
    buckets[b]++;
  }
  WiFi.scanDelete();
  String s = "RSSI histogram (" + String(n) + " APs)\n";
  for (int b=10;b>=0;b--){
    int lo = b*10 - 100;
    int hi = lo + 10;
    String bar;
    for (int j=0;j<buckets[b];j++) bar += "#";
    s += String(lo) + ".." + String(hi) + " dBm: " + String(buckets[b]) + " " + bar + "\n";
  }
  return s;
}

// Client inventory with vendor
static String diag_clients(){
  extern NearDev clients[];
  extern const int MAXC;
  String s;
  int shown = 0;
  unsigned long now = millis();
  for (int i=0;i<MAXC;i++){
    if (!clients[i].used) continue;
    if ((now - clients[i].last) / 1000 > 120) continue;
    shown++;
    s += diag_mac(clients[i].mac) + "  ";
    s += String(clients[i].rssi) + "dBm  ";
    s += diag_vendor(clients[i].mac) + "  ";
    if (clients[i].ssid[0]) s += "[" + String(clients[i].ssid) + "]  ";
    s += String((now-clients[i].last)/1000) + "s ago\n";
  }
  if (!shown) s = "no clients in last 120s";
  return s;
}

// IE decoder for one BSSID from Aegis's ApInfo
static String diag_ies(const String& bssid_str){
  extern ApInfo apinfo[];
  extern const int MAXAP;
  uint8_t b[6];
  if (sscanf(bssid_str.c_str(),"%x:%x:%x:%x:%x:%x",
       (unsigned*)&b[0],(unsigned*)&b[1],(unsigned*)&b[2],
       (unsigned*)&b[3],(unsigned*)&b[4],(unsigned*)&b[5]) != 6)
    return "bad bssid";
  for (int i=0;i<MAXAP;i++){
    if (apinfo[i].used && !memcmp(apinfo[i].bssid, b, 6)){
      ApInfo* a = &apinfo[i];
      String s = "BSSID: " + bssid_str + "\n";
      s += "  cipher: " + String(a->ccmp?"CCMP":(a->tkip?"TKIP":(a->wep?"WEP":"open"))) + "\n";
      s += "  PMF:    " + String(a->pmfR?"required":(a->pmfC?"capable":"disabled")) + "\n";
      s += "  std:    " + String(a->std==5?"ax":(a->std==4?"ac":(a->std==3?"n":"b/g"))) + "\n";
      s += "  WPS:    " + String(a->wps?"yes":"no") + "\n";
      s += "  hidden: " + String(a->hidden?"yes":"no") + "\n";
      s += "  CSA:    " + String(a->csa?"yes":"no") + "\n";
      s += "  beacon interval: " + String(a->bint) + " TU\n";
      s += "  RSSI: " + String(a->rmin) + ".." + String(a->rmax) + " dBm\n";
      s += "  distinct SSIDs seen: " + String(a->ssidCount);
      return s;
    }
  }
  return "not seen yet (open Aegis dashboard once)";
}

// Packet inspector — last N mgmt frames
struct DiagPkt { uint8_t fc0, fc1; uint8_t sa[6], da[6], bssid[6]; uint32_t ts; };
static DiagPkt diag_pkt_ring[16];
static volatile uint8_t diag_pkt_head = 0;

static void diag_record_pkt(uint8_t fc0, uint8_t fc1, const uint8_t* sa, const uint8_t* da, const uint8_t* bssid){
  uint8_t h = diag_pkt_head & 15;
  DiagPkt* p = &diag_pkt_ring[h];
  p->fc0=fc0; p->fc1=fc1;
  memcpy(p->sa, sa, 6); memcpy(p->da, da, 6); memcpy(p->bssid, bssid, 6);
  p->ts = millis();
  diag_pkt_head = (h + 1) & 15;
}

static String diag_packets(){
  String s;
  for (int i=0;i<16;i++){
    int idx = (diag_pkt_head - 1 - i + 32) % 16;
    DiagPkt* p = &diag_pkt_ring[idx];
    if (!p->ts) continue;
    uint8_t subtype = (p->fc0 >> 4) & 0xF;
    const char* name = "?";
    switch (subtype){
      case 0: name="assoc-req"; break; case 1: name="assoc-resp"; break;
      case 2: name="reassoc-req"; break; case 3: name="reassoc-resp"; break;
      case 4: name="probe-req"; break; case 5: name="probe-resp"; break;
      case 8: name="beacon"; break; case 10: name="disassoc"; break;
      case 11: name="auth"; break; case 12: name="deauth"; break;
      case 13: name="action"; break;
    }
    s += String((millis()-p->ts)/1000) + "s  ";
    s += name; s += "  ";
    s += diag_mac(p->sa) + " -> " + diag_mac(p->da) + "\n";
  }
  if (!s.length()) s = "no packets captured";
  return s;
}

String diag_dispatch(const String&, const String&, const String* pos){
  String sub = pos[0];
  sub.toLowerCase();
  if (sub == "channel") return cli_ok(diag_channel());
  if (sub == "heatmap") return cli_ok(diag_heatmap());
  if (sub == "clients") return cli_ok(diag_clients());
  if (sub == "ies")     return cli_ok(diag_ies(pos[1]));
  if (sub == "packets") return cli_ok(diag_packets());
  return cli_err("diag <channel|heatmap|clients|ies <bssid>|packets>");
}

inline void diag_routes(){
  extern WebServer server;
  g_diag_pkt_hook = diag_record_pkt;    // wire up the sniffer hook
  server.on("/api/diag/channel", HTTP_GET, [](){ server.send(200,"application/json","{\"out\":\""+cli_esc(diag_channel())+"\"}"); });
  server.on("/api/diag/heatmap", HTTP_GET, [](){ server.send(200,"application/json","{\"out\":\""+cli_esc(diag_heatmap())+"\"}"); });
  server.on("/api/diag/clients", HTTP_GET, [](){ server.send(200,"application/json","{\"out\":\""+cli_esc(diag_clients())+"\"}"); });
  server.on("/api/diag/packets", HTTP_GET, [](){ server.send(200,"application/json","{\"out\":\""+cli_esc(diag_packets())+"\"}"); });
  server.on("/api/diag/ies",     HTTP_GET, [](){ server.send(200,"application/json","{\"out\":\""+cli_esc(diag_ies(server.arg("bssid")))+"\"}"); });
}

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <esp_wifi.h>
#include <esp_random.h>

WebServer server(80);
uint16_t g_seq = 0;

#include "module.h"
#include "safety.h"
#include "cli.h"
#include "hub_html.h"
#include "types.h"
#include "frames.h"
#include "capture.h"
#include "station.h"
#include "aegis_ble.h"
#include "aegis.h"
#include "aegis_cli.h"
#include "deauth.h"
#include "beacon_spam.h"
#include "probe_flood.h"
#include "handshake.h"
#include "evil_portal.h"
#include "honeypot.h"
#include "ble_spam.h"
#include "i2c_sniff.h"
#include "spi_sniff.h"
#include "uart_bridge.h"
#include "persist.h"
#include "diag.h"
#include "evil_twin.h"
#include "auth_flood.h"
#include "karma.h"
#include "nav_flood.h"
#include "csa_attack.h"
#include "espnow_flood.h"
#include "beacon_adv.h"
#include "pmkid_hunt.h"
#include "wpa3_flood.h"
#include "chan_hop_deauth.h"
#include "pcap_mgr.h"
#include "board.h"
#include "btm_flood.h"
#include "eap_flood.h"
#include "null_flood.h"
#include "mgt_flood.h"
#include "deauth_multi.h"
#include "action_flood.h"
#include "probe_harvest.h"
#include "deauth_storm.h"
#include "pwnface.h"

static void doom_suspend(){
  esp_wifi_scan_stop();
  WiFi.scanDelete();
  esp_wifi_set_promiscuous(false);
}
static void doom_resume(){
  esp_wifi_set_promiscuous_rx_cb(aegisSniff);
  wifi_promiscuous_filter_t f={};
  f.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT|WIFI_PROMIS_FILTER_MASK_DATA;
  esp_wifi_set_promiscuous_filter(&f);
  esp_wifi_set_promiscuous(true);
}
static void serveHub(){
  server.sendHeader("Cache-Control","public, max-age=60");
  server.send_P(200,"text/html",HUB_HTML);
}
static void listModules(){
  String j="{\"active\":\""; j+=g_active?g_active:""; j+="\"";
  j+=",\"uptime\":"+String(millis()/1000)+",\"heap\":"+String(ESP.getFreeHeap())+",\"modules\":[";
  for(int i=0;i<g_tool_count;i++){
    if(i)j+=",";
    j+="{\"slug\":\""; j+=g_tools[i].slug;
    j+="\",\"name\":\""; j+=g_tools[i].name;
    j+="\",\"desc\":\""; j+=g_tools[i].desc;
    j+="\",\"radio\":"+String(g_tools[i].radio);
    j+=",\"passive\":"+String(g_tools[i].passive?1:0)+"}";
  }
  j+="]}"; server.send(200,"application/json",j);
}
static void aegisSetupW(){ aegisSetup(); }
static void aegisLoopW(){ aegisLoop(); }
static void aegisRoutesW(){ aegisRoutes(); }

// Scan APs — returns Aegis passive cache by default (zero disruption).
// ?live=1 forces a fresh WiFi scan only when no clients are connected.
static void scanAPs(){
  extern String scanJson;          // Aegis passively-built cache
  extern void buildScanJson(int);  // rebuild from scan results
  if(server.hasArg("live") && server.arg("live")=="1"){
    if(WiFi.softAPgetStationNum()>0){
      // Clients connected — refuse disruptive scan, return cached data
      server.send(200,"application/json",scanJson.length()>2?scanJson:"[]");
      return;
    }
    esp_wifi_set_promiscuous(false);
    int n = WiFi.scanNetworks(false, true);
    buildScanJson(n);
    WiFi.scanDelete();
    // Restore promiscuous mode
    esp_wifi_set_promiscuous_rx_cb(aegisSniff);
    wifi_promiscuous_filter_t f={};
    f.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT|WIFI_PROMIS_FILTER_MASK_DATA;
    esp_wifi_set_promiscuous_filter(&f);
    esp_wifi_set_promiscuous(true);
  }
  server.send(200,"application/json",scanJson.length()>2?scanJson:"[]");
}

// Captive portal detection endpoints (iOS, Android, Windows, Chrome)
static void captivePortalRoutes(){
  server.on("/hotspot-detect.html",HTTP_GET,[](){server.send(200,"text/html","<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");});
  server.on("/generate_204",HTTP_GET,[](){server.send(204,"text/plain","");});
  server.on("/ncsi.txt",HTTP_GET,[](){server.send(200,"text/plain","Microsoft NCSI");});
  server.on("/connecttest.txt",HTTP_GET,[](){server.send(200,"text/plain","Microsoft Connect Test");});
  server.on("/redirect",HTTP_GET,[](){server.sendHeader("Location","http://192.168.4.1");server.send(302,"text/plain","");});
  server.on("/kindle-wifi/wifistub.html",HTTP_GET,[](){server.send(200,"text/html","<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");});
}

static void register_tools(){
  tool_register({"aegis","Aegis IDS","Passive WiFi IDS + BLE.",1,true,aegisSetupW,aegisLoopW,aegisRoutesW,nullptr,nullptr});
  tool_register({"deauth","Deauth","802.11 deauth injection.",1,false,nullptr,nullptr,dt_routes,dt_start,dt_stop});
  tool_register({"beacon","Beacon Spam","Rotating fake beacons.",1,false,nullptr,nullptr,bs_routes,bs_start,bs_stop});
  tool_register({"probe","Probe Flood","Randomised probe flood.",1,false,nullptr,nullptr,pf_routes,pf_start,pf_stop});
  tool_register({"handshake","Handshake/PMKID","EAPOL→PCAP on flash.",1,false,nullptr,nullptr,hs_routes,hs_start,hs_stop});
  tool_register({"portal","Evil Portal","Captive portal cred capture.",1,false,nullptr,ep_loop,ep_routes,ep_start,ep_stop});
  tool_register({"honeypot","Honeypot","TCP 22/80/2323 + log.",0,false,nullptr,hp_loop,hp_routes,hp_start,hp_stop});
  tool_register({"blespam","BLE Spam","Apple/Android/Win/Samsung/AirTag/Continuity/AirDrop/Google/flood.",2,false,nullptr,blespam_loop,blespam_routesW,blespam_start,blespam_stop});
  tool_register({"i2c","I2C Sniff","I2C slave logger.",0,false,nullptr,nullptr,i2c_routes,i2c_start,i2c_stop});
  tool_register({"spi","SPI Sniff","ISR-safe MOSI sampler.",0,false,nullptr,nullptr,spi_routes,spi_start,spi_stop});
  tool_register({"uart","UART Bridge","UART1↔USB.",0,false,nullptr,uart_loop,uart_routes,uart_start,uart_stop});
  // WiFi attack modules
  tool_register({"eviltwin","Evil Twin","Rogue AP + deauth + DNS + cred capture.",1,false,nullptr,et_loop,et_routes,et_start,et_stop});
  tool_register({"authflood","Auth Flood","Auth/assoc/disassoc flood.",1,false,nullptr,nullptr,af_routes,af_start,af_stop});
  tool_register({"karma","KARMA","Probe response spoofing.",1,false,nullptr,nullptr,karma_routes,karma_start,karma_stop});
  tool_register({"navflood","NAV Flood","CTS/RTS NAV DoS.",1,false,nullptr,nullptr,nf_routes,nf_start,nf_stop});
  tool_register({"csa","CSA Attack","Channel switch + BTM.",1,false,nullptr,nullptr,csa_routes,csa_start,csa_stop});
  tool_register({"espnow","ESP-NOW Flood","ESP-NOW broadcast flood.",0,false,nullptr,nullptr,en_routes,en_start,en_stop});
  tool_register({"beaconadv","Adv Frames","WPS/fuzz/overflow/TKIP/WDS injection.",1,false,nullptr,nullptr,ba_routes,ba_start,ba_stop});
  tool_register({"pmkid","PMKID Hunt","Active PMKID extraction→PCAP.",1,false,nullptr,nullptr,ph_routes,ph_start,ph_stop});
  tool_register({"wpa3flood","WPA3 Flood","SAE commit/confirm flood.",1,false,nullptr,nullptr,w3_routes,w3_start,w3_stop});
  tool_register({"chanhop","Chan Hop Deauth","All-channel deauth sweep.",1,false,nullptr,nullptr,chd_routes,chd_start,chd_stop});
  tool_register({"btmflood","BTM Flood","802.11v BSS Transition — force STA roaming.",1,false,nullptr,nullptr,btm_routes,btm_start,btm_stop});
  tool_register({"eapflood","EAP Flood","WPA Enterprise EAPOL state exhaustion.",1,false,nullptr,nullptr,eap_routes,eap_start,eap_stop});
  tool_register({"nullflood","Null Flood","Power-save null/QoS-null/BAR confusion.",1,false,nullptr,nullptr,nf2_routes,nf2_start,nf2_stop});
  tool_register({"mgtflood","Mgmt Spray","All-subtype management frame spray.",1,false,nullptr,nullptr,mgf_routes,mgf_start,mgf_stop});
  tool_register({"multideauth","Multi Deauth","Deauth up to 8 BSSIDs simultaneously.",1,false,nullptr,nullptr,dm_routes,dm_start,dm_stop});
  tool_register({"actionflood","Action Flood","802.11 action frame category spray.",1,false,nullptr,nullptr,acf_routes,acf_start,acf_stop});
  tool_register({"pwnface","Pwngotchi","Autonomous UCB-guided handshake hunter (Pwnagotchi-inspired).",1,false,nullptr,nullptr,pw_routes,pw_start,pw_stop});
  tool_register({"probeharvest","Probe Harvest","Passive probe request sniffer — STA MAC + saved network history.",1,true,nullptr,nullptr,prh_routes,prh_start,prh_stop});
  tool_register({"stormdeauth","Deauth Storm","Multi-vector WPA2+WPA3 deauth storm (8 attack vectors).",1,false,nullptr,nullptr,ds_routes,ds_start,ds_stop});
}

void setup(){
  Serial.begin(115200); delay(100); Serial.println("\n[doom] boot");
  if (!LittleFS.begin(true)) {
    Serial.println("[doom] LittleFS mount failed — formatting");
    LittleFS.format();
    LittleFS.begin(false);
  }
  Serial.printf("[doom] FS: %u/%u bytes used\n", LittleFS.usedBytes(), LittleFS.totalBytes());
  fsLockInit();
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("Phantom32","phantom32");
  delay(150);
  Serial.printf("[doom] AP: %s\n", WiFi.softAPIP().toString().c_str());
  tool_wifi_suspend = doom_suspend;
  tool_wifi_resume  = doom_resume;
  safety_load();
  safety_autostop_hook = [](){ if(g_active) tool_stop(g_active); };
  safety_routes();
  diag_routes();
  board_routes();
  captivePortalRoutes();
  pcapmgr_routes();
  server.on("/",HTTP_GET,serveHub);
  server.on("/api/modules",HTTP_GET,listModules);
  server.on("/api/scan",HTTP_GET,scanAPs);
  register_tools();
  for(int i=0;i<g_tool_count;i++) if(g_tools[i].routes) g_tools[i].routes();
  server.on("/aegis",HTTP_GET,[](){handleRoot();});
  server.on("/cli",HTTP_GET,serveHub);
  server.onNotFound([](){server.send(404,"text/plain","nf");});
  cli_routes();
  server.begin();
  Serial.println("[doom] ready — connect to Phantom32 / phantom32 → http://192.168.4.1");
  for(int i=0;i<g_tool_count;i++) if(g_tools[i].setup) g_tools[i].setup();
}

void loop(){
  server.handleClient();
  safety_tick();
  for(int i=0;i<g_tool_count;i++) if(g_tools[i].loop) g_tools[i].loop();
  delay(1);
}

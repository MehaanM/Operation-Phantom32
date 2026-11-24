#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <string.h>
#include <stdint.h>

// ── WPS Scanner & Information Element Parser ───────────────────────────────────
// Passive WPS detection: parse WPS IEs from beacon and probe response frames.
// Extracts WPS version, AP setup state, config methods, device type, UUID.
// Prefix: "wps_"
// ─────────────────────────────────────────────────────────────────────────────

extern WebServer server;

#define WPS_MAX_APS        64
#define WPS_MAX_LOG        128
#define WPS_OUI_MICROSOFT  0x0050F204  // 00:50:F2 type 4

// WPS IE attribute IDs (big-endian in frame)
#define WPS_ATTR_VERSION          0x104A
#define WPS_ATTR_STATE            0x1044
#define WPS_ATTR_LOCKED           0x1057
#define WPS_ATTR_RESP_TYPE        0x103B
#define WPS_ATTR_UUID_E           0x1047
#define WPS_ATTR_MFR              0x1021
#define WPS_ATTR_MODEL            0x1023
#define WPS_ATTR_MODEL_NUM        0x1024
#define WPS_ATTR_SERIAL           0x1042
#define WPS_ATTR_PRIM_DEV_TYPE    0x1054
#define WPS_ATTR_DEV_NAME         0x1011
#define WPS_ATTR_CFG_METHODS      0x1008
#define WPS_ATTR_RF_BANDS         0x103C
#define WPS_ATTR_ASSOC_STATE      0x1002
#define WPS_ATTR_DEVICE_PASS_ID   0x1012
#define WPS_ATTR_OS_VERSION       0x102D
#define WPS_ATTR_FEATURES         0x1053
#define WPS_ATTR_SELECTED_REGIST  0x1041
#define WPS_ATTR_SELECTED_REG_CFG 0x1053
#define WPS_ATTR_CONN_TYPE_FLAGS  0x100D

// WPS config method flags
#define WPS_CFG_USBA         0x0001
#define WPS_CFG_ETHERNET     0x0002
#define WPS_CFG_LABEL        0x0004
#define WPS_CFG_DISPLAY      0x0008
#define WPS_CFG_EXT_NFC_TOK  0x0010
#define WPS_CFG_INT_NFC_TOK  0x0020
#define WPS_CFG_NFC_IFACE    0x0040
#define WPS_CFG_PUSHBUTTON   0x0080  // PBC
#define WPS_CFG_KEYPAD       0x0100
#define WPS_CFG_VIRT_DISPLAY 0x2000
#define WPS_CFG_PHY_DISPLAY  0x4000
#define WPS_CFG_VIRT_PBC     0x0280
#define WPS_CFG_PHY_PBC      0x0480

enum WpsApState : uint8_t {
  WPS_AP_NO_WPS    = 0,
  WPS_AP_UNCONFIGURED = 1,
  WPS_AP_CONFIGURED   = 2,
};

enum WpsLockState : uint8_t {
  WPS_LOCK_NONE    = 0,
  WPS_LOCK_LOCKED  = 1,
  WPS_LOCK_UNKNOWN = 2,
};

struct WpsApInfo {
  uint8_t    bssid[6];
  char       ssid[33];
  uint8_t    channel;
  int8_t     rssi;
  bool       has_wps;
  WpsApState state;
  WpsLockState lock_state;
  uint8_t    version;        // WPS version (0x10=1.0, 0x20=2.0)
  uint16_t   cfg_methods;    // bitmask of WPS_CFG_*
  bool       pbc_active;     // Push Button active
  bool       pin_active;     // PIN method active
  char       manufacturer[33];
  char       model_name[33];
  char       model_num[33];
  char       device_name[33];
  char       serial[33];
  uint8_t    uuid[16];
  bool       has_uuid;
  uint32_t   first_seen_ms;
  uint32_t   last_seen_ms;
  uint32_t   frame_count;
};

struct WpsLogEntry {
  uint32_t ts_ms;
  uint8_t  bssid[6];
  uint8_t  event; // 0=discovered, 1=state change, 2=locked, 3=pbc_started
  char     detail[48];
};

struct WpsState {
  WpsApInfo  aps[WPS_MAX_APS];
  uint8_t    ap_count;
  WpsLogEntry log[WPS_MAX_LOG];
  uint8_t    log_head;
  uint16_t   log_count;
  uint32_t   total_frames;
  uint32_t   wps_frames;
};

static WpsState g_wps = {};

// ── Helpers ───────────────────────────────────────────────────────────────────
static void wps_mac_str(const uint8_t* m, char* out) {
  snprintf(out,18,"%02X:%02X:%02X:%02X:%02X:%02X",m[0],m[1],m[2],m[3],m[4],m[5]);
}
static bool wps_mac_eq(const uint8_t* a, const uint8_t* b){return memcmp(a,b,6)==0;}

static void wps_log(const uint8_t* bssid, uint8_t event, const char* detail) {
  WpsLogEntry* e=&g_wps.log[g_wps.log_head];
  e->ts_ms=millis();
  if(bssid) memcpy(e->bssid,bssid,6); else memset(e->bssid,0,6);
  e->event=event;
  strncpy(e->detail,detail?detail:"",47);
  g_wps.log_head=(g_wps.log_head+1)%WPS_MAX_LOG;
  if(g_wps.log_count<WPS_MAX_LOG) g_wps.log_count++;
}

static WpsApInfo* wps_find_ap(const uint8_t* bssid){
  for(uint8_t i=0;i<g_wps.ap_count;i++)
    if(wps_mac_eq(g_wps.aps[i].bssid,bssid)) return &g_wps.aps[i];
  return nullptr;
}

static WpsApInfo* wps_upsert_ap(const uint8_t* bssid,const char* ssid,uint8_t ch,int8_t rssi){
  WpsApInfo* ap=wps_find_ap(bssid);
  if(!ap){
    if(g_wps.ap_count>=WPS_MAX_APS) return nullptr;
    ap=&g_wps.aps[g_wps.ap_count++];
    memset(ap,0,sizeof(WpsApInfo));
    memcpy(ap->bssid,bssid,6);
    ap->first_seen_ms=millis();
    wps_log(bssid,0,"WPS AP discovered");
  }
  if(ssid) strncpy(ap->ssid,ssid,32);
  ap->channel=ch;
  ap->rssi=rssi;
  ap->last_seen_ms=millis();
  ap->frame_count++;
  return ap;
}

// ── WPS IE parser ─────────────────────────────────────────────────────────────
// Walks vendor-specific IE: OUI 00:50:F2, type 4 = WPS
static void wps_parse_ie(WpsApInfo* ap, const uint8_t* data, int len) {
  // data points past the OUI+type (4 bytes already consumed by caller)
  // Format: TLV pairs, big-endian tag (2) + length (2) + value
  int pos=0;
  while(pos+4<=len){
    uint16_t tag =(uint16_t)((data[pos]<<8)|data[pos+1]);
    uint16_t vlen=(uint16_t)((data[pos+2]<<8)|data[pos+3]);
    pos+=4;
    if(pos+vlen>len) break;
    const uint8_t* v=data+pos;

    switch(tag){
      case WPS_ATTR_VERSION:
        if(vlen>=1) ap->version=v[0];
        break;
      case WPS_ATTR_STATE:
        if(vlen>=1){
          ap->has_wps=true;
          if(v[0]==1) ap->state=WPS_AP_UNCONFIGURED;
          else if(v[0]==2) ap->state=WPS_AP_CONFIGURED;
        }
        break;
      case WPS_ATTR_LOCKED:
        if(vlen>=1){
          if(v[0]==0x01){
            ap->lock_state=WPS_LOCK_LOCKED;
            wps_log(ap->bssid,2,"AP PIN locked");
          } else {
            ap->lock_state=WPS_LOCK_NONE;
          }
        }
        break;
      case WPS_ATTR_CFG_METHODS:
        if(vlen>=2){
          ap->cfg_methods=(uint16_t)((v[0]<<8)|v[1]);
          ap->pbc_active=(ap->cfg_methods & WPS_CFG_PUSHBUTTON) ||
                         (ap->cfg_methods & WPS_CFG_VIRT_PBC)   ||
                         (ap->cfg_methods & WPS_CFG_PHY_PBC);
          ap->pin_active=(ap->cfg_methods & WPS_CFG_LABEL)   ||
                         (ap->cfg_methods & WPS_CFG_KEYPAD)   ||
                         (ap->cfg_methods & WPS_CFG_DISPLAY)  ||
                         (ap->cfg_methods & WPS_CFG_VIRT_DISPLAY)||
                         (ap->cfg_methods & WPS_CFG_PHY_DISPLAY);
        }
        break;
      case WPS_ATTR_SELECTED_REGIST:
        if(vlen>=1 && v[0]==0x01){
          wps_log(ap->bssid,3,"PBC session active (Registrar selected)");
        }
        break;
      case WPS_ATTR_MFR:
        if(vlen>0){
          int cpy=vlen>32?32:(int)vlen;
          memcpy(ap->manufacturer,v,cpy);
          ap->manufacturer[cpy]=0;
        }
        break;
      case WPS_ATTR_MODEL:
        if(vlen>0){
          int cpy=vlen>32?32:(int)vlen;
          memcpy(ap->model_name,v,cpy);
          ap->model_name[cpy]=0;
        }
        break;
      case WPS_ATTR_MODEL_NUM:
        if(vlen>0){
          int cpy=vlen>32?32:(int)vlen;
          memcpy(ap->model_num,v,cpy);
          ap->model_num[cpy]=0;
        }
        break;
      case WPS_ATTR_DEV_NAME:
        if(vlen>0){
          int cpy=vlen>32?32:(int)vlen;
          memcpy(ap->device_name,v,cpy);
          ap->device_name[cpy]=0;
        }
        break;
      case WPS_ATTR_SERIAL:
        if(vlen>0){
          int cpy=vlen>32?32:(int)vlen;
          memcpy(ap->serial,v,cpy);
          ap->serial[cpy]=0;
        }
        break;
      case WPS_ATTR_UUID_E:
        if(vlen>=16){
          memcpy(ap->uuid,v,16);
          ap->has_uuid=true;
        }
        break;
      default: break;
    }
    pos+=vlen;
  }
}

// ── Main frame ingest ─────────────────────────────────────────────────────────
// Called with the raw 802.11 frame body (after radiotap header stripped)
static void wps_ingest(const uint8_t* frame, int flen, int8_t rssi) {
  if(!frame||flen<36) return;
  g_wps.total_frames++;

  uint8_t ftype=(frame[0]>>2)&0x3;  // 0=mgmt
  uint8_t fsub =(frame[0]>>4)&0xF;  // 8=beacon, 5=probe resp
  if(ftype!=0||(fsub!=8&&fsub!=5)) return;

  const uint8_t* bssid=frame+16; // BSSID in beacon/probe resp
  const uint8_t* src  =frame+10;

  // Parse SSID from fixed fields (beacon: 36 bytes header+fixed, IEs start at 36)
  const uint8_t* ie_ptr=frame+36;
  int ie_len=flen-36;
  if(ie_len<2) return;

  char ssid[33]={0};
  uint8_t ch=0;

  // Walk IEs once for SSID, channel, and WPS vendor
  int pos=0;
  while(pos+2<=ie_len){
    uint8_t tag=ie_ptr[pos];
    uint8_t tlen=ie_ptr[pos+1];
    pos+=2;
    if(pos+tlen>ie_len) break;
    if(tag==0 && tlen<=32){                 // SSID
      memcpy(ssid,ie_ptr+pos,tlen);
      ssid[tlen]=0;
    } else if(tag==3 && tlen==1){           // DS param set (channel)
      ch=ie_ptr[pos];
    } else if(tag==221 && tlen>=4){         // Vendor specific
      // Check for Microsoft OUI 00:50:F2, type 4 (WPS)
      if(ie_ptr[pos]==0x00 && ie_ptr[pos+1]==0x50 &&
         ie_ptr[pos+2]==0xF2 && ie_ptr[pos+3]==0x04){
        g_wps.wps_frames++;
        WpsApInfo* ap=wps_upsert_ap(bssid,ssid,ch,rssi);
        if(ap){
          wps_parse_ie(ap,ie_ptr+pos+4,tlen-4);
          ap->has_wps=true;
        }
      }
    }
    pos+=tlen;
  }
}

// ── JSON helpers ──────────────────────────────────────────────────────────────
static String wps_cfg_methods_str(uint16_t m) {
  String s="";
  if(m&WPS_CFG_PUSHBUTTON) s+="PBC,";
  if(m&WPS_CFG_VIRT_PBC)   s+="PBC_virt,";
  if(m&WPS_CFG_PHY_PBC)    s+="PBC_phy,";
  if(m&WPS_CFG_LABEL)      s+="PIN_label,";
  if(m&WPS_CFG_KEYPAD)     s+="PIN_keypad,";
  if(m&WPS_CFG_DISPLAY)    s+="PIN_display,";
  if(m&WPS_CFG_VIRT_DISPLAY) s+="PIN_disp_virt,";
  if(m&WPS_CFG_USBA)       s+="USB,";
  if(m&WPS_CFG_NFC_IFACE)  s+="NFC,";
  if(s.length()>0) s.remove(s.length()-1);
  return s;
}

static String wps_ap_json(const WpsApInfo& ap) {
  char mac_str[18];
  wps_mac_str(ap.bssid,mac_str);
  String j="{";
  j+="\"bssid\":\""; j+=mac_str; j+="\"";
  j+=",\"ssid\":\""; j+=ap.ssid; j+="\"";
  j+=",\"ch\":"; j+=ap.channel;
  j+=",\"rssi\":"; j+=ap.rssi;
  j+=",\"has_wps\":"; j+=ap.has_wps?1:0;
  const char* states[]={"no_wps","unconfigured","configured"};
  j+=",\"state\":\""; j+=states[ap.state]; j+="\"";
  const char* locks[]={"none","locked","unknown"};
  j+=",\"lock\":\""; j+=locks[ap.lock_state]; j+="\"";
  j+=",\"version\":\""; j+=String(ap.version>>4)+"."+String(ap.version&0xF); j+="\"";
  j+=",\"pbc\":"; j+=ap.pbc_active?1:0;
  j+=",\"pin\":"; j+=ap.pin_active?1:0;
  j+=",\"cfg_methods\":\""; j+=wps_cfg_methods_str(ap.cfg_methods); j+="\"";
  if(ap.manufacturer[0]){j+=",\"mfr\":\""; j+=ap.manufacturer; j+="\""; }
  if(ap.model_name[0])  {j+=",\"model\":\""; j+=ap.model_name; j+="\""; }
  if(ap.device_name[0]) {j+=",\"device\":\""; j+=ap.device_name; j+="\""; }
  if(ap.has_uuid){
    char uuid_s[48];
    snprintf(uuid_s,sizeof(uuid_s),
      "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
      ap.uuid[0],ap.uuid[1],ap.uuid[2],ap.uuid[3],
      ap.uuid[4],ap.uuid[5],ap.uuid[6],ap.uuid[7],
      ap.uuid[8],ap.uuid[9],ap.uuid[10],ap.uuid[11],
      ap.uuid[12],ap.uuid[13],ap.uuid[14],ap.uuid[15]);
    j+=",\"uuid\":\""; j+=uuid_s; j+="\"";
  }
  j+=",\"frames\":"; j+=ap.frame_count;
  j+=",\"age_ms\":"; j+=(uint32_t)(millis()-ap.first_seen_ms);
  j+="}";
  return j;
}

static String wps_summary_json() {
  uint32_t total=g_wps.ap_count,wps_found=0,locked=0,pbc=0,pin=0;
  for(uint8_t i=0;i<g_wps.ap_count;i++){
    if(g_wps.aps[i].has_wps){ wps_found++;
      if(g_wps.aps[i].lock_state==WPS_LOCK_LOCKED) locked++;
      if(g_wps.aps[i].pbc_active) pbc++;
      if(g_wps.aps[i].pin_active) pin++;
    }
  }
  String j="{";
  j+="\"total_aps\":"; j+=total;
  j+=",\"wps_enabled\":"; j+=wps_found;
  j+=",\"locked\":"; j+=locked;
  j+=",\"pbc_active\":"; j+=pbc;
  j+=",\"pin_active\":"; j+=pin;
  j+=",\"total_frames\":"; j+=g_wps.total_frames;
  j+=",\"wps_frames\":"; j+=g_wps.wps_frames;
  j+="}";
  return j;
}

static String wps_aps_json(int max_n) {
  if(max_n<=0||max_n>WPS_MAX_APS) max_n=WPS_MAX_APS;
  String j="{\"count\":"; j+=g_wps.ap_count; j+=",\"aps\":[";
  for(uint8_t i=0;i<g_wps.ap_count&&i<(uint8_t)max_n;i++){
    if(i) j+=",";
    j+=wps_ap_json(g_wps.aps[i]);
  }
  j+="]}";
  return j;
}

static String wps_log_json(int max_n) {
  if(max_n<1||max_n>WPS_MAX_LOG) max_n=WPS_MAX_LOG;
  String j="{\"count\":"; j+=g_wps.log_count; j+=",\"entries\":[";
  const char* evnames[]={"discovered","state_change","locked","pbc_active"};
  char mac_str[18];
  int start=(g_wps.log_head+WPS_MAX_LOG-g_wps.log_count)%WPS_MAX_LOG;
  for(int i=g_wps.log_count-1;i>=0&&i>=g_wps.log_count-max_n;i--){
    if(i<g_wps.log_count-1) j+=",";
    int idx=(start+i)%WPS_MAX_LOG;
    WpsLogEntry& e=g_wps.log[idx];
    wps_mac_str(e.bssid,mac_str);
    j+="{\"ts\":"; j+=e.ts_ms;
    j+=",\"bssid\":\""; j+=mac_str; j+="\"";
    j+=",\"event\":\""; j+=(e.event<4?evnames[e.event]:"?"); j+="\"";
    j+=",\"detail\":\""; j+=e.detail; j+="\"";
    j+="}";
  }
  j+="]}";
  return j;
}

// ── HTTP routes ───────────────────────────────────────────────────────────────
static void wps_routes() {
  server.on("/api/wps/summary", HTTP_GET, [](){
    server.send(200,"application/json", wps_summary_json());
  });

  server.on("/api/wps/aps", HTTP_GET, [](){
    int n=server.hasArg("per")?server.arg("per").toInt():WPS_MAX_APS;
    server.send(200,"application/json", wps_aps_json(n));
  });

  server.on("/api/wps/ap", HTTP_GET, [](){
    if(!server.hasArg("bssid")){server.send(400,"application/json","{\"error\":\"bssid required\"}");return;}
    uint8_t bssid[6];
    unsigned int b[6];
    if(sscanf(server.arg("bssid").c_str(),
      "%02X:%02X:%02X:%02X:%02X:%02X",&b[0],&b[1],&b[2],&b[3],&b[4],&b[5])!=6){
      server.send(400,"application/json","{\"error\":\"invalid bssid\"}"); return;
    }
    for(int i=0;i<6;i++) bssid[i]=(uint8_t)b[i];
    WpsApInfo* ap=wps_find_ap(bssid);
    if(!ap){server.send(404,"application/json","{\"error\":\"not found\"}");return;}
    server.send(200,"application/json",wps_ap_json(*ap));
  });

  server.on("/api/wps/log", HTTP_GET, [](){
    int n=server.hasArg("n")?server.arg("n").toInt():32;
    server.send(200,"application/json", wps_log_json(n));
  });

  server.on("/api/wps/clear", HTTP_POST, [](){
    memset(&g_wps,0,sizeof(WpsState));
    server.send(200,"application/json","{\"ok\":1}");
  });

  // Filter: only return WPS-enabled APs
  server.on("/api/wps/vulnerable", HTTP_GET, [](){
    String j="{\"aps\":[";
    bool first=true;
    for(uint8_t i=0;i<g_wps.ap_count;i++){
      WpsApInfo& ap=g_wps.aps[i];
      if(!ap.has_wps) continue;
      if(ap.lock_state==WPS_LOCK_LOCKED) continue; // already locked
      if(!first) j+=",";
      first=false;
      j+=wps_ap_json(ap);
    }
    j+="]}";
    server.send(200,"application/json",j);
  });
}

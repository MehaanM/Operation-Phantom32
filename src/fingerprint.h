#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <string.h>
#include <stdint.h>

// ── Device Fingerprinting Engine ──────────────────────────────────────────────
//
// Passive device fingerprinting from 802.11 frames: probe request IE ordering,
// supported rates, capabilities, timing, vendor-specific IEs, and OUI.
// Builds device profiles and classifies OS/device type.

#define FP_MAX_PROFILES  64
#define FP_MAX_PROBES    8    // SSIDs per device
#define FP_HISTORY_SIZE  16   // frame timing history
#define FP_SIG_MAX_LEN   32   // IE signature max length

// ── OS classification ─────────────────────────────────────────────────────────
enum FpOs : uint8_t {
  FP_OS_UNKNOWN  = 0,
  FP_OS_IOS      = 1,
  FP_OS_ANDROID  = 2,
  FP_OS_WINDOWS  = 3,
  FP_OS_MACOS    = 4,
  FP_OS_LINUX    = 5,
  FP_OS_CHROME   = 6,
  FP_OS_TIZEN    = 7,
  FP_OS_RTOS     = 8,   // embedded
};

static const char* fp_os_name(FpOs os) {
  switch(os){
    case FP_OS_IOS:     return "iOS";
    case FP_OS_ANDROID: return "Android";
    case FP_OS_WINDOWS: return "Windows";
    case FP_OS_MACOS:   return "macOS";
    case FP_OS_LINUX:   return "Linux";
    case FP_OS_CHROME:  return "ChromeOS";
    case FP_OS_TIZEN:   return "Tizen";
    case FP_OS_RTOS:    return "Embedded";
    default:            return "Unknown";
  }
}

// ── Device category ───────────────────────────────────────────────────────────
enum FpDevCat : uint8_t {
  FP_CAT_UNKNOWN   = 0,
  FP_CAT_PHONE     = 1,
  FP_CAT_TABLET    = 2,
  FP_CAT_LAPTOP    = 3,
  FP_CAT_DESKTOP   = 4,
  FP_CAT_IOT       = 5,
  FP_CAT_PRINTER   = 6,
  FP_CAT_TV        = 7,
  FP_CAT_CAMERA    = 8,
  FP_CAT_WEARABLE  = 9,
  FP_CAT_VEHICLE   = 10,
  FP_CAT_GAMING    = 11,
};

static const char* fp_cat_name(FpDevCat c) {
  switch(c){
    case FP_CAT_PHONE:   return "Phone";
    case FP_CAT_TABLET:  return "Tablet";
    case FP_CAT_LAPTOP:  return "Laptop";
    case FP_CAT_DESKTOP: return "Desktop";
    case FP_CAT_IOT:     return "IoT";
    case FP_CAT_PRINTER: return "Printer";
    case FP_CAT_TV:      return "Smart TV";
    case FP_CAT_CAMERA:  return "Camera";
    case FP_CAT_WEARABLE:return "Wearable";
    case FP_CAT_VEHICLE: return "Vehicle";
    case FP_CAT_GAMING:  return "Gaming";
    default:             return "Unknown";
  }
}

// ── Probe request IE signature ────────────────────────────────────────────────
// Fingerprint based on IE tag order in probe request
struct FpIeSig {
  uint8_t tags[FP_SIG_MAX_LEN];
  uint8_t tag_count;
  uint8_t rate_set[12];
  uint8_t rate_count;
  uint16_t ext_caps;
  bool    has_ht;
  bool    has_vht;
  bool    has_ext_rates;
  bool    has_wps_vendor;
  bool    has_ext_cap;
  bool    has_power_cap;
  bool    has_supp_channels;
  bool    has_rm_cap;
};

// ── Known probe IE signatures for OS detection ────────────────────────────────
struct FpSigDb {
  const char* os_name;
  FpOs        os;
  FpDevCat    cat;
  // IE tag order pattern (first N tags after SSID/Rates)
  const uint8_t* tags;
  uint8_t     tag_count;
  uint8_t     confidence;
};

static const uint8_t fp_ios_tags[]     = {1,50,3,45,127,191,221};
static const uint8_t fp_android_tags[] = {1,50,3,45,127};
static const uint8_t fp_windows_tags[] = {1,50,3,45,221,127};
static const uint8_t fp_macos_tags[]   = {1,50,3,45,127,221};
static const uint8_t fp_kindle_tags[]  = {1,3,50,45,127};

static const FpSigDb FP_SIG_DB[] = {
  {"iOS 14+",        FP_OS_IOS,     FP_CAT_PHONE,  fp_ios_tags,     7, 80},
  {"Android 10+",    FP_OS_ANDROID, FP_CAT_PHONE,  fp_android_tags, 5, 70},
  {"Windows 10",     FP_OS_WINDOWS, FP_CAT_LAPTOP, fp_windows_tags, 6, 75},
  {"macOS 11+",      FP_OS_MACOS,   FP_CAT_LAPTOP, fp_macos_tags,   6, 75},
  {"Kindle/FireOS",  FP_OS_ANDROID, FP_CAT_TABLET, fp_kindle_tags,  5, 65},
};
static const int FP_SIG_DB_SIZE = sizeof(FP_SIG_DB)/sizeof(FP_SIG_DB[0]);

// ── Device profile ────────────────────────────────────────────────────────────
struct FpProfile {
  uint8_t  mac[6];
  FpOs     os;
  FpDevCat cat;
  uint8_t  confidence;    // 0-100
  char     os_detail[32]; // e.g. "iOS 14+"
  char     vendor[32];
  char     probe_ssids[FP_MAX_PROBES][33];
  uint8_t  probe_count;
  FpIeSig  ie_sig;
  uint32_t frame_count;
  uint32_t probe_intervals[FP_HISTORY_SIZE]; // ms between probes
  uint8_t  int_head;
  uint8_t  int_count;
  uint32_t last_probe_ms;
  uint32_t first_seen_ms;
  uint32_t last_seen_ms;
  int8_t   rssi_last;
  int8_t   rssi_min;
  int8_t   rssi_max;
  bool     is_random_mac;
  bool     is_associated;
};

static FpProfile g_fp_profiles[FP_MAX_PROFILES];
static uint8_t   g_fp_count=0;

// ── Profile upsert ────────────────────────────────────────────────────────────
static FpProfile* fp_upsert(const uint8_t mac[6]) {
  for(uint8_t i=0;i<g_fp_count;i++){
    if(memcmp(g_fp_profiles[i].mac,mac,6)==0) return &g_fp_profiles[i];
  }
  if(g_fp_count>=FP_MAX_PROFILES){
    uint32_t oldest=g_fp_profiles[0].last_seen_ms;
    uint8_t  oi=0;
    for(uint8_t i=1;i<g_fp_count;i++){
      if(g_fp_profiles[i].last_seen_ms<oldest){oldest=g_fp_profiles[i].last_seen_ms;oi=i;}
    }
    memset(&g_fp_profiles[oi],0,sizeof(FpProfile));
    memcpy(g_fp_profiles[oi].mac,mac,6);
    g_fp_profiles[oi].first_seen_ms=millis();
    g_fp_profiles[oi].rssi_min=0;
    g_fp_profiles[oi].rssi_max=-100;
    return &g_fp_profiles[oi];
  }
  FpProfile* p=&g_fp_profiles[g_fp_count++];
  memset(p,0,sizeof(*p));
  memcpy(p->mac,mac,6);
  p->first_seen_ms=millis();
  p->rssi_min=0;
  p->rssi_max=-100;
  p->os=FP_OS_UNKNOWN;
  p->cat=FP_CAT_UNKNOWN;
  return p;
}

// ── Parse probe request IEs into signature ────────────────────────────────────
static void fp_parse_ies(const uint8_t* ies, int ies_len, FpIeSig* sig) {
  memset(sig,0,sizeof(*sig));
  int pos=0;
  while(pos+1<ies_len&&sig->tag_count<FP_SIG_MAX_LEN){
    uint8_t tag=ies[pos]; uint8_t elen=ies[pos+1];
    if(pos+2+elen>ies_len) break;
    const uint8_t* d=ies+pos+2;
    sig->tags[sig->tag_count++]=tag;
    switch(tag){
      case 1: // Rates
        sig->rate_count=elen<12?elen:12;
        memcpy(sig->rate_set,d,sig->rate_count);
        break;
      case 50: // Ext rates
        sig->has_ext_rates=true;
        break;
      case 45: // HT Capabilities
        sig->has_ht=true;
        break;
      case 127: // Extended capabilities
        sig->has_ext_cap=true;
        if(elen>=2) sig->ext_caps=(uint16_t)(d[0]|(d[1]<<8));
        break;
      case 191: // VHT Capabilities
        sig->has_vht=true;
        break;
      case 221: // Vendor specific
        if(elen>=4&&d[0]==0x00&&d[1]==0x50&&d[2]==0xF2&&d[3]==0x04)
          sig->has_wps_vendor=true;
        break;
      case 33: // Power capability
        sig->has_power_cap=true;
        break;
      case 36: // Supported channels
        sig->has_supp_channels=true;
        break;
      case 70: // RRM capabilities
        sig->has_rm_cap=true;
        break;
    }
    pos+=2+elen;
  }
}

// ── Classify OS from IE signature ─────────────────────────────────────────────
static void fp_classify(FpProfile* p) {
  FpIeSig* s=&p->ie_sig;
  uint8_t  best_conf=0;
  int      best_idx=-1;

  for(int i=0;i<FP_SIG_DB_SIZE;i++){
    const FpSigDb& db=FP_SIG_DB[i];
    // Count matching prefix
    uint8_t match=0;
    uint8_t check=db.tag_count<s->tag_count?db.tag_count:s->tag_count;
    for(uint8_t j=0;j<check;j++){
      if(s->tags[j]==db.tags[j]) match++;
    }
    uint8_t conf=(check>0)?(uint8_t)((match*100)/check):0;
    conf=(conf*(uint8_t)db.confidence)/100;
    if(conf>best_conf){best_conf=conf;best_idx=i;}
  }

  if(best_idx>=0){
    p->os=FP_SIG_DB[best_idx].os;
    p->cat=FP_SIG_DB[best_idx].cat;
    p->confidence=best_conf;
    strncpy(p->os_detail,FP_SIG_DB[best_idx].os_name,31);
  }

  // Refine: VHT → newer device
  if(s->has_vht&&p->confidence<70) p->confidence=70;
  // Random MAC → likely mobile
  if(p->is_random_mac&&p->cat==FP_CAT_UNKNOWN) p->cat=FP_CAT_PHONE;
  // No HT, old rates → IoT
  if(!s->has_ht&&!s->has_ext_rates&&p->cat==FP_CAT_UNKNOWN) p->cat=FP_CAT_IOT;
}

// ── Add probe SSID ────────────────────────────────────────────────────────────
static void fp_add_probe(FpProfile* p, const char* ssid) {
  if(!ssid||!ssid[0]) return;
  for(uint8_t i=0;i<p->probe_count;i++){
    if(strcmp(p->probe_ssids[i],ssid)==0) return;
  }
  uint8_t slot=p->probe_count<FP_MAX_PROBES?p->probe_count:FP_MAX_PROBES-1;
  if(p->probe_count>=FP_MAX_PROBES){
    memmove(p->probe_ssids[0],p->probe_ssids[1],33*(FP_MAX_PROBES-1));
  }
  strncpy(p->probe_ssids[slot],ssid,32);
  p->probe_ssids[slot][32]=0;
  if(p->probe_count<FP_MAX_PROBES) p->probe_count++;
}

// ── Track probe interval ──────────────────────────────────────────────────────
static void fp_track_interval(FpProfile* p) {
  uint32_t now=millis();
  if(p->last_probe_ms>0){
    uint32_t dt=now-p->last_probe_ms;
    p->probe_intervals[p->int_head]=dt;
    p->int_head=(p->int_head+1)%FP_HISTORY_SIZE;
    if(p->int_count<FP_HISTORY_SIZE) p->int_count++;
  }
  p->last_probe_ms=now;
}

// ── Average probe interval ────────────────────────────────────────────────────
static uint32_t fp_avg_interval(const FpProfile* p) {
  if(!p->int_count) return 0;
  uint32_t sum=0;
  for(uint8_t i=0;i<p->int_count;i++) sum+=p->probe_intervals[i];
  return sum/p->int_count;
}

// ── Ingest probe request ──────────────────────────────────────────────────────
static void fp_ingest_probe(const uint8_t src[6], const char* ssid,
                              const uint8_t* ies, int ies_len, int8_t rssi) {
  if(!src||(src[0]&1)) return; // skip multicast
  FpProfile* p=fp_upsert(src);
  p->rssi_last=rssi;
  if(p->rssi_min==0||rssi<p->rssi_min) p->rssi_min=rssi;
  if(rssi>p->rssi_max) p->rssi_max=rssi;
  p->frame_count++;
  p->last_seen_ms=millis();
  p->is_random_mac=(src[0]&0x02)!=0;
  fp_add_probe(p,ssid);
  fp_track_interval(p);
  if(ies&&ies_len>0){
    fp_parse_ies(ies,ies_len,&p->ie_sig);
    fp_classify(p);
  }
}

// ── Profile JSON ─────────────────────────────────────────────────────────────
static String fp_profiles_json(int page, int per) {
  if(per<1||per>50) per=20;
  int start=page*per, end=start+per;
  if(end>(int)g_fp_count) end=g_fp_count;
  String j="{\"total\":"+String(g_fp_count)+",\"profiles\":[";
  for(int i=start;i<end;i++){
    if(i>start) j+=",";
    FpProfile& p=g_fp_profiles[i];
    char mac[18];
    snprintf(mac,sizeof(mac),"%02X:%02X:%02X:%02X:%02X:%02X",
      p.mac[0],p.mac[1],p.mac[2],p.mac[3],p.mac[4],p.mac[5]);
    j+="{\"mac\":\""; j+=mac; j+="\"";
    j+=",\"os\":\""; j+=fp_os_name(p.os); j+="\"";
    j+=",\"os_detail\":\""; j+=p.os_detail; j+="\"";
    j+=",\"cat\":\""; j+=fp_cat_name(p.cat); j+="\"";
    j+=",\"conf\":"; j+=p.confidence;
    j+=",\"random\":"; j+=p.is_random_mac?1:0;
    j+=",\"rssi\":"; j+=p.rssi_last;
    j+=",\"frames\":"; j+=p.frame_count;
    j+=",\"probes\":"; j+=p.probe_count;
    j+=",\"interval_ms\":"; j+=fp_avg_interval(&p);
    j+=",\"ht\":"; j+=p.ie_sig.has_ht?1:0;
    j+=",\"vht\":"; j+=p.ie_sig.has_vht?1:0;
    if(p.probe_count>0){
      j+=",\"probe_ssids\":[";
      for(int q=0;q<p.probe_count;q++){
        if(q) j+=",";
        j+="\""; j+=p.probe_ssids[q]; j+="\"";
      }
      j+="]";
    }
    j+="}";
  }
  j+="]}";
  return j;
}

// ── OS distribution JSON ──────────────────────────────────────────────────────
static String fp_os_dist_json() {
  uint8_t counts[9]={};
  for(uint8_t i=0;i<g_fp_count;i++){
    uint8_t os=(uint8_t)g_fp_profiles[i].os;
    if(os<9) counts[os]++;
  }
  String j="{\"distribution\":[";
  static const FpOs oses[]={FP_OS_IOS,FP_OS_ANDROID,FP_OS_WINDOWS,FP_OS_MACOS,
                              FP_OS_LINUX,FP_OS_CHROME,FP_OS_TIZEN,FP_OS_RTOS,FP_OS_UNKNOWN};
  for(int i=0;i<9;i++){
    if(i) j+=",";
    j+="{\"os\":\""; j+=fp_os_name(oses[i]); j+="\"";
    j+=",\"count\":"; j+=counts[(uint8_t)oses[i]]; j+="}";
  }
  j+="]}";
  return j;
}

// ── HTTP routes ───────────────────────────────────────────────────────────────
static void fp_routes() {
  extern WebServer server;

  server.on("/api/fp/profiles",HTTP_GET,[](){
    int page=server.hasArg("page")?server.arg("page").toInt():0;
    int per =server.hasArg("per") ?server.arg("per").toInt() :20;
    server.send(200,"application/json",fp_profiles_json(page,per));
  });

  server.on("/api/fp/os_dist",HTTP_GET,[](){
    server.send(200,"application/json",fp_os_dist_json());
  });

  server.on("/api/fp/summary",HTTP_GET,[](){
    char buf[256];
    uint8_t random_mac=0;
    for(uint8_t i=0;i<g_fp_count;i++) if(g_fp_profiles[i].is_random_mac) random_mac++;
    snprintf(buf,sizeof(buf),
      "{\"profiles\":%u,\"random_macs\":%u}",
      g_fp_count, random_mac);
    server.send(200,"application/json",buf);
  });

  server.on("/api/fp/clear",HTTP_POST,[](){
    memset(g_fp_profiles,0,sizeof(g_fp_profiles));
    g_fp_count=0;
    server.send(200,"application/json","{\"ok\":1}");
  });
}

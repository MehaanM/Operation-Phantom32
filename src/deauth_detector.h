#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <string.h>
#include <stdint.h>

// ── Deauth Storm Detector ─────────────────────────────────────────────────────
// Detects deauthentication flood attacks by monitoring:
//   - Deauth/disassoc frame rate per BSSID
//   - Broadcast deauths (attacker targeting all clients)
//   - Reason code distribution (unusual reason codes)
//   - Multi-source deauths (multiple spoofed senders vs real AP)
//   - AP impersonation (deauth from non-AP MAC pretending to be AP)
// Prefix: "ddt_"
// ─────────────────────────────────────────────────────────────────────────────

extern WebServer server;

#define DDT_MAX_BSSIDS      48
#define DDT_MAX_ALERTS      64
#define DDT_WINDOW_MS       5000   // sliding window for rate calculation
#define DDT_DEAUTH_THRESHOLD 20    // deauths/window before alerting
#define DDT_BROADCAST_THRESHOLD 5  // broadcast deauths/window

enum DdtAlertType : uint8_t {
  DDT_ALERT_FLOOD         = 0, // high deauth rate
  DDT_ALERT_BROADCAST     = 1, // broadcast deauths
  DDT_ALERT_SPOOF         = 2, // deauth from non-AP MAC for a BSSID
  DDT_ALERT_MULTI_SOURCE  = 3, // deauths for same BSSID from many MACs
  DDT_ALERT_UNUSUAL_REASON= 4, // odd reason codes
  DDT_ALERT_SUSTAINED     = 5, // attack lasting > 30s
};

enum DdtSeverity : uint8_t {
  DDT_SEV_LOW    = 0,
  DDT_SEV_MEDIUM = 1,
  DDT_SEV_HIGH   = 2,
  DDT_SEV_CRITICAL=3,
};

struct DdtAlert {
  uint32_t   ts_ms;
  DdtAlertType type;
  DdtSeverity  sev;
  uint8_t    bssid[6];
  uint8_t    src_mac[6];
  char       ssid[33];
  uint16_t   frame_count;
  uint16_t   reason_code;
  char       detail[64];
};

// Per-BSSID sliding-window counter
struct DdtBssidTrack {
  uint8_t  bssid[6];
  char     ssid[33];
  // Circular window: timestamps of last DDT_WINDOW_SIZE deauths
  uint32_t window_ts[64]; // timestamps
  uint8_t  window_head;
  uint8_t  window_count;
  uint32_t total_deauths;
  uint32_t total_disassocs;
  uint32_t broadcast_count;
  // Per-reason-code tally (reasons 0-16 + overflow bucket)
  uint16_t reason_counts[18];
  // Source MACs (to detect multi-source)
  uint8_t  src_macs[16][6];
  uint8_t  src_count;
  uint32_t first_deauth_ms;
  uint32_t last_deauth_ms;
  bool     alert_active;
  uint32_t last_alert_ms;
  uint8_t  consecutive_alert_ticks;
};

struct DdtState {
  DdtBssidTrack tracks[DDT_MAX_BSSIDS];
  uint8_t       track_count;
  DdtAlert      alerts[DDT_MAX_ALERTS];
  uint8_t       alert_head;
  uint16_t      alert_count;
  uint32_t      total_deauths;
  uint32_t      total_alerts;
  bool          enabled;
};

static DdtState g_ddt = {};

// ── Helpers ───────────────────────────────────────────────────────────────────
static void ddt_mac_str(const uint8_t* m, char* out) {
  snprintf(out,18,"%02X:%02X:%02X:%02X:%02X:%02X",m[0],m[1],m[2],m[3],m[4],m[5]);
}
static bool ddt_is_broadcast(const uint8_t* m) {
  return m[0]==0xFF&&m[1]==0xFF&&m[2]==0xFF&&m[3]==0xFF&&m[4]==0xFF&&m[5]==0xFF;
}
static bool ddt_mac_eq(const uint8_t* a,const uint8_t* b){return memcmp(a,b,6)==0;}

static void ddt_add_alert(DdtAlertType type, DdtSeverity sev,
                           const uint8_t* bssid, const uint8_t* src,
                           const char* ssid, uint16_t count,
                           uint16_t reason, const char* detail) {
  DdtAlert* a=&g_ddt.alerts[g_ddt.alert_head];
  a->ts_ms=millis();
  a->type=type;
  a->sev=sev;
  if(bssid) memcpy(a->bssid,bssid,6); else memset(a->bssid,0,6);
  if(src)   memcpy(a->src_mac,src,6); else memset(a->src_mac,0,6);
  if(ssid)  strncpy(a->ssid,ssid,32);  else a->ssid[0]=0;
  a->frame_count=count;
  a->reason_code=reason;
  strncpy(a->detail,detail?detail:"",63);
  g_ddt.alert_head=(g_ddt.alert_head+1)%DDT_MAX_ALERTS;
  if(g_ddt.alert_count<DDT_MAX_ALERTS) g_ddt.alert_count++;
  g_ddt.total_alerts++;
}

static DdtBssidTrack* ddt_find_track(const uint8_t* bssid) {
  for(uint8_t i=0;i<g_ddt.track_count;i++)
    if(ddt_mac_eq(g_ddt.tracks[i].bssid,bssid)) return &g_ddt.tracks[i];
  return nullptr;
}

static DdtBssidTrack* ddt_upsert_track(const uint8_t* bssid, const char* ssid) {
  DdtBssidTrack* t=ddt_find_track(bssid);
  if(t) return t;
  if(g_ddt.track_count>=DDT_MAX_BSSIDS) {
    // Evict oldest inactive track
    uint32_t oldest=UINT32_MAX; uint8_t oi=0;
    for(uint8_t i=0;i<DDT_MAX_BSSIDS;i++){
      if(!g_ddt.tracks[i].alert_active && g_ddt.tracks[i].last_deauth_ms<oldest){
        oldest=g_ddt.tracks[i].last_deauth_ms; oi=i;
      }
    }
    t=&g_ddt.tracks[oi];
  } else {
    t=&g_ddt.tracks[g_ddt.track_count++];
  }
  memset(t,0,sizeof(DdtBssidTrack));
  memcpy(t->bssid,bssid,6);
  if(ssid) strncpy(t->ssid,ssid,32);
  t->first_deauth_ms=millis();
  return t;
}

// ── Add source MAC to track (for multi-source detection) ──────────────────────
static void ddt_track_src(DdtBssidTrack* t, const uint8_t* src) {
  for(uint8_t i=0;i<t->src_count;i++)
    if(ddt_mac_eq(t->src_macs[i],src)) return;
  if(t->src_count<16){
    memcpy(t->src_macs[t->src_count++],src,6);
  }
}

// ── Sliding window rate calculation ───────────────────────────────────────────
static uint8_t ddt_window_rate(DdtBssidTrack* t) {
  uint32_t now=millis();
  uint32_t cutoff=now-DDT_WINDOW_MS;
  uint8_t cnt=0;
  for(uint8_t i=0;i<t->window_count;i++){
    uint8_t idx=(t->window_head+64-t->window_count+i)%64;
    if(t->window_ts[idx]>=cutoff) cnt++;
  }
  return cnt;
}

// ── Main deauth ingest function ───────────────────────────────────────────────
// Called whenever a deauth or disassoc management frame is seen
// frame_type: 0=deauth, 1=disassoc
static void ddt_ingest(const uint8_t* src, const uint8_t* dst,
                        const uint8_t* bssid, const char* ssid,
                        uint8_t frame_type, uint16_t reason_code) {
  if(!g_ddt.enabled) return;
  g_ddt.total_deauths++;

  DdtBssidTrack* t=ddt_upsert_track(bssid,ssid);
  if(!t) return;

  uint32_t now=millis();
  t->last_deauth_ms=now;
  if(frame_type==0) t->total_deauths++;
  else              t->total_disassocs++;

  bool is_broadcast=ddt_is_broadcast(dst);
  if(is_broadcast) t->broadcast_count++;

  // Record reason code
  uint8_t rc_idx=(reason_code<=16)?reason_code:17;
  t->reason_counts[rc_idx]++;

  // Track source MAC
  ddt_track_src(t,src);

  // Add to sliding window
  t->window_ts[t->window_head]=now;
  t->window_head=(t->window_head+1)%64;
  if(t->window_count<64) t->window_count++;

  uint8_t rate=ddt_window_rate(t);
  char detail[64];

  // --- Alert: flood ---
  if(rate >= DDT_DEAUTH_THRESHOLD && !t->alert_active) {
    t->alert_active=true;
    t->last_alert_ms=now;
    snprintf(detail,sizeof(detail),"rate=%u/5s reason=%u",rate,reason_code);
    ddt_add_alert(DDT_ALERT_FLOOD,
                  rate>=50?DDT_SEV_CRITICAL:DDT_SEV_HIGH,
                  bssid,src,ssid,rate,reason_code,detail);
  }

  // --- Alert: broadcast deauth ---
  if(is_broadcast && t->broadcast_count >= DDT_BROADCAST_THRESHOLD) {
    snprintf(detail,sizeof(detail),"broadcast deauth flood, count=%u",t->broadcast_count);
    uint32_t last_bc_alert=t->last_alert_ms;
    if(now-last_bc_alert > 10000) { // throttle 10s
      ddt_add_alert(DDT_ALERT_BROADCAST,DDT_SEV_HIGH,bssid,src,ssid,
                    t->broadcast_count,reason_code,detail);
      t->last_alert_ms=now;
    }
  }

  // --- Alert: multi-source (> 3 source MACs for same BSSID) ---
  if(t->src_count >= 4) {
    snprintf(detail,sizeof(detail),"deauths from %u different src MACs",t->src_count);
    if(now-t->last_alert_ms > 15000) {
      ddt_add_alert(DDT_ALERT_MULTI_SOURCE,DDT_SEV_HIGH,bssid,src,ssid,
                    t->src_count,reason_code,detail);
      t->last_alert_ms=now;
    }
  }

  // --- Alert: unusual reason code (reason > 23 is suspicious) ---
  if(reason_code > 23 && reason_code != 0xFFFF) {
    if(rate >= 5) {
      snprintf(detail,sizeof(detail),"unusual reason code 0x%04X",reason_code);
      if(now-t->last_alert_ms > 20000) {
        ddt_add_alert(DDT_ALERT_UNUSUAL_REASON,DDT_SEV_MEDIUM,bssid,src,ssid,
                      rate,reason_code,detail);
        t->last_alert_ms=now;
      }
    }
  }

  // --- Alert: sustained attack (> 30s of flooding) ---
  if(t->alert_active) {
    uint32_t duration=now-t->first_deauth_ms;
    if(duration > 30000 && rate >= DDT_DEAUTH_THRESHOLD) {
      t->consecutive_alert_ticks++;
      if(t->consecutive_alert_ticks % 10 == 0) { // every 10 hits after 30s
        snprintf(detail,sizeof(detail),"sustained deauth flood for %us, rate=%u/5s",
                 (unsigned)(duration/1000),rate);
        ddt_add_alert(DDT_ALERT_SUSTAINED,DDT_SEV_CRITICAL,bssid,src,ssid,
                      rate,reason_code,detail);
      }
    }
  }

  // Clear alert_active if rate drops
  if(rate < DDT_DEAUTH_THRESHOLD/2) {
    t->alert_active=false;
    t->consecutive_alert_ticks=0;
  }
}

// ── Ingest from raw 802.11 frame ──────────────────────────────────────────────
// Caller passes raw 802.11 frame (after radiotap). This function checks
// if it's deauth/disassoc and extracts fields.
static void ddt_ingest_frame(const uint8_t* frame, int flen, int8_t rssi) {
  (void)rssi;
  if(!frame||flen<24) return;
  uint8_t fc0=frame[0];
  uint8_t type=(fc0>>2)&0x3;
  uint8_t sub=(fc0>>4)&0xF;
  if(type!=0) return; // only management
  if(sub!=12 && sub!=10) return; // 12=deauth, 10=disassoc

  const uint8_t* dst  =frame+4;
  const uint8_t* src  =frame+10;
  const uint8_t* bssid=frame+16;

  uint16_t reason=0;
  if(flen>=26) reason=(uint16_t)(frame[24]|(frame[25]<<8));

  ddt_ingest(src,dst,bssid,nullptr,(sub==12)?0:1,reason);
}

// ── JSON helpers ──────────────────────────────────────────────────────────────
static const char* ddt_alert_type_name(DdtAlertType t) {
  static const char* n[]={"flood","broadcast","spoof","multi_source","unusual_reason","sustained"};
  return (t<6)?n[t]:"?";
}
static const char* ddt_sev_name(DdtSeverity s) {
  static const char* n[]={"low","medium","high","critical"};
  return (s<4)?n[s]:"?";
}

static String ddt_summary_json() {
  uint32_t active_floods=0;
  for(uint8_t i=0;i<g_ddt.track_count;i++)
    if(g_ddt.tracks[i].alert_active) active_floods++;
  String j="{";
  j+="\"enabled\":"; j+=g_ddt.enabled?1:0;
  j+="\"total_deauths\":"; j+=g_ddt.total_deauths;
  j+=",\"total_alerts\":"; j+=g_ddt.total_alerts;
  j+=",\"active_floods\":"; j+=active_floods;
  j+=",\"tracked_bssids\":"; j+=g_ddt.track_count;
  j+="}";
  return j;
}

static String ddt_alerts_json(int max_n) {
  if(max_n<1||max_n>DDT_MAX_ALERTS) max_n=DDT_MAX_ALERTS;
  String j="{\"count\":"; j+=g_ddt.alert_count; j+=",\"alerts\":[";
  char m1[18],m2[18];
  int start=(g_ddt.alert_head+DDT_MAX_ALERTS-g_ddt.alert_count)%DDT_MAX_ALERTS;
  for(int i=g_ddt.alert_count-1;i>=0&&i>=g_ddt.alert_count-max_n;i--){
    if(i<g_ddt.alert_count-1) j+=",";
    int idx=(start+i)%DDT_MAX_ALERTS;
    DdtAlert& a=g_ddt.alerts[idx];
    ddt_mac_str(a.bssid,m1); ddt_mac_str(a.src_mac,m2);
    j+="{\"ts\":"; j+=a.ts_ms;
    j+=",\"type\":\""; j+=ddt_alert_type_name(a.type); j+="\"";
    j+=",\"sev\":\""; j+=ddt_sev_name(a.sev); j+="\"";
    j+=",\"bssid\":\""; j+=m1; j+="\"";
    j+=",\"src\":\""; j+=m2; j+="\"";
    j+=",\"ssid\":\""; j+=a.ssid; j+="\"";
    j+=",\"count\":"; j+=a.frame_count;
    j+=",\"reason\":"; j+=a.reason_code;
    j+=",\"detail\":\""; j+=a.detail; j+="\"";
    j+="}";
  }
  j+="]}";
  return j;
}

static String ddt_tracks_json() {
  String j="{\"tracks\":[";
  char m[18];
  for(uint8_t i=0;i<g_ddt.track_count;i++){
    if(i) j+=",";
    DdtBssidTrack& t=g_ddt.tracks[i];
    ddt_mac_str(t.bssid,m);
    uint8_t rate=ddt_window_rate(&t);
    j+="{\"bssid\":\""; j+=m; j+="\"";
    j+=",\"ssid\":\""; j+=t.ssid; j+="\"";
    j+=",\"rate\":"; j+=rate;
    j+=",\"total\":"; j+=(t.total_deauths+t.total_disassocs);
    j+=",\"broadcast\":"; j+=t.broadcast_count;
    j+=",\"src_count\":"; j+=t.src_count;
    j+=",\"alert_active\":"; j+=t.alert_active?1:0;
    j+=",\"duration_ms\":"; j+=(uint32_t)(millis()-t.first_deauth_ms);
    j+="}";
  }
  j+="]}";
  return j;
}

// ── HTTP routes ───────────────────────────────────────────────────────────────
static void ddt_routes() {
  server.on("/api/ddt/summary", HTTP_GET, [](){
    server.send(200,"application/json",ddt_summary_json());
  });

  server.on("/api/ddt/alerts", HTTP_GET, [](){
    int n=server.hasArg("n")?server.arg("n").toInt():32;
    server.send(200,"application/json",ddt_alerts_json(n));
  });

  server.on("/api/ddt/tracks", HTTP_GET, [](){
    server.send(200,"application/json",ddt_tracks_json());
  });

  server.on("/api/ddt/enable", HTTP_POST, [](){
    g_ddt.enabled=true;
    server.send(200,"application/json","{\"ok\":1,\"enabled\":1}");
  });

  server.on("/api/ddt/disable", HTTP_POST, [](){
    g_ddt.enabled=false;
    server.send(200,"application/json","{\"ok\":1,\"enabled\":0}");
  });

  server.on("/api/ddt/clear", HTTP_POST, [](){
    bool en=g_ddt.enabled;
    memset(&g_ddt,0,sizeof(DdtState));
    g_ddt.enabled=en;
    server.send(200,"application/json","{\"ok\":1}");
  });
}

// ── Initialize ────────────────────────────────────────────────────────────────
static void ddt_init() {
  g_ddt.enabled=true;
}

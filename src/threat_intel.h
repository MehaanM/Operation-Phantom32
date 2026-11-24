#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <string.h>
#include <stdint.h>

// ── Threat Intelligence Engine ────────────────────────────────────────────────
//
// Threat scoring, Indicators of Compromise (IoC), rogue AP detection,
// evil twin fingerprinting, and attack pattern recognition.

// ── Scoring weights ───────────────────────────────────────────────────────────
#define TI_SCORE_DEAUTH_BURST    30   // per deauth burst
#define TI_SCORE_PROBE_FLOOD     20
#define TI_SCORE_BEACON_FLOOD    25
#define TI_SCORE_AUTH_FLOOD      20
#define TI_SCORE_EVIL_TWIN       80
#define TI_SCORE_ROGUE_AP        60
#define TI_SCORE_KARMA           70
#define TI_SCORE_WEP_NETWORK     40
#define TI_SCORE_OPEN_HOTSPOT    15
#define TI_SCORE_KNOWN_ATKHW     50
#define TI_SCORE_RAPID_MAC_CHG   35
#define TI_SCORE_MULTIBAND_DEAUTH 45
#define TI_SCORE_PMF_BYPASS_ATMP 55
#define TI_SCORE_FAKE_MGMT_IE    40

// ── Threat severity ───────────────────────────────────────────────────────────
enum TiSeverity : uint8_t {
  TI_SEV_INFO     = 0,
  TI_SEV_LOW      = 1,
  TI_SEV_MEDIUM   = 2,
  TI_SEV_HIGH     = 3,
  TI_SEV_CRITICAL = 4,
};

static const char* ti_sev_name(TiSeverity s) {
  switch(s){
    case TI_SEV_INFO:     return "INFO";
    case TI_SEV_LOW:      return "LOW";
    case TI_SEV_MEDIUM:   return "MEDIUM";
    case TI_SEV_HIGH:     return "HIGH";
    case TI_SEV_CRITICAL: return "CRITICAL";
    default:              return "UNKNOWN";
  }
}

static TiSeverity ti_score_to_sev(uint8_t score) {
  if(score<10) return TI_SEV_INFO;
  if(score<30) return TI_SEV_LOW;
  if(score<60) return TI_SEV_MEDIUM;
  if(score<85) return TI_SEV_HIGH;
  return TI_SEV_CRITICAL;
}

// ── Threat category ───────────────────────────────────────────────────────────
enum TiCategory : uint8_t {
  TI_CAT_NONE         = 0,
  TI_CAT_JAMMING      = 1,
  TI_CAT_ROGUE_AP     = 2,
  TI_CAT_EVIL_TWIN    = 3,
  TI_CAT_MAN_IN_MIDDLE= 4,
  TI_CAT_RECON        = 5,
  TI_CAT_CRACKING     = 6,
  TI_CAT_INJECTION    = 7,
  TI_CAT_SPOOFING     = 8,
  TI_CAT_REPLAY       = 9,
};

static const char* ti_cat_name(TiCategory c) {
  switch(c){
    case TI_CAT_JAMMING:       return "Jamming/DoS";
    case TI_CAT_ROGUE_AP:      return "Rogue AP";
    case TI_CAT_EVIL_TWIN:     return "Evil Twin";
    case TI_CAT_MAN_IN_MIDDLE: return "MITM";
    case TI_CAT_RECON:         return "Reconnaissance";
    case TI_CAT_CRACKING:      return "Credential Attack";
    case TI_CAT_INJECTION:     return "Frame Injection";
    case TI_CAT_SPOOFING:      return "MAC Spoofing";
    case TI_CAT_REPLAY:        return "Replay Attack";
    default:                   return "None";
  }
}

// ── IoC types ─────────────────────────────────────────────────────────────────
enum TiIocType : uint8_t {
  TI_IOC_MAC      = 0,
  TI_IOC_SSID     = 1,
  TI_IOC_OUI      = 2,
  TI_IOC_PATTERN  = 3,
};

// ── IoC record ────────────────────────────────────────────────────────────────
struct TiIoc {
  TiIocType type;
  union {
    uint8_t mac[6];
    char    ssid[33];
    uint8_t oui[3];
  };
  TiCategory category;
  uint8_t  score;
  char     description[48];
  uint32_t added_ms;
  uint32_t last_match_ms;
  uint32_t match_count;
};

// ── Threat actor record ───────────────────────────────────────────────────────
struct TiActor {
  uint8_t  mac[6];
  uint8_t  score;           // 0-100
  TiSeverity severity;
  TiCategory category;
  uint32_t first_seen_ms;
  uint32_t last_seen_ms;
  uint32_t frame_count;
  uint32_t deauth_count;
  uint32_t probe_count;
  uint32_t beacon_count;
  bool     is_active;
  char     notes[64];
};

#define TI_MAX_IOCS     32
#define TI_MAX_ACTORS   24
#define TI_MAX_ALERTS   48

// ── Alert record ──────────────────────────────────────────────────────────────
struct TiAlert {
  TiSeverity severity;
  TiCategory category;
  uint8_t  score;
  uint8_t  mac[6];
  uint8_t  channel;
  char     message[80];
  uint32_t timestamp_ms;
  bool     acknowledged;
};

// ── Global threat state ───────────────────────────────────────────────────────
struct TiState {
  TiIoc   iocs[TI_MAX_IOCS];
  uint8_t ioc_count;
  TiActor actors[TI_MAX_ACTORS];
  uint8_t actor_count;
  TiAlert alerts[TI_MAX_ALERTS];
  uint8_t alert_head;
  uint8_t alert_count;
  uint32_t unack_count;
  uint32_t total_alerts;
};

static TiState g_ti = {};

// ── Initialization ─────────────────────────────────────────────────────────────
static void ti_init() {
  memset(&g_ti, 0, sizeof(g_ti));
  // Seed known malicious SSIDs
  static const char* bad_ssids[] = {
    "KARMA","karma_attack","pwned","Evil_Twin","free_wifi",
    "FREE WIFI","opennetwork","Starbucks_Free_Wifi","xfinitywifi_clone",
    nullptr
  };
  for(int i=0;bad_ssids[i]&&g_ti.ioc_count<TI_MAX_IOCS;i++){
    TiIoc* ioc=&g_ti.iocs[g_ti.ioc_count++];
    ioc->type=TI_IOC_SSID;
    strncpy(ioc->ssid,bad_ssids[i],32);
    ioc->category=TI_CAT_ROGUE_AP;
    ioc->score=TI_SCORE_ROGUE_AP;
    snprintf(ioc->description,sizeof(ioc->description),"Known malicious SSID pattern");
    ioc->added_ms=millis();
  }
  // Seed known pentest OUIs
  static const uint8_t bad_ouis[][3] = {
    {0x00,0xC0,0xCA}, // Alfa Network
    {0x00,0x25,0x9C}, // Alfa (Atheros)
    {0x20,0xC0,0x47}, // Hak5
    {0xE0,0x3F,0x49}, // common pentest NIC
  };
  for(int i=0;i<4&&g_ti.ioc_count<TI_MAX_IOCS;i++){
    TiIoc* ioc=&g_ti.iocs[g_ti.ioc_count++];
    ioc->type=TI_IOC_OUI;
    memcpy(ioc->oui,bad_ouis[i],3);
    ioc->category=TI_CAT_INJECTION;
    ioc->score=TI_SCORE_KNOWN_ATKHW;
    snprintf(ioc->description,sizeof(ioc->description),"Known pentest hardware OUI");
    ioc->added_ms=millis();
  }
}

// ── Alert push ────────────────────────────────────────────────────────────────
static void ti_push_alert(TiSeverity sev, TiCategory cat, uint8_t score,
                            const uint8_t mac[6], uint8_t chan,
                            const char* msg) {
  TiAlert* a=&g_ti.alerts[g_ti.alert_head];
  a->severity=sev;
  a->category=cat;
  a->score=score;
  if(mac) memcpy(a->mac,mac,6); else memset(a->mac,0,6);
  a->channel=chan;
  strncpy(a->message,msg?msg:"",79); a->message[79]=0;
  a->timestamp_ms=millis();
  a->acknowledged=false;
  g_ti.alert_head=(g_ti.alert_head+1)%TI_MAX_ALERTS;
  if(g_ti.alert_count<TI_MAX_ALERTS) g_ti.alert_count++;
  g_ti.unack_count++;
  g_ti.total_alerts++;
}

// ── Actor upsert ──────────────────────────────────────────────────────────────
static TiActor* ti_actor_upsert(const uint8_t mac[6]) {
  for(uint8_t i=0;i<g_ti.actor_count;i++){
    if(memcmp(g_ti.actors[i].mac,mac,6)==0) return &g_ti.actors[i];
  }
  if(g_ti.actor_count>=TI_MAX_ACTORS){
    // Evict lowest-score actor
    uint8_t min_score=g_ti.actors[0].score;
    uint8_t mi=0;
    for(uint8_t i=1;i<g_ti.actor_count;i++){
      if(g_ti.actors[i].score<min_score){min_score=g_ti.actors[i].score;mi=i;}
    }
    memset(&g_ti.actors[mi],0,sizeof(TiActor));
    memcpy(g_ti.actors[mi].mac,mac,6);
    g_ti.actors[mi].first_seen_ms=millis();
    return &g_ti.actors[mi];
  }
  TiActor* a=&g_ti.actors[g_ti.actor_count++];
  memset(a,0,sizeof(*a));
  memcpy(a->mac,mac,6);
  a->first_seen_ms=millis();
  return a;
}

// ── IoC match ────────────────────────────────────────────────────────────────
static TiIoc* ti_match_mac(const uint8_t mac[6]) {
  for(uint8_t i=0;i<g_ti.ioc_count;i++){
    TiIoc* ioc=&g_ti.iocs[i];
    if(ioc->type==TI_IOC_MAC && memcmp(ioc->mac,mac,6)==0){
      ioc->match_count++; ioc->last_match_ms=millis();
      return ioc;
    }
    if(ioc->type==TI_IOC_OUI && mac[0]==ioc->oui[0] &&
       mac[1]==ioc->oui[1] && mac[2]==ioc->oui[2]){
      ioc->match_count++; ioc->last_match_ms=millis();
      return ioc;
    }
  }
  return nullptr;
}

static TiIoc* ti_match_ssid(const char* ssid) {
  if(!ssid||!ssid[0]) return nullptr;
  for(uint8_t i=0;i<g_ti.ioc_count;i++){
    TiIoc* ioc=&g_ti.iocs[i];
    if(ioc->type==TI_IOC_SSID){
      if(strcasecmp(ioc->ssid,ssid)==0){
        ioc->match_count++; ioc->last_match_ms=millis();
        return ioc;
      }
    }
  }
  return nullptr;
}

// ── Analyze frame for threats ─────────────────────────────────────────────────
static void ti_analyze(const uint8_t* src, const uint8_t* bssid,
                        const char* ssid,
                        uint8_t type, uint8_t stype,
                        uint8_t channel, int8_t rssi) {
  if(!src) return;
  bool is_bcast=(src[0]&1);

  TiActor* actor = is_bcast ? nullptr : ti_actor_upsert(src);
  if(actor){
    actor->frame_count++;
    actor->last_seen_ms=millis();
    actor->is_active=true;

    // Check IoC match for source MAC
    TiIoc* ioc=ti_match_mac(src);
    if(ioc){
      uint8_t new_score=actor->score+(ioc->score/2);
      if(new_score>100) new_score=100;
      actor->score=new_score;
      actor->severity=ti_score_to_sev(actor->score);
      char msg[80];
      snprintf(msg,sizeof(msg),"IoC match: %s",ioc->description);
      ti_push_alert(actor->severity,ioc->category,actor->score,src,channel,msg);
    }
  }

  // Management frame analysis
  if(type==0){
    if(stype==10||stype==12){
      // Deauth / Disassoc
      if(actor){
        actor->deauth_count++;
        if(actor->deauth_count>=5){
          uint8_t score=TI_SCORE_DEAUTH_BURST;
          if(actor->score+score>100) score=100-actor->score;
          actor->score+=score;
          actor->severity=ti_score_to_sev(actor->score);
          actor->category=TI_CAT_JAMMING;
          if(actor->deauth_count==5){
            ti_push_alert(TI_SEV_HIGH,TI_CAT_JAMMING,actor->score,
              src,channel,"Deauth burst detected");
          }
        }
      }
    }
    if(stype==4){
      // Probe request
      if(actor) actor->probe_count++;
      if(ssid&&ssid[0]){
        TiIoc* ioc=ti_match_ssid(ssid);
        if(ioc){
          ti_push_alert(TI_SEV_MEDIUM,ioc->category,ioc->score,
            src,channel,"Probe for known malicious SSID");
        }
      }
    }
    if(stype==8){
      // Beacon
      if(actor) actor->beacon_count++;
      if(ssid&&ssid[0]){
        TiIoc* ioc=ti_match_ssid(ssid);
        if(ioc){
          ti_push_alert(TI_SEV_HIGH,ioc->category,ioc->score,
            src,channel,"Beacon advertising known malicious SSID");
        }
      }
    }
  }
}

// ── Evil twin detection: two APs with same SSID on different channels ─────────
struct TiEvTwinCandidate {
  char ssid[33];
  uint8_t mac1[6];
  uint8_t mac2[6];
  uint8_t chan1;
  uint8_t chan2;
  int8_t  rssi1;
  int8_t  rssi2;
};

#define TI_MAX_ET_CANDIDATES 8
static TiEvTwinCandidate g_et_candidates[TI_MAX_ET_CANDIDATES];
static uint8_t g_et_count=0;

static void ti_check_evil_twin(const char* ssid, const uint8_t bssid[6],
                                 uint8_t chan, int8_t rssi) {
  if(!ssid||!ssid[0]||!bssid) return;
  for(uint8_t i=0;i<g_et_count;i++){
    if(strcmp(g_et_candidates[i].ssid,ssid)==0){
      if(memcmp(g_et_candidates[i].mac1,bssid,6)!=0 &&
         g_et_candidates[i].chan1!=chan){
        // Same SSID, different BSSID and channel
        memcpy(g_et_candidates[i].mac2,bssid,6);
        g_et_candidates[i].chan2=chan;
        g_et_candidates[i].rssi2=rssi;
        char msg[80];
        snprintf(msg,sizeof(msg),"Evil Twin: '%s' on ch%d vs ch%d",
          ssid,g_et_candidates[i].chan1,chan);
        ti_push_alert(TI_SEV_CRITICAL,TI_CAT_EVIL_TWIN,TI_SCORE_EVIL_TWIN,
          bssid,chan,msg);
        return;
      }
    }
  }
  if(g_et_count<TI_MAX_ET_CANDIDATES){
    TiEvTwinCandidate& c=g_et_candidates[g_et_count++];
    strncpy(c.ssid,ssid,32); c.ssid[32]=0;
    memcpy(c.mac1,bssid,6);
    c.chan1=chan; c.rssi1=rssi;
  }
}

// ── Add custom IoC ────────────────────────────────────────────────────────────
static bool ti_add_mac_ioc(const uint8_t mac[6], TiCategory cat, uint8_t score, const char* desc) {
  if(g_ti.ioc_count>=TI_MAX_IOCS) return false;
  TiIoc* ioc=&g_ti.iocs[g_ti.ioc_count++];
  ioc->type=TI_IOC_MAC;
  memcpy(ioc->mac,mac,6);
  ioc->category=cat;
  ioc->score=score;
  strncpy(ioc->description,desc?desc:"",47);
  ioc->added_ms=millis();
  return true;
}

static bool ti_add_ssid_ioc(const char* ssid, TiCategory cat, uint8_t score, const char* desc) {
  if(!ssid||g_ti.ioc_count>=TI_MAX_IOCS) return false;
  TiIoc* ioc=&g_ti.iocs[g_ti.ioc_count++];
  ioc->type=TI_IOC_SSID;
  strncpy(ioc->ssid,ssid,32);
  ioc->category=cat;
  ioc->score=score;
  strncpy(ioc->description,desc?desc:"",47);
  ioc->added_ms=millis();
  return true;
}

// ── Acknowledge alerts ────────────────────────────────────────────────────────
static void ti_ack_all() {
  int start=(g_ti.alert_head+TI_MAX_ALERTS-g_ti.alert_count)%TI_MAX_ALERTS;
  for(uint8_t i=0;i<g_ti.alert_count;i++){
    int idx=(start+i)%TI_MAX_ALERTS;
    if(!g_ti.alerts[idx].acknowledged){
      g_ti.alerts[idx].acknowledged=true;
    }
  }
  g_ti.unack_count=0;
}

// ── Actors JSON ───────────────────────────────────────────────────────────────
static String ti_actors_json() {
  String j="{\"count\":"+String(g_ti.actor_count)+",\"actors\":[";
  for(uint8_t i=0;i<g_ti.actor_count;i++){
    if(i) j+=",";
    TiActor& a=g_ti.actors[i];
    char mac[18];
    snprintf(mac,sizeof(mac),"%02X:%02X:%02X:%02X:%02X:%02X",
      a.mac[0],a.mac[1],a.mac[2],a.mac[3],a.mac[4],a.mac[5]);
    j+="{\"mac\":\""; j+=mac; j+="\"";
    j+=",\"score\":"+String(a.score);
    j+=",\"sev\":\""+String(ti_sev_name(a.severity))+"\"";
    j+=",\"cat\":\""+String(ti_cat_name(a.category))+"\"";
    j+=",\"frames\":"+String(a.frame_count);
    j+=",\"deauths\":"+String(a.deauth_count);
    j+=",\"active\":"+String(a.is_active?1:0);
    j+="}";
  }
  j+="]}";
  return j;
}

// ── Alerts JSON ───────────────────────────────────────────────────────────────
static String ti_alerts_json(int max_alerts) {
  if(max_alerts<1||max_alerts>TI_MAX_ALERTS) max_alerts=TI_MAX_ALERTS;
  String j="{\"total\":"+String(g_ti.total_alerts);
  j+=",\"unack\":"+String(g_ti.unack_count);
  j+=",\"alerts\":[";
  int shown=0;
  int start=(g_ti.alert_head+TI_MAX_ALERTS-g_ti.alert_count)%TI_MAX_ALERTS;
  for(int i=g_ti.alert_count-1;i>=0&&shown<max_alerts;i--){
    int idx=(start+i)%TI_MAX_ALERTS;
    if(shown) j+=",";
    TiAlert& a=g_ti.alerts[idx];
    char mac[18];
    snprintf(mac,sizeof(mac),"%02X:%02X:%02X:%02X:%02X:%02X",
      a.mac[0],a.mac[1],a.mac[2],a.mac[3],a.mac[4],a.mac[5]);
    j+="{\"sev\":\""+String(ti_sev_name(a.severity))+"\"";
    j+=",\"cat\":\""+String(ti_cat_name(a.category))+"\"";
    j+=",\"score\":"+String(a.score);
    j+=",\"mac\":\""; j+=mac; j+="\"";
    j+=",\"ch\":"+String(a.channel);
    j+=",\"msg\":\""; j+=a.message; j+="\"";
    j+=",\"ts\":"+String(a.timestamp_ms);
    j+=",\"ack\":"+String(a.acknowledged?1:0);
    j+="}";
    shown++;
  }
  j+="]}";
  return j;
}

// ── IoC list JSON ─────────────────────────────────────────────────────────────
static String ti_iocs_json() {
  String j="{\"count\":"+String(g_ti.ioc_count)+",\"iocs\":[";
  for(uint8_t i=0;i<g_ti.ioc_count;i++){
    if(i) j+=",";
    TiIoc& ioc=g_ti.iocs[i];
    j+="{\"type\":"+String((int)ioc.type);
    j+=",\"cat\":\""+String(ti_cat_name(ioc.category))+"\"";
    j+=",\"score\":"+String(ioc.score);
    j+=",\"matches\":"+String(ioc.match_count);
    j+=",\"desc\":\""; j+=ioc.description; j+="\"";
    if(ioc.type==TI_IOC_SSID){j+=",\"ssid\":\""; j+=ioc.ssid; j+="\""; }
    else if(ioc.type==TI_IOC_MAC||ioc.type==TI_IOC_OUI){
      char mac[18];
      if(ioc.type==TI_IOC_MAC)
        snprintf(mac,sizeof(mac),"%02X:%02X:%02X:%02X:%02X:%02X",
          ioc.mac[0],ioc.mac[1],ioc.mac[2],ioc.mac[3],ioc.mac[4],ioc.mac[5]);
      else
        snprintf(mac,sizeof(mac),"%02X:%02X:%02X",ioc.oui[0],ioc.oui[1],ioc.oui[2]);
      j+=",\"mac\":\""; j+=mac; j+="\"";
    }
    j+="}";
  }
  j+="]}";
  return j;
}

// ── HTTP routes ───────────────────────────────────────────────────────────────
static void ti_routes() {
  extern WebServer server;

  server.on("/api/ti/actors",HTTP_GET,[](){
    server.send(200,"application/json",ti_actors_json());
  });

  server.on("/api/ti/alerts",HTTP_GET,[](){
    int n=server.hasArg("n")?server.arg("n").toInt():20;
    server.send(200,"application/json",ti_alerts_json(n));
  });

  server.on("/api/ti/iocs",HTTP_GET,[](){
    server.send(200,"application/json",ti_iocs_json());
  });

  server.on("/api/ti/ack",HTTP_POST,[](){
    ti_ack_all();
    server.send(200,"application/json","{\"ok\":1}");
  });

  server.on("/api/ti/add_ssid",HTTP_POST,[](){
    if(!server.hasArg("ssid")){server.send(400,"application/json","{\"error\":\"ssid required\"}");return;}
    bool ok=ti_add_ssid_ioc(server.arg("ssid").c_str(),TI_CAT_ROGUE_AP,50,"User-added SSID IoC");
    server.send(200,"application/json",ok?"{\"ok\":1}":"{\"error\":\"full\"}");
  });

  server.on("/api/ti/summary",HTTP_GET,[](){
    char buf[256];
    snprintf(buf,sizeof(buf),
      "{\"actors\":%u,\"iocs\":%u,\"alerts\":%u,\"unack\":%u,\"evtwins\":%u}",
      g_ti.actor_count, g_ti.ioc_count,
      g_ti.total_alerts, (unsigned)g_ti.unack_count,
      g_et_count);
    server.send(200,"application/json",buf);
  });

  server.on("/api/ti/reset",HTTP_POST,[](){
    ti_init();
    server.send(200,"application/json","{\"ok\":1}");
  });
}

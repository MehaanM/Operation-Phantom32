#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <string.h>
#include <stdint.h>

// ── RF Monitor ────────────────────────────────────────────────────────────────
//
// Channel hopping scanner with per-channel dwell, signal analysis,
// interference detection, and hidden SSID recovery heuristics.

// ── Configuration ─────────────────────────────────────────────────────────────
#define RFM_DWELL_MS_DEFAULT   100     // ms per channel
#define RFM_DWELL_MS_MIN        20
#define RFM_DWELL_MS_MAX      1000
#define RFM_SCAN_CHANNELS_24    13     // 2.4 GHz channels to scan
#define RFM_SCAN_CHANNELS_5     20     // 5 GHz channels to scan
#define RFM_MAX_SCANS           16     // rolling scan history
#define RFM_MAX_EVENTS          64     // event ring buffer
#define RFM_WINDOW_MS         5000     // anomaly detection window

// ── 2.4 GHz channel list ──────────────────────────────────────────────────────
static const uint8_t RFM_CHANS_24[] = {1,2,3,4,5,6,7,8,9,10,11,12,13};

// ── 5 GHz channel list (common UNII bands) ───────────────────────────────────
static const uint8_t RFM_CHANS_5[] = {
  36,40,44,48,52,56,60,64,     // UNII-1 + UNII-2
  100,104,108,112,116,120,124,128,132,136,140, // UNII-2e
  149,153,157,161,165           // UNII-3
};

// ── Channel scan result ───────────────────────────────────────────────────────
struct RfmChanResult {
  uint8_t  channel;
  uint32_t frame_count;
  uint32_t mgmt_count;
  uint32_t data_count;
  int8_t   rssi_min;
  int8_t   rssi_max;
  int16_t  rssi_avg_x10;
  uint8_t  bssid_count;
  uint32_t dwell_ms;
  uint32_t timestamp_ms;
  bool     has_hidden;      // at least one beacon with empty SSID seen
  bool     has_wep;
  bool     has_open;
  bool     has_wpa;
  bool     has_wpa2;
  bool     has_wpa3;
  uint8_t  hidden_bssids[4][6];  // up to 4 hidden AP MACs
  uint8_t  hidden_count;
};

// ── Full scan snapshot ────────────────────────────────────────────────────────
struct RfmScan {
  RfmChanResult results[RFM_SCAN_CHANNELS_24 + RFM_SCAN_CHANNELS_5];
  uint8_t  chan_count;
  uint32_t start_ms;
  uint32_t end_ms;
  uint32_t total_frames;
  uint8_t  band;          // 0=2.4, 1=5, 2=both
};

// ── RF event types ────────────────────────────────────────────────────────────
enum RfmEventType : uint8_t {
  RFM_EVT_NEW_BSSID       = 0,
  RFM_EVT_BSSID_GONE      = 1,
  RFM_EVT_HIGH_DEAUTH     = 2,
  RFM_EVT_HIGH_PROBE      = 3,
  RFM_EVT_CHANNEL_CHANGE  = 4,
  RFM_EVT_HIDDEN_SSID     = 5,
  RFM_EVT_DEAUTH_FLOOD    = 6,
  RFM_EVT_BEACON_FLOOD    = 7,
  RFM_EVT_AUTH_FLOOD      = 8,
  RFM_EVT_HANDSHAKE       = 9,
  RFM_EVT_PMKID           = 10,
  RFM_EVT_ROGUE_AP        = 11,
  RFM_EVT_EVIL_TWIN       = 12,
  RFM_EVT_KARMA           = 13,
  RFM_EVT_WEP_NETWORK     = 14,
  RFM_EVT_OPEN_NETWORK    = 15,
};

static const char* rfm_event_name(RfmEventType t) {
  switch(t){
    case RFM_EVT_NEW_BSSID:     return "New AP";
    case RFM_EVT_BSSID_GONE:    return "AP Gone";
    case RFM_EVT_HIGH_DEAUTH:   return "High Deauth Rate";
    case RFM_EVT_HIGH_PROBE:    return "High Probe Rate";
    case RFM_EVT_CHANNEL_CHANGE:return "Channel Change";
    case RFM_EVT_HIDDEN_SSID:   return "Hidden SSID AP";
    case RFM_EVT_DEAUTH_FLOOD:  return "Deauth Flood";
    case RFM_EVT_BEACON_FLOOD:  return "Beacon Flood";
    case RFM_EVT_AUTH_FLOOD:    return "Auth Flood";
    case RFM_EVT_HANDSHAKE:     return "EAPOL Handshake";
    case RFM_EVT_PMKID:         return "PMKID Captured";
    case RFM_EVT_ROGUE_AP:      return "Rogue AP";
    case RFM_EVT_EVIL_TWIN:     return "Evil Twin";
    case RFM_EVT_KARMA:         return "KARMA Response";
    case RFM_EVT_WEP_NETWORK:   return "WEP Network";
    case RFM_EVT_OPEN_NETWORK:  return "Open Network";
    default:                    return "Unknown";
  }
}

// ── RF event record ───────────────────────────────────────────────────────────
struct RfmEvent {
  RfmEventType type;
  uint8_t  channel;
  uint8_t  mac[6];
  int8_t   rssi;
  uint32_t timestamp_ms;
  char     detail[48];
};

// ── Interference source ───────────────────────────────────────────────────────
enum RfmInterfType : uint8_t {
  RFM_INTF_NONE       = 0,
  RFM_INTF_MICROWAVE  = 1,
  RFM_INTF_BLUETOOTH  = 2,
  RFM_INTF_ZIGBEE     = 3,
  RFM_INTF_RADAR      = 4,
  RFM_INTF_CORDLESS   = 5,
  RFM_INTF_VIDEO_CAM  = 6,
};

// ── Signal quality classification ─────────────────────────────────────────────
enum RfmQuality : uint8_t {
  RFM_QUALITY_EXCELLENT = 0,
  RFM_QUALITY_GOOD      = 1,
  RFM_QUALITY_FAIR      = 2,
  RFM_QUALITY_POOR      = 3,
  RFM_QUALITY_UNUSABLE  = 4,
};

static RfmQuality rfm_classify_rssi(int8_t rssi) {
  if(rssi >= -50) return RFM_QUALITY_EXCELLENT;
  if(rssi >= -60) return RFM_QUALITY_GOOD;
  if(rssi >= -70) return RFM_QUALITY_FAIR;
  if(rssi >= -80) return RFM_QUALITY_POOR;
  return RFM_QUALITY_UNUSABLE;
}

static const char* rfm_quality_name(RfmQuality q) {
  switch(q){
    case RFM_QUALITY_EXCELLENT: return "Excellent";
    case RFM_QUALITY_GOOD:      return "Good";
    case RFM_QUALITY_FAIR:      return "Fair";
    case RFM_QUALITY_POOR:      return "Poor";
    case RFM_QUALITY_UNUSABLE:  return "Unusable";
    default:                    return "Unknown";
  }
}

// ── Monitor state ─────────────────────────────────────────────────────────────
struct RfmState {
  bool     active;
  bool     hopping;        // channel hopping enabled
  uint8_t  band;           // 0=2.4, 1=5, 2=both
  uint8_t  current_chan;
  uint8_t  chan_index;     // position in channel list
  uint32_t dwell_ms;
  uint32_t last_hop_ms;

  RfmChanResult chan_stats[RFM_SCAN_CHANNELS_24];
  RfmScan       scans[RFM_MAX_SCANS];
  uint8_t       scan_head;
  uint8_t       scan_count;

  RfmEvent events[RFM_MAX_EVENTS];
  uint8_t  ev_head;
  uint8_t  ev_count;

  // Anomaly counters (reset each window)
  uint32_t window_start_ms;
  uint32_t deauth_in_window;
  uint32_t probe_in_window;
  uint32_t beacon_in_window;
  uint32_t auth_in_window;

  uint32_t total_hops;
  uint32_t total_scans_done;
};

static RfmState g_rfm = {};

// ── Initialization ─────────────────────────────────────────────────────────────
static void rfm_init() {
  memset(&g_rfm, 0, sizeof(g_rfm));
  g_rfm.dwell_ms      = RFM_DWELL_MS_DEFAULT;
  g_rfm.band          = 0;
  g_rfm.current_chan  = 1;
  g_rfm.window_start_ms = millis();
  for(int i=0;i<RFM_SCAN_CHANNELS_24;i++){
    g_rfm.chan_stats[i].channel=i+1;
    g_rfm.chan_stats[i].rssi_min=0;
    g_rfm.chan_stats[i].rssi_max=-100;
  }
}

// ── Event push ────────────────────────────────────────────────────────────────
static void rfm_push_event(RfmEventType type, uint8_t chan,
                             const uint8_t mac[6], int8_t rssi,
                             const char* detail) {
  RfmEvent* ev=&g_rfm.events[g_rfm.ev_head];
  ev->type=type;
  ev->channel=chan;
  if(mac) memcpy(ev->mac,mac,6); else memset(ev->mac,0,6);
  ev->rssi=rssi;
  ev->timestamp_ms=millis();
  strncpy(ev->detail,detail?detail:"",47);
  ev->detail[47]=0;
  g_rfm.ev_head=(g_rfm.ev_head+1)%RFM_MAX_EVENTS;
  if(g_rfm.ev_count<RFM_MAX_EVENTS) g_rfm.ev_count++;
}

// ── Channel hop ───────────────────────────────────────────────────────────────
static void rfm_hop() {
  uint32_t now=millis();
  if(now-g_rfm.last_hop_ms < g_rfm.dwell_ms) return;
  g_rfm.last_hop_ms=now;

  const uint8_t* clist=RFM_CHANS_24;
  uint8_t clen=RFM_SCAN_CHANNELS_24;
  if(g_rfm.band==1){clist=RFM_CHANS_5; clen=RFM_SCAN_CHANNELS_5;}

  g_rfm.chan_index=(g_rfm.chan_index+1)%clen;
  g_rfm.current_chan=clist[g_rfm.chan_index];
  esp_wifi_set_channel(g_rfm.current_chan, WIFI_SECOND_CHAN_NONE);
  g_rfm.total_hops++;
}

// ── Frame ingestion for RF monitor ───────────────────────────────────────────
static void rfm_record(const uint8_t* src, const uint8_t* bssid,
                        uint8_t type, uint8_t stype,
                        uint8_t channel, int8_t rssi, uint16_t len) {
  if(channel<1||channel>RFM_SCAN_CHANNELS_24) return;
  RfmChanResult* cs=&g_rfm.chan_stats[channel-1];
  cs->frame_count++;
  if(type==0) cs->mgmt_count++;
  else if(type==2) cs->data_count++;
  if(cs->rssi_min==0||rssi<cs->rssi_min) cs->rssi_min=rssi;
  if(rssi>cs->rssi_max) cs->rssi_max=rssi;
  cs->rssi_avg_x10=(cs->rssi_avg_x10*(cs->frame_count-1)+rssi*10)/cs->frame_count;

  uint32_t now=millis();

  // Window-based anomaly detection
  if(now-g_rfm.window_start_ms>=RFM_WINDOW_MS){
    g_rfm.window_start_ms=now;
    g_rfm.deauth_in_window=0;
    g_rfm.probe_in_window=0;
    g_rfm.beacon_in_window=0;
    g_rfm.auth_in_window=0;
  }

  if(type==0){
    if(stype==10||stype==12){
      g_rfm.deauth_in_window++;
      if(g_rfm.deauth_in_window>50){
        rfm_push_event(RFM_EVT_DEAUTH_FLOOD,channel,src,rssi,"Deauth flood detected");
      }
    }
    if(stype==4){
      g_rfm.probe_in_window++;
      if(g_rfm.probe_in_window>200){
        rfm_push_event(RFM_EVT_HIGH_PROBE,channel,src,rssi,"Probe flood");
      }
    }
    if(stype==8){
      g_rfm.beacon_in_window++;
      if(g_rfm.beacon_in_window>500){
        rfm_push_event(RFM_EVT_BEACON_FLOOD,channel,src,rssi,"Beacon flood");
      }
      // Hidden SSID detection: beacon from BSSID with empty SSID
    }
    if(stype==0||stype==11){
      g_rfm.auth_in_window++;
      if(g_rfm.auth_in_window>100){
        rfm_push_event(RFM_EVT_AUTH_FLOOD,channel,src,rssi,"Auth flood");
      }
    }
  }
}

// ── Build scan snapshot ───────────────────────────────────────────────────────
static void rfm_snapshot() {
  RfmScan* scan=&g_rfm.scans[g_rfm.scan_head];
  memset(scan,0,sizeof(*scan));
  scan->start_ms=millis();
  scan->band=g_rfm.band;
  uint8_t n=RFM_SCAN_CHANNELS_24;
  for(uint8_t i=0;i<n;i++){
    scan->results[i]=g_rfm.chan_stats[i];
    scan->total_frames+=g_rfm.chan_stats[i].frame_count;
  }
  scan->chan_count=n;
  scan->end_ms=millis();
  g_rfm.scan_head=(g_rfm.scan_head+1)%RFM_MAX_SCANS;
  if(g_rfm.scan_count<RFM_MAX_SCANS) g_rfm.scan_count++;
  g_rfm.total_scans_done++;
}

// ── Best non-overlapping channel recommendation ───────────────────────────────
static uint8_t rfm_best_channel() {
  // Compare 1, 6, 11 utilization
  uint32_t f1=g_rfm.chan_stats[0].frame_count;
  uint32_t f6=g_rfm.chan_stats[5].frame_count;
  uint32_t f11=g_rfm.chan_stats[10].frame_count;
  if(f1<=f6&&f1<=f11) return 1;
  if(f6<=f11) return 6;
  return 11;
}

// ── Interference heuristic ────────────────────────────────────────────────────
// Channels 1,6,11 all heavily loaded → potential microwave/BT interference
static RfmInterfType rfm_detect_interference() {
  uint32_t f1=g_rfm.chan_stats[0].frame_count;
  uint32_t f6=g_rfm.chan_stats[5].frame_count;
  uint32_t f11=g_rfm.chan_stats[10].frame_count;
  uint32_t avg=(f1+f6+f11)/3;
  if(avg>5000) return RFM_INTF_MICROWAVE;
  // Channels 2,3,4,5 loaded without APs on those channels → BT/FHSS
  uint32_t f2=g_rfm.chan_stats[1].frame_count;
  uint32_t f3=g_rfm.chan_stats[2].frame_count;
  if(f2>1000&&f3>1000) return RFM_INTF_BLUETOOTH;
  return RFM_INTF_NONE;
}

// ── Scan JSON ─────────────────────────────────────────────────────────────────
static String rfm_scan_json() {
  String j="{\"channels\":[";
  for(int i=0;i<RFM_SCAN_CHANNELS_24;i++){
    if(i) j+=",";
    RfmChanResult& c=g_rfm.chan_stats[i];
    j+="{\"ch\":"+String(c.channel);
    j+=",\"frames\":"+String(c.frame_count);
    j+=",\"mgmt\":"+String(c.mgmt_count);
    j+=",\"data\":"+String(c.data_count);
    j+=",\"rssi_min\":"+String(c.rssi_min);
    j+=",\"rssi_max\":"+String(c.rssi_max);
    j+=",\"rssi_avg\":"+String(c.rssi_avg_x10/10);
    j+=",\"hidden\":"+String(c.has_hidden?1:0);
    j+="}";
  }
  j+="],\"best_chan\":"+String(rfm_best_channel());
  j+=",\"current_chan\":"+String(g_rfm.current_chan);
  j+=",\"total_hops\":"+String(g_rfm.total_hops);
  j+=",\"hopping\":"+String(g_rfm.hopping?1:0);
  j+=",\"dwell_ms\":"+String(g_rfm.dwell_ms);
  j+="}";
  return j;
}

// ── Events JSON ───────────────────────────────────────────────────────────────
static String rfm_events_json(int max_events) {
  if(max_events<1||max_events>RFM_MAX_EVENTS) max_events=RFM_MAX_EVENTS;
  String j="{\"count\":"+String(g_rfm.ev_count)+",\"events\":[";
  int shown=0;
  int start=(g_rfm.ev_head+RFM_MAX_EVENTS-g_rfm.ev_count)%RFM_MAX_EVENTS;
  for(int i=g_rfm.ev_count-1;i>=0&&shown<max_events;i--){
    int idx=(start+i)%RFM_MAX_EVENTS;
    if(shown) j+=",";
    RfmEvent& ev=g_rfm.events[idx];
    char mac[18];
    snprintf(mac,sizeof(mac),"%02X:%02X:%02X:%02X:%02X:%02X",
      ev.mac[0],ev.mac[1],ev.mac[2],ev.mac[3],ev.mac[4],ev.mac[5]);
    j+="{\"type\":"+String((int)ev.type);
    j+=",\"name\":\""+String(rfm_event_name(ev.type))+"\"";
    j+=",\"ch\":"+String(ev.channel);
    j+=",\"mac\":\""; j+=mac; j+="\"";
    j+=",\"rssi\":"+String(ev.rssi);
    j+=",\"ts\":"+String(ev.timestamp_ms);
    j+=",\"detail\":\""; j+=ev.detail; j+="\"";
    j+="}";
    shown++;
  }
  j+="]}";
  return j;
}

// ── Monitor loop tick ─────────────────────────────────────────────────────────
static void rfm_loop() {
  if(!g_rfm.active) return;
  if(g_rfm.hopping) rfm_hop();
}

// ── Start/stop ────────────────────────────────────────────────────────────────
static void rfm_start(uint8_t band, bool hop, uint32_t dwell_ms) {
  rfm_init();
  g_rfm.active  = true;
  g_rfm.hopping = hop;
  g_rfm.band    = band;
  g_rfm.dwell_ms= dwell_ms>0?dwell_ms:RFM_DWELL_MS_DEFAULT;
  if(dwell_ms<RFM_DWELL_MS_MIN) g_rfm.dwell_ms=RFM_DWELL_MS_MIN;
  if(dwell_ms>RFM_DWELL_MS_MAX) g_rfm.dwell_ms=RFM_DWELL_MS_MAX;
  esp_wifi_set_channel(g_rfm.current_chan, WIFI_SECOND_CHAN_NONE);
}

static void rfm_stop() {
  g_rfm.active=false;
  g_rfm.hopping=false;
}

// ── HTTP routes ───────────────────────────────────────────────────────────────
static void rfm_routes() {
  extern WebServer server;

  server.on("/api/rfm/scan",HTTP_GET,[](){
    rfm_snapshot();
    server.send(200,"application/json",rfm_scan_json());
  });

  server.on("/api/rfm/events",HTTP_GET,[](){
    int n=server.hasArg("n")?server.arg("n").toInt():20;
    server.send(200,"application/json",rfm_events_json(n));
  });

  server.on("/api/rfm/start",HTTP_POST,[](){
    uint8_t band=(uint8_t)(server.hasArg("band")?server.arg("band").toInt():0);
    bool hop=!server.hasArg("hop")||server.arg("hop")!="0";
    uint32_t dwell=server.hasArg("dwell")?server.arg("dwell").toInt():RFM_DWELL_MS_DEFAULT;
    rfm_start(band,hop,dwell);
    server.send(200,"application/json","{\"ok\":1}");
  });

  server.on("/api/rfm/stop",HTTP_POST,[](){
    rfm_stop();
    server.send(200,"application/json","{\"ok\":1}");
  });

  server.on("/api/rfm/set_chan",HTTP_POST,[](){
    if(!server.hasArg("ch")){server.send(400,"application/json","{\"error\":\"ch required\"}");return;}
    uint8_t ch=(uint8_t)server.arg("ch").toInt();
    if(ch<1||ch>14){server.send(400,"application/json","{\"error\":\"invalid channel\"}");return;}
    g_rfm.current_chan=ch;
    g_rfm.hopping=false;
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    server.send(200,"application/json","{\"ok\":1}");
  });

  server.on("/api/rfm/dwell",HTTP_POST,[](){
    if(!server.hasArg("ms")){server.send(400,"application/json","{\"error\":\"ms required\"}");return;}
    uint32_t ms=(uint32_t)server.arg("ms").toInt();
    if(ms<RFM_DWELL_MS_MIN) ms=RFM_DWELL_MS_MIN;
    if(ms>RFM_DWELL_MS_MAX) ms=RFM_DWELL_MS_MAX;
    g_rfm.dwell_ms=ms;
    server.send(200,"application/json","{\"ok\":1}");
  });

  server.on("/api/rfm/status",HTTP_GET,[](){
    char buf[256];
    snprintf(buf,sizeof(buf),
      "{\"active\":%d,\"hopping\":%d,\"channel\":%d,\"band\":%d,"
      "\"dwell_ms\":%u,\"total_hops\":%u,\"events\":%u}",
      g_rfm.active?1:0, g_rfm.hopping?1:0,
      g_rfm.current_chan, g_rfm.band,
      (unsigned)g_rfm.dwell_ms, (unsigned)g_rfm.total_hops,
      (unsigned)g_rfm.ev_count);
    server.send(200,"application/json",buf);
  });

  server.on("/api/rfm/best",HTTP_GET,[](){
    char buf[64];
    uint8_t best=rfm_best_channel();
    RfmInterfType intf=rfm_detect_interference();
    snprintf(buf,sizeof(buf),"{\"best_chan\":%d,\"interference\":%d}",
      best,(int)intf);
    server.send(200,"application/json",buf);
  });
}

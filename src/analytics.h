#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

// ── Analytics Engine ──────────────────────────────────────────────────────────
//
// Time-series statistics, channel heatmaps, signal histograms, and
// per-device activity tracking for 802.11 environment monitoring.

// ── Time resolution ───────────────────────────────────────────────────────────
#define ANL_EPOCH_MS      1000UL      // 1-second epoch
#define ANL_HIST_BUCKETS  60          // 60-second rolling window
#define ANL_CHAN_MAX       14          // 2.4 GHz channels 1-14
#define ANL_CHAN5_MAX       25         // 5 GHz (selected channels)
#define ANL_MAX_BSSIDS    64          // tracked BSSIDs
#define ANL_MAX_STAS      128         // tracked STAs
#define ANL_RSSI_MIN     (-100)
#define ANL_RSSI_MAX     (-20)
#define ANL_RSSI_BUCKETS  16
#define ANL_MAX_SSIDS     32
#define ANL_RATE_BUCKETS  12          // 1,2,5.5,6,9,11,12,18,24,36,48,54 Mbps

// ── Channel activity record ──────────────────────────────────────────────────
struct AnlChanStat {
  uint8_t  channel;
  uint32_t frames_total;
  uint32_t frames_mgmt;
  uint32_t frames_data;
  uint32_t frames_ctrl;
  uint32_t bytes_total;
  int8_t   rssi_min;
  int8_t   rssi_max;
  int16_t  rssi_avg_x10;    // ×10 to avoid float
  uint16_t bssid_count;
  uint16_t sta_count;
  uint32_t last_seen_ms;
  uint16_t utilization;     // 0-1000 (0.0-100.0%)
};

// ── RSSI histogram bucket ─────────────────────────────────────────────────────
struct AnlRssiBucket {
  int8_t   rssi_low;
  int8_t   rssi_high;
  uint32_t count;
};

// ── Per-epoch sample (1-second slot) ─────────────────────────────────────────
struct AnlEpoch {
  uint32_t timestamp_s;
  uint32_t frames;
  uint32_t bytes;
  int8_t   rssi_avg;
  uint8_t  utilization;
};

// ── Rolling ring buffer ───────────────────────────────────────────────────────
struct AnlRing {
  AnlEpoch slots[ANL_HIST_BUCKETS];
  uint8_t  head;
  uint8_t  count;
};

// ── BSSID tracker entry ───────────────────────────────────────────────────────
struct AnlBssid {
  uint8_t  mac[6];
  char     ssid[33];
  uint8_t  ssid_len;
  uint8_t  channel;
  int8_t   rssi_last;
  int8_t   rssi_min;
  int8_t   rssi_max;
  int32_t  rssi_sum;
  uint32_t rssi_count;
  uint32_t frames_seen;
  uint32_t first_seen_ms;
  uint32_t last_seen_ms;
  uint8_t  enc_type;        // WpEncType
  bool     has_pmf;
  bool     is_mesh;
  bool     has_ht;
  bool     has_vht;
  AnlRing  ring;
};

// ── STA tracker entry ─────────────────────────────────────────────────────────
struct AnlSta {
  uint8_t  mac[6];
  uint8_t  assoc_bssid[6];  // zero if probing
  char     probe_ssids[4][33]; // last 4 probed SSIDs
  uint8_t  probe_count;
  int8_t   rssi_last;
  int8_t   rssi_min;
  int8_t   rssi_max;
  int32_t  rssi_sum;
  uint32_t rssi_count;
  uint32_t frames_seen;
  uint32_t first_seen_ms;
  uint32_t last_seen_ms;
  bool     is_associated;
  bool     is_random_mac;
  uint8_t  vendor_cat;      // OufCat
  AnlRing  ring;
};

// ── Channel heatmap ──────────────────────────────────────────────────────────
struct AnlHeatmap {
  AnlChanStat chans[ANL_CHAN_MAX];
  uint32_t    snapshot_ms;
  uint8_t     busiest_chan;
  uint8_t     quietest_chan;
  uint8_t     recommended_chan; // least utilized 1,6,11
};

// ── Global analytics state ────────────────────────────────────────────────────
struct AnlState {
  AnlBssid  bssids[ANL_MAX_BSSIDS];
  uint8_t   bssid_count;
  AnlSta    stas[ANL_MAX_STAS];
  uint8_t   sta_count;
  AnlHeatmap heatmap;
  AnlRssiBucket rssi_hist[ANL_RSSI_BUCKETS];
  uint32_t  total_frames;
  uint32_t  total_bytes;
  uint32_t  mgmt_frames;
  uint32_t  data_frames;
  uint32_t  ctrl_frames;
  uint32_t  deauth_frames;
  uint32_t  probe_req_frames;
  uint32_t  beacon_frames;
  uint32_t  handshake_frames;
  uint32_t  start_ms;
  uint32_t  last_epoch_ms;
};

static AnlState g_anl = {};

// ── Initialization ─────────────────────────────────────────────────────────────
static void anl_init() {
  memset(&g_anl, 0, sizeof(g_anl));
  g_anl.start_ms = millis();
  g_anl.last_epoch_ms = g_anl.start_ms;
  // Init RSSI histogram buckets
  for(int i=0;i<ANL_RSSI_BUCKETS;i++){
    g_anl.rssi_hist[i].rssi_low  = ANL_RSSI_MIN + i*5;
    g_anl.rssi_hist[i].rssi_high = ANL_RSSI_MIN + (i+1)*5 - 1;
  }
  // Init heatmap channel slots
  for(int i=0;i<ANL_CHAN_MAX;i++){
    g_anl.heatmap.chans[i].channel = i+1;
    g_anl.heatmap.chans[i].rssi_min = 0;
    g_anl.heatmap.chans[i].rssi_max = -100;
  }
}

// ── Ring buffer helpers ───────────────────────────────────────────────────────
static void anl_ring_push(AnlRing* r, const AnlEpoch* e) {
  r->slots[r->head] = *e;
  r->head = (r->head+1) % ANL_HIST_BUCKETS;
  if(r->count < ANL_HIST_BUCKETS) r->count++;
}

static uint32_t anl_ring_total_frames(const AnlRing* r) {
  uint32_t sum=0;
  for(uint8_t i=0;i<r->count;i++) sum+=r->slots[i].frames;
  return sum;
}

static float anl_ring_avg_rssi(const AnlRing* r) {
  if(!r->count) return -100.0f;
  int32_t sum=0;
  for(uint8_t i=0;i<r->count;i++) sum+=r->slots[i].rssi_avg;
  return (float)sum/r->count;
}

// ── RSSI to histogram bucket ──────────────────────────────────────────────────
static void anl_rssi_record(int8_t rssi) {
  int idx = (rssi - ANL_RSSI_MIN) / 5;
  if(idx<0) idx=0;
  if(idx>=ANL_RSSI_BUCKETS) idx=ANL_RSSI_BUCKETS-1;
  g_anl.rssi_hist[idx].count++;
}

// ── BSSID upsert ──────────────────────────────────────────────────────────────
static AnlBssid* anl_bssid_upsert(const uint8_t mac[6]) {
  for(uint8_t i=0;i<g_anl.bssid_count;i++){
    if(memcmp(g_anl.bssids[i].mac,mac,6)==0) return &g_anl.bssids[i];
  }
  if(g_anl.bssid_count>=ANL_MAX_BSSIDS) {
    // Evict oldest
    uint32_t oldest=g_anl.bssids[0].last_seen_ms;
    uint8_t  oi=0;
    for(uint8_t i=1;i<g_anl.bssid_count;i++){
      if(g_anl.bssids[i].last_seen_ms<oldest){oldest=g_anl.bssids[i].last_seen_ms;oi=i;}
    }
    memset(&g_anl.bssids[oi],0,sizeof(AnlBssid));
    memcpy(g_anl.bssids[oi].mac,mac,6);
    g_anl.bssids[oi].first_seen_ms=millis();
    g_anl.bssids[oi].rssi_min=0;
    g_anl.bssids[oi].rssi_max=-100;
    return &g_anl.bssids[oi];
  }
  AnlBssid* b=&g_anl.bssids[g_anl.bssid_count++];
  memset(b,0,sizeof(*b));
  memcpy(b->mac,mac,6);
  b->first_seen_ms=millis();
  b->rssi_min=0;
  b->rssi_max=-100;
  return b;
}

// ── STA upsert ────────────────────────────────────────────────────────────────
static AnlSta* anl_sta_upsert(const uint8_t mac[6]) {
  for(uint8_t i=0;i<g_anl.sta_count;i++){
    if(memcmp(g_anl.stas[i].mac,mac,6)==0) return &g_anl.stas[i];
  }
  if(g_anl.sta_count>=ANL_MAX_STAS) {
    uint32_t oldest=g_anl.stas[0].last_seen_ms;
    uint8_t  oi=0;
    for(uint8_t i=1;i<g_anl.sta_count;i++){
      if(g_anl.stas[i].last_seen_ms<oldest){oldest=g_anl.stas[i].last_seen_ms;oi=i;}
    }
    memset(&g_anl.stas[oi],0,sizeof(AnlSta));
    memcpy(g_anl.stas[oi].mac,mac,6);
    g_anl.stas[oi].first_seen_ms=millis();
    g_anl.stas[oi].rssi_min=0;
    g_anl.stas[oi].rssi_max=-100;
    return &g_anl.stas[oi];
  }
  AnlSta* s=&g_anl.stas[g_anl.sta_count++];
  memset(s,0,sizeof(*s));
  memcpy(s->mac,mac,6);
  s->first_seen_ms=millis();
  s->rssi_min=0;
  s->rssi_max=-100;
  return s;
}

// ── Frame ingestion ───────────────────────────────────────────────────────────
static void anl_record_frame(const uint8_t* src, const uint8_t* dst,
                               const uint8_t* bssid,
                               uint8_t type, uint8_t stype,
                               uint8_t channel, int8_t rssi,
                               uint16_t len) {
  g_anl.total_frames++;
  g_anl.total_bytes+=len;
  anl_rssi_record(rssi);

  if(type==0) g_anl.mgmt_frames++;
  else if(type==1) g_anl.ctrl_frames++;
  else if(type==2) g_anl.data_frames++;

  // Management subtypes
  if(type==0){
    if(stype==8) g_anl.beacon_frames++;
    if(stype==4) g_anl.probe_req_frames++;
    if(stype==10||stype==12) g_anl.deauth_frames++;
  }

  // Channel heatmap update
  if(channel>=1&&channel<=ANL_CHAN_MAX){
    AnlChanStat* cs=&g_anl.heatmap.chans[channel-1];
    cs->frames_total++;
    cs->bytes_total+=len;
    if(type==0) cs->frames_mgmt++;
    else if(type==1) cs->frames_ctrl++;
    else if(type==2) cs->frames_data++;
    if(cs->rssi_min==0||rssi<cs->rssi_min) cs->rssi_min=rssi;
    if(rssi>cs->rssi_max) cs->rssi_max=rssi;
    cs->rssi_avg_x10 = (cs->rssi_avg_x10*(cs->frames_total-1)+rssi*10)/cs->frames_total;
    cs->last_seen_ms=millis();
  }

  // BSSID tracking for beacons
  if(type==0&&stype==8&&bssid){
    AnlBssid* b=anl_bssid_upsert(bssid);
    b->frames_seen++;
    b->rssi_last=rssi;
    if(b->rssi_min==0||rssi<b->rssi_min) b->rssi_min=rssi;
    if(rssi>b->rssi_max) b->rssi_max=rssi;
    b->rssi_sum+=rssi; b->rssi_count++;
    b->channel=channel;
    b->last_seen_ms=millis();
  }

  // STA tracking for probe requests + data
  if(src && !( src[0]==0xFF&&src[1]==0xFF&&src[2]==0xFF&&
               src[3]==0xFF&&src[4]==0xFF&&src[5]==0xFF )){
    bool is_bcast = (src[0]&1);
    if(!is_bcast){
      AnlSta* s=anl_sta_upsert(src);
      s->frames_seen++;
      s->rssi_last=rssi;
      if(s->rssi_min==0||rssi<s->rssi_min) s->rssi_min=rssi;
      if(rssi>s->rssi_max) s->rssi_max=rssi;
      s->rssi_sum+=rssi; s->rssi_count++;
      s->last_seen_ms=millis();
      s->is_random_mac=(src[0]&0x02)!=0;
      if(type==2&&bssid){
        memcpy(s->assoc_bssid,bssid,6);
        s->is_associated=true;
      }
    }
  }
}

// ── Epoch tick (call once per second) ────────────────────────────────────────
static void anl_tick() {
  uint32_t now=millis();
  if(now-g_anl.last_epoch_ms < ANL_EPOCH_MS) return;
  g_anl.last_epoch_ms=now;
  uint32_t sec=now/1000;

  // Update channel utilization (frames per second → 0-1000)
  for(int i=0;i<ANL_CHAN_MAX;i++){
    AnlChanStat* cs=&g_anl.heatmap.chans[i];
    // Simple: assume 1000 frames/s = 100% utilization
    uint32_t fps=cs->frames_total; // delta would need prev snapshot
    cs->utilization = fps>1000?1000:(uint16_t)fps;
  }

  // Find busiest/quietest 2.4 GHz channel
  uint32_t max_f=0,min_f=UINT32_MAX;
  uint8_t  busy_ch=1,quiet_ch=1;
  for(int i=0;i<ANL_CHAN_MAX;i++){
    if(g_anl.heatmap.chans[i].frames_total>max_f){
      max_f=g_anl.heatmap.chans[i].frames_total; busy_ch=i+1;
    }
    if(g_anl.heatmap.chans[i].frames_total<min_f){
      min_f=g_anl.heatmap.chans[i].frames_total; quiet_ch=i+1;
    }
  }
  g_anl.heatmap.busiest_chan=busy_ch;
  g_anl.heatmap.quietest_chan=quiet_ch;

  // Recommended non-overlapping channel (1, 6, or 11)
  uint32_t f1=g_anl.heatmap.chans[0].frames_total;
  uint32_t f6=g_anl.heatmap.chans[5].frames_total;
  uint32_t f11=g_anl.heatmap.chans[10].frames_total;
  g_anl.heatmap.recommended_chan = (f1<=f6&&f1<=f11)?1:(f6<=f11)?6:11;
  g_anl.heatmap.snapshot_ms=now;
}

// ── RSSI quality label ────────────────────────────────────────────────────────
static const char* anl_rssi_label(int8_t rssi) {
  if(rssi>=-50) return "Excellent";
  if(rssi>=-60) return "Good";
  if(rssi>=-70) return "Fair";
  if(rssi>=-80) return "Poor";
  return "Very Poor";
}

// ── Channel utilization label ─────────────────────────────────────────────────
static const char* anl_util_label(uint16_t util) {
  if(util<100) return "Idle";
  if(util<300) return "Low";
  if(util<600) return "Medium";
  if(util<800) return "High";
  return "Congested";
}

// ── Uptime as HH:MM:SS string ─────────────────────────────────────────────────
static String anl_uptime_str() {
  uint32_t secs=(millis()-g_anl.start_ms)/1000;
  uint32_t h=secs/3600; secs%=3600;
  uint32_t m=secs/60;   secs%=60;
  char buf[16];
  snprintf(buf,sizeof(buf),"%02u:%02u:%02u",h,m,(unsigned)secs);
  return String(buf);
}

// ── Summary JSON ──────────────────────────────────────────────────────────────
static String anl_summary_json() {
  uint32_t uptime=(millis()-g_anl.start_ms)/1000;
  String j="{";
  j+="\"uptime\":"+String(uptime);
  j+=",\"uptime_str\":\""+anl_uptime_str()+"\"";
  j+=",\"total_frames\":"+String(g_anl.total_frames);
  j+=",\"total_bytes\":"+String(g_anl.total_bytes);
  j+=",\"mgmt\":"+String(g_anl.mgmt_frames);
  j+=",\"data\":"+String(g_anl.data_frames);
  j+=",\"ctrl\":"+String(g_anl.ctrl_frames);
  j+=",\"beacons\":"+String(g_anl.beacon_frames);
  j+=",\"probe_req\":"+String(g_anl.probe_req_frames);
  j+=",\"deauths\":"+String(g_anl.deauth_frames);
  j+=",\"bssids\":"+String(g_anl.bssid_count);
  j+=",\"stas\":"+String(g_anl.sta_count);
  j+=",\"heap\":"+String(ESP.getFreeHeap());
  j+=",\"busiest_chan\":"+String(g_anl.heatmap.busiest_chan);
  j+=",\"recommended_chan\":"+String(g_anl.heatmap.recommended_chan);
  j+="}";
  return j;
}

// ── Heatmap JSON ──────────────────────────────────────────────────────────────
static String anl_heatmap_json() {
  String j="{\"channels\":[";
  for(int i=0;i<ANL_CHAN_MAX;i++){
    if(i) j+=",";
    AnlChanStat& c=g_anl.heatmap.chans[i];
    j+="{\"ch\":"+String(c.channel);
    j+=",\"frames\":"+String(c.frames_total);
    j+=",\"bytes\":"+String(c.bytes_total);
    j+=",\"rssi_min\":"+String(c.rssi_min);
    j+=",\"rssi_max\":"+String(c.rssi_max);
    j+=",\"rssi_avg\":"+String(c.rssi_avg_x10/10);
    j+=",\"util\":"+String(c.utilization);
    j+=",\"util_label\":\""+String(anl_util_label(c.utilization))+"\"";
    j+=",\"bssids\":"+String(c.bssid_count);
    j+=",\"stas\":"+String(c.sta_count);
    j+="}";
  }
  j+="],\"busiest\":"+String(g_anl.heatmap.busiest_chan);
  j+=",\"quietest\":"+String(g_anl.heatmap.quietest_chan);
  j+=",\"recommended\":"+String(g_anl.heatmap.recommended_chan);
  j+="}";
  return j;
}

// ── RSSI histogram JSON ───────────────────────────────────────────────────────
static String anl_rssi_hist_json() {
  String j="{\"buckets\":[";
  for(int i=0;i<ANL_RSSI_BUCKETS;i++){
    if(i) j+=",";
    j+="{\"low\":"+String(g_anl.rssi_hist[i].rssi_low);
    j+=",\"high\":"+String(g_anl.rssi_hist[i].rssi_high);
    j+=",\"count\":"+String(g_anl.rssi_hist[i].count)+"}";
  }
  j+="]}";
  return j;
}

// ── BSSID list JSON ───────────────────────────────────────────────────────────
static String anl_bssid_json(int page, int per) {
  if(per<1||per>50) per=20;
  int start=page*per, end=start+per;
  if(end>(int)g_anl.bssid_count) end=g_anl.bssid_count;
  String j="{\"total\":"+String(g_anl.bssid_count);
  j+=",\"page\":"+String(page)+",\"per\":"+String(per);
  j+=",\"bssids\":[";
  for(int i=start;i<end;i++){
    if(i>start) j+=",";
    AnlBssid& b=g_anl.bssids[i];
    char mac[18];
    snprintf(mac,sizeof(mac),"%02X:%02X:%02X:%02X:%02X:%02X",
      b.mac[0],b.mac[1],b.mac[2],b.mac[3],b.mac[4],b.mac[5]);
    j+="{\"mac\":\""; j+=mac; j+="\"";
    j+=",\"ssid\":\""; j+=b.ssid; j+="\"";
    j+=",\"ch\":"; j+=b.channel;
    j+=",\"rssi\":"; j+=b.rssi_last;
    j+=",\"rssi_min\":"; j+=b.rssi_min;
    j+=",\"rssi_max\":"; j+=b.rssi_max;
    j+=",\"rssi_avg\":"; j+=(b.rssi_count?b.rssi_sum/(int32_t)b.rssi_count:0);
    j+=",\"frames\":"; j+=b.frames_seen;
    j+=",\"quality\":\""; j+=anl_rssi_label(b.rssi_last); j+="\"";
    j+=",\"pmf\":"; j+=b.has_pmf?1:0;
    j+=",\"ht\":"; j+=b.has_ht?1:0;
    j+=",\"vht\":"; j+=b.has_vht?1:0;
    uint32_t age=(millis()-b.last_seen_ms)/1000;
    j+=",\"age_s\":"; j+=age;
    j+="}";
  }
  j+="]}";
  return j;
}

// ── STA list JSON ─────────────────────────────────────────────────────────────
static String anl_sta_json(int page, int per) {
  if(per<1||per>50) per=20;
  int start=page*per, end=start+per;
  if(end>(int)g_anl.sta_count) end=g_anl.sta_count;
  String j="{\"total\":"+String(g_anl.sta_count);
  j+=",\"page\":"+String(page)+",\"per\":"+String(per);
  j+=",\"stas\":[";
  for(int i=start;i<end;i++){
    if(i>start) j+=",";
    AnlSta& s=g_anl.stas[i];
    char mac[18];
    snprintf(mac,sizeof(mac),"%02X:%02X:%02X:%02X:%02X:%02X",
      s.mac[0],s.mac[1],s.mac[2],s.mac[3],s.mac[4],s.mac[5]);
    j+="{\"mac\":\""; j+=mac; j+="\"";
    j+=",\"rssi\":"; j+=s.rssi_last;
    j+=",\"frames\":"; j+=s.frames_seen;
    j+=",\"assoc\":"; j+=s.is_associated?1:0;
    j+=",\"random\":"; j+=s.is_random_mac?1:0;
    j+=",\"probes\":"; j+=s.probe_count;
    uint32_t age=(millis()-s.last_seen_ms)/1000;
    j+=",\"age_s\":"; j+=age;
    if(s.probe_count>0){
      j+=",\"probe_ssids\":[";
      for(int p=0;p<s.probe_count&&p<4;p++){
        if(p) j+=",";
        j+="\""; j+=s.probe_ssids[p]; j+="\"";
      }
      j+="]";
    }
    j+="}";
  }
  j+="]}";
  return j;
}

// ── Top-N active channels ─────────────────────────────────────────────────────
static void anl_top_channels(uint8_t* out, uint8_t n) {
  // Simple insertion sort
  uint8_t sorted[ANL_CHAN_MAX];
  for(int i=0;i<ANL_CHAN_MAX;i++) sorted[i]=i;
  for(int i=0;i<ANL_CHAN_MAX-1;i++){
    for(int j=0;j<ANL_CHAN_MAX-i-1;j++){
      if(g_anl.heatmap.chans[sorted[j]].frames_total <
         g_anl.heatmap.chans[sorted[j+1]].frames_total){
        uint8_t t=sorted[j]; sorted[j]=sorted[j+1]; sorted[j+1]=t;
      }
    }
  }
  for(uint8_t i=0;i<n&&i<ANL_CHAN_MAX;i++) out[i]=sorted[i]+1;
}

// ── Frame rate (fps) over last window ────────────────────────────────────────
static float anl_fps() {
  uint32_t uptime=(millis()-g_anl.start_ms);
  if(!uptime) return 0.0f;
  return (float)g_anl.total_frames * 1000.0f / uptime;
}

// ── Throughput estimate (bytes/s) ─────────────────────────────────────────────
static float anl_bps() {
  uint32_t uptime=(millis()-g_anl.start_ms);
  if(!uptime) return 0.0f;
  return (float)g_anl.total_bytes * 1000.0f / uptime;
}

// ── Noise floor estimate from histogram ──────────────────────────────────────
static int8_t anl_noise_floor() {
  // Find the RSSI bucket with most frames as the "noise floor"
  uint32_t max_c=0;
  int8_t   noise=ANL_RSSI_MIN;
  for(int i=0;i<ANL_RSSI_BUCKETS;i++){
    if(g_anl.rssi_hist[i].count>max_c){
      max_c=g_anl.rssi_hist[i].count;
      noise=g_anl.rssi_hist[i].rssi_low+2;
    }
  }
  return noise;
}

// ── STA probe history add ─────────────────────────────────────────────────────
static void anl_sta_add_probe(AnlSta* s, const char* ssid) {
  if(!ssid||!ssid[0]) return;
  for(int i=0;i<s->probe_count&&i<4;i++){
    if(strcmp(s->probe_ssids[i],ssid)==0) return; // already seen
  }
  int slot=s->probe_count<4 ? s->probe_count : 3;
  if(s->probe_count>=4){
    // Shift
    memmove(s->probe_ssids[0],s->probe_ssids[1],33*3);
  }
  strncpy(s->probe_ssids[slot],ssid,32);
  s->probe_ssids[slot][32]=0;
  if(s->probe_count<4) s->probe_count++;
}

// ── Reset stats ───────────────────────────────────────────────────────────────
static void anl_reset() {
  anl_init();
}

// ── HTTP routes ───────────────────────────────────────────────────────────────
static void anl_routes() {
  extern WebServer server;

  server.on("/api/anl/summary",HTTP_GET,[](){
    anl_tick();
    server.send(200,"application/json",anl_summary_json());
  });

  server.on("/api/anl/heatmap",HTTP_GET,[](){
    anl_tick();
    server.send(200,"application/json",anl_heatmap_json());
  });

  server.on("/api/anl/rssi",HTTP_GET,[](){
    server.send(200,"application/json",anl_rssi_hist_json());
  });

  server.on("/api/anl/bssids",HTTP_GET,[](){
    int page=server.hasArg("page")?server.arg("page").toInt():0;
    int per =server.hasArg("per") ?server.arg("per").toInt() :20;
    server.send(200,"application/json",anl_bssid_json(page,per));
  });

  server.on("/api/anl/stas",HTTP_GET,[](){
    int page=server.hasArg("page")?server.arg("page").toInt():0;
    int per =server.hasArg("per") ?server.arg("per").toInt() :20;
    server.send(200,"application/json",anl_sta_json(page,per));
  });

  server.on("/api/anl/reset",HTTP_POST,[](){
    anl_reset();
    server.send(200,"application/json","{\"ok\":1}");
  });

  server.on("/api/anl/fps",HTTP_GET,[](){
    char buf[64];
    snprintf(buf,sizeof(buf),"{\"fps\":%.1f,\"bps\":%.0f,\"noise\":%d}",
      anl_fps(), anl_bps(), (int)anl_noise_floor());
    server.send(200,"application/json",buf);
  });

  server.on("/api/anl/top_channels",HTTP_GET,[](){
    uint8_t top[5];
    anl_top_channels(top,5);
    String j="{\"top\":[";
    for(int i=0;i<5;i++){
      if(i) j+=",";
      j+=top[i];
    }
    j+="]}";
    server.send(200,"application/json",j);
  });
}

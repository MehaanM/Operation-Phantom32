#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include <string.h>
#include <stdint.h>

// ── PCAP Writer ───────────────────────────────────────────────────────────────
//
// Writes captured 802.11 frames to PCAP files on LittleFS with
// full radiotap headers for use with Wireshark/aircrack-ng.

// ── PCAP constants ────────────────────────────────────────────────────────────
#define PCAP_MAGIC        0xA1B2C3D4
#define PCAP_VERSION_MAJ  2
#define PCAP_VERSION_MIN  4
#define PCAP_SNAPLEN      65535
#define PCAP_DLT_80211    105   // IEEE 802.11
#define PCAP_DLT_RADIOTAP 127  // IEEE 802.11 + radiotap header

#define PCW_MAX_FILES     8
#define PCW_MAX_FNAME     32
#define PCW_RING_SIZE     32   // frame ring buffer
#define PCW_FRAME_MAX     512  // max frame size in ring

// ── PCAP global file header ───────────────────────────────────────────────────
struct __attribute__((packed)) PcwFileHeader {
  uint32_t magic;
  uint16_t version_major;
  uint16_t version_minor;
  int32_t  thiszone;
  uint32_t sigfigs;
  uint32_t snaplen;
  uint32_t network;  // DLT
};

// ── PCAP per-packet header ────────────────────────────────────────────────────
struct __attribute__((packed)) PcwPktHeader {
  uint32_t ts_sec;
  uint32_t ts_usec;
  uint32_t incl_len;
  uint32_t orig_len;
};

// ── Radiotap header (minimal) ─────────────────────────────────────────────────
struct __attribute__((packed)) PcwRadiotap {
  uint8_t  it_version;
  uint8_t  it_pad;
  uint16_t it_len;
  uint32_t it_present;
  // Fields present: TSFT, Flags, Rate, Channel, Antenna Signal, Antenna
  uint64_t tsft;          // present bit 0
  uint8_t  flags;         // present bit 1
  uint8_t  rate;          // present bit 2 (500kbps units)
  uint16_t channel_freq;  // present bit 3
  uint16_t channel_flags;
  int8_t   ant_signal;    // present bit 5 (dBm)
  uint8_t  antenna;       // present bit 11
};

// ── Radiotap channel flags ────────────────────────────────────────────────────
#define RT_CHAN_FLAG_TURBO    0x0010
#define RT_CHAN_FLAG_CCK      0x0020
#define RT_CHAN_FLAG_OFDM     0x0040
#define RT_CHAN_FLAG_2GHZ     0x0080
#define RT_CHAN_FLAG_5GHZ     0x0100
#define RT_CHAN_FLAG_PASSIVE  0x0200
#define RT_CHAN_FLAG_DYN_CCK  0x0400
#define RT_CHAN_FLAG_HT40_PLUS  0x0800

// ── Radiotap it_present bitmask ───────────────────────────────────────────────
#define RT_PRESENT_TSFT   (1<<0)
#define RT_PRESENT_FLAGS  (1<<1)
#define RT_PRESENT_RATE   (1<<2)
#define RT_PRESENT_CHAN   (1<<3)
#define RT_PRESENT_SIGNAL (1<<5)
#define RT_PRESENT_ANTENNA (1<<11)

// ── Frame ring buffer entry ───────────────────────────────────────────────────
struct PcwRingEntry {
  uint8_t  frame[PCW_FRAME_MAX];
  uint16_t frame_len;
  int8_t   rssi;
  uint8_t  channel;
  uint8_t  rate;    // 500kbps units (e.g. 108 = 54 Mbps)
  uint32_t ts_sec;
  uint32_t ts_usec;
  bool     valid;
};

// ── PCAP file descriptor ──────────────────────────────────────────────────────
struct PcwFile {
  char     path[PCW_MAX_FNAME];
  uint32_t frame_count;
  uint32_t byte_count;
  uint32_t open_ts;
  bool     is_open;
};

// ── Writer state ──────────────────────────────────────────────────────────────
struct PcwState {
  PcwFile   files[PCW_MAX_FILES];
  uint8_t   file_count;
  char      active_path[PCW_MAX_FNAME];
  bool      active;
  bool      ring_enabled;
  PcwRingEntry ring[PCW_RING_SIZE];
  uint8_t   ring_head;
  uint8_t   ring_count;
  uint32_t  total_frames;
  uint32_t  dropped_frames;
};

static PcwState g_pcw = {};

// ── Channel to frequency ──────────────────────────────────────────────────────
static uint16_t pcw_chan_freq(uint8_t ch) {
  if(ch>=1&&ch<=13) return 2412+5*(ch-1);
  if(ch==14) return 2484;
  if(ch>=36&&ch<=165) return 5180+5*(ch-36);
  return 2412;
}

// ── Build radiotap header ─────────────────────────────────────────────────────
static uint16_t pcw_build_radiotap(PcwRadiotap* rt, uint8_t channel,
                                    int8_t rssi, uint8_t rate_500k) {
  memset(rt,0,sizeof(*rt));
  rt->it_version=0;
  rt->it_len=sizeof(PcwRadiotap);
  rt->it_present=RT_PRESENT_TSFT|RT_PRESENT_FLAGS|RT_PRESENT_RATE|
                  RT_PRESENT_CHAN|RT_PRESENT_SIGNAL|RT_PRESENT_ANTENNA;
  rt->tsft=millis()*1000ULL;
  rt->flags=0x00;
  rt->rate=rate_500k?rate_500k:2; // default 1 Mbps
  rt->channel_freq=pcw_chan_freq(channel);
  rt->channel_flags=(channel<=14)?RT_CHAN_FLAG_2GHZ|RT_CHAN_FLAG_CCK:RT_CHAN_FLAG_5GHZ|RT_CHAN_FLAG_OFDM;
  rt->ant_signal=rssi;
  rt->antenna=0;
  return sizeof(PcwRadiotap);
}

// ── Write PCAP file header ────────────────────────────────────────────────────
static bool pcw_write_file_header(File& f, bool radiotap) {
  PcwFileHeader hdr;
  hdr.magic=PCAP_MAGIC;
  hdr.version_major=PCAP_VERSION_MAJ;
  hdr.version_minor=PCAP_VERSION_MIN;
  hdr.thiszone=0;
  hdr.sigfigs=0;
  hdr.snaplen=PCAP_SNAPLEN;
  hdr.network=radiotap?PCAP_DLT_RADIOTAP:PCAP_DLT_80211;
  return f.write((uint8_t*)&hdr,sizeof(hdr))==sizeof(hdr);
}

// ── Open new PCAP file ────────────────────────────────────────────────────────
static bool pcw_open(const char* path, bool radiotap=true) {
  File f=LittleFS.open(path,"w");
  if(!f) return false;
  bool ok=pcw_write_file_header(f,radiotap);
  f.close();
  if(!ok) return false;
  if(g_pcw.file_count<PCW_MAX_FILES){
    PcwFile& pf=g_pcw.files[g_pcw.file_count++];
    strncpy(pf.path,path,PCW_MAX_FNAME-1);
    pf.frame_count=0; pf.byte_count=sizeof(PcwFileHeader);
    pf.open_ts=millis()/1000; pf.is_open=true;
  }
  strncpy(g_pcw.active_path,path,PCW_MAX_FNAME-1);
  g_pcw.active=true;
  return true;
}

// ── Append one frame to active PCAP ──────────────────────────────────────────
static bool pcw_write_frame(const uint8_t* frame, uint16_t frame_len,
                              uint8_t channel, int8_t rssi, uint8_t rate) {
  if(!g_pcw.active||!frame||!frame_len) return false;
  File f=LittleFS.open(g_pcw.active_path,"a");
  if(!f){ g_pcw.dropped_frames++; return false; }

  PcwRadiotap rt;
  uint16_t rt_len=pcw_build_radiotap(&rt,channel,rssi,rate);

  uint32_t incl_len=rt_len+frame_len;
  if(incl_len>PCAP_SNAPLEN) incl_len=PCAP_SNAPLEN;

  PcwPktHeader ph;
  uint32_t now_ms=millis();
  ph.ts_sec=now_ms/1000;
  ph.ts_usec=(now_ms%1000)*1000;
  ph.incl_len=incl_len;
  ph.orig_len=incl_len;

  bool ok=true;
  ok&=(f.write((uint8_t*)&ph,sizeof(ph))==sizeof(ph));
  ok&=(f.write((uint8_t*)&rt,rt_len)==rt_len);
  uint32_t to_write=incl_len-rt_len;
  ok&=(f.write(frame,to_write)==to_write);
  f.close();

  if(ok){
    g_pcw.total_frames++;
    // Update file stats
    for(uint8_t i=0;i<g_pcw.file_count;i++){
      if(strcmp(g_pcw.files[i].path,g_pcw.active_path)==0){
        g_pcw.files[i].frame_count++;
        g_pcw.files[i].byte_count+=sizeof(ph)+incl_len;
        break;
      }
    }
  } else {
    g_pcw.dropped_frames++;
  }
  return ok;
}

// ── Ring buffer capture ───────────────────────────────────────────────────────
static void pcw_ring_push(const uint8_t* frame, uint16_t frame_len,
                            uint8_t channel, int8_t rssi, uint8_t rate) {
  if(!g_pcw.ring_enabled) return;
  PcwRingEntry* e=&g_pcw.ring[g_pcw.ring_head];
  uint16_t copy=frame_len<PCW_FRAME_MAX?frame_len:PCW_FRAME_MAX;
  memcpy(e->frame,frame,copy);
  e->frame_len=copy;
  e->rssi=rssi;
  e->channel=channel;
  e->rate=rate;
  e->ts_sec=millis()/1000;
  e->ts_usec=(millis()%1000)*1000;
  e->valid=true;
  g_pcw.ring_head=(g_pcw.ring_head+1)%PCW_RING_SIZE;
  if(g_pcw.ring_count<PCW_RING_SIZE) g_pcw.ring_count++;
}

// ── Flush ring buffer to PCAP ─────────────────────────────────────────────────
static uint32_t pcw_flush_ring() {
  uint32_t written=0;
  uint8_t start=(g_pcw.ring_head+PCW_RING_SIZE-g_pcw.ring_count)%PCW_RING_SIZE;
  for(uint8_t i=0;i<g_pcw.ring_count;i++){
    uint8_t idx=(start+i)%PCW_RING_SIZE;
    PcwRingEntry& e=g_pcw.ring[idx];
    if(e.valid && pcw_write_frame(e.frame,e.frame_len,e.channel,e.rssi,e.rate))
      written++;
  }
  return written;
}

// ── List PCAP files ───────────────────────────────────────────────────────────
static String pcw_list_json() {
  String j="{\"files\":[";
  for(uint8_t i=0;i<g_pcw.file_count;i++){
    if(i) j+=",";
    PcwFile& pf=g_pcw.files[i];
    j+="{\"path\":\""; j+=pf.path; j+="\"";
    j+=",\"frames\":"; j+=pf.frame_count;
    j+=",\"bytes\":"; j+=pf.byte_count;
    j+=",\"open_ts\":"; j+=pf.open_ts;
    j+="}";
  }
  j+="],\"active\":\""; j+=g_pcw.active?g_pcw.active_path:""; j+="\"";
  j+=",\"total_frames\":"; j+=g_pcw.total_frames;
  j+=",\"dropped\":"; j+=g_pcw.dropped_frames;
  j+="}";
  return j;
}

// ── HTTP routes ───────────────────────────────────────────────────────────────
static void pcw_routes() {
  extern WebServer server;

  server.on("/api/pcw/list",HTTP_GET,[](){
    server.send(200,"application/json",pcw_list_json());
  });

  server.on("/api/pcw/open",HTTP_POST,[](){
    String path=server.hasArg("path")?server.arg("path"):"/cap.pcap";
    bool ok=pcw_open(path.c_str(),true);
    server.send(200,"application/json",ok?"{\"ok\":1}":"{\"error\":\"open failed\"}");
  });

  server.on("/api/pcw/stop",HTTP_POST,[](){
    g_pcw.active=false;
    server.send(200,"application/json","{\"ok\":1}");
  });

  server.on("/api/pcw/ring_flush",HTTP_POST,[](){
    uint32_t n=pcw_flush_ring();
    char buf[32]; snprintf(buf,sizeof(buf),"{\"written\":%u}",(unsigned)n);
    server.send(200,"application/json",buf);
  });

  server.on("/api/pcw/download",HTTP_GET,[](){
    String path=server.hasArg("path")?server.arg("path"):"/cap.pcap";
    File f=LittleFS.open(path,"r");
    if(!f){ server.send(404,"text/plain","not found"); return; }
    server.streamFile(f,"application/octet-stream");
    f.close();
  });

  server.on("/api/pcw/delete",HTTP_POST,[](){
    String path=server.hasArg("path")?server.arg("path"):"";
    if(path.length()==0||!path.startsWith("/")){
      server.send(400,"application/json","{\"error\":\"invalid path\"}");return;
    }
    bool ok=LittleFS.remove(path);
    server.send(200,"application/json",ok?"{\"ok\":1}":"{\"error\":\"delete failed\"}");
  });
}

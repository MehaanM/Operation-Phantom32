#pragma once
// pcap_live.h — Live PCAP capture to LittleFS with ring-file rotation.
// Writes standard PCAP format with radiotap headers. Prefix: pcl_

#include <Arduino.h>
#include <WebServer.h>
#include <LittleFS.h>

extern WebServer server;

#define PCL_MAX_FILES 5            // ring of capture files
#define PCL_MAX_SIZE (256 * 1024)  // max bytes per file before rotation
#define PCL_DIR "/pcap"
#define PCL_MAX_PKT 512  // max bytes per captured frame

// Radiotap header (minimal: flags, rate, channel, RSSI, noise)
#pragma pack(push, 1)
struct PclRadiotap {
  uint8_t it_version;  // 0
  uint8_t it_pad;
  uint16_t it_len;      // 36
  uint32_t it_present;  // 0x0000182F: TSFT+flags+rate+channel+ant_signal+ant_noise+antenna
  uint64_t tsft;
  uint8_t flags;
  uint8_t rate;
  uint16_t channel_freq;
  uint16_t channel_flags;
  int8_t ant_signal;
  int8_t ant_noise;
  uint8_t antenna;
  uint8_t _pad;  // alignment
};
#pragma pack(pop)

#pragma pack(push, 1)
struct PclPktHdr {
  uint32_t ts_sec;
  uint32_t ts_usec;
  uint32_t incl_len;
  uint32_t orig_len;
};
#pragma pack(pop)

static struct {
  bool capturing;
  File cur_file;
  char cur_path[32];
  uint8_t file_idx;  // 0-4

  uint32_t pkt_captured;
  uint32_t pkt_dropped;  // when file full or write error
  uint32_t bytes_written;
  uint32_t file_rotations;

  uint32_t start_ts;
} g_pcl = {};

static const uint32_t PCL_MAGIC = 0xA1B2C3D4;
static const uint16_t PCL_VER_MA = 2, PCL_VER_MI = 4;
static const uint32_t PCL_SNAPLEN = 65535;
static const uint32_t PCL_DLT_RADIOTAP = 127;

static void pcl_write_global_header(File &f) {
  // pcap global header: magic(4), ver_major(2), ver_minor(2),
  //                     thiszone(4), sigfigs(4), snaplen(4), network(4)
  uint32_t buf[6] = {PCL_MAGIC, 0x00040002, 0, 0, PCL_SNAPLEN, PCL_DLT_RADIOTAP};
  // pack ver fields: treat as byte array
  uint8_t hdr[24];
  hdr[0] = 0xD4;
  hdr[1] = 0xC3;
  hdr[2] = 0xB2;
  hdr[3] = 0xA1;  // magic
  hdr[4] = 0x02;
  hdr[5] = 0x00;  // major
  hdr[6] = 0x04;
  hdr[7] = 0x00;                              // minor
  hdr[8] = hdr[9] = hdr[10] = hdr[11] = 0;    // thiszone
  hdr[12] = hdr[13] = hdr[14] = hdr[15] = 0;  // sigfigs
  hdr[16] = 0xFF;
  hdr[17] = 0xFF;
  hdr[18] = 0;
  hdr[19] = 0;  // snaplen=65535
  hdr[20] = PCL_DLT_RADIOTAP & 0xFF;
  hdr[21] = (PCL_DLT_RADIOTAP >> 8) & 0xFF;
  hdr[22] = 0;
  hdr[23] = 0;
  f.write(hdr, 24);
}

static bool pcl_open_file() {
  char path[32];
  snprintf(path, sizeof(path), "%s/cap%02d.pcap", PCL_DIR, g_pcl.file_idx);
  strncpy(g_pcl.cur_path, path, 31);

  if (LittleFS.exists(path))
    LittleFS.remove(path);
  g_pcl.cur_file = LittleFS.open(path, "w");
  if (!g_pcl.cur_file)
    return false;

  pcl_write_global_header(g_pcl.cur_file);
  g_pcl.bytes_written = 24;
  return true;
}

// ─── Public API ──────────────────────────────────────────────────────────────
bool pcl_start() {
  if (!LittleFS.begin(true))
    return false;
  if (!LittleFS.exists(PCL_DIR))
    LittleFS.mkdir(PCL_DIR);
  if (g_pcl.capturing) {
    if (g_pcl.cur_file)
      g_pcl.cur_file.close();
  }
  g_pcl.file_idx = 0;
  g_pcl.pkt_captured = 0;
  g_pcl.pkt_dropped = 0;
  g_pcl.file_rotations = 0;
  g_pcl.start_ts = millis() / 1000;
  if (!pcl_open_file())
    return false;
  g_pcl.capturing = true;
  return true;
}

void pcl_stop() {
  if (!g_pcl.capturing)
    return;
  if (g_pcl.cur_file) {
    g_pcl.cur_file.flush();
    g_pcl.cur_file.close();
  }
  g_pcl.capturing = false;
}

void pcl_write_pkt(const uint8_t *data, uint16_t len, int8_t rssi, uint8_t noise, uint8_t channel,
                   uint8_t rate_500k) {
  if (!g_pcl.capturing || !g_pcl.cur_file) {
    g_pcl.pkt_dropped++;
    return;
  }

  if (len > PCL_MAX_PKT)
    len = PCL_MAX_PKT;

  // Build radiotap header
  PclRadiotap rt;
  memset(&rt, 0, sizeof(rt));
  rt.it_version = 0;
  rt.it_len = sizeof(PclRadiotap);
  rt.it_present = 0x0000182F;
  rt.tsft = millis() * 1000ULL;  // microseconds
  rt.flags = 0;
  rt.rate = rate_500k;
  // Channel freq from channel number (2.4 GHz band)
  rt.channel_freq = (channel <= 13) ? (2407 + channel * 5) : 2484;
  rt.channel_flags = 0x00C0;  // 2GHz, CCK
  rt.ant_signal = rssi;
  rt.ant_noise = noise;
  rt.antenna = 0;

  uint16_t pkt_total = sizeof(rt) + len;

  // Check if file rotation needed
  if (g_pcl.bytes_written + 16 + pkt_total > PCL_MAX_SIZE) {
    g_pcl.cur_file.flush();
    g_pcl.cur_file.close();
    g_pcl.file_idx = (g_pcl.file_idx + 1) % PCL_MAX_FILES;
    g_pcl.file_rotations++;
    if (!pcl_open_file()) {
      g_pcl.capturing = false;
      g_pcl.pkt_dropped++;
      return;
    }
  }

  // Write pcap packet header
  uint32_t now_s = millis() / 1000;
  uint32_t now_us = (millis() % 1000) * 1000;
  PclPktHdr hdr;
  hdr.ts_sec = now_s;
  hdr.ts_usec = now_us;
  hdr.incl_len = pkt_total;
  hdr.orig_len = pkt_total;
  g_pcl.cur_file.write((uint8_t *)&hdr, sizeof(hdr));
  g_pcl.cur_file.write((uint8_t *)&rt, sizeof(rt));
  g_pcl.cur_file.write(data, len);

  g_pcl.bytes_written += sizeof(hdr) + pkt_total;
  g_pcl.pkt_captured++;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void pcl_routes() {
  server.on("/api/pcl/status", HTTP_GET, []() {
    String s = "{\"capturing\":" + String(g_pcl.capturing ? "true" : "false");
    s += ",\"pkt_captured\":" + String(g_pcl.pkt_captured);
    s += ",\"pkt_dropped\":" + String(g_pcl.pkt_dropped);
    s += ",\"bytes_written\":" + String(g_pcl.bytes_written);
    s += ",\"file_idx\":" + String(g_pcl.file_idx);
    s += ",\"rotations\":" + String(g_pcl.file_rotations);
    s += ",\"cur_file\":\"" + String(g_pcl.cur_path) + "\"";
    s += "}";
    server.send(200, "application/json", s);
  });

  server.on("/api/pcl/start", HTTP_POST, []() {
    bool ok = pcl_start();
    server.send(200, "application/json", ok ? "{\"ok\":true}" : "{\"error\":\"start failed\"}");
  });

  server.on("/api/pcl/stop", HTTP_POST, []() {
    pcl_stop();
    server.send(200, "application/json", "{\"ok\":true}");
  });

  // Serve latest capture file for download
  server.on("/api/pcl/download", HTTP_GET, []() {
    if (!g_pcl.cur_path[0]) {
      server.send(404, "text/plain", "No capture");
      return;
    }
    File f = LittleFS.open(g_pcl.cur_path, "r");
    if (!f) {
      server.send(404, "text/plain", "File not found");
      return;
    }
    server.streamFile(f, "application/octet-stream");
    f.close();
  });

  server.on("/api/pcl/list", HTTP_GET, []() {
    String s = "[";
    bool first = true;
    for (int i = 0; i < PCL_MAX_FILES; i++) {
      char path[32];
      snprintf(path, sizeof(path), "%s/cap%02d.pcap", PCL_DIR, i);
      if (!LittleFS.exists(path))
        continue;
      File f = LittleFS.open(path, "r");
      size_t sz = f ? f.size() : 0;
      if (f)
        f.close();
      if (!first)
        s += ",";
      first = false;
      s += "{\"file\":\"" + String(path) + "\",\"size\":" + String(sz) + "}";
    }
    s += "]";
    server.send(200, "application/json", s);
  });
}

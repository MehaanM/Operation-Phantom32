#pragma once
// pcap_streamer.h — Live PCAP streaming over HTTP chunked transfer.
// Streams captured 802.11 frames as a valid PCAP file download in real time.
// Compatible with Wireshark, tcpdump. Prefix: pcaps_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define PCAPS_RING_SIZE      256
#define PCAPS_MAX_PKT_LEN    512
#define PCAPS_FILTER_COUNT   8

// PCAP global header
#define PCAP_MAGIC        0xA1B2C3D4
#define PCAP_VERSION_MAJ  2
#define PCAP_VERSION_MIN  4
#define PCAP_SNAPLEN      65535
#define PCAP_DLT_RADIOTAP 127   // IEEE 802.11 with radiotap header
#define PCAP_DLT_80211    105   // raw 802.11

// ─── Packet ring entry ────────────────────────────────────────────────────────
struct PcapsPacket {
  bool    used;
  uint32_t ts_sec;
  uint32_t ts_usec;
  uint16_t orig_len;
  uint16_t cap_len;
  uint8_t  data[PCAPS_MAX_PKT_LEN];
};

// ─── Capture filter ───────────────────────────────────────────────────────────
struct PcapsFilter {
  bool    active;
  uint8_t type;      // 0=frame_type, 1=mac_match, 2=channel, 3=min_rssi
  uint8_t frame_type;
  uint8_t frame_sub;
  uint8_t mac[6];
  bool    mac_src;   // true=match src, false=match dst
  uint8_t channel;
  int8_t  min_rssi;
};

// ─── State ────────────────────────────────────────────────────────────────────
static struct {
  PcapsPacket ring[PCAPS_RING_SIZE];
  uint16_t    write_head;
  uint16_t    total_captured;
  uint32_t    total_bytes;
  bool        capturing;
  bool        streaming;
  PcapsFilter filters[PCAPS_FILTER_COUNT];
  uint8_t     filter_count;
  // Statistics
  uint32_t    dropped;        // dropped due to full ring
  uint32_t    filtered_out;   // filtered by rules
  uint8_t     dlt;            // link type for PCAP header
} g_pcaps = {};

// ─── PCAP header writers ──────────────────────────────────────────────────────
static void pcaps_write_u32_le(uint8_t *buf, uint32_t val) {
  buf[0] = val & 0xFF; buf[1] = (val>>8)&0xFF;
  buf[2] = (val>>16)&0xFF; buf[3] = (val>>24)&0xFF;
}

static void pcaps_write_u16_le(uint8_t *buf, uint16_t val) {
  buf[0] = val & 0xFF; buf[1] = (val>>8)&0xFF;
}

static void pcaps_global_header(uint8_t *out) {
  pcaps_write_u32_le(out,    PCAP_MAGIC);
  pcaps_write_u16_le(out+4,  PCAP_VERSION_MAJ);
  pcaps_write_u16_le(out+6,  PCAP_VERSION_MIN);
  pcaps_write_u32_le(out+8,  0);       // timezone = UTC
  pcaps_write_u32_le(out+12, 0);       // accuracy
  pcaps_write_u32_le(out+16, PCAP_SNAPLEN);
  pcaps_write_u32_le(out+20, g_pcaps.dlt ? g_pcaps.dlt : PCAP_DLT_RADIOTAP);
}

static void pcaps_pkt_header(uint8_t *out, uint32_t ts_sec, uint32_t ts_usec,
                              uint16_t cap_len, uint16_t orig_len) {
  pcaps_write_u32_le(out,    ts_sec);
  pcaps_write_u32_le(out+4,  ts_usec);
  pcaps_write_u32_le(out+8,  cap_len);
  pcaps_write_u32_le(out+12, orig_len);
}

// ─── Filter check ─────────────────────────────────────────────────────────────
static bool pcaps_pass_filters(const uint8_t *raw, int len, const uint8_t *src,
                                const uint8_t *dst, uint8_t type, uint8_t sub,
                                uint8_t channel, int8_t rssi) {
  if (g_pcaps.filter_count == 0) return true;
  for (int i = 0; i < PCAPS_FILTER_COUNT; i++) {
    PcapsFilter &f = g_pcaps.filters[i];
    if (!f.active) continue;
    switch (f.type) {
      case 0: // frame type/subtype filter
        if (f.frame_type != 0xFF && type != f.frame_type) return false;
        if (f.frame_sub != 0xFF && sub != f.frame_sub) return false;
        break;
      case 1: // MAC filter
        if (f.mac_src && memcmp(src, f.mac, 6) != 0) return false;
        if (!f.mac_src && memcmp(dst, f.mac, 6) != 0) return false;
        break;
      case 2: // channel
        if (channel != f.channel) return false;
        break;
      case 3: // RSSI
        if (rssi < f.min_rssi) return false;
        break;
    }
  }
  return true;
}

// ─── Ingest packet ────────────────────────────────────────────────────────────
void pcaps_ingest(const uint8_t *raw, int len, const uint8_t *src, const uint8_t *dst,
                  uint8_t type, uint8_t sub, uint8_t channel, int8_t rssi) {
  if (!g_pcaps.capturing) return;

  if (!pcaps_pass_filters(raw, len, src, dst, type, sub, channel, rssi)) {
    g_pcaps.filtered_out++;
    return;
  }

  uint16_t idx = g_pcaps.write_head % PCAPS_RING_SIZE;
  PcapsPacket &pkt = g_pcaps.ring[idx];

  if (pkt.used) {
    g_pcaps.dropped++;
    return;
  }

  uint32_t now_ms = millis();
  pkt.ts_sec  = now_ms / 1000;
  pkt.ts_usec = (now_ms % 1000) * 1000;
  pkt.cap_len = (uint16_t)min(len, PCAPS_MAX_PKT_LEN);
  pkt.orig_len = (uint16_t)len;
  memcpy(pkt.data, raw, pkt.cap_len);
  pkt.used = true;

  g_pcaps.write_head++;
  g_pcaps.total_captured++;
  g_pcaps.total_bytes += pkt.cap_len;
}

// ─── Write to LittleFS ────────────────────────────────────────────────────────
inline bool pcaps_save_to_file(const char *path) {
  File f = LittleFS.open(path, "w");
  if (!f) return false;

  uint8_t global_hdr[24];
  pcaps_global_header(global_hdr);
  f.write(global_hdr, 24);

  for (int i = 0; i < PCAPS_RING_SIZE; i++) {
    int idx = (g_pcaps.write_head - g_pcaps.total_captured + i) % PCAPS_RING_SIZE;
    if (idx < 0) idx += PCAPS_RING_SIZE;
    PcapsPacket &pkt = g_pcaps.ring[idx];
    if (!pkt.used) continue;

    uint8_t pkt_hdr[16];
    pcaps_pkt_header(pkt_hdr, pkt.ts_sec, pkt.ts_usec, pkt.cap_len, pkt.orig_len);
    f.write(pkt_hdr, 16);
    f.write(pkt.data, pkt.cap_len);
  }
  f.close();
  return true;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void pcaps_routes() {
  server.on("/api/pcaps/status", HTTP_GET, []() {
    String s = "{\"capturing\":" + String(g_pcaps.capturing?"true":"false");
    s += ",\"total_captured\":" + String(g_pcaps.total_captured);
    s += ",\"total_bytes\":" + String(g_pcaps.total_bytes);
    s += ",\"dropped\":" + String(g_pcaps.dropped);
    s += ",\"filtered_out\":" + String(g_pcaps.filtered_out);
    s += ",\"ring_used\":" + String(g_pcaps.write_head % PCAPS_RING_SIZE);
    s += "}";
    server.send(200, "application/json", s);
  });

  server.on("/api/pcaps/start", HTTP_POST, []() {
    // Clear ring
    memset(g_pcaps.ring, 0, sizeof(g_pcaps.ring));
    g_pcaps.write_head = 0;
    g_pcaps.total_captured = 0;
    g_pcaps.total_bytes = 0;
    g_pcaps.dropped = 0;
    g_pcaps.filtered_out = 0;
    g_pcaps.capturing = true;
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/pcaps/stop", HTTP_POST, []() {
    g_pcaps.capturing = false;
    server.send(200, "application/json", "{\"ok\":true}");
  });

  // Live download as PCAP file
  server.on("/api/pcaps/download", HTTP_GET, []() {
    server.sendHeader("Content-Type", "application/octet-stream");
    server.sendHeader("Content-Disposition", "attachment; filename=phantom32_capture.pcap");
    server.sendHeader("Transfer-Encoding", "chunked");
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200);

    // Global header
    uint8_t global_hdr[24];
    pcaps_global_header(global_hdr);
    server.sendContent(String((char*)global_hdr).substring(0, 0)); // hack: use write
    server.client().write(global_hdr, 24);

    // Packets
    uint8_t pkt_hdr[16];
    for (int i = 0; i < PCAPS_RING_SIZE; i++) {
      int idx = (g_pcaps.write_head - g_pcaps.total_captured + i + PCAPS_RING_SIZE) % PCAPS_RING_SIZE;
      PcapsPacket &pkt = g_pcaps.ring[idx];
      if (!pkt.used) continue;
      pcaps_pkt_header(pkt_hdr, pkt.ts_sec, pkt.ts_usec, pkt.cap_len, pkt.orig_len);
      server.client().write(pkt_hdr, 16);
      server.client().write(pkt.data, pkt.cap_len);
    }
    server.client().stop();
  });

  server.on("/api/pcaps/filter/add", HTTP_POST, []() {
    if (g_pcaps.filter_count >= PCAPS_FILTER_COUNT) {
      server.send(400, "application/json", "{\"error\":\"filter list full\"}"); return;
    }
    PcapsFilter &f = g_pcaps.filters[g_pcaps.filter_count++];
    memset(&f, 0, sizeof(f));
    f.active = true;
    if (server.hasArg("type")) f.type = server.arg("type").toInt();
    if (server.hasArg("frame_type")) f.frame_type = server.arg("frame_type").toInt();
    if (server.hasArg("frame_sub"))  f.frame_sub  = server.arg("frame_sub").toInt();
    if (server.hasArg("channel"))    f.channel    = server.arg("channel").toInt();
    if (server.hasArg("min_rssi"))   f.min_rssi   = server.arg("min_rssi").toInt();
    if (server.hasArg("mac")) {
      sscanf(server.arg("mac").c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
             &f.mac[0],&f.mac[1],&f.mac[2],&f.mac[3],&f.mac[4],&f.mac[5]);
      f.mac_src = !server.hasArg("dst");
    }
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/pcaps/filter/clear", HTTP_POST, []() {
    g_pcaps.filter_count = 0;
    memset(g_pcaps.filters, 0, sizeof(g_pcaps.filters));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

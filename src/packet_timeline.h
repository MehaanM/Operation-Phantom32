#pragma once
// packet_timeline.h — Packet timeline recorder.
// Records windowed packet-rate samples for sparkline visualization.
// Prefix: ptl_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define PTL_BINS 120      // 2 minutes of 1-second bins
#define PTL_MAX_FLOWS 32  // per-flow sparklines

struct PtlBin {
  uint32_t pkt_count;
  uint32_t byte_count;
  uint32_t mgmt_count;
  uint32_t data_count;
  uint32_t ctrl_count;
  uint32_t deauth_count;
  uint32_t beacon_count;
  uint32_t probe_count;
};

struct PtlFlow {
  bool active;
  uint8_t bssid[6];
  char ssid[33];
  uint16_t pkt_rate[PTL_BINS];   // 16-bit pkt/s per bin (saves RAM)
  uint16_t byte_rate[PTL_BINS];  // bytes/s per bin
  uint8_t bin_head;
  uint32_t last_pkt;
  uint32_t last_byte;
  uint32_t last_ts;
};

static struct {
  PtlBin global[PTL_BINS];
  PtlFlow flows[PTL_MAX_FLOWS];
  uint8_t g_head;      // next write position in global ring
  uint32_t g_tick_ts;  // timestamp of last tick

  uint32_t total_pkts;
  uint32_t total_bytes;
  uint32_t peak_pps;
  uint32_t peak_bps;
} g_ptl = {};

// ─── Helpers ─────────────────────────────────────────────────────────────────
static bool ptl_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static PtlFlow *ptl_find_flow(const uint8_t *bssid) {
  for (int i = 0; i < PTL_MAX_FLOWS; i++)
    if (g_ptl.flows[i].active && ptl_mac_eq(g_ptl.flows[i].bssid, bssid))
      return &g_ptl.flows[i];
  return nullptr;
}

static PtlFlow *ptl_alloc_flow(const uint8_t *bssid, const char *ssid) {
  for (int i = 0; i < PTL_MAX_FLOWS; i++) {
    if (!g_ptl.flows[i].active) {
      PtlFlow &f = g_ptl.flows[i];
      memset(&f, 0, sizeof(f));
      f.active = true;
      memcpy(f.bssid, bssid, 6);
      strncpy(f.ssid, ssid ? ssid : "", 32);
      return &f;
    }
  }
  // Evict oldest
  uint32_t oldest = UINT32_MAX;
  int idx = 0;
  for (int i = 0; i < PTL_MAX_FLOWS; i++)
    if (g_ptl.flows[i].last_ts < oldest) {
      oldest = g_ptl.flows[i].last_ts;
      idx = i;
    }
  memset(&g_ptl.flows[idx], 0, sizeof(g_ptl.flows[0]));
  g_ptl.flows[idx].active = true;
  memcpy(g_ptl.flows[idx].bssid, bssid, 6);
  strncpy(g_ptl.flows[idx].ssid, ssid ? ssid : "", 32);
  return &g_ptl.flows[idx];
}

// ─── Per-second tick — called from timer/main loop ───────────────────────────
void ptl_tick() {
  uint32_t now = millis() / 1000;
  if (now == g_ptl.g_tick_ts)
    return;
  g_ptl.g_tick_ts = now;

  // Advance global ring to next bin
  g_ptl.g_head = (g_ptl.g_head + 1) % PTL_BINS;
  memset(&g_ptl.global[g_ptl.g_head], 0, sizeof(PtlBin));

  // Advance each flow's ring
  for (int i = 0; i < PTL_MAX_FLOWS; i++) {
    PtlFlow &f = g_ptl.flows[i];
    if (!f.active)
      continue;
    f.bin_head = (f.bin_head + 1) % PTL_BINS;
    f.pkt_rate[f.bin_head] = 0;
    f.byte_rate[f.bin_head] = 0;
  }
}

// ─── Ingest a frame ──────────────────────────────────────────────────────────
void ptl_ingest(const uint8_t *bssid, const char *ssid, uint8_t type, uint8_t subtype,
                uint16_t len) {
  ptl_tick();  // ensure ring is current

  PtlBin &b = g_ptl.global[g_ptl.g_head];
  b.pkt_count++;
  b.byte_count += len;

  if (type == 0)
    b.mgmt_count++;
  else if (type == 1)
    b.ctrl_count++;
  else if (type == 2)
    b.data_count++;

  if (type == 0) {
    if (subtype == 8)
      b.beacon_count++;
    if (subtype == 4 || subtype == 5)
      b.probe_count++;
    if (subtype == 12)
      b.deauth_count++;
  }

  g_ptl.total_pkts++;
  g_ptl.total_bytes += len;

  // Per-flow
  if (!bssid)
    return;
  PtlFlow *f = ptl_find_flow(bssid);
  if (!f)
    f = ptl_alloc_flow(bssid, ssid);
  if (!f)
    return;

  uint16_t pps = f->pkt_rate[f->bin_head] < 65535 ? f->pkt_rate[f->bin_head] + 1 : 65535;
  f->pkt_rate[f->bin_head] = pps;
  uint16_t bps = f->byte_rate[f->bin_head];
  f->byte_rate[f->bin_head] = (bps + len < 65535) ? bps + len : 65535;
  f->last_ts = millis() / 1000;

  // Update peaks
  if (pps > g_ptl.peak_pps)
    g_ptl.peak_pps = pps;
}

// ─── JSON ─────────────────────────────────────────────────────────────────────
static String ptl_json_global() {
  // Return last PTL_BINS entries in order (oldest first)
  String s = "{\"bins\":" + String(PTL_BINS);
  s += ",\"total_pkts\":" + String(g_ptl.total_pkts);
  s += ",\"total_bytes\":" + String(g_ptl.total_bytes);
  s += ",\"peak_pps\":" + String(g_ptl.peak_pps);
  s += ",\"pkt\":[";
  for (int i = 0; i < PTL_BINS; i++) {
    int pos = (g_ptl.g_head + 1 + i) % PTL_BINS;
    if (i)
      s += ",";
    s += String(g_ptl.global[pos].pkt_count);
  }
  s += "],\"bytes\":[";
  for (int i = 0; i < PTL_BINS; i++) {
    int pos = (g_ptl.g_head + 1 + i) % PTL_BINS;
    if (i)
      s += ",";
    s += String(g_ptl.global[pos].byte_count);
  }
  s += "],\"mgmt\":[";
  for (int i = 0; i < PTL_BINS; i++) {
    int pos = (g_ptl.g_head + 1 + i) % PTL_BINS;
    if (i)
      s += ",";
    s += String(g_ptl.global[pos].mgmt_count);
  }
  s += "],\"data\":[";
  for (int i = 0; i < PTL_BINS; i++) {
    int pos = (g_ptl.g_head + 1 + i) % PTL_BINS;
    if (i)
      s += ",";
    s += String(g_ptl.global[pos].data_count);
  }
  s += "],\"deauth\":[";
  for (int i = 0; i < PTL_BINS; i++) {
    int pos = (g_ptl.g_head + 1 + i) % PTL_BINS;
    if (i)
      s += ",";
    s += String(g_ptl.global[pos].deauth_count);
  }
  s += "]}";
  return s;
}

static void ptl_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String ptl_json_flows() {
  String s = "[";
  char mac[18];
  bool first = true;
  for (int i = 0; i < PTL_MAX_FLOWS; i++) {
    PtlFlow &f = g_ptl.flows[i];
    if (!f.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    ptl_mac_str(f.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(f.ssid) + "\"";
    s += ",\"pkt_rate\":[";
    for (int j = 0; j < PTL_BINS; j++) {
      int pos = (f.bin_head + 1 + j) % PTL_BINS;
      if (j)
        s += ",";
      s += String(f.pkt_rate[pos]);
    }
    s += "],\"byte_rate\":[";
    for (int j = 0; j < PTL_BINS; j++) {
      int pos = (f.bin_head + 1 + j) % PTL_BINS;
      if (j)
        s += ",";
      s += String(f.byte_rate[pos]);
    }
    s += "]}";
  }
  s += "]";
  return s;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void ptl_routes() {
  server.on("/api/ptl/global", HTTP_GET,
            []() { server.send(200, "application/json", ptl_json_global()); });
  server.on("/api/ptl/flows", HTTP_GET,
            []() { server.send(200, "application/json", ptl_json_flows()); });
  server.on("/api/ptl/clear", HTTP_POST, []() {
    memset(&g_ptl, 0, sizeof(g_ptl));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

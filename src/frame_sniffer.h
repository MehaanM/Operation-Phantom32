#pragma once
// frame_sniffer.h — Raw 802.11 frame capture and type statistics.
// Deep-inspects captured frames and buckets them by subtype/direction.
// Prefix: frs_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define FRS_HIST_LEN 60  // 1-minute rolling window
#define FRS_MAX_SSID_TABLE 64

enum FrsDirection : uint8_t {
  FRS_DIR_FROMDS = 0,  // FromDS=1, ToDS=0
  FRS_DIR_TODS = 1,    // FromDS=0, ToDS=1
  FRS_DIR_BSSDS = 2,   // Both DS bits set (WDS)
  FRS_DIR_IBSS = 3,    // Both DS bits clear
};

struct FrsBucket {
  uint32_t total;
  uint32_t mgmt;
  uint32_t ctrl;
  uint32_t data;
  uint32_t null_data;
  uint32_t qos_data;
  uint32_t beacon;
  uint32_t probe_req;
  uint32_t probe_rsp;
  uint32_t assoc_req;
  uint32_t assoc_rsp;
  uint32_t disassoc;
  uint32_t deauth;
  uint32_t auth;
  uint32_t action;
  uint32_t rts_cts;
  uint32_t ack;
  uint32_t bytes;
};

static struct {
  FrsBucket hist[FRS_HIST_LEN];
  uint32_t hist_head;
  FrsBucket total;
  uint32_t dir_counts[4];
  uint32_t per_channel[14];  // ch 1-13
  uint32_t last_tick_s;
  FrsBucket cur;
} g_frs = {};

void frs_ingest(const uint8_t *data, uint16_t len, uint8_t ch) {
  if (!data || len < 10)
    return;
  uint8_t fc0 = data[0];
  uint8_t fc1 = data[1];
  uint8_t type = (fc0 >> 2) & 0x03;
  uint8_t subtype = (fc0 >> 4) & 0x0f;
  bool to_ds = fc1 & 0x01;
  bool from_ds = (fc1 >> 1) & 0x01;

  FrsBucket &b = g_frs.cur;
  FrsBucket &t = g_frs.total;
  b.total++;
  t.total++;
  b.bytes += len;
  t.bytes += len;

  if (ch >= 1 && ch <= 13) {
    g_frs.per_channel[ch - 1]++;
  }

  FrsDirection dir;
  if (!from_ds && !to_ds)
    dir = FRS_DIR_IBSS;
  else if (!from_ds && to_ds)
    dir = FRS_DIR_TODS;
  else if (from_ds && !to_ds)
    dir = FRS_DIR_FROMDS;
  else
    dir = FRS_DIR_BSSDS;
  g_frs.dir_counts[dir]++;

  switch (type) {
    case 0:  // management
      b.mgmt++;
      t.mgmt++;
      switch (subtype) {
        case 8:
          b.beacon++;
          t.beacon++;
          break;
        case 4:
          b.probe_req++;
          t.probe_req++;
          break;
        case 5:
          b.probe_rsp++;
          t.probe_rsp++;
          break;
        case 0:
          b.assoc_req++;
          t.assoc_req++;
          break;
        case 1:
          b.assoc_rsp++;
          t.assoc_rsp++;
          break;
        case 10:
          b.disassoc++;
          t.disassoc++;
          break;
        case 12:
          b.deauth++;
          t.deauth++;
          break;
        case 11:
        case 1:
          b.auth++;
          t.auth++;
          break;
        case 13:
          b.action++;
          t.action++;
          break;
      }
      break;
    case 1:  // control
      b.ctrl++;
      t.ctrl++;
      switch (subtype) {
        case 11:
          b.rts_cts++;
          t.rts_cts++;
          break;
        case 12:
          b.rts_cts++;
          t.rts_cts++;
          break;
        case 13:
          b.ack++;
          t.ack++;
          break;
      }
      break;
    case 2:  // data
      b.data++;
      t.data++;
      if (subtype == 4) {
        b.null_data++;
        t.null_data++;
      }
      if (subtype & 0x8) {
        b.qos_data++;
        t.qos_data++;
      }
      break;
  }
}

void frs_tick() {
  uint32_t now_s = millis() / 1000;
  if (now_s == g_frs.last_tick_s)
    return;
  g_frs.last_tick_s = now_s;
  g_frs.hist[g_frs.hist_head % FRS_HIST_LEN] = g_frs.cur;
  g_frs.hist_head = (g_frs.hist_head + 1) % FRS_HIST_LEN;
  memset(&g_frs.cur, 0, sizeof(g_frs.cur));
}

static String frs_hist_array(uint32_t FrsBucket::*field) {
  String s = "[";
  for (int i = 0; i < FRS_HIST_LEN; i++) {
    int idx = (g_frs.hist_head + i) % FRS_HIST_LEN;
    if (i)
      s += ",";
    s += String(g_frs.hist[idx].*field);
  }
  s += "]";
  return s;
}

static String frs_json_all() {
  FrsBucket &t = g_frs.total;
  String s = "{\"total\":" + String(t.total);
  s += ",\"bytes\":" + String(t.bytes);
  s += ",\"mgmt\":" + String(t.mgmt);
  s += ",\"ctrl\":" + String(t.ctrl);
  s += ",\"data\":" + String(t.data);
  s += ",\"beacon\":" + String(t.beacon);
  s += ",\"probe_req\":" + String(t.probe_req);
  s += ",\"probe_rsp\":" + String(t.probe_rsp);
  s += ",\"assoc_req\":" + String(t.assoc_req);
  s += ",\"assoc_rsp\":" + String(t.assoc_rsp);
  s += ",\"disassoc\":" + String(t.disassoc);
  s += ",\"deauth\":" + String(t.deauth);
  s += ",\"auth\":" + String(t.auth);
  s += ",\"action\":" + String(t.action);
  s += ",\"rts_cts\":" + String(t.rts_cts);
  s += ",\"ack\":" + String(t.ack);
  s += ",\"null_data\":" + String(t.null_data);
  s += ",\"qos_data\":" + String(t.qos_data);
  s += ",\"dir_fromds\":" + String(g_frs.dir_counts[0]);
  s += ",\"dir_tods\":" + String(g_frs.dir_counts[1]);
  s += ",\"dir_bssds\":" + String(g_frs.dir_counts[2]);
  s += ",\"dir_ibss\":" + String(g_frs.dir_counts[3]);
  s += ",\"per_channel\":[";
  for (int i = 0; i < 13; i++) {
    if (i)
      s += ",";
    s += String(g_frs.per_channel[i]);
  }
  s += "],\"hist_total\":" + frs_hist_array(&FrsBucket::total);
  s += ",\"hist_mgmt\":" + frs_hist_array(&FrsBucket::mgmt);
  s += ",\"hist_data\":" + frs_hist_array(&FrsBucket::data);
  s += ",\"hist_ctrl\":" + frs_hist_array(&FrsBucket::ctrl);
  s += "}";
  return s;
}

inline void frs_routes() {
  server.on("/api/frs/all", HTTP_GET, []() {
    frs_tick();
    server.send(200, "application/json", frs_json_all());
  });
  server.on("/api/frs/clear", HTTP_POST, []() {
    memset(&g_frs, 0, sizeof(g_frs));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

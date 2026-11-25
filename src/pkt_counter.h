#pragma once
// pkt_counter.h — Global and per-channel packet counters with per-second rates.
// Lightweight always-on counter backend. Prefix: pkc_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define PKC_CHANNELS 14
#define PKC_RATE_WIN 60  // 60-second rate window

struct PkcChannel {
  uint64_t total_frames;
  uint64_t total_bytes;
  uint32_t mgmt_frames;
  uint32_t ctrl_frames;
  uint32_t data_frames;
  uint32_t retry_frames;
  uint32_t frag_frames;

  // Current 1-second rate
  uint32_t cur_fps;  // frames per second
  uint32_t cur_bps;  // bytes per second

  // Rolling rate ring (60 seconds)
  uint16_t fps_ring[PKC_RATE_WIN];
  uint16_t bps_ring[PKC_RATE_WIN];
  uint8_t ring_head;

  // Per-second accumulator
  uint32_t acc_frames;
  uint32_t acc_bytes;
  uint32_t acc_ts;
};

struct PkcGlobal {
  uint64_t total_frames;
  uint64_t total_bytes;
  uint32_t mgmt;
  uint32_t ctrl;
  uint32_t data;
  uint32_t retry;
  uint32_t frag;
  uint32_t qos;
  uint32_t protected_frames;

  uint32_t peak_fps;
  uint32_t peak_bps;
  uint32_t cur_fps;
  uint32_t cur_bps;

  uint32_t acc_frames;
  uint32_t acc_bytes;
  uint32_t acc_ts;

  uint16_t fps_ring[PKC_RATE_WIN];
  uint16_t bps_ring[PKC_RATE_WIN];
  uint8_t ring_head;

  uint32_t start_ts;
};

static struct {
  PkcGlobal global;
  PkcChannel ch[PKC_CHANNELS];
} g_pkc = {};

static void pkc_tick_global(uint32_t now) {
  if (now == g_pkc.global.acc_ts)
    return;
  g_pkc.global.acc_ts = now;

  g_pkc.global.cur_fps = g_pkc.global.acc_frames;
  g_pkc.global.cur_bps = g_pkc.global.acc_bytes;

  if (g_pkc.global.cur_fps > g_pkc.global.peak_fps)
    g_pkc.global.peak_fps = g_pkc.global.cur_fps;
  if (g_pkc.global.cur_bps > g_pkc.global.peak_bps)
    g_pkc.global.peak_bps = g_pkc.global.cur_bps;

  g_pkc.global.fps_ring[g_pkc.global.ring_head] =
      (uint16_t)min(g_pkc.global.cur_fps, (uint32_t)65535);
  g_pkc.global.bps_ring[g_pkc.global.ring_head] =
      (uint16_t)min(g_pkc.global.cur_bps / 8, (uint32_t)65535);
  g_pkc.global.ring_head = (g_pkc.global.ring_head + 1) % PKC_RATE_WIN;

  g_pkc.global.acc_frames = 0;
  g_pkc.global.acc_bytes = 0;
}

static void pkc_tick_channel(PkcChannel &c, uint32_t now) {
  if (now == c.acc_ts)
    return;
  c.acc_ts = now;

  c.cur_fps = c.acc_frames;
  c.cur_bps = c.acc_bytes;

  c.fps_ring[c.ring_head] = (uint16_t)min(c.cur_fps, (uint32_t)65535);
  c.bps_ring[c.ring_head] = (uint16_t)min(c.cur_bps / 8, (uint32_t)65535);
  c.ring_head = (c.ring_head + 1) % PKC_RATE_WIN;

  c.acc_frames = 0;
  c.acc_bytes = 0;
}

void pkc_init() {
  g_pkc.global.start_ts = millis() / 1000;
  g_pkc.global.acc_ts = g_pkc.global.start_ts;
  for (int i = 0; i < PKC_CHANNELS; i++)
    g_pkc.ch[i].acc_ts = g_pkc.global.start_ts;
}

void pkc_ingest(uint8_t type, uint8_t channel, uint16_t len, bool retry, bool frag, bool qos,
                bool prot) {
  uint32_t now = millis() / 1000;
  pkc_tick_global(now);

  g_pkc.global.total_frames++;
  g_pkc.global.total_bytes += len;
  g_pkc.global.acc_frames++;
  g_pkc.global.acc_bytes += len;

  if (type == 0)
    g_pkc.global.mgmt++;
  else if (type == 1)
    g_pkc.global.ctrl++;
  else if (type == 2)
    g_pkc.global.data++;

  if (retry)
    g_pkc.global.retry++;
  if (frag)
    g_pkc.global.frag++;
  if (qos)
    g_pkc.global.qos++;
  if (prot)
    g_pkc.global.protected_frames++;

  if (channel > 0 && channel <= PKC_CHANNELS) {
    PkcChannel &c = g_pkc.ch[channel - 1];
    pkc_tick_channel(c, now);

    c.total_frames++;
    c.total_bytes += len;
    c.acc_frames++;
    c.acc_bytes += len;

    if (type == 0)
      c.mgmt_frames++;
    else if (type == 1)
      c.ctrl_frames++;
    else if (type == 2)
      c.data_frames++;

    if (retry)
      c.retry_frames++;
    if (frag)
      c.frag_frames++;
  }
}

static String pkc_json_summary() {
  uint32_t now = millis() / 1000;
  pkc_tick_global(now);

  String s = "{\"frames\":" + String((uint32_t)g_pkc.global.total_frames);
  s += ",\"bytes\":" + String((uint32_t)g_pkc.global.total_bytes);
  s += ",\"cur_fps\":" + String(g_pkc.global.cur_fps);
  s += ",\"cur_bps\":" + String(g_pkc.global.cur_bps);
  s += ",\"peak_fps\":" + String(g_pkc.global.peak_fps);
  s += ",\"peak_bps\":" + String(g_pkc.global.peak_bps);
  s += ",\"mgmt\":" + String(g_pkc.global.mgmt);
  s += ",\"ctrl\":" + String(g_pkc.global.ctrl);
  s += ",\"data\":" + String(g_pkc.global.data);
  s += ",\"retry\":" + String(g_pkc.global.retry);
  s += ",\"frag\":" + String(g_pkc.global.frag);
  s += ",\"qos\":" + String(g_pkc.global.qos);
  s += ",\"prot\":" + String(g_pkc.global.protected_frames);
  s += ",\"uptime\":" + String(now - g_pkc.global.start_ts);
  s += ",\"fps_ring\":[";
  for (int i = 0; i < PKC_RATE_WIN; i++) {
    int pos = (g_pkc.global.ring_head + i) % PKC_RATE_WIN;
    if (i)
      s += ",";
    s += String(g_pkc.global.fps_ring[pos]);
  }
  s += "]}";
  return s;
}

static String pkc_json_channels() {
  uint32_t now = millis() / 1000;
  String s = "[";
  bool first = true;
  for (int i = 0; i < PKC_CHANNELS; i++) {
    PkcChannel &c = g_pkc.ch[i];
    pkc_tick_channel(c, now);
    if (!c.total_frames)
      continue;
    if (!first)
      s += ",";
    first = false;
    s += "{\"ch\":" + String(i + 1);
    s += ",\"frames\":" + String((uint32_t)c.total_frames);
    s += ",\"bytes\":" + String((uint32_t)c.total_bytes);
    s += ",\"fps\":" + String(c.cur_fps);
    s += ",\"mgmt\":" + String(c.mgmt_frames);
    s += ",\"ctrl\":" + String(c.ctrl_frames);
    s += ",\"data\":" + String(c.data_frames);
    s += ",\"retry\":" + String(c.retry_frames);
    s += "}";
  }
  s += "]";
  return s;
}

inline void pkc_routes() {
  server.on("/api/pkc/summary", HTTP_GET,
            []() { server.send(200, "application/json", pkc_json_summary()); });
  server.on("/api/pkc/channels", HTTP_GET,
            []() { server.send(200, "application/json", pkc_json_channels()); });
  server.on("/api/pkc/reset", HTTP_POST, []() {
    memset(&g_pkc, 0, sizeof(g_pkc));
    pkc_init();
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

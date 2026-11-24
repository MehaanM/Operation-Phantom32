#pragma once
// frame_rate_tracker.h — Per-AP and per-channel 802.11 frame rate monitoring.
// Tracks frame rates in 1-second buckets with 60-second rolling history.
// Prefix: frt_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define FRT_MAX_APS       48
#define FRT_HIST_LEN      60   // seconds of history
#define FRT_MAX_CHANNELS  14

struct FrtBucket {
  uint32_t frames;
  uint32_t bytes;
};

struct FrtAP {
  bool     active;
  uint8_t  bssid[6];
  char     ssid[33];
  uint8_t  channel;
  int8_t   rssi;

  FrtBucket hist[FRT_HIST_LEN];
  uint8_t   hist_head;

  FrtBucket accum;         // current second accumulator
  uint32_t  accum_ts;      // second of last accum

  uint32_t  peak_fps;
  uint32_t  total_frames;
  uint32_t  total_bytes;
  uint32_t  current_fps;
  uint32_t  avg_fps;

  uint32_t  first_ts;
  uint32_t  last_ts;
};

struct FrtChannel {
  FrtBucket hist[FRT_HIST_LEN];
  uint8_t   hist_head;
  FrtBucket accum;
  uint32_t  accum_ts;
  uint32_t  peak_fps;
  uint32_t  current_fps;
  uint32_t  total_frames;
};

static struct {
  FrtAP      aps[FRT_MAX_APS];
  uint8_t    ap_count;
  FrtChannel channels[FRT_MAX_CHANNELS];
  uint32_t   global_fps;
  uint32_t   global_total;
} g_frt = {};

static bool frt_mac_eq(const uint8_t *a, const uint8_t *b) { return memcmp(a, b, 6) == 0; }

static FrtAP* frt_find_or_alloc(const uint8_t *bssid) {
  for (int i = 0; i < FRT_MAX_APS; i++)
    if (g_frt.aps[i].active && frt_mac_eq(g_frt.aps[i].bssid, bssid))
      return &g_frt.aps[i];
  for (int i = 0; i < FRT_MAX_APS; i++) {
    if (!g_frt.aps[i].active) {
      FrtAP &a = g_frt.aps[i];
      memset(&a, 0, sizeof(a));
      a.active   = true;
      memcpy(a.bssid, bssid, 6);
      a.first_ts = millis()/1000;
      g_frt.ap_count++;
      return &a;
    }
  }
  uint32_t oldest = UINT32_MAX; int idx = 0;
  for (int i = 0; i < FRT_MAX_APS; i++)
    if (g_frt.aps[i].last_ts < oldest) { oldest = g_frt.aps[i].last_ts; idx = i; }
  memset(&g_frt.aps[idx], 0, sizeof(g_frt.aps[0]));
  g_frt.aps[idx].active = true;
  memcpy(g_frt.aps[idx].bssid, bssid, 6);
  g_frt.aps[idx].first_ts = millis()/1000;
  return &g_frt.aps[idx];
}

static void frt_tick_ap(FrtAP &a, uint32_t now_s) {
  if (a.accum_ts == now_s) return;
  a.hist[a.hist_head] = a.accum;
  a.hist_head = (a.hist_head + 1) % FRT_HIST_LEN;
  a.current_fps = a.accum.frames;
  if (a.current_fps > a.peak_fps) a.peak_fps = a.current_fps;
  uint32_t sum = 0;
  for (int i = 0; i < FRT_HIST_LEN; i++) sum += a.hist[i].frames;
  a.avg_fps = sum / FRT_HIST_LEN;
  memset(&a.accum, 0, sizeof(a.accum));
  a.accum_ts = now_s;
}

static void frt_tick_ch(FrtChannel &ch, uint32_t now_s) {
  if (ch.accum_ts == now_s) return;
  ch.hist[ch.hist_head] = ch.accum;
  ch.hist_head = (ch.hist_head + 1) % FRT_HIST_LEN;
  ch.current_fps = ch.accum.frames;
  if (ch.current_fps > ch.peak_fps) ch.peak_fps = ch.current_fps;
  memset(&ch.accum, 0, sizeof(ch.accum));
  ch.accum_ts = now_s;
}

void frt_ingest(const uint8_t *bssid, const char *ssid,
                uint8_t channel, int8_t rssi, uint16_t len) {
  uint32_t now_s = millis() / 1000;

  FrtAP *a = frt_find_or_alloc(bssid);
  if (a) {
    frt_tick_ap(*a, now_s);
    if (ssid && ssid[0] && !a->ssid[0]) strncpy(a->ssid, ssid, 32);
    a->channel = channel;
    if (rssi < 0) a->rssi = rssi;
    a->accum.frames++;
    a->accum.bytes += len;
    a->total_frames++;
    a->total_bytes += len;
    a->last_ts = now_s;
  }

  if (channel >= 1 && channel <= FRT_MAX_CHANNELS) {
    FrtChannel &ch = g_frt.channels[channel - 1];
    frt_tick_ch(ch, now_s);
    ch.accum.frames++;
    ch.accum.bytes += len;
    ch.total_frames++;
  }

  g_frt.global_total++;
}

static void frt_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String frt_json_aps() {
  String s = "{\"ap_count\":" + String(g_frt.ap_count);
  s += ",\"global_total\":" + String(g_frt.global_total);
  s += ",\"aps\":[";
  char mac[18]; bool first = true;
  for (int i = 0; i < FRT_MAX_APS; i++) {
    FrtAP &a = g_frt.aps[i];
    if (!a.active) continue;
    if (!first) s += ","; first = false;
    frt_mac_str(a.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(a.ssid) + "\"";
    s += ",\"ch\":" + String(a.channel);
    s += ",\"rssi\":" + String(a.rssi);
    s += ",\"fps\":" + String(a.current_fps);
    s += ",\"avg_fps\":" + String(a.avg_fps);
    s += ",\"peak_fps\":" + String(a.peak_fps);
    s += ",\"total\":" + String(a.total_frames);
    s += ",\"bytes\":" + String(a.total_bytes);
    s += ",\"hist\":[";
    for (int j = 0; j < FRT_HIST_LEN; j++) {
      int pos = (a.hist_head + j) % FRT_HIST_LEN;
      if (j) s += ",";
      s += String(a.hist[pos].frames);
    }
    s += "]}";
  }
  s += "]}";
  return s;
}

static String frt_json_channels() {
  String s = "[";
  for (int i = 0; i < FRT_MAX_CHANNELS; i++) {
    FrtChannel &ch = g_frt.channels[i];
    if (i) s += ",";
    s += "{\"ch\":" + String(i+1);
    s += ",\"fps\":" + String(ch.current_fps);
    s += ",\"peak\":" + String(ch.peak_fps);
    s += ",\"total\":" + String(ch.total_frames);
    s += ",\"hist\":[";
    for (int j = 0; j < FRT_HIST_LEN; j++) {
      int pos = (ch.hist_head + j) % FRT_HIST_LEN;
      if (j) s += ",";
      s += String(ch.hist[pos].frames);
    }
    s += "]}";
  }
  s += "]";
  return s;
}

inline void frt_routes() {
  server.on("/api/frt/aps", HTTP_GET, []() {
    server.send(200, "application/json", frt_json_aps());
  });
  server.on("/api/frt/channels", HTTP_GET, []() {
    server.send(200, "application/json", frt_json_channels());
  });
  server.on("/api/frt/clear", HTTP_POST, []() {
    memset(&g_frt, 0, sizeof(g_frt));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

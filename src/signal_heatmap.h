#pragma once
// signal_heatmap.h — Per-channel RSSI signal heatmap and statistics.
// Tracks signal distribution across channels, AP density, noise floor
// estimates, and per-channel utilization from observed 802.11 frames.
// Prefix: shm_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define SHM_MAX_CHANNELS   14
#define SHM_MAX_APS_PER_CH 16
#define SHM_HIST_BUCKETS   10  // RSSI histogram buckets: -100 to -20 dBm
#define SHM_SAMPLE_HIST    30  // rolling sample history

// RSSI histogram bucket boundaries (in dBm): -100..-90, -90..-80, ..., -30..-20
static const int8_t SHM_BUCKET_LOW[]  = {-100,-90,-80,-70,-65,-60,-55,-50,-45,-40};
static const int8_t SHM_BUCKET_HIGH[] = { -90,-80,-70,-65,-60,-55,-50,-45,-40,-20};
static const char*  SHM_BUCKET_LABEL[] = {
  "<-90","-90/-80","-80/-70","-70/-65","-65/-60",
  "-60/-55","-55/-50","-50/-45","-45/-40",">-40"
};

// Band classification
enum ShmBand : uint8_t {
  SHM_BAND_2G = 0,
  SHM_BAND_5G = 1,
};

static inline ShmBand shm_band(uint8_t ch) {
  return ch <= 14 ? SHM_BAND_2G : SHM_BAND_5G;
}

// AP record for a given channel
struct ShmApRecord {
  bool    active;
  uint8_t bssid[6];
  char    ssid[33];
  int8_t  rssi;
  uint16_t frames;
  uint32_t last_ts;
};

// Per-channel stats
struct ShmChannel {
  bool    has_data;
  uint8_t ch;
  ShmBand band;

  // RSSI distribution
  uint32_t rssi_hist[SHM_HIST_BUCKETS]; // histogram counts
  uint32_t total_frames;
  int8_t   rssi_min;
  int8_t   rssi_max;
  int16_t  rssi_sum;        // accumulated for mean (saturates at 32k)
  uint32_t rssi_count;      // frame count for mean
  int8_t   rssi_mean;

  // Noise floor estimate: lowest observed RSSI in past 60s
  int8_t   noise_floor_dBm;
  uint32_t noise_floor_ts;

  // Utilization: frames per second (rolling 10s window)
  uint16_t frames_per_sec_10s;
  uint16_t fps_samples[10];
  uint8_t  fps_head;
  uint32_t fps_last_ts;
  uint16_t fps_accumulator;

  // AP list
  ShmApRecord aps[SHM_MAX_APS_PER_CH];
  uint8_t  ap_count;

  // Rolling RSSI samples for sparkline
  int8_t   rssi_samples[SHM_SAMPLE_HIST];
  uint8_t  sample_head;
  uint32_t sample_ts;
};

static struct {
  ShmChannel ch[SHM_MAX_CHANNELS + 1]; // index = channel number (1-14 for 2.4G, 36+ not indexed)
  uint32_t   total_frames_all;
  uint32_t   start_ts;
} g_shm = {};

static int shm_ch_idx(uint8_t ch) {
  if (ch >= 1 && ch <= 14) return ch;
  return -1; // 5 GHz not indexed yet
}

static int shm_rssi_bucket(int8_t rssi) {
  for (int i = 0; i < SHM_HIST_BUCKETS; i++) {
    if (rssi >= SHM_BUCKET_LOW[i] && rssi < SHM_BUCKET_HIGH[i])
      return i;
  }
  return SHM_HIST_BUCKETS - 1;
}

static ShmApRecord* shm_find_ap(ShmChannel &c, const uint8_t *bssid) {
  for (int i = 0; i < SHM_MAX_APS_PER_CH; i++)
    if (c.aps[i].active && memcmp(c.aps[i].bssid, bssid, 6) == 0)
      return &c.aps[i];
  return nullptr;
}

static ShmApRecord* shm_alloc_ap(ShmChannel &c, const uint8_t *bssid) {
  for (int i = 0; i < SHM_MAX_APS_PER_CH; i++) {
    if (!c.aps[i].active) {
      memset(&c.aps[i], 0, sizeof(ShmApRecord));
      c.aps[i].active = true;
      memcpy(c.aps[i].bssid, bssid, 6);
      c.ap_count++;
      return &c.aps[i];
    }
  }
  uint32_t oldest = UINT32_MAX; int idx = 0;
  for (int i = 0; i < SHM_MAX_APS_PER_CH; i++)
    if (c.aps[i].last_ts < oldest) { oldest = c.aps[i].last_ts; idx = i; }
  memset(&c.aps[idx], 0, sizeof(ShmApRecord));
  c.aps[idx].active = true;
  memcpy(c.aps[idx].bssid, bssid, 6);
  return &c.aps[idx];
}

static void shm_update_fps(ShmChannel &c) {
  uint32_t now = millis()/1000;
  if (now - c.fps_last_ts >= 1) {
    c.fps_samples[c.fps_head % 10] = c.fps_accumulator;
    c.fps_head = (c.fps_head + 1) % 10;
    c.fps_accumulator = 0;
    c.fps_last_ts = now;

    uint32_t total = 0;
    for (int i = 0; i < 10; i++) total += c.fps_samples[i];
    c.frames_per_sec_10s = (uint16_t)(total / 10);
  }
  c.fps_accumulator++;
}

void shm_ingest(uint8_t ch, int8_t rssi, const uint8_t *bssid, const char *ssid) {
  int idx = shm_ch_idx(ch);
  if (idx < 0) return;

  ShmChannel &c = g_shm.ch[idx];
  if (!c.has_data) {
    c.has_data = true;
    c.ch = ch;
    c.band = shm_band(ch);
    c.rssi_min = rssi;
    c.rssi_max = rssi;
    c.noise_floor_dBm = rssi;
    if (g_shm.start_ts == 0) g_shm.start_ts = millis()/1000;
  }

  // RSSI stats
  int bucket = shm_rssi_bucket(rssi);
  c.rssi_hist[bucket]++;
  c.total_frames++;
  g_shm.total_frames_all++;

  if (rssi < c.rssi_min) c.rssi_min = rssi;
  if (rssi > c.rssi_max) c.rssi_max = rssi;
  c.rssi_sum += rssi; c.rssi_count++;
  if (c.rssi_count > 0) c.rssi_mean = (int8_t)(c.rssi_sum / (int16_t)c.rssi_count);

  // Noise floor: update if lower than current and hasn't updated in 10s
  uint32_t now = millis()/1000;
  if (rssi < c.noise_floor_dBm || (now - c.noise_floor_ts > 10)) {
    if (rssi < c.noise_floor_dBm) {
      c.noise_floor_dBm = rssi;
      c.noise_floor_ts = now;
    }
  }

  // Rolling RSSI sample for sparkline (1 per second)
  if (now - c.sample_ts >= 1) {
    c.rssi_samples[c.sample_head % SHM_SAMPLE_HIST] = c.rssi_mean;
    c.sample_head = (c.sample_head + 1) % SHM_SAMPLE_HIST;
    c.sample_ts = now;
  }

  // FPS tracking
  shm_update_fps(c);

  // AP record
  if (bssid) {
    ShmApRecord *ap = shm_find_ap(c, bssid);
    if (!ap) ap = shm_alloc_ap(c, bssid);
    if (ap) {
      if (ssid) strncpy(ap->ssid, ssid, 32);
      ap->rssi = rssi;
      ap->frames++;
      ap->last_ts = now;
    }
  }
}

static void shm_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String shm_json_all() {
  String s = "{\"total_frames\":" + String(g_shm.total_frames_all);
  s += ",\"uptime\":" + String(millis()/1000 - g_shm.start_ts);
  s += ",\"channels\":[";
  bool first = true;
  for (int idx = 1; idx <= SHM_MAX_CHANNELS; idx++) {
    ShmChannel &c = g_shm.ch[idx];
    if (!c.has_data) continue;
    if (!first) s += ","; first = false;
    s += "{\"ch\":" + String(c.ch);
    s += ",\"band\":" + String(c.band);
    s += ",\"frames\":" + String(c.total_frames);
    s += ",\"fps\":" + String(c.frames_per_sec_10s);
    s += ",\"rssi_min\":" + String(c.rssi_min);
    s += ",\"rssi_max\":" + String(c.rssi_max);
    s += ",\"rssi_mean\":" + String(c.rssi_mean);
    s += ",\"noise_floor\":" + String(c.noise_floor_dBm);
    s += ",\"ap_count\":" + String(c.ap_count);
    s += ",\"hist\":[";
    for (int b = 0; b < SHM_HIST_BUCKETS; b++) {
      if (b) s += ",";
      s += String(c.rssi_hist[b]);
    }
    s += "],\"samples\":[";
    for (int i = 0; i < SHM_SAMPLE_HIST; i++) {
      if (i) s += ",";
      s += String(c.rssi_samples[(c.sample_head + i) % SHM_SAMPLE_HIST]);
    }
    s += "],\"aps\":[";
    char mac[18]; bool afirst = true;
    for (int i = 0; i < SHM_MAX_APS_PER_CH; i++) {
      if (!c.aps[i].active) continue;
      if (!afirst) s += ","; afirst = false;
      shm_mac_str(c.aps[i].bssid, mac);
      s += "{\"bssid\":\"" + String(mac) + "\",\"ssid\":\"" + String(c.aps[i].ssid) + "\"";
      s += ",\"rssi\":" + String(c.aps[i].rssi);
      s += ",\"frames\":" + String(c.aps[i].frames) + "}";
    }
    s += "]}";
  }
  s += "]}";
  return s;
}

inline void shm_routes() {
  server.on("/api/shm/all", HTTP_GET, []() {
    server.send(200, "application/json", shm_json_all());
  });
  server.on("/api/shm/clear", HTTP_POST, []() {
    memset(&g_shm, 0, sizeof(g_shm));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

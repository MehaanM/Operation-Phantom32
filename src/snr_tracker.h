#pragma once
// snr_tracker.h — Signal-to-noise ratio tracker per AP.
// Computes SNR and link quality estimates from RSSI and noise floor data.
// Prefix: snr_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define SNR_MAX_APS    48
#define SNR_HIST_LEN   30  // last 30 seconds of SNR samples

struct SnrAP {
  bool    active;
  uint8_t bssid[6];
  char    ssid[33];
  uint8_t channel;

  int8_t  rssi;
  int8_t  noise_floor;
  int8_t  snr;             // rssi - noise_floor

  // History ring (SNR per second)
  int8_t  snr_hist[SNR_HIST_LEN];
  uint8_t hist_head;

  int8_t  snr_min;
  int8_t  snr_max;
  int8_t  snr_avg;         // EMA

  // Link quality estimate (0-100)
  uint8_t link_quality;

  // Instability: std-dev proxy (max-min difference)
  uint8_t snr_range;

  uint32_t sample_count;
  uint32_t last_ts;
  uint32_t first_ts;
};

static struct {
  SnrAP   aps[SNR_MAX_APS];
  uint8_t ap_count;

  // Global noise floor per channel
  int8_t  ch_noise[14];   // ch 1-14
} g_snr = {};

static bool snr_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static SnrAP* snr_find_or_alloc(const uint8_t *bssid) {
  for (int i = 0; i < SNR_MAX_APS; i++)
    if (g_snr.aps[i].active && snr_mac_eq(g_snr.aps[i].bssid, bssid))
      return &g_snr.aps[i];
  for (int i = 0; i < SNR_MAX_APS; i++) {
    if (!g_snr.aps[i].active) {
      SnrAP &a = g_snr.aps[i];
      memset(&a, 0, sizeof(a));
      a.active = true;
      memcpy(a.bssid, bssid, 6);
      a.snr_min = 127;
      a.snr_max = -128;
      a.snr_avg = 0;
      a.link_quality = 50;
      a.first_ts = millis()/1000;
      g_snr.ap_count++;
      return &a;
    }
  }
  uint32_t oldest = UINT32_MAX; int idx = 0;
  for (int i = 0; i < SNR_MAX_APS; i++)
    if (g_snr.aps[i].last_ts < oldest) { oldest = g_snr.aps[i].last_ts; idx = i; }
  memset(&g_snr.aps[idx], 0, sizeof(g_snr.aps[0]));
  g_snr.aps[idx].active = true;
  memcpy(g_snr.aps[idx].bssid, bssid, 6);
  g_snr.aps[idx].first_ts = millis()/1000;
  return &g_snr.aps[idx];
}

// ─── Link quality formula ─────────────────────────────────────────────────────
// SNR dB → quality %
// SNR >=30 → 100%, 20-29 → 80%, 10-19 → 60%, 5-9 → 40%, <5 → 20%
static uint8_t snr_to_quality(int8_t snr) {
  if (snr >= 30) return 100;
  if (snr >= 25) return 90;
  if (snr >= 20) return 80;
  if (snr >= 15) return 70;
  if (snr >= 10) return 55;
  if (snr >= 5)  return 40;
  return 20;
}

void snr_ingest(const uint8_t *bssid, const char *ssid,
                int8_t rssi, int8_t noise_floor, uint8_t channel) {
  SnrAP *ap = snr_find_or_alloc(bssid);
  if (!ap) return;

  if (ssid && ssid[0]) strncpy(ap->ssid, ssid, 32);
  ap->rssi        = rssi;
  ap->noise_floor = noise_floor;
  ap->channel     = channel;

  int8_t snr = rssi - noise_floor;
  ap->snr = snr;

  // Update history ring
  ap->snr_hist[ap->hist_head] = snr;
  ap->hist_head = (ap->hist_head + 1) % SNR_HIST_LEN;

  // Update min/max
  if (snr < ap->snr_min) ap->snr_min = snr;
  if (snr > ap->snr_max) ap->snr_max = snr;
  ap->snr_range = ap->snr_max - ap->snr_min;

  // EMA (alpha = 0.2)
  ap->snr_avg = (int8_t)((ap->snr_avg * 8 + snr * 2) / 10);

  ap->link_quality = snr_to_quality(snr);
  ap->sample_count++;
  ap->last_ts = millis()/1000;

  // Update global noise floor for channel
  if (channel > 0 && channel <= 14) {
    if (noise_floor < g_snr.ch_noise[channel-1] || g_snr.ch_noise[channel-1] == 0)
      g_snr.ch_noise[channel-1] = noise_floor;
  }
}

static void snr_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String snr_json_all() {
  String s = "{\"ap_count\":" + String(g_snr.ap_count);
  s += ",\"ch_noise\":[";
  for (int i = 0; i < 14; i++) {
    if (i) s += ",";
    s += String(g_snr.ch_noise[i]);
  }
  s += "],\"aps\":[";
  char mac[18];
  bool first = true;
  for (int i = 0; i < SNR_MAX_APS; i++) {
    SnrAP &ap = g_snr.aps[i];
    if (!ap.active) continue;
    if (!first) s += ",";
    first = false;
    snr_mac_str(ap.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(ap.ssid) + "\"";
    s += ",\"ch\":" + String(ap.channel);
    s += ",\"rssi\":" + String(ap.rssi);
    s += ",\"noise\":" + String(ap.noise_floor);
    s += ",\"snr\":" + String(ap.snr);
    s += ",\"snr_avg\":" + String(ap.snr_avg);
    s += ",\"snr_min\":" + String(ap.snr_min);
    s += ",\"snr_max\":" + String(ap.snr_max);
    s += ",\"quality\":" + String(ap.link_quality);
    s += ",\"range\":" + String(ap.snr_range);
    s += ",\"hist\":[";
    for (int j = 0; j < SNR_HIST_LEN; j++) {
      int pos = (ap.hist_head + j) % SNR_HIST_LEN;
      if (j) s += ",";
      s += String(ap.snr_hist[pos]);
    }
    s += "]}";
  }
  s += "]}";
  return s;
}

inline void snr_routes() {
  server.on("/api/snr/all", HTTP_GET, []() {
    server.send(200, "application/json", snr_json_all());
  });
  server.on("/api/snr/clear", HTTP_POST, []() {
    memset(&g_snr, 0, sizeof(g_snr));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

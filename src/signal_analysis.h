#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

// ── Signal Analysis ───────────────────────────────────────────────────────────
//
// RSSI time-series tracking, path-loss modeling, distance estimation,
// signal quality metrics, and spectrum visualization helpers.

// ── RSSI ring buffer ──────────────────────────────────────────────────────────
#define SA_RING_SIZE 64
#define SA_MAX_TRACKS 32

struct SaRssiRing {
  int8_t samples[SA_RING_SIZE];
  uint32_t timestamps[SA_RING_SIZE];
  uint8_t head;
  uint8_t count;
};

static void sa_ring_push(SaRssiRing *r, int8_t rssi, uint32_t ts) {
  r->samples[r->head] = rssi;
  r->timestamps[r->head] = ts;
  r->head = (r->head + 1) % SA_RING_SIZE;
  if (r->count < SA_RING_SIZE)
    r->count++;
}

static float sa_ring_mean(const SaRssiRing *r) {
  if (!r->count)
    return -100.0f;
  int32_t sum = 0;
  for (uint8_t i = 0; i < r->count; i++)
    sum += r->samples[i];
  return (float)sum / r->count;
}

static float sa_ring_stddev(const SaRssiRing *r) {
  if (r->count < 2)
    return 0.0f;
  float mean = sa_ring_mean(r);
  float sq = 0.0f;
  for (uint8_t i = 0; i < r->count; i++) {
    float d = r->samples[i] - mean;
    sq += d * d;
  }
  return sqrtf(sq / (r->count - 1));
}

static int8_t sa_ring_min(const SaRssiRing *r) {
  if (!r->count)
    return -100;
  int8_t m = r->samples[0];
  for (uint8_t i = 1; i < r->count; i++)
    if (r->samples[i] < m)
      m = r->samples[i];
  return m;
}

static int8_t sa_ring_max(const SaRssiRing *r) {
  if (!r->count)
    return -100;
  int8_t m = r->samples[0];
  for (uint8_t i = 1; i < r->count; i++)
    if (r->samples[i] > m)
      m = r->samples[i];
  return m;
}

// ── Exponential moving average ────────────────────────────────────────────────
struct SaEma {
  float value;
  float alpha;  // 0 < alpha <= 1; smaller = more smoothing
  bool initialized;
};

static void sa_ema_init(SaEma *e, float alpha) {
  e->value = 0.0f;
  e->alpha = alpha;
  e->initialized = false;
}

static float sa_ema_update(SaEma *e, float sample) {
  if (!e->initialized) {
    e->value = sample;
    e->initialized = true;
  } else
    e->value = e->alpha * sample + (1.0f - e->alpha) * e->value;
  return e->value;
}

// ── Path-loss distance models ─────────────────────────────────────────────────
// Free-space path loss model: d = 10^((Ptx - RSSI - 20*log10(f) + 147.55) / 20)
// Simplified 2.4 GHz: d = 10^((A - RSSI) / (10*n))
//   A = reference RSSI at 1m (~-40 dBm indoors)
//   n = path loss exponent (2.0 free space, 2.7-3.5 indoor)

static float sa_distance_m(int8_t rssi, int8_t rssi_1m, float n) {
  if (n <= 0.0f)
    n = 2.7f;
  if (rssi_1m == 0)
    rssi_1m = -40;
  float exp = (float)(rssi_1m - rssi) / (10.0f * n);
  return powf(10.0f, exp);
}

static float sa_distance_indoor(int8_t rssi) {
  return sa_distance_m(rssi, -40, 2.8f);
}

static float sa_distance_outdoor(int8_t rssi) {
  return sa_distance_m(rssi, -30, 2.1f);
}

// ── Link budget helpers ───────────────────────────────────────────────────────
// SNR estimate: rssi - noise_floor
static int8_t sa_snr(int8_t rssi, int8_t noise) {
  return rssi > noise ? rssi - noise : 0;
}

// MCS index estimate from RSSI (802.11n 20 MHz)
static uint8_t sa_mcs_estimate(int8_t rssi) {
  if (rssi >= -65)
    return 7;  // 65 Mbps
  if (rssi >= -68)
    return 6;
  if (rssi >= -71)
    return 5;
  if (rssi >= -74)
    return 4;
  if (rssi >= -77)
    return 3;
  if (rssi >= -80)
    return 2;
  if (rssi >= -83)
    return 1;
  return 0;  // 6.5 Mbps
}

static const float SA_MCS_RATE_20[] = {6.5f, 13.0f, 19.5f, 26.0f, 39.0f, 52.0f, 58.5f, 65.0f};

static float sa_rate_estimate_mbps(int8_t rssi, bool ht) {
  if (!ht) {
    if (rssi >= -65)
      return 54.0f;
    if (rssi >= -70)
      return 48.0f;
    if (rssi >= -73)
      return 36.0f;
    if (rssi >= -78)
      return 24.0f;
    if (rssi >= -82)
      return 18.0f;
    if (rssi >= -85)
      return 12.0f;
    if (rssi >= -88)
      return 9.0f;
    return 6.0f;
  }
  uint8_t mcs = sa_mcs_estimate(rssi);
  return SA_MCS_RATE_20[mcs];
}

// ── RSSI to signal bar (0-5) ─────────────────────────────────────────────────
static uint8_t sa_signal_bars(int8_t rssi) {
  if (rssi >= -50)
    return 5;
  if (rssi >= -60)
    return 4;
  if (rssi >= -70)
    return 3;
  if (rssi >= -80)
    return 2;
  if (rssi >= -90)
    return 1;
  return 0;
}

// ── RSSI trend (rising/falling/stable) ───────────────────────────────────────
enum SaTrend : uint8_t {
  SA_TREND_STABLE = 0,
  SA_TREND_RISING = 1,
  SA_TREND_FALLING = 2,
};

static SaTrend sa_trend(const SaRssiRing *r) {
  if (r->count < 4)
    return SA_TREND_STABLE;
  // Compare last 2 average vs previous 2
  int32_t sum_new = 0, sum_old = 0;
  uint8_t n = r->count;
  for (int i = 0; i < 2; i++) {
    int idx = (r->head - 1 - i + SA_RING_SIZE) % SA_RING_SIZE;
    sum_new += r->samples[idx];
  }
  for (int i = 2; i < 4; i++) {
    int idx = (r->head - 1 - i + SA_RING_SIZE) % SA_RING_SIZE;
    sum_old += r->samples[idx];
  }
  int diff = (sum_new / 2) - (sum_old / 2);
  if (diff >= 3)
    return SA_TREND_RISING;
  if (diff <= -3)
    return SA_TREND_FALLING;
  return SA_TREND_STABLE;
}

static const char *sa_trend_name(SaTrend t) {
  switch (t) {
    case SA_TREND_RISING:
      return "rising";
    case SA_TREND_FALLING:
      return "falling";
    default:
      return "stable";
  }
}

// ── Signal track (per BSSID) ──────────────────────────────────────────────────
struct SaTrack {
  uint8_t bssid[6];
  SaRssiRing ring;
  SaEma ema;
  int8_t rssi_last;
  float distance_m;
  SaTrend trend;
  uint32_t last_update_ms;
  bool active;
};

static SaTrack g_sa_tracks[SA_MAX_TRACKS];
static uint8_t g_sa_track_count = 0;

// ── Track upsert ──────────────────────────────────────────────────────────────
static SaTrack *sa_track_upsert(const uint8_t bssid[6]) {
  for (uint8_t i = 0; i < g_sa_track_count; i++) {
    if (memcmp(g_sa_tracks[i].bssid, bssid, 6) == 0)
      return &g_sa_tracks[i];
  }
  if (g_sa_track_count >= SA_MAX_TRACKS) {
    // Evict least recently updated
    uint32_t oldest = g_sa_tracks[0].last_update_ms;
    uint8_t oi = 0;
    for (uint8_t i = 1; i < g_sa_track_count; i++) {
      if (g_sa_tracks[i].last_update_ms < oldest) {
        oldest = g_sa_tracks[i].last_update_ms;
        oi = i;
      }
    }
    memset(&g_sa_tracks[oi], 0, sizeof(SaTrack));
    memcpy(g_sa_tracks[oi].bssid, bssid, 6);
    sa_ema_init(&g_sa_tracks[oi].ema, 0.3f);
    return &g_sa_tracks[oi];
  }
  SaTrack *t = &g_sa_tracks[g_sa_track_count++];
  memset(t, 0, sizeof(*t));
  memcpy(t->bssid, bssid, 6);
  sa_ema_init(&t->ema, 0.3f);
  t->active = true;
  return t;
}

// ── Update track with new RSSI sample ─────────────────────────────────────────
static void sa_update(const uint8_t bssid[6], int8_t rssi) {
  SaTrack *t = sa_track_upsert(bssid);
  uint32_t now = millis();
  sa_ring_push(&t->ring, rssi, now);
  sa_ema_update(&t->ema, (float)rssi);
  t->rssi_last = rssi;
  t->distance_m = sa_distance_indoor(rssi);
  t->trend = sa_trend(&t->ring);
  t->last_update_ms = now;
  t->active = true;
}

// ── Spectrum histogram (per 5 dBm bucket) ────────────────────────────────────
#define SA_SPEC_BUCKETS 16

struct SaSpectrum {
  int8_t bucket_low[SA_SPEC_BUCKETS];
  uint32_t counts[SA_SPEC_BUCKETS];
  uint32_t total;
};

static SaSpectrum g_sa_spec = {};

static void sa_spec_init() {
  for (int i = 0; i < SA_SPEC_BUCKETS; i++)
    g_sa_spec.bucket_low[i] = -100 + i * 5;
}

static void sa_spec_record(int8_t rssi) {
  int idx = (rssi + 100) / 5;
  if (idx < 0)
    idx = 0;
  if (idx >= SA_SPEC_BUCKETS)
    idx = SA_SPEC_BUCKETS - 1;
  g_sa_spec.counts[idx]++;
  g_sa_spec.total++;
}

// ── Track JSON ────────────────────────────────────────────────────────────────
static String sa_tracks_json() {
  String j = "{\"count\":" + String(g_sa_track_count) + ",\"tracks\":[";
  for (uint8_t i = 0; i < g_sa_track_count; i++) {
    if (i)
      j += ",";
    SaTrack &t = g_sa_tracks[i];
    char mac[18];
    snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X", t.bssid[0], t.bssid[1], t.bssid[2],
             t.bssid[3], t.bssid[4], t.bssid[5]);
    float mean = sa_ring_mean(&t.ring);
    float sd = sa_ring_stddev(&t.ring);
    j += "{\"mac\":\"";
    j += mac;
    j += "\"";
    j += ",\"rssi\":";
    j += t.rssi_last;
    j += ",\"mean\":";
    j += (int)mean;
    j += ",\"stddev\":";
    j += (int)(sd * 10);  // ×10 to avoid float in JSON
    j += ",\"dist_cm\":";
    j += (int)(t.distance_m * 100);
    j += ",\"bars\":";
    j += sa_signal_bars(t.rssi_last);
    j += ",\"trend\":\"";
    j += sa_trend_name(t.trend);
    j += "\"";
    j += ",\"samples\":";
    j += t.ring.count;
    j += ",\"ema\":";
    j += (int)t.ema.value;
    j += "}";
  }
  j += "]}";
  return j;
}

// ── Spectrum JSON ─────────────────────────────────────────────────────────────
static String sa_spectrum_json() {
  String j = "{\"total\":" + String(g_sa_spec.total) + ",\"buckets\":[";
  for (int i = 0; i < SA_SPEC_BUCKETS; i++) {
    if (i)
      j += ",";
    j += "{\"low\":";
    j += g_sa_spec.bucket_low[i];
    j += ",\"count\":";
    j += g_sa_spec.counts[i];
    if (g_sa_spec.total) {
      j += ",\"pct\":";
      j += (int)(g_sa_spec.counts[i] * 100 / g_sa_spec.total);
    }
    j += "}";
  }
  j += "]}";
  return j;
}

// ── RSSI to JSON for single device ───────────────────────────────────────────
static String sa_device_json(const uint8_t bssid[6]) {
  for (uint8_t i = 0; i < g_sa_track_count; i++) {
    if (memcmp(g_sa_tracks[i].bssid, bssid, 6) == 0) {
      SaTrack &t = g_sa_tracks[i];
      char mac[18];
      snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X", t.bssid[0], t.bssid[1],
               t.bssid[2], t.bssid[3], t.bssid[4], t.bssid[5]);
      float mean = sa_ring_mean(&t.ring);
      float sd = sa_ring_stddev(&t.ring);
      String j = "{\"mac\":\"";
      j += mac;
      j += "\"";
      j += ",\"rssi\":";
      j += t.rssi_last;
      j += ",\"mean\":";
      j += (int)mean;
      j += ",\"min\":";
      j += sa_ring_min(&t.ring);
      j += ",\"max\":";
      j += sa_ring_max(&t.ring);
      j += ",\"stddev\":";
      j += (int)(sd * 10);
      j += ",\"ema\":";
      j += (int)t.ema.value;
      j += ",\"dist_m\":";
      j += (int)t.distance_m;
      j += ",\"bars\":";
      j += sa_signal_bars(t.rssi_last);
      j += ",\"trend\":\"";
      j += sa_trend_name(t.trend);
      j += "\"";
      j += ",\"samples\":";
      j += t.ring.count;
      j += "}";
      return j;
    }
  }
  return "{}";
}

// ── HTTP routes ───────────────────────────────────────────────────────────────
static void sa_routes() {
  extern WebServer server;

  server.on("/api/sa/tracks", HTTP_GET,
            []() { server.send(200, "application/json", sa_tracks_json()); });

  server.on("/api/sa/spectrum", HTTP_GET,
            []() { server.send(200, "application/json", sa_spectrum_json()); });

  server.on("/api/sa/device", HTTP_GET, []() {
    if (!server.hasArg("mac")) {
      server.send(400, "application/json", "{\"error\":\"mac required\"}");
      return;
    }
    uint8_t mac[6];
    unsigned m[6];
    if (sscanf(server.arg("mac").c_str(), "%x:%x:%x:%x:%x:%x", &m[0], &m[1], &m[2], &m[3], &m[4],
               &m[5]) != 6) {
      server.send(400, "application/json", "{\"error\":\"invalid mac\"}");
      return;
    }
    for (int i = 0; i < 6; i++)
      mac[i] = (uint8_t)m[i];
    server.send(200, "application/json", sa_device_json(mac));
  });

  server.on("/api/sa/distance", HTTP_GET, []() {
    if (!server.hasArg("rssi")) {
      server.send(400, "application/json", "{\"error\":\"rssi required\"}");
      return;
    }
    int8_t rssi = (int8_t)server.arg("rssi").toInt();
    float di = sa_distance_indoor(rssi);
    float dout = sa_distance_outdoor(rssi);
    char buf[128];
    snprintf(buf, sizeof(buf), "{\"rssi\":%d,\"indoor_m\":%.1f,\"outdoor_m\":%.1f,\"bars\":%d}",
             rssi, di, dout, sa_signal_bars(rssi));
    server.send(200, "application/json", buf);
  });

  server.on("/api/sa/mcs", HTTP_GET, []() {
    if (!server.hasArg("rssi")) {
      server.send(400, "application/json", "{\"error\":\"rssi required\"}");
      return;
    }
    int8_t rssi = (int8_t)server.arg("rssi").toInt();
    uint8_t mcs = sa_mcs_estimate(rssi);
    float rate_ht = SA_MCS_RATE_20[mcs];
    float rate_legacy = sa_rate_estimate_mbps(rssi, false);
    char buf[128];
    snprintf(buf, sizeof(buf), "{\"rssi\":%d,\"mcs\":%d,\"rate_ht\":%.1f,\"rate_legacy\":%.1f}",
             rssi, mcs, rate_ht, rate_legacy);
    server.send(200, "application/json", buf);
  });
}

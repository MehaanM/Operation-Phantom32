#pragma once
// freq_hopper.h — Intelligent channel-hopping with dwell-time control.
// Smart hopping based on AP density, activity, and user-defined priorities.
// Prefix: fhp_

#include <Arduino.h>
#include <WebServer.h>
#include <esp_wifi.h>

extern WebServer server;

#define FHP_MAX_CHANNELS  13
#define FHP_HIST_LEN      13   // last N dwells per channel

struct FhpChannelWeight {
  uint8_t  ch;
  float    weight;         // 0.0–1.0, higher = more time
  uint32_t ap_count;
  uint32_t frame_count;
  uint32_t dwell_ms;       // current dwell time (ms)
  uint32_t dwell_count;    // total times dwelled
  uint32_t total_ms;       // total time spent on channel
  uint8_t  activity_hist[FHP_HIST_LEN]; // frame counts per last N dwells
  uint8_t  hist_head;
};

enum FhpMode : uint8_t {
  FHP_MODE_SEQUENTIAL  = 0,  // 1→2→3...→13
  FHP_MODE_NON_OVERLAP = 1,  // 1→6→11 (2.4 GHz non-overlapping)
  FHP_MODE_ADAPTIVE    = 2,  // weight-based: more time on active channels
  FHP_MODE_FIXED       = 3,  // stay on a single channel
};

static struct {
  FhpChannelWeight channels[FHP_MAX_CHANNELS];
  uint8_t  current_ch;
  uint8_t  current_idx;    // index into hop sequence
  uint32_t last_hop_ms;    // millis() of last hop
  uint32_t current_dwell;  // ms to stay on current channel
  uint32_t total_hops;
  uint32_t total_dwell_ms;

  FhpMode  mode;
  uint8_t  base_dwell_ms;  // default dwell per channel (ms, /10 stored)
  uint8_t  min_dwell_ms_x10;  // minimum dwell (ms/10)
  uint8_t  max_dwell_ms_x10;  // maximum dwell (ms/10)

  uint8_t  hop_seq[FHP_MAX_CHANNELS];
  uint8_t  hop_seq_len;

  bool     active;
} g_fhp = {};

static const uint8_t FHP_NON_OVERLAP[] = {1, 6, 11};
static const uint8_t FHP_ALL_CHANNELS[] = {1,2,3,4,5,6,7,8,9,10,11,12,13};

static void fhp_build_sequence() {
  if (g_fhp.mode == FHP_MODE_NON_OVERLAP) {
    memcpy(g_fhp.hop_seq, FHP_NON_OVERLAP, 3);
    g_fhp.hop_seq_len = 3;
  } else if (g_fhp.mode == FHP_MODE_SEQUENTIAL || g_fhp.mode == FHP_MODE_ADAPTIVE) {
    memcpy(g_fhp.hop_seq, FHP_ALL_CHANNELS, FHP_MAX_CHANNELS);
    g_fhp.hop_seq_len = FHP_MAX_CHANNELS;
  } else {
    g_fhp.hop_seq[0] = g_fhp.current_ch;
    g_fhp.hop_seq_len = 1;
  }
}

void fhp_init(FhpMode mode, uint8_t base_dwell_ms, uint8_t first_ch) {
  memset(&g_fhp, 0, sizeof(g_fhp));
  g_fhp.mode = mode;
  g_fhp.base_dwell_ms = base_dwell_ms;
  g_fhp.min_dwell_ms_x10 = 1;    // 10ms minimum
  g_fhp.max_dwell_ms_x10 = 50;   // 500ms maximum
  g_fhp.current_ch = first_ch ? first_ch : 1;
  g_fhp.active = true;

  for (int i = 0; i < FHP_MAX_CHANNELS; i++) {
    g_fhp.channels[i].ch = i + 1;
    g_fhp.channels[i].weight = 1.0f / FHP_MAX_CHANNELS;
    g_fhp.channels[i].dwell_ms = base_dwell_ms;
  }

  fhp_build_sequence();
  esp_wifi_set_channel(g_fhp.current_ch, WIFI_SECOND_CHAN_NONE);
  g_fhp.last_hop_ms = millis();
  g_fhp.current_dwell = base_dwell_ms;
}

void fhp_update_ap_count(uint8_t ch, uint32_t ap_count) {
  if (ch < 1 || ch > FHP_MAX_CHANNELS) return;
  g_fhp.channels[ch-1].ap_count = ap_count;
}

void fhp_ingest_frame(uint8_t ch) {
  if (ch < 1 || ch > FHP_MAX_CHANNELS) return;
  g_fhp.channels[ch-1].frame_count++;
}

static void fhp_reweight() {
  float total = 0.0f;
  for (int i = 0; i < FHP_MAX_CHANNELS; i++) {
    FhpChannelWeight &c = g_fhp.channels[i];
    // Weight = AP count + recent activity (last dwell frames)
    float score = (float)c.ap_count + (float)c.activity_hist[(c.hist_head + FHP_HIST_LEN - 1) % FHP_HIST_LEN] * 0.5f;
    if (score < 0.1f) score = 0.1f;
    c.weight = score;
    total += score;
  }
  if (total > 0) {
    for (int i = 0; i < FHP_MAX_CHANNELS; i++)
      g_fhp.channels[i].weight /= total;
  }

  // Assign dwell times proportional to weights
  uint32_t base = g_fhp.base_dwell_ms;
  for (int i = 0; i < FHP_MAX_CHANNELS; i++) {
    FhpChannelWeight &c = g_fhp.channels[i];
    uint32_t dw = (uint32_t)(base * FHP_MAX_CHANNELS * c.weight);
    dw = max((uint32_t)(g_fhp.min_dwell_ms_x10 * 10), min((uint32_t)(g_fhp.max_dwell_ms_x10 * 10), dw));
    c.dwell_ms = dw;
  }
}

bool fhp_tick() {
  if (!g_fhp.active || g_fhp.hop_seq_len == 0) return false;

  uint32_t now = millis();
  if (now - g_fhp.last_hop_ms < g_fhp.current_dwell) return false;

  FhpChannelWeight &cur = g_fhp.channels[g_fhp.current_ch - 1];
  cur.total_ms += (now - g_fhp.last_hop_ms);
  cur.dwell_count++;
  cur.activity_hist[cur.hist_head] = (uint8_t)min(255u, (unsigned)cur.frame_count);
  cur.hist_head = (cur.hist_head + 1) % FHP_HIST_LEN;
  cur.frame_count = 0;

  g_fhp.total_hops++;
  g_fhp.total_dwell_ms += (now - g_fhp.last_hop_ms);

  if (g_fhp.mode == FHP_MODE_ADAPTIVE) fhp_reweight();

  g_fhp.current_idx = (g_fhp.current_idx + 1) % g_fhp.hop_seq_len;
  uint8_t next_ch = g_fhp.hop_seq[g_fhp.current_idx];
  g_fhp.current_ch = next_ch;

  FhpChannelWeight &nc = g_fhp.channels[next_ch - 1];
  g_fhp.current_dwell = nc.dwell_ms ? nc.dwell_ms : g_fhp.base_dwell_ms;
  g_fhp.last_hop_ms = now;

  esp_wifi_set_channel(next_ch, WIFI_SECOND_CHAN_NONE);
  return true;
}

static String fhp_json_status() {
  static const char* const mode_str[] = {"sequential","non-overlap","adaptive","fixed"};
  String s = "{\"active\":" + String(g_fhp.active?"true":"false");
  s += ",\"mode\":\"" + String(mode_str[g_fhp.mode]) + "\"";
  s += ",\"current_ch\":" + String(g_fhp.current_ch);
  s += ",\"total_hops\":" + String(g_fhp.total_hops);
  s += ",\"total_dwell_ms\":" + String(g_fhp.total_dwell_ms);
  s += ",\"hop_seq_len\":" + String(g_fhp.hop_seq_len);
  s += ",\"channels\":[";
  for (int i = 0; i < FHP_MAX_CHANNELS; i++) {
    FhpChannelWeight &c = g_fhp.channels[i];
    if (i) s += ",";
    s += "{\"ch\":" + String(c.ch);
    s += ",\"weight\":" + String(c.weight, 4);
    s += ",\"dwell_ms\":" + String(c.dwell_ms);
    s += ",\"aps\":" + String(c.ap_count);
    s += ",\"total_ms\":" + String(c.total_ms);
    s += ",\"dwells\":" + String(c.dwell_count);
    s += ",\"frames\":" + String(c.frame_count);
    s += "}";
  }
  s += "]}";
  return s;
}

inline void fhp_routes() {
  server.on("/api/fhp/status", HTTP_GET, []() {
    server.send(200, "application/json", fhp_json_status());
  });
  server.on("/api/fhp/hop", HTTP_POST, []() {
    fhp_tick();
    server.send(200, "application/json", "{\"ch\":" + String(g_fhp.current_ch) + "}");
  });
  server.on("/api/fhp/set_mode", HTTP_POST, []() {
    if (server.hasArg("mode")) {
      uint8_t m = server.arg("mode").toInt();
      if (m <= 3) {
        g_fhp.mode = (FhpMode)m;
        fhp_build_sequence();
        g_fhp.current_idx = 0;
      }
    }
    if (server.hasArg("dwell")) {
      g_fhp.base_dwell_ms = server.arg("dwell").toInt();
    }
    server.send(200, "application/json", "{\"ok\":true}");
  });
  server.on("/api/fhp/set_channel", HTTP_POST, []() {
    if (server.hasArg("ch")) {
      uint8_t ch = server.arg("ch").toInt();
      if (ch >= 1 && ch <= 13) {
        g_fhp.mode = FHP_MODE_FIXED;
        g_fhp.current_ch = ch;
        g_fhp.hop_seq[0] = ch;
        g_fhp.hop_seq_len = 1;
        esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
      }
    }
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

#pragma once
// channel_hopper.h — Advanced channel hopping engine with multiple strategies.
// Supports sequential, random, weighted, dwell-time-adaptive, and
// target-lock hopping modes. Tracks per-channel activity for smart dwell.
// Prefix: chop_

#include <Arduino.h>
#include <WebServer.h>
#include <esp_wifi.h>

extern WebServer server;

// ─── Hop strategies ───────────────────────────────────────────────────────────
enum ChopStrategy {
  CHOP_SEQ = 0,          // 1→2→3→…→13→14→1
  CHOP_RANDOM,           // random channel each hop
  CHOP_WEIGHTED,         // hop more on busy channels
  CHOP_ADAPTIVE,         // dwell longer on active channels
  CHOP_1_6_11,           // North American non-overlapping
  CHOP_1_6_11_36_40,     // 2.4+5 GHz combo
  CHOP_CUSTOM,           // user-defined list
  CHOP_TARGET_LOCK,      // stay on one channel
  CHOP_5GHZ_ONLY,        // 5 GHz channels
};

// ─── State ────────────────────────────────────────────────────────────────────
#define CHOP_MAX_CUSTOM    32
#define CHOP_HISTORY       64

static const uint8_t CHOP_NON_OVERLAP_24[]  = {1, 6, 11};
static const uint8_t CHOP_5GHZ_CHANNELS[]   = {
  36,40,44,48,52,56,60,64,100,104,108,112,116,120,124,128,132,136,140,149,153,157,161,165
};
#define CHOP_5GHZ_COUNT    24

struct ChopChannelStats {
  uint32_t packets;
  uint32_t dwell_ms_total;
  uint32_t visits;
  uint32_t last_visit_ts;
  int8_t   avg_rssi;
  float    weight;
};

static struct {
  ChopStrategy strategy;
  bool     running;
  uint8_t  current_channel;
  uint8_t  target_channel;   // for TARGET_LOCK
  uint32_t dwell_ms;         // base dwell time
  uint32_t min_dwell_ms;
  uint32_t max_dwell_ms;
  uint32_t last_hop_ts;
  uint32_t total_hops;
  uint32_t hop_idx;          // position in current list
  uint8_t  custom_channels[CHOP_MAX_CUSTOM];
  uint8_t  custom_count;
  ChopChannelStats stats[14 + CHOP_5GHZ_COUNT]; // 2.4 + 5GHz
  uint8_t  history[CHOP_HISTORY];
  uint8_t  hist_idx;
  TaskHandle_t task_handle;
  uint32_t packets_this_dwell; // used by adaptive
} g_chop = {};

// ─── Channel → stats index ────────────────────────────────────────────────────
static int chop_ch_to_idx(uint8_t ch) {
  if (ch >= 1 && ch <= 14) return ch - 1;
  for (int i = 0; i < CHOP_5GHZ_COUNT; i++) {
    if (CHOP_5GHZ_CHANNELS[i] == ch) return 14 + i;
  }
  return -1;
}

// ─── Normalize weights ────────────────────────────────────────────────────────
static void chop_normalize_weights() {
  float total = 0;
  for (int i = 0; i < 14; i++) total += g_chop.stats[i].packets + 1;
  for (int i = 0; i < 14; i++) g_chop.stats[i].weight = (g_chop.stats[i].packets + 1) / total;
}

// ─── Next channel selection ───────────────────────────────────────────────────
static uint8_t chop_next_channel() {
  switch (g_chop.strategy) {
    case CHOP_SEQ:
      g_chop.hop_idx = (g_chop.hop_idx + 1) % 13; // ch 1-13
      return (uint8_t)(g_chop.hop_idx + 1);

    case CHOP_RANDOM: {
      uint8_t rnd;
      esp_fill_random(&rnd, 1);
      return (rnd % 13) + 1;
    }

    case CHOP_WEIGHTED: {
      chop_normalize_weights();
      uint8_t rnd;
      esp_fill_random(&rnd, 1);
      float r = (float)rnd / 255.0f;
      float cumulative = 0;
      for (int i = 0; i < 13; i++) {
        cumulative += g_chop.stats[i].weight;
        if (r <= cumulative) return (uint8_t)(i + 1);
      }
      return 6;
    }

    case CHOP_1_6_11:
      g_chop.hop_idx = (g_chop.hop_idx + 1) % 3;
      return CHOP_NON_OVERLAP_24[g_chop.hop_idx];

    case CHOP_1_6_11_36_40: {
      static const uint8_t list[] = {1, 6, 11, 36, 40};
      g_chop.hop_idx = (g_chop.hop_idx + 1) % 5;
      return list[g_chop.hop_idx];
    }

    case CHOP_CUSTOM:
      if (g_chop.custom_count == 0) return 1;
      g_chop.hop_idx = (g_chop.hop_idx + 1) % g_chop.custom_count;
      return g_chop.custom_channels[g_chop.hop_idx];

    case CHOP_TARGET_LOCK:
      return g_chop.target_channel;

    case CHOP_5GHZ_ONLY:
      g_chop.hop_idx = (g_chop.hop_idx + 1) % CHOP_5GHZ_COUNT;
      return CHOP_5GHZ_CHANNELS[g_chop.hop_idx];

    case CHOP_ADAPTIVE: {
      // Stay longer on active channels
      int best = 0;
      uint32_t best_pkt = 0;
      for (int i = 0; i < 13; i++) {
        if (g_chop.stats[i].packets > best_pkt) {
          best_pkt = g_chop.stats[i].packets;
          best = i;
        }
      }
      // Mix: 70% most-active, 30% least-visited
      uint8_t rnd;
      esp_fill_random(&rnd, 1);
      if (rnd < 180) return (uint8_t)(best + 1);
      // Find least visited
      int least = 0;
      uint32_t least_v = 0xFFFFFFFF;
      for (int i = 0; i < 13; i++) {
        if (g_chop.stats[i].visits < least_v) {
          least_v = g_chop.stats[i].visits;
          least = i;
        }
      }
      return (uint8_t)(least + 1);
    }

    default:
      return 1;
  }
}

// ─── Adaptive dwell time ─────────────────────────────────────────────────────
static uint32_t chop_dwell_for(uint8_t ch) {
  if (g_chop.strategy != CHOP_ADAPTIVE) return g_chop.dwell_ms;
  int idx = chop_ch_to_idx(ch);
  if (idx < 0) return g_chop.dwell_ms;
  uint32_t pkts = g_chop.stats[idx].packets;
  // Scale: 0 pkts → min_dwell, many pkts → max_dwell
  uint32_t dwell = g_chop.min_dwell_ms + (uint32_t)((float)(g_chop.max_dwell_ms - g_chop.min_dwell_ms) *
    min(1.0f, (float)pkts / 100.0f));
  return dwell;
}

// ─── Hop now ─────────────────────────────────────────────────────────────────
static void chop_do_hop() {
  // Record stats for current channel
  int idx = chop_ch_to_idx(g_chop.current_channel);
  if (idx >= 0) {
    g_chop.stats[idx].dwell_ms_total += millis() - g_chop.last_hop_ts;
    g_chop.stats[idx].packets += g_chop.packets_this_dwell;
    g_chop.stats[idx].visits++;
    g_chop.stats[idx].last_visit_ts = millis() / 1000;
  }
  g_chop.packets_this_dwell = 0;

  uint8_t next = chop_next_channel();
  wifi_second_chan_t sec = WIFI_SECOND_CHAN_NONE;
  esp_wifi_set_channel(next, sec);

  g_chop.history[g_chop.hist_idx % CHOP_HISTORY] = next;
  g_chop.hist_idx++;
  g_chop.current_channel = next;
  g_chop.last_hop_ts = millis();
  g_chop.total_hops++;
}

// ─── Hopper task ─────────────────────────────────────────────────────────────
static void chop_task(void *arg) {
  while (g_chop.running) {
    uint32_t dwell = chop_dwell_for(g_chop.current_channel);
    vTaskDelay(pdMS_TO_TICKS(dwell));
    if (g_chop.running) chop_do_hop();
  }
  g_chop.task_handle = nullptr;
  vTaskDelete(nullptr);
}

// ─── Packet counter (call from promiscuous callback) ─────────────────────────
inline void chop_count_packet() { g_chop.packets_this_dwell++; }
inline uint8_t chop_current_channel() { return g_chop.current_channel; }

// ─── Control ─────────────────────────────────────────────────────────────────
inline void chop_start(ChopStrategy strat, uint32_t dwell_ms = 200) {
  if (g_chop.running) return;
  g_chop.strategy = strat;
  g_chop.dwell_ms = dwell_ms < 50 ? 50 : dwell_ms;
  g_chop.min_dwell_ms = 100;
  g_chop.max_dwell_ms = 2000;
  g_chop.running = true;
  g_chop.total_hops = 0;
  g_chop.last_hop_ts = millis();
  xTaskCreate(chop_task, "chop_task", 2048, nullptr, 6, &g_chop.task_handle);
}

inline void chop_stop() {
  g_chop.running = false;
  uint32_t deadline = millis() + 500;
  while (g_chop.task_handle && millis() < deadline) vTaskDelay(10);
}

static String chop_json_status() {
  String s = "{\"running\":";
  s += g_chop.running ? "true" : "false";
  s += ",\"strategy\":" + String(g_chop.strategy);
  s += ",\"current_ch\":" + String(g_chop.current_channel);
  s += ",\"dwell_ms\":" + String(g_chop.dwell_ms);
  s += ",\"total_hops\":" + String(g_chop.total_hops);
  s += ",\"channel_stats\":[";
  for (int i = 0; i < 14; i++) {
    if (i) s += ",";
    s += "{\"ch\":" + String(i + 1);
    s += ",\"pkts\":" + String(g_chop.stats[i].packets);
    s += ",\"visits\":" + String(g_chop.stats[i].visits);
    s += ",\"dwell_total_ms\":" + String(g_chop.stats[i].dwell_ms_total);
    s += "}";
  }
  s += "],\"history\":[";
  int hist_total = min((int)g_chop.hist_idx, CHOP_HISTORY);
  for (int i = 0; i < hist_total; i++) {
    if (i) s += ",";
    int idx = (g_chop.hist_idx - 1 - i + CHOP_HISTORY) % CHOP_HISTORY;
    s += String(g_chop.history[idx]);
  }
  s += "]}";
  return s;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void chop_routes() {
  server.on("/api/chop/status", HTTP_GET, []() {
    server.send(200, "application/json", chop_json_status());
  });

  server.on("/api/chop/start", HTTP_POST, []() {
    int strat = server.hasArg("strategy") ? server.arg("strategy").toInt() : CHOP_SEQ;
    int dwell = server.hasArg("dwell") ? server.arg("dwell").toInt() : 200;
    if (server.hasArg("target")) g_chop.target_channel = server.arg("target").toInt();
    chop_start((ChopStrategy)strat, dwell);
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/chop/stop", HTTP_POST, []() {
    chop_stop();
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/chop/channel", HTTP_POST, []() {
    if (!server.hasArg("ch")) { server.send(400, "application/json", "{\"error\":\"missing ch\"}"); return; }
    uint8_t ch = server.arg("ch").toInt();
    wifi_second_chan_t sec = WIFI_SECOND_CHAN_NONE;
    esp_wifi_set_channel(ch, sec);
    g_chop.current_channel = ch;
    server.send(200, "application/json", "{\"ok\":true,\"ch\":" + String(ch) + "}");
  });

  server.on("/api/chop/custom/add", HTTP_POST, []() {
    if (!server.hasArg("ch")) { server.send(400, "application/json", "{\"error\":\"missing ch\"}"); return; }
    if (g_chop.custom_count >= CHOP_MAX_CUSTOM) { server.send(400, "application/json", "{\"error\":\"list full\"}"); return; }
    g_chop.custom_channels[g_chop.custom_count++] = server.arg("ch").toInt();
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/chop/custom/clear", HTTP_POST, []() {
    g_chop.custom_count = 0;
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/chop/dwell", HTTP_POST, []() {
    if (server.hasArg("ms"))    g_chop.dwell_ms     = server.arg("ms").toInt();
    if (server.hasArg("min"))   g_chop.min_dwell_ms = server.arg("min").toInt();
    if (server.hasArg("max"))   g_chop.max_dwell_ms = server.arg("max").toInt();
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

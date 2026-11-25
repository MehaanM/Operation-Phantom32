#pragma once
// bss_color.h — 802.11ax (Wi-Fi 6) BSS Color tracker.
// Tracks BSS Color assignments, color collision detection,
// spatial reuse parameters, and OBSS/PD thresholds.
// Prefix: bsc_

#include <Arduino.h>
#include <WebServer.h>
#include "psram_attr.h"

extern WebServer server;

#define BSC_MAX_APS 32
#define BSC_COLOR_MAX 63
#define BSC_HIST_LEN 20

// BSS Color state
enum BscColorState : uint8_t {
  BSC_STATE_NORMAL = 0,     // Normal operation
  BSC_STATE_COLLISION = 1,  // Color collision with neighbor
  BSC_STATE_CHANGING = 2,   // Color change in progress
  BSC_STATE_DISABLED = 3,   // BSS Coloring disabled
};

static const char *bsc_state_str(BscColorState s) {
  switch (s) {
    case BSC_STATE_NORMAL:
      return "Normal";
    case BSC_STATE_COLLISION:
      return "Collision";
    case BSC_STATE_CHANGING:
      return "Changing";
    case BSC_STATE_DISABLED:
      return "Disabled";
  }
  return "Unknown";
}

struct BscColorConflict {
  uint8_t other_bssid[6];
  char other_ssid[33];
  int8_t rssi;
  uint32_t ts;
};

struct BscAP {
  bool active;
  uint8_t bssid[6];
  char ssid[33];
  uint8_t ch;
  int8_t rssi;

  // BSS Color parameters
  bool he_capable;         // 802.11ax capable
  uint8_t color;           // Current BSS Color (1-63, 0=disabled)
  bool partial_bss_color;  // Partial BSS Color bit
  bool color_disabled;     // BSS Color Disabled bit

  // Spatial reuse parameters
  uint8_t sr_control;      // Spatial Reuse Control field
  int8_t obss_pd_min_dBm;  // OBSS/PD Min Threshold
  int8_t obss_pd_max_dBm;  // OBSS/PD Max Threshold
  bool srp_disallowed;     // SRP-based SR disallowed
  bool non_srg_offset_present;
  uint8_t non_srg_obss_pd_max_offset;  // in dB

  // Color collision detection
  BscColorState state;
  uint8_t color_history[8];  // past color values seen
  uint8_t color_hist_count;
  uint16_t color_changes;
  uint32_t last_color_change_ts;

  // Conflict list (APs with same color)
  BscColorConflict conflicts[4];
  uint8_t conflict_count;

  // HE operation parameters
  uint8_t he_op_params[3];  // HE Operation Parameters
  uint32_t beacon_count;

  uint32_t first_ts;
  uint32_t last_ts;
};

typedef struct {
  BscAP aps[BSC_MAX_APS];
  uint8_t count;
  uint8_t color_map[BSC_COLOR_MAX + 1];  // how many APs use each color
  uint16_t total_collisions;
  uint16_t total_color_changes;
} G_BscState;
#ifdef BOARD_HAS_PSRAM
static G_BscState *_g_bsc_raw;
#define g_bsc (*_g_bsc_raw)
#else
static G_BscState g_bsc;
#endif

static bool bsc_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static BscAP *bsc_find(const uint8_t *bssid) {
  for (int i = 0; i < BSC_MAX_APS; i++)
    if (g_bsc.aps[i].active && bsc_mac_eq(g_bsc.aps[i].bssid, bssid))
      return &g_bsc.aps[i];
  return nullptr;
}

static BscAP *bsc_alloc(const uint8_t *bssid) {
  for (int i = 0; i < BSC_MAX_APS; i++) {
    if (!g_bsc.aps[i].active) {
      BscAP &a = g_bsc.aps[i];
      memset(&a, 0, sizeof(a));
      a.active = true;
      memcpy(a.bssid, bssid, 6);
      a.state = BSC_STATE_NORMAL;
      a.first_ts = millis() / 1000;
      g_bsc.count++;
      return &a;
    }
  }
  uint32_t oldest = UINT32_MAX;
  int idx = 0;
  for (int i = 0; i < BSC_MAX_APS; i++)
    if (g_bsc.aps[i].last_ts < oldest) {
      oldest = g_bsc.aps[i].last_ts;
      idx = i;
    }
  if (g_bsc.aps[idx].color <= BSC_COLOR_MAX && g_bsc.color_map[g_bsc.aps[idx].color] > 0)
    g_bsc.color_map[g_bsc.aps[idx].color]--;
  memset(&g_bsc.aps[idx], 0, sizeof(BscAP));
  g_bsc.aps[idx].active = true;
  memcpy(g_bsc.aps[idx].bssid, bssid, 6);
  g_bsc.aps[idx].state = BSC_STATE_NORMAL;
  g_bsc.aps[idx].first_ts = millis() / 1000;
  return &g_bsc.aps[idx];
}

static void bsc_check_collision(BscAP &a) {
  for (int i = 0; i < BSC_MAX_APS; i++) {
    BscAP &other = g_bsc.aps[i];
    if (!other.active || bsc_mac_eq(other.bssid, a.bssid))
      continue;
    if (other.color == a.color && a.color > 0) {
      // Collision found
      a.state = BSC_STATE_COLLISION;
      g_bsc.total_collisions++;

      // Add to conflict list
      bool already = false;
      for (int j = 0; j < a.conflict_count; j++) {
        if (bsc_mac_eq(a.conflicts[j].other_bssid, other.bssid)) {
          already = true;
          break;
        }
      }
      if (!already && a.conflict_count < 4) {
        BscColorConflict &c = a.conflicts[a.conflict_count++];
        memcpy(c.other_bssid, other.bssid, 6);
        strncpy(c.other_ssid, other.ssid, 32);
        c.rssi = other.rssi;
        c.ts = millis() / 1000;
      }
    }
  }
}

void bsc_ingest(const uint8_t *bssid, const char *ssid, uint8_t ch, int8_t rssi, bool he_capable,
                uint8_t color, bool partial_color, bool color_disabled, uint8_t sr_control,
                int8_t obss_pd_min, int8_t obss_pd_max, bool srp_disallowed) {
  BscAP *a = bsc_find(bssid);
  if (!a)
    a = bsc_alloc(bssid);
  if (!a)
    return;

  if (ssid)
    strncpy(a->ssid, ssid, 32);
  a->ch = ch;
  a->rssi = rssi;
  a->he_capable = he_capable;
  a->partial_bss_color = partial_color;
  a->color_disabled = color_disabled;
  a->sr_control = sr_control;
  a->obss_pd_min_dBm = obss_pd_min;
  a->obss_pd_max_dBm = obss_pd_max;
  a->srp_disallowed = srp_disallowed;

  // Detect color change
  if (a->color != color && a->color != 0) {
    a->color_history[a->color_hist_count % 8] = a->color;
    a->color_hist_count = (a->color_hist_count + 1) % 8;
    a->color_changes++;
    g_bsc.total_color_changes++;
    a->last_color_change_ts = millis() / 1000;
    a->state = BSC_STATE_CHANGING;

    if (a->color <= BSC_COLOR_MAX && g_bsc.color_map[a->color] > 0)
      g_bsc.color_map[a->color]--;
  }

  if (a->color <= BSC_COLOR_MAX && a->color > 0)
    g_bsc.color_map[a->color]--;
  a->color = color;
  if (color > 0 && color <= BSC_COLOR_MAX)
    g_bsc.color_map[color]++;

  if (color_disabled)
    a->state = BSC_STATE_DISABLED;
  else
    bsc_check_collision(*a);

  a->beacon_count++;
  a->last_ts = millis() / 1000;
}

static void bsc_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String bsc_json_all() {
  char mac[18];
  String s = "{\"count\":" + String(g_bsc.count);
  s += ",\"total_collisions\":" + String(g_bsc.total_collisions);
  s += ",\"total_changes\":" + String(g_bsc.total_color_changes);

  // Color usage map
  s += ",\"color_map\":{";
  bool first = true;
  for (int c = 1; c <= BSC_COLOR_MAX; c++) {
    if (g_bsc.color_map[c] == 0)
      continue;
    if (!first)
      s += ",";
    first = false;
    s += "\"" + String(c) + "\":" + String(g_bsc.color_map[c]);
  }
  s += "},\"aps\":[";
  first = true;
  for (int i = 0; i < BSC_MAX_APS; i++) {
    BscAP &a = g_bsc.aps[i];
    if (!a.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    bsc_mac_str(a.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(a.ssid) + "\"";
    s += ",\"ch\":" + String(a.ch);
    s += ",\"rssi\":" + String(a.rssi);
    s += ",\"he\":" + String(a.he_capable ? "true" : "false");
    s += ",\"color\":" + String(a.color);
    s += ",\"partial\":" + String(a.partial_bss_color ? "true" : "false");
    s += ",\"disabled\":" + String(a.color_disabled ? "true" : "false");
    s += ",\"state\":\"" + String(bsc_state_str(a.state)) + "\"";
    s += ",\"obss_pd_min\":" + String(a.obss_pd_min_dBm);
    s += ",\"obss_pd_max\":" + String(a.obss_pd_max_dBm);
    s += ",\"srp_dis\":" + String(a.srp_disallowed ? "true" : "false");
    s += ",\"conflicts\":" + String(a.conflict_count);
    s += ",\"color_changes\":" + String(a.color_changes);
    if (a.conflict_count > 0) {
      s += ",\"conflict_list\":[";
      for (int j = 0; j < a.conflict_count; j++) {
        if (j)
          s += ",";
        bsc_mac_str(a.conflicts[j].other_bssid, mac);
        s += "{\"bssid\":\"" + String(mac) + "\",\"ssid\":\"" + String(a.conflicts[j].other_ssid) +
             "\"}";
      }
      s += "]";
    }
    s += "}";
  }
  s += "]}";
  return s;
}

inline void bsc_routes() {
  server.on("/api/bsc/all", HTTP_GET,
            []() { server.send(200, "application/json", bsc_json_all()); });
  server.on("/api/bsc/clear", HTTP_POST, []() {
    memset(&g_bsc, 0, sizeof(g_bsc));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

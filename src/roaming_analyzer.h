#pragma once
// roaming_analyzer.h — 802.11 client roaming event analyzer.
// Tracks BSS transitions (11r/11v), roaming triggers, and transition timing.
// Prefix: rom_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define ROM_MAX_EVENTS 64
#define ROM_MAX_CLIENTS 24

// Roaming trigger classification
enum RomTrigger : uint8_t {
  ROM_TRIG_UNKNOWN = 0,
  ROM_TRIG_RSSI_DROP = 1,  // Client-driven: RSSI fell below threshold
  ROM_TRIG_DEAUTH = 2,     // AP kicked client (deauth/disassoc)
  ROM_TRIG_BTM = 3,        // BSS Transition Management (AP-requested)
  ROM_TRIG_LOAD_BAL = 4,   // Load balancing
  ROM_TRIG_PROBE_RSP = 5,  // Client probed new AP then roamed
  ROM_TRIG_FAST_BSS = 6,   // 802.11r Fast BSS Transition
};

// Roaming event
struct RomEvent {
  uint8_t client_mac[6];
  uint8_t from_bssid[6];
  uint8_t to_bssid[6];
  char from_ssid[33];
  char to_ssid[33];
  uint8_t from_ch;
  uint8_t to_ch;
  int8_t from_rssi;
  int8_t to_rssi;
  RomTrigger trigger;
  bool fast_bss;           // 802.11r used
  bool same_ssid;          // from/to same SSID (intra-ESS)
  uint32_t transition_ms;  // time from last seen at old AP to assoc at new AP
  uint32_t ts;
};

// Per-client roaming profile
struct RomClient {
  bool active;
  uint8_t mac[6];

  uint8_t current_bssid[6];
  char current_ssid[33];
  uint8_t current_ch;
  int8_t current_rssi;

  uint16_t total_roams;
  uint16_t intra_ess_roams;  // same SSID (between APs of same network)
  uint16_t inter_ess_roams;  // different SSID
  uint16_t fast_bss_roams;   // 802.11r used
  uint16_t btm_roams;        // AP-requested via BTM
  uint16_t deauth_roams;     // kicked

  // Timing metrics
  uint32_t min_trans_ms;
  uint32_t max_trans_ms;
  uint32_t avg_trans_ms;

  // Last seen on old AP (for transition time estimation)
  uint32_t last_seen_prev_ms;
  uint8_t prev_bssid[6];

  uint32_t first_ts;
  uint32_t last_ts;
};

static struct {
  RomEvent events[ROM_MAX_EVENTS];
  uint8_t evt_head;
  uint8_t evt_count;
  RomClient clients[ROM_MAX_CLIENTS];
  uint8_t client_count;
  uint32_t total_events;
  uint32_t avg_trans_ms;
} g_rom = {};

static bool rom_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static RomClient *rom_find_client(const uint8_t *mac) {
  for (int i = 0; i < ROM_MAX_CLIENTS; i++)
    if (g_rom.clients[i].active && rom_mac_eq(g_rom.clients[i].mac, mac))
      return &g_rom.clients[i];
  return nullptr;
}

static RomClient *rom_alloc_client(const uint8_t *mac) {
  for (int i = 0; i < ROM_MAX_CLIENTS; i++) {
    if (!g_rom.clients[i].active) {
      RomClient &c = g_rom.clients[i];
      memset(&c, 0, sizeof(c));
      c.active = true;
      memcpy(c.mac, mac, 6);
      c.min_trans_ms = UINT32_MAX;
      c.first_ts = millis() / 1000;
      g_rom.client_count++;
      return &c;
    }
  }
  uint32_t oldest = UINT32_MAX;
  int idx = 0;
  for (int i = 0; i < ROM_MAX_CLIENTS; i++)
    if (g_rom.clients[i].last_ts < oldest) {
      oldest = g_rom.clients[i].last_ts;
      idx = i;
    }
  memset(&g_rom.clients[idx], 0, sizeof(RomClient));
  g_rom.clients[idx].active = true;
  g_rom.clients[idx].min_trans_ms = UINT32_MAX;
  memcpy(g_rom.clients[idx].mac, mac, 6);
  g_rom.clients[idx].first_ts = millis() / 1000;
  return &g_rom.clients[idx];
}

void rom_ingest_assoc(const uint8_t *client_mac, const uint8_t *ap_bssid, const char *ssid,
                      uint8_t ch, int8_t rssi, bool fast_bss) {
  RomClient *c = rom_find_client(client_mac);
  if (!c)
    c = rom_alloc_client(client_mac);
  if (!c)
    return;

  bool roamed = c->current_ch > 0 && !rom_mac_eq(c->current_bssid, ap_bssid);

  if (roamed) {
    // Record roam event
    RomEvent &e = g_rom.events[g_rom.evt_head % ROM_MAX_EVENTS];
    memset(&e, 0, sizeof(e));
    memcpy(e.client_mac, client_mac, 6);
    memcpy(e.from_bssid, c->current_bssid, 6);
    memcpy(e.to_bssid, ap_bssid, 6);
    strncpy(e.from_ssid, c->current_ssid, 32);
    if (ssid)
      strncpy(e.to_ssid, ssid, 32);
    e.from_ch = c->current_ch;
    e.to_ch = ch;
    e.from_rssi = c->current_rssi;
    e.to_rssi = rssi;
    e.fast_bss = fast_bss;
    e.same_ssid = (ssid && strncmp(c->current_ssid, ssid, 32) == 0);
    e.trigger = fast_bss ? ROM_TRIG_FAST_BSS : ROM_TRIG_UNKNOWN;

    if (c->last_seen_prev_ms > 0) {
      e.transition_ms = millis() - c->last_seen_prev_ms;
    }
    e.ts = millis() / 1000;
    g_rom.evt_head = (g_rom.evt_head + 1) % ROM_MAX_EVENTS;
    if (g_rom.evt_count < ROM_MAX_EVENTS)
      g_rom.evt_count++;
    g_rom.total_events++;

    // Update client stats
    c->total_roams++;
    if (e.same_ssid)
      c->intra_ess_roams++;
    else
      c->inter_ess_roams++;
    if (fast_bss)
      c->fast_bss_roams++;

    if (e.transition_ms > 0) {
      if (e.transition_ms < c->min_trans_ms)
        c->min_trans_ms = e.transition_ms;
      if (e.transition_ms > c->max_trans_ms)
        c->max_trans_ms = e.transition_ms;
      if (c->avg_trans_ms == 0)
        c->avg_trans_ms = e.transition_ms;
      else
        c->avg_trans_ms = (c->avg_trans_ms * 7 + e.transition_ms) / 8;

      // Global avg
      if (g_rom.avg_trans_ms == 0)
        g_rom.avg_trans_ms = e.transition_ms;
      else
        g_rom.avg_trans_ms = (g_rom.avg_trans_ms * 7 + e.transition_ms) / 8;
    }
  }

  // Update current association
  memcpy(c->prev_bssid, c->current_bssid, 6);
  memcpy(c->current_bssid, ap_bssid, 6);
  if (ssid)
    strncpy(c->current_ssid, ssid, 32);
  c->current_ch = ch;
  c->current_rssi = rssi;
  c->last_ts = millis() / 1000;
}

void rom_ingest_deauth(const uint8_t *client_mac, const uint8_t *ap_bssid) {
  RomClient *c = rom_find_client(client_mac);
  if (!c)
    return;
  c->last_seen_prev_ms = millis();
  // Mark trigger for next assoc
  // We'll check if next event from this client happened to new AP
}

void rom_ingest_data_frame(const uint8_t *mac, const uint8_t *bssid, int8_t rssi) {
  RomClient *c = rom_find_client(mac);
  if (!c)
    return;
  if (rom_mac_eq(c->current_bssid, bssid)) {
    c->current_rssi = rssi;
    c->last_seen_prev_ms = millis();
  }
}

static const char *rom_trigger_str(RomTrigger t) {
  switch (t) {
    case ROM_TRIG_RSSI_DROP:
      return "RSSI Drop";
    case ROM_TRIG_DEAUTH:
      return "Deauth";
    case ROM_TRIG_BTM:
      return "BTM";
    case ROM_TRIG_LOAD_BAL:
      return "Load Bal";
    case ROM_TRIG_PROBE_RSP:
      return "Probe Rsp";
    case ROM_TRIG_FAST_BSS:
      return "Fast BSS";
    default:
      return "Unknown";
  }
}

static void rom_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String rom_json_all() {
  String s = "{\"total_events\":" + String(g_rom.total_events);
  s += ",\"avg_trans_ms\":" + String(g_rom.avg_trans_ms);
  s += ",\"client_count\":" + String(g_rom.client_count);

  // Recent events (last N)
  s += ",\"events\":[";
  char mac[18], fb[18], tb[18];
  bool first = true;
  int start = g_rom.evt_count >= ROM_MAX_EVENTS ? g_rom.evt_head : 0;
  for (int i = g_rom.evt_count - 1; i >= 0; i--) {
    int idx = (start + i) % ROM_MAX_EVENTS;
    RomEvent &e = g_rom.events[idx];
    if (!e.ts)
      continue;
    if (!first)
      s += ",";
    first = false;
    rom_mac_str(e.client_mac, mac);
    rom_mac_str(e.from_bssid, fb);
    rom_mac_str(e.to_bssid, tb);
    s += "{\"mac\":\"" + String(mac) + "\"";
    s += ",\"from\":\"" + String(fb) + "\"";
    s += ",\"from_ssid\":\"" + String(e.from_ssid) + "\"";
    s += ",\"to\":\"" + String(tb) + "\"";
    s += ",\"to_ssid\":\"" + String(e.to_ssid) + "\"";
    s += ",\"from_ch\":" + String(e.from_ch);
    s += ",\"to_ch\":" + String(e.to_ch);
    s += ",\"from_rssi\":" + String(e.from_rssi);
    s += ",\"to_rssi\":" + String(e.to_rssi);
    s += ",\"trigger\":\"" + String(rom_trigger_str(e.trigger)) + "\"";
    s += ",\"fast_bss\":" + String(e.fast_bss ? "true" : "false");
    s += ",\"same_ssid\":" + String(e.same_ssid ? "true" : "false");
    s += ",\"trans_ms\":" + String(e.transition_ms);
    s += ",\"ts\":" + String(e.ts) + "}";
  }
  s += "],\"clients\":[";
  first = true;
  for (int i = 0; i < ROM_MAX_CLIENTS; i++) {
    RomClient &c = g_rom.clients[i];
    if (!c.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    rom_mac_str(c.mac, mac);
    rom_mac_str(c.current_bssid, fb);
    s += "{\"mac\":\"" + String(mac) + "\"";
    s += ",\"current_bssid\":\"" + String(fb) + "\"";
    s += ",\"current_ssid\":\"" + String(c.current_ssid) + "\"";
    s += ",\"current_ch\":" + String(c.current_ch);
    s += ",\"current_rssi\":" + String(c.current_rssi);
    s += ",\"total_roams\":" + String(c.total_roams);
    s += ",\"intra\":" + String(c.intra_ess_roams);
    s += ",\"inter\":" + String(c.inter_ess_roams);
    s += ",\"fast_bss\":" + String(c.fast_bss_roams);
    s += ",\"btm\":" + String(c.btm_roams);
    s += ",\"avg_trans\":" + String(c.avg_trans_ms);
    s += ",\"min_trans\":" + String(c.min_trans_ms == UINT32_MAX ? 0 : c.min_trans_ms);
    s += ",\"max_trans\":" + String(c.max_trans_ms) + "}";
  }
  s += "]}";
  return s;
}

inline void rom_routes() {
  server.on("/api/rom/all", HTTP_GET,
            []() { server.send(200, "application/json", rom_json_all()); });
  server.on("/api/rom/clear", HTTP_POST, []() {
    memset(&g_rom, 0, sizeof(g_rom));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

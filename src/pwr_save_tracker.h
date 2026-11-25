#pragma once
// pwr_save_tracker.h — 802.11 Power Save mode tracker.
// Tracks TIM (Traffic Indication Map) delivery, PS-Poll, U-APSD,
// and WMM power-save per AP/client pair.
// Prefix: pst_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define PST_MAX_APS 24
#define PST_MAX_CLIENTS 16  // per AP
#define PST_HIST_LEN 30

// Power-save mode type
enum PstMode : uint8_t {
  PST_MODE_NONE = 0,
  PST_MODE_LEGACY = 1,  // 802.11 legacy PS (PS-Poll)
  PST_MODE_UAPSD = 2,   // Unscheduled APSD
  PST_MODE_WMM_PS = 3,  // WMM Power Save
  PST_MODE_TWT = 4,     // Target Wake Time (802.11ax)
};

struct PstClient {
  bool active;
  uint8_t mac[6];
  PstMode mode;

  uint32_t ps_enter_count;    // times entered power save
  uint32_t ps_exit_count;     // times exited
  uint32_t ps_poll_count;     // PS-Poll frames sent
  uint32_t null_data_count;   // null-data for PS state changes
  uint32_t buffered_frames;   // current AID-TIM buffered frame count
  uint32_t delivered_frames;  // frames delivered from buffer

  // Power-save duration estimation (ms)
  uint32_t total_doze_ms;
  uint32_t avg_doze_ms;
  uint32_t last_ps_enter_ms;

  // Duty cycle estimation (doze_ms / awake_ms ratio * 100)
  uint8_t doze_pct;

  uint32_t last_ts;
};

struct PstAP {
  bool active;
  uint8_t bssid[6];
  char ssid[33];
  uint8_t ch;
  int8_t rssi;

  // TIM info from beacons
  uint8_t dtim_count;
  uint8_t dtim_period;
  uint32_t tim_frames;  // beacons with TIM bit set

  // Global AP-side counters
  uint32_t ps_clients_now;  // currently dozing
  uint32_t ps_events_total;
  uint32_t buffered_total;

  // Per-client tracking
  PstClient clients[PST_MAX_CLIENTS];
  uint8_t client_count;

  // History: clients in PS per second
  uint8_t ps_hist[PST_HIST_LEN];
  uint8_t hist_head;
  uint32_t cur_ps_clients;
  uint32_t last_tick_s;
};

static struct {
  PstAP aps[PST_MAX_APS];
  uint8_t count;
  uint32_t global_events;
} g_pst = {};

static bool pst_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static PstAP *pst_find_ap(const uint8_t *bssid) {
  for (int i = 0; i < PST_MAX_APS; i++)
    if (g_pst.aps[i].active && pst_mac_eq(g_pst.aps[i].bssid, bssid))
      return &g_pst.aps[i];
  return nullptr;
}

static PstAP *pst_alloc_ap(const uint8_t *bssid) {
  for (int i = 0; i < PST_MAX_APS; i++) {
    if (!g_pst.aps[i].active) {
      PstAP &a = g_pst.aps[i];
      memset(&a, 0, sizeof(a));
      a.active = true;
      memcpy(a.bssid, bssid, 6);
      g_pst.count++;
      return &a;
    }
  }
  uint32_t oldest = UINT32_MAX;
  int idx = 0;
  for (int i = 0; i < PST_MAX_APS; i++)
    if (g_pst.aps[i].last_tick_s < oldest) {
      oldest = g_pst.aps[i].last_tick_s;
      idx = i;
    }
  memset(&g_pst.aps[idx], 0, sizeof(PstAP));
  g_pst.aps[idx].active = true;
  memcpy(g_pst.aps[idx].bssid, bssid, 6);
  return &g_pst.aps[idx];
}

static PstClient *pst_find_client(PstAP &a, const uint8_t *mac) {
  for (int i = 0; i < a.client_count; i++)
    if (a.clients[i].active && pst_mac_eq(a.clients[i].mac, mac))
      return &a.clients[i];
  return nullptr;
}

static PstClient *pst_alloc_client(PstAP &a, const uint8_t *mac) {
  if (a.client_count < PST_MAX_CLIENTS) {
    PstClient &c = a.clients[a.client_count++];
    memset(&c, 0, sizeof(c));
    c.active = true;
    memcpy(c.mac, mac, 6);
    return &c;
  }
  uint32_t oldest = UINT32_MAX;
  int idx = 0;
  for (int i = 0; i < PST_MAX_CLIENTS; i++)
    if (a.clients[i].last_ts < oldest) {
      oldest = a.clients[i].last_ts;
      idx = i;
    }
  memset(&a.clients[idx], 0, sizeof(PstClient));
  a.clients[idx].active = true;
  memcpy(a.clients[idx].mac, mac, 6);
  return &a.clients[idx];
}

static void pst_tick(PstAP &a) {
  uint32_t now_s = millis() / 1000;
  if (now_s == a.last_tick_s)
    return;
  a.ps_hist[a.hist_head % PST_HIST_LEN] =
      (uint8_t)(a.cur_ps_clients > 255 ? 255 : a.cur_ps_clients);
  a.hist_head = (a.hist_head + 1) % PST_HIST_LEN;
  a.last_tick_s = now_s;
}

void pst_ingest_beacon(const uint8_t *bssid, const char *ssid, uint8_t ch, int8_t rssi,
                       uint8_t dtim_count, uint8_t dtim_period, bool tim_set) {
  PstAP *a = pst_find_ap(bssid);
  if (!a)
    a = pst_alloc_ap(bssid);
  if (!a)
    return;

  pst_tick(*a);
  a->ch = ch;
  a->rssi = rssi;
  if (ssid)
    strncpy(a->ssid, ssid, 32);
  a->dtim_count = dtim_count;
  a->dtim_period = dtim_period;
  if (tim_set)
    a->tim_frames++;
}

void pst_ingest_ps_change(const uint8_t *client_mac, const uint8_t *ap_bssid, bool entering_ps,
                          PstMode mode) {
  PstAP *a = pst_find_ap(ap_bssid);
  if (!a)
    a = pst_alloc_ap(ap_bssid);
  if (!a)
    return;

  PstClient *c = pst_find_client(*a, client_mac);
  if (!c)
    c = pst_alloc_client(*a, client_mac);
  if (!c)
    return;

  c->mode = mode;
  c->last_ts = millis() / 1000;

  if (entering_ps) {
    c->ps_enter_count++;
    c->last_ps_enter_ms = millis();
    a->ps_clients_now++;
    a->ps_events_total++;
    g_pst.global_events++;
    pst_tick(*a);
    a->cur_ps_clients = a->ps_clients_now;
  } else {
    if (a->ps_clients_now > 0)
      a->ps_clients_now--;
    c->ps_exit_count++;

    // Estimate doze duration
    if (c->last_ps_enter_ms > 0) {
      uint32_t doze = millis() - c->last_ps_enter_ms;
      c->total_doze_ms += doze;
      if (c->avg_doze_ms == 0)
        c->avg_doze_ms = doze;
      else
        c->avg_doze_ms = (c->avg_doze_ms * 7 + doze) / 8;
    }
  }
}

void pst_ingest_ps_poll(const uint8_t *client_mac, const uint8_t *ap_bssid) {
  PstAP *a = pst_find_ap(ap_bssid);
  if (!a)
    return;
  PstClient *c = pst_find_client(*a, client_mac);
  if (!c)
    c = pst_alloc_client(*a, client_mac);
  if (!c)
    return;
  c->ps_poll_count++;
  c->last_ts = millis() / 1000;
}

void pst_ingest_null_data(const uint8_t *client_mac, const uint8_t *ap_bssid, bool ps_bit) {
  PstAP *a = pst_find_ap(ap_bssid);
  if (!a)
    return;
  PstClient *c = pst_find_client(*a, client_mac);
  if (!c)
    c = pst_alloc_client(*a, client_mac);
  if (!c)
    return;
  c->null_data_count++;
  c->last_ts = millis() / 1000;
  // ps_bit in null-data = station transitioning PS state
  if (ps_bit)
    pst_ingest_ps_change(client_mac, ap_bssid, true, PST_MODE_LEGACY);
  else
    pst_ingest_ps_change(client_mac, ap_bssid, false, PST_MODE_LEGACY);
}

static void pst_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String pst_hist_json(const uint8_t *hist, int len, int head) {
  String s = "[";
  for (int i = 0; i < len; i++) {
    if (i)
      s += ",";
    s += String(hist[(head + i) % len]);
  }
  return s + "]";
}

static const char *pst_mode_str(PstMode m) {
  switch (m) {
    case PST_MODE_LEGACY:
      return "Legacy";
    case PST_MODE_UAPSD:
      return "UAPSD";
    case PST_MODE_WMM_PS:
      return "WMM-PS";
    case PST_MODE_TWT:
      return "TWT";
    default:
      return "None";
  }
}

static String pst_json_all() {
  String s = "{\"count\":" + String(g_pst.count);
  s += ",\"global_events\":" + String(g_pst.global_events);
  s += ",\"aps\":[";
  char mac[18];
  bool first = true;
  for (int i = 0; i < PST_MAX_APS; i++) {
    PstAP &a = g_pst.aps[i];
    if (!a.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    pst_mac_str(a.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(a.ssid) + "\"";
    s += ",\"ch\":" + String(a.ch);
    s += ",\"rssi\":" + String(a.rssi);
    s += ",\"dtim_period\":" + String(a.dtim_period);
    s += ",\"tim_frames\":" + String(a.tim_frames);
    s += ",\"ps_clients_now\":" + String(a.ps_clients_now);
    s += ",\"ps_events\":" + String(a.ps_events_total);
    s += ",\"ps_hist\":" + pst_hist_json(a.ps_hist, PST_HIST_LEN, a.hist_head);
    s += ",\"clients\":[";
    char cmac[18];
    bool cf = true;
    for (int j = 0; j < a.client_count; j++) {
      PstClient &c = a.clients[j];
      if (!c.active)
        continue;
      if (!cf)
        s += ",";
      cf = false;
      pst_mac_str(c.mac, cmac);
      s += "{\"mac\":\"" + String(cmac) + "\"";
      s += ",\"mode\":\"" + String(pst_mode_str(c.mode)) + "\"";
      s += ",\"enter\":" + String(c.ps_enter_count);
      s += ",\"exit\":" + String(c.ps_exit_count);
      s += ",\"polls\":" + String(c.ps_poll_count);
      s += ",\"avg_doze\":" + String(c.avg_doze_ms);
      s += ",\"total_doze\":" + String(c.total_doze_ms) + "}";
    }
    s += "]}";
  }
  s += "]}";
  return s;
}

inline void pst_routes() {
  server.on("/api/pst/all", HTTP_GET,
            []() { server.send(200, "application/json", pst_json_all()); });
  server.on("/api/pst/clear", HTTP_POST, []() {
    memset(&g_pst, 0, sizeof(g_pst));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

#pragma once
// client_tracker_ext.h — Extended client tracking with connection state machine.
// Tracks per-client auth/assoc/connected state, retries, and roaming events.
// Prefix: cte_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define CTE_MAX_CLIENTS  64
#define CTE_MAX_HISTORY  16  // events per client

enum CteState {
  CTE_STATE_UNSEEN     = 0,
  CTE_STATE_PROBING    = 1,
  CTE_STATE_AUTHED     = 2,
  CTE_STATE_ASSOC      = 3,
  CTE_STATE_CONNECTED  = 4,
  CTE_STATE_DEAUTHED   = 5,
  CTE_STATE_DISASSOC   = 6,
  CTE_STATE_ROAMING    = 7,
};

static const char* CTE_STATE_NAMES[] = {
  "Unseen","Probing","Authenticated","Associated","Connected",
  "Deauthed","Disassoc","Roaming"
};

enum CteEvent {
  CTE_EV_PROBE_REQ   = 0,
  CTE_EV_PROBE_RESP  = 1,
  CTE_EV_AUTH_REQ    = 2,
  CTE_EV_AUTH_RESP   = 3,
  CTE_EV_ASSOC_REQ   = 4,
  CTE_EV_ASSOC_RESP  = 5,
  CTE_EV_DEAUTH      = 6,
  CTE_EV_DISASSOC    = 7,
  CTE_EV_DATA        = 8,
  CTE_EV_EAPOL       = 9,
  CTE_EV_ROAM        = 10,
};

static const char* CTE_EV_NAMES[] = {
  "ProbeReq","ProbeResp","AuthReq","AuthResp","AssocReq","AssocResp",
  "Deauth","Disassoc","Data","EAPOL","Roam"
};

struct CteHistEntry {
  CteEvent event;
  uint32_t ts;
  uint8_t  bssid[6];
  uint8_t  reason;
};

struct CteClient {
  bool    active;
  uint8_t mac[6];
  uint8_t cur_bssid[6];  // currently associated AP
  uint8_t prev_bssid[6]; // before last roam

  char    ssid[33];
  int8_t  rssi;
  uint8_t channel;

  CteState state;

  // Connection quality
  uint32_t retry_count;
  uint32_t deauth_count;
  uint32_t roam_count;
  uint32_t data_frames;
  uint32_t eapol_frames;

  // Auth timing
  uint32_t auth_ts;
  uint32_t assoc_ts;
  uint32_t connect_ts;
  uint32_t last_ts;
  uint32_t first_ts;

  // History ring
  CteHistEntry hist[CTE_MAX_HISTORY];
  uint8_t  hist_head;
  uint8_t  hist_count;

  bool     wpa3;
  bool     eap_seen;
};

static struct {
  CteClient clients[CTE_MAX_CLIENTS];
  uint8_t   client_count;

  uint32_t total_events;
  uint32_t roam_events;
  uint32_t deauth_events;

  // State distribution
  uint32_t state_count[8];
} g_cte = {};

static bool cte_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static bool cte_is_broadcast(const uint8_t *m) {
  static const uint8_t bc[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
  return cte_mac_eq(m, bc);
}

static CteClient* cte_find_or_alloc(const uint8_t *mac) {
  for (int i = 0; i < CTE_MAX_CLIENTS; i++)
    if (g_cte.clients[i].active && cte_mac_eq(g_cte.clients[i].mac, mac))
      return &g_cte.clients[i];
  for (int i = 0; i < CTE_MAX_CLIENTS; i++) {
    if (!g_cte.clients[i].active) {
      CteClient &c = g_cte.clients[i];
      memset(&c, 0, sizeof(c));
      c.active = true;
      memcpy(c.mac, mac, 6);
      c.state = CTE_STATE_UNSEEN;
      c.first_ts = millis()/1000;
      g_cte.client_count++;
      return &c;
    }
  }
  uint32_t oldest = UINT32_MAX; int idx = 0;
  for (int i = 0; i < CTE_MAX_CLIENTS; i++)
    if (g_cte.clients[i].last_ts < oldest) { oldest = g_cte.clients[i].last_ts; idx = i; }
  if (g_cte.state_count[g_cte.clients[idx].state] > 0)
    g_cte.state_count[g_cte.clients[idx].state]--;
  memset(&g_cte.clients[idx], 0, sizeof(g_cte.clients[0]));
  g_cte.clients[idx].active = true;
  memcpy(g_cte.clients[idx].mac, mac, 6);
  g_cte.clients[idx].first_ts = millis()/1000;
  return &g_cte.clients[idx];
}

static void cte_push_hist(CteClient &c, CteEvent ev, const uint8_t *bssid, uint8_t reason = 0) {
  CteHistEntry &h = c.hist[c.hist_head];
  h.event  = ev;
  h.ts     = millis()/1000;
  h.reason = reason;
  if (bssid) memcpy(h.bssid, bssid, 6);
  c.hist_head = (c.hist_head + 1) % CTE_MAX_HISTORY;
  if (c.hist_count < CTE_MAX_HISTORY) c.hist_count++;
}

void cte_ingest(const uint8_t *client_mac, const uint8_t *bssid,
                CteEvent event, uint8_t reason,
                int8_t rssi, uint8_t channel,
                const char *ssid = nullptr) {
  if (cte_is_broadcast(client_mac)) return;

  CteClient *c = cte_find_or_alloc(client_mac);
  if (!c) return;

  uint32_t now = millis()/1000;
  g_cte.total_events++;

  if (rssi != 0) c->rssi = rssi;
  if (channel)   c->channel = channel;
  if (ssid && ssid[0]) strncpy(c->ssid, ssid, 32);
  c->last_ts = now;

  CteState old_state = c->state;

  cte_push_hist(*c, event, bssid, reason);

  switch (event) {
    case CTE_EV_PROBE_REQ:
      if (c->state == CTE_STATE_UNSEEN) c->state = CTE_STATE_PROBING;
      break;

    case CTE_EV_AUTH_REQ:
      if (bssid) memcpy(c->cur_bssid, bssid, 6);
      c->auth_ts = now;
      c->state = CTE_STATE_AUTHED;
      break;

    case CTE_EV_ASSOC_REQ:
      if (bssid) memcpy(c->cur_bssid, bssid, 6);
      c->assoc_ts = now;
      c->state = CTE_STATE_ASSOC;
      break;

    case CTE_EV_ASSOC_RESP:
      c->connect_ts = now;
      c->state = CTE_STATE_CONNECTED;
      break;

    case CTE_EV_DEAUTH:
      c->deauth_count++;
      g_cte.deauth_events++;
      // If client was connected, check for roam
      if (c->state == CTE_STATE_CONNECTED && reason == 0) {
        c->state = CTE_STATE_DEAUTHED;
      } else {
        c->state = CTE_STATE_DEAUTHED;
      }
      break;

    case CTE_EV_DISASSOC:
      c->state = CTE_STATE_DISASSOC;
      break;

    case CTE_EV_DATA:
      c->data_frames++;
      if (c->state != CTE_STATE_CONNECTED)
        c->state = CTE_STATE_CONNECTED;
      if (bssid) {
        if (!cte_mac_eq(bssid, c->cur_bssid)) {
          memcpy(c->prev_bssid, c->cur_bssid, 6);
          memcpy(c->cur_bssid, bssid, 6);
          c->state = CTE_STATE_ROAMING;
          c->roam_count++;
          g_cte.roam_events++;
        }
      }
      break;

    case CTE_EV_EAPOL:
      c->eapol_frames++;
      c->eap_seen = true;
      break;

    case CTE_EV_ROAM:
      if (bssid && !cte_mac_eq(bssid, c->cur_bssid)) {
        memcpy(c->prev_bssid, c->cur_bssid, 6);
        memcpy(c->cur_bssid, bssid, 6);
        c->roam_count++;
        g_cte.roam_events++;
        c->state = CTE_STATE_ROAMING;
      }
      break;

    default: break;
  }

  // Update state counts
  if (old_state != c->state) {
    if (g_cte.state_count[old_state] > 0) g_cte.state_count[old_state]--;
    g_cte.state_count[c->state]++;
  }
}

static void cte_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String cte_json_all() {
  String s = "{\"client_count\":" + String(g_cte.client_count);
  s += ",\"total_events\":" + String(g_cte.total_events);
  s += ",\"roam_events\":" + String(g_cte.roam_events);
  s += ",\"deauth_events\":" + String(g_cte.deauth_events);
  s += ",\"connected\":" + String(g_cte.state_count[CTE_STATE_CONNECTED]);
  s += ",\"probing\":" + String(g_cte.state_count[CTE_STATE_PROBING]);
  s += ",\"clients\":[";
  char mac[18], bssid[18];
  bool first = true;
  for (int i = 0; i < CTE_MAX_CLIENTS; i++) {
    CteClient &c = g_cte.clients[i];
    if (!c.active) continue;
    if (!first) s += ",";
    first = false;
    cte_mac_str(c.mac, mac);
    cte_mac_str(c.cur_bssid, bssid);
    s += "{\"mac\":\"" + String(mac) + "\"";
    s += ",\"bssid\":\"" + String(bssid) + "\"";
    s += ",\"ssid\":\"" + String(c.ssid) + "\"";
    s += ",\"state\":\"" + String(CTE_STATE_NAMES[c.state]) + "\"";
    s += ",\"rssi\":" + String(c.rssi);
    s += ",\"ch\":" + String(c.channel);
    s += ",\"retries\":" + String(c.retry_count);
    s += ",\"deauths\":" + String(c.deauth_count);
    s += ",\"roams\":" + String(c.roam_count);
    s += ",\"data\":" + String(c.data_frames);
    s += ",\"eap\":" + String(c.eap_seen?"true":"false");
    s += ",\"ts\":" + String(c.last_ts) + "}";
  }
  s += "]}";
  return s;
}

inline void cte_routes() {
  server.on("/api/cte/all", HTTP_GET, []() {
    server.send(200, "application/json", cte_json_all());
  });
  server.on("/api/cte/clear", HTTP_POST, []() {
    memset(&g_cte, 0, sizeof(g_cte));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

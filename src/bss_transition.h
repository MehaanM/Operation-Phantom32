#pragma once
// bss_transition.h — 802.11v BSS Transition Management tracking.
// Monitors BSS Transition Request/Response frames used in roaming.
// Prefix: bst_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define BST_MAX_APS       32
#define BST_MAX_EVENTS    96
#define BST_MAX_TARGETS   64

// 802.11v BSS Transition status codes
#define BST_STATUS_ACCEPT        0
#define BST_STATUS_REJECT_UNSPEC 1
#define BST_STATUS_REJECT_INSUF  2
#define BST_STATUS_REJECT_BSS    3
#define BST_STATUS_REJECT_DELAY  5
#define BST_STATUS_REJECT_LEAVING 6

struct BstCandidate {
  uint8_t  bssid[6];
  uint8_t  op_class;
  uint8_t  channel;
  uint8_t  preference;    // 0-255, higher = more preferred
  uint32_t bssid_info;
  uint32_t last_ts;
  uint32_t seen_count;
};

struct BstEvent {
  uint32_t ts;
  uint8_t  ap[6];
  uint8_t  client[6];
  bool     is_request;    // true=request from AP, false=response from client
  uint8_t  status_code;   // response only
  bool     disassoc_imminent;
  uint16_t disassoc_timer;    // TBTTs until disassoc
  uint8_t  candidate_count;
  char     ssid[33];
  char     reason[48];
};

struct BstAP {
  bool     active;
  uint8_t  bssid[6];
  char     ssid[33];
  uint8_t  channel;
  int8_t   rssi;

  // BSS TM capability
  bool     btm_capable;

  // Counts
  uint32_t req_count;
  uint32_t rsp_accept_count;
  uint32_t rsp_reject_count;
  uint32_t disassoc_imminent_count;
  uint32_t abridged_count;

  uint32_t first_ts;
  uint32_t last_ts;
};

static struct {
  BstAP        aps[BST_MAX_APS];
  uint8_t      ap_count;
  BstCandidate targets[BST_MAX_TARGETS];
  uint8_t      target_count;
  BstEvent     events[BST_MAX_EVENTS];
  uint8_t      event_head;
  uint32_t     event_count;

  uint32_t total_requests;
  uint32_t total_accepts;
  uint32_t total_rejects;
  uint32_t total_disassoc_imminent;
} g_bst = {};

static bool bst_mac_eq(const uint8_t *a, const uint8_t *b) { return memcmp(a, b, 6) == 0; }

static BstAP* bst_find_or_alloc(const uint8_t *bssid) {
  for (int i = 0; i < BST_MAX_APS; i++)
    if (g_bst.aps[i].active && bst_mac_eq(g_bst.aps[i].bssid, bssid))
      return &g_bst.aps[i];
  for (int i = 0; i < BST_MAX_APS; i++) {
    if (!g_bst.aps[i].active) {
      BstAP &a = g_bst.aps[i];
      memset(&a, 0, sizeof(a));
      a.active = true;
      memcpy(a.bssid, bssid, 6);
      a.first_ts = millis()/1000;
      g_bst.ap_count++;
      return &a;
    }
  }
  uint32_t oldest = UINT32_MAX; int idx = 0;
  for (int i = 0; i < BST_MAX_APS; i++)
    if (g_bst.aps[i].last_ts < oldest) { oldest = g_bst.aps[i].last_ts; idx = i; }
  memset(&g_bst.aps[idx], 0, sizeof(g_bst.aps[0]));
  g_bst.aps[idx].active = true;
  memcpy(g_bst.aps[idx].bssid, bssid, 6);
  g_bst.aps[idx].first_ts = millis()/1000;
  return &g_bst.aps[idx];
}

static void bst_add_event(const uint8_t *ap, const uint8_t *client,
                           bool is_req, uint8_t status,
                           bool disassoc, uint16_t timer,
                           uint8_t cand_cnt, const char *ssid) {
  BstEvent &e = g_bst.events[g_bst.event_head % BST_MAX_EVENTS];
  e.ts = millis()/1000;
  if (ap)     memcpy(e.ap,     ap,     6); else memset(e.ap,     0, 6);
  if (client) memcpy(e.client, client, 6); else memset(e.client, 0, 6);
  e.is_request       = is_req;
  e.status_code      = status;
  e.disassoc_imminent= disassoc;
  e.disassoc_timer   = timer;
  e.candidate_count  = cand_cnt;
  if (ssid) strncpy(e.ssid, ssid, 32);

  if (is_req && disassoc)
    snprintf(e.reason, 47, "Disassoc in %d TBTTs, %d candidates", timer, cand_cnt);
  else if (!is_req) {
    static const char* const status_names[] = {
      "Accept","Reject-Unspec","Reject-InsufBW","Reject-BSS",
      "?","Reject-Delay","Reject-Leaving"
    };
    snprintf(e.reason, 47, "Status: %s", (status<7)?status_names[status]:"Unknown");
  } else
    snprintf(e.reason, 47, "BSS TM Req, %d candidates", cand_cnt);

  g_bst.event_head = (g_bst.event_head + 1) % BST_MAX_EVENTS;
  g_bst.event_count++;
}

void bst_ingest_ap(const uint8_t *bssid, const char *ssid,
                   uint8_t channel, int8_t rssi, bool btm_cap) {
  BstAP *a = bst_find_or_alloc(bssid);
  if (!a) return;
  if (ssid && ssid[0]) strncpy(a->ssid, ssid, 32);
  a->channel = channel;
  if (rssi < 0) a->rssi = rssi;
  a->btm_capable = btm_cap;
  a->last_ts = millis()/1000;
}

void bst_ingest_request(const uint8_t *ap, const uint8_t *client,
                         const char *ssid, bool disassoc_imminent,
                         uint16_t disassoc_timer, bool abridged,
                         uint8_t candidate_count) {
  BstAP *a = bst_find_or_alloc(ap);
  if (a) {
    a->req_count++;
    if (disassoc_imminent) a->disassoc_imminent_count++;
    if (abridged)          a->abridged_count++;
    a->btm_capable = true;
    a->last_ts = millis()/1000;
  }
  g_bst.total_requests++;
  if (disassoc_imminent) g_bst.total_disassoc_imminent++;
  bst_add_event(ap, client, true, 0, disassoc_imminent, disassoc_timer, candidate_count, ssid);
}

void bst_ingest_response(const uint8_t *ap, const uint8_t *client,
                          const char *ssid, uint8_t status,
                          const uint8_t *target_bssid, uint8_t target_ch) {
  BstAP *a = bst_find_or_alloc(ap);
  if (a) {
    if (status == BST_STATUS_ACCEPT) a->rsp_accept_count++;
    else                              a->rsp_reject_count++;
    a->last_ts = millis()/1000;
  }
  if (status == BST_STATUS_ACCEPT) g_bst.total_accepts++;
  else                              g_bst.total_rejects++;

  if (target_bssid) {
    bool found = false;
    for (int i = 0; i < BST_MAX_TARGETS; i++) {
      if (g_bst.targets[i].channel &&
          bst_mac_eq(g_bst.targets[i].bssid, target_bssid)) {
        g_bst.targets[i].seen_count++;
        g_bst.targets[i].last_ts = millis()/1000;
        found = true; break;
      }
    }
    if (!found) {
      for (int i = 0; i < BST_MAX_TARGETS; i++) {
        if (!g_bst.targets[i].channel) {
          BstCandidate &c = g_bst.targets[i];
          memset(&c, 0, sizeof(c));
          memcpy(c.bssid,  target_bssid, 6);
          c.channel   = target_ch;
          c.seen_count = 1;
          c.last_ts   = millis()/1000;
          g_bst.target_count++;
          break;
        }
      }
    }
  }
  bst_add_event(ap, client, false, status, false, 0, 0, ssid);
}

static void bst_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String bst_json_all() {
  String s = "{\"ap_count\":" + String(g_bst.ap_count);
  s += ",\"total_requests\":" + String(g_bst.total_requests);
  s += ",\"total_accepts\":" + String(g_bst.total_accepts);
  s += ",\"total_rejects\":" + String(g_bst.total_rejects);
  s += ",\"total_disassoc\":" + String(g_bst.total_disassoc_imminent);
  s += ",\"aps\":[";
  char mac[18]; bool first = true;
  for (int i = 0; i < BST_MAX_APS; i++) {
    BstAP &a = g_bst.aps[i];
    if (!a.active) continue;
    if (!first) s += ","; first = false;
    bst_mac_str(a.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(a.ssid) + "\"";
    s += ",\"ch\":" + String(a.channel);
    s += ",\"rssi\":" + String(a.rssi);
    s += ",\"btm_cap\":" + String(a.btm_capable?"true":"false");
    s += ",\"reqs\":" + String(a.req_count);
    s += ",\"accepts\":" + String(a.rsp_accept_count);
    s += ",\"rejects\":" + String(a.rsp_reject_count);
    s += ",\"disassoc\":" + String(a.disassoc_imminent_count);
    s += "}";
  }
  s += "],\"targets\":[";
  first = true;
  for (int i = 0; i < BST_MAX_TARGETS; i++) {
    BstCandidate &c = g_bst.targets[i];
    if (!c.channel) continue;
    if (!first) s += ","; first = false;
    bst_mac_str(c.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ch\":" + String(c.channel);
    s += ",\"pref\":" + String(c.preference);
    s += ",\"seen\":" + String(c.seen_count);
    s += "}";
  }
  s += "]}";
  return s;
}

static String bst_json_events() {
  String s = "[";
  char ap[18], cl[18]; bool first = true;
  uint32_t cnt = g_bst.event_count;
  uint32_t num = min((uint32_t)BST_MAX_EVENTS, cnt);
  uint32_t start = (cnt > BST_MAX_EVENTS) ? g_bst.event_head : 0;
  for (uint32_t i = 0; i < num; i++) {
    uint32_t idx = (start + i) % BST_MAX_EVENTS;
    BstEvent &e  = g_bst.events[idx];
    if (!e.ts) continue;
    if (!first) s += ","; first = false;
    bst_mac_str(e.ap,     ap);
    bst_mac_str(e.client, cl);
    s += "{\"ts\":" + String(e.ts);
    s += ",\"ap\":\"" + String(ap) + "\"";
    s += ",\"client\":\"" + String(cl) + "\"";
    s += ",\"req\":" + String(e.is_request?"true":"false");
    s += ",\"status\":" + String(e.status_code);
    s += ",\"disassoc\":" + String(e.disassoc_imminent?"true":"false");
    s += ",\"timer\":" + String(e.disassoc_timer);
    s += ",\"candidates\":" + String(e.candidate_count);
    s += ",\"ssid\":\"" + String(e.ssid) + "\"";
    s += ",\"reason\":\"" + String(e.reason) + "\"";
    s += "}";
  }
  s += "]";
  return s;
}

inline void bst_routes() {
  server.on("/api/bst/all", HTTP_GET, []() {
    server.send(200, "application/json", bst_json_all());
  });
  server.on("/api/bst/events", HTTP_GET, []() {
    server.send(200, "application/json", bst_json_events());
  });
  server.on("/api/bst/clear", HTTP_POST, []() {
    memset(&g_bst, 0, sizeof(g_bst));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

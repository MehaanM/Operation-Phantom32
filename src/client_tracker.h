#pragma once
// client_tracker.h — Comprehensive 802.11 client station tracker.
// Tracks individual STA lifecycle: discovery, association, roaming, power save,
// transmission rates, probe history, and inactivity. Each entry represents one
// unique MAC address observed in the air. Maintains a fixed-size table with LRU
// eviction and exposes a full JSON API for the web UI.
// Prefix: clt_

#include <Arduino.h>
#include <WebServer.h>
#include <string.h>

extern WebServer server;

#define CLT_MAX_CLIENTS 64
#define CLT_MAX_PROBES 8
#define CLT_MAX_APS 4           // APs a client has been associated with
#define CLT_HISTORY_SAMPLES 20  // per-client RSSI history
#define CLT_INACTIVE_S 60       // mark inactive after 60s of silence

// Client connection state
enum CltState : uint8_t {
  CLT_STATE_PROBING = 0,      // only seen probe requests
  CLT_STATE_ASSOCIATING = 1,  // auth/assoc frames seen
  CLT_STATE_ASSOCIATED = 2,   // data frames observed
  CLT_STATE_ROAMING = 3,      // roam event in progress
  CLT_STATE_SLEEPING = 4,     // power save (null frames with PM=1)
  CLT_STATE_INACTIVE = 5,     // no frames for CLT_INACTIVE_S seconds
  CLT_STATE_DEAUTHED = 6,     // deauth received
};

// Client capability flags
#define CLT_CAP_HT 0x01
#define CLT_CAP_VHT 0x02
#define CLT_CAP_HE 0x04
#define CLT_CAP_SHORT_GI 0x08
#define CLT_CAP_PMF 0x10
#define CLT_CAP_MBO 0x20
#define CLT_CAP_BTM 0x40

// A single BSSID the client was associated with
struct CltAssoc {
  uint8_t bssid[6];
  char ssid[33];
  uint8_t ch;
  int8_t rssi;
  uint32_t first_ts;
  uint32_t last_ts;
  uint32_t frames;
};

// Client station record
struct CltStation {
  bool active;
  uint8_t mac[6];
  char vendor[32];  // from OUI lookup

  CltState state;
  uint8_t cap_flags;  // CLT_CAP_* bitmask

  // Current association
  uint8_t cur_bssid[6];
  char cur_ssid[33];
  uint8_t cur_ch;
  int8_t cur_rssi;

  // Association history (last CLT_MAX_APS BSSIDs)
  CltAssoc assoc_hist[CLT_MAX_APS];
  uint8_t assoc_count;

  // Probe history (SSIDs probed for)
  char probes[CLT_MAX_PROBES][33];
  uint8_t probe_count;

  // Per-client RSSI history
  int8_t rssi_hist[CLT_HISTORY_SAMPLES];
  uint8_t rssi_hist_head;

  // Frame counters
  uint32_t total_frames;
  uint32_t data_frames;
  uint32_t mgmt_frames;
  uint32_t retry_frames;
  uint32_t ps_null_frames;  // power-save null frames
  uint32_t deauth_count;    // deauths sent/received by this client

  // Roaming
  uint32_t roam_events;  // number of BSSID changes observed
  uint32_t last_roam_ts;

  // Power-save state
  bool ps_active;       // currently in power save
  uint32_t ps_entries;  // times power save was entered
  uint32_t ps_exits;    // times power save was exited

  // Data rate
  uint16_t last_rate_kbps;
  uint16_t peak_rate_kbps;

  // Timing
  uint32_t first_seen_ts;
  uint32_t last_seen_ts;
  uint32_t active_seconds;

  // Anomaly flags
  bool deauth_flooder;
  bool probe_sweeper;
  bool ps_abuser;
};

static struct {
  CltStation sta[CLT_MAX_CLIENTS];
  uint8_t count;

  // Global counters
  uint32_t total_seen;
  uint32_t total_assoc_events;
  uint32_t total_roam_events;
  uint32_t total_deauth_events;
  uint32_t total_probes;
  uint32_t active_now;  // clients with state != INACTIVE

  // Notable clients
  uint8_t most_active_idx;
  uint8_t most_probes_idx;
  uint8_t most_roams_idx;
} g_clt = {};

static bool clt_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}
static bool clt_mac_zero(const uint8_t *a) {
  return a[0] == 0 && a[1] == 0 && a[2] == 0 && a[3] == 0 && a[4] == 0 && a[5] == 0;
}

// Forward: OUI lookup (provided by vendor_lookup.h)
extern const char *vnd_lookup(const uint8_t *mac);

static CltStation *clt_find(const uint8_t *mac) {
  for (int i = 0; i < CLT_MAX_CLIENTS; i++)
    if (g_clt.sta[i].active && clt_mac_eq(g_clt.sta[i].mac, mac))
      return &g_clt.sta[i];
  return nullptr;
}

static CltStation *clt_alloc(const uint8_t *mac) {
  for (int i = 0; i < CLT_MAX_CLIENTS; i++) {
    if (!g_clt.sta[i].active) {
      memset(&g_clt.sta[i], 0, sizeof(CltStation));
      g_clt.sta[i].active = true;
      memcpy(g_clt.sta[i].mac, mac, 6);
      const char *v = vnd_lookup(mac);
      strncpy(g_clt.sta[i].vendor, v ? v : "Unknown", 31);
      g_clt.sta[i].first_seen_ts = millis() / 1000;
      g_clt.count++;
      g_clt.total_seen++;
      return &g_clt.sta[i];
    }
  }
  // LRU eviction: remove oldest inactive
  uint32_t oldest = UINT32_MAX;
  int idx = 0;
  for (int i = 0; i < CLT_MAX_CLIENTS; i++) {
    if (g_clt.sta[i].state == CLT_STATE_INACTIVE && g_clt.sta[i].last_seen_ts < oldest) {
      oldest = g_clt.sta[i].last_seen_ts;
      idx = i;
    }
  }
  // If no inactive, evict absolute oldest
  if (oldest == UINT32_MAX) {
    for (int i = 0; i < CLT_MAX_CLIENTS; i++) {
      if (g_clt.sta[i].last_seen_ts < oldest) {
        oldest = g_clt.sta[i].last_seen_ts;
        idx = i;
      }
    }
  }
  memset(&g_clt.sta[idx], 0, sizeof(CltStation));
  g_clt.sta[idx].active = true;
  memcpy(g_clt.sta[idx].mac, mac, 6);
  const char *v = vnd_lookup(mac);
  strncpy(g_clt.sta[idx].vendor, v ? v : "Unknown", 31);
  g_clt.sta[idx].first_seen_ts = millis() / 1000;
  g_clt.total_seen++;
  return &g_clt.sta[idx];
}

static bool clt_has_probe(CltStation *s, const char *ssid) {
  for (int i = 0; i < s->probe_count; i++)
    if (strncmp(s->probes[i], ssid, 32) == 0)
      return true;
  return false;
}

static void clt_add_probe(CltStation *s, const char *ssid) {
  if (!ssid || !ssid[0])
    return;
  if (clt_has_probe(s, ssid))
    return;
  int idx = s->probe_count < CLT_MAX_PROBES ? s->probe_count : (s->probe_count % CLT_MAX_PROBES);
  strncpy(s->probes[idx], ssid, 32);
  if (s->probe_count < CLT_MAX_PROBES)
    s->probe_count++;
  g_clt.total_probes++;
}

static void clt_update_assoc(CltStation *s, const uint8_t *bssid, const char *ssid, uint8_t ch,
                             int8_t rssi) {
  if (!bssid || clt_mac_zero(bssid))
    return;

  uint32_t now = millis() / 1000;

  // Check if this is a new BSSID (roam)
  bool is_new_bssid = !clt_mac_zero(s->cur_bssid) && !clt_mac_eq(s->cur_bssid, bssid);
  if (is_new_bssid) {
    s->roam_events++;
    s->last_roam_ts = now;
    s->state = CLT_STATE_ROAMING;
    g_clt.total_roam_events++;
  }

  // Update association history
  bool found = false;
  for (int i = 0; i < CLT_MAX_APS; i++) {
    if (clt_mac_eq(s->assoc_hist[i].bssid, bssid)) {
      s->assoc_hist[i].last_ts = now;
      s->assoc_hist[i].rssi = rssi;
      s->assoc_hist[i].frames++;
      found = true;
      break;
    }
  }
  if (!found) {
    int idx = s->assoc_count < CLT_MAX_APS ? s->assoc_count : (s->assoc_count % CLT_MAX_APS);
    memcpy(s->assoc_hist[idx].bssid, bssid, 6);
    if (ssid)
      strncpy(s->assoc_hist[idx].ssid, ssid, 32);
    s->assoc_hist[idx].ch = ch;
    s->assoc_hist[idx].rssi = rssi;
    s->assoc_hist[idx].first_ts = now;
    s->assoc_hist[idx].last_ts = now;
    s->assoc_hist[idx].frames = 1;
    if (s->assoc_count < CLT_MAX_APS)
      s->assoc_count++;
    g_clt.total_assoc_events++;
  }

  memcpy(s->cur_bssid, bssid, 6);
  if (ssid)
    strncpy(s->cur_ssid, ssid, 32);
  s->cur_ch = ch;
  s->cur_rssi = rssi;
  if (is_new_bssid)
    s->state = CLT_STATE_ASSOCIATED;
}

void clt_ingest_probe(const uint8_t *src, const char *ssid, uint8_t ch, int8_t rssi) {
  if (!src)
    return;
  // Ignore broadcast probes (wildcard SSID)
  CltStation *s = clt_find(src);
  if (!s)
    s = clt_alloc(src);
  if (!s)
    return;

  uint32_t now = millis() / 1000;
  s->last_seen_ts = now;
  s->mgmt_frames++;
  s->total_frames++;

  s->rssi_hist[s->rssi_hist_head % CLT_HISTORY_SAMPLES] = rssi;
  s->rssi_hist_head = (s->rssi_hist_head + 1) % CLT_HISTORY_SAMPLES;

  if (ssid && ssid[0])
    clt_add_probe(s, ssid);
  if (s->state == CLT_STATE_INACTIVE)
    s->state = CLT_STATE_PROBING;
  if (s->state == CLT_STATE_PROBING)
    s->cur_ch = ch;
  s->cur_rssi = rssi;

  // Probe sweep detection
  if (s->probe_count >= 6 && s->state == CLT_STATE_PROBING) {
    s->probe_sweeper = true;
  }
}

void clt_ingest_data(const uint8_t *src, const uint8_t *bssid, const char *ssid, uint8_t ch,
                     int8_t rssi, bool retry, bool ps_flag, uint16_t rate_kbps) {
  if (!src || !bssid)
    return;
  // Filter out multicast sources
  if (src[0] & 0x01)
    return;

  CltStation *s = clt_find(src);
  if (!s)
    s = clt_alloc(src);
  if (!s)
    return;

  uint32_t now = millis() / 1000;
  s->last_seen_ts = now;
  s->data_frames++;
  s->total_frames++;
  if (retry)
    s->retry_frames++;

  s->rssi_hist[s->rssi_hist_head % CLT_HISTORY_SAMPLES] = rssi;
  s->rssi_hist_head = (s->rssi_hist_head + 1) % CLT_HISTORY_SAMPLES;
  s->cur_rssi = rssi;

  if (!clt_mac_zero(bssid)) {
    clt_update_assoc(s, bssid, ssid, ch, rssi);
    s->state = CLT_STATE_ASSOCIATED;
  }

  // Power save tracking
  if (ps_flag && !s->ps_active) {
    s->ps_active = true;
    s->ps_entries++;
    s->ps_null_frames++;
    s->state = CLT_STATE_SLEEPING;
    if (s->ps_entries > 50)
      s->ps_abuser = true;
  } else if (!ps_flag && s->ps_active) {
    s->ps_active = false;
    s->ps_exits++;
    s->state = CLT_STATE_ASSOCIATED;
  }

  if (rate_kbps > 0) {
    s->last_rate_kbps = rate_kbps;
    if (rate_kbps > s->peak_rate_kbps)
      s->peak_rate_kbps = rate_kbps;
  }
}

void clt_ingest_deauth(const uint8_t *src, uint8_t ch, int8_t rssi) {
  if (!src)
    return;
  if (src[0] & 0x01)
    return;

  CltStation *s = clt_find(src);
  if (!s)
    s = clt_alloc(src);
  if (!s)
    return;

  uint32_t now = millis() / 1000;
  s->last_seen_ts = now;
  s->mgmt_frames++;
  s->total_frames++;
  s->deauth_count++;
  s->state = CLT_STATE_DEAUTHED;
  s->cur_rssi = rssi;

  g_clt.total_deauth_events++;
  if (s->deauth_count > 20)
    s->deauth_flooder = true;
}

void clt_ingest_assoc(const uint8_t *src, const uint8_t *bssid, const char *ssid, uint8_t ch,
                      int8_t rssi, uint8_t cap_flags) {
  if (!src)
    return;

  CltStation *s = clt_find(src);
  if (!s)
    s = clt_alloc(src);
  if (!s)
    return;

  uint32_t now = millis() / 1000;
  s->last_seen_ts = now;
  s->mgmt_frames++;
  s->total_frames++;
  s->cap_flags |= cap_flags;
  s->state = CLT_STATE_ASSOCIATING;
  s->cur_rssi = rssi;

  if (bssid && !clt_mac_zero(bssid))
    clt_update_assoc(s, bssid, ssid, ch, rssi);
}

// Called periodically to age inactive clients
void clt_age_clients() {
  uint32_t now = millis() / 1000;
  g_clt.active_now = 0;
  for (int i = 0; i < CLT_MAX_CLIENTS; i++) {
    CltStation &s = g_clt.sta[i];
    if (!s.active)
      continue;
    uint32_t age = now - s.last_seen_ts;
    if (age > CLT_INACTIVE_S && s.state != CLT_STATE_INACTIVE) {
      s.state = CLT_STATE_INACTIVE;
    }
    if (s.state != CLT_STATE_INACTIVE)
      g_clt.active_now++;
    s.active_seconds = now - s.first_seen_ts;
  }

  // Find notable clients
  g_clt.most_active_idx = 0;
  g_clt.most_probes_idx = 0;
  g_clt.most_roams_idx = 0;
  for (int i = 0; i < CLT_MAX_CLIENTS; i++) {
    if (!g_clt.sta[i].active)
      continue;
    if (g_clt.sta[i].total_frames > g_clt.sta[g_clt.most_active_idx].total_frames)
      g_clt.most_active_idx = i;
    if (g_clt.sta[i].probe_count > g_clt.sta[g_clt.most_probes_idx].probe_count)
      g_clt.most_probes_idx = i;
    if (g_clt.sta[i].roam_events > g_clt.sta[g_clt.most_roams_idx].roam_events)
      g_clt.most_roams_idx = i;
  }
}

static const char *clt_state_name(CltState s) {
  switch (s) {
    case CLT_STATE_PROBING:
      return "Probing";
    case CLT_STATE_ASSOCIATING:
      return "Associating";
    case CLT_STATE_ASSOCIATED:
      return "Associated";
    case CLT_STATE_ROAMING:
      return "Roaming";
    case CLT_STATE_SLEEPING:
      return "PowerSave";
    case CLT_STATE_INACTIVE:
      return "Inactive";
    case CLT_STATE_DEAUTHED:
      return "Deauthed";
  }
  return "Unknown";
}

static void clt_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String clt_json_all() {
  String s = "{";
  s += "\"count\":" + String(g_clt.count);
  s += ",\"active\":" + String(g_clt.active_now);
  s += ",\"total_seen\":" + String(g_clt.total_seen);
  s += ",\"total_assoc\":" + String(g_clt.total_assoc_events);
  s += ",\"total_roam\":" + String(g_clt.total_roam_events);
  s += ",\"total_deauth\":" + String(g_clt.total_deauth_events);
  s += ",\"total_probes\":" + String(g_clt.total_probes);
  s += ",\"clients\":[";
  char mac[18];
  bool first = true;
  for (int i = 0; i < CLT_MAX_CLIENTS; i++) {
    CltStation &c = g_clt.sta[i];
    if (!c.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    clt_mac_str(c.mac, mac);
    s += "{\"mac\":\"" + String(mac) + "\"";
    s += ",\"vendor\":\"" + String(c.vendor) + "\"";
    s += ",\"state\":\"" + String(clt_state_name(c.state)) + "\"";
    char bssid_str[18] = "";
    if (!clt_mac_zero(c.cur_bssid))
      clt_mac_str(c.cur_bssid, bssid_str);
    s += ",\"bssid\":\"" + String(bssid_str) + "\"";
    s += ",\"ssid\":\"" + String(c.cur_ssid) + "\"";
    s += ",\"ch\":" + String(c.cur_ch);
    s += ",\"rssi\":" + String(c.cur_rssi);
    s += ",\"frames\":" + String(c.total_frames);
    s += ",\"data\":" + String(c.data_frames);
    s += ",\"mgmt\":" + String(c.mgmt_frames);
    s += ",\"retries\":" + String(c.retry_frames);
    s += ",\"roams\":" + String(c.roam_events);
    s += ",\"probes\":[";
    for (int p = 0; p < c.probe_count; p++) {
      if (p)
        s += ",";
      s += "\"" + String(c.probes[p]) + "\"";
    }
    s += "]";
    s += ",\"probe_count\":" + String(c.probe_count);
    s += ",\"ps\":" + String(c.ps_active ? "true" : "false");
    s += ",\"ps_entries\":" + String(c.ps_entries);
    s += ",\"deauths\":" + String(c.deauth_count);
    s += ",\"peak_kbps\":" + String(c.peak_rate_kbps);
    s += ",\"last_kbps\":" + String(c.last_rate_kbps);
    s += ",\"ht\":" + String((c.cap_flags & CLT_CAP_HT) ? "true" : "false");
    s += ",\"vht\":" + String((c.cap_flags & CLT_CAP_VHT) ? "true" : "false");
    s += ",\"he\":" + String((c.cap_flags & CLT_CAP_HE) ? "true" : "false");
    s += ",\"pmf\":" + String((c.cap_flags & CLT_CAP_PMF) ? "true" : "false");
    s += ",\"btm\":" + String((c.cap_flags & CLT_CAP_BTM) ? "true" : "false");
    s += ",\"flooder\":" + String(c.deauth_flooder ? "true" : "false");
    s += ",\"sweeper\":" + String(c.probe_sweeper ? "true" : "false");
    s += ",\"ps_abuser\":" + String(c.ps_abuser ? "true" : "false");
    s += ",\"first_ts\":" + String(c.first_seen_ts);
    s += ",\"last_ts\":" + String(c.last_seen_ts);
    s += ",\"active_s\":" + String(c.active_seconds);
    s += ",\"rssi_hist\":[";
    for (int r = 0; r < CLT_HISTORY_SAMPLES; r++) {
      if (r)
        s += ",";
      s += String(c.rssi_hist[(c.rssi_hist_head + r) % CLT_HISTORY_SAMPLES]);
    }
    s += "],\"assoc_hist\":[";
    bool afirst = true;
    for (int a = 0; a < CLT_MAX_APS; a++) {
      if (clt_mac_zero(c.assoc_hist[a].bssid))
        continue;
      if (!afirst)
        s += ",";
      afirst = false;
      char abssid[18];
      clt_mac_str(c.assoc_hist[a].bssid, abssid);
      s += "{\"bssid\":\"" + String(abssid) + "\"";
      s += ",\"ssid\":\"" + String(c.assoc_hist[a].ssid) + "\"";
      s += ",\"ch\":" + String(c.assoc_hist[a].ch);
      s += ",\"rssi\":" + String(c.assoc_hist[a].rssi);
      s += ",\"frames\":" + String(c.assoc_hist[a].frames);
      s += ",\"first_ts\":" + String(c.assoc_hist[a].first_ts);
      s += ",\"last_ts\":" + String(c.assoc_hist[a].last_ts) + "}";
    }
    s += "]}";
  }
  s += "]}";
  return s;
}

inline void clt_routes() {
  server.on("/api/clt/all", HTTP_GET, []() {
    clt_age_clients();
    server.send(200, "application/json", clt_json_all());
  });
  server.on("/api/clt/clear", HTTP_POST, []() {
    memset(&g_clt, 0, sizeof(g_clt));
    server.send(200, "application/json", "{\"ok\":true}");
  });
  server.on("/api/clt/active", HTTP_GET, []() {
    clt_age_clients();
    String s = "{\"active\":" + String(g_clt.active_now);
    s += ",\"total\":" + String(g_clt.count);
    s += ",\"roams\":" + String(g_clt.total_roam_events);
    s += "}";
    server.send(200, "application/json", s);
  });
}

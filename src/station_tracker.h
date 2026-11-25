#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <string.h>
#include <stdint.h>

// ── Station Tracker ───────────────────────────────────────────────────────────
//
// Extended per-station lifecycle tracking with connection state machine,
// roaming detection, probe request history, rate monitoring, and alert
// generation for suspicious STA behavior.

#define ST_MAX_STAS 128
#define ST_MAX_PROBES_PER 16
#define ST_MAX_BSSIDS_PER 8
#define ST_MAX_ALERTS 64
#define ST_PROBE_WIN_MS 5000  // probe flood detection window

// ── Station connection state ──────────────────────────────────────────────────
enum StaConnState : uint8_t {
  STA_STATE_UNKNOWN = 0,
  STA_STATE_PROBING = 1,
  STA_STATE_AUTH = 2,
  STA_STATE_ASSOC = 3,
  STA_STATE_CONNECTED = 4,
  STA_STATE_DEAUTHED = 5,
  STA_STATE_ROAMING = 6,
};

static const char *st_conn_state_name(StaConnState s) {
  switch (s) {
    case STA_STATE_UNKNOWN:
      return "unknown";
    case STA_STATE_PROBING:
      return "probing";
    case STA_STATE_AUTH:
      return "auth";
    case STA_STATE_ASSOC:
      return "assoc";
    case STA_STATE_CONNECTED:
      return "connected";
    case STA_STATE_DEAUTHED:
      return "deauthed";
    case STA_STATE_ROAMING:
      return "roaming";
    default:
      return "?";
  }
}

// ── Alert type ────────────────────────────────────────────────────────────────
enum StAlertType : uint8_t {
  ST_ALERT_PROBE_FLOOD = 0,
  ST_ALERT_RAPID_ROAM = 1,
  ST_ALERT_REPEATED_DEAUTH = 2,
  ST_ALERT_KARMA_PROBE = 3,
  ST_ALERT_HIDDEN_PROBE = 4,
  ST_ALERT_HIGH_RATE = 5,
  ST_ALERT_REAPPEAR = 6,
};

static const char *st_alert_name(StAlertType t) {
  switch (t) {
    case ST_ALERT_PROBE_FLOOD:
      return "probe_flood";
    case ST_ALERT_RAPID_ROAM:
      return "rapid_roam";
    case ST_ALERT_REPEATED_DEAUTH:
      return "repeated_deauth";
    case ST_ALERT_KARMA_PROBE:
      return "karma_probe";
    case ST_ALERT_HIDDEN_PROBE:
      return "hidden_probe";
    case ST_ALERT_HIGH_RATE:
      return "high_rate";
    case ST_ALERT_REAPPEAR:
      return "reappear";
    default:
      return "unknown";
  }
}

// ── Probe request record ──────────────────────────────────────────────────────
struct StProbeRecord {
  char ssid[33];  // empty = wildcard/null
  uint32_t ts_ms;
  int8_t rssi;
  bool directed;  // non-broadcast target SSID
};

// ── BSSID association record ──────────────────────────────────────────────────
struct StBssidRecord {
  uint8_t bssid[6];
  uint32_t first_assoc_ms;
  uint32_t last_assoc_ms;
  uint32_t deauth_count;
  uint32_t frame_count;
  bool active;
};

// ── Station record ────────────────────────────────────────────────────────────
struct StaRecord {
  uint8_t mac[6];
  StaConnState state;
  uint8_t current_bssid[6];  // zero = none
  uint8_t prev_bssid[6];     // for roam detection
  uint8_t channel;

  // Probe history
  StProbeRecord probes[ST_MAX_PROBES_PER];
  uint8_t probe_head;
  uint8_t probe_count;
  uint32_t probes_per_window;  // probe flood counter
  uint32_t probe_window_start_ms;

  // Association history
  StBssidRecord bssids[ST_MAX_BSSIDS_PER];
  uint8_t bssid_count;

  // Signal
  int8_t rssi;
  int8_t rssi_min;
  int8_t rssi_max;
  uint32_t rssi_sum;
  uint32_t rssi_samples;

  // Timing
  uint32_t first_seen_ms;
  uint32_t last_seen_ms;
  uint32_t last_state_change_ms;
  uint32_t session_count;  // number of assoc sessions

  // Frame counters
  uint32_t mgmt_frames;
  uint32_t data_frames;
  uint32_t deauth_rx_count;
  uint32_t deauth_tx_count;
  uint32_t roam_count;

  // Rate
  uint32_t bytes_est;  // estimated bytes transferred
  uint32_t rate_kbps;

  // Identification
  char vendor[24];
  char os_guess[16];
  bool randomized_mac;  // LAA bit set

  // Alert tracking
  uint8_t alert_flags;  // bitmask of StAlertType
};

// ── Alert record ──────────────────────────────────────────────────────────────
struct StAlert {
  uint8_t mac[6];
  StAlertType type;
  uint32_t ts_ms;
  char detail[48];
};

// ── Tracker state ─────────────────────────────────────────────────────────────
struct StState {
  StaRecord stas[ST_MAX_STAS];
  uint16_t sta_count;
  StAlert alerts[ST_MAX_ALERTS];
  uint8_t alert_head;
  uint8_t alert_count;
  uint32_t total_frames;
  uint32_t total_probes;
  uint32_t total_assocs;
  uint32_t total_deauths;
};

static StState g_st = {};

// ── Find or create station ────────────────────────────────────────────────────
static StaRecord *st_upsert(const uint8_t mac[6]) {
  for (uint16_t i = 0; i < g_st.sta_count; i++) {
    if (memcmp(g_st.stas[i].mac, mac, 6) == 0)
      return &g_st.stas[i];
  }
  if (g_st.sta_count >= ST_MAX_STAS) {
    // Evict oldest inactive
    uint32_t oldest = g_st.stas[0].last_seen_ms;
    uint16_t oi = 0;
    for (uint16_t i = 1; i < g_st.sta_count; i++) {
      if (g_st.stas[i].state <= STA_STATE_PROBING && g_st.stas[i].last_seen_ms < oldest) {
        oldest = g_st.stas[i].last_seen_ms;
        oi = i;
      }
    }
    memset(&g_st.stas[oi], 0, sizeof(StaRecord));
    memcpy(g_st.stas[oi].mac, mac, 6);
    g_st.stas[oi].first_seen_ms = millis();
    g_st.stas[oi].rssi_min = -100;
    g_st.stas[oi].rssi_max = -10;
    return &g_st.stas[oi];
  }
  StaRecord *s = &g_st.stas[g_st.sta_count++];
  memset(s, 0, sizeof(*s));
  memcpy(s->mac, mac, 6);
  s->first_seen_ms = millis();
  s->rssi_min = -100;
  s->rssi_max = -10;
  return s;
}

// ── Raise alert ───────────────────────────────────────────────────────────────
static void st_alert(const uint8_t mac[6], StAlertType type, const char *detail) {
  StAlert *a = &g_st.alerts[g_st.alert_head];
  if (mac)
    memcpy(a->mac, mac, 6);
  else
    memset(a->mac, 0, 6);
  a->type = type;
  a->ts_ms = millis();
  strncpy(a->detail, detail ? detail : "", 47);
  a->detail[47] = 0;
  g_st.alert_head = (g_st.alert_head + 1) % ST_MAX_ALERTS;
  if (g_st.alert_count < ST_MAX_ALERTS)
    g_st.alert_count++;
}

// ── RSSI update ───────────────────────────────────────────────────────────────
static void st_update_rssi(StaRecord *s, int8_t rssi) {
  s->rssi = rssi;
  if (rssi < s->rssi_min)
    s->rssi_min = rssi;
  if (rssi > s->rssi_max)
    s->rssi_max = rssi;
  s->rssi_sum += (uint32_t)(rssi + 100);
  s->rssi_samples++;
}

// ── Probe request ingestion ───────────────────────────────────────────────────
static void st_ingest_probe(const uint8_t mac[6], const char *ssid, int8_t rssi, uint8_t channel) {
  if (!mac)
    return;
  // Ignore broadcast source
  if (mac[0] & 1)
    return;
  g_st.total_probes++;
  StaRecord *s = st_upsert(mac);
  s->last_seen_ms = millis();
  s->channel = channel;
  st_update_rssi(s, rssi);
  s->mgmt_frames++;
  g_st.total_frames++;

  // Check randomized MAC (LAA bit)
  s->randomized_mac = (mac[0] & 0x02) ? true : false;

  // Store probe
  StProbeRecord *p = &s->probes[s->probe_head];
  if (ssid) {
    strncpy(p->ssid, ssid, 32);
    p->ssid[32] = 0;
    p->directed = (ssid[0] != 0);
  } else {
    p->ssid[0] = 0;
    p->directed = false;
  }
  p->ts_ms = millis();
  p->rssi = rssi;
  s->probe_head = (s->probe_head + 1) % ST_MAX_PROBES_PER;
  if (s->probe_count < ST_MAX_PROBES_PER)
    s->probe_count++;

  // Update state
  if (s->state < STA_STATE_PROBING)
    s->state = STA_STATE_PROBING;

  // Probe flood detection
  uint32_t now = millis();
  if (now - s->probe_window_start_ms > ST_PROBE_WIN_MS) {
    s->probe_window_start_ms = now;
    s->probes_per_window = 0;
  }
  s->probes_per_window++;
  if (s->probes_per_window > 20 && !(s->alert_flags & (1 << ST_ALERT_PROBE_FLOOD))) {
    s->alert_flags |= (1 << ST_ALERT_PROBE_FLOOD);
    char detail[64];
    snprintf(detail, sizeof(detail), "Probe flood: %u probes in %ums",
             (unsigned)s->probes_per_window, (unsigned)ST_PROBE_WIN_MS);
    st_alert(mac, ST_ALERT_PROBE_FLOOD, detail);
  }

  // Hidden SSID probe
  if (ssid && ssid[0] && !(s->alert_flags & (1 << ST_ALERT_HIDDEN_PROBE))) {
    // Directed probe to non-broadcast SSID is worth noting
    s->alert_flags |= (1 << ST_ALERT_HIDDEN_PROBE);
    char detail[64];
    snprintf(detail, sizeof(detail), "Directed probe: %s", ssid);
    st_alert(mac, ST_ALERT_HIDDEN_PROBE, detail);
  }
}

// ── Association ingestion ─────────────────────────────────────────────────────
static void st_ingest_assoc(const uint8_t sta_mac[6], const uint8_t bssid[6], uint8_t channel,
                            int8_t rssi) {
  if (!sta_mac || !bssid)
    return;
  if (sta_mac[0] & 1)
    return;
  g_st.total_assocs++;
  StaRecord *s = st_upsert(sta_mac);
  s->last_seen_ms = millis();
  s->channel = channel;
  st_update_rssi(s, rssi);
  s->mgmt_frames++;
  g_st.total_frames++;

  // Check roaming
  bool prev_had_bssid = false;
  for (int i = 0; i < 6; i++)
    if (s->current_bssid[i]) {
      prev_had_bssid = true;
      break;
    }
  if (prev_had_bssid && memcmp(s->current_bssid, bssid, 6) != 0) {
    // Roaming event
    memcpy(s->prev_bssid, s->current_bssid, 6);
    s->roam_count++;
    uint32_t time_on_prev = millis() - s->last_state_change_ms;
    if (time_on_prev < 3000) {
      char detail[64];
      snprintf(detail, sizeof(detail), "Rapid roam: %ums on prev AP", (unsigned)time_on_prev);
      st_alert(sta_mac, ST_ALERT_RAPID_ROAM, detail);
    }
    s->state = STA_STATE_ROAMING;
  } else {
    s->state = STA_STATE_ASSOC;
  }

  memcpy(s->current_bssid, bssid, 6);
  s->last_state_change_ms = millis();
  s->session_count++;

  // Track BSSID history
  bool found = false;
  for (uint8_t i = 0; i < s->bssid_count; i++) {
    if (memcmp(s->bssids[i].bssid, bssid, 6) == 0) {
      s->bssids[i].last_assoc_ms = millis();
      s->bssids[i].frame_count++;
      s->bssids[i].active = true;
      found = true;
      break;
    }
  }
  if (!found && s->bssid_count < ST_MAX_BSSIDS_PER) {
    StBssidRecord &br = s->bssids[s->bssid_count++];
    memcpy(br.bssid, bssid, 6);
    br.first_assoc_ms = millis();
    br.last_assoc_ms = millis();
    br.frame_count = 1;
    br.active = true;
  }
}

// ── Deauth ingestion ──────────────────────────────────────────────────────────
static void st_ingest_deauth(const uint8_t sta_mac[6], bool as_src) {
  if (!sta_mac || sta_mac[0] & 1)
    return;
  g_st.total_deauths++;
  StaRecord *s = st_upsert(sta_mac);
  s->last_seen_ms = millis();
  if (as_src)
    s->deauth_tx_count++;
  else
    s->deauth_rx_count++;
  s->mgmt_frames++;
  s->state = STA_STATE_DEAUTHED;
  memset(s->current_bssid, 0, 6);
  g_st.total_frames++;

  if (s->deauth_rx_count > 5 && !(s->alert_flags & (1 << ST_ALERT_REPEATED_DEAUTH))) {
    s->alert_flags |= (1 << ST_ALERT_REPEATED_DEAUTH);
    char detail[64];
    snprintf(detail, sizeof(detail), "Deauth storm: %u deauths received",
             (unsigned)s->deauth_rx_count);
    st_alert(sta_mac, ST_ALERT_REPEATED_DEAUTH, detail);
  }
}

// ── Data frame ingestion ──────────────────────────────────────────────────────
static void st_ingest_data(const uint8_t sta_mac[6], uint16_t frame_len, uint8_t channel,
                           int8_t rssi) {
  if (!sta_mac || sta_mac[0] & 1)
    return;
  StaRecord *s = st_upsert(sta_mac);
  s->last_seen_ms = millis();
  s->channel = channel;
  st_update_rssi(s, rssi);
  s->data_frames++;
  s->bytes_est += frame_len;
  g_st.total_frames++;
  if (s->state < STA_STATE_CONNECTED)
    s->state = STA_STATE_CONNECTED;
}

// ── Aging ─────────────────────────────────────────────────────────────────────
static void st_age(uint32_t stale_ms) {
  uint32_t now = millis();
  for (uint16_t i = 0; i < g_st.sta_count; i++) {
    StaRecord &s = g_st.stas[i];
    if (now - s.last_seen_ms > stale_ms) {
      if (s.state == STA_STATE_CONNECTED || s.state == STA_STATE_ASSOC) {
        s.state = STA_STATE_UNKNOWN;
        memset(s.current_bssid, 0, 6);
      }
    }
  }
}

// ── Station JSON ──────────────────────────────────────────────────────────────
static String st_sta_json(const StaRecord &s) {
  char mac[18];
  snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X", s.mac[0], s.mac[1], s.mac[2],
           s.mac[3], s.mac[4], s.mac[5]);
  char bssid[18] = "";
  bool has_bssid = false;
  for (int i = 0; i < 6; i++)
    if (s.current_bssid[i]) {
      has_bssid = true;
      break;
    }
  if (has_bssid)
    snprintf(bssid, sizeof(bssid), "%02X:%02X:%02X:%02X:%02X:%02X", s.current_bssid[0],
             s.current_bssid[1], s.current_bssid[2], s.current_bssid[3], s.current_bssid[4],
             s.current_bssid[5]);

  String j = "{\"mac\":\"";
  j += mac;
  j += "\"";
  j += ",\"state\":\"";
  j += st_conn_state_name(s.state);
  j += "\"";
  j += ",\"bssid\":\"";
  j += bssid;
  j += "\"";
  j += ",\"channel\":";
  j += s.channel;
  j += ",\"rssi\":";
  j += s.rssi;
  int8_t avg_rssi = (s.rssi_samples > 0) ? (int8_t)(s.rssi_sum / s.rssi_samples - 100) : s.rssi;
  j += ",\"rssi_avg\":";
  j += avg_rssi;
  j += ",\"rssi_min\":";
  j += s.rssi_min;
  j += ",\"rssi_max\":";
  j += s.rssi_max;
  j += ",\"vendor\":\"";
  j += s.vendor;
  j += "\"";
  j += ",\"os\":\"";
  j += s.os_guess;
  j += "\"";
  j += ",\"rand_mac\":";
  j += s.randomized_mac ? 1 : 0;
  j += ",\"probes\":";
  j += s.probe_count;
  j += ",\"data_frames\":";
  j += s.data_frames;
  j += ",\"deauth_rx\":";
  j += s.deauth_rx_count;
  j += ",\"roams\":";
  j += s.roam_count;
  j += ",\"sessions\":";
  j += s.session_count;
  j += ",\"bytes_est\":";
  j += s.bytes_est;
  j += ",\"bssids\":";
  j += s.bssid_count;
  j += ",\"alert_flags\":";
  j += s.alert_flags;
  uint32_t age = (millis() - s.last_seen_ms) / 1000;
  j += ",\"age_s\":";
  j += age;
  j += "}";
  return j;
}

// ── List JSON ─────────────────────────────────────────────────────────────────
static String st_list_json(int page, int per) {
  if (per < 1 || per > 50)
    per = 25;
  int start = page * per;
  int end = start + per;
  if (end > (int)g_st.sta_count)
    end = g_st.sta_count;
  String j = "{\"total\":" + String(g_st.sta_count);
  j += ",\"page\":" + String(page) + ",\"per\":" + String(per);
  j += ",\"stas\":[";
  for (int i = start; i < end; i++) {
    if (i > start)
      j += ",";
    j += st_sta_json(g_st.stas[i]);
  }
  j += "]}";
  return j;
}

// ── Alerts JSON ───────────────────────────────────────────────────────────────
static String st_alerts_json(int max_n) {
  if (max_n < 1 || max_n > ST_MAX_ALERTS)
    max_n = ST_MAX_ALERTS;
  String j = "{\"count\":" + String(g_st.alert_count) + ",\"alerts\":[";
  int start = (g_st.alert_head + ST_MAX_ALERTS - g_st.alert_count) % ST_MAX_ALERTS;
  int shown = 0;
  for (int i = g_st.alert_count - 1; i >= 0 && shown < max_n; i--) {
    if (shown)
      j += ",";
    int idx = (start + i) % ST_MAX_ALERTS;
    StAlert &a = g_st.alerts[idx];
    char mac[18];
    snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X", a.mac[0], a.mac[1], a.mac[2],
             a.mac[3], a.mac[4], a.mac[5]);
    j += "{\"mac\":\"";
    j += mac;
    j += "\"";
    j += ",\"type\":\"";
    j += st_alert_name(a.type);
    j += "\"";
    j += ",\"ts\":";
    j += a.ts_ms;
    j += ",\"detail\":\"";
    j += a.detail;
    j += "\"";
    j += "}";
    shown++;
  }
  j += "]}";
  return j;
}

// ── Summary JSON ──────────────────────────────────────────────────────────────
static String st_summary_json() {
  uint32_t connected = 0, probing = 0, rand_mac = 0;
  for (uint16_t i = 0; i < g_st.sta_count; i++) {
    if (g_st.stas[i].state >= STA_STATE_ASSOC)
      connected++;
    if (g_st.stas[i].state == STA_STATE_PROBING)
      probing++;
    if (g_st.stas[i].randomized_mac)
      rand_mac++;
  }
  String j = "{\"total\":" + String(g_st.sta_count);
  j += ",\"connected\":" + String(connected);
  j += ",\"probing\":" + String(probing);
  j += ",\"rand_mac\":" + String(rand_mac);
  j += ",\"total_frames\":" + String(g_st.total_frames);
  j += ",\"total_probes\":" + String(g_st.total_probes);
  j += ",\"total_assocs\":" + String(g_st.total_assocs);
  j += ",\"total_deauths\":" + String(g_st.total_deauths);
  j += ",\"alerts\":" + String(g_st.alert_count);
  j += "}";
  return j;
}

// ── HTTP routes ───────────────────────────────────────────────────────────────
static void st_routes() {
  extern WebServer server;

  server.on("/api/st/stas", HTTP_GET, []() {
    int page = server.hasArg("page") ? server.arg("page").toInt() : 0;
    int per = server.hasArg("per") ? server.arg("per").toInt() : 25;
    server.send(200, "application/json", st_list_json(page, per));
  });

  server.on("/api/st/alerts", HTTP_GET, []() {
    int n = server.hasArg("n") ? server.arg("n").toInt() : 20;
    server.send(200, "application/json", st_alerts_json(n));
  });

  server.on("/api/st/summary", HTTP_GET,
            []() { server.send(200, "application/json", st_summary_json()); });

  server.on("/api/st/clear", HTTP_POST, []() {
    memset(&g_st, 0, sizeof(g_st));
    server.send(200, "application/json", "{\"ok\":1}");
  });

  server.on("/api/st/age", HTTP_POST, []() {
    uint32_t ms = server.hasArg("ms") ? server.arg("ms").toInt() : 60000;
    st_age(ms);
    server.send(200, "application/json", "{\"ok\":1}");
  });

  server.on("/api/st/sta", HTTP_GET, []() {
    if (!server.hasArg("mac")) {
      server.send(400, "application/json", "{\"error\":\"mac required\"}");
      return;
    }
    String mac_s = server.arg("mac");
    uint8_t mac[6];
    unsigned b[6];
    if (sscanf(mac_s.c_str(), "%x:%x:%x:%x:%x:%x", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) != 6) {
      server.send(400, "application/json", "{\"error\":\"invalid mac\"}");
      return;
    }
    for (int i = 0; i < 6; i++)
      mac[i] = (uint8_t)b[i];
    for (uint16_t i = 0; i < g_st.sta_count; i++) {
      if (memcmp(g_st.stas[i].mac, mac, 6) == 0) {
        server.send(200, "application/json", st_sta_json(g_st.stas[i]));
        return;
      }
    }
    server.send(404, "application/json", "{\"error\":\"not found\"}");
  });
}

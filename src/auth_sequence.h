#pragma once
// auth_sequence.h — 802.11 authentication & association sequence tracker.
// Tracks full join sequence: Probe Req/Rsp → Auth → Assoc → 4-way EAPOL.
// Detects anomalies: auth timeouts, repeated failures, out-of-order frames.
// Prefix: aus_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define AUS_MAX_SESSIONS 48
#define AUS_MAX_EVENTS 16  // per session
#define AUS_TIMEOUT_S 30

// Sequence step flags
enum AusStep : uint8_t {
  AUS_STEP_NONE = 0x00,
  AUS_STEP_PROBE_REQ = 0x01,
  AUS_STEP_PROBE_RSP = 0x02,
  AUS_STEP_AUTH_REQ = 0x04,
  AUS_STEP_AUTH_RSP = 0x08,
  AUS_STEP_ASSOC_REQ = 0x10,
  AUS_STEP_ASSOC_RSP = 0x20,
  AUS_STEP_EAPOL_MSG1 = 0x40,
  AUS_STEP_EAPOL_MSG2 = 0x80
};

// Session state
enum AusState : uint8_t {
  AUS_STATE_PROBING = 0,
  AUS_STATE_AUTHING = 1,
  AUS_STATE_ASSOC = 2,
  AUS_STATE_HANDSHAKE = 3,
  AUS_STATE_CONNECTED = 4,
  AUS_STATE_FAILED = 5,
  AUS_STATE_TIMEOUT = 6
};

// Auth failure reasons
enum AusFailReason : uint8_t {
  AUS_FAIL_NONE = 0,
  AUS_FAIL_AUTH_REJECT = 1,
  AUS_FAIL_ASSOC_REJECT = 2,
  AUS_FAIL_DEAUTH = 3,
  AUS_FAIL_TIMEOUT = 4,
  AUS_FAIL_MIC_ERR = 5,
  AUS_FAIL_OUT_OF_ORDER = 6
};

struct AusEvent {
  uint32_t ts;
  uint8_t step;
  bool success;
  uint16_t status_code;  // from auth/assoc response
  uint16_t reason_code;  // from deauth/disassoc
};

struct AusSession {
  bool active;
  uint8_t client_mac[6];
  uint8_t ap_bssid[6];
  char ssid[33];
  uint8_t ch;
  int8_t rssi;

  AusState state;
  AusFailReason fail_reason;
  uint8_t steps_done;  // bitmask of AusStep
  uint8_t retry_count;
  uint8_t assoc_reject_count;

  AusEvent events[AUS_MAX_EVENTS];
  uint8_t event_count;

  uint32_t probe_ts;
  uint32_t auth_ts;
  uint32_t assoc_ts;
  uint32_t eapol_ts;
  uint32_t complete_ts;

  // Timing (ms)
  uint32_t probe_to_auth_ms;
  uint32_t auth_to_assoc_ms;
  uint32_t assoc_to_eapol_ms;
  uint32_t total_join_ms;

  uint32_t first_ts;
  uint32_t last_ts;
};

// Global state
static struct {
  AusSession sessions[AUS_MAX_SESSIONS];
  uint16_t count;
  uint32_t total_joins;
  uint32_t total_failures;
  uint32_t total_timeouts;
  // Timing histogram: join time buckets in ms (0-100, 100-500, 500-2000, >2000)
  uint32_t join_time_hist[4];
} g_aus = {};

static bool aus_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static AusSession *aus_find(const uint8_t *client, const uint8_t *ap) {
  for (int i = 0; i < AUS_MAX_SESSIONS; i++) {
    AusSession &s = g_aus.sessions[i];
    if (s.active && aus_mac_eq(s.client_mac, client) && aus_mac_eq(s.ap_bssid, ap))
      return &s;
  }
  return nullptr;
}

static AusSession *aus_find_or_alloc(const uint8_t *client, const uint8_t *ap) {
  AusSession *s = aus_find(client, ap);
  if (s)
    return s;
  for (int i = 0; i < AUS_MAX_SESSIONS; i++) {
    if (!g_aus.sessions[i].active) {
      AusSession &ns = g_aus.sessions[i];
      memset(&ns, 0, sizeof(ns));
      ns.active = true;
      memcpy(ns.client_mac, client, 6);
      memcpy(ns.ap_bssid, ap, 6);
      ns.state = AUS_STATE_PROBING;
      ns.first_ts = millis() / 1000;
      g_aus.count++;
      return &ns;
    }
  }
  // Evict oldest completed or failed session
  uint32_t oldest = UINT32_MAX;
  int idx = 0;
  for (int i = 0; i < AUS_MAX_SESSIONS; i++) {
    AusSession &s2 = g_aus.sessions[i];
    if ((s2.state == AUS_STATE_CONNECTED || s2.state == AUS_STATE_FAILED ||
         s2.state == AUS_STATE_TIMEOUT) &&
        s2.last_ts < oldest) {
      oldest = s2.last_ts;
      idx = i;
    }
  }
  AusSession &evicted = g_aus.sessions[idx];
  memset(&evicted, 0, sizeof(AusSession));
  evicted.active = true;
  memcpy(evicted.client_mac, client, 6);
  memcpy(evicted.ap_bssid, ap, 6);
  evicted.state = AUS_STATE_PROBING;
  evicted.first_ts = millis() / 1000;
  return &evicted;
}

static void aus_add_event(AusSession &s, uint8_t step, bool ok, uint16_t status = 0,
                          uint16_t reason = 0) {
  if (s.event_count >= AUS_MAX_EVENTS) {
    // Shift to make room
    memmove(&s.events[0], &s.events[1], (AUS_MAX_EVENTS - 1) * sizeof(AusEvent));
    s.event_count = AUS_MAX_EVENTS - 1;
  }
  AusEvent &e = s.events[s.event_count++];
  e.ts = millis() / 1000;
  e.step = step;
  e.success = ok;
  e.status_code = status;
  e.reason_code = reason;
}

void aus_ingest_probe_req(const uint8_t *client, const uint8_t *ap, const char *ssid, uint8_t ch,
                          int8_t rssi) {
  AusSession *s = aus_find_or_alloc(client, ap);
  if (!s)
    return;
  if (ssid && ssid[0])
    strncpy(s->ssid, ssid, 32);
  s->ch = ch;
  s->rssi = rssi;
  s->steps_done |= AUS_STEP_PROBE_REQ;
  s->probe_ts = millis();
  s->last_ts = millis() / 1000;
  aus_add_event(*s, AUS_STEP_PROBE_REQ, true);
}

void aus_ingest_probe_rsp(const uint8_t *ap, const uint8_t *client, uint8_t ch, int8_t rssi) {
  AusSession *s = aus_find(client, ap);
  if (!s)
    return;
  s->ch = ch;
  s->rssi = rssi;
  s->steps_done |= AUS_STEP_PROBE_RSP;
  s->last_ts = millis() / 1000;
  aus_add_event(*s, AUS_STEP_PROBE_RSP, true);
}

void aus_ingest_auth(const uint8_t *src, const uint8_t *dst, bool is_response, bool success,
                     uint16_t status_code, uint8_t ch, int8_t rssi) {
  const uint8_t *client = is_response ? dst : src;
  const uint8_t *ap = is_response ? src : dst;
  AusSession *s = aus_find_or_alloc(client, ap);
  if (!s)
    return;
  s->ch = ch;
  s->rssi = rssi;

  if (!is_response) {
    s->steps_done |= AUS_STEP_AUTH_REQ;
    s->auth_ts = millis();
    if (s->probe_ts > 0)
      s->probe_to_auth_ms = millis() - s->probe_ts;
    s->state = AUS_STATE_AUTHING;
    aus_add_event(*s, AUS_STEP_AUTH_REQ, true);
  } else {
    s->steps_done |= AUS_STEP_AUTH_RSP;
    if (!success) {
      s->state = AUS_STATE_FAILED;
      s->fail_reason = AUS_FAIL_AUTH_REJECT;
      g_aus.total_failures++;
    }
    aus_add_event(*s, AUS_STEP_AUTH_RSP, success, status_code);
  }
  s->last_ts = millis() / 1000;
}

void aus_ingest_assoc(const uint8_t *src, const uint8_t *dst, bool is_response, bool success,
                      uint16_t status_code, uint8_t ch, int8_t rssi) {
  const uint8_t *client = is_response ? dst : src;
  const uint8_t *ap = is_response ? src : dst;
  AusSession *s = aus_find(client, ap);
  if (!s)
    s = aus_find_or_alloc(client, ap);
  if (!s)
    return;
  s->ch = ch;
  s->rssi = rssi;

  if (!is_response) {
    s->steps_done |= AUS_STEP_ASSOC_REQ;
    s->assoc_ts = millis();
    if (s->auth_ts > 0)
      s->auth_to_assoc_ms = millis() - s->auth_ts;
    s->state = AUS_STATE_ASSOC;
    aus_add_event(*s, AUS_STEP_ASSOC_REQ, true);
  } else {
    s->steps_done |= AUS_STEP_ASSOC_RSP;
    if (!success) {
      s->assoc_reject_count++;
      if (s->assoc_reject_count >= 3) {
        s->state = AUS_STATE_FAILED;
        s->fail_reason = AUS_FAIL_ASSOC_REJECT;
        g_aus.total_failures++;
      }
    } else {
      s->state = AUS_STATE_HANDSHAKE;
    }
    aus_add_event(*s, AUS_STEP_ASSOC_RSP, success, status_code);
  }
  s->last_ts = millis() / 1000;
}

void aus_ingest_eapol(const uint8_t *client, const uint8_t *ap, uint8_t msg_num) {
  AusSession *s = aus_find(client, ap);
  if (!s)
    return;

  if (msg_num == 1) {
    s->steps_done |= AUS_STEP_EAPOL_MSG1;
    s->eapol_ts = millis();
    if (s->assoc_ts > 0)
      s->assoc_to_eapol_ms = millis() - s->assoc_ts;
    aus_add_event(*s, AUS_STEP_EAPOL_MSG1, true);
  } else if (msg_num == 2) {
    s->steps_done |= AUS_STEP_EAPOL_MSG2;
    aus_add_event(*s, AUS_STEP_EAPOL_MSG2, true);
  } else if (msg_num == 4) {
    s->state = AUS_STATE_CONNECTED;
    s->complete_ts = millis();
    if (s->probe_ts > 0) {
      s->total_join_ms = millis() - s->probe_ts;
      // Classify join time
      if (s->total_join_ms < 100)
        g_aus.join_time_hist[0]++;
      else if (s->total_join_ms < 500)
        g_aus.join_time_hist[1]++;
      else if (s->total_join_ms < 2000)
        g_aus.join_time_hist[2]++;
      else
        g_aus.join_time_hist[3]++;
    }
    g_aus.total_joins++;
  }
  s->last_ts = millis() / 1000;
}

static void aus_check_timeouts() {
  uint32_t now_s = millis() / 1000;
  for (int i = 0; i < AUS_MAX_SESSIONS; i++) {
    AusSession &s = g_aus.sessions[i];
    if (!s.active || s.state == AUS_STATE_CONNECTED || s.state == AUS_STATE_FAILED ||
        s.state == AUS_STATE_TIMEOUT)
      continue;
    if (now_s - s.last_ts > AUS_TIMEOUT_S) {
      s.state = AUS_STATE_TIMEOUT;
      s.fail_reason = AUS_FAIL_TIMEOUT;
      g_aus.total_timeouts++;
    }
  }
}

static void aus_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static const char *aus_state_str(AusState st) {
  switch (st) {
    case AUS_STATE_PROBING:
      return "Probing";
    case AUS_STATE_AUTHING:
      return "Auth";
    case AUS_STATE_ASSOC:
      return "Assoc";
    case AUS_STATE_HANDSHAKE:
      return "Handshake";
    case AUS_STATE_CONNECTED:
      return "Connected";
    case AUS_STATE_FAILED:
      return "Failed";
    case AUS_STATE_TIMEOUT:
      return "Timeout";
  }
  return "Unknown";
}

static const char *aus_fail_str(AusFailReason f) {
  switch (f) {
    case AUS_FAIL_NONE:
      return "";
    case AUS_FAIL_AUTH_REJECT:
      return "Auth Rejected";
    case AUS_FAIL_ASSOC_REJECT:
      return "Assoc Rejected";
    case AUS_FAIL_DEAUTH:
      return "Deauth";
    case AUS_FAIL_TIMEOUT:
      return "Timeout";
    case AUS_FAIL_MIC_ERR:
      return "MIC Error";
    case AUS_FAIL_OUT_OF_ORDER:
      return "Out of Order";
  }
  return "";
}

static String aus_json_all() {
  aus_check_timeouts();
  String s = "{\"count\":" + String(g_aus.count);
  s += ",\"total_joins\":" + String(g_aus.total_joins);
  s += ",\"total_failures\":" + String(g_aus.total_failures);
  s += ",\"total_timeouts\":" + String(g_aus.total_timeouts);
  s += ",\"join_time_hist\":[" + String(g_aus.join_time_hist[0]) + "," +
       String(g_aus.join_time_hist[1]) + "," + String(g_aus.join_time_hist[2]) + "," +
       String(g_aus.join_time_hist[3]) + "]";
  s += ",\"sessions\":[";
  char mac[18];
  bool first = true;
  for (int i = 0; i < AUS_MAX_SESSIONS; i++) {
    AusSession &ss = g_aus.sessions[i];
    if (!ss.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    aus_mac_str(ss.client_mac, mac);
    s += "{\"client\":\"" + String(mac) + "\"";
    aus_mac_str(ss.ap_bssid, mac);
    s += ",\"ap\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(ss.ssid) + "\"";
    s += ",\"ch\":" + String(ss.ch);
    s += ",\"rssi\":" + String(ss.rssi);
    s += ",\"state\":\"" + String(aus_state_str(ss.state)) + "\"";
    s += ",\"fail\":\"" + String(aus_fail_str(ss.fail_reason)) + "\"";
    s += ",\"steps\":" + String(ss.steps_done);
    s += ",\"retries\":" + String(ss.retry_count);
    s += ",\"probe_to_auth\":" + String(ss.probe_to_auth_ms);
    s += ",\"auth_to_assoc\":" + String(ss.auth_to_assoc_ms);
    s += ",\"assoc_to_eapol\":" + String(ss.assoc_to_eapol_ms);
    s += ",\"total_ms\":" + String(ss.total_join_ms);
    s += ",\"last_ts\":" + String(ss.last_ts) + "}";
  }
  s += "]}";
  return s;
}

inline void aus_routes() {
  server.on("/api/aus/all", HTTP_GET,
            []() { server.send(200, "application/json", aus_json_all()); });
  server.on("/api/aus/clear", HTTP_POST, []() {
    memset(&g_aus, 0, sizeof(g_aus));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

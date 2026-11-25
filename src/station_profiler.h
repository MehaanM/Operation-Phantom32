#pragma once
// station_profiler.h — Per-client station behavior profiler.
// Tracks each observed client MAC: probe patterns, association history,
// roaming events, power-save state, data volume, and behavioral fingerprint.
// Prefix: stp_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define STP_MAX_STATIONS 48
#define STP_MAX_PROBES 8      // unique SSIDs probed per station
#define STP_MAX_ASSOC_HIST 6  // association history per station
#define STP_HIST_LEN 30

// Station state
enum StpState : uint8_t {
  STP_ST_UNSEEN = 0,
  STP_ST_PROBING = 1,
  STP_ST_ASSOCIATED = 2,
  STP_ST_AUTHENTICATED = 3,
  STP_ST_ROAMING = 4,
  STP_ST_DORMANT = 5,
};

// Power-save state
enum StpPwrSave : uint8_t {
  STP_PS_UNKNOWN = 0,
  STP_PS_ACTIVE = 1,
  STP_PS_DOZE = 2,
  STP_PS_UAPSD = 3,
};

// Probe entry
struct StpProbe {
  char ssid[33];
  uint8_t ch;
  uint32_t last_ts;
  uint16_t count;
};

// Association history entry
struct StpAssocHistEntry {
  uint8_t bssid[6];
  char ssid[33];
  uint8_t ch;
  uint32_t assoc_ts;
  uint32_t deassoc_ts;
  uint16_t duration_s;
};

struct StpStation {
  bool active;
  uint8_t mac[6];

  StpState state;
  StpPwrSave pwr_save;

  // Manufacturer OUI info
  char oui_vendor[32];

  // Associated AP
  uint8_t ap_bssid[6];
  char ap_ssid[33];
  uint8_t ap_ch;
  int8_t rssi;

  // Probe history
  StpProbe probes[STP_MAX_PROBES];
  uint8_t probe_count;
  uint32_t total_probes;
  bool is_wildcard_prober;

  // Association history (ring)
  StpAssocHistEntry assoc_hist[STP_MAX_ASSOC_HIST];
  uint8_t assoc_hist_head;
  uint8_t assoc_count;

  // Roaming
  uint16_t roam_count;
  uint32_t last_roam_ts;

  // Traffic
  uint32_t data_frames;
  uint32_t data_bytes;
  uint32_t retry_frames;
  uint32_t mgmt_frames;

  // Rate history (frames per sec)
  uint8_t rate_hist[STP_HIST_LEN];
  uint8_t hist_head;
  uint32_t cur_frames;
  uint32_t last_tick_s;

  // Power-save events
  uint32_t ps_enter_count;
  uint32_t ps_exit_count;

  // Timestamps
  uint32_t first_ts;
  uint32_t last_ts;

  // 802.11r/k/v support flags
  bool cap_ft;
  bool cap_rrm;
  bool cap_btm;
  bool is_random_mac;
};

static struct {
  StpStation stations[STP_MAX_STATIONS];
  uint8_t count;
  uint32_t total_frames;
} g_stp = {};

static bool stp_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}
static bool stp_is_bcast(const uint8_t *m) {
  return m[0] == 0xff && m[1] == 0xff;
}
static bool stp_is_mcast(const uint8_t *m) {
  return m[0] & 0x01;
}

static bool stp_is_random_mac(const uint8_t *m) {
  return (m[0] & 0x02) != 0;  // locally administered bit
}

static StpStation *stp_find(const uint8_t *mac) {
  for (int i = 0; i < STP_MAX_STATIONS; i++)
    if (g_stp.stations[i].active && stp_mac_eq(g_stp.stations[i].mac, mac))
      return &g_stp.stations[i];
  return nullptr;
}

static StpStation *stp_alloc(const uint8_t *mac) {
  for (int i = 0; i < STP_MAX_STATIONS; i++) {
    if (!g_stp.stations[i].active) {
      StpStation &s = g_stp.stations[i];
      memset(&s, 0, sizeof(s));
      s.active = true;
      s.state = STP_ST_PROBING;
      s.pwr_save = STP_PS_UNKNOWN;
      memcpy(s.mac, mac, 6);
      s.first_ts = millis() / 1000;
      s.is_random_mac = stp_is_random_mac(mac);
      g_stp.count++;
      return &s;
    }
  }
  uint32_t oldest = UINT32_MAX;
  int idx = 0;
  for (int i = 0; i < STP_MAX_STATIONS; i++)
    if (g_stp.stations[i].roam_count == 0 && g_stp.stations[i].last_ts < oldest) {
      oldest = g_stp.stations[i].last_ts;
      idx = i;
    }
  memset(&g_stp.stations[idx], 0, sizeof(StpStation));
  g_stp.stations[idx].active = true;
  memcpy(g_stp.stations[idx].mac, mac, 6);
  g_stp.stations[idx].first_ts = millis() / 1000;
  g_stp.stations[idx].is_random_mac = stp_is_random_mac(mac);
  return &g_stp.stations[idx];
}

static void stp_tick(StpStation &s) {
  uint32_t now_s = millis() / 1000;
  if (now_s == s.last_tick_s)
    return;
  s.rate_hist[s.hist_head % STP_HIST_LEN] = (uint8_t)(s.cur_frames > 255 ? 255 : s.cur_frames);
  s.hist_head = (s.hist_head + 1) % STP_HIST_LEN;
  s.cur_frames = 0;
  s.last_tick_s = now_s;
}

void stp_ingest_probe(const uint8_t *mac, const char *ssid, uint8_t ch, int8_t rssi) {
  if (!mac || stp_is_bcast(mac) || stp_is_mcast(mac))
    return;
  StpStation *s = stp_find(mac);
  if (!s)
    s = stp_alloc(mac);
  if (!s)
    return;

  stp_tick(*s);
  s->rssi = rssi;
  s->total_probes++;
  s->cur_frames++;
  s->last_ts = millis() / 1000;

  if (!ssid || !ssid[0]) {
    s->is_wildcard_prober = true;
    return;
  }

  for (int i = 0; i < s->probe_count; i++) {
    if (strncmp(s->probes[i].ssid, ssid, 32) == 0) {
      s->probes[i].count++;
      s->probes[i].last_ts = millis() / 1000;
      s->probes[i].ch = ch;
      return;
    }
  }
  if (s->probe_count < STP_MAX_PROBES) {
    StpProbe &p = s->probes[s->probe_count++];
    strncpy(p.ssid, ssid, 32);
    p.ch = ch;
    p.count = 1;
    p.last_ts = millis() / 1000;
  }
}

void stp_ingest_assoc(const uint8_t *mac, const uint8_t *ap_bssid, const char *ssid, uint8_t ch,
                      int8_t rssi, bool ft, bool rrm, bool btm) {
  if (!mac || stp_is_bcast(mac))
    return;
  StpStation *s = stp_find(mac);
  if (!s)
    s = stp_alloc(mac);
  if (!s)
    return;

  // Detect roaming: AP changed
  bool roaming =
      s->state == STP_ST_ASSOCIATED && s->ap_bssid[0] > 0 && !stp_mac_eq(s->ap_bssid, ap_bssid);
  if (roaming) {
    s->roam_count++;
    s->last_roam_ts = millis() / 1000;
    s->state = STP_ST_ROAMING;
  } else
    s->state = STP_ST_ASSOCIATED;

  memcpy(s->ap_bssid, ap_bssid, 6);
  if (ssid)
    strncpy(s->ap_ssid, ssid, 32);
  s->ap_ch = ch;
  s->rssi = rssi;
  s->cap_ft = ft;
  s->cap_rrm = rrm;
  s->cap_btm = btm;

  // Add association history entry
  StpAssocHistEntry &h = s->assoc_hist[s->assoc_hist_head % STP_MAX_ASSOC_HIST];
  memset(&h, 0, sizeof(h));
  memcpy(h.bssid, ap_bssid, 6);
  if (ssid)
    strncpy(h.ssid, ssid, 32);
  h.ch = ch;
  h.assoc_ts = millis() / 1000;
  s->assoc_hist_head = (s->assoc_hist_head + 1) % STP_MAX_ASSOC_HIST;
  s->assoc_count++;
  s->last_ts = millis() / 1000;
}

void stp_ingest_data(const uint8_t *mac, bool retry, uint16_t len) {
  if (!mac || stp_is_bcast(mac) || stp_is_mcast(mac))
    return;
  StpStation *s = stp_find(mac);
  if (!s)
    s = stp_alloc(mac);
  if (!s)
    return;

  stp_tick(*s);
  s->data_frames++;
  s->data_bytes += len;
  if (retry)
    s->retry_frames++;
  s->cur_frames++;
  g_stp.total_frames++;
  s->last_ts = millis() / 1000;
}

void stp_ingest_power_save(const uint8_t *mac, bool entering) {
  StpStation *s = stp_find(mac);
  if (!s)
    return;
  if (entering) {
    s->pwr_save = STP_PS_DOZE;
    s->ps_enter_count++;
  } else {
    s->pwr_save = STP_PS_ACTIVE;
    s->ps_exit_count++;
  }
}

static const char *stp_state_str(StpState st) {
  switch (st) {
    case STP_ST_PROBING:
      return "Probing";
    case STP_ST_ASSOCIATED:
      return "Associated";
    case STP_ST_AUTHENTICATED:
      return "Authenticated";
    case STP_ST_ROAMING:
      return "Roaming";
    case STP_ST_DORMANT:
      return "Dormant";
    default:
      return "Unseen";
  }
}

static const char *stp_ps_str(StpPwrSave ps) {
  switch (ps) {
    case STP_PS_ACTIVE:
      return "Active";
    case STP_PS_DOZE:
      return "Doze";
    case STP_PS_UAPSD:
      return "UAPSD";
    default:
      return "Unknown";
  }
}

static void stp_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String stp_hist_json(const uint8_t *hist, int len, int head) {
  String s = "[";
  for (int i = 0; i < len; i++) {
    if (i)
      s += ",";
    s += String(hist[(head + i) % len]);
  }
  return s + "]";
}

static String stp_json_all() {
  String s = "{\"count\":" + String(g_stp.count);
  s += ",\"total_frames\":" + String(g_stp.total_frames);
  s += ",\"stations\":[";
  char mac[18], ap[18];
  bool first = true;
  for (int i = 0; i < STP_MAX_STATIONS; i++) {
    StpStation &st = g_stp.stations[i];
    if (!st.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    stp_mac_str(st.mac, mac);
    stp_mac_str(st.ap_bssid, ap);
    s += "{\"mac\":\"" + String(mac) + "\"";
    s += ",\"state\":\"" + String(stp_state_str(st.state)) + "\"";
    s += ",\"ps\":\"" + String(stp_ps_str(st.pwr_save)) + "\"";
    s += ",\"rssi\":" + String(st.rssi);
    s += ",\"ap_bssid\":\"" + String(ap) + "\"";
    s += ",\"ap_ssid\":\"" + String(st.ap_ssid) + "\"";
    s += ",\"ap_ch\":" + String(st.ap_ch);
    s += ",\"probe_cnt\":" + String(st.probe_count);
    s += ",\"total_probes\":" + String(st.total_probes);
    s += ",\"wildcard\":" + String(st.is_wildcard_prober ? "true" : "false");
    s += ",\"rand_mac\":" + String(st.is_random_mac ? "true" : "false");
    s += ",\"roam_count\":" + String(st.roam_count);
    s += ",\"data\":" + String(st.data_frames);
    s += ",\"data_bytes\":" + String(st.data_bytes);
    s += ",\"retries\":" + String(st.retry_frames);
    s += ",\"assoc_count\":" + String(st.assoc_count);
    s += ",\"cap_ft\":" + String(st.cap_ft ? "true" : "false");
    s += ",\"cap_rrm\":" + String(st.cap_rrm ? "true" : "false");
    s += ",\"cap_btm\":" + String(st.cap_btm ? "true" : "false");
    s += ",\"ps_enter\":" + String(st.ps_enter_count);
    s += ",\"ps_exit\":" + String(st.ps_exit_count);
    s += ",\"rate_hist\":" + stp_hist_json(st.rate_hist, STP_HIST_LEN, st.hist_head);
    // Probe list
    s += ",\"probes\":[";
    for (int j = 0; j < st.probe_count; j++) {
      if (j)
        s += ",";
      s += "{\"ssid\":\"" + String(st.probes[j].ssid) + "\"";
      s += ",\"ch\":" + String(st.probes[j].ch);
      s += ",\"count\":" + String(st.probes[j].count) + "}";
    }
    s += "]}";
  }
  s += "]}";
  return s;
}

inline void stp_routes() {
  server.on("/api/stp/all", HTTP_GET,
            []() { server.send(200, "application/json", stp_json_all()); });
  server.on("/api/stp/clear", HTTP_POST, []() {
    memset(&g_stp, 0, sizeof(g_stp));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

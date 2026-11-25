#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <string.h>
#include <stdint.h>

// ── Channel Scanner ───────────────────────────────────────────────────────────
//
// Active and passive channel scanner with AP catalog, encryption survey,
// hidden SSID recovery, and best-channel recommendation logic.

#define CS_MAX_APS 64
#define CS_MAX_SCANS 8
#define CS_SCAN_TIMEOUT 5000  // ms per scan
#define CS_PASSIVE_DWELL 200  // ms passive dwell
#define CS_HOP_CHANNELS 13    // 2.4 GHz

// ── AP encryption type (mirrors wifi_auth_mode_t for display) ─────────────────
enum CsEnc : uint8_t {
  CS_ENC_OPEN = 0,
  CS_ENC_WEP = 1,
  CS_ENC_WPA = 2,
  CS_ENC_WPA2 = 3,
  CS_ENC_WPA2E = 4,
  CS_ENC_WPA3 = 5,
  CS_ENC_WPA2_WPA3 = 6,
  CS_ENC_OWE = 7,
};

static const char *cs_enc_name(CsEnc e) {
  switch (e) {
    case CS_ENC_OPEN:
      return "Open";
    case CS_ENC_WEP:
      return "WEP";
    case CS_ENC_WPA:
      return "WPA";
    case CS_ENC_WPA2:
      return "WPA2";
    case CS_ENC_WPA2E:
      return "WPA2-Ent";
    case CS_ENC_WPA3:
      return "WPA3";
    case CS_ENC_WPA2_WPA3:
      return "WPA2/WPA3";
    case CS_ENC_OWE:
      return "OWE";
    default:
      return "Unknown";
  }
}

// ── Scanned AP record ─────────────────────────────────────────────────────────
struct CsAp {
  char ssid[33];
  uint8_t ssid_len;
  uint8_t bssid[6];
  uint8_t channel;
  int8_t rssi;
  int8_t rssi_min;
  int8_t rssi_max;
  CsEnc enc;
  bool hidden;
  bool pmf;
  bool ht;   // 802.11n
  bool vht;  // 802.11ac
  uint8_t spatial_streams;
  uint16_t interval;
  uint32_t first_seen_ms;
  uint32_t last_seen_ms;
  uint32_t beacon_count;
  uint8_t vendor_oui[3];
  char vendor[32];
};

// ── Scan result snapshot ──────────────────────────────────────────────────────
struct CsScanResult {
  uint32_t timestamp_ms;
  uint32_t duration_ms;
  uint8_t ap_count;
  uint8_t hidden_count;
  uint8_t open_count;
  uint8_t wep_count;
  uint8_t wpa_count;
  uint8_t wpa2_count;
  uint8_t wpa3_count;
  uint8_t channels_seen;
  int8_t strongest_rssi;
  int8_t weakest_rssi;
  uint8_t recommended_channel;
  bool active;
};

// ── Channel load per 2.4 GHz channel ─────────────────────────────────────────
struct CsChanLoad {
  uint8_t channel;
  uint8_t ap_count;
  int8_t avg_rssi;
  uint16_t overlap_score;  // higher = more crowded
};

// ── Scanner state ─────────────────────────────────────────────────────────────
struct CsState {
  CsAp aps[CS_MAX_APS];
  uint8_t ap_count;
  CsScanResult last_result;
  CsChanLoad chan_loads[CS_HOP_CHANNELS];
  bool scanning;
  bool passive_mode;
  uint32_t scan_start_ms;
  uint32_t total_scans;
  uint8_t current_chan;
  uint32_t last_chan_ms;
};

static CsState g_cs = {};

// ── Initialization ─────────────────────────────────────────────────────────────
static void cs_init() {
  memset(&g_cs, 0, sizeof(g_cs));
  for (int i = 0; i < CS_HOP_CHANNELS; i++)
    g_cs.chan_loads[i].channel = i + 1;
}

// ── AP upsert from promiscuous capture ───────────────────────────────────────
static CsAp *cs_ap_upsert(const uint8_t bssid[6]) {
  for (uint8_t i = 0; i < g_cs.ap_count; i++) {
    if (memcmp(g_cs.aps[i].bssid, bssid, 6) == 0)
      return &g_cs.aps[i];
  }
  if (g_cs.ap_count >= CS_MAX_APS) {
    // Evict oldest
    uint32_t oldest = g_cs.aps[0].last_seen_ms;
    uint8_t oi = 0;
    for (uint8_t i = 1; i < g_cs.ap_count; i++) {
      if (g_cs.aps[i].last_seen_ms < oldest) {
        oldest = g_cs.aps[i].last_seen_ms;
        oi = i;
      }
    }
    memset(&g_cs.aps[oi], 0, sizeof(CsAp));
    memcpy(g_cs.aps[oi].bssid, bssid, 6);
    g_cs.aps[oi].first_seen_ms = millis();
    g_cs.aps[oi].rssi_min = 0;
    g_cs.aps[oi].rssi_max = -100;
    return &g_cs.aps[oi];
  }
  CsAp *ap = &g_cs.aps[g_cs.ap_count++];
  memset(ap, 0, sizeof(*ap));
  memcpy(ap->bssid, bssid, 6);
  ap->first_seen_ms = millis();
  ap->rssi_min = 0;
  ap->rssi_max = -100;
  return ap;
}

// ── Ingest beacon info ────────────────────────────────────────────────────────
static void cs_ingest_beacon(const uint8_t bssid[6], const char *ssid, uint8_t ssid_len,
                             uint8_t channel, int8_t rssi, CsEnc enc, bool pmf, bool ht, bool vht) {
  CsAp *ap = cs_ap_upsert(bssid);
  ap->channel = channel;
  ap->rssi = rssi;
  ap->enc = enc;
  ap->pmf = pmf;
  ap->ht = ht;
  ap->vht = vht;
  ap->beacon_count++;
  if (ap->rssi_min == 0 || rssi < ap->rssi_min)
    ap->rssi_min = rssi;
  if (rssi > ap->rssi_max)
    ap->rssi_max = rssi;
  if (ssid_len > 0 && ssid) {
    ap->ssid_len = ssid_len < 32 ? ssid_len : 32;
    memcpy(ap->ssid, ssid, ap->ssid_len);
    ap->ssid[ap->ssid_len] = 0;
    ap->hidden = false;
  } else {
    ap->hidden = true;
  }
  ap->last_seen_ms = millis();
  // Update channel load
  if (channel >= 1 && channel <= CS_HOP_CHANNELS) {
    CsChanLoad &cl = g_cs.chan_loads[channel - 1];
    bool found = false;
    for (uint8_t i = 0; i < g_cs.ap_count; i++) {
      if (g_cs.aps[i].channel == channel) {
        if (!found) {
          cl.ap_count = 0;
          found = true;
        }
        cl.ap_count++;
      }
    }
    int32_t sum = 0;
    uint8_t cnt = 0;
    for (uint8_t i = 0; i < g_cs.ap_count; i++) {
      if (g_cs.aps[i].channel == channel) {
        sum += g_cs.aps[i].rssi;
        cnt++;
      }
    }
    if (cnt)
      cl.avg_rssi = (int8_t)(sum / cnt);
  }
}

// ── Active scan using esp_wifi_scan ───────────────────────────────────────────
static bool cs_start_active_scan() {
  if (g_cs.scanning)
    return false;
  wifi_scan_config_t cfg = {};
  cfg.show_hidden = true;
  if (esp_wifi_scan_start(&cfg, false) != ESP_OK)
    return false;
  g_cs.scanning = true;
  g_cs.scan_start_ms = millis();
  return true;
}

static void cs_poll_active_scan() {
  if (!g_cs.scanning)
    return;
  uint16_t n = 0;
  if (esp_wifi_scan_get_ap_num(&n) != ESP_OK)
    return;
  if (n == 0 && millis() - g_cs.scan_start_ms < CS_SCAN_TIMEOUT)
    return;
  wifi_ap_record_t *recs = (wifi_ap_record_t *)malloc(n * sizeof(wifi_ap_record_t));
  if (!recs) {
    g_cs.scanning = false;
    return;
  }
  if (esp_wifi_scan_get_ap_records(&n, recs) != ESP_OK) {
    free(recs);
    g_cs.scanning = false;
    return;
  }
  for (uint16_t i = 0; i < n; i++) {
    wifi_ap_record_t &r = recs[i];
    CsEnc enc = CS_ENC_OPEN;
    switch (r.authmode) {
      case WIFI_AUTH_WEP:
        enc = CS_ENC_WEP;
        break;
      case WIFI_AUTH_WPA_PSK:
        enc = CS_ENC_WPA;
        break;
      case WIFI_AUTH_WPA2_PSK:
        enc = CS_ENC_WPA2;
        break;
      case WIFI_AUTH_WPA_WPA2_PSK:
        enc = CS_ENC_WPA2;
        break;
      case WIFI_AUTH_WPA2_ENTERPRISE:
        enc = CS_ENC_WPA2E;
        break;
      case WIFI_AUTH_WPA3_PSK:
        enc = CS_ENC_WPA3;
        break;
      case WIFI_AUTH_WPA2_WPA3_PSK:
        enc = CS_ENC_WPA2_WPA3;
        break;
      default:
        enc = CS_ENC_OPEN;
        break;
    }
    bool ht = (r.phy_11n);
    bool vht = false;  // phy_11ac may not exist on all SDK versions
    cs_ingest_beacon(r.bssid, (const char *)r.ssid, strnlen((char *)r.ssid, 32), r.primary, r.rssi,
                     enc, false, ht, vht);
  }
  free(recs);
  g_cs.scanning = false;
  g_cs.total_scans++;
  // Build result snapshot
  CsScanResult &res = g_cs.last_result;
  res.timestamp_ms = millis();
  res.duration_ms = millis() - g_cs.scan_start_ms;
  res.ap_count = g_cs.ap_count;
  res.hidden_count = res.open_count = res.wep_count = 0;
  res.wpa_count = res.wpa2_count = res.wpa3_count = 0;
  res.strongest_rssi = -100;
  res.weakest_rssi = 0;
  uint8_t chans_seen = 0;
  bool chan_seen[CS_HOP_CHANNELS] = {};
  for (uint8_t i = 0; i < g_cs.ap_count; i++) {
    CsAp &ap = g_cs.aps[i];
    if (ap.hidden)
      res.hidden_count++;
    switch (ap.enc) {
      case CS_ENC_OPEN:
        res.open_count++;
        break;
      case CS_ENC_WEP:
        res.wep_count++;
        break;
      case CS_ENC_WPA:
        res.wpa_count++;
        break;
      case CS_ENC_WPA2:
      case CS_ENC_WPA2E:
        res.wpa2_count++;
        break;
      case CS_ENC_WPA3:
      case CS_ENC_WPA2_WPA3:
        res.wpa3_count++;
        break;
      default:
        break;
    }
    if (ap.rssi > res.strongest_rssi)
      res.strongest_rssi = ap.rssi;
    if (ap.rssi < res.weakest_rssi)
      res.weakest_rssi = ap.rssi;
    if (ap.channel >= 1 && ap.channel <= CS_HOP_CHANNELS && !chan_seen[ap.channel - 1]) {
      chan_seen[ap.channel - 1] = true;
      chans_seen++;
    }
  }
  res.channels_seen = chans_seen;
  // Best channel
  uint32_t f1 = g_cs.chan_loads[0].ap_count;
  uint32_t f6 = g_cs.chan_loads[5].ap_count;
  uint32_t f11 = g_cs.chan_loads[10].ap_count;
  res.recommended_channel = (f1 <= f6 && f1 <= f11) ? 1 : (f6 <= f11) ? 6 : 11;
  res.active = true;
}

// ── Channel overlap score (0=clear, >8=congested) ────────────────────────────
static uint8_t cs_overlap_score(uint8_t channel) {
  // Channels within ±4 interfere
  uint8_t score = 0;
  for (uint8_t i = 0; i < g_cs.ap_count; i++) {
    uint8_t ch = g_cs.aps[i].channel;
    int diff = abs((int)ch - (int)channel);
    if (diff <= 4)
      score++;
  }
  return score;
}

// ── AP JSON ───────────────────────────────────────────────────────────────────
static String cs_ap_json(int page, int per) {
  if (per < 1 || per > 50)
    per = 20;
  int start = page * per, end = start + per;
  if (end > (int)g_cs.ap_count)
    end = g_cs.ap_count;
  String j = "{\"total\":" + String(g_cs.ap_count);
  j += ",\"page\":" + String(page) + ",\"per\":" + String(per);
  j += ",\"aps\":[";
  for (int i = start; i < end; i++) {
    if (i > start)
      j += ",";
    CsAp &ap = g_cs.aps[i];
    char mac[18];
    snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X", ap.bssid[0], ap.bssid[1],
             ap.bssid[2], ap.bssid[3], ap.bssid[4], ap.bssid[5]);
    j += "{\"bssid\":\"";
    j += mac;
    j += "\"";
    j += ",\"ssid\":\"";
    j += ap.ssid;
    j += "\"";
    j += ",\"ch\":";
    j += ap.channel;
    j += ",\"rssi\":";
    j += ap.rssi;
    j += ",\"enc\":\"";
    j += cs_enc_name(ap.enc);
    j += "\"";
    j += ",\"hidden\":";
    j += ap.hidden ? 1 : 0;
    j += ",\"pmf\":";
    j += ap.pmf ? 1 : 0;
    j += ",\"ht\":";
    j += ap.ht ? 1 : 0;
    j += ",\"vht\":";
    j += ap.vht ? 1 : 0;
    j += ",\"beacons\":";
    j += ap.beacon_count;
    uint32_t age = (millis() - ap.last_seen_ms) / 1000;
    j += ",\"age_s\":";
    j += age;
    j += "}";
  }
  j += "]}";
  return j;
}

// ── Scan result JSON ──────────────────────────────────────────────────────────
static String cs_result_json() {
  cs_poll_active_scan();
  CsScanResult &res = g_cs.last_result;
  String j = "{\"ap_count\":" + String(res.ap_count);
  j += ",\"hidden\":" + String(res.hidden_count);
  j += ",\"open\":" + String(res.open_count);
  j += ",\"wep\":" + String(res.wep_count);
  j += ",\"wpa\":" + String(res.wpa_count);
  j += ",\"wpa2\":" + String(res.wpa2_count);
  j += ",\"wpa3\":" + String(res.wpa3_count);
  j += ",\"channels_seen\":" + String(res.channels_seen);
  j += ",\"recommended_chan\":" + String(res.recommended_channel);
  j += ",\"scanning\":" + String(g_cs.scanning ? 1 : 0);
  j += ",\"total_scans\":" + String(g_cs.total_scans);
  j += "}";
  return j;
}

// ── Channel loads JSON ────────────────────────────────────────────────────────
static String cs_chan_loads_json() {
  String j = "{\"channels\":[";
  for (int i = 0; i < CS_HOP_CHANNELS; i++) {
    if (i)
      j += ",";
    CsChanLoad &cl = g_cs.chan_loads[i];
    j += "{\"ch\":" + String(cl.channel);
    j += ",\"aps\":" + String(cl.ap_count);
    j += ",\"rssi_avg\":" + String(cl.avg_rssi);
    j += ",\"overlap\":" + String(cs_overlap_score(cl.channel));
    j += "}";
  }
  j += "]}";
  return j;
}

// ── HTTP routes ───────────────────────────────────────────────────────────────
static void cs_routes() {
  extern WebServer server;

  server.on("/api/cs/scan", HTTP_POST, []() {
    if (g_cs.scanning) {
      server.send(200, "application/json", "{\"status\":\"scanning\"}");
      return;
    }
    bool ok = cs_start_active_scan();
    server.send(200, "application/json",
                ok ? "{\"status\":\"started\"}" : "{\"error\":\"failed\"}");
  });

  server.on("/api/cs/result", HTTP_GET,
            []() { server.send(200, "application/json", cs_result_json()); });

  server.on("/api/cs/aps", HTTP_GET, []() {
    int page = server.hasArg("page") ? server.arg("page").toInt() : 0;
    int per = server.hasArg("per") ? server.arg("per").toInt() : 20;
    server.send(200, "application/json", cs_ap_json(page, per));
  });

  server.on("/api/cs/channels", HTTP_GET,
            []() { server.send(200, "application/json", cs_chan_loads_json()); });

  server.on("/api/cs/best", HTTP_GET, []() {
    uint32_t f1 = g_cs.chan_loads[0].ap_count;
    uint32_t f6 = g_cs.chan_loads[5].ap_count;
    uint32_t f11 = g_cs.chan_loads[10].ap_count;
    uint8_t best = (f1 <= f6 && f1 <= f11) ? 1 : (f6 <= f11) ? 6 : 11;
    char buf[32];
    snprintf(buf, sizeof(buf), "{\"best_chan\":%d}", best);
    server.send(200, "application/json", buf);
  });
}

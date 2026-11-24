#pragma once
// sta_profiler.h — Extended station profiler with traffic pattern analysis.
// Builds deep profiles for each observed WiFi client station. Prefix: stap_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define STAP_MAX_STAS    64
#define STAP_SSID_LEN    33
#define STAP_OUI_LEN     32
#define STAP_PROBE_MAX   8    // max probed SSIDs per STA
#define STAP_HIST_LEN    30   // 30-second rate history

// ─── Traffic pattern ──────────────────────────────────────────────────────────
enum StapPattern {
  STAP_PAT_UNKNOWN = 0,
  STAP_PAT_IDLE,          // mostly quiet, occasional probes
  STAP_PAT_BROWSING,      // moderate data, short bursts
  STAP_PAT_STREAMING,     // sustained high data rate
  STAP_PAT_VOIP,          // regular small data frames
  STAP_PAT_SCANNING,      // lots of probe requests
  STAP_PAT_GAMING,        // mixed management + data, low latency
  STAP_PAT_HOTSPOT,       // device is acting as AP (BSSID in its beacons)
};

// ─── Station profile ──────────────────────────────────────────────────────────
struct StapSta {
  bool    active;
  uint8_t mac[6];
  char    vendor[STAP_OUI_LEN];
  uint8_t current_bssid[6];
  uint8_t channel;
  int8_t  rssi;
  int8_t  rssi_min;
  int8_t  rssi_max;

  // Probed SSIDs
  char    probed_ssids[STAP_PROBE_MAX][STAP_SSID_LEN];
  uint8_t probe_count;

  // Frame counters
  uint32_t data_frames;
  uint32_t mgmt_frames;
  uint32_t ctrl_frames;
  uint32_t probe_reqs;
  uint32_t auth_frames;
  uint32_t assoc_frames;
  uint32_t deauth_frames;
  uint32_t disassoc_frames;
  uint32_t null_data_frames;
  uint32_t qos_data_frames;
  uint32_t total_bytes;

  // Rate history (frames per second, last 30s)
  uint16_t rate_hist[STAP_HIST_LEN];
  uint8_t  rate_idx;
  uint32_t last_rate_ts;

  // Timestamps
  uint32_t first_seen;
  uint32_t last_seen;
  uint32_t active_seconds;

  // Analysis
  StapPattern pattern;
  uint8_t     mobility;      // 0-100: how much RSSI varies (high = moving)
  uint8_t     aggressiveness; // 0-100: probe flood score
  bool        power_save;    // last frame had PS bit set
  bool        pmf_capable;   // management frame protection seen
  bool        ht_capable;    // 802.11n
  bool        vht_capable;   // 802.11ac
  bool        he_capable;    // 802.11ax
  uint8_t     max_data_rate; // in Mbps (from HT/VHT/HE IEs)

  // Associations history
  uint8_t  assoc_bssids[4][6];
  uint8_t  assoc_count;
};

// ─── State ────────────────────────────────────────────────────────────────────
static struct {
  StapSta stas[STAP_MAX_STAS];
  uint8_t  count;
  uint32_t total_frames;
  uint32_t last_tick_ts;
} g_stap = {};

// ─── Helpers ─────────────────────────────────────────────────────────────────
static bool stap_mac_eq(const uint8_t *a, const uint8_t *b) { return memcmp(a,b,6)==0; }
static bool stap_is_mcast(const uint8_t *m) { return m[0] & 0x01; }
static bool stap_is_local(const uint8_t *m) { return m[0] & 0x02; }

static StapSta* stap_find(const uint8_t *mac) {
  for (int i = 0; i < STAP_MAX_STAS; i++) {
    if (g_stap.stas[i].active && stap_mac_eq(g_stap.stas[i].mac, mac))
      return &g_stap.stas[i];
  }
  return nullptr;
}

static StapSta* stap_alloc(const uint8_t *mac) {
  for (int i = 0; i < STAP_MAX_STAS; i++) {
    if (!g_stap.stas[i].active) {
      StapSta &s = g_stap.stas[i];
      memset(&s, 0, sizeof(s));
      memcpy(s.mac, mac, 6);
      s.active = true;
      s.rssi_min = 0;
      s.rssi_max = -127;
      s.first_seen = millis() / 1000;
      g_stap.count++;
      return &s;
    }
  }
  return nullptr;
}

// ─── Probe SSID tracking ─────────────────────────────────────────────────────
static void stap_add_probe(StapSta *sta, const char *ssid) {
  if (!ssid || ssid[0] == '\0') return;
  for (int i = 0; i < sta->probe_count; i++) {
    if (strcmp(sta->probed_ssids[i], ssid) == 0) return;
  }
  if (sta->probe_count < STAP_PROBE_MAX) {
    strncpy(sta->probed_ssids[sta->probe_count++], ssid, STAP_SSID_LEN - 1);
  }
}

// ─── Rate calculation ─────────────────────────────────────────────────────────
static void stap_update_rate(StapSta *sta, uint16_t frames) {
  uint32_t now = millis() / 1000;
  if (now != sta->last_rate_ts) {
    sta->rate_hist[sta->rate_idx % STAP_HIST_LEN] = frames;
    sta->rate_idx++;
    sta->last_rate_ts = now;
  }
}

// ─── Pattern classifier ───────────────────────────────────────────────────────
static void stap_classify(StapSta *sta) {
  uint32_t total = sta->data_frames + sta->mgmt_frames;
  if (total == 0) { sta->pattern = STAP_PAT_IDLE; return; }

  float probe_ratio = (float)sta->probe_reqs / max(total, 1UL);
  float data_ratio  = (float)sta->data_frames / max(total, 1UL);
  float qos_ratio   = (float)sta->qos_data_frames / max(sta->data_frames + 1UL, 1UL);
  float null_ratio  = (float)sta->null_data_frames / max(sta->data_frames + 1UL, 1UL);

  // Calculate average rate
  uint32_t rate_sum = 0;
  for (int i = 0; i < STAP_HIST_LEN; i++) rate_sum += sta->rate_hist[i];
  float avg_rate = rate_sum / (float)STAP_HIST_LEN;

  if (probe_ratio > 0.6f) {
    sta->pattern = STAP_PAT_SCANNING;
    sta->aggressiveness = (uint8_t)min(100.0f, probe_ratio * 150.0f);
  } else if (avg_rate > 100 && qos_ratio > 0.5f) {
    sta->pattern = STAP_PAT_STREAMING;
  } else if (avg_rate > 20 && avg_rate < 100) {
    sta->pattern = STAP_PAT_BROWSING;
  } else if (null_ratio > 0.3f) {
    sta->pattern = STAP_PAT_VOIP;
  } else if (data_ratio < 0.1f) {
    sta->pattern = STAP_PAT_IDLE;
  } else {
    sta->pattern = STAP_PAT_BROWSING;
  }

  // Mobility: coefficient of variation of RSSI-like proxy
  int8_t rssi_range = sta->rssi_max - sta->rssi_min;
  sta->mobility = (uint8_t)min(100, (int)(rssi_range * 3));
}

// ─── Ingest frame ────────────────────────────────────────────────────────────
void stap_ingest(const uint8_t *raw, int len, const uint8_t *src,
                 const uint8_t *dst, const uint8_t *bssid,
                 uint8_t type, uint8_t sub, int8_t rssi,
                 uint8_t channel, const char *ssid) {

  // Skip multicast sources, APs (BSSID matches src)
  if (!src || stap_is_mcast(src)) return;
  if (bssid && stap_mac_eq(src, bssid)) return;

  StapSta *sta = stap_find(src);
  if (!sta) sta = stap_alloc(src);
  if (!sta) return;

  g_stap.total_frames++;
  sta->last_seen = millis() / 1000;
  sta->channel = channel;
  sta->rssi = rssi;
  if (rssi < sta->rssi_min || sta->rssi_min == 0) sta->rssi_min = rssi;
  if (rssi > sta->rssi_max) sta->rssi_max = rssi;

  if (bssid) memcpy(sta->current_bssid, bssid, 6);
  sta->total_bytes += len;

  // Frame type classification
  if      (type == 0) sta->mgmt_frames++;
  else if (type == 1) sta->ctrl_frames++;
  else if (type == 2) sta->data_frames++;

  if (type == 0) { // management
    switch (sub) {
      case 4:  // probe request
        sta->probe_reqs++;
        stap_add_probe(sta, ssid);
        break;
      case 11: sta->auth_frames++; break;
      case 0: case 2: sta->assoc_frames++; break;
      case 12: sta->deauth_frames++; break;
      case 10: sta->disassoc_frames++; break;
    }
    // Check PMF (MFP) capability in assoc req IE 48 (RSN) caps bit 7
    if ((sub == 0 || sub == 2) && len > 36) {
      // Basic capability - simplified
      sta->pmf_capable = true; // Will be refined with IE parsing
    }
  }
  if (type == 2) {
    uint8_t fc1 = raw[1];
    if (fc1 & 0x10) sta->power_save = true; // PS bit
    if (sub == 4 || sub == 12 || sub == 6 || sub == 14) sta->null_data_frames++;
    if (sub >= 8) sta->qos_data_frames++;
  }

  // Track unique BSSIDs associated with
  if (bssid && !stap_mac_eq(bssid, (const uint8_t*)"\xff\xff\xff\xff\xff\xff")) {
    bool found = false;
    for (int i = 0; i < sta->assoc_count; i++) {
      if (stap_mac_eq(sta->assoc_bssids[i], bssid)) { found = true; break; }
    }
    if (!found && sta->assoc_count < 4) {
      memcpy(sta->assoc_bssids[sta->assoc_count++], bssid, 6);
    }
  }

  // Classify every 50 frames
  if ((sta->data_frames + sta->mgmt_frames) % 50 == 0)
    stap_classify(sta);
}

// ─── Tick (called ~1/sec) ────────────────────────────────────────────────────
void stap_tick() {
  uint32_t now = millis() / 1000;
  if (now == g_stap.last_tick_ts) return;
  g_stap.last_tick_ts = now;
  for (int i = 0; i < STAP_MAX_STAS; i++) {
    StapSta &s = g_stap.stas[i];
    if (!s.active) continue;
    if (now - s.last_seen < 300) s.active_seconds++;
  }
}

// ─── JSON ─────────────────────────────────────────────────────────────────────
static void stap_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0],m[1],m[2],m[3],m[4],m[5]);
}

static const char* stap_pattern_str(StapPattern p) {
  switch (p) {
    case STAP_PAT_IDLE:      return "idle";
    case STAP_PAT_BROWSING:  return "browsing";
    case STAP_PAT_STREAMING: return "streaming";
    case STAP_PAT_VOIP:      return "voip";
    case STAP_PAT_SCANNING:  return "scanning";
    case STAP_PAT_GAMING:    return "gaming";
    case STAP_PAT_HOTSPOT:   return "hotspot";
    default:                  return "unknown";
  }
}

static String stap_json_sta(const StapSta &s) {
  char mac[18], bssid[18];
  stap_mac_str(s.mac, mac);
  stap_mac_str(s.current_bssid, bssid);
  String j = "{\"mac\":\"" + String(mac) + "\"";
  j += ",\"vendor\":\"" + String(s.vendor) + "\"";
  j += ",\"bssid\":\"" + String(bssid) + "\"";
  j += ",\"rssi\":" + String(s.rssi);
  j += ",\"rssi_min\":" + String(s.rssi_min);
  j += ",\"rssi_max\":" + String(s.rssi_max);
  j += ",\"ch\":" + String(s.channel);
  j += ",\"data\":" + String(s.data_frames);
  j += ",\"mgmt\":" + String(s.mgmt_frames);
  j += ",\"probes\":" + String(s.probe_reqs);
  j += ",\"bytes\":" + String(s.total_bytes);
  j += ",\"active_s\":" + String(s.active_seconds);
  j += ",\"pattern\":\"" + String(stap_pattern_str(s.pattern)) + "\"";
  j += ",\"mobility\":" + String(s.mobility);
  j += ",\"aggression\":" + String(s.aggressiveness);
  j += ",\"ps\":" + String(s.power_save?"true":"false");
  j += ",\"pmf\":" + String(s.pmf_capable?"true":"false");
  j += ",\"ht\":" + String(s.ht_capable?"true":"false");
  j += ",\"vht\":" + String(s.vht_capable?"true":"false");
  j += ",\"probed_ssids\":[";
  for (int i = 0; i < s.probe_count; i++) {
    if (i) j += ",";
    j += "\"" + String(s.probed_ssids[i]) + "\"";
  }
  j += "],\"assocs\":" + String(s.assoc_count);
  j += "}";
  return j;
}

static String stap_json_all() {
  String s = "{\"count\":" + String(g_stap.count);
  s += ",\"total_frames\":" + String(g_stap.total_frames);
  s += ",\"stas\":[";
  bool first = true;
  for (int i = 0; i < STAP_MAX_STAS; i++) {
    if (!g_stap.stas[i].active) continue;
    if (!first) s += ",";
    first = false;
    s += stap_json_sta(g_stap.stas[i]);
  }
  s += "]}";
  return s;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void stap_routes() {
  server.on("/api/stap/all", HTTP_GET, []() {
    server.send(200, "application/json", stap_json_all());
  });

  server.on("/api/stap/sta", HTTP_GET, []() {
    if (!server.hasArg("mac")) {
      server.send(400, "application/json", "{\"error\":\"missing mac\"}"); return;
    }
    uint8_t mac[6];
    sscanf(server.arg("mac").c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
           &mac[0],&mac[1],&mac[2],&mac[3],&mac[4],&mac[5]);
    StapSta *sta = stap_find(mac);
    if (!sta) { server.send(404, "application/json", "{\"error\":\"not found\"}"); return; }
    server.send(200, "application/json", stap_json_sta(*sta));
  });

  server.on("/api/stap/clear", HTTP_POST, []() {
    memset(&g_stap, 0, sizeof(g_stap));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

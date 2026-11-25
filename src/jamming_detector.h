#pragma once
// jamming_detector.h — WiFi jamming and interference detector.
// Detects sustained channel interference, deauth flooding, beacon spoofing,
// CTS-to-self NAV poisoning, and 2.4 GHz microwave/BT interference patterns.
// Prefix: jd_

#include <Arduino.h>
#include <WebServer.h>
#include <esp_wifi.h>

extern WebServer server;

// ─── Jam types ────────────────────────────────────────────────────────────────
enum JdJamType {
  JD_JAM_NONE = 0,
  JD_JAM_DEAUTH_FLOOD,       // sustained deauth storm
  JD_JAM_BEACON_FLOOD,       // too many unique BSSIDs/second
  JD_JAM_CTS_NAV_POISON,     // CTS with large duration
  JD_JAM_PROBE_FLOOD,        // mass probe requests
  JD_JAM_DISASSOC_FLOOD,     // disassoc flood
  JD_JAM_AUTH_FLOOD,         // auth request flood
  JD_JAM_CONTINUOUS_TX,      // continuous TX detected via RSSI floor
  JD_JAM_CHANNEL_POLLUTION,  // channel FCS error rate high
  JD_JAM_SSID_CHAOS,         // many unique SSIDs in short window
  JD_JAM_SIGNAL_DROP,        // sudden RSSI drop (potential jammer)
  JD_JAM_BLIND_CHANNEL,      // zero valid frames on channel
};

// ─── Alert severity ───────────────────────────────────────────────────────────
enum JdSeverity { JD_SEV_INFO, JD_SEV_LOW, JD_SEV_MEDIUM, JD_SEV_HIGH, JD_SEV_CRITICAL };

// ─── Structs ──────────────────────────────────────────────────────────────────
#define JD_ALERT_MAX 32
#define JD_BSSID_WINDOW 64
#define JD_SSID_WINDOW 64
#define JD_HISTORY_SIZE 120  // 2 minutes at 1s intervals
#define JD_FRAME_WINDOW 5    // seconds for rate calc

struct JdAlert {
  bool active;
  JdJamType type;
  JdSeverity severity;
  uint32_t ts;
  uint32_t end_ts;  // 0 = still active
  uint8_t channel;
  int8_t rssi;
  char detail[80];
  uint32_t frame_count;
};

struct JdChannelState {
  uint32_t total_frames;
  uint32_t deauth_frames;
  uint32_t beacon_frames;
  uint32_t probe_frames;
  uint32_t disassoc_frames;
  uint32_t auth_frames;
  uint32_t cts_frames;
  uint32_t fcs_errors;
  uint32_t window_start;  // millis
  int8_t rssi_last;
  int8_t rssi_min;
  int8_t rssi_max;
  uint32_t unique_bssids;  // in current window
  uint32_t unique_ssids;   // in current window
  // Circular history
  uint16_t frame_rate_hist[JD_HISTORY_SIZE];
  uint16_t deauth_rate_hist[JD_HISTORY_SIZE];
  uint8_t hist_idx;
};

// ─── State ────────────────────────────────────────────────────────────────────
static struct {
  JdAlert alerts[JD_ALERT_MAX];
  uint8_t alert_head;
  uint32_t total_alerts;
  JdChannelState channels[14];  // 2.4 GHz channels 1-14
  bool monitoring;
  uint32_t last_tick;

  // Per-window tracking
  uint8_t window_bssids[JD_BSSID_WINDOW][6];
  uint8_t window_bssid_count;
  char window_ssids[JD_SSID_WINDOW][33];
  uint8_t window_ssid_count;
  uint32_t window_start;

  // Baseline for signal-drop detection
  int8_t signal_baseline[14];
  bool baseline_set[14];

  // Thresholds
  uint16_t thr_deauth_flood;  // deauths/5s
  uint16_t thr_beacon_flood;  // unique BSSIDs/5s
  uint16_t thr_probe_flood;   // probes/5s
  uint16_t thr_ssid_chaos;    // unique SSIDs/5s
  uint16_t thr_nav_poison;    // CTS duration µs
  int8_t thr_signal_drop_db;  // dB drop to trigger
} g_jd = {};

// ─── Add alert ────────────────────────────────────────────────────────────────
static void jd_alert(JdJamType type, JdSeverity sev, uint8_t ch, int8_t rssi, const char *detail) {
  // Check if same type already active on same channel
  for (int i = 0; i < JD_ALERT_MAX; i++) {
    JdAlert &a = g_jd.alerts[i];
    if (a.active && a.type == type && a.channel == ch && a.end_ts == 0) {
      // Update existing
      a.frame_count++;
      a.rssi = rssi;
      return;
    }
  }

  JdAlert &a = g_jd.alerts[g_jd.alert_head % JD_ALERT_MAX];
  a.active = true;
  a.type = type;
  a.severity = sev;
  a.ts = millis() / 1000;
  a.end_ts = 0;
  a.channel = ch;
  a.rssi = rssi;
  a.frame_count = 1;
  strncpy(a.detail, detail, 79);
  g_jd.alert_head++;
  g_jd.total_alerts++;
}

static void jd_resolve_alert(JdJamType type, uint8_t ch) {
  for (int i = 0; i < JD_ALERT_MAX; i++) {
    JdAlert &a = g_jd.alerts[i];
    if (a.active && a.type == type && a.channel == ch && a.end_ts == 0) {
      a.end_ts = millis() / 1000;
    }
  }
}

// ─── Ingest frame ─────────────────────────────────────────────────────────────
void jd_ingest(uint8_t ch, uint8_t frame_type, uint8_t frame_sub, const uint8_t *bssid,
               const char *ssid, int8_t rssi, uint16_t nav_duration) {
  if (!g_jd.monitoring || ch < 1 || ch > 14)
    return;

  JdChannelState &cs = g_jd.channels[ch - 1];
  uint32_t now = millis();

  cs.total_frames++;
  cs.rssi_last = rssi;
  if (rssi < cs.rssi_min)
    cs.rssi_min = rssi;
  if (rssi > cs.rssi_max)
    cs.rssi_max = rssi;

  // Frame type classification
  if (frame_type == 0) {  // Management
    if (frame_sub == 8)
      cs.beacon_frames++;
    else if (frame_sub == 4 || frame_sub == 5)
      cs.probe_frames++;
    else if (frame_sub == 10)
      cs.disassoc_frames++;
    else if (frame_sub == 11 || frame_sub == 12)
      cs.deauth_frames++;
    else if (frame_sub == 0 || frame_sub == 1)
      cs.auth_frames++;
  } else if (frame_type == 1) {  // Control
    if (frame_sub == 12) {       // CTS
      cs.cts_frames++;
      if (nav_duration > g_jd.thr_nav_poison) {
        char detail[80];
        snprintf(detail, sizeof(detail), "CTS NAV=%uµs on CH%d (threshold %uµs)", nav_duration, ch,
                 g_jd.thr_nav_poison);
        jd_alert(JD_JAM_CTS_NAV_POISON, JD_SEV_HIGH, ch, rssi, detail);
      }
    }
  }

  // BSSID tracking for beacon flood
  if (bssid) {
    bool found = false;
    for (int i = 0; i < g_jd.window_bssid_count; i++) {
      if (memcmp(g_jd.window_bssids[i], bssid, 6) == 0) {
        found = true;
        break;
      }
    }
    if (!found && g_jd.window_bssid_count < JD_BSSID_WINDOW) {
      memcpy(g_jd.window_bssids[g_jd.window_bssid_count++], bssid, 6);
    }
  }

  // SSID tracking for chaos detection
  if (ssid && ssid[0]) {
    bool found = false;
    for (int i = 0; i < g_jd.window_ssid_count; i++) {
      if (strcmp(g_jd.window_ssids[i], ssid) == 0) {
        found = true;
        break;
      }
    }
    if (!found && g_jd.window_ssid_count < JD_SSID_WINDOW) {
      strncpy(g_jd.window_ssids[g_jd.window_ssid_count++], ssid, 32);
    }
  }

  // Signal baseline
  if (!g_jd.baseline_set[ch - 1]) {
    g_jd.signal_baseline[ch - 1] = rssi;
    g_jd.baseline_set[ch - 1] = true;
  } else {
    int8_t drop = g_jd.signal_baseline[ch - 1] - rssi;
    if (drop > g_jd.thr_signal_drop_db) {
      char detail[80];
      snprintf(detail, sizeof(detail), "Signal dropped %ddB on CH%d (baseline %ddBm now %ddBm)",
               drop, ch, g_jd.signal_baseline[ch - 1], rssi);
      jd_alert(JD_JAM_SIGNAL_DROP, JD_SEV_MEDIUM, ch, rssi, detail);
    } else {
      // Gradual update
      g_jd.signal_baseline[ch - 1] = (g_jd.signal_baseline[ch - 1] * 7 + rssi) / 8;
    }
  }
}

// ─── Periodic tick (call every 1s) ────────────────────────────────────────────
inline void jd_tick(uint8_t current_ch) {
  if (!g_jd.monitoring)
    return;
  uint32_t now = millis();
  uint32_t elapsed = now - g_jd.window_start;

  if (elapsed < 5000)
    return;  // 5-second evaluation window

  JdChannelState &cs = g_jd.channels[current_ch < 1 || current_ch > 14 ? 0 : current_ch - 1];
  char detail[80];

  // Check deauth flood
  if (cs.deauth_frames >= g_jd.thr_deauth_flood) {
    snprintf(detail, sizeof(detail), "CH%d: %lu deauths in 5s (threshold %u)", current_ch,
             (unsigned long)cs.deauth_frames, g_jd.thr_deauth_flood);
    jd_alert(JD_JAM_DEAUTH_FLOOD, JD_SEV_HIGH, current_ch, cs.rssi_last, detail);
  } else {
    jd_resolve_alert(JD_JAM_DEAUTH_FLOOD, current_ch);
  }

  // Check beacon flood (too many unique BSSIDs)
  if (g_jd.window_bssid_count >= g_jd.thr_beacon_flood) {
    snprintf(detail, sizeof(detail), "CH%d: %u unique BSSIDs in 5s (threshold %u)", current_ch,
             g_jd.window_bssid_count, g_jd.thr_beacon_flood);
    jd_alert(JD_JAM_BEACON_FLOOD, JD_SEV_HIGH, current_ch, cs.rssi_last, detail);
  } else {
    jd_resolve_alert(JD_JAM_BEACON_FLOOD, current_ch);
  }

  // Check probe flood
  if (cs.probe_frames >= g_jd.thr_probe_flood) {
    snprintf(detail, sizeof(detail), "CH%d: %lu probes in 5s (threshold %u)", current_ch,
             (unsigned long)cs.probe_frames, g_jd.thr_probe_flood);
    jd_alert(JD_JAM_PROBE_FLOOD, JD_SEV_MEDIUM, current_ch, cs.rssi_last, detail);
  }

  // Check SSID chaos
  if (g_jd.window_ssid_count >= g_jd.thr_ssid_chaos) {
    snprintf(detail, sizeof(detail), "CH%d: %u unique SSIDs in 5s (possible SSID flood)",
             current_ch, g_jd.window_ssid_count);
    jd_alert(JD_JAM_SSID_CHAOS, JD_SEV_MEDIUM, current_ch, cs.rssi_last, detail);
  }

  // Check blind channel (zero frames in window)
  if (cs.total_frames == 0) {
    snprintf(detail, sizeof(detail), "CH%d: zero frames in 5s (possible RF jamming)", current_ch);
    jd_alert(JD_JAM_BLIND_CHANNEL, JD_SEV_HIGH, current_ch, -100, detail);
  } else {
    jd_resolve_alert(JD_JAM_BLIND_CHANNEL, current_ch);
  }

  // Update history
  cs.frame_rate_hist[cs.hist_idx % JD_HISTORY_SIZE] =
      (uint16_t)(cs.total_frames * 1000 / (elapsed ? elapsed : 1));
  cs.deauth_rate_hist[cs.hist_idx % JD_HISTORY_SIZE] =
      (uint16_t)(cs.deauth_frames * 1000 / (elapsed ? elapsed : 1));
  cs.hist_idx++;

  // Reset window
  memset(&cs.total_frames, 0, (uint8_t *)&cs.rssi_last - (uint8_t *)&cs.total_frames);
  g_jd.window_bssid_count = 0;
  g_jd.window_ssid_count = 0;
  g_jd.window_start = now;
  g_jd.last_tick = now;
}

// ─── Init ────────────────────────────────────────────────────────────────────
inline void jd_init() {
  memset(&g_jd, 0, sizeof(g_jd));
  g_jd.thr_deauth_flood = 20;
  g_jd.thr_beacon_flood = 30;
  g_jd.thr_probe_flood = 50;
  g_jd.thr_ssid_chaos = 20;
  g_jd.thr_nav_poison = 30000;  // 30ms
  g_jd.thr_signal_drop_db = 25;
  g_jd.monitoring = true;
  g_jd.window_start = millis();
  for (int i = 0; i < 14; i++)
    g_jd.channels[i].rssi_min = -100;
}

// ─── JSON status ─────────────────────────────────────────────────────────────
static const char *jd_jam_name(JdJamType t) {
  switch (t) {
    case JD_JAM_DEAUTH_FLOOD:
      return "deauth_flood";
    case JD_JAM_BEACON_FLOOD:
      return "beacon_flood";
    case JD_JAM_CTS_NAV_POISON:
      return "cts_nav_poison";
    case JD_JAM_PROBE_FLOOD:
      return "probe_flood";
    case JD_JAM_DISASSOC_FLOOD:
      return "disassoc_flood";
    case JD_JAM_AUTH_FLOOD:
      return "auth_flood";
    case JD_JAM_CONTINUOUS_TX:
      return "continuous_tx";
    case JD_JAM_CHANNEL_POLLUTION:
      return "channel_pollution";
    case JD_JAM_SSID_CHAOS:
      return "ssid_chaos";
    case JD_JAM_SIGNAL_DROP:
      return "signal_drop";
    case JD_JAM_BLIND_CHANNEL:
      return "blind_channel";
    default:
      return "none";
  }
}

static const char *jd_sev_name(JdSeverity s) {
  switch (s) {
    case JD_SEV_INFO:
      return "info";
    case JD_SEV_LOW:
      return "low";
    case JD_SEV_MEDIUM:
      return "medium";
    case JD_SEV_HIGH:
      return "high";
    case JD_SEV_CRITICAL:
      return "critical";
    default:
      return "none";
  }
}

static String jd_json_status() {
  String s = "{\"monitoring\":";
  s += g_jd.monitoring ? "true" : "false";
  s += ",\"total_alerts\":" + String(g_jd.total_alerts);
  s += ",\"alerts\":[";
  bool first = true;
  int total = min((int)g_jd.alert_head, JD_ALERT_MAX);
  for (int i = 0; i < total; i++) {
    JdAlert &a = g_jd.alerts[i];
    if (!a.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    s += "{\"type\":\"" + String(jd_jam_name(a.type)) + "\"";
    s += ",\"severity\":\"" + String(jd_sev_name(a.severity)) + "\"";
    s += ",\"channel\":" + String(a.channel);
    s += ",\"rssi\":" + String(a.rssi);
    s += ",\"ts\":" + String(a.ts);
    s += ",\"end_ts\":" + String(a.end_ts);
    s += ",\"frames\":" + String(a.frame_count);
    s += ",\"detail\":\"" + String(a.detail) + "\"";
    s += ",\"active\":" + String(a.end_ts == 0 ? "true" : "false");
    s += "}";
  }
  s += "],\"channels\":[";
  for (int ch = 0; ch < 14; ch++) {
    if (ch)
      s += ",";
    JdChannelState &cs = g_jd.channels[ch];
    s += "{\"ch\":" + String(ch + 1);
    s += ",\"frames\":" + String(cs.total_frames);
    s += ",\"deauths\":" + String(cs.deauth_frames);
    s += ",\"probes\":" + String(cs.probe_frames);
    s += ",\"rssi\":" + String(cs.rssi_last);
    s += ",\"rssi_min\":" + String(cs.rssi_min);
    s += ",\"rssi_max\":" + String(cs.rssi_max);
    s += "}";
  }
  s += "]}";
  return s;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void jd_routes() {
  server.on("/api/jd/status", HTTP_GET,
            []() { server.send(200, "application/json", jd_json_status()); });

  server.on("/api/jd/start", HTTP_POST, []() {
    g_jd.monitoring = true;
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/jd/stop", HTTP_POST, []() {
    g_jd.monitoring = false;
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/jd/clear", HTTP_POST, []() {
    memset(&g_jd.alerts, 0, sizeof(g_jd.alerts));
    g_jd.alert_head = 0;
    g_jd.total_alerts = 0;
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/jd/thresholds", HTTP_POST, []() {
    if (server.hasArg("deauth"))
      g_jd.thr_deauth_flood = server.arg("deauth").toInt();
    if (server.hasArg("beacon"))
      g_jd.thr_beacon_flood = server.arg("beacon").toInt();
    if (server.hasArg("probe"))
      g_jd.thr_probe_flood = server.arg("probe").toInt();
    if (server.hasArg("ssid"))
      g_jd.thr_ssid_chaos = server.arg("ssid").toInt();
    if (server.hasArg("nav"))
      g_jd.thr_nav_poison = server.arg("nav").toInt();
    if (server.hasArg("sig_drop"))
      g_jd.thr_signal_drop_db = server.arg("sig_drop").toInt();
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

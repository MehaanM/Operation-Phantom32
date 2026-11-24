#pragma once
// time_sync.h — NTP time synchronization and timestamp correlation.
// Connects to NTP over STA mode, maintains epoch offset, correlates
// millis()-based timestamps to wall-clock time. Prefix: ntp_

#include <Arduino.h>
#include <WebServer.h>
#include <WiFiUdp.h>

extern WebServer server;

#define NTP_PACKET_SIZE    48
#define NTP_PORT           123
#define NTP_TIMEOUT_MS     5000
#define NTP_RETRY_MAX      3
#define NTP_SYNC_INTERVAL  3600  // seconds between syncs

// NTP epoch is Jan 1 1900; Unix epoch is Jan 1 1970
#define NTP_UNIX_OFFSET    2208988800UL

static const char* NTP_SERVERS[] = {
  "pool.ntp.org",
  "time.google.com",
  "time.cloudflare.com",
  "time.windows.com",
};
#define NTP_SERVER_COUNT 4

// ─── State ────────────────────────────────────────────────────────────────────
static struct {
  bool     synced;
  uint32_t epoch;          // Unix timestamp at last sync
  uint32_t sync_millis;    // millis() value at last sync
  uint32_t last_sync_ts;   // millis() of last successful sync
  uint32_t sync_count;
  uint32_t fail_count;
  char     server_used[32];
  int32_t  offset_ms;      // accumulated drift estimate
  int8_t   retry_count;
  int8_t   current_server_idx;
  WiFiUDP  udp;
  bool     udp_started;
} g_ntp = {};

// ─── NTP packet builder ───────────────────────────────────────────────────────
static void ntp_build_packet(uint8_t *buf) {
  memset(buf, 0, NTP_PACKET_SIZE);
  buf[0] = 0b11100011; // LI=3 (unsynchronized), VN=4, Mode=3 (client)
  buf[1] = 0;          // Stratum (unspecified)
  buf[2] = 6;          // Polling interval
  buf[3] = 0xEC;       // Precision
  // Root delay, root dispersion, reference ID: left as 0
}

// ─── Send NTP request ─────────────────────────────────────────────────────────
static bool ntp_send_request(const char *server_host) {
  uint8_t pkt[NTP_PACKET_SIZE];
  ntp_build_packet(pkt);
  g_ntp.udp.begin(NTP_PORT + 1); // local port
  g_ntp.udp.beginPacket(server_host, NTP_PORT);
  g_ntp.udp.write(pkt, NTP_PACKET_SIZE);
  return g_ntp.udp.endPacket() == 1;
}

// ─── Parse NTP response ───────────────────────────────────────────────────────
static bool ntp_parse_response(uint32_t *epoch_out) {
  uint32_t deadline = millis() + NTP_TIMEOUT_MS;
  while (millis() < deadline) {
    int pktLen = g_ntp.udp.parsePacket();
    if (pktLen >= NTP_PACKET_SIZE) {
      uint8_t pkt[NTP_PACKET_SIZE];
      g_ntp.udp.read(pkt, NTP_PACKET_SIZE);
      g_ntp.udp.stop();
      // Transmit timestamp is bytes 40-43
      uint32_t secs = ((uint32_t)pkt[40] << 24) | ((uint32_t)pkt[41] << 16) |
                      ((uint32_t)pkt[42] << 8)  |  (uint32_t)pkt[43];
      if (secs == 0) return false;
      *epoch_out = secs - NTP_UNIX_OFFSET;
      return true;
    }
    delay(10);
  }
  g_ntp.udp.stop();
  return false;
}

// ─── Sync ────────────────────────────────────────────────────────────────────
inline bool ntp_sync(const char *server_host = nullptr) {
  if (!server_host) {
    server_host = NTP_SERVERS[g_ntp.current_server_idx % NTP_SERVER_COUNT];
  }
  strncpy(g_ntp.server_used, server_host, 31);

  if (!ntp_send_request(server_host)) {
    g_ntp.fail_count++;
    return false;
  }

  uint32_t epoch;
  if (!ntp_parse_response(&epoch)) {
    g_ntp.fail_count++;
    // Try next server next time
    g_ntp.current_server_idx = (g_ntp.current_server_idx + 1) % NTP_SERVER_COUNT;
    return false;
  }

  uint32_t sync_ms = millis();
  if (g_ntp.synced) {
    // Estimate drift
    uint32_t elapsed_ms = sync_ms - g_ntp.sync_millis;
    uint32_t elapsed_expected_s = epoch - g_ntp.epoch;
    int32_t drift_ms = (int32_t)elapsed_ms - (int32_t)(elapsed_expected_s * 1000);
    g_ntp.offset_ms += drift_ms;
  }

  g_ntp.epoch       = epoch;
  g_ntp.sync_millis = sync_ms;
  g_ntp.last_sync_ts = sync_ms;
  g_ntp.synced      = true;
  g_ntp.sync_count++;
  return true;
}

// ─── Get current time ─────────────────────────────────────────────────────────
inline uint32_t ntp_epoch() {
  if (!g_ntp.synced) return 0;
  uint32_t elapsed_ms = millis() - g_ntp.sync_millis;
  return g_ntp.epoch + elapsed_ms / 1000;
}

// Convert a millis()-based timestamp to epoch
inline uint32_t ntp_ms_to_epoch(uint32_t ts_ms) {
  if (!g_ntp.synced) return 0;
  if (ts_ms <= g_ntp.sync_millis) return g_ntp.epoch;
  return g_ntp.epoch + (ts_ms - g_ntp.sync_millis) / 1000;
}

// Format epoch as "YYYY-MM-DD HH:MM:SS UTC"
static void ntp_format(uint32_t epoch, char *buf, int buflen) {
  // Days since epoch
  uint32_t days = epoch / 86400;
  uint32_t secs_today = epoch % 86400;
  int hh = secs_today / 3600;
  int mm = (secs_today % 3600) / 60;
  int ss = secs_today % 60;

  // Gregorian calendar
  uint32_t a = days + 2440588; // JDN for 1970-01-01
  uint32_t f = a + 1401 + (((4*a + 274277) / 146097) * 3) / 4 - 38;
  uint32_t e = 4*f + 3;
  uint32_t g = (e % 1461) / 4;
  uint32_t h = 5*g + 2;
  int D = (h % 153) / 5 + 1;
  int M = (h / 153 + 2) % 12 + 1;
  int Y = e / 1461 - 4716 + (14 - M) / 12;

  snprintf(buf, buflen, "%04d-%02d-%02d %02d:%02d:%02d UTC", Y, M, D, hh, mm, ss);
}

// ─── Auto-sync ticker (call from loop or task) ────────────────────────────────
inline void ntp_tick() {
  if (!g_ntp.synced || (millis() - g_ntp.last_sync_ts) > NTP_SYNC_INTERVAL * 1000UL) {
    // Only attempt sync if STA is connected
    if (WiFi.status() == WL_CONNECTED) {
      ntp_sync();
    }
  }
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void ntp_routes() {
  server.on("/api/ntp/status", HTTP_GET, []() {
    char time_str[32] = "not synced";
    if (g_ntp.synced) ntp_format(ntp_epoch(), time_str, sizeof(time_str));
    String s = "{\"synced\":" + String(g_ntp.synced ? "true" : "false");
    s += ",\"epoch\":" + String(ntp_epoch());
    s += ",\"time\":\"" + String(time_str) + "\"";
    s += ",\"server\":\"" + String(g_ntp.server_used) + "\"";
    s += ",\"sync_count\":" + String(g_ntp.sync_count);
    s += ",\"fail_count\":" + String(g_ntp.fail_count);
    s += ",\"drift_ms\":" + String(g_ntp.offset_ms);
    s += "}";
    server.send(200, "application/json", s);
  });

  server.on("/api/ntp/sync", HTTP_POST, []() {
    bool ok = ntp_sync(server.hasArg("server") ? server.arg("server").c_str() : nullptr);
    char time_str[32] = "";
    if (ok) ntp_format(ntp_epoch(), time_str, sizeof(time_str));
    String s = String("{\"ok\":") + (ok ? "true" : "false") + ",\"time\":\"" + time_str + "\"}";
    server.send(200, "application/json", s);
  });

  server.on("/api/ntp/convert", HTTP_GET, []() {
    if (!server.hasArg("ms")) { server.send(400, "application/json", "{\"error\":\"missing ms\"}"); return; }
    uint32_t ms = (uint32_t)server.arg("ms").toInt();
    uint32_t ep = ntp_ms_to_epoch(ms);
    char buf[32] = "";
    if (ep) ntp_format(ep, buf, sizeof(buf));
    server.send(200, "application/json", "{\"epoch\":" + String(ep) + ",\"time\":\"" + String(buf) + "\"}");
  });
}

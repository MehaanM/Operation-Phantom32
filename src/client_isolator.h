#pragma once
// client_isolator.h — Client isolation detection and AP-side enforcement helper.
// Detects APs with client isolation enabled by monitoring inter-client traffic.
// Also provides active client isolation testing (ARP probe injection).
// Prefix: ci_

#include <Arduino.h>
#include <WebServer.h>
#include <esp_wifi.h>

extern WebServer server;

#define CI_MAX_CLIENTS 32
#define CI_MAX_APS 16
#define CI_PROBE_TIMEOUT 3000
#define CI_ARP_LOG_SIZE 64

// ─── AP isolation record ──────────────────────────────────────────────────────
struct CiAP {
  bool active;
  uint8_t bssid[6];
  char ssid[33];
  uint8_t channel;
  int8_t rssi;
  bool isolation_detected;       // inter-client traffic seen = no isolation
  bool isolation_confirmed;      // ARP response indicates isolation in place
  uint32_t inter_client_frames;  // frames between clients on same AP
  uint32_t total_frames;
  uint32_t first_seen;
  uint32_t last_seen;
};

// ─── Client-to-client frame log ───────────────────────────────────────────────
struct CiClientFrame {
  uint32_t ts;
  uint8_t src[6];
  uint8_t dst[6];
  uint8_t bssid[6];
  uint8_t frame_type;
  uint8_t frame_sub;
  int8_t rssi;
};

// ─── State ────────────────────────────────────────────────────────────────────
static struct {
  CiAP aps[CI_MAX_APS];
  CiClientFrame log[CI_ARP_LOG_SIZE];
  uint8_t ap_count;
  uint8_t log_head;
  uint32_t total_inter_client;
  // Active probe state
  bool probing;
  uint8_t probe_src[6];
  uint8_t probe_dst[6];
  uint8_t probe_bssid[6];
  uint8_t probe_channel;
  uint32_t arp_sent;
  uint32_t arp_responses;
  uint32_t probe_start_ts;
  TaskHandle_t probe_task;
} g_ci = {};

// ─── Helpers ─────────────────────────────────────────────────────────────────
static bool ci_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}
static bool ci_is_broadcast(const uint8_t *m) {
  return m[0] == 0xFF && m[1] == 0xFF && m[2] == 0xFF && m[3] == 0xFF && m[4] == 0xFF &&
         m[5] == 0xFF;
}

static CiAP *ci_find_ap(const uint8_t *bssid) {
  for (int i = 0; i < CI_MAX_APS; i++) {
    if (g_ci.aps[i].active && ci_mac_eq(g_ci.aps[i].bssid, bssid))
      return &g_ci.aps[i];
  }
  return nullptr;
}

static CiAP *ci_alloc_ap(const uint8_t *bssid, const char *ssid, uint8_t ch) {
  for (int i = 0; i < CI_MAX_APS; i++) {
    if (!g_ci.aps[i].active) {
      CiAP &ap = g_ci.aps[i];
      memset(&ap, 0, sizeof(ap));
      memcpy(ap.bssid, bssid, 6);
      if (ssid)
        strncpy(ap.ssid, ssid, 32);
      ap.channel = ch;
      ap.active = true;
      ap.first_seen = millis() / 1000;
      g_ci.ap_count++;
      return &ap;
    }
  }
  return nullptr;
}

// ─── Build ARP Request frame ──────────────────────────────────────────────────
static uint16_t ci_build_arp(uint8_t *buf, int max_len, const uint8_t *src_mac,
                             const uint8_t *dst_mac, const uint8_t *bssid, uint8_t channel,
                             const uint8_t *src_ip, const uint8_t *dst_ip) {
  if (max_len < 70)
    return 0;
  int pos = 0;

  // Radiotap
  buf[pos++] = 0x00;
  buf[pos++] = 0x00;
  buf[pos++] = 0x08;
  buf[pos++] = 0x00;
  buf[pos++] = 0x00;
  buf[pos++] = 0x00;
  buf[pos++] = 0x00;
  buf[pos++] = 0x00;

  // MAC header - Data + ToDS
  buf[pos++] = 0x08;
  buf[pos++] = 0x01;  // FC: data, ToDS
  buf[pos++] = 0x3A;
  buf[pos++] = 0x01;  // duration
  memcpy(buf + pos, bssid, 6);
  pos += 6;  // DA (BSSID for ToDS)
  memcpy(buf + pos, src_mac, 6);
  pos += 6;  // SA
  memcpy(buf + pos, dst_mac, 6);
  pos += 6;  // DA (actual)
  buf[pos++] = 0x00;
  buf[pos++] = 0x00;  // seq ctrl

  // LLC/SNAP header
  buf[pos++] = 0xAA;
  buf[pos++] = 0xAA;
  buf[pos++] = 0x03;
  buf[pos++] = 0x00;
  buf[pos++] = 0x00;
  buf[pos++] = 0x00;  // OUI
  buf[pos++] = 0x08;
  buf[pos++] = 0x06;  // Ethertype ARP

  // ARP payload
  buf[pos++] = 0x00;
  buf[pos++] = 0x01;  // hardware type: ethernet
  buf[pos++] = 0x08;
  buf[pos++] = 0x00;  // protocol: IPv4
  buf[pos++] = 0x06;
  buf[pos++] = 0x04;  // hw/proto addr len
  buf[pos++] = 0x00;
  buf[pos++] = 0x01;  // op: request
  memcpy(buf + pos, src_mac, 6);
  pos += 6;  // sender MAC
  memcpy(buf + pos, src_ip, 4);
  pos += 4;  // sender IP
  memset(buf + pos, 0x00, 6);
  pos += 6;  // target MAC (unknown)
  memcpy(buf + pos, dst_ip, 4);
  pos += 4;  // target IP

  return (uint16_t)pos;
}

// ─── Probe task ───────────────────────────────────────────────────────────────
static void ci_probe_task(void *arg) {
  g_ci.arp_sent = 0;
  g_ci.arp_responses = 0;

  uint8_t fake_src_ip[] = {192, 168, 4, 100};
  uint8_t target_ip[] = {192, 168, 4, 2};

  for (int i = 0; i < 5 && g_ci.probing; i++) {
    uint8_t buf[128];
    uint16_t len = ci_build_arp(buf, sizeof(buf), g_ci.probe_src, g_ci.probe_dst, g_ci.probe_bssid,
                                g_ci.probe_channel, fake_src_ip, target_ip);
    if (len > 0 && esp_wifi_80211_tx(WIFI_IF_AP, buf, len, false) == ESP_OK) {
      g_ci.arp_sent++;
    }
    vTaskDelay(pdMS_TO_TICKS(600));
  }

  // Wait for a response window
  vTaskDelay(pdMS_TO_TICKS(1000));

  // Update AP record
  CiAP *ap = ci_find_ap(g_ci.probe_bssid);
  if (ap) {
    if (g_ci.arp_responses > 0) {
      ap->isolation_detected = false;  // responses → no isolation
    } else {
      ap->isolation_confirmed = true;  // no response → isolation likely
    }
  }

  g_ci.probing = false;
  g_ci.probe_task = nullptr;
  vTaskDelete(nullptr);
}

// ─── Ingest 802.11 frame ─────────────────────────────────────────────────────
void ci_ingest(const uint8_t *raw, int len, const uint8_t *src, const uint8_t *dst,
               const uint8_t *bssid, uint8_t type, uint8_t sub, int8_t rssi, uint8_t channel) {
  // Look for data frames where both src and dst are stations (not AP BSSID)
  if (type == 2 && !ci_is_broadcast(dst) && !ci_mac_eq(src, bssid) && !ci_mac_eq(dst, bssid)) {
    CiAP *ap = ci_find_ap(bssid);
    if (!ap)
      ap = ci_alloc_ap(bssid, nullptr, channel);
    if (!ap)
      return;

    ap->inter_client_frames++;
    ap->isolation_detected = true;  // inter-client traffic = isolation disabled
    ap->total_frames++;
    ap->last_seen = millis() / 1000;
    ap->rssi = rssi;

    g_ci.total_inter_client++;

    // Log
    CiClientFrame &log = g_ci.log[g_ci.log_head % CI_ARP_LOG_SIZE];
    log.ts = millis() / 1000;
    memcpy(log.src, src, 6);
    memcpy(log.dst, dst, 6);
    memcpy(log.bssid, bssid, 6);
    log.frame_type = type;
    log.frame_sub = sub;
    log.rssi = rssi;
    g_ci.log_head++;
  }

  // Check for ARP response (might be reply to our probe)
  if (type == 2 && len > 50) {
    int snap_off = 8 + 24 + 4;  // rtap + mach + LLC start
    if (snap_off + 8 < len && raw[snap_off] == 0xAA && raw[snap_off + 1] == 0xAA &&
        raw[snap_off + 2] == 0x03 && raw[snap_off + 6] == 0x08 && raw[snap_off + 7] == 0x06) {
      // ARP
      int arp_off = snap_off + 8;
      if (arp_off + 7 < len) {
        uint16_t op = (raw[arp_off + 6] << 8) | raw[arp_off + 7];
        if (op == 2)
          g_ci.arp_responses++;  // ARP reply
      }
    }
  }
}

// ─── JSON ─────────────────────────────────────────────────────────────────────
static void ci_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String ci_json_status() {
  String s = "{\"total_inter_client\":" + String(g_ci.total_inter_client);
  s += ",\"ap_count\":" + String(g_ci.ap_count);
  s += ",\"probing\":" + String(g_ci.probing ? "true" : "false");
  s += ",\"arp_sent\":" + String(g_ci.arp_sent);
  s += ",\"arp_responses\":" + String(g_ci.arp_responses);
  s += ",\"aps\":[";
  char mac[18];
  bool first = true;
  for (int i = 0; i < CI_MAX_APS; i++) {
    CiAP &ap = g_ci.aps[i];
    if (!ap.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    ci_mac_str(ap.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(ap.ssid) + "\"";
    s += ",\"ch\":" + String(ap.channel);
    s += ",\"rssi\":" + String(ap.rssi);
    s += ",\"isolation_detected\":" + String(ap.isolation_detected ? "true" : "false");
    s += ",\"isolation_confirmed\":" + String(ap.isolation_confirmed ? "true" : "false");
    s += ",\"inter_client\":" + String(ap.inter_client_frames);
    s += "}";
  }
  s += "]}";
  return s;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void ci_routes() {
  server.on("/api/ci/status", HTTP_GET,
            []() { server.send(200, "application/json", ci_json_status()); });

  server.on("/api/ci/probe", HTTP_POST, []() {
    if (!server.hasArg("src") || !server.hasArg("dst") || !server.hasArg("bssid")) {
      server.send(400, "application/json", "{\"error\":\"missing params\"}");
      return;
    }
    sscanf(server.arg("src").c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &g_ci.probe_src[0],
           &g_ci.probe_src[1], &g_ci.probe_src[2], &g_ci.probe_src[3], &g_ci.probe_src[4],
           &g_ci.probe_src[5]);
    sscanf(server.arg("dst").c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &g_ci.probe_dst[0],
           &g_ci.probe_dst[1], &g_ci.probe_dst[2], &g_ci.probe_dst[3], &g_ci.probe_dst[4],
           &g_ci.probe_dst[5]);
    sscanf(server.arg("bssid").c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &g_ci.probe_bssid[0],
           &g_ci.probe_bssid[1], &g_ci.probe_bssid[2], &g_ci.probe_bssid[3], &g_ci.probe_bssid[4],
           &g_ci.probe_bssid[5]);
    g_ci.probe_channel = server.hasArg("ch") ? server.arg("ch").toInt() : 6;
    if (!g_ci.probing) {
      g_ci.probing = true;
      g_ci.probe_start_ts = millis() / 1000;
      xTaskCreate(ci_probe_task, "ci_probe", 2048, nullptr, 4, &g_ci.probe_task);
    }
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/ci/clear", HTTP_POST, []() {
    memset(&g_ci, 0, sizeof(g_ci));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

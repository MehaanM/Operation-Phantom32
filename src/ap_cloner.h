#pragma once
// ap_cloner.h — Full AP profile cloning: copies SSID, channel, security type,
// BSSID (locally administered), country IE, vendor IEs, HT/VHT capabilities.
// Used for authorized RF testing and roaming evaluation. Prefix: apc_

#include <Arduino.h>
#include <WebServer.h>
#include <esp_wifi.h>

extern WebServer server;

#define APC_MAX_CLONES 8
#define APC_SSID_LEN 33
#define APC_IE_POOL_SIZE 256  // bytes for extra IEs per clone
#define APC_LOG_SIZE 32

// ─── Clone record ─────────────────────────────────────────────────────────────
struct ApcClone {
  bool active;
  char ssid[APC_SSID_LEN];
  uint8_t cloned_bssid[6];  // original BSSID
  uint8_t our_bssid[6];     // our locally administered BSSID
  uint8_t channel;
  uint8_t security;  // 0=open, 1=wpa2, 2=wpa3, 3=wep
  uint8_t op_class;
  uint16_t beacon_interval;  // TUs
  uint16_t capability;
  // Extra IEs to clone into our beacons
  uint8_t extra_ies[APC_IE_POOL_SIZE];
  uint16_t extra_ie_len;
  // Statistics
  uint32_t beacons_sent;
  uint32_t last_sent_ts;
  bool running;
  TaskHandle_t task;
};

struct ApcLog {
  uint32_t ts;
  char ssid[APC_SSID_LEN];
  uint8_t original_bssid[6];
  uint8_t our_bssid[6];
  uint8_t channel;
};

// ─── State ────────────────────────────────────────────────────────────────────
static struct {
  ApcClone clones[APC_MAX_CLONES];
  ApcLog log[APC_LOG_SIZE];
  uint8_t log_head;
  uint8_t clone_count;
  uint32_t total_beacons;
} g_apc = {};

// ─── Random locally-administered MAC ─────────────────────────────────────────
static void apc_local_mac(const uint8_t *original, uint8_t *out) {
  memcpy(out, original, 6);
  out[0] = (out[0] & ~0x01) | 0x02;  // LA bit set, unicast
  // Flip one byte so it differs
  out[5] ^= 0x01;
}

// ─── Build RSN IE (WPA2) ──────────────────────────────────────────────────────
static int apc_rsn_ie(uint8_t *out, int max_len) {
  if (max_len < 22)
    return 0;
  static const uint8_t rsn[] = {0x30, 0x14, 0x01, 0x00, 0x00, 0x0F, 0xAC, 0x04, 0x01, 0x00, 0x00,
                                0x0F, 0xAC, 0x04, 0x01, 0x00, 0x00, 0x0F, 0xAC, 0x02, 0x00, 0x00};
  memcpy(out, rsn, sizeof(rsn));
  return sizeof(rsn);
}

// ─── Build beacon frame ───────────────────────────────────────────────────────
static uint16_t apc_build_beacon(uint8_t *buf, int max_len, ApcClone &cl, uint32_t seq) {
  if (max_len < 128)
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

  // MAC header - Beacon
  buf[pos++] = 0x80;
  buf[pos++] = 0x00;
  buf[pos++] = 0x00;
  buf[pos++] = 0x00;
  memset(buf + pos, 0xFF, 6);
  pos += 6;  // DA
  memcpy(buf + pos, cl.our_bssid, 6);
  pos += 6;  // SA
  memcpy(buf + pos, cl.our_bssid, 6);
  pos += 6;  // BSSID
  buf[pos++] = (seq & 0xF) << 4;
  buf[pos++] = (seq >> 4) & 0xFF;

  // Fixed params
  uint64_t ts = (uint64_t)(millis() * 1000);
  memcpy(buf + pos, &ts, 8);
  pos += 8;
  uint16_t bi = cl.beacon_interval ? cl.beacon_interval : 100;
  buf[pos++] = bi & 0xFF;
  buf[pos++] = bi >> 8;
  uint16_t caps = cl.capability ? cl.capability : 0x0431;
  buf[pos++] = caps & 0xFF;
  buf[pos++] = caps >> 8;

  // SSID IE
  int sl = strlen(cl.ssid);
  buf[pos++] = 0x00;
  buf[pos++] = (uint8_t)sl;
  if (sl) {
    memcpy(buf + pos, cl.ssid, sl);
    pos += sl;
  }

  // Rates
  buf[pos++] = 0x01;
  buf[pos++] = 0x08;
  static const uint8_t rates[] = {0x82, 0x84, 0x8B, 0x96, 0x24, 0x30, 0x48, 0x6C};
  memcpy(buf + pos, rates, 8);
  pos += 8;

  // DS param
  buf[pos++] = 0x03;
  buf[pos++] = 0x01;
  buf[pos++] = cl.channel;

  // TIM
  buf[pos++] = 0x05;
  buf[pos++] = 0x04;
  buf[pos++] = 0x00;
  buf[pos++] = 0x01;
  buf[pos++] = 0x00;
  buf[pos++] = 0x00;

  // RSN if WPA2
  if (cl.security == 1) {
    int rsn_len = apc_rsn_ie(buf + pos, max_len - pos);
    pos += rsn_len;
  }

  // Extra IEs from original beacon
  if (cl.extra_ie_len > 0 && pos + cl.extra_ie_len < max_len) {
    memcpy(buf + pos, cl.extra_ies, cl.extra_ie_len);
    pos += cl.extra_ie_len;
  }

  if (pos > max_len)
    pos = max_len;
  return (uint16_t)pos;
}

// ─── Clone task ───────────────────────────────────────────────────────────────
static void apc_task(void *arg) {
  ApcClone *cl = (ApcClone *)arg;
  uint8_t buf[512];
  uint32_t seq = 0;

  while (cl->running) {
    uint16_t len = apc_build_beacon(buf, sizeof(buf), *cl, seq++);
    if (len > 0 && esp_wifi_80211_tx(WIFI_IF_AP, buf, len, false) == ESP_OK) {
      cl->beacons_sent++;
      g_apc.total_beacons++;
    }
    cl->last_sent_ts = millis() / 1000;
    vTaskDelay(pdMS_TO_TICKS(100));  // 10 beacons/s
  }
  cl->task = nullptr;
  vTaskDelete(nullptr);
}

// ─── Control ─────────────────────────────────────────────────────────────────
static ApcClone *apc_find_slot() {
  for (int i = 0; i < APC_MAX_CLONES; i++) {
    if (!g_apc.clones[i].active)
      return &g_apc.clones[i];
  }
  return nullptr;
}

static bool apc_start_clone(const char *ssid, const uint8_t *original_bssid, uint8_t channel,
                            uint8_t security, uint16_t bi, uint16_t caps, const uint8_t *extra_ies,
                            uint16_t extra_len) {
  ApcClone *cl = apc_find_slot();
  if (!cl)
    return false;

  memset(cl, 0, sizeof(*cl));
  strncpy(cl->ssid, ssid, APC_SSID_LEN - 1);
  memcpy(cl->cloned_bssid, original_bssid, 6);
  apc_local_mac(original_bssid, cl->our_bssid);
  cl->channel = channel;
  cl->security = security;
  cl->beacon_interval = bi;
  cl->capability = caps;
  if (extra_ies && extra_len > 0 && extra_len <= APC_IE_POOL_SIZE) {
    memcpy(cl->extra_ies, extra_ies, extra_len);
    cl->extra_ie_len = extra_len;
  }
  cl->active = true;
  cl->running = true;
  g_apc.clone_count++;

  // Log
  ApcLog &l = g_apc.log[g_apc.log_head % APC_LOG_SIZE];
  l.ts = millis() / 1000;
  strncpy(l.ssid, ssid, APC_SSID_LEN - 1);
  memcpy(l.original_bssid, original_bssid, 6);
  memcpy(l.our_bssid, cl->our_bssid, 6);
  l.channel = channel;
  g_apc.log_head++;

  xTaskCreate(apc_task, "apc_task", 4096, cl, 5, &cl->task);
  return true;
}

static void apc_stop_all() {
  for (int i = 0; i < APC_MAX_CLONES; i++) {
    if (!g_apc.clones[i].active)
      continue;
    g_apc.clones[i].running = false;
    uint32_t deadline = millis() + 300;
    while (g_apc.clones[i].task && millis() < deadline)
      vTaskDelay(10);
    g_apc.clones[i].active = false;
  }
  g_apc.clone_count = 0;
}

static void apc_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String apc_json_status() {
  String s = "{\"clone_count\":" + String(g_apc.clone_count);
  s += ",\"total_beacons\":" + String(g_apc.total_beacons);
  s += ",\"clones\":[";
  char mac1[18], mac2[18];
  bool first = true;
  for (int i = 0; i < APC_MAX_CLONES; i++) {
    ApcClone &cl = g_apc.clones[i];
    if (!cl.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    apc_mac_str(cl.cloned_bssid, mac1);
    apc_mac_str(cl.our_bssid, mac2);
    s += "{\"ssid\":\"" + String(cl.ssid) + "\"";
    s += ",\"original_bssid\":\"" + String(mac1) + "\"";
    s += ",\"our_bssid\":\"" + String(mac2) + "\"";
    s += ",\"ch\":" + String(cl.channel);
    s += ",\"security\":" + String(cl.security);
    s += ",\"running\":" + String(cl.running ? "true" : "false");
    s += ",\"beacons_sent\":" + String(cl.beacons_sent);
    s += "}";
  }
  s += "]}";
  return s;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void apc_routes() {
  server.on("/api/apc/status", HTTP_GET,
            []() { server.send(200, "application/json", apc_json_status()); });

  server.on("/api/apc/start", HTTP_POST, []() {
    if (!server.hasArg("ssid") || !server.hasArg("bssid")) {
      server.send(400, "application/json", "{\"error\":\"missing ssid or bssid\"}");
      return;
    }
    uint8_t bssid[6];
    sscanf(server.arg("bssid").c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &bssid[0], &bssid[1],
           &bssid[2], &bssid[3], &bssid[4], &bssid[5]);
    int ch = server.hasArg("ch") ? server.arg("ch").toInt() : 6;
    int sec = server.hasArg("sec") ? server.arg("sec").toInt() : 1;
    bool ok = apc_start_clone(server.arg("ssid").c_str(), bssid, (uint8_t)ch, (uint8_t)sec, 100,
                              0x0431, nullptr, 0);
    server.send(200, "application/json", String("{\"ok\":") + (ok ? "true" : "false") + "}");
  });

  server.on("/api/apc/stop", HTTP_POST, []() {
    apc_stop_all();
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

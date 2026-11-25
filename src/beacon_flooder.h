#pragma once
// beacon_flooder.h — Beacon flood testing module for authorized AP stress testing.
// Sends crafted beacon frames with configurable SSIDs, security configs,
// and channel settings. Useful for testing wireless infrastructure resilience.
// Authorized use only on networks/labs you own or have written permission to test.
// Prefix: bfl_

#include <Arduino.h>
#include <WebServer.h>
#include <esp_wifi.h>

extern WebServer server;

#define BFL_MAX_SSIDS 64
#define BFL_SSID_LEN 33
#define BFL_LOG_SIZE 64

// ─── Beacon modes ─────────────────────────────────────────────────────────────
enum BflMode {
  BFL_MODE_OFF = 0,
  BFL_MODE_SINGLE,      // single SSID repeated
  BFL_MODE_LIST,        // cycle through SSID list
  BFL_MODE_SEQUENTIAL,  // SSID_0 … SSID_N
  BFL_MODE_RANDOM,      // random SSIDs
  BFL_MODE_CLONE_POOL,  // clone from discovered APs
};

enum BflSecurity {
  BFL_SEC_OPEN = 0,
  BFL_SEC_WEP,
  BFL_SEC_WPA2,
  BFL_SEC_WPA3,
  BFL_SEC_MIXED,
};

// ─── State ────────────────────────────────────────────────────────────────────
static struct {
  BflMode mode;
  BflSecurity security;
  bool running;
  uint32_t sent;
  uint32_t errors;
  uint32_t interval_ms;
  uint8_t base_mac[6];
  char ssids[BFL_MAX_SSIDS][BFL_SSID_LEN];
  uint8_t ssid_count;
  uint8_t current_ssid_idx;
  uint8_t target_channel;
  bool randomize_mac;
  bool randomize_channel;
  uint32_t seq_counter;
  TaskHandle_t task_handle;
  struct {
    uint32_t ts;
    char ssid[BFL_SSID_LEN];
    uint8_t mac[6];
    uint8_t channel;
  } log[BFL_LOG_SIZE];
  uint8_t log_idx;
} g_bfl = {};

// ─── Random MAC ───────────────────────────────────────────────────────────────
static void bfl_random_mac(uint8_t *mac) {
  esp_fill_random(mac, 6);
  mac[0] |= 0x02;   // locally administered
  mac[0] &= ~0x01;  // unicast
}

// ─── Build RSN IE for WPA2 ───────────────────────────────────────────────────
static int bfl_build_rsn_ie(uint8_t *out) {
  // RSN IE: CCMP cipher, PSK AKM
  static const uint8_t rsn[] = {
      0x30, 0x14,              // IE=48, len=20
      0x01, 0x00,              // version 1
      0x00, 0x0F, 0xAC, 0x04,  // group: CCMP
      0x01, 0x00,              // pairwise count: 1
      0x00, 0x0F, 0xAC, 0x04,  // pairwise: CCMP
      0x01, 0x00,              // AKM count: 1
      0x00, 0x0F, 0xAC, 0x02,  // AKM: PSK
      0x00, 0x00               // RSN caps
  };
  memcpy(out, rsn, sizeof(rsn));
  return sizeof(rsn);
}

// Build beacon frame into buf, return length
static uint16_t bfl_build_beacon(uint8_t *buf, int max_len, const uint8_t *src_mac,
                                 const char *ssid, int ssid_len, uint8_t channel, BflSecurity sec,
                                 uint32_t seq) {
  if (max_len < 128)
    return 0;
  int pos = 0;

  // Radiotap header
  buf[pos++] = 0x00;
  buf[pos++] = 0x00;
  buf[pos++] = 0x08;
  buf[pos++] = 0x00;  // length = 8
  buf[pos++] = 0x00;
  buf[pos++] = 0x00;
  buf[pos++] = 0x00;
  buf[pos++] = 0x00;  // present=0

  // 802.11 MAC header — Beacon (type=0 subtype=8)
  buf[pos++] = 0x80;
  buf[pos++] = 0x00;  // FC
  buf[pos++] = 0x00;
  buf[pos++] = 0x00;  // duration
  memset(buf + pos, 0xFF, 6);
  pos += 6;  // DA = broadcast
  memcpy(buf + pos, src_mac, 6);
  pos += 6;  // SA
  memcpy(buf + pos, src_mac, 6);
  pos += 6;  // BSSID = SA for AP
  // Sequence control: fragment=0, seq=seq
  buf[pos++] = (uint8_t)((seq & 0xF) << 4);
  buf[pos++] = (uint8_t)((seq >> 4) & 0xFF);

  // Fixed parameters
  uint64_t ts = (uint64_t)(millis() * 1000);  // timestamp (µs)
  memcpy(buf + pos, &ts, 8);
  pos += 8;
  buf[pos++] = 0x64;
  buf[pos++] = 0x00;  // beacon interval = 100 TUs
  // Capability info: ESS=1, privacy=(sec!=OPEN)?1:0
  uint16_t caps = 0x0001;  // ESS
  if (sec != BFL_SEC_OPEN)
    caps |= 0x0010;  // Privacy
  buf[pos++] = (uint8_t)(caps & 0xFF);
  buf[pos++] = (uint8_t)(caps >> 8);

  // SSID IE
  buf[pos++] = 0x00;
  int sl = ssid_len < 32 ? ssid_len : 32;
  buf[pos++] = (uint8_t)sl;
  if (sl > 0) {
    memcpy(buf + pos, ssid, sl);
    pos += sl;
  }

  // Supported Rates IE
  buf[pos++] = 0x01;
  buf[pos++] = 0x08;
  uint8_t rates[] = {0x82, 0x84, 0x8B, 0x96, 0x24, 0x30, 0x48, 0x6C};
  memcpy(buf + pos, rates, 8);
  pos += 8;

  // DS Parameter IE
  buf[pos++] = 0x03;
  buf[pos++] = 0x01;
  buf[pos++] = channel;

  // TIM IE (minimal)
  buf[pos++] = 0x05;
  buf[pos++] = 0x04;
  buf[pos++] = 0x00;  // DTIM count
  buf[pos++] = 0x01;  // DTIM period
  buf[pos++] = 0x00;  // bitmap control
  buf[pos++] = 0x00;  // partial virtual bitmap

  // Extended Supported Rates
  buf[pos++] = 0x32;
  buf[pos++] = 0x04;
  buf[pos++] = 0x0C;
  buf[pos++] = 0x12;
  buf[pos++] = 0x18;
  buf[pos++] = 0x60;

  // RSN IE if WPA2+
  if (sec == BFL_SEC_WPA2 || sec == BFL_SEC_MIXED) {
    if (pos + 22 < max_len) {
      pos += bfl_build_rsn_ie(buf + pos);
    }
  }

  // WPA IE if WEP/WPA
  if (sec == BFL_SEC_WEP) {
    // WEP privacy flag already set in caps
  }

  // HT Capabilities IE (generic)
  if (pos + 28 < max_len) {
    buf[pos++] = 0x2D;
    buf[pos++] = 0x1A;  // HT Caps, 26 bytes
    buf[pos++] = 0xEF;
    buf[pos++] = 0x19;  // HT Cap Info
    buf[pos++] = 0x1B;  // A-MPDU params
    uint8_t mcs[16] = {0xFF, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    memcpy(buf + pos, mcs, 16);
    pos += 16;
    buf[pos++] = 0x00;
    buf[pos++] = 0x00;  // HT Extended Caps
    buf[pos++] = 0x00;
    buf[pos++] = 0x00;
    buf[pos++] = 0x00;
    buf[pos++] = 0x00;  // TxBF
    buf[pos++] = 0x00;  // ASEL
  }

  // Country IE
  if (pos + 8 < max_len) {
    buf[pos++] = 0x07;
    buf[pos++] = 0x06;
    buf[pos++] = 'U';
    buf[pos++] = 'S';
    buf[pos++] = ' ';  // US
    buf[pos++] = 0x01;
    buf[pos++] = 0x0B;
    buf[pos++] = 0x1E;
  }

  if (pos > max_len)
    pos = max_len;
  return (uint16_t)pos;
}

// ─── Random SSID generator ───────────────────────────────────────────────────
static void bfl_random_ssid(char *out, int len) {
  static const char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  if (len < 1)
    len = 1;
  if (len > 32)
    len = 32;
  uint8_t rnd[32];
  esp_fill_random(rnd, len);
  for (int i = 0; i < len; i++)
    out[i] = chars[rnd[i] % 64];
  out[len] = '\0';
}

// ─── Beacon task ─────────────────────────────────────────────────────────────
static void bfl_task(void *arg) {
  uint8_t buf[256];
  uint8_t cur_mac[6];

  while (g_bfl.running) {
    memcpy(cur_mac, g_bfl.base_mac, 6);
    if (g_bfl.randomize_mac)
      bfl_random_mac(cur_mac);

    uint8_t ch = g_bfl.target_channel;
    if (g_bfl.randomize_channel) {
      uint8_t rnd;
      esp_fill_random(&rnd, 1);
      ch = (rnd % 13) + 1;
    }
    if (ch < 1 || ch > 14)
      ch = 6;

    char ssid[BFL_SSID_LEN];
    int ssid_len = 0;

    switch (g_bfl.mode) {
      case BFL_MODE_SINGLE:
        if (g_bfl.ssid_count > 0) {
          strncpy(ssid, g_bfl.ssids[0], BFL_SSID_LEN - 1);
          ssid_len = strlen(ssid);
        }
        break;
      case BFL_MODE_LIST:
        if (g_bfl.ssid_count > 0) {
          uint8_t idx = g_bfl.current_ssid_idx % g_bfl.ssid_count;
          strncpy(ssid, g_bfl.ssids[idx], BFL_SSID_LEN - 1);
          ssid_len = strlen(ssid);
          g_bfl.current_ssid_idx++;
        }
        break;
      case BFL_MODE_SEQUENTIAL: {
        char seq[12];
        snprintf(seq, sizeof(seq), "%s%lu", g_bfl.ssids[0], (unsigned long)g_bfl.sent);
        int sl = strlen(seq);
        if (sl > 32)
          sl = 32;
        memcpy(ssid, seq, sl);
        ssid_len = sl;
        break;
      }
      case BFL_MODE_RANDOM: {
        uint8_t rlen;
        esp_fill_random(&rlen, 1);
        int len = (rlen % 16) + 4;
        bfl_random_ssid(ssid, len);
        ssid_len = len;
        break;
      }
      default:
        ssid_len = 0;
        break;
    }

    uint16_t pkt_len = bfl_build_beacon(buf, sizeof(buf), cur_mac, ssid, ssid_len, ch,
                                        g_bfl.security, g_bfl.seq_counter++);
    if (pkt_len > 0) {
      esp_err_t err = esp_wifi_80211_tx(WIFI_IF_AP, buf, pkt_len, false);
      if (err == ESP_OK)
        g_bfl.sent++;
      else
        g_bfl.errors++;

      // Log
      auto &le = g_bfl.log[g_bfl.log_idx % BFL_LOG_SIZE];
      le.ts = millis() / 1000;
      memcpy(le.mac, cur_mac, 6);
      strncpy(le.ssid, ssid, BFL_SSID_LEN - 1);
      le.channel = ch;
      g_bfl.log_idx++;
    }

    vTaskDelay(pdMS_TO_TICKS(g_bfl.interval_ms));
  }
  g_bfl.task_handle = nullptr;
  vTaskDelete(nullptr);
}

// ─── Control ─────────────────────────────────────────────────────────────────
inline void bfl_start(BflMode mode, uint32_t interval_ms = 50) {
  if (g_bfl.running)
    return;
  g_bfl.mode = mode;
  g_bfl.interval_ms = max((uint32_t)10, interval_ms);
  g_bfl.running = true;
  g_bfl.sent = 0;
  g_bfl.errors = 0;
  if (g_bfl.base_mac[0] == 0)
    bfl_random_mac(g_bfl.base_mac);
  xTaskCreate(bfl_task, "bfl_task", 4096, nullptr, 5, &g_bfl.task_handle);
}

inline void bfl_stop() {
  g_bfl.running = false;
  uint32_t deadline = millis() + 500;
  while (g_bfl.task_handle && millis() < deadline)
    vTaskDelay(10);
}

static String bfl_json_status() {
  String s = "{\"running\":";
  s += g_bfl.running ? "true" : "false";
  s += ",\"mode\":" + String(g_bfl.mode);
  s += ",\"sent\":" + String(g_bfl.sent);
  s += ",\"errors\":" + String(g_bfl.errors);
  s += ",\"interval_ms\":" + String(g_bfl.interval_ms);
  s += ",\"ssid_count\":" + String(g_bfl.ssid_count);
  s += ",\"channel\":" + String(g_bfl.target_channel);
  s += ",\"security\":" + String(g_bfl.security);
  s += "}";
  return s;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void bfl_routes() {
  server.on("/api/bfl/status", HTTP_GET,
            []() { server.send(200, "application/json", bfl_json_status()); });

  server.on("/api/bfl/start", HTTP_POST, []() {
    int mode = server.hasArg("mode") ? server.arg("mode").toInt() : BFL_MODE_LIST;
    int interval = server.hasArg("interval") ? server.arg("interval").toInt() : 50;
    if (server.hasArg("channel"))
      g_bfl.target_channel = server.arg("channel").toInt();
    if (server.hasArg("security"))
      g_bfl.security = (BflSecurity)server.arg("security").toInt();
    g_bfl.randomize_mac = server.hasArg("random_mac");
    g_bfl.randomize_channel = server.hasArg("random_channel");
    bfl_start((BflMode)mode, interval);
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/bfl/stop", HTTP_POST, []() {
    bfl_stop();
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/bfl/ssid/add", HTTP_POST, []() {
    if (!server.hasArg("ssid")) {
      server.send(400, "application/json", "{\"error\":\"missing ssid\"}");
      return;
    }
    if (g_bfl.ssid_count >= BFL_MAX_SSIDS) {
      server.send(400, "application/json", "{\"error\":\"list full\"}");
      return;
    }
    strncpy(g_bfl.ssids[g_bfl.ssid_count], server.arg("ssid").c_str(), BFL_SSID_LEN - 1);
    g_bfl.ssid_count++;
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/bfl/ssid/clear", HTTP_POST, []() {
    g_bfl.ssid_count = 0;
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

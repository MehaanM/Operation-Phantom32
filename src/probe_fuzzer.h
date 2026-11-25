#pragma once
// probe_fuzzer.h — 802.11 probe request fuzzer.
// Sends crafted probe requests with mutated SSIDs, random MACs, edge-case IEs,
// boundary-condition channel numbers, and oversized elements.
// All frames use esp_wifi_80211_tx (raw injection, AP interface).
// Authorized testing only. Prefix: pfz_

#include <Arduino.h>
#include <WebServer.h>
#include <esp_wifi.h>

extern WebServer server;

// ─── Fuzz modes ───────────────────────────────────────────────────────────────
enum PfzMode {
  PFZ_MODE_OFF = 0,
  PFZ_MODE_SSID_MUTATION,    // mutate SSID bytes
  PFZ_MODE_LENGTH_BOUNDARY,  // SSID length 0, 1, 32, 33 (edge)
  PFZ_MODE_RANDOM_MAC,       // random source MAC
  PFZ_MODE_MULTI_IE,         // extra duplicate IEs
  PFZ_MODE_GIANT_IE,         // oversized vendor IE
  PFZ_MODE_BROADCAST,        // broadcast probe (blank SSID)
  PFZ_MODE_SEQUENTIAL,       // sequential SSID enumeration
  PFZ_MODE_DICT,             // dictionary of common SSIDs
  PFZ_MODE_REPLAY_MUTATE,    // replay captured probes with mutations
};

// ─── State ────────────────────────────────────────────────────────────────────
#define PFZ_DICT_SIZE 64
#define PFZ_LOG_SIZE 128
#define PFZ_MAX_SSID 33

struct PfzLogEntry {
  uint32_t ts;
  uint8_t src[6];
  char ssid[PFZ_MAX_SSID];
  uint8_t mode;
  uint16_t pkt_len;
};

static struct {
  PfzMode mode;
  bool running;
  uint32_t sent;
  uint32_t errors;
  uint32_t interval_ms;
  uint8_t base_mac[6];
  char base_ssid[PFZ_MAX_SSID];
  uint32_t seq_idx;
  PfzLogEntry log[PFZ_LOG_SIZE];
  uint8_t log_idx;
  TaskHandle_t task_handle;
  uint8_t target_channel;
  bool change_mac_per_frame;
  bool verbose;

  // Dictionary
  char dict[PFZ_DICT_SIZE][PFZ_MAX_SSID];
  uint8_t dict_len;
  uint8_t dict_idx;
} g_pfz = {};

// ─── Common SSIDs dictionary ──────────────────────────────────────────────────
static const char *PFZ_COMMON_SSIDS[] = {
    "",
    "home",
    "Home",
    "HOME",
    "wifi",
    "WiFi",
    "WIFI",
    "linksys",
    "netgear",
    "dlink",
    "belkin",
    "asus",
    "tp-link",
    "xfinitywifi",
    "XFINITY",
    "ATT",
    "Verizon",
    "CenturyLink",
    "NETGEAR",
    "NETGEAR5",
    "Linksys",
    "Linksys5",
    "linksys-",
    "default",
    "Default",
    "DIRECT-",
    "AndroidAP",
    "iPhone",
    "Guest",
    "guest",
    "Free WiFi",
    "FreeWifi",
    "public",
    "Airport",
    "Starbucks",
    "Marriott",
    "hotel_wifi",
    "hotspot",
    "192.168.1.1",
    "admin",
    "Admin",
    "ADMIN",
    "password",
    "00:00:00:00:00:00",
    "\x00\x00",
    "\xff\xff",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",   // 31 A's
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",  // 32 A's
    "\x00\x01\x02\x03\x04\x05\x06\x07",  // binary
    "\xff\xfe\xfd\xfc\xfb\xfa",          // high bytes
};
#define PFZ_COMMON_SSIDS_LEN 48

// ─── Frame builder ────────────────────────────────────────────────────────────
static uint16_t pfz_build_probe_req(uint8_t *out, int max_len, const uint8_t *src_mac,
                                    const char *ssid, int ssid_len, bool multi_ie,
                                    bool giant_vendor) {
  if (max_len < 50)
    return 0;
  int pos = 0;

  // Radiotap header (minimal)
  out[pos++] = 0x00;  // version
  out[pos++] = 0x00;  // pad
  out[pos++] = 0x08;  // length lo
  out[pos++] = 0x00;  // length hi
  out[pos++] = 0x00;
  out[pos++] = 0x00;
  out[pos++] = 0x00;
  out[pos++] = 0x00;  // present=0

  // 802.11 MAC header — Probe Request (type=0 subtype=4)
  out[pos++] = 0x40;  // FC byte 0: type=mgmt(00), subtype=0100(probe req)
  out[pos++] = 0x00;  // FC byte 1: flags
  out[pos++] = 0x00;
  out[pos++] = 0x00;  // duration
  // DA = FF:FF:FF:FF:FF:FF
  memset(out + pos, 0xFF, 6);
  pos += 6;
  // SA = src_mac
  memcpy(out + pos, src_mac, 6);
  pos += 6;
  // BSSID = FF:FF:FF:FF:FF:FF
  memset(out + pos, 0xFF, 6);
  pos += 6;
  // Sequence control
  out[pos++] = 0x00;
  out[pos++] = 0x00;

  // SSID IE (tag 0)
  out[pos++] = 0x00;  // tag
  int clamped_ssid = ssid_len < 0 ? 0 : (ssid_len > 32 ? 32 : ssid_len);
  out[pos++] = (uint8_t)clamped_ssid;
  if (clamped_ssid > 0) {
    memcpy(out + pos, ssid, clamped_ssid);
    pos += clamped_ssid;
  }

  // Supported Rates IE (tag 1)
  out[pos++] = 0x01;
  out[pos++] = 0x08;
  uint8_t rates[] = {0x82, 0x84, 0x8B, 0x96, 0x24, 0x30, 0x48, 0x6C};
  memcpy(out + pos, rates, 8);
  pos += 8;

  // DS Parameter Set IE (tag 3) — channel
  out[pos++] = 0x03;
  out[pos++] = 0x01;
  out[pos++] = g_pfz.target_channel ? g_pfz.target_channel : 1;

  // Extended Supported Rates IE (tag 50)
  out[pos++] = 0x32;
  out[pos++] = 0x04;
  out[pos++] = 0x0C;
  out[pos++] = 0x12;
  out[pos++] = 0x18;
  out[pos++] = 0x60;

  if (multi_ie) {
    // Duplicate SSID IE (tag 0 again — edge case)
    out[pos++] = 0x00;
    out[pos++] = clamped_ssid;
    if (clamped_ssid > 0 && pos + clamped_ssid < max_len) {
      memcpy(out + pos, ssid, clamped_ssid);
      pos += clamped_ssid;
    }
    // Unknown IE with large tag number
    if (pos + 4 < max_len) {
      out[pos++] = 0xDD;  // vendor-specific
      out[pos++] = 0x04;
      out[pos++] = 0x00;
      out[pos++] = 0x50;
      out[pos++] = 0xF2;
      out[pos++] = 0x01;
    }
  }

  if (giant_vendor && pos + 32 < max_len) {
    // Vendor IE with oversized payload
    out[pos++] = 0xDD;
    out[pos++] = 0x1E;  // 30 bytes
    uint8_t oui[] = {0x00, 0x50, 0xF2, 0x04};
    memcpy(out + pos, oui, 4);
    pos += 4;
    memset(out + pos, 0xAA, 26);
    pos += 26;
  }

  if (pos > max_len)
    pos = max_len;
  return (uint16_t)pos;
}

// ─── Random MAC ───────────────────────────────────────────────────────────────
static void pfz_random_mac(uint8_t *mac) {
  esp_fill_random(mac, 6);
  mac[0] |= 0x02;   // locally administered
  mac[0] &= ~0x01;  // unicast
}

// ─── Mutation strategies ──────────────────────────────────────────────────────
static void pfz_mutate_ssid(const char *base, int base_len, char *out, int *out_len,
                            uint32_t iter) {
  *out_len = base_len;
  memcpy(out, base, base_len);
  if (base_len == 0)
    return;

  uint8_t strat = iter % 8;
  switch (strat) {
    case 0:  // flip random bit
      out[iter % base_len] ^= (1 << (iter % 8));
      break;
    case 1:  // increment random byte
      out[iter % base_len]++;
      break;
    case 2:  // null byte injection
      out[iter % base_len] = 0x00;
      break;
    case 3:  // 0xFF injection
      out[iter % base_len] = 0xFF;
      break;
    case 4:  // extend by 1
      if (base_len < 32) {
        out[base_len] = 'A';
        (*out_len)++;
      }
      break;
    case 5:  // truncate
      if (base_len > 1)
        (*out_len)--;
      break;
    case 6:  // append null
      if (base_len < 32) {
        out[base_len] = 0x00;
        (*out_len)++;
      }
      break;
    case 7:  // repeat first byte
      memset(out, out[0], base_len);
      break;
  }
}

// ─── Send frame ───────────────────────────────────────────────────────────────
static void pfz_send_frame(const uint8_t *buf, uint16_t len) {
  esp_err_t err = esp_wifi_80211_tx(WIFI_IF_AP, buf, len, false);
  if (err == ESP_OK) {
    g_pfz.sent++;
  } else {
    g_pfz.errors++;
  }
}

// ─── Log entry ───────────────────────────────────────────────────────────────
static void pfz_log(const uint8_t *src, const char *ssid, int ssid_len, uint16_t pkt_len) {
  PfzLogEntry &e = g_pfz.log[g_pfz.log_idx % PFZ_LOG_SIZE];
  e.ts = millis() / 1000;
  memcpy(e.src, src, 6);
  int cp = ssid_len < (int)(PFZ_MAX_SSID - 1) ? ssid_len : PFZ_MAX_SSID - 1;
  memcpy(e.ssid, ssid, cp);
  e.ssid[cp] = '\0';
  e.mode = (uint8_t)g_pfz.mode;
  e.pkt_len = pkt_len;
  g_pfz.log_idx++;
}

// ─── Fuzzer task ─────────────────────────────────────────────────────────────
static void pfz_task(void *arg) {
  uint8_t buf[256];
  uint8_t cur_mac[6];

  while (g_pfz.running) {
    memcpy(cur_mac, g_pfz.base_mac, 6);
    if (g_pfz.change_mac_per_frame)
      pfz_random_mac(cur_mac);

    char ssid[PFZ_MAX_SSID];
    int ssid_len = 0;
    bool multi = false;
    bool giant = false;

    switch (g_pfz.mode) {
      case PFZ_MODE_SSID_MUTATION: {
        int base_len = strlen(g_pfz.base_ssid);
        pfz_mutate_ssid(g_pfz.base_ssid, base_len, ssid, &ssid_len, g_pfz.seq_idx);
        break;
      }
      case PFZ_MODE_LENGTH_BOUNDARY: {
        int lens[] = {0, 1, 2, 16, 31, 32};
        int li = g_pfz.seq_idx % 6;
        ssid_len = lens[li];
        memset(ssid, 'A', ssid_len);
        break;
      }
      case PFZ_MODE_RANDOM_MAC:
        pfz_random_mac(cur_mac);
        ssid_len = strlen(g_pfz.base_ssid);
        memcpy(ssid, g_pfz.base_ssid, ssid_len);
        break;
      case PFZ_MODE_MULTI_IE:
        ssid_len = strlen(g_pfz.base_ssid);
        memcpy(ssid, g_pfz.base_ssid, ssid_len);
        multi = true;
        break;
      case PFZ_MODE_GIANT_IE:
        ssid_len = strlen(g_pfz.base_ssid);
        memcpy(ssid, g_pfz.base_ssid, ssid_len);
        giant = true;
        break;
      case PFZ_MODE_BROADCAST:
        ssid_len = 0;
        break;
      case PFZ_MODE_SEQUENTIAL: {
        char seq_str[12];
        snprintf(seq_str, sizeof(seq_str), "%s%lu", g_pfz.base_ssid, (unsigned long)g_pfz.seq_idx);
        ssid_len = strlen(seq_str);
        if (ssid_len > 32)
          ssid_len = 32;
        memcpy(ssid, seq_str, ssid_len);
        break;
      }
      case PFZ_MODE_DICT:
        if (g_pfz.dict_len > 0) {
          const char *s = g_pfz.dict[g_pfz.dict_idx % g_pfz.dict_len];
          ssid_len = strlen(s);
          if (ssid_len > 32)
            ssid_len = 32;
          memcpy(ssid, s, ssid_len);
          g_pfz.dict_idx = (g_pfz.dict_idx + 1) % g_pfz.dict_len;
        } else {
          uint8_t ci = g_pfz.seq_idx % PFZ_COMMON_SSIDS_LEN;
          const char *s = PFZ_COMMON_SSIDS[ci];
          ssid_len = strlen(s);
          if (ssid_len > 32)
            ssid_len = 32;
          memcpy(ssid, s, ssid_len);
        }
        break;
      default:
        ssid_len = 0;
        break;
    }

    uint16_t pkt_len = pfz_build_probe_req(buf, sizeof(buf), cur_mac, ssid, ssid_len, multi, giant);
    if (pkt_len > 0) {
      pfz_send_frame(buf, pkt_len);
      if (g_pfz.verbose)
        pfz_log(cur_mac, ssid, ssid_len, pkt_len);
    }

    g_pfz.seq_idx++;
    vTaskDelay(pdMS_TO_TICKS(g_pfz.interval_ms));
  }
  g_pfz.task_handle = nullptr;
  vTaskDelete(nullptr);
}

// ─── Control ─────────────────────────────────────────────────────────────────
inline void pfz_start(PfzMode mode, uint32_t interval_ms = 100) {
  if (g_pfz.running)
    return;
  g_pfz.mode = mode;
  g_pfz.interval_ms = interval_ms < 10 ? 10 : interval_ms;
  g_pfz.running = true;
  g_pfz.sent = 0;
  g_pfz.errors = 0;
  g_pfz.seq_idx = 0;
  g_pfz.dict_idx = 0;

  // Default base MAC if not set
  if (pmk_mac_zero(g_pfz.base_mac))
    pfz_random_mac(g_pfz.base_mac);

  xTaskCreate(pfz_task, "pfz_task", 4096, nullptr, 5, &g_pfz.task_handle);
}

inline void pfz_stop() {
  g_pfz.running = false;
  uint32_t deadline = millis() + 500;
  while (g_pfz.task_handle && millis() < deadline)
    vTaskDelay(10);
}

static bool pmk_mac_zero_pfz(const uint8_t *m) {
  return !m[0] && !m[1] && !m[2] && !m[3] && !m[4] && !m[5];
}

// ─── JSON status ─────────────────────────────────────────────────────────────
static String pfz_json_status() {
  char mac[18];
  snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X", g_pfz.base_mac[0], g_pfz.base_mac[1],
           g_pfz.base_mac[2], g_pfz.base_mac[3], g_pfz.base_mac[4], g_pfz.base_mac[5]);
  String s = "{\"running\":";
  s += g_pfz.running ? "true" : "false";
  s += ",\"mode\":" + String(g_pfz.mode);
  s += ",\"sent\":" + String(g_pfz.sent);
  s += ",\"errors\":" + String(g_pfz.errors);
  s += ",\"interval_ms\":" + String(g_pfz.interval_ms);
  s += ",\"seq_idx\":" + String(g_pfz.seq_idx);
  s += ",\"base_mac\":\"" + String(mac) + "\"";
  s += ",\"base_ssid\":\"" + String(g_pfz.base_ssid) + "\"";
  s += ",\"dict_len\":" + String(g_pfz.dict_len);
  s += ",\"log_entries\":" + String(min((int)g_pfz.log_idx, PFZ_LOG_SIZE));
  s += "}";
  return s;
}

static String pfz_json_log() {
  int total = min((int)g_pfz.log_idx, PFZ_LOG_SIZE);
  String s = "{\"entries\":[";
  bool first = true;
  for (int i = 0; i < total; i++) {
    int idx = (g_pfz.log_idx - 1 - i + PFZ_LOG_SIZE) % PFZ_LOG_SIZE;
    PfzLogEntry &e = g_pfz.log[idx];
    if (!first)
      s += ",";
    first = false;
    char mac[18];
    snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X", e.src[0], e.src[1], e.src[2],
             e.src[3], e.src[4], e.src[5]);
    s += "{\"ts\":" + String(e.ts);
    s += ",\"src\":\"" + String(mac) + "\"";
    // JSON-escape SSID
    s += ",\"ssid\":\"";
    for (int j = 0; e.ssid[j] && j < 32; j++) {
      char c = e.ssid[j];
      if (c < 0x20 || c == '"' || c == '\\') {
        char esc[8];
        snprintf(esc, sizeof(esc), "\\u%04X", (uint8_t)c);
        s += esc;
      } else {
        s += c;
      }
    }
    s += "\"";
    s += ",\"mode\":" + String(e.mode);
    s += ",\"len\":" + String(e.pkt_len);
    s += "}";
  }
  s += "]}";
  return s;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void pfz_routes() {
  server.on("/api/pfz/status", HTTP_GET,
            []() { server.send(200, "application/json", pfz_json_status()); });

  server.on("/api/pfz/start", HTTP_POST, []() {
    int mode = server.hasArg("mode") ? server.arg("mode").toInt() : PFZ_MODE_SSID_MUTATION;
    int interval = server.hasArg("interval") ? server.arg("interval").toInt() : 100;
    if (server.hasArg("ssid")) {
      strncpy(g_pfz.base_ssid, server.arg("ssid").c_str(), PFZ_MAX_SSID - 1);
      g_pfz.base_ssid[PFZ_MAX_SSID - 1] = '\0';
    }
    if (server.hasArg("mac")) {
      sscanf(server.arg("mac").c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &g_pfz.base_mac[0],
             &g_pfz.base_mac[1], &g_pfz.base_mac[2], &g_pfz.base_mac[3], &g_pfz.base_mac[4],
             &g_pfz.base_mac[5]);
    }
    if (server.hasArg("channel"))
      g_pfz.target_channel = server.arg("channel").toInt();
    g_pfz.change_mac_per_frame = server.hasArg("random_mac");
    pfz_start((PfzMode)mode, interval);
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/pfz/stop", HTTP_POST, []() {
    pfz_stop();
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/pfz/log", HTTP_GET,
            []() { server.send(200, "application/json", pfz_json_log()); });

  server.on("/api/pfz/clear_log", HTTP_POST, []() {
    g_pfz.log_idx = 0;
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/pfz/dict/add", HTTP_POST, []() {
    if (!server.hasArg("ssid")) {
      server.send(400, "application/json", "{\"error\":\"missing ssid\"}");
      return;
    }
    if (g_pfz.dict_len >= PFZ_DICT_SIZE) {
      server.send(400, "application/json", "{\"error\":\"dict full\"}");
      return;
    }
    strncpy(g_pfz.dict[g_pfz.dict_len], server.arg("ssid").c_str(), PFZ_MAX_SSID - 1);
    g_pfz.dict[g_pfz.dict_len][PFZ_MAX_SSID - 1] = '\0';
    g_pfz.dict_len++;
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/pfz/dict/clear", HTTP_POST, []() {
    g_pfz.dict_len = 0;
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

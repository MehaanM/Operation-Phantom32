#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdlib.h>
#include <string.h>
#include "module.h"
#include "safety.h"
#include "frames.h"

// ── SSID Fuzzer ───────────────────────────────────────────────────────────────
//
// Sends beacon/probe-response frames with carefully crafted SSIDs to test
// how clients, scanners, and drivers handle unusual UTF-8, length-edge,
// format-string, and control-character sequences.
//
// Fuzz modes:
//   utf8      — Unicode ranges: BOM, right-to-left, zero-width, surrogates
//   long      — length edge cases: 0, 1, 31, 32, 33, 128, 255 byte SSIDs
//   ctrl      — control chars (0x00-0x1F, DEL, non-printable ASCII)
//   fmt       — C-style format sequences (%s %d %x %n %p)
//   inject    — SSID IE overflow candidates and malformed IE lengths
//   emoji     — high-codepoint emoji and multi-byte sequences
//   repeated  — single-byte repetitions across the visible ASCII range
//   probe     — probe request flood with each fuzz SSID
//   all       — cycle all modes in sequence

// ── Fuzz templates ────────────────────────────────────────────────────────────
struct FuzzEntry {
  const char *label;
  const char *ssid;  // UTF-8 literal; null-terminated; may contain \x00 bytes only if len is set
  uint8_t len;       // 0 = use strlen; nonzero = exact length override
};

#define FUZZ_ENTRIES_MAX 80

static const FuzzEntry FUZZ_UTF8[] = {
    {"bom",
     "\xEF\xBB\xBF"
     "BOM",
     0},
    {"rtl-mark",
     "\xE2\x80\x8F"
     "RTL",
     0},
    {"ltr-mark",
     "\xE2\x80\x8E"
     "LTR",
     0},
    {"zero-width",
     "\xE2\x80\x8B"
     "ZeroW",
     0},
    {"null-embed", "SSID\x00END", 9},
    {"nul-only", "\x00", 1},
    {"lone-surr", "\xED\xA0\x80", 0},                   // U+D800 (lone surrogate)
    {"overlong-2", "\xC0\xAF", 0},                      // overlong /
    {"overlong-3", "\xE0\x80\xAF", 0},                  // overlong /
    {"maxpoint", "\xF4\x8F\xBF\xBF", 0},                // U+10FFFF
    {"beyond-max", "\xF4\x90\x80\x80", 0},              // U+110000 (invalid)
    {"arabic", "\xD8\xAD\xD9\x8A\xD8\xA7\xD9\x81", 0},  // حياف
    {"chinese", "\xE4\xB8\xAD\xE6\x96\x87", 0},         // 中文
    {"japanese", "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E", 0},
    {"greek", "\xCE\x91\xCE\xB2\xCE\xB3", 0},  // Αβγ
    {"replacement", "\xEF\xBF\xBD", 0},        // U+FFFD
    {"private-use", "\xEF\x80\x80", 0},        // U+F000
    {"invisible",
     "\xE2\x81\xA0"
     "text",
     0},                               // word joiner
    {"variation", "\xE2\x80\x8C", 0},  // ZWNJ
};
static const int FUZZ_UTF8_COUNT = sizeof(FUZZ_UTF8) / sizeof(FUZZ_UTF8[0]);

static const FuzzEntry FUZZ_LONG[] = {
    {"empty", "", 0},
    {"1char", "X", 0},
    {"31chars", "0123456789012345678901234567890", 0},
    {"32chars", "01234567890123456789012345678901", 0},
    {"33chars", "012345678901234567890123456789012", 33},
    {"all-spaces", "                                ", 32},
    {"63chars", "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", 63},
    {"255bytes", nullptr, 0},  // generated at runtime
};
#define FUZZ_LONG_STATIC 7

static const FuzzEntry FUZZ_CTRL[] = {
    {"nul", "\x00", 1},
    {"soh", "\x01Test", 0},
    {"bel", "\x07Bell\x07", 0},
    {"bs", "\x08BS", 0},
    {"tab", "\x09Tab", 0},
    {"lf", "Line\x0AFeed", 0},
    {"cr", "CR\x0D", 0},
    {"esc", "\x1BESC", 0},
    {"del", "\x7FDEL", 0},
    {"high-c0", "\x1F\x1E\x1D", 0},
    {"c1-80", "\xC2\x80", 0},
    {"c1-9f", "\xC2\x9F", 0},
    {"mixed-ctrl", "A\x01B\x02C", 0},
    {"all-ctrl", "\x01\x02\x03\x04\x05\x06\x07\x08", 8},
};
static const int FUZZ_CTRL_COUNT = sizeof(FUZZ_CTRL) / sizeof(FUZZ_CTRL[0]);

static const FuzzEntry FUZZ_FMT[] = {
    {"pct-s", "%s", 0},
    {"pct-d", "%d", 0},
    {"pct-x", "%x", 0},
    {"pct-n", "%n", 0},
    {"pct-p", "%p", 0},
    {"pct-pct", "%%", 0},
    {"pct-10s", "%10s", 0},
    {"pct-neg", "%-10s", 0},
    {"repeat-pct-s", "%s%s%s%s%s", 0},
    {"format-chain", "%s/%s/%d.%d", 0},
    {"log4j-like", "${jndi:ldap://fuzz}", 0},
    {"sqli", "' OR 1=1--", 0},
    {"sqli2", "'; DROP TABLE aps;", 0},
    {"xss", "<script>alert(1)</script>", 0},
    {"path-trav", "../../../../etc/passwd", 0},
    {"null-fmt", "\x00%s\x00", 5},
};
static const int FUZZ_FMT_COUNT = sizeof(FUZZ_FMT) / sizeof(FUZZ_FMT[0]);

static const FuzzEntry FUZZ_EMOJI[] = {
    {"fire", "\xF0\x9F\x94\xA5", 0},
    {"wifi", "\xF0\x9F\x93\xB6", 0},
    {"lock", "\xF0\x9F\x94\x92", 0},
    {"skull", "\xF0\x9F\x92\x80", 0},
    {"zap", "\xE2\x9A\xA1", 0},
    {"star", "\xE2\xAD\x90", 0},
    {"rainbow", "\xF0\x9F\x8C\x88", 0},
    {"100", "\xF0\x9F\x92\xAF", 0},
    {"multi-emoji", "\xF0\x9F\x94\xA5\xF0\x9F\x92\x80\xE2\x9A\xA1", 0},
    {"zwj-sequence", "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x92\xBB", 0},  // man+laptop ZWJ
    {"skin-tone", "\xF0\x9F\x91\x8D\xF0\x9F\x8F\xBB", 0},                 // thumbs up + light skin
    {"regional", "\xF0\x9F\x87\xBA\xF0\x9F\x87\xB8", 0},                  // flag US
    {"variation-16", "\xE2\x9C\x82\xEF\xB8\x8F", 0},                      // scissors + variation-16
};
static const int FUZZ_EMOJI_COUNT = sizeof(FUZZ_EMOJI) / sizeof(FUZZ_EMOJI[0]);

static const FuzzEntry FUZZ_INJECT[] = {
    {"ie-len-ff", nullptr, 0},      // 255-byte IE body generated at runtime
    {"ie-len-00", nullptr, 0},      // 0-byte SSID IE
    {"multi-ssid-ie", nullptr, 0},  // two SSID IEs in one beacon
    {"fmt-ssid-ie", nullptr, 0},    // SSID IE claiming len=1 but containing 32 bytes
};

// ── State ─────────────────────────────────────────────────────────────────────
static bool sf_running = false;
static TaskHandle_t sf_task = nullptr;
static volatile uint32_t sf_sent = 0;
static volatile uint32_t sf_epoch = 0;
static volatile int sf_mode_idx = 0;  // current fuzz mode
static char sf_cur_ssid[33] = "";
static char sf_cur_label[32] = "";
static char sf_log[12][64];
static int sf_log_head = 0;
static int sf_log_count = 0;

static const char *const SF_MODE_NAMES[] = {"utf8",  "long",     "ctrl",  "fmt", "inject",
                                            "emoji", "repeated", "probe", "all"};

static void sf_log_add(const char *fmt, ...) {
  char tmp[64];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(tmp, 64, fmt, ap);
  va_end(ap);
  memcpy(sf_log[sf_log_head], tmp, 64);
  sf_log_head = (sf_log_head + 1) % 12;
  if (sf_log_count < 12)
    sf_log_count++;
}

// ── Craft and transmit a beacon with arbitrary SSID bytes ─────────────────────
static void sf_send_beacon(const uint8_t *ssid_bytes, uint8_t ssid_len, uint8_t ch) {
  static uint8_t buf[512];
  static uint8_t bssid[6] = {0x02, 0xAB, 0xCD, 0x00, 0x00, 0x01};
  // Randomize BSSID low 3 bytes each call
  bssid[3] = (uint8_t)(esp_random() & 0xFF);
  bssid[4] = (uint8_t)(esp_random() & 0xFF);
  bssid[5] = (uint8_t)(esp_random() & 0xFF);
  static const uint8_t DST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  uint8_t seq = (uint8_t)(esp_random() & 0xFF);

  int pos = 0;
  // Frame control: beacon (0x80 0x00)
  buf[pos++] = 0x80;
  buf[pos++] = 0x00;
  // Duration
  buf[pos++] = 0x00;
  buf[pos++] = 0x00;
  // DA (broadcast), SA (bssid), BSSID
  memcpy(buf + pos, DST, 6);
  pos += 6;
  memcpy(buf + pos, bssid, 6);
  pos += 6;
  memcpy(buf + pos, bssid, 6);
  pos += 6;
  // Seq ctrl
  buf[pos++] = (seq << 4) & 0xF0;
  buf[pos++] = seq >> 4;
  // Timestamp (8 bytes)
  uint64_t ts = (uint64_t)millis() * 1000ULL;
  memcpy(buf + pos, &ts, 8);
  pos += 8;
  // Beacon interval (100 TUs = 0x64 0x00)
  buf[pos++] = 0x64;
  buf[pos++] = 0x00;
  // Capability info (ESS + Privacy = WPA2 presence hint)
  buf[pos++] = 0x11;
  buf[pos++] = 0x04;
  // SSID IE (id=0)
  buf[pos++] = 0x00;
  buf[pos++] = ssid_len;
  if (ssid_len) {
    memcpy(buf + pos, ssid_bytes, ssid_len);
    pos += ssid_len;
  }
  // Supported rates IE (id=1)
  buf[pos++] = 0x01;
  buf[pos++] = 0x08;
  static const uint8_t rates[] = {0x82, 0x84, 0x8B, 0x96, 0x24, 0x30, 0x48, 0x6C};
  memcpy(buf + pos, rates, 8);
  pos += 8;
  // DS Parameter IE (id=3) — channel
  buf[pos++] = 0x03;
  buf[pos++] = 0x01;
  buf[pos++] = ch;
  // TIM IE minimal (id=5)
  buf[pos++] = 0x05;
  buf[pos++] = 0x04;
  buf[pos++] = 0x00;
  buf[pos++] = 0x01;
  buf[pos++] = 0x00;
  buf[pos++] = 0x00;
  // RSN IE (WPA2) so clients show it as secured
  static const uint8_t rsn[] = {0x30, 0x14, 0x01, 0x00, 0x00, 0x0F, 0xAC, 0x04, 0x01, 0x00, 0x00,
                                0x0F, 0xAC, 0x04, 0x01, 0x00, 0x00, 0x0F, 0xAC, 0x02, 0x00, 0x00};
  memcpy(buf + pos, rsn, sizeof(rsn));
  pos += sizeof(rsn);

  esp_wifi_80211_tx(WIFI_IF_AP, buf, pos, false);
}

static void sf_send_probe_req(const uint8_t *ssid_bytes, uint8_t ssid_len, uint8_t ch) {
  static uint8_t buf[128];
  static uint8_t src[6] = {0x02, 0xCC, 0xDD, 0x00, 0x00, 0x01};
  static const uint8_t BCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  src[3] = (uint8_t)(esp_random() & 0xFF);
  src[4] = (uint8_t)(esp_random() & 0xFF);
  src[5] = (uint8_t)(esp_random() & 0xFF);

  int pos = 0;
  buf[pos++] = 0x40;
  buf[pos++] = 0x00;  // probe request
  buf[pos++] = 0x00;
  buf[pos++] = 0x00;  // duration
  memcpy(buf + pos, BCAST, 6);
  pos += 6;
  memcpy(buf + pos, src, 6);
  pos += 6;
  memcpy(buf + pos, BCAST, 6);
  pos += 6;
  buf[pos++] = 0x00;
  buf[pos++] = 0x00;  // seq
  // SSID IE
  buf[pos++] = 0x00;
  buf[pos++] = ssid_len;
  if (ssid_len) {
    memcpy(buf + pos, ssid_bytes, ssid_len);
    pos += ssid_len;
  }
  // Supported rates
  buf[pos++] = 0x01;
  buf[pos++] = 0x08;
  static const uint8_t rates[] = {0x02, 0x04, 0x0B, 0x16, 0x24, 0x30, 0x48, 0x6C};
  memcpy(buf + pos, rates, 8);
  pos += 8;

  esp_wifi_80211_tx(WIFI_IF_AP, buf, pos, false);
}

// ── Fuzz runner helpers ───────────────────────────────────────────────────────
static void sf_run_entry(const FuzzEntry &e, bool probe, uint8_t ch) {
  const uint8_t *bytes = nullptr;
  uint8_t len = 0;
  static uint8_t longbuf[256];

  if (e.ssid == nullptr) {
    // Generate at runtime
    if (e.len == 255) {
      memset(longbuf, 'A', 255);
      bytes = longbuf;
      len = 255;
    } else {
      memset(longbuf, 0x41, 32);
      bytes = longbuf;
      len = 32;
    }
  } else {
    bytes = (const uint8_t *)e.ssid;
    len = e.len ? e.len : (uint8_t)strlen(e.ssid);
    if (len > 32)
      len = 32;  // cap at IE max for well-formed beacons
  }

  if (bytes) {
    int slen = len < 32 ? len : 32;
    memcpy(sf_cur_ssid, bytes, slen);
    sf_cur_ssid[slen] = 0;
  }
  strncpy(sf_cur_label, e.label, 31);
  sf_cur_label[31] = 0;

  if (probe)
    sf_send_probe_req(bytes, len, ch);
  else
    sf_send_beacon(bytes, len, ch);
  sf_sent++;
}

static uint8_t sf_mode_from_str(const String &m) {
  for (int i = 0; i < 9; i++)
    if (m == SF_MODE_NAMES[i])
      return (uint8_t)i;
  return 7;  // "all"
}

// ── Task ──────────────────────────────────────────────────────────────────────
static uint8_t sf_mode = 7;       // "all"
static uint8_t sf_target_ch = 0;  // 0=rotate

static void sf_run_task(void *) {
  sf_log_add("Fuzzer started mode=%s", SF_MODE_NAMES[sf_mode]);
  uint8_t ch = 1;

  while (sf_running) {
    if (sf_target_ch)
      ch = sf_target_ch;
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);

    // Dispatch to mode-specific set
    bool all = (sf_mode == 8);
    uint8_t m = all ? (uint8_t)(sf_epoch % 7) : sf_mode;

    switch (m) {
      case 0:  // utf8
        for (int i = 0; i < FUZZ_UTF8_COUNT && sf_running; i++) {
          sf_run_entry(FUZZ_UTF8[i], false, ch);
          vTaskDelay(pdMS_TO_TICKS(50));
        }
        break;
      case 1:  // long
        for (int i = 0; i < FUZZ_LONG_STATIC && sf_running; i++) {
          sf_run_entry(FUZZ_LONG[i], false, ch);
          vTaskDelay(pdMS_TO_TICKS(50));
        }
        {  // 255-byte
          static uint8_t lb[256];
          memset(lb, 0x41, 255);
          strncpy(sf_cur_label, "255bytes", 31);
          sf_send_beacon(lb, 255, ch);
          sf_sent++;
          vTaskDelay(pdMS_TO_TICKS(50));
        }
        break;
      case 2:  // ctrl
        for (int i = 0; i < FUZZ_CTRL_COUNT && sf_running; i++) {
          sf_run_entry(FUZZ_CTRL[i], false, ch);
          vTaskDelay(pdMS_TO_TICKS(50));
        }
        break;
      case 3:  // fmt
        for (int i = 0; i < FUZZ_FMT_COUNT && sf_running; i++) {
          sf_run_entry(FUZZ_FMT[i], false, ch);
          vTaskDelay(pdMS_TO_TICKS(50));
        }
        break;
      case 4:  // inject
      {
        static uint8_t ie_buf[256];
        memset(ie_buf, 0xAA, 255);
        strncpy(sf_cur_label, "ie-ff", 31);
        sf_send_beacon(ie_buf, 255, ch);
        sf_sent++;
        vTaskDelay(pdMS_TO_TICKS(100));
        // Zero-length SSID IE
        strncpy(sf_cur_label, "ie-len0", 31);
        sf_send_beacon(ie_buf, 0, ch);
        sf_sent++;
        vTaskDelay(pdMS_TO_TICKS(100));
      } break;
      case 5:  // emoji
        for (int i = 0; i < FUZZ_EMOJI_COUNT && sf_running; i++) {
          sf_run_entry(FUZZ_EMOJI[i], false, ch);
          vTaskDelay(pdMS_TO_TICKS(50));
        }
        break;
      case 6:  // repeated single-byte
        for (int b = 0x20; b <= 0x7E && sf_running; b++) {
          static uint8_t rep[32];
          memset(rep, b, 16);
          snprintf(sf_cur_label, 32, "rep-0x%02X", b);
          sf_send_beacon(rep, 16, ch);
          sf_sent++;
          vTaskDelay(pdMS_TO_TICKS(30));
        }
        break;
      case 7:  // probe mode — send probe requests instead
        for (int i = 0; i < FUZZ_UTF8_COUNT && sf_running; i++) {
          sf_run_entry(FUZZ_UTF8[i], true, ch);
          vTaskDelay(pdMS_TO_TICKS(50));
        }
        for (int i = 0; i < FUZZ_FMT_COUNT && sf_running; i++) {
          sf_run_entry(FUZZ_FMT[i], true, ch);
          vTaskDelay(pdMS_TO_TICKS(50));
        }
        break;
      default:
        break;
    }

    sf_epoch++;
    if (!sf_target_ch)
      ch = ch % 13 + 1;
    sf_log_add("epoch %u: sent=%u ch=%u mode=%s", (unsigned)sf_epoch, (unsigned)sf_sent, ch,
               SF_MODE_NAMES[m]);
    vTaskDelay(pdMS_TO_TICKS(200));
  }

  sf_task = nullptr;
  vTaskDelete(nullptr);
}

// ── Start/Stop ────────────────────────────────────────────────────────────────
static bool sf_start(const String &args) {
  if (sf_running)
    return false;
  sf_sent = 0;
  sf_epoch = 0;
  sf_log_head = 0;
  sf_log_count = 0;
  sf_cur_ssid[0] = 0;
  sf_cur_label[0] = 0;
  // Parse mode
  int p = args.indexOf("mode=");
  if (p >= 0) {
    String mv = args.substring(p + 5);
    int end = mv.indexOf('&');
    if (end > 0)
      mv = mv.substring(0, end);
    sf_mode = sf_mode_from_str(mv);
  } else
    sf_mode = 7;  // all
  // Parse channel
  p = args.indexOf("ch=");
  if (p >= 0)
    sf_target_ch = (uint8_t)args.substring(p + 3).toInt();
  else
    sf_target_ch = 0;
  sf_running = true;
  xTaskCreate(sf_run_task, "ssid_fuzz", 4096, nullptr, 3, &sf_task);
  return true;
}
static void sf_stop() {
  sf_running = false;
  if (sf_task) {
    vTaskDelay(pdMS_TO_TICKS(100));
    sf_task = nullptr;
  }
}

// ── Routes ────────────────────────────────────────────────────────────────────
static void sf_routes() {
  extern WebServer server;

  server.on("/api/ssidfuzz/start", HTTP_POST, []() {
    String q;
    for (int i = 0; i < server.args(); i++) {
      if (i)
        q += '&';
      q += server.argName(i) + '=' + server.arg(i);
    }
    bool ok = tool_start("ssidfuzz", q);
    server.send(200, "application/json", ok ? "{\"ok\":1}" : "{\"ok\":0,\"err\":\"busy\"}");
  });

  server.on("/api/ssidfuzz/stop", HTTP_POST, []() {
    tool_stop("ssidfuzz");
    server.send(200, "application/json", "{\"ok\":1}");
  });

  server.on("/api/ssidfuzz/status", HTTP_GET, []() {
    String j = "{";
    j += "\"running\":" + String(sf_running ? 1 : 0);
    j += ",\"sent\":" + String(sf_sent);
    j += ",\"epoch\":" + String(sf_epoch);
    j += ",\"mode\":\"" + String(SF_MODE_NAMES[sf_mode]) + "\"";
    j += ",\"cur_label\":\"" + String(sf_cur_label) + "\"";
    // SSID as hex
    j += ",\"cur_ssid_hex\":\"";
    for (int i = 0; sf_cur_ssid[i] && i < 32; i++) {
      uint8_t b = (uint8_t)sf_cur_ssid[i];
      if (b < 16)
        j += '0';
      j += String(b, 16);
    }
    j += "\"";
    // Log
    j += ",\"log\":[";
    bool first = true;
    for (int i = 0; i < sf_log_count; i++) {
      int idx = ((sf_log_head - 1 - i) + 12) % 12;
      if (!sf_log[idx][0])
        continue;
      if (!first)
        j += ",";
      j += "\"";
      j += sf_log[idx];
      j += "\"";
      first = false;
    }
    j += "]}";
    server.send(200, "application/json", j);
  });

  server.on("/api/ssidfuzz/modes", HTTP_GET, []() {
    String j = "[";
    for (int i = 0; i < 9; i++) {
      if (i)
        j += ",";
      j += "\"";
      j += SF_MODE_NAMES[i];
      j += "\"";
    }
    j += "]";
    server.send(200, "application/json", j);
  });
}

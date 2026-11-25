#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <string.h>
#include <stdint.h>
#include "esp_wifi.h"

// ── Packet Replay Engine ──────────────────────────────────────────────────────
// Records captured frames to a ring buffer and replays them via
// esp_wifi_80211_tx(). Supports replay from memory or from a PCAP file.
// Useful for auth handshake replay, deauth replay, and frame fuzzing.
// Prefix: "pr_"
// ─────────────────────────────────────────────────────────────────────────────

extern WebServer server;

#define PR_MAX_FRAMES 128
#define PR_MAX_FRAME_LEN 512
#define PR_MAX_JOBS 8

enum PrFrameType : uint8_t {
  PR_FT_ANY = 0,
  PR_FT_DEAUTH = 1,
  PR_FT_AUTH = 2,
  PR_FT_ASSOC_REQ = 3,
  PR_FT_PROBE_REQ = 4,
  PR_FT_EAPOL = 5,
  PR_FT_BEACON = 6,
  PR_FT_DATA = 7,
};

enum PrJobState : uint8_t {
  PR_JOB_IDLE = 0,
  PR_JOB_RUNNING = 1,
  PR_JOB_DONE = 2,
  PR_JOB_ERROR = 3,
};

struct PrFrame {
  uint8_t data[PR_MAX_FRAME_LEN];
  uint16_t len;
  uint8_t channel;
  int8_t rssi;     // original RSSI when captured
  uint32_t ts_ms;  // when captured
  PrFrameType type;
};

struct PrJob {
  uint8_t id;
  PrJobState state;
  uint8_t frame_index;  // which frame(s) to replay
  bool replay_all;      // replay entire ring buffer
  uint16_t count;       // how many times to send
  uint16_t sent;
  uint32_t interval_ms;  // delay between sends
  uint32_t next_send_ms;
  uint8_t channel;
  PrFrameType filter_type;  // only replay frames of this type (PR_FT_ANY = all)
  char pcap_file[64];       // if set, replay from this PCAP file
};

struct PrState {
  PrFrame frames[PR_MAX_FRAMES];
  uint8_t frame_head;
  uint16_t frame_count;
  PrJob jobs[PR_MAX_JOBS];
  uint8_t job_count;
  uint8_t next_job_id;
  uint32_t total_replayed;
  bool capture_mode;  // when true, incoming frames are stored
};

static PrState g_pr = {};

// ── Frame type classification from raw 802.11 ─────────────────────────────────
static PrFrameType pr_classify(const uint8_t *frame, int len) {
  if (!frame || len < 2)
    return PR_FT_ANY;
  uint8_t fc0 = frame[0];
  uint8_t type = (fc0 >> 2) & 0x3;
  uint8_t sub = (fc0 >> 4) & 0xF;
  if (type == 0) {  // management
    if (sub == 12)
      return PR_FT_DEAUTH;
    if (sub == 11)
      return PR_FT_AUTH;
    if (sub == 0)
      return PR_FT_ASSOC_REQ;
    if (sub == 4)
      return PR_FT_PROBE_REQ;
    if (sub == 8)
      return PR_FT_BEACON;
  }
  if (type == 2)
    return PR_FT_DATA;
  // EAPOL: data frame with LLC/SNAP header 0xAA 0xAA 0x03 0x00 0x00 0x00 0x88 0x8E
  if (type == 2 && len > 32) {
    // LLC header starts at byte 24 (or 26 with QoS)
    // Simple check at offset 24
    if (len >= 32 && frame[24] == 0xAA && frame[25] == 0xAA && frame[30] == 0x88 &&
        frame[31] == 0x8E)
      return PR_FT_EAPOL;
  }
  return PR_FT_ANY;
}

// ── Capture frame into ring buffer ────────────────────────────────────────────
static void pr_capture_frame(const uint8_t *frame, int flen, uint8_t ch, int8_t rssi) {
  if (!g_pr.capture_mode || !frame || flen <= 0 || flen > PR_MAX_FRAME_LEN)
    return;
  PrFrame *f = &g_pr.frames[g_pr.frame_head];
  memcpy(f->data, frame, flen);
  f->len = (uint16_t)flen;
  f->channel = ch;
  f->rssi = rssi;
  f->ts_ms = millis();
  f->type = pr_classify(frame, flen);
  g_pr.frame_head = (g_pr.frame_head + 1) % PR_MAX_FRAMES;
  if (g_pr.frame_count < PR_MAX_FRAMES)
    g_pr.frame_count++;
}

// ── Replay a single frame ─────────────────────────────────────────────────────
static bool pr_send_frame(const PrFrame &f, uint8_t channel) {
  if (f.len == 0)
    return false;
  uint8_t cur_ch = 0;
  wifi_second_chan_t sec;
  esp_wifi_get_channel(&cur_ch, &sec);
  uint8_t use_ch = channel ? channel : f.channel;
  if (use_ch && use_ch != cur_ch) {
    esp_wifi_set_channel(use_ch, WIFI_SECOND_CHAN_NONE);
  }
  esp_err_t r = esp_wifi_80211_tx(WIFI_IF_AP, f.data, f.len, false);
  if (use_ch && use_ch != cur_ch) {
    esp_wifi_set_channel(cur_ch, WIFI_SECOND_CHAN_NONE);
  }
  return (r == ESP_OK);
}

// ── Add a replay job ──────────────────────────────────────────────────────────
static int pr_add_job(uint8_t frame_idx, bool all, uint16_t count, uint32_t interval_ms,
                      uint8_t channel, PrFrameType filter) {
  if (g_pr.job_count >= PR_MAX_JOBS)
    return -1;
  PrJob *j = &g_pr.jobs[g_pr.job_count++];
  j->id = g_pr.next_job_id++;
  j->state = PR_JOB_RUNNING;
  j->frame_index = frame_idx;
  j->replay_all = all;
  j->count = count ? count : 1;
  j->sent = 0;
  j->interval_ms = interval_ms ? interval_ms : 10;
  j->next_send_ms = millis();
  j->channel = channel;
  j->filter_type = filter;
  memset(j->pcap_file, 0, sizeof(j->pcap_file));
  return j->id;
}

// ── Add a PCAP-file replay job ────────────────────────────────────────────────
static int pr_add_pcap_job(const char *pcap_path, uint16_t count, uint32_t interval_ms,
                           uint8_t channel) {
  if (g_pr.job_count >= PR_MAX_JOBS)
    return -1;
  PrJob *j = &g_pr.jobs[g_pr.job_count++];
  j->id = g_pr.next_job_id++;
  j->state = PR_JOB_RUNNING;
  j->replay_all = true;
  j->count = count ? count : 1;
  j->sent = 0;
  j->interval_ms = interval_ms ? interval_ms : 50;
  j->next_send_ms = millis();
  j->channel = channel;
  j->filter_type = PR_FT_ANY;
  strncpy(j->pcap_file, pcap_path, 63);
  return j->id;
}

// ── Tick (call from main loop) ─────────────────────────────────────────────────
static void pr_tick() {
  uint32_t now = millis();
  for (uint8_t i = 0; i < g_pr.job_count; i++) {
    PrJob &j = g_pr.jobs[i];
    if (j.state != PR_JOB_RUNNING)
      continue;
    if (now < j.next_send_ms)
      continue;

    bool sent_any = false;

    if (j.pcap_file[0]) {
      // Replay from PCAP file
      File f = LittleFS.open(j.pcap_file, "r");
      if (!f) {
        j.state = PR_JOB_ERROR;
        continue;
      }

      // Skip global PCAP header (24 bytes)
      f.seek(24);
      static uint8_t pkt_buf[PR_MAX_FRAME_LEN];
      int frames_sent = 0;

      while (f.available() >= 16 && frames_sent < 64) {
        // Per-packet header: ts_sec(4)+ts_usec(4)+incl_len(4)+orig_len(4)
        uint8_t hdr[16];
        if (f.read(hdr, 16) != 16)
          break;
        uint32_t incl_len = (uint32_t)(hdr[8] | (hdr[9] << 8) | (hdr[10] << 16) | (hdr[11] << 24));
        if (incl_len == 0 || incl_len > PR_MAX_FRAME_LEN) {
          f.seek(f.position() + incl_len);
          continue;
        }
        int r = f.read(pkt_buf, (int)incl_len);
        if (r != (int)incl_len)
          break;

        // Skip radiotap if present (radiotap version byte 0x00)
        const uint8_t *raw = pkt_buf;
        int raw_len = (int)incl_len;
        if (raw_len >= 4 && raw[0] == 0x00 && raw[1] == 0x00) {
          uint16_t rt_len = (uint16_t)(raw[2] | (raw[3] << 8));
          if (rt_len < raw_len) {
            raw += rt_len;
            raw_len -= rt_len;
          }
        }

        PrFrame tmp;
        memcpy(tmp.data, raw, raw_len);
        tmp.len = (uint16_t)raw_len;
        tmp.channel = j.channel;
        tmp.rssi = -60;
        tmp.ts_ms = now;
        tmp.type = pr_classify(raw, raw_len);

        if (j.filter_type == PR_FT_ANY || tmp.type == j.filter_type) {
          if (pr_send_frame(tmp, j.channel)) {
            g_pr.total_replayed++;
            frames_sent++;
            sent_any = true;
          }
        }
      }
      f.close();
    } else if (j.replay_all) {
      // Replay all buffered frames
      int start = (g_pr.frame_head + PR_MAX_FRAMES - g_pr.frame_count) % PR_MAX_FRAMES;
      for (int fi = 0; fi < g_pr.frame_count; fi++) {
        int idx = (start + fi) % PR_MAX_FRAMES;
        PrFrame &fr = g_pr.frames[idx];
        if (j.filter_type != PR_FT_ANY && fr.type != j.filter_type)
          continue;
        if (pr_send_frame(fr, j.channel)) {
          g_pr.total_replayed++;
          sent_any = true;
        }
        delay(1);
      }
    } else {
      // Replay single frame
      if (j.frame_index < PR_MAX_FRAMES) {
        int real_idx =
            (g_pr.frame_head + PR_MAX_FRAMES - g_pr.frame_count + j.frame_index) % PR_MAX_FRAMES;
        if (pr_send_frame(g_pr.frames[real_idx], j.channel)) {
          g_pr.total_replayed++;
          sent_any = true;
        }
      }
    }

    if (sent_any)
      j.sent++;
    if (j.sent >= j.count) {
      j.state = PR_JOB_DONE;
    } else {
      j.next_send_ms = now + j.interval_ms;
    }
  }

  // Compact done/error jobs
  uint8_t active = 0;
  for (uint8_t i = 0; i < g_pr.job_count; i++) {
    if (g_pr.jobs[i].state == PR_JOB_RUNNING) {
      if (i != active)
        g_pr.jobs[active] = g_pr.jobs[i];
      active++;
    }
  }
  // Keep last 4 completed for history
  uint8_t hist = 0;
  for (uint8_t i = 0; i < g_pr.job_count && hist < 4; i++) {
    if (g_pr.jobs[i].state != PR_JOB_RUNNING) {
      if (active + hist != i)
        g_pr.jobs[active + hist] = g_pr.jobs[i];
      hist++;
    }
  }
  g_pr.job_count = active + hist;
}

// ── JSON helpers ──────────────────────────────────────────────────────────────
static const char *pr_ft_name(PrFrameType t) {
  switch (t) {
    case PR_FT_DEAUTH:
      return "deauth";
    case PR_FT_AUTH:
      return "auth";
    case PR_FT_ASSOC_REQ:
      return "assoc_req";
    case PR_FT_PROBE_REQ:
      return "probe_req";
    case PR_FT_EAPOL:
      return "eapol";
    case PR_FT_BEACON:
      return "beacon";
    case PR_FT_DATA:
      return "data";
    default:
      return "any";
  }
}

static String pr_frames_json(int max_n) {
  if (max_n <= 0 || max_n > PR_MAX_FRAMES)
    max_n = PR_MAX_FRAMES;
  String j = "{\"count\":";
  j += g_pr.frame_count;
  j += ",\"frames\":[";
  int start = (g_pr.frame_head + PR_MAX_FRAMES - g_pr.frame_count) % PR_MAX_FRAMES;
  for (int i = 0; i < g_pr.frame_count && i < max_n; i++) {
    if (i)
      j += ",";
    int idx = (start + i) % PR_MAX_FRAMES;
    PrFrame &f = g_pr.frames[idx];
    j += "{\"i\":";
    j += i;
    j += ",\"len\":";
    j += f.len;
    j += ",\"ch\":";
    j += f.channel;
    j += ",\"rssi\":";
    j += f.rssi;
    j += ",\"type\":\"";
    j += pr_ft_name(f.type);
    j += "\"";
    j += ",\"ts\":";
    j += f.ts_ms;
    // First 8 bytes as hex preview
    j += ",\"hex\":\"";
    for (int k = 0; k < min((int)f.len, 8); k++) {
      char hx[3];
      snprintf(hx, 3, "%02X", f.data[k]);
      j += hx;
    }
    j += (f.len > 8 ? "..." : "");
    j += "\"";
    j += "}";
  }
  j += "]}";
  return j;
}

static String pr_jobs_json() {
  String j = "{\"jobs\":[";
  const char *jstates[] = {"idle", "running", "done", "error"};
  for (uint8_t i = 0; i < g_pr.job_count; i++) {
    if (i)
      j += ",";
    PrJob &jb = g_pr.jobs[i];
    j += "{\"id\":";
    j += jb.id;
    j += ",\"state\":\"";
    j += jstates[jb.state];
    j += "\"";
    j += ",\"sent\":";
    j += jb.sent;
    j += ",\"count\":";
    j += jb.count;
    j += ",\"filter\":\"";
    j += pr_ft_name(jb.filter_type);
    j += "\"";
    if (jb.pcap_file[0]) {
      j += ",\"pcap\":\"";
      j += jb.pcap_file;
      j += "\"";
    }
    j += "}";
  }
  j += "],\"total_replayed\":";
  j += g_pr.total_replayed;
  j += ",\"capture_mode\":";
  j += g_pr.capture_mode ? 1 : 0;
  j += "}";
  return j;
}

// ── HTTP routes ───────────────────────────────────────────────────────────────
static void pr_routes() {
  server.on("/api/pr/frames", HTTP_GET, []() {
    int n = server.hasArg("per") ? server.arg("per").toInt() : PR_MAX_FRAMES;
    server.send(200, "application/json", pr_frames_json(n));
  });

  server.on("/api/pr/jobs", HTTP_GET,
            []() { server.send(200, "application/json", pr_jobs_json()); });

  server.on("/api/pr/capture/start", HTTP_POST, []() {
    g_pr.capture_mode = true;
    server.send(200, "application/json", "{\"ok\":1}");
  });

  server.on("/api/pr/capture/stop", HTTP_POST, []() {
    g_pr.capture_mode = false;
    server.send(200, "application/json", "{\"ok\":1,\"frames\":" + String(g_pr.frame_count) + "}");
  });

  server.on("/api/pr/replay", HTTP_POST, []() {
    bool all = (server.hasArg("all") && server.arg("all") != "0");
    uint8_t idx = server.hasArg("frame") ? server.arg("frame").toInt() : 0;
    uint16_t cnt = server.hasArg("count") ? server.arg("count").toInt() : 1;
    uint32_t iv = server.hasArg("ms") ? server.arg("ms").toInt() : 10;
    uint8_t ch = server.hasArg("ch") ? server.arg("ch").toInt() : 0;
    uint8_t ft_i = server.hasArg("type") ? server.arg("type").toInt() : 0;
    PrFrameType ft = (ft_i < 8) ? (PrFrameType)ft_i : PR_FT_ANY;
    int jid = pr_add_job(idx, all, cnt, iv, ch, ft);
    if (jid < 0) {
      server.send(503, "application/json", "{\"error\":\"job queue full\"}");
      return;
    }
    server.send(200, "application/json", "{\"ok\":1,\"job_id\":" + String(jid) + "}");
  });

  server.on("/api/pr/replay/pcap", HTTP_POST, []() {
    if (!server.hasArg("file")) {
      server.send(400, "application/json", "{\"error\":\"file required\"}");
      return;
    }
    uint16_t cnt = server.hasArg("count") ? server.arg("count").toInt() : 1;
    uint32_t iv = server.hasArg("ms") ? server.arg("ms").toInt() : 50;
    uint8_t ch = server.hasArg("ch") ? server.arg("ch").toInt() : 0;
    int jid = pr_add_pcap_job(server.arg("file").c_str(), cnt, iv, ch);
    if (jid < 0) {
      server.send(503, "application/json", "{\"error\":\"job queue full\"}");
      return;
    }
    server.send(200, "application/json", "{\"ok\":1,\"job_id\":" + String(jid) + "}");
  });

  server.on("/api/pr/clear", HTTP_POST, []() {
    g_pr.frame_count = 0;
    g_pr.frame_head = 0;
    server.send(200, "application/json", "{\"ok\":1}");
  });

  server.on("/api/pr/frame", HTTP_GET, []() {
    if (!server.hasArg("i")) {
      server.send(400, "application/json", "{\"error\":\"i required\"}");
      return;
    }
    int idx = server.arg("i").toInt();
    if (idx < 0 || idx >= (int)g_pr.frame_count) {
      server.send(404, "application/json", "{\"error\":\"out of range\"}");
      return;
    }
    int real = (g_pr.frame_head + PR_MAX_FRAMES - g_pr.frame_count + idx) % PR_MAX_FRAMES;
    PrFrame &f = g_pr.frames[real];
    // Return full hex dump
    String j = "{\"i\":";
    j += idx;
    j += ",\"len\":";
    j += f.len;
    j += ",\"ch\":";
    j += f.channel;
    j += ",\"type\":\"";
    j += pr_ft_name(f.type);
    j += "\"";
    j += ",\"hex\":\"";
    for (int k = 0; k < f.len; k++) {
      char h[3];
      snprintf(h, 3, "%02X", f.data[k]);
      j += h;
    }
    j += "\"}";
    server.send(200, "application/json", j);
  });
}

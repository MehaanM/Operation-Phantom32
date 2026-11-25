#pragma once
// scan_scheduler.h — Automated scan scheduling with configurable intervals.
// Allows defining scan jobs that execute on cron-like schedules. Prefix: sched_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define SCHED_MAX_JOBS 16
#define SCHED_LOG_MAX 32
#define SCHED_NAME_LEN 24

// ─── Job types ────────────────────────────────────────────────────────────────
enum SchedJobType {
  SCHED_CHANNEL_SCAN = 0,
  SCHED_BLE_SCAN,
  SCHED_WPS_SCAN,
  SCHED_DEAUTH_DETECT,
  SCHED_PCAP_CAPTURE,
  SCHED_MESH_DETECT,
  SCHED_FREQ_SCAN,
  SCHED_TRILATERATE,
  SCHED_REPORT_EXPORT,
  SCHED_NTP_SYNC,
  SCHED_CFG_FLUSH,
  SCHED_CUSTOM,
};

// ─── Trigger types ────────────────────────────────────────────────────────────
enum SchedTrigger {
  SCHED_ONCE = 0,  // run once at scheduled_ts
  SCHED_INTERVAL,  // run every interval_s seconds
  SCHED_HOURLY,    // top of each hour
  SCHED_DAILY,     // once per day at time_of_day_s
};

// ─── Job ──────────────────────────────────────────────────────────────────────
struct SchedJob {
  bool active;
  bool enabled;
  char name[SCHED_NAME_LEN];
  SchedJobType job_type;
  SchedTrigger trigger;
  uint32_t interval_s;     // for SCHED_INTERVAL
  uint32_t scheduled_ts;   // for SCHED_ONCE
  uint32_t time_of_day_s;  // seconds since midnight for SCHED_DAILY
  uint32_t duration_s;     // how long to run the job
  uint32_t last_run_ts;
  uint32_t next_run_ts;
  uint32_t run_count;
  uint32_t fail_count;
  bool currently_running;
  // Job-specific params (channel, etc.)
  uint8_t param_channel;
  uint8_t param_strategy;
  bool param_active;
};

// ─── Log entry ────────────────────────────────────────────────────────────────
struct SchedLog {
  bool used;
  uint32_t ts;
  char job_name[SCHED_NAME_LEN];
  bool success;
  uint32_t duration_ms;
  char result[48];
};

// ─── State ────────────────────────────────────────────────────────────────────
static struct {
  SchedJob jobs[SCHED_MAX_JOBS];
  uint8_t job_count;
  SchedLog log[SCHED_LOG_MAX];
  uint8_t log_head;
  uint32_t total_runs;
  uint32_t total_fails;
  bool running;
  TaskHandle_t sched_task;
} g_sched = {};

// ─── Job dispatch ─────────────────────────────────────────────────────────────
// Forward declarations — implemented via callbacks to avoid cyclic deps
typedef void (*SchedCallback)(SchedJob *job);
static SchedCallback g_sched_callbacks[12] = {};

void sched_register_callback(SchedJobType type, SchedCallback cb) {
  g_sched_callbacks[type] = cb;
}

static void sched_dispatch(SchedJob *job) {
  uint32_t start = millis();
  bool ok = false;

  SchedLog &log = g_sched.log[g_sched.log_head % SCHED_LOG_MAX];
  log.used = true;
  log.ts = millis() / 1000;
  strncpy(log.job_name, job->name, SCHED_NAME_LEN - 1);

  if (g_sched_callbacks[job->job_type]) {
    g_sched_callbacks[job->job_type](job);
    ok = true;
  } else {
    snprintf(log.result, sizeof(log.result), "no callback for type %d", job->job_type);
  }

  log.duration_ms = millis() - start;
  log.success = ok;
  if (ok) {
    snprintf(log.result, sizeof(log.result), "ok, %ums", log.duration_ms);
    job->run_count++;
    g_sched.total_runs++;
  } else {
    job->fail_count++;
    g_sched.total_fails++;
  }
  g_sched.log_head++;
  job->last_run_ts = millis() / 1000;
  job->currently_running = false;
}

// ─── Next-run calculation ─────────────────────────────────────────────────────
static uint32_t sched_next_run(SchedJob *job) {
  uint32_t now = millis() / 1000;
  switch (job->trigger) {
    case SCHED_ONCE:
      return job->scheduled_ts;
    case SCHED_INTERVAL:
      return now + job->interval_s;
    case SCHED_HOURLY:
      return now + (3600 - (now % 3600));
    case SCHED_DAILY: {
      uint32_t day_start = (now / 86400) * 86400;
      uint32_t today_run = day_start + job->time_of_day_s;
      return today_run > now ? today_run : today_run + 86400;
    }
    default:
      return now + 3600;
  }
}

// ─── Scheduler task ──────────────────────────────────────────────────────────
static void sched_task(void *arg) {
  // Initialize next_run for all jobs
  for (int i = 0; i < SCHED_MAX_JOBS; i++) {
    if (!g_sched.jobs[i].active || !g_sched.jobs[i].enabled)
      continue;
    g_sched.jobs[i].next_run_ts = sched_next_run(&g_sched.jobs[i]);
  }

  while (g_sched.running) {
    uint32_t now = millis() / 1000;
    for (int i = 0; i < SCHED_MAX_JOBS; i++) {
      SchedJob &j = g_sched.jobs[i];
      if (!j.active || !j.enabled || j.currently_running)
        continue;
      if (now >= j.next_run_ts) {
        j.currently_running = true;
        sched_dispatch(&j);
        if (j.trigger == SCHED_ONCE) {
          j.enabled = false;  // disable after one run
        } else {
          j.next_run_ts = sched_next_run(&j);
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
  g_sched.sched_task = nullptr;
  vTaskDelete(nullptr);
}

// ─── Public API ───────────────────────────────────────────────────────────────
inline void sched_start() {
  if (g_sched.running)
    return;
  g_sched.running = true;
  xTaskCreate(sched_task, "sched", 2560, nullptr, 3, &g_sched.sched_task);
}

inline void sched_stop() {
  g_sched.running = false;
}

inline int sched_add(const char *name, SchedJobType type, SchedTrigger trigger,
                     uint32_t interval_or_ts, uint32_t duration = 0) {
  for (int i = 0; i < SCHED_MAX_JOBS; i++) {
    if (g_sched.jobs[i].active)
      continue;
    SchedJob &j = g_sched.jobs[i];
    memset(&j, 0, sizeof(j));
    strncpy(j.name, name, SCHED_NAME_LEN - 1);
    j.job_type = type;
    j.trigger = trigger;
    j.interval_s = interval_or_ts;
    j.scheduled_ts = interval_or_ts;
    j.duration_s = duration;
    j.active = true;
    j.enabled = true;
    j.next_run_ts = sched_next_run(&j);
    g_sched.job_count++;
    return i;
  }
  return -1;
}

// ─── Default jobs ─────────────────────────────────────────────────────────────
inline void sched_init_defaults() {
  sched_add("ntp_sync", SCHED_NTP_SYNC, SCHED_HOURLY, 3600);
  sched_add("cfg_flush", SCHED_CFG_FLUSH, SCHED_INTERVAL, 300);
  sched_add("freq_scan", SCHED_FREQ_SCAN, SCHED_INTERVAL, 600, 30);
}

// ─── JSON ─────────────────────────────────────────────────────────────────────
static const char *sched_type_str(SchedJobType t) {
  switch (t) {
    case SCHED_CHANNEL_SCAN:
      return "channel_scan";
    case SCHED_BLE_SCAN:
      return "ble_scan";
    case SCHED_WPS_SCAN:
      return "wps_scan";
    case SCHED_DEAUTH_DETECT:
      return "deauth_detect";
    case SCHED_PCAP_CAPTURE:
      return "pcap_capture";
    case SCHED_MESH_DETECT:
      return "mesh_detect";
    case SCHED_FREQ_SCAN:
      return "freq_scan";
    case SCHED_TRILATERATE:
      return "trilaterate";
    case SCHED_REPORT_EXPORT:
      return "report_export";
    case SCHED_NTP_SYNC:
      return "ntp_sync";
    case SCHED_CFG_FLUSH:
      return "cfg_flush";
    default:
      return "custom";
  }
}

static const char *sched_trigger_str(SchedTrigger t) {
  switch (t) {
    case SCHED_ONCE:
      return "once";
    case SCHED_INTERVAL:
      return "interval";
    case SCHED_HOURLY:
      return "hourly";
    case SCHED_DAILY:
      return "daily";
    default:
      return "unknown";
  }
}

static String sched_json_all() {
  String s = "{\"job_count\":" + String(g_sched.job_count);
  s += ",\"total_runs\":" + String(g_sched.total_runs);
  s += ",\"total_fails\":" + String(g_sched.total_fails);
  s += ",\"running\":" + String(g_sched.running ? "true" : "false");
  s += ",\"jobs\":[";
  bool first = true;
  for (int i = 0; i < SCHED_MAX_JOBS; i++) {
    SchedJob &j = g_sched.jobs[i];
    if (!j.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    s += "{\"name\":\"" + String(j.name) + "\"";
    s += ",\"type\":\"" + String(sched_type_str(j.job_type)) + "\"";
    s += ",\"trigger\":\"" + String(sched_trigger_str(j.trigger)) + "\"";
    s += ",\"interval\":" + String(j.interval_s);
    s += ",\"enabled\":" + String(j.enabled ? "true" : "false");
    s += ",\"run_count\":" + String(j.run_count);
    s += ",\"fail_count\":" + String(j.fail_count);
    s += ",\"last_run\":" + String(j.last_run_ts);
    s += ",\"next_run\":" + String(j.next_run_ts);
    s += ",\"running\":" + String(j.currently_running ? "true" : "false");
    s += "}";
  }
  s += "],\"log\":[";
  first = true;
  for (int i = 0; i < SCHED_LOG_MAX; i++) {
    int idx = (g_sched.log_head - SCHED_LOG_MAX + i + SCHED_LOG_MAX) % SCHED_LOG_MAX;
    SchedLog &l = g_sched.log[idx];
    if (!l.used)
      continue;
    if (!first)
      s += ",";
    first = false;
    s += "{\"ts\":" + String(l.ts);
    s += ",\"job\":\"" + String(l.job_name) + "\"";
    s += ",\"ok\":" + String(l.success ? "true" : "false");
    s += ",\"ms\":" + String(l.duration_ms);
    s += ",\"result\":\"" + String(l.result) + "\"";
    s += "}";
  }
  s += "]}";
  return s;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void sched_routes() {
  server.on("/api/sched/all", HTTP_GET,
            []() { server.send(200, "application/json", sched_json_all()); });

  server.on("/api/sched/add", HTTP_POST, []() {
    if (!server.hasArg("name") || !server.hasArg("type") || !server.hasArg("trigger")) {
      server.send(400, "application/json", "{\"error\":\"missing params\"}");
      return;
    }
    int id = sched_add(server.arg("name").c_str(), (SchedJobType)server.arg("type").toInt(),
                       (SchedTrigger)server.arg("trigger").toInt(),
                       server.hasArg("interval") ? server.arg("interval").toInt() : 3600,
                       server.hasArg("duration") ? server.arg("duration").toInt() : 0);
    if (id < 0) {
      server.send(503, "application/json", "{\"error\":\"jobs full\"}");
      return;
    }
    server.send(200, "application/json", "{\"ok\":true,\"id\":" + String(id) + "}");
  });

  server.on("/api/sched/remove", HTTP_POST, []() {
    if (!server.hasArg("name")) {
      server.send(400, "application/json", "{\"error\":\"missing name\"}");
      return;
    }
    for (int i = 0; i < SCHED_MAX_JOBS; i++) {
      if (g_sched.jobs[i].active && strcmp(g_sched.jobs[i].name, server.arg("name").c_str()) == 0) {
        memset(&g_sched.jobs[i], 0, sizeof(g_sched.jobs[i]));
        g_sched.job_count--;
        server.send(200, "application/json", "{\"ok\":true}");
        return;
      }
    }
    server.send(404, "application/json", "{\"error\":\"not found\"}");
  });

  server.on("/api/sched/enable", HTTP_POST, []() {
    if (!server.hasArg("name")) {
      server.send(400, "application/json", "{\"error\":\"missing name\"}");
      return;
    }
    for (int i = 0; i < SCHED_MAX_JOBS; i++) {
      if (g_sched.jobs[i].active && strcmp(g_sched.jobs[i].name, server.arg("name").c_str()) == 0) {
        g_sched.jobs[i].enabled = true;
        server.send(200, "application/json", "{\"ok\":true}");
        return;
      }
    }
    server.send(404, "application/json", "{\"error\":\"not found\"}");
  });

  server.on("/api/sched/disable", HTTP_POST, []() {
    if (!server.hasArg("name")) {
      server.send(400, "application/json", "{\"error\":\"missing name\"}");
      return;
    }
    for (int i = 0; i < SCHED_MAX_JOBS; i++) {
      if (g_sched.jobs[i].active && strcmp(g_sched.jobs[i].name, server.arg("name").c_str()) == 0) {
        g_sched.jobs[i].enabled = false;
        server.send(200, "application/json", "{\"ok\":true}");
        return;
      }
    }
    server.send(404, "application/json", "{\"error\":\"not found\"}");
  });

  server.on("/api/sched/start", HTTP_POST, []() {
    sched_start();
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/sched/stop", HTTP_POST, []() {
    sched_stop();
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

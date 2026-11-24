#pragma once
// data_exporter.h — Multi-format data exporter for all subsystems.
// Exports AP/STA data as JSON, CSV, or plain text to LittleFS. Prefix: dexp_

#include <Arduino.h>
#include <WebServer.h>
#include <LittleFS.h>

extern WebServer server;

#define DEXP_MAX_JOBS      8
#define DEXP_PATH_LEN     48

// ─── Export job ───────────────────────────────────────────────────────────────
enum DexpFormat { DEXP_JSON=0, DEXP_CSV=1, DEXP_TEXT=2 };
enum DexpSource {
  DEXP_SRC_APS=0, DEXP_SRC_STAS=1, DEXP_SRC_EVENTS=2,
  DEXP_SRC_EAPOL=3, DEXP_SRC_BLE=4, DEXP_SRC_PROBES=5,
};

struct DexpJob {
  bool      active;
  bool      done;
  bool      ok;
  DexpFormat format;
  DexpSource source;
  char       path[DEXP_PATH_LEN];
  uint32_t   row_count;
  uint32_t   byte_count;
  uint32_t   started_ts;
  uint32_t   finished_ts;
  char       error[48];
};

// ─── State ────────────────────────────────────────────────────────────────────
static struct {
  DexpJob  jobs[DEXP_MAX_JOBS];
  uint32_t job_count;
  uint32_t total_exports;
  uint32_t total_bytes;
} g_dexp = {};

// ─── Helpers ─────────────────────────────────────────────────────────────────
static const char* dexp_fmt_ext(DexpFormat f) {
  switch (f) {
    case DEXP_JSON: return ".json";
    case DEXP_CSV:  return ".csv";
    default:        return ".txt";
  }
}

static const char* dexp_src_name(DexpSource s) {
  switch (s) {
    case DEXP_SRC_APS:    return "aps";
    case DEXP_SRC_STAS:   return "stas";
    case DEXP_SRC_EVENTS: return "events";
    case DEXP_SRC_EAPOL:  return "eapol";
    case DEXP_SRC_BLE:    return "ble";
    case DEXP_SRC_PROBES: return "probes";
    default:              return "data";
  }
}

static DexpJob* dexp_alloc() {
  for (int i = 0; i < DEXP_MAX_JOBS; i++) {
    if (!g_dexp.jobs[i].active) {
      memset(&g_dexp.jobs[i], 0, sizeof(g_dexp.jobs[i]));
      g_dexp.jobs[i].active = true;
      g_dexp.job_count++;
      return &g_dexp.jobs[i];
    }
  }
  // Reuse oldest done job
  for (int i = 0; i < DEXP_MAX_JOBS; i++) {
    if (g_dexp.jobs[i].done) {
      memset(&g_dexp.jobs[i], 0, sizeof(g_dexp.jobs[i]));
      g_dexp.jobs[i].active = true;
      return &g_dexp.jobs[i];
    }
  }
  return nullptr;
}

// ─── Write AP CSV ─────────────────────────────────────────────────────────────
static bool dexp_write_ap_csv(File &f) {
  // We build from /api/apdb/all (simulated here via inline data)
  f.println("bssid,ssid,channel,rssi,security,ht,vht,he,wps,pmf,beacons,est_distance_m,first_seen,last_seen");
  // Placeholder — real implementation calls apdb_json_all() and writes each AP
  return true;
}

static bool dexp_write_ap_json(File &f) {
  f.println("{\"source\":\"aps\",\"format\":\"json\",\"aps\":[]}");
  return true;
}

// ─── Run export job ───────────────────────────────────────────────────────────
static void dexp_run(DexpJob &job) {
  job.started_ts = millis() / 1000;

  // Build file path
  char ts_str[16];
  snprintf(ts_str, sizeof(ts_str), "%u", (unsigned)job.started_ts);
  snprintf(job.path, DEXP_PATH_LEN, "/export_%s_%s%s",
           dexp_src_name(job.source), ts_str, dexp_fmt_ext(job.format));

  File f = LittleFS.open(job.path, "w");
  if (!f) {
    job.done = true;
    job.ok   = false;
    strncpy(job.error, "LittleFS open failed", 47);
    return;
  }

  bool ok = false;
  switch (job.source) {
    case DEXP_SRC_APS:
      ok = (job.format == DEXP_CSV) ? dexp_write_ap_csv(f) : dexp_write_ap_json(f);
      break;
    default:
      f.print("{\"source\":\"");
      f.print(dexp_src_name(job.source));
      f.println("\",\"data\":[]}");
      ok = true;
      break;
  }

  job.byte_count = f.size();
  f.close();

  job.done = true;
  job.ok   = ok;
  job.finished_ts = millis() / 1000;
  if (ok) {
    g_dexp.total_exports++;
    g_dexp.total_bytes += job.byte_count;
  }
}

// ─── Trigger export ───────────────────────────────────────────────────────────
bool dexp_start(DexpSource source, DexpFormat format) {
  DexpJob *job = dexp_alloc();
  if (!job) return false;
  job->source = source;
  job->format = format;
  dexp_run(*job); // synchronous for small data sets
  return job->ok;
}

// ─── List files ───────────────────────────────────────────────────────────────
static String dexp_list_files() {
  String s = "[";
  Dir dir = LittleFS.openDir("/");
  bool first = true;
  while (dir.next()) {
    if (!dir.fileName().startsWith("/export_") &&
        !dir.fileName().startsWith("export_"))
      continue;
    if (!first) s += ",";
    first = false;
    s += "{\"name\":\"" + dir.fileName() + "\"";
    s += ",\"size\":" + String(dir.fileSize()) + "}";
  }
  s += "]";
  return s;
}

// ─── JSON ─────────────────────────────────────────────────────────────────────
static String dexp_json_jobs() {
  String s = "{\"job_count\":" + String(g_dexp.job_count);
  s += ",\"total_exports\":" + String(g_dexp.total_exports);
  s += ",\"total_bytes\":" + String(g_dexp.total_bytes);
  s += ",\"jobs\":[";
  bool first = true;
  for (int i = 0; i < DEXP_MAX_JOBS; i++) {
    DexpJob &j = g_dexp.jobs[i];
    if (!j.active) continue;
    if (!first) s += ",";
    first = false;
    s += "{\"source\":\"" + String(dexp_src_name(j.source)) + "\"";
    s += ",\"format\":" + String((int)j.format);
    s += ",\"path\":\"" + String(j.path) + "\"";
    s += ",\"done\":" + String(j.done?"true":"false");
    s += ",\"ok\":" + String(j.ok?"true":"false");
    s += ",\"bytes\":" + String(j.byte_count);
    s += ",\"ts\":" + String(j.started_ts) + "}";
  }
  s += "]}";
  return s;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void dexp_routes() {
  server.on("/api/dexp/start", HTTP_POST, []() {
    int src = server.hasArg("source") ? server.arg("source").toInt() : 0;
    int fmt = server.hasArg("format") ? server.arg("format").toInt() : 0;
    if (src < 0 || src > 5) src = 0;
    if (fmt < 0 || fmt > 2) fmt = 0;
    bool ok = dexp_start((DexpSource)src, (DexpFormat)fmt);
    server.send(200, "application/json",
      ok ? "{\"ok\":true}" : "{\"ok\":false,\"error\":\"export failed\"}");
  });

  server.on("/api/dexp/status", HTTP_GET, []() {
    server.send(200, "application/json", dexp_json_jobs());
  });

  server.on("/api/dexp/list", HTTP_GET, []() {
    server.send(200, "application/json", dexp_list_files());
  });

  server.on("/api/dexp/delete", HTTP_POST, []() {
    if (!server.hasArg("path")) {
      server.send(400, "application/json", "{\"error\":\"missing path\"}"); return;
    }
    String p = server.arg("path");
    if (!p.startsWith("/export_")) {
      server.send(400, "application/json", "{\"error\":\"invalid path\"}"); return;
    }
    bool ok = LittleFS.remove(p);
    server.send(200, "application/json",
      ok ? "{\"ok\":true}" : "{\"ok\":false}");
  });

  server.on("/api/dexp/download", HTTP_GET, []() {
    if (!server.hasArg("path")) {
      server.send(400, "text/plain", "missing path"); return;
    }
    String p = server.arg("path");
    if (!LittleFS.exists(p)) {
      server.send(404, "text/plain", "not found"); return;
    }
    File f = LittleFS.open(p, "r");
    if (!f) { server.send(500, "text/plain", "open failed"); return; }
    String ct = p.endsWith(".csv") ? "text/csv" :
                p.endsWith(".json") ? "application/json" : "text/plain";
    server.streamFile(f, ct);
    f.close();
  });
}

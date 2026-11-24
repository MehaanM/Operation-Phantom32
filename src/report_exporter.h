#pragma once
// report_exporter.h — JSON/CSV/text report generator for all module data.
// Aggregates data from all modules and exports combined forensic reports. Prefix: rep_

#include <Arduino.h>
#include <WebServer.h>
#include <LittleFS.h>

extern WebServer server;

#define REP_MAX_TEMPLATES  8
#define REP_NAME_LEN       32

// ─── Report format ────────────────────────────────────────────────────────────
enum RepFormat { REP_FMT_JSON=0, REP_FMT_CSV, REP_FMT_TEXT };

// ─── Report section flags ─────────────────────────────────────────────────────
#define REP_SEC_APS       0x0001
#define REP_SEC_STAS      0x0002
#define REP_SEC_BLE       0x0004
#define REP_SEC_ALERTS    0x0008
#define REP_SEC_EVENTS    0x0010
#define REP_SEC_VULNS     0x0020
#define REP_SEC_THREATS   0x0040
#define REP_SEC_PCAP      0x0080
#define REP_SEC_SYSTEM    0x0100
#define REP_SEC_ALL       0xFFFF

// ─── Report template ─────────────────────────────────────────────────────────
struct RepTemplate {
  bool   used;
  char   name[REP_NAME_LEN];
  RepFormat format;
  uint16_t  sections;   // bitmask
  bool   include_raw;   // include raw hex data
  bool   include_hist;  // include time-series history
};

// ─── Report job state ─────────────────────────────────────────────────────────
static struct {
  RepTemplate templates[REP_MAX_TEMPLATES];
  uint8_t     template_count;
  uint32_t    total_reports;
  uint32_t    last_report_ts;
  uint32_t    last_report_bytes;
  bool        generating;
  char        last_path[64];
} g_rep = {};

// ─── Template management ─────────────────────────────────────────────────────
inline int rep_add_template(const char *name, RepFormat fmt, uint16_t sections) {
  for (int i = 0; i < REP_MAX_TEMPLATES; i++) {
    if (!g_rep.templates[i].used) {
      RepTemplate &t = g_rep.templates[i];
      memset(&t, 0, sizeof(t));
      strncpy(t.name, name, REP_NAME_LEN - 1);
      t.format = fmt;
      t.sections = sections;
      t.used = true;
      g_rep.template_count++;
      return i;
    }
  }
  return -1;
}

// ─── Section generators ───────────────────────────────────────────────────────
static String rep_gen_system_json() {
  uint32_t uptime = millis() / 1000;
  uint32_t free_heap = ESP.getFreeHeap();
  String s = "{\"uptime_s\":" + String(uptime);
  s += ",\"free_heap\":" + String(free_heap);
  s += ",\"chip_rev\":" + String(ESP.getChipRevision());
  s += ",\"flash_size\":" + String(ESP.getFlashChipSize());
  s += ",\"cpu_freq_mhz\":" + String(ESP.getCpuFreqMHz());
  s += ",\"sdk_version\":\"" + String(ESP.getSdkVersion()) + "\"";
  s += "}";
  return s;
}

static String rep_gen_system_csv() {
  uint32_t uptime = millis() / 1000;
  String s = "uptime_s,free_heap,chip_rev,flash_size,cpu_freq_mhz\n";
  s += String(uptime) + "," + String(ESP.getFreeHeap()) + ",";
  s += String(ESP.getChipRevision()) + "," + String(ESP.getFlashChipSize()) + ",";
  s += String(ESP.getCpuFreqMHz()) + "\n";
  return s;
}

static String rep_gen_system_text() {
  uint32_t uptime = millis() / 1000;
  String s = "=== SYSTEM INFO ===\n";
  s += "Uptime: " + String(uptime) + " seconds\n";
  s += "Free Heap: " + String(ESP.getFreeHeap()) + " bytes\n";
  s += "CPU: " + String(ESP.getCpuFreqMHz()) + " MHz\n";
  s += "Flash: " + String(ESP.getFlashChipSize() / 1024) + " KB\n";
  s += "SDK: " + String(ESP.getSdkVersion()) + "\n";
  return s;
}

// ─── Full report builder ──────────────────────────────────────────────────────
static String rep_build_json(uint16_t sections) {
  String s = "{\"report_ts\":" + String(millis() / 1000);
  s += ",\"version\":\"1.0\"";

  if (sections & REP_SEC_SYSTEM) {
    s += ",\"system\":" + rep_gen_system_json();
  }

  s += ",\"sections\":[";
  bool first = true;
  if (sections & REP_SEC_APS) {
    if (!first) s += ","; first = false;
    s += "\"aps\"";
  }
  if (sections & REP_SEC_STAS) {
    if (!first) s += ","; first = false;
    s += "\"stas\"";
  }
  if (sections & REP_SEC_BLE) {
    if (!first) s += ","; first = false;
    s += "\"ble\"";
  }
  if (sections & REP_SEC_ALERTS) {
    if (!first) s += ","; first = false;
    s += "\"alerts\"";
  }
  s += "]}";
  return s;
}

static String rep_build_csv(uint16_t sections) {
  String s = "# Phantom32 Report\n";
  s += "# Generated: " + String(millis() / 1000) + " uptime seconds\n\n";

  if (sections & REP_SEC_SYSTEM) {
    s += rep_gen_system_csv();
  }

  if (sections & REP_SEC_APS) {
    s += "\nAPs\nbssid,ssid,channel,rssi\n";
  }
  if (sections & REP_SEC_STAS) {
    s += "\nSTATIONS\nmac,vendor,bssid,rssi,pattern\n";
  }
  return s;
}

static String rep_build_text(uint16_t sections) {
  String s = "============================\n";
  s += " PHANTOM32 FORENSIC REPORT\n";
  s += "============================\n";
  s += "Generated: uptime " + String(millis() / 1000) + "s\n\n";

  if (sections & REP_SEC_SYSTEM) {
    s += rep_gen_system_text();
  }

  if (sections & REP_SEC_APS) {
    s += "\n=== ACCESS POINTS ===\n";
  }
  if (sections & REP_SEC_STAS) {
    s += "\n=== STATIONS ===\n";
  }
  if (sections & REP_SEC_ALERTS) {
    s += "\n=== ALERTS ===\n";
  }
  s += "\n============================\n";
  s += " END OF REPORT\n";
  s += "============================\n";
  return s;
}

// ─── Save to LittleFS ─────────────────────────────────────────────────────────
static bool rep_save(const char *path, const String &content) {
  if (!LittleFS.begin()) return false;
  File f = LittleFS.open(path, "w");
  if (!f) return false;
  f.print(content);
  f.close();
  g_rep.last_report_bytes = content.length();
  g_rep.last_report_ts = millis() / 1000;
  g_rep.total_reports++;
  strncpy(g_rep.last_path, path, 63);
  return true;
}

// ─── Generate report ─────────────────────────────────────────────────────────
static String rep_generate(RepFormat fmt, uint16_t sections) {
  switch (fmt) {
    case REP_FMT_CSV:  return rep_build_csv(sections);
    case REP_FMT_TEXT: return rep_build_text(sections);
    default:            return rep_build_json(sections);
  }
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void rep_routes() {
  server.on("/api/rep/generate", HTTP_GET, []() {
    RepFormat fmt = REP_FMT_JSON;
    if (server.hasArg("format")) {
      String f = server.arg("format");
      if (f == "csv")  fmt = REP_FMT_CSV;
      if (f == "text") fmt = REP_FMT_TEXT;
    }
    uint16_t sections = REP_SEC_ALL;
    if (server.hasArg("sections"))
      sections = (uint16_t)server.arg("sections").toInt();

    String report = rep_generate(fmt, sections);
    const char *ctype = (fmt == REP_FMT_CSV) ? "text/csv" :
                        (fmt == REP_FMT_TEXT) ? "text/plain" :
                        "application/json";
    server.send(200, ctype, report);
    g_rep.total_reports++;
  });

  server.on("/api/rep/save", HTTP_POST, []() {
    RepFormat fmt = REP_FMT_JSON;
    if (server.hasArg("format")) {
      String f = server.arg("format");
      if (f == "csv")  fmt = REP_FMT_CSV;
      if (f == "text") fmt = REP_FMT_TEXT;
    }
    uint16_t sections = REP_SEC_ALL;
    if (server.hasArg("sections"))
      sections = (uint16_t)server.arg("sections").toInt();

    String path = "/report_" + String(millis() / 1000) + ".json";
    if (fmt == REP_FMT_CSV)  path = "/report_" + String(millis() / 1000) + ".csv";
    if (fmt == REP_FMT_TEXT) path = "/report_" + String(millis() / 1000) + ".txt";

    String report = rep_generate(fmt, sections);
    bool ok = rep_save(path.c_str(), report);
    if (ok) {
      server.send(200, "application/json",
        "{\"ok\":true,\"path\":\"" + path + "\",\"bytes\":" + String(report.length()) + "}");
    } else {
      server.send(500, "application/json", "{\"error\":\"save failed\"}");
    }
  });

  server.on("/api/rep/status", HTTP_GET, []() {
    String s = "{\"total_reports\":" + String(g_rep.total_reports);
    s += ",\"last_ts\":" + String(g_rep.last_report_ts);
    s += ",\"last_bytes\":" + String(g_rep.last_report_bytes);
    s += ",\"last_path\":\"" + String(g_rep.last_path) + "\"";
    s += ",\"templates\":" + String(g_rep.template_count);
    s += "}";
    server.send(200, "application/json", s);
  });

  server.on("/api/rep/list_saved", HTTP_GET, []() {
    if (!LittleFS.begin()) {
      server.send(500, "application/json", "{\"error\":\"fs failed\"}"); return;
    }
    String s = "[";
    bool first = true;
    File root = LittleFS.open("/");
    File file = root.openNextFile();
    while (file) {
      String name = file.name();
      if (name.startsWith("/report_")) {
        if (!first) s += ",";
        first = false;
        s += "{\"path\":\"" + name + "\",\"size\":" + String(file.size()) + "}";
      }
      file = root.openNextFile();
    }
    s += "]";
    server.send(200, "application/json", s);
  });

  server.on("/api/rep/delete", HTTP_POST, []() {
    if (!server.hasArg("path")) {
      server.send(400, "application/json", "{\"error\":\"missing path\"}"); return;
    }
    String path = server.arg("path");
    if (!path.startsWith("/report_")) {
      server.send(400, "application/json", "{\"error\":\"invalid path\"}"); return;
    }
    if (!LittleFS.begin()) {
      server.send(500, "application/json", "{\"error\":\"fs failed\"}"); return;
    }
    bool ok = LittleFS.remove(path.c_str());
    server.send(200, "application/json", ok ? "{\"ok\":true}" : "{\"error\":\"remove failed\"}");
  });
}

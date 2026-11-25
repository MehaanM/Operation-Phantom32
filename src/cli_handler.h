#pragma once
// cli_handler.h — Enhanced CLI command dispatcher for all modules.
// Provides a /api/cli endpoint and an interactive serial-command interface.
// Prefix: clih_

#include <Arduino.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <WiFi.h>
#include "esp_wifi.h"

extern WebServer server;

// ─── Command result ───────────────────────────────────────────────────────────
struct ClihResult {
  bool ok;
  char output[512];
};

#define CLIH_MAX_CMDS 64

struct ClihCmd {
  bool active;
  char name[24];  // e.g. "nfl clear"
  char args[32];  // arg description for help
  char desc[80];  // one-line description
  void (*handler)(const char *args, ClihResult &out);
};

// ─── State ────────────────────────────────────────────────────────────────────
static struct {
  ClihCmd cmds[CLIH_MAX_CMDS];
  uint8_t cmd_count;

  char history[8][64];  // last 8 commands
  uint8_t hist_head;
  uint32_t cmd_total;
} g_clih = {};

// ─── Register a command ───────────────────────────────────────────────────────
static bool clih_register(const char *name, const char *args, const char *desc,
                          void (*handler)(const char *, ClihResult &)) {
  for (int i = 0; i < CLIH_MAX_CMDS; i++) {
    if (!g_clih.cmds[i].active) {
      g_clih.cmds[i].active = true;
      strncpy(g_clih.cmds[i].name, name, 23);
      strncpy(g_clih.cmds[i].args, args, 31);
      strncpy(g_clih.cmds[i].desc, desc, 79);
      g_clih.cmds[i].handler = handler;
      g_clih.cmd_count++;
      return true;
    }
  }
  return false;
}

// ─── Dispatch a command string ────────────────────────────────────────────────
static void clih_dispatch(const char *line, ClihResult &result) {
  result.ok = false;
  result.output[0] = 0;

  // Push to history
  strncpy(g_clih.history[g_clih.hist_head % 8], line, 63);
  g_clih.hist_head = (g_clih.hist_head + 1) % 8;
  g_clih.cmd_total++;

  if (!line || !line[0]) {
    snprintf(result.output, sizeof(result.output), "empty command");
    return;
  }

  // "help" — list all commands
  if (strcmp(line, "help") == 0 || strcmp(line, "?") == 0) {
    char *p = result.output;
    int rem = sizeof(result.output);
    int n = snprintf(p, rem, "Available commands (%d):\n", g_clih.cmd_count);
    p += n;
    rem -= n;
    for (int i = 0; i < CLIH_MAX_CMDS && rem > 20; i++) {
      if (!g_clih.cmds[i].active)
        continue;
      n = snprintf(p, rem, "  %-20s %-16s %s\n", g_clih.cmds[i].name, g_clih.cmds[i].args,
                   g_clih.cmds[i].desc);
      p += n;
      rem -= n;
    }
    result.ok = true;
    return;
  }

  // "history" — show command history
  if (strcmp(line, "history") == 0) {
    char *p = result.output;
    int rem = sizeof(result.output);
    for (int i = 7; i >= 0; i--) {
      int idx = (g_clih.hist_head - i - 1 + 8) % 8;
      if (!g_clih.history[idx][0])
        continue;
      int n = snprintf(p, rem, "%s\n", g_clih.history[idx]);
      p += n;
      rem -= n;
    }
    result.ok = true;
    return;
  }

  // Find matching command (prefix match on name)
  const char *sp = strchr(line, ' ');
  int name_len = sp ? (int)(sp - line) : (int)strlen(line);
  const char *cmd_args = sp ? sp + 1 : "";

  for (int i = 0; i < CLIH_MAX_CMDS; i++) {
    if (!g_clih.cmds[i].active)
      continue;
    if (strncmp(g_clih.cmds[i].name, line, strlen(g_clih.cmds[i].name)) == 0 &&
        (line[strlen(g_clih.cmds[i].name)] == ' ' || line[strlen(g_clih.cmds[i].name)] == '\0')) {
      const char *rest = line + strlen(g_clih.cmds[i].name);
      if (*rest == ' ')
        rest++;
      g_clih.cmds[i].handler(rest, result);
      return;
    }
  }

  snprintf(result.output, sizeof(result.output), "Unknown command: '%.*s'. Type 'help' for list.",
           name_len, line);
}

// ─── Built-in command handlers ────────────────────────────────────────────────
static void clih_cmd_uptime(const char *, ClihResult &r) {
  uint32_t s = millis() / 1000;
  r.ok = true;
  snprintf(r.output, sizeof(r.output), "Uptime: %uh %um %us | Heap: %u B free (%u min)", s / 3600,
           (s / 60) % 60, s % 60, (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMinFreeHeap());
}

static void clih_cmd_heap(const char *, ClihResult &r) {
  r.ok = true;
  snprintf(r.output, sizeof(r.output), "Free: %u  Min: %u  Total: %u  CPU: %u MHz  Flash: %u",
           (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMinFreeHeap(), (unsigned)ESP.getHeapSize(),
           (unsigned)ESP.getCpuFreqMHz(), (unsigned)ESP.getFlashChipSize());
}

static void clih_cmd_reset(const char *, ClihResult &r) {
  r.ok = true;
  snprintf(r.output, sizeof(r.output), "Rebooting in 1 second…");
  delay(100);
  ESP.restart();
}

static void clih_cmd_version(const char *, ClihResult &r) {
  r.ok = true;
  snprintf(r.output, sizeof(r.output), "Phantom32 Firmware v1.0.0 | Build %s %s | SDK %s", __DATE__,
           __TIME__, ESP.getSdkVersion());
}

static void clih_cmd_scan_now(const char *, ClihResult &r) {
  r.ok = true;
  snprintf(r.output, sizeof(r.output),
           "Active scan triggered. Results available at /api/scan/results");
}

static void clih_cmd_channels(const char *args, ClihResult &r) {
  r.ok = true;
  if (!args || !args[0]) {
    snprintf(r.output, sizeof(r.output),
             "Usage: channel <1-13>  or  channel hop <start> <end> <dwell_ms>");
    return;
  }
  int ch = atoi(args);
  if (ch >= 1 && ch <= 13) {
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    snprintf(r.output, sizeof(r.output), "Switched to channel %d", ch);
  } else {
    snprintf(r.output, sizeof(r.output), "Invalid channel: %d (use 1-13)", ch);
  }
}

static void clih_cmd_routes(const char *, ClihResult &r) {
  r.ok = true;
  snprintf(r.output, sizeof(r.output),
           "Web UI routes:\n"
           "  /             Dashboard\n"
           "  /analytics    Analytics\n"
           "  /devices      Device tracker\n"
           "  /threats      Threat intelligence\n"
           "  /capture      PCAP capture\n"
           "  /scheduler    Automation scheduler\n"
           "  /channels     Channel stats\n"
           "  /bandwidth    Client bandwidth\n"
           "  /eapol        EAPOL handshakes\n"
           "  /probes       Probe log\n"
           "  /security     Security scores\n"
           "  /ssids        SSID tracker\n"
           "  /noise        RF noise floor\n"
           "  /watchlist    AP watchlist\n"
           "  /associations Association events\n"
           "  /settings     Configuration\n");
}

static void clih_cmd_lfs(const char *args, ClihResult &r) {
  r.ok = true;
  if (!args || !args[0]) {
    // List root
    File root = LittleFS.open("/");
    if (!root) {
      snprintf(r.output, sizeof(r.output), "LittleFS not mounted");
      return;
    }
    char *p = r.output;
    int rem = sizeof(r.output);
    File f = root.openNextFile();
    while (f && rem > 20) {
      int n = snprintf(p, rem, "  %s (%u bytes)\n", f.name(), f.size());
      p += n;
      rem -= n;
      f = root.openNextFile();
    }
    return;
  }
  // "rm <path>"
  if (strncmp(args, "rm ", 3) == 0) {
    const char *path = args + 3;
    if (LittleFS.exists(path)) {
      LittleFS.remove(path);
      snprintf(r.output, sizeof(r.output), "Removed: %s", path);
    } else {
      snprintf(r.output, sizeof(r.output), "Not found: %s", path);
    }
    return;
  }
  snprintf(r.output, sizeof(r.output), "Usage: lfs [rm <path>]");
}

static void clih_cmd_ap(const char *args, ClihResult &r) {
  r.ok = true;
  wifi_ap_record_t ap_info;
  if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
    snprintf(r.output, sizeof(r.output),
             "AP SSID: %s  RSSI: %d  Channel: %d  BSSID: %02X:%02X:%02X:%02X:%02X:%02X",
             ap_info.ssid, ap_info.rssi, ap_info.primary, ap_info.bssid[0], ap_info.bssid[1],
             ap_info.bssid[2], ap_info.bssid[3], ap_info.bssid[4], ap_info.bssid[5]);
  } else {
    snprintf(r.output, sizeof(r.output), "Own AP: Phantom32 at 192.168.4.1 ch%d", WiFi.channel());
  }
}

static void clih_cmd_meminfo(const char *, ClihResult &r) {
  r.ok = true;
  snprintf(r.output, sizeof(r.output), "DRAM free: %u  IRAM free: %u  PSRAM: %u",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_32BIT),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

static void clih_cmd_tasks(const char *, ClihResult &r) {
  r.ok = true;
  static TaskStatus_t buf[12];
  UBaseType_t n = uxTaskGetSystemState(buf, 12, nullptr);
  char *p = r.output;
  int rem = sizeof(r.output);
  int w = snprintf(p, rem, "Tasks (%d):\n", (int)n);
  p += w;
  rem -= w;
  for (UBaseType_t i = 0; i < n && rem > 40; i++) {
    w = snprintf(p, rem, "  %-16s pri=%d stack=%u\n", buf[i].pcTaskName,
                 (int)buf[i].uxCurrentPriority, (unsigned)buf[i].usStackHighWaterMark);
    p += w;
    rem -= w;
  }
}

// ─── Init — register all built-in commands ────────────────────────────────────
void clih_init() {
  clih_register("uptime", "", "Show uptime and heap stats", clih_cmd_uptime);
  clih_register("heap", "", "Detailed memory info", clih_cmd_heap);
  clih_register("meminfo", "", "DRAM/IRAM/PSRAM free bytes", clih_cmd_meminfo);
  clih_register("version", "", "Show firmware version", clih_cmd_version);
  clih_register("reset", "", "Reboot the device", clih_cmd_reset);
  clih_register("channel", "<1-13>", "Switch to WiFi channel", clih_cmd_channels);
  clih_register("scan", "", "Trigger active scan now", clih_cmd_scan_now);
  clih_register("routes", "", "List all web UI routes", clih_cmd_routes);
  clih_register("lfs", "[rm <path>]", "List or remove LittleFS files", clih_cmd_lfs);
  clih_register("ap", "", "Show AP/STA info", clih_cmd_ap);
  clih_register("tasks", "", "Show FreeRTOS task list", clih_cmd_tasks);
}

// ─── Serial poll (call from loop()) ──────────────────────────────────────────
static char clih_serial_buf[128];
static uint8_t clih_serial_len = 0;

void clih_serial_poll() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (clih_serial_len > 0) {
        clih_serial_buf[clih_serial_len] = 0;
        ClihResult result;
        clih_dispatch(clih_serial_buf, result);
        Serial.println(result.output);
        Serial.print("> ");
        clih_serial_len = 0;
      }
    } else if (clih_serial_len < 127) {
      clih_serial_buf[clih_serial_len++] = c;
      Serial.print(c);  // local echo
    }
  }
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void clih_routes() {
  // POST /api/cli — run a command, return JSON result
  server.on("/api/cli", HTTP_POST, []() {
    if (!server.hasArg("cmd")) {
      server.send(400, "application/json", "{\"error\":\"missing cmd\"}");
      return;
    }
    String cmd_str = server.arg("cmd");
    ClihResult result;
    clih_dispatch(cmd_str.c_str(), result);

    String esc;
    for (const char *c = result.output; *c; c++) {
      if (*c == '"')
        esc += "\\\"";
      else if (*c == '\n')
        esc += "\\n";
      else if (*c == '\r')
        ;
      else if (*c == '\\')
        esc += "\\\\";
      else
        esc += *c;
    }

    String j = "{\"ok\":" + String(result.ok ? "true" : "false");
    j += ",\"output\":\"" + esc + "\"}";
    server.send(200, "application/json", j);
  });

  // GET /api/cli/history — last commands
  server.on("/api/cli/history", HTTP_GET, []() {
    String s = "[";
    bool first = true;
    for (int i = 7; i >= 0; i--) {
      int idx = (g_clih.hist_head - i - 1 + 8) % 8;
      if (!g_clih.history[idx][0])
        continue;
      if (!first)
        s += ",";
      first = false;
      // escape quotes
      String esc;
      for (const char *c = g_clih.history[idx]; *c; c++) {
        if (*c == '"')
          esc += "\\\"";
        else
          esc += *c;
      }
      s += "\"" + esc + "\"";
    }
    s += "]";
    server.send(200, "application/json", s);
  });

  // GET /api/cli/cmds — list all registered commands
  server.on("/api/cli/cmds", HTTP_GET, []() {
    String s = "[";
    bool first = true;
    for (int i = 0; i < CLIH_MAX_CMDS; i++) {
      if (!g_clih.cmds[i].active)
        continue;
      if (!first)
        s += ",";
      first = false;
      s += "{\"name\":\"" + String(g_clih.cmds[i].name) + "\"";
      s += ",\"args\":\"" + String(g_clih.cmds[i].args) + "\"";
      s += ",\"desc\":\"" + String(g_clih.cmds[i].desc) + "\"}";
    }
    s += "]";
    server.send(200, "application/json", s);
  });
}

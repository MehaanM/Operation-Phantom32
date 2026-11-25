#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <string.h>
#include <stdint.h>

// ── Extended CLI Command Interface ────────────────────────────────────────────
// Provides a text-based command line interface over HTTP (GET /api/cli/run?cmd=)
// and WebSocket-like polling. Commands map to module functions.
// Prefix: "cli2_"
// ─────────────────────────────────────────────────────────────────────────────

extern WebServer server;

#define CLI2_MAX_HISTORY 64
#define CLI2_MAX_OUTPUT 2048
#define CLI2_MAX_CMD_LEN 128
#define CLI2_MAX_ARGS 8

struct Cli2Command {
  const char *name;
  const char *usage;
  const char *help;
  String (*handler)(int argc, char *argv[]);
};

struct Cli2HistoryEntry {
  uint32_t ts_ms;
  char cmd[CLI2_MAX_CMD_LEN + 1];
  char result_snippet[64];
};

struct Cli2State {
  Cli2HistoryEntry history[CLI2_MAX_HISTORY];
  uint8_t hist_head;
  uint16_t hist_count;
  uint32_t cmd_count;
  uint32_t err_count;
};

static Cli2State g_cli2 = {};

// ── String utility ────────────────────────────────────────────────────────────
static void cli2_trim(char *s) {
  while (*s == ' ' || *s == '\t')
    memmove(s, s + 1, strlen(s));
  int l = strlen(s);
  while (l > 0 && (s[l - 1] == ' ' || s[l - 1] == '\t' || s[l - 1] == '\n' || s[l - 1] == '\r'))
    s[--l] = 0;
}

static int cli2_split(char *buf, char *argv[], int max_argc) {
  int argc = 0;
  char *p = buf;
  while (*p && argc < max_argc) {
    while (*p == ' ' || *p == '\t')
      p++;
    if (!*p)
      break;
    argv[argc++] = p;
    if (*p == '"') {
      p++;
      argv[argc - 1] = p;
      while (*p && *p != '"')
        p++;
      if (*p)
        *p++ = 0;
    } else {
      while (*p && *p != ' ' && *p != '\t')
        p++;
      if (*p)
        *p++ = 0;
    }
  }
  return argc;
}

static void cli2_log(const char *cmd, const char *result_snippet) {
  Cli2HistoryEntry *e = &g_cli2.history[g_cli2.hist_head];
  e->ts_ms = millis();
  strncpy(e->cmd, cmd, CLI2_MAX_CMD_LEN);
  strncpy(e->result_snippet, result_snippet, 63);
  g_cli2.hist_head = (g_cli2.hist_head + 1) % CLI2_MAX_HISTORY;
  if (g_cli2.hist_count < CLI2_MAX_HISTORY)
    g_cli2.hist_count++;
  g_cli2.cmd_count++;
}

// ── Command implementations ───────────────────────────────────────────────────

static String cmd_help(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  return "Commands: help, info, scan, aps, stas, vulns, threats, channels,\n"
         "          deauth <bssid> [ch] [pps] [dur]\n"
         "          estop\n"
         "          capture start [file] | stop | status\n"
         "          pcap replay <file> [count]\n"
         "          karma start [mode 0-3] [ch] | stop | status | add <ssid> | list\n"
         "          twin start <ssid> [ch] [portal 0-3] | stop | creds | status\n"
         "          wps list | filter\n"
         "          ble scan [ms] | stop | list | clear\n"
         "          iot list | risky | clear\n"
         "          ddt enable | disable | alerts | tracks | clear\n"
         "          replay capture start | stop | replay [all] [count] [ms]\n"
         "          fs list [path] | cat <file> | rm <file> | df\n"
         "          reset | reboot\n"
         "          set <key> <value> | get <key>  (runtime knobs)\n"
         "          history | clear_history";
}

static String cmd_info(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  char buf[256];
  snprintf(buf, sizeof(buf), "chip=%s rev=%d sdk=%s heap=%u flash=%u cpu=%uMHz", ESP.getChipModel(),
           (int)ESP.getChipRevision(), ESP.getSdkVersion(), (unsigned)ESP.getFreeHeap(),
           (unsigned)ESP.getFlashChipSize(), (unsigned)ESP.getCpuFreqMHz());
  return String(buf);
}

static String cmd_scan(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  WiFi.scanNetworks(true, true);
  return "Scan started (async). Poll: GET /api/sys/scan_results";
}

static String cmd_aps(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  // Pull from cs_aps endpoint
  return "Query: GET /api/cs/aps?per=200";
}

static String cmd_stas(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  return "Query: GET /api/st/stas?per=200";
}

static String cmd_vulns(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  return "Query: GET /api/vs/vulns";
}

static String cmd_threats(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  return "Query: GET /api/ti/threats";
}

static String cmd_channels(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  return "Query: GET /api/anl/heatmap";
}

static String cmd_deauth(int argc, char *argv[]) {
  if (argc < 2)
    return "Usage: deauth <bssid> [ch=6] [pps=10] [dur=5000]";
  String url = "/api/sched/deauth?bssid=" + String(argv[1]);
  if (argc > 2)
    url += "&ch=" + String(argv[2]);
  if (argc > 3)
    url += "&pps=" + String(argv[3]);
  if (argc > 4)
    url += "&dur=" + String(argv[4]);
  return "POST " + url;
}

static String cmd_estop(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  return "POST /api/sched/estop";
}

static String cmd_capture(int argc, char *argv[]) {
  if (argc < 2)
    return "Usage: capture start [file] | stop | status";
  if (strcmp(argv[1], "start") == 0) {
    String f = (argc > 2) ? String(argv[2]) : "/capture.pcap";
    return "POST /api/pcap/start?file=" + f;
  }
  if (strcmp(argv[1], "stop") == 0)
    return "POST /api/pcap/stop";
  if (strcmp(argv[1], "status") == 0)
    return "GET /api/pcap/status";
  return "Unknown subcommand: " + String(argv[1]);
}

static String cmd_karma(int argc, char *argv[]) {
  if (argc < 2)
    return "Usage: karma start|stop|status|add|list";
  if (strcmp(argv[1], "start") == 0) {
    int mode = (argc > 2) ? atoi(argv[2]) : 1;
    int ch = (argc > 3) ? atoi(argv[3]) : 6;
    return "POST /api/ka/start?mode=" + String(mode) + "&ch=" + String(ch);
  }
  if (strcmp(argv[1], "stop") == 0)
    return "POST /api/ka/stop";
  if (strcmp(argv[1], "status") == 0)
    return "GET /api/ka/status";
  if (strcmp(argv[1], "list") == 0)
    return "GET /api/ka/pool";
  if (strcmp(argv[1], "add") == 0) {
    if (argc < 3)
      return "Usage: karma add <ssid>";
    return "POST /api/ka/pool/add?ssid=" + String(argv[2]);
  }
  return "Unknown: " + String(argv[1]);
}

static String cmd_twin(int argc, char *argv[]) {
  if (argc < 2)
    return "Usage: twin start|stop|creds|status";
  if (strcmp(argv[1], "start") == 0) {
    if (argc < 3)
      return "Usage: twin start <ssid> [ch] [portal]";
    int ch = (argc > 3) ? atoi(argv[3]) : 6;
    int portal = (argc > 4) ? atoi(argv[4]) : 1;
    return "POST /api/et/start?ssid=" + String(argv[2]) + "&ch=" + String(ch) +
           "&portal=" + String(portal);
  }
  if (strcmp(argv[1], "stop") == 0)
    return "POST /api/et/stop";
  if (strcmp(argv[1], "creds") == 0)
    return "GET /api/et/creds";
  if (strcmp(argv[1], "status") == 0)
    return "GET /api/et/status";
  return "Unknown: " + String(argv[1]);
}

static String cmd_wps(int argc, char *argv[]) {
  if (argc < 2)
    return "Usage: wps list | filter | vulnerable";
  if (strcmp(argv[1], "list") == 0)
    return "GET /api/wps/aps";
  if (strcmp(argv[1], "filter") == 0)
    return "GET /api/wps/vulnerable";
  if (strcmp(argv[1], "vulnerable") == 0)
    return "GET /api/wps/vulnerable";
  return "Unknown: " + String(argv[1]);
}

static String cmd_ble(int argc, char *argv[]) {
  if (argc < 2)
    return "Usage: ble scan [ms] | stop | list | clear";
  if (strcmp(argv[1], "scan") == 0) {
    int ms = (argc > 2) ? atoi(argv[2]) : 5000;
    return "POST /api/ble/scan/start?ms=" + String(ms);
  }
  if (strcmp(argv[1], "stop") == 0)
    return "POST /api/ble/scan/stop";
  if (strcmp(argv[1], "list") == 0)
    return "GET /api/ble/devices";
  if (strcmp(argv[1], "clear") == 0)
    return "POST /api/ble/clear";
  return "Unknown: " + String(argv[1]);
}

static String cmd_iot(int argc, char *argv[]) {
  if (argc < 2)
    return "Usage: iot list | risky | clear";
  if (strcmp(argv[1], "list") == 0)
    return "GET /api/iot/devices";
  if (strcmp(argv[1], "risky") == 0)
    return "GET /api/iot/risky";
  if (strcmp(argv[1], "clear") == 0)
    return "POST /api/iot/clear";
  return "Unknown: " + String(argv[1]);
}

static String cmd_ddt(int argc, char *argv[]) {
  if (argc < 2)
    return "Usage: ddt enable|disable|alerts|tracks|clear";
  if (strcmp(argv[1], "enable") == 0)
    return "POST /api/ddt/enable";
  if (strcmp(argv[1], "disable") == 0)
    return "POST /api/ddt/disable";
  if (strcmp(argv[1], "alerts") == 0)
    return "GET /api/ddt/alerts";
  if (strcmp(argv[1], "tracks") == 0)
    return "GET /api/ddt/tracks";
  if (strcmp(argv[1], "clear") == 0)
    return "POST /api/ddt/clear";
  return "Unknown: " + String(argv[1]);
}

static String cmd_replay(int argc, char *argv[]) {
  if (argc < 2)
    return "Usage: replay capture start|stop | replay [all] [count] [ms]";
  if (strcmp(argv[1], "capture") == 0) {
    if (argc < 3)
      return "Usage: replay capture start|stop";
    if (strcmp(argv[2], "start") == 0)
      return "POST /api/pr/capture/start";
    if (strcmp(argv[2], "stop") == 0)
      return "POST /api/pr/capture/stop";
  }
  if (strcmp(argv[1], "replay") == 0) {
    bool all = (argc > 2 && strcmp(argv[2], "all") == 0);
    int cnt = (argc > 3) ? atoi(argv[3]) : 1;
    int ms = (argc > 4) ? atoi(argv[4]) : 10;
    return "POST /api/pr/replay?all=" + String(all ? 1 : 0) + "&count=" + String(cnt) +
           "&ms=" + String(ms);
  }
  return "Unknown: " + String(argv[1]);
}

static String cmd_fs(int argc, char *argv[]) {
  if (argc < 2)
    return "Usage: fs list [path] | cat <file> | rm <file> | df";
  if (strcmp(argv[1], "df") == 0) {
    char buf[64];
    size_t total = LittleFS.totalBytes(), used = LittleFS.usedBytes();
    snprintf(buf, sizeof(buf), "total=%uK used=%uK free=%uK", (unsigned)(total / 1024),
             (unsigned)(used / 1024), (unsigned)((total - used) / 1024));
    return String(buf);
  }
  if (strcmp(argv[1], "list") == 0) {
    const char *path = (argc > 2) ? argv[2] : "/";
    File dir = LittleFS.open(path);
    if (!dir || !dir.isDirectory())
      return "Not a directory: " + String(path);
    String out = "";
    File f = dir.openNextFile();
    while (f) {
      out += String(f.name()) + "\t" + String(f.size()) + "B\n";
      f = dir.openNextFile();
    }
    return out.length() ? out : "(empty)";
  }
  if (strcmp(argv[1], "cat") == 0) {
    if (argc < 3)
      return "Usage: fs cat <file>";
    File f = LittleFS.open(argv[2], "r");
    if (!f)
      return "File not found: " + String(argv[2]);
    String out = "";
    while (f.available() && out.length() < 1024)
      out += (char)f.read();
    if (f.available())
      out += "\n...(truncated)";
    return out;
  }
  if (strcmp(argv[1], "rm") == 0) {
    if (argc < 3)
      return "Usage: fs rm <file>";
    bool ok = LittleFS.remove(argv[2]);
    return ok ? "Deleted: " + String(argv[2]) : "Failed to delete: " + String(argv[2]);
  }
  return "Unknown: " + String(argv[1]);
}

static String cmd_reset(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  return "POST /api/sys/reset";
}

static String cmd_reboot(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  ESP.restart();
  return "Rebooting...";
}

static String cmd_history(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  String out = "";
  int start = (g_cli2.hist_head + CLI2_MAX_HISTORY - g_cli2.hist_count) % CLI2_MAX_HISTORY;
  for (int i = 0; i < g_cli2.hist_count; i++) {
    int idx = (start + i) % CLI2_MAX_HISTORY;
    out += String(g_cli2.history[idx].ts_ms) + " " + g_cli2.history[idx].cmd + "\n";
  }
  return out.length() ? out : "(no history)";
}

static String cmd_clear_history(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  g_cli2.hist_count = 0;
  g_cli2.hist_head = 0;
  return "History cleared";
}

// ── Knob store ────────────────────────────────────────────────────────────────
static char g_cli2_knob_keys[8][24];
static char g_cli2_knob_vals[8][64];
static uint8_t g_cli2_knob_count = 0;

static String cmd_set(int argc, char *argv[]) {
  if (argc < 3)
    return "Usage: set <key> <value>";
  for (uint8_t i = 0; i < g_cli2_knob_count; i++) {
    if (strcmp(g_cli2_knob_keys[i], argv[1]) == 0) {
      strncpy(g_cli2_knob_vals[i], argv[2], 63);
      return String("set ") + argv[1] + "=" + argv[2];
    }
  }
  if (g_cli2_knob_count < 8) {
    strncpy(g_cli2_knob_keys[g_cli2_knob_count], argv[1], 23);
    strncpy(g_cli2_knob_vals[g_cli2_knob_count], argv[2], 63);
    g_cli2_knob_count++;
    return String("set ") + argv[1] + "=" + argv[2];
  }
  return "Knob store full";
}

static String cmd_get(int argc, char *argv[]) {
  if (argc < 2) {
    String out = "";
    for (uint8_t i = 0; i < g_cli2_knob_count; i++)
      out += String(g_cli2_knob_keys[i]) + "=" + g_cli2_knob_vals[i] + "\n";
    return out.length() ? out : "(no knobs set)";
  }
  for (uint8_t i = 0; i < g_cli2_knob_count; i++)
    if (strcmp(g_cli2_knob_keys[i], argv[1]) == 0)
      return String(argv[1]) + "=" + g_cli2_knob_vals[i];
  return String("not found: ") + argv[1];
}

// ── Command dispatch table ────────────────────────────────────────────────────
static const Cli2Command CLI2_CMDS[] = {
    {"help", "help", "Show all commands", cmd_help},
    {"info", "info", "System info", cmd_info},
    {"scan", "scan", "Start WiFi scan", cmd_scan},
    {"aps", "aps", "List access points", cmd_aps},
    {"stas", "stas", "List stations", cmd_stas},
    {"vulns", "vulns", "List vulnerabilities", cmd_vulns},
    {"threats", "threats", "List threats", cmd_threats},
    {"channels", "channels", "Channel heatmap", cmd_channels},
    {"deauth", "deauth <bssid> [ch] [pps] [dur]", "Deauth burst", cmd_deauth},
    {"estop", "estop", "Emergency stop all jobs", cmd_estop},
    {"capture", "capture start|stop|status", "PCAP capture control", cmd_capture},
    {"karma", "karma start|stop|add|list", "KARMA attack control", cmd_karma},
    {"twin", "twin start|stop|creds", "Evil twin control", cmd_twin},
    {"wps", "wps list|filter", "WPS info", cmd_wps},
    {"ble", "ble scan|stop|list|clear", "BLE scanner", cmd_ble},
    {"iot", "iot list|risky|clear", "IoT fingerprint", cmd_iot},
    {"ddt", "ddt enable|disable|alerts", "Deauth detector", cmd_ddt},
    {"replay", "replay capture|replay", "Packet replay", cmd_replay},
    {"fs", "fs list|cat|rm|df", "Filesystem", cmd_fs},
    {"reset", "reset", "Post soft-reset request", cmd_reset},
    {"reboot", "reboot", "Hard reboot ESP32", cmd_reboot},
    {"history", "history", "Command history", cmd_history},
    {"clear_history", "clear_history", "Clear history", cmd_clear_history},
    {"set", "set <key> <val>", "Set runtime knob", cmd_set},
    {"get", "get [key]", "Get runtime knob", cmd_get},
};
static const int CLI2_CMD_COUNT = sizeof(CLI2_CMDS) / sizeof(CLI2_CMDS[0]);

// ── Command runner ────────────────────────────────────────────────────────────
static String cli2_run(const char *input) {
  if (!input || strlen(input) == 0)
    return "";
  static char buf[CLI2_MAX_CMD_LEN + 1];
  strncpy(buf, input, CLI2_MAX_CMD_LEN);
  cli2_trim(buf);

  char *argv[CLI2_MAX_ARGS];
  int argc = cli2_split(buf, argv, CLI2_MAX_ARGS);
  if (argc == 0)
    return "";

  String result;
  bool found = false;
  for (int i = 0; i < CLI2_CMD_COUNT; i++) {
    if (strcmp(CLI2_CMDS[i].name, argv[0]) == 0) {
      result = CLI2_CMDS[i].handler(argc, argv);
      found = true;
      g_cli2.cmd_count++;
      break;
    }
  }
  if (!found) {
    result = "Unknown command: " + String(argv[0]) + ". Type 'help' for a list.";
    g_cli2.err_count++;
  }

  cli2_log(input, result.substring(0, 63).c_str());
  return result;
}

// ── HTTP routes ───────────────────────────────────────────────────────────────
static void cli2_routes() {
  server.on("/api/cli/run", HTTP_GET, []() {
    if (!server.hasArg("cmd")) {
      server.send(400, "application/json", "{\"error\":\"cmd required\"}");
      return;
    }
    String result = cli2_run(server.arg("cmd").c_str());
    // Escape JSON
    result.replace("\\", "\\\\");
    result.replace("\"", "\\\"");
    result.replace("\n", "\\n");
    result.replace("\r", "");
    server.send(200, "application/json", "{\"result\":\"" + result + "\"}");
  });

  server.on("/api/cli/run", HTTP_POST, []() {
    String cmd = server.hasArg("cmd") ? server.arg("cmd") : server.arg("plain");
    String result = cli2_run(cmd.c_str());
    result.replace("\\", "\\\\");
    result.replace("\"", "\\\"");
    result.replace("\n", "\\n");
    result.replace("\r", "");
    server.send(200, "application/json", "{\"result\":\"" + result + "\"}");
  });

  server.on("/api/cli/history", HTTP_GET, []() {
    String j = "{\"count\":";
    j += g_cli2.hist_count;
    j += ",\"entries\":[";
    int start = (g_cli2.hist_head + CLI2_MAX_HISTORY - g_cli2.hist_count) % CLI2_MAX_HISTORY;
    for (int i = 0; i < g_cli2.hist_count; i++) {
      if (i)
        j += ",";
      int idx = (start + i) % CLI2_MAX_HISTORY;
      j += "{\"ts\":";
      j += g_cli2.history[idx].ts_ms;
      j += ",\"cmd\":\"";
      j += g_cli2.history[idx].cmd;
      j += "\"";
      j += "}";
    }
    j += "]}";
    server.send(200, "application/json", j);
  });

  server.on("/api/cli/commands", HTTP_GET, []() {
    String j = "{\"commands\":[";
    for (int i = 0; i < CLI2_CMD_COUNT; i++) {
      if (i)
        j += ",";
      j += "{\"name\":\"";
      j += CLI2_CMDS[i].name;
      j += "\"";
      j += ",\"usage\":\"";
      j += CLI2_CMDS[i].usage;
      j += "\"";
      j += ",\"help\":\"";
      j += CLI2_CMDS[i].help;
      j += "\"}";
    }
    j += "]}";
    server.send(200, "application/json", j);
  });

  server.on("/api/cli/stats", HTTP_GET, []() {
    server.send(200, "application/json",
                "{\"total_cmds\":" + String(g_cli2.cmd_count) + ",\"errors\":" +
                    String(g_cli2.err_count) + ",\"history\":" + String(g_cli2.hist_count) + "}");
  });
}

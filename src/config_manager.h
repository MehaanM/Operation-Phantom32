#pragma once
// config_manager.h — LittleFS-backed JSON configuration persistence.
// All module parameters are stored here and survive reboots. Prefix: cfg_

#include <Arduino.h>
#include <WebServer.h>
#include <LittleFS.h>

extern WebServer server;

#define CFG_MAX_KEYS    64
#define CFG_KEY_LEN     32
#define CFG_VAL_LEN     64
#define CFG_FILE_PATH   "/config.json"
#define CFG_BACKUP_PATH "/config.bak"

// ─── Key-value store ──────────────────────────────────────────────────────────
struct CfgEntry {
  bool   used;
  char   key[CFG_KEY_LEN];
  char   val[CFG_VAL_LEN];
  bool   dirty;        // modified but not flushed
  uint8_t type;        // 0=string, 1=int, 2=bool, 3=float
};

static struct {
  CfgEntry entries[CFG_MAX_KEYS];
  uint8_t  count;
  bool     loaded;
  uint32_t last_flush_ts;
  uint32_t flush_count;
  uint32_t load_count;
} g_cfg = {};

// ─── Helpers ─────────────────────────────────────────────────────────────────
static CfgEntry* cfg_find(const char *key) {
  for (int i = 0; i < CFG_MAX_KEYS; i++) {
    if (g_cfg.entries[i].used && strcmp(g_cfg.entries[i].key, key) == 0)
      return &g_cfg.entries[i];
  }
  return nullptr;
}

static CfgEntry* cfg_alloc(const char *key) {
  for (int i = 0; i < CFG_MAX_KEYS; i++) {
    if (!g_cfg.entries[i].used) {
      CfgEntry &e = g_cfg.entries[i];
      memset(&e, 0, sizeof(e));
      strncpy(e.key, key, CFG_KEY_LEN - 1);
      e.used = true;
      g_cfg.count++;
      return &e;
    }
  }
  return nullptr;
}

// ─── Set/get ──────────────────────────────────────────────────────────────────
void cfg_set(const char *key, const char *val) {
  CfgEntry *e = cfg_find(key);
  if (!e) e = cfg_alloc(key);
  if (!e) return;
  strncpy(e->val, val, CFG_VAL_LEN - 1);
  e->type = 0;
  e->dirty = true;
}

void cfg_set_int(const char *key, int val) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%d", val);
  cfg_set(key, buf);
  CfgEntry *e = cfg_find(key);
  if (e) e->type = 1;
}

void cfg_set_bool(const char *key, bool val) {
  cfg_set(key, val ? "true" : "false");
  CfgEntry *e = cfg_find(key);
  if (e) e->type = 2;
}

void cfg_set_float(const char *key, float val) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%.4f", val);
  cfg_set(key, buf);
  CfgEntry *e = cfg_find(key);
  if (e) e->type = 3;
}

const char* cfg_get(const char *key, const char *def = "") {
  CfgEntry *e = cfg_find(key);
  return e ? e->val : def;
}

int cfg_get_int(const char *key, int def = 0) {
  CfgEntry *e = cfg_find(key);
  return e ? atoi(e->val) : def;
}

bool cfg_get_bool(const char *key, bool def = false) {
  CfgEntry *e = cfg_find(key);
  if (!e) return def;
  return strcmp(e->val, "true") == 0 || strcmp(e->val, "1") == 0;
}

float cfg_get_float(const char *key, float def = 0.0f) {
  CfgEntry *e = cfg_find(key);
  return e ? atof(e->val) : def;
}

bool cfg_exists(const char *key) { return cfg_find(key) != nullptr; }

void cfg_delete(const char *key) {
  CfgEntry *e = cfg_find(key);
  if (e) { memset(e, 0, sizeof(*e)); g_cfg.count--; }
}

// ─── Persist to LittleFS ──────────────────────────────────────────────────────
bool cfg_flush() {
  if (!LittleFS.begin()) return false;
  // Backup existing
  if (LittleFS.exists(CFG_FILE_PATH))
    LittleFS.rename(CFG_FILE_PATH, CFG_BACKUP_PATH);

  File f = LittleFS.open(CFG_FILE_PATH, "w");
  if (!f) return false;

  f.print("{");
  bool first = true;
  for (int i = 0; i < CFG_MAX_KEYS; i++) {
    CfgEntry &e = g_cfg.entries[i];
    if (!e.used) continue;
    if (!first) f.print(",");
    first = false;
    f.printf("\"%s\":\"%s\"", e.key, e.val);
    e.dirty = false;
  }
  f.print("}");
  f.close();

  g_cfg.last_flush_ts = millis() / 1000;
  g_cfg.flush_count++;
  return true;
}

// ─── Minimal JSON parser ──────────────────────────────────────────────────────
static void cfg_parse_json(const char *json, int len) {
  // Simple k:v parser for flat JSON object
  int i = 0;
  while (i < len) {
    // Skip to opening quote
    while (i < len && json[i] != '"') i++;
    if (i >= len) break;
    i++; // skip "
    // Read key
    int key_start = i;
    while (i < len && json[i] != '"') i++;
    if (i >= len) break;
    int key_len = min(i - key_start, CFG_KEY_LEN - 1);
    char key[CFG_KEY_LEN] = {};
    memcpy(key, json + key_start, key_len);
    i++; // skip closing "
    // Skip : and whitespace
    while (i < len && (json[i] == ':' || json[i] == ' ')) i++;
    if (i >= len) break;
    if (json[i] == '"') {
      i++; // skip opening "
      int val_start = i;
      while (i < len && json[i] != '"') i++;
      if (i >= len) break;
      int val_len = min(i - val_start, CFG_VAL_LEN - 1);
      char val[CFG_VAL_LEN] = {};
      memcpy(val, json + val_start, val_len);
      cfg_set(key, val);
      i++; // skip closing "
    }
    // Skip to next key
    while (i < len && json[i] != ',' && json[i] != '}') i++;
    if (i < len && json[i] == ',') i++;
  }
}

bool cfg_load() {
  if (!LittleFS.begin()) return false;
  if (!LittleFS.exists(CFG_FILE_PATH)) {
    // Try backup
    if (!LittleFS.exists(CFG_BACKUP_PATH)) return false;
    LittleFS.rename(CFG_BACKUP_PATH, CFG_FILE_PATH);
  }
  File f = LittleFS.open(CFG_FILE_PATH, "r");
  if (!f) return false;
  int sz = f.size();
  if (sz <= 0 || sz > 4096) { f.close(); return false; }
  char *buf = (char*)malloc(sz + 1);
  if (!buf) { f.close(); return false; }
  f.readBytes(buf, sz);
  buf[sz] = '\0';
  f.close();

  cfg_parse_json(buf, sz);
  free(buf);
  g_cfg.loaded = true;
  g_cfg.load_count++;
  return true;
}

// ─── Default initialization ───────────────────────────────────────────────────
void cfg_init_defaults() {
  if (!cfg_exists("ap_ssid"))       cfg_set("ap_ssid", "Phantom32");
  if (!cfg_exists("ap_password"))   cfg_set("ap_password", "phantom32"); // ap-default
  if (!cfg_exists("ap_channel"))    cfg_set_int("ap_channel", 6);
  if (!cfg_exists("ap_hidden"))     cfg_set_bool("ap_hidden", false);
  if (!cfg_exists("auto_scan"))     cfg_set_bool("auto_scan", true);
  if (!cfg_exists("scan_dwell_ms")) cfg_set_int("scan_dwell_ms", 150);
  if (!cfg_exists("pcap_dlt"))      cfg_set_int("pcap_dlt", 127);
  if (!cfg_exists("log_level"))     cfg_set_int("log_level", 2);
  if (!cfg_exists("web_refresh"))   cfg_set_int("web_refresh", 5000);
  if (!cfg_exists("tx_power"))      cfg_set_int("tx_power", 20);
  if (!cfg_exists("hostname"))      cfg_set("hostname", "phantom32"); // ap-default
  if (!cfg_exists("timezone_offset")) cfg_set_int("timezone_offset", 0);
  if (!cfg_exists("enable_ble"))    cfg_set_bool("enable_ble", true);
  if (!cfg_exists("enable_ntp"))    cfg_set_bool("enable_ntp", true);
  if (!cfg_exists("auto_flush"))    cfg_set_bool("auto_flush", true);
}

// ─── Auto-flush tick ─────────────────────────────────────────────────────────
void cfg_tick() {
  if (!cfg_get_bool("auto_flush", true)) return;
  // Flush every 60 seconds if any dirty entries
  for (int i = 0; i < CFG_MAX_KEYS; i++) {
    if (g_cfg.entries[i].used && g_cfg.entries[i].dirty) {
      uint32_t now = millis() / 1000;
      if (now - g_cfg.last_flush_ts > 60) cfg_flush();
      break;
    }
  }
}

// ─── JSON export ─────────────────────────────────────────────────────────────
static String cfg_json_all() {
  String s = "{\"count\":" + String(g_cfg.count);
  s += ",\"loaded\":" + String(g_cfg.loaded?"true":"false");
  s += ",\"flush_count\":" + String(g_cfg.flush_count);
  s += ",\"entries\":{";
  bool first = true;
  for (int i = 0; i < CFG_MAX_KEYS; i++) {
    CfgEntry &e = g_cfg.entries[i];
    if (!e.used) continue;
    if (!first) s += ",";
    first = false;
    s += "\"" + String(e.key) + "\":\"" + String(e.val) + "\"";
  }
  s += "}}";
  return s;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void cfg_routes() {
  server.on("/api/cfg/all", HTTP_GET, []() {
    server.send(200, "application/json", cfg_json_all());
  });

  server.on("/api/cfg/get", HTTP_GET, []() {
    if (!server.hasArg("key")) {
      server.send(400, "application/json", "{\"error\":\"missing key\"}"); return;
    }
    const char *key = server.arg("key").c_str();
    if (!cfg_exists(key)) {
      server.send(404, "application/json", "{\"error\":\"not found\"}"); return;
    }
    server.send(200, "application/json",
      "{\"key\":\"" + String(key) + "\",\"value\":\"" + String(cfg_get(key)) + "\"}");
  });

  server.on("/api/cfg/set", HTTP_POST, []() {
    if (!server.hasArg("key") || !server.hasArg("value")) {
      server.send(400, "application/json", "{\"error\":\"missing params\"}"); return;
    }
    cfg_set(server.arg("key").c_str(), server.arg("value").c_str());
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/cfg/delete", HTTP_POST, []() {
    if (!server.hasArg("key")) {
      server.send(400, "application/json", "{\"error\":\"missing key\"}"); return;
    }
    cfg_delete(server.arg("key").c_str());
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/cfg/flush", HTTP_POST, []() {
    bool ok = cfg_flush();
    server.send(200, "application/json", ok ? "{\"ok\":true}" : "{\"error\":\"flush failed\"}");
  });

  server.on("/api/cfg/load", HTTP_POST, []() {
    bool ok = cfg_load();
    server.send(200, "application/json", ok ? "{\"ok\":true}" : "{\"error\":\"load failed\"}");
  });

  server.on("/api/cfg/reset", HTTP_POST, []() {
    memset(&g_cfg, 0, sizeof(g_cfg));
    cfg_init_defaults();
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

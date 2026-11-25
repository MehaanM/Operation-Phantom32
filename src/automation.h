#pragma once
// automation.h — Trigger-based automation rules engine for Phantom32.
// Supports event triggers (new AP, deauth flood, signal threshold, timer),
// conditions, and actions (start module, change channel, alert, log).
// Prefix: auto_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

// ─── Trigger types ────────────────────────────────────────────────────────────
enum AutoTriggerType {
  AUTO_TRIG_NONE = 0,
  AUTO_TRIG_NEW_AP,        // new AP discovered
  AUTO_TRIG_NEW_STA,       // new station discovered
  AUTO_TRIG_DEAUTH_FLOOD,  // deauth flood detected
  AUTO_TRIG_SIGNAL_LOW,    // AP RSSI drops below threshold
  AUTO_TRIG_SIGNAL_HIGH,   // AP RSSI rises above threshold
  AUTO_TRIG_OPEN_AP,       // open (unencrypted) AP found
  AUTO_TRIG_WPS_AP,        // WPS-enabled AP found
  AUTO_TRIG_TIMER,         // periodic timer
  AUTO_TRIG_CHANNEL_IDLE,  // channel drops below packet rate
  AUTO_TRIG_THREAT_LEVEL,  // threat intelligence level crosses threshold
  AUTO_TRIG_SSID_MATCH,    // SSID matches a pattern
  AUTO_TRIG_MAC_MATCH,     // MAC matches OUI/mask
  AUTO_TRIG_HANDSHAKE,     // 4-way handshake / PMKID captured
  AUTO_TRIG_MANUAL,        // manually triggered via API
};

// ─── Action types ─────────────────────────────────────────────────────────────
enum AutoActionType {
  AUTO_ACT_NONE = 0,
  AUTO_ACT_LOG,             // write to event log
  AUTO_ACT_CHANGE_CHANNEL,  // hop to specified channel
  AUTO_ACT_START_SCAN,      // trigger active scan
  AUTO_ACT_START_KARMA,     // start KARMA attack
  AUTO_ACT_STOP_KARMA,      // stop KARMA attack
  AUTO_ACT_START_PMK,       // start PMKID extraction
  AUTO_ACT_STOP_PMK,        // stop PMKID extraction
  AUTO_ACT_START_PCAP,      // start PCAP recording
  AUTO_ACT_STOP_PCAP,       // stop PCAP recording
  AUTO_ACT_BLINK_LED,       // blink status LED
  AUTO_ACT_SET_VAR,         // set a runtime variable
  AUTO_ACT_HTTP_NOTIFY,     // HTTP GET to notify URL (local only)
  AUTO_ACT_CHAIN_RULE,      // trigger another rule
};

// ─── Condition types ──────────────────────────────────────────────────────────
enum AutoCondType {
  AUTO_COND_NONE = 0,
  AUTO_COND_CHANNEL_EQ,
  AUTO_COND_CHANNEL_IN,
  AUTO_COND_TIME_RANGE,
  AUTO_COND_VAR_EQ,
  AUTO_COND_COUNT_GT,
  AUTO_COND_COUNT_LT,
  AUTO_COND_FLAG_SET,
};

// ─── Structs ──────────────────────────────────────────────────────────────────
#define AUTO_MAX_RULES 16
#define AUTO_STR_LEN 48
#define AUTO_MAX_EVENTS 64
#define AUTO_CHAIN_MAX 4

struct AutoTrigger {
  AutoTriggerType type;
  int32_t int_param;             // threshold / interval_ms / channel
  char str_param[AUTO_STR_LEN];  // SSID pattern / MAC prefix
  uint8_t mac_prefix[3];         // OUI match for MAC triggers
  bool mac_prefix_set;
};

struct AutoCondition {
  AutoCondType type;
  int32_t int_param;
  char str_param[AUTO_STR_LEN];
};

struct AutoAction {
  AutoActionType type;
  int32_t int_param;              // channel / blink_count / etc.
  char str_param[AUTO_STR_LEN];   // log message / var name / url path
  char str_param2[AUTO_STR_LEN];  // var value / rule name
};

struct AutoRule {
  bool active;
  bool enabled;
  char name[AUTO_STR_LEN];
  AutoTrigger trigger;
  AutoCondition condition;
  AutoAction action;
  uint32_t fire_count;
  uint32_t last_fire_ts;
  uint32_t cooldown_ms;  // min ms between fires
  int32_t max_fires;     // -1 = unlimited
};

struct AutoEvent {
  uint32_t ts;
  char rule_name[AUTO_STR_LEN];
  char detail[80];
  uint8_t trig_type;
  uint8_t act_type;
};

// ─── State ───────────────────────────────────────────────────────────────────
static struct {
  AutoRule rules[AUTO_MAX_RULES];
  AutoEvent events[AUTO_MAX_EVENTS];
  uint8_t event_head;
  bool engine_running;
  uint32_t eval_count;
  uint32_t fire_count;

  // Runtime variables (key-value store)
  char var_keys[8][AUTO_STR_LEN];
  char var_vals[8][AUTO_STR_LEN];
  uint8_t var_count;
} g_auto = {};

// ─── Helpers ─────────────────────────────────────────────────────────────────
static void auto_log_event(const char *rule_name, const char *detail, AutoTriggerType tt,
                           AutoActionType at) {
  AutoEvent &ev = g_auto.events[g_auto.event_head % AUTO_MAX_EVENTS];
  ev.ts = millis() / 1000;
  strncpy(ev.rule_name, rule_name, AUTO_STR_LEN - 1);
  strncpy(ev.detail, detail, 79);
  ev.trig_type = (uint8_t)tt;
  ev.act_type = (uint8_t)at;
  g_auto.event_head++;
}

static const char *auto_var_get(const char *key) {
  for (int i = 0; i < g_auto.var_count; i++) {
    if (strcmp(g_auto.var_keys[i], key) == 0)
      return g_auto.var_vals[i];
  }
  return "";
}

static void auto_var_set(const char *key, const char *val) {
  for (int i = 0; i < g_auto.var_count; i++) {
    if (strcmp(g_auto.var_keys[i], key) == 0) {
      strncpy(g_auto.var_vals[i], val, AUTO_STR_LEN - 1);
      return;
    }
  }
  if (g_auto.var_count < 8) {
    strncpy(g_auto.var_keys[g_auto.var_count], key, AUTO_STR_LEN - 1);
    strncpy(g_auto.var_vals[g_auto.var_count], val, AUTO_STR_LEN - 1);
    g_auto.var_count++;
  }
}

// ─── Condition evaluator ──────────────────────────────────────────────────────
static bool auto_eval_condition(const AutoCondition &cond, int32_t context_int,
                                const char *context_str) {
  switch (cond.type) {
    case AUTO_COND_NONE:
      return true;
    case AUTO_COND_CHANNEL_EQ:
      return context_int == cond.int_param;
    case AUTO_COND_CHANNEL_IN: {
      // int_param encodes bitmask for channels 1-14
      return (cond.int_param & (1 << (context_int - 1))) != 0;
    }
    case AUTO_COND_VAR_EQ: {
      const char *v = auto_var_get(cond.str_param);
      return strcmp(v, context_str) == 0;
    }
    case AUTO_COND_COUNT_GT:
      return context_int > cond.int_param;
    case AUTO_COND_COUNT_LT:
      return context_int < cond.int_param;
    case AUTO_COND_FLAG_SET: {
      const char *v = auto_var_get(cond.str_param);
      return strcmp(v, "1") == 0 || strcmp(v, "true") == 0;
    }
    default:
      return true;
  }
}

// ─── Action executor ─────────────────────────────────────────────────────────
static void auto_exec_action(AutoRule &rule, int32_t ctx_int, const char *ctx_str) {
  AutoAction &a = rule.action;
  switch (a.type) {
    case AUTO_ACT_NONE:
      break;
    case AUTO_ACT_LOG: {
      char detail[128];
      snprintf(detail, sizeof(detail), "[%s] %s (ctx=%ld)", rule.name, a.str_param, (long)ctx_int);
      auto_log_event(rule.name, detail, rule.trigger.type, a.type);
      break;
    }
    case AUTO_ACT_CHANGE_CHANNEL: {
      uint8_t ch = (uint8_t)a.int_param;
      if (ch >= 1 && ch <= 14) {
        wifi_second_chan_t sec = WIFI_SECOND_CHAN_NONE;
        esp_wifi_set_channel(ch, sec);
        char detail[64];
        snprintf(detail, sizeof(detail), "Changed to channel %d", ch);
        auto_log_event(rule.name, detail, rule.trigger.type, a.type);
      }
      break;
    }
    case AUTO_ACT_START_PCAP: {
      auto_log_event(rule.name, "PCAP recording started by automation", rule.trigger.type, a.type);
      break;
    }
    case AUTO_ACT_STOP_PCAP: {
      auto_log_event(rule.name, "PCAP recording stopped by automation", rule.trigger.type, a.type);
      break;
    }
    case AUTO_ACT_SET_VAR: {
      auto_var_set(a.str_param, a.str_param2);
      char detail[80];
      snprintf(detail, sizeof(detail), "Set var %s = %s", a.str_param, a.str_param2);
      auto_log_event(rule.name, detail, rule.trigger.type, a.type);
      break;
    }
    case AUTO_ACT_BLINK_LED: {
      // LED on GPIO2 (built-in on most ESP32 dev boards)
      for (int i = 0; i < a.int_param && i < 10; i++) {
        digitalWrite(2, HIGH);
        delay(100);
        digitalWrite(2, LOW);
        delay(100);
      }
      break;
    }
    default:
      break;
  }
  rule.fire_count++;
  rule.last_fire_ts = millis() / 1000;
  g_auto.fire_count++;
}

// ─── Fire a rule ─────────────────────────────────────────────────────────────
static void auto_try_fire(AutoRule &rule, int32_t ctx_int, const char *ctx_str) {
  if (!rule.enabled || !rule.active)
    return;
  if (rule.max_fires > 0 && (int32_t)rule.fire_count >= rule.max_fires)
    return;
  uint32_t now = millis();
  if (rule.cooldown_ms > 0 && (now - rule.last_fire_ts * 1000) < rule.cooldown_ms)
    return;
  if (!auto_eval_condition(rule.condition, ctx_int, ctx_str))
    return;
  auto_exec_action(rule, ctx_int, ctx_str);
}

// ─── Public trigger API (called from other modules) ───────────────────────────
void auto_trigger(AutoTriggerType type, int32_t ctx_int = 0, const char *ctx_str = "") {
  if (!g_auto.engine_running)
    return;
  g_auto.eval_count++;
  for (int i = 0; i < AUTO_MAX_RULES; i++) {
    AutoRule &r = g_auto.rules[i];
    if (!r.active || !r.enabled)
      continue;
    if (r.trigger.type != type)
      continue;

    // Pattern matching for string-based triggers
    if (type == AUTO_TRIG_SSID_MATCH) {
      if (r.trigger.str_param[0] && strstr(ctx_str, r.trigger.str_param) == nullptr)
        continue;
    }
    if (type == AUTO_TRIG_MAC_MATCH) {
      if (r.trigger.mac_prefix_set) {
        uint8_t mac[6];
        sscanf(ctx_str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &mac[0], &mac[1], &mac[2], &mac[3],
               &mac[4], &mac[5]);
        if (memcmp(mac, r.trigger.mac_prefix, 3) != 0)
          continue;
      }
    }
    if (type == AUTO_TRIG_SIGNAL_LOW && ctx_int > r.trigger.int_param)
      continue;
    if (type == AUTO_TRIG_SIGNAL_HIGH && ctx_int < r.trigger.int_param)
      continue;

    auto_try_fire(r, ctx_int, ctx_str);
  }
}

// ─── Timer-based evaluation ───────────────────────────────────────────────────
static uint32_t last_timer_eval = 0;
void auto_tick() {
  if (!g_auto.engine_running)
    return;
  uint32_t now = millis();
  for (int i = 0; i < AUTO_MAX_RULES; i++) {
    AutoRule &r = g_auto.rules[i];
    if (!r.active || !r.enabled)
      continue;
    if (r.trigger.type != AUTO_TRIG_TIMER)
      continue;
    uint32_t interval = (uint32_t)r.trigger.int_param;
    if (interval == 0)
      interval = 60000;
    if (now - r.last_fire_ts * 1000 >= interval) {
      auto_try_fire(r, now / 1000, "timer");
    }
  }
}

// ─── Rule management ─────────────────────────────────────────────────────────
inline void auto_start() {
  g_auto.engine_running = true;
}
inline void auto_stop() {
  g_auto.engine_running = false;
}

static int auto_alloc_rule() {
  for (int i = 0; i < AUTO_MAX_RULES; i++) {
    if (!g_auto.rules[i].active)
      return i;
  }
  return -1;
}

static AutoRule *auto_find_rule(const char *name) {
  for (int i = 0; i < AUTO_MAX_RULES; i++) {
    if (g_auto.rules[i].active && strcmp(g_auto.rules[i].name, name) == 0)
      return &g_auto.rules[i];
  }
  return nullptr;
}

// ─── JSON serialization ───────────────────────────────────────────────────────
static const char *auto_trig_name(AutoTriggerType t) {
  switch (t) {
    case AUTO_TRIG_NEW_AP:
      return "new_ap";
    case AUTO_TRIG_NEW_STA:
      return "new_sta";
    case AUTO_TRIG_DEAUTH_FLOOD:
      return "deauth_flood";
    case AUTO_TRIG_SIGNAL_LOW:
      return "signal_low";
    case AUTO_TRIG_SIGNAL_HIGH:
      return "signal_high";
    case AUTO_TRIG_OPEN_AP:
      return "open_ap";
    case AUTO_TRIG_WPS_AP:
      return "wps_ap";
    case AUTO_TRIG_TIMER:
      return "timer";
    case AUTO_TRIG_SSID_MATCH:
      return "ssid_match";
    case AUTO_TRIG_MAC_MATCH:
      return "mac_match";
    case AUTO_TRIG_HANDSHAKE:
      return "handshake";
    case AUTO_TRIG_MANUAL:
      return "manual";
    default:
      return "none";
  }
}

static const char *auto_act_name(AutoActionType a) {
  switch (a) {
    case AUTO_ACT_LOG:
      return "log";
    case AUTO_ACT_CHANGE_CHANNEL:
      return "change_channel";
    case AUTO_ACT_START_SCAN:
      return "start_scan";
    case AUTO_ACT_START_KARMA:
      return "start_karma";
    case AUTO_ACT_STOP_KARMA:
      return "stop_karma";
    case AUTO_ACT_START_PMK:
      return "start_pmk";
    case AUTO_ACT_STOP_PMK:
      return "stop_pmk";
    case AUTO_ACT_START_PCAP:
      return "start_pcap";
    case AUTO_ACT_STOP_PCAP:
      return "stop_pcap";
    case AUTO_ACT_BLINK_LED:
      return "blink_led";
    case AUTO_ACT_SET_VAR:
      return "set_var";
    case AUTO_ACT_HTTP_NOTIFY:
      return "http_notify";
    case AUTO_ACT_CHAIN_RULE:
      return "chain_rule";
    default:
      return "none";
  }
}

static String auto_json_rules() {
  String s = "{\"running\":";
  s += g_auto.engine_running ? "true" : "false";
  s += ",\"eval_count\":" + String(g_auto.eval_count);
  s += ",\"fire_count\":" + String(g_auto.fire_count);
  s += ",\"rules\":[";
  bool first = true;
  for (int i = 0; i < AUTO_MAX_RULES; i++) {
    AutoRule &r = g_auto.rules[i];
    if (!r.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    s += "{\"name\":\"" + String(r.name) + "\"";
    s += ",\"enabled\":" + String(r.enabled ? "true" : "false");
    s += ",\"trigger\":\"" + String(auto_trig_name(r.trigger.type)) + "\"";
    s += ",\"action\":\"" + String(auto_act_name(r.action.type)) + "\"";
    s += ",\"fire_count\":" + String(r.fire_count);
    s += ",\"last_fire\":" + String(r.last_fire_ts);
    s += ",\"cooldown_ms\":" + String(r.cooldown_ms);
    s += ",\"max_fires\":" + String(r.max_fires);
    s += "}";
  }
  s += "]}";
  return s;
}

static String auto_json_events() {
  int total = min((int)g_auto.event_head, AUTO_MAX_EVENTS);
  String s = "{\"events\":[";
  bool first = true;
  for (int i = 0; i < total; i++) {
    int idx = (g_auto.event_head - 1 - i + AUTO_MAX_EVENTS) % AUTO_MAX_EVENTS;
    AutoEvent &e = g_auto.events[idx];
    if (!first)
      s += ",";
    first = false;
    s += "{\"ts\":" + String(e.ts);
    s += ",\"rule\":\"" + String(e.rule_name) + "\"";
    s += ",\"detail\":\"" + String(e.detail) + "\"";
    s += ",\"trig\":" + String(e.trig_type);
    s += ",\"act\":" + String(e.act_type);
    s += "}";
  }
  s += "]}";
  return s;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void auto_routes() {
  server.on("/api/auto/status", HTTP_GET,
            []() { server.send(200, "application/json", auto_json_rules()); });

  server.on("/api/auto/start", HTTP_POST, []() {
    auto_start();
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/auto/stop", HTTP_POST, []() {
    auto_stop();
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/auto/events", HTTP_GET,
            []() { server.send(200, "application/json", auto_json_events()); });

  server.on("/api/auto/rule/add", HTTP_POST, []() {
    int idx = auto_alloc_rule();
    if (idx < 0) {
      server.send(400, "application/json", "{\"error\":\"rule table full\"}");
      return;
    }
    AutoRule &r = g_auto.rules[idx];
    memset(&r, 0, sizeof(r));
    r.active = true;
    r.enabled = true;
    r.max_fires = -1;
    r.cooldown_ms = 1000;

    if (server.hasArg("name"))
      strncpy(r.name, server.arg("name").c_str(), AUTO_STR_LEN - 1);
    if (server.hasArg("trigger"))
      r.trigger.type = (AutoTriggerType)server.arg("trigger").toInt();
    if (server.hasArg("action"))
      r.action.type = (AutoActionType)server.arg("action").toInt();
    if (server.hasArg("trig_int"))
      r.trigger.int_param = server.arg("trig_int").toInt();
    if (server.hasArg("trig_str"))
      strncpy(r.trigger.str_param, server.arg("trig_str").c_str(), AUTO_STR_LEN - 1);
    if (server.hasArg("act_int"))
      r.action.int_param = server.arg("act_int").toInt();
    if (server.hasArg("act_str"))
      strncpy(r.action.str_param, server.arg("act_str").c_str(), AUTO_STR_LEN - 1);
    if (server.hasArg("act_str2"))
      strncpy(r.action.str_param2, server.arg("act_str2").c_str(), AUTO_STR_LEN - 1);
    if (server.hasArg("cooldown"))
      r.cooldown_ms = server.arg("cooldown").toInt();
    if (server.hasArg("max_fires"))
      r.max_fires = server.arg("max_fires").toInt();

    server.send(200, "application/json", "{\"ok\":true,\"idx\":" + String(idx) + "}");
  });

  server.on("/api/auto/rule/delete", HTTP_POST, []() {
    if (!server.hasArg("name")) {
      server.send(400, "application/json", "{\"error\":\"missing name\"}");
      return;
    }
    AutoRule *r = auto_find_rule(server.arg("name").c_str());
    if (!r) {
      server.send(404, "application/json", "{\"error\":\"not found\"}");
      return;
    }
    r->active = false;
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/auto/rule/enable", HTTP_POST, []() {
    if (!server.hasArg("name")) {
      server.send(400, "application/json", "{\"error\":\"missing name\"}");
      return;
    }
    AutoRule *r = auto_find_rule(server.arg("name").c_str());
    if (!r) {
      server.send(404, "application/json", "{\"error\":\"not found\"}");
      return;
    }
    r->enabled = true;
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/auto/rule/disable", HTTP_POST, []() {
    if (!server.hasArg("name")) {
      server.send(400, "application/json", "{\"error\":\"missing name\"}");
      return;
    }
    AutoRule *r = auto_find_rule(server.arg("name").c_str());
    if (!r) {
      server.send(404, "application/json", "{\"error\":\"not found\"}");
      return;
    }
    r->enabled = false;
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/auto/fire", HTTP_POST, []() {
    if (!server.hasArg("name")) {
      server.send(400, "application/json", "{\"error\":\"missing name\"}");
      return;
    }
    AutoRule *r = auto_find_rule(server.arg("name").c_str());
    if (!r) {
      server.send(404, "application/json", "{\"error\":\"not found\"}");
      return;
    }
    auto_exec_action(*r, 0, "manual");
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/auto/vars", HTTP_GET, []() {
    String s = "{\"vars\":{";
    for (int i = 0; i < g_auto.var_count; i++) {
      if (i)
        s += ",";
      s += "\"" + String(g_auto.var_keys[i]) + "\":\"" + String(g_auto.var_vals[i]) + "\"";
    }
    s += "}}";
    server.send(200, "application/json", s);
  });

  server.on("/api/auto/vars/set", HTTP_POST, []() {
    if (!server.hasArg("key") || !server.hasArg("val")) {
      server.send(400, "application/json", "{\"error\":\"missing key or val\"}");
      return;
    }
    auto_var_set(server.arg("key").c_str(), server.arg("val").c_str());
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/auto/clear_events", HTTP_POST, []() {
    g_auto.event_head = 0;
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

#pragma once
// frame_filter.h — Programmable 802.11 frame filter engine.
// Supports MAC-based, channel-based, SSID-based, and type/subtype filters.
// Prefix: ff_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define FF_MAX_RULES    32
#define FF_MAX_SSID_LEN 33

enum FfAction {
  FF_ACTION_PASS   = 0,
  FF_ACTION_DROP   = 1,
  FF_ACTION_ALERT  = 2,
  FF_ACTION_LOG    = 3,
};

enum FfMatchType {
  FF_MATCH_ANY         = 0,
  FF_MATCH_SRC_MAC     = 1,
  FF_MATCH_DST_MAC     = 2,
  FF_MATCH_BSSID_MAC   = 3,
  FF_MATCH_CHANNEL     = 4,
  FF_MATCH_FRAME_TYPE  = 5,
  FF_MATCH_FRAME_SUBTYPE = 6,
  FF_MATCH_SSID_PREFIX = 7,
  FF_MATCH_SSID_EXACT  = 8,
  FF_MATCH_RSSI_ABOVE  = 9,
  FF_MATCH_RSSI_BELOW  = 10,
};

struct FfRule {
  bool     active;
  uint8_t  priority;      // 0-255, lower = higher priority
  FfMatchType match_type;
  FfAction action;

  // Match parameters (union-like, field used depends on match_type)
  uint8_t  mac[6];
  uint8_t  channel;
  uint8_t  frame_type;
  uint8_t  frame_subtype;
  char     ssid[FF_MAX_SSID_LEN];
  int8_t   rssi_threshold;

  // Stats
  uint32_t match_count;
  uint32_t last_match_ts;
  char     name[24];
};

static struct {
  FfRule   rules[FF_MAX_RULES];
  uint8_t  rule_count;

  uint32_t total_pass;
  uint32_t total_drop;
  uint32_t total_alert;
  uint32_t total_log;
  uint32_t total_frames;
} g_ff = {};

static bool ff_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static bool ff_mac_match(const uint8_t *a, const uint8_t *b) {
  // support broadcast (all FF) as wildcard
  static const uint8_t broadcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
  if (ff_mac_eq(b, broadcast)) return true;
  return ff_mac_eq(a, b);
}

// ─── Evaluate a rule against a frame ─────────────────────────────────────────
static bool ff_rule_matches(const FfRule &r,
                            const uint8_t *src, const uint8_t *dst,
                            const uint8_t *bssid,
                            uint8_t type, uint8_t subtype,
                            uint8_t channel, int8_t rssi,
                            const char *ssid) {
  switch (r.match_type) {
    case FF_MATCH_ANY:          return true;
    case FF_MATCH_SRC_MAC:      return ff_mac_match(src, r.mac);
    case FF_MATCH_DST_MAC:      return ff_mac_match(dst, r.mac);
    case FF_MATCH_BSSID_MAC:    return ff_mac_match(bssid, r.mac);
    case FF_MATCH_CHANNEL:      return channel == r.channel;
    case FF_MATCH_FRAME_TYPE:   return type == r.frame_type;
    case FF_MATCH_FRAME_SUBTYPE:return type == r.frame_type && subtype == r.frame_subtype;
    case FF_MATCH_SSID_PREFIX:
      if (!ssid) return false;
      return strncmp(ssid, r.ssid, strlen(r.ssid)) == 0;
    case FF_MATCH_SSID_EXACT:
      if (!ssid) return false;
      return strcmp(ssid, r.ssid) == 0;
    case FF_MATCH_RSSI_ABOVE:   return rssi > r.rssi_threshold;
    case FF_MATCH_RSSI_BELOW:   return rssi < r.rssi_threshold;
  }
  return false;
}

// ─── Evaluate all rules for a frame — returns action ────────────────────────
FfAction ff_evaluate(const uint8_t *src, const uint8_t *dst,
                     const uint8_t *bssid,
                     uint8_t type, uint8_t subtype,
                     uint8_t channel, int8_t rssi,
                     const char *ssid) {
  g_ff.total_frames++;

  // Sort-by-priority at eval time (simple linear scan, good enough for 32 rules)
  uint8_t  best_prio  = 0xFF;
  int      best_idx   = -1;
  for (int i = 0; i < FF_MAX_RULES; i++) {
    if (!g_ff.rules[i].active) continue;
    if (g_ff.rules[i].priority < best_prio) {
      if (ff_rule_matches(g_ff.rules[i], src, dst, bssid,
                          type, subtype, channel, rssi, ssid)) {
        best_prio = g_ff.rules[i].priority;
        best_idx  = i;
      }
    }
  }

  FfAction action = FF_ACTION_PASS;  // default
  if (best_idx >= 0) {
    action = g_ff.rules[best_idx].action;
    g_ff.rules[best_idx].match_count++;
    g_ff.rules[best_idx].last_match_ts = millis()/1000;
  }

  switch (action) {
    case FF_ACTION_PASS:  g_ff.total_pass++;  break;
    case FF_ACTION_DROP:  g_ff.total_drop++;  break;
    case FF_ACTION_ALERT: g_ff.total_alert++; break;
    case FF_ACTION_LOG:   g_ff.total_log++;   break;
  }
  return action;
}

// ─── Rule management ──────────────────────────────────────────────────────────
int ff_add_rule(FfMatchType match, FfAction action,
                const char *name, uint8_t priority = 128) {
  for (int i = 0; i < FF_MAX_RULES; i++) {
    if (!g_ff.rules[i].active) {
      FfRule &r = g_ff.rules[i];
      memset(&r, 0, sizeof(r));
      r.active    = true;
      r.match_type = match;
      r.action    = action;
      r.priority  = priority;
      strncpy(r.name, name ? name : "", 23);
      g_ff.rule_count++;
      return i;
    }
  }
  return -1; // full
}

void ff_remove_rule(int idx) {
  if (idx < 0 || idx >= FF_MAX_RULES) return;
  if (!g_ff.rules[idx].active) return;
  memset(&g_ff.rules[idx], 0, sizeof(g_ff.rules[0]));
  if (g_ff.rule_count) g_ff.rule_count--;
}

// ─── JSON ─────────────────────────────────────────────────────────────────────
static const char* ff_action_name(FfAction a) {
  switch(a) {
    case FF_ACTION_PASS:  return "pass";
    case FF_ACTION_DROP:  return "drop";
    case FF_ACTION_ALERT: return "alert";
    case FF_ACTION_LOG:   return "log";
  }
  return "?";
}

static const char* ff_match_name(FfMatchType m) {
  switch(m) {
    case FF_MATCH_ANY:          return "any";
    case FF_MATCH_SRC_MAC:      return "src_mac";
    case FF_MATCH_DST_MAC:      return "dst_mac";
    case FF_MATCH_BSSID_MAC:    return "bssid";
    case FF_MATCH_CHANNEL:      return "channel";
    case FF_MATCH_FRAME_TYPE:   return "type";
    case FF_MATCH_FRAME_SUBTYPE:return "subtype";
    case FF_MATCH_SSID_PREFIX:  return "ssid_prefix";
    case FF_MATCH_SSID_EXACT:   return "ssid_exact";
    case FF_MATCH_RSSI_ABOVE:   return "rssi_above";
    case FF_MATCH_RSSI_BELOW:   return "rssi_below";
  }
  return "?";
}

static String ff_json_rules() {
  String s = "{\"rule_count\":" + String(g_ff.rule_count);
  s += ",\"total_frames\":" + String(g_ff.total_frames);
  s += ",\"pass\":" + String(g_ff.total_pass);
  s += ",\"drop\":" + String(g_ff.total_drop);
  s += ",\"alert\":" + String(g_ff.total_alert);
  s += ",\"log\":" + String(g_ff.total_log);
  s += ",\"rules\":[";
  bool first = true;
  for (int i = 0; i < FF_MAX_RULES; i++) {
    FfRule &r = g_ff.rules[i];
    if (!r.active) continue;
    if (!first) s += ",";
    first = false;
    s += "{\"id\":" + String(i);
    s += ",\"name\":\"" + String(r.name) + "\"";
    s += ",\"match\":\"" + String(ff_match_name(r.match_type)) + "\"";
    s += ",\"action\":\"" + String(ff_action_name(r.action)) + "\"";
    s += ",\"prio\":" + String(r.priority);
    s += ",\"hits\":" + String(r.match_count);
    s += "}";
  }
  s += "]}";
  return s;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void ff_routes() {
  server.on("/api/ff/rules", HTTP_GET, []() {
    server.send(200, "application/json", ff_json_rules());
  });

  server.on("/api/ff/add", HTTP_POST, []() {
    if (!server.hasArg("match") || !server.hasArg("action")) {
      server.send(400, "application/json", "{\"error\":\"missing params\"}");
      return;
    }
    FfMatchType match = FF_MATCH_ANY;
    String ms = server.arg("match");
    if      (ms == "src_mac")   match = FF_MATCH_SRC_MAC;
    else if (ms == "dst_mac")   match = FF_MATCH_DST_MAC;
    else if (ms == "bssid")     match = FF_MATCH_BSSID_MAC;
    else if (ms == "channel")   match = FF_MATCH_CHANNEL;
    else if (ms == "type")      match = FF_MATCH_FRAME_TYPE;
    else if (ms == "subtype")   match = FF_MATCH_FRAME_SUBTYPE;
    else if (ms == "ssid_prefix") match = FF_MATCH_SSID_PREFIX;
    else if (ms == "ssid_exact")  match = FF_MATCH_SSID_EXACT;
    else if (ms == "rssi_above")  match = FF_MATCH_RSSI_ABOVE;
    else if (ms == "rssi_below")  match = FF_MATCH_RSSI_BELOW;

    FfAction action = FF_ACTION_PASS;
    String as = server.arg("action");
    if      (as == "drop")  action = FF_ACTION_DROP;
    else if (as == "alert") action = FF_ACTION_ALERT;
    else if (as == "log")   action = FF_ACTION_LOG;

    uint8_t prio = 128;
    if (server.hasArg("prio")) prio = (uint8_t)server.arg("prio").toInt();

    String name = server.hasArg("name") ? server.arg("name") : "rule";
    int idx = ff_add_rule(match, action, name.c_str(), prio);

    if (idx >= 0) {
      FfRule &r = g_ff.rules[idx];
      if (server.hasArg("channel"))   r.channel   = server.arg("channel").toInt();
      if (server.hasArg("type"))      r.frame_type = server.arg("type").toInt();
      if (server.hasArg("subtype"))   r.frame_subtype = server.arg("subtype").toInt();
      if (server.hasArg("rssi"))      r.rssi_threshold = server.arg("rssi").toInt();
      if (server.hasArg("ssid"))      strncpy(r.ssid, server.arg("ssid").c_str(), 32);
      if (server.hasArg("mac")) {
        int m[6];
        sscanf(server.arg("mac").c_str(), "%x:%x:%x:%x:%x:%x",
               &m[0],&m[1],&m[2],&m[3],&m[4],&m[5]);
        for (int i = 0; i < 6; i++) r.mac[i] = m[i];
      }
      server.send(200, "application/json", "{\"ok\":true,\"id\":" + String(idx) + "}");
    } else {
      server.send(500, "application/json", "{\"error\":\"table full\"}");
    }
  });

  server.on("/api/ff/remove", HTTP_POST, []() {
    if (!server.hasArg("id")) { server.send(400, "application/json", "{\"error\":\"missing id\"}"); return; }
    ff_remove_rule(server.arg("id").toInt());
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/ff/clear", HTTP_POST, []() {
    memset(&g_ff, 0, sizeof(g_ff));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

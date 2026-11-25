#pragma once
// rate_limiter.h — Per-source rate limiting and traffic shaping analysis.
// Token-bucket per MAC, sliding-window counters, anomaly detection for
// unusually high probe/beacon/data rates. Prefix: rl_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define RL_MAX_BUCKETS 64
#define RL_WINDOW_SIZE_MS 5000  // 5-second window
#define RL_WINDOW_SLOTS 5       // 1 slot per second
#define RL_ALERT_QUEUE 32

// ─── Rate limit policy ────────────────────────────────────────────────────────
enum RlPolicy {
  RL_POLICY_NONE = 0,
  RL_POLICY_PROBE_FLOOD,   // too many probe requests from one source
  RL_POLICY_BEACON_FLOOD,  // beacon rate anomaly
  RL_POLICY_DEAUTH_FLOOD,  // deauth storm
  RL_POLICY_DATA_FLOOD,    // data burst
  RL_POLICY_MGMT_FLOOD,    // general management frame flood
};

// ─── Token bucket per MAC ─────────────────────────────────────────────────────
struct RlBucket {
  bool active;
  uint8_t mac[6];
  RlPolicy triggered_policy;

  // Token-bucket state
  float tokens;
  float capacity;     // max burst
  float refill_rate;  // tokens/ms
  uint32_t last_refill_ms;

  // Sliding window per frame type
  uint32_t window_frames[RL_WINDOW_SLOTS];  // frames per second
  uint8_t window_slot;
  uint32_t window_slot_ts;
  uint32_t window_total;  // sum of last 5 slots

  // Statistics
  uint32_t total_frames;
  uint32_t total_dropped;  // frames that exceeded limit
  uint32_t total_alerts;
  uint32_t first_seen;
  uint32_t last_seen;
  int8_t rssi;

  // Per-type counters
  uint32_t probe_count;
  uint32_t beacon_count;
  uint32_t deauth_count;
  uint32_t data_count;
  uint32_t mgmt_count;
};

struct RlAlert {
  uint32_t ts;
  uint8_t mac[6];
  RlPolicy policy;
  uint32_t rate;  // frames/s when alert triggered
  uint8_t channel;
};

// ─── Thresholds ───────────────────────────────────────────────────────────────
struct RlThreshold {
  uint32_t probe_per_5s;   // default 50
  uint32_t beacon_per_5s;  // default 100
  uint32_t deauth_per_5s;  // default 10
  uint32_t data_per_5s;    // default 500
  uint32_t mgmt_per_5s;    // default 200
};

// ─── State ────────────────────────────────────────────────────────────────────
static struct {
  RlBucket buckets[RL_MAX_BUCKETS];
  RlAlert alerts[RL_ALERT_QUEUE];
  uint8_t alert_head;
  uint32_t total_alerts;
  RlThreshold thresh;
  bool enabled;
} g_rl = {};

// ─── Init ─────────────────────────────────────────────────────────────────────
inline void rl_init() {
  memset(&g_rl, 0, sizeof(g_rl));
  g_rl.thresh.probe_per_5s = 50;
  g_rl.thresh.beacon_per_5s = 100;
  g_rl.thresh.deauth_per_5s = 10;
  g_rl.thresh.data_per_5s = 500;
  g_rl.thresh.mgmt_per_5s = 200;
  g_rl.enabled = true;
}

// ─── Helpers ─────────────────────────────────────────────────────────────────
static bool rl_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static RlBucket *rl_find(const uint8_t *mac) {
  for (int i = 0; i < RL_MAX_BUCKETS; i++) {
    if (g_rl.buckets[i].active && rl_mac_eq(g_rl.buckets[i].mac, mac))
      return &g_rl.buckets[i];
  }
  return nullptr;
}

static RlBucket *rl_alloc(const uint8_t *mac) {
  for (int i = 0; i < RL_MAX_BUCKETS; i++) {
    if (!g_rl.buckets[i].active) {
      RlBucket &b = g_rl.buckets[i];
      memset(&b, 0, sizeof(b));
      memcpy(b.mac, mac, 6);
      b.active = true;
      b.capacity = 20.0f;
      b.tokens = 20.0f;
      b.refill_rate = 0.004f;  // 4 tokens/second
      b.last_refill_ms = millis();
      b.first_seen = millis() / 1000;
      b.window_slot_ts = millis();
      return &b;
    }
  }
  // Evict oldest
  uint32_t oldest = 0xFFFFFFFF;
  int idx = 0;
  for (int i = 0; i < RL_MAX_BUCKETS; i++) {
    if (g_rl.buckets[i].last_seen < oldest) {
      oldest = g_rl.buckets[i].last_seen;
      idx = i;
    }
  }
  memset(&g_rl.buckets[idx], 0, sizeof(g_rl.buckets[idx]));
  memcpy(g_rl.buckets[idx].mac, mac, 6);
  g_rl.buckets[idx].active = true;
  g_rl.buckets[idx].capacity = 20.0f;
  g_rl.buckets[idx].tokens = 20.0f;
  g_rl.buckets[idx].refill_rate = 0.004f;
  g_rl.buckets[idx].last_refill_ms = millis();
  g_rl.buckets[idx].first_seen = millis() / 1000;
  g_rl.buckets[idx].window_slot_ts = millis();
  return &g_rl.buckets[idx];
}

static void rl_push_alert(const uint8_t *mac, RlPolicy policy, uint32_t rate, uint8_t ch) {
  RlAlert &a = g_rl.alerts[g_rl.alert_head % RL_ALERT_QUEUE];
  a.ts = millis() / 1000;
  memcpy(a.mac, mac, 6);
  a.policy = policy;
  a.rate = rate;
  a.channel = ch;
  g_rl.alert_head++;
  g_rl.total_alerts++;
}

// ─── Ingest ───────────────────────────────────────────────────────────────────
// frame_type: 0=mgmt, 1=ctrl, 2=data; frame_sub: subtype
// Returns true if frame is within rate limits (not anomalous)
bool rl_ingest(const uint8_t *src_mac, uint8_t frame_type, uint8_t frame_sub, uint8_t channel,
               int8_t rssi) {
  if (!g_rl.enabled)
    return true;

  RlBucket *b = rl_find(src_mac);
  if (!b)
    b = rl_alloc(src_mac);
  if (!b)
    return true;

  uint32_t now_ms = millis();
  b->last_seen = now_ms / 1000;
  b->rssi = rssi;
  b->total_frames++;

  // Refill tokens
  uint32_t elapsed = now_ms - b->last_refill_ms;
  b->tokens = min(b->capacity, b->tokens + b->refill_rate * elapsed);
  b->last_refill_ms = now_ms;

  // Advance sliding window slot
  uint32_t slot_elapsed = now_ms - b->window_slot_ts;
  if (slot_elapsed >= 1000) {
    b->window_slot = (b->window_slot + 1) % RL_WINDOW_SLOTS;
    b->window_total -= b->window_frames[b->window_slot];
    b->window_frames[b->window_slot] = 0;
    b->window_slot_ts = now_ms;
  }
  b->window_frames[b->window_slot]++;
  b->window_total++;

  // Per-type counters
  bool is_probe = (frame_type == 0 && frame_sub == 4);
  bool is_beacon = (frame_type == 0 && frame_sub == 8);
  bool is_deauth = (frame_type == 0 && (frame_sub == 12 || frame_sub == 10));
  bool is_data = (frame_type == 2);

  if (is_probe)
    b->probe_count++;
  if (is_beacon)
    b->beacon_count++;
  if (is_deauth)
    b->deauth_count++;
  if (is_data)
    b->data_count++;
  if (frame_type == 0)
    b->mgmt_count++;

  // Check thresholds
  RlPolicy alert_policy = RL_POLICY_NONE;
  if (is_probe && b->probe_count > g_rl.thresh.probe_per_5s)
    alert_policy = RL_POLICY_PROBE_FLOOD;
  if (is_beacon && b->beacon_count > g_rl.thresh.beacon_per_5s)
    alert_policy = RL_POLICY_BEACON_FLOOD;
  if (is_deauth && b->deauth_count > g_rl.thresh.deauth_per_5s)
    alert_policy = RL_POLICY_DEAUTH_FLOOD;
  if (is_data && b->data_count > g_rl.thresh.data_per_5s)
    alert_policy = RL_POLICY_DATA_FLOOD;
  if (frame_type == 0 && b->mgmt_count > g_rl.thresh.mgmt_per_5s)
    alert_policy = RL_POLICY_MGMT_FLOOD;

  if (alert_policy != RL_POLICY_NONE && b->triggered_policy != alert_policy) {
    b->triggered_policy = alert_policy;
    b->total_alerts++;
    rl_push_alert(src_mac, alert_policy, b->window_total, channel);
  }

  // Token bucket consume
  if (b->tokens >= 1.0f) {
    b->tokens -= 1.0f;
    return true;
  }
  b->total_dropped++;
  return false;  // over limit
}

// ─── Reset per-bucket period counters (call every 5s) ────────────────────────
inline void rl_tick_5s() {
  for (int i = 0; i < RL_MAX_BUCKETS; i++) {
    if (!g_rl.buckets[i].active)
      continue;
    g_rl.buckets[i].probe_count = 0;
    g_rl.buckets[i].beacon_count = 0;
    g_rl.buckets[i].deauth_count = 0;
    g_rl.buckets[i].data_count = 0;
    g_rl.buckets[i].mgmt_count = 0;
    g_rl.buckets[i].triggered_policy = RL_POLICY_NONE;
  }
}

// ─── JSON ─────────────────────────────────────────────────────────────────────
static const char *rl_policy_str(RlPolicy p) {
  switch (p) {
    case RL_POLICY_PROBE_FLOOD:
      return "probe_flood";
    case RL_POLICY_BEACON_FLOOD:
      return "beacon_flood";
    case RL_POLICY_DEAUTH_FLOOD:
      return "deauth_flood";
    case RL_POLICY_DATA_FLOOD:
      return "data_flood";
    case RL_POLICY_MGMT_FLOOD:
      return "mgmt_flood";
    default:
      return "none";
  }
}

static void rl_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String rl_json_status() {
  int cnt = 0;
  for (int i = 0; i < RL_MAX_BUCKETS; i++)
    if (g_rl.buckets[i].active)
      cnt++;

  String s = "{\"enabled\":" + String(g_rl.enabled ? "true" : "false");
  s += ",\"total_alerts\":" + String(g_rl.total_alerts);
  s += ",\"active_buckets\":" + String(cnt);
  s += ",\"thresholds\":{";
  s += "\"probe\":" + String(g_rl.thresh.probe_per_5s);
  s += ",\"beacon\":" + String(g_rl.thresh.beacon_per_5s);
  s += ",\"deauth\":" + String(g_rl.thresh.deauth_per_5s);
  s += ",\"data\":" + String(g_rl.thresh.data_per_5s);
  s += ",\"mgmt\":" + String(g_rl.thresh.mgmt_per_5s);
  s += "},\"alerts\":[";
  char mac[18];
  int alert_total = min((int)g_rl.total_alerts, RL_ALERT_QUEUE);
  for (int i = 0; i < alert_total; i++) {
    int idx = ((int)g_rl.alert_head - 1 - i + RL_ALERT_QUEUE) % RL_ALERT_QUEUE;
    RlAlert &a = g_rl.alerts[idx];
    if (a.ts == 0)
      continue;
    if (i)
      s += ",";
    rl_mac_str(a.mac, mac);
    s += "{\"ts\":" + String(a.ts);
    s += ",\"mac\":\"" + String(mac) + "\"";
    s += ",\"policy\":\"" + String(rl_policy_str(a.policy)) + "\"";
    s += ",\"rate\":" + String(a.rate);
    s += ",\"ch\":" + String(a.channel);
    s += "}";
  }
  s += "],\"top_buckets\":[";
  // Sort by total_frames, top 10
  struct {
    RlBucket *b;
  } sorted[RL_MAX_BUCKETS];
  int sc = 0;
  for (int i = 0; i < RL_MAX_BUCKETS; i++)
    if (g_rl.buckets[i].active)
      sorted[sc++] = {&g_rl.buckets[i]};
  for (int i = 0; i < min(sc, 10); i++) {
    for (int j = i + 1; j < sc; j++) {
      if (sorted[j].b->total_frames > sorted[i].b->total_frames) {
        auto tmp = sorted[i];
        sorted[i] = sorted[j];
        sorted[j] = tmp;
      }
    }
  }
  for (int i = 0; i < min(sc, 10); i++) {
    if (i)
      s += ",";
    RlBucket &b = *sorted[i].b;
    rl_mac_str(b.mac, mac);
    s += "{\"mac\":\"" + String(mac) + "\"";
    s += ",\"total\":" + String(b.total_frames);
    s += ",\"dropped\":" + String(b.total_dropped);
    s += ",\"alerts\":" + String(b.total_alerts);
    s += ",\"policy\":\"" + String(rl_policy_str(b.triggered_policy)) + "\"";
    s += ",\"rate\":" + String(b.window_total / RL_WINDOW_SLOTS);
    s += "}";
  }
  s += "]}";
  return s;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void rl_routes() {
  server.on("/api/rl/status", HTTP_GET,
            []() { server.send(200, "application/json", rl_json_status()); });

  server.on("/api/rl/enable", HTTP_POST, []() {
    g_rl.enabled = true;
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/rl/disable", HTTP_POST, []() {
    g_rl.enabled = false;
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/rl/clear", HTTP_POST, []() {
    bool en = g_rl.enabled;
    RlThreshold th = g_rl.thresh;
    memset(&g_rl, 0, sizeof(g_rl));
    g_rl.enabled = en;
    g_rl.thresh = th;
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/rl/threshold", HTTP_POST, []() {
    if (server.hasArg("probe"))
      g_rl.thresh.probe_per_5s = server.arg("probe").toInt();
    if (server.hasArg("beacon"))
      g_rl.thresh.beacon_per_5s = server.arg("beacon").toInt();
    if (server.hasArg("deauth"))
      g_rl.thresh.deauth_per_5s = server.arg("deauth").toInt();
    if (server.hasArg("data"))
      g_rl.thresh.data_per_5s = server.arg("data").toInt();
    if (server.hasArg("mgmt"))
      g_rl.thresh.mgmt_per_5s = server.arg("mgmt").toInt();
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

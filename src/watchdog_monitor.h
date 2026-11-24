#pragma once
// watchdog_monitor.h — System health, heap, stack, and task watchdog.
// Monitors ESP32 resources and alerts on anomalies. Prefix: wdg_

#include <Arduino.h>
#include <WebServer.h>
#include <esp_system.h>
#include <esp_heap_caps.h>

extern WebServer server;

#define WDG_HIST_LEN      60   // 60 second rolling history
#define WDG_TASK_MAX      16   // max tasks to track
#define WDG_ALERT_MAX     32   // max alert log entries

// ─── Alert levels ─────────────────────────────────────────────────────────────
enum WdgLevel { WDG_OK=0, WDG_WARN=1, WDG_CRIT=2 };

// ─── Alert entry ─────────────────────────────────────────────────────────────
struct WdgAlert {
  bool     used;
  uint32_t ts;
  WdgLevel level;
  char     msg[64];
};

// ─── Task snapshot ────────────────────────────────────────────────────────────
struct WdgTask {
  bool    active;
  char    name[16];
  uint32_t stack_hwm;    // high water mark (bytes remaining)
  UBaseType_t priority;
  eTaskState state;
  uint32_t last_seen;
};

// ─── State ────────────────────────────────────────────────────────────────────
static struct {
  // Heap history (free bytes, sampled every second)
  uint32_t heap_hist[WDG_HIST_LEN];
  uint8_t  heap_idx;
  uint32_t heap_min_ever;

  // CPU approximate (via tick counter delta)
  uint32_t cpu_ticks_hist[WDG_HIST_LEN];
  uint8_t  cpu_idx;

  // Flash usage
  uint32_t fs_total;
  uint32_t fs_used;

  // Uptime
  uint32_t boot_ts;

  // Task info
  WdgTask  tasks[WDG_TASK_MAX];
  uint8_t  task_count;

  // Alerts
  WdgAlert alerts[WDG_ALERT_MAX];
  uint8_t  alert_head;
  uint32_t total_alerts;

  // Thresholds
  uint32_t heap_warn_bytes;    // warn if free heap below
  uint32_t heap_crit_bytes;    // critical below
  uint32_t stack_warn_bytes;   // warn if task stack hwm below

  // Restart tracking
  uint32_t restart_count;
  esp_reset_reason_t last_reset_reason;

  TaskHandle_t monitor_task;
  bool running;
} g_wdg = {
  .heap_min_ever = UINT32_MAX,
  .heap_warn_bytes = 20000,
  .heap_crit_bytes = 8000,
  .stack_warn_bytes = 256
};

// ─── Log alert ────────────────────────────────────────────────────────────────
static void wdg_alert(WdgLevel level, const char *fmt, ...) {
  WdgAlert &a = g_wdg.alerts[g_wdg.alert_head % WDG_ALERT_MAX];
  a.used = true;
  a.ts = millis() / 1000;
  a.level = level;
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(a.msg, sizeof(a.msg), fmt, ap);
  va_end(ap);
  g_wdg.alert_head++;
  g_wdg.total_alerts++;
}

// ─── Sample heap ─────────────────────────────────────────────────────────────
static void wdg_sample_heap() {
  uint32_t free_heap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
  g_wdg.heap_hist[g_wdg.heap_idx % WDG_HIST_LEN] = free_heap;
  g_wdg.heap_idx++;

  if (free_heap < g_wdg.heap_min_ever) g_wdg.heap_min_ever = free_heap;

  if (free_heap < g_wdg.heap_crit_bytes) {
    wdg_alert(WDG_CRIT, "Heap critical: %u bytes free", free_heap);
  } else if (free_heap < g_wdg.heap_warn_bytes) {
    wdg_alert(WDG_WARN, "Heap low: %u bytes free", free_heap);
  }
}

// ─── Sample tasks ─────────────────────────────────────────────────────────────
static void wdg_sample_tasks() {
  TaskStatus_t status_arr[WDG_TASK_MAX];
  uint32_t total_run;
  int n = uxTaskGetSystemState(status_arr, WDG_TASK_MAX, &total_run);
  g_wdg.task_count = (uint8_t)min(n, WDG_TASK_MAX);

  for (int i = 0; i < g_wdg.task_count; i++) {
    WdgTask &t = g_wdg.tasks[i];
    t.active = true;
    strncpy(t.name, status_arr[i].pcTaskName, 15);
    t.stack_hwm = status_arr[i].usStackHighWaterMark * sizeof(StackType_t);
    t.priority = status_arr[i].uxCurrentPriority;
    t.state = status_arr[i].eCurrentState;
    t.last_seen = millis() / 1000;

    if (t.stack_hwm < g_wdg.stack_warn_bytes) {
      wdg_alert(WDG_WARN, "Task %s stack low: %u bytes", t.name, t.stack_hwm);
    }
  }
}

// ─── Sample filesystem ────────────────────────────────────────────────────────
static void wdg_sample_fs() {
  if (!LittleFS.begin()) return;
  g_wdg.fs_total = LittleFS.totalBytes();
  g_wdg.fs_used  = LittleFS.usedBytes();
}

// ─── Monitor task ─────────────────────────────────────────────────────────────
static void wdg_monitor_task(void *arg) {
  g_wdg.boot_ts = millis() / 1000;
  g_wdg.last_reset_reason = esp_reset_reason();

  while (g_wdg.running) {
    wdg_sample_heap();
    wdg_sample_tasks();

    static uint8_t fs_tick = 0;
    if (++fs_tick >= 10) { // Every 10 seconds
      wdg_sample_fs();
      fs_tick = 0;
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  g_wdg.monitor_task = nullptr;
  vTaskDelete(nullptr);
}

// ─── Start/stop ───────────────────────────────────────────────────────────────
inline void wdg_start() {
  if (g_wdg.running) return;
  g_wdg.running = true;
  xTaskCreate(wdg_monitor_task, "wdg_mon", 3072, nullptr, 2, &g_wdg.monitor_task);
}

inline void wdg_stop() {
  g_wdg.running = false;
}

// ─── Reset reason string ──────────────────────────────────────────────────────
static const char* wdg_reset_reason_str(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:  return "power_on";
    case ESP_RST_EXT:      return "external";
    case ESP_RST_SW:       return "software";
    case ESP_RST_PANIC:    return "panic";
    case ESP_RST_INT_WDT:  return "int_watchdog";
    case ESP_RST_TASK_WDT: return "task_watchdog";
    case ESP_RST_WDT:      return "watchdog";
    case ESP_RST_DEEPSLEEP: return "deep_sleep";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_SDIO:     return "sdio";
    default: return "unknown";
  }
}

static const char* wdg_task_state_str(eTaskState s) {
  switch (s) {
    case eRunning:   return "running";
    case eReady:     return "ready";
    case eBlocked:   return "blocked";
    case eSuspended: return "suspended";
    case eDeleted:   return "deleted";
    default:         return "unknown";
  }
}

// ─── JSON export ─────────────────────────────────────────────────────────────
static String wdg_json_status() {
  uint32_t uptime = millis() / 1000;
  uint32_t free_heap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
  uint32_t largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
  uint32_t total_heap = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);

  String s = "{\"uptime_s\":" + String(uptime);
  s += ",\"free_heap\":" + String(free_heap);
  s += ",\"heap_min_ever\":" + String(g_wdg.heap_min_ever);
  s += ",\"largest_block\":" + String(largest_block);
  s += ",\"total_heap\":" + String(total_heap);
  s += ",\"heap_pct\":" + String((int)(100.0f * free_heap / total_heap));
  s += ",\"fs_total\":" + String(g_wdg.fs_total);
  s += ",\"fs_used\":" + String(g_wdg.fs_used);
  s += ",\"task_count\":" + String(g_wdg.task_count);
  s += ",\"total_alerts\":" + String(g_wdg.total_alerts);
  s += ",\"reset_reason\":\"" + String(wdg_reset_reason_str(g_wdg.last_reset_reason)) + "\"";
  s += ",\"heap_hist\":[";
  for (int i = 0; i < WDG_HIST_LEN; i++) {
    if (i) s += ",";
    int idx = (g_wdg.heap_idx - WDG_HIST_LEN + i + WDG_HIST_LEN) % WDG_HIST_LEN;
    s += String(g_wdg.heap_hist[idx]);
  }
  s += "],\"tasks\":[";
  bool first = true;
  for (int i = 0; i < g_wdg.task_count; i++) {
    WdgTask &t = g_wdg.tasks[i];
    if (!t.active) continue;
    if (!first) s += ",";
    first = false;
    s += "{\"name\":\"" + String(t.name) + "\"";
    s += ",\"stack_hwm\":" + String(t.stack_hwm);
    s += ",\"prio\":" + String(t.priority);
    s += ",\"state\":\"" + String(wdg_task_state_str(t.state)) + "\"";
    s += "}";
  }
  s += "],\"alerts\":[";
  first = true;
  for (int i = 0; i < WDG_ALERT_MAX; i++) {
    int idx = (g_wdg.alert_head - WDG_ALERT_MAX + i + WDG_ALERT_MAX) % WDG_ALERT_MAX;
    WdgAlert &a = g_wdg.alerts[idx];
    if (!a.used) continue;
    if (!first) s += ",";
    first = false;
    s += "{\"ts\":" + String(a.ts);
    s += ",\"level\":" + String((int)a.level);
    s += ",\"msg\":\"" + String(a.msg) + "\"";
    s += "}";
  }
  s += "]}";
  return s;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void wdg_routes() {
  server.on("/api/wdg/status", HTTP_GET, []() {
    server.send(200, "application/json", wdg_json_status());
  });

  server.on("/api/wdg/start", HTTP_POST, []() {
    wdg_start();
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/wdg/stop", HTTP_POST, []() {
    wdg_stop();
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/wdg/clear_alerts", HTTP_POST, []() {
    memset(g_wdg.alerts, 0, sizeof(g_wdg.alerts));
    g_wdg.alert_head = 0;
    g_wdg.total_alerts = 0;
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/wdg/heap_hist", HTTP_GET, []() {
    String s = "[";
    for (int i = 0; i < WDG_HIST_LEN; i++) {
      if (i) s += ",";
      int idx = (g_wdg.heap_idx - WDG_HIST_LEN + i + WDG_HIST_LEN) % WDG_HIST_LEN;
      s += String(g_wdg.heap_hist[idx]);
    }
    s += "]";
    server.send(200, "application/json", s);
  });

  server.on("/api/wdg/reboot", HTTP_POST, []() {
    server.send(200, "application/json", "{\"ok\":true,\"msg\":\"rebooting\"}");
    delay(100);
    ESP.restart();
  });
}

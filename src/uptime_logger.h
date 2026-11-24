#pragma once
// uptime_logger.h — Firmware uptime and periodic system-event logger.
// Maintains a time-stamped ring log of system events (boot, channel change,
// promisc toggle, low-heap warnings, module starts/stops, etc.) and exposes
// them via /api/syslog and a simple /syslog web page for real-time viewing.
// Routes: /api/syslog, /api/syslog/clear, /syslog (UI). Prefix: ulg_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

#define ULG_MAX_EVENTS  128
#define ULG_MSG_LEN     80

enum UlgLevel : uint8_t {
  ULG_INFO  = 0,
  ULG_WARN  = 1,
  ULG_ERROR = 2,
  ULG_DEBUG = 3,
};

static const char* const ULG_LEVEL_STR[] = { "INFO","WARN","ERR","DBG" };

struct UlgEvent {
  uint32_t  ms;
  UlgLevel  level;
  char      msg[ULG_MSG_LEN];
};

static struct {
  UlgEvent  buf[ULG_MAX_EVENTS];
  uint16_t  head;
  uint32_t  total;
  uint32_t  boot_ms;
  uint32_t  per_level[4];
} g_ulg = {};

static inline void ulg_init() {
  g_ulg.boot_ms = millis();
  memset(g_ulg.per_level, 0, sizeof(g_ulg.per_level));
}

static inline void ulg_log(UlgLevel level, const char *fmt, ...) {
  UlgEvent &e = g_ulg.buf[g_ulg.head % ULG_MAX_EVENTS];
  e.ms    = millis();
  e.level = level;
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(e.msg, ULG_MSG_LEN, fmt, ap);
  va_end(ap);
  g_ulg.per_level[level]++;
  g_ulg.head = (g_ulg.head + 1) % ULG_MAX_EVENTS;
  g_ulg.total++;
  Serial.printf("[%s] %s\n", ULG_LEVEL_STR[level], e.msg);
}

#define ULG_INFO(...)  ulg_log(ULG_INFO,  __VA_ARGS__)
#define ULG_WARN(...)  ulg_log(ULG_WARN,  __VA_ARGS__)
#define ULG_ERROR(...) ulg_log(ULG_ERROR, __VA_ARGS__)
#define ULG_DEBUG(...) ulg_log(ULG_DEBUG, __VA_ARGS__)

static const char ULG_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>System Log — Phantom32</title>
<style>
:root{--bg:#0a0c10;--bg2:#12151c;--bg3:#1a1f2e;--border:#2a3045;
  --accent:#00d4ff;--success:#22c55e;--warn:#f59e0b;--danger:#ef4444;
  --text:#e2e8f0;--muted:#64748b;--radius:8px;}
*{box-sizing:border-box;margin:0;padding:0;}
body{font-family:'Inter',system-ui,sans-serif;background:var(--bg);color:var(--text);min-height:100vh;}
.top-bar{display:flex;align-items:center;gap:12px;padding:14px 20px;
  background:var(--bg2);border-bottom:1px solid var(--border);position:sticky;top:0;z-index:10;}
.logo{font-size:18px;font-weight:700;color:var(--accent);letter-spacing:2px;}
.nav-links{display:flex;gap:8px;margin-left:auto;flex-wrap:wrap;}
.nav-links a{color:var(--muted);text-decoration:none;font-size:12px;padding:5px 9px;border-radius:5px;}
.nav-links a:hover{color:var(--text);background:var(--bg3);}
.nav-links a.active{color:var(--accent);background:rgba(0,212,255,.08);}
.container{max-width:1000px;margin:0 auto;padding:20px 16px;}
h1{font-size:22px;font-weight:700;margin-bottom:4px;}
.subtitle{color:var(--muted);font-size:13px;margin-bottom:20px;}
.hero{display:grid;grid-template-columns:repeat(4,1fr);gap:10px;margin-bottom:16px;}
.stat{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:13px;}
.stat label{font-size:10px;color:var(--muted);text-transform:uppercase;}
.stat .val{font-size:22px;font-weight:800;color:var(--accent);margin-top:3px;}
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:18px;margin-bottom:14px;}
.section h3{font-size:12px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:14px;}
.log-line{display:flex;gap:8px;padding:5px 0;border-bottom:1px solid rgba(42,48,69,.5);font-size:10px;font-family:monospace;}
.log-line:last-child{border-bottom:none;}
.lts{color:var(--muted);min-width:65px;}
.llvl{min-width:34px;font-weight:700;}
.llvl-INFO{color:var(--accent);}
.llvl-WARN{color:var(--warn);}
.llvl-ERR{color:var(--danger);}
.llvl-DBG{color:var(--muted);}
.lmsg{color:var(--text);flex:1;word-break:break-all;}
.filter-row{display:flex;gap:8px;margin-bottom:12px;flex-wrap:wrap;}
.filter-sel{background:var(--bg3);border:1px solid var(--border);border-radius:5px;color:var(--text);padding:7px 10px;font-size:11px;outline:none;}
.filter-input{background:var(--bg3);border:1px solid var(--border);border-radius:5px;color:var(--text);padding:7px 12px;font-size:11px;outline:none;min-width:200px;}
.filter-input:focus,.filter-sel:focus{border-color:var(--accent);}
.btn{padding:7px 13px;border:none;border-radius:5px;font-size:11px;font-weight:600;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;margin-top:12px;align-items:center;}
.ts-label{font-size:9px;color:var(--muted);margin-left:auto;}
#log-box{max-height:500px;overflow-y:auto;}
@media(max-width:700px){.hero{grid-template-columns:repeat(2,1fr)}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/admin">Admin</a>
    <a href="/syslog" class="active">Syslog</a>
  </nav>
</div>
<div class="container">
  <h1>System Log</h1>
  <p class="subtitle">Ring-buffer of the last 128 firmware events: boot, channel changes, promiscuous mode, low-heap warnings, module starts, and errors. Streams live every 2 seconds.</p>

  <div class="hero">
    <div class="stat"><label>Total Events</label><div class="val" id="h-total">0</div></div>
    <div class="stat"><label>Warnings</label><div class="val" id="h-warn" style="color:var(--warn)">0</div></div>
    <div class="stat"><label>Errors</label><div class="val" id="h-err" style="color:var(--danger)">0</div></div>
    <div class="stat"><label>Uptime</label><div class="val" id="h-up" style="font-size:14px">—</div></div>
  </div>

  <div class="section">
    <h3>Events</h3>
    <div class="filter-row">
      <select class="filter-sel" id="filter-lvl" onchange="applyFilter()">
        <option value="">All Levels</option>
        <option value="INFO">INFO</option>
        <option value="WARN">WARN</option>
        <option value="ERR">ERR</option>
        <option value="DBG">DBG</option>
      </select>
      <input class="filter-input" id="filter-msg" placeholder="Filter message…" oninput="applyFilter()">
    </div>
    <div id="log-box"></div>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="clearLog()">Clear</button>
      <button class="btn btn-ghost" onclick="pauseToggle()" id="pause-btn">Pause</button>
      <span class="ts-label">Buf: <span id="buf-use">0</span>/128 | <span id="ts-time">—</span></span>
    </div>
  </div>
</div>

<script>
let allEvents = [], paused = false;

function reltime(ms) {
  const s = Math.floor(ms / 1000);
  const m = Math.floor(s / 60), sec = s % 60;
  if (s < 60) return s + 's';
  if (s < 3600) return m + 'm' + sec + 's';
  return Math.floor(s/3600) + 'h' + m%60 + 'm';
}

function renderLog(events) {
  const lvl_f = document.getElementById('filter-lvl').value;
  const msg_f = document.getElementById('filter-msg').value.toLowerCase();
  const rows = events.filter(e => {
    if (lvl_f && e.level !== lvl_f) return false;
    if (msg_f && !e.msg.toLowerCase().includes(msg_f)) return false;
    return true;
  }).slice().reverse();
  document.getElementById('log-box').innerHTML = rows.length
    ? rows.map(e => `<div class="log-line">
        <span class="lts">+${reltime(e.ms)}</span>
        <span class="llvl llvl-${e.level}">${e.level}</span>
        <span class="lmsg">${e.msg}</span>
      </div>`).join('')
    : '<div style="color:var(--muted);font-size:11px;text-align:center;padding:14px">No events matching filter.</div>';
}

function applyFilter() { renderLog(allEvents); }

function pauseToggle() {
  paused = !paused;
  document.getElementById('pause-btn').textContent = paused ? 'Resume' : 'Pause';
}

async function refresh() {
  if (paused) return;
  const r = await fetch('/api/syslog').then(x=>x.json()).catch(()=>({}));
  allEvents = r.events || [];
  document.getElementById('h-total').textContent = (r.total||0).toLocaleString();
  document.getElementById('h-warn').textContent  = (r.warns||0).toLocaleString();
  document.getElementById('h-err').textContent   = (r.errors||0).toLocaleString();
  document.getElementById('h-up').textContent    = reltime(r.uptime_ms||0);
  document.getElementById('buf-use').textContent = allEvents.length;
  document.getElementById('ts-time').textContent = new Date().toLocaleTimeString();
  renderLog(allEvents);
}

async function clearLog() {
  await fetch('/api/syslog/clear', {method:'POST'});
  allEvents = [];
  renderLog([]);
}

refresh();
setInterval(refresh, 2000);
</script>
</body>
</html>
)rawhtml";

inline void ulg_routes() {
  server.on("/syslog", HTTP_GET, []() {
    server.send_P(200, "text/html", ULG_PAGE);
  });

  server.on("/api/syslog", HTTP_GET, []() {
    uint16_t count = (g_ulg.total < ULG_MAX_EVENTS) ? g_ulg.total : ULG_MAX_EVENTS;
    uint16_t start = (g_ulg.head + ULG_MAX_EVENTS - count) % ULG_MAX_EVENTS;

    String out;
    out.reserve(4096);
    out += "{\"total\":";     out += g_ulg.total;
    out += ",\"warns\":";     out += g_ulg.per_level[ULG_WARN];
    out += ",\"errors\":";    out += g_ulg.per_level[ULG_ERROR];
    out += ",\"uptime_ms\":"; out += millis() - g_ulg.boot_ms;
    out += ",\"events\":[";
    for (uint16_t i = 0; i < count; i++) {
      const UlgEvent &e = g_ulg.buf[(start + i) % ULG_MAX_EVENTS];
      if (i > 0) out += ',';
      out += "{\"ms\":"; out += e.ms;
      out += ",\"level\":\""; out += ULG_LEVEL_STR[e.level]; out += '"';
      out += ",\"msg\":\"";
      // Escape quotes in message
      for (uint8_t c = 0; e.msg[c]; c++) {
        if (e.msg[c] == '"') out += '\\';
        out += e.msg[c];
      }
      out += "\"}";
    }
    out += "]}";
    server.send(200, "application/json", out);
  });

  server.on("/api/syslog/clear", HTTP_POST, []() {
    memset(&g_ulg, 0, sizeof(g_ulg));
    g_ulg.boot_ms = millis();
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

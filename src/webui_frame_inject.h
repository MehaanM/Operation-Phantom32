#pragma once
// webui_frame_inject.h — Frame injection activity log UI.
// Route: /frame-inject-log. Prefix: wufil_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUFIL_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Frame Inject Log — Phantom32</title>
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
.container{max-width:1100px;margin:0 auto;padding:20px 16px;}
h1{font-size:22px;font-weight:700;margin-bottom:4px;}
.subtitle{color:var(--muted);font-size:13px;margin-bottom:20px;}
.hero{display:grid;grid-template-columns:repeat(4,1fr);gap:10px;margin-bottom:16px;}
.stat{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:13px;}
.stat label{font-size:10px;color:var(--muted);text-transform:uppercase;}
.stat .val{font-size:22px;font-weight:800;color:var(--accent);margin-top:3px;}
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:18px;margin-bottom:14px;}
.section h3{font-size:12px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:14px;}
.type-bars{display:flex;flex-direction:column;gap:4px;margin-bottom:16px;}
.type-row{display:flex;align-items:center;gap:8px;font-size:11px;}
.type-label{width:90px;color:var(--muted);text-align:right;}
.type-bar-wrap{flex:1;height:8px;background:var(--border);border-radius:4px;overflow:hidden;}
.type-bar-fill{height:100%;border-radius:4px;background:var(--accent);}
.type-count{width:50px;text-align:right;font-size:10px;color:var(--muted);}
table{width:100%;border-collapse:collapse;font-size:11px;}
th{text-align:left;padding:7px 8px;font-size:9px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;}
td{padding:6px 8px;border-bottom:1px solid var(--border);font-size:11px;}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
tr.fail td{opacity:.6;}
.badge{padding:1px 5px;border-radius:3px;font-size:9px;font-weight:700;display:inline-block;}
.badge-ok{background:rgba(34,197,94,.12);color:var(--success);}
.badge-fail{background:rgba(239,68,68,.12);color:var(--danger);}
.badge-type{background:rgba(0,212,255,.08);color:var(--accent);}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;margin-top:12px;}
@media(max-width:700px){.hero{grid-template-columns:1fr 1fr}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/frame-sniffer">Sniffer</a>
    <a href="/frame-inject-log" class="active">Inject Log</a>
    <a href="/beacon-spoofer">Spoofer</a>
  </nav>
</div>
<div class="container">
  <h1>Frame Injection Log</h1>
  <p class="subtitle">Real-time log of all frames transmitted by Phantom32. Tracks frame type, destination, channel, length, and success status.</p>

  <div class="hero">
    <div class="stat"><label>Sent</label><div class="val" id="h-sent">0</div></div>
    <div class="stat"><label>Success</label><div class="val" id="h-ok" style="color:var(--success)">0</div></div>
    <div class="stat"><label>Failed</label><div class="val" id="h-fail" style="color:var(--danger)">0</div></div>
    <div class="stat"><label>Rate /s</label><div class="val" id="h-rate" style="color:var(--warn)">0</div></div>
  </div>

  <div class="section">
    <h3>Frame Type Breakdown</h3>
    <div class="type-bars" id="type-bars">
      <div style="color:var(--muted);text-align:center">No data.</div>
    </div>
  </div>

  <div class="section">
    <h3>Recent Inject Events (last 64)</h3>
    <table>
      <thead>
        <tr><th>Time</th><th>Type</th><th>Dst</th><th>BSSID</th><th>CH</th><th>Len</th><th>Status</th><th>Note</th></tr>
      </thead>
      <tbody id="log-tbody">
        <tr><td colspan="8" style="color:var(--muted);text-align:center">No frames logged yet.</td></tr>
      </tbody>
    </table>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>
</div>

<script>
let g_data = {total_sent:0, total_ok:0, total_fail:0, rate_per_sec:0, by_type:[], log:[]};

function renderTypeBars() {
  const types = g_data.by_type.filter(t => t.count > 0);
  if (!types.length) {
    document.getElementById('type-bars').innerHTML = '<div style="color:var(--muted);text-align:center">No data.</div>';
    return;
  }
  const max = Math.max(...types.map(t => t.count), 1);
  document.getElementById('type-bars').innerHTML = types.map(t => `<div class="type-row">
    <div class="type-label">${t.type}</div>
    <div class="type-bar-wrap">
      <div class="type-bar-fill" style="width:${(t.count/max*100).toFixed(1)}%"></div>
    </div>
    <div class="type-count">${t.count}</div>
  </div>`).join('');
}

function renderLog() {
  document.getElementById('log-tbody').innerHTML = g_data.log.length
    ? g_data.log.map(e => `<tr class="${e.ok?'':'fail'}">
        <td style="color:var(--muted)">${new Date(e.ts*1000).toLocaleTimeString()}</td>
        <td><span class="badge badge-type">${e.type}</span></td>
        <td><code style="font-size:9px">${e.dst}</code></td>
        <td><code style="font-size:9px;color:var(--muted)">${e.bssid}</code></td>
        <td>${e.ch}</td>
        <td>${e.len}B</td>
        <td>${e.ok
          ? '<span class="badge badge-ok">OK</span>'
          : '<span class="badge badge-fail">FAIL</span>'}</td>
        <td style="color:var(--muted);font-size:10px">${e.note || '—'}</td>
      </tr>`).join('')
    : '<tr><td colspan="8" style="color:var(--muted);text-align:center">No data.</td></tr>';
}

async function refresh() {
  const r = await fetch('/api/fil/all').then(x => x.json()).catch(() => g_data);
  g_data = r;
  document.getElementById('h-sent').textContent = r.total_sent || 0;
  document.getElementById('h-ok').textContent   = r.total_ok   || 0;
  document.getElementById('h-fail').textContent = r.total_fail || 0;
  document.getElementById('h-rate').textContent = r.rate_per_sec || 0;
  renderTypeBars();
  renderLog();
}

async function clearData() {
  await fetch('/api/fil/clear', {method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 1000);
</script>
</body>
</html>
)rawhtml";

inline void wufil_routes() {
  server.on("/frame-inject-log", HTTP_GET, []() {
    server.send_P(200, "text/html", WUFIL_PAGE);
  });
}

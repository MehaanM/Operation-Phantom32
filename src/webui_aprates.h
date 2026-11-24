#pragma once
// webui_aprates.h — AP rate tracker UI.
// Route: /aprates. Prefix: wuart_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUART_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>AP Rates — Phantom32</title>
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
table{width:100%;border-collapse:collapse;font-size:12px;}
th{text-align:left;padding:7px 8px;font-size:10px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;cursor:pointer;}
th:hover{color:var(--text);}
td{padding:7px 8px;border-bottom:1px solid var(--border);}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
.badge{padding:1px 7px;border-radius:4px;font-size:10px;font-weight:700;}
.badge-burst{background:rgba(239,68,68,.2);color:var(--danger);}
.badge-flood{background:rgba(245,158,11,.2);color:var(--warn);}
.badge-silent{background:rgba(100,116,139,.2);color:var(--muted);}
.badge-ok{background:rgba(34,197,94,.12);color:var(--success);}
.rate-bar{display:flex;align-items:center;gap:5px;}
.rate-track{height:5px;width:50px;background:var(--bg3);border-radius:2px;}
.rate-fill{height:100%;border-radius:2px;}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;margin-top:12px;}
@media(max-width:700px){.hero{grid-template-columns:repeat(2,1fr)}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/analytics">Analytics</a>
    <a href="/aprates" class="active">AP Rates</a>
    <a href="/timeline">Timeline</a>
    <a href="/traffic">Traffic</a>
  </nav>
</div>
<div class="container">
  <h1>AP Rate Tracker</h1>
  <p class="subtitle">Rolling 10-second packet and beacon rate per access point, with burst/flood detection.</p>

  <div class="hero">
    <div class="stat"><label>Active APs</label><div class="val" id="h-aps">0</div></div>
    <div class="stat"><label>Global PPS</label><div class="val" id="h-pps">0</div></div>
    <div class="stat"><label>Bursting</label><div class="val" id="h-burst" style="color:var(--danger)">0</div></div>
    <div class="stat"><label>Beacon Floods</label><div class="val" id="h-flood" style="color:var(--warn)">0</div></div>
  </div>

  <div class="section">
    <h3>Per-AP Rates (10-second rolling)</h3>
    <table>
      <thead><tr>
        <th onclick="sort('bssid')">BSSID</th>
        <th onclick="sort('ssid')">SSID</th>
        <th>CH</th>
        <th onclick="sort('pps')">PPS</th>
        <th onclick="sort('bps')">BPS</th>
        <th onclick="sort('beacons')">Beacons/s</th>
        <th>Status</th>
      </tr></thead>
      <tbody id="ap-tbody">
        <tr><td colspan="7" style="color:var(--muted);text-align:center">Loading…</td></tr>
      </tbody>
    </table>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-ghost" onclick="clearData()">Reset</button>
    </div>
  </div>
</div>

<script>
let g_all = [];
let g_sort = {key:'pps', asc:false};

function sort(key) {
  if(g_sort.key===key) g_sort.asc=!g_sort.asc;
  else {g_sort.key=key; g_sort.asc=false;}
  renderTable();
}

function fmt_bps(b) {
  if(b>1024) return (b/1024).toFixed(1)+'K/s';
  return b+'/s';
}

function rate_bar(val, max_val, color) {
  const pct = max_val > 0 ? Math.min(100, Math.round(100*val/max_val)) : 0;
  return `<div class="rate-bar">
    <span style="color:${color}">${val}</span>
    <div class="rate-track"><div class="rate-fill" style="width:${pct}%;background:${color}"></div></div>
  </div>`;
}

function renderTable() {
  const max_pps = Math.max(...g_all.map(a=>a.pps), 1);
  const max_bps = Math.max(...g_all.map(a=>a.bps), 1);

  const list = [...g_all].sort((a,b)=>{
    const va=a[g_sort.key], vb=b[g_sort.key];
    if(typeof va==='number') return g_sort.asc?va-vb:vb-va;
    return g_sort.asc?String(va).localeCompare(String(vb)):String(vb).localeCompare(String(va));
  });

  document.getElementById('ap-tbody').innerHTML = list.length
    ? list.map(ap=>{
        const status = ap.burst ? 'badge-burst' : ap.bflood ? 'badge-flood' : ap.silent ? 'badge-silent' : 'badge-ok';
        const status_lbl = ap.burst ? 'BURST' : ap.bflood ? 'FLOOD' : ap.silent ? 'Silent' : 'OK';
        return `<tr>
          <td><code style="font-size:10px">${ap.bssid}</code></td>
          <td>${ap.ssid||'—'}</td>
          <td>${ap.ch}</td>
          <td>${rate_bar(ap.pps, max_pps, 'var(--accent)')}</td>
          <td>${fmt_bps(ap.bps)}</td>
          <td>${rate_bar(ap.beacons, 100, 'var(--warn)')}</td>
          <td><span class="badge ${status}">${status_lbl}</span></td>
        </tr>`;
      }).join('')
    : '<tr><td colspan="7" style="color:var(--muted);text-align:center">No APs observed.</td></tr>';
}

async function refresh() {
  const r = await fetch('/api/art/all').then(x=>x.json()).catch(()=>({}));
  document.getElementById('h-aps').textContent   = r.ap_count||0;
  document.getElementById('h-pps').textContent   = r.global_pps||0;
  document.getElementById('h-burst').textContent = r.burst_count||0;
  document.getElementById('h-flood').textContent = (r.aps||[]).filter(a=>a.bflood).length;
  g_all = r.aps||[];
  renderTable();
}

async function clearData() {
  await fetch('/api/art/clear',{method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 1500);
</script>
</body>
</html>
)rawhtml";

inline void wuart_routes() {
  server.on("/aprates", HTTP_GET, []() {
    server.send_P(200, "text/html", WUART_PAGE);
  });
}

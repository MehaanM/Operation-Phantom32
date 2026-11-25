#pragma once
// webui_probes.h — Probe request log & SSID frequency analysis web UI.
// Route: /probes. Prefix: wupb_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUPB_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Probe Log — Phantom32</title>
<style>
:root{--bg:#0a0c10;--bg2:#12151c;--bg3:#1a1f2e;--border:#2a3045;
  --accent:#00d4ff;--success:#22c55e;--warn:#f59e0b;--danger:#ef4444;
  --text:#e2e8f0;--muted:#64748b;--radius:8px;}
*{box-sizing:border-box;margin:0;padding:0;}
body{font-family:'Inter',system-ui,sans-serif;background:var(--bg);color:var(--text);min-height:100vh;}
.top-bar{display:flex;align-items:center;gap:12px;padding:14px 20px;
  background:var(--bg2);border-bottom:1px solid var(--border);position:sticky;top:0;z-index:10;}
.logo{font-size:18px;font-weight:700;color:var(--accent);letter-spacing:2px;}
.nav-links{display:flex;gap:8px;margin-left:auto;}
.nav-links a{color:var(--muted);text-decoration:none;font-size:13px;padding:5px 10px;border-radius:5px;transition:all .2s;}
.nav-links a:hover{color:var(--text);background:var(--bg3);}
.nav-links a.active{color:var(--accent);background:rgba(0,212,255,.08);}
.container{max-width:1100px;margin:0 auto;padding:24px 16px;}
h1{font-size:22px;font-weight:700;margin-bottom:4px;}
.subtitle{color:var(--muted);font-size:14px;margin-bottom:24px;}
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:20px;margin-bottom:16px;}
.section h3{font-size:13px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:16px;}
.stat-row{display:grid;grid-template-columns:repeat(4,1fr);gap:12px;margin-bottom:20px;}
.stat{background:var(--bg2);border:1px solid var(--border);border-radius:8px;padding:14px;}
.stat label{font-size:11px;color:var(--muted);text-transform:uppercase;}
.stat .val{font-size:22px;font-weight:700;color:var(--accent);margin-top:4px;}
.btn{padding:7px 14px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;transition:all .2s;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.btn-danger{background:var(--danger);color:#fff;}
.action-row{display:flex;gap:8px;flex-wrap:wrap;margin-top:14px;}
.tabs{display:flex;gap:4px;margin-bottom:16px;border-bottom:1px solid var(--border);}
.tab{padding:8px 16px;cursor:pointer;font-size:13px;color:var(--muted);border-bottom:2px solid transparent;margin-bottom:-1px;transition:all .2s;}
.tab.act{color:var(--accent);border-bottom-color:var(--accent);}
table{width:100%;border-collapse:collapse;font-size:13px;}
th{text-align:left;padding:7px 10px;font-size:11px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;}
td{padding:7px 10px;border-bottom:1px solid var(--border);}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
.bar-bg{background:var(--bg3);border-radius:3px;height:6px;min-width:60px;}
.bar-fg{height:6px;border-radius:3px;background:var(--accent);}
input[type=text]{padding:7px 10px;background:var(--bg3);border:1px solid var(--border);border-radius:5px;color:var(--text);font-size:13px;outline:none;width:200px;}
input:focus{border-color:var(--accent);}
.pane{display:none;} .pane.show{display:block;}
@media(max-width:700px){.stat-row{grid-template-columns:1fr 1fr;}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/">Dashboard</a>
    <a href="/forensics">Forensics</a>
    <a href="/devices">Devices</a>
    <a href="/channels">Channels</a>
    <a href="/probes" class="active">Probes</a>
    <a href="/settings">Settings</a>
  </nav>
</div>

<div class="container">
  <h1>Probe Request Log</h1>
  <p class="subtitle">Live feed of 802.11 probe requests — directed SSID lookups and wildcard probes.</p>

  <div class="stat-row">
    <div class="stat"><label>Total Probes</label><div class="val" id="st-total">0</div></div>
    <div class="stat"><label>Unique SSIDs</label><div class="val" id="st-ssids">0</div></div>
    <div class="stat"><label>Unique STAs</label><div class="val" id="st-stas">0</div></div>
    <div class="stat"><label>Directed %</label><div class="val" id="st-pct">0%</div></div>
  </div>

  <div class="section">
    <div class="tabs">
      <div class="tab act" onclick="switchTab(0)">Recent Probes</div>
      <div class="tab" onclick="switchTab(1)">Top SSIDs</div>
      <div class="tab" onclick="switchTab(2)">Device Profiles</div>
    </div>

    <div id="pane0" class="pane show">
      <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:12px;">
        <input type="text" id="filter-mac" placeholder="Filter by MAC…" oninput="filterRecent()">
        <label style="font-size:12px;color:var(--muted)"><input type="checkbox" id="cb-directed" onchange="filterRecent()"> Directed only</label>
      </div>
      <div style="overflow-x:auto;">
        <table>
          <thead><tr><th>Time</th><th>Source MAC</th><th>SSID</th><th>RSSI</th><th>Ch</th><th>Type</th></tr></thead>
          <tbody id="recent-tbody"></tbody>
        </table>
      </div>
    </div>

    <div id="pane1" class="pane">
      <div style="overflow-x:auto;">
        <table>
          <thead><tr><th>SSID</th><th>Probe Count</th><th>Best RSSI</th><th>Frequency</th></tr></thead>
          <tbody id="ssid-tbody"></tbody>
        </table>
      </div>
    </div>

    <div id="pane2" class="pane">
      <div style="overflow-x:auto;">
        <table>
          <thead><tr><th>Client MAC</th><th>Total</th><th>Directed</th><th>Wildcard</th><th>RSSI</th><th>Known SSIDs</th></tr></thead>
          <tbody id="sta-tbody"></tbody>
        </table>
      </div>
    </div>

    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-danger" onclick="clearAll()">Clear</button>
    </div>
  </div>
</div>

<script>
let recentData = [];
let curTab = 0;

function switchTab(n) {
  curTab = n;
  document.querySelectorAll('.tab').forEach((t,i)=>t.classList.toggle('act',i===n));
  document.querySelectorAll('.pane').forEach((p,i)=>p.classList.toggle('show',i===n));
}

function rssiColor(r) {
  if (r > -60) return '#22c55e';
  if (r > -75) return '#f59e0b';
  return '#ef4444';
}

function filterRecent() {
  const fmac = document.getElementById('filter-mac').value.toLowerCase();
  const dirOnly = document.getElementById('cb-directed').checked;
  const rows = recentData.filter(e=>{
    if (dirOnly && !e.directed) return false;
    if (fmac && !e.src.toLowerCase().includes(fmac)) return false;
    return true;
  });
  renderRecent(rows);
}

function renderRecent(data) {
  const tbody = document.getElementById('recent-tbody');
  if (!data.length) {
    tbody.innerHTML='<tr><td colspan="6" style="color:var(--muted);text-align:center">No probes.</td></tr>';
    return;
  }
  tbody.innerHTML = data.slice(0,100).map(e=>{
    const t = new Date(e.ts*1000).toLocaleTimeString();
    const typeBadge = e.directed
      ? '<span style="color:var(--accent);font-size:11px">directed</span>'
      : '<span style="color:var(--muted);font-size:11px">wildcard</span>';
    return `<tr>
      <td>${t}</td>
      <td><code style="font-size:11px">${e.src}</code></td>
      <td>${e.ssid||'<em style="color:var(--muted)">broadcast</em>'}</td>
      <td style="color:${rssiColor(e.rssi)}">${e.rssi} dBm</td>
      <td>${e.ch}</td>
      <td>${typeBadge}</td>
    </tr>`;
  }).join('');
}

async function refresh() {
  const r = await fetch('/api/plog/recent?limit=200').then(x=>x.json()).catch(()=>({}));
  if (!r.recent) return;

  recentData = r.recent;
  document.getElementById('st-total').textContent = r.total||0;
  document.getElementById('st-ssids').textContent = r.ssid_count||0;
  document.getElementById('st-stas').textContent  = r.sta_count||0;
  const dirPct = r.total > 0 ? Math.round(100 * r.recent.filter(e=>e.directed).length / Math.min(r.total, r.recent.length)) : 0;
  document.getElementById('st-pct').textContent = dirPct + '%';

  filterRecent();

  // Top SSIDs
  if (r.top_ssids) {
    const maxCount = Math.max(...r.top_ssids.map(s=>s.count), 1);
    document.getElementById('ssid-tbody').innerHTML = r.top_ssids.map(s=>`
      <tr>
        <td>${s.ssid}</td>
        <td>${s.count}</td>
        <td style="color:${rssiColor(s.rssi)}">${s.rssi} dBm</td>
        <td><div class="bar-bg"><div class="bar-fg" style="width:${Math.round(100*s.count/maxCount)}%"></div></div></td>
      </tr>`).join('');
  }

  // Device profiles
  const sr = await fetch('/api/plog/stas').then(x=>x.json()).catch(()=>[]);
  if (sr.length) {
    document.getElementById('sta-tbody').innerHTML = sr.map(s=>`
      <tr>
        <td><code style="font-size:11px">${s.mac}</code></td>
        <td>${s.total}</td>
        <td>${s.directed}</td>
        <td>${s.wildcard}</td>
        <td style="color:${rssiColor(s.rssi)}">${s.rssi} dBm</td>
        <td style="font-size:11px;color:var(--muted)">${s.ssids.slice(0,3).join(', ')}${s.ssids.length>3?'…':''}</td>
      </tr>`).join('');
  }
}

async function clearAll() {
  if (!confirm('Clear probe log?')) return;
  await fetch('/api/plog/clear',{method:'POST'});
  recentData=[];
  renderRecent([]);
}

refresh();
setInterval(refresh, 4000);
</script>
</body>
</html>
)rawhtml";

inline void wupb_routes() {
  server.on("/probes", HTTP_GET, []() { server.send_P(200, "text/html", WUPB_PAGE); });
}

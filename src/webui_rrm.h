#pragma once
// webui_rrm.h — 802.11k Radio Resource Management UI.
// Route: /rrm. Prefix: wurrm_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WURRM_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>802.11k RRM — Phantom32</title>
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
.tabs{display:flex;gap:4px;margin-bottom:14px;}
.tab{padding:5px 12px;border-radius:5px;font-size:12px;cursor:pointer;border:1px solid var(--border);color:var(--muted);}
.tab.active{background:rgba(0,212,255,.12);color:var(--accent);border-color:var(--accent);}
table{width:100%;border-collapse:collapse;font-size:12px;}
th{text-align:left;padding:7px 8px;font-size:10px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;cursor:pointer;}
th:hover{color:var(--text);}
td{padding:7px 8px;border-bottom:1px solid var(--border);}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
.badge{padding:2px 7px;border-radius:4px;font-size:9px;font-weight:700;display:inline-block;}
.load-bar{width:60px;height:6px;background:var(--bg3);border-radius:3px;display:inline-block;vertical-align:middle;}
.load-fill{height:100%;border-radius:3px;}
.sparkline{display:flex;align-items:flex-end;gap:1px;height:20px;}
.sbar{width:4px;border-radius:1px;min-height:1px;}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;margin-top:12px;}
.ev-row{padding:6px 8px;border-bottom:1px solid var(--border);font-size:11px;display:grid;grid-template-columns:60px 120px 100px 1fr;gap:8px;}
.ev-ts{color:var(--muted);}
@media(max-width:700px){.hero{grid-template-columns:1fr 1fr};.ev-row{grid-template-columns:1fr}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/rrm" class="active">802.11k RRM</a>
    <a href="/bss-transition">802.11v BSS</a>
    <a href="/spatial-reuse">802.11ax</a>
    <a href="/neighbor-report">Neighbor</a>
  </nav>
</div>
<div class="container">
  <h1>802.11k Radio Resource Management</h1>
  <p class="subtitle">Monitors RRM action frames: Neighbor Report requests/responses, Channel Load measurements, and Link Measurements. Used by APs to coordinate roaming.</p>

  <div class="hero">
    <div class="stat"><label>RRM-Capable APs</label><div class="val" id="h-aps">0</div></div>
    <div class="stat"><label>Neighbor Reports</label><div class="val" id="h-nbr" style="color:var(--success)">0</div></div>
    <div class="stat"><label>Chan Load Reports</label><div class="val" id="h-cl" style="color:var(--warn)">0</div></div>
    <div class="stat"><label>Neighbors Found</label><div class="val" id="h-nb">0</div></div>
  </div>

  <div class="section">
    <div class="tabs">
      <div class="tab active" onclick="showTab('aps',this)">AP RRM Stats</div>
      <div class="tab" onclick="showTab('nb',this)">Neighbor Entries</div>
      <div class="tab" onclick="showTab('events',this)">Events</div>
    </div>

    <div id="tab-aps">
      <table>
        <thead><tr>
          <th>BSSID</th><th>SSID</th><th>CH</th>
          <th>RSSI</th><th>RRM Cap</th><th>Nb Reqs</th>
          <th>Nb Rsps</th><th>CL Rsps</th><th>Load</th>
        </tr></thead>
        <tbody id="ap-tbody">
          <tr><td colspan="9" style="color:var(--muted);text-align:center">Waiting for RRM frames…</td></tr>
        </tbody>
      </table>
    </div>

    <div id="tab-nb" style="display:none">
      <table>
        <thead><tr>
          <th>BSSID</th><th>SSID</th><th>CH</th>
          <th>RSSI</th><th>Reports</th><th>BSSID Info</th>
        </tr></thead>
        <tbody id="nb-tbody">
          <tr><td colspan="6" style="color:var(--muted);text-align:center">No neighbor entries yet.</td></tr>
        </tbody>
      </table>
    </div>

    <div id="tab-events" style="display:none">
      <div id="ev-list">
        <div style="color:var(--muted);text-align:center;padding:20px;">No RRM events yet.</div>
      </div>
    </div>

    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>
</div>

<script>
let g_data = {};
let g_events = [];
let g_active_tab = 'aps';

const ACTION_NAMES = {4:'NB Req',5:'NB Rsp',3:'CL Req',4:'CL Rsp',6:'LM Req',7:'LM Rsp'};

function showTab(name, el) {
  g_active_tab = name;
  document.querySelectorAll('.tab').forEach(t=>t.classList.remove('active'));
  el.classList.add('active');
  document.getElementById('tab-aps').style.display    = name==='aps'?'':'none';
  document.getElementById('tab-nb').style.display     = name==='nb'?'':'none';
  document.getElementById('tab-events').style.display = name==='events'?'':'none';
}

function load_color(v) {
  if(v>80) return 'var(--danger)';
  if(v>50) return 'var(--warn)';
  return 'var(--success)';
}

function sparkline(hist) {
  const max = Math.max(...hist, 1);
  return '<div class="sparkline">' + hist.map(v=>{
    const h = Math.round((v/max)*18)+2;
    return `<div class="sbar" style="height:${h}px;background:${load_color(v)}"></div>`;
  }).join('') + '</div>';
}

function renderAPs(aps) {
  document.getElementById('ap-tbody').innerHTML = aps.length
    ? aps.map(a=>`<tr>
        <td><code style="font-size:10px">${a.bssid}</code></td>
        <td>${a.ssid||'—'}</td><td>${a.ch}</td>
        <td>${a.rssi} dBm</td>
        <td>${a.rrm_cap?'<span class="badge" style="background:rgba(34,197,94,.15);color:var(--success)">RRM</span>':'—'}</td>
        <td>${a.nb_reqs}</td><td>${a.nb_rsps}</td><td>${a.load_rsps}</td>
        <td style="display:flex;align-items:center;gap:5px;">
          <span style="color:${load_color(a.load)};font-size:11px">${a.load}%</span>
          <div class="load-bar"><div class="load-fill" style="width:${a.load}%;background:${load_color(a.load)}"></div></div>
        </td>
      </tr>`).join('')
    : '<tr><td colspan="9" style="color:var(--muted);text-align:center">No RRM APs.</td></tr>';
}

function renderNeighbors(nbs) {
  document.getElementById('nb-tbody').innerHTML = nbs.length
    ? nbs.map(n=>`<tr>
        <td><code style="font-size:10px">${n.bssid}</code></td>
        <td>${n.ssid||'—'}</td><td>${n.ch}</td>
        <td>${n.rssi} dBm</td>
        <td>${n.reports}</td>
        <td style="font-size:10px;font-family:monospace">${(n.bssid_info>>>0).toString(16).padStart(8,'0').toUpperCase()}</td>
      </tr>`).join('')
    : '<tr><td colspan="6" style="color:var(--muted);text-align:center">No neighbors.</td></tr>';
}

const ACTION_LABEL = {3:'CL Req',4:'NB/CL',5:'NB Rsp',6:'LM Req',7:'LM Rsp'};

function renderEvents(evs) {
  document.getElementById('ev-list').innerHTML = evs.length
    ? evs.slice().reverse().map(e=>`
      <div class="ev-row">
        <span class="ev-ts">${new Date(e.ts*1000).toLocaleTimeString()}</span>
        <span><code style="font-size:10px">${e.ap.substring(0,17)}</code></span>
        <span style="color:var(--accent)">${ACTION_LABEL[e.action]||'Action '+e.action}</span>
        <span style="color:var(--muted)">${e.detail}</span>
      </div>`).join('')
    : '<div style="color:var(--muted);text-align:center;padding:20px;">No RRM events.</div>';
}

async function refresh() {
  const [data, events] = await Promise.all([
    fetch('/api/rrm/all').then(x=>x.json()).catch(()=>({})),
    fetch('/api/rrm/events').then(x=>x.json()).catch(()=>[]),
  ]);
  g_data = data; g_events = events;
  document.getElementById('h-aps').textContent = data.ap_count||0;
  document.getElementById('h-nbr').textContent = (data.total_neighbor_reqs||0)+(data.total_neighbor_rsps||0);
  document.getElementById('h-cl').textContent  = data.total_chan_loads||0;
  document.getElementById('h-nb').textContent  = data.neighbor_count||0;
  renderAPs(data.aps||[]);
  renderNeighbors(data.neighbors||[]);
  renderEvents(events);
}

async function clearData() {
  await fetch('/api/rrm/clear',{method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 4000);
</script>
</body>
</html>
)rawhtml";

inline void wurrm_routes() {
  server.on("/rrm", HTTP_GET, []() {
    server.send_P(200, "text/html", WURRM_PAGE);
  });
}

#pragma once
// webui_client_activity.h — Client activity profiling UI.
// Route: /client-activity. Prefix: wucac_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUCAC_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Client Activity — Phantom32</title>
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
.container{max-width:1200px;margin:0 auto;padding:20px 16px;}
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
.badge{padding:2px 6px;border-radius:4px;font-size:9px;font-weight:700;display:inline-block;}
.state-active{background:rgba(34,197,94,.18);color:var(--success);}
.state-idle{background:rgba(100,116,139,.18);color:var(--muted);}
.state-roaming{background:rgba(0,212,255,.12);color:var(--accent);}
.state-probing{background:rgba(245,158,11,.12);color:var(--warn);}
.state-deauthed{background:rgba(239,68,68,.15);color:var(--danger);}
.spark{display:flex;align-items:flex-end;gap:1px;height:20px;}
.sbar{width:3px;border-radius:1px;min-height:1px;background:var(--accent);}
.ap-pill{display:inline-flex;align-items:center;gap:3px;padding:2px 6px;
  background:var(--bg3);border-radius:4px;font-size:9px;margin:1px;}
.filter-row{display:flex;gap:8px;margin-bottom:12px;flex-wrap:wrap;}
input,select{background:var(--bg3);border:1px solid var(--border);border-radius:5px;padding:4px 8px;color:var(--text);font-size:12px;}
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
    <a href="/clients-ext">Clients</a>
    <a href="/client-activity" class="active">Activity</a>
    <a href="/client-devices">Device Types</a>
    <a href="/assoc-history">Assoc History</a>
  </nav>
</div>
<div class="container">
  <h1>Client Activity Profiler</h1>
  <p class="subtitle">Per-client frame type breakdown, activity history, idle detection, and roaming tracking.</p>

  <div class="hero">
    <div class="stat"><label>Clients</label><div class="val" id="h-cl">0</div></div>
    <div class="stat"><label>Active Now</label><div class="val" id="h-active" style="color:var(--success)">0</div></div>
    <div class="stat"><label>Total Roams</label><div class="val" id="h-roams" style="color:var(--warn)">0</div></div>
    <div class="stat"><label>Total Probes</label><div class="val" id="h-probes">0</div></div>
  </div>

  <div class="section">
    <h3>Client Activity</h3>
    <div class="filter-row">
      <input id="f-search" placeholder="Search MAC…" style="flex:1;min-width:140px">
      <select id="f-state">
        <option value="">All States</option>
        <option value="active">Active</option>
        <option value="idle">Idle</option>
        <option value="roaming">Roaming</option>
        <option value="probing">Probing</option>
        <option value="deauthed">Deauthed</option>
      </select>
    </div>
    <table>
      <thead><tr>
        <th onclick="sortClients('mac')">MAC</th>
        <th>CH</th>
        <th onclick="sortClients('rssi')">RSSI</th>
        <th onclick="sortClients('state')">State</th>
        <th onclick="sortClients('total')">Frames</th>
        <th>Data/Mgmt</th>
        <th onclick="sortClients('roams')">Roams</th>
        <th>APs Seen</th>
        <th onclick="sortClients('idle')">Idle</th>
        <th>60s Activity</th>
      </tr></thead>
      <tbody id="cl-tbody">
        <tr><td colspan="10" style="color:var(--muted);text-align:center">Loading…</td></tr>
      </tbody>
    </table>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>
</div>

<script>
let g_clients = [];
let g_sort = {key:'total', asc:false};

const STATE_CLASSES = {active:'state-active',idle:'state-idle',roaming:'state-roaming',probing:'state-probing',deauthed:'state-deauthed'};

function sparkline(hist) {
  const max = Math.max(...hist, 1);
  return '<div class="spark">' + hist.map(v=>{
    const h = Math.round((v/max)*18)+1;
    return `<div class="sbar" style="height:${h}px;opacity:${v?1:0.2}"></div>`;
  }).join('') + '</div>';
}

function sortClients(key) {
  if(g_sort.key===key) g_sort.asc=!g_sort.asc;
  else {g_sort.key=key; g_sort.asc=false;}
  renderClients();
}

function fmtBytes(b) {
  if(b>1048576) return (b/1048576).toFixed(1)+'MB';
  if(b>1024) return (b/1024).toFixed(1)+'KB';
  return b+'B';
}

function fmtDuration(s) {
  if(s<60) return s+'s';
  if(s<3600) return Math.floor(s/60)+'m'+String(s%60).padStart(2,'0')+'s';
  return Math.floor(s/3600)+'h'+String(Math.floor((s%3600)/60)).padStart(2,'0')+'m';
}

function renderClients() {
  const fsearch = document.getElementById('f-search').value.toLowerCase();
  const fstate  = document.getElementById('f-state').value;
  const filtered = g_clients.filter(c=>{
    if(fsearch && !c.mac.toLowerCase().includes(fsearch)) return false;
    if(fstate && c.state !== fstate) return false;
    return true;
  });
  const sorted = [...filtered].sort((a,b)=>{
    const va=a[g_sort.key],vb=b[g_sort.key];
    if(typeof va==='number') return g_sort.asc?va-vb:vb-va;
    return g_sort.asc?String(va).localeCompare(String(vb)):String(vb).localeCompare(String(va));
  });

  document.getElementById('cl-tbody').innerHTML = sorted.length
    ? sorted.map(c=>`<tr>
        <td><code style="font-size:10px">${c.mac}</code></td>
        <td>${c.ch}</td>
        <td>${c.rssi} dBm</td>
        <td><span class="badge ${STATE_CLASSES[c.state]||''}">${c.state}</span></td>
        <td>${c.total.toLocaleString()}</td>
        <td style="font-size:10px">${c.data}/${c.mgmt}</td>
        <td>${c.roams||0}</td>
        <td style="font-size:10px">${(c.assocs||[]).map(a=>`<span class="ap-pill">${a.bssid.substring(0,8)}</span>`).join('')}</td>
        <td style="color:${c.idle>60?'var(--danger)':c.idle>20?'var(--warn)':'var(--muted)'}">${fmtDuration(c.idle)}</td>
        <td>${sparkline(c.activity||[])}</td>
      </tr>`).join('')
    : '<tr><td colspan="10" style="color:var(--muted);text-align:center">No clients.</td></tr>';
}

async function refresh() {
  const r = await fetch('/api/cac/all').then(x=>x.json()).catch(()=>({}));
  g_clients = r.clients||[];
  document.getElementById('h-cl').textContent     = r.client_count||0;
  document.getElementById('h-active').textContent = g_clients.filter(c=>c.state==='active').length;
  document.getElementById('h-roams').textContent  = r.total_roams||0;
  document.getElementById('h-probes').textContent = r.total_probes||0;
  renderClients();
}

async function clearData() {
  await fetch('/api/cac/clear',{method:'POST'});
  refresh();
}

document.getElementById('f-search').addEventListener('input', renderClients);
document.getElementById('f-state').addEventListener('change', renderClients);

refresh();
setInterval(refresh, 2000);
</script>
</body>
</html>
)rawhtml";

inline void wucac_routes() {
  server.on("/client-activity", HTTP_GET, []() { server.send_P(200, "text/html", WUCAC_PAGE); });
}

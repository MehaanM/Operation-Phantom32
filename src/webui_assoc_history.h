#pragma once
// webui_assoc_history.h — Association history event log UI.
// Route: /assoc-history. Prefix: wuash_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUASH_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Association History — Phantom32</title>
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
.tab{padding:5px 12px;border-radius:5px;font-size:12px;cursor:pointer;border:1px solid var(--border);background:transparent;color:var(--muted);}
.tab.active{background:var(--accent);color:#000;border-color:var(--accent);font-weight:600;}
table{width:100%;border-collapse:collapse;font-size:12px;}
th{text-align:left;padding:7px 8px;font-size:10px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;}
td{padding:7px 8px;border-bottom:1px solid var(--border);}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
.ev-badge{padding:2px 8px;border-radius:4px;font-size:10px;font-weight:600;display:inline-block;}
.ev-assoc{background:rgba(34,197,94,.12);color:var(--success);}
.ev-deauth{background:rgba(239,68,68,.12);color:var(--danger);}
.ev-disassoc{background:rgba(245,158,11,.12);color:var(--warn);}
.ev-auth{background:rgba(0,212,255,.1);color:var(--accent);}
.ev-reassoc{background:rgba(168,85,247,.1);color:#a855f7;}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;margin-top:12px;}
.filter-row{display:flex;gap:8px;margin-bottom:12px;flex-wrap:wrap;}
select,input{background:var(--bg3);border:1px solid var(--border);border-radius:5px;padding:4px 8px;color:var(--text);font-size:12px;}
@media(max-width:700px){.hero{grid-template-columns:repeat(2,1fr)}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/clients-ext">Clients</a>
    <a href="/assoc-history" class="active">Assoc History</a>
    <a href="/deauth">Deauth</a>
    <a href="/session">Sessions</a>
  </nav>
</div>
<div class="container">
  <h1>Association / Disassociation Log</h1>
  <p class="subtitle">Full 802.11 management event log: auth, association, reassociation, disassociation, deauthentication.</p>

  <div class="hero">
    <div class="stat"><label>Total Assocs</label><div class="val" id="h-assocs">0</div></div>
    <div class="stat"><label>Deauths</label><div class="val" id="h-deauths" style="color:var(--danger)">0</div></div>
    <div class="stat"><label>Disassocs</label><div class="val" id="h-disassocs" style="color:var(--warn)">0</div></div>
    <div class="stat"><label>Clients</label><div class="val" id="h-clients">0</div></div>
  </div>

  <div class="section">
    <div class="tabs">
      <button class="tab active" onclick="switchTab('events',this)">Events</button>
      <button class="tab" onclick="switchTab('clients',this)">Clients</button>
    </div>

    <div id="events-panel">
      <div class="filter-row">
        <select id="f-evtype">
          <option value="all">All Events</option>
          <option value="Assoc Req">Assoc Req</option>
          <option value="Assoc Resp">Assoc Resp</option>
          <option value="Reassoc">Reassoc</option>
          <option value="Disassoc">Disassoc</option>
          <option value="Deauth">Deauth</option>
          <option value="Auth">Auth</option>
        </select>
        <input id="f-search" type="text" placeholder="Search MAC/SSID…" style="flex:1;min-width:140px">
      </div>
      <table>
        <thead><tr>
          <th>Time</th>
          <th>Type</th>
          <th>Client</th>
          <th>AP</th>
          <th>SSID</th>
          <th>CH</th>
          <th>RSSI</th>
          <th>Code</th>
        </tr></thead>
        <tbody id="ev-tbody">
          <tr><td colspan="8" style="color:var(--muted);text-align:center">Loading…</td></tr>
        </tbody>
      </table>
    </div>

    <div id="clients-panel" style="display:none">
      <table>
        <thead><tr>
          <th>Client MAC</th>
          <th>AP</th>
          <th>SSID</th>
          <th>Status</th>
          <th>Assocs</th>
          <th>Deauths</th>
          <th>Last Seen</th>
        </tr></thead>
        <tbody id="cl-tbody">
          <tr><td colspan="7" style="color:var(--muted);text-align:center">Loading…</td></tr>
        </tbody>
      </table>
    </div>

    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>
</div>

<script>
let g_events = [];
let g_clients = [];
let g_tab = 'events';

function ev_badge(type) {
  const cls = {
    'Assoc Req':'ev-assoc','Assoc Resp':'ev-assoc',
    'Reassoc':'ev-reassoc','Disassoc':'ev-disassoc',
    'Deauth':'ev-deauth','Auth':'ev-auth'
  };
  return `<span class="ev-badge ${cls[type]||''}">${type}</span>`;
}

function switchTab(t, btn) {
  g_tab = t;
  document.querySelectorAll('.tab').forEach(b=>b.classList.remove('active'));
  btn.classList.add('active');
  document.getElementById('events-panel').style.display  = t==='events'  ? '' : 'none';
  document.getElementById('clients-panel').style.display = t==='clients' ? '' : 'none';
}

function renderEvents() {
  const ftype   = document.getElementById('f-evtype').value;
  const fsearch = document.getElementById('f-search').value.toLowerCase();
  const filtered = g_events.filter(e=>{
    if(ftype!=='all' && e.type!==ftype) return false;
    if(fsearch && !e.client.toLowerCase().includes(fsearch) &&
       !e.ssid.toLowerCase().includes(fsearch) &&
       !e.ap.toLowerCase().includes(fsearch)) return false;
    return true;
  });

  document.getElementById('ev-tbody').innerHTML = filtered.length
    ? filtered.map(e=>`<tr>
        <td>${new Date(e.ts*1000).toLocaleTimeString()}</td>
        <td>${ev_badge(e.type)}</td>
        <td><code style="font-size:10px">${e.client}</code></td>
        <td><code style="font-size:10px">${e.ap}</code></td>
        <td>${e.ssid||'—'}</td>
        <td>${e.ch}</td>
        <td>${e.rssi} dBm</td>
        <td>${e.reason||e.status||'—'}</td>
      </tr>`).join('')
    : '<tr><td colspan="8" style="color:var(--muted);text-align:center">No events.</td></tr>';
}

function renderClients() {
  document.getElementById('cl-tbody').innerHTML = g_clients.length
    ? g_clients.map(c=>`<tr>
        <td><code style="font-size:10px">${c.mac}</code></td>
        <td><code style="font-size:10px">${c.ap}</code></td>
        <td>${c.ssid||'—'}</td>
        <td>${c.assoc?'<span style="color:var(--success)">Associated</span>':'<span style="color:var(--muted)">Idle</span>'}</td>
        <td>${c.assocs}</td>
        <td style="color:var(--danger)">${c.deauths}</td>
        <td>${new Date(c.last_ts*1000).toLocaleTimeString()}</td>
      </tr>`).join('')
    : '<tr><td colspan="7" style="color:var(--muted);text-align:center">No clients.</td></tr>';
}

async function refresh() {
  const [sum, evs, cls] = await Promise.all([
    fetch('/api/ash/summary').then(x=>x.json()).catch(()=>({})),
    fetch('/api/ash/events').then(x=>x.json()).catch(()=>[]),
    fetch('/api/ash/clients').then(x=>x.json()).catch(()=>[]),
  ]);
  document.getElementById('h-assocs').textContent   = sum.total_assocs||0;
  document.getElementById('h-deauths').textContent  = sum.total_deauths||0;
  document.getElementById('h-disassocs').textContent = sum.total_disassocs||0;
  document.getElementById('h-clients').textContent  = sum.client_count||0;
  g_events  = evs;
  g_clients = cls;
  renderEvents();
  renderClients();
}

async function clearData() {
  await fetch('/api/ash/clear',{method:'POST'});
  refresh();
}

document.getElementById('f-evtype').addEventListener('change', renderEvents);
document.getElementById('f-search').addEventListener('input',  renderEvents);

refresh();
setInterval(refresh, 2500);
</script>
</body>
</html>
)rawhtml";

inline void wuash_routes() {
  server.on("/assoc-history", HTTP_GET, []() {
    server.send_P(200, "text/html", WUASH_PAGE);
  });
}

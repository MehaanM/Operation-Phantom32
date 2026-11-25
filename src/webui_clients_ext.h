#pragma once
// webui_clients_ext.h — Extended client tracking UI.
// Route: /clients-ext. Prefix: wucte_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUCTE_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Client States — Phantom32</title>
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
.hero{display:grid;grid-template-columns:repeat(5,1fr);gap:10px;margin-bottom:16px;}
.stat{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:13px;}
.stat label{font-size:10px;color:var(--muted);text-transform:uppercase;}
.stat .val{font-size:22px;font-weight:800;color:var(--accent);margin-top:3px;}
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:18px;margin-bottom:14px;}
.section h3{font-size:12px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:14px;}
.filter-row{display:flex;gap:8px;margin-bottom:12px;flex-wrap:wrap;}
input[type=text]{background:var(--bg3);border:1px solid var(--border);border-radius:5px;
  padding:5px 10px;font-size:12px;color:var(--text);width:180px;}
input:focus{outline:none;border-color:var(--accent);}
select{background:var(--bg3);border:1px solid var(--border);border-radius:5px;
  padding:5px 8px;font-size:12px;color:var(--text);}
table{width:100%;border-collapse:collapse;font-size:12px;}
th{text-align:left;padding:7px 8px;font-size:10px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;cursor:pointer;}
th:hover{color:var(--text);}
td{padding:7px 8px;border-bottom:1px solid var(--border);}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
.state-dot{display:inline-block;width:8px;height:8px;border-radius:50%;margin-right:5px;}
.badge{padding:1px 7px;border-radius:4px;font-size:10px;font-weight:600;}
.b-connected{background:rgba(34,197,94,.12);color:var(--success);}
.b-probing{background:rgba(0,212,255,.1);color:var(--accent);}
.b-authed{background:rgba(245,158,11,.1);color:var(--warn);}
.b-assoc{background:rgba(245,158,11,.2);color:var(--warn);}
.b-deauthed{background:rgba(239,68,68,.2);color:var(--danger);}
.b-roaming{background:rgba(139,92,246,.2);color:#a78bfa;}
.b-unseen{background:rgba(100,116,139,.1);color:var(--muted);}
.b-disassoc{background:rgba(239,68,68,.1);color:var(--danger);}
.b-eap{background:rgba(0,212,255,.1);color:var(--accent);font-size:9px;margin-left:3px;}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;margin-top:12px;}
@media(max-width:700px){.hero{grid-template-columns:repeat(3,1fr)}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/devices">Devices</a>
    <a href="/clients-ext" class="active">Client States</a>
    <a href="/associations">Assoc</a>
    <a href="/eapol">EAPOL</a>
  </nav>
</div>
<div class="container">
  <h1>Client State Machine</h1>
  <p class="subtitle">Per-client 802.11 connection state: Probing → Auth → Assoc → Connected → Roaming.</p>

  <div class="hero">
    <div class="stat"><label>Clients</label><div class="val" id="h-clients">0</div></div>
    <div class="stat"><label>Connected</label><div class="val" id="h-conn" style="color:var(--success)">0</div></div>
    <div class="stat"><label>Probing</label><div class="val" id="h-probe" style="color:var(--accent)">0</div></div>
    <div class="stat"><label>Roam Events</label><div class="val" id="h-roam" style="color:#a78bfa">0</div></div>
    <div class="stat"><label>Deauths</label><div class="val" id="h-deauth" style="color:var(--danger)">0</div></div>
  </div>

  <div class="section">
    <h3>Client List</h3>
    <div class="filter-row">
      <input type="text" id="f-mac" placeholder="Filter MAC…" oninput="applyFilter()">
      <input type="text" id="f-ssid" placeholder="Filter SSID…" oninput="applyFilter()">
      <select id="f-state" onchange="applyFilter()">
        <option value="">All States</option>
        <option value="Connected">Connected</option>
        <option value="Probing">Probing</option>
        <option value="Authenticated">Authenticated</option>
        <option value="Associated">Associated</option>
        <option value="Deauthed">Deauthed</option>
        <option value="Roaming">Roaming</option>
      </select>
    </div>
    <table>
      <thead><tr>
        <th onclick="sort('mac')">MAC</th>
        <th onclick="sort('ssid')">SSID</th>
        <th>State</th>
        <th onclick="sort('rssi')">RSSI</th>
        <th>CH</th>
        <th onclick="sort('data')">Data</th>
        <th onclick="sort('roams')">Roams</th>
        <th onclick="sort('deauths')">Deauths</th>
        <th onclick="sort('ts')">Last Seen</th>
      </tr></thead>
      <tbody id="client-tbody">
        <tr><td colspan="9" style="color:var(--muted);text-align:center">Loading…</td></tr>
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
let g_sort = {key:'ts', asc:false};

const STATE_BADGE = {
  'Connected':'b-connected','Probing':'b-probing',
  'Authenticated':'b-authed','Associated':'b-assoc',
  'Deauthed':'b-deauthed','Disassoc':'b-disassoc',
  'Roaming':'b-roaming','Unseen':'b-unseen'
};

function sort(key) {
  if(g_sort.key===key) g_sort.asc=!g_sort.asc;
  else {g_sort.key=key; g_sort.asc=false;}
  applyFilter();
}

function applyFilter() {
  const fmac  = document.getElementById('f-mac').value.toLowerCase();
  const fssid = document.getElementById('f-ssid').value.toLowerCase();
  const fst   = document.getElementById('f-state').value;

  let list = g_all.filter(c=>{
    if(fmac  && !c.mac.toLowerCase().includes(fmac))   return false;
    if(fssid && !c.ssid.toLowerCase().includes(fssid)) return false;
    if(fst   && c.state !== fst) return false;
    return true;
  }).sort((a,b)=>{
    const va=a[g_sort.key], vb=b[g_sort.key];
    if(typeof va==='number') return g_sort.asc?va-vb:vb-va;
    return g_sort.asc?String(va).localeCompare(String(vb)):String(vb).localeCompare(String(va));
  });

  document.getElementById('client-tbody').innerHTML = list.length
    ? list.map(c=>{
        const bc = STATE_BADGE[c.state]||'b-unseen';
        const rssi_col = c.rssi>-70?'var(--success)':c.rssi>-85?'var(--warn)':'var(--danger)';
        return `<tr>
          <td><code style="font-size:11px">${c.mac}</code></td>
          <td>${c.ssid||'—'}</td>
          <td><span class="badge ${bc}">${c.state}</span>${c.eap?'<span class="badge b-eap">EAP</span>':''}</td>
          <td style="color:${rssi_col}">${c.rssi} dBm</td>
          <td>${c.ch}</td>
          <td>${c.data}</td>
          <td style="color:#a78bfa">${c.roams}</td>
          <td style="color:var(--danger)">${c.deauths}</td>
          <td style="color:var(--muted)">${new Date(c.ts*1000).toLocaleTimeString()}</td>
        </tr>`;
      }).join('')
    : '<tr><td colspan="9" style="color:var(--muted);text-align:center">No clients.</td></tr>';
}

async function refresh() {
  const r = await fetch('/api/cte/all').then(x=>x.json()).catch(()=>({}));
  document.getElementById('h-clients').textContent = r.client_count||0;
  document.getElementById('h-conn').textContent    = r.connected||0;
  document.getElementById('h-probe').textContent   = r.probing||0;
  document.getElementById('h-roam').textContent    = r.roam_events||0;
  document.getElementById('h-deauth').textContent  = r.deauth_events||0;
  g_all = r.clients||[];
  applyFilter();
}

async function clearData() {
  await fetch('/api/cte/clear',{method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 2000);
</script>
</body>
</html>
)rawhtml";

inline void wucte_routes() {
  server.on("/clients-ext", HTTP_GET, []() { server.send_P(200, "text/html", WUCTE_PAGE); });
}

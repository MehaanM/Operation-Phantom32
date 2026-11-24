#pragma once
// webui_ds_state.h — DS state tracker UI.
// Route: /ds-state. Prefix: wudst_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUDST_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>DS State — Phantom32</title>
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
.filter-row{display:flex;gap:8px;flex-wrap:wrap;margin-bottom:12px;}
.chip{padding:5px 10px;border:1px solid var(--border);border-radius:20px;font-size:11px;cursor:pointer;color:var(--muted);}
.chip.active{background:var(--accent);color:#000;border-color:var(--accent);}
table{width:100%;border-collapse:collapse;font-size:10px;}
th{text-align:left;padding:8px 10px;border-bottom:2px solid var(--border);color:var(--muted);font-weight:600;font-size:9px;text-transform:uppercase;cursor:pointer;}
td{padding:7px 10px;border-bottom:1px solid var(--border);}
tr:hover td{background:var(--bg3);}
.state-pill{padding:2px 6px;border-radius:4px;font-size:9px;font-weight:700;}
.s-Assoc{background:rgba(34,197,94,.12);color:var(--success);}
.s-Auth{background:rgba(0,212,255,.12);color:var(--accent);}
.s-Unauth{background:rgba(100,116,139,.1);color:var(--muted);}
.s-Error{background:rgba(239,68,68,.12);color:var(--danger);}
.anom-pill{padding:1px 5px;border-radius:3px;font-size:8px;font-weight:600;background:rgba(239,68,68,.12);color:var(--danger);}
.cap-pills{display:flex;gap:2px;flex-wrap:wrap;}
.cap-pill{padding:1px 4px;border-radius:3px;font-size:7px;background:rgba(0,212,255,.08);color:var(--muted);}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;margin-top:12px;}
@media(max-width:700px){.hero{grid-template-columns:1fr 1fr};table{font-size:9px}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/ds-state" class="active">DS State</a>
    <a href="/auth-sequence">Auth Seq</a>
    <a href="/stations">Stations</a>
  </nav>
</div>
<div class="container">
  <h1>DS State Tracker</h1>
  <p class="subtitle">Monitors 802.11 Distribution System state machine transitions per client. Detects class-2/3 frame violations, deauthentication storms, and state anomalies for all observed client/AP pairs.</p>

  <div class="hero">
    <div class="stat"><label>Tracked Pairs</label><div class="val" id="h-cnt">0</div></div>
    <div class="stat"><label>Total Deauths</label><div class="val" id="h-da" style="color:var(--danger)">0</div></div>
    <div class="stat"><label>Total Assocs</label><div class="val" id="h-as" style="color:var(--success)">0</div></div>
    <div class="stat"><label>Anomalies</label><div class="val" id="h-anom" style="color:var(--warn)">0</div></div>
  </div>

  <div class="section">
    <h3>Client DS State Table</h3>
    <div class="filter-row">
      <span class="chip active" id="f-all"   onclick="setFilter('all')">All</span>
      <span class="chip"        id="f-assoc" onclick="setFilter('assoc')">Associated</span>
      <span class="chip"        id="f-anom"  onclick="setFilter('anom')">Anomalies</span>
      <span class="chip"        id="f-storm" onclick="setFilter('storm')">Deauth Storm</span>
    </div>
    <div style="overflow-x:auto">
    <table>
      <thead><tr>
        <th onclick="sortBy('mac')">Client MAC</th>
        <th onclick="sortBy('ssid')">AP SSID</th>
        <th onclick="sortBy('state')">State</th>
        <th onclick="sortBy('aid')">AID</th>
        <th onclick="sortBy('assoc_ok')">Assoc</th>
        <th onclick="sortBy('auth_ok')">Auth</th>
        <th onclick="sortBy('deauth_rx')">Deauth RX</th>
        <th onclick="sortBy('deauth_tx')">Deauth TX</th>
        <th>Reason</th>
        <th>Caps</th>
        <th>Anomalies</th>
      </tr></thead>
      <tbody id="tbl-body"></tbody>
    </table>
    </div>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>
</div>

<script>
let g_data = [];
let g_sort = {key:'deauth_rx', asc:false};
let g_filter = 'all';

function setFilter(f) {
  g_filter = f;
  ['all','assoc','anom','storm'].forEach(id => {
    const el = document.getElementById('f-' + id);
    if (el) el.className = 'chip' + (f === id ? ' active' : '');
  });
  renderTable();
}

function sortBy(k) {
  if (g_sort.key === k) g_sort.asc = !g_sort.asc;
  else { g_sort.key = k; g_sort.asc = false; }
  renderTable();
}

function stateClass(s) {
  if (s === 'Assoc')  return 's-Assoc';
  if (s === 'Auth')   return 's-Auth';
  if (s === 'Unauth') return 's-Unauth';
  return 's-Error';
}

function capPills(c) {
  const caps = [];
  if (c.cap_qos)     caps.push('QoS');
  if (c.cap_privacy) caps.push('Priv');
  if (c.cap_apsd)    caps.push('APSD');
  if (c.cap_rm)      caps.push('RM');
  if (!caps.length) return '—';
  return '<div class="cap-pills">' + caps.map(x => `<span class="cap-pill">${x}</span>`).join('') + '</div>';
}

function anomPills(c) {
  const a = [];
  if (c.class2_viol)    a.push('Class2');
  if (c.class3_viol)    a.push('Class3');
  if (c.deauth_storm)   a.push('Storm×'+c.deauth_storm_cnt);
  if (!a.length) return '—';
  return a.map(x => `<span class="anom-pill">${x}</span>`).join(' ');
}

function renderTable() {
  let rows = g_data.filter(c => {
    if (g_filter === 'assoc'  && c.state !== 'Assoc')   return false;
    if (g_filter === 'anom'   && !c.class2_viol && !c.class3_viol && !c.deauth_storm) return false;
    if (g_filter === 'storm'  && !c.deauth_storm) return false;
    return true;
  });
  const k = g_sort.key, asc = g_sort.asc;
  rows.sort((a,b) => {
    const va = typeof a[k] === 'string' ? a[k] : Number(a[k]||0);
    const vb = typeof b[k] === 'string' ? b[k] : Number(b[k]||0);
    return asc ? (va > vb ? 1 : -1) : (vb > va ? 1 : -1);
  });

  document.getElementById('tbl-body').innerHTML = rows.length
    ? rows.map(c => `<tr>
        <td style="font-family:monospace">${c.mac}</td>
        <td>${c.ssid||c.ap||'—'}</td>
        <td><span class="state-pill ${stateClass(c.state)}">${c.state}</span></td>
        <td style="color:var(--muted)">${c.aid||'—'}</td>
        <td style="color:var(--success)">${c.assoc_ok}</td>
        <td>${c.auth_ok}</td>
        <td style="color:${c.deauth_rx>0?'var(--danger)':'var(--muted)'}">${c.deauth_rx}</td>
        <td style="color:${c.deauth_tx>5?'var(--danger)':c.deauth_tx>0?'var(--warn)':'var(--muted)'}">${c.deauth_tx}</td>
        <td style="font-size:9px;color:var(--muted)" title="${c.deauth_reason}">${c.deauth_reason>0?c.deauth_reason_str:'—'}</td>
        <td>${capPills(c)}</td>
        <td>${anomPills(c)}</td>
      </tr>`).join('')
    : `<tr><td colspan="11" style="text-align:center;color:var(--muted);padding:20px">No DS state data.</td></tr>`;
}

async function refresh() {
  const r = await fetch('/api/dst/all').then(x => x.json()).catch(()=>({}));
  g_data = r.entries || [];
  document.getElementById('h-cnt').textContent  = r.count || 0;
  document.getElementById('h-da').textContent   = r.total_deauths || 0;
  document.getElementById('h-as').textContent   = r.total_assocs || 0;
  document.getElementById('h-anom').textContent = r.anomalies || 0;
  renderTable();
}

async function clearData() {
  await fetch('/api/dst/clear', {method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 2000);
</script>
</body>
</html>
)rawhtml";

inline void wudst_routes() {
  server.on("/ds-state", HTTP_GET, []() {
    server.send_P(200, "text/html", WUDST_PAGE);
  });
}

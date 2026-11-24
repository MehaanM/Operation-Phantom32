#pragma once
// webui_client_tracker.h — Client Station Tracker UI.
// Route: /clients. Prefix: wuclt_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUCLT_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Client Tracker — Phantom32</title>
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
.container{max-width:1300px;margin:0 auto;padding:20px 16px;}
h1{font-size:22px;font-weight:700;margin-bottom:4px;}
.subtitle{color:var(--muted);font-size:13px;margin-bottom:20px;}
.hero{display:grid;grid-template-columns:repeat(5,1fr);gap:10px;margin-bottom:16px;}
.stat{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:13px;}
.stat label{font-size:10px;color:var(--muted);text-transform:uppercase;}
.stat .val{font-size:22px;font-weight:800;color:var(--accent);margin-top:3px;}
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:18px;margin-bottom:14px;}
.section h3{font-size:12px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:14px;}
.filter-row{display:flex;gap:8px;margin-bottom:12px;flex-wrap:wrap;align-items:center;}
.filter-input{background:var(--bg3);border:1px solid var(--border);border-radius:5px;color:var(--text);padding:7px 12px;font-size:11px;outline:none;min-width:200px;}
.filter-input:focus{border-color:var(--accent);}
.filter-sel{background:var(--bg3);border:1px solid var(--border);border-radius:5px;color:var(--text);padding:7px 10px;font-size:11px;outline:none;}
.filter-sel:focus{border-color:var(--accent);}
table{width:100%;border-collapse:collapse;font-size:10px;}
th{text-align:left;padding:7px 10px;border-bottom:2px solid var(--border);color:var(--muted);font-size:9px;text-transform:uppercase;cursor:pointer;}
th:hover{color:var(--text);}
td{padding:6px 10px;border-bottom:1px solid var(--border);}
tr:hover td{background:var(--bg3);}
.state-pill{padding:2px 6px;border-radius:3px;font-size:8px;font-weight:700;}
.s-Associated{background:rgba(34,197,94,.1);color:var(--success);}
.s-Probing{background:rgba(0,212,255,.1);color:var(--accent);}
.s-Roaming{background:rgba(245,158,11,.1);color:var(--warn);}
.s-PowerSave{background:rgba(100,116,139,.1);color:var(--muted);}
.s-Inactive{background:rgba(100,116,139,.06);color:#3a4560;}
.s-Deauthed{background:rgba(239,68,68,.1);color:var(--danger);}
.s-Associating{background:rgba(0,212,255,.08);color:var(--accent);}
.flag-pill{padding:1px 4px;border-radius:2px;font-size:7px;font-weight:700;margin-right:2px;}
.f-flooder{background:rgba(239,68,68,.15);color:var(--danger);}
.f-sweeper{background:rgba(245,158,11,.15);color:var(--warn);}
.f-ps{background:rgba(100,116,139,.15);color:var(--muted);}
.cap-badge{padding:1px 3px;border-radius:2px;font-size:7px;background:rgba(0,212,255,.08);color:var(--accent);margin-right:1px;}
.mini-rssi{display:flex;align-items:flex-end;gap:1px;height:16px;}
.mr-bar{width:3px;min-height:1px;border-radius:1px 1px 0 0;}
.probe-tags{display:flex;gap:2px;flex-wrap:wrap;max-width:150px;}
.probe-tag{padding:0 3px;border-radius:2px;font-size:7px;background:rgba(0,212,255,.06);color:var(--muted);white-space:nowrap;}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;margin-top:12px;align-items:center;}
.ts-label{font-size:9px;color:var(--muted);margin-left:auto;}
@media(max-width:900px){.hero{grid-template-columns:repeat(3,1fr)}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/clients" class="active">Clients</a>
    <a href="/topology">Topology</a>
    <a href="/probe-flood">Probes</a>
    <a href="/vendor-lookup">Vendor</a>
  </nav>
</div>
<div class="container">
  <h1>Client Station Tracker</h1>
  <p class="subtitle">Passively tracks 802.11 client devices: association state, roaming events, power-save mode, capability flags, probe history, and anomaly detection (deauth flooders, probe sweepers, PS abusers).</p>

  <div class="hero">
    <div class="stat"><label>Total Seen</label><div class="val" id="h-total">0</div></div>
    <div class="stat"><label>Active Now</label><div class="val" id="h-active" style="color:var(--success)">0</div></div>
    <div class="stat"><label>Roam Events</label><div class="val" id="h-roam" style="color:var(--warn)">0</div></div>
    <div class="stat"><label>Deauths</label><div class="val" id="h-deauth" style="color:var(--danger)">0</div></div>
    <div class="stat"><label>Probe SSIDs</label><div class="val" id="h-probes">0</div></div>
  </div>

  <div class="section">
    <h3>Clients</h3>
    <div class="filter-row">
      <input class="filter-input" id="filter-mac" placeholder="Filter by MAC or vendor…" oninput="applyFilter()">
      <select class="filter-sel" id="filter-state" onchange="applyFilter()">
        <option value="">All States</option>
        <option value="Associated">Associated</option>
        <option value="Probing">Probing</option>
        <option value="Roaming">Roaming</option>
        <option value="PowerSave">Power Save</option>
        <option value="Deauthed">Deauthed</option>
        <option value="Inactive">Inactive</option>
      </select>
      <select class="filter-sel" id="filter-flag" onchange="applyFilter()">
        <option value="">All</option>
        <option value="flooder">Deauth Flooder</option>
        <option value="sweeper">Probe Sweeper</option>
        <option value="ps_abuser">PS Abuser</option>
      </select>
    </div>
    <div style="overflow-x:auto">
    <table>
      <thead><tr>
        <th>MAC</th><th>Vendor</th><th>State</th>
        <th>SSID (Assoc)</th><th>CH</th><th>RSSI</th>
        <th>Frames</th><th>Roams</th><th>Caps</th>
        <th>Probes</th><th>Flags</th><th>RSSI Hist</th><th>Seen</th>
      </tr></thead>
      <tbody id="clt-body"></tbody>
    </table>
    </div>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
      <span class="ts-label">Updated: <span id="ts">—</span></span>
    </div>
  </div>
</div>

<script>
let allClients = [];
function fmt(n) { return Number(n).toLocaleString(); }

function reltime(ts) {
  if (!ts) return '—';
  const s = Math.floor(Date.now()/1000) - ts;
  if (s < 60) return s + 's';
  if (s < 3600) return Math.floor(s/60) + 'm';
  return Math.floor(s/3600) + 'h';
}

function rssiColor(r) {
  if (r >= -55) return 'var(--success)';
  if (r >= -70) return 'var(--warn)';
  return 'var(--danger)';
}

function miniRssi(hist) {
  if (!hist || !hist.length) return '';
  const vals = hist.filter(v => v !== 0);
  if (!vals.length) return '';
  const max = Math.max(...vals.map(v => -v));
  return `<div class="mini-rssi">` +
    hist.map(v => v === 0 ? `<div class="mr-bar" style="height:1px;background:var(--border)"></div>` :
      `<div class="mr-bar" style="height:${Math.max(2,((-v)/max)*16)}px;background:${rssiColor(v)}"></div>`).join('') +
    `</div>`;
}

function probeTags(probes) {
  if (!probes || !probes.length) return '—';
  return `<div class="probe-tags">` + probes.map(p => `<span class="probe-tag">${p}</span>`).join('') + `</div>`;
}

function caps(c) {
  let out = '';
  if (c.ht)  out += '<span class="cap-badge">n</span>';
  if (c.vht) out += '<span class="cap-badge">ac</span>';
  if (c.he)  out += '<span class="cap-badge">ax</span>';
  if (c.pmf) out += '<span class="cap-badge">PMF</span>';
  if (c.btm) out += '<span class="cap-badge">BTM</span>';
  return out || '—';
}

function flags(c) {
  let out = '';
  if (c.flooder)   out += '<span class="flag-pill f-flooder">FLOODER</span>';
  if (c.sweeper)   out += '<span class="flag-pill f-sweeper">SWEEP</span>';
  if (c.ps_abuser) out += '<span class="flag-pill f-ps">PS-ABU</span>';
  return out;
}

function renderTable(clients) {
  const mac_f   = document.getElementById('filter-mac').value.toLowerCase();
  const state_f = document.getElementById('filter-state').value;
  const flag_f  = document.getElementById('filter-flag').value;

  const rows = clients.filter(c => {
    if (mac_f && !c.mac.toLowerCase().includes(mac_f) && !c.vendor.toLowerCase().includes(mac_f)) return false;
    if (state_f && c.state !== state_f) return false;
    if (flag_f === 'flooder' && !c.flooder) return false;
    if (flag_f === 'sweeper' && !c.sweeper) return false;
    if (flag_f === 'ps_abuser' && !c.ps_abuser) return false;
    return true;
  });

  document.getElementById('clt-body').innerHTML = rows.length
    ? rows.map(c => `<tr>
        <td style="font-family:monospace;font-size:10px">${c.mac}</td>
        <td style="color:var(--muted)">${c.vendor}</td>
        <td><span class="state-pill s-${c.state}">${c.state}</span></td>
        <td style="max-width:100px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap">${c.ssid||'—'}</td>
        <td>${c.ch||'—'}</td>
        <td style="color:${rssiColor(c.rssi)}">${c.rssi}</td>
        <td>${fmt(c.frames)}</td>
        <td style="color:${c.roams>0?'var(--warn)':'var(--muted)'}">${c.roams}</td>
        <td>${caps(c)}</td>
        <td>${probeTags(c.probes)}</td>
        <td>${flags(c)}</td>
        <td>${miniRssi(c.rssi_hist)}</td>
        <td style="color:var(--muted)">${reltime(c.last_ts)}</td>
      </tr>`).join('')
    : '<tr><td colspan="13" style="text-align:center;color:var(--muted);padding:16px">No clients tracked yet.</td></tr>';
}

function applyFilter() { renderTable(allClients); }

async function refresh() {
  const r = await fetch('/api/clt/all').then(x=>x.json()).catch(()=>({}));
  allClients = r.clients || [];

  document.getElementById('h-total').textContent   = r.total_seen || 0;
  document.getElementById('h-active').textContent  = r.active || 0;
  document.getElementById('h-roam').textContent    = r.total_roam || 0;
  document.getElementById('h-deauth').textContent  = r.total_deauth || 0;
  document.getElementById('h-probes').textContent  = r.total_probes || 0;
  document.getElementById('ts').textContent        = new Date().toLocaleTimeString();

  renderTable(allClients);
}

async function clearData() {
  await fetch('/api/clt/clear', {method:'POST'});
  allClients = [];
  renderTable([]);
}

refresh();
setInterval(refresh, 3000);
</script>
</body>
</html>
)rawhtml";

inline void wuclt_routes() {
  server.on("/clients", HTTP_GET, []() {
    server.send_P(200, "text/html", WUCLT_PAGE);
  });
}

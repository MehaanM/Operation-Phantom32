#pragma once
// webui_client_assoc.h — Client association history tracker UI.
// Route: /client-assoc. Prefix: wucat_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUCAT_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Client Assoc History — Phantom32</title>
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
table{width:100%;border-collapse:collapse;font-size:11px;}
th{text-align:left;padding:7px 8px;font-size:9px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;cursor:pointer;}
th:hover{color:var(--text);}
td{padding:6px 8px;border-bottom:1px solid var(--border);}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
.badge{padding:1px 5px;border-radius:3px;font-size:9px;font-weight:700;display:inline-block;}
.badge-assoc{background:rgba(34,197,94,.12);color:var(--success);}
.badge-free{background:rgba(100,116,139,.14);color:var(--muted);}
.badge-roam{background:rgba(245,158,11,.12);color:var(--warn);}
details{margin-top:4px;}
details summary{font-size:10px;color:var(--muted);cursor:pointer;user-select:none;}
details summary:hover{color:var(--text);}
.hist-entry{font-size:9px;color:var(--muted);padding:2px 8px;border-left:2px solid var(--border);}
.hist-entry.roam{border-left-color:var(--warn);}
.hist-entry.reassoc{border-left-color:var(--accent);}
.onair-bar{height:4px;border-radius:2px;background:linear-gradient(90deg,var(--accent),var(--success));margin-top:3px;}
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
    <a href="/station-behavior">Behavior</a>
    <a href="/client-assoc" class="active">Assoc History</a>
    <a href="/ds-state">DS State</a>
  </nav>
</div>
<div class="container">
  <h1>Client Association History</h1>
  <p class="subtitle">Tracks which APs each client has associated with. Roaming events, re-associations, and time on air are computed from observed 802.11 management frames.</p>

  <div class="hero">
    <div class="stat"><label>Clients</label><div class="val" id="h-cnt">0</div></div>
    <div class="stat"><label>Associated</label><div class="val" id="h-assoc" style="color:var(--success)">0</div></div>
    <div class="stat"><label>Roam Events</label><div class="val" id="h-roams" style="color:var(--warn)">0</div></div>
    <div class="stat"><label>Heavy Roamers</label><div class="val" id="h-heavy">0</div></div>
  </div>

  <div class="section">
    <h3>Client Association Table</h3>
    <table>
      <thead>
        <tr>
          <th onclick="sortTable('mac')">Client MAC</th>
          <th onclick="sortTable('assoc')">Status</th>
          <th>Current AP / SSID</th>
          <th>CH</th>
          <th onclick="sortTable('roams')">Roams</th>
          <th onclick="sortTable('on_air_s')">On Air</th>
          <th>History</th>
        </tr>
      </thead>
      <tbody id="cli-tbody">
        <tr><td colspan="7" style="color:var(--muted);text-align:center">Waiting for assoc frames…</td></tr>
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
let g_sort = {key:'roams', asc:false};

function formatDur(s) {
  if (!s) return '—';
  if (s < 60) return s + 's';
  if (s < 3600) return Math.floor(s/60) + 'm' + (s%60) + 's';
  return Math.floor(s/3600) + 'h' + Math.floor((s%3600)/60) + 'm';
}

function rssiColor(r) {
  if (r >= -60) return 'var(--success)';
  if (r >= -75) return 'var(--warn)';
  return 'var(--danger)';
}

function histDetails(c) {
  if (!c.history || !c.history.length) return '';
  const entries = [...c.history].reverse();
  const rows = entries.map(e => `<div class="hist-entry ${e.reassoc?'reassoc':'roam'}">
    ${new Date(e.ts*1000).toLocaleTimeString()} — ${e.ssid||'Hidden'}
    <code style="font-size:8px">${e.ap}</code> CH${e.ch} ${e.rssi}dBm
    ${e.reassoc ? '(re-assoc)' : '(roam)'}
  </div>`).join('');
  return `<details><summary>History (${c.history.length})</summary>${rows}</details>`;
}

function sortTable(key) {
  if (g_sort.key === key) g_sort.asc = !g_sort.asc;
  else { g_sort.key = key; g_sort.asc = false; }
  renderTable();
}

function renderTable() {
  const sorted = [...g_clients].sort((a,b) => {
    const va = a[g_sort.key], vb = b[g_sort.key];
    if (typeof va === 'number') return g_sort.asc ? va - vb : vb - va;
    return g_sort.asc ? String(va).localeCompare(String(vb)) : String(vb).localeCompare(String(va));
  });
  document.getElementById('cli-tbody').innerHTML = sorted.length
    ? sorted.map(c => `<tr>
        <td><code style="font-size:10px">${c.mac}</code></td>
        <td>${c.assoc
          ? '<span class="badge badge-assoc">● Assoc</span>'
          : '<span class="badge badge-free">○ Free</span>'}</td>
        <td style="font-size:10px">
          <code>${c.cur_ap}</code>
          ${c.cur_ssid ? `<br><span style="color:var(--muted)">${c.cur_ssid}</span>` : ''}
        </td>
        <td>${c.cur_ch || '—'}</td>
        <td>${c.roams > 2
          ? `<span class="badge badge-roam">⟳ ${c.roams}</span>`
          : c.roams}</td>
        <td style="color:var(--muted)">${formatDur(c.on_air_s)}</td>
        <td>${histDetails(c)}</td>
      </tr>`).join('')
    : '<tr><td colspan="7" style="color:var(--muted);text-align:center">No data.</td></tr>';
}

async function refresh() {
  const r = await fetch('/api/cat/all').then(x => x.json()).catch(() => ({}));
  g_clients = r.clients || [];
  document.getElementById('h-cnt').textContent   = r.count || 0;
  document.getElementById('h-assoc').textContent = g_clients.filter(c => c.assoc).length;
  const totalRoams = g_clients.reduce((s,c) => s + c.roams, 0);
  document.getElementById('h-roams').textContent  = totalRoams;
  document.getElementById('h-heavy').textContent  = g_clients.filter(c => c.roams >= 3).length;
  renderTable();
}

async function clearData() {
  await fetch('/api/cat/clear', {method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 3000);
</script>
</body>
</html>
)rawhtml";

inline void wucat_routes() {
  server.on("/client-assoc", HTTP_GET, []() { server.send_P(200, "text/html", WUCAT_PAGE); });
}

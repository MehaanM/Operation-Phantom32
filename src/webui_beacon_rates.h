#pragma once
// webui_beacon_rates.h — Beacon rate tracker UI.
// Route: /beacon-rates. Prefix: wubrt_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUBRT_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Beacon Rates — Phantom32</title>
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
.search-box{width:100%;padding:8px 12px;background:var(--bg3);border:1px solid var(--border);
  border-radius:6px;color:var(--text);font-size:12px;margin-bottom:12px;outline:none;}
.search-box:focus{border-color:var(--accent);}
table{width:100%;border-collapse:collapse;font-size:11px;}
th{text-align:left;padding:8px 10px;border-bottom:2px solid var(--border);color:var(--muted);font-weight:600;font-size:10px;text-transform:uppercase;cursor:pointer;}
th:hover{color:var(--text);}
td{padding:7px 10px;border-bottom:1px solid var(--border);}
tr:hover td{background:var(--bg3);}
.anom-cell{display:flex;gap:3px;flex-wrap:wrap;}
.anom-pill{padding:1px 5px;border-radius:3px;font-size:8px;font-weight:700;}
.anom-flood{background:rgba(239,68,68,.15);color:var(--danger);}
.anom-irregular{background:rgba(245,158,11,.15);color:var(--warn);}
.anom-silent{background:rgba(100,116,139,.15);color:var(--muted);}
.anom-interval{background:rgba(0,212,255,.12);color:var(--accent);}
.mini-hist{display:flex;align-items:flex-end;gap:1px;height:16px;}
.mini-bar{width:3px;border-radius:1px 1px 0 0;background:var(--accent);}
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
    <a href="/rogue-ap">Rogue AP</a>
    <a href="/beacon-rates" class="active">Beacon Rates</a>
    <a href="/spectrum">Spectrum</a>
  </nav>
</div>
<div class="container">
  <h1>Beacon Rate Tracker</h1>
  <p class="subtitle">Monitors per-AP beacon rates, timing intervals, jitter, and missed beacons. Detects beacon flooding (rogue deauth tools), irregular intervals, and silent APs.</p>

  <div class="hero">
    <div class="stat"><label>Tracked APs</label><div class="val" id="h-cnt">0</div></div>
    <div class="stat"><label>Total Beacons</label><div class="val" id="h-tot" style="font-size:14px">0</div></div>
    <div class="stat"><label>Flood Events</label><div class="val" id="h-flood" style="color:var(--danger)">0</div></div>
    <div class="stat"><label>Anomalies</label><div class="val" id="h-anom" style="color:var(--warn)">0</div></div>
  </div>

  <div class="section">
    <h3>Beacon Rate Table</h3>
    <input class="search-box" id="search" placeholder="Search BSSID or SSID…" oninput="renderTable()">
    <div style="overflow-x:auto">
    <table>
      <thead><tr>
        <th onclick="sortBy('ssid')">SSID</th>
        <th onclick="sortBy('bssid')">BSSID</th>
        <th onclick="sortBy('ch')">CH</th>
        <th onclick="sortBy('rssi')">RSSI</th>
        <th onclick="sortBy('bcn_int')">Interval</th>
        <th onclick="sortBy('total')">Beacons</th>
        <th onclick="sortBy('missed')">Missed</th>
        <th onclick="sortBy('avg_gap')">Avg Gap</th>
        <th onclick="sortBy('jitter')">Jitter</th>
        <th>Rate Hist</th>
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
let g_sort = {key:'total', asc:false};

const ANOM_BITS = [
  {bit:0x01, cls:'anom-flood',    label:'FLOOD'},
  {bit:0x02, cls:'anom-irregular',label:'IRREGULAR'},
  {bit:0x04, cls:'anom-silent',   label:'SILENT'},
  {bit:0x08, cls:'anom-interval', label:'INT_CHANGE'},
  {bit:0x10, cls:'anom-interval', label:'SSID_CHANGE'},
];

function anomPills(flags) {
  if (!flags) return '—';
  return ANOM_BITS.filter(a => flags & a.bit)
    .map(a => `<span class="anom-pill ${a.cls}">${a.label}</span>`)
    .join('');
}

function miniHist(hist) {
  if (!hist || !hist.length) return '';
  const max = Math.max(...hist, 1);
  return '<div class="mini-hist">' + hist.map(v => {
    const h = Math.max(1, Math.round((v/max)*14));
    return `<div class="mini-bar" style="height:${h}px"></div>`;
  }).join('') + '</div>';
}

function rssiColor(r) {
  return r >= -60 ? 'var(--success)' : r >= -75 ? 'var(--warn)' : 'var(--danger)';
}

function sortBy(key) {
  if (g_sort.key === key) g_sort.asc = !g_sort.asc;
  else { g_sort.key = key; g_sort.asc = false; }
  renderTable();
}

function renderTable() {
  const q = document.getElementById('search').value.toLowerCase();
  let rows = g_data.filter(a =>
    a.ssid.toLowerCase().includes(q) ||
    a.bssid.toLowerCase().includes(q)
  );
  const k = g_sort.key, asc = g_sort.asc;
  rows.sort((a,b) => {
    const va = typeof a[k] === 'string' ? a[k] : Number(a[k]||0);
    const vb = typeof b[k] === 'string' ? b[k] : Number(b[k]||0);
    return asc ? (va > vb ? 1 : -1) : (vb > va ? 1 : -1);
  });

  document.getElementById('tbl-body').innerHTML = rows.length
    ? rows.map(a => `<tr>
        <td>${a.ssid || '<em style="color:var(--muted)">[hidden]</em>'}</td>
        <td style="font-family:monospace">${a.bssid}</td>
        <td>${a.ch}</td>
        <td style="color:${rssiColor(a.rssi)}">${a.rssi} dBm</td>
        <td>${a.bcn_int} TU (${a.expected_gap} ms)</td>
        <td>${a.total.toLocaleString()}</td>
        <td style="color:${a.missed>0?'var(--warn)':'var(--muted)'}">${a.missed}</td>
        <td>${a.avg_gap} ms</td>
        <td style="color:${a.jitter>50?'var(--danger)':a.jitter>20?'var(--warn)':'var(--muted)'}">${a.jitter} ms</td>
        <td>${miniHist(a.rate_hist)}</td>
        <td><div class="anom-cell">${anomPills(a.anomaly)}</div></td>
      </tr>`).join('')
    : `<tr><td colspan="11" style="text-align:center;color:var(--muted);padding:20px">No AP beacons tracked.</td></tr>`;
}

async function refresh() {
  const r = await fetch('/api/brt/all').then(x => x.json()).catch(()=>({}));
  g_data = r.aps || [];
  document.getElementById('h-cnt').textContent   = r.count || 0;
  document.getElementById('h-tot').textContent   = (r.total_beacons||0).toLocaleString();
  document.getElementById('h-flood').textContent = r.flood_events || 0;
  document.getElementById('h-anom').textContent  = g_data.filter(a => a.anomaly).length;
  renderTable();
}

async function clearData() {
  await fetch('/api/brt/clear', {method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 2000);
</script>
</body>
</html>
)rawhtml";

inline void wubrt_routes() {
  server.on("/beacon-rates", HTTP_GET, []() {
    server.send_P(200, "text/html", WUBRT_PAGE);
  });
}

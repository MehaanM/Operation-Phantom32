#pragma once
// webui_scan_cache.h — Scan result cache UI.
// Route: /scan-cache. Prefix: wusrc_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUSRC_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Scan Cache — Phantom32</title>
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
.hero{display:grid;grid-template-columns:repeat(5,1fr);gap:10px;margin-bottom:16px;}
.stat{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:13px;}
.stat label{font-size:10px;color:var(--muted);text-transform:uppercase;}
.stat .val{font-size:22px;font-weight:800;color:var(--accent);margin-top:3px;}
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:18px;margin-bottom:14px;}
.section h3{font-size:12px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:12px;}
.filter-row{display:flex;gap:8px;flex-wrap:wrap;margin-bottom:10px;}
.chip{padding:5px 10px;border:1px solid var(--border);border-radius:20px;font-size:11px;cursor:pointer;color:var(--muted);}
.chip.active{background:var(--accent);color:#000;border-color:var(--accent);}
.search{flex:1;min-width:140px;padding:5px 10px;background:var(--bg3);border:1px solid var(--border);
  border-radius:5px;color:var(--text);font-size:11px;outline:none;}
.search:focus{border-color:var(--accent);}
table{width:100%;border-collapse:collapse;font-size:10px;}
th{text-align:left;padding:7px 9px;border-bottom:2px solid var(--border);color:var(--muted);font-weight:600;font-size:9px;text-transform:uppercase;cursor:pointer;white-space:nowrap;}
td{padding:6px 9px;border-bottom:1px solid var(--border);}
tr:hover td{background:var(--bg3);}
.rssi-bar{width:40px;height:6px;background:var(--border);border-radius:3px;overflow:hidden;display:inline-block;}
.rssi-fill{height:100%;border-radius:3px;}
.sec-pill{padding:1px 5px;border-radius:3px;font-size:8px;font-weight:700;}
.s-Open{background:rgba(239,68,68,.12);color:var(--danger);}
.s-WEP{background:rgba(239,68,68,.2);color:var(--danger);}
.s-WPA{background:rgba(245,158,11,.12);color:var(--warn);}
.s-WPA2{background:rgba(0,212,255,.08);color:var(--accent);}
.s-WPA3{background:rgba(34,197,94,.12);color:var(--success);}
.s-OWE{background:rgba(34,197,94,.08);color:var(--success);}
.gen-pill{padding:1px 5px;border-radius:3px;font-size:8px;background:rgba(100,116,139,.1);color:var(--muted);}
.gen-WiFi6{background:rgba(0,212,255,.12);color:var(--accent);}
.gen-WiFi5{background:rgba(34,197,94,.08);color:var(--success);}
.gen-WiFi4{background:rgba(245,158,11,.08);color:var(--warn);}
.mini-chart{display:flex;align-items:flex-end;gap:1px;height:12px;}
.mini-bar{flex:1;border-radius:1px 1px 0 0;min-height:1px;}
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
    <a href="/scan-cache" class="active">Scan Cache</a>
    <a href="/ssid-pool">SSID Pool</a>
    <a href="/stations">Stations</a>
  </nav>
</div>
<div class="container">
  <h1>Scan Result Cache</h1>
  <p class="subtitle">Passive scan result cache with RSSI history, security profiling, Wi-Fi generation, and channel width. Entries age out after 5 minutes of silence.</p>

  <div class="hero">
    <div class="stat"><label>Total APs</label><div class="val" id="h-cnt">0</div></div>
    <div class="stat"><label>Open Networks</label><div class="val" id="h-open" style="color:var(--danger)">0</div></div>
    <div class="stat"><label>Wi-Fi 6 APs</label><div class="val" id="h-ax" style="color:var(--success)">0</div></div>
    <div class="stat"><label>Captive Portal</label><div class="val" id="h-cap" style="color:var(--warn)">0</div></div>
    <div class="stat"><label>Hidden SSIDs</label><div class="val" id="h-hid" style="color:var(--muted)">0</div></div>
  </div>

  <div class="section">
    <h3>Scan Results</h3>
    <div class="filter-row">
      <span class="chip active" id="f-all"  onclick="setFilter('all')">All</span>
      <span class="chip"        id="f-2g"   onclick="setFilter('2g')">2.4 GHz</span>
      <span class="chip"        id="f-5g"   onclick="setFilter('5g')">5 GHz</span>
      <span class="chip"        id="f-open" onclick="setFilter('open')">Open</span>
      <span class="chip"        id="f-6"    onclick="setFilter('6')">Wi-Fi 6</span>
      <input class="search" id="search" placeholder="Search SSID / BSSID…" oninput="renderTable()">
    </div>
    <div style="overflow-x:auto">
    <table>
      <thead><tr>
        <th onclick="sortBy('ssid')">SSID</th>
        <th onclick="sortBy('bssid')">BSSID</th>
        <th onclick="sortBy('ch')">CH</th>
        <th onclick="sortBy('rssi')">RSSI</th>
        <th onclick="sortBy('sec')">Security</th>
        <th onclick="sortBy('gen')">Gen</th>
        <th onclick="sortBy('bw')">BW</th>
        <th onclick="sortBy('max_rate')">Rate</th>
        <th onclick="sortBy('ss')">SS</th>
        <th>PMF</th>
        <th onclick="sortBy('beacons')">Beacons</th>
        <th onclick="sortBy('age')">Age</th>
        <th>Signal</th>
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
let g_sort = {key:'rssi', asc:false};
let g_filter = 'all';

function setFilter(f) {
  g_filter = f;
  ['all','2g','5g','open','6'].forEach(id => {
    const el = document.getElementById('f-'+id);
    if (el) el.className = 'chip' + (f===id?' active':'');
  });
  renderTable();
}

function sortBy(k) {
  if (g_sort.key===k) g_sort.asc = !g_sort.asc;
  else {g_sort.key=k; g_sort.asc=false;}
  renderTable();
}

function rssiBar(r) {
  const pct = Math.min(100, Math.max(0, (r + 100) * 2));
  const c = r >= -60 ? 'var(--success)' : r >= -75 ? 'var(--warn)' : 'var(--danger)';
  return `<div class="rssi-bar"><div class="rssi-fill" style="width:${pct}%;background:${c}"></div></div>`;
}

function miniChart(hist) {
  if (!hist || !hist.length) return '';
  const vals = hist.map(v => v + 100);
  const max = Math.max(...vals, 1);
  return vals.map(v => {
    const h = Math.max(1, Math.round((v/max)*10));
    const pct = v/max;
    const c = pct > .5 ? 'var(--success)' : 'var(--warn)';
    return `<div class="mini-bar" style="height:${h}px;background:${c}"></div>`;
  }).join('');
}

function secClass(s) { return 's-' + s.replace(/[^a-zA-Z0-9]/g,''); }
function genClass(g) { return 'gen-' + g.replace(/ /g,''); }

function pmfColor(p) {
  if (p==='Required') return 'var(--success)';
  if (p==='Capable')  return 'var(--warn)';
  return 'var(--muted)';
}

function renderTable() {
  const q = document.getElementById('search').value.toLowerCase();
  let rows = g_data.filter(e => {
    if (g_filter==='2g'   && e.ch > 14) return false;
    if (g_filter==='5g'   && e.ch <= 14) return false;
    if (g_filter==='open' && e.sec !== 'Open') return false;
    if (g_filter==='6'    && e.gen !== 'Wi-Fi 6') return false;
    if (q && !e.ssid.toLowerCase().includes(q) && !e.bssid.toLowerCase().includes(q)) return false;
    return true;
  });
  const k = g_sort.key, asc = g_sort.asc;
  rows.sort((a,b)=>{
    const va = typeof a[k]==='string' ? a[k] : Number(a[k]||0);
    const vb = typeof b[k]==='string' ? b[k] : Number(b[k]||0);
    return asc ? (va>vb?1:-1) : (vb>va?1:-1);
  });
  document.getElementById('tbl-body').innerHTML = rows.length
    ? rows.map(e => `<tr>
        <td>${e.hidden ? '<span style="color:var(--muted)">Hidden</span>' : e.ssid||'—'}${e.captive?' <span style="font-size:8px;color:var(--warn)">Captive</span>':''}</td>
        <td style="font-family:monospace;font-size:9px">${e.bssid}</td>
        <td>${e.ch}</td>
        <td style="color:${e.rssi>=-60?'var(--success)':e.rssi>=-75?'var(--warn)':'var(--danger)'}"><strong>${e.rssi}</strong><span style="color:var(--muted);font-size:9px"> avg:${e.rssi_avg}</span></td>
        <td><span class="sec-pill ${secClass(e.sec)}">${e.sec}</span></td>
        <td><span class="gen-pill ${genClass(e.gen)}">${e.gen}</span></td>
        <td style="color:var(--muted)">${e.bw||'—'}MHz</td>
        <td>${e.max_rate||'—'} Mbps</td>
        <td>${e.ss||'—'}</td>
        <td style="color:${pmfColor(e.pmf)};font-size:9px">${e.pmf}</td>
        <td style="color:var(--muted)">${e.beacons.toLocaleString()}</td>
        <td style="color:var(--muted)">${e.age}s</td>
        <td><div class="mini-chart">${miniChart(e.rssi_hist)}</div></td>
      </tr>`).join('')
    : `<tr><td colspan="13" style="text-align:center;color:var(--muted);padding:16px">No scan results cached.</td></tr>`;
}

async function refresh() {
  const r = await fetch('/api/src/all').then(x=>x.json()).catch(()=>({}));
  const entries = r.entries || [];
  document.getElementById('h-cnt').textContent  = r.count || 0;
  document.getElementById('h-open').textContent = entries.filter(e=>e.sec==='Open').length;
  document.getElementById('h-ax').textContent   = entries.filter(e=>e.gen==='Wi-Fi 6').length;
  document.getElementById('h-cap').textContent  = entries.filter(e=>e.captive).length;
  document.getElementById('h-hid').textContent  = entries.filter(e=>e.hidden).length;
  g_data = entries;
  renderTable();
}

async function clearData() {
  await fetch('/api/src/clear', {method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 3000);
</script>
</body>
</html>
)rawhtml";

inline void wusrc_routes() {
  server.on("/scan-cache", HTTP_GET, []() { server.send_P(200, "text/html", WUSRC_PAGE); });
}

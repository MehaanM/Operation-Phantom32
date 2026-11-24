#pragma once
// webui_station_profile.h — Station profiler UI.
// Route: /stations. Prefix: wustp_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUSTP_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Station Profiler — Phantom32</title>
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
.section h3{font-size:12px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:14px;}
.filter-row{display:flex;gap:8px;flex-wrap:wrap;margin-bottom:12px;}
.chip{padding:5px 10px;border:1px solid var(--border);border-radius:20px;font-size:11px;cursor:pointer;color:var(--muted);}
.chip.active{background:var(--accent);color:#000;border-color:var(--accent);}
.search-box{flex:1;min-width:200px;padding:7px 12px;background:var(--bg3);border:1px solid var(--border);
  border-radius:6px;color:var(--text);font-size:12px;outline:none;}
.search-box:focus{border-color:var(--accent);}
table{width:100%;border-collapse:collapse;font-size:11px;}
th{text-align:left;padding:8px 10px;border-bottom:2px solid var(--border);color:var(--muted);font-weight:600;font-size:10px;text-transform:uppercase;cursor:pointer;user-select:none;}
th:hover{color:var(--text);}
td{padding:7px 10px;border-bottom:1px solid var(--border);vertical-align:middle;}
tr:hover td{background:var(--bg3);}
.state-pill{padding:2px 6px;border-radius:4px;font-size:9px;font-weight:700;display:inline-block;}
.s-Probing{background:rgba(100,116,139,.15);color:var(--muted);}
.s-Associated{background:rgba(34,197,94,.12);color:var(--success);}
.s-Roaming{background:rgba(0,212,255,.12);color:var(--accent);}
.s-Dormant{background:rgba(239,68,68,.12);color:var(--danger);}
.s-Authenticated{background:rgba(245,158,11,.1);color:var(--warn);}
.cap-pill{padding:1px 5px;border-radius:3px;font-size:8px;background:rgba(0,212,255,.1);color:var(--accent);border:1px solid rgba(0,212,255,.2);}
.rand-pill{background:rgba(245,158,11,.1);color:var(--warn);border:1px solid rgba(245,158,11,.2);}
.probe-pills{display:flex;gap:2px;flex-wrap:wrap;}
.probe-pill{padding:1px 5px;border-radius:3px;font-size:8px;background:var(--bg2);color:var(--muted);border:1px solid var(--border);}
.mini-hist{display:flex;align-items:flex-end;gap:1px;height:14px;}
.mini-bar{width:3px;border-radius:1px 1px 0 0;background:var(--accent);}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;margin-top:12px;}
@media(max-width:900px){.hero{grid-template-columns:repeat(3,1fr)}}
@media(max-width:600px){.hero{grid-template-columns:1fr 1fr};table{font-size:9px}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/stations" class="active">Stations</a>
    <a href="/auth-sequence">Auth Seq</a>
    <a href="/probe-flood">Probe Flood</a>
  </nav>
</div>
<div class="container">
  <h1>Station Profiler</h1>
  <p class="subtitle">Passive per-client station tracker: probe history, association events, roaming detection, power-save state, data volume, and 802.11k/r/v capability flags.</p>

  <div class="hero">
    <div class="stat"><label>Stations</label><div class="val" id="h-cnt">0</div></div>
    <div class="stat"><label>Associated</label><div class="val" id="h-assoc" style="color:var(--success)">0</div></div>
    <div class="stat"><label>Roaming</label><div class="val" id="h-roam" style="color:var(--accent)">0</div></div>
    <div class="stat"><label>Random MACs</label><div class="val" id="h-rand" style="color:var(--warn)">0</div></div>
    <div class="stat"><label>Total Frames</label><div class="val" id="h-fr" style="font-size:14px">0</div></div>
  </div>

  <div class="section">
    <h3>Station Table</h3>
    <div class="filter-row">
      <span class="chip active" id="f-all"  onclick="setFilter('all')">All</span>
      <span class="chip" id="f-assoc"       onclick="setFilter('assoc')">Associated</span>
      <span class="chip" id="f-probing"     onclick="setFilter('probing')">Probing</span>
      <span class="chip" id="f-rand"        onclick="setFilter('rand')">Random MAC</span>
      <input class="search-box" id="search" placeholder="Search MAC or AP SSID…" oninput="renderTable()">
    </div>
    <div style="overflow-x:auto">
    <table>
      <thead><tr>
        <th onclick="sortBy('mac')">MAC</th>
        <th onclick="sortBy('state')">State</th>
        <th onclick="sortBy('rssi')">RSSI</th>
        <th onclick="sortBy('ap_ssid')">AP SSID</th>
        <th onclick="sortBy('ap_ch')">CH</th>
        <th onclick="sortBy('roam_count')">Roams</th>
        <th onclick="sortBy('data')">Frames</th>
        <th>Probes</th>
        <th>Caps</th>
        <th>Rate</th>
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
let g_sort = {key:'data', asc:false};
let g_filter = 'all';

function setFilter(f) {
  g_filter = f;
  ['all','assoc','probing','rand'].forEach(id => {
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

function rssiColor(r) {
  return r >= -60 ? 'var(--success)' : r >= -75 ? 'var(--warn)' : 'var(--danger)';
}

function miniHist(hist) {
  if (!hist || !hist.length) return '';
  const max = Math.max(...hist, 1);
  return '<div class="mini-hist">' + hist.map(v => {
    const h = Math.max(1, Math.round((v/max)*12));
    return `<div class="mini-bar" style="height:${h}px"></div>`;
  }).join('') + '</div>';
}

function probePills(probes) {
  if (!probes || !probes.length) return '—';
  const top = probes.slice().sort((a,b) => b.count - a.count).slice(0,4);
  return '<div class="probe-pills">' + top.map(p =>
    `<span class="probe-pill">${p.ssid||'<wildcard>'} (${p.count})</span>`
  ).join('') + (probes.length > 4 ? `<span class="probe-pill">+${probes.length-4}</span>` : '') + '</div>';
}

function capPills(st) {
  const caps = [];
  if (st.cap_ft)  caps.push('FT');
  if (st.cap_rrm) caps.push('RRM');
  if (st.cap_btm) caps.push('BTM');
  if (st.rand_mac) caps.push('RandMAC');
  if (!caps.length) return '—';
  return caps.map(c =>
    `<span class="cap-pill${c==='RandMAC'?' rand-pill':''}">${c}</span>`
  ).join(' ');
}

function renderTable() {
  const q = document.getElementById('search').value.toLowerCase();
  let rows = g_data.filter(st => {
    if (g_filter === 'assoc'   && st.state !== 'Associated') return false;
    if (g_filter === 'probing' && st.state !== 'Probing') return false;
    if (g_filter === 'rand'    && !st.rand_mac) return false;
    return !q || st.mac.toLowerCase().includes(q) ||
      (st.ap_ssid && st.ap_ssid.toLowerCase().includes(q));
  });
  const k = g_sort.key, asc = g_sort.asc;
  rows.sort((a,b) => {
    const va = typeof a[k] === 'string' ? a[k] : Number(a[k]||0);
    const vb = typeof b[k] === 'string' ? b[k] : Number(b[k]||0);
    return asc ? (va > vb ? 1 : -1) : (vb > va ? 1 : -1);
  });

  document.getElementById('tbl-body').innerHTML = rows.length
    ? rows.map(st => `<tr>
        <td style="font-family:monospace">${st.mac}</td>
        <td><span class="state-pill s-${st.state}">${st.state}</span></td>
        <td style="color:${rssiColor(st.rssi)}">${st.rssi} dBm</td>
        <td>${st.ap_ssid||'—'}</td>
        <td>${st.ap_ch||'—'}</td>
        <td style="color:${st.roam_count>0?'var(--accent)':'var(--muted)'}">${st.roam_count}</td>
        <td>${(st.data||0).toLocaleString()}</td>
        <td>${probePills(st.probes)}</td>
        <td>${capPills(st)}</td>
        <td>${miniHist(st.rate_hist)}</td>
      </tr>`).join('')
    : `<tr><td colspan="10" style="text-align:center;color:var(--muted);padding:20px">No stations observed.</td></tr>`;
}

async function refresh() {
  const r = await fetch('/api/stp/all').then(x => x.json()).catch(()=>({}));
  g_data = r.stations || [];
  document.getElementById('h-cnt').textContent   = r.count || 0;
  document.getElementById('h-assoc').textContent = g_data.filter(s => s.state==='Associated').length;
  document.getElementById('h-roam').textContent  = g_data.filter(s => s.roam_count>0).length;
  document.getElementById('h-rand').textContent  = g_data.filter(s => s.rand_mac).length;
  document.getElementById('h-fr').textContent    = (r.total_frames||0).toLocaleString();
  renderTable();
}

async function clearData() {
  await fetch('/api/stp/clear', {method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 2000);
</script>
</body>
</html>
)rawhtml";

inline void wustp_routes() {
  server.on("/stations", HTTP_GET, []() {
    server.send_P(200, "text/html", WUSTP_PAGE);
  });
}

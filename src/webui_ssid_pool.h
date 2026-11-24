#pragma once
// webui_ssid_pool.h — SSID pool popularity tracker UI.
// Route: /ssid-pool. Prefix: wuspl_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUSPL_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>SSID Pool — Phantom32</title>
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
th{text-align:left;padding:8px 10px;border-bottom:2px solid var(--border);color:var(--muted);font-weight:600;font-size:10px;text-transform:uppercase;cursor:pointer;user-select:none;}
th:hover{color:var(--text);}
td{padding:7px 10px;border-bottom:1px solid var(--border);vertical-align:middle;}
tr:hover td{background:var(--bg3);}
.sec-pill{padding:2px 6px;border-radius:4px;font-size:9px;font-weight:700;}
.s-Open{background:rgba(239,68,68,.12);color:var(--danger);}
.s-WEP{background:rgba(239,68,68,.2);color:var(--danger);}
.s-WPA{background:rgba(245,158,11,.12);color:var(--warn);}
.s-WPA2{background:rgba(0,212,255,.12);color:var(--accent);}
.s-WPA3{background:rgba(34,197,94,.12);color:var(--success);}
.s-Mixed{background:rgba(245,158,11,.12);color:var(--warn);}
.flag-pill{padding:1px 5px;border-radius:3px;font-size:8px;font-weight:600;background:rgba(239,68,68,.1);color:var(--danger);}
.bssid-list{display:flex;gap:3px;flex-wrap:wrap;}
.bssid-chip{padding:1px 5px;border-radius:3px;font-size:8px;background:var(--bg2);color:var(--muted);font-family:monospace;border:1px solid var(--border);}
.mini-hist{display:flex;align-items:flex-end;gap:1px;height:14px;}
.mini-bar{width:3px;border-radius:1px 1px 0 0;background:var(--accent);}
.popularity-bar{height:5px;background:var(--border);border-radius:3px;overflow:hidden;}
.pop-fill{height:100%;border-radius:3px;background:var(--accent);}
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
    <a href="/ssid-pool" class="active">SSID Pool</a>
    <a href="/hidden-ssid">Hidden SSID</a>
    <a href="/rogue-ap">Rogue AP</a>
  </nav>
</div>
<div class="container">
  <h1>SSID Pool</h1>
  <p class="subtitle">Aggregates all observed SSIDs: AP instance count per SSID, channel spread, security profile inconsistencies (honeypot risk), and beacon popularity over time.</p>

  <div class="hero">
    <div class="stat"><label>Unique SSIDs</label><div class="val" id="h-cnt">0</div></div>
    <div class="stat"><label>Total Beacons</label><div class="val" id="h-tot" style="font-size:14px">0</div></div>
    <div class="stat"><label>Mixed Security</label><div class="val" id="h-mixed" style="color:var(--warn)">0</div></div>
    <div class="stat"><label>Open Networks</label><div class="val" id="h-open" style="color:var(--danger)">0</div></div>
  </div>

  <div class="section">
    <h3>SSID Popularity Table</h3>
    <input class="search-box" id="search" placeholder="Search SSID…" oninput="renderTable()">
    <div style="overflow-x:auto">
    <table>
      <thead><tr>
        <th onclick="sortBy('ssid')">SSID</th>
        <th onclick="sortBy('bssid_count')">APs</th>
        <th onclick="sortBy('ch_count')">Channels</th>
        <th onclick="sortBy('total_beacons')">Beacons</th>
        <th onclick="sortBy('probe_req')">Probed</th>
        <th onclick="sortBy('sec')">Security</th>
        <th>Flags</th>
        <th>BSSIDs</th>
        <th>Activity</th>
        <th>Popularity</th>
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
let g_sort = {key:'total_beacons', asc:false};

function sortBy(k) {
  if (g_sort.key === k) g_sort.asc = !g_sort.asc;
  else { g_sort.key = k; g_sort.asc = false; }
  renderTable();
}

function miniHist(hist) {
  if (!hist || !hist.length) return '';
  const max = Math.max(...hist, 1);
  return '<div class="mini-hist">' + hist.map(v => {
    const h = Math.max(1, Math.round((v/max)*12));
    return `<div class="mini-bar" style="height:${h}px"></div>`;
  }).join('') + '</div>';
}

function bssidChips(bssids) {
  if (!bssids || !bssids.length) return '—';
  const chips = bssids.slice(0,3).map(b =>
    `<span class="bssid-chip" title="CH${b.ch} ${b.rssi}dBm">${b.bssid}</span>`
  );
  if (bssids.length > 3) chips.push(`<span class="bssid-chip">+${bssids.length-3}</span>`);
  return '<div class="bssid-list">' + chips.join('') + '</div>';
}

function flags(ss) {
  const f = [];
  if (ss.mixed_sec) f.push('Mixed Sec');
  if (ss.has_open)  f.push('Open');
  if (ss.hidden)    f.push('Hidden');
  return f.map(x => `<span class="flag-pill">${x}</span>`).join(' ') || '—';
}

function popBar(total, max) {
  const pct = max > 0 ? (total/max*100).toFixed(0) : 0;
  return `<div class="popularity-bar"><div class="pop-fill" style="width:${pct}%"></div></div>`;
}

function secClass(sec) {
  return 's-' + (sec||'Open').replace(/[^A-Za-z0-9]/g,'');
}

function renderTable() {
  const q = document.getElementById('search').value.toLowerCase();
  let rows = g_data.filter(s =>
    s.ssid.toLowerCase().includes(q)
  );
  const k = g_sort.key, asc = g_sort.asc;
  rows.sort((a,b) => {
    const va = typeof a[k] === 'string' ? a[k] : Number(a[k]||0);
    const vb = typeof b[k] === 'string' ? b[k] : Number(b[k]||0);
    return asc ? (va > vb ? 1 : -1) : (vb > va ? 1 : -1);
  });
  const maxBcn = rows.length ? Math.max(...rows.map(r => r.total_beacons||0), 1) : 1;
  document.getElementById('tbl-body').innerHTML = rows.length
    ? rows.map(ss => `<tr>
        <td><strong>${ss.ssid||'[Hidden]'}</strong></td>
        <td>${ss.bssid_count}</td>
        <td>${ss.ch_count}</td>
        <td>${(ss.total_beacons||0).toLocaleString()}</td>
        <td style="color:${ss.probe_req>0?'var(--warn)':'var(--muted)'}">${ss.probe_req}</td>
        <td><span class="sec-pill ${secClass(ss.sec)}">${ss.sec||'Open'}${ss.has_wpa3?' + WPA3':''}</span></td>
        <td>${flags(ss)}</td>
        <td>${bssidChips(ss.bssids)}</td>
        <td>${miniHist(ss.beacon_hist)}</td>
        <td style="min-width:80px">${popBar(ss.total_beacons, maxBcn)}</td>
      </tr>`).join('')
    : `<tr><td colspan="10" style="text-align:center;color:var(--muted);padding:20px">No SSIDs observed.</td></tr>`;
}

async function refresh() {
  const r = await fetch('/api/spl/all').then(x => x.json()).catch(()=>({}));
  g_data = r.ssids || [];
  document.getElementById('h-cnt').textContent   = r.count || 0;
  document.getElementById('h-tot').textContent   = (r.total_beacons||0).toLocaleString();
  document.getElementById('h-mixed').textContent = g_data.filter(s => s.mixed_sec).length;
  document.getElementById('h-open').textContent  = g_data.filter(s => s.has_open).length;
  renderTable();
}

async function clearData() {
  await fetch('/api/spl/clear', {method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 5000);
</script>
</body>
</html>
)rawhtml";

inline void wuspl_routes() {
  server.on("/ssid-pool", HTTP_GET, []() {
    server.send_P(200, "text/html", WUSPL_PAGE);
  });
}

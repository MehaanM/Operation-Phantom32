#pragma once
// webui_aggregator.h — Frame aggregation monitor UI.
// Route: /frame-aggr. Prefix: wufag_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUFAG_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Frame Aggregation — Phantom32</title>
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
.mini-hist{display:flex;align-items:flex-end;gap:1px;height:18px;}
.mini-bar{width:3px;border-radius:1px 1px 0 0;background:var(--accent);}
.sf-dist{display:flex;height:12px;border-radius:3px;overflow:hidden;width:80px;}
.sf-seg{height:100%;}
.eff-bar{height:5px;border-radius:3px;background:var(--border);}
.eff-fill{height:100%;border-radius:3px;background:linear-gradient(90deg,var(--warn),var(--success));}
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
    <a href="/phy-stats">PHY Stats</a>
    <a href="/frame-aggr" class="active">Aggregation</a>
    <a href="/data-rates">Data Rates</a>
  </nav>
</div>
<div class="container">
  <h1>Frame Aggregation Monitor</h1>
  <p class="subtitle">Tracks A-MPDU and A-MSDU frame aggregation statistics per AP. High subframe counts indicate heavy traffic and good aggregation efficiency.</p>

  <div class="hero">
    <div class="stat"><label>APs</label><div class="val" id="h-cnt">0</div></div>
    <div class="stat"><label>A-MPDUs Total</label><div class="val" id="h-ampdu">0</div></div>
    <div class="stat"><label>A-MSDUs Total</label><div class="val" id="h-amsdu">0</div></div>
    <div class="stat"><label>Avg Subframes</label><div class="val" id="h-sf" style="font-size:16px">—</div></div>
  </div>

  <div class="section">
    <h3>AP Aggregation Statistics</h3>
    <table>
      <thead>
        <tr>
          <th onclick="sort('bssid')">BSSID</th>
          <th onclick="sort('ssid')">SSID</th>
          <th>CH</th>
          <th onclick="sort('ampdu_count')">A-MPDUs</th>
          <th onclick="sort('avg_subframes')">Avg SF</th>
          <th onclick="sort('max_subframes')">Max SF</th>
          <th onclick="sort('amsdu_count')">A-MSDUs</th>
          <th onclick="sort('avg_bps')">Avg Bytes/SF</th>
          <th>SF Size Dist</th>
          <th>Throughput</th>
        </tr>
      </thead>
      <tbody id="ap-tbody">
        <tr><td colspan="10" style="color:var(--muted);text-align:center">Waiting for A-MPDU/A-MSDU frames…</td></tr>
      </tbody>
    </table>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>
</div>

<script>
let g_aps = [];
let g_sort = {key:'ampdu_count', asc:false};

const SF_BUCKETS = ['<128','128','256','384','512','768','1024','>2048'];
const SF_COLORS  = ['#64748b','#4b9eff','#00d4ff','#22c55e','#f59e0b','#f97316','#ef4444','#c026d3'];

function sfDist(hist) {
  if (!hist || !hist.length) return '';
  const total = hist.reduce((s,v) => s+v, 0) || 1;
  return '<div class="sf-dist" title="Subframe size distribution">' +
    hist.map((v, i) => `<div class="sf-seg" style="width:${(v/total*100).toFixed(1)}%;background:${SF_COLORS[i]}" title="${SF_BUCKETS[i]}: ${v}"></div>`).join('') +
  '</div>';
}

function miniHist(hist) {
  if (!hist || !hist.length) return '';
  const max = Math.max(...hist, 1);
  return '<div class="mini-hist">' + hist.slice(-20).map(v => {
    const h = Math.round((v / max) * 16) + 2;
    return `<div class="mini-bar" style="height:${h}px"></div>`;
  }).join('') + '</div>';
}

function effBar(avg_bps) {
  const pct = Math.min((avg_bps / 1500) * 100, 100).toFixed(1);
  return `<div class="eff-bar"><div class="eff-fill" style="width:${pct}%"></div></div>`;
}

function sort(key) {
  if (g_sort.key === key) g_sort.asc = !g_sort.asc;
  else { g_sort.key = key; g_sort.asc = false; }
  renderTable();
}

function renderTable() {
  const sorted = [...g_aps].sort((a,b) => {
    const va = a[g_sort.key], vb = b[g_sort.key];
    if (typeof va === 'number') return g_sort.asc ? va - vb : vb - va;
    return g_sort.asc ? String(va).localeCompare(String(vb)) : String(vb).localeCompare(String(va));
  });
  document.getElementById('ap-tbody').innerHTML = sorted.length
    ? sorted.map(a => `<tr>
        <td><code style="font-size:9px">${a.bssid}</code></td>
        <td>${a.ssid || '<em style="color:var(--muted)">Hidden</em>'}</td>
        <td>${a.ch}</td>
        <td style="color:var(--accent)">${a.ampdu_count.toLocaleString()}</td>
        <td style="font-weight:600">${a.avg_subframes}</td>
        <td style="color:${a.max_subframes>=32?'var(--success)':'var(--muted)'}">${a.max_subframes}</td>
        <td style="color:var(--muted)">${a.amsdu_count.toLocaleString()}</td>
        <td style="font-size:10px">${a.avg_bps}B ${effBar(a.avg_bps)}</td>
        <td>${sfDist(a.sf_size_hist)}</td>
        <td>${miniHist(a.bytes_hist)}</td>
      </tr>`).join('')
    : '<tr><td colspan="10" style="color:var(--muted);text-align:center">No data.</td></tr>';
}

async function refresh() {
  const r = await fetch('/api/fag/all').then(x => x.json()).catch(() => ({}));
  g_aps = r.aps || [];
  document.getElementById('h-cnt').textContent   = r.count || 0;
  document.getElementById('h-ampdu').textContent = g_aps.reduce((s,a)=>s+a.ampdu_count,0).toLocaleString();
  document.getElementById('h-amsdu').textContent = g_aps.reduce((s,a)=>s+a.amsdu_count,0).toLocaleString();
  const totalSF = g_aps.reduce((s,a)=>s+a.ampdu_subframes,0);
  const totalAMPDU = g_aps.reduce((s,a)=>s+a.ampdu_count,0);
  document.getElementById('h-sf').textContent = totalAMPDU > 0 ? (totalSF/totalAMPDU).toFixed(1) : '—';
  renderTable();
}

async function clearData() {
  await fetch('/api/fag/clear', {method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 3000);
</script>
</body>
</html>
)rawhtml";

inline void wufag_routes() {
  server.on("/frame-aggr", HTTP_GET, []() { server.send_P(200, "text/html", WUFAG_PAGE); });
}

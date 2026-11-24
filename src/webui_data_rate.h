#pragma once
// webui_data_rate.h — 802.11 data rate tracker UI.
// Route: /data-rates. Prefix: wudrt_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUDRT_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Data Rates — Phantom32</title>
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
table{width:100%;border-collapse:collapse;font-size:12px;}
th{text-align:left;padding:7px 8px;font-size:10px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;cursor:pointer;}
th:hover{color:var(--text);}
td{padding:7px 8px;border-bottom:1px solid var(--border);}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
.rate-pill{display:inline-block;padding:2px 7px;border-radius:4px;font-size:10px;font-weight:600;background:rgba(0,212,255,.12);color:var(--accent);}
.rate-high{background:rgba(34,197,94,.18);color:var(--success);}
.rate-mid{background:rgba(245,158,11,.18);color:var(--warn);}
.rate-low{background:rgba(239,68,68,.18);color:var(--danger);}
.spark{display:inline-flex;align-items:flex-end;gap:1px;height:16px;vertical-align:middle;}
.spark-bar{width:3px;border-radius:1px 1px 0 0;}
.rate-bar{height:4px;border-radius:2px;background:var(--bg3);}
.rate-fill{height:100%;border-radius:2px;background:var(--accent);}
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
    <a href="/frame-sniffer">Frames</a>
    <a href="/data-rates" class="active">Data Rates</a>
    <a href="/pkt-loss">Packet Loss</a>
    <a href="/analytics">Analytics</a>
  </nav>
</div>
<div class="container">
  <h1>802.11 Data Rate Analysis</h1>
  <p class="subtitle">Per-AP data rate distribution and throughput estimates derived from observed frame sizes and arrival timing.</p>

  <div class="hero">
    <div class="stat"><label>APs Tracked</label><div class="val" id="h-cnt">0</div></div>
    <div class="stat"><label>Highest Rate</label><div class="val" id="h-max" style="font-size:14px;margin-top:4px">—</div></div>
    <div class="stat"><label>Lowest Rate</label><div class="val" id="h-min" style="font-size:14px;margin-top:4px">—</div></div>
    <div class="stat"><label>Avg Throughput</label><div class="val" id="h-avg">—</div></div>
  </div>

  <div class="section">
    <h3>AP Rate Table</h3>
    <table>
      <thead>
        <tr>
          <th onclick="sortTable('bssid')">BSSID</th>
          <th onclick="sortTable('ssid')">SSID</th>
          <th>CH</th>
          <th onclick="sortTable('last_rate')">Last Rate</th>
          <th onclick="sortTable('avg_rate')">Avg Rate</th>
          <th onclick="sortTable('max_rate')">Max Rate</th>
          <th onclick="sortTable('frames')">Frames</th>
          <th>Throughput (60s)</th>
          <th>Rate Distribution</th>
        </tr>
      </thead>
      <tbody id="ap-tbody">
        <tr><td colspan="9" style="color:var(--muted);text-align:center">Waiting for data…</td></tr>
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
let g_sort = {key:'avg_rate', asc:false};

function rateColor(r10) {
  const mbps = r10 / 10;
  if (mbps >= 54) return 'var(--success)';
  if (mbps >= 11) return 'var(--warn)';
  return 'var(--danger)';
}

function ratePill(r10) {
  const mbps = r10 / 10;
  const cls = mbps >= 54 ? 'rate-high' : mbps >= 11 ? 'rate-mid' : 'rate-low';
  return `<span class="rate-pill ${cls}">${(mbps).toFixed(1)} Mbps</span>`;
}

function kbpsSparkline(hist) {
  if (!hist || !hist.length) return '';
  const max = Math.max(...hist, 1);
  return '<div class="spark">' + hist.map(v => {
    const h = Math.round((v / max) * 14) + 2;
    const color = v > max * 0.6 ? 'var(--success)' : v > max * 0.3 ? 'var(--warn)' : 'var(--accent)';
    return `<div class="spark-bar" style="height:${h}px;background:${color}"></div>`;
  }).join('') + '</div>';
}

function rateDistribution(rate_table) {
  if (!rate_table || !rate_table.length) return '—';
  const total = rate_table.reduce((s, r) => s + r.n, 0);
  if (!total) return '—';
  const top3 = [...rate_table].sort((a, b) => b.n - a.n).slice(0, 3);
  return top3.map(r => {
    const pct = Math.round(r.n / total * 100);
    const mbps = (r.r / 10).toFixed(1);
    return `<div style="margin:2px 0">
      <div style="display:flex;justify-content:space-between;font-size:9px;margin-bottom:1px">
        <span>${mbps}M</span><span style="color:var(--muted)">${pct}%</span>
      </div>
      <div class="rate-bar" style="width:80px">
        <div class="rate-fill" style="width:${pct}%;background:${rateColor(r.r)}"></div>
      </div>
    </div>`;
  }).join('');
}

function sortTable(key) {
  if (g_sort.key === key) g_sort.asc = !g_sort.asc;
  else { g_sort.key = key; g_sort.asc = false; }
  renderTable();
}

function renderTable() {
  const sorted = [...g_aps].sort((a, b) => {
    const va = a[g_sort.key], vb = b[g_sort.key];
    if (typeof va === 'number') return g_sort.asc ? va - vb : vb - va;
    return g_sort.asc ? String(va).localeCompare(String(vb)) : String(vb).localeCompare(String(va));
  });
  document.getElementById('ap-tbody').innerHTML = sorted.length
    ? sorted.map(a => `<tr>
        <td><code style="font-size:10px">${a.bssid}</code></td>
        <td>${a.ssid || '<em style="color:var(--muted)">Hidden</em>'}</td>
        <td>${a.ch}</td>
        <td>${ratePill(a.last_rate)}</td>
        <td>${ratePill(a.avg_rate)}</td>
        <td>${ratePill(a.max_rate)}</td>
        <td style="color:var(--muted)">${a.frames.toLocaleString()}</td>
        <td>${kbpsSparkline(a.hist)}</td>
        <td>${rateDistribution(a.rate_table)}</td>
      </tr>`).join('')
    : '<tr><td colspan="9" style="color:var(--muted);text-align:center">No data.</td></tr>';
}

async function refresh() {
  const r = await fetch('/api/drt/all').then(x => x.json()).catch(() => ({}));
  g_aps = r.aps || [];
  document.getElementById('h-cnt').textContent = r.count || 0;
  if (g_aps.length) {
    const best = g_aps.reduce((a, b) => a.max_rate > b.max_rate ? a : b);
    const worst = g_aps.reduce((a, b) => a.avg_rate < b.avg_rate ? a : b);
    document.getElementById('h-max').innerHTML = ratePill(best.max_rate) + ` <span style="font-size:10px;color:var(--muted)">${best.ssid || best.bssid.substring(0,8)}</span>`;
    document.getElementById('h-min').innerHTML = ratePill(worst.avg_rate) + ` <span style="font-size:10px;color:var(--muted)">${worst.ssid || worst.bssid.substring(0,8)}</span>`;
    const avgAll = Math.round(g_aps.reduce((s, a) => s + a.avg_rate, 0) / g_aps.length / 10);
    document.getElementById('h-avg').textContent = avgAll + ' Mbps';
  }
  renderTable();
}

async function clearData() {
  await fetch('/api/drt/clear', {method: 'POST'});
  refresh();
}

refresh();
setInterval(refresh, 3000);
</script>
</body>
</html>
)rawhtml";

inline void wudrt_routes() {
  server.on("/data-rates", HTTP_GET, []() {
    server.send_P(200, "text/html", WUDRT_PAGE);
  });
}

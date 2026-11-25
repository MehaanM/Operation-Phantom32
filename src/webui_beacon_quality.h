#pragma once
// webui_beacon_quality.h — Beacon quality monitoring UI.
// Route: /beacon-quality. Prefix: wubcq_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUBCQ_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Beacon Quality — Phantom32</title>
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
.hero{display:grid;grid-template-columns:repeat(3,1fr);gap:10px;margin-bottom:16px;}
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
.q-bar{display:flex;align-items:center;gap:5px;}
.qt{height:7px;width:70px;background:var(--bg3);border-radius:4px;}
.qf{height:100%;border-radius:4px;}
.anomaly-badges{display:flex;flex-wrap:wrap;gap:3px;}
.ab{padding:1px 5px;border-radius:3px;font-size:9px;font-weight:700;}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;margin-top:12px;}
@media(max-width:700px){.hero{grid-template-columns:1fr}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/analytics">Analytics</a>
    <a href="/beacon-quality" class="active">Beacon Quality</a>
    <a href="/anomaly">Anomaly</a>
    <a href="/fingerprint">Fingerprint</a>
  </nav>
</div>
<div class="container">
  <h1>Beacon Quality Monitor</h1>
  <p class="subtitle">Tracks beacon frame consistency: interval jitter, SSID changes, timestamp anomalies, and rate spikes.</p>

  <div class="hero">
    <div class="stat"><label>APs Monitored</label><div class="val" id="h-aps">0</div></div>
    <div class="stat"><label>Total Beacons</label><div class="val" id="h-beacons">0</div></div>
    <div class="stat"><label>Total Anomalies</label><div class="val" id="h-anom" style="color:var(--warn)">0</div></div>
  </div>

  <div class="section">
    <h3>AP Beacon Quality</h3>
    <table>
      <thead><tr>
        <th onclick="sort('bssid')">BSSID</th>
        <th onclick="sort('ssid')">SSID</th>
        <th>CH</th>
        <th onclick="sort('interval_tu')">Interval (TU)</th>
        <th onclick="sort('avg_ms')">Obs. Interval</th>
        <th onclick="sort('jitter_avg')">Avg Jitter</th>
        <th onclick="sort('quality')">Quality</th>
        <th>Anomalies</th>
      </tr></thead>
      <tbody id="ap-tbody">
        <tr><td colspan="8" style="color:var(--muted);text-align:center">Loading…</td></tr>
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
let g_sort = {key:'quality', asc:true};

function sort(key) {
  if(g_sort.key===key) g_sort.asc=!g_sort.asc;
  else {g_sort.key=key; g_sort.asc=false;}
  renderTable();
}

function q_color(q) {
  if(q>=80) return 'var(--success)';
  if(q>=55) return 'var(--warn)';
  return 'var(--danger)';
}

function renderTable() {
  const sorted = [...g_aps].sort((a,b)=>{
    const va=a[g_sort.key],vb=b[g_sort.key];
    if(typeof va==='number') return g_sort.asc?va-vb:vb-va;
    return g_sort.asc?String(va).localeCompare(String(vb)):String(vb).localeCompare(String(va));
  });

  document.getElementById('ap-tbody').innerHTML = sorted.length
    ? sorted.map(ap=>{
        const qcol=q_color(ap.quality);
        const anomalies = [
          ap.ssid_changed      ? `<span class="ab" style="background:rgba(239,68,68,.15);color:var(--danger)">SSID Change</span>` : '',
          ap.interval_changed  ? `<span class="ab" style="background:rgba(245,158,11,.15);color:var(--warn)">Interval Change</span>` : '',
          ap.rate_spike        ? `<span class="ab" style="background:rgba(239,68,68,.12);color:var(--danger)">Rate Spike</span>` : '',
        ].filter(Boolean).join('');

        return `<tr>
          <td><code style="font-size:10px">${ap.bssid}</code></td>
          <td>${ap.ssid||'—'}</td>
          <td>${ap.ch}</td>
          <td>${ap.interval_tu} TU</td>
          <td>${ap.avg_ms||'—'} ms</td>
          <td>${ap.jitter_avg} ms</td>
          <td><div class="q-bar">
            <span style="color:${qcol};min-width:34px">${ap.quality}%</span>
            <div class="qt"><div class="qf" style="width:${ap.quality}%;background:${qcol}"></div></div>
          </div></td>
          <td><div class="anomaly-badges">${anomalies||'<span style="color:var(--success);font-size:10px">None</span>'}</div></td>
        </tr>`;
      }).join('')
    : '<tr><td colspan="8" style="color:var(--muted);text-align:center">No data.</td></tr>';
}

async function refresh() {
  const r = await fetch('/api/bcq/all').then(x=>x.json()).catch(()=>({}));
  document.getElementById('h-aps').textContent     = r.ap_count||0;
  document.getElementById('h-beacons').textContent = r.total_beacons||0;
  document.getElementById('h-anom').textContent    = r.total_anomalies||0;
  g_aps = r.aps||[];
  renderTable();
}

async function clearData() {
  await fetch('/api/bcq/clear',{method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 3000);
</script>
</body>
</html>
)rawhtml";

inline void wubcq_routes() {
  server.on("/beacon-quality", HTTP_GET, []() { server.send_P(200, "text/html", WUBCQ_PAGE); });
}

#pragma once
// webui_ap_timing.h — AP beacon timing analysis UI.
// Route: /ap-timing. Prefix: wuapt_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUAPT_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>AP Beacon Timing — Phantom32</title>
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
.spark{display:inline-flex;align-items:flex-end;gap:1px;height:20px;}
.spark-bar{width:4px;background:var(--accent);border-radius:1px;}
.badge{padding:2px 6px;border-radius:3px;font-size:9px;font-weight:700;display:inline-block;}
.b-ok{background:rgba(34,197,94,.18);color:var(--success);}
.b-warn{background:rgba(245,158,11,.18);color:var(--warn);}
.b-bad{background:rgba(239,68,68,.18);color:var(--danger);}
.drift-pos{color:var(--warn);}
.drift-neg{color:var(--accent);}
.drift-ok{color:var(--muted);}
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
    <a href="/ap-timing" class="active">AP Timing</a>
    <a href="/beacon-quality">Beacon Quality</a>
  </nav>
</div>
<div class="container">
  <h1>AP Beacon Timing Analysis</h1>
  <p class="subtitle">Measures actual 802.11 beacon intervals and detects timing anomalies, missed beacons, clock drift, and jitter. Useful for detecting virtualised or software APs.</p>

  <div class="hero">
    <div class="stat"><label>APs Tracked</label><div class="val" id="h-aps">0</div></div>
    <div class="stat"><label>Total Beacons</label><div class="val" id="h-total">0</div></div>
    <div class="stat"><label>Anomalous APs</label><div class="val" id="h-anom" style="color:var(--warn)">0</div></div>
  </div>

  <div class="section">
    <h3>Beacon Timing Table</h3>
    <div style="overflow-x:auto">
    <table>
      <thead><tr>
        <th>BSSID</th>
        <th>SSID</th>
        <th>CH</th>
        <th onclick="sortAPs('nominal_ms')">Nominal</th>
        <th onclick="sortAPs('avg_ms')">Avg</th>
        <th onclick="sortAPs('jitter_ms')">Jitter</th>
        <th onclick="sortAPs('drift_ppm')">Drift PPM</th>
        <th onclick="sortAPs('missed')">Missed</th>
        <th onclick="sortAPs('anomalies')">Anomalies</th>
        <th>Samples</th>
        <th>History</th>
        <th>Health</th>
      </tr></thead>
      <tbody id="ap-tbody">
        <tr><td colspan="12" style="color:var(--muted);text-align:center">Loading…</td></tr>
      </tbody>
    </table>
    </div>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>
</div>

<script>
let g_aps = [];
let g_sort = {key:'jitter_ms', asc:false};

function sortAPs(key) {
  if(g_sort.key===key) g_sort.asc=!g_sort.asc;
  else {g_sort.key=key; g_sort.asc=false;}
  renderAPs();
}

function sparkline(hist) {
  if (!hist || !hist.length) return '<span style="color:var(--muted);font-size:9px">—</span>';
  const max = Math.max(...hist, 1);
  return '<div class="spark">' + hist.map(v=>
    `<div class="spark-bar" style="height:${Math.round((v/max)*18)+2}px"></div>`
  ).join('') + '</div>';
}

function healthBadge(a) {
  if (a.samples < 5) return '<span class="badge b-warn">NO DATA</span>';
  const jitPct = a.nominal_ms > 0 ? (a.jitter_ms * 100 / a.nominal_ms) : 0;
  if (a.anomalies > 10 || jitPct > 30 || Math.abs(a.drift_ppm) > 500)
    return '<span class="badge b-bad">POOR</span>';
  if (a.anomalies > 2 || jitPct > 10 || Math.abs(a.drift_ppm) > 100)
    return '<span class="badge b-warn">FAIR</span>';
  return '<span class="badge b-ok">GOOD</span>';
}

function driftSpan(ppm) {
  if (Math.abs(ppm) < 50) return `<span class="drift-ok">${ppm}</span>`;
  if (ppm > 0) return `<span class="drift-pos">+${ppm}</span>`;
  return `<span class="drift-neg">${ppm}</span>`;
}

function renderAPs() {
  const sorted = [...g_aps].sort((a,b)=>{
    const va=a[g_sort.key], vb=b[g_sort.key];
    if(typeof va==='number') return g_sort.asc?va-vb:vb-va;
    return g_sort.asc?String(va).localeCompare(String(vb)):String(vb).localeCompare(String(va));
  });
  document.getElementById('h-anom').textContent = sorted.filter(a=>a.anomalies>5||a.jitter_ms>20).length;
  document.getElementById('ap-tbody').innerHTML = sorted.length
    ? sorted.map(a=>`<tr>
        <td><code style="font-size:10px">${a.bssid}</code></td>
        <td>${a.ssid||'—'}</td>
        <td>${a.ch}</td>
        <td>${a.nominal_ms}ms</td>
        <td>${a.avg_ms||'—'}ms</td>
        <td style="color:${a.jitter_ms>20?'var(--warn)':'var(--muted)'}">${a.jitter_ms}ms</td>
        <td>${driftSpan(a.drift_ppm)}</td>
        <td style="color:${a.missed>0?'var(--warn)':'var(--muted)'}">${a.missed}</td>
        <td style="color:${a.anomalies>5?'var(--danger)':'var(--muted)'}">${a.anomalies}</td>
        <td>${a.samples}</td>
        <td>${sparkline(a.hist)}</td>
        <td>${healthBadge(a)}</td>
      </tr>`).join('')
    : '<tr><td colspan="12" style="color:var(--muted);text-align:center">No APs tracked yet.</td></tr>';
}

async function refresh() {
  const r = await fetch('/api/apt/all').then(x=>x.json()).catch(()=>({}));
  g_aps = r.aps||[];
  document.getElementById('h-aps').textContent   = r.ap_count||0;
  document.getElementById('h-total').textContent = r.total_beacons||0;
  renderAPs();
}

async function clearData() {
  await fetch('/api/apt/clear',{method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 2000);
</script>
</body>
</html>
)rawhtml";

inline void wuapt_routes() {
  server.on("/ap-timing", HTTP_GET, []() { server.send_P(200, "text/html", WUAPT_PAGE); });
}

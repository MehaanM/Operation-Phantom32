#pragma once
// webui_frame_rates.h — Frame rate monitoring UI.
// Route: /frame-rates. Prefix: wufrt_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUFRT_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Frame Rates — Phantom32</title>
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
.hero{display:grid;grid-template-columns:repeat(3,1fr);gap:10px;margin-bottom:16px;}
.stat{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:13px;}
.stat label{font-size:10px;color:var(--muted);text-transform:uppercase;}
.stat .val{font-size:22px;font-weight:800;color:var(--accent);margin-top:3px;}
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:18px;margin-bottom:14px;}
.section h3{font-size:12px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:14px;}
.chart-wrap{position:relative;height:140px;margin-bottom:12px;}
canvas{display:block;}
.ch-grid{display:grid;grid-template-columns:repeat(7,1fr);gap:8px;margin-bottom:12px;}
.ch-card{background:var(--bg3);border:1px solid var(--border);border-radius:6px;padding:8px;text-align:center;}
.ch-num{font-size:11px;font-weight:700;color:var(--accent);}
.ch-fps{font-size:14px;font-weight:800;margin:3px 0;}
.ch-peak{font-size:9px;color:var(--muted);}
.ch-bar{height:4px;background:var(--border);border-radius:2px;margin-top:4px;}
.ch-fill{height:100%;border-radius:2px;background:var(--accent);}
table{width:100%;border-collapse:collapse;font-size:12px;}
th{text-align:left;padding:7px 8px;font-size:10px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;cursor:pointer;}
th:hover{color:var(--text);}
td{padding:7px 8px;border-bottom:1px solid var(--border);}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
.spark{display:flex;align-items:flex-end;gap:1px;height:22px;}
.sbar{width:3px;border-radius:1px;min-height:1px;background:var(--accent);}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;margin-top:12px;}
@media(max-width:700px){.hero{grid-template-columns:1fr};.ch-grid{grid-template-columns:repeat(4,1fr)}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/pktcounter">Pkt Counter</a>
    <a href="/frame-rates" class="active">Frame Rates</a>
    <a href="/interference">Interference</a>
    <a href="/channels">Channels</a>
  </nav>
</div>
<div class="container">
  <h1>Frame Rate Monitor</h1>
  <p class="subtitle">Per-AP and per-channel 802.11 frame rates with 60-second rolling history.</p>

  <div class="hero">
    <div class="stat"><label>Active APs</label><div class="val" id="h-aps">0</div></div>
    <div class="stat"><label>Total Frames</label><div class="val" id="h-total">0</div></div>
    <div class="stat"><label>Peak FPS (single AP)</label><div class="val" id="h-peak">0</div></div>
  </div>

  <div class="section">
    <h3>Channel Frame Rates</h3>
    <div class="ch-grid" id="ch-grid"></div>
  </div>

  <div class="section">
    <h3>AP Frame Rate Table</h3>
    <table>
      <thead><tr>
        <th onclick="sortAPs('bssid')">BSSID</th>
        <th onclick="sortAPs('ssid')">SSID</th>
        <th>CH</th>
        <th>RSSI</th>
        <th onclick="sortAPs('fps')">FPS Now</th>
        <th onclick="sortAPs('avg_fps')">FPS Avg</th>
        <th onclick="sortAPs('peak_fps')">Peak FPS</th>
        <th onclick="sortAPs('total')">Total</th>
        <th>60s History</th>
      </tr></thead>
      <tbody id="ap-tbody">
        <tr><td colspan="9" style="color:var(--muted);text-align:center">Loading…</td></tr>
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
let g_sort = {key:'fps', asc:false};

function fmtBytes(b) {
  if(b>1048576) return (b/1048576).toFixed(1)+'MB';
  if(b>1024) return (b/1024).toFixed(1)+'KB';
  return b+'B';
}

function sparkline(hist) {
  const max = Math.max(...hist, 1);
  return '<div class="spark">' + hist.map(v=>{
    const h = Math.round((v/max)*20)+1;
    return `<div class="sbar" style="height:${h}px"></div>`;
  }).join('') + '</div>';
}

function sortAPs(key) {
  if(g_sort.key===key) g_sort.asc=!g_sort.asc;
  else {g_sort.key=key; g_sort.asc=false;}
  renderAPs();
}

function renderAPs() {
  const sorted = [...g_aps].sort((a,b)=>{
    const va=a[g_sort.key],vb=b[g_sort.key];
    if(typeof va==='number') return g_sort.asc?va-vb:vb-va;
    return g_sort.asc?String(va).localeCompare(String(vb)):String(vb).localeCompare(String(va));
  });
  const max_fps = Math.max(...sorted.map(a=>a.peak_fps), 1);
  document.getElementById('ap-tbody').innerHTML = sorted.length
    ? sorted.map(a=>`<tr>
        <td><code style="font-size:10px">${a.bssid}</code></td>
        <td>${a.ssid||'—'}</td>
        <td>${a.ch}</td>
        <td>${a.rssi} dBm</td>
        <td style="color:var(--accent);font-weight:700">${a.fps}</td>
        <td>${a.avg_fps}</td>
        <td style="color:var(--warn)">${a.peak_fps}</td>
        <td>${a.total.toLocaleString()}</td>
        <td>${sparkline(a.hist||[])}</td>
      </tr>`).join('')
    : '<tr><td colspan="9" style="color:var(--muted);text-align:center">No APs.</td></tr>';

  const max_peak = Math.max(...g_aps.map(a=>a.peak_fps||0), 1);
  document.getElementById('h-peak').textContent = max_peak;
}

function renderChannels(chs) {
  const max_fps = Math.max(...chs.map(c=>c.fps||0), 1);
  document.getElementById('ch-grid').innerHTML = chs.map(c=>`
    <div class="ch-card">
      <div class="ch-num">CH ${c.ch}</div>
      <div class="ch-fps" style="color:${c.fps>50?'var(--danger)':c.fps>20?'var(--warn)':'var(--text)'}">${c.fps}</div>
      <div class="ch-peak">peak: ${c.peak}</div>
      <div class="ch-bar"><div class="ch-fill" style="width:${Math.round((c.fps/max_fps)*100)}%"></div></div>
    </div>`).join('');
}

async function refresh() {
  const [apData, chData] = await Promise.all([
    fetch('/api/frt/aps').then(x=>x.json()).catch(()=>({})),
    fetch('/api/frt/channels').then(x=>x.json()).catch(()=>[]),
  ]);
  document.getElementById('h-aps').textContent   = apData.ap_count||0;
  document.getElementById('h-total').textContent = (apData.global_total||0).toLocaleString();
  g_aps = apData.aps||[];
  renderAPs();
  renderChannels(chData);
}

async function clearData() {
  await fetch('/api/frt/clear',{method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 1500);
</script>
</body>
</html>
)rawhtml";

inline void wufrt_routes() {
  server.on("/frame-rates", HTTP_GET, []() { server.send_P(200, "text/html", WUFRT_PAGE); });
}

#pragma once
// webui_apload.h — AP load monitor UI.
// Route: /ap-load. Prefix: wualt_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUALT_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>AP Load Monitor — Phantom32</title>
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
.hero{display:grid;grid-template-columns:repeat(2,1fr);gap:10px;margin-bottom:16px;}
.stat{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:13px;}
.stat label{font-size:10px;color:var(--muted);text-transform:uppercase;}
.stat .val{font-size:22px;font-weight:800;color:var(--accent);margin-top:3px;}
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:18px;margin-bottom:14px;}
.section h3{font-size:12px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:14px;}
.ap-card-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(240px,1fr));gap:12px;}
.ap-card{background:var(--bg3);border:1px solid var(--border);border-radius:7px;padding:12px 14px;}
.ap-card-top{display:flex;justify-content:space-between;align-items:flex-start;margin-bottom:8px;}
.ap-name{font-size:12px;font-weight:700;color:var(--text);}
.ap-bssid{font-size:10px;color:var(--muted);font-family:monospace;margin-top:1px;}
.load-ring-wrap{display:flex;align-items:center;justify-content:center;}
.load-ring{width:48px;height:48px;position:relative;}
.load-ring svg{transform:rotate(-90deg);}
.load-ring .pct{position:absolute;inset:0;display:flex;align-items:center;justify-content:center;font-size:12px;font-weight:800;}
.ap-meta{display:flex;gap:10px;font-size:10px;color:var(--muted);margin-top:6px;}
.ap-sparkline{margin-top:6px;}
table{width:100%;border-collapse:collapse;font-size:12px;}
th{text-align:left;padding:7px 8px;font-size:10px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;cursor:pointer;}
td{padding:7px 8px;border-bottom:1px solid var(--border);}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
.load-bar{display:flex;align-items:center;gap:5px;}
.l-track{height:7px;width:70px;background:var(--bg3);border-radius:4px;}
.l-fill{height:100%;border-radius:4px;}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;margin-top:12px;}
.view-toggle{display:flex;gap:4px;margin-bottom:12px;}
.vt-btn{padding:4px 10px;border-radius:4px;font-size:11px;cursor:pointer;border:1px solid var(--border);background:transparent;color:var(--muted);}
.vt-btn.active{background:var(--bg3);color:var(--text);}
@media(max-width:700px){.hero{grid-template-columns:1fr};.ap-card-grid{grid-template-columns:1fr}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/analytics">Analytics</a>
    <a href="/ap-load" class="active">AP Load</a>
    <a href="/aprates">AP Rates</a>
    <a href="/bandwidth-est">Bandwidth</a>
  </nav>
</div>
<div class="container">
  <h1>AP Load Monitor</h1>
  <p class="subtitle">Per-AP channel utilization estimates based on observed frame rates, throughput, and client density.</p>

  <div class="hero">
    <div class="stat"><label>Monitored APs</label><div class="val" id="h-aps">0</div></div>
    <div class="stat"><label>Highest Load</label><div class="val" id="h-top-load" style="color:var(--warn)">0%</div></div>
  </div>

  <div class="section">
    <h3>AP Load Overview</h3>
    <div class="view-toggle">
      <button class="vt-btn active" onclick="switchView('cards',this)">Cards</button>
      <button class="vt-btn" onclick="switchView('table',this)">Table</button>
    </div>

    <div id="cards-view">
      <div class="ap-card-grid" id="ap-card-grid">
        <div style="color:var(--muted);font-size:12px">Loading…</div>
      </div>
    </div>

    <div id="table-view" style="display:none">
      <table>
        <thead><tr>
          <th onclick="sort('bssid')">BSSID</th>
          <th onclick="sort('ssid')">SSID</th>
          <th>CH</th>
          <th onclick="sort('fps')">FPS</th>
          <th onclick="sort('clients')">Clients</th>
          <th onclick="sort('load')">Load</th>
          <th>30-sec Trend</th>
        </tr></thead>
        <tbody id="ap-tbody">
          <tr><td colspan="7" style="color:var(--muted);text-align:center">Loading…</td></tr>
        </tbody>
      </table>
    </div>

    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-ghost" onclick="clearData()">Reset</button>
    </div>
  </div>
</div>

<script>
let g_aps  = [];
let g_view = 'cards';
let g_sort = {key:'load', asc:false};

function switchView(v, btn) {
  g_view = v;
  document.querySelectorAll('.vt-btn').forEach(b=>b.classList.remove('active'));
  btn.classList.add('active');
  document.getElementById('cards-view').style.display = v==='cards' ? '' : 'none';
  document.getElementById('table-view').style.display = v==='table' ? '' : 'none';
}

function sort(key) {
  if(g_sort.key===key) g_sort.asc=!g_sort.asc;
  else {g_sort.key=key; g_sort.asc=false;}
  renderTable();
}

function load_color(l) {
  if(l<40) return 'var(--success)';
  if(l<70) return 'var(--warn)';
  return 'var(--danger)';
}

function draw_spark_load(id, hist) {
  const c = document.getElementById(id);
  if(!c) return;
  c.width=160; c.height=24;
  const ctx=c.getContext('2d');
  ctx.clearRect(0,0,160,24);
  if(!hist||!hist.length) return;
  const max_v=Math.max(...hist,1);
  ctx.strokeStyle='#00d4ff';ctx.lineWidth=1.5;ctx.beginPath();
  hist.forEach((v,i)=>{const x=i*(160/(hist.length-1));const y=22-v/max_v*20;i===0?ctx.moveTo(x,y):ctx.lineTo(x,y);});
  ctx.stroke();
}

function renderCards() {
  const sorted = [...g_aps].sort((a,b)=>b.load-a.load);
  document.getElementById('ap-card-grid').innerHTML = sorted.length
    ? sorted.map(ap=>{
        const col=load_color(ap.load);
        const r=20, cx=24, cy=24, circ=2*Math.PI*r;
        const dash=circ*(ap.load/100), gap=circ-dash;
        return `<div class="ap-card">
          <div class="ap-card-top">
            <div>
              <div class="ap-name">${ap.ssid||'(unnamed)'}</div>
              <div class="ap-bssid">${ap.bssid}</div>
            </div>
            <div class="load-ring">
              <svg width="48" height="48" viewBox="0 0 48 48">
                <circle cx="${cx}" cy="${cy}" r="${r}" fill="none" stroke="#2a3045" stroke-width="5"/>
                <circle cx="${cx}" cy="${cy}" r="${r}" fill="none" stroke="${col}" stroke-width="5"
                  stroke-dasharray="${dash.toFixed(1)} ${gap.toFixed(1)}"/>
              </svg>
              <div class="pct" style="color:${col}">${ap.load}%</div>
            </div>
          </div>
          <div class="ap-meta">
            <span>Ch ${ap.ch}</span>
            <span>${ap.rssi} dBm</span>
            <span>${ap.fps} fps</span>
            <span>${ap.clients}👤</span>
          </div>
          <div class="ap-sparkline"><canvas id="card-sp-${ap.bssid.replace(/:/g,'')}"></canvas></div>
        </div>`;
      }).join('')
    : '<div style="color:var(--muted);font-size:12px">No APs.</div>';

  sorted.forEach(ap=>draw_spark_load('card-sp-'+ap.bssid.replace(/:/g,''), ap.load_hist));
}

function renderTable() {
  const max_l = Math.max(...g_aps.map(a=>a.load), 1);
  const sorted = [...g_aps].sort((a,b)=>{
    const va=a[g_sort.key],vb=b[g_sort.key];
    if(typeof va==='number') return g_sort.asc?va-vb:vb-va;
    return g_sort.asc?String(va).localeCompare(String(vb)):String(vb).localeCompare(String(va));
  });

  document.getElementById('ap-tbody').innerHTML = sorted.length
    ? sorted.map((ap,i)=>{
        const col=load_color(ap.load);
        return `<tr>
          <td><code style="font-size:10px">${ap.bssid}</code></td>
          <td>${ap.ssid||'—'}</td>
          <td>${ap.ch}</td>
          <td>${ap.fps}</td>
          <td>${ap.clients}</td>
          <td><div class="load-bar">
            <span style="color:${col};min-width:34px">${ap.load}%</span>
            <div class="l-track"><div class="l-fill" style="width:${ap.load}%;background:${col}"></div></div>
          </div></td>
          <td><canvas id="tsp-${i}"></canvas></td>
        </tr>`;
      }).join('')
    : '<tr><td colspan="7" style="color:var(--muted);text-align:center">No APs.</td></tr>';

  sorted.forEach((ap,i)=>draw_spark_load('tsp-'+i, ap.load_hist));
}

function render() {
  if(g_view==='cards') renderCards();
  else renderTable();
}

async function refresh() {
  const r = await fetch('/api/alt/all').then(x=>x.json()).catch(()=>({}));
  document.getElementById('h-aps').textContent = r.ap_count||0;

  g_aps = r.aps||[];
  if(g_aps.length) {
    const topLoad = Math.max(...g_aps.map(a=>a.load));
    document.getElementById('h-top-load').textContent = topLoad+'%';
  }
  render();
}

async function clearData() {
  await fetch('/api/alt/clear',{method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 2000);
</script>
</body>
</html>
)rawhtml";

inline void wualt_routes() {
  server.on("/ap-load", HTTP_GET, []() {
    server.send_P(200, "text/html", WUALT_PAGE);
  });
}

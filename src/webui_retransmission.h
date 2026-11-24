#pragma once
// webui_retransmission.h — Retry/retransmission analysis UI.
// Route: /retransmission. Prefix: wurta_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WURTA_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Retransmission Analyzer — Phantom32</title>
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
.tabs{display:flex;gap:4px;margin-bottom:14px;}
.tab{padding:5px 12px;border-radius:5px;font-size:12px;cursor:pointer;border:1px solid var(--border);background:transparent;color:var(--muted);}
.tab.active{background:var(--accent);color:#000;border-color:var(--accent);font-weight:600;}
table{width:100%;border-collapse:collapse;font-size:12px;}
th{text-align:left;padding:7px 8px;font-size:10px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;cursor:pointer;}
th:hover{color:var(--text);}
td{padding:7px 8px;border-bottom:1px solid var(--border);}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
.retry-bar{display:flex;align-items:center;gap:5px;}
.r-track{height:7px;width:70px;background:var(--bg3);border-radius:4px;}
.r-fill{height:100%;border-radius:4px;}
.spark canvas{display:block;}
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
    <a href="/retransmission" class="active">Retransmissions</a>
    <a href="/pktcounter">Packet Counter</a>
    <a href="/protocol">Protocol Stats</a>
  </nav>
</div>
<div class="container">
  <h1>Retransmission Analyzer</h1>
  <p class="subtitle">Per-AP and per-client 802.11 retry/retransmission rates, burst detection, and 30-second trend history.</p>

  <div class="hero">
    <div class="stat"><label>Global Retry %</label><div class="val" id="h-gpct">0%</div></div>
    <div class="stat"><label>Tracked APs</label><div class="val" id="h-aps">0</div></div>
    <div class="stat"><label>Tracked Clients</label><div class="val" id="h-cls">0</div></div>
  </div>

  <div class="section">
    <h3>Nodes</h3>
    <div class="tabs">
      <button class="tab active" onclick="switchTab('aps',this)">APs</button>
      <button class="tab" onclick="switchTab('clients',this)">Clients</button>
    </div>
    <table>
      <thead><tr>
        <th onclick="sort('mac')">MAC</th>
        <th>CH</th>
        <th>RSSI</th>
        <th onclick="sort('frames')">Frames</th>
        <th onclick="sort('retry_pct')">Retry %</th>
        <th>Bursts</th>
        <th>Max Burst</th>
        <th>30-sec Trend</th>
      </tr></thead>
      <tbody id="node-tbody">
        <tr><td colspan="8" style="color:var(--muted);text-align:center">Loading…</td></tr>
      </tbody>
    </table>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-ghost" onclick="clearData()">Reset</button>
    </div>
  </div>
</div>

<script>
let g_data = {};
let g_tab  = 'aps';
let g_sort = {key:'retry_pct', asc:false};

function switchTab(t, btn) {
  g_tab = t;
  document.querySelectorAll('.tab').forEach(b=>b.classList.remove('active'));
  btn.classList.add('active');
  renderTable();
}

function sort(key) {
  if(g_sort.key===key) g_sort.asc=!g_sort.asc;
  else {g_sort.key=key; g_sort.asc=false;}
  renderTable();
}

function retry_color(p) {
  if(p<10) return 'var(--success)';
  if(p<25) return 'var(--warn)';
  return 'var(--danger)';
}

function draw_spark(id, hist) {
  const c = document.getElementById(id);
  if(!c) return;
  c.width=80; c.height=20;
  const ctx=c.getContext('2d');
  ctx.clearRect(0,0,80,20);
  if(!hist||!hist.length) return;
  const max_v=Math.max(...hist,1);
  ctx.strokeStyle='#f59e0b';ctx.lineWidth=1;ctx.beginPath();
  hist.forEach((v,i)=>{const x=i*(80/(hist.length-1));const y=18-v/max_v*16;i===0?ctx.moveTo(x,y):ctx.lineTo(x,y);});
  ctx.stroke();
}

function renderTable() {
  const nodes = (g_tab==='aps' ? g_data.aps : g_data.clients)||[];
  const sorted = [...nodes].sort((a,b)=>{
    const va=a[g_sort.key],vb=b[g_sort.key];
    if(typeof va==='number') return g_sort.asc?va-vb:vb-va;
    return g_sort.asc?String(va).localeCompare(String(vb)):String(vb).localeCompare(String(va));
  });

  document.getElementById('node-tbody').innerHTML = sorted.length
    ? sorted.map((n,i)=>{
        const col=retry_color(n.retry_pct);
        return `<tr>
          <td><code style="font-size:10px">${n.mac}</code></td>
          <td>${n.ch}</td>
          <td>${n.rssi} dBm</td>
          <td>${n.frames}</td>
          <td>
            <div class="retry-bar">
              <span style="color:${col};min-width:34px">${n.retry_pct}%</span>
              <div class="r-track"><div class="r-fill" style="width:${n.retry_pct}%;background:${col}"></div></div>
            </div>
          </td>
          <td>${n.bursts}</td>
          <td>${n.max_burst}</td>
          <td class="spark"><canvas id="sp-${i}"></canvas></td>
        </tr>`;
      }).join('')
    : '<tr><td colspan="8" style="color:var(--muted);text-align:center">No data.</td></tr>';

  sorted.forEach((n,i)=>draw_spark('sp-'+i, n.hist));
}

async function refresh() {
  g_data = await fetch('/api/rta/summary').then(x=>x.json()).catch(()=>({}));
  document.getElementById('h-gpct').textContent = (g_data.global_retry_pct||0)+'%';
  document.getElementById('h-aps').textContent  = g_data.ap_count||0;
  document.getElementById('h-cls').textContent  = g_data.client_count||0;
  renderTable();
}

async function clearData() {
  await fetch('/api/rta/clear',{method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 2000);
</script>
</body>
</html>
)rawhtml";

inline void wurta_routes() {
  server.on("/retransmission", HTTP_GET, []() {
    server.send_P(200, "text/html", WURTA_PAGE);
  });
}

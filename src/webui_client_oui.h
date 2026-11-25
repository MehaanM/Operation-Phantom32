#pragma once
// webui_client_oui.h — Client OUI / manufacturer tracking UI.
// Route: /client-devices. Prefix: wucot_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUCOT_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Client Devices — Phantom32</title>
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
.chart-row{display:grid;grid-template-columns:1fr 1fr;gap:16px;margin-bottom:14px;}
canvas.pie{display:block;margin:0 auto;}
table{width:100%;border-collapse:collapse;font-size:12px;}
th{text-align:left;padding:7px 8px;font-size:10px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;cursor:pointer;}
th:hover{color:var(--text);}
td{padding:7px 8px;border-bottom:1px solid var(--border);}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
.bar-cell{width:100px;}
.bar-wrap{background:var(--bg3);border-radius:4px;height:8px;}
.bar-fill{height:100%;border-radius:4px;background:var(--accent);}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;margin-top:12px;}
.filter-row{display:flex;gap:8px;margin-bottom:12px;flex-wrap:wrap;}
input{background:var(--bg3);border:1px solid var(--border);border-radius:5px;padding:4px 8px;color:var(--text);font-size:12px;}
@media(max-width:700px){.hero{grid-template-columns:1fr};.chart-row{grid-template-columns:1fr}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/clients-ext">Clients</a>
    <a href="/client-devices" class="active">Device Types</a>
    <a href="/vendors">AP Vendors</a>
    <a href="/iot">IoT</a>
  </nav>
</div>
<div class="container">
  <h1>Client Device Types</h1>
  <p class="subtitle">Identifies client device manufacturers from MAC OUIs. Shows device type distribution across the wireless environment.</p>

  <div class="hero">
    <div class="stat"><label>Total Clients</label><div class="val" id="h-total">0</div></div>
    <div class="stat"><label>Identified</label><div class="val" id="h-id" style="color:var(--success)">0</div></div>
    <div class="stat"><label>Unknown</label><div class="val" id="h-unknown" style="color:var(--muted)">0</div></div>
  </div>

  <div class="section">
    <h3>Manufacturer Distribution</h3>
    <div class="chart-row">
      <div>
        <canvas id="vendor-chart" width="260" height="260" class="pie"></canvas>
      </div>
      <div id="vendor-legend" style="padding:12px;display:flex;flex-direction:column;gap:6px;justify-content:center;"></div>
    </div>
  </div>

  <div class="section">
    <h3>Client Devices</h3>
    <div class="filter-row">
      <input id="f-search" type="text" placeholder="Search MAC/vendor…" style="flex:1;min-width:140px">
    </div>
    <table>
      <thead><tr>
        <th onclick="sort('mac')">MAC</th>
        <th onclick="sort('vendor')">Manufacturer</th>
        <th onclick="sort('category')">Category</th>
        <th>CH</th>
        <th onclick="sort('rssi')">RSSI</th>
        <th onclick="sort('frames')">Frames</th>
        <th>Last Seen</th>
      </tr></thead>
      <tbody id="cl-tbody">
        <tr><td colspan="7" style="color:var(--muted);text-align:center">Loading…</td></tr>
      </tbody>
    </table>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>
</div>

<script>
let g_clients = [];
let g_vendors = [];
let g_sort = {key:'frames', asc:false};

const PALETTE = ['#00d4ff','#22c55e','#f59e0b','#ef4444','#a855f7','#ec4899',
  '#14b8a6','#f97316','#84cc16','#8b5cf6','#f43f5e','#10b981'];

function sort(key) {
  if(g_sort.key===key) g_sort.asc=!g_sort.asc;
  else {g_sort.key=key; g_sort.asc=false;}
  renderTable();
}

function draw_pie(vendors) {
  const c = document.getElementById('vendor-chart');
  const ctx = c.getContext('2d');
  ctx.clearRect(0,0,260,260);
  const total = vendors.reduce((s,v)=>s+v.devices,0);
  if(!total) return;

  let startAngle = -Math.PI/2;
  const cx=130, cy=130, r=100, ri=55;

  vendors.forEach((v,i)=>{
    const slice = (v.devices/total)*(Math.PI*2);
    ctx.beginPath();
    ctx.moveTo(cx,cy);
    ctx.arc(cx,cy,r,startAngle,startAngle+slice);
    ctx.closePath();
    ctx.fillStyle = PALETTE[i%PALETTE.length];
    ctx.fill();
    startAngle += slice;
  });

  // Donut hole
  ctx.beginPath();
  ctx.arc(cx,cy,ri,0,Math.PI*2);
  ctx.fillStyle = getComputedStyle(document.documentElement).getPropertyValue('--bg2').trim()||'#12151c';
  ctx.fill();

  // Center text
  ctx.fillStyle = '#e2e8f0';
  ctx.font = 'bold 20px system-ui';
  ctx.textAlign = 'center';
  ctx.textBaseline = 'middle';
  ctx.fillText(total, cx, cy-6);
  ctx.font = '11px system-ui';
  ctx.fillStyle = '#64748b';
  ctx.fillText('devices', cx, cy+10);

  // Legend
  document.getElementById('vendor-legend').innerHTML = vendors.map((v,i)=>`
    <div style="display:flex;align-items:center;gap:7px;font-size:11px">
      <div style="width:10px;height:10px;border-radius:50%;background:${PALETTE[i%PALETTE.length]};flex-shrink:0"></div>
      <span style="flex:1">${v.name}</span>
      <span style="color:var(--muted)">${v.devices}</span>
    </div>`).join('');
}

function renderTable() {
  const fsearch = document.getElementById('f-search').value.toLowerCase();
  const filtered = g_clients.filter(c=>{
    if(fsearch && !c.mac.toLowerCase().includes(fsearch) && !c.vendor.toLowerCase().includes(fsearch)) return false;
    return true;
  });
  const sorted = [...filtered].sort((a,b)=>{
    const va=a[g_sort.key],vb=b[g_sort.key];
    if(typeof va==='number') return g_sort.asc?va-vb:vb-va;
    return g_sort.asc?String(va).localeCompare(String(vb)):String(vb).localeCompare(String(va));
  });
  document.getElementById('cl-tbody').innerHTML = sorted.length
    ? sorted.map(c=>`<tr>
        <td><code style="font-size:10px">${c.mac}</code></td>
        <td>${c.vendor}</td>
        <td>${c.category}</td>
        <td>${c.ch}</td>
        <td>${c.rssi} dBm</td>
        <td>${c.frames}</td>
        <td>${new Date(c.last_ts*1000).toLocaleTimeString()}</td>
      </tr>`).join('')
    : '<tr><td colspan="7" style="color:var(--muted);text-align:center">No clients.</td></tr>';
}

async function refresh() {
  const [sum, clients] = await Promise.all([
    fetch('/api/cot/summary').then(x=>x.json()).catch(()=>({})),
    fetch('/api/cot/clients').then(x=>x.json()).catch(()=>[]),
  ]);
  document.getElementById('h-total').textContent   = sum.total_clients||0;
  document.getElementById('h-id').textContent      = sum.identified||0;
  document.getElementById('h-unknown').textContent = sum.unidentified||0;

  g_vendors = (sum.vendors||[]).sort((a,b)=>b.devices-a.devices);
  g_clients = clients;

  draw_pie(g_vendors.slice(0,10));
  renderTable();
}

async function clearData() {
  await fetch('/api/cot/clear',{method:'POST'});
  refresh();
}

document.getElementById('f-search').addEventListener('input', renderTable);

refresh();
setInterval(refresh, 5000);
</script>
</body>
</html>
)rawhtml";

inline void wucot_routes() {
  server.on("/client-devices", HTTP_GET, []() { server.send_P(200, "text/html", WUCOT_PAGE); });
}

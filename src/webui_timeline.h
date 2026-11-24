#pragma once
// webui_timeline.h — Packet timeline visualization UI.
// Route: /timeline. Prefix: wuptl_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUPTL_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Timeline — Phantom32</title>
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
canvas.main-chart{display:block;width:100%;background:var(--bg3);border-radius:4px;}
.legend{display:flex;gap:16px;font-size:11px;color:var(--muted);margin-top:8px;}
.legend-item{display:flex;align-items:center;gap:5px;}
.legend-dot{width:10px;height:10px;border-radius:2px;}
.flow-grid{display:grid;gap:8px;}
.flow-row{display:grid;grid-template-columns:140px 1fr 60px;align-items:center;gap:8px;}
.sparkline-wrap{background:var(--bg3);border-radius:3px;overflow:hidden;height:28px;}
canvas.spark{display:block;}
.toggle-row{display:flex;gap:8px;margin-bottom:12px;flex-wrap:wrap;}
.toggle{padding:4px 10px;border:1px solid var(--border);border-radius:12px;font-size:11px;cursor:pointer;color:var(--muted);}
.toggle.on{border-color:var(--accent);color:var(--accent);background:rgba(0,212,255,.08);}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;margin-top:12px;}
@media(max-width:700px){.hero{grid-template-columns:repeat(2,1fr)}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/analytics">Analytics</a>
    <a href="/timeline" class="active">Timeline</a>
    <a href="/traffic">Traffic</a>
    <a href="/protocol">Protocol</a>
  </nav>
</div>
<div class="container">
  <h1>Packet Timeline</h1>
  <p class="subtitle">2-minute sliding window of packet rate by frame type. 1-second resolution.</p>

  <div class="hero">
    <div class="stat"><label>Total Packets</label><div class="val" id="h-pkts">0</div></div>
    <div class="stat"><label>Total Bytes</label><div class="val" id="h-bytes">0</div></div>
    <div class="stat"><label>Peak PPS</label><div class="val" id="h-peak">0</div></div>
    <div class="stat"><label>Active Flows</label><div class="val" id="h-flows">0</div></div>
  </div>

  <div class="section">
    <h3>Frame Rate — 2-Minute Window</h3>
    <div class="toggle-row">
      <div class="toggle on" data-key="pkt" onclick="toggleSeries(this)">Total</div>
      <div class="toggle on" data-key="mgmt" onclick="toggleSeries(this)">Mgmt</div>
      <div class="toggle on" data-key="data" onclick="toggleSeries(this)">Data</div>
      <div class="toggle on" data-key="deauth" onclick="toggleSeries(this)" style="color:var(--danger);border-color:var(--danger)">Deauth</div>
    </div>
    <canvas id="main-chart" class="main-chart" height="200"></canvas>
    <div class="legend">
      <div class="legend-item"><div class="legend-dot" style="background:#00d4ff"></div>Total</div>
      <div class="legend-item"><div class="legend-dot" style="background:#22c55e"></div>Mgmt</div>
      <div class="legend-item"><div class="legend-dot" style="background:#f59e0b"></div>Data</div>
      <div class="legend-item"><div class="legend-dot" style="background:#ef4444"></div>Deauth</div>
    </div>
  </div>

  <div class="section">
    <h3>Per-AP Sparklines (top 8 by activity)</h3>
    <div class="flow-grid" id="flow-grid">
      <span style="color:var(--muted)">Waiting for AP traffic…</span>
    </div>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="clearData()">Reset</button>
    </div>
  </div>
</div>

<script>
let g_data = {pkt:[],bytes:[],mgmt:[],data:[],deauth:[]};
let g_flows = [];
let g_show = {pkt:true, mgmt:true, data:true, deauth:true};

function toggleSeries(el) {
  const key = el.dataset.key;
  g_show[key] = !g_show[key];
  el.classList.toggle('on', g_show[key]);
  drawMain();
}

function fmt_bytes(b) {
  if(b>1048576) return (b/1048576).toFixed(1)+'M';
  if(b>1024) return (b/1024).toFixed(1)+'K';
  return b+'B';
}

function drawMain() {
  const c = document.getElementById('main-chart');
  if(!c) return;
  c.width = c.clientWidth||900;
  c.height = 200;
  const ctx = c.getContext('2d');
  ctx.fillStyle='#1a1f2e';
  ctx.fillRect(0,0,c.width,c.height);

  const series = [];
  if(g_show.pkt)    series.push({data:g_data.pkt,    color:'#00d4ff', alpha:0.8});
  if(g_show.mgmt)   series.push({data:g_data.mgmt,   color:'#22c55e', alpha:0.7});
  if(g_show.data)   series.push({data:g_data.data,   color:'#f59e0b', alpha:0.7});
  if(g_show.deauth) series.push({data:g_data.deauth, color:'#ef4444', alpha:0.9});

  if(!series.length) return;

  const all_vals = series.flatMap(s=>s.data);
  const max_v = Math.max(...all_vals, 1);
  const n = g_data.pkt.length;
  if(!n) return;

  const W = c.width, H = 200, pad = 25;
  const chart_w = W - 10, chart_h = H - pad;
  const step = chart_w / (n-1);

  series.forEach(s => {
    ctx.beginPath();
    ctx.strokeStyle = s.color;
    ctx.lineWidth = 1.5;
    ctx.globalAlpha = s.alpha;
    s.data.forEach((v,i) => {
      const x = 5 + i*step;
      const y = pad + chart_h - (v/max_v)*chart_h;
      i===0 ? ctx.moveTo(x,y) : ctx.lineTo(x,y);
    });
    ctx.stroke();
  });
  ctx.globalAlpha = 1;

  // Y axis labels
  ctx.fillStyle='#64748b';
  ctx.font='9px system-ui';
  ctx.fillText(max_v, 5, pad+10);
  ctx.fillText(0, 5, H-6);

  // X axis: -120s … -0s
  const x_labels = ['-120s','-90s','-60s','-30s','now'];
  x_labels.forEach((lbl,i) => {
    const x = 5 + ((n-1)*i/(x_labels.length-1))*step;
    ctx.fillText(lbl, x, H-1);
  });
}

function drawSparkline(canvas_id, data, color) {
  const c = document.getElementById(canvas_id);
  if(!c) return;
  c.width = c.parentElement.clientWidth||200;
  c.height = 28;
  const ctx = c.getContext('2d');
  ctx.fillStyle='transparent';
  ctx.clearRect(0,0,c.width,28);
  const max_v = Math.max(...data, 1);
  const n = data.length;
  const step = (c.width-2)/(n-1);
  ctx.beginPath();
  ctx.strokeStyle = color;
  ctx.lineWidth = 1.2;
  data.forEach((v,i) => {
    const x = 1 + i*step;
    const y = 26 - (v/max_v)*24;
    i===0 ? ctx.moveTo(x,y) : ctx.lineTo(x,y);
  });
  ctx.stroke();
}

async function refresh() {
  const g = await fetch('/api/ptl/global').then(x=>x.json()).catch(()=>({}));
  const flows = await fetch('/api/ptl/flows').then(x=>x.json()).catch(()=>[]);

  document.getElementById('h-pkts').textContent  = g.total_pkts||0;
  document.getElementById('h-bytes').textContent = fmt_bytes(g.total_bytes||0);
  document.getElementById('h-peak').textContent  = g.peak_pps||0;

  g_data.pkt    = g.pkt    || [];
  g_data.bytes  = g.bytes  || [];
  g_data.mgmt   = g.mgmt   || [];
  g_data.data   = g.data   || [];
  g_data.deauth = g.deauth || [];

  drawMain();

  const active_flows = flows.filter(f=>f.pkt_rate.some(v=>v>0));
  document.getElementById('h-flows').textContent = active_flows.length;

  // Top 8 by total frames
  const top8 = active_flows
    .map(f=>({...f, total:f.pkt_rate.reduce((a,b)=>a+b,0)}))
    .sort((a,b)=>b.total-a.total).slice(0,8);

  g_flows = top8;

  const fg = document.getElementById('flow-grid');
  fg.innerHTML = top8.length
    ? top8.map(f=>`
        <div class="flow-row">
          <span style="font-size:11px;overflow:hidden;text-overflow:ellipsis">
            ${f.ssid||f.bssid.substring(0,8)+'…'}
          </span>
          <div class="sparkline-wrap">
            <canvas class="spark" id="spark-${f.bssid.replace(/:/g,'')}"></canvas>
          </div>
          <span style="font-size:11px;text-align:right;color:var(--muted)">${f.total}/2m</span>
        </div>`).join('')
    : '<span style="color:var(--muted)">No active flows.</span>';

  top8.forEach(f => {
    drawSparkline('spark-'+f.bssid.replace(/:/g,''), f.pkt_rate, '#00d4ff');
  });
}

async function clearData() {
  await fetch('/api/ptl/clear',{method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 1000);
</script>
</body>
</html>
)rawhtml";

inline void wuptl_routes() {
  server.on("/timeline", HTTP_GET, []() {
    server.send_P(200, "text/html", WUPTL_PAGE);
  });
}

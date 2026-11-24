#pragma once
// webui_protocol.h — Protocol statistics web UI.
// Route: /protocol. Prefix: wuprs_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUPRS_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Protocol Stats — Phantom32</title>
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
.hero{display:grid;grid-template-columns:repeat(5,1fr);gap:10px;margin-bottom:16px;}
.stat{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:13px;}
.stat label{font-size:10px;color:var(--muted);text-transform:uppercase;}
.stat .val{font-size:22px;font-weight:800;color:var(--accent);margin-top:3px;}
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:18px;margin-bottom:14px;}
.section h3{font-size:12px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:14px;}
.row2{display:grid;grid-template-columns:1fr 1fr;gap:14px;margin-bottom:14px;}
canvas{display:block;width:100%;background:var(--bg3);border-radius:4px;}
.bar-container{display:flex;flex-direction:column;gap:6px;}
.bar-row{display:grid;grid-template-columns:120px 1fr 60px;align-items:center;gap:8px;font-size:11px;}
.bar-track{height:8px;background:var(--bg3);border-radius:4px;overflow:hidden;}
.bar-fill{height:100%;border-radius:4px;background:var(--accent);}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;margin-top:12px;}
@media(max-width:700px){.hero{grid-template-columns:repeat(3,1fr)}.row2{grid-template-columns:1fr}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/analytics">Analytics</a>
    <a href="/traffic">Traffic</a>
    <a href="/protocol" class="active">Protocol</a>
    <a href="/channels">Channels</a>
  </nav>
</div>
<div class="container">
  <h1>Protocol Statistics</h1>
  <p class="subtitle">802.11 frame type, subtype, and channel distribution breakdown.</p>

  <div class="hero">
    <div class="stat"><label>Total Frames</label><div class="val" id="h-frames">0</div></div>
    <div class="stat"><label>Total Bytes</label><div class="val" id="h-bytes">0</div></div>
    <div class="stat"><label>Retry Frames</label><div class="val" id="h-retry" style="color:var(--warn)">0</div></div>
    <div class="stat"><label>QoS Frames</label><div class="val" id="h-qos">0</div></div>
    <div class="stat"><label>Encrypted</label><div class="val" id="h-prot" style="color:var(--success)">0</div></div>
  </div>

  <div class="row2">
    <div class="section">
      <h3>Frame Types</h3>
      <canvas id="type-canvas" height="150"></canvas>
    </div>
    <div class="section">
      <h3>Channel Distribution</h3>
      <canvas id="ch-canvas" height="150"></canvas>
    </div>
  </div>

  <div class="row2">
    <div class="section">
      <h3>Management Subtypes</h3>
      <div class="bar-container" id="mgmt-bars"></div>
    </div>
    <div class="section">
      <h3>Data Subtypes</h3>
      <div class="bar-container" id="data-bars"></div>
    </div>
  </div>

  <div class="action-row">
    <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
    <button class="btn btn-ghost" onclick="clearData()">Reset</button>
  </div>
</div>

<script>
function fmt_bytes(b) {
  if(b>1048576) return (b/1048576).toFixed(1)+'M';
  if(b>1024) return (b/1024).toFixed(1)+'K';
  return b+'B';
}

function draw_donut(canvas_id, labels, values, colors, height=150) {
  const c = document.getElementById(canvas_id);
  if(!c) return;
  c.width = c.clientWidth||400;
  c.height = height;
  const ctx = c.getContext('2d');
  ctx.fillStyle='#1a1f2e';
  ctx.fillRect(0,0,c.width,c.height);
  const total = values.reduce((a,b)=>a+b,0);
  if(!total) return;
  const cx=c.width/2, cy=height/2, r=Math.min(cx,cy)-20, ri=r*0.5;
  let angle=-Math.PI/2;
  values.forEach((v,i)=>{
    if(!v) return;
    const sweep=(v/total)*2*Math.PI;
    ctx.beginPath();
    ctx.arc(cx,cy,r,angle,angle+sweep);
    ctx.arc(cx,cy,ri,angle+sweep,angle,true);
    ctx.closePath();
    ctx.fillStyle=colors[i];
    ctx.fill();
    angle+=sweep;
  });
  // Legend
  let lx=10;
  values.forEach((v,i)=>{
    if(!v||lx>c.width-80) return;
    ctx.fillStyle=colors[i];
    ctx.fillRect(lx, height-16, 8, 8);
    ctx.fillStyle='#94a3b8';
    ctx.font='10px system-ui';
    ctx.fillText(labels[i]+' '+Math.round(100*v/total)+'%', lx+10, height-9);
    lx+=90;
  });
}

function draw_bar_chart(canvas_id, labels, values, color, height=150) {
  const c = document.getElementById(canvas_id);
  if(!c) return;
  c.width = c.clientWidth||400;
  c.height = height;
  const ctx = c.getContext('2d');
  ctx.fillStyle='#1a1f2e';
  ctx.fillRect(0,0,c.width,c.height);
  const max_val = Math.max(...values, 1);
  const n = labels.length;
  if(!n) return;
  const bar_w = Math.floor((c.width-20)/n);
  const chart_h = height - 25;
  labels.forEach((lbl,i)=>{
    const h = Math.round(chart_h * values[i] / max_val);
    const x = 10 + i*bar_w;
    const y = chart_h - h;
    ctx.fillStyle = color;
    ctx.fillRect(x+2, y, bar_w-4, h);
    ctx.fillStyle='#64748b';
    ctx.font='8px system-ui';
    ctx.fillText(lbl, x, height-4);
  });
}

function render_bars(container_id, items, max_val) {
  const c = document.getElementById(container_id);
  if(!c) return;
  c.innerHTML = items.map(it=>`
    <div class="bar-row">
      <span>${it.name}</span>
      <div class="bar-track"><div class="bar-fill" style="width:${Math.round(100*it.count/max_val)}%"></div></div>
      <span style="text-align:right;color:var(--muted)">${it.count}</span>
    </div>`).join('');
}

async function refresh() {
  const g = await fetch('/api/prs/global').then(x=>x.json()).catch(()=>({}));
  const ch = await fetch('/api/prs/channels').then(x=>x.json()).catch(()=>[]);

  document.getElementById('h-frames').textContent = g.frames||0;
  document.getElementById('h-bytes').textContent  = fmt_bytes(g.bytes||0);
  document.getElementById('h-retry').textContent  = g.retry||0;
  document.getElementById('h-qos').textContent    = g.qos||0;
  document.getElementById('h-prot').textContent   = g.prot||0;

  const types = g.types||[0,0,0];
  draw_donut('type-canvas',
    ['Mgmt','Ctrl','Data'], types,
    ['#00d4ff','#f59e0b','#22c55e']);

  // Channel bar chart
  const ch_labels = ch.map(c=>'Ch'+c.ch);
  const ch_vals   = ch.map(c=>c.frames);
  draw_bar_chart('ch-canvas', ch_labels, ch_vals, '#00d4ff');

  // Mgmt subtypes
  const mgmt = (g.top_mgmt||[]).sort((a,b)=>b.count-a.count).slice(0,8);
  const max_m = Math.max(...mgmt.map(x=>x.count), 1);
  render_bars('mgmt-bars', mgmt, max_m);

  // Data subtypes
  const data = (g.top_data||[]).sort((a,b)=>b.count-a.count).slice(0,8);
  const max_d = Math.max(...data.map(x=>x.count), 1);
  render_bars('data-bars', data, max_d);
}

async function clearData() {
  await fetch('/api/prs/clear',{method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 2000);
</script>
</body>
</html>
)rawhtml";

inline void wuprs_routes() {
  server.on("/protocol", HTTP_GET, []() {
    server.send_P(200, "text/html", WUPRS_PAGE);
  });
}

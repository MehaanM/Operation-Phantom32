#pragma once
// webui_frame_sniffer.h — Raw frame type statistics UI.
// Route: /frame-sniffer. Prefix: wufrs_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUFRS_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Frame Sniffer — Phantom32</title>
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
.type-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(140px,1fr));gap:8px;margin-bottom:14px;}
.type-card{background:var(--bg3);border:1px solid var(--border);border-radius:6px;padding:10px;}
.type-card label{font-size:9px;color:var(--muted);text-transform:uppercase;}
.type-card .cnt{font-size:17px;font-weight:700;color:var(--text);margin-top:2px;}
.bar-row{display:flex;flex-direction:column;gap:7px;}
.bar-item{display:flex;align-items:center;gap:8px;font-size:11px;}
.bar-label{min-width:90px;color:var(--muted);}
.bar-track{flex:1;background:var(--bg3);border-radius:3px;height:8px;}
.bar-fill{height:100%;border-radius:3px;transition:width .3s;}
.bar-val{min-width:50px;text-align:right;font-size:10px;}
canvas{width:100%;height:140px;display:block;}
.ch-grid{display:grid;grid-template-columns:repeat(13,1fr);gap:4px;}
.ch-cell{background:var(--bg3);border-radius:4px;padding:5px 2px;text-align:center;}
.ch-cell .ch-num{font-size:9px;color:var(--muted);}
.ch-cell .ch-cnt{font-size:10px;font-weight:700;color:var(--accent);}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;margin-top:12px;}
@media(max-width:700px){.hero{grid-template-columns:1fr 1fr};.type-grid{grid-template-columns:1fr 1fr};.ch-grid{grid-template-columns:repeat(7,1fr)}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/analytics">Analytics</a>
    <a href="/frame-sniffer" class="active">Frame Sniffer</a>
    <a href="/protocol">Protocol Stats</a>
    <a href="/capture">Capture</a>
  </nav>
</div>
<div class="container">
  <h1>Raw Frame Sniffer</h1>
  <p class="subtitle">Live breakdown of 802.11 frame types. Counts management, control, and data frames from all channels in promiscuous mode.</p>

  <div class="hero">
    <div class="stat"><label>Total Frames</label><div class="val" id="h-total">0</div></div>
    <div class="stat"><label>Management</label><div class="val" id="h-mgmt" style="color:#a855f7">0</div></div>
    <div class="stat"><label>Data</label><div class="val" id="h-data" style="color:var(--success)">0</div></div>
    <div class="stat"><label>Control</label><div class="val" id="h-ctrl" style="color:var(--warn)">0</div></div>
  </div>

  <div class="section">
    <h3>Frame Type Breakdown</h3>
    <div class="type-grid" id="type-grid"></div>
  </div>

  <div class="section">
    <h3>Distribution</h3>
    <div class="bar-row" id="bar-rows"></div>
  </div>

  <div class="section">
    <h3>DS Direction</h3>
    <div class="bar-row" id="dir-rows"></div>
  </div>

  <div class="section">
    <h3>Per-Channel Activity</h3>
    <div class="ch-grid" id="ch-grid"></div>
  </div>

  <div class="section">
    <h3>60s History</h3>
    <canvas id="hist-canvas"></canvas>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>
</div>

<script>
const FRAME_TYPES = [
  {key:'beacon',    label:'Beacon'},
  {key:'probe_req', label:'Probe Req'},
  {key:'probe_rsp', label:'Probe Rsp'},
  {key:'auth',      label:'Auth'},
  {key:'assoc_req', label:'Assoc Req'},
  {key:'assoc_rsp', label:'Assoc Rsp'},
  {key:'disassoc',  label:'Disassoc'},
  {key:'deauth',    label:'Deauth'},
  {key:'action',    label:'Action'},
  {key:'qos_data',  label:'QoS Data'},
  {key:'null_data', label:'Null Data'},
  {key:'rts_cts',   label:'RTS/CTS'},
  {key:'ack',       label:'ACK'},
];

function fmtBytes(b) {
  if (b > 1048576) return (b/1048576).toFixed(1)+'MB';
  if (b > 1024)    return (b/1024).toFixed(1)+'KB';
  return b+'B';
}

function drawHist(r) {
  const c = document.getElementById('hist-canvas');
  c.width = c.offsetWidth; c.height = 140;
  const ctx = c.getContext('2d');
  ctx.clearRect(0, 0, c.width, c.height);
  const hT = r.hist_total||[];
  const hM = r.hist_mgmt||[];
  const hD = r.hist_data||[];
  const hC = r.hist_ctrl||[];
  if (!hT.length) return;
  const max = Math.max(...hT, 1);
  const W = c.width, H = c.height;
  const barW = W / hT.length;

  const series = [{data:hT,color:'rgba(0,212,255,0.3)'},{data:hM,color:'rgba(168,85,247,0.6)'},
                  {data:hD,color:'rgba(34,197,94,0.6)'},{data:hC,color:'rgba(245,158,11,0.5)'}];
  series.forEach(({data, color}) => {
    ctx.beginPath();
    data.forEach((v, i) => {
      const x = i * barW;
      const h = (v / max) * (H - 20);
      if (i === 0) ctx.moveTo(x, H - h);
      else ctx.lineTo(x, H - h);
    });
    ctx.strokeStyle = color;
    ctx.lineWidth = 1.5;
    ctx.stroke();
  });
}

async function refresh() {
  const r = await fetch('/api/frs/all').then(x=>x.json()).catch(()=>({}));
  document.getElementById('h-total').textContent = r.total||0;
  document.getElementById('h-mgmt').textContent  = r.mgmt||0;
  document.getElementById('h-data').textContent  = r.data||0;
  document.getElementById('h-ctrl').textContent  = r.ctrl||0;

  document.getElementById('type-grid').innerHTML = FRAME_TYPES.map(t=>`
    <div class="type-card">
      <label>${t.label}</label>
      <div class="cnt">${r[t.key]||0}</div>
    </div>`).join('');

  const total = r.total||1;
  const bars = [
    {label:'Beacon',     val:r.beacon||0,    color:'var(--accent)'},
    {label:'Probe Req',  val:r.probe_req||0, color:'rgba(168,85,247,.8)'},
    {label:'Probe Rsp',  val:r.probe_rsp||0, color:'rgba(168,85,247,.5)'},
    {label:'Data',       val:r.data||0,      color:'var(--success)'},
    {label:'Deauth',     val:r.deauth||0,    color:'var(--danger)'},
    {label:'Auth',       val:r.auth||0,      color:'var(--warn)'},
    {label:'ACK',        val:r.ack||0,       color:'rgba(100,116,139,.8)'},
  ];
  document.getElementById('bar-rows').innerHTML = bars.map(b=>`
    <div class="bar-item">
      <span class="bar-label">${b.label}</span>
      <div class="bar-track"><div class="bar-fill" style="width:${((b.val/total)*100).toFixed(1)}%;background:${b.color}"></div></div>
      <span class="bar-val">${b.val}</span>
    </div>`).join('');

  const dir = [
    {label:'From DS',val:r.dir_fromds||0},
    {label:'To DS',  val:r.dir_tods||0},
    {label:'WDS',    val:r.dir_bssds||0},
    {label:'IBSS',   val:r.dir_ibss||0},
  ];
  const dmax = Math.max(...dir.map(d=>d.val), 1);
  document.getElementById('dir-rows').innerHTML = dir.map(d=>`
    <div class="bar-item">
      <span class="bar-label">${d.label}</span>
      <div class="bar-track"><div class="bar-fill" style="width:${((d.val/dmax)*100).toFixed(1)}%;background:var(--accent)"></div></div>
      <span class="bar-val">${d.val}</span>
    </div>`).join('');

  const ch = r.per_channel||[];
  const chmax = Math.max(...ch, 1);
  document.getElementById('ch-grid').innerHTML = ch.map((v,i)=>`
    <div class="ch-cell" style="background:rgba(0,212,255,${0.04 + (v/chmax)*0.2})">
      <div class="ch-num">ch${i+1}</div>
      <div class="ch-cnt">${v}</div>
    </div>`).join('');

  drawHist(r);
}

window.addEventListener('resize', ()=>refresh());
refresh();
setInterval(refresh, 1500);
</script>
</body>
</html>
)rawhtml";

inline void wufrs_routes() {
  server.on("/frame-sniffer", HTTP_GET, []() { server.send_P(200, "text/html", WUFRS_PAGE); });
}

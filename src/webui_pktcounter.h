#pragma once
// webui_pktcounter.h — Packet counter dashboard UI.
// Route: /pktcounter. Prefix: wupkc_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUPKC_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Packet Counter — Phantom32</title>
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
.stat .sub{font-size:11px;color:var(--muted);margin-top:2px;}
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:18px;margin-bottom:14px;}
.section h3{font-size:12px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:14px;}
canvas{display:block;width:100%;max-width:900px;}
table{width:100%;border-collapse:collapse;font-size:12px;}
th{text-align:left;padding:7px 8px;font-size:10px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;}
td{padding:7px 8px;border-bottom:1px solid var(--border);}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
.bar-cell{width:120px;}
.bar-wrap{background:var(--bg3);border-radius:4px;height:8px;}
.bar-fill{height:100%;border-radius:4px;background:var(--accent);}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;margin-top:12px;}
.type-badges{display:flex;gap:8px;margin-bottom:12px;}
.badge{padding:3px 10px;border-radius:4px;font-size:11px;font-weight:600;}
@media(max-width:700px){.hero{grid-template-columns:repeat(2,1fr)}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/analytics">Analytics</a>
    <a href="/pktcounter" class="active">Packet Counter</a>
    <a href="/protocol">Protocol Stats</a>
    <a href="/timeline">Timeline</a>
  </nav>
</div>
<div class="container">
  <h1>Packet Counter</h1>
  <p class="subtitle">Real-time per-channel and global 802.11 frame counters with rate history.</p>

  <div class="hero">
    <div class="stat"><label>Total Frames</label><div class="val" id="h-frames">0</div></div>
    <div class="stat"><label>Cur Rate</label><div class="val" id="h-fps">0</div><div class="sub">frames/sec</div></div>
    <div class="stat"><label>Peak Rate</label><div class="val" id="h-peak">0</div><div class="sub">frames/sec</div></div>
    <div class="stat"><label>Uptime</label><div class="val" id="h-uptime">0s</div></div>
  </div>

  <div class="section">
    <h3>60-second FPS History</h3>
    <canvas id="fps-chart" height="80"></canvas>
  </div>

  <div class="section">
    <h3>Frame Type Breakdown</h3>
    <div class="type-badges">
      <span class="badge" style="background:rgba(0,212,255,.15);color:var(--accent)" id="b-mgmt">Mgmt: 0</span>
      <span class="badge" style="background:rgba(34,197,94,.15);color:var(--success)" id="b-ctrl">Ctrl: 0</span>
      <span class="badge" style="background:rgba(245,158,11,.15);color:var(--warn)" id="b-data">Data: 0</span>
      <span class="badge" style="background:rgba(100,116,139,.15);color:var(--muted)" id="b-retry">Retry: 0</span>
      <span class="badge" style="background:rgba(100,116,139,.15);color:var(--muted)" id="b-frag">Frag: 0</span>
      <span class="badge" style="background:rgba(100,116,139,.15);color:var(--muted)" id="b-prot">Prot: 0</span>
    </div>
  </div>

  <div class="section">
    <h3>Per-Channel Statistics</h3>
    <table>
      <thead><tr>
        <th>CH</th>
        <th>Frames</th>
        <th>FPS</th>
        <th>Mgmt</th>
        <th>Ctrl</th>
        <th>Data</th>
        <th>Retry</th>
        <th>Activity</th>
      </tr></thead>
      <tbody id="ch-tbody">
        <tr><td colspan="8" style="color:var(--muted);text-align:center">No data yet…</td></tr>
      </tbody>
    </table>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-ghost" onclick="resetCounters()">Reset</button>
    </div>
  </div>
</div>

<script>
let g_hist = [];
let g_max_frames = 0;

function fmt_num(n) {
  if(n>=1000000) return (n/1000000).toFixed(1)+'M';
  if(n>=1000)    return (n/1000).toFixed(1)+'K';
  return String(n);
}

function draw_fps_chart(hist) {
  const c = document.getElementById('fps-chart');
  const W = c.offsetWidth||900, H = 80;
  c.width = W; c.height = H;
  const ctx = c.getContext('2d');
  ctx.clearRect(0,0,W,H);

  if(!hist||!hist.length) return;
  const max_v = Math.max(...hist, 1);

  // Grid lines
  ctx.strokeStyle = '#2a3045'; ctx.lineWidth = 1;
  [0.25,0.5,0.75,1].forEach(f=>{
    const y = H - f*H;
    ctx.beginPath(); ctx.moveTo(0,y); ctx.lineTo(W,y); ctx.stroke();
  });

  // FPS line
  const step = W/(hist.length-1);
  ctx.strokeStyle = '#00d4ff'; ctx.lineWidth = 2;
  ctx.beginPath();
  hist.forEach((v,i)=>{
    const x = i*step;
    const y = H - (v/max_v)*(H-4);
    i===0?ctx.moveTo(x,y):ctx.lineTo(x,y);
  });
  ctx.stroke();

  // Fill
  ctx.strokeStyle = 'transparent';
  ctx.fillStyle = 'rgba(0,212,255,0.08)';
  ctx.beginPath();
  ctx.moveTo(0,H);
  hist.forEach((v,i)=>{
    const x=i*step, y=H-(v/max_v)*(H-4);
    ctx.lineTo(x,y);
  });
  ctx.lineTo(W,H);
  ctx.closePath();
  ctx.fill();
}

async function refresh() {
  const [sum, chs] = await Promise.all([
    fetch('/api/pkc/summary').then(x=>x.json()).catch(()=>({})),
    fetch('/api/pkc/channels').then(x=>x.json()).catch(()=>[])
  ]);

  document.getElementById('h-frames').textContent = fmt_num(sum.frames||0);
  document.getElementById('h-fps').textContent    = sum.cur_fps||0;
  document.getElementById('h-peak').textContent   = sum.peak_fps||0;

  const up = sum.uptime||0;
  const h = Math.floor(up/3600), m = Math.floor((up%3600)/60), s = up%60;
  document.getElementById('h-uptime').textContent = h?`${h}h${m}m`:`${m}m${s}s`;

  document.getElementById('b-mgmt').textContent  = 'Mgmt: '+fmt_num(sum.mgmt||0);
  document.getElementById('b-ctrl').textContent  = 'Ctrl: '+fmt_num(sum.ctrl||0);
  document.getElementById('b-data').textContent  = 'Data: '+fmt_num(sum.data||0);
  document.getElementById('b-retry').textContent = 'Retry: '+fmt_num(sum.retry||0);
  document.getElementById('b-frag').textContent  = 'Frag: '+fmt_num(sum.frag||0);
  document.getElementById('b-prot').textContent  = 'Prot: '+fmt_num(sum.prot||0);

  draw_fps_chart(sum.fps_ring||[]);

  // Channel table
  const max_f = Math.max(...(chs.map(c=>c.frames)), 1);
  document.getElementById('ch-tbody').innerHTML = chs.length
    ? chs.map(c=>`<tr>
        <td><strong>Ch ${c.ch}</strong></td>
        <td>${fmt_num(c.frames)}</td>
        <td>${c.fps}</td>
        <td>${fmt_num(c.mgmt)}</td>
        <td>${fmt_num(c.ctrl)}</td>
        <td>${fmt_num(c.data)}</td>
        <td>${fmt_num(c.retry)}</td>
        <td class="bar-cell">
          <div class="bar-wrap">
            <div class="bar-fill" style="width:${Math.round(c.frames/max_f*100)}%"></div>
          </div>
        </td>
      </tr>`).join('')
    : '<tr><td colspan="8" style="color:var(--muted);text-align:center">No channel data.</td></tr>';
}

async function resetCounters() {
  await fetch('/api/pkc/reset',{method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 1000);
</script>
</body>
</html>
)rawhtml";

inline void wupkc_routes() {
  server.on("/pktcounter", HTTP_GET, []() {
    server.send_P(200, "text/html", WUPKC_PAGE);
  });
}

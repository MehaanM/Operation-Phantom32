#pragma once
// webui_channels.h — Channel statistics and spectrum analysis web UI.
// Route: /channels. Prefix: wuch_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUCH_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Channels — Phantom32</title>
<style>
:root{--bg:#0a0c10;--bg2:#12151c;--bg3:#1a1f2e;--border:#2a3045;
  --accent:#00d4ff;--success:#22c55e;--warn:#f59e0b;--danger:#ef4444;
  --text:#e2e8f0;--muted:#64748b;--radius:8px;}
*{box-sizing:border-box;margin:0;padding:0;}
body{font-family:'Inter',system-ui,sans-serif;background:var(--bg);color:var(--text);min-height:100vh;}
.top-bar{display:flex;align-items:center;gap:12px;padding:14px 20px;
  background:var(--bg2);border-bottom:1px solid var(--border);position:sticky;top:0;z-index:10;}
.logo{font-size:18px;font-weight:700;color:var(--accent);letter-spacing:2px;}
.nav-links{display:flex;gap:8px;margin-left:auto;}
.nav-links a{color:var(--muted);text-decoration:none;font-size:13px;padding:5px 10px;border-radius:5px;transition:all .2s;}
.nav-links a:hover{color:var(--text);background:var(--bg3);}
.nav-links a.active{color:var(--accent);background:rgba(0,212,255,.08);}
.container{max-width:1100px;margin:0 auto;padding:24px 16px;}
h1{font-size:22px;font-weight:700;margin-bottom:4px;}
.subtitle{color:var(--muted);font-size:14px;margin-bottom:24px;}
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:20px;margin-bottom:16px;}
.section h3{font-size:13px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:16px;}
.stat-row{display:grid;grid-template-columns:repeat(4,1fr);gap:12px;margin-bottom:20px;}
.stat{background:var(--bg2);border:1px solid var(--border);border-radius:8px;padding:14px;}
.stat label{font-size:11px;color:var(--muted);text-transform:uppercase;}
.stat .val{font-size:22px;font-weight:700;color:var(--accent);margin-top:4px;}
canvas{display:block;width:100%;border-radius:5px;}
.btn{padding:7px 14px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;transition:all .2s;}
.btn-primary{background:var(--accent);color:#000;}
.btn-primary:hover{opacity:.85;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;flex-wrap:wrap;margin-top:14px;}
.ch-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(80px,1fr));gap:8px;margin-top:4px;}
.ch-card{background:var(--bg3);border:1px solid var(--border);border-radius:6px;padding:10px;text-align:center;cursor:pointer;transition:all .2s;}
.ch-card:hover,.ch-card.sel{border-color:var(--accent);background:rgba(0,212,255,.07);}
.ch-card .ch-num{font-size:20px;font-weight:700;color:var(--accent);}
.ch-card .ch-aps{font-size:11px;color:var(--muted);}
.ch-card .ch-util{font-size:12px;margin-top:4px;}
.detail-box{background:var(--bg3);border-radius:5px;padding:14px;font-size:13px;}
.detail-box .row{display:flex;justify-content:space-between;padding:5px 0;border-bottom:1px solid var(--border);}
.detail-box .row:last-child{border-bottom:none;}
.detail-box .row span:last-child{color:var(--accent);font-weight:500;}
@media(max-width:700px){.stat-row{grid-template-columns:1fr 1fr;}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/">Dashboard</a>
    <a href="/analytics">Analytics</a>
    <a href="/forensics">Forensics</a>
    <a href="/devices">Devices</a>
    <a href="/capture">Capture</a>
    <a href="/scheduler">Scheduler</a>
    <a href="/channels" class="active">Channels</a>
    <a href="/settings">Settings</a>
  </nav>
</div>

<div class="container">
  <h1>Channel Analysis</h1>
  <p class="subtitle">2.4 GHz band utilization, AP density, and interference scores per channel.</p>

  <div class="stat-row">
    <div class="stat"><label>Best Channel</label><div class="val" id="st-best">—</div></div>
    <div class="stat"><label>Busiest Ch</label><div class="val" id="st-busy">—</div></div>
    <div class="stat"><label>Total Frames/s</label><div class="val" id="st-fps">0</div></div>
    <div class="stat"><label>Active Channels</label><div class="val" id="st-active">0</div></div>
  </div>

  <div class="section">
    <h3>Channel Utilization (Frames/s)</h3>
    <canvas id="chart-util" height="140"></canvas>
  </div>

  <div class="section">
    <h3>Channel Grid — Click for Details</h3>
    <div class="ch-grid" id="ch-grid"></div>
    <div id="ch-detail" style="margin-top:16px;display:none;">
      <h3 style="font-size:13px;font-weight:600;color:var(--accent);text-transform:uppercase;margin-bottom:10px;">
        Channel <span id="det-ch"></span> Details</h3>
      <div class="detail-box" id="det-box"></div>
    </div>
  </div>

  <div class="section">
    <h3>Interference Map (Channel Overlap 2.4 GHz)</h3>
    <canvas id="chart-overlap" height="100"></canvas>
    <p style="font-size:11px;color:var(--muted);margin-top:8px">
      Channels 1, 6, 11 are non-overlapping. Bars show relative interference from adjacent channels.</p>
  </div>

  <div class="action-row">
    <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
    <button class="btn btn-ghost" onclick="clearStats()">Clear Stats</button>
  </div>
</div>

<script>
let chData = [];
let selCh = -1;

function drawUtil(data) {
  const c = document.getElementById('chart-util');
  const ctx = c.getContext('2d');
  c.height = 140;
  const W = c.offsetWidth; c.width = W;
  ctx.clearRect(0,0,W,140);

  if (!data.length) return;
  const maxRate = Math.max(...data.map(d=>d.rate_1s||0), 1);
  const barW = Math.floor(W / 14) - 4;
  const channels = data.filter(d=>d.channel);

  ctx.fillStyle='rgba(0,212,255,0.08)';
  ctx.fillRect(0,0,W,140);

  for (let i = 0; i < 14; i++) {
    const ch = channels.find(d=>d.channel===i+1) || {};
    const rate = ch.rate_1s || 0;
    const h = Math.round(100 * rate / maxRate);
    const x = i * (barW + 4) + 2;
    const y = 110 - h;

    // Color by utilization
    const pct = rate/maxRate;
    const r = Math.round(255*pct), g = Math.round(100*(1-pct)+212*pct);
    ctx.fillStyle = `rgba(${r},${g},255,0.7)`;
    ctx.fillRect(x, y, barW, h);
    ctx.fillRect(x, y, barW, h);

    // Label
    ctx.fillStyle='#64748b';
    ctx.font='10px Inter,sans-serif';
    ctx.textAlign='center';
    ctx.fillText(i+1, x + barW/2, 128);
    if (rate > 0) {
      ctx.fillStyle='#e2e8f0';
      ctx.font='9px Inter,sans-serif';
      ctx.fillText(rate.toFixed(1), x+barW/2, Math.max(y-2, 10));
    }

    // Non-overlapping marker
    if (i===0 || i===5 || i===10) {
      ctx.fillStyle='rgba(34,197,94,0.3)';
      ctx.fillRect(x, 112, barW, 4);
    }
  }
  ctx.fillStyle='#22c55e'; ctx.font='9px Inter,sans-serif'; ctx.textAlign='left';
  ctx.fillText('★ Non-overlapping channels (1,6,11) shown in green', 4, 138);
}

function drawOverlap(data) {
  const c = document.getElementById('chart-overlap');
  const ctx = c.getContext('2d');
  c.height = 100;
  const W = c.offsetWidth; c.width = W;
  ctx.clearRect(0,0,W,100);

  const CH_FREQ = [2412,2417,2422,2427,2432,2437,2442,2447,2452,2457,2462,2467,2472,2484];
  const BW = 22; // MHz

  const chRates = Array(14).fill(0);
  data.forEach(d=>{ if(d.channel>=1&&d.channel<=14) chRates[d.channel-1]=d.rate_1s||0; });

  for (let i = 0; i < 14; i++) {
    const fi = CH_FREQ[i];
    let interference = 0;
    for (let j = 0; j < 14; j++) {
      if (i===j) continue;
      const fj = CH_FREQ[j];
      const overlap = Math.max(0, BW - Math.abs(fi-fj));
      interference += (overlap / BW) * chRates[j];
    }
    const h = Math.min(70, Math.round(70 * interference / Math.max(...chRates, 1)));
    const barW = Math.floor(W/14)-4;
    const x = i*(barW+4)+2;

    const alpha = i===0||i===5||i===10 ? 0.9 : 0.5;
    ctx.fillStyle = i===0||i===5||i===10 ? `rgba(34,197,94,${alpha})` : `rgba(239,68,68,${alpha})`;
    ctx.fillRect(x, 70-h, barW, h);

    ctx.fillStyle='#64748b'; ctx.font='9px Inter,sans-serif'; ctx.textAlign='center';
    ctx.fillText(i+1, x+barW/2, 88);
  }
}

function renderGrid(data) {
  const grid = document.getElementById('ch-grid');
  grid.innerHTML = '';
  for (let ch = 1; ch <= 14; ch++) {
    const d = data.find(x=>x.channel===ch) || {channel:ch,ap_count:0,rate_1s:0,interference:0};
    const util = Math.round((d.rate_1s||0));
    const sel  = ch === selCh ? ' sel' : '';
    grid.innerHTML += `
      <div class="ch-card${sel}" onclick="selectCh(${ch})">
        <div class="ch-num">${ch}</div>
        <div class="ch-aps">${d.ap_count||0} APs</div>
        <div class="ch-util" style="color:${util>50?'#ef4444':util>20?'#f59e0b':'#22c55e'}">${util}/s</div>
      </div>`;
  }
}

function selectCh(ch) {
  selCh = ch;
  renderGrid(chData);

  const d = chData.find(x=>x.channel===ch) || {channel:ch};
  document.getElementById('det-ch').textContent = ch;
  document.getElementById('ch-detail').style.display = 'block';

  const freq = ch<=13 ? (2412+(ch-1)*5) : 2484;
  const avg = (d.avg_rssi||0).toFixed(1);
  const best = (d.best_rssi||0);
  const intf = (d.interference||0).toFixed(1);

  document.getElementById('det-box').innerHTML = `
    <div class="row"><span>Frequency</span><span>${freq} MHz</span></div>
    <div class="row"><span>AP Count</span><span>${d.ap_count||0}</span></div>
    <div class="row"><span>Frames/s (1s avg)</span><span>${(d.rate_1s||0).toFixed(1)}</span></div>
    <div class="row"><span>Frames/s (30s avg)</span><span>${(d.avg_rate||0).toFixed(1)}</span></div>
    <div class="row"><span>Total Frames</span><span>${d.total_frames||0}</span></div>
    <div class="row"><span>Avg RSSI</span><span>${avg} dBm</span></div>
    <div class="row"><span>Best RSSI</span><span>${best} dBm</span></div>
    <div class="row"><span>Interference Score</span><span>${intf} / 100</span></div>
    <div class="row"><span>Non-overlapping</span><span>${(ch===1||ch===6||ch===11)?'✓ Yes':'No'}</span></div>`;
}

async function refresh() {
  const r = await fetch('/api/chs/all').then(x=>x.json()).catch(()=>({}));
  if (!r.channels) return;

  chData = r.channels;
  renderGrid(r.channels);
  drawUtil(r.channels);
  drawOverlap(r.channels);

  const best = r.best_channel||'—';
  document.getElementById('st-best').textContent = best;

  const busy = r.channels.reduce((m,d)=>d.rate_1s>m.rate ? {ch:d.channel,rate:d.rate_1s} : m, {ch:'—',rate:0});
  document.getElementById('st-busy').textContent = busy.ch;

  const totalFps = r.channels.reduce((s,d)=>s+(d.rate_1s||0),0);
  document.getElementById('st-fps').textContent = totalFps.toFixed(1);

  const active = r.channels.filter(d=>d.rate_1s>0).length;
  document.getElementById('st-active').textContent = active;

  if (selCh > 0) selectCh(selCh);
}

async function clearStats() {
  await fetch('/api/chs/clear',{method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 3000);
window.addEventListener('resize', ()=>{drawUtil(chData);drawOverlap(chData);});
</script>
</body>
</html>
)rawhtml";

inline void wuch_routes() {
  server.on("/channels", HTTP_GET, []() {
    server.send_P(200, "text/html", WUCH_PAGE);
  });
}

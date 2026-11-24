#pragma once
// webui_noise.h — Noise floor visualization and RF environment health UI.
// Route: /noise. Prefix: wunf_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUNF_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>RF Noise — Phantom32</title>
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
.stat .val.warn{color:var(--warn);}
.stat .val.danger{color:var(--danger);}
.btn{padding:7px 14px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;transition:all .2s;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.btn-primary{background:var(--accent);color:#000;}
.btn-primary:hover{opacity:.85;}
.action-row{display:flex;gap:8px;flex-wrap:wrap;margin-top:14px;}
canvas{border-radius:5px;background:var(--bg3);display:block;width:100%;}
.channel-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(120px,1fr));gap:10px;}
.ch-card{background:var(--bg3);border:1px solid var(--border);border-radius:6px;padding:12px;cursor:pointer;transition:all .2s;}
.ch-card:hover{border-color:var(--accent);}
.ch-card.selected{border-color:var(--accent);background:rgba(0,212,255,.05);}
.ch-card.alert{border-color:var(--danger);background:rgba(239,68,68,.05);}
.ch-title{font-size:13px;font-weight:700;color:var(--accent);margin-bottom:4px;}
.ch-noise{font-size:20px;font-weight:700;margin-bottom:2px;}
.ch-baseline{font-size:11px;color:var(--muted);}
.ch-samples{font-size:11px;color:var(--muted);}
.alert-badge{display:inline-block;padding:1px 6px;border-radius:4px;background:rgba(239,68,68,.2);color:var(--danger);font-size:10px;margin-top:4px;}
.hist-label{font-size:11px;color:var(--muted);margin-bottom:6px;}
@media(max-width:700px){.stat-row{grid-template-columns:1fr 1fr;}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/">Dashboard</a>
    <a href="/analytics">Analytics</a>
    <a href="/channels">Channels</a>
    <a href="/noise" class="active">RF Noise</a>
    <a href="/settings">Settings</a>
  </nav>
</div>

<div class="container">
  <h1>RF Noise Floor</h1>
  <p class="subtitle">Per-channel noise floor estimation and interference detection based on RSSI histogram analysis.</p>

  <div class="stat-row">
    <div class="stat"><label>Best Channel</label><div class="val" id="st-best">—</div></div>
    <div class="stat"><label>Cleanest Floor</label><div class="val" id="st-floor">—</div></div>
    <div class="stat"><label>Channels in Alert</label><div class="val" id="st-alerts" style="color:var(--danger)">0</div></div>
    <div class="stat"><label>Total Samples</label><div class="val" id="st-samples">0</div></div>
  </div>

  <div class="section">
    <h3>Channel Noise Overview</h3>
    <div class="channel-grid" id="ch-grid">
      <div style="color:var(--muted)">Loading…</div>
    </div>
  </div>

  <div class="section">
    <h3>Noise Floor Timeline</h3>
    <div style="color:var(--muted);font-size:12px;margin-bottom:10px">
      Hover over a channel card above to see its RSSI histogram below.
    </div>
    <div class="hist-label" id="hist-title">Select a channel above</div>
    <canvas id="hist-canvas" height="140"></canvas>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-primary" onclick="clearNoise()">Reset Baselines</button>
    </div>
  </div>

  <div class="section">
    <h3>Noise Floor Map (Channels 1–13)</h3>
    <canvas id="map-canvas" height="120"></canvas>
    <div style="font-size:11px;color:var(--muted);margin-top:8px">
      Lower noise floor = better for use. Alert threshold shown in red.
    </div>
  </div>
</div>

<script>
let g_channels = [];
let g_selected_ch = 1;

function nflColor(floor, baseline, alert) {
  if (alert) return '#ef4444';
  const diff = floor - baseline;
  if (diff > 5) return '#ef4444';
  if (diff > 2) return '#f59e0b';
  return '#22c55e';
}

async function refresh() {
  const r = await fetch('/api/nfl/all').then(x=>x.json()).catch(()=>({}));
  g_channels = r.channels || [];

  let best_ch = 0, best_floor = -50;
  let total_samples = 0;
  let alerts = 0;

  g_channels.forEach(c=>{
    total_samples += c.samples||0;
    if ((c.noise_floor||0) < best_floor || best_ch===0) {
      best_floor = c.noise_floor||0;
      best_ch = c.ch;
    }
    if (c.alert) alerts++;
  });

  document.getElementById('st-best').textContent    = best_ch ? 'ch '+best_ch : '—';
  document.getElementById('st-floor').textContent   = best_floor ? best_floor+' dBm' : '—';
  document.getElementById('st-alerts').textContent  = alerts;
  document.getElementById('st-samples').textContent = total_samples;

  // Channel grid
  document.getElementById('ch-grid').innerHTML = g_channels.map(c=>{
    const col = nflColor(c.noise_floor||0, c.baseline||0, c.alert);
    return `<div class="ch-card ${c.alert?'alert':''} ${c.ch===g_selected_ch?'selected':''}"
              onclick="selectChannel(${c.ch})">
      <div class="ch-title">Channel ${c.ch}</div>
      <div class="ch-noise" style="color:${col}">${c.noise_floor||'?'} <span style="font-size:12px">dBm</span></div>
      <div class="ch-baseline">Baseline: ${c.baseline||'?'} dBm</div>
      <div class="ch-samples">${c.samples||0} samples</div>
      ${c.alert?'<div class="alert-badge">ELEVATED</div>':''}
    </div>`;
  }).join('');

  drawMap();
  if (g_selected_ch) loadHistogram(g_selected_ch);
}

function selectChannel(ch) {
  g_selected_ch = ch;
  refresh();
}

async function loadHistogram(ch) {
  const r = await fetch('/api/nfl/hist?ch='+ch).then(x=>x.json()).catch(()=>({}));
  if (!r.bins) return;

  document.getElementById('hist-title').textContent = 'Channel '+ch+' RSSI Histogram';

  const canvas = document.getElementById('hist-canvas');
  const ctx = canvas.getContext('2d');
  const W = canvas.clientWidth || 600;
  canvas.width = W;

  ctx.fillStyle = '#1a1f2e';
  ctx.fillRect(0, 0, W, 140);

  const bins = r.bins;
  const max_count = Math.max(...bins, 1);
  const bar_w = Math.floor((W - 40) / bins.length);

  bins.forEach((count, i) => {
    const x = 40 + i * bar_w;
    const h = Math.round(90 * count / max_count);
    const rssi = -100 + i * (40 / bins.length);

    // Color: near noise floor = yellow, far below = green
    const heat = count / max_count;
    const r_ = Math.round(heat * 239);
    const g_ = Math.round((1-heat) * 212 + heat * 68);
    ctx.fillStyle = `rgb(${r_},${g_},68)`;
    ctx.fillRect(x, 100 - h, bar_w - 1, h);
  });

  // X axis labels
  ctx.fillStyle = '#64748b';
  ctx.font = '10px system-ui';
  ctx.fillText('-100', 40, 125);
  ctx.fillText('-80', 40 + Math.floor(bins.length/2)*bar_w, 125);
  ctx.fillText('-60', 40 + (bins.length-1)*bar_w, 125);

  // Noise floor line
  if (r.noise_floor) {
    const nf_bin = Math.round((r.noise_floor + 100) * bins.length / 40);
    const nf_x = 40 + nf_bin * bar_w;
    ctx.strokeStyle = '#00d4ff';
    ctx.lineWidth = 1;
    ctx.setLineDash([3,3]);
    ctx.beginPath(); ctx.moveTo(nf_x, 10); ctx.lineTo(nf_x, 110); ctx.stroke();
    ctx.setLineDash([]);
    ctx.fillStyle = '#00d4ff';
    ctx.fillText('NF', nf_x+2, 20);
  }
}

function drawMap() {
  const canvas = document.getElementById('map-canvas');
  const ctx = canvas.getContext('2d');
  const W = canvas.clientWidth || 700;
  canvas.width = W;

  ctx.fillStyle = '#1a1f2e';
  ctx.fillRect(0, 0, W, 120);

  if (!g_channels.length) return;

  const ch_w = Math.floor((W - 40) / 14);
  const min_floor = -105, max_floor = -50;

  g_channels.slice(0,13).forEach((c, i) => {
    const floor = c.noise_floor || -90;
    const h = Math.round(80 * (floor - min_floor) / (max_floor - min_floor));
    const x = 40 + i * ch_w;
    const col = c.alert ? '#ef4444' : (floor > -75 ? '#f59e0b' : '#22c55e');

    ctx.fillStyle = col+'33';
    ctx.fillRect(x, 100 - h, ch_w - 2, h);

    ctx.strokeStyle = col;
    ctx.lineWidth = 1;
    ctx.strokeRect(x, 100 - h, ch_w - 2, h);

    ctx.fillStyle = '#e2e8f0';
    ctx.font = '10px system-ui';
    ctx.fillText(c.ch, x + 2, 115);
    ctx.fillStyle = col;
    ctx.fillText(floor, x, 100 - h - 3);
  });
}

async function clearNoise() {
  if (!confirm('Reset noise floor baselines?')) return;
  await fetch('/api/nfl/clear', {method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 4000);
</script>
</body>
</html>
)rawhtml";

inline void wunf_routes() {
  server.on("/noise", HTTP_GET, []() {
    server.send_P(200, "text/html", WUNF_PAGE);
  });
}

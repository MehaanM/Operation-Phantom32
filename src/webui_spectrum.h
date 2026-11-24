#pragma once
// webui_spectrum.h — Spectrum analyzer UI with waterfall chart.
// Route: /spectrum. Prefix: wusps_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUSPS_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Spectrum Scan — Phantom32</title>
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
#spectrum-canvas{width:100%;height:200px;background:var(--bg3);border-radius:7px;cursor:crosshair;}
#waterfall-canvas{width:100%;height:140px;background:#000;border-radius:7px;margin-top:8px;}
.canvas-label{font-size:9px;color:var(--muted);display:flex;justify-content:space-between;margin-top:3px;}
table{width:100%;border-collapse:collapse;font-size:11px;}
th{text-align:left;padding:7px 8px;font-size:9px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;cursor:pointer;}
th:hover{color:var(--text);}
td{padding:6px 8px;border-bottom:1px solid var(--border);}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
.busy-bar{height:6px;border-radius:3px;background:var(--border);}
.busy-fill{height:100%;border-radius:3px;}
.badge{padding:1px 5px;border-radius:3px;font-size:9px;font-weight:700;display:inline-block;}
.badge-nw{background:rgba(239,68,68,.2);color:var(--danger);}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;margin-top:12px;}
.band-label{display:inline-block;padding:1px 5px;border-radius:3px;font-size:9px;background:rgba(0,212,255,.08);color:var(--accent);}
@media(max-width:700px){.hero{grid-template-columns:1fr 1fr}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/noise-floor">Noise Floor</a>
    <a href="/spectrum" class="active">Spectrum</a>
    <a href="/channel-util">CH Util</a>
  </nav>
</div>
<div class="container">
  <h1>Spectrum Analyzer</h1>
  <p class="subtitle">Per-channel occupancy and RSSI-domain visualization from observed 802.11 frames. CCA busy time is estimated from observed frame density.</p>

  <div class="hero">
    <div class="stat"><label>Channels</label><div class="val" id="h-cnt">0</div></div>
    <div class="stat"><label>Cleanest 2.4GHz</label><div class="val" id="h-clean24" style="font-size:16px">—</div></div>
    <div class="stat"><label>Cleanest 5GHz</label><div class="val" id="h-clean5" style="font-size:16px">—</div></div>
    <div class="stat"><label>Interference</label><div class="val" id="h-inter" style="color:var(--warn)">0</div></div>
  </div>

  <div class="section">
    <h3>Spectrum View</h3>
    <canvas id="spectrum-canvas"></canvas>
    <canvas id="waterfall-canvas"></canvas>
    <div class="canvas-label">
      <span>2.4GHz (CH 1-13)</span>
      <span>5GHz (CH 36+)</span>
    </div>
  </div>

  <div class="section">
    <h3>Channel Detail Table</h3>
    <table>
      <thead>
        <tr>
          <th onclick="sortTable('ch')">CH</th>
          <th onclick="sortTable('freq')">Freq</th>
          <th>Band</th>
          <th onclick="sortTable('cca')">CCA Busy</th>
          <th onclick="sortTable('occ')">Occupancy</th>
          <th onclick="sortTable('noise')">Noise</th>
          <th onclick="sortTable('peak')">Peak</th>
          <th onclick="sortTable('frames')">Frames</th>
          <th>Non-WiFi</th>
        </tr>
      </thead>
      <tbody id="ch-tbody"></tbody>
    </table>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>
</div>

<script>
let g_chs = [];
let g_waterfall = [];
let g_sort = {key:'ch', asc:true};

function busyColor(pct) {
  if (pct >= 70) return 'var(--danger)';
  if (pct >= 40) return 'var(--warn)';
  return 'var(--success)';
}

function drawSpectrum() {
  const canvas = document.getElementById('spectrum-canvas');
  const W = canvas.offsetWidth, H = canvas.offsetHeight;
  if (!W || !H) return;
  canvas.width = W; canvas.height = H;
  const ctx = canvas.getContext('2d');
  ctx.clearRect(0, 0, W, H);

  // Sort channels by freq for x-axis
  const sorted = [...g_chs].sort((a,b) => a.freq - b.freq);
  if (!sorted.length) return;

  const minF = sorted[0].freq - 10;
  const maxF = sorted[sorted.length-1].freq + 10;
  const fRange = maxF - minF;

  const freqX = f => ((f - minF) / fRange) * W;

  sorted.forEach(c => {
    const x = freqX(c.freq);
    const bw = (c.bw || 20) / 2;
    const x0 = freqX(c.freq - bw);
    const x1 = freqX(c.freq + bw);
    const h  = (c.cca / 100) * H;

    // Draw channel shape
    const grad = ctx.createLinearGradient(x, H - h, x, H);
    const busy = c.cca;
    const r = busy >= 70 ? 239 : busy >= 40 ? 245 : 0;
    const g2 = busy >= 70 ? 68 : busy >= 40 ? 158 : 212;
    const b2 = busy >= 70 ? 68 : busy >= 40 ? 11 : 255;
    grad.addColorStop(0, `rgba(${r},${g2},${b2},0.8)`);
    grad.addColorStop(1, `rgba(${r},${g2},${b2},0.1)`);
    ctx.fillStyle = grad;
    ctx.beginPath();
    ctx.moveTo(x0, H);
    ctx.lineTo(x0, H - h * 0.1);
    ctx.quadraticCurveTo(x, H - h, x1, H - h * 0.1);
    ctx.lineTo(x1, H);
    ctx.closePath();
    ctx.fill();

    // Label
    ctx.fillStyle = 'rgba(226,232,240,0.7)';
    ctx.font = '9px system-ui';
    ctx.textAlign = 'center';
    ctx.fillText('CH' + c.ch, x, H - h - 4);
  });

  // Grid lines
  ctx.strokeStyle = 'rgba(42,48,69,0.4)';
  ctx.lineWidth = 1;
  for (let i = 0; i <= 4; i++) {
    const y = (H / 4) * i;
    ctx.beginPath();
    ctx.moveTo(0, y);
    ctx.lineTo(W, y);
    ctx.stroke();
    ctx.fillStyle = 'rgba(100,116,139,0.6)';
    ctx.font = '8px system-ui';
    ctx.textAlign = 'left';
    ctx.fillText(`${100 - i * 25}%`, 2, y + 8);
  }
}

function drawWaterfall() {
  const canvas = document.getElementById('waterfall-canvas');
  const W = canvas.offsetWidth, H = canvas.offsetHeight;
  if (!W || !H) return;
  canvas.width = W; canvas.height = H;
  const ctx = canvas.getContext('2d');
  ctx.fillStyle = '#000';
  ctx.fillRect(0, 0, W, H);

  if (!g_chs.length) return;
  const sorted = [...g_chs].sort((a,b) => a.freq - b.freq);
  const minF = sorted[0].freq - 10;
  const maxF = sorted[sorted.length-1].freq + 10;
  const fRange = maxF - minF;
  const freqX = f => ((f - minF) / fRange) * W;

  // Draw waterfall from busy_hist
  sorted.forEach(c => {
    if (!c.busy_hist || !c.busy_hist.length) return;
    const x0 = Math.round(freqX(c.freq - (c.bw||20)/2));
    const x1 = Math.round(freqX(c.freq + (c.bw||20)/2));
    const w  = Math.max(1, x1 - x0);
    const rows = c.busy_hist.length;
    c.busy_hist.forEach((val, row) => {
      const y = Math.round((row / rows) * H);
      const rh = Math.max(1, Math.round(H / rows));
      const alpha = val / 100;
      const r = val >= 70 ? 239 : val >= 40 ? 245 : 0;
      const g2 = val >= 70 ? 68 : val >= 40 ? 158 : 180;
      const b2 = val >= 70 ? 68 : val >= 40 ? 11 : 255;
      ctx.fillStyle = `rgba(${r},${g2},${b2},${alpha})`;
      ctx.fillRect(x0, y, w, rh);
    });
  });
}

function sortTable(key) {
  if (g_sort.key === key) g_sort.asc = !g_sort.asc;
  else { g_sort.key = key; g_sort.asc = true; }
  renderTable();
}

function renderTable() {
  const sorted = [...g_chs].sort((a,b) => {
    const va = a[g_sort.key], vb = b[g_sort.key];
    return g_sort.asc ? va - vb : vb - va;
  });
  document.getElementById('ch-tbody').innerHTML = sorted.length
    ? sorted.map(c => `<tr>
        <td><strong>${c.ch}</strong></td>
        <td style="color:var(--muted)">${c.freq} MHz</td>
        <td><span class="band-label">${c.is5 ? '5GHz' : '2.4GHz'}</span></td>
        <td><div class="busy-bar"><div class="busy-fill" style="width:${c.cca}%;background:${busyColor(c.cca)}"></div></div> ${c.cca}%</td>
        <td style="color:${busyColor(c.occ)}">${c.occ}%</td>
        <td style="color:var(--warn)">${c.noise} dBm</td>
        <td style="color:var(--success)">${c.peak} dBm</td>
        <td style="color:var(--muted)">${c.frames.toLocaleString()}</td>
        <td>${c.non_wifi ? '<span class="badge badge-nw">⚡ Detected</span>' : '—'}</td>
      </tr>`).join('')
    : '<tr><td colspan="9" style="color:var(--muted);text-align:center">No data.</td></tr>';
}

async function refresh() {
  const r = await fetch('/api/sps/all').then(x => x.json()).catch(() => ({}));
  g_chs = r.channels || [];
  document.getElementById('h-cnt').textContent = r.count || 0;
  document.getElementById('h-inter').textContent = g_chs.filter(c => c.non_wifi).length;

  const ch24 = g_chs.filter(c => !c.is5 && c.frames > 0);
  const ch5  = g_chs.filter(c =>  c.is5 && c.frames > 0);
  const best24 = ch24.length ? ch24.reduce((a,b) => a.cca < b.cca ? a : b) : null;
  const best5  = ch5.length  ? ch5.reduce( (a,b) => a.cca < b.cca ? a : b) : null;
  document.getElementById('h-clean24').textContent = best24 ? `CH ${best24.ch}` : '—';
  document.getElementById('h-clean5').textContent  = best5  ? `CH ${best5.ch}`  : '—';

  renderTable();
  drawSpectrum();
  drawWaterfall();
}

async function clearData() {
  await fetch('/api/sps/clear', {method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 2000);
window.addEventListener('resize', () => { drawSpectrum(); drawWaterfall(); });
</script>
</body>
</html>
)rawhtml";

inline void wusps_routes() {
  server.on("/spectrum", HTTP_GET, []() {
    server.send_P(200, "text/html", WUSPS_PAGE);
  });
}

#pragma once
// webui_analytics.h — Full analytics dashboard with real-time charts, packet rate graphs,
// RSSI history, threat timeline, channel utilization heatmap. Prefix: wua_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

// ─── PROGMEM analytics page ─────────────────────────────────────────────────
static const char WUA_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Phantom32 · Analytics</title>
<style>
:root {
  --bg: #0a0e1a;
  --surface: #111827;
  --surface2: #1a2235;
  --border: #2d3748;
  --accent: #00d4ff;
  --accent2: #7c3aed;
  --accent3: #10b981;
  --accent4: #f59e0b;
  --accent5: #ef4444;
  --text: #e2e8f0;
  --muted: #64748b;
  --chart-h: 180px;
}
*{margin:0;padding:0;box-sizing:border-box;}
body{background:var(--bg);color:var(--text);font-family:'Segoe UI',system-ui,sans-serif;font-size:13px;}
header{background:var(--surface);border-bottom:1px solid var(--border);padding:12px 20px;display:flex;align-items:center;gap:16px;}
header h1{font-size:18px;font-weight:700;color:var(--accent);letter-spacing:.5px;}
.badge{background:var(--accent2);color:#fff;padding:2px 8px;border-radius:12px;font-size:11px;}
nav{display:flex;gap:4px;margin-left:auto;}
nav a{color:var(--muted);text-decoration:none;padding:6px 12px;border-radius:6px;font-size:12px;transition:all .2s;}
nav a:hover,nav a.active{background:var(--surface2);color:var(--accent);}
.container{padding:16px;display:grid;grid-template-columns:repeat(4,1fr);gap:12px;}
.stat-card{background:var(--surface);border:1px solid var(--border);border-radius:10px;padding:14px;display:flex;flex-direction:column;gap:4px;}
.stat-label{font-size:11px;color:var(--muted);text-transform:uppercase;letter-spacing:.5px;}
.stat-value{font-size:28px;font-weight:700;line-height:1;}
.stat-delta{font-size:11px;display:flex;align-items:center;gap:4px;}
.delta-up{color:var(--accent3);}
.delta-down{color:var(--accent5);}
.chart-row{grid-column:1/-1;display:grid;grid-template-columns:2fr 1fr;gap:12px;}
.chart-card{background:var(--surface);border:1px solid var(--border);border-radius:10px;padding:14px;}
.chart-card h3{font-size:13px;font-weight:600;color:var(--text);margin-bottom:12px;display:flex;justify-content:space-between;align-items:center;}
.chart-card h3 span{font-size:11px;color:var(--muted);}
canvas{width:100% !important;height:var(--chart-h) !important;}
.full-row{grid-column:1/-1;}
.heatmap-grid{display:grid;grid-template-columns:repeat(14,1fr);gap:3px;margin-top:8px;}
.hm-cell{aspect-ratio:1;border-radius:3px;background:var(--surface2);position:relative;cursor:pointer;}
.hm-cell:hover::after{content:attr(data-tip);position:absolute;bottom:calc(100%+4px);left:50%;transform:translateX(-50%);background:#000;color:#fff;padding:4px 8px;border-radius:4px;font-size:10px;white-space:nowrap;z-index:10;pointer-events:none;}
.hm-label{font-size:10px;color:var(--muted);display:flex;justify-content:space-between;margin-top:4px;}
.timeline-wrap{max-height:260px;overflow-y:auto;}
.timeline-item{display:flex;gap:10px;padding:8px 0;border-bottom:1px solid var(--border);}
.tl-time{color:var(--muted);font-size:11px;white-space:nowrap;width:60px;}
.tl-icon{width:20px;text-align:center;flex-shrink:0;}
.tl-body{flex:1;}
.tl-title{font-weight:600;font-size:12px;}
.tl-detail{color:var(--muted);font-size:11px;margin-top:1px;}
.sev-critical{color:var(--accent5);}
.sev-high{color:var(--accent4);}
.sev-medium{color:var(--accent);}
.sev-low{color:var(--accent3);}
.channel-bars{display:flex;gap:6px;align-items:flex-end;height:120px;margin-top:8px;}
.ch-bar-wrap{flex:1;display:flex;flex-direction:column;align-items:center;gap:3px;}
.ch-bar{width:100%;border-radius:3px 3px 0 0;background:var(--accent);min-height:2px;transition:height .4s;}
.ch-num{font-size:9px;color:var(--muted);}
.top-aps{margin-top:8px;}
.ap-row{display:flex;align-items:center;gap:8px;padding:5px 0;border-bottom:1px solid var(--border);}
.ap-bar-bg{flex:1;height:6px;background:var(--surface2);border-radius:3px;overflow:hidden;}
.ap-bar-fill{height:100%;border-radius:3px;background:var(--accent2);}
.ap-label{font-size:11px;color:var(--text);width:120px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;}
.ap-val{font-size:11px;color:var(--muted);width:40px;text-align:right;}
.two-col{grid-column:1/-1;display:grid;grid-template-columns:1fr 1fr;gap:12px;}
.proto-pie-wrap{display:flex;gap:16px;align-items:center;margin-top:8px;}
.pie-legend{display:flex;flex-direction:column;gap:6px;}
.pie-item{display:flex;align-items:center;gap:6px;font-size:11px;}
.pie-dot{width:10px;height:10px;border-radius:50%;}
.rssi-hist{display:flex;gap:2px;align-items:flex-end;height:80px;margin-top:8px;}
.rssi-bar{flex:1;border-radius:2px 2px 0 0;background:var(--accent3);min-height:2px;transition:height .4s;}
.controls{display:flex;gap:8px;align-items:center;}
.btn{background:var(--surface2);border:1px solid var(--border);color:var(--text);padding:5px 12px;border-radius:6px;cursor:pointer;font-size:12px;transition:all .2s;}
.btn:hover{background:var(--border);color:var(--accent);}
.btn.active{background:var(--accent);color:#000;border-color:var(--accent);}
.refresh-indicator{width:8px;height:8px;border-radius:50%;background:var(--accent3);display:inline-block;animation:pulse 2s infinite;}
@keyframes pulse{0%,100%{opacity:1;}50%{opacity:.3;}}
.table-wrap{overflow-x:auto;margin-top:8px;}
table{width:100%;border-collapse:collapse;font-size:11px;}
th{color:var(--muted);text-align:left;padding:5px 8px;border-bottom:1px solid var(--border);font-weight:600;}
td{padding:5px 8px;border-bottom:1px solid var(--border);}
tr:hover td{background:var(--surface2);}
.tag{padding:2px 6px;border-radius:4px;font-size:10px;font-weight:600;}
.tag-open{background:#ef44441a;color:var(--accent5);}
.tag-wpa2{background:#3b82f61a;color:#3b82f6;}
.tag-wpa3{background:#10b9811a;color:var(--accent3);}
.tag-wep{background:#f59e0b1a;color:var(--accent4);}
.metric-row{display:flex;gap:8px;margin-top:8px;flex-wrap:wrap;}
.metric-box{background:var(--surface2);border:1px solid var(--border);border-radius:6px;padding:8px 12px;flex:1;min-width:100px;}
.metric-box .label{font-size:10px;color:var(--muted);}
.metric-box .val{font-size:18px;font-weight:700;color:var(--accent);}
</style>
</head>
<body>
<header>
  <h1>⚡ Phantom32</h1>
  <span class="badge">ANALYTICS</span>
  <div class="controls">
    <span class="refresh-indicator"></span>
    <span id="last-update" style="color:var(--muted);font-size:11px;">updating...</span>
    <button class="btn active" id="btn-live" onclick="toggleLive()">Live</button>
    <button class="btn" onclick="exportData()">Export</button>
  </div>
  <nav>
    <a href="/">Dashboard</a>
    <a href="/topology">Topology</a>
    <a href="/spectrum">Spectrum</a>
    <a href="/analytics" class="active">Analytics</a>
    <a href="/forensics">Forensics</a>
    <a href="/ble-iot">BLE/IoT</a>
    <a href="/cli">CLI</a>
    <a href="/settings">Settings</a>
  </nav>
</header>

<div class="container">

  <!-- Stat cards -->
  <div class="stat-card">
    <div class="stat-label">Packets / sec</div>
    <div class="stat-value" id="pps" style="color:var(--accent)">—</div>
    <div class="stat-delta"><span class="delta-up" id="pps-delta">—</span></div>
  </div>
  <div class="stat-card">
    <div class="stat-label">APs Visible</div>
    <div class="stat-value" id="ap-count" style="color:var(--accent2)">—</div>
    <div class="stat-delta"><span id="ap-delta" class="delta-up">—</span></div>
  </div>
  <div class="stat-card">
    <div class="stat-label">Stations</div>
    <div class="stat-value" id="sta-count" style="color:var(--accent3)">—</div>
    <div class="stat-delta"><span id="sta-delta" class="delta-up">—</span></div>
  </div>
  <div class="stat-card">
    <div class="stat-label">Active Threats</div>
    <div class="stat-value" id="threat-count" style="color:var(--accent5)">—</div>
    <div class="stat-delta"><span id="threat-delta" class="delta-down">—</span></div>
  </div>

  <!-- Packet rate chart -->
  <div class="chart-row">
    <div class="chart-card">
      <h3>Packet Rate <span id="rate-info">—</span></h3>
      <canvas id="rateCanvas"></canvas>
    </div>
    <div class="chart-card">
      <h3>Protocol Distribution <span>last 60s</span></h3>
      <div class="proto-pie-wrap">
        <canvas id="pieCanvas" style="width:120px !important;height:120px !important;--chart-h:120px;"></canvas>
        <div class="pie-legend" id="pie-legend"></div>
      </div>
    </div>
  </div>

  <!-- RSSI history -->
  <div class="chart-card full-row">
    <h3>RSSI Distribution (top APs) <span id="rssi-info">—</span></h3>
    <canvas id="rssiCanvas"></canvas>
  </div>

  <!-- Channel utilization heatmap -->
  <div class="chart-card full-row">
    <h3>Channel Utilization (2.4 GHz) <span>color = AP density</span></h3>
    <div class="heatmap-grid" id="hmGrid"></div>
    <div class="hm-label"><span>CH 1</span><span>CH 14</span></div>
    <div style="margin-top:12px;">
      <h3>5 GHz Channels</h3>
      <div class="channel-bars" id="ch5Bars"></div>
    </div>
  </div>

  <!-- Two-column section -->
  <div class="two-col">
    <!-- Top APs by frame count -->
    <div class="chart-card">
      <h3>Top APs by Traffic</h3>
      <div class="top-aps" id="topAps"></div>
    </div>

    <!-- Threat timeline -->
    <div class="chart-card">
      <h3>Threat Timeline</h3>
      <div class="timeline-wrap" id="timeline"></div>
    </div>
  </div>

  <!-- Deauth flood timeline -->
  <div class="chart-card full-row">
    <h3>Deauth Storm Detection <span id="ddt-info">—</span></h3>
    <canvas id="deauthCanvas"></canvas>
  </div>

  <!-- Per-channel metrics table -->
  <div class="chart-card full-row">
    <h3>Per-Channel Statistics</h3>
    <div class="table-wrap">
      <table id="chTable">
        <thead>
          <tr><th>Ch</th><th>APs</th><th>Stations</th><th>Pkts/s</th><th>Avg RSSI</th><th>Top SSID</th><th>Security</th></tr>
        </thead>
        <tbody id="chTbody"></tbody>
      </table>
    </div>
  </div>

  <!-- Encryption breakdown -->
  <div class="chart-card full-row">
    <h3>Security Posture</h3>
    <div class="metric-row" id="secMetrics"></div>
  </div>

</div>

<script>
// ─── State ──────────────────────────────────────────────────────────────────
let liveMode = true;
let timer = null;
const HISTORY = 60;
let rateHist = new Array(HISTORY).fill(0);
let deauthHist = new Array(HISTORY).fill(0);
let rssiHistory = {};
let prevStats = {};

// ─── Canvas helpers ──────────────────────────────────────────────────────────
function setupCanvas(id) {
  const c = document.getElementById(id);
  if (!c) return null;
  const dpr = window.devicePixelRatio || 1;
  const rect = c.parentElement.getBoundingClientRect();
  c.width = rect.width * dpr;
  c.height = parseInt(getComputedStyle(c).getPropertyValue('--chart-h') || '180') * dpr;
  c.style.width = '100%';
  c.style.height = (parseInt(getComputedStyle(c).getPropertyValue('--chart-h') || '180')) + 'px';
  const ctx = c.getContext('2d');
  ctx.scale(dpr, dpr);
  return { c, ctx, w: c.width / dpr, h: c.height / dpr };
}

function drawLine(ctx, data, w, h, color, fill, min, max) {
  if (!data.length) return;
  const rng = max - min || 1;
  ctx.beginPath();
  data.forEach((v, i) => {
    const x = (i / (data.length - 1)) * w;
    const y = h - ((v - min) / rng) * (h - 20) - 5;
    i === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y);
  });
  if (fill) {
    const lx = ((data.length - 1) / (data.length - 1)) * w;
    ctx.lineTo(lx, h);
    ctx.lineTo(0, h);
    ctx.closePath();
    ctx.fillStyle = fill;
    ctx.fill();
    ctx.beginPath();
    data.forEach((v, i) => {
      const x = (i / (data.length - 1)) * w;
      const y = h - ((v - min) / rng) * (h - 20) - 5;
      i === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y);
    });
  }
  ctx.strokeStyle = color;
  ctx.lineWidth = 2;
  ctx.stroke();
}

function drawGrid(ctx, w, h, rows = 4) {
  ctx.strokeStyle = '#2d374820';
  ctx.lineWidth = 1;
  for (let i = 0; i <= rows; i++) {
    const y = h - (i / rows) * (h - 20) - 5;
    ctx.beginPath();
    ctx.moveTo(0, y);
    ctx.lineTo(w, y);
    ctx.stroke();
  }
}

function drawLabel(ctx, text, x, y, color = '#64748b', size = 10) {
  ctx.fillStyle = color;
  ctx.font = `${size}px system-ui`;
  ctx.fillText(text, x, y);
}

// ─── Rate chart ──────────────────────────────────────────────────────────────
function drawRateChart() {
  const cv = setupCanvas('rateCanvas');
  if (!cv) return;
  const { ctx, w, h } = cv;
  ctx.clearRect(0, 0, w, h);
  drawGrid(ctx, w, h);
  const mx = Math.max(...rateHist, 1);
  drawLine(ctx, rateHist, w, h, '#00d4ff', '#00d4ff15', 0, mx * 1.1);
  drawLine(ctx, deauthHist, w, h, '#ef4444', null, 0, mx * 1.1);
  // y-axis labels
  drawLabel(ctx, mx.toFixed(0), 2, 14);
  drawLabel(ctx, '0', 2, h - 2);
  // x-axis time
  drawLabel(ctx, '-60s', 2, h - 2);
  drawLabel(ctx, 'now', w - 25, h - 2);
  // legend
  ctx.fillStyle = '#00d4ff';
  ctx.fillRect(w - 120, 10, 10, 2);
  drawLabel(ctx, 'Total', w - 106, 14, '#e2e8f0', 10);
  ctx.fillStyle = '#ef4444';
  ctx.fillRect(w - 70, 10, 10, 2);
  drawLabel(ctx, 'Deauth', w - 56, 14, '#e2e8f0', 10);
}

// ─── RSSI chart ──────────────────────────────────────────────────────────────
function drawRssiChart(aps) {
  const cv = setupCanvas('rssiCanvas');
  if (!cv) return;
  const { ctx, w, h } = cv;
  ctx.clearRect(0, 0, w, h);
  drawGrid(ctx, w, h);
  const colors = ['#00d4ff', '#7c3aed', '#10b981', '#f59e0b', '#ef4444'];
  const sliced = aps.slice(0, 5);
  sliced.forEach((ap, idx) => {
    const hist = rssiHistory[ap.mac] || new Array(HISTORY).fill(ap.rssi || -90);
    drawLine(ctx, hist, w, h, colors[idx], null, -100, -20);
    drawLabel(ctx, (ap.ssid || ap.mac).substring(0, 12), w - 130, 14 + idx * 14, colors[idx], 10);
  });
  drawLabel(ctx, '-20 dBm', 2, 14);
  drawLabel(ctx, '-100 dBm', 2, h - 2);
}

// ─── Pie chart ───────────────────────────────────────────────────────────────
function drawPie(data) {
  const c = document.getElementById('pieCanvas');
  if (!c) return;
  const dpr = window.devicePixelRatio || 1;
  c.width = 120 * dpr;
  c.height = 120 * dpr;
  const ctx = c.getContext('2d');
  ctx.scale(dpr, dpr);
  const total = data.reduce((a, b) => a + b.v, 0) || 1;
  let start = -Math.PI / 2;
  const cx = 60, cy = 60, r = 50;
  data.forEach(d => {
    const angle = (d.v / total) * Math.PI * 2;
    ctx.beginPath();
    ctx.moveTo(cx, cy);
    ctx.arc(cx, cy, r, start, start + angle);
    ctx.closePath();
    ctx.fillStyle = d.c;
    ctx.fill();
    start += angle;
  });
  ctx.beginPath();
  ctx.arc(cx, cy, 28, 0, Math.PI * 2);
  ctx.fillStyle = '#111827';
  ctx.fill();
  ctx.fillStyle = '#e2e8f0';
  ctx.font = 'bold 11px system-ui';
  ctx.textAlign = 'center';
  ctx.fillText(total + ' frm', cx, cy + 4);

  const leg = document.getElementById('pie-legend');
  if (leg) {
    leg.innerHTML = data.map(d =>
      `<div class="pie-item"><div class="pie-dot" style="background:${d.c}"></div>${d.n}: ${d.v}</div>`
    ).join('');
  }
}

// ─── Deauth chart ────────────────────────────────────────────────────────────
function drawDeauthChart() {
  const cv = setupCanvas('deauthCanvas');
  if (!cv) return;
  const { ctx, w, h } = cv;
  ctx.clearRect(0, 0, w, h);
  drawGrid(ctx, w, h);
  const mx = Math.max(...deauthHist, 1);
  // Fill areas
  drawLine(ctx, deauthHist, w, h, '#ef4444', '#ef444420', 0, mx * 1.2);
  // Threshold line
  const thr = 20;
  const thrY = h - (thr / (mx * 1.2)) * (h - 20) - 5;
  ctx.setLineDash([4, 4]);
  ctx.strokeStyle = '#f59e0b';
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.moveTo(0, thrY);
  ctx.lineTo(w, thrY);
  ctx.stroke();
  ctx.setLineDash([]);
  drawLabel(ctx, 'Flood threshold (20/5s)', 4, thrY - 3, '#f59e0b', 9);
  drawLabel(ctx, mx.toFixed(0), 2, 14);
}

// ─── Heatmap ─────────────────────────────────────────────────────────────────
function buildHeatmap(chData) {
  const grid = document.getElementById('hmGrid');
  if (!grid) return;
  grid.innerHTML = '';
  const channels = Array.from({length: 14}, (_, i) => i + 1);
  channels.forEach(ch => {
    const v = chData[ch] || 0;
    const intensity = Math.min(v / 10, 1);
    const cell = document.createElement('div');
    cell.className = 'hm-cell';
    cell.setAttribute('data-tip', `CH${ch}: ${v} APs`);
    cell.style.background = `rgba(${Math.round(0 + intensity * 124)},${Math.round(212 - intensity * 150)},${Math.round(255 - intensity * 200)},${0.3 + intensity * 0.7})`;
    cell.style.height = '32px';
    grid.appendChild(cell);
  });
}

function build5GBars(chData5) {
  const wrap = document.getElementById('ch5Bars');
  if (!wrap) return;
  wrap.innerHTML = '';
  const ch5 = [36,40,44,48,52,56,60,64,100,104,108,112,116,120,124,128,132,136,140,149,153,157,161,165];
  const mx = Math.max(...ch5.map(c => chData5[c] || 0), 1);
  ch5.forEach(ch => {
    const v = chData5[ch] || 0;
    const pct = (v / mx) * 100;
    const w2 = document.createElement('div');
    w2.className = 'ch-bar-wrap';
    w2.innerHTML = `<div class="ch-bar" style="height:${Math.max(pct,2)}%;background:var(--accent2);" title="CH${ch}: ${v} APs"></div><div class="ch-num">${ch}</div>`;
    wrap.appendChild(w2);
  });
}

// ─── Top APs ─────────────────────────────────────────────────────────────────
function buildTopAps(aps) {
  const wrap = document.getElementById('topAps');
  if (!wrap) return;
  const mx = aps[0]?.frames || 1;
  wrap.innerHTML = aps.slice(0, 8).map(ap => `
    <div class="ap-row">
      <div class="ap-label">${ap.ssid || ap.mac}</div>
      <div class="ap-bar-bg"><div class="ap-bar-fill" style="width:${((ap.frames||0)/mx*100).toFixed(1)}%"></div></div>
      <div class="ap-val">${ap.frames || 0}</div>
    </div>
  `).join('');
}

// ─── Threat timeline ─────────────────────────────────────────────────────────
function buildTimeline(events) {
  const wrap = document.getElementById('timeline');
  if (!wrap) return;
  const icons = {critical:'🔴', high:'🟠', medium:'🟡', low:'🟢'};
  wrap.innerHTML = events.slice(0, 20).map(e => {
    const t = new Date(e.ts * 1000);
    const hh = t.getHours().toString().padStart(2,'0');
    const mm = t.getMinutes().toString().padStart(2,'0');
    const ss = t.getSeconds().toString().padStart(2,'0');
    return `
    <div class="timeline-item">
      <div class="tl-time">${hh}:${mm}:${ss}</div>
      <div class="tl-icon">${icons[e.severity] || '⚪'}</div>
      <div class="tl-body">
        <div class="tl-title sev-${e.severity}">${e.type}</div>
        <div class="tl-detail">${e.detail || ''}</div>
      </div>
    </div>`;
  }).join('');
}

// ─── Channel table ───────────────────────────────────────────────────────────
function buildChTable(rows) {
  const tbody = document.getElementById('chTbody');
  if (!tbody) return;
  const secTag = s => {
    if (!s) return '';
    if (s.includes('WPA3')) return '<span class="tag tag-wpa3">WPA3</span>';
    if (s.includes('WPA2')) return '<span class="tag tag-wpa2">WPA2</span>';
    if (s.includes('WEP')) return '<span class="tag tag-wep">WEP</span>';
    return '<span class="tag tag-open">Open</span>';
  };
  tbody.innerHTML = rows.map(r => `
    <tr>
      <td>${r.ch}</td>
      <td>${r.aps || 0}</td>
      <td>${r.stas || 0}</td>
      <td>${r.pps || 0}</td>
      <td>${r.rssi || '—'} dBm</td>
      <td>${(r.top_ssid || '').substring(0,16) || '—'}</td>
      <td>${secTag(r.security)}</td>
    </tr>`
  ).join('');
}

// ─── Security metrics ────────────────────────────────────────────────────────
function buildSecMetrics(data) {
  const wrap = document.getElementById('secMetrics');
  if (!wrap) return;
  const metrics = [
    { label: 'Open APs', val: data.open || 0, color: 'var(--accent5)' },
    { label: 'WEP (broken)', val: data.wep || 0, color: 'var(--accent4)' },
    { label: 'WPA2 APs', val: data.wpa2 || 0, color: 'var(--accent)' },
    { label: 'WPA3 APs', val: data.wpa3 || 0, color: 'var(--accent3)' },
    { label: 'Hidden SSIDs', val: data.hidden || 0, color: 'var(--accent2)' },
    { label: 'WPS Enabled', val: data.wps || 0, color: 'var(--accent4)' },
    { label: 'High Vuln Score', val: data.vuln_high || 0, color: 'var(--accent5)' },
    { label: 'Rogue APs', val: data.rogue || 0, color: 'var(--accent5)' },
  ];
  wrap.innerHTML = metrics.map(m => `
    <div class="metric-box">
      <div class="label">${m.label}</div>
      <div class="val" style="color:${m.color}">${m.val}</div>
    </div>
  `).join('');
}

// ─── Main data fetch ─────────────────────────────────────────────────────────
async function fetchAnalytics() {
  try {
    const [statsR, apsR, ddtR, threatR] = await Promise.all([
      fetch('/api/pkt/stats').then(r => r.json()).catch(() => ({})),
      fetch('/api/scan/results').then(r => r.json()).catch(() => ({aps:[]})),
      fetch('/api/ddt/status').then(r => r.json()).catch(() => ({})),
      fetch('/api/ti/events').then(r => r.json()).catch(() => ({events:[]})),
    ]);

    const pps = statsR.pps || statsR.packets_per_sec || 0;
    const deauthPps = ddtR.deauths_per_sec || 0;

    // Update history
    rateHist.push(pps); if (rateHist.length > HISTORY) rateHist.shift();
    deauthHist.push(deauthPps); if (deauthHist.length > HISTORY) deauthHist.shift();

    // Update stat cards
    const ppsDelta = pps - (prevStats.pps || 0);
    document.getElementById('pps').textContent = pps;
    document.getElementById('pps-delta').textContent = (ppsDelta >= 0 ? '▲ ' : '▼ ') + Math.abs(ppsDelta).toFixed(0) + ' vs last';
    document.getElementById('pps-delta').className = ppsDelta >= 0 ? 'delta-up' : 'delta-down';

    const aps = apsR.aps || [];
    document.getElementById('ap-count').textContent = aps.length;
    const stas = apsR.stations || [];
    document.getElementById('sta-count').textContent = stas.length;
    const threats = (threatR.events || []).filter(e => e.severity === 'critical' || e.severity === 'high');
    document.getElementById('threat-count').textContent = threats.length;
    document.getElementById('threat-delta').textContent = threats.length > 0 ? '▲ ACTIVE' : '✓ Clear';

    // Rate info
    document.getElementById('rate-info').textContent = `avg: ${(rateHist.reduce((a,b)=>a+b,0)/rateHist.length).toFixed(1)} p/s`;

    // Deauth info
    const totalDeauths = ddtR.total_deauths || 0;
    document.getElementById('ddt-info').textContent = `total: ${totalDeauths} | alerts: ${ddtR.alert_count || 0}`;

    // RSSI history update
    aps.forEach(ap => {
      if (!rssiHistory[ap.mac]) rssiHistory[ap.mac] = new Array(HISTORY).fill(ap.rssi || -90);
      rssiHistory[ap.mac].push(ap.rssi || -90);
      if (rssiHistory[ap.mac].length > HISTORY) rssiHistory[ap.mac].shift();
    });
    document.getElementById('rssi-info').textContent = `${aps.length} APs tracked`;

    // Protocol distribution
    const protos = statsR.proto_counts || {};
    const pieData = [
      { n: 'Beacon', v: protos.beacon || 0, c: '#00d4ff' },
      { n: 'Probe', v: protos.probe || 0, c: '#7c3aed' },
      { n: 'Data', v: protos.data || 0, c: '#10b981' },
      { n: 'Mgmt', v: protos.mgmt || 0, c: '#f59e0b' },
      { n: 'Ctrl', v: protos.ctrl || 0, c: '#ef4444' },
    ];
    drawPie(pieData);

    // Channel data
    const chMap = {};
    const ch5Map = {};
    aps.forEach(ap => {
      const ch = ap.channel || 1;
      if (ch <= 14) chMap[ch] = (chMap[ch] || 0) + 1;
      else ch5Map[ch] = (ch5Map[ch] || 0) + 1;
    });
    buildHeatmap(chMap);
    build5GBars(ch5Map);

    // Top APs by frames
    const sorted = [...aps].sort((a, b) => (b.frames || 0) - (a.frames || 0));
    buildTopAps(sorted);

    // Threat timeline
    buildTimeline(threatR.events || []);

    // Channel table rows
    const chRows = Object.keys({...chMap, ...ch5Map}).map(ch => {
      const chAps = aps.filter(a => a.channel == ch);
      const avgRssi = chAps.length ? Math.round(chAps.reduce((a, b) => a + (b.rssi || -90), 0) / chAps.length) : null;
      const topAp = chAps.sort((a, b) => (b.frames || 0) - (a.frames || 0))[0];
      return {
        ch: parseInt(ch),
        aps: chAps.length,
        stas: 0,
        pps: 0,
        rssi: avgRssi,
        top_ssid: topAp?.ssid || '',
        security: topAp?.security || '',
      };
    }).sort((a, b) => a.ch - b.ch);
    buildChTable(chRows);

    // Security metrics
    const secData = {
      open: aps.filter(a => !a.security || a.security === 'Open').length,
      wep: aps.filter(a => a.security && a.security.includes('WEP')).length,
      wpa2: aps.filter(a => a.security && a.security.includes('WPA2')).length,
      wpa3: aps.filter(a => a.security && a.security.includes('WPA3')).length,
      hidden: aps.filter(a => !a.ssid || a.ssid === '').length,
      wps: 0,
      vuln_high: 0,
      rogue: 0,
    };
    buildSecMetrics(secData);

    // Draw charts
    drawRateChart();
    drawRssiChart(sorted.slice(0, 5));
    drawDeauthChart();

    prevStats = { pps };
    document.getElementById('last-update').textContent = new Date().toLocaleTimeString();
  } catch (e) {
    console.error('Analytics fetch error:', e);
  }
}

function toggleLive() {
  liveMode = !liveMode;
  const btn = document.getElementById('btn-live');
  btn.className = liveMode ? 'btn active' : 'btn';
  if (liveMode) startPolling(); else clearInterval(timer);
}

function startPolling() {
  clearInterval(timer);
  fetchAnalytics();
  timer = setInterval(fetchAnalytics, 2000);
}

function exportData() {
  const data = { rateHist, deauthHist, timestamp: Date.now() };
  const blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = `phantom32-analytics-${Date.now()}.json`;
  a.click();
}

window.addEventListener('resize', () => {
  drawRateChart();
  drawDeauthChart();
});

startPolling();
</script>
</body>
</html>
)rawhtml";

// ─── Route registration ───────────────────────────────────────────────────────
inline void wua_routes() {
  server.on("/analytics", HTTP_GET, []() {
    server.sendHeader("Content-Encoding", "identity");
    server.send_P(200, "text/html", WUA_PAGE);
  });
}

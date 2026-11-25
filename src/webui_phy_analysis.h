#pragma once
// webui_phy_analysis.h — PHY Analysis UI.
// Route: /phy-analysis. Prefix: wupha_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUPHA_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>PHY Analysis — Phantom32</title>
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
.container{max-width:1200px;margin:0 auto;padding:20px 16px;}
h1{font-size:22px;font-weight:700;margin-bottom:4px;}
.subtitle{color:var(--muted);font-size:13px;margin-bottom:20px;}
.hero{display:grid;grid-template-columns:repeat(5,1fr);gap:10px;margin-bottom:16px;}
.stat{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:13px;}
.stat label{font-size:10px;color:var(--muted);text-transform:uppercase;}
.stat .val{font-size:22px;font-weight:800;color:var(--accent);margin-top:3px;}
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:18px;margin-bottom:14px;}
.section h3{font-size:12px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:14px;}
.two-col{display:grid;grid-template-columns:1fr 1fr;gap:14px;margin-bottom:14px;}
table{width:100%;border-collapse:collapse;font-size:10px;}
th{text-align:left;padding:8px 10px;border-bottom:2px solid var(--border);color:var(--muted);font-weight:600;font-size:9px;text-transform:uppercase;}
td{padding:7px 10px;border-bottom:1px solid var(--border);}
tr:hover td{background:var(--bg3);}
.snr-pill{padding:2px 6px;border-radius:3px;font-size:9px;font-weight:700;}
.s-Excellent{background:rgba(34,197,94,.1);color:var(--success);}
.s-Good{background:rgba(0,212,255,.1);color:var(--accent);}
.s-Fair{background:rgba(245,158,11,.1);color:var(--warn);}
.s-Poor{background:rgba(239,68,68,.1);color:var(--danger);}
.s-Critical{background:rgba(239,68,68,.2);color:#ff6666;}
.mini-fps{display:flex;align-items:flex-end;gap:1px;height:20px;}
.fps-bar{width:3px;min-height:1px;border-radius:1px 1px 0 0;background:var(--accent);}
.rssi-hist{display:flex;align-items:flex-end;gap:1px;height:28px;padding:0 2px;}
.rh-bar{flex:1;min-height:1px;border-radius:1px 1px 0 0;}
.pct-bar{display:inline-block;height:5px;border-radius:2px;vertical-align:middle;}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;margin-top:12px;}
.al-item{padding:7px 12px;border-radius:5px;border-left:2px solid var(--warn);background:rgba(245,158,11,.04);margin-bottom:5px;font-size:10px;}
.al-ts{color:var(--muted);font-size:9px;margin-right:6px;}
@media(max-width:800px){.hero{grid-template-columns:repeat(3,1fr)};.two-col{grid-template-columns:1fr}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/phy-analysis" class="active">PHY Analysis</a>
    <a href="/channel-analyzer">Channels</a>
    <a href="/signal-heatmap">Signal Map</a>
    <a href="/rate-tracker">Rates</a>
  </nav>
</div>
<div class="container">
  <h1>PHY Layer Analysis</h1>
  <p class="subtitle">Advanced 802.11 PHY statistics: per-channel RSSI distributions, SNR estimation, retry rates, FCS errors, frame type breakdowns, and PHY anomaly detection (hidden-node, near-far, high-retry, PHY errors).</p>

  <div class="hero">
    <div class="stat"><label>Total Frames</label><div class="val" id="h-total">0</div></div>
    <div class="stat"><label>Total Retries</label><div class="val" id="h-ret" style="color:var(--warn)">0</div></div>
    <div class="stat"><label>FCS Errors</label><div class="val" id="h-err" style="color:var(--danger)">0</div></div>
    <div class="stat"><label>Anomalies</label><div class="val" id="h-anom" style="color:var(--warn)">0</div></div>
    <div class="stat"><label>Cleanest CH</label><div class="val" id="h-clean" style="color:var(--success)">—</div></div>
  </div>

  <div class="section">
    <h3>Per-Channel PHY Statistics</h3>
    <div style="overflow-x:auto">
    <table>
      <thead><tr>
        <th>CH</th><th>Frames</th><th>Retry%</th><th>FCS Err</th>
        <th>RSSI Range</th><th>SNR</th><th>Frame Types</th>
        <th>RSSI Dist.</th><th>FPS Hist.</th>
      </tr></thead>
      <tbody id="ch-body"></tbody>
    </table>
    </div>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>

  <div class="two-col">
    <div class="section">
      <h3>PHY Anomalies</h3>
      <div id="anomaly-feed">
        <div style="color:var(--muted);font-size:11px">No PHY anomalies detected.</div>
      </div>
    </div>

    <div class="section">
      <h3>Global Frame Distribution</h3>
      <canvas id="dist-canvas" width="300" height="150" style="width:100%;border-radius:5px;background:var(--bg3)"></canvas>
    </div>
  </div>
</div>

<script>
function fmt(n) { return Number(n).toLocaleString(); }

function retryColor(r) {
  if (r < 5)  return 'var(--success)';
  if (r < 15) return 'var(--warn)';
  return 'var(--danger)';
}

function snrColor(snr) {
  if (snr >= 25) return 'var(--success)';
  if (snr >= 15) return 'var(--accent)';
  if (snr >= 10) return 'var(--warn)';
  return 'var(--danger)';
}

function rssiColor(rssi) {
  if (rssi >= -60) return 'var(--success)';
  if (rssi >= -70) return 'var(--warn)';
  return 'var(--danger)';
}

function miniBarChart(data, color) {
  const max = Math.max(...data, 1);
  return `<div class="mini-fps">` +
    data.map(v => `<div class="fps-bar" style="height:${Math.max(1,(v/max)*20)}px;background:${color}"></div>`).join('') +
    `</div>`;
}

function rssiHistBars(hist) {
  if (!hist || !hist.length) return '';
  const max = Math.max(...hist, 1);
  const colors = ['#ef4444','#ef4444','#f97316','#f97316','#f59e0b','#f59e0b',
                  '#eab308','#eab308','#84cc16','#22c55e','#22c55e','#22c55e',
                  '#00d4ff','#00d4ff','#818cf8','#818cf8'];
  return `<div class="rssi-hist">` +
    hist.map((v,i) => `<div class="rh-bar" title="${PHA_RSSI_BOUNDS[i]}dBm: ${v}" style="height:${Math.max(1,(v/max)*28)}px;background:${colors[i]||'var(--accent)'}"></div>`).join('') +
    `</div>`;
}

const PHA_RSSI_BOUNDS = [-100,-95,-90,-85,-80,-75,-70,-65,-60,-55,-50,-45,-40,-35,-30,-25,-20];

function frameTypePct(mgmt, data, ctrl) {
  const total = mgmt + data + ctrl || 1;
  const pm = mgmt/total*100, pd = data/total*100, pc = ctrl/total*100;
  return `<div style="display:flex;height:5px;border-radius:2px;overflow:hidden;min-width:50px;width:70px">
    <div style="width:${pm}%;background:var(--accent)" title="Mgmt ${pm.toFixed(0)}%"></div>
    <div style="width:${pd}%;background:var(--success)" title="Data ${pd.toFixed(0)}%"></div>
    <div style="width:${pc}%;background:var(--warn)" title="Ctrl ${pc.toFixed(0)}%"></div>
  </div>
  <div style="font-size:8px;color:var(--muted);margin-top:2px">M:${pm.toFixed(0)}% D:${pd.toFixed(0)}% C:${pc.toFixed(0)}%</div>`;
}

function drawDist(chs) {
  const canvas = document.getElementById('dist-canvas');
  const ctx = canvas.getContext('2d');
  const W = canvas.width, H = canvas.height;
  ctx.clearRect(0, 0, W, H);
  if (!chs.length) return;

  const totalM = chs.reduce((s,c)=>s+c.mgmt, 0);
  const totalD = chs.reduce((s,c)=>s+c.data, 0);
  const totalC = chs.reduce((s,c)=>s+c.ctrl, 0);
  const grand = totalM + totalD + totalC || 1;

  const slices = [
    {label:'Mgmt', val:totalM/grand, color:'#00d4ff'},
    {label:'Data', val:totalD/grand, color:'#22c55e'},
    {label:'Ctrl', val:totalC/grand, color:'#f59e0b'},
  ];

  let start = -Math.PI/2;
  const cx = W/2, cy = H/2 - 10, r = Math.min(cx, cy) - 20;
  slices.forEach(sl => {
    const angle = sl.val * 2 * Math.PI;
    ctx.beginPath();
    ctx.moveTo(cx, cy);
    ctx.arc(cx, cy, r, start, start + angle);
    ctx.closePath();
    ctx.fillStyle = sl.color;
    ctx.fill();
    start += angle;
  });

  // Legend
  ctx.font = '9px Inter,sans-serif';
  slices.forEach((sl, i) => {
    const lx = 10 + i * 90, ly = H - 8;
    ctx.fillStyle = sl.color;
    ctx.fillRect(lx, ly - 7, 7, 7);
    ctx.fillStyle = '#e2e8f0';
    ctx.fillText(`${sl.label} ${(sl.val*100).toFixed(1)}%`, lx + 10, ly);
  });
}

async function refresh() {
  const r = await fetch('/api/pha/all').then(x=>x.json()).catch(()=>({}));
  const chs = r.channels || [];
  const events = r.events || [];

  document.getElementById('h-total').textContent = fmt(r.total_frames || 0);
  document.getElementById('h-ret').textContent   = fmt(r.total_retries || 0);
  document.getElementById('h-err').textContent   = fmt(r.total_errors || 0);
  document.getElementById('h-anom').textContent  = fmt(r.anomalies || 0);
  document.getElementById('h-clean').textContent = r.cleanest_ch ? 'CH'+r.cleanest_ch : '—';

  document.getElementById('ch-body').innerHTML = chs.length
    ? chs.map(c => `<tr>
        <td><b>${c.ch}</b></td>
        <td>${fmt(c.frames)}</td>
        <td style="color:${retryColor(c.retry_rate)};font-weight:700">${c.retry_rate}%</td>
        <td style="color:${c.fcs_err>0?'var(--danger)':'var(--muted)'}">${fmt(c.fcs_err)}</td>
        <td style="color:var(--muted)"><span style="color:${rssiColor(c.rssi_max)}">${c.rssi_max}</span> / <span style="color:${rssiColor(c.rssi_min)}">${c.rssi_min}</span> dBm</td>
        <td><span style="color:${snrColor(c.snr)};font-weight:700">${c.snr}dB</span> <span class="snr-pill s-${c.snr_quality}">${c.snr_quality}</span></td>
        <td>${frameTypePct(c.mgmt,c.data,c.ctrl)}</td>
        <td>${rssiHistBars(c.rssi_hist)}</td>
        <td>${miniBarChart((c.fps_hist||[]).slice(-20), 'var(--accent)')}</td>
      </tr>`).join('')
    : '<tr><td colspan="9" style="text-align:center;color:var(--muted);padding:16px">No PHY data.</td></tr>';

  document.getElementById('anomaly-feed').innerHTML = events.length
    ? events.slice().reverse().map(ev => `<div class="al-item">
        <span class="al-ts">${ev.ts}s</span>
        <b>CH${ev.ch}</b> — ${ev.type}: ${ev.detail}
        <span style="color:var(--muted)"> (${ev.rssi}dBm)</span>
      </div>`).join('')
    : '<div style="color:var(--muted);font-size:11px">No anomalies detected.</div>';

  drawDist(chs);
}

async function clearData() {
  await fetch('/api/pha/clear', {method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 2000);
</script>
</body>
</html>
)rawhtml";

inline void wupha_routes() {
  server.on("/phy-analysis", HTTP_GET, []() { server.send_P(200, "text/html", WUPHA_PAGE); });
}

#pragma once
// webui_signal_heatmap.h — Per-channel RSSI signal heatmap UI.
// Route: /signal-heatmap. Prefix: wushm_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUSHM_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Signal Heatmap — Phantom32</title>
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
.hero{display:grid;grid-template-columns:repeat(4,1fr);gap:10px;margin-bottom:16px;}
.stat{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:13px;}
.stat label{font-size:10px;color:var(--muted);text-transform:uppercase;}
.stat .val{font-size:22px;font-weight:800;color:var(--accent);margin-top:3px;}
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:18px;margin-bottom:14px;}
.section h3{font-size:12px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:14px;}
.heatmap-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(140px,1fr));gap:10px;}
.ch-card{background:var(--bg3);border:1px solid var(--border);border-radius:7px;padding:12px;position:relative;}
.ch-num{font-size:22px;font-weight:800;color:var(--accent);}
.ch-band{font-size:9px;color:var(--muted);text-transform:uppercase;margin-bottom:6px;}
.ch-rssi{font-size:14px;font-weight:700;}
.ch-fps{font-size:10px;color:var(--muted);}
.ch-aps{font-size:10px;color:var(--text);}
.mini-hist{display:flex;align-items:flex-end;gap:1px;height:28px;margin:8px 0;}
.mini-hist-bar{flex:1;border-radius:1px;min-width:3px;}
.mini-spark{display:flex;align-items:flex-end;gap:1px;height:20px;margin:4px 0;}
.mini-spark-bar{width:3px;border-radius:1px;}
.noise-row{display:flex;align-items:center;gap:5px;font-size:9px;color:var(--muted);margin-top:4px;}
.noise-val{font-weight:700;}

.heatmap-table-wrap{overflow-x:auto;}
table{width:100%;border-collapse:collapse;font-size:10px;}
th{text-align:left;padding:8px 10px;border-bottom:2px solid var(--border);color:var(--muted);font-weight:600;font-size:9px;text-transform:uppercase;}
td{padding:7px 10px;border-bottom:1px solid var(--border);}
tr:hover td{background:var(--bg3);}
.rssi-bar{height:6px;border-radius:3px;display:inline-block;}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;margin-top:12px;}
@media(max-width:700px){.hero{grid-template-columns:1fr 1fr}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/signal-heatmap" class="active">Signal Map</a>
    <a href="/scan-cache">Scan Cache</a>
    <a href="/topology">Topology</a>
  </nav>
</div>
<div class="container">
  <h1>Signal Heatmap</h1>
  <p class="subtitle">Per-channel RSSI distribution and frame density across 2.4 GHz channels. Tracks signal levels, noise floor estimates, AP density, and frame rates over rolling 10-second windows.</p>

  <div class="hero">
    <div class="stat"><label>Total Frames</label><div class="val" id="h-frames">0</div></div>
    <div class="stat"><label>Active Channels</label><div class="val" id="h-chs">0</div></div>
    <div class="stat"><label>Uptime</label><div class="val" id="h-up">0s</div></div>
    <div class="stat"><label>Busiest Ch</label><div class="val" id="h-busy" style="color:var(--warn)">—</div></div>
  </div>

  <div class="section">
    <h3>Channel Heatmap</h3>
    <div class="heatmap-grid" id="ch-grid">
      <div style="color:var(--muted)">No channel data.</div>
    </div>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>

  <div class="section">
    <h3>Channel Details</h3>
    <div class="heatmap-table-wrap">
    <table>
      <thead><tr>
        <th>Ch</th><th>Frames</th><th>fps</th>
        <th>RSSI Min</th><th>RSSI Mean</th><th>RSSI Max</th>
        <th>Noise Floor</th><th>APs</th><th>Distribution</th>
      </tr></thead>
      <tbody id="ch-table"></tbody>
    </table>
    </div>
  </div>
</div>

<script>
const BUCKET_LABELS = ['<-90','-90/-80','-80/-70','-70/-65','-65/-60','-60/-55','-55/-50','-50/-45','-45/-40','>-40'];

function rssiColor(r) {
  if (r >= -55) return 'var(--success)';
  if (r >= -70) return 'var(--warn)';
  return 'var(--danger)';
}

function bucketColor(i) {
  const h = 120 - (i / 9) * 120;
  return `hsl(${h},70%,50%)`;
}

function sparklineHTML(samples) {
  const vals = samples.map(Number).filter(x => x !== 0);
  if (!vals.length) return '';
  const mn = Math.min(...vals)-5, mx = Math.max(...vals)+5;
  return `<div class="mini-spark">${samples.map(v => {
    const h = Math.max(2, ((Number(v) - mn) / (mx - mn + 1)) * 20);
    return `<div class="mini-spark-bar" style="height:${h}px;background:${rssiColor(Number(v))}"></div>`;
  }).join('')}</div>`;
}

function histHTML(hist) {
  const max = Math.max(...hist.map(Number), 1);
  return `<div class="mini-hist">${hist.map((v,i) => {
    const h = Math.max(2, (Number(v)/max)*28);
    return `<div class="mini-hist-bar" title="${BUCKET_LABELS[i]}: ${v}" style="height:${h}px;background:${bucketColor(i)}"></div>`;
  }).join('')}</div>`;
}

function chCard(c) {
  return `<div class="ch-card">
    <div class="ch-num">CH${c.ch}</div>
    <div class="ch-band">${c.band===0?'2.4 GHz':'5 GHz'}</div>
    <div class="ch-rssi" style="color:${rssiColor(c.rssi_mean)}">${c.rssi_mean} dBm avg</div>
    <div class="ch-fps">${c.fps} fps &nbsp;·&nbsp; ${c.ap_count} APs</div>
    ${histHTML(c.hist)}
    ${sparklineHTML(c.samples)}
    <div class="noise-row">Noise: <span class="noise-val" style="color:var(--muted)">${c.noise_floor} dBm</span></div>
  </div>`;
}

function distBar(hist) {
  const total = hist.reduce((a,b)=>a+Number(b),0) || 1;
  return `<div style="display:flex;height:8px;border-radius:3px;overflow:hidden;min-width:80px;">
    ${hist.map((v,i) => Number(v)?`<div title="${BUCKET_LABELS[i]}: ${v}" style="flex:${Number(v)/total};background:${bucketColor(i)}"></div>`:'').join('')}
  </div>`;
}

async function refresh() {
  const r = await fetch('/api/shm/all').then(x=>x.json()).catch(()=>({}));
  const chs = r.channels || [];
  document.getElementById('h-frames').textContent = (r.total_frames||0).toLocaleString();
  document.getElementById('h-chs').textContent    = chs.length;
  const up = r.uptime||0;
  document.getElementById('h-up').textContent = up>3600?`${Math.floor(up/3600)}h`:up>60?`${Math.floor(up/60)}m`:`${up}s`;

  if (chs.length) {
    const busiest = chs.reduce((a,b) => b.fps > a.fps ? b : a, chs[0]);
    document.getElementById('h-busy').textContent = 'CH'+busiest.ch;
  }

  document.getElementById('ch-grid').innerHTML = chs.length
    ? chs.map(chCard).join('')
    : '<div style="color:var(--muted)">No channel data yet.</div>';

  document.getElementById('ch-table').innerHTML = chs.length
    ? chs.map(c => `<tr>
        <td><b>CH${c.ch}</b></td>
        <td>${(c.frames||0).toLocaleString()}</td>
        <td>${c.fps}</td>
        <td style="color:var(--danger)">${c.rssi_min}</td>
        <td style="color:${rssiColor(c.rssi_mean)};font-weight:700">${c.rssi_mean}</td>
        <td style="color:var(--success)">${c.rssi_max}</td>
        <td style="color:var(--muted)">${c.noise_floor}</td>
        <td>${c.ap_count}</td>
        <td>${distBar(c.hist)}</td>
      </tr>`).join('')
    : '<tr><td colspan="9" style="text-align:center;color:var(--muted);padding:16px">No data.</td></tr>';
}

async function clearData() {
  await fetch('/api/shm/clear', {method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 2000);
</script>
</body>
</html>
)rawhtml";

inline void wushm_routes() {
  server.on("/signal-heatmap", HTTP_GET, []() {
    server.send_P(200, "text/html", WUSHM_PAGE);
  });
}

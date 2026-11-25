#pragma once
// webui_snr.h — SNR tracker web UI.
// Route: /snr. Prefix: wusnr_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUSNR_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>SNR Tracker — Phantom32</title>
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
.hero{display:grid;grid-template-columns:repeat(3,1fr);gap:10px;margin-bottom:16px;}
.stat{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:13px;}
.stat label{font-size:10px;color:var(--muted);text-transform:uppercase;}
.stat .val{font-size:22px;font-weight:800;color:var(--accent);margin-top:3px;}
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:18px;margin-bottom:14px;}
.section h3{font-size:12px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:14px;}
table{width:100%;border-collapse:collapse;font-size:12px;}
th{text-align:left;padding:7px 8px;font-size:10px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;cursor:pointer;}
th:hover{color:var(--text);}
td{padding:7px 8px;border-bottom:1px solid var(--border);}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
.quality-bar{display:flex;align-items:center;gap:5px;}
.q-track{height:7px;width:70px;background:var(--bg3);border-radius:4px;}
.q-fill{height:100%;border-radius:4px;}
.spark-cell canvas{display:block;}
.ch-noise-grid{display:grid;grid-template-columns:repeat(7,1fr);gap:8px;}
.ch-cell{background:var(--bg3);border-radius:5px;padding:8px;text-align:center;}
.ch-cell .ch-num{font-size:11px;color:var(--muted);}
.ch-cell .ch-val{font-size:16px;font-weight:700;margin-top:2px;}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;margin-top:12px;}
@media(max-width:700px){.hero{grid-template-columns:1fr}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/analytics">Analytics</a>
    <a href="/snr" class="active">SNR</a>
    <a href="/noise">RF Noise</a>
    <a href="/heatmap">Heatmap</a>
  </nav>
</div>
<div class="container">
  <h1>SNR Tracker</h1>
  <p class="subtitle">Signal-to-noise ratio, link quality estimate, and channel noise floor tracking.</p>

  <div class="hero">
    <div class="stat"><label>APs Tracked</label><div class="val" id="h-aps">0</div></div>
    <div class="stat"><label>Best SNR</label><div class="val" id="h-best" style="color:var(--success)">—</div></div>
    <div class="stat"><label>Worst SNR</label><div class="val" id="h-worst" style="color:var(--danger)">—</div></div>
  </div>

  <div class="section">
    <h3>Channel Noise Floor (best observed, dBm)</h3>
    <div class="ch-noise-grid" id="ch-noise-grid"></div>
  </div>

  <div class="section">
    <h3>AP Link Quality</h3>
    <table>
      <thead><tr>
        <th onclick="sort('bssid')">BSSID</th>
        <th onclick="sort('ssid')">SSID</th>
        <th>CH</th>
        <th onclick="sort('snr')">SNR (dB)</th>
        <th onclick="sort('quality')">Quality</th>
        <th>Range</th>
        <th>30-sec History</th>
      </tr></thead>
      <tbody id="ap-tbody">
        <tr><td colspan="7" style="color:var(--muted);text-align:center">Loading…</td></tr>
      </tbody>
    </table>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-ghost" onclick="clearData()">Reset</button>
    </div>
  </div>
</div>

<script>
let g_all = [];
let g_sort = {key:'quality', asc:false};

function sort(key) {
  if(g_sort.key===key) g_sort.asc=!g_sort.asc;
  else {g_sort.key=key; g_sort.asc=false;}
  renderTable();
}

function quality_color(q) {
  if(q>=80) return 'var(--success)';
  if(q>=55) return 'var(--warn)';
  return 'var(--danger)';
}

function draw_sparkline(id, data) {
  const c = document.getElementById(id);
  if(!c) return;
  c.width  = 80;
  c.height = 20;
  const ctx = c.getContext('2d');
  ctx.clearRect(0,0,80,20);
  if(!data||!data.length) return;
  const max_v = Math.max(...data, 1);
  const min_v = Math.min(...data, 0);
  const range = Math.max(max_v - min_v, 1);
  ctx.strokeStyle = '#00d4ff';
  ctx.lineWidth = 1;
  ctx.beginPath();
  data.forEach((v,i)=>{
    const x = i*(80/(data.length-1));
    const y = 18-(v-min_v)/range*16;
    i===0?ctx.moveTo(x,y):ctx.lineTo(x,y);
  });
  ctx.stroke();
}

function renderTable() {
  const list = [...g_all].sort((a,b)=>{
    const va=a[g_sort.key], vb=b[g_sort.key];
    if(typeof va==='number') return g_sort.asc?va-vb:vb-va;
    return g_sort.asc?String(va).localeCompare(String(vb)):String(vb).localeCompare(String(va));
  });

  document.getElementById('ap-tbody').innerHTML = list.length
    ? list.map(ap=>{
        const qcol = quality_color(ap.quality);
        return `<tr>
          <td><code style="font-size:10px">${ap.bssid}</code></td>
          <td>${ap.ssid||'—'}</td>
          <td>${ap.ch}</td>
          <td style="color:${quality_color(ap.quality)};font-weight:700">${ap.snr} dB</td>
          <td><div class="quality-bar">
            <span style="color:${qcol};min-width:34px">${ap.quality}%</span>
            <div class="q-track"><div class="q-fill" style="width:${ap.quality}%;background:${qcol}"></div></div>
          </div></td>
          <td style="color:${ap.range>15?'var(--warn)':'var(--muted)'}">${ap.range} dB</td>
          <td class="spark-cell"><canvas id="spark-${ap.bssid.replace(/:/g,'')}"></canvas></td>
        </tr>`;
      }).join('')
    : '<tr><td colspan="7" style="color:var(--muted);text-align:center">No APs.</td></tr>';

  list.forEach(ap => draw_sparkline('spark-'+ap.bssid.replace(/:/g,''), ap.hist));
}

async function refresh() {
  const r = await fetch('/api/snr/all').then(x=>x.json()).catch(()=>({}));
  document.getElementById('h-aps').textContent = r.ap_count||0;

  const aps = r.aps||[];
  if(aps.length) {
    const best  = Math.max(...aps.map(a=>a.snr));
    const worst = Math.min(...aps.map(a=>a.snr));
    document.getElementById('h-best').textContent  = best+' dB';
    document.getElementById('h-worst').textContent = worst+' dB';
  }

  // Channel noise grid
  const noise = r.ch_noise||[];
  document.getElementById('ch-noise-grid').innerHTML = noise.map((n,i)=>{
    if(!n) return '';
    const col = n>-70?'var(--success)':n>-85?'var(--warn)':'var(--danger)';
    return `<div class="ch-cell"><div class="ch-num">Ch ${i+1}</div>
      <div class="ch-val" style="color:${col}">${n}</div></div>`;
  }).join('');

  g_all = aps;
  renderTable();
}

async function clearData() {
  await fetch('/api/snr/clear',{method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 2500);
</script>
</body>
</html>
)rawhtml";

inline void wusnr_routes() {
  server.on("/snr", HTTP_GET, []() { server.send_P(200, "text/html", WUSNR_PAGE); });
}

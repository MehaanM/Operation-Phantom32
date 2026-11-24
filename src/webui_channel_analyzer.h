#pragma once
// webui_channel_analyzer.h — Channel analyzer UI.
// Route: /channel-analyzer. Prefix: wucaz_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUCAZ_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Channel Analyzer — Phantom32</title>
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

/* Spectrum chart */
#spectrum-wrap{position:relative;height:160px;background:var(--bg3);border-radius:7px;overflow:hidden;margin-bottom:12px;}
#spectrum-canvas{width:100%;height:100%;}
.spectrum-labels{display:flex;justify-content:space-around;padding:4px 0;font-size:9px;color:var(--muted);}

.ch-cards{display:grid;grid-template-columns:repeat(auto-fill,minmax(120px,1fr));gap:8px;margin-top:12px;}
.ch-card{background:var(--bg3);border:1px solid var(--border);border-radius:7px;padding:10px;text-align:center;}
.ch-card.best{border:2px solid var(--success);}
.ch-card.busy{border-left:3px solid var(--danger);}
.ch-num{font-size:20px;font-weight:800;color:var(--text);}
.ch-pref{font-size:8px;color:var(--success);text-transform:uppercase;margin-bottom:4px;}
.ch-int-bar{height:5px;border-radius:2px;margin:5px 0;}
.ch-stat{font-size:9px;color:var(--muted);}
.best-banner{background:rgba(34,197,94,.08);border:1px solid var(--success);border-radius:5px;padding:8px 12px;margin-bottom:14px;display:flex;align-items:center;gap:10px;}

table{width:100%;border-collapse:collapse;font-size:10px;}
th{text-align:left;padding:8px 10px;border-bottom:2px solid var(--border);color:var(--muted);font-weight:600;font-size:9px;text-transform:uppercase;}
td{padding:7px 10px;border-bottom:1px solid var(--border);}
tr:hover td{background:var(--bg3);}
.int-bar{display:inline-block;height:6px;border-radius:3px;}
.badge{padding:1px 5px;border-radius:3px;font-size:8px;font-weight:700;background:rgba(0,212,255,.08);color:var(--accent);}
.badge-n{background:rgba(34,197,94,.08);color:var(--success);}
.badge-ac{background:rgba(245,158,11,.08);color:var(--warn);}
.badge-ax{background:rgba(239,68,68,.08);color:var(--danger);}
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
    <a href="/channel-analyzer" class="active">Channels</a>
    <a href="/signal-heatmap">Signal Map</a>
    <a href="/bss-load">BSS Load</a>
  </nav>
</div>
<div class="container">
  <h1>Channel Analyzer</h1>
  <p class="subtitle">Detailed 2.4 GHz channel usage, AP density, interference scores, and spectrum visualization. Channels 1, 6, and 11 are non-overlapping; other channels cause co-channel interference with their neighbors.</p>

  <div class="hero">
    <div class="stat"><label>Active Channels</label><div class="val" id="h-chs">0</div></div>
    <div class="stat"><label>Total APs</label><div class="val" id="h-aps">0</div></div>
    <div class="stat"><label>Total Frames</label><div class="val" id="h-frames">0</div></div>
    <div class="stat"><label>Best Channel</label><div class="val" id="h-best" style="color:var(--success)">—</div></div>
  </div>

  <div class="section">
    <h3>Spectrum View</h3>
    <div id="spectrum-wrap">
      <canvas id="spectrum-canvas"></canvas>
    </div>
    <div class="spectrum-labels" id="spec-labels"></div>
  </div>

  <div class="section">
    <h3>Channel Cards</h3>
    <div id="best-banner" style="display:none" class="best-banner">
      <div style="font-size:18px;color:var(--success)">★</div>
      <div>
        <div style="font-size:13px;font-weight:700;color:var(--success)">Recommended: Channel <span id="best-ch">—</span></div>
        <div style="font-size:10px;color:var(--muted)">Lowest observed interference score</div>
      </div>
    </div>
    <div class="ch-cards" id="ch-cards"></div>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>

  <div class="section">
    <h3>Channel Detail Table</h3>
    <div style="overflow-x:auto">
    <table>
      <thead><tr>
        <th>CH</th><th>APs</th><th>Frames</th>
        <th>RSSI</th><th>Pref</th>
        <th>Overlap</th><th>Interference</th><th>Protocols</th>
      </tr></thead>
      <tbody id="ch-table"></tbody>
    </table>
    </div>
  </div>
</div>

<script>
const canvas = document.getElementById('spectrum-canvas');
const ctx = canvas.getContext('2d');
let chData = [];

function resize() {
  canvas.width  = canvas.offsetWidth * devicePixelRatio;
  canvas.height = canvas.offsetHeight * devicePixelRatio;
  ctx.scale(devicePixelRatio, devicePixelRatio);
}
window.addEventListener('resize', resize);
resize();

function intColor(score) {
  if (score < 20) return 'var(--success)';
  if (score < 50) return 'var(--warn)';
  return 'var(--danger)';
}

function drawSpectrum(chs) {
  const W = canvas.offsetWidth, H = canvas.offsetHeight;
  ctx.clearRect(0, 0, W, H);

  if (!chs.length) return;
  const maxAps = Math.max(...chs.map(c=>c.ap_count), 1);
  const chW = W / 13;

  // Grid
  ctx.strokeStyle = 'rgba(42,48,69,0.5)';
  ctx.lineWidth = 0.5;
  for (let y = 0; y <= 5; y++) {
    ctx.beginPath();
    ctx.moveTo(0, y*(H/5));
    ctx.lineTo(W, y*(H/5));
    ctx.stroke();
  }

  // Channel bars
  chs.forEach(c => {
    const x = (c.ch - 1) * chW;
    const h = (c.ap_count / maxAps) * (H - 20);
    const ci = intColor(c.interference);

    // 20MHz channel width (~4 channels wide in spectrum terms)
    const spread = chW * 4;
    const grd = ctx.createLinearGradient(x+chW/2 - spread/2, 0, x+chW/2 + spread/2, 0);
    if (c.best) {
      grd.addColorStop(0, 'transparent');
      grd.addColorStop(0.3, 'rgba(34,197,94,0.15)');
      grd.addColorStop(0.7, 'rgba(34,197,94,0.15)');
      grd.addColorStop(1, 'transparent');
    } else {
      grd.addColorStop(0, 'transparent');
      grd.addColorStop(0.3, 'rgba(0,212,255,0.08)');
      grd.addColorStop(0.7, 'rgba(0,212,255,0.08)');
      grd.addColorStop(1, 'transparent');
    }
    ctx.fillStyle = grd;
    ctx.fillRect(x+chW/2-spread/2, 0, spread, H);

    // AP count bar
    ctx.fillStyle = c.best ? '#22c55e' : c.interference > 50 ? '#ef4444' : '#00d4ff';
    ctx.globalAlpha = 0.8;
    ctx.fillRect(x + 2, H - h - 2, chW - 4, h);
    ctx.globalAlpha = 1;

    // Channel label
    ctx.font = `bold 9px Inter,sans-serif`;
    ctx.fillStyle = c.best ? '#22c55e' : '#64748b';
    ctx.textAlign = 'center';
    ctx.fillText(c.ch, x + chW/2, H - 2);
  });
}

async function refresh() {
  const r = await fetch('/api/caz/all').then(x=>x.json()).catch(()=>({}));
  chData = r.channels || [];
  const totalAps = chData.reduce((s,c)=>s+c.ap_count, 0);

  document.getElementById('h-chs').textContent    = chData.length;
  document.getElementById('h-aps').textContent    = totalAps;
  document.getElementById('h-frames').textContent = (r.total_frames||0).toLocaleString();

  if (r.best_ch) {
    document.getElementById('h-best').textContent = 'CH'+r.best_ch;
    document.getElementById('best-ch').textContent = r.best_ch;
    document.getElementById('best-banner').style.display = 'flex';
  }

  drawSpectrum(chData);

  document.getElementById('ch-cards').innerHTML = chData.length
    ? chData.map(c => `<div class="ch-card ${c.best?'best':''} ${c.interference>60?'busy':''}">
        <div class="ch-num" style="color:${c.best?'var(--success)':c.interference>60?'var(--danger)':'var(--text)'}">${c.ch}</div>
        <div class="ch-pref">${c.preferred?'Preferred':''}</div>
        <div style="font-size:9px;color:var(--muted)">${c.ap_count} APs</div>
        <div class="ch-int-bar" style="background:var(--border)"><div style="width:${c.interference}%;height:100%;background:${intColor(c.interference)};border-radius:2px"></div></div>
        <div class="ch-stat">${c.interference}% int.</div>
        <div class="ch-stat">${c.rssi_mean} dBm</div>
      </div>`).join('')
    : '<div style="color:var(--muted)">No channel data.</div>';

  document.getElementById('ch-table').innerHTML = chData.length
    ? chData.map(c => {
        const protos = [
          c.ht  > 0 ? `<span class="badge badge-n">802.11n×${c.ht}</span>` : '',
          c.vht > 0 ? `<span class="badge badge-ac">ac×${c.vht}</span>` : '',
          c.he  > 0 ? `<span class="badge badge-ax">ax×${c.he}</span>` : '',
        ].filter(Boolean).join(' ');
        return `<tr>
          <td><b${c.best?' style="color:var(--success)"':''}>CH${c.ch}</b></td>
          <td>${c.ap_count}</td>
          <td>${(c.frames||0).toLocaleString()}</td>
          <td style="color:${c.rssi_mean>=-65?'var(--success)':c.rssi_mean>=-75?'var(--warn)':'var(--danger)'}">${c.rssi_mean}</td>
          <td>${c.preferred?'★':''}</td>
          <td style="color:var(--muted)">${c.overlap_count}</td>
          <td>
            <div style="display:flex;align-items:center;gap:5px">
              <span style="color:${intColor(c.interference)};font-size:10px">${c.interference}%</span>
              <div class="int-bar" style="width:${c.interference}px;max-width:80px;background:${intColor(c.interference)}"></div>
            </div>
          </td>
          <td>${protos||'—'}</td>
        </tr>`;
      }).join('')
    : '<tr><td colspan="8" style="text-align:center;color:var(--muted);padding:16px">No data.</td></tr>';
}

async function clearData() {
  await fetch('/api/caz/clear', {method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 3000);
</script>
</body>
</html>
)rawhtml";

inline void wucaz_routes() {
  server.on("/channel-analyzer", HTTP_GET, []() {
    server.send_P(200, "text/html", WUCAZ_PAGE);
  });
}

#pragma once
// webui_heatmap.h — Signal heatmap UI.
// Route: /heatmap. Prefix: wushm_

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
.container{max-width:1000px;margin:0 auto;padding:20px 16px;}
h1{font-size:22px;font-weight:700;margin-bottom:4px;}
.subtitle{color:var(--muted);font-size:13px;margin-bottom:20px;}
.hero{display:grid;grid-template-columns:repeat(4,1fr);gap:10px;margin-bottom:16px;}
.stat{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:13px;}
.stat label{font-size:10px;color:var(--muted);text-transform:uppercase;}
.stat .val{font-size:22px;font-weight:800;color:var(--accent);margin-top:3px;}
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:18px;margin-bottom:14px;}
.section h3{font-size:12px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:14px;}
.heatmap-wrap{position:relative;overflow:hidden;border-radius:7px;background:var(--bg3);}
canvas#heatmap{width:100%;display:block;}
canvas#overlay{position:absolute;top:0;left:0;width:100%;height:100%;pointer-events:none;}
.legend{display:flex;align-items:center;gap:8px;margin-top:8px;font-size:10px;color:var(--muted);}
.legend-bar{flex:1;height:8px;border-radius:4px;}
.ap-list{display:grid;grid-template-columns:repeat(auto-fill,minmax(200px,1fr));gap:8px;margin-top:4px;}
.ap-item{background:var(--bg3);border:1px solid var(--border);border-radius:6px;padding:10px;display:flex;align-items:center;gap:10px;}
.ap-dot{width:10px;height:10px;border-radius:50%;}
.ap-info{flex:1;min-width:0;}
.ap-ssid{font-size:11px;font-weight:600;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;}
.ap-mac{font-size:9px;color:var(--muted);}
.rssi-pill{padding:2px 6px;border-radius:3px;font-size:9px;font-weight:600;}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;margin-top:12px;}
.info-box{background:rgba(0,212,255,.04);border:1px solid rgba(0,212,255,.15);border-radius:6px;padding:10px;font-size:11px;color:var(--muted);margin-bottom:12px;}
@media(max-width:700px){.hero{grid-template-columns:1fr 1fr}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/spectrum">Spectrum</a>
    <a href="/heatmap" class="active">Heatmap</a>
    <a href="/noise-floor">Noise Floor</a>
  </nav>
</div>
<div class="container">
  <h1>RSSI Signal Heatmap</h1>
  <p class="subtitle">Visualizes relative signal strength of nearby APs as a 2D heatmap. Stronger signals appear brighter and closer to center. The heatmap updates as new beacon frames are received.</p>

  <div class="info-box">
    <strong>Note:</strong> This is a <em>relative proximity</em> map — the ESP32 has no GPS or physical anchors.
    Stronger RSSI = closer to center; weaker = edges. Useful for identifying signal overlap and dead zones.
  </div>

  <div class="hero">
    <div class="stat"><label>APs</label><div class="val" id="h-cnt">0</div></div>
    <div class="stat"><label>Samples</label><div class="val" id="h-samp" style="font-size:14px">0</div></div>
    <div class="stat"><label>Strongest</label><div class="val" id="h-str" style="font-size:14px">—</div></div>
    <div class="stat"><label>Weakest</label><div class="val" id="h-weak" style="font-size:14px">—</div></div>
  </div>

  <div class="section">
    <h3>Signal Heatmap</h3>
    <div class="heatmap-wrap">
      <canvas id="heatmap" height="300"></canvas>
      <canvas id="overlay" height="300"></canvas>
    </div>
    <div class="legend">
      <span>Weak (−100 dBm)</span>
      <div class="legend-bar" id="legend-bar" style="background:linear-gradient(90deg,#0a0c10,#06b6d4aa,#22c55e)"></div>
      <span>Strong (−30 dBm)</span>
    </div>
  </div>

  <div class="section">
    <h3>AP Signal Map</h3>
    <div class="ap-list" id="ap-list">
      <div style="color:var(--muted)">No APs.</div>
    </div>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>
</div>

<script>
let g_data = {aps:[], grid:[], grid_w:16, grid_h:12, ap_count:0, total_samples:0};

// Color scale: deep blue → teal → green for RSSI −100 to −30
function rssiToRgb(rssi) {
  if (!rssi || rssi === 0) return [10,12,16,0]; // transparent
  const t = Math.max(0, Math.min(1, (rssi + 100) / 70));
  if (t < 0.5) {
    // dark blue → teal
    const f = t * 2;
    return [Math.round(6*f), Math.round(12 + 182*f), Math.round(16 + 196*f), Math.round(255*t*2)];
  } else {
    // teal → green
    const f = (t - 0.5) * 2;
    return [Math.round(6 + 28*f), Math.round(194 - 24*f), Math.round(212 - 118*f), 220];
  }
}

function rssiColor(r) {
  return r >= -60 ? '#22c55e' : r >= -75 ? '#f59e0b' : '#ef4444';
}

function drawHeatmap(grid, W, H, aps) {
  const hCanvas = document.getElementById('heatmap');
  const oCanvas = document.getElementById('overlay');
  const hCtx = hCanvas.getContext('2d');
  const oCtx = oCanvas.getContext('2d');
  const pW = hCanvas.offsetWidth, pH = 300;
  hCanvas.width = pW; hCanvas.height = pH;
  oCanvas.width = pW; oCanvas.height = pH;

  const cw = pW / W, ch = pH / H;

  // Clear
  hCtx.fillStyle = '#0a0c10';
  hCtx.fillRect(0, 0, pW, pH);
  oCtx.clearRect(0, 0, pW, pH);

  // Draw heatmap cells
  for (let y = 0; y < H; y++) {
    for (let x = 0; x < W; x++) {
      const v = grid[y] ? grid[y][x] : 0;
      const [r,g,b,a] = rssiToRgb(v);
      hCtx.fillStyle = `rgba(${r},${g},${b},${a/255})`;
      hCtx.fillRect(x*cw, y*ch, cw+1, ch+1);
    }
  }

  // Grid lines
  hCtx.strokeStyle = 'rgba(42,48,69,0.3)';
  hCtx.lineWidth = 0.5;
  for (let x = 0; x <= W; x++) { hCtx.beginPath(); hCtx.moveTo(x*cw,0); hCtx.lineTo(x*cw,pH); hCtx.stroke(); }
  for (let y = 0; y <= H; y++) { hCtx.beginPath(); hCtx.moveTo(0,y*ch); hCtx.lineTo(pW,y*ch); hCtx.stroke(); }

  // Overlay AP positions
  aps.forEach((a, i) => {
    if (!a.position_set && a.gx == null) return;
    const px = (a.gx + 0.5) * cw, py = (a.gy + 0.5) * ch;
    const color = rssiColor(a.rssi);
    // Pulsing circle
    oCtx.beginPath();
    oCtx.arc(px, py, 8, 0, 2*Math.PI);
    oCtx.fillStyle = color + '33';
    oCtx.fill();
    oCtx.beginPath();
    oCtx.arc(px, py, 5, 0, 2*Math.PI);
    oCtx.fillStyle = color;
    oCtx.fill();
    oCtx.strokeStyle = '#0a0c10';
    oCtx.lineWidth = 1.5;
    oCtx.stroke();
    // Label
    oCtx.fillStyle = '#e2e8f0';
    oCtx.font = '8px system-ui';
    oCtx.textAlign = 'center';
    oCtx.fillText(a.ssid ? a.ssid.slice(0,8) : a.bssid.slice(-5), px, py + 15);
  });
}

function updateAPList(aps) {
  const sorted = [...aps].sort((a,b) => b.rssi - a.rssi);
  document.getElementById('ap-list').innerHTML = sorted.length
    ? sorted.map((a, i) => {
        const color = rssiColor(a.rssi);
        return `<div class="ap-item">
          <div class="ap-dot" style="background:${color}"></div>
          <div class="ap-info">
            <div class="ap-ssid">${a.ssid || 'Hidden'}</div>
            <div class="ap-mac">${a.bssid}</div>
          </div>
          <span class="rssi-pill" style="background:${color}22;color:${color}">${a.rssi} dBm</span>
        </div>`;
      }).join('')
    : '<div style="color:var(--muted)">No APs.</div>';
}

async function refresh() {
  const r = await fetch('/api/shm/all').then(x => x.json()).catch(() => g_data);
  g_data = r;
  document.getElementById('h-cnt').textContent  = r.ap_count || 0;
  document.getElementById('h-samp').textContent = (r.total_samples||0).toLocaleString();
  const aps = r.aps || [];
  if (aps.length) {
    const best  = aps.reduce((a,b) => a.rssi > b.rssi ? a : b);
    const worst = aps.reduce((a,b) => a.rssi < b.rssi ? a : b);
    document.getElementById('h-str').textContent  = best.rssi + ' dBm';
    document.getElementById('h-weak').textContent = worst.rssi + ' dBm';
  }
  if (r.grid && r.grid.length) {
    drawHeatmap(r.grid, r.grid_w, r.grid_h, aps);
  }
  updateAPList(aps);
}

async function clearData() {
  await fetch('/api/shm/clear', {method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 3000);
window.addEventListener('resize', () => {
  if (g_data.grid) drawHeatmap(g_data.grid, g_data.grid_w, g_data.grid_h, g_data.aps||[]);
});
</script>
</body>
</html>
)rawhtml";

inline void wushm_routes() {
  server.on("/heatmap", HTTP_GET, []() { server.send_P(200, "text/html", WUSHM_PAGE); });
}

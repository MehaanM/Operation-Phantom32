#pragma once
// webui_map.h — Network map / topology visualization page.
// Canvas-based force-directed graph of APs, stations, and links.
// Color-coded by security, animated RSSI pulse rings.
// Prefix: wum_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUM_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Phantom32 · Network Map</title>
<style>
:root{--bg:#0a0e1a;--surface:#111827;--surface2:#1a2235;--border:#2d3748;--accent:#00d4ff;--text:#e2e8f0;--muted:#64748b;}
*{margin:0;padding:0;box-sizing:border-box;}
body{background:var(--bg);color:var(--text);font-family:'Segoe UI',system-ui,sans-serif;font-size:13px;height:100vh;display:flex;flex-direction:column;overflow:hidden;}
header{background:var(--surface);border-bottom:1px solid var(--border);padding:10px 16px;display:flex;align-items:center;gap:12px;flex-shrink:0;}
header h1{font-size:16px;font-weight:700;color:var(--accent);}
nav a{color:var(--muted);text-decoration:none;padding:6px 10px;border-radius:6px;font-size:12px;}
nav a:hover{background:var(--surface2);color:var(--accent);}
.toolbar{background:var(--surface);border-bottom:1px solid var(--border);padding:8px 14px;display:flex;gap:8px;align-items:center;flex-wrap:wrap;flex-shrink:0;}
.btn{background:var(--surface2);border:1px solid var(--border);color:var(--text);padding:5px 12px;border-radius:6px;cursor:pointer;font-size:12px;}
.btn:hover{background:var(--border);}
.btn.active{background:var(--accent);color:#000;}
select{background:var(--surface2);border:1px solid var(--border);color:var(--text);padding:5px 8px;border-radius:6px;font-size:12px;}
.map-wrap{flex:1;position:relative;overflow:hidden;}
#mapCanvas{width:100%;height:100%;display:block;cursor:grab;}
#mapCanvas:active{cursor:grabbing;}
.legend{position:absolute;bottom:14px;left:14px;background:var(--surface);border:1px solid var(--border);border-radius:8px;padding:10px 14px;font-size:11px;}
.legend-row{display:flex;align-items:center;gap:6px;margin-bottom:5px;}
.legend-row:last-child{margin-bottom:0;}
.legend-dot{width:12px;height:12px;border-radius:50%;}
.info-panel{position:absolute;top:14px;right:14px;width:220px;background:var(--surface);border:1px solid var(--border);border-radius:8px;padding:12px;display:none;}
.info-panel.open{display:block;}
.info-h{font-size:12px;font-weight:700;color:var(--accent);margin-bottom:8px;}
.info-row{display:flex;gap:6px;margin-bottom:4px;font-size:11px;}
.info-key{color:var(--muted);width:80px;flex-shrink:0;}
.stats-bar{position:absolute;bottom:14px;right:14px;background:var(--surface);border:1px solid var(--border);border-radius:8px;padding:10px 14px;font-size:11px;display:flex;gap:14px;}
.stats-item .n{font-weight:700;color:var(--accent);font-size:16px;}
.stats-item .l{color:var(--muted);font-size:10px;}
</style>
</head>
<body>
<header>
  <h1>⚡ Phantom32</h1>
  <span style="background:var(--accent);color:#000;padding:2px 8px;border-radius:12px;font-size:11px;font-weight:700;">MAP</span>
  <div id="node-count" style="color:var(--muted);font-size:11px;"></div>
  <nav style="display:flex;gap:2px;margin-left:auto;">
    <a href="/">Home</a>
    <a href="/threats">Threats</a>
    <a href="/analytics">Analytics</a>
    <a href="/cli">CLI</a>
  </nav>
</header>
<div class="toolbar">
  <button class="btn active" id="btnPhysics" onclick="togglePhysics()">Physics</button>
  <button class="btn" onclick="resetView()">Reset View</button>
  <button class="btn" onclick="fitAll()">Fit All</button>
  <select id="colorMode" onchange="setColorMode(this.value)">
    <option value="security">Color by Security</option>
    <option value="channel">Color by Channel</option>
    <option value="rssi">Color by RSSI</option>
    <option value="vendor">Color by Vendor</option>
  </select>
  <label style="font-size:12px;color:var(--muted);display:flex;align-items:center;gap:5px;">
    <input type="checkbox" id="showLabels" checked onchange="render()"> Labels
  </label>
  <label style="font-size:12px;color:var(--muted);display:flex;align-items:center;gap:5px;">
    <input type="checkbox" id="showStations" checked onchange="loadData()"> Stations
  </label>
  <span style="color:var(--muted);font-size:11px;margin-left:auto;" id="last-refresh"></span>
  <button class="btn" onclick="loadData()">Refresh</button>
</div>
<div class="map-wrap">
  <canvas id="mapCanvas"></canvas>
  <div class="legend">
    <div class="legend-row"><div class="legend-dot" style="background:#10b981"></div> Open</div>
    <div class="legend-row"><div class="legend-dot" style="background:#f59e0b"></div> WPA/WPA2</div>
    <div class="legend-row"><div class="legend-dot" style="background:#00d4ff"></div> WPA3/SAE</div>
    <div class="legend-row"><div class="legend-dot" style="background:#ef4444"></div> WEP</div>
    <div class="legend-row"><div class="legend-dot" style="background:#64748b"></div> Station</div>
  </div>
  <div class="info-panel" id="infoPanel">
    <div class="info-h" id="info-title">Device</div>
    <div id="info-body"></div>
    <button onclick="document.getElementById('infoPanel').classList.remove('open')" style="margin-top:8px;background:var(--surface2);border:1px solid var(--border);color:var(--muted);padding:3px 10px;border-radius:4px;cursor:pointer;font-size:11px;width:100%;">Close</button>
  </div>
  <div class="stats-bar">
    <div class="stats-item"><div class="n" id="st-aps">0</div><div class="l">APs</div></div>
    <div class="stats-item"><div class="n" id="st-stas">0</div><div class="l">Stations</div></div>
    <div class="stats-item"><div class="n" id="st-links">0</div><div class="l">Links</div></div>
  </div>
</div>

<script>
const canvas = document.getElementById('mapCanvas');
const ctx = canvas.getContext('2d');

let nodes = [], edges = [];
let physics = true;
let colorMode = 'security';
let scale = 1, offX = 0, offY = 0;
let dragging = false, dragNode = null, dragStart = {x:0,y:0}, lastMouse = {x:0,y:0};
let animReq = null;

function resize() {
  canvas.width  = canvas.offsetWidth;
  canvas.height = canvas.offsetHeight;
}
window.addEventListener('resize', resize);
resize();

// ─── Color helpers ─────────────────────────────────────────────────────────────
const CH_COLORS = ['#ef4444','#f97316','#f59e0b','#eab308','#84cc16','#22c55e',
                   '#10b981','#14b8a6','#06b6d4','#0ea5e9','#6366f1','#8b5cf6','#a855f7'];

function nodeColor(n) {
  if (n.type === 'sta') return '#64748b';
  switch(colorMode) {
    case 'security':
      if (n.security === 'OPEN') return '#10b981';
      if ((n.security||'').includes('WPA3') || (n.security||'').includes('SAE')) return '#00d4ff';
      if ((n.security||'').includes('WEP')) return '#ef4444';
      return '#f59e0b';
    case 'channel':
      return CH_COLORS[(n.channel||1) % CH_COLORS.length];
    case 'rssi': {
      const r = n.rssi || -80;
      const t = Math.max(0, Math.min(1, (r + 90) / 60));
      return `hsl(${(t*120).toFixed(0)},80%,50%)`;
    }
    case 'vendor':
      return '#7c3aed';
    default: return '#00d4ff';
  }
}

// ─── Force simulation ──────────────────────────────────────────────────────────
function tickPhysics() {
  if (!physics) return;
  const W = canvas.width / scale;
  const H = canvas.height / scale;
  const cx = W / 2 - offX / scale;
  const cy = H / 2 - offY / scale;

  for (const n of nodes) {
    n.vx = (n.vx || 0) * 0.85;
    n.vy = (n.vy || 0) * 0.85;
    // gravity toward center
    n.vx += (cx - n.x) * 0.0003;
    n.vy += (cy - n.y) * 0.0003;
  }

  // Repulsion
  for (let i = 0; i < nodes.length; i++) {
    for (let j = i+1; j < nodes.length; j++) {
      const a = nodes[i], b = nodes[j];
      const dx = b.x - a.x, dy = b.y - a.y;
      const dist = Math.sqrt(dx*dx + dy*dy) || 1;
      const repel = 3000 / (dist * dist);
      const fx = dx/dist * repel, fy = dy/dist * repel;
      a.vx -= fx; a.vy -= fy;
      b.vx += fx; b.vy += fy;
    }
  }

  // Attraction along edges
  for (const e of edges) {
    const a = nodes.find(n => n.id === e.a);
    const b = nodes.find(n => n.id === e.b);
    if (!a || !b) continue;
    const dx = b.x - a.x, dy = b.y - a.y;
    const dist = Math.sqrt(dx*dx + dy*dy) || 1;
    const target = 120;
    const strength = (dist - target) * 0.01;
    const fx = dx/dist * strength, fy = dy/dist * strength;
    a.vx += fx; a.vy += fy;
    b.vx -= fx; b.vy -= fy;
  }

  for (const n of nodes) {
    if (n === dragNode) continue;
    n.x += n.vx;
    n.y += n.vy;
  }
}

// ─── Render ────────────────────────────────────────────────────────────────────
function render() {
  ctx.clearRect(0, 0, canvas.width, canvas.height);

  ctx.save();
  ctx.translate(offX + canvas.width/2, offY + canvas.height/2);
  ctx.scale(scale, scale);

  // Edges
  for (const e of edges) {
    const a = nodes.find(n => n.id === e.a);
    const b = nodes.find(n => n.id === e.b);
    if (!a || !b) continue;
    ctx.beginPath();
    ctx.moveTo(a.x, a.y);
    ctx.lineTo(b.x, b.y);
    ctx.strokeStyle = 'rgba(45,55,72,0.8)';
    ctx.lineWidth = 1;
    ctx.stroke();
  }

  const showLabels = document.getElementById('showLabels').checked;
  const now = Date.now();

  for (const n of nodes) {
    const r = n.type === 'ap' ? 14 : 8;
    const col = nodeColor(n);

    // Pulse ring for recent activity
    if (n.last_seen && (now/1000 - n.last_seen) < 5) {
      const phase = ((now % 1500) / 1500);
      const pr = r + phase * 20;
      ctx.beginPath();
      ctx.arc(n.x, n.y, pr, 0, Math.PI*2);
      ctx.strokeStyle = col + '44';
      ctx.lineWidth = 2;
      ctx.stroke();
    }

    ctx.beginPath();
    ctx.arc(n.x, n.y, r, 0, Math.PI*2);
    ctx.fillStyle = col + 'cc';
    ctx.fill();
    ctx.strokeStyle = col;
    ctx.lineWidth = 2;
    ctx.stroke();

    if (n.type === 'ap') {
      ctx.fillStyle = '#fff';
      ctx.font = 'bold 8px monospace';
      ctx.textAlign = 'center';
      ctx.textBaseline = 'middle';
      ctx.fillText('AP', n.x, n.y);
    }

    if (showLabels) {
      ctx.fillStyle = '#e2e8f0';
      ctx.font = '10px sans-serif';
      ctx.textAlign = 'center';
      ctx.textBaseline = 'top';
      ctx.fillText(n.label || n.id, n.x, n.y + r + 3);
    }
  }

  ctx.restore();
}

function loop() {
  tickPhysics();
  render();
  animReq = requestAnimationFrame(loop);
}

// ─── Data loading ──────────────────────────────────────────────────────────────
async function loadData() {
  try {
    const r = await fetch('/api/scan/results');
    const data = await r.json();
    const aps = data.aps || data.results || [];
    const showStas = document.getElementById('showStations').checked;

    const W = canvas.width / scale;
    const H = canvas.height / scale;

    // Build node list
    const prevNodes = {};
    for (const n of nodes) prevNodes[n.id] = {x:n.x, y:n.y};

    nodes = [];
    edges = [];

    for (const ap of aps) {
      const id = ap.bssid || ap.mac;
      if (!id) continue;
      const prev = prevNodes[id];
      nodes.push({
        id,
        type: 'ap',
        label: ap.ssid || id.substring(9),
        x: prev ? prev.x : (Math.random() - 0.5) * W * 0.6,
        y: prev ? prev.y : (Math.random() - 0.5) * H * 0.6,
        vx: 0, vy: 0,
        rssi: ap.rssi,
        channel: ap.channel,
        security: ap.security || ap.auth,
        vendor: ap.vendor,
        last_seen: ap.last_seen,
        raw: ap,
      });

      // Station edges
      if (showStas && ap.stations) {
        for (const sta of ap.stations) {
          const sid = sta.mac || sta;
          if (!nodes.find(n => n.id === sid)) {
            const prev2 = prevNodes[sid];
            nodes.push({
              id: sid,
              type: 'sta',
              label: sid.substring(9),
              x: prev2 ? prev2.x : (nodes[nodes.length-1].x + (Math.random()-0.5)*60),
              y: prev2 ? prev2.y : (nodes[nodes.length-1].y + (Math.random()-0.5)*60),
              vx: 0, vy: 0,
              rssi: sta.rssi,
              last_seen: sta.last_seen,
              raw: sta,
            });
          }
          edges.push({a: id, b: sid});
        }
      }
    }

    document.getElementById('st-aps').textContent   = nodes.filter(n=>n.type==='ap').length;
    document.getElementById('st-stas').textContent  = nodes.filter(n=>n.type==='sta').length;
    document.getElementById('st-links').textContent = edges.length;
    document.getElementById('node-count').textContent = `${nodes.length} nodes`;
    document.getElementById('last-refresh').textContent = 'Updated ' + new Date().toLocaleTimeString();

  } catch(e) {
    document.getElementById('node-count').textContent = 'Fetch error';
  }
}

// ─── Interactions ──────────────────────────────────────────────────────────────
function hitTest(mx, my) {
  const wx = (mx - canvas.width/2 - offX) / scale;
  const wy = (my - canvas.height/2 - offY) / scale;
  for (let i = nodes.length-1; i >= 0; i--) {
    const n = nodes[i];
    const r = n.type === 'ap' ? 18 : 12;
    if ((wx-n.x)*(wx-n.x) + (wy-n.y)*(wy-n.y) < r*r) return n;
  }
  return null;
}

canvas.addEventListener('mousedown', e => {
  const hit = hitTest(e.offsetX, e.offsetY);
  if (hit) {
    dragNode = hit;
    dragging = true;
    showInfo(hit);
  } else {
    dragging = true;
    dragNode = null;
  }
  dragStart = {x: e.offsetX - offX, y: e.offsetY - offY};
  lastMouse = {x: e.offsetX, y: e.offsetY};
});

canvas.addEventListener('mousemove', e => {
  if (!dragging) return;
  if (dragNode) {
    const wx = (e.offsetX - canvas.width/2 - offX) / scale;
    const wy = (e.offsetY - canvas.height/2 - offY) / scale;
    dragNode.x = wx;
    dragNode.y = wy;
    dragNode.vx = 0;
    dragNode.vy = 0;
  } else {
    offX += e.offsetX - lastMouse.x;
    offY += e.offsetY - lastMouse.y;
  }
  lastMouse = {x: e.offsetX, y: e.offsetY};
});

canvas.addEventListener('mouseup', () => { dragging = false; dragNode = null; });
canvas.addEventListener('mouseleave', () => { dragging = false; dragNode = null; });

canvas.addEventListener('wheel', e => {
  e.preventDefault();
  const factor = e.deltaY < 0 ? 1.1 : 0.9;
  scale = Math.max(0.1, Math.min(5, scale * factor));
}, {passive: false});

function showInfo(n) {
  document.getElementById('info-title').textContent = n.type === 'ap' ? (n.label || n.id) : n.id;
  const rows = [
    ['Type', n.type === 'ap' ? 'Access Point' : 'Station'],
    ['BSSID', n.id],
    ...(n.type === 'ap' ? [
      ['SSID', n.raw?.ssid || '—'],
      ['Channel', n.channel || '—'],
      ['Security', n.security || '—'],
      ['RSSI', (n.rssi || '—') + (n.rssi ? ' dBm' : '')],
      ['Vendor', n.vendor || '—'],
    ] : [
      ['RSSI', (n.rssi || '—') + (n.rssi ? ' dBm' : '')],
    ]),
  ];
  document.getElementById('info-body').innerHTML = rows.map(([k,v]) =>
    `<div class="info-row"><div class="info-key">${k}</div><div>${v}</div></div>`
  ).join('');
  document.getElementById('infoPanel').classList.add('open');
}

function togglePhysics() {
  physics = !physics;
  document.getElementById('btnPhysics').classList.toggle('active', physics);
}

function resetView() { offX = 0; offY = 0; scale = 1; }

function fitAll() {
  if (!nodes.length) return;
  let minX=Infinity, maxX=-Infinity, minY=Infinity, maxY=-Infinity;
  for (const n of nodes) {
    if (n.x < minX) minX = n.x;
    if (n.x > maxX) maxX = n.x;
    if (n.y < minY) minY = n.y;
    if (n.y > maxY) maxY = n.y;
  }
  const padX = (maxX - minX) * 0.15 + 60;
  const padY = (maxY - minY) * 0.15 + 60;
  scale = Math.min(canvas.width / (maxX - minX + padX*2), canvas.height / (maxY - minY + padY*2), 2);
  offX = -(minX + maxX) / 2 * scale;
  offY = -(minY + maxY) / 2 * scale;
}

function setColorMode(m) { colorMode = m; }

loadData();
setInterval(loadData, 20000);
loop();
</script>
</body>
</html>
)rawhtml";

inline void wum_routes() {
  server.on("/map", HTTP_GET, []() { server.send_P(200, "text/html", WUM_PAGE); });
}

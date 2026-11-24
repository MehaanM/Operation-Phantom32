#pragma once
// webui_net_topology.h — Network topology graph UI.
// Route: /topology. Prefix: wuntt_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUNTT_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Network Topology — Phantom32</title>
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
#topology-canvas{width:100%;height:460px;background:var(--bg3);border-radius:7px;cursor:grab;user-select:none;}
#topology-canvas:active{cursor:grabbing;}
.two-col{display:grid;grid-template-columns:1fr 1fr;gap:14px;}
table{width:100%;border-collapse:collapse;font-size:10px;}
th{text-align:left;padding:7px 10px;border-bottom:2px solid var(--border);color:var(--muted);font-weight:600;font-size:9px;text-transform:uppercase;}
td{padding:6px 10px;border-bottom:1px solid var(--border);}
tr:hover td{background:var(--bg3);}
.node-type-ap{color:var(--accent);}
.node-type-client{color:var(--success);}
.rand-badge{display:inline-block;padding:1px 5px;border-radius:3px;font-size:8px;background:rgba(245,158,11,.1);color:var(--warn);border:1px solid rgba(245,158,11,.2);}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;margin-top:12px;}
.legend{display:flex;gap:16px;font-size:10px;color:var(--muted);flex-wrap:wrap;margin-bottom:12px;}
.legend-item{display:flex;align-items:center;gap:5px;}
.legend-dot{width:10px;height:10px;border-radius:50%;}
.tooltip{position:fixed;pointer-events:none;background:var(--bg2);border:1px solid var(--border);border-radius:6px;padding:8px 10px;font-size:10px;z-index:100;display:none;max-width:200px;}
@media(max-width:700px){.hero{grid-template-columns:1fr 1fr};.two-col{grid-template-columns:1fr}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/topology" class="active">Topology</a>
    <a href="/scan-cache">Scan Cache</a>
    <a href="/probe-flood">Probe Flood</a>
  </nav>
</div>
<div class="container">
  <h1>Network Topology</h1>
  <p class="subtitle">Live graph of observed AP-client associations, roaming events, and ESS (same-SSID) relationships. Each node is an AP (cyan) or client (green); edges show association, probe, roam, or ESS links.</p>

  <div class="hero">
    <div class="stat"><label>Nodes</label><div class="val" id="h-nodes">0</div></div>
    <div class="stat"><label>Edges</label><div class="val" id="h-edges">0</div></div>
    <div class="stat"><label>ESS Groups</label><div class="val" id="h-ess" style="color:var(--warn)">0</div></div>
    <div class="stat"><label>Associations</label><div class="val" id="h-assoc" style="color:var(--success)">0</div></div>
    <div class="stat"><label>Roam Events</label><div class="val" id="h-roam" style="color:var(--danger)">0</div></div>
  </div>

  <div class="section">
    <h3>Topology Graph</h3>
    <div class="legend">
      <div class="legend-item"><div class="legend-dot" style="background:var(--accent)"></div> AP</div>
      <div class="legend-item"><div class="legend-dot" style="background:var(--success)"></div> Client</div>
      <div class="legend-item"><div class="legend-dot" style="background:var(--warn);width:30px;height:3px;border-radius:0"></div> Association</div>
      <div class="legend-item"><div class="legend-dot" style="background:var(--danger);width:30px;height:1px;border-radius:0;border-top:1px dashed var(--danger)"></div> Roam</div>
      <div class="legend-item"><div class="legend-dot" style="background:var(--border);width:30px;height:1px;border-radius:0"></div> Probe</div>
    </div>
    <canvas id="topology-canvas"></canvas>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
      <button class="btn btn-ghost" onclick="resetLayout()">Reset Layout</button>
    </div>
  </div>

  <div class="two-col">
    <div class="section">
      <h3>Nodes</h3>
      <div style="overflow-x:auto;max-height:260px;overflow-y:auto">
      <table>
        <thead><tr><th>MAC</th><th>Type</th><th>SSID</th><th>CH</th><th>RSSI</th><th>Frames</th></tr></thead>
        <tbody id="node-body"></tbody>
      </table>
      </div>
    </div>
    <div class="section">
      <h3>ESS Groups (Multi-AP SSIDs)</h3>
      <div id="ess-list">
        <div style="color:var(--muted)">No ESS groups detected.</div>
      </div>
    </div>
  </div>
</div>
<div class="tooltip" id="tooltip"></div>

<script>
const canvas = document.getElementById('topology-canvas');
const ctx = canvas.getContext('2d');
const tooltip = document.getElementById('tooltip');

let nodes = [], edges = [], drag = null, pan = {x:0,y:0}, panStart = null;
let velocities = {};
let simRunning = false;

function resize() {
  canvas.width  = canvas.offsetWidth * devicePixelRatio;
  canvas.height = canvas.offsetHeight * devicePixelRatio;
  ctx.scale(devicePixelRatio, devicePixelRatio);
}

window.addEventListener('resize', resize);
resize();

// Force-directed layout
function simulate() {
  if (!simRunning) return;
  const W = canvas.offsetWidth, H = canvas.offsetHeight;
  const cx = W/2 + pan.x, cy = H/2 + pan.y;
  const K = 80, REPEL = 4000, DAMP = 0.85;

  // Init positions if needed
  nodes.forEach(n => {
    if (!n.x) {
      n.x = cx + (Math.random()-0.5)*200;
      n.y = cy + (Math.random()-0.5)*200;
    }
    if (!velocities[n.mac]) velocities[n.mac] = {x:0,y:0};
  });

  // Repulsion
  for (let i = 0; i < nodes.length; i++) {
    for (let j = i+1; j < nodes.length; j++) {
      const a = nodes[i], b = nodes[j];
      const dx = b.x - a.x, dy = b.y - a.y;
      const dist = Math.sqrt(dx*dx+dy*dy) || 1;
      const f = REPEL / (dist*dist);
      const fx = dx/dist*f, fy = dy/dist*f;
      velocities[a.mac].x -= fx; velocities[a.mac].y -= fy;
      velocities[b.mac].x += fx; velocities[b.mac].y += fy;
    }
  }

  // Attraction along edges
  const nodeMap = {};
  nodes.forEach(n => nodeMap[n.mac] = n);
  edges.forEach(e => {
    const a = nodeMap[e.src], b = nodeMap[e.dst];
    if (!a || !b) return;
    const dx = b.x - a.x, dy = b.y - a.y;
    const dist = Math.sqrt(dx*dx+dy*dy) || 1;
    const f = (dist - K) * 0.015;
    const fx = dx/dist*f, fy = dy/dist*f;
    if (velocities[a.mac]) { velocities[a.mac].x += fx; velocities[a.mac].y += fy; }
    if (velocities[b.mac]) { velocities[b.mac].x -= fx; velocities[b.mac].y -= fy; }
  });

  // Gravity toward center
  nodes.forEach(n => {
    const dx = cx - n.x, dy = cy - n.y;
    if (velocities[n.mac]) {
      velocities[n.mac].x += dx * 0.003;
      velocities[n.mac].y += dy * 0.003;
    }
  });

  // Apply velocities
  nodes.forEach(n => {
    if (n === drag) return;
    const v = velocities[n.mac];
    if (!v) return;
    n.x += v.x; n.y += v.y;
    v.x *= DAMP; v.y *= DAMP;
  });

  draw();
  requestAnimationFrame(simulate);
}

function edgeColor(type) {
  switch(type) {
    case 'assoc':    return 'rgba(245,158,11,0.6)';
    case 'roam':     return 'rgba(239,68,68,0.6)';
    case 'probe':    return 'rgba(100,116,139,0.4)';
    case 'same_ess': return 'rgba(0,212,255,0.3)';
    default:         return 'rgba(100,116,139,0.3)';
  }
}

function draw() {
  const W = canvas.offsetWidth, H = canvas.offsetHeight;
  ctx.clearRect(0, 0, W, H);
  const nodeMap = {};
  nodes.forEach(n => nodeMap[n.mac] = n);

  // Edges
  edges.forEach(e => {
    const a = nodeMap[e.src], b = nodeMap[e.dst];
    if (!a || !b || !a.x || !b.x) return;
    ctx.beginPath();
    ctx.moveTo(a.x, a.y);
    ctx.lineTo(b.x, b.y);
    ctx.strokeStyle = edgeColor(e.type);
    ctx.lineWidth = e.type === 'assoc' ? 1.5 : e.type === 'roam' ? 2 : 0.8;
    if (e.type === 'probe') { ctx.setLineDash([3,4]); } else { ctx.setLineDash([]); }
    ctx.stroke();
    ctx.setLineDash([]);
  });

  // Nodes
  nodes.forEach(n => {
    if (!n.x) return;
    const isAP = n.type === 0;
    const r = isAP ? 11 : 8;
    ctx.beginPath();
    if (isAP) {
      // Hexagon for APs
      for (let i = 0; i < 6; i++) {
        const a = Math.PI/3*i - Math.PI/6;
        i === 0 ? ctx.moveTo(n.x + r*Math.cos(a), n.y + r*Math.sin(a))
                : ctx.lineTo(n.x + r*Math.cos(a), n.y + r*Math.sin(a));
      }
      ctx.closePath();
    } else {
      ctx.arc(n.x, n.y, r, 0, Math.PI*2);
    }
    ctx.fillStyle = isAP ? 'rgba(0,212,255,0.15)' : 'rgba(34,197,94,0.15)';
    ctx.fill();
    ctx.strokeStyle = isAP ? '#00d4ff' : '#22c55e';
    ctx.lineWidth = 1.5;
    ctx.stroke();

    // Label
    if (n.ssid) {
      ctx.font = '9px Inter,sans-serif';
      ctx.fillStyle = 'rgba(226,232,240,0.7)';
      ctx.textAlign = 'center';
      ctx.fillText(n.ssid.substring(0,12), n.x, n.y + r + 11);
    }
  });
}

// Mouse events
canvas.addEventListener('mousedown', e => {
  const rect = canvas.getBoundingClientRect();
  const mx = e.clientX - rect.left, my = e.clientY - rect.top;
  const hit = nodes.find(n => n.x && Math.hypot(n.x-mx,n.y-my) < 14);
  if (hit) { drag = hit; }
  else { panStart = {x: e.clientX - pan.x, y: e.clientY - pan.y}; }
});

canvas.addEventListener('mousemove', e => {
  const rect = canvas.getBoundingClientRect();
  const mx = e.clientX - rect.left, my = e.clientY - rect.top;
  if (drag) {
    drag.x = mx; drag.y = my;
    if (velocities[drag.mac]) { velocities[drag.mac].x = 0; velocities[drag.mac].y = 0; }
    return;
  }
  if (panStart) {
    pan.x = e.clientX - panStart.x;
    pan.y = e.clientY - panStart.y;
    return;
  }
  const hit = nodes.find(n => n.x && Math.hypot(n.x-mx,n.y-my) < 14);
  if (hit) {
    tooltip.style.display = 'block';
    tooltip.style.left = (e.clientX + 12) + 'px';
    tooltip.style.top  = (e.clientY - 10) + 'px';
    tooltip.innerHTML = `<b>${hit.mac}</b><br>
      ${hit.type===0?'AP':'Client'} · CH${hit.ch} · ${hit.rssi} dBm<br>
      SSID: ${hit.ssid||'—'}<br>
      Frames: ${hit.frames}
      ${hit.rand?'<br><span style="color:var(--warn)">Randomized MAC</span>':''}`;
  } else {
    tooltip.style.display = 'none';
  }
});

canvas.addEventListener('mouseup', () => { drag = null; panStart = null; });
canvas.addEventListener('mouseleave', () => { drag = null; panStart = null; tooltip.style.display='none'; });

function resetLayout() {
  nodes.forEach(n => { delete n.x; delete n.y; velocities[n.mac] = {x:0,y:0}; });
}

async function refresh() {
  const r = await fetch('/api/ntt/all').then(x=>x.json()).catch(()=>({}));
  nodes = r.nodes || [];
  edges = r.edges || [];
  const ess  = r.ess  || [];

  document.getElementById('h-nodes').textContent = r.node_count || 0;
  document.getElementById('h-edges').textContent = r.edge_count || 0;
  document.getElementById('h-ess').textContent   = r.ess_count  || 0;
  document.getElementById('h-assoc').textContent = r.total_assoc || 0;
  document.getElementById('h-roam').textContent  = r.total_roam  || 0;

  // Node table
  document.getElementById('node-body').innerHTML = nodes.length
    ? nodes.map(n => `<tr>
        <td style="font-family:monospace;font-size:9px">${n.mac.substring(0,8)}…</td>
        <td class="${n.type===0?'node-type-ap':'node-type-client'}">${n.type===0?'AP':'Client'}</td>
        <td>${n.ssid||'—'}</td>
        <td>${n.ch}</td>
        <td style="color:${n.rssi>=-65?'var(--success)':n.rssi>=-80?'var(--warn)':'var(--danger)'}">${n.rssi}</td>
        <td>${n.frames}</td>
      </tr>`).join('')
    : '<tr><td colspan="6" style="text-align:center;color:var(--muted);padding:16px">No nodes.</td></tr>';

  // ESS list
  document.getElementById('ess-list').innerHTML = ess.length
    ? ess.map(e => `<div style="padding:8px;background:var(--bg3);border-radius:5px;margin-bottom:6px;border-left:2px solid var(--accent)">
        <div style="font-size:11px;font-weight:700;color:var(--accent)">${e.ssid}</div>
        <div style="font-size:9px;color:var(--muted)">${e.aps} BSSIDs (roaming domain)</div>
      </div>`).join('')
    : '<div style="color:var(--muted)">No multi-AP SSIDs detected.</div>';

  if (!simRunning) { simRunning = true; simulate(); }
}

async function clearData() {
  await fetch('/api/ntt/clear', {method:'POST'});
  nodes = []; edges = []; velocities = {};
  refresh();
}

refresh();
setInterval(refresh, 4000);
</script>
</body>
</html>
)rawhtml";

inline void wuntt_routes() {
  server.on("/topology", HTTP_GET, []() {
    server.send_P(200, "text/html", WUNTT_PAGE);
  });
}

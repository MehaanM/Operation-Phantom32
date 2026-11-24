#pragma once
// webui_mesh.h — Mesh network detector UI.
// Route: /mesh. Prefix: wumsh_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUMSH_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Mesh Detector — Phantom32</title>
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
canvas{background:var(--bg3);border-radius:7px;}
table{width:100%;border-collapse:collapse;font-size:11px;}
th{text-align:left;padding:7px 8px;font-size:9px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;}
td{padding:6px 8px;border-bottom:1px solid var(--border);}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
.badge{padding:1px 5px;border-radius:3px;font-size:9px;font-weight:700;display:inline-block;}
.badge-root{background:rgba(0,212,255,.12);color:var(--accent);}
.badge-gate{background:rgba(34,197,94,.12);color:var(--success);}
.badge-estab{background:rgba(34,197,94,.12);color:var(--success);}
.badge-open{background:rgba(245,158,11,.12);color:var(--warn);}
.badge-idle{background:rgba(100,116,139,.1);color:var(--muted);}
.cap-pill{padding:1px 5px;border-radius:10px;font-size:8px;border:1px solid var(--border);color:var(--muted);display:inline-block;margin:1px;}
.cap-on{color:var(--success);border-color:rgba(34,197,94,.3);}
.mini-hist{display:flex;align-items:flex-end;gap:1px;height:16px;}
.mini-bar{width:2px;border-radius:1px 1px 0 0;background:var(--accent);}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;margin-top:12px;}
.info-box{background:rgba(0,212,255,.04);border:1px solid rgba(0,212,255,.15);border-radius:6px;padding:12px;font-size:11px;line-height:1.6;color:var(--muted);margin-bottom:14px;}
.info-box strong{color:var(--accent);}
@media(max-width:700px){.hero{grid-template-columns:1fr 1fr}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/network-topology">Topology</a>
    <a href="/mesh" class="active">Mesh</a>
    <a href="/load-balance">Load Balance</a>
  </nav>
</div>
<div class="container">
  <h1>802.11s Mesh Detector</h1>
  <p class="subtitle">Discovers and maps 802.11s mesh networks. Identifies mesh nodes, peering state, HWMP paths, root nodes, and mesh gates connecting to external networks.</p>

  <div class="info-box">
    <strong>How it works:</strong> Passively captures 802.11s mesh beacons, peering action frames, and HWMP (Hybrid Wireless Mesh Protocol) path discovery packets.
    <strong>Root</strong> = mesh root announced via RANN/Gate; <strong>Gate</strong> = external network portal;
    <strong>Metric</strong> = 802.11s airtime link metric (lower is better).
  </div>

  <div class="hero">
    <div class="stat"><label>Mesh Nodes</label><div class="val" id="h-nodes">0</div></div>
    <div class="stat"><label>Active Links</label><div class="val" id="h-links">0</div></div>
    <div class="stat"><label>Established</label><div class="val" id="h-estab" style="color:var(--success)">0</div></div>
    <div class="stat"><label>Roots/Gates</label><div class="val" id="h-roots">0</div></div>
  </div>

  <div class="section">
    <h3>Mesh Topology (Canvas)</h3>
    <canvas id="mesh-canvas" height="260" style="width:100%"></canvas>
  </div>

  <div class="section">
    <h3>Mesh Nodes</h3>
    <table>
      <thead>
        <tr>
          <th>Address</th>
          <th>Mesh ID</th>
          <th>CH</th>
          <th>RSSI</th>
          <th>Role</th>
          <th>Peer State</th>
          <th>Peers</th>
          <th>Caps</th>
          <th>Activity</th>
        </tr>
      </thead>
      <tbody id="node-tbody">
        <tr><td colspan="9" style="color:var(--muted);text-align:center">No mesh nodes detected.</td></tr>
      </tbody>
    </table>
  </div>

  <div class="section">
    <h3>Mesh Links</h3>
    <table>
      <thead>
        <tr>
          <th>Node A</th>
          <th>Node B</th>
          <th>State</th>
          <th>Metric</th>
          <th>RSSI</th>
        </tr>
      </thead>
      <tbody id="link-tbody">
        <tr><td colspan="5" style="color:var(--muted);text-align:center">No links.</td></tr>
      </tbody>
    </table>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>
</div>

<script>
let g_data = {nodes:[], links:[], node_count:0, link_count:0};

function miniHist(hist) {
  if (!hist || !hist.length) return '';
  const max = Math.max(...hist, 1);
  return '<div class="mini-hist">' + hist.slice(-20).map(v => {
    const h = Math.round((v/max)*14)+2;
    return `<div class="mini-bar" style="height:${h}px"></div>`;
  }).join('') + '</div>';
}

function peerBadge(state) {
  if (state === 'Established') return '<span class="badge badge-estab">Established</span>';
  if (state.includes('Open'))  return '<span class="badge badge-open">' + state + '</span>';
  return '<span class="badge badge-idle">' + state + '</span>';
}

function rssiColor(r) {
  return r >= -60 ? 'var(--success)' : r >= -75 ? 'var(--warn)' : 'var(--danger)';
}

function drawTopology(nodes, links) {
  const canvas = document.getElementById('mesh-canvas');
  const W = canvas.width = canvas.offsetWidth;
  const H = canvas.height = 260;
  const ctx = canvas.getContext('2d');
  ctx.clearRect(0,0,W,H);

  if (!nodes.length) {
    ctx.fillStyle = '#64748b';
    ctx.font = '12px system-ui';
    ctx.textAlign = 'center';
    ctx.fillText('No mesh nodes detected', W/2, H/2);
    return;
  }

  // Position nodes in a circle
  const positions = {};
  nodes.forEach((n, i) => {
    const angle = (2 * Math.PI * i / nodes.length) - Math.PI/2;
    const r = Math.min(W, H) * 0.35;
    positions[n.addr] = {
      x: W/2 + r * Math.cos(angle),
      y: H/2 + r * Math.sin(angle)
    };
  });

  // Draw links
  ctx.strokeStyle = '#2a3045';
  ctx.lineWidth = 1.5;
  links.forEach(l => {
    const pa = positions[l.a], pb = positions[l.b];
    if (!pa || !pb) return;
    ctx.beginPath();
    ctx.moveTo(pa.x, pa.y);
    ctx.lineTo(pb.x, pb.y);
    if (l.state === 'Established') ctx.strokeStyle = '#22c55e44';
    else ctx.strokeStyle = '#2a3045';
    ctx.stroke();
    if (l.metric) {
      const mx = (pa.x + pb.x) / 2, my = (pa.y + pb.y) / 2;
      ctx.fillStyle = '#64748b';
      ctx.font = '8px system-ui';
      ctx.textAlign = 'center';
      ctx.fillText(l.metric, mx, my);
    }
  });

  // Draw nodes
  nodes.forEach(n => {
    const p = positions[n.addr];
    if (!p) return;
    const color = n.is_root ? '#00d4ff' : n.is_gate ? '#22c55e' : '#a855f7';
    const r = n.is_root ? 10 : 7;
    ctx.beginPath();
    ctx.arc(p.x, p.y, r, 0, 2*Math.PI);
    ctx.fillStyle = color;
    ctx.fill();
    ctx.strokeStyle = '#0a0c10';
    ctx.lineWidth = 2;
    ctx.stroke();
    ctx.fillStyle = '#e2e8f0';
    ctx.font = '8px system-ui';
    ctx.textAlign = 'center';
    const label = n.name || n.addr.slice(-5);
    ctx.fillText(label, p.x, p.y + r + 10);
  });
}

function renderTable() {
  document.getElementById('node-tbody').innerHTML = g_data.nodes.length
    ? g_data.nodes.map(n => `<tr>
        <td><code style="font-size:9px">${n.addr}</code></td>
        <td style="color:var(--accent)">${n.mesh_id || '<em style="color:var(--muted)">Unknown</em>'}</td>
        <td>${n.ch}</td>
        <td style="color:${rssiColor(n.rssi)}">${n.rssi}</td>
        <td>${n.is_root ? '<span class="badge badge-root">Root</span>' : ''}
            ${n.is_gate ? '<span class="badge badge-gate">Gate</span>' : ''}</td>
        <td>${peerBadge(n.peer_state)}</td>
        <td style="color:var(--muted)">${n.num_peerings}</td>
        <td>
          <span class="cap-pill ${n.forwarding?'cap-on':''}">FWD</span>
        </td>
        <td>${miniHist(n.hist)}</td>
      </tr>`).join('')
    : '<tr><td colspan="9" style="color:var(--muted);text-align:center">No mesh nodes.</td></tr>';

  document.getElementById('link-tbody').innerHTML = g_data.links.length
    ? g_data.links.map(l => `<tr>
        <td><code style="font-size:9px">${l.a}</code></td>
        <td><code style="font-size:9px">${l.b}</code></td>
        <td>${l.state === 'Established' ? '<span class="badge badge-estab">Established</span>' : l.state}</td>
        <td style="color:var(--muted)">${l.metric || '—'}</td>
        <td style="color:${rssiColor(l.rssi)}">${l.rssi}</td>
      </tr>`).join('')
    : '<tr><td colspan="5" style="color:var(--muted);text-align:center">No links.</td></tr>';
}

async function refresh() {
  const r = await fetch('/api/msh/all').then(x => x.json()).catch(() => g_data);
  g_data = r;
  document.getElementById('h-nodes').textContent = r.node_count || 0;
  document.getElementById('h-links').textContent = r.link_count || 0;
  document.getElementById('h-estab').textContent = (r.links||[]).filter(l => l.state === 'Established').length;
  document.getElementById('h-roots').textContent = (r.nodes||[]).filter(n => n.is_root || n.is_gate).length;
  drawTopology(r.nodes||[], r.links||[]);
  renderTable();
}

async function clearData() {
  await fetch('/api/msh/clear', {method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 4000);
window.addEventListener('resize', () => drawTopology(g_data.nodes||[], g_data.links||[]));
</script>
</body>
</html>
)rawhtml";

inline void wumsh_routes() {
  server.on("/mesh", HTTP_GET, []() {
    server.send_P(200, "text/html", WUMSH_PAGE);
  });
}

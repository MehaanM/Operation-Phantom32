#pragma once
// webui_topology.h — Network topology visualization UI.
// Route: /topology. Prefix: wunto_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUNTO_PAGE[] PROGMEM = R"rawhtml(
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
.hero{display:grid;grid-template-columns:repeat(4,1fr);gap:10px;margin-bottom:16px;}
.stat{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:13px;}
.stat label{font-size:10px;color:var(--muted);text-transform:uppercase;}
.stat .val{font-size:22px;font-weight:800;color:var(--accent);margin-top:3px;}
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:18px;margin-bottom:14px;}
.section h3{font-size:12px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:14px;}
#topo-canvas{width:100%;height:450px;border-radius:6px;background:var(--bg3);cursor:grab;display:block;}
#topo-canvas:active{cursor:grabbing;}
.legend{display:flex;gap:16px;flex-wrap:wrap;margin-top:12px;font-size:11px;}
.leg-item{display:flex;align-items:center;gap:5px;}
.leg-dot{width:10px;height:10px;border-radius:50%;}
.tabs{display:flex;gap:4px;margin-bottom:14px;}
.tab{padding:5px 12px;border-radius:5px;font-size:12px;cursor:pointer;border:1px solid var(--border);color:var(--muted);}
.tab.active{background:rgba(0,212,255,.12);color:var(--accent);border-color:var(--accent);}
table{width:100%;border-collapse:collapse;font-size:12px;}
th{text-align:left;padding:7px 8px;font-size:10px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;}
td{padding:7px 8px;border-bottom:1px solid var(--border);}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
.badge{padding:2px 7px;border-radius:4px;font-size:9px;font-weight:700;display:inline-block;}
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
    <a href="/network-map">Map</a>
    <a href="/topology" class="active">Topology</a>
    <a href="/clients-ext">Clients</a>
    <a href="/analytics">Analytics</a>
  </nav>
</div>
<div class="container">
  <h1>Network Topology</h1>
  <p class="subtitle">Live association graph. APs shown as larger circles; associated clients orbit around them. Click an AP to filter its clients.</p>

  <div class="hero">
    <div class="stat"><label>APs</label><div class="val" id="h-aps">0</div></div>
    <div class="stat"><label>Clients</label><div class="val" id="h-cl">0</div></div>
    <div class="stat"><label>Links</label><div class="val" id="h-links">0</div></div>
    <div class="stat"><label>Associations</label><div class="val" id="h-assocs">0</div></div>
  </div>

  <div class="section">
    <h3>Topology Graph</h3>
    <canvas id="topo-canvas"></canvas>
    <div class="legend">
      <div class="leg-item"><div class="leg-dot" style="background:#00d4ff"></div>Access Point</div>
      <div class="leg-item"><div class="leg-dot" style="background:#22c55e"></div>Associated Client</div>
      <div class="leg-item"><div class="leg-dot" style="background:#f59e0b"></div>Probe Only</div>
      <div class="leg-item"><div class="leg-dot" style="background:#64748b"></div>Unassociated</div>
    </div>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-ghost" onclick="resetView()">Reset View</button>
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>

  <div class="section">
    <div class="tabs">
      <div class="tab active" onclick="showTab('aps',this)">Access Points</div>
      <div class="tab" onclick="showTab('clients',this)">Clients</div>
      <div class="tab" onclick="showTab('links',this)">Links</div>
    </div>
    <div id="tab-aps">
      <table><thead><tr>
        <th>BSSID</th><th>SSID</th><th>CH</th><th>RSSI</th><th>Clients</th>
      </tr></thead>
      <tbody id="ap-tbody"></tbody></table>
    </div>
    <div id="tab-clients" style="display:none">
      <table><thead><tr>
        <th>MAC</th><th>AP</th><th>SSID</th><th>CH</th><th>RSSI</th><th>Frames</th>
      </tr></thead>
      <tbody id="cl-tbody"></tbody></table>
    </div>
    <div id="tab-links" style="display:none">
      <table><thead><tr>
        <th>AP</th><th>Client</th><th>State</th><th>Frames</th><th>RSSI</th>
      </tr></thead>
      <tbody id="lk-tbody"></tbody></table>
    </div>
  </div>
</div>

<script>
let g_data = {aps:[], clients:[], links:[]};
let g_sel_ap = null;
let pan = {x:0,y:0,dragging:false,sx:0,sy:0};
const LINK_STATES = ['Assoc','Probe','Data'];
const LINK_COLORS = ['#22c55e','#f59e0b','#00d4ff'];

function showTab(name, el) {
  document.querySelectorAll('.tab').forEach(t=>t.classList.remove('active'));
  el.classList.add('active');
  document.getElementById('tab-aps').style.display     = name==='aps'?'':'none';
  document.getElementById('tab-clients').style.display = name==='clients'?'':'none';
  document.getElementById('tab-links').style.display   = name==='links'?'':'none';
}

function drawGraph() {
  const c = document.getElementById('topo-canvas');
  c.width  = c.offsetWidth;
  c.height = 450;
  const ctx = c.getContext('2d');
  ctx.clearRect(0, 0, c.width, c.height);

  const aps = g_data.aps || [];
  const cls = g_data.clients || [];
  const lks = g_data.links || [];
  if (!aps.length) {
    ctx.fillStyle = '#64748b';
    ctx.font = '14px system-ui';
    ctx.textAlign = 'center';
    ctx.fillText('No network data yet. Waiting for beacons and associations…', c.width/2, c.height/2);
    return;
  }

  const W = c.width + pan.x;
  const H = c.height + pan.y;
  const cx = W/2, cy = H/2;

  // Layout APs in a circle
  const ap_positions = {};
  const r_ap = Math.min(c.width, c.height) * 0.3;
  aps.forEach((ap, i) => {
    const angle = (i / aps.length) * Math.PI * 2 - Math.PI/2;
    ap_positions[ap.bssid] = {
      x: cx + Math.cos(angle) * r_ap,
      y: cy + Math.sin(angle) * r_ap,
    };
  });

  // Layout clients around their AP
  const cl_counts = {};
  cls.forEach(cl => {
    if (!cl_counts[cl.ap]) cl_counts[cl.ap] = 0;
    cl_counts[cl.ap]++;
  });
  const cl_positions = {};
  const cl_idxs = {};
  cls.forEach(cl => {
    if (!cl_idxs[cl.ap]) cl_idxs[cl.ap] = 0;
    const ap_pos = ap_positions[cl.ap];
    if (ap_pos) {
      const cnt   = cl_counts[cl.ap] || 1;
      const idx   = cl_idxs[cl.ap]++;
      const angle = (idx / cnt) * Math.PI * 2;
      const r     = 55 + Math.min(cnt * 5, 30);
      cl_positions[cl.mac] = {
        x: ap_pos.x + Math.cos(angle) * r,
        y: ap_pos.y + Math.sin(angle) * r,
      };
    } else {
      cl_positions[cl.mac] = {x: cx + (Math.random()-0.5)*200, y: cy + (Math.random()-0.5)*200};
    }
  });

  // Draw links
  lks.forEach(lk => {
    const ap_p = ap_positions[lk.ap];
    const cl_p = cl_positions[lk.client];
    if (!ap_p || !cl_p) return;
    ctx.beginPath();
    ctx.moveTo(ap_p.x, ap_p.y);
    ctx.lineTo(cl_p.x, cl_p.y);
    ctx.strokeStyle = LINK_COLORS[lk.state] || '#64748b';
    ctx.globalAlpha = 0.35;
    ctx.lineWidth = 1.5;
    ctx.stroke();
    ctx.globalAlpha = 1;
  });

  // Draw APs
  aps.forEach(ap => {
    const p = ap_positions[ap.bssid];
    if (!p) return;
    const hl = g_sel_ap === ap.bssid;
    ctx.beginPath();
    ctx.arc(p.x, p.y, hl ? 18 : 14, 0, Math.PI*2);
    ctx.fillStyle = hl ? '#00d4ff' : 'rgba(0,212,255,0.75)';
    ctx.fill();
    ctx.strokeStyle = '#fff';
    ctx.lineWidth = 1.5;
    ctx.stroke();
    ctx.fillStyle = '#fff';
    ctx.font = 'bold 8px system-ui';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.fillText('AP', p.x, p.y);
    // Label
    ctx.fillStyle = '#e2e8f0';
    ctx.font = '9px system-ui';
    ctx.fillText(ap.ssid||ap.bssid.substring(0,8), p.x, p.y+22);
    ctx.fillStyle = '#64748b';
    ctx.font = '8px system-ui';
    ctx.fillText('ch'+ap.ch, p.x, p.y+32);
  });

  // Draw clients
  cls.forEach(cl => {
    const p = cl_positions[cl.mac];
    if (!p) return;
    ctx.beginPath();
    ctx.arc(p.x, p.y, 7, 0, Math.PI*2);
    ctx.fillStyle = cl.has_assoc ? 'rgba(34,197,94,0.8)' : 'rgba(100,116,139,0.6)';
    ctx.fill();
    ctx.strokeStyle = '#fff';
    ctx.lineWidth = 1;
    ctx.stroke();
  });
}

async function refresh() {
  const r = await fetch('/api/nto/topology').then(x=>x.json()).catch(()=>({}));
  g_data = r;
  document.getElementById('h-aps').textContent    = r.ap_count||0;
  document.getElementById('h-cl').textContent     = r.client_count||0;
  document.getElementById('h-links').textContent  = r.link_count||0;
  document.getElementById('h-assocs').textContent = r.total_assocs||0;
  drawGraph();

  const aps = r.aps||[];
  document.getElementById('ap-tbody').innerHTML = aps.length
    ? aps.map(a=>`<tr>
        <td><code style="font-size:10px">${a.bssid}</code></td>
        <td>${a.ssid||'—'}</td><td>${a.ch}</td><td>${a.rssi}dBm</td><td>${a.clients}</td>
      </tr>`).join('')
    : '<tr><td colspan="5" style="color:var(--muted);text-align:center">No APs.</td></tr>';

  const cls = r.clients||[];
  document.getElementById('cl-tbody').innerHTML = cls.length
    ? cls.map(c=>`<tr>
        <td><code style="font-size:10px">${c.mac}</code></td>
        <td><code style="font-size:10px">${c.ap}</code></td>
        <td>${c.ssid||'—'}</td><td>${c.ch}</td><td>${c.rssi}dBm</td><td>${c.frames}</td>
      </tr>`).join('')
    : '<tr><td colspan="6" style="color:var(--muted);text-align:center">No clients.</td></tr>';

  const lks = r.links||[];
  document.getElementById('lk-tbody').innerHTML = lks.length
    ? lks.map(l=>`<tr>
        <td><code style="font-size:10px">${l.ap}</code></td>
        <td><code style="font-size:10px">${l.client}</code></td>
        <td><span class="badge" style="background:rgba(34,197,94,.12);color:${LINK_COLORS[l.state]}">${LINK_STATES[l.state]}</span></td>
        <td>${l.frames}</td><td>${l.rssi}dBm</td>
      </tr>`).join('')
    : '<tr><td colspan="5" style="color:var(--muted);text-align:center">No links.</td></tr>';
}

function resetView() { pan.x=0; pan.y=0; drawGraph(); }

async function clearData() {
  await fetch('/api/nto/clear',{method:'POST'});
  g_sel_ap = null;
  refresh();
}

window.addEventListener('resize', drawGraph);
refresh();
setInterval(refresh, 5000);
</script>
</body>
</html>
)rawhtml";

inline void wunto_routes() {
  server.on("/topology", HTTP_GET, []() { server.send_P(200, "text/html", WUNTO_PAGE); });
}

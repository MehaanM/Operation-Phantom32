#pragma once
// webui_bss_color.h — BSS Color tracker UI.
// Route: /bss-color. Prefix: wubsc_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUBSC_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>BSS Color — Phantom32</title>
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
.hero{display:grid;grid-template-columns:repeat(5,1fr);gap:10px;margin-bottom:16px;}
.stat{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:13px;}
.stat label{font-size:10px;color:var(--muted);text-transform:uppercase;}
.stat .val{font-size:22px;font-weight:800;color:var(--accent);margin-top:3px;}
.two-col{display:grid;grid-template-columns:1fr 1fr;gap:14px;margin-bottom:14px;}
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:18px;margin-bottom:14px;}
.section h3{font-size:12px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:14px;}
table{width:100%;border-collapse:collapse;font-size:10px;}
th{text-align:left;padding:8px 10px;border-bottom:2px solid var(--border);color:var(--muted);font-weight:600;font-size:9px;text-transform:uppercase;cursor:pointer;}
td{padding:7px 10px;border-bottom:1px solid var(--border);}
tr:hover td{background:var(--bg3);}
.color-swatch{display:inline-flex;align-items:center;justify-content:center;width:28px;height:20px;border-radius:4px;font-size:9px;font-weight:700;color:#000;}
.state-pill{padding:2px 6px;border-radius:4px;font-size:9px;font-weight:700;}
.s-Normal{background:rgba(34,197,94,.12);color:var(--success);}
.s-Collision{background:rgba(239,68,68,.12);color:var(--danger);}
.s-Changing{background:rgba(245,158,11,.12);color:var(--warn);}
.s-Disabled{background:rgba(100,116,139,.1);color:var(--muted);}
.color-usage-row{display:flex;flex-wrap:wrap;gap:4px;}
.cu-chip{display:inline-flex;align-items:center;gap:4px;padding:3px 7px;border-radius:4px;font-size:9px;}
.cu-collision{border:1px solid var(--danger);background:rgba(239,68,68,.08);}
.cu-normal{border:1px solid var(--border);background:var(--bg3);}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;margin-top:12px;}
@media(max-width:700px){.hero{grid-template-columns:1fr 1fr};.two-col{grid-template-columns:1fr}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/bss-color" class="active">BSS Color</a>
    <a href="/tsf">TSF</a>
    <a href="/timing">Timing</a>
  </nav>
</div>
<div class="container">
  <h1>BSS Color Tracker</h1>
  <p class="subtitle">Monitors 802.11ax BSS Color assignments for Wi-Fi 6 networks. Detects color collisions between nearby APs and tracks OBSS/PD spatial reuse parameters.</p>

  <div class="hero">
    <div class="stat"><label>HE APs</label><div class="val" id="h-he">0</div></div>
    <div class="stat"><label>Colors Used</label><div class="val" id="h-col" style="color:var(--accent)">0</div></div>
    <div class="stat"><label>Collisions</label><div class="val" id="h-coll" style="color:var(--danger)">0</div></div>
    <div class="stat"><label>Color Changes</label><div class="val" id="h-chg" style="color:var(--warn)">0</div></div>
    <div class="stat"><label>Disabled</label><div class="val" id="h-dis" style="color:var(--muted)">0</div></div>
  </div>

  <div class="section">
    <h3>Color Assignment Map</h3>
    <div class="color-usage-row" id="color-map">
      <span style="color:var(--muted)">No color data.</span>
    </div>
  </div>

  <div class="two-col">
    <div class="section">
      <h3>AP Color Table</h3>
      <div style="overflow-x:auto">
      <table>
        <thead><tr>
          <th>BSSID</th><th>SSID</th><th>CH</th>
          <th>Color</th><th>State</th>
          <th>OBSS/PD</th><th>Conflicts</th>
        </tr></thead>
        <tbody id="ap-body"></tbody>
      </table>
      </div>
      <div class="action-row">
        <button class="btn btn-ghost" onclick="clearData()">Clear</button>
      </div>
    </div>

    <div class="section">
      <h3>Color Collision Details</h3>
      <div id="collision-list">
        <div style="color:var(--muted)">No collisions detected.</div>
      </div>
    </div>
  </div>
</div>

<script>
function colorPalette(c) {
  const hue = (c * 137.5) % 360;
  return `hsl(${hue},70%,55%)`;
}

function stateClass(s) { return 's-'+s; }

async function refresh() {
  const r = await fetch('/api/bsc/all').then(x=>x.json()).catch(()=>({}));
  const aps = r.aps || [];
  const colorMap = r.color_map || {};

  document.getElementById('h-he').textContent   = aps.filter(a=>a.he).length;
  document.getElementById('h-col').textContent  = Object.keys(colorMap).length;
  document.getElementById('h-coll').textContent = r.total_collisions || 0;
  document.getElementById('h-chg').textContent  = r.total_changes || 0;
  document.getElementById('h-dis').textContent  = aps.filter(a=>a.disabled).length;

  // Color assignment map
  const colorEntries = Object.entries(colorMap).sort((a,b)=>Number(a[0])-Number(b[0]));
  document.getElementById('color-map').innerHTML = colorEntries.length
    ? colorEntries.map(([c, cnt]) => {
        const isCollision = cnt > 1;
        const bg = colorPalette(Number(c));
        return `<div class="cu-chip ${isCollision?'cu-collision':'cu-normal'}">
          <div class="color-swatch" style="background:${bg}">${c}</div>
          <span>${cnt} AP${cnt>1?'s':''}</span>
          ${isCollision?'<span style="color:var(--danger)">✗</span>':''}
        </div>`;
      }).join('')
    : '<span style="color:var(--muted)">No color data yet.</span>';

  // AP table
  document.getElementById('ap-body').innerHTML = aps.length
    ? aps.map(a => `<tr>
        <td style="font-family:monospace;font-size:9px">${a.bssid}</td>
        <td>${a.ssid||'Hidden'}</td>
        <td>${a.ch}</td>
        <td>
          ${a.color > 0
            ? `<div class="color-swatch" style="background:${colorPalette(a.color)}">${a.color}</div>`
            : '<span style="color:var(--muted)">—</span>'}
        </td>
        <td><span class="state-pill ${stateClass(a.state)}">${a.state}</span></td>
        <td style="font-size:9px;color:var(--muted)">${a.obss_pd_min}/${a.obss_pd_max} dBm</td>
        <td style="color:${a.conflicts>0?'var(--danger)':'var(--muted)'}">${a.conflicts}</td>
      </tr>`).join('')
    : `<tr><td colspan="7" style="text-align:center;color:var(--muted);padding:16px">No Wi-Fi 6 APs found.</td></tr>`;

  // Collision details
  const collisions = aps.filter(a => a.state==='Collision' && a.conflict_list);
  document.getElementById('collision-list').innerHTML = collisions.length
    ? collisions.map(a => `<div style="margin-bottom:8px;padding:8px;background:var(--bg3);border-radius:5px;border-left:2px solid var(--danger)">
        <div style="font-size:10px;font-weight:700;color:var(--danger)">${a.ssid||a.bssid}</div>
        <div style="font-size:9px;color:var(--muted)">Color ${a.color} conflicts with:</div>
        ${a.conflict_list.map(c=>`<div style="font-size:9px;padding:2px 0;color:var(--warn)">&nbsp;↳ ${c.ssid||c.bssid}</div>`).join('')}
      </div>`).join('')
    : '<div style="color:var(--muted)">No collisions detected.</div>';
}

async function clearData() {
  await fetch('/api/bsc/clear', {method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 4000);
</script>
</body>
</html>
)rawhtml";

inline void wubsc_routes() {
  server.on("/bss-color", HTTP_GET, []() {
    server.send_P(200, "text/html", WUBSC_PAGE);
  });
}

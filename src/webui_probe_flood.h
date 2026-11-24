#pragma once
// webui_probe_flood.h — Probe flood / scanner detector UI.
// Route: /probe-flood. Prefix: wupfd_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUPFD_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Probe Flood — Phantom32</title>
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
.two-col{display:grid;grid-template-columns:1.4fr 1fr;gap:14px;margin-bottom:14px;}
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:18px;margin-bottom:14px;}
.section h3{font-size:12px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:14px;}
table{width:100%;border-collapse:collapse;font-size:10px;}
th{text-align:left;padding:8px 10px;border-bottom:2px solid var(--border);color:var(--muted);font-weight:600;font-size:9px;text-transform:uppercase;cursor:pointer;}
td{padding:7px 10px;border-bottom:1px solid var(--border);}
tr:hover td{background:var(--bg3);}
.type-pill{padding:1px 6px;border-radius:3px;font-size:9px;font-weight:700;}
.t-Flood{background:rgba(239,68,68,.2);color:var(--danger);}
.t-Sweep{background:rgba(245,158,11,.12);color:var(--warn);}
.t-Directed{background:rgba(0,212,255,.08);color:var(--accent);}
.t-Active{background:rgba(100,116,139,.1);color:var(--muted);}
.t-Scanner{background:rgba(239,68,68,.12);color:var(--danger);}
.t-Passive{background:rgba(34,197,94,.08);color:var(--success);}
.t-Unknown{background:rgba(100,116,139,.1);color:var(--muted);}
.rand-badge{display:inline-block;padding:1px 5px;border-radius:3px;font-size:7px;font-weight:700;background:rgba(139,92,246,.12);color:#a78bfa;}
.mini-chart{display:flex;align-items:flex-end;gap:1px;height:12px;}
.mini-bar{flex:1;border-radius:1px 1px 0 0;min-height:1px;}
.ev-row{padding:4px 8px;border-bottom:1px solid var(--border);font-size:9px;display:flex;gap:6px;align-items:center;flex-wrap:wrap;}
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
    <a href="/probe-flood" class="active">Probe Flood</a>
    <a href="/timing">Timing</a>
    <a href="/stations">Stations</a>
  </nav>
</div>
<div class="container">
  <h1>Probe Flood &amp; Scanner Detector</h1>
  <p class="subtitle">Detects MAC randomization, probe floods (&gt;20/sec), channel sweeps, and directed-network scanners. Tracks per-device probe patterns and target SSID lists.</p>

  <div class="hero">
    <div class="stat"><label>Scanners</label><div class="val" id="h-cnt">0</div></div>
    <div class="stat"><label>Total Probes</label><div class="val" id="h-tot">0</div></div>
    <div class="stat"><label>Flood Events</label><div class="val" id="h-fl" style="color:var(--danger)">0</div></div>
    <div class="stat"><label>Sweep Events</label><div class="val" id="h-sw" style="color:var(--warn)">0</div></div>
  </div>

  <div class="two-col">
    <div class="section">
      <h3>Detected Scanners</h3>
      <div style="overflow-x:auto">
      <table>
        <thead><tr>
          <th onclick="sortBy('mac')">MAC</th>
          <th onclick="sortBy('type')">Type</th>
          <th onclick="sortBy('total')">Probes</th>
          <th onclick="sortBy('rate')">Rate/s</th>
          <th onclick="sortBy('peak')">Peak</th>
          <th onclick="sortBy('directed')">Directed</th>
          <th>Target</th>
          <th>CH Gap</th>
          <th>Hist</th>
        </tr></thead>
        <tbody id="tbl-body"></tbody>
      </table>
      </div>
      <div class="action-row">
        <button class="btn btn-ghost" onclick="clearData()">Clear</button>
      </div>
    </div>

    <div class="section">
      <h3>Recent Events</h3>
      <div id="ev-log">
        <div style="color:var(--muted)">No events yet.</div>
      </div>
    </div>
  </div>
</div>

<script>
let g_data = [];
let g_sort = {key:'total', asc:false};

function sortBy(k) {
  if (g_sort.key===k) g_sort.asc = !g_sort.asc;
  else {g_sort.key=k; g_sort.asc=false;}
  renderTable();
}

const TYPE_COLORS = {
  Flood:'var(--danger)',Sweep:'var(--warn)',Directed:'var(--accent)',
  Active:'var(--muted)',Scanner:'var(--danger)',Passive:'var(--success)',Unknown:'var(--muted)'
};

function miniChart(hist) {
  if (!hist || !hist.length) return '';
  const max = Math.max(...hist, 1);
  return hist.map(v => {
    const h = Math.max(1, Math.round((v/max)*10));
    const c = v/max > .7 ? 'var(--danger)' : v/max > .4 ? 'var(--warn)' : 'var(--accent)';
    return `<div class="mini-bar" style="height:${h}px;background:${c}"></div>`;
  }).join('');
}

function renderTable() {
  const rows = [...g_data];
  const k = g_sort.key, asc = g_sort.asc;
  rows.sort((a,b)=>{
    const va = typeof a[k]==='string'?a[k]:Number(a[k]||0);
    const vb = typeof b[k]==='string'?b[k]:Number(b[k]||0);
    return asc?(va>vb?1:-1):(vb>va?1:-1);
  });
  document.getElementById('tbl-body').innerHTML = rows.length
    ? rows.map(s => `<tr>
        <td style="font-family:monospace;font-size:9px">${s.mac}${s.rand?'<span class="rand-badge">RAND</span>':''}</td>
        <td><span class="type-pill t-${s.type}">${s.type}</span></td>
        <td>${s.total.toLocaleString()}</td>
        <td style="color:${s.rate>=20?'var(--danger)':s.rate>=5?'var(--warn)':'var(--text)'}">${s.rate}/s</td>
        <td style="color:var(--muted)">${s.peak}</td>
        <td>${s.directed}</td>
        <td style="font-size:9px;max-width:80px;overflow:hidden;text-overflow:ellipsis">${s.top_target||'—'}</td>
        <td style="color:var(--muted)">${s.min_ch_gap===65535?'—':s.min_ch_gap+'ms'}</td>
        <td><div class="mini-chart">${miniChart(s.rate_hist)}</div></td>
      </tr>`).join('')
    : `<tr><td colspan="9" style="text-align:center;color:var(--muted);padding:16px">No scanners detected.</td></tr>`;
}

async function refresh() {
  const r = await fetch('/api/pfd/all').then(x=>x.json()).catch(()=>({}));
  document.getElementById('h-cnt').textContent = r.count || 0;
  document.getElementById('h-tot').textContent = (r.total_probes||0).toLocaleString();
  document.getElementById('h-fl').textContent  = r.total_flood || 0;
  document.getElementById('h-sw').textContent  = r.total_sweep || 0;
  g_data = r.scanners || [];
  renderTable();

  const evts = r.events || [];
  document.getElementById('ev-log').innerHTML = evts.length
    ? evts.map(e => `<div class="ev-row">
        <span class="type-pill t-${e.type}">${e.type}</span>
        <span style="font-family:monospace;font-size:9px">${e.mac}</span>
        <span style="color:var(--muted)">CH${e.ch}</span>
        ${e.ssid ? `<span style="color:var(--accent);font-size:9px">${e.ssid}</span>` : ''}
        ${e.rate>0 ? `<span style="color:var(--warn)">${e.rate}/s</span>` : ''}
        <span style="margin-left:auto;color:var(--muted)">${e.ts}s</span>
      </div>`).join('')
    : '<div style="color:var(--muted);padding:8px">No events.</div>';
}

async function clearData() {
  await fetch('/api/pfd/clear', {method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 2000);
</script>
</body>
</html>
)rawhtml";

inline void wupfd_routes() {
  server.on("/probe-flood", HTTP_GET, []() {
    server.send_P(200, "text/html", WUPFD_PAGE);
  });
}

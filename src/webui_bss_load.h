#pragma once
// webui_bss_load.h — BSS Load history UI.
// Route: /bss-load. Prefix: wubsl_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUBSL_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>BSS Load — Phantom32</title>
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
.two-col{display:grid;grid-template-columns:1fr 1fr;gap:14px;margin-bottom:14px;}
table{width:100%;border-collapse:collapse;font-size:10px;}
th{text-align:left;padding:8px 10px;border-bottom:2px solid var(--border);color:var(--muted);font-weight:600;font-size:9px;text-transform:uppercase;}
td{padding:7px 10px;border-bottom:1px solid var(--border);}
tr:hover td{background:var(--bg3);}
.util-bar{display:inline-block;height:8px;border-radius:4px;vertical-align:middle;}
.state-pill{padding:2px 6px;border-radius:3px;font-size:9px;font-weight:700;}
.s-Idle{background:rgba(100,116,139,.1);color:var(--muted);}
.s-Light{background:rgba(34,197,94,.1);color:var(--success);}
.s-Moderate{background:rgba(245,158,11,.1);color:var(--warn);}
.s-Heavy{background:rgba(239,68,68,.1);color:var(--danger);}
.hist-mini{display:flex;align-items:flex-end;gap:1px;height:24px;}
.hist-bar{width:5px;border-radius:1px 1px 0 0;}
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
    <a href="/bss-load" class="active">BSS Load</a>
    <a href="/signal-heatmap">Signal Map</a>
    <a href="/scan-cache">Scan Cache</a>
  </nav>
</div>
<div class="container">
  <h1>BSS Load History</h1>
  <p class="subtitle">Monitors 802.11e BSS Load IE from AP beacons: channel utilization (%), connected stations, and available admission capacity. Detects congested channels and correlates AP load over time.</p>

  <div class="hero">
    <div class="stat"><label>APs w/ BSS Load</label><div class="val" id="h-cnt">0</div></div>
    <div class="stat"><label>Heavy Events</label><div class="val" id="h-heavy" style="color:var(--danger)">0</div></div>
    <div class="stat"><label>Busiest Channel</label><div class="val" id="h-bch" style="color:var(--warn)">—</div></div>
    <div class="stat"><label>Busiest Util</label><div class="val" id="h-but" style="color:var(--warn)">—%</div></div>
  </div>

  <div class="two-col">
    <div class="section">
      <h3>AP Load Table</h3>
      <div style="overflow-x:auto">
      <table>
        <thead><tr>
          <th>SSID</th><th>CH</th><th>Util</th><th>State</th>
          <th>STAs</th><th>Max Util</th><th>Heavy</th><th>History</th>
        </tr></thead>
        <tbody id="ap-body"></tbody>
      </table>
      </div>
      <div class="action-row">
        <button class="btn btn-ghost" onclick="clearData()">Clear</button>
      </div>
    </div>

    <div class="section">
      <h3>Heavy Congestion Events</h3>
      <div style="overflow-x:auto;max-height:280px;overflow-y:auto">
      <table>
        <thead><tr><th>Time</th><th>SSID</th><th>Util</th><th>STAs</th><th>State</th></tr></thead>
        <tbody id="ev-body"></tbody>
      </table>
      </div>
    </div>
  </div>
</div>

<script>
function reltime(ts) {
  const s = Math.floor(Date.now()/1000) - ts;
  if (s < 60)  return s + 's ago';
  if (s < 3600) return Math.floor(s/60) + 'm ago';
  return Math.floor(s/3600) + 'h ago';
}

function utilColor(u) {
  if (u < 25) return 'var(--muted)';
  if (u < 50) return 'var(--success)';
  if (u < 75) return 'var(--warn)';
  return 'var(--danger)';
}

function histMini(hist) {
  if (!hist || !hist.length) return '';
  const max = Math.max(...hist.map(h => h.u), 1);
  return `<div class="hist-mini">` +
    hist.map(h => `<div class="hist-bar" title="${h.u}% / ${h.s} STAs" style="height:${Math.max(2,(h.u/max)*24)}px;background:${utilColor(h.u)}"></div>`).join('') +
    `</div>`;
}

async function refresh() {
  const r = await fetch('/api/bsl/all').then(x=>x.json()).catch(()=>({}));
  const aps = r.aps || [];
  const events = r.events || [];

  document.getElementById('h-cnt').textContent   = r.ap_count || 0;
  document.getElementById('h-heavy').textContent = r.total_heavy || 0;
  document.getElementById('h-bch').textContent   = r.busiest_ch ? 'CH'+r.busiest_ch : '—';
  document.getElementById('h-but').textContent   = r.busiest_util != null ? r.busiest_util+'%' : '—%';

  document.getElementById('ap-body').innerHTML = aps.length
    ? aps.map(a => {
        const barW = Math.min(100, a.util);
        return `<tr>
          <td>${a.ssid||'Hidden'}</td>
          <td>CH${a.ch}</td>
          <td>
            <div style="display:flex;align-items:center;gap:5px">
              <span style="color:${utilColor(a.util)};font-weight:700">${a.util}%</span>
              <div style="flex:1;height:4px;background:var(--border);border-radius:2px;min-width:40px">
                <div style="width:${barW}%;height:100%;background:${utilColor(a.util)};border-radius:2px"></div>
              </div>
            </div>
          </td>
          <td><span class="state-pill s-${a.state}">${a.state}</span></td>
          <td>${a.sta}</td>
          <td style="color:${utilColor(a.util_max)}">${a.util_max}%</td>
          <td style="color:${a.heavy_events>0?'var(--danger)':'var(--muted)'}">${a.heavy_events}</td>
          <td>${histMini(a.hist)}</td>
        </tr>`;
      }).join('')
    : '<tr><td colspan="8" style="text-align:center;color:var(--muted);padding:16px">No BSS Load data.</td></tr>';

  document.getElementById('ev-body').innerHTML = events.length
    ? events.slice().reverse().map(ev => `<tr>
        <td style="color:var(--muted)">${reltime(ev.ts)}</td>
        <td>${ev.ssid||'—'}</td>
        <td style="color:${utilColor(ev.util)};font-weight:700">${ev.util}%</td>
        <td>${ev.sta}</td>
        <td><span class="state-pill s-${ev.state}">${ev.state}</span></td>
      </tr>`).join('')
    : '<tr><td colspan="5" style="text-align:center;color:var(--muted);padding:16px">No events.</td></tr>';
}

async function clearData() {
  await fetch('/api/bsl/clear', {method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 3000);
</script>
</body>
</html>
)rawhtml";

inline void wubsl_routes() {
  server.on("/bss-load", HTTP_GET, []() { server.send_P(200, "text/html", WUBSL_PAGE); });
}

#pragma once
// webui_proberesp.h — Probe request/response tracker UI.
// Route: /probe-response. Prefix: wuprt_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUPRT_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Probe Response Tracker — Phantom32</title>
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
.stat .val.red{color:var(--danger);}
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:18px;margin-bottom:14px;}
.section h3{font-size:12px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:14px;}
table{width:100%;border-collapse:collapse;font-size:12px;}
th{text-align:left;padding:7px 8px;font-size:10px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;}
td{padding:7px 8px;border-bottom:1px solid var(--border);}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
.karma-badge{background:rgba(239,68,68,.15);color:var(--danger);padding:1px 7px;border-radius:4px;font-size:10px;font-weight:700;}
.ok-badge{background:rgba(34,197,94,.12);color:var(--success);padding:1px 7px;border-radius:4px;font-size:10px;}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;margin-top:12px;}
@media(max-width:700px){.hero{grid-template-columns:repeat(2,1fr)}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/probes">Probes</a>
    <a href="/probe-response" class="active">Probe Correlator</a>
    <a href="/anomaly">Anomaly</a>
  </nav>
</div>
<div class="container">
  <h1>Probe Request/Response Correlator</h1>
  <p class="subtitle">Correlates probe requests from clients with probe responses from APs. Detects KARMA-style attacks where APs respond to all probes.</p>

  <div class="hero">
    <div class="stat"><label>Total Probes</label><div class="val" id="h-probes">0</div></div>
    <div class="stat"><label>Responses</label><div class="val" id="h-resp">0</div></div>
    <div class="stat"><label>Correlated</label><div class="val" id="h-corr">0</div></div>
    <div class="stat"><label>KARMA Suspects</label><div class="val red" id="h-karma">0</div></div>
  </div>

  <div class="section">
    <h3>AP Response Statistics</h3>
    <table>
      <thead><tr>
        <th>BSSID</th>
        <th>SSID</th>
        <th>Total Responses</th>
        <th>Wildcard</th>
        <th>Directed</th>
        <th>Avg Latency</th>
        <th>KARMA</th>
      </tr></thead>
      <tbody id="ap-tbody">
        <tr><td colspan="7" style="color:var(--muted);text-align:center">No data yet…</td></tr>
      </tbody>
    </table>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>

  <div class="section">
    <h3>Recent Correlations</h3>
    <table>
      <thead><tr>
        <th>Client MAC</th>
        <th>AP BSSID</th>
        <th>SSID</th>
        <th>CH</th>
        <th>Latency</th>
        <th>Time</th>
      </tr></thead>
      <tbody id="corr-tbody">
        <tr><td colspan="6" style="color:var(--muted);text-align:center">No correlations yet.</td></tr>
      </tbody>
    </table>
  </div>
</div>

<script>
async function refresh() {
  const [stats, corrs] = await Promise.all([
    fetch('/api/prt/stats').then(x=>x.json()).catch(()=>({})),
    fetch('/api/prt/correlations').then(x=>x.json()).catch(()=>[])
  ]);

  document.getElementById('h-probes').textContent = stats.total_probes||0;
  document.getElementById('h-resp').textContent   = stats.total_responses||0;
  document.getElementById('h-corr').textContent   = stats.total_correlations||0;
  document.getElementById('h-karma').textContent  = stats.karma_suspects||0;

  const aps = (stats.aps||[]).sort((a,b)=>b.responses-a.responses);
  document.getElementById('ap-tbody').innerHTML = aps.length
    ? aps.map(ap=>`<tr>
        <td><code style="font-size:10px">${ap.bssid}</code></td>
        <td>${ap.ssid||'—'}</td>
        <td>${ap.responses}</td>
        <td>${ap.wildcard}</td>
        <td>${ap.directed}</td>
        <td>${ap.avg_latency} ms</td>
        <td>${ap.karma?'<span class="karma-badge">KARMA</span>':'<span class="ok-badge">OK</span>'}</td>
      </tr>`).join('')
    : '<tr><td colspan="7" style="color:var(--muted);text-align:center">No APs.</td></tr>';

  document.getElementById('corr-tbody').innerHTML = corrs.length
    ? corrs.map(c=>`<tr>
        <td><code style="font-size:10px">${c.client}</code></td>
        <td><code style="font-size:10px">${c.ap}</code></td>
        <td>${c.ssid||'(wildcard)'}</td>
        <td>${c.ch}</td>
        <td>${c.latency} ms</td>
        <td>${new Date(c.ts).toLocaleTimeString()}</td>
      </tr>`).join('')
    : '<tr><td colspan="6" style="color:var(--muted);text-align:center">No correlations.</td></tr>';
}

async function clearData() {
  await fetch('/api/prt/clear',{method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 2500);
</script>
</body>
</html>
)rawhtml";

inline void wuprt_routes() {
  server.on("/probe-response", HTTP_GET, []() { server.send_P(200, "text/html", WUPRT_PAGE); });
}

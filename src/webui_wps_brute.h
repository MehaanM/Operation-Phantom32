#pragma once
// webui_wps_brute.h — WPS brute-force attempt tracker UI.
// Route: /wps-brute. Prefix: wuwpb_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUWPB_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>WPS Brute Tracker — Phantom32</title>
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
table{width:100%;border-collapse:collapse;font-size:12px;}
th{text-align:left;padding:7px 8px;font-size:10px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;}
td{padding:7px 8px;border-bottom:1px solid var(--border);}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
.badge{padding:2px 7px;border-radius:4px;font-size:9px;font-weight:700;display:inline-block;}
.b-success{background:rgba(34,197,94,.18);color:var(--success);}
.b-locked{background:rgba(239,68,68,.18);color:var(--danger);}
.b-fail{background:rgba(100,116,139,.18);color:var(--muted);}
.b-unk{background:rgba(0,212,255,.12);color:var(--accent);}
.cracked{color:var(--success);font-weight:700;font-family:monospace;}
.tabs{display:flex;gap:4px;margin-bottom:14px;}
.tab{padding:5px 12px;border-radius:5px;font-size:12px;cursor:pointer;border:1px solid var(--border);color:var(--muted);}
.tab.active{background:rgba(0,212,255,.12);color:var(--accent);border-color:var(--accent);}
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
    <a href="/wps-scanner">WPS Scanner</a>
    <a href="/wps-brute" class="active">WPS Brute</a>
    <a href="/security">Security</a>
  </nav>
</div>
<div class="container">
  <h1>WPS Brute-Force Tracker</h1>
  <p class="subtitle">Monitors WPS authentication attempts and their outcomes. Tracks lock-outs, success, and device information from WPS Information Elements.</p>

  <div class="hero">
    <div class="stat"><label>WPS APs</label><div class="val" id="h-aps">0</div></div>
    <div class="stat"><label>Attempts</label><div class="val" id="h-att" style="color:var(--warn)">0</div></div>
    <div class="stat"><label>Cracked</label><div class="val" id="h-suc" style="color:var(--success)">0</div></div>
    <div class="stat"><label>Locked</label><div class="val" id="h-lck" style="color:var(--danger)">0</div></div>
  </div>

  <div class="section">
    <div class="tabs">
      <div class="tab active" onclick="showTab('aps',this)">WPS Access Points</div>
      <div class="tab" onclick="showTab('attempts',this)">Attempt Log</div>
    </div>

    <div id="tab-aps">
      <table><thead><tr>
        <th>BSSID</th><th>SSID</th><th>CH</th><th>RSSI</th>
        <th>WPS</th><th>State</th><th>Device</th><th>Maker</th>
        <th>Attempts</th><th>PIN</th>
      </tr></thead>
      <tbody id="ap-tbody">
        <tr><td colspan="10" style="color:var(--muted);text-align:center">Loading…</td></tr>
      </tbody></table>
    </div>

    <div id="tab-attempts" style="display:none">
      <table><thead><tr>
        <th>Time</th><th>BSSID</th><th>PIN</th><th>Outcome</th><th>RTT</th>
      </tr></thead>
      <tbody id="att-tbody">
        <tr><td colspan="5" style="color:var(--muted);text-align:center">No attempts yet.</td></tr>
      </tbody></table>
    </div>

    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>
</div>

<script>
const OC = {SUCCESS:'b-success',LOCKED:'b-locked',AUTH_FAIL:'b-fail',PIN_FAIL:'b-fail',
            NAK:'b-unk',TIMEOUT:'b-fail',UNKNOWN:'b-unk'};

function showTab(name, el) {
  document.querySelectorAll('.tab').forEach(t=>t.classList.remove('active'));
  el.classList.add('active');
  document.getElementById('tab-aps').style.display       = name==='aps'?'':'none';
  document.getElementById('tab-attempts').style.display  = name==='attempts'?'':'none';
}

async function refresh() {
  const r = await fetch('/api/wpb/all').then(x=>x.json()).catch(()=>({}));
  document.getElementById('h-aps').textContent = r.ap_count||0;
  document.getElementById('h-att').textContent = r.total_attempts||0;
  document.getElementById('h-suc').textContent = r.total_success||0;
  document.getElementById('h-lck').textContent = r.total_locked||0;

  const aps = r.aps||[];
  document.getElementById('ap-tbody').innerHTML = aps.length
    ? aps.map(a=>`<tr>
        <td><code style="font-size:10px">${a.bssid}</code></td>
        <td>${a.ssid||'—'}</td><td>${a.ch}</td><td>${a.rssi}dBm</td>
        <td>${a.enabled?`<span class="badge b-success">ON</span>`:`<span class="badge b-fail">OFF</span>`}
            ${a.locked?`<span class="badge b-locked">LOCKED</span>`:''}</td>
        <td>${a.state===1?'Configured':'Unconfigured'}</td>
        <td style="font-size:10px">${a.device||'—'}</td>
        <td style="font-size:10px">${a.maker||'—'}</td>
        <td>${a.attempts}</td>
        <td>${a.cracked?`<span class="cracked">${a.cracked}</span>`:'—'}</td>
      </tr>`).join('')
    : '<tr><td colspan="10" style="color:var(--muted);text-align:center">No WPS APs detected.</td></tr>';

  const atts = r.attempts||[];
  document.getElementById('att-tbody').innerHTML = atts.length
    ? atts.slice().reverse().map(a=>`<tr>
        <td>${new Date(a.ts*1000).toLocaleTimeString()}</td>
        <td><code style="font-size:10px">${a.bssid}</code></td>
        <td><code>${a.pin||'—'}</code></td>
        <td><span class="badge ${OC[a.outcome]||'b-unk'}">${a.outcome}</span></td>
        <td>${a.duration_ms}ms</td>
      </tr>`).join('')
    : '<tr><td colspan="5" style="color:var(--muted);text-align:center">No attempts.</td></tr>';
}

async function clearData() {
  await fetch('/api/wpb/clear',{method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 3000);
</script>
</body>
</html>
)rawhtml";

inline void wuwpb_routes() {
  server.on("/wps-brute", HTTP_GET, []() { server.send_P(200, "text/html", WUWPB_PAGE); });
}

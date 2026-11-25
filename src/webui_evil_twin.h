#pragma once
// webui_evil_twin.h — Evil twin / rogue AP detector UI.
// Route: /evil-twin. Prefix: wuetd_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUETD_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Evil Twin Detector — Phantom32</title>
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
.hero{display:grid;grid-template-columns:repeat(3,1fr);gap:10px;margin-bottom:16px;}
.stat{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:13px;}
.stat label{font-size:10px;color:var(--muted);text-transform:uppercase;}
.stat .val{font-size:22px;font-weight:800;color:var(--accent);margin-top:3px;}
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:18px;margin-bottom:14px;}
.section h3{font-size:12px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:14px;}
.alert-box{padding:12px 16px;border-radius:7px;margin-bottom:14px;border-left:3px solid;}
.alert-critical{background:rgba(239,68,68,.08);border-color:var(--danger);color:var(--danger);}
.alert-probable{background:rgba(245,158,11,.08);border-color:var(--warn);color:var(--warn);}
.alert-suspect{background:rgba(0,212,255,.08);border-color:var(--accent);color:var(--accent);}
.card{background:var(--bg3);border:1px solid var(--border);border-radius:7px;padding:14px;margin-bottom:10px;}
.card-header{display:flex;align-items:center;gap:8px;margin-bottom:8px;}
.card-body{font-size:11px;color:var(--muted);display:grid;grid-template-columns:1fr 1fr;gap:4px;}
.badge{padding:2px 7px;border-radius:4px;font-size:9px;font-weight:700;display:inline-block;}
.badge-crit{background:rgba(239,68,68,.2);color:var(--danger);}
.badge-prob{background:rgba(245,158,11,.18);color:var(--warn);}
.badge-susp{background:rgba(0,212,255,.12);color:var(--accent);}
table{width:100%;border-collapse:collapse;font-size:12px;}
th{text-align:left;padding:7px 8px;font-size:10px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;}
td{padding:7px 8px;border-bottom:1px solid var(--border);}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
.ev-row{padding:7px 8px;border-bottom:1px solid var(--border);font-size:11px;display:flex;gap:10px;align-items:flex-start;}
.ev-ts{color:var(--muted);min-width:65px;flex-shrink:0;}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.btn-primary{background:var(--accent);color:#000;font-weight:600;}
.btn-primary:hover{opacity:.9;}
.action-row{display:flex;gap:8px;margin-top:12px;flex-wrap:wrap;}
.form-row{display:flex;gap:8px;flex-wrap:wrap;align-items:flex-end;margin-bottom:14px;}
input{background:var(--bg3);border:1px solid var(--border);border-radius:5px;padding:5px 8px;color:var(--text);font-size:12px;}
label{font-size:11px;color:var(--muted);}
.tabs{display:flex;gap:4px;margin-bottom:14px;}
.tab{padding:5px 12px;border-radius:5px;font-size:12px;cursor:pointer;border:1px solid var(--border);color:var(--muted);}
.tab.active{background:rgba(0,212,255,.12);color:var(--accent);border-color:var(--accent);}
@media(max-width:700px){.hero{grid-template-columns:1fr};.card-body{grid-template-columns:1fr}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/security">Security</a>
    <a href="/evil-twin" class="active">Evil Twin</a>
    <a href="/karma">KARMA</a>
    <a href="/threats">Threats</a>
  </nav>
</div>
<div class="container">
  <h1>Evil Twin Detector</h1>
  <p class="subtitle">Detects rogue APs that impersonate your trusted networks. Add trusted APs to watch, then the firmware monitors for evil twins with matching SSIDs.</p>

  <div class="hero">
    <div class="stat"><label>Trusted APs</label><div class="val" id="h-legit">0</div></div>
    <div class="stat"><label>Rogue APs</label><div class="val" id="h-rogue" style="color:var(--danger)">0</div></div>
    <div class="stat"><label>Total Alerts</label><div class="val" id="h-det" style="color:var(--warn)">0</div></div>
  </div>

  <div id="alert-container"></div>

  <div class="section">
    <h3>Add Trusted AP</h3>
    <div class="form-row">
      <div>
        <label>BSSID</label><br>
        <input id="f-bssid" placeholder="AA:BB:CC:DD:EE:FF" style="width:150px">
      </div>
      <div>
        <label>SSID</label><br>
        <input id="f-ssid" placeholder="Network name" style="width:140px">
      </div>
      <div>
        <label>Security</label><br>
        <select id="f-sec" style="background:var(--bg3);border:1px solid var(--border);padding:5px 8px;color:var(--text);border-radius:5px;font-size:12px">
          <option value="4">WPA3</option>
          <option value="3" selected>WPA2</option>
          <option value="2">WPA</option>
          <option value="1">WEP</option>
          <option value="0">Open</option>
        </select>
      </div>
      <button class="btn btn-primary" onclick="addTrusted()">Add Trusted</button>
    </div>
  </div>

  <div class="section">
    <div class="tabs">
      <div class="tab active" onclick="showTab('rogues',this)">Rogue APs</div>
      <div class="tab" onclick="showTab('events',this)">Event Log</div>
    </div>

    <div id="tab-rogues">
      <div id="rogue-cards">
        <div style="color:var(--muted);text-align:center;padding:20px;">No rogues detected. Add trusted APs to monitor.</div>
      </div>
    </div>

    <div id="tab-events" style="display:none">
      <div id="ev-list">
        <div style="color:var(--muted);text-align:center;padding:20px;">No events yet.</div>
      </div>
    </div>

    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-ghost" onclick="clearData()">Clear All</button>
    </div>
  </div>
</div>

<script>
const LEVEL_CLASS = {SUSPECT:'badge-susp', PROBABLE:'badge-prob', CRITICAL:'badge-crit'};
const LEVEL_ALERT = {SUSPECT:'alert-suspect', PROBABLE:'alert-probable', CRITICAL:'alert-critical'};
const SEC_NAMES = ['Open','WEP','WPA','WPA2','WPA3'];

function showTab(name, el) {
  document.querySelectorAll('.tab').forEach(t=>t.classList.remove('active'));
  el.classList.add('active');
  document.getElementById('tab-rogues').style.display = name==='rogues'?'':'none';
  document.getElementById('tab-events').style.display = name==='events'?'':'none';
}

async function addTrusted() {
  const bssid = document.getElementById('f-bssid').value.trim();
  const ssid  = document.getElementById('f-ssid').value.trim();
  const sec   = document.getElementById('f-sec').value;
  if (!bssid || !ssid) return;
  await fetch(`/api/etd/add_trusted?bssid=${encodeURIComponent(bssid)}&ssid=${encodeURIComponent(ssid)}&sec=${sec}`, {method:'POST'});
  refresh();
}

function renderAlerts(rogues) {
  const critical = rogues.filter(r=>r.alert==='CRITICAL');
  const probable = rogues.filter(r=>r.alert==='PROBABLE');
  let html = '';
  if (critical.length)
    html += `<div class="alert-box alert-critical">🚨 CRITICAL: ${critical.length} critical evil twin(s) detected! ${critical.map(r=>`"${r.ssid}"`).join(', ')}</div>`;
  if (probable.length)
    html += `<div class="alert-box alert-probable">⚠️ Probable evil twin(s): ${probable.map(r=>`"${r.ssid}"`).join(', ')}</div>`;
  document.getElementById('alert-container').innerHTML = html;
}

async function refresh() {
  const r = await fetch('/api/etd/all').then(x=>x.json()).catch(()=>({}));
  document.getElementById('h-legit').textContent = r.legit_count||0;
  document.getElementById('h-rogue').textContent = r.rogue_count||0;
  document.getElementById('h-det').textContent   = r.total_detections||0;

  const rogues = r.rogues||[];
  renderAlerts(rogues);

  document.getElementById('rogue-cards').innerHTML = rogues.length
    ? rogues.sort((a,b)=>b.alert_code-a.alert_code).map(rogue=>`
      <div class="card">
        <div class="card-header">
          <span class="badge ${LEVEL_CLASS[rogue.alert]||''}">⚠ ${rogue.alert}</span>
          <strong>${rogue.ssid}</strong>
          <code style="font-size:10px;color:var(--muted)">${rogue.bssid}</code>
        </div>
        <div class="card-body">
          <div>Rogue CH: ${rogue.ch}</div>
          <div>Rogue RSSI: ${rogue.rssi} dBm</div>
          <div>Security: ${SEC_NAMES[rogue.sec]||rogue.sec}</div>
          <div>PMF: ${rogue.pmf?'Yes':'No'}</div>
          <div>Mimics: <code style="font-size:9px">${rogue.legit_bssid}</code></div>
          <div>Detections: ${rogue.detections}</div>
        </div>
        <div style="font-size:10px;color:var(--warn);margin-top:7px">${rogue.reason}</div>
      </div>`).join('')
    : '<div style="color:var(--muted);text-align:center;padding:20px;">No rogues detected.</div>';

  const events = r.events||[];
  document.getElementById('ev-list').innerHTML = events.length
    ? events.slice().reverse().map(e=>`
      <div class="ev-row">
        <span class="ev-ts">${new Date(e.ts*1000).toLocaleTimeString()}</span>
        <div>
          <span class="badge ${LEVEL_CLASS[e.level]||''}">${e.level}</span>
          <strong style="font-size:12px">"${e.ssid}"</strong>
          <div style="font-size:10px;color:var(--muted);margin-top:3px">${e.reason}</div>
          <div style="font-size:9px;color:var(--muted)">Rogue: ${e.rogue} vs Legit: ${e.legit}</div>
        </div>
      </div>`).join('')
    : '<div style="color:var(--muted);text-align:center;padding:20px;">No events.</div>';
}

async function clearData() {
  await fetch('/api/etd/clear',{method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 5000);
</script>
</body>
</html>
)rawhtml";

inline void wuetd_routes() {
  server.on("/evil-twin", HTTP_GET, []() { server.send_P(200, "text/html", WUETD_PAGE); });
}

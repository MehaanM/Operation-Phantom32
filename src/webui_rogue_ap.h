#pragma once
// webui_rogue_ap.h — Rogue AP detector UI.
// Route: /rogue-ap. Prefix: wurgd_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WURGD_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Rogue AP Detector — Phantom32</title>
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
table{width:100%;border-collapse:collapse;font-size:11px;}
th{text-align:left;padding:7px 8px;font-size:9px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;}
td{padding:6px 8px;border-bottom:1px solid var(--border);}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
.badge{padding:2px 7px;border-radius:4px;font-size:9px;font-weight:700;display:inline-block;}
.badge-confirmed{background:rgba(239,68,68,.2);color:var(--danger);}
.badge-probable{background:rgba(245,158,11,.18);color:var(--warn);}
.badge-suspicious{background:rgba(0,212,255,.1);color:var(--accent);}
.badge-known{background:rgba(34,197,94,.12);color:var(--success);}
.badge-unknown{background:rgba(100,116,139,.14);color:var(--muted);}
.cap-pill{display:inline-block;padding:1px 4px;border-radius:3px;font-size:8px;font-weight:600;background:var(--bg3);color:var(--muted);margin:1px;}
.cap-pill.on{background:rgba(0,212,255,.1);color:var(--accent);}
.btn{padding:5px 10px;border:none;border-radius:5px;font-size:11px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.btn-mark{background:rgba(34,197,94,.1);border:1px solid var(--success);color:var(--success);}
.btn-mark:hover{background:rgba(34,197,94,.18);}
.action-row{display:flex;gap:8px;margin-top:12px;flex-wrap:wrap;}
.tabs{display:flex;gap:4px;margin-bottom:14px;}
.tab{padding:5px 12px;border-radius:5px;font-size:12px;cursor:pointer;border:1px solid var(--border);color:var(--muted);}
.tab.active{background:rgba(0,212,255,.12);color:var(--accent);border-color:var(--accent);}
.sec-name{font-size:9px;}
@media(max-width:700px){.hero{grid-template-columns:1fr 1fr}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/ssid-collision">SSID Collision</a>
    <a href="/rogue-ap" class="active">Rogue AP</a>
    <a href="/security-audit">Audit</a>
  </nav>
</div>
<div class="container">
  <h1>Rogue AP Detector</h1>
  <p class="subtitle">Identifies APs impersonating known-good SSIDs. Mark legitimate APs as "Known"; the system flags new APs sharing that SSID with different OUI, security, or capabilities.</p>

  <div class="hero">
    <div class="stat"><label>APs Seen</label><div class="val" id="h-cnt">0</div></div>
    <div class="stat"><label>Confirmed Rogue</label><div class="val" id="h-confirmed" style="color:var(--danger)">0</div></div>
    <div class="stat"><label>Probable</label><div class="val" id="h-probable" style="color:var(--warn)">0</div></div>
    <div class="stat"><label>Total Alerts</label><div class="val" id="h-alerts">0</div></div>
  </div>

  <div class="section">
    <div class="tabs">
      <div class="tab active" onclick="showTab('ap',this)">All APs</div>
      <div class="tab" onclick="showTab('alerts',this)">Alert Log</div>
    </div>

    <div id="tab-ap">
      <table>
        <thead>
          <tr>
            <th>BSSID</th><th>SSID</th><th>CH</th><th>RSSI</th>
            <th>Security</th><th>Capabilities</th><th>Trust</th><th>Action</th>
          </tr>
        </thead>
        <tbody id="ap-tbody">
          <tr><td colspan="8" style="color:var(--muted);text-align:center">Waiting for beacon data…</td></tr>
        </tbody>
      </table>
    </div>

    <div id="tab-alerts" style="display:none">
      <table>
        <thead>
          <tr><th>Time</th><th>Rogue BSSID</th><th>Legit BSSID</th><th>SSID</th><th>Risk</th><th>Reason</th></tr>
        </thead>
        <tbody id="alert-tbody">
          <tr><td colspan="6" style="color:var(--muted);text-align:center">No alerts.</td></tr>
        </tbody>
      </table>
    </div>

    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-ghost" onclick="clearData()">Clear All</button>
    </div>
  </div>
</div>

<script>
let g_data = {aps:[], alerts:[], confirmed:0, probable:0};
const SEC_NAMES = ['Open','WEP','WPA','WPA2','WPA3'];

function showTab(name, el) {
  document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
  el.classList.add('active');
  document.getElementById('tab-ap').style.display     = name === 'ap'     ? '' : 'none';
  document.getElementById('tab-alerts').style.display = name === 'alerts' ? '' : 'none';
}

function capPills(a) {
  const caps = [
    ['PMF', a.pmf], ['WPS', a.wps], ['HT', a.ht], ['VHT', a.vht], ['HE', a.he]
  ];
  return caps.map(([name, on]) => `<span class="cap-pill${on?' on':''}">${name}</span>`).join('');
}

function trustBadge(a) {
  return a.known
    ? '<span class="badge badge-known">✓ Known</span>'
    : '<span class="badge badge-unknown">Unknown</span>';
}

async function markKnown(bssid) {
  await fetch(`/api/rgd/mark_known?bssid=${encodeURIComponent(bssid)}`, {method:'POST'});
  refresh();
}

function renderAPs() {
  document.getElementById('ap-tbody').innerHTML = g_data.aps.length
    ? g_data.aps.map(a => `<tr>
        <td><code style="font-size:9px">${a.bssid}</code></td>
        <td><strong>${a.ssid || '<em style="color:var(--muted)">Hidden</em>'}</strong></td>
        <td>${a.ch}</td>
        <td>${a.rssi} dBm</td>
        <td><span class="sec-name">${SEC_NAMES[a.security] || a.security}</span></td>
        <td>${capPills(a)}</td>
        <td>${trustBadge(a)}</td>
        <td>${!a.known ? `<button class="btn btn-mark" onclick="markKnown('${a.bssid}')">✓ Mark Known</button>` : '—'}</td>
      </tr>`).join('')
    : '<tr><td colspan="8" style="color:var(--muted);text-align:center">No APs seen.</td></tr>';
}

function riskBadge(r) {
  if (r === 'Confirmed') return '<span class="badge badge-confirmed">⚡ Confirmed</span>';
  if (r === 'Probable')  return '<span class="badge badge-probable">⚠ Probable</span>';
  return '<span class="badge badge-suspicious">? Suspicious</span>';
}

function renderAlerts() {
  document.getElementById('alert-tbody').innerHTML = g_data.alerts.length
    ? g_data.alerts.map(a => `<tr>
        <td style="color:var(--muted)">${new Date(a.ts*1000).toLocaleTimeString()}</td>
        <td><code style="font-size:9px;color:var(--danger)">${a.rogue}</code></td>
        <td><code style="font-size:9px;color:var(--success)">${a.legit}</code></td>
        <td><strong>${a.ssid}</strong></td>
        <td>${riskBadge(a.risk)}</td>
        <td style="font-size:10px;color:var(--muted)">${a.reason}</td>
      </tr>`).join('')
    : '<tr><td colspan="6" style="color:var(--muted);text-align:center">No alerts.</td></tr>';
}

async function refresh() {
  const r = await fetch('/api/rgd/all').then(x => x.json()).catch(() => g_data);
  g_data = r;
  document.getElementById('h-cnt').textContent       = r.ap_count || 0;
  document.getElementById('h-confirmed').textContent = r.confirmed || 0;
  document.getElementById('h-probable').textContent  = r.probable  || 0;
  document.getElementById('h-alerts').textContent    = r.total_alerts || 0;
  renderAPs();
  renderAlerts();
}

async function clearData() {
  await fetch('/api/rgd/clear', {method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 4000);
</script>
</body>
</html>
)rawhtml";

inline void wurgd_routes() {
  server.on("/rogue-ap", HTTP_GET, []() { server.send_P(200, "text/html", WURGD_PAGE); });
}

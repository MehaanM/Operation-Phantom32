#pragma once
// webui_captive.h — Captive portal detector UI.
// Route: /captive. Prefix: wucpd_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUCPD_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Captive Portal Detector — Phantom32</title>
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
.card-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(260px,1fr));gap:12px;}
.ap-card{background:var(--bg3);border:1px solid var(--border);border-radius:7px;padding:14px;}
.ap-card.high{border-left:3px solid var(--danger);}
.ap-card.medium{border-left:3px solid var(--warn);}
.ap-card.low{border-left:3px solid var(--muted);}
.card-ssid{font-size:14px;font-weight:700;}
.card-bssid{font-family:monospace;font-size:9px;color:var(--muted);margin:2px 0 8px;}
.badge-row{display:flex;flex-wrap:wrap;gap:4px;margin-bottom:8px;}
.badge{padding:2px 7px;border-radius:3px;font-size:9px;font-weight:700;}
.b-high{background:rgba(239,68,68,.12);color:var(--danger);}
.b-medium{background:rgba(245,158,11,.12);color:var(--warn);}
.b-low{background:rgba(100,116,139,.12);color:var(--muted);}
.b-type{background:rgba(0,212,255,.08);color:var(--accent);}
.b-open{background:rgba(239,68,68,.08);color:var(--danger);}
.b-pmf{background:rgba(34,197,94,.08);color:var(--success);}
.card-reason{font-size:9px;color:var(--muted);margin-top:4px;border-top:1px solid var(--border);padding-top:6px;}
table{width:100%;border-collapse:collapse;font-size:10px;}
th{text-align:left;padding:8px 10px;border-bottom:2px solid var(--border);color:var(--muted);font-weight:600;font-size:9px;text-transform:uppercase;}
td{padding:7px 10px;border-bottom:1px solid var(--border);}
tr:hover td{background:var(--bg3);}
.conf-High{color:var(--danger);}
.conf-Medium{color:var(--warn);}
.conf-Low{color:var(--muted);}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;margin-top:12px;}
@media(max-width:700px){.hero{grid-template-columns:1fr 1fr};.card-grid{grid-template-columns:1fr}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/captive" class="active">Captive</a>
    <a href="/scan-cache">Scan Cache</a>
    <a href="/rogue-ap">Rogue AP</a>
  </nav>
</div>
<div class="container">
  <h1>Captive Portal Detector</h1>
  <p class="subtitle">Identifies captive portals and potential credential-harvesting APs based on SSID patterns, vendor OUI, 802.11 IE flags, and security mismatches. High-confidence alerts flag suspected evil twins and honeypots.</p>

  <div class="hero">
    <div class="stat"><label>APs Monitored</label><div class="val" id="h-cnt">0</div></div>
    <div class="stat"><label>Detected</label><div class="val" id="h-det" style="color:var(--warn)">0</div></div>
    <div class="stat"><label>Evil Twin</label><div class="val" id="h-et" style="color:var(--danger)">0</div></div>
    <div class="stat"><label>Honeypots</label><div class="val" id="h-hp" style="color:var(--danger)">0</div></div>
  </div>

  <div class="section">
    <h3>Detected APs (Medium+ Confidence)</h3>
    <div class="card-grid" id="ap-grid">
      <div style="color:var(--muted)">No portals detected.</div>
    </div>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>

  <div class="section">
    <h3>Recent Detection Events</h3>
    <div style="overflow-x:auto">
    <table>
      <thead><tr>
        <th>Time</th><th>SSID</th><th>BSSID</th>
        <th>Type</th><th>Confidence</th><th>Reason</th>
      </tr></thead>
      <tbody id="ev-body"></tbody>
    </table>
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

function confClass(c) { return 'conf-' + c; }

function apCard(a) {
  const cls = a.conf === 'High' ? 'high' : a.conf === 'Medium' ? 'medium' : 'low';
  return `<div class="ap-card ${cls}">
    <div class="card-ssid">${a.ssid||'Hidden'}</div>
    <div class="card-bssid">${a.bssid}</div>
    <div class="badge-row">
      <span class="badge b-${cls}">${a.conf}</span>
      <span class="badge b-type">${a.type}</span>
      ${a.open ? '<span class="badge b-open">Open</span>' : ''}
      ${a.pmf  ? '<span class="badge b-pmf">PMF</span>' : ''}
    </div>
    <div style="font-size:9px;color:var(--muted)">CH${a.ch} &nbsp; ${a.rssi} dBm &nbsp; ${a.beacons} beacons</div>
    <div class="card-reason">${a.reason||'—'}</div>
  </div>`;
}

async function refresh() {
  const r = await fetch('/api/cpd/all').then(x=>x.json()).catch(()=>({}));
  const aps = r.aps || [];
  const events = r.events || [];

  document.getElementById('h-cnt').textContent = r.ap_count || 0;
  document.getElementById('h-det').textContent = r.total_detected || 0;
  document.getElementById('h-et').textContent  = r.total_evil_twin || 0;
  document.getElementById('h-hp').textContent  = r.total_honeypot || 0;

  const alertAps = aps.filter(a => a.conf === 'High' || a.conf === 'Medium');
  document.getElementById('ap-grid').innerHTML = alertAps.length
    ? alertAps.map(apCard).join('')
    : '<div style="color:var(--muted)">No portals detected yet.</div>';

  document.getElementById('ev-body').innerHTML = events.length
    ? events.slice().reverse().map(ev => `<tr>
        <td style="color:var(--muted)">${reltime(ev.ts)}</td>
        <td>${ev.ssid||'—'}</td>
        <td style="font-family:monospace;font-size:9px">${ev.bssid.substring(0,8)}…</td>
        <td><span class="badge b-type">${ev.type}</span></td>
        <td class="${confClass(ev.conf)}">${ev.conf}</td>
        <td style="color:var(--muted)">${ev.reason||'—'}</td>
      </tr>`).join('')
    : '<tr><td colspan="6" style="text-align:center;color:var(--muted);padding:16px">No events.</td></tr>';
}

async function clearData() {
  await fetch('/api/cpd/clear', {method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 3000);
</script>
</body>
</html>
)rawhtml";

inline void wucpd_routes() {
  server.on("/captive", HTTP_GET, []() { server.send_P(200, "text/html", WUCPD_PAGE); });
}

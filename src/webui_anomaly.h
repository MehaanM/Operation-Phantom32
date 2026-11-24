#pragma once
// webui_anomaly.h — AP anomaly detection UI.
// Route: /anomaly. Prefix: wuapa_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUAPA_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Anomaly Detection — Phantom32</title>
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
.stat .val{font-size:22px;font-weight:800;color:var(--danger);margin-top:3px;}
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:18px;margin-bottom:14px;}
.section h3{font-size:12px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:14px;}
.alert-list{display:flex;flex-direction:column;gap:8px;}
.alert-card{background:var(--bg3);border:1px solid var(--border);border-radius:7px;padding:12px 14px;position:relative;}
.alert-card.high{border-left:3px solid var(--danger);}
.alert-card.med{border-left:3px solid var(--warn);}
.alert-card.low{border-left:3px solid var(--accent);}
.alert-type{font-size:11px;font-weight:700;color:var(--danger);}
.alert-bssid{font-size:10px;color:var(--muted);margin-top:2px;font-family:monospace;}
.alert-detail{font-size:11px;color:var(--text);margin-top:4px;}
.alert-meta{font-size:10px;color:var(--muted);margin-top:5px;display:flex;gap:12px;}
.badge-cnt{background:rgba(239,68,68,.15);color:var(--danger);padding:1px 7px;border-radius:4px;font-size:10px;font-weight:700;}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;margin-top:12px;}
.empty-msg{color:var(--muted);text-align:center;padding:20px;font-size:13px;}
@media(max-width:700px){.hero{grid-template-columns:1fr}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/threats">Threats</a>
    <a href="/anomaly" class="active">Anomaly</a>
    <a href="/deauth">Deauth</a>
    <a href="/security">Security</a>
  </nav>
</div>
<div class="container">
  <h1>Anomaly Detection</h1>
  <p class="subtitle">Passive detection of AP behavioral anomalies: spoofing, cloning, flooding, downgrade attacks.</p>

  <div class="hero">
    <div class="stat"><label>Active Alerts</label><div class="val" id="h-alerts">0</div></div>
    <div class="stat"><label>Total Anomalies</label><div class="val" id="h-total">0</div></div>
    <div class="stat"><label>High Severity</label><div class="val" id="h-high">0</div></div>
  </div>

  <div class="section">
    <h3>Active Alerts</h3>
    <div id="alert-list" class="alert-list">
      <div class="empty-msg">No anomalies detected. Monitoring…</div>
    </div>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-ghost" onclick="clearAlerts()">Clear</button>
    </div>
  </div>
</div>

<script>
const HIGH_TYPES = ['SSID Spoof','BSSID Clone','Evil Twin','KARMA Attack','Sudden Open'];
const MED_TYPES  = ['Beacon Flood','Mgmt Flood','Rapid Channel Change'];

function severity(type) {
  if(HIGH_TYPES.includes(type)) return 'high';
  if(MED_TYPES.includes(type))  return 'med';
  return 'low';
}

async function refresh() {
  const r = await fetch('/api/apa/alerts').then(x=>x.json()).catch(()=>({}));
  document.getElementById('h-alerts').textContent = r.alert_count||0;
  document.getElementById('h-total').textContent  = r.total_anomalies||0;

  const alerts = (r.alerts||[]).sort((a,b)=>{
    const sa = severity(a.type), sb = severity(b.type);
    if(sa===sb) return b.ts-a.ts;
    return sa==='high'?-1:sb==='high'?1:sa==='med'?-1:1;
  });

  const high_count = alerts.filter(a=>severity(a.type)==='high').length;
  document.getElementById('h-high').textContent = high_count;

  const list = document.getElementById('alert-list');
  if(!alerts.length) {
    list.innerHTML = '<div class="empty-msg">No anomalies detected. Monitoring…</div>';
    return;
  }

  list.innerHTML = alerts.map(a=>{
    const sev = severity(a.type);
    return `<div class="alert-card ${sev}">
      <div style="display:flex;align-items:flex-start;justify-content:space-between">
        <div>
          <div class="alert-type">${a.type}</div>
          <div class="alert-bssid">${a.bssid} · ${a.ssid||'(hidden)'}</div>
          <div class="alert-detail">${a.detail}</div>
          <div class="alert-meta">
            <span>Ch ${a.ch}</span>
            <span>${a.rssi} dBm</span>
            <span>${new Date(a.ts*1000).toLocaleTimeString()}</span>
          </div>
        </div>
        <div>
          <span class="badge-cnt">${a.count}×</span>
        </div>
      </div>
    </div>`;
  }).join('');
}

async function clearAlerts() {
  if(!confirm('Clear all anomaly alerts?')) return;
  await fetch('/api/apa/clear',{method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 2000);
</script>
</body>
</html>
)rawhtml";

inline void wuapa_routes() {
  server.on("/anomaly", HTTP_GET, []() {
    server.send_P(200, "text/html", WUAPA_PAGE);
  });
}

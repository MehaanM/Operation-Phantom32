#pragma once
// webui_ssid_collision.h — SSID collision detector UI.
// Route: /ssid-collision. Prefix: wussc_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUSSC_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>SSID Collision — Phantom32</title>
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
.ssid-card{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:16px;margin-bottom:10px;}
.ssid-card.risk-critical{border-left:3px solid var(--danger);}
.ssid-card.risk-high{border-left:3px solid #f97316;}
.ssid-card.risk-medium{border-left:3px solid var(--warn);}
.ssid-card.risk-low{border-left:3px solid var(--accent);}
.ssid-header{display:flex;align-items:center;gap:10px;margin-bottom:12px;}
.ssid-name{font-size:15px;font-weight:700;}
.badge{padding:2px 7px;border-radius:4px;font-size:9px;font-weight:700;display:inline-block;}
.risk-crit-badge{background:rgba(239,68,68,.2);color:var(--danger);}
.risk-high-badge{background:rgba(249,115,22,.2);color:#f97316;}
.risk-medium-badge{background:rgba(245,158,11,.18);color:var(--warn);}
.risk-low-badge{background:rgba(0,212,255,.12);color:var(--accent);}
.risk-none-badge{background:rgba(100,116,139,.18);color:var(--muted);}
.ap-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(200px,1fr));gap:8px;}
.ap-tile{background:var(--bg3);border:1px solid var(--border);border-radius:6px;padding:10px;font-size:11px;}
.ap-bssid{font-family:monospace;font-size:10px;color:var(--muted);}
.sec-label{font-size:9px;padding:1px 5px;border-radius:3px;background:rgba(0,212,255,.1);color:var(--accent);}
.alert-list{margin-top:12px;border-top:1px solid var(--border);padding-top:12px;}
.alert-item{padding:8px;border-radius:5px;background:rgba(239,68,68,.06);border:1px solid rgba(239,68,68,.2);margin-bottom:6px;font-size:11px;}
.tabs{display:flex;gap:4px;margin-bottom:14px;}
.tab{padding:5px 12px;border-radius:5px;font-size:12px;cursor:pointer;border:1px solid var(--border);color:var(--muted);}
.tab.active{background:rgba(0,212,255,.12);color:var(--accent);border-color:var(--accent);}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;margin-top:12px;}
@media(max-width:700px){.hero{grid-template-columns:1fr 1fr};.ap-grid{grid-template-columns:1fr}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/evil-twin">Evil Twin</a>
    <a href="/ssid-collision" class="active">SSID Collision</a>
    <a href="/security-audit">Audit</a>
  </nav>
</div>
<div class="container">
  <h1>SSID Collision Detection</h1>
  <p class="subtitle">Multiple APs broadcasting the same SSID. Critical when security modes differ — classic evil twin indicator.</p>

  <div class="hero">
    <div class="stat"><label>SSIDs</label><div class="val" id="h-ssids">0</div></div>
    <div class="stat"><label>Critical</label><div class="val" id="h-crit" style="color:var(--danger)">0</div></div>
    <div class="stat"><label>High Risk</label><div class="val" id="h-high" style="color:#f97316">0</div></div>
    <div class="stat"><label>Alerts</label><div class="val" id="h-alerts">0</div></div>
  </div>

  <div style="background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:18px;margin-bottom:14px;">
    <div class="tabs">
      <div class="tab active" onclick="showTab('ssids',this)">SSID Groups</div>
      <div class="tab" onclick="showTab('alerts',this)">Alert Log</div>
    </div>

    <div id="tab-ssids">
      <div id="ssid-cards"><div style="color:var(--muted);text-align:center;padding:20px;">No collisions detected.</div></div>
    </div>

    <div id="tab-alerts" style="display:none">
      <div id="alert-cards"><div style="color:var(--muted);text-align:center;padding:20px;">No alerts.</div></div>
    </div>

    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>
</div>

<script>
const SEC_NAMES = ['Open','WEP','WPA','WPA2','WPA3'];
const RISK_BADGE = {
  CRITICAL:'risk-crit-badge', HIGH:'risk-high-badge',
  MEDIUM:'risk-medium-badge', LOW:'risk-low-badge', NONE:'risk-none-badge'
};
const CARD_RISK = {
  CRITICAL:'risk-critical', HIGH:'risk-high',
  MEDIUM:'risk-medium', LOW:'risk-low', NONE:''
};

function showTab(name, el) {
  document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
  el.classList.add('active');
  document.getElementById('tab-ssids').style.display  = name === 'ssids'  ? '' : 'none';
  document.getElementById('tab-alerts').style.display = name === 'alerts' ? '' : 'none';
}

function rssiColor(r) {
  if (r >= -60) return 'var(--success)';
  if (r >= -75) return 'var(--warn)';
  return 'var(--danger)';
}

function renderSSIDs(ssids) {
  const collisions = ssids.filter(s => s.ap_count > 1 || s.risk !== 'NONE');
  const all = [...ssids].sort((a, b) => b.risk_code - a.risk_code);
  document.getElementById('ssid-cards').innerHTML = all.length
    ? all.map(s => `
      <div class="ssid-card ${CARD_RISK[s.risk] || ''}">
        <div class="ssid-header">
          <div class="ssid-name">${s.ssid || '<em>Hidden</em>'}</div>
          <span class="badge ${RISK_BADGE[s.risk] || ''}">${s.risk}</span>
          <span style="font-size:10px;color:var(--muted);margin-left:auto">${s.ap_count} AP${s.ap_count !== 1 ? 's' : ''}</span>
        </div>
        <div class="ap-grid">
          ${s.aps.map(a => `
            <div class="ap-tile">
              <div class="ap-bssid">${a.bssid}</div>
              <div style="margin:4px 0;display:flex;gap:4px;align-items:center">
                <span style="color:${rssiColor(a.rssi)};font-weight:600">${a.rssi}dBm</span>
                <span style="color:var(--muted)">CH${a.ch}</span>
                <span class="sec-label">${SEC_NAMES[a.sec] || '?'}</span>
              </div>
            </div>`).join('')}
        </div>
      </div>`).join('')
    : '<div style="color:var(--muted);text-align:center;padding:20px;">No SSIDs with multiple APs.</div>';
}

function renderAlerts(alerts) {
  document.getElementById('alert-cards').innerHTML = alerts.length
    ? [...alerts].reverse().map(a => `
      <div class="alert-item">
        <div style="display:flex;align-items:center;gap:8px;margin-bottom:4px">
          <span class="badge ${RISK_BADGE[a.risk] || ''}">${a.risk}</span>
          <strong>${a.ssid}</strong>
          <span style="color:var(--muted);font-size:10px;margin-left:auto">${new Date(a.ts*1000).toLocaleTimeString()}</span>
        </div>
        <div style="color:var(--muted);font-size:10px">
          <code>${a.bssid_a}</code> vs <code>${a.bssid_b}</code>
        </div>
        <div style="margin-top:3px;font-size:11px">${a.reason}</div>
      </div>`).join('')
    : '<div style="color:var(--muted);text-align:center;padding:20px;">No alerts.</div>';
}

async function refresh() {
  const r = await fetch('/api/ssc/all').then(x => x.json()).catch(() => ({}));
  const ssids = r.ssids || [];
  document.getElementById('h-ssids').textContent  = r.count || 0;
  document.getElementById('h-crit').textContent   = ssids.filter(s => s.risk === 'CRITICAL').length;
  document.getElementById('h-high').textContent   = ssids.filter(s => s.risk === 'HIGH').length;
  document.getElementById('h-alerts').textContent = r.total_alerts || 0;
  renderSSIDs(ssids);
  renderAlerts(r.alerts || []);
}

async function clearData() {
  await fetch('/api/ssc/clear', {method: 'POST'});
  refresh();
}

refresh();
setInterval(refresh, 4000);
</script>
</body>
</html>
)rawhtml";

inline void wussc_routes() {
  server.on("/ssid-collision", HTTP_GET, []() { server.send_P(200, "text/html", WUSSC_PAGE); });
}

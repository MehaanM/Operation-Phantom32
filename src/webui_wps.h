#pragma once
// webui_wps.h — WPS detector UI.
// Route: /wps. Prefix: wuwps_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUWPS_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>WPS — Phantom32</title>
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
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:18px;margin-bottom:14px;}
.section h3{font-size:12px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:14px;}
.card{background:var(--bg3);border:1px solid var(--border);border-radius:7px;padding:14px;margin-bottom:8px;position:relative;}
.card.pixie{border-left:3px solid var(--danger);}
.card.locked{border-left:3px solid var(--warn);}
.card.pbc{border-left:3px solid var(--success);}
.card-header{display:flex;align-items:center;gap:10px;margin-bottom:8px;}
.ssid{font-size:14px;font-weight:700;color:var(--accent);}
.bssid{font-size:9px;color:var(--muted);font-family:monospace;}
.badges{display:flex;gap:4px;flex-wrap:wrap;margin-bottom:8px;}
.badge{padding:1px 6px;border-radius:3px;font-size:8px;font-weight:700;}
.b-warn{background:rgba(245,158,11,.12);color:var(--warn);}
.b-danger{background:rgba(239,68,68,.12);color:var(--danger);}
.b-success{background:rgba(34,197,94,.12);color:var(--success);}
.b-info{background:rgba(0,212,255,.08);color:var(--accent);}
.b-muted{background:rgba(100,116,139,.1);color:var(--muted);}
.card-info{display:flex;gap:16px;font-size:10px;color:var(--muted);flex-wrap:wrap;}
.card-info span{color:var(--text);font-weight:500;}
.mini-chart{display:flex;align-items:flex-end;gap:1px;height:12px;margin-top:8px;}
.mini-bar{flex:1;border-radius:1px 1px 0 0;min-height:1px;background:var(--accent);}
table{width:100%;border-collapse:collapse;font-size:10px;}
th{text-align:left;padding:8px 10px;border-bottom:2px solid var(--border);color:var(--muted);font-weight:600;font-size:9px;text-transform:uppercase;}
td{padding:7px 10px;border-bottom:1px solid var(--border);}
tr:hover td{background:var(--bg3);}
.prog-bar{display:flex;gap:2px;}
.prog-step{width:14px;height:14px;border-radius:2px;display:flex;align-items:center;justify-content:center;font-size:7px;font-weight:700;}
.p-done{background:var(--success);color:#000;}
.p-curr{background:var(--warn);color:#000;}
.p-todo{background:var(--border);color:var(--muted);}
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
    <a href="/wps" class="active">WPS</a>
    <a href="/eapol">EAPOL</a>
    <a href="/ssid-pool">SSID Pool</a>
  </nav>
</div>
<div class="container">
  <h1>WPS Detector</h1>
  <p class="subtitle">Passively detects WPS-enabled access points from beacon frames. Identifies method (PBC/PIN), version, device info, locked state, and Pixie Dust vulnerability indicators.</p>

  <div class="hero">
    <div class="stat"><label>WPS APs</label><div class="val" id="h-ap">0</div></div>
    <div class="stat"><label>PBC Sessions</label><div class="val" id="h-pbc" style="color:var(--success)">0</div></div>
    <div class="stat"><label>PIN Sessions</label><div class="val" id="h-pin" style="color:var(--accent)">0</div></div>
    <div class="stat"><label>Locked APs</label><div class="val" id="h-lock" style="color:var(--warn)">0</div></div>
    <div class="stat"><label>Pixie Suspect</label><div class="val" id="h-pix" style="color:var(--danger)">0</div></div>
  </div>

  <div class="section">
    <h3>WPS Access Points</h3>
    <div id="ap-list">
      <div style="color:var(--muted)">No WPS APs detected.</div>
    </div>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>

  <div class="section">
    <h3>WPS Sessions</h3>
    <table>
      <thead><tr>
        <th>Client</th><th>AP SSID</th><th>Method</th>
        <th>Progress</th><th>Status</th><th>Duration</th>
      </tr></thead>
      <tbody id="sess-body"></tbody>
    </table>
  </div>
</div>

<script>
function methodStr(m) {
  const parts = [];
  if (m & 1) parts.push('PBC');
  if (m & 2) parts.push('PIN');
  if (m & 4) parts.push('NFC');
  return parts.length ? parts.join('+') : 'None';
}

function miniChart(hist) {
  if (!hist || !hist.length) return '';
  const max = Math.max(...hist, 1);
  return hist.map(v => {
    const h = Math.max(1, Math.round((v/max)*10));
    return `<div class="mini-bar" style="height:${h}px"></div>`;
  }).join('');
}

function msgProg(msg) {
  return [1,2,3,4,5,6,7,8].map(n => {
    let cls = n < msg ? 'p-done' : n === msg ? 'p-curr' : 'p-todo';
    return `<div class="prog-step ${cls}">M${n}</div>`;
  }).join('');
}

function rssiColor(r) {
  return r >= -60 ? 'var(--success)' : r >= -75 ? 'var(--warn)' : 'var(--danger)';
}

async function refresh() {
  const r = await fetch('/api/wps/all').then(x => x.json()).catch(()=>({}));
  const aps = r.aps || [];
  document.getElementById('h-ap').textContent   = r.ap_count   || 0;
  document.getElementById('h-pbc').textContent  = r.total_pbc  || 0;
  document.getElementById('h-pin').textContent  = r.total_pin  || 0;
  document.getElementById('h-lock').textContent = aps.filter(a => a.locked || a.pin_locked).length;
  document.getElementById('h-pix').textContent  = aps.filter(a => a.pixie).length;

  document.getElementById('ap-list').innerHTML = aps.length
    ? aps.map(a => {
        const cls = a.pixie ? 'card pixie' : a.locked||a.pin_locked ? 'card locked' : a.pbc ? 'card pbc' : 'card';
        const badges = [];
        if (a.pbc)       badges.push('<span class="badge b-success">PBC Active</span>');
        if (a.locked)    badges.push('<span class="badge b-warn">AP Locked</span>');
        if (a.pin_locked)badges.push('<span class="badge b-warn">PIN Lock</span>');
        if (a.pixie)     badges.push('<span class="badge b-danger">Pixie Suspect</span>');
        if (a.state===0) badges.push('<span class="badge b-muted">Unconfigured</span>');
        if (a.version)   badges.push(`<span class="badge b-info">WPS ${a.version}</span>`);
        const methods = methodStr(a.methods);
        if (methods !== 'None') badges.push(`<span class="badge b-info">${methods}</span>`);
        return `<div class="${cls}">
          <div class="card-header">
            <div>
              <div class="ssid">${a.ssid||'Hidden'}</div>
              <div class="bssid">${a.bssid}</div>
            </div>
            <div style="margin-left:auto;font-size:10px;color:${rssiColor(a.rssi)}">${a.rssi} dBm CH${a.ch}</div>
          </div>
          <div class="badges">${badges.join('')}</div>
          <div class="card-info">
            <span>Device: <span>${a.device||'—'}</span></span>
            <span>Mfr: <span>${a.mfr||'—'}</span></span>
            <span>Model: <span>${a.model||'—'}</span></span>
          </div>
          <div class="mini-chart">${miniChart(a.hist)}</div>
        </div>`;
      }).join('')
    : '<div style="color:var(--muted)">No WPS APs detected.</div>';

  const sessions = r.sessions || [];
  document.getElementById('sess-body').innerHTML = sessions.length
    ? sessions.map(s => `<tr>
        <td style="font-family:monospace;font-size:9px">${s.mac}</td>
        <td>${s.ssid||s.ap}</td>
        <td>${s.method & 1 ? 'PBC' : 'PIN'}</td>
        <td><div class="prog-bar">${msgProg(s.msg)}</div></td>
        <td style="color:${s.done?'var(--success)':s.failed?'var(--danger)':'var(--muted)'}">
          ${s.done ? 'Done' : s.failed ? 'Failed' : `M${s.msg}`}
        </td>
        <td style="color:var(--muted)">${s.dur_ms}ms</td>
      </tr>`).join('')
    : `<tr><td colspan="6" style="text-align:center;color:var(--muted);padding:16px">No sessions observed.</td></tr>`;
}

async function clearData() {
  await fetch('/api/wps/clear', {method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 3000);
</script>
</body>
</html>
)rawhtml";

inline void wuwps_routes() {
  server.on("/wps", HTTP_GET, []() {
    server.send_P(200, "text/html", WUWPS_PAGE);
  });
}

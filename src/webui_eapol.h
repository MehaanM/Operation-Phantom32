#pragma once
// webui_eapol.h — EAPOL / 802.1X handshake monitor UI.
// Route: /eapol. Prefix: wueap_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUEAP_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>EAPOL — Phantom32</title>
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
.two-col{display:grid;grid-template-columns:1.2fr 1fr;gap:14px;margin-bottom:14px;}
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:18px;margin-bottom:14px;}
.section h3{font-size:12px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:14px;}
table{width:100%;border-collapse:collapse;font-size:10px;}
th{text-align:left;padding:8px 10px;border-bottom:2px solid var(--border);color:var(--muted);font-weight:600;font-size:9px;text-transform:uppercase;}
td{padding:7px 10px;border-bottom:1px solid var(--border);}
tr:hover td{background:var(--bg3);}
.hs-steps{display:flex;gap:2px;align-items:center;}
.hs-step{width:16px;height:16px;border-radius:3px;display:flex;align-items:center;justify-content:center;font-size:7px;font-weight:700;}
.step-done{background:var(--success);color:#000;}
.step-curr{background:var(--warn);color:#000;}
.step-todo{background:var(--border);color:var(--muted);}
.anom-pill{padding:1px 5px;border-radius:3px;font-size:8px;font-weight:600;background:rgba(239,68,68,.12);color:var(--danger);margin-right:2px;}
.pmkid-box{font-family:monospace;font-size:8px;color:var(--warn);background:rgba(245,158,11,.08);padding:3px 7px;border-radius:4px;word-break:break-all;}
.event-row{padding:5px 8px;border-bottom:1px solid var(--border);font-size:9px;display:flex;gap:8px;align-items:center;}
.msg-pill{padding:1px 5px;border-radius:3px;font-size:8px;font-weight:700;}
.m-M1{background:rgba(0,212,255,.12);color:var(--accent);}
.m-M2{background:rgba(99,102,241,.12);color:#818cf8;}
.m-M3{background:rgba(245,158,11,.12);color:var(--warn);}
.m-M4{background:rgba(34,197,94,.12);color:var(--success);}
.m-GTK1,.m-GTK2{background:rgba(100,116,139,.1);color:var(--muted);}
.m-Unknown{background:rgba(239,68,68,.12);color:var(--danger);}
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
    <a href="/eapol" class="active">EAPOL</a>
    <a href="/ds-state">DS State</a>
    <a href="/stations">Stations</a>
  </nav>
</div>
<div class="container">
  <h1>EAPOL / WPA2 Handshake Monitor</h1>
  <p class="subtitle">Passively monitors 802.1X/WPA2 EAPOL key exchanges. Tracks 4-way handshake progress, extracts PMKID from M1 frames, detects MIC failures and replay attacks.</p>

  <div class="hero">
    <div class="stat"><label>EAPOL Frames</label><div class="val" id="h-tot">0</div></div>
    <div class="stat"><label>Handshakes Done</label><div class="val" id="h-hs" style="color:var(--success)">0</div></div>
    <div class="stat"><label>Failed HS</label><div class="val" id="h-fail" style="color:var(--danger)">0</div></div>
    <div class="stat"><label>PMKID Seen</label><div class="val" id="h-pmkid" style="color:var(--warn)">0</div></div>
    <div class="stat"><label>MIC Failures</label><div class="val" id="h-mic" style="color:var(--danger)">0</div></div>
  </div>

  <div class="two-col">
    <div class="section">
      <h3>Sessions</h3>
      <div style="overflow-x:auto">
      <table>
        <thead><tr>
          <th>Client MAC</th>
          <th>AP / SSID</th>
          <th>Progress</th>
          <th>HS</th>
          <th>ms</th>
          <th>Anomalies</th>
          <th>PMKID</th>
        </tr></thead>
        <tbody id="sess-body"></tbody>
      </table>
      </div>
      <div class="action-row">
        <button class="btn btn-ghost" onclick="clearData()">Clear</button>
      </div>
    </div>

    <div class="section">
      <h3>Recent Events</h3>
      <div id="evt-log">
        <div style="color:var(--muted)">No events yet.</div>
      </div>
    </div>
  </div>
</div>

<script>
const ANOM_LABELS = {
  1: 'MIC Fail', 2: 'Replay', 4: 'Timeout',
  8: 'Deauth-Mid', 16: 'PMKID', 32: 'RapidRetry'
};

function anomPills(a) {
  if (!a) return '—';
  return Object.entries(ANOM_LABELS)
    .filter(([k]) => (a & k) !== 0)
    .map(([,v]) => `<span class="anom-pill">${v}</span>`)
    .join('');
}

function hsSteps(last, done) {
  return [1,2,3,4].map(n => {
    let cls;
    if (done && n <= 4) cls = 'step-done';
    else if (n < last) cls = 'step-done';
    else if (n === last) cls = 'step-curr';
    else cls = 'step-todo';
    return `<div class="hs-step ${cls}">M${n}</div>`;
  }).join('');
}

function msgClass(m) {
  const map = {M1:'m-M1',M2:'m-M2',M3:'m-M3',M4:'m-M4',GTK1:'m-GTK1',GTK2:'m-GTK2'};
  return map[m] || 'm-Unknown';
}

async function refresh() {
  const r = await fetch('/api/eap/all').then(x => x.json()).catch(()=>({}));
  document.getElementById('h-tot').textContent  = r.total_eapol  || 0;
  document.getElementById('h-hs').textContent   = r.total_hs     || 0;
  document.getElementById('h-fail').textContent = r.total_failed || 0;
  document.getElementById('h-pmkid').textContent= r.total_pmkid  || 0;
  document.getElementById('h-mic').textContent  = r.total_mic_fail || 0;

  const sessions = r.sessions || [];
  document.getElementById('sess-body').innerHTML = sessions.length
    ? sessions.map(s => `<tr>
        <td style="font-family:monospace;font-size:9px">${s.mac}</td>
        <td>${s.ssid||s.ap}</td>
        <td><div class="hs-steps">${hsSteps(s.last_msg, s.done)}</div></td>
        <td style="color:var(--success)">${s.hs_ok}</td>
        <td style="color:var(--muted)">${s.hs_ms||0}</td>
        <td>${anomPills(s.anoms)}</td>
        <td>${s.has_pmkid ? `<div class="pmkid-box">${s.pmkid}</div>` : '—'}</td>
      </tr>`).join('')
    : `<tr><td colspan="7" style="text-align:center;color:var(--muted);padding:16px">No sessions observed.</td></tr>`;

  const evts = r.events || [];
  document.getElementById('evt-log').innerHTML = evts.length
    ? evts.map(e => `<div class="event-row">
        <span class="msg-pill ${msgClass(e.msg)}">${e.msg}</span>
        <span style="font-family:monospace;font-size:9px">${e.mac}</span>
        ${e.anoms ? `<span style="color:var(--danger);font-size:8px">${Object.entries(ANOM_LABELS).filter(([k])=>(e.anoms&k)!==0).map(([,v])=>v).join(' ')}</span>` : ''}
        <span style="color:var(--muted);margin-left:auto">${e.ts}s</span>
      </div>`).join('')
    : '<div style="color:var(--muted);padding:8px">No events yet.</div>';
}

async function clearData() {
  await fetch('/api/eap/clear', {method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 2000);
</script>
</body>
</html>
)rawhtml";

inline void wueap_routes() {
  server.on("/eapol", HTTP_GET, []() { server.send_P(200, "text/html", WUEAP_PAGE); });
}

#pragma once
// webui_tsf.h — TSF tracker UI.
// Route: /tsf. Prefix: wutsf_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUTSF_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>TSF Tracker — Phantom32</title>
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
.ap-card.skewed{border-left:3px solid var(--warn);}
.ap-card.jump{border-left:3px solid var(--danger);}
.ap-card.ok{border-left:3px solid var(--success);}
.card-title{font-size:14px;font-weight:700;color:var(--accent);}
.card-bssid{font-size:9px;color:var(--muted);font-family:monospace;margin-bottom:10px;}
.uptime-big{font-size:24px;font-weight:800;color:var(--text);}
.uptime-unit{font-size:11px;color:var(--muted);}
.drift-row{display:flex;align-items:center;gap:8px;margin-top:8px;}
.drift-val{font-size:14px;font-weight:700;}
.drift-bar{flex:1;height:6px;background:var(--border);border-radius:3px;overflow:hidden;}
.drift-fill{height:100%;border-radius:3px;}
.state-pill{padding:2px 6px;border-radius:4px;font-size:9px;font-weight:700;}
.s-Tracking{background:rgba(34,197,94,.12);color:var(--success);}
.s-Init{background:rgba(100,116,139,.1);color:var(--muted);}
.s-Skewed{background:rgba(245,158,11,.12);color:var(--warn);}
.s-Jump{background:rgba(239,68,68,.12);color:var(--danger);}
.s-Rollover{background:rgba(239,68,68,.2);color:var(--danger);}
.card-stats{display:flex;gap:16px;font-size:10px;color:var(--muted);flex-wrap:wrap;margin-top:8px;}
.card-stats span{color:var(--text);font-weight:500;}
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
    <a href="/tsf" class="active">TSF</a>
    <a href="/timing">Timing</a>
    <a href="/beacon-rates">Beacons</a>
  </nav>
</div>
<div class="container">
  <h1>TSF Tracker</h1>
  <p class="subtitle">Estimates AP uptime from the 64-bit Timing Synchronization Function (TSF) counter in beacon frames. Monitors clock drift (target: ±100 ppm), TSF discontinuities, and missed beacons.</p>

  <div class="hero">
    <div class="stat"><label>Tracked APs</label><div class="val" id="h-cnt">0</div></div>
    <div class="stat"><label>Good Clock</label><div class="val" id="h-ok" style="color:var(--success)">0</div></div>
    <div class="stat"><label>Skewed Clock</label><div class="val" id="h-sk" style="color:var(--warn)">0</div></div>
    <div class="stat"><label>TSF Jumps</label><div class="val" id="h-jmp" style="color:var(--danger)">0</div></div>
  </div>

  <div class="section">
    <h3>AP Uptime &amp; Clock Quality</h3>
    <div class="card-grid" id="ap-grid">
      <div style="color:var(--muted)">No AP TSF data.</div>
    </div>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>
</div>

<script>
function formatUptime(a) {
  if (a.days > 0) return `${a.days}d ${a.hours}h`;
  if (a.hours > 0) return `${a.hours}h ${a.mins}m`;
  if (a.mins > 0) return `${a.mins}m ${a.secs}s`;
  return `${a.secs}s`;
}

function driftColor(d) {
  const abs = Math.abs(d);
  if (abs > 500) return 'var(--danger)';
  if (abs > 100) return 'var(--warn)';
  return 'var(--success)';
}

function stateClass(s) {
  return 's-' + s;
}

function cardClass(a) {
  if (a.state === 'Jump') return 'ap-card jump';
  if (a.state === 'Skewed') return 'ap-card skewed';
  if (a.drift_ok) return 'ap-card ok';
  return 'ap-card';
}

function rssiColor(r) {
  return r >= -60 ? 'var(--success)' : r >= -75 ? 'var(--warn)' : 'var(--danger)';
}

async function refresh() {
  const r = await fetch('/api/tsf/all').then(x=>x.json()).catch(()=>({}));
  const aps = r.aps || [];
  document.getElementById('h-cnt').textContent = r.count || 0;
  document.getElementById('h-ok').textContent  = aps.filter(a=>a.drift_ok).length;
  document.getElementById('h-sk').textContent  = aps.filter(a=>!a.drift_ok && a.state!=='Jump').length;
  document.getElementById('h-jmp').textContent = aps.filter(a=>a.jumps>0).length;

  document.getElementById('ap-grid').innerHTML = aps.length
    ? aps.map(a => {
        const driftPct = Math.min(100, Math.abs(a.drift_ppm) / 10);
        const driftC = driftColor(a.drift_ppm);
        return `<div class="${cardClass(a)}">
          <div class="card-title">${a.ssid||'Hidden'}</div>
          <div class="card-bssid">${a.bssid} &nbsp; CH${a.ch} &nbsp; <span style="color:${rssiColor(a.rssi)}">${a.rssi} dBm</span></div>
          <div style="display:flex;align-items:center;gap:8px;margin-bottom:8px">
            <span class="state-pill ${stateClass(a.state)}">${a.state}</span>
          </div>
          <div class="uptime-big">${formatUptime(a)}</div>
          <div class="uptime-unit">uptime (${(a.uptime_s||0).toLocaleString()}s from TSF)</div>
          <div class="drift-row">
            <div class="drift-val" style="color:${driftC}">${a.drift_ppm > 0 ? '+' : ''}${a.drift_ppm} ppm</div>
            <div class="drift-bar"><div class="drift-fill" style="width:${driftPct}%;background:${driftC}"></div></div>
          </div>
          <div class="card-stats">
            <span>Beacons: <span>${(a.beacons||0).toLocaleString()}</span></span>
            <span>Missed: <span style="color:${a.missed>5?'var(--warn)':'var(--muted)'}">${a.missed}</span></span>
            <span>Jumps: <span style="color:${a.jumps>0?'var(--danger)':'var(--muted)'}">${a.jumps}</span></span>
          </div>
        </div>`;
      }).join('')
    : '<div style="color:var(--muted)">No AP TSF data yet.</div>';
}

async function clearData() {
  await fetch('/api/tsf/clear', {method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 3000);
</script>
</body>
</html>
)rawhtml";

inline void wutsf_routes() {
  server.on("/tsf", HTTP_GET, []() {
    server.send_P(200, "text/html", WUTSF_PAGE);
  });
}

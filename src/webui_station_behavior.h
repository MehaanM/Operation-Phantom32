#pragma once
// webui_station_behavior.h — STA behavioral profiling UI.
// Route: /station-behavior. Prefix: wustb_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUSTB_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Station Behavior — Phantom32</title>
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
th{text-align:left;padding:7px 8px;font-size:10px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;cursor:pointer;}
th:hover{color:var(--text);}
td{padding:7px 8px;border-bottom:1px solid var(--border);}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
.client-card{background:var(--bg3);border:1px solid var(--border);border-radius:7px;padding:12px;margin-bottom:8px;}
.cc-header{display:flex;align-items:center;gap:8px;margin-bottom:8px;}
.cc-mac{font-family:monospace;font-size:11px;}
.assoc-badge{background:rgba(34,197,94,.18);color:var(--success);padding:2px 7px;border-radius:4px;font-size:9px;font-weight:700;}
.searching-badge{background:rgba(245,158,11,.18);color:var(--warn);padding:2px 7px;border-radius:4px;font-size:9px;font-weight:700;}
.cc-stats{display:grid;grid-template-columns:repeat(5,1fr);gap:4px;font-size:10px;margin-bottom:8px;}
.cc-stat label{color:var(--muted);display:block;}
.cc-stat .v{font-weight:600;}
.duty-bar{height:4px;border-radius:2px;background:var(--bg2);}
.duty-fill{height:100%;border-radius:2px;}
.spark{display:inline-flex;align-items:flex-end;gap:1px;height:16px;}
.spark-bar{width:3px;border-radius:1px 1px 0 0;}
.ssid-chip{display:inline-flex;align-items:center;gap:4px;background:rgba(0,212,255,.08);border:1px solid var(--border);border-radius:4px;padding:2px 6px;margin:2px;font-size:10px;}
.ssid-cnt{color:var(--muted);font-size:9px;}
.roam-badge{background:rgba(249,115,22,.18);color:#f97316;padding:1px 5px;border-radius:3px;font-size:9px;font-weight:600;margin-left:4px;}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;margin-top:12px;}
@media(max-width:700px){.hero{grid-template-columns:1fr 1fr};.cc-stats{grid-template-columns:1fr 1fr}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/client-activity">Client Activity</a>
    <a href="/station-behavior" class="active">Station Behavior</a>
    <a href="/ds-state">DS State</a>
  </nav>
</div>
<div class="container">
  <h1>Station Behavioral Profiling</h1>
  <p class="subtitle">Probe patterns, roaming behavior, duty cycle, and activity fingerprinting for every observed station.</p>

  <div class="hero">
    <div class="stat"><label>Clients</label><div class="val" id="h-cnt">0</div></div>
    <div class="stat"><label>Associated</label><div class="val" id="h-assoc" style="color:var(--success)">0</div></div>
    <div class="stat"><label>Searching</label><div class="val" id="h-search" style="color:var(--warn)">0</div></div>
    <div class="stat"><label>Roamers</label><div class="val" id="h-roam" style="color:#f97316">0</div></div>
  </div>

  <div class="section">
    <h3>Station Profiles</h3>
    <div id="client-cards"><div style="color:var(--muted);text-align:center;padding:20px;">No stations observed.</div></div>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>
</div>

<script>
function rssiColor(r) {
  if (r >= -60) return 'var(--success)';
  if (r >= -75) return 'var(--warn)';
  return 'var(--danger)';
}

function sparkline(hist) {
  if (!hist || !hist.length) return '';
  const max = Math.max(...hist, 1);
  return '<div class="spark">' + hist.map(v => {
    const h = Math.round((v / max) * 14) + 2;
    return `<div class="spark-bar" style="height:${h}px;background:var(--accent)"></div>`;
  }).join('') + '</div>';
}

function dutyColor(d) {
  if (d > 70) return 'var(--success)';
  if (d > 30) return 'var(--warn)';
  return 'var(--muted)';
}

async function refresh() {
  const r = await fetch('/api/stb/all').then(x => x.json()).catch(() => ({}));
  const clients = r.clients || [];
  document.getElementById('h-cnt').textContent   = r.count || 0;
  document.getElementById('h-assoc').textContent  = clients.filter(c => c.assoc).length;
  document.getElementById('h-search').textContent = clients.filter(c => !c.assoc && c.probes > 0).length;
  document.getElementById('h-roam').textContent   = clients.filter(c => c.roams > 0).length;

  const sorted = [...clients].sort((a, b) => b.frames - a.frames);
  document.getElementById('client-cards').innerHTML = sorted.length
    ? sorted.map(c => `
      <div class="client-card">
        <div class="cc-header">
          <span class="cc-mac">${c.mac}</span>
          ${c.assoc
            ? `<span class="assoc-badge">ASSOCIATED</span><span style="font-size:10px;color:var(--muted)">${c.assoc_ssid||c.assoc_bssid}</span>`
            : `<span class="searching-badge">SEARCHING</span>`}
          ${c.roams > 0 ? `<span class="roam-badge">↗ ${c.roams} roam${c.roams>1?'s':''}</span>` : ''}
          <span style="margin-left:auto;font-size:10px;color:${rssiColor(c.rssi)}">${c.rssi}dBm</span>
        </div>
        <div class="cc-stats">
          <div class="cc-stat"><label>Frames</label><div class="v">${c.frames.toLocaleString()}</div></div>
          <div class="cc-stat"><label>Probes</label><div class="v">${c.probes}</div></div>
          <div class="cc-stat"><label>Wildcard</label><div class="v">${c.wildcard_probes}</div></div>
          <div class="cc-stat"><label>Data</label><div class="v">${c.data}</div></div>
          <div class="cc-stat"><label>Inter-probe</label><div class="v">${c.inter_probe_ms ? c.inter_probe_ms+'ms' : '—'}</div></div>
        </div>
        <div style="margin-bottom:8px">
          <div style="display:flex;justify-content:space-between;font-size:10px;margin-bottom:2px">
            <span style="color:var(--muted)">Activity duty cycle</span>
            <span style="color:${dutyColor(c.duty_cycle)}">${c.duty_cycle}%</span>
          </div>
          <div class="duty-bar">
            <div class="duty-fill" style="width:${c.duty_cycle}%;background:${dutyColor(c.duty_cycle)}"></div>
          </div>
        </div>
        ${sparkline(c.hist)}
        ${c.probed_ssids && c.probed_ssids.length ? `
        <div style="margin-top:8px">
          <div style="font-size:10px;color:var(--muted);margin-bottom:4px">Probed SSIDs:</div>
          <div style="flex-wrap:wrap;display:flex">
            ${[...c.probed_ssids].sort((a,b)=>b.n-a.n).map(p =>
              `<span class="ssid-chip">${p.ssid}<span class="ssid-cnt">${p.n}</span></span>`
            ).join('')}
          </div>
        </div>` : ''}
      </div>`).join('')
    : '<div style="color:var(--muted);text-align:center;padding:20px;">No stations.</div>';
}

async function clearData() {
  await fetch('/api/stb/clear', {method: 'POST'});
  refresh();
}

refresh();
setInterval(refresh, 3000);
</script>
</body>
</html>
)rawhtml";

inline void wustb_routes() {
  server.on("/station-behavior", HTTP_GET, []() {
    server.send_P(200, "text/html", WUSTB_PAGE);
  });
}

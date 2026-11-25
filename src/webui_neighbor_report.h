#pragma once
// webui_neighbor_report.h — 802.11k neighbor report UI.
// Route: /neighbor-report. Prefix: wunbr_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUNBR_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Neighbor Reports — Phantom32</title>
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
.ap-card{background:var(--bg3);border:1px solid var(--border);border-radius:7px;padding:12px;margin-bottom:8px;}
.ap-header{display:flex;align-items:center;gap:10px;margin-bottom:8px;}
.ap-ssid{font-size:14px;font-weight:700;}
.badge{padding:2px 6px;border-radius:3px;font-size:9px;font-weight:700;display:inline-block;}
.badge-rrm{background:rgba(0,212,255,.12);color:var(--accent);}
.badge-btm{background:rgba(34,197,94,.12);color:var(--success);}
.nbr-list{display:grid;grid-template-columns:repeat(auto-fill,minmax(220px,1fr));gap:6px;}
.nbr-tile{background:var(--bg2);border:1px solid var(--border);border-radius:5px;padding:8px;font-size:11px;}
.nbr-bssid{font-family:monospace;font-size:10px;color:var(--muted);}
.cap-pill{display:inline-block;padding:1px 5px;border-radius:3px;font-size:9px;background:rgba(0,212,255,.08);color:var(--accent);margin-right:2px;margin-top:2px;}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;margin-top:12px;}
@media(max-width:700px){.hero{grid-template-columns:1fr 1fr};.nbr-list{grid-template-columns:1fr}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/rrm">RRM Tracker</a>
    <a href="/neighbor-report" class="active">Neighbor Reports</a>
    <a href="/bss-transition">BSS Transition</a>
  </nav>
</div>
<div class="container">
  <h1>802.11k Neighbor Reports</h1>
  <p class="subtitle">Advertised neighbor AP lists (RRM Neighbor Report Element). Shows ESS topology — which APs know about each other.</p>

  <div class="hero">
    <div class="stat"><label>APs</label><div class="val" id="h-cnt">0</div></div>
    <div class="stat"><label>RRM Enabled</label><div class="val" id="h-rrm" style="color:var(--success)">0</div></div>
    <div class="stat"><label>BTM Enabled</label><div class="val" id="h-btm" style="color:var(--accent)">0</div></div>
    <div class="stat"><label>Total Reports</label><div class="val" id="h-rpts">0</div></div>
  </div>

  <div class="section">
    <h3>AP Neighbor Relationships</h3>
    <div id="ap-cards"><div style="color:var(--muted);text-align:center;padding:20px;">No neighbor reports received.</div></div>
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

const PHY_NAMES = {0:'Unknown',1:'FHSS',2:'DSSS',3:'IR',4:'OFDM',5:'HT',6:'VHT',7:'HE'};

function caps(n) {
  const c = [];
  if (n.qos) c.push('QoS');
  if (n.rrm) c.push('RRM');
  return c.map(x => `<span class="cap-pill">${x}</span>`).join('');
}

async function refresh() {
  const r = await fetch('/api/nbr/all').then(x => x.json()).catch(() => ({}));
  const aps = r.aps || [];
  document.getElementById('h-cnt').textContent   = r.count || 0;
  document.getElementById('h-rrm').textContent   = aps.filter(a => a.rrm).length;
  document.getElementById('h-btm').textContent   = aps.filter(a => a.btm).length;
  document.getElementById('h-rpts').textContent  = r.total_reports || 0;

  const withNeighbors = aps.filter(a => a.neighbors.length > 0);
  document.getElementById('ap-cards').innerHTML = withNeighbors.length
    ? withNeighbors.map(a => `
      <div class="ap-card">
        <div class="ap-header">
          <div class="ap-ssid">${a.ssid || 'Hidden'}</div>
          ${a.rrm ? '<span class="badge badge-rrm">RRM</span>' : ''}
          ${a.btm ? '<span class="badge badge-btm">BTM</span>' : ''}
          <span style="margin-left:auto;font-family:monospace;font-size:10px;color:var(--muted)">${a.bssid}</span>
          <span style="font-size:10px;color:var(--muted)">CH${a.ch}</span>
        </div>
        <div style="font-size:11px;color:var(--muted);margin-bottom:8px">
          Advertises ${a.neighbors.length} neighbor${a.neighbors.length>1?'s':''}
        </div>
        <div class="nbr-list">
          ${a.neighbors.map(n => `
            <div class="nbr-tile">
              <div class="nbr-bssid">${n.bssid}</div>
              <div style="margin-top:4px">
                CH${n.ch} &nbsp; <span style="color:var(--muted);font-size:9px">${PHY_NAMES[n.phy] || '?'}</span>
              </div>
              <div style="margin-top:3px">${caps(n)}</div>
            </div>`).join('')}
        </div>
      </div>`).join('')
    : '<div style="color:var(--muted);text-align:center;padding:20px;">No neighbor reports yet.</div>';
}

async function clearData() {
  await fetch('/api/nbr/clear', {method: 'POST'});
  refresh();
}

refresh();
setInterval(refresh, 5000);
</script>
</body>
</html>
)rawhtml";

inline void wunbr_routes() {
  server.on("/neighbor-report", HTTP_GET, []() { server.send_P(200, "text/html", WUNBR_PAGE); });
}

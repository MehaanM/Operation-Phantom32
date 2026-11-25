#pragma once
// webui_deauth_storm.h — Deauth storm detector UI.
// Route: /deauth-storm. Prefix: wudas_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUDAS_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Deauth Storm — Phantom32</title>
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
.storm-card{background:var(--bg3);border:2px solid var(--danger);border-radius:8px;padding:12px 14px;margin-bottom:8px;animation:flash 1s infinite alternate;}
@keyframes flash{from{border-color:var(--danger)}to{border-color:rgba(239,68,68,.3)}}
.ap-card{background:var(--bg3);border:1px solid var(--border);border-radius:8px;padding:12px 14px;margin-bottom:6px;}
.ap-card.storm{border-left:3px solid var(--danger);}
.ap-card.high{border-left:3px solid var(--warn);}
.ap-card.normal{border-left:3px solid var(--success);}
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:18px;margin-bottom:14px;}
.section h3{font-size:12px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:14px;}
table{width:100%;border-collapse:collapse;font-size:12px;}
th{text-align:left;padding:7px 8px;font-size:10px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;}
td{padding:7px 8px;border-bottom:1px solid var(--border);}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
.badge{padding:2px 7px;border-radius:4px;font-size:9px;font-weight:700;display:inline-block;}
.badge-storm{background:rgba(239,68,68,.2);color:var(--danger);}
.badge-normal{background:rgba(34,197,94,.12);color:var(--success);}
.badge-bcast{background:rgba(245,158,11,.12);color:var(--warn);}
.spark{display:inline-flex;align-items:flex-end;gap:1px;height:16px;}
.spark-bar{width:3px;border-radius:1px 1px 0 0;}
.tabs{display:flex;gap:4px;margin-bottom:14px;}
.tab{padding:5px 12px;border-radius:5px;font-size:12px;cursor:pointer;border:1px solid var(--border);color:var(--muted);}
.tab.active{background:rgba(0,212,255,.12);color:var(--accent);border-color:var(--accent);}
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
    <a href="/ds-state">DS State</a>
    <a href="/deauth-storm" class="active">Deauth Storm</a>
    <a href="/security-audit">Audit</a>
  </nav>
</div>
<div class="container">
  <h1>Deauth Storm Detector</h1>
  <p class="subtitle">Monitors 802.11 deauthentication and disassociation frame rates. Storms ≥5/sec indicate active disconnection attacks or severe RF interference.</p>

  <div class="hero">
    <div class="stat"><label>APs Watched</label><div class="val" id="h-cnt">0</div></div>
    <div class="stat"><label>Active Storms</label><div class="val" id="h-storms" style="color:var(--danger)">0</div></div>
    <div class="stat"><label>Total Events</label><div class="val" id="h-events">0</div></div>
    <div class="stat"><label>Broadcast Deauths</label><div class="val" id="h-bcast" style="color:var(--warn)">0</div></div>
  </div>

  <div id="storm-banner" style="display:none" class="storm-card">
    <strong style="color:var(--danger)">⚡ ACTIVE DEAUTH STORM DETECTED</strong>
    <div id="storm-detail" style="font-size:12px;color:var(--muted);margin-top:4px"></div>
  </div>

  <div class="section">
    <div class="tabs">
      <div class="tab active" onclick="showTab('ap',this)">AP Summary</div>
      <div class="tab" onclick="showTab('log',this)">Event Log</div>
    </div>

    <div id="tab-ap">
      <div id="ap-list"></div>
    </div>

    <div id="tab-log" style="display:none">
      <table>
        <thead>
          <tr><th>AP</th><th>Client</th><th>Type</th><th>Reason</th><th>Bcast</th><th>Time</th></tr>
        </thead>
        <tbody id="ev-tbody">
          <tr><td colspan="6" style="color:var(--muted);text-align:center">No events yet.</td></tr>
        </tbody>
      </table>
    </div>

    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>
</div>

<script>
let g_data = {aps:[], events:[], active_storms:0};

function showTab(name, el) {
  document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
  el.classList.add('active');
  document.getElementById('tab-ap').style.display  = name === 'ap'  ? '' : 'none';
  document.getElementById('tab-log').style.display = name === 'log' ? '' : 'none';
}

function sparkline(hist) {
  if (!hist || !hist.length) return '';
  const max = Math.max(...hist, 1);
  return '<div class="spark">' + hist.map(v => {
    const h = Math.round((v / max) * 14) + 2;
    const color = v >= 5 ? 'var(--danger)' : v >= 2 ? 'var(--warn)' : 'var(--muted)';
    return `<div class="spark-bar" style="height:${h}px;background:${color}"></div>`;
  }).join('') + '</div>';
}

function renderAPs() {
  const html = g_data.aps.map(a => {
    const cls = a.storm ? 'storm' : (a.rate >= 20 ? 'high' : 'normal');
    const badge = a.storm
      ? '<span class="badge badge-storm">⚡ STORM</span>'
      : '<span class="badge badge-normal">OK</span>';
    return `<div class="ap-card ${cls}">
      <div style="display:flex;align-items:center;gap:8px;margin-bottom:6px">
        ${badge}
        <strong>${a.ssid || 'Hidden'}</strong>
        <code style="font-size:9px;color:var(--muted)">${a.bssid}</code>
        <span style="font-size:10px;color:var(--muted);margin-left:auto">CH ${a.ch}</span>
      </div>
      <div style="display:flex;gap:16px;font-size:11px;color:var(--muted)">
        <span>Deauths: <strong style="color:var(--text)">${a.total_deauths}</strong></span>
        <span>Disassoc: <strong style="color:var(--text)">${a.total_disassoc}</strong></span>
        <span>Bcast: <strong style="color:${a.bcast_deauths>0?'var(--warn)':'var(--muted)'}">${a.bcast_deauths}</strong></span>
        <span>Rate: <strong style="color:${a.rate>=50?'var(--danger)':a.rate>=20?'var(--warn)':'var(--text)'}">${(a.rate/10).toFixed(1)}/s</strong></span>
        <span>Top reason: <strong style="color:var(--text)">${a.top_reason} (${a.top_reason_name})</strong></span>
      </div>
      <div style="margin-top:8px">${sparkline(a.hist)}</div>
    </div>`;
  }).join('') || '<div style="color:var(--muted);text-align:center;padding:20px">No APs.</div>';
  document.getElementById('ap-list').innerHTML = html;
}

function renderEvents() {
  document.getElementById('ev-tbody').innerHTML = g_data.events.length
    ? g_data.events.map(e => `<tr>
        <td><code style="font-size:9px">${e.ap}</code></td>
        <td><code style="font-size:9px;color:${e.bcast?'var(--warn)':'var(--text)'}">${e.bcast?'FF:FF:FF:FF:FF:FF':e.client}</code></td>
        <td><span class="badge ${e.type==='Deauth'?'badge-storm':'badge-bcast'}">${e.type}</span></td>
        <td style="font-size:10px">${e.reason} (${e.reason_name})</td>
        <td>${e.bcast?'<span class="badge badge-bcast">✓</span>':'—'}</td>
        <td style="color:var(--muted);font-size:10px">${new Date(e.ts*1000).toLocaleTimeString()}</td>
      </tr>`).join('')
    : '<tr><td colspan="6" style="color:var(--muted);text-align:center">No events.</td></tr>';
}

async function refresh() {
  const r = await fetch('/api/das/all').then(x => x.json()).catch(() => ({aps:[],events:[]}));
  g_data = r;
  document.getElementById('h-cnt').textContent   = r.ap_count || 0;
  document.getElementById('h-storms').textContent = r.active_storms || 0;
  document.getElementById('h-events').textContent = r.total_events || 0;
  document.getElementById('h-bcast').textContent  = (r.aps || []).reduce((s,a)=>s+a.bcast_deauths,0);

  const stormBanner = document.getElementById('storm-banner');
  if (r.active_storms > 0) {
    stormBanner.style.display = '';
    const names = (r.aps || []).filter(a => a.storm).map(a => a.ssid || a.bssid).join(', ');
    document.getElementById('storm-detail').textContent = `Storm active on: ${names}`;
  } else {
    stormBanner.style.display = 'none';
  }

  renderAPs();
  renderEvents();
}

async function clearData() {
  await fetch('/api/das/clear', {method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 2000);
</script>
</body>
</html>
)rawhtml";

inline void wudas_routes() {
  server.on("/deauth-storm", HTTP_GET, []() { server.send_P(200, "text/html", WUDAS_PAGE); });
}

#pragma once
// webui_client_rssi_map.h — Client RSSI multi-AP view UI.
// Route: /client-rssi. Prefix: wucrm_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUCRM_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Client RSSI Map — Phantom32</title>
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
.stat .val{font-size:22px;font-weight:800;color:var(--accent);margin-top:3px;}
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:18px;margin-bottom:14px;}
.section h3{font-size:12px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:14px;}
.client-card{background:var(--bg3);border:1px solid var(--border);border-radius:7px;padding:13px;margin-bottom:10px;}
.client-header{display:flex;gap:10px;align-items:center;margin-bottom:10px;}
.client-mac{font-family:monospace;font-size:12px;font-weight:600;}
.client-info{font-size:11px;color:var(--muted);}
.ap-row{display:flex;align-items:center;gap:8px;margin-bottom:4px;}
.ap-name{min-width:100px;font-size:10px;color:var(--muted);}
.rssi-bar-track{flex:1;background:var(--bg2);border-radius:3px;height:6px;}
.rssi-bar-fill{height:100%;border-radius:3px;}
.rssi-val{min-width:40px;text-align:right;font-size:10px;}
.spark-inline{display:inline-flex;align-items:flex-end;gap:1px;height:14px;vertical-align:middle;}
.spark-bar{width:3px;background:var(--accent);border-radius:1px;}
.spread-badge{padding:1px 5px;border-radius:3px;font-size:9px;font-weight:700;}
.sp-low{background:rgba(34,197,94,.18);color:var(--success);}
.sp-mid{background:rgba(245,158,11,.18);color:var(--warn);}
.sp-high{background:rgba(0,212,255,.12);color:var(--accent);}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;margin-top:12px;}
@media(max-width:700px){.hero{grid-template-columns:1fr}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/clients-ext">Clients</a>
    <a href="/client-rssi" class="active">RSSI Map</a>
    <a href="/rssi-tracker">RSSI Trends</a>
    <a href="/heatmap">Heatmap</a>
  </nav>
</div>
<div class="container">
  <h1>Client RSSI Multi-AP Map</h1>
  <p class="subtitle">Shows each client device's RSSI as observed from multiple access points. High RSSI spread indicates the client is not close to its primary AP.</p>

  <div class="hero">
    <div class="stat"><label>Clients</label><div class="val" id="h-cnt">0</div></div>
    <div class="stat"><label>Total Updates</label><div class="val" id="h-upd">0</div></div>
    <div class="stat"><label>Multi-AP Clients</label><div class="val" id="h-multi" style="color:var(--warn)">0</div></div>
  </div>

  <div class="section">
    <h3>Client RSSI Views</h3>
    <div id="client-list">
      <div style="color:var(--muted);text-align:center;padding:20px;">No clients. Waiting for data frames…</div>
    </div>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>
</div>

<script>
function rssiColor(rssi) {
  if (rssi >= -60) return 'var(--success)';
  if (rssi >= -75) return 'var(--warn)';
  return 'var(--danger)';
}

function rssiWidth(rssi) {
  return Math.max(2, Math.round(((rssi + 100) / 60) * 100));
}

function sparkline(hist) {
  if (!hist || !hist.length) return '';
  const max = Math.max(...hist.map(v=>-v), 1);
  return '<div class="spark-inline">' + hist.map(v=>{
    const h = Math.round(((-v) / max) * 12) + 2;
    return `<div class="spark-bar" style="height:${h}px;background:${rssiColor(v)}"></div>`;
  }).join('') + '</div>';
}

function spreadBadge(sp) {
  if (sp < 10) return `<span class="spread-badge sp-low">LOW</span>`;
  if (sp < 25) return `<span class="spread-badge sp-mid">MED</span>`;
  return `<span class="spread-badge sp-high">HIGH</span>`;
}

async function refresh() {
  const r = await fetch('/api/crm/all').then(x=>x.json()).catch(()=>({}));
  const clients = r.clients||[];
  document.getElementById('h-cnt').textContent   = r.count||0;
  document.getElementById('h-upd').textContent   = r.total_updates||0;
  document.getElementById('h-multi').textContent = clients.filter(c=>c.ap_count>1).length;

  document.getElementById('client-list').innerHTML = clients.length
    ? clients.sort((a,b)=>b.ap_count-a.ap_count).map(c=>`
      <div class="client-card">
        <div class="client-header">
          <span class="client-mac">${c.mac}</span>
          <span class="client-info">${c.ap_count} AP view${c.ap_count!==1?'s':''} · ${c.frames} frames</span>
          ${spreadBadge(c.spread)}
          <span style="font-size:10px;color:var(--muted)">Best: ${c.best_rssi}dBm from ${c.best_ap}</span>
        </div>
        ${(c.aps||[]).sort((a,b)=>b.rssi-a.rssi).map(ap=>`
          <div class="ap-row">
            <span class="ap-name" title="${ap.bssid}">${ap.ssid||ap.bssid.substring(0,8)} ch${ap.ch}</span>
            <div class="rssi-bar-track">
              <div class="rssi-bar-fill" style="width:${rssiWidth(ap.rssi)}%;background:${rssiColor(ap.rssi)}"></div>
            </div>
            <span class="rssi-val" style="color:${rssiColor(ap.rssi)}">${ap.rssi}dBm</span>
            <span style="font-size:9px;color:var(--muted)">${sparkline(ap.hist)}</span>
          </div>`).join('')}
      </div>`).join('')
    : '<div style="color:var(--muted);text-align:center;padding:20px;">No client RSSI data yet.</div>';
}

async function clearData() {
  await fetch('/api/crm/clear',{method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 2500);
</script>
</body>
</html>
)rawhtml";

inline void wucrm_routes() {
  server.on("/client-rssi", HTTP_GET, []() {
    server.send_P(200, "text/html", WUCRM_PAGE);
  });
}

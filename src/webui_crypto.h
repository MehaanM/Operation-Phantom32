#pragma once
// webui_crypto.h — Crypto suite analysis web UI.
// Route: /crypto. Prefix: wucra_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUCRA_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Crypto — Phantom32</title>
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
.row2{display:grid;grid-template-columns:1fr 1fr;gap:14px;margin-bottom:14px;}
canvas{display:block;width:100%;background:var(--bg3);border-radius:4px;}
table{width:100%;border-collapse:collapse;font-size:12px;}
th{text-align:left;padding:6px 8px;font-size:10px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;}
td{padding:6px 8px;border-bottom:1px solid var(--border);}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
.score-bar{height:8px;border-radius:4px;background:var(--bg3);min-width:60px;}
.score-fill{height:100%;border-radius:4px;}
.badge{padding:1px 7px;border-radius:4px;font-size:10px;font-weight:600;}
.badge-safe{background:rgba(34,197,94,.15);color:var(--success);}
.badge-warn{background:rgba(245,158,11,.15);color:var(--warn);}
.badge-risk{background:rgba(239,68,68,.2);color:var(--danger);}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;margin-top:12px;}
@media(max-width:700px){.hero{grid-template-columns:repeat(3,1fr)}.row2{grid-template-columns:1fr}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/security">Security</a>
    <a href="/crypto" class="active">Crypto</a>
    <a href="/ssids">SSIDs</a>
    <a href="/threats">Threats</a>
  </nav>
</div>
<div class="container">
  <h1>Crypto Suite Analysis</h1>
  <p class="subtitle">RSN/WPA cipher distribution, AKM suites, PMF status, and downgrade risk detection.</p>

  <div class="hero">
    <div class="stat"><label>Total APs</label><div class="val" id="h-total">0</div></div>
    <div class="stat"><label>WPA3</label><div class="val" id="h-wpa3" style="color:var(--success)">0</div></div>
    <div class="stat"><label>Enterprise</label><div class="val" id="h-ent" style="color:var(--accent)">0</div></div>
    <div class="stat"><label>Downgrade Risk</label><div class="val" id="h-dg" style="color:var(--warn)">0</div></div>
    <div class="stat"><label>Open / WEP</label><div class="val" id="h-weak" style="color:var(--danger)">0</div></div>
  </div>

  <div class="row2">
    <div class="section">
      <h3>Security Distribution</h3>
      <canvas id="sec-canvas" height="150"></canvas>
    </div>
    <div class="section">
      <h3>PMF (Management Frame Protection)</h3>
      <div id="pmf-grid" style="display:grid;grid-template-columns:1fr 1fr 1fr;gap:12px;padding:8px 0;"></div>
    </div>
  </div>

  <div class="section">
    <h3>Weakest APs (by crypto score)</h3>
    <table>
      <thead><tr>
        <th>BSSID</th><th>SSID</th><th>Score</th><th>Strength</th><th>Issues</th><th>PMF</th>
      </tr></thead>
      <tbody id="ap-tbody">
        <tr><td colspan="6" style="color:var(--muted);text-align:center">Loading…</td></tr>
      </tbody>
    </table>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
    </div>
  </div>
</div>

<script>
const AKM_NAMES = {1:'802.1X',2:'PSK',3:'FT-802.1X',4:'FT-PSK',5:'802.1X-SHA256',
  6:'PSK-SHA256',7:'TDLS',8:'SAE (WPA3)',9:'FT-SAE',18:'OWE'};

function score_color(s) {
  if(s>=75) return 'var(--success)';
  if(s>=50) return 'var(--warn)';
  return 'var(--danger)';
}
function score_grade(s) {
  if(s>=90) return 'A+';
  if(s>=80) return 'A';
  if(s>=70) return 'B';
  if(s>=60) return 'C';
  if(s>=50) return 'D';
  return 'F';
}

function drawSecPie(canvas_id, open, wep, wpa2, wpa3, ent) {
  const c = document.getElementById(canvas_id);
  if(!c) return;
  const ctx = c.getContext('2d');
  const W = c.clientWidth||300;
  c.width = W;
  ctx.fillStyle='#1a1f2e';
  ctx.fillRect(0,0,W,150);

  const data   = [open, wep, wpa2, wpa3, ent];
  const colors = ['#ef4444','#f97316','#f59e0b','#22c55e','#00d4ff'];
  const labels = ['Open','WEP','WPA2','WPA3','Enterprise'];
  const total  = data.reduce((a,b)=>a+b,0);
  if(!total) return;

  const cx=W/2, cy=65, r=55;
  let angle=-Math.PI/2;
  data.forEach((v,i)=>{
    if(!v) return;
    const sweep=(v/total)*2*Math.PI;
    ctx.beginPath();
    ctx.moveTo(cx,cy);
    ctx.arc(cx,cy,r,angle,angle+sweep);
    ctx.closePath();
    ctx.fillStyle=colors[i];
    ctx.fill();
    angle+=sweep;
  });

  // Legend
  let lx=10;
  data.forEach((v,i)=>{
    if(!v) return;
    ctx.fillStyle=colors[i];
    ctx.fillRect(lx,130,8,8);
    ctx.fillStyle='#e2e8f0';
    ctx.font='10px system-ui';
    ctx.fillText(labels[i]+' ('+v+')',lx+10,138);
    lx+=80;
  });
}

async function refresh() {
  const r = await fetch('/api/cra/all').then(x=>x.json()).catch(()=>({}));

  document.getElementById('h-total').textContent = r.ap_count||0;
  document.getElementById('h-wpa3').textContent  = r.wpa3||0;
  document.getElementById('h-ent').textContent   = r.enterprise||0;
  document.getElementById('h-dg').textContent    = r.downgrade_risk||0;
  const weak = (r.open||0)+(r.wep||0);
  document.getElementById('h-weak').textContent  = weak;

  drawSecPie('sec-canvas',
    r.open||0, r.wep||0,
    Math.max(0,(r.ap_count||0)-(r.open||0)-(r.wep||0)-(r.wpa3||0)-(r.enterprise||0)),
    r.wpa3||0, r.enterprise||0);

  const total = r.ap_count||1;
  document.getElementById('pmf-grid').innerHTML = [
    {label:'PMF Required',val:r.pmf_required||0,color:'var(--success)'},
    {label:'PMF Capable',val:r.pmf_capable||0,color:'var(--accent)'},
    {label:'No PMF',val:total-(r.pmf_capable||0),color:'var(--danger)'},
  ].map(m=>{
    const pct=Math.round(100*m.val/total);
    return `<div style="text-align:center">
      <div style="font-size:11px;color:var(--muted)">${m.label}</div>
      <div style="font-size:22px;font-weight:800;color:${m.color}">${m.val}</div>
      <div style="font-size:10px;color:var(--muted)">${pct}%</div>
    </div>`;
  }).join('');

  // AP table (sorted by score, worst first)
  const aps = (r.aps||[]).sort((a,b)=>a.score-b.score);
  document.getElementById('ap-tbody').innerHTML = aps.slice(0,20).map(ap=>{
    const sc = ap.score;
    const col = score_color(sc);
    const issues = [];
    if(ap.open) issues.push('Open');
    if(ap.wep) issues.push('WEP');
    if(ap.downgrade) issues.push('TKIP-DG');
    if(!ap.pmf_cap && !ap.open) issues.push('No-PMF');
    const badge = sc<50?'badge-risk':sc<75?'badge-warn':'badge-safe';
    const akm_names = (ap.akms||[]).map(a=>AKM_NAMES[a]||'AKM-'+a).join(', ');
    return `<tr>
      <td><code style="font-size:10px">${ap.bssid}</code></td>
      <td style="max-width:140px;overflow:hidden;text-overflow:ellipsis">${ap.ssid||'—'}</td>
      <td><span class="badge ${badge}">${score_grade(sc)} (${sc})</span></td>
      <td><div class="score-bar" style="width:80px"><div class="score-fill" style="width:${sc}%;background:${col}"></div></div></td>
      <td style="font-size:11px;color:var(--warn)">${issues.join(' · ')||'—'}</td>
      <td style="font-size:11px">${ap.pmf_req?'Required':ap.pmf_cap?'Capable':'None'}</td>
    </tr>`;
  }).join('');
}

refresh();
setInterval(refresh, 5000);
</script>
</body>
</html>
)rawhtml";

inline void wucra_routes() {
  server.on("/crypto", HTTP_GET, []() {
    server.send_P(200, "text/html", WUCRA_PAGE);
  });
}

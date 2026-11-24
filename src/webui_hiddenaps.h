#pragma once
// webui_hiddenaps.h — Hidden AP detection UI.
// Route: /hidden-aps. Prefix: wuhad_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUHAD_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Hidden AP Detector — Phantom32</title>
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
th{text-align:left;padding:7px 8px;font-size:10px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;}
td{padding:7px 8px;border-bottom:1px solid var(--border);vertical-align:middle;}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
.badge{padding:2px 7px;border-radius:4px;font-size:10px;font-weight:600;display:inline-block;}
.badge-hidden{background:rgba(239,68,68,.15);color:var(--danger);}
.badge-revealed{background:rgba(34,197,94,.15);color:var(--success);}
.badge-open{background:rgba(100,116,139,.12);color:var(--muted);}
.alias-list{display:flex;flex-wrap:wrap;gap:4px;}
.alias-pill{background:var(--bg3);border:1px solid var(--border);border-radius:4px;padding:1px 6px;font-size:10px;}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;margin-top:12px;}
.filter-row{display:flex;gap:8px;margin-bottom:12px;flex-wrap:wrap;}
select,input{background:var(--bg3);border:1px solid var(--border);border-radius:5px;padding:4px 8px;color:var(--text);font-size:12px;}
@media(max-width:700px){.hero{grid-template-columns:repeat(2,1fr)}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/devices">APs</a>
    <a href="/hidden-aps" class="active">Hidden APs</a>
    <a href="/anomaly">Anomaly</a>
    <a href="/probe-response">Probe Correlator</a>
  </nav>
</div>
<div class="container">
  <h1>Hidden AP Detector</h1>
  <p class="subtitle">Passively reveals hidden SSIDs from probe response frames. Hidden APs are listed even before revelation.</p>

  <div class="hero">
    <div class="stat"><label>Total APs</label><div class="val" id="h-total">0</div></div>
    <div class="stat"><label>Hidden APs</label><div class="val" id="h-hidden" style="color:var(--warn)">0</div></div>
    <div class="stat"><label>Revealed</label><div class="val" id="h-revealed" style="color:var(--success)">0</div></div>
    <div class="stat"><label>Probe Responses</label><div class="val" id="h-presps">0</div></div>
  </div>

  <div class="section">
    <h3>Detected APs</h3>
    <div class="filter-row">
      <select id="f-status">
        <option value="all">All APs</option>
        <option value="hidden">Hidden only</option>
        <option value="revealed">Revealed only</option>
      </select>
      <input id="f-search" type="text" placeholder="Search BSSID/SSID…" style="flex:1;min-width:140px">
    </div>
    <table>
      <thead><tr>
        <th>BSSID</th>
        <th>Status</th>
        <th>Known SSID(s)</th>
        <th>CH</th>
        <th>RSSI</th>
        <th>Beacons</th>
        <th>Probe Resps</th>
        <th>Revealed At</th>
      </tr></thead>
      <tbody id="ap-tbody">
        <tr><td colspan="8" style="color:var(--muted);text-align:center">Loading…</td></tr>
      </tbody>
    </table>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>
</div>

<script>
let g_aps = [];

function renderTable() {
  const fstatus = document.getElementById('f-status').value;
  const fsearch = document.getElementById('f-search').value.toLowerCase();

  const filtered = g_aps.filter(ap=>{
    if(fstatus==='hidden'   && !ap.hidden)   return false;
    if(fstatus==='revealed' && !ap.revealed) return false;
    if(fsearch && !ap.bssid.toLowerCase().includes(fsearch) &&
       !ap.ssid.toLowerCase().includes(fsearch)) return false;
    return true;
  });

  document.getElementById('ap-tbody').innerHTML = filtered.length
    ? filtered.map(ap=>{
        let badge = '';
        if(ap.hidden && !ap.revealed)
          badge = '<span class="badge badge-hidden">HIDDEN</span>';
        else if(ap.hidden && ap.revealed)
          badge = '<span class="badge badge-revealed">REVEALED</span>';
        else
          badge = '<span class="badge badge-open">Visible</span>';

        const aliases = ap.aliases.length
          ? `<div class="alias-list">${ap.aliases.map(s=>`<span class="alias-pill">${s}</span>`).join('')}</div>`
          : (ap.ssid ? ap.ssid : '<span style="color:var(--muted)">Unknown</span>');

        const rev_time = ap.discovery_ts
          ? new Date(ap.discovery_ts*1000).toLocaleTimeString()
          : '—';

        return `<tr>
          <td><code style="font-size:10px">${ap.bssid}</code></td>
          <td>${badge}</td>
          <td>${aliases}</td>
          <td>${ap.ch}</td>
          <td>${ap.rssi} dBm</td>
          <td>${ap.beacons}</td>
          <td>${ap.probe_resps}</td>
          <td>${rev_time}</td>
        </tr>`;
      }).join('')
    : '<tr><td colspan="8" style="color:var(--muted);text-align:center">No APs found.</td></tr>';
}

async function refresh() {
  const r = await fetch('/api/had/all').then(x=>x.json()).catch(()=>({}));
  document.getElementById('h-total').textContent   = r.ap_count||0;
  document.getElementById('h-hidden').textContent  = r.hidden_count||0;
  document.getElementById('h-revealed').textContent = r.revealed_count||0;
  document.getElementById('h-presps').textContent  = r.total_probe_resps||0;
  g_aps = (r.aps||[]).sort((a,b)=>{
    if(a.hidden && !b.hidden) return -1;
    if(!a.hidden && b.hidden) return 1;
    return b.beacons - a.beacons;
  });
  renderTable();
}

async function clearData() {
  await fetch('/api/had/clear',{method:'POST'});
  refresh();
}

document.getElementById('f-status').addEventListener('change', renderTable);
document.getElementById('f-search').addEventListener('input',  renderTable);

refresh();
setInterval(refresh, 4000);
</script>
</body>
</html>
)rawhtml";

inline void wuhad_routes() {
  server.on("/hidden-aps", HTTP_GET, []() {
    server.send_P(200, "text/html", WUHAD_PAGE);
  });
}

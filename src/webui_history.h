#pragma once
// webui_history.h — Scan history web UI.
// Route: /history. Prefix: wusch_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUSCH_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Scan History — Phantom32</title>
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
.filter-row{display:flex;gap:8px;margin-bottom:12px;flex-wrap:wrap;}
input[type=text]{background:var(--bg3);border:1px solid var(--border);border-radius:5px;
  padding:5px 10px;font-size:12px;color:var(--text);width:200px;}
input[type=text]:focus{outline:none;border-color:var(--accent);}
select{background:var(--bg3);border:1px solid var(--border);border-radius:5px;
  padding:5px 8px;font-size:12px;color:var(--text);}
table{width:100%;border-collapse:collapse;font-size:12px;}
th{text-align:left;padding:7px 8px;font-size:10px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;cursor:pointer;user-select:none;}
th:hover{color:var(--text);}
td{padding:6px 8px;border-bottom:1px solid var(--border);}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
.badge{padding:1px 7px;border-radius:4px;font-size:10px;font-weight:600;}
.b-open{background:rgba(239,68,68,.2);color:var(--danger);}
.b-wep{background:rgba(249,115,22,.2);color:#f97316;}
.b-wpa{background:rgba(245,158,11,.15);color:var(--warn);}
.b-wpa2{background:rgba(0,212,255,.1);color:var(--accent);}
.b-wpa3{background:rgba(34,197,94,.15);color:var(--success);}
.b-wps{background:rgba(245,158,11,.15);color:var(--warn);margin-left:4px;}
.b-pmf{background:rgba(34,197,94,.12);color:var(--success);margin-left:4px;}
.rssi-bar{display:inline-flex;align-items:center;gap:5px;}
.rssi-track{height:4px;width:40px;background:var(--bg3);border-radius:2px;}
.rssi-fill{height:100%;border-radius:2px;}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.btn-danger{background:var(--danger);color:#fff;}
.action-row{display:flex;gap:8px;margin-top:12px;}
@media(max-width:700px){.hero{grid-template-columns:repeat(3,1fr)}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/ssids">SSIDs</a>
    <a href="/history" class="active">History</a>
    <a href="/analytics">Analytics</a>
    <a href="/security">Security</a>
  </nav>
</div>
<div class="container">
  <h1>Scan History</h1>
  <p class="subtitle">Persistent record of all AP observations. Saved to LittleFS flash.</p>

  <div class="hero">
    <div class="stat"><label>Records</label><div class="val" id="h-count">0</div></div>
    <div class="stat"><label>Total Saved</label><div class="val" id="h-total">0</div></div>
    <div class="stat"><label>Open APs</label><div class="val" id="h-open" style="color:var(--danger)">0</div></div>
    <div class="stat"><label>WPA3</label><div class="val" id="h-wpa3" style="color:var(--success)">0</div></div>
    <div class="stat"><label>Flash</label><div class="val" id="h-lfs">?</div></div>
  </div>

  <div class="section">
    <h3>AP History</h3>
    <div class="filter-row">
      <input type="text" id="filter-ssid" placeholder="Filter SSID..." oninput="applyFilter()">
      <select id="filter-sec" onchange="applyFilter()">
        <option value="">All Security</option>
        <option value="Open">Open</option>
        <option value="WEP">WEP</option>
        <option value="WPA">WPA</option>
        <option value="WPA2">WPA2</option>
        <option value="WPA3">WPA3</option>
      </select>
      <select id="filter-ch" onchange="applyFilter()">
        <option value="">All Channels</option>
      </select>
    </div>
    <table>
      <thead><tr>
        <th>#</th>
        <th onclick="sort('bssid')">BSSID</th>
        <th onclick="sort('ssid')">SSID</th>
        <th onclick="sort('rssi')">RSSI</th>
        <th onclick="sort('channel')">CH</th>
        <th>Security</th>
        <th onclick="sort('ts')">Time</th>
      </tr></thead>
      <tbody id="ap-tbody">
        <tr><td colspan="7" style="color:var(--muted);text-align:center">Loading…</td></tr>
      </tbody>
    </table>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-danger" onclick="clearData()">Clear History</button>
    </div>
  </div>
</div>

<script>
let g_all = [];
let g_sort_key = 'ts';
let g_sort_asc = false;

function sec_badge(sec) {
  const cls = {'Open':'b-open','WEP':'b-wep','WPA':'b-wpa','WPA2':'b-wpa2','WPA3':'b-wpa3'}[sec]||'b-wpa';
  return `<span class="badge ${cls}">${sec}</span>`;
}

function rssi_bar(rssi) {
  const pct = Math.max(0,Math.min(100,(rssi+100)*1.67));
  const col = pct>60?'var(--success)':pct>35?'var(--warn)':'var(--danger)';
  return `<div class="rssi-bar">
    <span style="color:${col}">${rssi}</span>
    <div class="rssi-track"><div class="rssi-fill" style="width:${pct}%;background:${col}"></div></div>
  </div>`;
}

function sort(key) {
  if (g_sort_key === key) g_sort_asc = !g_sort_asc;
  else { g_sort_key = key; g_sort_asc = true; }
  applyFilter();
}

function applyFilter() {
  const f_ssid = document.getElementById('filter-ssid').value.toLowerCase();
  const f_sec  = document.getElementById('filter-sec').value;
  const f_ch   = document.getElementById('filter-ch').value;

  let list = g_all.filter(r=>{
    if (f_ssid && !r.ssid.toLowerCase().includes(f_ssid)) return false;
    if (f_sec  && r.sec !== f_sec)  return false;
    if (f_ch   && String(r.ch) !== f_ch) return false;
    return true;
  });

  list.sort((a,b)=>{
    const va = a[g_sort_key], vb = b[g_sort_key];
    if (typeof va === 'number') return g_sort_asc ? va-vb : vb-va;
    return g_sort_asc ? String(va).localeCompare(String(vb)) : String(vb).localeCompare(String(va));
  });

  document.getElementById('ap-tbody').innerHTML = list.length
    ? list.map((r,i)=>`<tr>
        <td style="color:var(--muted)">${i+1}</td>
        <td><code style="font-size:10px">${r.bssid}</code></td>
        <td>${r.ssid||'<em style="color:var(--muted)">hidden</em>'}</td>
        <td>${rssi_bar(r.rssi)}</td>
        <td>${r.ch}</td>
        <td>${sec_badge(r.sec)}${r.wps?'<span class="badge b-wps">WPS</span>':''}${r.pmf?'<span class="badge b-pmf">PMF</span>':''}</td>
        <td style="color:var(--muted)">${new Date(r.ts*1000).toLocaleTimeString()}</td>
      </tr>`).join('')
    : '<tr><td colspan="7" style="color:var(--muted);text-align:center">No results.</td></tr>';
}

async function refresh() {
  const stats = await fetch('/api/sch/stats').then(x=>x.json()).catch(()=>({}));
  const data  = await fetch('/api/sch/recent?limit=100').then(x=>x.json()).catch(()=>[]);

  document.getElementById('h-count').textContent = stats.count||0;
  document.getElementById('h-total').textContent = stats.total_saved||0;
  document.getElementById('h-open').textContent  = stats.open||0;
  document.getElementById('h-wpa3').textContent  = stats.wpa3||0;
  document.getElementById('h-lfs').textContent   = stats.lfs_ok?'OK':'N/A';

  g_all = data;

  // Populate channel filter
  const chs = [...new Set(data.map(r=>r.ch))].sort((a,b)=>a-b);
  const sel = document.getElementById('filter-ch');
  const cur_val = sel.value;
  sel.innerHTML = '<option value="">All Channels</option>' +
    chs.map(c=>`<option value="${c}" ${String(c)===cur_val?'selected':''}>Ch ${c}</option>`).join('');

  applyFilter();
}

async function clearData() {
  if(!confirm('Delete all scan history?')) return;
  await fetch('/api/sch/clear',{method:'POST'});
  g_all = [];
  refresh();
}

refresh();
setInterval(refresh, 5000);
</script>
</body>
</html>
)rawhtml";

inline void wusch_routes() {
  server.on("/history", HTTP_GET, []() {
    server.send_P(200, "text/html", WUSCH_PAGE);
  });
}

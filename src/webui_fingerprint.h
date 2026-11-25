#pragma once
// webui_fingerprint.h — AP Fingerprint UI.
// Route: /fingerprint. Prefix: wuapf_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUAPF_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>AP Fingerprint — Phantom32</title>
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
td{padding:7px 8px;border-bottom:1px solid var(--border);vertical-align:top;}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
.cap-tags{display:flex;flex-wrap:wrap;gap:3px;}
.cap{padding:1px 5px;border-radius:3px;font-size:9px;font-weight:700;}
.cap-on{background:rgba(34,197,94,.12);color:var(--success);}
.cap-off{background:rgba(42,48,69,.5);color:var(--muted);}
.conf-bar{display:flex;align-items:center;gap:5px;}
.conf-track{height:5px;width:60px;background:var(--bg3);border-radius:2px;}
.conf-fill{height:100%;border-radius:2px;}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;margin-top:12px;}
@media(max-width:700px){.hero{grid-template-columns:repeat(2,1fr)}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/analytics">Analytics</a>
    <a href="/fingerprint" class="active">Fingerprint</a>
    <a href="/crypto">Crypto</a>
    <a href="/security">Security</a>
  </nav>
</div>
<div class="container">
  <h1>AP Fingerprinting</h1>
  <p class="subtitle">Multi-signal AP identification: OUI, capability IEs, 802.11r/k/v, WiFi generations.</p>

  <div class="hero">
    <div class="stat"><label>APs Profiled</label><div class="val" id="h-aps">0</div></div>
    <div class="stat"><label>802.11r (FT)</label><div class="val" id="h-11r" style="color:var(--success)">0</div></div>
    <div class="stat"><label>802.11k (RRM)</label><div class="val" id="h-11k" style="color:var(--success)">0</div></div>
    <div class="stat"><label>WiFi 6 (HE)</label><div class="val" id="h-he" style="color:var(--accent)">0</div></div>
  </div>

  <div class="section">
    <h3>AP Profiles</h3>
    <table>
      <thead><tr>
        <th onclick="sort('bssid')">BSSID</th>
        <th onclick="sort('ssid')">SSID</th>
        <th>Vendor (guess)</th>
        <th>Confidence</th>
        <th>WiFi Gen</th>
        <th>Capabilities</th>
        <th>CC</th>
        <th onclick="sort('rssi')">RSSI</th>
      </tr></thead>
      <tbody id="ap-tbody">
        <tr><td colspan="8" style="color:var(--muted);text-align:center">Loading…</td></tr>
      </tbody>
    </table>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-ghost" onclick="clearData()">Reset</button>
    </div>
  </div>
</div>

<script>
let g_all = [];
let g_sort = {key:'rssi', asc:false};

function sort(key) {
  if(g_sort.key===key) g_sort.asc=!g_sort.asc;
  else {g_sort.key=key; g_sort.asc=false;}
  renderTable();
}

function wifi_gen(caps) {
  if(caps.he)  return '<span style="color:var(--accent)">WiFi 6 (HE)</span>';
  if(caps.vht) return '<span style="color:var(--success)">WiFi 5 (AC)</span>';
  if(caps.ht)  return '<span style="color:var(--warn)">WiFi 4 (N)</span>';
  return '<span style="color:var(--muted)">WiFi 3 (G/B)</span>';
}

function cap_tag(label, on) {
  return `<span class="cap ${on?'cap-on':'cap-off'}">${label}</span>`;
}

function conf_bar(c) {
  const col = c>=70?'var(--success)':c>=40?'var(--warn)':'var(--danger)';
  return `<div class="conf-bar">
    <span style="color:${col};min-width:28px">${c}%</span>
    <div class="conf-track"><div class="conf-fill" style="width:${c}%;background:${col}"></div></div>
  </div>`;
}

function renderTable() {
  const list = [...g_all].sort((a,b)=>{
    const va=a[g_sort.key], vb=b[g_sort.key];
    if(typeof va==='number') return g_sort.asc?va-vb:vb-va;
    return g_sort.asc?String(va).localeCompare(String(vb)):String(vb).localeCompare(String(va));
  });
  document.getElementById('ap-tbody').innerHTML = list.length
    ? list.map(ap=>`<tr>
        <td><code style="font-size:10px">${ap.bssid}</code></td>
        <td>${ap.ssid||'—'}</td>
        <td>${ap.vendor}</td>
        <td>${conf_bar(ap.conf)}</td>
        <td>${wifi_gen(ap.caps)}</td>
        <td><div class="cap-tags">
          ${cap_tag('FT',ap.caps.ft)}
          ${cap_tag('RRM',ap.caps.rrm)}
          ${cap_tag('BTM',ap.caps.bss_tm)}
          ${cap_tag('WPS',ap.caps.wps)}
          ${cap_tag('WPA3',ap.caps.wpa3)}
          ${cap_tag('PMF',ap.caps.pmf)}
        </div></td>
        <td>${ap.cc||'—'}</td>
        <td style="color:${ap.rssi>-70?'var(--success)':ap.rssi>-85?'var(--warn)':'var(--danger)'}">${ap.rssi}</td>
      </tr>`).join('')
    : '<tr><td colspan="8" style="color:var(--muted);text-align:center">No APs profiled.</td></tr>';
}

async function refresh() {
  const r = await fetch('/api/apf/all').then(x=>x.json()).catch(()=>({}));
  document.getElementById('h-aps').textContent = r.ap_count||0;
  document.getElementById('h-11r').textContent = r.with_11r||0;
  document.getElementById('h-11k').textContent = r.with_11k||0;
  document.getElementById('h-he').textContent  = r.with_he||0;
  g_all = (r.aps||[]).map(a=>({...a, vendor:a.vendor, conf:a.conf, caps:a.caps}));
  renderTable();
}

async function clearData() {
  await fetch('/api/apf/clear',{method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 5000);
</script>
</body>
</html>
)rawhtml";

inline void wuapf_routes() {
  server.on("/fingerprint", HTTP_GET, []() { server.send_P(200, "text/html", WUAPF_PAGE); });
}

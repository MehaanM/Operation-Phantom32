#pragma once
// webui_pmf.h — 802.11w PMF / network security summary UI.
// Route: /pmf. Prefix: wupmf_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUPMF_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>PMF Security — Phantom32</title>
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
table{width:100%;border-collapse:collapse;font-size:12px;}
th{text-align:left;padding:7px 8px;font-size:10px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;cursor:pointer;}
th:hover{color:var(--text);}
td{padding:7px 8px;border-bottom:1px solid var(--border);}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
.badge{padding:2px 7px;border-radius:4px;font-size:9px;font-weight:700;display:inline-block;margin:1px;}
.badge-pmf-req{background:rgba(34,197,94,.18);color:var(--success);}
.badge-pmf-cap{background:rgba(0,212,255,.12);color:var(--accent);}
.badge-pmf-no{background:rgba(239,68,68,.15);color:var(--danger);}
.badge-wpa3{background:rgba(168,85,247,.18);color:#a855f7;}
.badge-owe{background:rgba(236,72,153,.18);color:#ec4899;}
.badge-wpa2{background:rgba(245,158,11,.12);color:var(--warn);}
.badge-open{background:rgba(239,68,68,.2);color:var(--danger);}
.filter-row{display:flex;gap:8px;margin-bottom:12px;flex-wrap:wrap;}
input,select{background:var(--bg3);border:1px solid var(--border);border-radius:5px;padding:4px 8px;color:var(--text);font-size:12px;}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;margin-top:12px;}
.score-bar{display:flex;align-items:center;gap:5px;}
.sbar-wrap{background:var(--bg3);border-radius:4px;height:6px;width:60px;}
.sbar-fill{height:100%;border-radius:4px;}
@media(max-width:700px){.hero{grid-template-columns:1fr 1fr}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/security">Security</a>
    <a href="/pmf" class="active">PMF / 802.11w</a>
    <a href="/wpa3">WPA3</a>
    <a href="/beacon-quality">Beacons</a>
  </nav>
</div>
<div class="container">
  <h1>802.11w PMF Security Audit</h1>
  <p class="subtitle">Detects which APs support Protected Management Frames (802.11w), WPA3-SAE, OWE (Opportunistic Wireless Encryption), and their cipher suites.</p>

  <div class="hero">
    <div class="stat"><label>Total APs</label><div class="val" id="h-total">0</div></div>
    <div class="stat"><label>PMF Required</label><div class="val" id="h-req" style="color:var(--success)">0</div></div>
    <div class="stat"><label>PMF Capable</label><div class="val" id="h-cap" style="color:var(--accent)">0</div></div>
    <div class="stat"><label>WPA3</label><div class="val" id="h-wpa3" style="color:#a855f7">0</div></div>
    <div class="stat"><label>Unprotected</label><div class="val" id="h-open" style="color:var(--danger)">0</div></div>
  </div>

  <div class="section">
    <h3>AP Security Details</h3>
    <div class="filter-row">
      <input id="f-search" placeholder="Search…" style="flex:1;min-width:120px">
      <select id="f-pmf">
        <option value="">All</option>
        <option value="2">PMF Required</option>
        <option value="1">PMF Capable</option>
        <option value="0">No PMF</option>
      </select>
    </div>
    <table>
      <thead><tr>
        <th>BSSID</th>
        <th onclick="sortAPs('ssid')">SSID</th>
        <th>CH</th>
        <th onclick="sortAPs('rssi')">RSSI</th>
        <th>PMF</th>
        <th>Protocol</th>
        <th>AKM Suite</th>
        <th>Cipher</th>
        <th>Score</th>
      </tr></thead>
      <tbody id="ap-tbody">
        <tr><td colspan="9" style="color:var(--muted);text-align:center">Loading…</td></tr>
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
let g_sort = {key:'pmf_mode', asc:false};

function pmf_badge(mode) {
  if(mode===2) return '<span class="badge badge-pmf-req">PMF Required</span>';
  if(mode===1) return '<span class="badge badge-pmf-cap">PMF Capable</span>';
  return '<span class="badge badge-pmf-no">No PMF</span>';
}

function proto_badges(ap) {
  const b = [];
  if(ap.wpa3) b.push('<span class="badge badge-wpa3">WPA3</span>');
  if(ap.owe)  b.push('<span class="badge badge-owe">OWE</span>');
  if(ap.wpa2) b.push('<span class="badge badge-wpa2">WPA2</span>');
  if(!ap.has_rsn) b.push('<span class="badge badge-open">Open</span>');
  return b.join('') || '—';
}

function sec_score(ap) {
  let s = 0;
  if(ap.wpa3) s += 40;
  else if(ap.wpa2) s += 25;
  if(ap.pmf_mode === 2) s += 35;
  else if(ap.pmf_mode === 1) s += 20;
  if(ap.pairwise_cipher === 'CCMP' || ap.pairwise_cipher === 'GCMP') s += 25;
  else if(ap.pairwise_cipher === 'TKIP') s += 5;
  return Math.min(100, s);
}

function score_color(s) {
  if(s>=75) return 'var(--success)';
  if(s>=45) return 'var(--warn)';
  return 'var(--danger)';
}

function sortAPs(key) {
  if(g_sort.key===key) g_sort.asc=!g_sort.asc;
  else {g_sort.key=key; g_sort.asc=false;}
  renderAPs();
}

function renderAPs() {
  const fsearch = document.getElementById('f-search').value.toLowerCase();
  const fpmf    = document.getElementById('f-pmf').value;
  const filtered = g_aps.filter(a=>{
    if(fsearch && !a.bssid.toLowerCase().includes(fsearch) && !a.ssid.toLowerCase().includes(fsearch)) return false;
    if(fpmf !== '' && String(a.pmf_mode) !== fpmf) return false;
    return true;
  });
  const sorted = [...filtered].sort((a,b)=>{
    const va=a[g_sort.key],vb=b[g_sort.key];
    if(typeof va==='number') return g_sort.asc?va-vb:vb-va;
    return g_sort.asc?String(va).localeCompare(String(vb)):String(vb).localeCompare(String(va));
  });
  document.getElementById('ap-tbody').innerHTML = sorted.length
    ? sorted.map(a=>{
        const score = sec_score(a);
        return `<tr>
          <td><code style="font-size:10px">${a.bssid}</code></td>
          <td>${a.ssid||'—'}</td>
          <td>${a.ch}</td>
          <td>${a.rssi} dBm</td>
          <td>${pmf_badge(a.pmf_mode)}</td>
          <td>${proto_badges(a)}</td>
          <td style="font-size:10px">${a.akm}</td>
          <td style="font-size:10px">${a.pairwise_cipher}</td>
          <td>
            <div class="score-bar">
              <span style="color:${score_color(score)};min-width:28px;font-size:11px">${score}</span>
              <div class="sbar-wrap"><div class="sbar-fill" style="width:${score}%;background:${score_color(score)}"></div></div>
            </div>
          </td>
        </tr>`;
      }).join('')
    : '<tr><td colspan="9" style="color:var(--muted);text-align:center">No data.</td></tr>';
}

async function refresh() {
  const r = await fetch('/api/pmf/all').then(x=>x.json()).catch(()=>({}));
  g_aps = r.aps||[];
  document.getElementById('h-total').textContent = r.ap_count||0;
  document.getElementById('h-req').textContent   = r.pmf_required||0;
  document.getElementById('h-cap').textContent   = r.pmf_capable||0;
  document.getElementById('h-wpa3').textContent  = r.wpa3_count||0;
  document.getElementById('h-open').textContent  = r.unprotected||0;
  renderAPs();
}

async function clearData() {
  await fetch('/api/pmf/clear',{method:'POST'});
  refresh();
}

document.getElementById('f-search').addEventListener('input', renderAPs);
document.getElementById('f-pmf').addEventListener('change', renderAPs);

refresh();
setInterval(refresh, 5000);
</script>
</body>
</html>
)rawhtml";

inline void wupmf_routes() {
  server.on("/pmf", HTTP_GET, []() { server.send_P(200, "text/html", WUPMF_PAGE); });
}

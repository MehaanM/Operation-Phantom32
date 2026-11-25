#pragma once
// webui_ap_capability.h — AP WiFi generation and capability matrix UI.
// Route: /ap-capabilities. Prefix: wuapc_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUAPC_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>AP Capabilities — Phantom32</title>
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
.container{max-width:1200px;margin:0 auto;padding:20px 16px;}
h1{font-size:22px;font-weight:700;margin-bottom:4px;}
.subtitle{color:var(--muted);font-size:13px;margin-bottom:20px;}
.hero{display:grid;grid-template-columns:repeat(5,1fr);gap:10px;margin-bottom:16px;}
.stat{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:13px;}
.stat label{font-size:10px;color:var(--muted);text-transform:uppercase;}
.stat .val{font-size:22px;font-weight:800;color:var(--accent);margin-top:3px;}
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:18px;margin-bottom:14px;}
.section h3{font-size:12px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:14px;}
table{width:100%;border-collapse:collapse;font-size:11px;}
th{text-align:left;padding:7px 6px;font-size:9px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;cursor:pointer;white-space:nowrap;}
th:hover{color:var(--text);}
td{padding:6px 6px;border-bottom:1px solid var(--border);}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
.badge{padding:1px 5px;border-radius:3px;font-size:9px;font-weight:700;display:inline-block;}
.check{color:var(--success);font-size:11px;}
.cross{color:var(--danger);font-size:11px;opacity:.4;}
.gen-6{background:rgba(168,85,247,.18);color:#a855f7;}
.gen-5{background:rgba(0,212,255,.12);color:var(--accent);}
.gen-4{background:rgba(34,197,94,.12);color:var(--success);}
.gen-3{background:rgba(100,116,139,.18);color:var(--muted);}
.rate-bar{display:flex;align-items:center;gap:5px;}
.rbar{background:var(--bg3);border-radius:3px;height:5px;width:60px;}
.rfill{height:100%;border-radius:3px;background:var(--accent);}
.filter-row{display:flex;gap:8px;margin-bottom:12px;}
input,select{background:var(--bg3);border:1px solid var(--border);border-radius:5px;padding:4px 8px;color:var(--text);font-size:12px;}
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
    <a href="/analytics">Analytics</a>
    <a href="/ap-capabilities" class="active">AP Caps</a>
    <a href="/pmf">PMF/WPA3</a>
    <a href="/fingerprint">Fingerprint</a>
  </nav>
</div>
<div class="container">
  <h1>AP Capability Matrix</h1>
  <p class="subtitle">Decodes 802.11 HT/VHT/HE IEs to show each AP's WiFi generation, spatial streams, beamforming support, guard intervals, and theoretical max throughput.</p>

  <div class="hero">
    <div class="stat"><label>Total APs</label><div class="val" id="h-total">0</div></div>
    <div class="stat"><label>WiFi 6 (ax)</label><div class="val" id="h-6" style="color:#a855f7">0</div></div>
    <div class="stat"><label>WiFi 5 (ac)</label><div class="val" id="h-5" style="color:var(--accent)">0</div></div>
    <div class="stat"><label>WiFi 4 (n)</label><div class="val" id="h-4" style="color:var(--success)">0</div></div>
    <div class="stat"><label>Legacy</label><div class="val" id="h-leg" style="color:var(--muted)">0</div></div>
  </div>

  <div class="section">
    <h3>AP Capabilities</h3>
    <div class="filter-row">
      <input id="f-search" placeholder="Search…" style="flex:1;min-width:120px">
      <select id="f-gen">
        <option value="">All Generations</option>
        <option value="6">WiFi 6 (ax)</option>
        <option value="5">WiFi 5 (ac)</option>
        <option value="4">WiFi 4 (n)</option>
        <option value="3">Legacy</option>
      </select>
    </div>
    <div style="overflow-x:auto">
    <table>
      <thead><tr>
        <th>BSSID</th><th onclick="sortAPs('ssid')">SSID</th><th>CH</th>
        <th onclick="sortAPs('rssi')">RSSI</th>
        <th onclick="sortAPs('gen')">Gen</th>
        <th onclick="sortAPs('streams')">SS</th>
        <th onclick="sortAPs('max_rate')">Max Rate</th>
        <th>HT</th><th>VHT</th><th>HE</th>
        <th>SGI20</th><th>SGI40</th><th>40MHz</th>
        <th>160MHz</th><th>LDPC</th>
        <th>BF</th><th>MU-BF</th>
        <th>TWT</th><th>OFDMA</th>
        <th>TXSTBC</th>
      </tr></thead>
      <tbody id="ap-tbody">
        <tr><td colspan="20" style="color:var(--muted);text-align:center">Loading…</td></tr>
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
let g_aps = [];
let g_sort = {key:'gen', asc:false};

const GEN_CLASS = {6:'gen-6',5:'gen-5',4:'gen-4',3:'gen-3'};
const CHECK = '<span class="check">✓</span>';
const CROSS = '<span class="cross">✕</span>';

function sortAPs(key) {
  if(g_sort.key===key) g_sort.asc=!g_sort.asc;
  else {g_sort.key=key; g_sort.asc=false;}
  renderAPs();
}

function fmtRate(r) {
  if(r>=1000) return (r/1000).toFixed(1)+'Gbps';
  return r+'Mbps';
}

function renderAPs() {
  const fsearch = document.getElementById('f-search').value.toLowerCase();
  const fgen    = document.getElementById('f-gen').value;
  const filtered = g_aps.filter(a=>{
    if(fsearch && !a.bssid.toLowerCase().includes(fsearch) && !a.ssid.toLowerCase().includes(fsearch)) return false;
    if(fgen && String(a.gen) !== fgen) return false;
    return true;
  });
  const sorted = [...filtered].sort((a,b)=>{
    const va=a[g_sort.key],vb=b[g_sort.key];
    if(typeof va==='number') return g_sort.asc?va-vb:vb-va;
    return g_sort.asc?String(va).localeCompare(String(vb)):String(vb).localeCompare(String(va));
  });
  const max_rate = Math.max(...sorted.map(a=>a.max_rate||0), 1);
  document.getElementById('ap-tbody').innerHTML = sorted.length
    ? sorted.map(a=>`<tr>
        <td><code style="font-size:9px">${a.bssid}</code></td>
        <td>${a.ssid||'—'}</td><td>${a.ch}</td><td>${a.rssi}dBm</td>
        <td><span class="badge ${GEN_CLASS[a.gen]||'gen-3'}">${a.gen_str}</span></td>
        <td style="font-weight:700">${a.streams||1}</td>
        <td>
          <div class="rate-bar">
            <span style="font-size:10px;min-width:52px">${fmtRate(a.max_rate)}</span>
            <div class="rbar"><div class="rfill" style="width:${Math.round((a.max_rate/max_rate)*100)}%"></div></div>
          </div>
        </td>
        <td>${a.has_ht?CHECK:CROSS}</td>
        <td>${a.has_vht?CHECK:CROSS}</td>
        <td>${a.has_he?CHECK:CROSS}</td>
        <td>${a.ht_sgi20?CHECK:CROSS}</td>
        <td>${a.ht_sgi40?CHECK:CROSS}</td>
        <td>${a.ht_40mhz?CHECK:CROSS}</td>
        <td>${a.vht_160?CHECK:CROSS}</td>
        <td>${a.ht_ldpc?CHECK:CROSS}</td>
        <td>${a.vht_bfee?CHECK:CROSS}</td>
        <td>${a.vht_bfee&&a.has_he?CHECK:CROSS}</td>
        <td>${a.he_twt?CHECK:CROSS}</td>
        <td>${a.he_ofdma?CHECK:CROSS}</td>
        <td>${(a.ht_ldpc||a.vht_bfee)?CHECK:CROSS}</td>
      </tr>`).join('')
    : '<tr><td colspan="20" style="color:var(--muted);text-align:center">No APs.</td></tr>';
}

async function refresh() {
  const r = await fetch('/api/apc/all').then(x=>x.json()).catch(()=>({}));
  g_aps = r.aps||[];
  document.getElementById('h-total').textContent = r.ap_count||0;
  document.getElementById('h-6').textContent     = r.wifi6||0;
  document.getElementById('h-5').textContent     = r.wifi5||0;
  document.getElementById('h-4').textContent     = r.wifi4||0;
  document.getElementById('h-leg').textContent   = r.legacy||0;
  renderAPs();
}

async function clearData() {
  await fetch('/api/apc/clear',{method:'POST'});
  refresh();
}

document.getElementById('f-search').addEventListener('input', renderAPs);
document.getElementById('f-gen').addEventListener('change', renderAPs);

refresh();
setInterval(refresh, 5000);
</script>
</body>
</html>
)rawhtml";

inline void wuapc_routes() {
  server.on("/ap-capabilities", HTTP_GET, []() { server.send_P(200, "text/html", WUAPC_PAGE); });
}

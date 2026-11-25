#pragma once
// webui_ap_vendor_map.h — AP vendor classification UI.
// Route: /ap-vendors. Prefix: wuavm_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUAVM_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>AP Vendor Map — Phantom32</title>
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
.badge{padding:2px 6px;border-radius:3px;font-size:9px;font-weight:700;display:inline-block;}
.t-router{background:rgba(0,212,255,.12);color:var(--accent);}
.t-ent{background:rgba(168,85,247,.18);color:#a855f7;}
.t-iot{background:rgba(34,197,94,.12);color:var(--success);}
.t-virt{background:rgba(239,68,68,.12);color:var(--danger);}
.t-mobile{background:rgba(245,158,11,.12);color:var(--warn);}
.t-other{background:rgba(100,116,139,.12);color:var(--muted);}
.trust-bar{display:flex;align-items:center;gap:5px;}
.tbar-track{flex:1;background:var(--bg3);border-radius:3px;height:6px;min-width:50px;}
.tbar-fill{height:100%;border-radius:3px;background:var(--success);}
.filter-row{display:flex;gap:8px;margin-bottom:12px;flex-wrap:wrap;}
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
    <a href="/ap-vendors" class="active">AP Vendors</a>
    <a href="/fingerprint">Fingerprint</a>
    <a href="/vendor">Vendor Stats</a>
  </nav>
</div>
<div class="container">
  <h1>AP Vendor Classification</h1>
  <p class="subtitle">OUI-based AP vendor identification and device type classification. Trust score reflects security posture and vendor reputation.</p>

  <div class="hero">
    <div class="stat"><label>Total APs</label><div class="val" id="h-cnt">0</div></div>
    <div class="stat"><label>Enterprise</label><div class="val" id="h-ent" style="color:#a855f7">0</div></div>
    <div class="stat"><label>Consumer</label><div class="val" id="h-rtr" style="color:var(--accent)">0</div></div>
    <div class="stat"><label>Virtual/Unknown</label><div class="val" id="h-oth" style="color:var(--warn)">0</div></div>
  </div>

  <div class="section">
    <h3>AP Vendor Table</h3>
    <div class="filter-row">
      <input id="f-search" placeholder="Search…" style="flex:1;min-width:120px">
      <select id="f-type">
        <option value="">All Types</option>
        <option value="Router">Router</option>
        <option value="Enterprise">Enterprise</option>
        <option value="IoT">IoT</option>
        <option value="Virtual">Virtual</option>
        <option value="Mobile">Mobile</option>
      </select>
    </div>
    <div style="overflow-x:auto">
    <table>
      <thead><tr>
        <th>BSSID</th>
        <th onclick="sortAPs('ssid')">SSID</th>
        <th>CH</th>
        <th onclick="sortAPs('rssi')">RSSI</th>
        <th onclick="sortAPs('vendor')">Vendor</th>
        <th>Product</th>
        <th onclick="sortAPs('type_code')">Type</th>
        <th onclick="sortAPs('gen')">WiFi Gen</th>
        <th>WPS</th><th>PMF</th><th>Sec</th>
        <th onclick="sortAPs('trust')">Trust</th>
      </tr></thead>
      <tbody id="ap-tbody">
        <tr><td colspan="12" style="color:var(--muted);text-align:center">Loading…</td></tr>
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
let g_sort = {key:'trust', asc:false};

const SEC = ['Open','WEP','WPA','WPA2','WPA3'];
const GEN = ['','','','Legacy','WiFi 4','WiFi 5','WiFi 6','WiFi 7'];
const TYPE_CLASS = {Router:'t-router',Enterprise:'t-ent',IoT:'t-iot',Virtual:'t-virt',Mobile:'t-mobile'};

function sortAPs(k) {
  if(g_sort.key===k) g_sort.asc=!g_sort.asc; else {g_sort.key=k; g_sort.asc=false;}
  renderAPs();
}

function renderAPs() {
  const fsearch = document.getElementById('f-search').value.toLowerCase();
  const ftype   = document.getElementById('f-type').value;
  const filtered = g_aps.filter(a=>{
    if(fsearch && !a.bssid.toLowerCase().includes(fsearch) &&
       !a.ssid.toLowerCase().includes(fsearch) &&
       !a.vendor.toLowerCase().includes(fsearch)) return false;
    if(ftype && a.type !== ftype) return false;
    return true;
  });
  const sorted = [...filtered].sort((a,b)=>{
    const va=a[g_sort.key],vb=b[g_sort.key];
    if(typeof va==='number') return g_sort.asc?va-vb:vb-va;
    return g_sort.asc?String(va).localeCompare(String(vb)):String(vb).localeCompare(String(va));
  });
  const CHECK = '<span style="color:var(--success)">✓</span>';
  const CROSS = '<span style="color:var(--danger);opacity:.4">✕</span>';
  document.getElementById('ap-tbody').innerHTML = sorted.length
    ? sorted.map(a=>`<tr>
        <td><code style="font-size:10px">${a.bssid}</code></td>
        <td>${a.ssid||'—'}</td>
        <td>${a.ch}</td>
        <td>${a.rssi}dBm</td>
        <td>${a.vendor}</td>
        <td style="font-size:10px;color:var(--muted)">${a.product||'—'}</td>
        <td><span class="badge ${TYPE_CLASS[a.type]||'t-other'}">${a.type}</span></td>
        <td style="font-size:11px">${GEN[a.gen]||'Legacy'}</td>
        <td>${a.wps?CHECK:CROSS}</td>
        <td>${a.pmf?CHECK:CROSS}</td>
        <td>${SEC[a.sec]||'?'}</td>
        <td>
          <div class="trust-bar">
            <span style="font-size:10px;min-width:22px">${a.trust}</span>
            <div class="tbar-track"><div class="tbar-fill" style="width:${a.trust}%;background:${a.trust>=70?'var(--success)':a.trust>=40?'var(--warn)':'var(--danger)'}"></div></div>
          </div>
        </td>
      </tr>`).join('')
    : '<tr><td colspan="12" style="color:var(--muted);text-align:center">No APs.</td></tr>';
}

async function refresh() {
  const r = await fetch('/api/avm/all').then(x=>x.json()).catch(()=>({}));
  g_aps = r.aps||[];
  document.getElementById('h-cnt').textContent = r.count||0;
  document.getElementById('h-ent').textContent = g_aps.filter(a=>a.type==='Enterprise').length;
  document.getElementById('h-rtr').textContent = g_aps.filter(a=>a.type==='Router').length;
  document.getElementById('h-oth').textContent = g_aps.filter(a=>a.type==='Virtual'||a.type==='Unknown').length;
  renderAPs();
}

async function clearData() {
  await fetch('/api/avm/clear',{method:'POST'});
  refresh();
}

document.getElementById('f-search').addEventListener('input', renderAPs);
document.getElementById('f-type').addEventListener('change', renderAPs);
refresh();
setInterval(refresh, 4000);
</script>
</body>
</html>
)rawhtml";

inline void wuavm_routes() {
  server.on("/ap-vendors", HTTP_GET, []() { server.send_P(200, "text/html", WUAVM_PAGE); });
}

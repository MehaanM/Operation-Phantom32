#pragma once
// webui_rssi_tracker.h — RSSI trend analysis UI.
// Route: /rssi-tracker. Prefix: wuart_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUART_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>RSSI Tracker — Phantom32</title>
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
table{width:100%;border-collapse:collapse;font-size:12px;}
th{text-align:left;padding:7px 8px;font-size:10px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;cursor:pointer;}
th:hover{color:var(--text);}
td{padding:7px 8px;border-bottom:1px solid var(--border);}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
.rssi-bar{display:inline-flex;align-items:center;gap:5px;}
.rbar-wrap{background:var(--bg3);border-radius:4px;height:7px;width:70px;}
.rbar-fill{height:100%;border-radius:4px;}
.spark{display:flex;align-items:flex-end;gap:1px;height:22px;}
.sbar{width:3px;border-radius:1px;min-height:1px;}
.trend-arrow{font-size:12px;}
.badge{padding:2px 7px;border-radius:4px;font-size:9px;font-weight:700;display:inline-block;}
.stab-bar{display:inline-flex;align-items:center;gap:5px;}
.stab-wrap{background:var(--bg3);border-radius:4px;height:5px;width:50px;}
.stab-fill{height:100%;border-radius:4px;}
.filter-row{display:flex;gap:8px;margin-bottom:12px;flex-wrap:wrap;}
input,select{background:var(--bg3);border:1px solid var(--border);border-radius:5px;padding:4px 8px;color:var(--text);font-size:12px;}
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
    <a href="/analytics">Analytics</a>
    <a href="/rssi-tracker" class="active">RSSI Trends</a>
    <a href="/signal-map">Signal Map</a>
    <a href="/beacon-quality">Beacon Quality</a>
  </nav>
</div>
<div class="container">
  <h1>RSSI Trend Tracker</h1>
  <p class="subtitle">Monitors per-AP RSSI over time with EMA smoothing, min/max/avg, trend direction, and stability scoring.</p>

  <div class="hero">
    <div class="stat"><label>APs Tracked</label><div class="val" id="h-aps">0</div></div>
    <div class="stat"><label>Strongest AP</label><div class="val" id="h-best" style="color:var(--success)">—</div></div>
    <div class="stat"><label>Weakest AP</label><div class="val" id="h-worst" style="color:var(--danger)">—</div></div>
  </div>

  <div class="section">
    <h3>AP RSSI Details</h3>
    <div class="filter-row">
      <input id="f-search" placeholder="Search…" style="flex:1;min-width:120px">
      <select id="f-trend">
        <option value="">All Trends</option>
        <option value="rising">Rising</option>
        <option value="falling">Falling</option>
        <option value="stable">Stable</option>
      </select>
    </div>
    <table>
      <thead><tr>
        <th onclick="sortAPs('bssid')">BSSID</th>
        <th onclick="sortAPs('ssid')">SSID</th>
        <th>CH</th>
        <th onclick="sortAPs('rssi')">RSSI</th>
        <th>Range</th>
        <th onclick="sortAPs('trend')">Trend</th>
        <th onclick="sortAPs('stability')">Stability</th>
        <th>Jitter</th>
        <th>120s History</th>
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
let g_sort = {key:'rssi', asc:false};

const TREND_ICONS = {rising:'▲', falling:'▼', stable:'→', unknown:'?'};
const TREND_COLORS = {rising:'var(--success)', falling:'var(--danger)', stable:'var(--muted)', unknown:'var(--muted)'};

function rssi_color(v) {
  if(v>=-65) return 'var(--success)';
  if(v>=-75) return 'var(--warn)';
  return 'var(--danger)';
}

function rssi_pct(v) { return Math.max(0, Math.min(100, (v+100)*2)); }

function sparkline(hist) {
  const vals = hist.filter(v=>v!==0);
  if(!vals.length) return '<div style="color:var(--muted);font-size:9px">—</div>';
  const min_v = Math.min(...vals), max_v = Math.max(...vals);
  const range = max_v - min_v || 1;
  return '<div class="spark">' + hist.map(v=>{
    if(!v) return '<div class="sbar" style="height:1px;opacity:.2;background:var(--muted)"></div>';
    const h = Math.round(((v-min_v)/range)*20)+2;
    return `<div class="sbar" style="height:${h}px;background:${rssi_color(v)}"></div>`;
  }).join('') + '</div>';
}

function sortAPs(key) {
  if(g_sort.key===key) g_sort.asc=!g_sort.asc;
  else {g_sort.key=key; g_sort.asc=false;}
  renderAPs();
}

function renderAPs() {
  const fsearch = document.getElementById('f-search').value.toLowerCase();
  const ftrend  = document.getElementById('f-trend').value;
  const filtered = g_aps.filter(a=>{
    if(fsearch && !a.bssid.toLowerCase().includes(fsearch) && !a.ssid.toLowerCase().includes(fsearch)) return false;
    if(ftrend && a.trend !== ftrend) return false;
    return true;
  });
  const sorted = [...filtered].sort((a,b)=>{
    const va=a[g_sort.key],vb=b[g_sort.key];
    if(typeof va==='number') return g_sort.asc?va-vb:vb-va;
    return g_sort.asc?String(va).localeCompare(String(vb)):String(vb).localeCompare(String(va));
  });
  document.getElementById('ap-tbody').innerHTML = sorted.length
    ? sorted.map(a=>`<tr>
        <td><code style="font-size:10px">${a.bssid}</code></td>
        <td>${a.ssid||'—'}</td>
        <td>${a.ch}</td>
        <td>
          <div class="rssi-bar">
            <span style="color:${rssi_color(a.rssi)};min-width:36px">${a.rssi} dBm</span>
            <div class="rbar-wrap"><div class="rbar-fill" style="width:${rssi_pct(a.rssi)}%;background:${rssi_color(a.rssi)}"></div></div>
          </div>
        </td>
        <td style="font-size:10px;color:var(--muted)">${a.rssi_min}/${a.rssi_max}</td>
        <td>
          <span class="trend-arrow" style="color:${TREND_COLORS[a.trend]}">${TREND_ICONS[a.trend]||'?'}</span>
          <span style="font-size:10px;color:${TREND_COLORS[a.trend]}">${a.trend}</span>
          ${a.trend_delta?`<span style="font-size:9px;color:var(--muted)">(${a.trend_delta>0?'+':''}${a.trend_delta}dB)</span>`:''}
        </td>
        <td>
          <div class="stab-bar">
            <span style="color:${a.stability>70?'var(--success)':a.stability>40?'var(--warn)':'var(--danger)'};min-width:28px">${a.stability}%</span>
            <div class="stab-wrap"><div class="stab-fill" style="width:${a.stability}%;background:${a.stability>70?'var(--success)':a.stability>40?'var(--warn)':'var(--danger)'}"></div></div>
          </div>
        </td>
        <td style="color:var(--muted);font-size:11px">±${a.jitter} dBm</td>
        <td>${sparkline(a.hist||[])}</td>
      </tr>`).join('')
    : '<tr><td colspan="9" style="color:var(--muted);text-align:center">No data.</td></tr>';
}

async function refresh() {
  const r = await fetch('/api/art/all').then(x=>x.json()).catch(()=>({}));
  g_aps = r.aps||[];
  document.getElementById('h-aps').textContent = r.ap_count||0;
  if(g_aps.length) {
    const best  = g_aps.reduce((m,a)=>a.rssi>m.rssi?a:m, g_aps[0]);
    const worst = g_aps.reduce((m,a)=>a.rssi<m.rssi?a:m, g_aps[0]);
    document.getElementById('h-best').textContent  = best.rssi+' dBm';
    document.getElementById('h-worst').textContent = worst.rssi+' dBm';
  }
  renderAPs();
}

async function clearData() {
  await fetch('/api/art/clear',{method:'POST'});
  refresh();
}

document.getElementById('f-search').addEventListener('input', renderAPs);
document.getElementById('f-trend').addEventListener('change', renderAPs);

refresh();
setInterval(refresh, 2000);
</script>
</body>
</html>
)rawhtml";

inline void wuart_routes() {
  server.on("/rssi-tracker", HTTP_GET, []() { server.send_P(200, "text/html", WUART_PAGE); });
}

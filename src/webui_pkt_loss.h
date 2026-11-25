#pragma once
// webui_pkt_loss.h — Packet loss / retry rate UI.
// Route: /pkt-loss. Prefix: wuplt_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUPLT_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Packet Loss Tracker — Phantom32</title>
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
.retry-bar{display:flex;align-items:center;gap:5px;}
.rbar-track{flex:1;background:var(--bg3);border-radius:3px;height:7px;min-width:50px;}
.rbar-fill{height:100%;border-radius:3px;transition:width .3s;}
.spark{display:inline-flex;align-items:flex-end;gap:1px;height:18px;}
.spark-bar{width:3px;border-radius:1px;}
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
    <a href="/pkt-loss" class="active">Pkt Loss</a>
    <a href="/retransmission">Retx</a>
    <a href="/protocol">Protocol</a>
  </nav>
</div>
<div class="container">
  <h1>Packet Loss / Retry Rate</h1>
  <p class="subtitle">Monitors 802.11 retry flag in frame control. High retry rates indicate congestion, interference, or weak signal.</p>

  <div class="hero">
    <div class="stat"><label>APs Tracked</label><div class="val" id="h-cnt">0</div></div>
    <div class="stat"><label>Total Frames</label><div class="val" id="h-total">0</div></div>
    <div class="stat"><label>Retry Frames</label><div class="val" id="h-retry" style="color:var(--warn)">0</div></div>
    <div class="stat"><label>Global Retry %</label><div class="val" id="h-pct" style="color:var(--danger)">0%</div></div>
  </div>

  <div class="section">
    <h3>Per-AP Retry Rate</h3>
    <table>
      <thead><tr>
        <th>BSSID</th>
        <th onclick="sortAPs('ssid')">SSID</th>
        <th>CH</th>
        <th onclick="sortAPs('rssi')">RSSI</th>
        <th onclick="sortAPs('total')">Total</th>
        <th onclick="sortAPs('retry')">Retries</th>
        <th onclick="sortAPs('retry_rate')">Rate</th>
        <th>60s History</th>
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
let g_sort = {key:'retry_rate', asc:false};

function sortAPs(k) {
  if(g_sort.key===k) g_sort.asc=!g_sort.asc; else {g_sort.key=k;g_sort.asc=false;}
  renderAPs();
}

function retryColor(r) {
  if (r < 5)  return 'var(--success)';
  if (r < 15) return 'var(--warn)';
  return 'var(--danger)';
}

function sparkline(data, colorFn) {
  if (!data || !data.length) return '';
  const max = Math.max(...data, 1);
  return '<div class="spark">' + data.map(v=>{
    const h = Math.round((v/max)*16)+2;
    return `<div class="spark-bar" style="height:${h}px;background:${colorFn(v)}"></div>`;
  }).join('') + '</div>';
}

function fmtBytes(b) {
  if(b>1048576) return (b/1048576).toFixed(1)+'MB';
  if(b>1024)    return (b/1024).toFixed(1)+'KB';
  return b+'B';
}

function renderAPs() {
  const sorted = [...g_aps].sort((a,b)=>{
    const va=a[g_sort.key],vb=b[g_sort.key];
    if(typeof va==='number') return g_sort.asc?va-vb:vb-va;
    return g_sort.asc?String(va).localeCompare(String(vb)):String(vb).localeCompare(String(va));
  });
  document.getElementById('ap-tbody').innerHTML = sorted.length
    ? sorted.map(a=>{
        const pct = (a.retry_rate / 100).toFixed(1);
        return `<tr>
          <td><code style="font-size:10px">${a.bssid}</code></td>
          <td>${a.ssid||'—'}</td>
          <td>${a.ch}</td>
          <td>${a.rssi}dBm</td>
          <td>${a.total}</td>
          <td style="color:${retryColor(pct)}">${a.retry}</td>
          <td>
            <div class="retry-bar">
              <span style="font-size:10px;min-width:36px;color:${retryColor(pct)}">${pct}%</span>
              <div class="rbar-track"><div class="rbar-fill" style="width:${Math.min(pct,100)}%;background:${retryColor(pct)}"></div></div>
            </div>
          </td>
          <td>${sparkline(a.hist_retry, v=>retryColor(v))}</td>
        </tr>`;
      }).join('')
    : '<tr><td colspan="8" style="color:var(--muted);text-align:center">No APs tracked.</td></tr>';
}

async function refresh() {
  const r = await fetch('/api/plt/all').then(x=>x.json()).catch(()=>({}));
  g_aps = r.aps||[];
  document.getElementById('h-cnt').textContent   = r.ap_count||0;
  document.getElementById('h-total').textContent = r.global_total||0;
  document.getElementById('h-retry').textContent = r.global_retry||0;
  document.getElementById('h-pct').textContent   = (r.global_retry_pct||0)+'%';
  renderAPs();
}

async function clearData() {
  await fetch('/api/plt/clear',{method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 2000);
</script>
</body>
</html>
)rawhtml";

inline void wuplt_routes() {
  server.on("/pkt-loss", HTTP_GET, []() { server.send_P(200, "text/html", WUPLT_PAGE); });
}

#pragma once
// webui_ap_uptime.h — AP uptime tracker UI.
// Route: /ap-uptime. Prefix: wuaut_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUAUT_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>AP Uptime — Phantom32</title>
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
.uptime-bar{height:6px;border-radius:3px;background:linear-gradient(90deg,var(--success),var(--accent));margin-top:3px;}
.drift-pos{color:var(--warn);}
.drift-neg{color:var(--accent);}
.drift-ok{color:var(--muted);}
.reboot-pill{display:inline-block;padding:1px 6px;border-radius:4px;font-size:9px;font-weight:600;}
.reboot-0{background:rgba(34,197,94,.1);color:var(--success);}
.reboot-some{background:rgba(245,158,11,.12);color:var(--warn);}
.reboot-many{background:rgba(239,68,68,.12);color:var(--danger);}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;margin-top:12px;}
.info-box{background:rgba(0,212,255,.04);border:1px solid rgba(0,212,255,.15);border-radius:7px;padding:12px;margin-bottom:14px;font-size:12px;line-height:1.6;color:var(--muted);}
@media(max-width:700px){.hero{grid-template-columns:1fr 1fr}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/beacon-anomaly">Beacon</a>
    <a href="/ap-uptime" class="active">AP Uptime</a>
    <a href="/ap-timing">Timing</a>
  </nav>
</div>
<div class="container">
  <h1>AP Uptime Tracker</h1>
  <p class="subtitle">Estimates how long each AP has been running since its last boot, using the TSF (Timing Synchronization Function) field in beacon frames.</p>

  <div class="info-box">
    <strong style="color:var(--accent)">How it works:</strong> 802.11 beacons include a 64-bit TSF counter reset to zero at each AP boot and incrementing at 1MHz.
    Dividing the current TSF by 1,000,000 gives seconds since boot. TSF rollback detection flags unexpected reboots.
    Crystal drift is estimated via EMA comparison with the ESP32 wall clock (accuracy ±50ppm).
  </div>

  <div class="hero">
    <div class="stat"><label>APs Tracked</label><div class="val" id="h-cnt">0</div></div>
    <div class="stat"><label>Avg Uptime</label><div class="val" id="h-avg" style="font-size:16px">—</div></div>
    <div class="stat"><label>Reboots Seen</label><div class="val" id="h-reboots" style="color:var(--warn)">0</div></div>
    <div class="stat"><label>High Drift APs</label><div class="val" id="h-drift" style="color:var(--warn)">0</div></div>
  </div>

  <div class="section">
    <h3>AP Uptime Table</h3>
    <table>
      <thead>
        <tr>
          <th onclick="sortTable('bssid')">BSSID</th>
          <th onclick="sortTable('ssid')">SSID</th>
          <th>CH</th>
          <th onclick="sortTable('uptime_s')">Uptime</th>
          <th>Visual</th>
          <th onclick="sortTable('drift_ppb')">Drift (ppb)</th>
          <th onclick="sortTable('reboots')">Reboots</th>
          <th>RSSI</th>
        </tr>
      </thead>
      <tbody id="ap-tbody">
        <tr><td colspan="8" style="color:var(--muted);text-align:center">Waiting for beacon TSF data…</td></tr>
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
let g_sort = {key:'uptime_s', asc:false};

const MAX_UPTIME = 30 * 24 * 3600; // 30 days reference

function uptimeBar(s) {
  const pct = Math.min((s / MAX_UPTIME) * 100, 100).toFixed(1);
  return `<div style="width:80px;background:var(--border);border-radius:3px;height:6px;">
    <div class="uptime-bar" style="width:${pct}%"></div>
  </div>`;
}

function driftClass(ppb) {
  const abs = Math.abs(ppb);
  if (abs > 100) return 'drift-pos';
  if (abs > 50)  return 'drift-neg';
  return 'drift-ok';
}

function driftText(ppb) {
  if (ppb === 0) return '—';
  return (ppb > 0 ? '+' : '') + ppb + ' ppb';
}

function rebootPill(n) {
  if (n === 0) return '<span class="reboot-pill reboot-0">None</span>';
  if (n < 3)   return `<span class="reboot-pill reboot-some">${n} ⚠</span>`;
  return `<span class="reboot-pill reboot-many">${n} !</span>`;
}

function rssiColor(r) {
  if (r >= -60) return 'var(--success)';
  if (r >= -75) return 'var(--warn)';
  return 'var(--danger)';
}

function sortTable(key) {
  if (g_sort.key === key) g_sort.asc = !g_sort.asc;
  else { g_sort.key = key; g_sort.asc = false; }
  renderTable();
}

function renderTable() {
  const sorted = [...g_aps].sort((a, b) => {
    const va = a[g_sort.key], vb = b[g_sort.key];
    if (typeof va === 'number') return g_sort.asc ? va - vb : vb - va;
    return g_sort.asc ? String(va).localeCompare(String(vb)) : String(vb).localeCompare(String(va));
  });
  document.getElementById('ap-tbody').innerHTML = sorted.length
    ? sorted.map(a => `<tr>
        <td><code style="font-size:10px">${a.bssid}</code></td>
        <td>${a.ssid || '<em style="color:var(--muted)">Hidden</em>'}</td>
        <td>${a.ch}</td>
        <td style="font-weight:600">${a.uptime_str}</td>
        <td>${uptimeBar(a.uptime_s)}</td>
        <td class="${driftClass(a.drift_ppb)}">${driftText(a.drift_ppb)}</td>
        <td>${rebootPill(a.reboots)}</td>
        <td style="color:${rssiColor(a.rssi)}">${a.rssi} dBm</td>
      </tr>`).join('')
    : '<tr><td colspan="8" style="color:var(--muted);text-align:center">No data.</td></tr>';
}

function formatUptime(s) {
  if (!s || s < 60) return s + 's';
  if (s < 3600) return Math.floor(s/60) + 'm';
  if (s < 86400) return Math.floor(s/3600) + 'h';
  return Math.floor(s/86400) + 'd';
}

async function refresh() {
  const r = await fetch('/api/aut/all').then(x => x.json()).catch(() => ({}));
  g_aps = r.aps || [];
  document.getElementById('h-cnt').textContent     = r.count || 0;
  document.getElementById('h-reboots').textContent = g_aps.reduce((s, a) => s + a.reboots, 0);
  document.getElementById('h-drift').textContent   = g_aps.filter(a => Math.abs(a.drift_ppb) > 100).length;
  const avgUp = g_aps.length ? g_aps.reduce((s,a) => s + a.uptime_s, 0) / g_aps.length : 0;
  document.getElementById('h-avg').textContent = formatUptime(Math.round(avgUp));
  renderTable();
}

async function clearData() {
  await fetch('/api/aut/clear', {method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 5000);
</script>
</body>
</html>
)rawhtml";

inline void wuaut_routes() {
  server.on("/ap-uptime", HTTP_GET, []() {
    server.send_P(200, "text/html", WUAUT_PAGE);
  });
}

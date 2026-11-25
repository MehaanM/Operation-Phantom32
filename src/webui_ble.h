#pragma once
// webui_ble.h — BLE advertisement scanner UI.
// Route: /ble. Prefix: wuble_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUBLE_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>BLE Scanner — Phantom32</title>
<style>
:root{--bg:#0a0c10;--bg2:#12151c;--bg3:#1a1f2e;--border:#2a3045;
  --accent:#00d4ff;--accent2:#a855f7;--success:#22c55e;--warn:#f59e0b;--danger:#ef4444;
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
.filter-row{display:flex;gap:8px;margin-bottom:12px;flex-wrap:wrap;}
.search-box{flex:1;min-width:140px;padding:7px 10px;background:var(--bg3);border:1px solid var(--border);border-radius:5px;color:var(--text);font-size:12px;outline:none;}
.search-box:focus{border-color:var(--accent);}
.chip{padding:3px 8px;border-radius:12px;font-size:10px;font-weight:600;cursor:pointer;border:1px solid var(--border);background:transparent;color:var(--muted);}
.chip.active{background:rgba(0,212,255,.12);color:var(--accent);border-color:var(--accent);}
table{width:100%;border-collapse:collapse;font-size:11px;}
th{text-align:left;padding:7px 8px;font-size:9px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;cursor:pointer;user-select:none;}
th:hover{color:var(--text);}
td{padding:7px 8px;border-bottom:1px solid var(--border);vertical-align:middle;}
tr:last-child td{border-bottom:none;}
tr:hover td{background:rgba(26,31,46,.7);}
.rssi-bar{display:inline-block;height:8px;border-radius:2px;vertical-align:middle;margin-left:4px;}
.svc-pill{padding:1px 5px;border-radius:10px;font-size:8px;background:rgba(168,85,247,.12);color:var(--accent2);border:1px solid rgba(168,85,247,.2);display:inline-block;margin:1px;}
.mfr-pill{padding:1px 5px;border-radius:10px;font-size:8px;background:rgba(0,212,255,.1);color:var(--accent);border:1px solid rgba(0,212,255,.15);display:inline-block;}
.badge{padding:1px 5px;border-radius:3px;font-size:9px;font-weight:700;display:inline-block;}
.badge-conn{background:rgba(34,197,94,.12);color:var(--success);}
.badge-nconn{background:rgba(100,116,139,.1);color:var(--muted);}
.badge-public{background:rgba(0,212,255,.08);color:var(--accent);}
.badge-random{background:rgba(245,158,11,.08);color:var(--warn);}
.mini-rssi{display:inline-flex;align-items:flex-end;gap:1px;height:14px;vertical-align:middle;}
.mini-rssi-bar{width:2px;border-radius:1px 1px 0 0;}
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
    <a href="/ble" class="active">BLE</a>
    <a href="/spectrum">RF Spectrum</a>
    <a href="/probe-flood">Probe Flood</a>
  </nav>
</div>
<div class="container">
  <h1>BLE Advertisement Scanner</h1>
  <p class="subtitle">Passively monitors Bluetooth Low Energy advertisements. Shows device names, services, manufacturer data, signal strength, and address types. No connection is established.</p>

  <div class="hero">
    <div class="stat"><label>Devices</label><div class="val" id="h-cnt">0</div></div>
    <div class="stat"><label>Connectable</label><div class="val" id="h-conn" style="color:var(--success)">0</div></div>
    <div class="stat"><label>Apple Devices</label><div class="val" id="h-apple">0</div></div>
    <div class="stat"><label>Adverts/Total</label><div class="val" id="h-adv" style="font-size:14px">0</div></div>
  </div>

  <div class="section">
    <h3>Nearby BLE Devices</h3>
    <div class="filter-row">
      <input class="search-box" id="search" placeholder="Filter by name, MAC, vendor..." oninput="renderTable()">
      <button class="chip" id="chip-conn" onclick="toggleChip('conn')">Connectable</button>
      <button class="chip" id="chip-named" onclick="toggleChip('named')">Named</button>
      <button class="chip" id="chip-svc" onclick="toggleChip('svc')">Has Services</button>
    </div>
    <table>
      <thead>
        <tr>
          <th onclick="sort('addr')">Address</th>
          <th>Type</th>
          <th onclick="sort('name')">Name</th>
          <th onclick="sort('rssi')">RSSI</th>
          <th>Signal</th>
          <th>Vendor</th>
          <th>Services</th>
          <th onclick="sort('adv_count')">Adverts</th>
          <th>Status</th>
        </tr>
      </thead>
      <tbody id="ble-tbody">
        <tr><td colspan="9" style="color:var(--muted);text-align:center">BLE scanning not started — connect via serial.</td></tr>
      </tbody>
    </table>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>
</div>

<script>
let g_devices = [];
let g_sort = {key:'rssi', asc:true};
let g_chips = {conn: false, named: false, svc: false};

function rssiToWidth(r) { return Math.max(4, Math.min(100, (r + 100) * 2)); }
function rssiColor(r) {
  if (r >= -55) return 'var(--success)';
  if (r >= -70) return 'var(--warn)';
  return 'var(--danger)';
}

function miniSignal(hist) {
  if (!hist || !hist.length) return '';
  const max = Math.max(...hist.map(v => -v), 1);
  return '<div class="mini-rssi">' + hist.map(v => {
    const h = Math.round((-v / max) * 12) + 2;
    return `<div class="mini-rssi-bar" style="height:${h}px;background:${rssiColor(v)}"></div>`;
  }).join('') + '</div>';
}

function toggleChip(name) {
  g_chips[name] = !g_chips[name];
  document.getElementById('chip-'+name).classList.toggle('active', g_chips[name]);
  renderTable();
}

function sort(key) {
  if (g_sort.key === key) g_sort.asc = !g_sort.asc;
  else { g_sort.key = key; g_sort.asc = key === 'rssi'; }
  renderTable();
}

function filterDevices() {
  const q = document.getElementById('search').value.toLowerCase();
  return g_devices.filter(d => {
    if (g_chips.conn && !d.connectable) return false;
    if (g_chips.named && !d.name) return false;
    if (g_chips.svc && (!d.svcs || d.svcs.length === 0)) return false;
    if (!q) return true;
    return d.addr.toLowerCase().includes(q) ||
           (d.name && d.name.toLowerCase().includes(q)) ||
           (d.mfr_name && d.mfr_name.toLowerCase().includes(q)) ||
           (d.addr_type && d.addr_type.toLowerCase().includes(q));
  });
}

function renderTable() {
  const filtered = filterDevices().sort((a,b) => {
    const va = a[g_sort.key], vb = b[g_sort.key];
    if (typeof va === 'number') return g_sort.asc ? va - vb : vb - va;
    return g_sort.asc ? String(va||'').localeCompare(String(vb||'')) : String(vb||'').localeCompare(String(va||''));
  });

  document.getElementById('ble-tbody').innerHTML = filtered.length
    ? filtered.map(d => {
        const svcs = (d.svcs || []).map(s =>
          `<span class="svc-pill">${s.name || ('0x' + s.uuid.toString(16).toUpperCase().padStart(4,'0'))}</span>`
        ).join('');
        return `<tr>
          <td><code style="font-size:9px">${d.addr}</code></td>
          <td>${d.addr_type.includes('Public')
            ? '<span class="badge badge-public">Public</span>'
            : '<span class="badge badge-random">Random</span>'}</td>
          <td style="font-weight:${d.name?600:400};color:${d.name?'var(--text)':'var(--muted)'}">${d.name || '<em>Unknown</em>'}</td>
          <td style="color:${rssiColor(d.rssi)};font-weight:600">${d.rssi} dBm</td>
          <td><div style="display:inline-block;width:${rssiToWidth(d.rssi)}px;height:4px;border-radius:2px;background:${rssiColor(d.rssi)}"></div></td>
          <td>${d.mfr_name ? `<span class="mfr-pill">${d.mfr_name}</span>` : '<span style="color:var(--muted)">—</span>'}</td>
          <td>${svcs || '<span style="color:var(--muted)">—</span>'}</td>
          <td style="color:var(--muted)">${d.adv_count.toLocaleString()}</td>
          <td>${d.connectable
            ? '<span class="badge badge-conn">↔ Conn</span>'
            : '<span class="badge badge-nconn">◉ Adv</span>'}</td>
        </tr>`;
      }).join('')
    : '<tr><td colspan="9" style="color:var(--muted);text-align:center">No devices matching filter.</td></tr>';
}

async function refresh() {
  const r = await fetch('/api/ble/all').then(x => x.json()).catch(() => ({}));
  g_devices = r.devices || [];
  document.getElementById('h-cnt').textContent   = r.count || 0;
  document.getElementById('h-conn').textContent  = g_devices.filter(d => d.connectable).length;
  document.getElementById('h-apple').textContent = g_devices.filter(d => d.mfr_name === 'Apple').length;
  document.getElementById('h-adv').textContent   = r.total_adverts?.toLocaleString() || '0';
  renderTable();
}

async function clearData() {
  await fetch('/api/ble/clear', {method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 2000);
</script>
</body>
</html>
)rawhtml";

inline void wuble_routes() {
  server.on("/ble", HTTP_GET, []() { server.send_P(200, "text/html", WUBLE_PAGE); });
}

#pragma once
// webui_ap_fingerprint.h — AP fingerprinting UI.
// Route: /ap-fingerprint. Prefix: wuapf_

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
.container{max-width:1150px;margin:0 auto;padding:20px 16px;}
h1{font-size:22px;font-weight:700;margin-bottom:4px;}
.subtitle{color:var(--muted);font-size:13px;margin-bottom:20px;}
.hero{display:grid;grid-template-columns:repeat(4,1fr);gap:10px;margin-bottom:16px;}
.stat{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:13px;}
.stat label{font-size:10px;color:var(--muted);text-transform:uppercase;}
.stat .val{font-size:22px;font-weight:800;color:var(--accent);margin-top:3px;}
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:18px;margin-bottom:14px;}
.section h3{font-size:12px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:14px;}
.search-bar{width:100%;padding:8px 12px;background:var(--bg3);border:1px solid var(--border);border-radius:5px;color:var(--text);font-size:12px;outline:none;margin-bottom:12px;}
.search-bar:focus{border-color:var(--accent);}
table{width:100%;border-collapse:collapse;font-size:11px;}
th{text-align:left;padding:7px 8px;font-size:9px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;cursor:pointer;user-select:none;}
th:hover{color:var(--text);}
td{padding:7px 8px;border-bottom:1px solid var(--border);vertical-align:top;}
tr:last-child td{border-bottom:none;}
tr:hover td{background:rgba(26,31,46,.7);}
.cap-pill{padding:1px 5px;border-radius:10px;font-size:8px;border:1px solid var(--border);color:var(--muted);display:inline-block;margin:1px;}
.cap-on{color:var(--success);border-color:rgba(34,197,94,.3);}
.cap-sec{color:var(--accent);border-color:rgba(0,212,255,.3);}
.cap-warn{color:var(--warn);border-color:rgba(245,158,11,.3);}
.conf-badge{padding:2px 6px;border-radius:3px;font-size:9px;font-weight:700;display:inline-block;}
.conf-High{background:rgba(34,197,94,.12);color:var(--success);}
.conf-Probable{background:rgba(0,212,255,.1);color:var(--accent);}
.conf-Likely{background:rgba(245,158,11,.1);color:var(--warn);}
.conf-Guess{background:rgba(100,116,139,.1);color:var(--muted);}
.rate-bar{height:3px;background:var(--border);border-radius:2px;margin-top:3px;}
.rate-fill{height:100%;border-radius:2px;background:linear-gradient(90deg,var(--accent),var(--success));}
.chip-badge{padding:2px 7px;border-radius:4px;font-size:9px;font-weight:600;}
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
    <a href="/ap-uptime">AP Uptime</a>
    <a href="/ap-fingerprint" class="active">Fingerprint</a>
    <a href="/rogue-ap">Rogue AP</a>
  </nav>
</div>
<div class="container">
  <h1>AP Fingerprinting</h1>
  <p class="subtitle">Identifies AP vendor, model, and chip platform from 802.11 beacon information elements. Analyzes HT/VHT/HE capabilities, vendor IEs, and operational parameters.</p>

  <div class="hero">
    <div class="stat"><label>APs</label><div class="val" id="h-cnt">0</div></div>
    <div class="stat"><label>Wi-Fi 6 (HE)</label><div class="val" id="h-he" style="color:var(--success)">0</div></div>
    <div class="stat"><label>WPA3</label><div class="val" id="h-wpa3" style="color:var(--accent)">0</div></div>
    <div class="stat"><label>High Conf</label><div class="val" id="h-high">0</div></div>
  </div>

  <div class="section">
    <h3>AP Fingerprint Table</h3>
    <input class="search-bar" id="search" placeholder="Filter by SSID, vendor, model, chip..." oninput="renderTable()">
    <table>
      <thead>
        <tr>
          <th onclick="sort('bssid')">BSSID</th>
          <th onclick="sort('ssid')">SSID</th>
          <th>CH</th>
          <th onclick="sort('rssi')">RSSI</th>
          <th onclick="sort('vendor')">Vendor</th>
          <th onclick="sort('model')">Model</th>
          <th onclick="sort('chip')">Chip</th>
          <th onclick="sort('max_rate')">Max Rate</th>
          <th>Capabilities</th>
          <th onclick="sort('confidence')">Confidence</th>
          <th onclick="sort('beacons')">Beacons</th>
        </tr>
      </thead>
      <tbody id="ap-tbody">
        <tr><td colspan="11" style="color:var(--muted);text-align:center">Waiting for beacon frames…</td></tr>
      </tbody>
    </table>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>
</div>

<script>
let g_aps = [];
let g_sort = {key:'rssi', asc:true};

const CHIP_COLORS = {
  'Broadcom':'#4b9eff','Qualcomm/Atheros':'#f97316','Realtek':'#22c55e',
  'MediaTek':'#a855f7','Marvell':'#06b6d4','Intel':'#64748b','Espressif':'#00d4ff'
};
const CONF_ORDER = {High:4, Probable:3, Likely:2, Guess:1, Unknown:0};

function capPills(caps) {
  if (!caps || !caps.length) return '—';
  const cls = {HE:'cap-on',VHT:'cap-on',HT:'cap-on',WPA3:'cap-sec',WPA2:'cap-sec',
                PMF:'cap-sec',RRM:'cap-warn',BTM:'cap-warn',FT:'cap-warn'};
  return caps.map(c => `<span class="cap-pill ${cls[c]||''}">${c}</span>`).join('');
}

function sort(key) {
  if (g_sort.key === key) g_sort.asc = !g_sort.asc;
  else { g_sort.key = key; g_sort.asc = key === 'rssi'; }
  renderTable();
}

function rssiColor(r) {
  return r >= -60 ? 'var(--success)' : r >= -75 ? 'var(--warn)' : 'var(--danger)';
}

function filterAPs() {
  const q = document.getElementById('search').value.toLowerCase();
  if (!q) return g_aps;
  return g_aps.filter(a =>
    a.ssid.toLowerCase().includes(q) ||
    a.bssid.toLowerCase().includes(q) ||
    a.vendor.toLowerCase().includes(q) ||
    a.model.toLowerCase().includes(q) ||
    a.chip.toLowerCase().includes(q)
  );
}

function renderTable() {
  const aps = filterAPs().sort((a,b) => {
    if (g_sort.key === 'confidence') {
      const va = CONF_ORDER[a.confidence]||0, vb = CONF_ORDER[b.confidence]||0;
      return g_sort.asc ? va - vb : vb - va;
    }
    const va = a[g_sort.key], vb = b[g_sort.key];
    if (typeof va === 'number') return g_sort.asc ? va - vb : vb - va;
    return g_sort.asc ? String(va||'').localeCompare(String(vb||'')) : String(vb||'').localeCompare(String(va||''));
  });

  document.getElementById('ap-tbody').innerHTML = aps.length
    ? aps.map(a => {
        const chipColor = CHIP_COLORS[a.chip] || 'var(--muted)';
        const ratePct = Math.min((a.max_rate / 10000) * 100, 100).toFixed(0);
        return `<tr>
          <td><code style="font-size:9px">${a.bssid}</code></td>
          <td>${a.ssid || '<em style="color:var(--muted)">Hidden</em>'}</td>
          <td>${a.ch}</td>
          <td style="color:${rssiColor(a.rssi)}">${a.rssi}</td>
          <td style="font-size:10px">${a.vendor || '<span style="color:var(--muted)">—</span>'}</td>
          <td style="font-size:10px;color:var(--accent)">${a.model || '<span style="color:var(--muted)">—</span>'}</td>
          <td><span class="chip-badge" style="background:${chipColor}22;color:${chipColor}">${a.chip}</span></td>
          <td>
            ${a.max_rate ? a.max_rate + ' Mbps' : '—'}
            ${a.max_rate ? `<div class="rate-bar"><div class="rate-fill" style="width:${ratePct}%"></div></div>` : ''}
          </td>
          <td style="max-width:200px;white-space:normal">${capPills(a.caps)}</td>
          <td><span class="conf-badge conf-${a.confidence||'Guess'}">${a.confidence||'Guess'}</span></td>
          <td style="color:var(--muted)">${(a.beacons||0).toLocaleString()}</td>
        </tr>`;
      }).join('')
    : '<tr><td colspan="11" style="color:var(--muted);text-align:center">No data.</td></tr>';
}

async function refresh() {
  const r = await fetch('/api/apf/all').then(x => x.json()).catch(() => ({}));
  g_aps = r.aps || [];
  document.getElementById('h-cnt').textContent   = r.count || 0;
  document.getElementById('h-he').textContent    = g_aps.filter(a => (a.caps||[]).includes('HE')).length;
  document.getElementById('h-wpa3').textContent  = g_aps.filter(a => (a.caps||[]).includes('WPA3')).length;
  document.getElementById('h-high').textContent  = g_aps.filter(a => a.confidence === 'High').length;
  renderTable();
}

async function clearData() {
  await fetch('/api/apf/clear', {method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 5000);
</script>
</body>
</html>
)rawhtml";

inline void wuapf_routes() {
  server.on("/ap-fingerprint", HTTP_GET, []() { server.send_P(200, "text/html", WUAPF_PAGE); });
}

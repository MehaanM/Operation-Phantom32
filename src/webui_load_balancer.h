#pragma once
// webui_load_balancer.h — AP load balancer UI.
// Route: /load-balance. Prefix: wualb_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUALB_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>AP Load Balancer — Phantom32</title>
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
.group-card{background:var(--bg3);border:1px solid var(--border);border-radius:7px;padding:14px;margin-bottom:10px;}
.group-card.balanced{border-left:3px solid var(--success);}
.group-card.unbalanced{border-left:3px solid var(--warn);}
.group-card.severe{border-left:3px solid var(--danger);}
table{width:100%;border-collapse:collapse;font-size:11px;}
th{text-align:left;padding:7px 8px;font-size:9px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;cursor:pointer;}
th:hover{color:var(--text);}
td{padding:6px 8px;border-bottom:1px solid var(--border);}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
.load-bar{height:5px;border-radius:3px;background:var(--border);display:inline-block;width:60px;vertical-align:middle;}
.load-fill{height:100%;border-radius:3px;}
.badge{padding:1px 5px;border-radius:3px;font-size:9px;font-weight:700;display:inline-block;}
.badge-btm{background:rgba(0,212,255,.1);color:var(--accent);}
.badge-nobtm{background:rgba(100,116,139,.1);color:var(--muted);}
.spark{display:inline-flex;align-items:flex-end;gap:1px;height:14px;}
.spark-bar{width:2px;border-radius:1px 1px 0 0;background:var(--accent);}
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
    <a href="/bss-transition">BTM</a>
    <a href="/load-balance" class="active">Load Balance</a>
    <a href="/neighbor-report">Neighbor</a>
  </nav>
</div>
<div class="container">
  <h1>AP Load Balancer Monitor</h1>
  <p class="subtitle">Monitors client distribution across APs sharing the same SSID. 802.11v BSS Transition Management (BTM) requests and imbalance detection help identify load-balancing behavior.</p>

  <div class="hero">
    <div class="stat"><label>APs</label><div class="val" id="h-ap">0</div></div>
    <div class="stat"><label>SSID Groups</label><div class="val" id="h-grp">0</div></div>
    <div class="stat"><label>BTM-Capable</label><div class="val" id="h-btm" style="color:var(--accent)">0</div></div>
    <div class="stat"><label>Imbalanced Groups</label><div class="val" id="h-imbal" style="color:var(--warn)">0</div></div>
  </div>

  <div class="section">
    <h3>SSID Load Groups</h3>
    <div id="group-list">
      <div style="color:var(--muted);text-align:center;padding:20px">No multi-AP SSIDs detected yet.</div>
    </div>
  </div>

  <div class="section">
    <h3>AP Detail</h3>
    <table>
      <thead>
        <tr>
          <th onclick="sortTable('bssid')">BSSID</th>
          <th onclick="sortTable('ssid')">SSID</th>
          <th>CH</th>
          <th onclick="sortTable('clients')">Clients</th>
          <th>Load</th>
          <th onclick="sortTable('peak_clients')">Peak</th>
          <th>BTM</th>
          <th>BTM Req/OK</th>
          <th>Activity</th>
        </tr>
      </thead>
      <tbody id="ap-tbody">
        <tr><td colspan="9" style="color:var(--muted);text-align:center">No data.</td></tr>
      </tbody>
    </table>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>
</div>

<script>
let g_data = {aps:[], groups:[], ap_count:0, group_count:0};
let g_sort = {key:'clients', asc:false};

function groupClass(imb) {
  if (imb >= 60) return 'severe';
  if (imb >= 30) return 'unbalanced';
  return 'balanced';
}

function groupImbalColor(imb) {
  if (imb >= 60) return 'var(--danger)';
  if (imb >= 30) return 'var(--warn)';
  return 'var(--success)';
}

function sparkline(hist) {
  if (!hist || !hist.length) return '';
  const max = Math.max(...hist, 1);
  return '<div class="spark">' + hist.map(v => {
    const h = Math.round((v / max) * 12) + 2;
    return `<div class="spark-bar" style="height:${h}px"></div>`;
  }).join('') + '</div>';
}

function renderGroups() {
  const html = g_data.groups.map(g => {
    const aps = g_data.aps.filter(a => a.ssid === g.ssid);
    const maxC = Math.max(...aps.map(a => a.clients), 1);
    return `<div class="group-card ${groupClass(g.imbalance_pct)}">
      <div style="display:flex;align-items:center;gap:10px;margin-bottom:10px">
        <strong>${g.ssid}</strong>
        <span style="font-size:10px;color:var(--muted)">${g.ap_count} APs</span>
        <span style="font-size:10px;color:${groupImbalColor(g.imbalance_pct)}">Imbalance: ${g.imbalance_pct}%</span>
        <span style="font-size:10px;color:var(--muted);margin-left:auto">${g.total_clients} clients total</span>
      </div>
      <div style="display:flex;gap:8px;flex-wrap:wrap">
        ${aps.map(a => {
          const pct = (a.clients / maxC * 100).toFixed(0);
          return `<div style="flex:1;min-width:120px;background:var(--bg2);border:1px solid var(--border);border-radius:5px;padding:8px">
            <div style="font-size:9px;color:var(--muted)"><code>${a.bssid.slice(-8)}</code> CH${a.ch}</div>
            <div style="font-size:18px;font-weight:700;color:var(--accent)">${a.clients}</div>
            <div style="height:4px;background:var(--border);border-radius:2px;margin-top:4px">
              <div style="width:${pct}%;height:100%;background:var(--accent);border-radius:2px"></div>
            </div>
          </div>`;
        }).join('')}
      </div>
    </div>`;
  }).join('') || '<div style="color:var(--muted);text-align:center;padding:20px">No groups.</div>';
  document.getElementById('group-list').innerHTML = html;
}

function sortTable(key) {
  if (g_sort.key === key) g_sort.asc = !g_sort.asc;
  else { g_sort.key = key; g_sort.asc = false; }
  renderTable();
}

function renderTable() {
  const sorted = [...g_data.aps].sort((a,b) => {
    const va = a[g_sort.key], vb = b[g_sort.key];
    if (typeof va === 'number') return g_sort.asc ? va - vb : vb - va;
    return g_sort.asc ? String(va).localeCompare(String(vb)) : String(vb).localeCompare(String(va));
  });
  document.getElementById('ap-tbody').innerHTML = sorted.length
    ? sorted.map(a => {
        const maxP = a.peak_clients || 1;
        const pct = (a.clients / maxP * 100).toFixed(0);
        return `<tr>
          <td><code style="font-size:9px">${a.bssid}</code></td>
          <td>${a.ssid || '<em style="color:var(--muted)">Hidden</em>'}</td>
          <td>${a.ch}</td>
          <td style="font-weight:600">${a.clients}</td>
          <td><div class="load-bar"><div class="load-fill" style="width:${pct}%;background:var(--accent)"></div></div></td>
          <td style="color:var(--muted)">${a.peak_clients}</td>
          <td>${a.has_btm
            ? '<span class="badge badge-btm">✓ BTM</span>'
            : '<span class="badge badge-nobtm">No BTM</span>'}</td>
          <td style="font-size:10px">${a.btm_req}/${a.btm_ok}</td>
          <td>${sparkline(a.hist)}</td>
        </tr>`;
      }).join('')
    : '<tr><td colspan="9" style="color:var(--muted);text-align:center">No data.</td></tr>';
}

async function refresh() {
  const r = await fetch('/api/alb/all').then(x => x.json()).catch(() => g_data);
  g_data = r;
  document.getElementById('h-ap').textContent    = r.ap_count || 0;
  document.getElementById('h-grp').textContent   = r.group_count || 0;
  document.getElementById('h-btm').textContent   = (r.aps || []).filter(a => a.has_btm).length;
  document.getElementById('h-imbal').textContent = (r.groups || []).filter(g => g.imbalance_pct >= 30).length;
  renderGroups();
  renderTable();
}

async function clearData() {
  await fetch('/api/alb/clear', {method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 4000);
</script>
</body>
</html>
)rawhtml";

inline void wualb_routes() {
  server.on("/load-balance", HTTP_GET, []() {
    server.send_P(200, "text/html", WUALB_PAGE);
  });
}

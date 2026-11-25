#pragma once
// webui_beacon_anomaly.h — Beacon anomaly detector UI.
// Route: /beacon-anomaly. Prefix: wuban_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUBAN_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Beacon Anomaly — Phantom32</title>
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
.alert-row{background:var(--bg3);border:1px solid var(--border);border-radius:6px;padding:10px;margin-bottom:6px;}
.alert-row.type-rapid{border-left:3px solid var(--danger);}
.alert-row.type-ssid{border-left:3px solid var(--warn);}
.alert-row.type-tsf{border-left:3px solid #f97316;}
.alert-row.type-bint{border-left:3px solid var(--accent);}
.alert-row.type-miss{border-left:3px solid var(--muted);}
.badge{padding:2px 7px;border-radius:4px;font-size:9px;font-weight:700;display:inline-block;}
.badge-rapid{background:rgba(239,68,68,.2);color:var(--danger);}
.badge-ssid{background:rgba(245,158,11,.18);color:var(--warn);}
.badge-tsf{background:rgba(249,115,22,.2);color:#f97316;}
.badge-bint{background:rgba(0,212,255,.12);color:var(--accent);}
.badge-miss{background:rgba(100,116,139,.18);color:var(--muted);}
.tabs{display:flex;gap:4px;margin-bottom:14px;}
.tab{padding:5px 12px;border-radius:5px;font-size:12px;cursor:pointer;border:1px solid var(--border);color:var(--muted);}
.tab.active{background:rgba(0,212,255,.12);color:var(--accent);border-color:var(--accent);}
.ok-dot{color:var(--success);}
.bad-dot{color:var(--danger);}
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
    <a href="/ap-timing">AP Timing</a>
    <a href="/beacon-anomaly" class="active">Beacon Anomaly</a>
    <a href="/security-audit">Audit</a>
  </nav>
</div>
<div class="container">
  <h1>Beacon Anomaly Detector</h1>
  <p class="subtitle">Detects anomalous beacon behavior: TSF jumps, rapid injection, interval changes, and SSID mutations — indicators of beacon spoofing or misconfiguration.</p>

  <div class="hero">
    <div class="stat"><label>APs</label><div class="val" id="h-cnt">0</div></div>
    <div class="stat"><label>Anomalies</label><div class="val" id="h-total" style="color:var(--warn)">0</div></div>
    <div class="stat"><label>Rapid Inject</label><div class="val" id="h-rapid" style="color:var(--danger)">0</div></div>
    <div class="stat"><label>SSID Changes</label><div class="val" id="h-ssid" style="color:#f97316">0</div></div>
  </div>

  <div class="section">
    <div class="tabs">
      <div class="tab active" onclick="showTab('ap',this)">AP Summary</div>
      <div class="tab" onclick="showTab('alerts',this)">Anomaly Log</div>
    </div>

    <div id="tab-ap">
      <table>
        <thead>
          <tr>
            <th>BSSID</th><th onclick="sortTable('ssid')">SSID</th><th>CH</th>
            <th onclick="sortTable('anomalies')">Anomalies</th>
            <th>TSF</th><th>BInt</th>
            <th>Avg IBI</th><th>Min IBI</th>
            <th onclick="sortTable('ssid_count')">SSIDs Seen</th>
          </tr>
        </thead>
        <tbody id="ap-tbody"></tbody>
      </table>
    </div>

    <div id="tab-alerts" style="display:none">
      <div id="alert-log"><div style="color:var(--muted);text-align:center;padding:20px;">No anomalies logged.</div></div>
    </div>

    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>
</div>

<script>
let g_aps = [];
let g_sort = {key:'anomalies', asc:false};

const TYPE_BADGE = {
  'Rapid Beacon':'badge-rapid','SSID Change':'badge-ssid','TSF Jump':'badge-tsf',
  'Beacon Interval':'badge-bint','Missing Burst':'badge-miss'
};
const ROW_CLS = {
  'Rapid Beacon':'type-rapid','SSID Change':'type-ssid','TSF Jump':'type-tsf',
  'Beacon Interval':'type-bint','Missing Burst':'type-miss'
};

function showTab(name, el) {
  document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
  el.classList.add('active');
  document.getElementById('tab-ap').style.display     = name === 'ap'     ? '' : 'none';
  document.getElementById('tab-alerts').style.display = name === 'alerts' ? '' : 'none';
}

function sortTable(key) {
  if (g_sort.key === key) g_sort.asc = !g_sort.asc;
  else { g_sort.key = key; g_sort.asc = false; }
  renderAPs();
}

function renderAPs() {
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
        <td style="color:${a.anomalies>0?'var(--warn)':'var(--muted)'}">${a.anomalies}</td>
        <td class="${a.tsf_ok?'ok-dot':'bad-dot'}">${a.tsf_ok?'✓ OK':'✗ Err'}</td>
        <td style="font-size:10px">${a.bint > 0 ? a.bint+'TU' : '—'}</td>
        <td style="color:var(--muted)">${a.avg_inter_ms ? a.avg_inter_ms+'ms' : '—'}</td>
        <td style="color:${a.min_inter_ms < 50 && a.min_inter_ms > 0 ? 'var(--danger)' : 'var(--muted)'}">${a.min_inter_ms > 0 ? a.min_inter_ms+'ms' : '—'}</td>
        <td style="color:${a.ssid_count>1?'var(--warn)':'var(--muted)'}">${a.ssid_count}</td>
      </tr>`).join('')
    : '<tr><td colspan="9" style="color:var(--muted);text-align:center">No APs.</td></tr>';
}

async function refresh() {
  const r = await fetch('/api/ban/all').then(x => x.json()).catch(() => ({}));
  g_aps = r.aps || [];
  const alerts = r.alerts || [];
  document.getElementById('h-cnt').textContent   = r.count || 0;
  document.getElementById('h-total').textContent = r.total_anomalies || 0;
  document.getElementById('h-rapid').textContent = alerts.filter(a => a.type === 'Rapid Beacon').length;
  document.getElementById('h-ssid').textContent  = alerts.filter(a => a.type === 'SSID Change').length;
  renderAPs();

  document.getElementById('alert-log').innerHTML = alerts.length
    ? [...alerts].reverse().map(a => {
        const cls = ROW_CLS[a.type] || '';
        const bcls = TYPE_BADGE[a.type] || '';
        return `<div class="alert-row ${cls}">
          <div style="display:flex;align-items:center;gap:8px;margin-bottom:4px">
            <span class="badge ${bcls}">${a.type}</span>
            <strong>${a.ssid||'Hidden'}</strong>
            <code style="font-size:9px;color:var(--muted)">${a.bssid}</code>
            <span style="margin-left:auto;font-size:10px;color:var(--muted)">${new Date(a.ts*1000).toLocaleTimeString()}</span>
          </div>
          <div style="font-size:11px;color:var(--muted)">${a.detail}</div>
        </div>`;
      }).join('')
    : '<div style="color:var(--muted);text-align:center;padding:20px;">No anomalies.</div>';
}

async function clearData() {
  await fetch('/api/ban/clear', {method: 'POST'});
  refresh();
}

refresh();
setInterval(refresh, 3000);
</script>
</body>
</html>
)rawhtml";

inline void wuban_routes() {
  server.on("/beacon-anomaly", HTTP_GET, []() { server.send_P(200, "text/html", WUBAN_PAGE); });
}

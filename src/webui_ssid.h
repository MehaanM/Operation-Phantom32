#pragma once
// webui_ssid.h — SSID tracker and conflict detector web UI.
// Route: /ssids. Prefix: wuss_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUSS_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>SSIDs — Phantom32</title>
<style>
:root{--bg:#0a0c10;--bg2:#12151c;--bg3:#1a1f2e;--border:#2a3045;
  --accent:#00d4ff;--success:#22c55e;--warn:#f59e0b;--danger:#ef4444;
  --text:#e2e8f0;--muted:#64748b;--radius:8px;}
*{box-sizing:border-box;margin:0;padding:0;}
body{font-family:'Inter',system-ui,sans-serif;background:var(--bg);color:var(--text);min-height:100vh;}
.top-bar{display:flex;align-items:center;gap:12px;padding:14px 20px;
  background:var(--bg2);border-bottom:1px solid var(--border);position:sticky;top:0;z-index:10;}
.logo{font-size:18px;font-weight:700;color:var(--accent);letter-spacing:2px;}
.nav-links{display:flex;gap:8px;margin-left:auto;}
.nav-links a{color:var(--muted);text-decoration:none;font-size:13px;padding:5px 10px;border-radius:5px;transition:all .2s;}
.nav-links a:hover{color:var(--text);background:var(--bg3);}
.nav-links a.active{color:var(--accent);background:rgba(0,212,255,.08);}
.container{max-width:1100px;margin:0 auto;padding:24px 16px;}
h1{font-size:22px;font-weight:700;margin-bottom:4px;}
.subtitle{color:var(--muted);font-size:14px;margin-bottom:24px;}
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:20px;margin-bottom:16px;}
.section h3{font-size:13px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:16px;}
.stat-row{display:grid;grid-template-columns:repeat(4,1fr);gap:12px;margin-bottom:20px;}
.stat{background:var(--bg2);border:1px solid var(--border);border-radius:8px;padding:14px;}
.stat label{font-size:11px;color:var(--muted);text-transform:uppercase;}
.stat .val{font-size:22px;font-weight:700;color:var(--accent);margin-top:4px;}
.btn{padding:7px 14px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;transition:all .2s;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.btn-danger{background:var(--danger);color:#fff;}
.action-row{display:flex;gap:8px;flex-wrap:wrap;margin-top:14px;}
.tabs{display:flex;gap:4px;border-bottom:2px solid var(--border);margin-bottom:16px;}
.tab{padding:8px 16px;cursor:pointer;font-size:13px;border-bottom:2px solid transparent;margin-bottom:-2px;color:var(--muted);}
.tab.active{color:var(--accent);border-color:var(--accent);}
.tab-content{display:none;}.tab-content.active{display:block;}
table{width:100%;border-collapse:collapse;font-size:13px;}
th{text-align:left;padding:7px 10px;font-size:11px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;}
td{padding:8px 10px;border-bottom:1px solid var(--border);vertical-align:middle;}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
.badge{display:inline-block;padding:2px 8px;border-radius:12px;font-size:11px;font-weight:600;margin:1px;}
.badge-open{background:rgba(239,68,68,.2);color:var(--danger);}
.badge-wpa{background:rgba(245,158,11,.2);color:var(--warn);}
.badge-wpa2{background:rgba(0,212,255,.15);color:var(--accent);}
.badge-wpa3{background:rgba(34,197,94,.15);color:var(--success);}
.badge-conflict{background:rgba(239,68,68,.2);color:var(--danger);}
.badge-active{background:rgba(34,197,94,.15);color:var(--success);}
.badge-stale{background:rgba(100,116,139,.2);color:var(--muted);}
.detail-row{background:var(--bg3);padding:14px 16px;border-left:3px solid var(--accent);margin-top:4px;font-size:12px;}
.search-box{display:flex;gap:8px;margin-bottom:12px;}
input[type=text]{flex:1;padding:7px 10px;background:var(--bg3);border:1px solid var(--border);border-radius:5px;color:var(--text);font-size:13px;outline:none;}
input:focus{border-color:var(--accent);}
.conflict-card{background:var(--bg3);border:1px solid var(--danger);border-radius:6px;padding:14px;margin-bottom:10px;}
.conflict-card h4{color:var(--danger);margin-bottom:8px;font-size:14px;}
.conflict-bssid{font-family:monospace;font-size:12px;padding:4px 0;border-bottom:1px solid var(--border);}
.conflict-bssid:last-child{border-bottom:none;}
.sec-pill{display:inline-block;padding:1px 6px;border-radius:4px;font-size:10px;margin-right:4px;}
@media(max-width:700px){.stat-row{grid-template-columns:1fr 1fr;}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/">Dashboard</a>
    <a href="/threats">Threats</a>
    <a href="/devices">Devices</a>
    <a href="/security">Security</a>
    <a href="/ssids" class="active">SSIDs</a>
    <a href="/settings">Settings</a>
  </nav>
</div>

<div class="container">
  <h1>SSID Tracker</h1>
  <p class="subtitle">Monitor observed SSIDs, detect naming conflicts, and track channel history.</p>

  <div class="stat-row">
    <div class="stat"><label>Unique SSIDs</label><div class="val" id="st-ssids">0</div></div>
    <div class="stat"><label>Active</label><div class="val" id="st-active" style="color:var(--success)">0</div></div>
    <div class="stat"><label>Conflicts</label><div class="val" id="st-conflicts" style="color:var(--danger)">0</div></div>
    <div class="stat"><label>Total BSSIDs</label><div class="val" id="st-bssids">0</div></div>
  </div>

  <div class="section">
    <div class="tabs">
      <div class="tab active" onclick="switchTab('all')">All SSIDs</div>
      <div class="tab" onclick="switchTab('conflicts')">Conflicts</div>
      <div class="tab" onclick="switchTab('new')">Recently New</div>
    </div>

    <!-- All SSIDs -->
    <div id="tab-all" class="tab-content active">
      <div class="search-box">
        <input type="text" id="ssid-search" placeholder="Filter by SSID…" oninput="renderAll()">
      </div>
      <div style="overflow-x:auto">
        <table>
          <thead><tr>
            <th>SSID</th><th>BSSIDs</th><th>Channels</th><th>Security</th>
            <th>Last Seen</th><th>RSSI (best)</th><th>Status</th>
          </tr></thead>
          <tbody id="all-tbody">
            <tr><td colspan="7" style="color:var(--muted);text-align:center">Loading…</td></tr>
          </tbody>
        </table>
      </div>
    </div>

    <!-- Conflicts -->
    <div id="tab-conflicts" class="tab-content">
      <div id="conflicts-container">
        <div style="color:var(--muted)">No conflicts detected. Conflicts occur when the same SSID is observed on multiple BSSIDs with different security modes (possible evil twin).</div>
      </div>
    </div>

    <!-- Recently New -->
    <div id="tab-new" class="tab-content">
      <div style="overflow-x:auto">
        <table>
          <thead><tr><th>SSID</th><th>BSSID</th><th>First Seen</th><th>Channel</th><th>RSSI</th><th>Security</th></tr></thead>
          <tbody id="new-tbody">
            <tr><td colspan="6" style="color:var(--muted);text-align:center">Loading…</td></tr>
          </tbody>
        </table>
      </div>
    </div>

    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-danger" onclick="clearData()">Clear</button>
    </div>
  </div>
</div>

<script>
let g_data = {ssids:[], conflicts:[]};
const SEC_LABELS = ['Open','WEP','WPA','WPA2','WPA3','?'];
const SEC_CLASSES = ['badge-open','badge-open','badge-wpa','badge-wpa2','badge-wpa3',''];

function secBadge(mask) {
  const names = [];
  for (let i = 0; i < SEC_LABELS.length; i++) {
    if (mask & (1<<i)) names.push(`<span class="badge ${SEC_CLASSES[i]}">${SEC_LABELS[i]}</span>`);
  }
  return names.join('') || '<span class="badge">?</span>';
}

function timeSince(ts) {
  if (!ts) return '—';
  const s = Math.floor(Date.now()/1000) - ts;
  if (s < 60) return s+'s';
  if (s < 3600) return Math.floor(s/60)+'m';
  return Math.floor(s/3600)+'h';
}

function switchTab(name) {
  document.querySelectorAll('.tab').forEach((t,i)=>{
    const names = ['all','conflicts','new'];
    t.classList.toggle('active', names[i]===name);
    document.getElementById('tab-'+names[i]).classList.toggle('active', names[i]===name);
  });
}

function renderAll() {
  const filter = (document.getElementById('ssid-search')?.value||'').toLowerCase();
  const list = g_data.ssids.filter(s=>!filter||s.ssid.toLowerCase().includes(filter));
  const now = Math.floor(Date.now()/1000);
  document.getElementById('all-tbody').innerHTML = list.length
    ? list.map(s=>{
        const stale = now - (s.last_ts||0) > 90;
        const conflict = g_data.conflicts.some(c=>c.ssid===s.ssid);
        return `<tr>
          <td style="max-width:220px;overflow:hidden;text-overflow:ellipsis">
            ${conflict?'<span style="color:var(--danger)">⚠ </span>':''}
            <strong>${s.ssid||'<hidden>'}</strong>
          </td>
          <td>${s.bssid_count||0}</td>
          <td style="font-size:11px">${(s.channels||[]).join(', ')}</td>
          <td>${secBadge(s.sec_mask||0)}</td>
          <td>${timeSince(s.last_ts)}</td>
          <td style="color:${(s.best_rssi||0)>-70?'var(--success)':'var(--warn)'}">${s.best_rssi||'—'} dBm</td>
          <td><span class="badge ${stale?'badge-stale':'badge-active'}">${stale?'Stale':'Active'}</span>
              ${conflict?'<span class="badge badge-conflict">Conflict</span>':''}</td>
        </tr>`;
      }).join('')
    : '<tr><td colspan="7" style="color:var(--muted);text-align:center">No SSIDs yet.</td></tr>';
}

function renderConflicts() {
  const c = g_data.conflicts;
  document.getElementById('st-conflicts').textContent = c.length;
  if (!c.length) {
    document.getElementById('conflicts-container').innerHTML =
      '<div style="color:var(--muted)">No conflicts detected.</div>';
    return;
  }
  document.getElementById('conflicts-container').innerHTML = c.map(conflict=>`
    <div class="conflict-card">
      <h4>⚠ SSID Conflict: "${conflict.ssid}"</h4>
      <div style="font-size:12px;color:var(--muted);margin-bottom:8px">
        Same network name on multiple BSSIDs with different security — possible evil twin or misconfiguration.
      </div>
      ${(conflict.bssids||[]).map(b=>`
        <div class="conflict-bssid">
          <code>${b.bssid}</code>
          ${secBadge(b.sec_mask||0)}
          <span style="color:var(--muted);font-size:11px"> ch${b.channel||'?'} · ${b.rssi||'?'} dBm</span>
        </div>`).join('')}
    </div>`).join('');
}

function renderNew() {
  const recent = (g_data.ssids||[])
    .filter(s=>s.first_ts && (Math.floor(Date.now()/1000)-s.first_ts) < 300)
    .sort((a,b)=>b.first_ts-a.first_ts);
  document.getElementById('new-tbody').innerHTML = recent.length
    ? recent.map(s=>`<tr>
        <td><strong>${s.ssid||'<hidden>'}</strong></td>
        <td><code style="font-size:11px">${(s.bssids||[])[0]||'?'}</code></td>
        <td style="color:var(--success)">${timeSince(s.first_ts)} ago</td>
        <td>ch${(s.channels||[])[0]||'?'}</td>
        <td>${s.best_rssi||'?'} dBm</td>
        <td>${secBadge(s.sec_mask||0)}</td>
      </tr>`).join('')
    : '<tr><td colspan="6" style="color:var(--muted);text-align:center">No SSIDs seen in last 5 min.</td></tr>';
}

async function refresh() {
  const all = await fetch('/api/sst/all').then(x=>x.json()).catch(()=>({}));
  const conf = await fetch('/api/sst/conflicts').then(x=>x.json()).catch(()=>({}));

  g_data.ssids     = all.ssids    || [];
  g_data.conflicts = conf.conflicts || [];

  const totalBssids = g_data.ssids.reduce((s,x)=>s+(x.bssid_count||0),0);
  const active = g_data.ssids.filter(s=>(Math.floor(Date.now()/1000)-(s.last_ts||0))<90).length;

  document.getElementById('st-ssids').textContent  = g_data.ssids.length;
  document.getElementById('st-active').textContent = active;
  document.getElementById('st-bssids').textContent = totalBssids;

  renderAll();
  renderConflicts();
  renderNew();
}

async function clearData() {
  if (!confirm('Clear all SSID data?')) return;
  await fetch('/api/sst/clear',{method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 5000);
</script>
</body>
</html>
)rawhtml";

inline void wuss_routes() {
  server.on("/ssids", HTTP_GET, []() { server.send_P(200, "text/html", WUSS_PAGE); });
}

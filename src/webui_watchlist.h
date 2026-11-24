#pragma once
// webui_watchlist.h — AP watchlist management web UI.
// Route: /watchlist. Prefix: wuwl_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUWL_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Watchlist — Phantom32</title>
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
.container{max-width:1000px;margin:0 auto;padding:24px 16px;}
h1{font-size:22px;font-weight:700;margin-bottom:4px;}
.subtitle{color:var(--muted);font-size:14px;margin-bottom:24px;}
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:20px;margin-bottom:16px;}
.section h3{font-size:13px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:16px;}
.stat-row{display:grid;grid-template-columns:repeat(3,1fr);gap:12px;margin-bottom:20px;}
.stat{background:var(--bg2);border:1px solid var(--border);border-radius:8px;padding:14px;}
.stat label{font-size:11px;color:var(--muted);text-transform:uppercase;}
.stat .val{font-size:22px;font-weight:700;color:var(--accent);margin-top:4px;}
.btn{padding:7px 14px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;transition:all .2s;}
.btn-primary{background:var(--accent);color:#000;}
.btn-primary:hover{opacity:.85;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.btn-danger{background:var(--danger);color:#fff;}
.btn-sm{padding:4px 9px;font-size:11px;}
.action-row{display:flex;gap:8px;flex-wrap:wrap;margin-top:14px;}
.field-row{display:flex;gap:16px;align-items:flex-start;padding:10px 0;border-bottom:1px solid var(--border);}
.field-row:last-child{border-bottom:none;}
.field-label{width:130px;min-width:130px;font-size:13px;color:var(--text);padding-top:2px;}
.field-label small{display:block;font-size:11px;color:var(--muted);}
.field-input{flex:1;}
input[type=text],select{width:100%;padding:7px 10px;background:var(--bg3);border:1px solid var(--border);border-radius:5px;color:var(--text);font-size:13px;outline:none;}
input:focus,select:focus{border-color:var(--accent);}
table{width:100%;border-collapse:collapse;font-size:13px;}
th{text-align:left;padding:7px 10px;font-size:11px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;}
td{padding:7px 10px;border-bottom:1px solid var(--border);vertical-align:middle;}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
.badge{display:inline-block;padding:2px 8px;border-radius:12px;font-size:11px;font-weight:600;}
.badge-active{background:rgba(34,197,94,.15);color:var(--success);}
.badge-disabled{background:rgba(100,116,139,.15);color:var(--muted);}
.alert-feed{background:var(--bg3);border-radius:5px;padding:12px;height:160px;overflow-y:auto;font-family:monospace;font-size:12px;}
.alert-line{padding:2px 0;border-bottom:1px solid rgba(42,48,69,.5);color:var(--warn);}
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
    <a href="/watchlist" class="active">Watchlist</a>
    <a href="/settings">Settings</a>
  </nav>
</div>

<div class="container">
  <h1>AP Watchlist</h1>
  <p class="subtitle">Define rules to alert when specific APs or patterns appear in the RF environment.</p>

  <div class="stat-row">
    <div class="stat"><label>Active Rules</label><div class="val" id="st-rules">0</div></div>
    <div class="stat"><label>Total Alerts</label><div class="val" id="st-alerts" style="color:var(--warn)">0</div></div>
    <div class="stat"><label>Recent Hits</label><div class="val" id="st-recent">0</div></div>
  </div>

  <div class="section">
    <h3>Add Rule</h3>
    <div class="field-row">
      <div class="field-label">Rule Type</div>
      <div class="field-input">
        <select id="add-type" onchange="onTypeChange()">
          <option value="0">Specific BSSID</option>
          <option value="1">Exact SSID</option>
          <option value="2">SSID contains…</option>
          <option value="3">Vendor OUI prefix</option>
          <option value="4">Any open network</option>
          <option value="5">Any WPS-enabled</option>
        </select>
      </div>
    </div>
    <div id="type-fields">
      <div class="field-row">
        <div class="field-label">MAC / BSSID<small>AA:BB:CC:DD:EE:FF</small></div>
        <div class="field-input"><input type="text" id="add-mac" placeholder="AA:BB:CC:DD:EE:FF"></div>
      </div>
    </div>
    <div class="field-row">
      <div class="field-label">Note<small>Optional label</small></div>
      <div class="field-input"><input type="text" id="add-note" placeholder="Why am I watching this?"></div>
    </div>
    <div class="action-row">
      <button class="btn btn-primary" onclick="addRule()">+ Add Rule</button>
    </div>
  </div>

  <div class="section">
    <h3>Active Rules</h3>
    <table>
      <thead><tr><th>ID</th><th>Type</th><th>Pattern</th><th>Note</th><th>Hits</th><th>Status</th><th>Actions</th></tr></thead>
      <tbody id="rules-tbody">
        <tr><td colspan="7" style="color:var(--muted);text-align:center">No rules defined.</td></tr>
      </tbody>
    </table>
  </div>

  <div class="section">
    <h3>Recent Alerts</h3>
    <div class="alert-feed" id="alert-feed">
      <div style="color:var(--muted)">No alerts yet.</div>
    </div>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-ghost" onclick="clearAlerts()">Clear Alerts</button>
    </div>
  </div>
</div>

<script>
const RULE_TYPE_NAMES = ['BSSID','SSID (exact)','SSID contains','Vendor OUI','Any open','Any WPS'];

function onTypeChange() {
  const type = parseInt(document.getElementById('add-type').value);
  let html = '';
  if (type === 0 || type === 3) {
    html = `<div class="field-row">
      <div class="field-label">MAC / OUI<small>${type===0?'Full MAC':'First 3 bytes: OO:OO:OO'}</small></div>
      <div class="field-input"><input type="text" id="add-mac" placeholder="${type===0?'AA:BB:CC:DD:EE:FF':'AA:BB:CC:00:00:00'}"></div>
    </div>`;
  } else if (type === 1 || type === 2) {
    html = `<div class="field-row">
      <div class="field-label">SSID pattern</div>
      <div class="field-input"><input type="text" id="add-str" placeholder="${type===1?'ExactSSID':'substring'}"></div>
    </div>`;
  } else {
    html = '<div style="color:var(--muted);font-size:13px;padding:10px 0">No parameters needed — rule matches any AP of this type.</div>';
  }
  document.getElementById('type-fields').innerHTML = html;
}

async function addRule() {
  const type = document.getElementById('add-type').value;
  const mac  = document.getElementById('add-mac')?.value  || '00:00:00:00:00:00';
  const str  = document.getElementById('add-str')?.value  || '';
  const note = document.getElementById('add-note')?.value || '';

  const params = new URLSearchParams({type, mac, str, note});
  const r = await fetch('/api/awl/add', {method:'POST', body:params,
    headers:{'Content-Type':'application/x-www-form-urlencoded'}}).then(x=>x.json()).catch(()=>({}));

  if (r.ok) {
    document.getElementById('add-note').value = '';
    refresh();
  } else {
    alert('Error: ' + (r.error||'unknown'));
  }
}

async function removeRule(id) {
  if (!confirm('Remove rule #'+id+'?')) return;
  const r = await fetch('/api/awl/remove', {method:'POST', body:'id='+id,
    headers:{'Content-Type':'application/x-www-form-urlencoded'}}).then(x=>x.json());
  if (r.ok) refresh();
}

async function refresh() {
  const r = await fetch('/api/awl/status').then(x=>x.json()).catch(()=>({}));
  if (!r.rules) return;

  document.getElementById('st-rules').textContent   = r.rule_count||0;
  document.getElementById('st-alerts').textContent  = r.total_alerts||0;
  document.getElementById('st-recent').textContent  = (r.recent_alerts||[]).length;

  const tbody = document.getElementById('rules-tbody');
  tbody.innerHTML = r.rules.length
    ? r.rules.map(rule=>`<tr>
        <td>${rule.id}</td>
        <td>${RULE_TYPE_NAMES[rule.type]||rule.type}</td>
        <td style="font-family:monospace">${rule.str||rule.mac||'—'}</td>
        <td style="color:var(--muted)">${rule.note||''}</td>
        <td>${rule.hits||0}</td>
        <td><span class="badge ${rule.enabled?'badge-active':'badge-disabled'}">${rule.enabled?'ON':'OFF'}</span></td>
        <td><button class="btn btn-danger btn-sm" onclick="removeRule(${rule.id})">Del</button></td>
      </tr>`).join('')
    : '<tr><td colspan="7" style="color:var(--muted);text-align:center">No rules.</td></tr>';

  const alerts = r.recent_alerts||[];
  if (alerts.length) {
    document.getElementById('alert-feed').innerHTML = alerts.reverse().map(a=>{
      const ts = new Date(a.ts*1000).toLocaleTimeString();
      return `<div class="alert-line">[${ts}] Rule #${a.rule_id} hit: ${a.ssid||'<hidden>'} @ ${a.bssid} (${a.rssi} dBm)</div>`;
    }).join('');
  }
}

async function clearAlerts() {
  await fetch('/api/awl/clear_alerts',{method:'POST'});
  refresh();
}

onTypeChange();
refresh();
setInterval(refresh, 5000);
</script>
</body>
</html>
)rawhtml";

inline void wuwl_routes() {
  server.on("/watchlist", HTTP_GET, []() {
    server.send_P(200, "text/html", WUWL_PAGE);
  });
}

#pragma once
// webui_filters.h — Frame filter rule management UI.
// Route: /filters. Prefix: wuff_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUFF_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Frame Filters — Phantom32</title>
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
.container{max-width:1000px;margin:0 auto;padding:20px 16px;}
h1{font-size:22px;font-weight:700;margin-bottom:4px;}
.subtitle{color:var(--muted);font-size:13px;margin-bottom:20px;}
.hero{display:grid;grid-template-columns:repeat(5,1fr);gap:10px;margin-bottom:16px;}
.stat{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:13px;}
.stat label{font-size:10px;color:var(--muted);text-transform:uppercase;}
.stat .val{font-size:22px;font-weight:800;color:var(--accent);margin-top:3px;}
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:18px;margin-bottom:14px;}
.section h3{font-size:12px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:14px;}
.form-grid{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-bottom:12px;}
.form-group{display:flex;flex-direction:column;gap:4px;}
.form-group label{font-size:11px;color:var(--muted);}
input,select{background:var(--bg3);border:1px solid var(--border);border-radius:5px;
  padding:6px 10px;font-size:12px;color:var(--text);}
input:focus,select:focus{outline:none;border-color:var(--accent);}
table{width:100%;border-collapse:collapse;font-size:12px;}
th{text-align:left;padding:7px 8px;font-size:10px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;}
td{padding:7px 8px;border-bottom:1px solid var(--border);}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
.badge{padding:1px 7px;border-radius:4px;font-size:10px;font-weight:700;}
.b-pass{background:rgba(34,197,94,.12);color:var(--success);}
.b-drop{background:rgba(239,68,68,.2);color:var(--danger);}
.b-alert{background:rgba(245,158,11,.15);color:var(--warn);}
.b-log{background:rgba(0,212,255,.1);color:var(--accent);}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-primary{background:var(--accent);color:#0a0c10;}
.btn-primary:hover{background:#00b8e0;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.btn-danger{background:transparent;border:1px solid var(--danger);color:var(--danger);padding:3px 8px;font-size:11px;}
.action-row{display:flex;gap:8px;margin-top:12px;}
@media(max-width:700px){.hero{grid-template-columns:repeat(3,1fr)}.form-grid{grid-template-columns:1fr}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/analytics">Analytics</a>
    <a href="/filters" class="active">Filters</a>
    <a href="/scheduler">Scheduler</a>
    <a href="/security">Security</a>
  </nav>
</div>
<div class="container">
  <h1>Frame Filters</h1>
  <p class="subtitle">Programmable 802.11 frame filter rules. Up to 32 rules, evaluated by priority.</p>

  <div class="hero">
    <div class="stat"><label>Rules</label><div class="val" id="h-rules">0</div></div>
    <div class="stat"><label>Passed</label><div class="val" id="h-pass" style="color:var(--success)">0</div></div>
    <div class="stat"><label>Dropped</label><div class="val" id="h-drop" style="color:var(--danger)">0</div></div>
    <div class="stat"><label>Alerts</label><div class="val" id="h-alert" style="color:var(--warn)">0</div></div>
    <div class="stat"><label>Logged</label><div class="val" id="h-log">0</div></div>
  </div>

  <div class="section">
    <h3>Add Rule</h3>
    <div class="form-grid">
      <div class="form-group">
        <label>Rule Name</label>
        <input id="f-name" type="text" placeholder="e.g. block-deauths">
      </div>
      <div class="form-group">
        <label>Match Type</label>
        <select id="f-match">
          <option value="any">Any Frame</option>
          <option value="src_mac">Source MAC</option>
          <option value="dst_mac">Destination MAC</option>
          <option value="bssid">BSSID</option>
          <option value="channel">Channel</option>
          <option value="type">Frame Type</option>
          <option value="subtype">Frame Subtype</option>
          <option value="ssid_prefix">SSID Prefix</option>
          <option value="ssid_exact">SSID Exact</option>
          <option value="rssi_above">RSSI Above</option>
          <option value="rssi_below">RSSI Below</option>
        </select>
      </div>
      <div class="form-group">
        <label>Action</label>
        <select id="f-action">
          <option value="pass">Pass</option>
          <option value="drop">Drop</option>
          <option value="alert">Alert</option>
          <option value="log">Log</option>
        </select>
      </div>
      <div class="form-group">
        <label>Priority (0=highest)</label>
        <input id="f-prio" type="number" value="128" min="0" max="255">
      </div>
      <div class="form-group">
        <label>MAC / SSID / Channel / RSSI value</label>
        <input id="f-value" type="text" placeholder="e.g. AA:BB:CC:DD:EE:FF or 6 or -80">
      </div>
      <div class="form-group" style="justify-content:flex-end">
        <button class="btn btn-primary" onclick="addRule()">Add Rule</button>
      </div>
    </div>
  </div>

  <div class="section">
    <h3>Active Rules</h3>
    <table>
      <thead><tr>
        <th>ID</th><th>Name</th><th>Match</th><th>Action</th><th>Priority</th><th>Hits</th><th></th>
      </tr></thead>
      <tbody id="rules-tbody">
        <tr><td colspan="7" style="color:var(--muted);text-align:center">No rules configured.</td></tr>
      </tbody>
    </table>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-ghost" onclick="clearRules()">Clear All</button>
    </div>
  </div>
</div>

<script>
const ACTION_BADGE = {
  pass:'<span class="badge b-pass">PASS</span>',
  drop:'<span class="badge b-drop">DROP</span>',
  alert:'<span class="badge b-alert">ALERT</span>',
  log:'<span class="badge b-log">LOG</span>'
};

async function addRule() {
  const name   = document.getElementById('f-name').value||'rule';
  const match  = document.getElementById('f-match').value;
  const action = document.getElementById('f-action').value;
  const prio   = document.getElementById('f-prio').value||128;
  const value  = document.getElementById('f-value').value;

  const params = new URLSearchParams({name, match, action, prio});

  if (match==='src_mac'||match==='dst_mac'||match==='bssid') params.set('mac', value);
  else if (match==='channel')    params.set('channel', value);
  else if (match==='type')       params.set('type', value);
  else if (match==='subtype')    params.set('subtype', value);
  else if (match.startsWith('ssid')) params.set('ssid', value);
  else if (match.startsWith('rssi')) params.set('rssi', value);

  const r = await fetch('/api/ff/add', {
    method:'POST',
    headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:params.toString()
  }).then(x=>x.json()).catch(()=>({}));

  if(r.ok) refresh();
  else alert(r.error||'Failed');
}

async function removeRule(id) {
  const params = new URLSearchParams({id});
  await fetch('/api/ff/remove',{
    method:'POST',
    headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:params.toString()
  });
  refresh();
}

async function clearRules() {
  if(!confirm('Remove all rules?')) return;
  await fetch('/api/ff/clear',{method:'POST'});
  refresh();
}

async function refresh() {
  const r = await fetch('/api/ff/rules').then(x=>x.json()).catch(()=>({}));
  document.getElementById('h-rules').textContent = r.rule_count||0;
  document.getElementById('h-pass').textContent  = r.pass||0;
  document.getElementById('h-drop').textContent  = r.drop||0;
  document.getElementById('h-alert').textContent = r.alert||0;
  document.getElementById('h-log').textContent   = r.log||0;

  const rules = r.rules||[];
  document.getElementById('rules-tbody').innerHTML = rules.length
    ? rules.map(rule=>`<tr>
        <td>${rule.id}</td>
        <td>${rule.name}</td>
        <td style="font-size:11px">${rule.match}</td>
        <td>${ACTION_BADGE[rule.action]||rule.action}</td>
        <td>${rule.prio}</td>
        <td style="color:var(--accent)">${rule.hits}</td>
        <td><button class="btn btn-danger" onclick="removeRule(${rule.id})">×</button></td>
      </tr>`).join('')
    : '<tr><td colspan="7" style="color:var(--muted);text-align:center">No rules configured.</td></tr>';
}

refresh();
setInterval(refresh, 3000);
</script>
</body>
</html>
)rawhtml";

inline void wuff_routes() {
  server.on("/filters", HTTP_GET, []() {
    server.send_P(200, "text/html", WUFF_PAGE);
  });
}

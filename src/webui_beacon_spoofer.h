#pragma once
// webui_beacon_spoofer.h — Beacon spoofer control UI.
// Route: /beacon-spoofer. Prefix: wubsp_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUBSP_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Beacon Spoofer — Phantom32</title>
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
.hero{display:grid;grid-template-columns:repeat(3,1fr);gap:10px;margin-bottom:16px;}
.stat{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:13px;}
.stat label{font-size:10px;color:var(--muted);text-transform:uppercase;}
.stat .val{font-size:22px;font-weight:800;color:var(--accent);margin-top:3px;}
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:18px;margin-bottom:14px;}
.section h3{font-size:12px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:14px;}
.warning-box{padding:12px 16px;border-radius:7px;background:rgba(239,68,68,.07);border-left:3px solid var(--danger);color:var(--danger);margin-bottom:16px;font-size:12px;}
.form-row{display:flex;gap:8px;flex-wrap:wrap;align-items:flex-end;margin-bottom:14px;}
.form-col{display:flex;flex-direction:column;gap:4px;}
label{font-size:11px;color:var(--muted);}
input,select{background:var(--bg3);border:1px solid var(--border);border-radius:5px;padding:5px 8px;color:var(--text);font-size:12px;}
.toggle-row{display:flex;gap:16px;}
.chk{display:flex;align-items:center;gap:5px;font-size:12px;}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.btn-primary{background:var(--accent);color:#000;font-weight:600;}
.btn-primary:hover{opacity:.9;}
.btn-danger{background:rgba(239,68,68,.15);border:1px solid var(--danger);color:var(--danger);}
.btn-danger:hover{background:rgba(239,68,68,.25);}
.btn-success{background:rgba(34,197,94,.15);border:1px solid var(--success);color:var(--success);}
.btn-success:hover{background:rgba(34,197,94,.25);}
table{width:100%;border-collapse:collapse;font-size:12px;}
th{text-align:left;padding:7px 8px;font-size:10px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;}
td{padding:7px 8px;border-bottom:1px solid var(--border);}
tr:last-child td{border-bottom:none;}
.action-row{display:flex;gap:8px;margin-top:12px;}
.status-pill{padding:4px 10px;border-radius:12px;font-size:11px;font-weight:600;}
.pill-on{background:rgba(34,197,94,.18);color:var(--success);}
.pill-off{background:rgba(100,116,139,.18);color:var(--muted);}
@media(max-width:700px){.hero{grid-template-columns:1fr}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/beacon-flooder">Flooder</a>
    <a href="/beacon-spoofer" class="active">Spoofer</a>
    <a href="/evil-twin">Evil Twin</a>
  </nav>
</div>
<div class="container">
  <h1>Beacon Spoofer</h1>
  <p class="subtitle">Craft and inject custom 802.11 beacon frames with arbitrary SSIDs. For authorized security testing and research on your own networks.</p>

  <div class="warning-box">⚠ Authorized use only. Beacon injection on unauthorized networks may violate local laws.</div>

  <div class="hero">
    <div class="stat"><label>Entries</label><div class="val" id="h-cnt">0</div></div>
    <div class="stat"><label>Total Sent</label><div class="val" id="h-sent">0</div></div>
    <div class="stat"><label>Status</label><div class="val" id="h-status" style="font-size:14px;margin-top:5px"><span class="status-pill pill-off">Stopped</span></div></div>
  </div>

  <div class="section">
    <h3>Add Beacon Entry</h3>
    <div class="form-row">
      <div class="form-col">
        <label>SSID</label>
        <input id="f-ssid" placeholder="Network name" style="width:160px">
      </div>
      <div class="form-col">
        <label>Channel</label>
        <select id="f-ch">
          <option value="1">1</option><option value="6" selected>6</option>
          <option value="11">11</option><option value="2">2</option>
          <option value="3">3</option><option value="4">4</option>
          <option value="5">5</option><option value="7">7</option>
          <option value="8">8</option><option value="9">9</option>
          <option value="10">10</option><option value="13">13</option>
        </select>
      </div>
      <div class="form-col">
        <label>Interval (ms)</label>
        <input id="f-iv" type="number" value="100" style="width:80px">
      </div>
      <div class="form-col">
        <label>&nbsp;</label>
        <div class="toggle-row">
          <label class="chk"><input type="checkbox" id="f-hidden"> Hidden</label>
          <label class="chk"><input type="checkbox" id="f-open"> Open</label>
        </div>
      </div>
      <div class="form-col">
        <label>&nbsp;</label>
        <button class="btn btn-primary" onclick="addEntry()">Add</button>
      </div>
    </div>
  </div>

  <div class="section">
    <h3>Active Entries</h3>
    <table><thead><tr>
      <th>#</th><th>SSID</th><th>BSSID</th><th>CH</th>
      <th>Hidden</th><th>Security</th><th>Interval</th><th>Sent</th><th>Action</th>
    </tr></thead>
    <tbody id="entry-tbody">
      <tr><td colspan="9" style="color:var(--muted);text-align:center">No entries. Add one above.</td></tr>
    </tbody></table>
    <div class="action-row">
      <button class="btn btn-success" onclick="startAll()">▶ Start</button>
      <button class="btn btn-danger"  onclick="stopAll()">■ Stop</button>
      <button class="btn btn-ghost"   onclick="refresh()">Refresh</button>
      <button class="btn btn-ghost"   onclick="clearAll()">Clear All</button>
    </div>
  </div>
</div>

<script>
async function addEntry() {
  const ssid   = document.getElementById('f-ssid').value.trim();
  const ch     = document.getElementById('f-ch').value;
  const iv     = document.getElementById('f-iv').value;
  const hidden = document.getElementById('f-hidden').checked;
  const open   = document.getElementById('f-open').checked;
  if (!ssid) return;
  let url = `/api/bsp/add?ssid=${encodeURIComponent(ssid)}&ch=${ch}&iv=${iv}`;
  if (hidden) url += '&hidden=1';
  if (open)   url += '&open=1';
  await fetch(url, {method:'POST'});
  document.getElementById('f-ssid').value = '';
  refresh();
}

async function removeEntry(idx) {
  await fetch(`/api/bsp/remove?idx=${idx}`, {method:'POST'});
  refresh();
}

async function startAll() {
  await fetch('/api/bsp/start', {method:'POST'});
  refresh();
}

async function stopAll() {
  await fetch('/api/bsp/stop', {method:'POST'});
  refresh();
}

async function clearAll() {
  await fetch('/api/bsp/clear', {method:'POST'});
  refresh();
}

async function refresh() {
  const r = await fetch('/api/bsp/all').then(x=>x.json()).catch(()=>({}));
  document.getElementById('h-cnt').textContent  = r.count||0;
  document.getElementById('h-sent').textContent = r.total_sent||0;
  const pill = r.running
    ? '<span class="status-pill pill-on">Running</span>'
    : '<span class="status-pill pill-off">Stopped</span>';
  document.getElementById('h-status').innerHTML = pill;

  const ents = r.entries||[];
  document.getElementById('entry-tbody').innerHTML = ents.length
    ? ents.map(e=>`<tr>
        <td>${e.idx}</td>
        <td>${e.ssid||'(hidden)'}</td>
        <td><code style="font-size:10px">${e.bssid}</code></td>
        <td>${e.ch}</td>
        <td>${e.hidden?'Yes':'No'}</td>
        <td>${e.open?'Open':'WPA2'}</td>
        <td>${e.interval_ms}ms</td>
        <td style="font-weight:600">${e.sent}</td>
        <td><button class="btn btn-ghost" style="padding:3px 7px;font-size:10px" onclick="removeEntry(${e.idx})">Remove</button></td>
      </tr>`).join('')
    : '<tr><td colspan="9" style="color:var(--muted);text-align:center">No entries.</td></tr>';
}

refresh();
setInterval(refresh, 2000);
</script>
</body>
</html>
)rawhtml";

inline void wubsp_routes() {
  server.on("/beacon-spoofer", HTTP_GET, []() {
    server.send_P(200, "text/html", WUBSP_PAGE);
  });
}

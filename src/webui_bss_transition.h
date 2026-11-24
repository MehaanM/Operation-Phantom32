#pragma once
// webui_bss_transition.h — 802.11v BSS Transition Management UI.
// Route: /bss-transition. Prefix: wubst_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUBST_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>802.11v BSS Transition — Phantom32</title>
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
.tabs{display:flex;gap:4px;margin-bottom:14px;}
.tab{padding:5px 12px;border-radius:5px;font-size:12px;cursor:pointer;border:1px solid var(--border);color:var(--muted);}
.tab.active{background:rgba(0,212,255,.12);color:var(--accent);border-color:var(--accent);}
table{width:100%;border-collapse:collapse;font-size:12px;}
th{text-align:left;padding:7px 8px;font-size:10px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;}
td{padding:7px 8px;border-bottom:1px solid var(--border);}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
.badge{padding:2px 7px;border-radius:4px;font-size:9px;font-weight:700;display:inline-block;}
.ev-row{padding:7px 8px;border-bottom:1px solid var(--border);font-size:11px;}
.ev-meta{display:flex;gap:8px;flex-wrap:wrap;align-items:center;}
.ev-ts{color:var(--muted);min-width:65px;}
.ev-reason{color:var(--muted);font-size:10px;margin-top:3px;}
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
    <a href="/rrm">802.11k RRM</a>
    <a href="/bss-transition" class="active">802.11v BSS</a>
    <a href="/handshakes">Handshakes</a>
    <a href="/security">Security</a>
  </nav>
</div>
<div class="container">
  <h1>802.11v BSS Transition Management</h1>
  <p class="subtitle">Monitors BSS Transition Request/Response frames used by enterprise APs to steer clients to better access points (roaming optimization).</p>

  <div class="hero">
    <div class="stat"><label>BTM Requests</label><div class="val" id="h-reqs">0</div></div>
    <div class="stat"><label>Accepts</label><div class="val" id="h-acc" style="color:var(--success)">0</div></div>
    <div class="stat"><label>Rejects</label><div class="val" id="h-rej" style="color:var(--danger)">0</div></div>
    <div class="stat"><label>Disassoc Imminent</label><div class="val" id="h-dis" style="color:var(--warn)">0</div></div>
  </div>

  <div class="section">
    <div class="tabs">
      <div class="tab active" onclick="showTab('aps',this)">AP BTM Stats</div>
      <div class="tab" onclick="showTab('targets',this)">Target APs</div>
      <div class="tab" onclick="showTab('events',this)">Events Log</div>
    </div>

    <div id="tab-aps">
      <table>
        <thead><tr>
          <th>BSSID</th><th>SSID</th><th>CH</th><th>BTM Cap</th>
          <th>Reqs</th><th>Accepts</th><th>Rejects</th><th>Disassoc</th>
        </tr></thead>
        <tbody id="ap-tbody">
          <tr><td colspan="8" style="color:var(--muted);text-align:center">Waiting for BTM frames…</td></tr>
        </tbody>
      </table>
    </div>

    <div id="tab-targets" style="display:none">
      <table>
        <thead><tr>
          <th>BSSID</th><th>Channel</th><th>Preference</th><th>Times Targeted</th>
        </tr></thead>
        <tbody id="target-tbody">
          <tr><td colspan="4" style="color:var(--muted);text-align:center">No transition targets seen.</td></tr>
        </tbody>
      </table>
    </div>

    <div id="tab-events" style="display:none">
      <div id="ev-list">
        <div style="color:var(--muted);text-align:center;padding:20px;">No BSS TM events.</div>
      </div>
    </div>

    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>
</div>

<script>
function showTab(name, el) {
  document.querySelectorAll('.tab').forEach(t=>t.classList.remove('active'));
  el.classList.add('active');
  document.getElementById('tab-aps').style.display     = name==='aps'?'':'none';
  document.getElementById('tab-targets').style.display = name==='targets'?'':'none';
  document.getElementById('tab-events').style.display  = name==='events'?'':'none';
}

function status_badge(code) {
  if(code===0) return '<span class="badge" style="background:rgba(34,197,94,.15);color:var(--success)">Accept</span>';
  const rej_names = {1:'Unspec',2:'BW',3:'BSS',5:'Delay',6:'Leaving'};
  return `<span class="badge" style="background:rgba(239,68,68,.15);color:var(--danger)">${rej_names[code]||'Reject'}</span>`;
}

async function refresh() {
  const [data, events] = await Promise.all([
    fetch('/api/bst/all').then(x=>x.json()).catch(()=>({})),
    fetch('/api/bst/events').then(x=>x.json()).catch(()=>[]),
  ]);

  document.getElementById('h-reqs').textContent = data.total_requests||0;
  document.getElementById('h-acc').textContent  = data.total_accepts||0;
  document.getElementById('h-rej').textContent  = data.total_rejects||0;
  document.getElementById('h-dis').textContent  = data.total_disassoc||0;

  const aps = data.aps||[];
  document.getElementById('ap-tbody').innerHTML = aps.length
    ? aps.map(a=>`<tr>
        <td><code style="font-size:10px">${a.bssid}</code></td>
        <td>${a.ssid||'—'}</td><td>${a.ch}</td>
        <td>${a.btm_cap?'<span class="badge" style="background:rgba(0,212,255,.12);color:var(--accent)">BTM</span>':'—'}</td>
        <td>${a.reqs}</td>
        <td style="color:var(--success)">${a.accepts}</td>
        <td style="color:var(--danger)">${a.rejects}</td>
        <td style="color:var(--warn)">${a.disassoc}</td>
      </tr>`).join('')
    : '<tr><td colspan="8" style="color:var(--muted);text-align:center">No BTM APs.</td></tr>';

  const targets = data.targets||[];
  document.getElementById('target-tbody').innerHTML = targets.length
    ? targets.sort((a,b)=>b.seen-a.seen).map(t=>`<tr>
        <td><code style="font-size:10px">${t.bssid}</code></td>
        <td>CH ${t.ch}</td>
        <td>${t.pref}/255</td>
        <td>${t.seen}</td>
      </tr>`).join('')
    : '<tr><td colspan="4" style="color:var(--muted);text-align:center">No targets.</td></tr>';

  document.getElementById('ev-list').innerHTML = events.length
    ? events.slice().reverse().map(e=>`
      <div class="ev-row">
        <div class="ev-meta">
          <span class="ev-ts">${new Date(e.ts*1000).toLocaleTimeString()}</span>
          <span class="badge" style="background:${e.req?'rgba(0,212,255,.12)':'rgba(34,197,94,.12)'};color:${e.req?'var(--accent)':'var(--success)'}">${e.req?'Request':'Response'}</span>
          ${!e.req?status_badge(e.status):''}
          ${e.disassoc?'<span class="badge" style="background:rgba(239,68,68,.15);color:var(--danger)">Disassoc Imminent</span>':''}
          <code style="font-size:10px">${e.ap}</code>
          ${e.ssid?`<span style="color:var(--text)">${e.ssid}</span>`:''}
        </div>
        <div class="ev-reason">${e.reason}</div>
      </div>`).join('')
    : '<div style="color:var(--muted);text-align:center;padding:20px;">No events.</div>';
}

async function clearData() {
  await fetch('/api/bst/clear',{method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 4000);
</script>
</body>
</html>
)rawhtml";

inline void wubst_routes() {
  server.on("/bss-transition", HTTP_GET, []() {
    server.send_P(200, "text/html", WUBST_PAGE);
  });
}

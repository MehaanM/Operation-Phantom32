#pragma once
// webui_assoc.h — Association/disassociation events web UI.
// Route: /associations. Prefix: wuasc_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUASC_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Associations — Phantom32</title>
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
.stat-row{display:grid;grid-template-columns:repeat(5,1fr);gap:12px;margin-bottom:20px;}
.stat{background:var(--bg2);border:1px solid var(--border);border-radius:8px;padding:14px;text-align:center;}
.stat label{font-size:11px;color:var(--muted);text-transform:uppercase;}
.stat .val{font-size:22px;font-weight:700;color:var(--accent);margin-top:4px;}
.btn{padding:7px 14px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;transition:all .2s;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.btn-danger{background:var(--danger);color:#fff;}
.action-row{display:flex;gap:8px;flex-wrap:wrap;margin-top:14px;}
.tabs{display:flex;gap:4px;margin-bottom:16px;border-bottom:1px solid var(--border);}
.tab{padding:8px 16px;cursor:pointer;font-size:13px;color:var(--muted);border-bottom:2px solid transparent;margin-bottom:-1px;transition:all .2s;}
.tab.act{color:var(--accent);border-bottom-color:var(--accent);}
.pane{display:none;} .pane.show{display:block;}
table{width:100%;border-collapse:collapse;font-size:13px;}
th{text-align:left;padding:7px 10px;font-size:11px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;}
td{padding:7px 10px;border-bottom:1px solid var(--border);vertical-align:middle;}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
.ev-assoc{color:var(--success);}
.ev-reassoc{color:#22d3ee;}
.ev-disassoc{color:var(--warn);}
.ev-deauth{color:var(--danger);}
.ev-auth_ok{color:var(--success);}
.ev-auth_fail{color:var(--danger);}
.sess-active{color:var(--success);}
.sess-ended{color:var(--muted);}
@media(max-width:800px){.stat-row{grid-template-columns:repeat(3,1fr);}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/">Dashboard</a>
    <a href="/forensics">Forensics</a>
    <a href="/devices">Devices</a>
    <a href="/probes">Probes</a>
    <a href="/associations" class="active">Associations</a>
    <a href="/settings">Settings</a>
  </nav>
</div>

<div class="container">
  <h1>Association Events</h1>
  <p class="subtitle">Real-time 802.11 association, authentication, and deauthentication event tracking.</p>

  <div class="stat-row">
    <div class="stat"><label>Total Events</label><div class="val" id="st-total">0</div></div>
    <div class="stat"><label>Associations</label><div class="val" id="st-assoc" style="color:var(--success)">0</div></div>
    <div class="stat"><label>Disassocs</label><div class="val" id="st-disassoc" style="color:var(--warn)">0</div></div>
    <div class="stat"><label>Deauths</label><div class="val" id="st-deauth" style="color:var(--danger)">0</div></div>
    <div class="stat"><label>Active Clients</label><div class="val" id="st-active" style="color:var(--accent)">0</div></div>
  </div>

  <div class="section">
    <div class="tabs">
      <div class="tab act" onclick="switchTab(0)">Recent Events</div>
      <div class="tab" onclick="switchTab(1)">Active Sessions</div>
      <div class="tab" onclick="switchTab(2)">Session History</div>
    </div>

    <div id="pane0" class="pane show">
      <div style="overflow-x:auto;">
        <table>
          <thead><tr>
            <th>Time</th><th>Event</th><th>Client</th><th>AP BSSID</th><th>Reason</th><th>RSSI</th><th>Ch</th>
          </tr></thead>
          <tbody id="ev-tbody">
            <tr><td colspan="7" style="color:var(--muted);text-align:center">Waiting for events…</td></tr>
          </tbody>
        </table>
      </div>
    </div>

    <div id="pane1" class="pane">
      <div style="overflow-x:auto;">
        <table>
          <thead><tr>
            <th>Client</th><th>AP BSSID</th><th>SSID</th><th>Duration</th><th>RSSI</th>
          </tr></thead>
          <tbody id="active-tbody">
            <tr><td colspan="5" style="color:var(--muted);text-align:center">No active sessions.</td></tr>
          </tbody>
        </table>
      </div>
    </div>

    <div id="pane2" class="pane">
      <div style="overflow-x:auto;">
        <table>
          <thead><tr>
            <th>Client</th><th>AP BSSID</th><th>SSID</th><th>Duration</th><th>Reason</th><th>Status</th>
          </tr></thead>
          <tbody id="hist-tbody">
            <tr><td colspan="6" style="color:var(--muted);text-align:center">No history.</td></tr>
          </tbody>
        </table>
      </div>
    </div>

    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-danger" onclick="clearAll()">Clear</button>
    </div>
  </div>
</div>

<script>
function switchTab(n) {
  document.querySelectorAll('.tab').forEach((t,i)=>t.classList.toggle('act',i===n));
  document.querySelectorAll('.pane').forEach((p,i)=>p.classList.toggle('show',i===n));
}

const EV_COLORS = {assoc:'#22c55e',reassoc:'#22d3ee',disassoc:'#f59e0b',deauth:'#ef4444',auth_ok:'#22c55e',auth_fail:'#ef4444'};

function fmtDur(s) {
  if (!s || s < 0) return '—';
  if (s < 60) return s + 's';
  if (s < 3600) return Math.floor(s/60) + 'm ' + (s%60) + 's';
  return Math.floor(s/3600) + 'h ' + Math.floor((s%3600)/60) + 'm';
}

async function refresh() {
  const r = await fetch('/api/asc/status').then(x=>x.json()).catch(()=>({}));
  if (!r.total_events && r.total_events !== 0) return;

  document.getElementById('st-total').textContent   = r.total_events||0;
  document.getElementById('st-assoc').textContent   = r.total_assocs||0;
  document.getElementById('st-disassoc').textContent= r.total_disassocs||0;
  document.getElementById('st-deauth').textContent  = r.total_deauths||0;
  document.getElementById('st-active').textContent  = r.active_clients||0;

  // Events
  const evs = r.recent_events||[];
  document.getElementById('ev-tbody').innerHTML = evs.length
    ? evs.slice(0,60).map(e=>{
        const t = new Date(e.ts*1000).toLocaleTimeString();
        const col = EV_COLORS[e.type]||'#64748b';
        return `<tr>
          <td>${t}</td>
          <td style="color:${col};font-weight:600">${e.type}</td>
          <td><code style="font-size:11px">${e.client}</code></td>
          <td><code style="font-size:11px">${e.bssid}</code></td>
          <td>${e.reason||'—'}</td>
          <td>${e.rssi} dBm</td>
          <td>${e.ch}</td>
        </tr>`;
      }).join('')
    : '<tr><td colspan="7" style="color:var(--muted);text-align:center">No events.</td></tr>';

  // Active sessions
  const active = r.active_sessions||[];
  document.getElementById('active-tbody').innerHTML = active.length
    ? active.map(s=>`
        <tr>
          <td><code style="font-size:11px">${s.client}</code></td>
          <td><code style="font-size:11px">${s.bssid}</code></td>
          <td>${s.ssid||'—'}</td>
          <td style="color:var(--success)">${fmtDur(s.duration_s)}</td>
          <td>${s.rssi} dBm</td>
        </tr>`).join('')
    : '<tr><td colspan="5" style="color:var(--muted);text-align:center">No active sessions.</td></tr>';

  // History
  const sr = await fetch('/api/asc/sessions').then(x=>x.json()).catch(()=>[]);
  document.getElementById('hist-tbody').innerHTML = sr.length
    ? sr.map(s=>
        `<tr>
          <td><code style="font-size:11px">${s.client}</code></td>
          <td><code style="font-size:11px">${s.bssid}</code></td>
          <td>${s.ssid||'—'}</td>
          <td>${fmtDur(s.duration_s)}</td>
          <td>${s.reason||'—'}</td>
          <td style="color:${s.active?'#22c55e':'#64748b'}">${s.active?'Active':'Ended'}</td>
        </tr>`).join('')
    : '<tr><td colspan="6" style="color:var(--muted);text-align:center">No history.</td></tr>';
}

async function clearAll() {
  if (!confirm('Clear all association events?')) return;
  await fetch('/api/asc/clear',{method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 3000);
</script>
</body>
</html>
)rawhtml";

inline void wuasc_routes() {
  server.on("/associations", HTTP_GET, []() { server.send_P(200, "text/html", WUASC_PAGE); });
}

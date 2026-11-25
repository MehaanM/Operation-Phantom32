#pragma once
// webui_roaming.h — Roaming analyzer UI.
// Route: /roaming. Prefix: wurom_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUROM_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Roaming — Phantom32</title>
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
.tabs{display:flex;gap:2px;border-bottom:1px solid var(--border);margin-bottom:14px;}
.tab{padding:7px 14px;font-size:12px;cursor:pointer;color:var(--muted);border-radius:5px 5px 0 0;}
.tab.active{color:var(--accent);background:rgba(0,212,255,.08);}
table{width:100%;border-collapse:collapse;font-size:11px;}
th{text-align:left;padding:8px 10px;border-bottom:2px solid var(--border);color:var(--muted);font-weight:600;font-size:10px;text-transform:uppercase;cursor:pointer;}
td{padding:7px 10px;border-bottom:1px solid var(--border);}
tr:hover td{background:var(--bg3);}
.ev-row{display:flex;align-items:center;gap:10px;padding:8px 0;border-bottom:1px solid var(--border);}
.ev-arrow{color:var(--muted);font-size:14px;}
.ev-side{flex:1;}
.ev-bssid{font-size:9px;color:var(--muted);font-family:monospace;}
.ev-ssid{font-size:11px;font-weight:600;}
.ev-ch{font-size:9px;color:var(--muted);}
.ev-rssi{font-size:10px;}
.trig-pill{padding:1px 6px;border-radius:3px;font-size:8px;font-weight:700;}
.t-FastBSS{background:rgba(34,197,94,.12);color:var(--success);}
.t-BTM{background:rgba(0,212,255,.12);color:var(--accent);}
.t-Deauth{background:rgba(239,68,68,.12);color:var(--danger);}
.t-Unknown{background:rgba(100,116,139,.1);color:var(--muted);}
.trans-time{font-size:9px;color:var(--warn);}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;margin-top:12px;}
@media(max-width:700px){.hero{grid-template-columns:1fr 1fr};table{font-size:9px}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/stations">Stations</a>
    <a href="/roaming" class="active">Roaming</a>
    <a href="/auth-sequence">Auth Seq</a>
  </nav>
</div>
<div class="container">
  <h1>Roaming Analyzer</h1>
  <p class="subtitle">Tracks BSS transitions (roaming events): detects 802.11r Fast BSS Transition, AP-requested BTM roams, deauth-driven roams, and measures transition latency.</p>

  <div class="hero">
    <div class="stat"><label>Total Roams</label><div class="val" id="h-ev">0</div></div>
    <div class="stat"><label>Clients</label><div class="val" id="h-cl">0</div></div>
    <div class="stat"><label>Avg Transition</label><div class="val" id="h-avg" style="font-size:14px;color:var(--warn)">—</div></div>
    <div class="stat"><label>Fast BSS</label><div class="val" id="h-fast" style="color:var(--success)">0</div></div>
  </div>

  <div class="section">
    <div class="tabs">
      <div class="tab active" id="tab-ev" onclick="showTab('ev')">Events</div>
      <div class="tab"        id="tab-cl" onclick="showTab('cl')">Clients</div>
    </div>

    <div id="pane-ev">
      <div id="evt-list">
        <div style="color:var(--muted)">No roaming events yet.</div>
      </div>
    </div>

    <div id="pane-cl" style="display:none">
      <div style="overflow-x:auto">
      <table><thead><tr>
        <th>MAC</th><th>AP SSID</th><th>CH</th><th>RSSI</th>
        <th>Total Roams</th><th>Intra-ESS</th><th>Inter-ESS</th>
        <th>Fast BSS</th><th>Avg Trans</th>
      </tr></thead><tbody id="cl-body"></tbody></table>
      </div>
    </div>

    <div class="action-row">
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>
</div>

<script>
let g_data = {events:[], clients:[]};
let g_tab = 'ev';

function showTab(t) {
  g_tab = t;
  document.getElementById('pane-ev').style.display = t === 'ev' ? '' : 'none';
  document.getElementById('pane-cl').style.display = t === 'cl' ? '' : 'none';
  document.getElementById('tab-ev').className = 'tab' + (t === 'ev' ? ' active' : '');
  document.getElementById('tab-cl').className = 'tab' + (t === 'cl' ? ' active' : '');
}

function rssiColor(r) {
  return r >= -60 ? 'var(--success)' : r >= -75 ? 'var(--warn)' : 'var(--danger)';
}

function trigClass(t) {
  if (!t) return 't-Unknown';
  t = t.replace(/\s/g,'');
  if (t === 'FastBSS') return 't-FastBSS';
  if (t === 'BTM') return 't-BTM';
  if (t === 'Deauth') return 't-Deauth';
  return 't-Unknown';
}

function msToTime(ms) {
  if (!ms) return '—';
  if (ms < 1000) return ms + 'ms';
  return (ms/1000).toFixed(2) + 's';
}

function renderEvents() {
  const evts = g_data.events || [];
  document.getElementById('evt-list').innerHTML = evts.length
    ? evts.map(e => `<div class="ev-row">
        <div class="ev-side">
          <div class="ev-ssid">${e.from_ssid||'Unknown'}</div>
          <div class="ev-bssid">${e.from}</div>
          <div class="ev-ch">CH ${e.from_ch}</div>
          <div class="ev-rssi" style="color:${rssiColor(e.from_rssi)}">${e.from_rssi} dBm</div>
        </div>
        <div style="text-align:center">
          <div class="ev-arrow">→</div>
          <span class="trig-pill ${trigClass(e.trigger)}">${e.trigger||'?'}</span>
          ${e.trans_ms ? `<div class="trans-time">${msToTime(e.trans_ms)}</div>` : ''}
          ${e.fast_bss ? '<div style="font-size:8px;color:var(--success)">11r</div>' : ''}
        </div>
        <div class="ev-side" style="text-align:right">
          <div class="ev-ssid">${e.to_ssid||'Unknown'}</div>
          <div class="ev-bssid">${e.to}</div>
          <div class="ev-ch">CH ${e.to_ch}</div>
          <div class="ev-rssi" style="color:${rssiColor(e.to_rssi)}">${e.to_rssi} dBm</div>
        </div>
      </div>`).join('')
    : '<div style="color:var(--muted)">No roaming events detected.</div>';
}

function renderClients() {
  const cls = g_data.clients || [];
  document.getElementById('cl-body').innerHTML = cls.length
    ? cls.map(c => `<tr>
        <td style="font-family:monospace">${c.mac}</td>
        <td>${c.current_ssid||'—'}</td>
        <td>${c.current_ch||'—'}</td>
        <td style="color:${rssiColor(c.current_rssi)}">${c.current_rssi} dBm</td>
        <td style="font-weight:700;color:${c.total_roams>5?'var(--warn)':'var(--text)'}">${c.total_roams}</td>
        <td>${c.intra}</td>
        <td>${c.inter}</td>
        <td style="color:var(--success)">${c.fast_bss}</td>
        <td>${msToTime(c.avg_trans)}</td>
      </tr>`).join('')
    : `<tr><td colspan="9" style="text-align:center;color:var(--muted);padding:20px">No roaming clients.</td></tr>`;
}

async function refresh() {
  const r = await fetch('/api/rom/all').then(x => x.json()).catch(()=>({}));
  g_data = r;
  document.getElementById('h-ev').textContent   = r.total_events || 0;
  document.getElementById('h-cl').textContent   = r.client_count || 0;
  document.getElementById('h-avg').textContent  = r.avg_trans_ms ? r.avg_trans_ms+'ms' : '—';
  const fasts = (r.clients||[]).reduce((s,c) => s + (c.fast_bss||0), 0);
  document.getElementById('h-fast').textContent = fasts;
  renderEvents();
  renderClients();
}

async function clearData() {
  await fetch('/api/rom/clear', {method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 3000);
</script>
</body>
</html>
)rawhtml";

inline void wurom_routes() {
  server.on("/roaming", HTTP_GET, []() { server.send_P(200, "text/html", WUROM_PAGE); });
}

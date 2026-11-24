#pragma once
// webui_deauth.h — Deauthentication storm detector web UI.
// Route: /deauth. Prefix: wudat_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUDAT_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Deauth — Phantom32</title>
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
.hero{display:grid;grid-template-columns:repeat(5,1fr);gap:10px;margin-bottom:16px;}
.stat{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:13px;}
.stat label{font-size:10px;color:var(--muted);text-transform:uppercase;}
.stat .val{font-size:22px;font-weight:800;color:var(--accent);margin-top:3px;}
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:18px;margin-bottom:14px;}
.section h3{font-size:12px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:14px;}
.storm-banner{display:none;background:rgba(239,68,68,.15);border:1px solid var(--danger);
  border-radius:8px;padding:14px;margin-bottom:16px;text-align:center;}
.storm-banner.active{display:block;}
.storm-title{font-size:18px;font-weight:700;color:var(--danger);margin-bottom:4px;}
table{width:100%;border-collapse:collapse;font-size:12px;}
th{text-align:left;padding:6px 8px;font-size:10px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;}
td{padding:6px 8px;border-bottom:1px solid var(--border);}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
.badge-storm{background:rgba(239,68,68,.2);color:var(--danger);padding:1px 7px;border-radius:4px;font-size:10px;font-weight:700;}
.badge-ok{background:rgba(34,197,94,.15);color:var(--success);padding:1px 7px;border-radius:4px;font-size:10px;}
.event-feed{height:200px;overflow-y:auto;background:var(--bg3);border-radius:5px;padding:10px;font-family:monospace;font-size:11px;}
.ev-dis{color:var(--warn);}
.ev-dauth{color:var(--danger);}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.btn-danger{background:var(--danger);color:#fff;}
.action-row{display:flex;gap:8px;margin-top:12px;}
@media(max-width:700px){.hero{grid-template-columns:repeat(3,1fr)}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/threats">Threats</a>
    <a href="/deauth" class="active">Deauth</a>
    <a href="/security">Security</a>
    <a href="/associations">Assoc</a>
  </nav>
</div>
<div class="container">
  <h1>Deauth / Disassoc Tracker</h1>
  <p class="subtitle">Passive detection of 802.11 deauthentication storms and rogue frame injection.</p>

  <div id="storm-banner" class="storm-banner">
    <div class="storm-title">⚠ DEAUTH STORM DETECTED</div>
    <div id="storm-detail" style="color:var(--text);font-size:13px"></div>
  </div>

  <div class="hero">
    <div class="stat"><label>Total Deauths</label><div class="val" id="h-deauth" style="color:var(--danger)">0</div></div>
    <div class="stat"><label>Disassocs</label><div class="val" id="h-disassoc" style="color:var(--warn)">0</div></div>
    <div class="stat"><label>Storms</label><div class="val" id="h-storms" style="color:var(--danger)">0</div></div>
    <div class="stat"><label>Attackers</label><div class="val" id="h-atk">0</div></div>
    <div class="stat"><label>Targets</label><div class="val" id="h-tgt">0</div></div>
  </div>

  <div class="section">
    <h3>Detected Attackers</h3>
    <table>
      <thead><tr>
        <th>MAC</th><th>Total Sent</th><th>Peak Rate</th><th>Status</th><th>Last Seen</th>
      </tr></thead>
      <tbody id="atk-tbody">
        <tr><td colspan="5" style="color:var(--muted);text-align:center">Monitoring…</td></tr>
      </tbody>
    </table>
  </div>

  <div class="section">
    <h3>Recent Events <span style="color:var(--muted);font-weight:400">(newest first)</span></h3>
    <div class="event-feed" id="event-feed">
      <span style="color:var(--muted)">Waiting for deauth/disassoc frames…</span>
    </div>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-danger" onclick="clearAll()">Clear</button>
    </div>
  </div>
</div>

<script>
const REASON_NAMES = ['?','Unspecified','Prev Auth Invalid','STA Leaving',
  'Inactivity','AP Full','Cls2 Non-Auth','Cls3 Non-Assoc',
  'STA Left','Not Authed','PMK Expired','Mesh Canceled'];

function ts_str(ts) {
  return new Date(ts*1000).toLocaleTimeString();
}

function reason_name(r) {
  return REASON_NAMES[r] || 'Code '+r;
}

async function refresh() {
  const st = await fetch('/api/dat/status').then(x=>x.json()).catch(()=>({}));
  const ev = await fetch('/api/dat/events?limit=80').then(x=>x.json()).catch(()=>[]);

  document.getElementById('h-deauth').textContent   = st.total_deauths||0;
  document.getElementById('h-disassoc').textContent = st.total_disassocs||0;
  document.getElementById('h-storms').textContent   = st.storm_count||0;
  document.getElementById('h-atk').textContent      = st.attacker_count||0;
  document.getElementById('h-tgt').textContent      = st.target_count||0;

  // Storm banner
  const storms = st.active_storms||[];
  const banner = document.getElementById('storm-banner');
  if(storms.length) {
    banner.classList.add('active');
    document.getElementById('storm-detail').textContent =
      storms.map(s=>`${s.mac} — ${s.peak_rate} deauths/s (${s.total} total)`).join(' | ');
  } else {
    banner.classList.remove('active');
  }

  // Attackers table
  const atks = st.attackers||[];
  document.getElementById('atk-tbody').innerHTML = atks.length
    ? atks.map(a=>`<tr>
        <td><code>${a.mac}</code></td>
        <td>${a.total}</td>
        <td style="color:${a.peak>5?'var(--danger)':'var(--warn)'}">${a.peak}/s</td>
        <td><span class="${a.storm?'badge-storm':'badge-ok'}">${a.storm?'STORM':'Normal'}</span></td>
        <td>${ts_str(a.ts)}</td>
      </tr>`).join('')
    : '<tr><td colspan="5" style="color:var(--muted);text-align:center">No attackers detected.</td></tr>';

  // Events feed
  const feed = document.getElementById('event-feed');
  if(ev.length) {
    feed.innerHTML = ev.map(e=>{
      const cls = e.dis ? 'ev-dis' : 'ev-dauth';
      const type = e.dis ? 'Disassoc' : 'Deauth  ';
      return `<div class="${cls}">[${ts_str(e.ts)}] ${type} ${e.src} → ${e.dst} ch${e.ch} ${e.rssi}dBm (${reason_name(e.reason)})</div>`;
    }).join('');
    feed.scrollTop = feed.scrollHeight;
  }
}

async function clearAll() {
  if(!confirm('Clear deauth data?')) return;
  await fetch('/api/dat/clear',{method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 1500);
</script>
</body>
</html>
)rawhtml";

inline void wudat_routes() {
  server.on("/deauth", HTTP_GET, []() {
    server.send_P(200, "text/html", WUDAT_PAGE);
  });
}

#pragma once
// webui_handshake.h — WPA handshake capture UI.
// Route: /handshakes. Prefix: wuwht_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUWHT_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>WPA Handshake Tracker — Phantom32</title>
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
.hero{display:grid;grid-template-columns:repeat(3,1fr);gap:10px;margin-bottom:16px;}
.stat{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:13px;}
.stat label{font-size:10px;color:var(--muted);text-transform:uppercase;}
.stat .val{font-size:22px;font-weight:800;color:var(--accent);margin-top:3px;}
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:18px;margin-bottom:14px;}
.section h3{font-size:12px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:14px;}
table{width:100%;border-collapse:collapse;font-size:12px;}
th{text-align:left;padding:7px 8px;font-size:10px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;}
td{padding:7px 8px;border-bottom:1px solid var(--border);}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
.badge{padding:2px 7px;border-radius:4px;font-size:10px;font-weight:600;display:inline-block;}
.full-badge{background:rgba(34,197,94,.15);color:var(--success);}
.partial-badge{background:rgba(245,158,11,.15);color:var(--warn);}
.hex{font-family:monospace;font-size:9px;color:var(--muted);max-width:200px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.btn-primary{background:var(--accent);color:#000;font-weight:600;}
.btn-primary:hover{opacity:.9;}
.action-row{display:flex;gap:8px;margin-top:12px;}
.info-box{background:var(--bg3);border:1px solid var(--border);border-radius:7px;padding:12px;margin-bottom:14px;font-size:11px;color:var(--muted);}
@media(max-width:700px){.hero{grid-template-columns:1fr}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/eapol">EAPOL</a>
    <a href="/handshakes" class="active">Handshakes</a>
    <a href="/security">Security</a>
    <a href="/forensics">Forensics</a>
  </nav>
</div>
<div class="container">
  <h1>WPA Handshake Tracker</h1>
  <p class="subtitle">Passively captures WPA2/WPA3 4-way handshake frames. Partial captures (EAPOL M1+M2) are usable for hash cracking.</p>

  <div class="info-box">
    ℹ️ This tracker captures EAPOL handshake material from frames observed in promiscuous mode. Full captures require all 4 EAPOL messages from the same session. Partial captures (M1+M2 with ANonce, SNonce, MIC) can be exported as WPA*02 Hashcat 22000 format.
  </div>

  <div class="hero">
    <div class="stat"><label>EAPOL M1 Seen</label><div class="val" id="h-m1">0</div></div>
    <div class="stat"><label>Partial Captures</label><div class="val" id="h-partial" style="color:var(--warn)">0</div></div>
    <div class="stat"><label>Full Captures</label><div class="val" id="h-full" style="color:var(--success)">0</div></div>
  </div>

  <div class="section">
    <h3>Captured Handshakes</h3>
    <table>
      <thead><tr>
        <th>AP BSSID</th>
        <th>Client MAC</th>
        <th>SSID</th>
        <th>Type</th>
        <th>Time</th>
        <th>ANonce (truncated)</th>
        <th>MIC (truncated)</th>
      </tr></thead>
      <tbody id="cap-tbody">
        <tr><td colspan="7" style="color:var(--muted);text-align:center">Waiting for handshakes…</td></tr>
      </tbody>
    </table>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-primary" onclick="exportHC()">Export Hashcat</button>
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>

  <div class="section">
    <h3>Active Sessions</h3>
    <table>
      <thead><tr>
        <th>AP</th>
        <th>Client</th>
        <th>SSID</th>
        <th>State</th>
        <th>Last Activity</th>
      </tr></thead>
      <tbody id="sess-tbody">
        <tr><td colspan="5" style="color:var(--muted);text-align:center">No active sessions.</td></tr>
      </tbody>
    </table>
  </div>
</div>

<script>
const STATE_NAMES = ['Idle','M1 Seen','M2 Seen','M3 Seen','M4 Seen','Complete'];

async function refresh() {
  const [caps, sessions] = await Promise.all([
    fetch('/api/wht/captures').then(x=>x.json()).catch(()=>({})),
    fetch('/api/wht/sessions').then(x=>x.json()).catch(()=>[]),
  ]);

  document.getElementById('h-m1').textContent      = caps.total_msg1||0;
  document.getElementById('h-partial').textContent = (caps.capture_count||0) - (caps.total_full||0);
  document.getElementById('h-full').textContent    = caps.total_full||0;

  const c_list = caps.captures||[];
  document.getElementById('cap-tbody').innerHTML = c_list.length
    ? c_list.map(c=>`<tr>
        <td><code style="font-size:10px">${c.ap}</code></td>
        <td><code style="font-size:10px">${c.client}</code></td>
        <td>${c.ssid||'—'}</td>
        <td>${c.full?'<span class="badge full-badge">Full 4-way</span>':'<span class="badge partial-badge">Partial M1+M2</span>'}</td>
        <td>${new Date(c.ts*1000).toLocaleTimeString()}</td>
        <td class="hex">${c.anonce?c.anonce.substring(0,16)+'…':'—'}</td>
        <td class="hex">${c.mic?c.mic.substring(0,16)+'…':'—'}</td>
      </tr>`).join('')
    : '<tr><td colspan="7" style="color:var(--muted);text-align:center">No captures yet.</td></tr>';

  document.getElementById('sess-tbody').innerHTML = sessions.length
    ? sessions.map(s=>`<tr>
        <td><code style="font-size:10px">${s.ap}</code></td>
        <td><code style="font-size:10px">${s.client}</code></td>
        <td>${s.ssid||'—'}</td>
        <td style="color:${s.complete?'var(--success)':s.partial?'var(--warn)':'var(--muted)'}">${STATE_NAMES[s.state]||'Unknown'}</td>
        <td>${new Date(s.last_ts*1000).toLocaleTimeString()}</td>
      </tr>`).join('')
    : '<tr><td colspan="5" style="color:var(--muted);text-align:center">No sessions.</td></tr>';
}

function exportHC() {
  fetch('/api/wht/captures').then(x=>x.json()).then(r=>{
    const caps = r.captures||[];
    const lines = caps.map(c=>{
      const ssid_hex = Array.from(c.ssid||'').map(ch=>ch.charCodeAt(0).toString(16).padStart(2,'0')).join('');
      return `WPA*02*${c.mic}*${c.ap.replace(/:/g,'')}*${c.client.replace(/:/g,'')}*${ssid_hex}*${c.anonce}**`;
    });
    const blob = new Blob([lines.join('\n')], {type:'text/plain'});
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href=url; a.download='phantom32_hc22000.txt'; a.click();
    URL.revokeObjectURL(url);
  });
}

async function clearData() {
  await fetch('/api/wht/clear',{method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 3000);
</script>
</body>
</html>
)rawhtml";

inline void wuwht_routes() {
  server.on("/handshakes", HTTP_GET, []() {
    server.send_P(200, "text/html", WUWHT_PAGE);
  });
}

#pragma once
// webui_wpa_handshake.h — WPA handshake tracker UI.
// Route: /wpa-handshake. Prefix: wuwht_

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
<title>WPA Handshake — Phantom32</title>
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
table{width:100%;border-collapse:collapse;font-size:11px;}
th{text-align:left;padding:7px 8px;font-size:9px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;cursor:pointer;}
th:hover{color:var(--text);}
td{padding:7px 8px;border-bottom:1px solid var(--border);}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
tr.complete-row td:first-child{border-left:3px solid var(--success);}
tr.fail-row td:first-child{border-left:3px solid var(--danger);}
.badge{padding:2px 7px;border-radius:4px;font-size:9px;font-weight:700;display:inline-block;}
.badge-complete{background:rgba(34,197,94,.18);color:var(--success);}
.badge-m1{background:rgba(100,116,139,.18);color:var(--muted);}
.badge-m2{background:rgba(0,212,255,.1);color:var(--accent);}
.badge-m3{background:rgba(245,158,11,.12);color:var(--warn);}
.badge-fail{background:rgba(239,68,68,.18);color:var(--danger);}
.msg-chain{display:flex;gap:3px;align-items:center;}
.msg-dot{width:14px;height:14px;border-radius:50%;border:2px solid var(--border);font-size:7px;font-weight:700;display:flex;align-items:center;justify-content:center;}
.msg-dot.done{border-color:var(--success);background:rgba(34,197,94,.18);color:var(--success);}
.msg-dot.current{border-color:var(--accent);background:rgba(0,212,255,.12);color:var(--accent);}
.msg-dot.fail{border-color:var(--danger);background:rgba(239,68,68,.1);color:var(--danger);}
.msg-line{width:12px;height:2px;background:var(--border);border-radius:1px;}
.pmkid-hex{font-family:monospace;font-size:9px;color:var(--success);word-break:break-all;}
.warning-box{background:rgba(239,68,68,.06);border:1px solid rgba(239,68,68,.3);border-radius:7px;padding:12px;margin-bottom:14px;font-size:12px;line-height:1.6;}
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
    <a href="/pmkid">PMKID</a>
    <a href="/wpa-handshake" class="active">Handshake</a>
    <a href="/security-audit">Audit</a>
  </nav>
</div>
<div class="container">
  <h1>WPA 4-Way Handshake Tracker</h1>
  <p class="subtitle">Passively monitors EAPOL message state machines. A complete M1→M2→M3→M4 sequence captures the pairwise key exchange material needed for offline cracking.</p>

  <div class="warning-box">
    <strong style="color:var(--danger)">Authorization Required:</strong> Capturing WPA handshakes for offline cracking requires explicit authorization from the network owner.
    Passive capture of EAPOL frames is logged for authorized security assessments only.
  </div>

  <div class="hero">
    <div class="stat"><label>Sessions</label><div class="val" id="h-cnt">0</div></div>
    <div class="stat"><label>Complete</label><div class="val" id="h-complete" style="color:var(--success)">0</div></div>
    <div class="stat"><label>Failed</label><div class="val" id="h-fail" style="color:var(--danger)">0</div></div>
    <div class="stat"><label>MIC Failures</label><div class="val" id="h-mic" style="color:var(--warn)">0</div></div>
  </div>

  <div class="section">
    <h3>EAPOL Sessions</h3>
    <table>
      <thead>
        <tr>
          <th onclick="sortTable('ap')">AP</th>
          <th onclick="sortTable('ssid')">SSID</th>
          <th>Client</th>
          <th>CH</th>
          <th onclick="sortTable('state')">State</th>
          <th>Progress</th>
          <th>PMKID</th>
          <th>MIC</th>
          <th>Key Ver</th>
        </tr>
      </thead>
      <tbody id="sess-tbody">
        <tr><td colspan="9" style="color:var(--muted);text-align:center">Waiting for EAPOL frames…</td></tr>
      </tbody>
    </table>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>
</div>

<script>
let g_sessions = [];
let g_sort = {key:'state', asc:false};

const KV_NAMES = {0:'?', 1:'TKIP', 2:'CCMP', 3:'GCMP'};

function msgChain(state) {
  const msgs = ['M1','M2','M3','M4'];
  const stateMap = {M1:1, M2:2, M3:3, Complete:4, Failed:-1};
  const cur = stateMap[state] || 0;
  let html = '<div class="msg-chain">';
  msgs.forEach((m, i) => {
    if (i > 0) html += '<div class="msg-line"></div>';
    let cls = '';
    if (cur < 0 && i === 0) cls = 'fail';
    else if (i < cur) cls = 'done';
    else if (i === cur - 1) cls = 'done';
    html += `<div class="msg-dot ${cls}">${m}</div>`;
  });
  return html + '</div>';
}

function stateBadge(s) {
  if (s === 'Complete') return '<span class="badge badge-complete">✓ Complete</span>';
  if (s === 'Failed')   return '<span class="badge badge-fail">✗ Failed</span>';
  if (s === 'M1') return '<span class="badge badge-m1">M1</span>';
  if (s === 'M2') return '<span class="badge badge-m2">M2</span>';
  if (s === 'M3') return '<span class="badge badge-m3">M3</span>';
  return '<span class="badge badge-m1">—</span>';
}

function sortTable(key) {
  if (g_sort.key === key) g_sort.asc = !g_sort.asc;
  else { g_sort.key = key; g_sort.asc = false; }
  renderTable();
}

function renderTable() {
  const sorted = [...g_sessions].sort((a,b) => {
    const va = a[g_sort.key], vb = b[g_sort.key];
    if (typeof va === 'number') return g_sort.asc ? va - vb : vb - va;
    return g_sort.asc ? String(va).localeCompare(String(vb)) : String(vb).localeCompare(String(va));
  });
  document.getElementById('sess-tbody').innerHTML = sorted.length
    ? sorted.map(s => `<tr class="${s.complete?'complete-row':s.mic_failed?'fail-row':''}">
        <td><code style="font-size:9px">${s.ap}</code></td>
        <td>${s.ssid || '<em style="color:var(--muted)">Hidden</em>'}</td>
        <td><code style="font-size:9px">${s.client}</code></td>
        <td>${s.ch}</td>
        <td>${stateBadge(s.state)}</td>
        <td>${msgChain(s.state)}</td>
        <td>${s.has_pmkid
          ? `<div class="pmkid-hex">${s.pmkid.match(/.{1,8}/g).join(' ')}</div>`
          : '<span style="color:var(--muted)">—</span>'}</td>
        <td>${s.mic_failed
          ? '<span style="color:var(--danger)">✗ Fail</span>'
          : (s.state !== 'Idle' ? '<span style="color:var(--success)">✓</span>' : '—')}</td>
        <td style="font-size:10px;color:var(--muted)">${KV_NAMES[s.key_ver] || s.key_ver}</td>
      </tr>`).join('')
    : '<tr><td colspan="9" style="color:var(--muted);text-align:center">No sessions.</td></tr>';
}

async function refresh() {
  const r = await fetch('/api/wht/all').then(x => x.json()).catch(() => ({}));
  g_sessions = r.sessions || [];
  document.getElementById('h-cnt').textContent      = r.count || 0;
  document.getElementById('h-complete').textContent = r.total_complete || 0;
  document.getElementById('h-fail').textContent     = r.total_failed || 0;
  document.getElementById('h-mic').textContent      = g_sessions.filter(s => s.mic_failed).length;
  renderTable();
}

async function clearData() {
  await fetch('/api/wht/clear', {method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 3000);
</script>
</body>
</html>
)rawhtml";

inline void wuwht_routes() {
  server.on("/wpa-handshake", HTTP_GET, []() {
    server.send_P(200, "text/html", WUWHT_PAGE);
  });
}

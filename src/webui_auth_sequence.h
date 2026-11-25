#pragma once
// webui_auth_sequence.h — Auth sequence tracker UI.
// Route: /auth-sequence. Prefix: wuaus_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUAUS_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Auth Sequence — Phantom32</title>
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
th{text-align:left;padding:7px 8px;font-size:9px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;cursor:pointer;user-select:none;}
th:hover{color:var(--text);}
td{padding:7px 8px;border-bottom:1px solid var(--border);vertical-align:middle;}
tr:last-child td{border-bottom:none;}
tr:hover td{background:rgba(26,31,46,.7);}
.state-badge{padding:2px 7px;border-radius:4px;font-size:9px;font-weight:700;display:inline-block;}
.s-Connected{background:rgba(34,197,94,.12);color:var(--success);}
.s-Failed,.s-Timeout{background:rgba(239,68,68,.12);color:var(--danger);}
.s-Handshake,.s-Assoc{background:rgba(0,212,255,.1);color:var(--accent);}
.s-Auth,.s-Probing{background:rgba(245,158,11,.1);color:var(--warn);}
/* Step chain */
.step-chain{display:flex;align-items:center;gap:0;}
.step{width:22px;height:22px;border-radius:50%;border:1.5px solid var(--border);font-size:7px;font-weight:700;display:flex;align-items:center;justify-content:center;color:var(--muted);}
.step.done{background:var(--success);border-color:var(--success);color:#000;}
.step.fail{background:var(--danger);border-color:var(--danger);color:#fff;}
.step-line{width:8px;height:1.5px;background:var(--border);}
.step-line.done{background:var(--success);}
.timing-bar{height:4px;background:var(--border);border-radius:2px;width:60px;display:inline-block;vertical-align:middle;margin-left:4px;}
.timing-fill{height:100%;border-radius:2px;}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;margin-top:12px;}
.time-chart{display:flex;gap:4px;align-items:flex-end;height:50px;margin-top:4px;}
.time-bar{flex:1;border-radius:2px 2px 0 0;position:relative;min-height:4px;}
.time-label{font-size:8px;color:var(--muted);text-align:center;}
@media(max-width:700px){.hero{grid-template-columns:1fr 1fr}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/wpa-handshake">WPA</a>
    <a href="/auth-sequence" class="active">Auth Seq</a>
    <a href="/client-assoc">Assoc History</a>
  </nav>
</div>
<div class="container">
  <h1>Authentication Sequence Tracker</h1>
  <p class="subtitle">Monitors the full 802.11 join process — Probe → Auth → Association → EAPOL handshake. Detects authentication failures, timeouts, and sequence anomalies.</p>

  <div class="hero">
    <div class="stat"><label>Sessions</label><div class="val" id="h-cnt">0</div></div>
    <div class="stat"><label>Connected</label><div class="val" id="h-conn" style="color:var(--success)">0</div></div>
    <div class="stat"><label>Failures</label><div class="val" id="h-fail" style="color:var(--danger)">0</div></div>
    <div class="stat"><label>Timeouts</label><div class="val" id="h-to" style="color:var(--warn)">0</div></div>
  </div>

  <div class="section">
    <h3>Join Time Distribution</h3>
    <div class="time-chart" id="time-chart"></div>
  </div>

  <div class="section">
    <h3>Active Sessions</h3>
    <table>
      <thead>
        <tr>
          <th onclick="sort('client')">Client MAC</th>
          <th onclick="sort('ap')">AP BSSID</th>
          <th>SSID</th>
          <th>CH</th>
          <th onclick="sort('state')">State</th>
          <th>Steps</th>
          <th onclick="sort('total_ms')">Join Time</th>
          <th>Timings</th>
          <th>Fail Reason</th>
        </tr>
      </thead>
      <tbody id="sess-tbody">
        <tr><td colspan="9" style="color:var(--muted);text-align:center">No sessions.</td></tr>
      </tbody>
    </table>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>
</div>

<script>
let g_data = {};
let g_sort = {key:'state', asc:false};

const STEP_BITS = [
  {bit:0x01, label:'PR', title:'Probe Req'},
  {bit:0x02, label:'PS', title:'Probe Rsp'},
  {bit:0x04, label:'AQ', title:'Auth Req'},
  {bit:0x08, label:'AR', title:'Auth Rsp'},
  {bit:0x10, label:'SQ', title:'Assoc Req'},
  {bit:0x20, label:'SR', title:'Assoc Rsp'},
  {bit:0x40, label:'E1', title:'EAPOL M1'},
  {bit:0x80, label:'E2', title:'EAPOL M2'},
];

const TIME_LABELS = ['< 100ms', '100-500ms', '500-2s', '> 2s'];
const TIME_COLORS = ['#22c55e','#4b9eff','#f59e0b','#ef4444'];

function stepChain(steps) {
  return '<div class="step-chain">' + STEP_BITS.map((s, i) => {
    const done = !!(steps & s.bit);
    return (i > 0 ? `<div class="step-line${done?'done':''}"></div>` : '') +
      `<div class="step ${done?'done':''}" title="${s.title}">${s.label}</div>`;
  }).join('') + '</div>';
}

const STATE_ORDER = {Failed:5,Timeout:4,Handshake:3,Connected:2,Assoc:1,Auth:0,Probing:0};

function sort(key) {
  if (g_sort.key === key) g_sort.asc = !g_sort.asc;
  else { g_sort.key = key; g_sort.asc = false; }
  renderTable();
}

function renderTimeChart() {
  const hist = g_data.join_time_hist || [0,0,0,0];
  const max = Math.max(...hist, 1);
  document.getElementById('time-chart').innerHTML = hist.map((v, i) => `
    <div style="flex:1;text-align:center">
      <div class="time-bar" style="height:${Math.round((v/max)*46)+4}px;background:${TIME_COLORS[i]}88"></div>
      <div class="time-label">${TIME_LABELS[i]}</div>
      <div class="time-label" style="color:var(--text);font-weight:600">${v}</div>
    </div>`).join('');
}

function formatMs(ms) {
  if (!ms) return '—';
  if (ms < 1000) return ms + 'ms';
  return (ms/1000).toFixed(1) + 's';
}

function rssiColor(r) {
  return r >= -60 ? 'var(--success)' : r >= -75 ? 'var(--warn)' : 'var(--danger)';
}

function timingBar(ms, maxMs) {
  if (!ms || !maxMs) return '';
  const pct = Math.min((ms / maxMs) * 100, 100).toFixed(0);
  const color = ms < 100 ? 'var(--success)' : ms < 500 ? 'var(--warn)' : 'var(--danger)';
  return `<div class="timing-bar"><div class="timing-fill" style="width:${pct}%;background:${color}"></div></div>`;
}

function renderTable() {
  const sessions = (g_data.sessions || []).sort((a,b) => {
    if (g_sort.key === 'state') {
      const oa = STATE_ORDER[a.state]||0, ob = STATE_ORDER[b.state]||0;
      return g_sort.asc ? oa - ob : ob - oa;
    }
    const va = a[g_sort.key], vb = b[g_sort.key];
    if (typeof va === 'number') return g_sort.asc ? va - vb : vb - va;
    return g_sort.asc ? String(va||'').localeCompare(String(vb||'')) : String(vb||'').localeCompare(String(va||''));
  });

  const maxMs = Math.max(...sessions.map(s => s.total_ms||0), 1);
  document.getElementById('sess-tbody').innerHTML = sessions.length
    ? sessions.map(s => `<tr>
        <td><code style="font-size:9px">${s.client}</code></td>
        <td><code style="font-size:9px">${s.ap.slice(-8)}</code></td>
        <td style="font-size:10px">${s.ssid || '<em style="color:var(--muted)">?</em>'}</td>
        <td>${s.ch}</td>
        <td><span class="state-badge s-${s.state.replace(' ','')}">${s.state}</span></td>
        <td>${stepChain(s.steps)}</td>
        <td style="color:${s.total_ms > 2000?'var(--danger)':s.total_ms>500?'var(--warn)':'var(--success)'}">${formatMs(s.total_ms)}</td>
        <td style="font-size:9px;color:var(--muted)">
          P→A: ${formatMs(s.probe_to_auth)}
          A→S: ${formatMs(s.auth_to_assoc)}
          S→E: ${formatMs(s.assoc_to_eapol)}
        </td>
        <td style="color:var(--danger);font-size:10px">${s.fail || ''}</td>
      </tr>`).join('')
    : '<tr><td colspan="9" style="color:var(--muted);text-align:center">No sessions.</td></tr>';
}

async function refresh() {
  const r = await fetch('/api/aus/all').then(x => x.json()).catch(() => g_data);
  g_data = r;
  document.getElementById('h-cnt').textContent  = r.count || 0;
  const sessions = r.sessions || [];
  document.getElementById('h-conn').textContent = sessions.filter(s => s.state === 'Connected').length;
  document.getElementById('h-fail').textContent = r.total_failures || 0;
  document.getElementById('h-to').textContent   = r.total_timeouts || 0;
  renderTimeChart();
  renderTable();
}

async function clearData() {
  await fetch('/api/aus/clear', {method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 2000);
</script>
</body>
</html>
)rawhtml";

inline void wuaus_routes() {
  server.on("/auth-sequence", HTTP_GET, []() { server.send_P(200, "text/html", WUAUS_PAGE); });
}

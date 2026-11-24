#pragma once
// webui_security_audit.h — Security audit dashboard UI.
// Route: /security-audit. Prefix: wusau_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUSAU_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Security Audit — Phantom32</title>
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
.score-ring{display:inline-flex;align-items:center;justify-content:center;width:48px;height:48px;border-radius:50%;font-weight:800;font-size:14px;}
.score-high{background:rgba(34,197,94,.18);color:var(--success);border:2px solid var(--success);}
.score-mid{background:rgba(245,158,11,.18);color:var(--warn);border:2px solid var(--warn);}
.score-low{background:rgba(239,68,68,.18);color:var(--danger);border:2px solid var(--danger);}
.score-bar{height:4px;border-radius:2px;margin-top:3px;}
table{width:100%;border-collapse:collapse;font-size:12px;}
th{text-align:left;padding:7px 8px;font-size:10px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;cursor:pointer;}
th:hover{color:var(--text);}
td{padding:7px 8px;border-bottom:1px solid var(--border);}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
.badge{padding:2px 7px;border-radius:4px;font-size:9px;font-weight:700;display:inline-block;}
.sev-critical{background:rgba(239,68,68,.2);color:var(--danger);}
.sev-high{background:rgba(239,68,68,.12);color:#f87171;}
.sev-medium{background:rgba(245,158,11,.18);color:var(--warn);}
.sev-low{background:rgba(0,212,255,.12);color:var(--accent);}
.sev-info{background:rgba(100,116,139,.18);color:var(--muted);}
.tabs{display:flex;gap:4px;margin-bottom:14px;}
.tab{padding:5px 12px;border-radius:5px;font-size:12px;cursor:pointer;border:1px solid var(--border);color:var(--muted);}
.tab.active{background:rgba(0,212,255,.12);color:var(--accent);border-color:var(--accent);}
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
    <a href="/security">Security</a>
    <a href="/security-audit" class="active">Audit</a>
    <a href="/pmf">PMF/WPA3</a>
    <a href="/threats">Threats</a>
  </nav>
</div>
<div class="container">
  <h1>Network Security Audit</h1>
  <p class="subtitle">Automatic security scoring for every AP. Findings cover authentication, management frame protection, WPS vulnerabilities, and configuration issues.</p>

  <div class="hero">
    <div class="stat"><label>APs Audited</label><div class="val" id="h-cnt">0</div></div>
    <div class="stat"><label>Critical</label><div class="val" id="h-crit" style="color:var(--danger)">0</div></div>
    <div class="stat"><label>High</label><div class="val" id="h-high" style="color:#f87171">0</div></div>
    <div class="stat"><label>Avg Score</label><div class="val" id="h-avg">—</div></div>
  </div>

  <div class="section">
    <div class="tabs">
      <div class="tab active" onclick="showTab('aps',this)">AP Scores</div>
      <div class="tab" onclick="showTab('findings',this)">Findings</div>
    </div>

    <div id="tab-aps">
      <table><thead><tr>
        <th>BSSID</th><th onclick="sortAPs('ssid')">SSID</th><th>CH</th>
        <th onclick="sortAPs('score_total')">Total</th>
        <th onclick="sortAPs('score_auth')">Auth</th>
        <th onclick="sortAPs('score_mgmt')">PMF</th>
        <th onclick="sortAPs('score_proto')">Proto</th>
        <th onclick="sortAPs('score_config')">Config</th>
        <th onclick="sortAPs('findings')">Issues</th>
      </tr></thead>
      <tbody id="ap-tbody">
        <tr><td colspan="9" style="color:var(--muted);text-align:center">Loading…</td></tr>
      </tbody></table>
    </div>

    <div id="tab-findings" style="display:none">
      <table><thead><tr>
        <th>Time</th><th>BSSID</th><th>SSID</th>
        <th>Severity</th><th>Category</th><th>Finding</th>
      </tr></thead>
      <tbody id="find-tbody">
        <tr><td colspan="6" style="color:var(--muted);text-align:center">No findings.</td></tr>
      </tbody></table>
    </div>

    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>
</div>

<script>
let g_aps = [];
let g_sort = {key:'score_total', asc:true};

const SEV_CLASS = {CRITICAL:'sev-critical',HIGH:'sev-high',MEDIUM:'sev-medium',LOW:'sev-low',INFO:'sev-info'};
const SEC_NAMES = ['Open','WEP','WPA','WPA2','WPA3'];

function showTab(name, el) {
  document.querySelectorAll('.tab').forEach(t=>t.classList.remove('active'));
  el.classList.add('active');
  document.getElementById('tab-aps').style.display      = name==='aps'?'':'none';
  document.getElementById('tab-findings').style.display = name==='findings'?'':'none';
}

function sortAPs(k) {
  if(g_sort.key===k) g_sort.asc=!g_sort.asc; else {g_sort.key=k;g_sort.asc=false;}
  renderAPs();
}

function scoreColor(s) {
  if (s >= 70) return 'var(--success)';
  if (s >= 40) return 'var(--warn)';
  return 'var(--danger)';
}

function scoreRing(s) {
  const cls = s>=70?'score-high':s>=40?'score-mid':'score-low';
  return `<div class="score-ring ${cls}">${s}</div>`;
}

function miniBar(s, color) {
  return `<div style="display:flex;align-items:center;gap:4px">
    <span style="font-size:10px;min-width:20px">${s}</span>
    <div style="flex:1;height:4px;background:var(--bg3);border-radius:2px">
      <div style="width:${s}%;height:100%;border-radius:2px;background:${color||scoreColor(s)}"></div>
    </div>
  </div>`;
}

function renderAPs() {
  const sorted = [...g_aps].sort((a,b)=>{
    const va=a[g_sort.key],vb=b[g_sort.key];
    if(typeof va==='number') return g_sort.asc?va-vb:vb-va;
    return g_sort.asc?String(va).localeCompare(String(vb)):String(vb).localeCompare(String(va));
  });
  document.getElementById('ap-tbody').innerHTML = sorted.length
    ? sorted.map(a=>`<tr>
        <td><code style="font-size:10px">${a.bssid}</code></td>
        <td>${a.ssid||'Hidden'}</td>
        <td>${a.ch}</td>
        <td>${scoreRing(a.score_total)}</td>
        <td>${miniBar(a.score_auth)}</td>
        <td>${miniBar(a.score_mgmt)}</td>
        <td>${miniBar(a.score_proto)}</td>
        <td>${miniBar(a.score_config)}</td>
        <td style="color:${a.findings>0?'var(--warn)':'var(--muted)';}">${a.findings}</td>
      </tr>`).join('')
    : '<tr><td colspan="9" style="color:var(--muted);text-align:center">No APs.</td></tr>';
}

async function refresh() {
  const r = await fetch('/api/sau/all').then(x=>x.json()).catch(()=>({}));
  g_aps = r.aps||[];
  document.getElementById('h-cnt').textContent  = r.count||0;

  const finds = r.recent_findings||[];
  document.getElementById('h-crit').textContent = finds.filter(f=>f.sev==='CRITICAL').length;
  document.getElementById('h-high').textContent = finds.filter(f=>f.sev==='HIGH').length;
  if (g_aps.length) {
    const avg = Math.round(g_aps.reduce((s,a)=>s+a.score_total,0)/g_aps.length);
    document.getElementById('h-avg').innerHTML = `<span style="color:${scoreColor(avg)}">${avg}</span>`;
  }
  renderAPs();

  document.getElementById('find-tbody').innerHTML = finds.length
    ? finds.slice().reverse().map(f=>`<tr>
        <td>${new Date(f.ts*1000).toLocaleTimeString()}</td>
        <td><code style="font-size:10px">${f.bssid}</code></td>
        <td>${f.ssid||'—'}</td>
        <td><span class="badge ${SEV_CLASS[f.sev]||''}">${f.sev}</span></td>
        <td style="font-size:10px;color:var(--muted)">${f.cat}</td>
        <td>
          <strong style="font-size:11px">${f.title}</strong>
          <div style="font-size:10px;color:var(--muted)">${f.detail}</div>
        </td>
      </tr>`).join('')
    : '<tr><td colspan="6" style="color:var(--muted);text-align:center">No findings.</td></tr>';
}

async function clearData() {
  await fetch('/api/sau/clear',{method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 5000);
</script>
</body>
</html>
)rawhtml";

inline void wusau_routes() {
  server.on("/security-audit", HTTP_GET, []() {
    server.send_P(200, "text/html", WUSAU_PAGE);
  });
}

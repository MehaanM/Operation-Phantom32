#pragma once
// webui_security.h — Security posture and risk scoring web UI.
// Route: /security. Prefix: wusec_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUSEC_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Security — Phantom32</title>
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
.stat .val{font-size:26px;font-weight:700;margin-top:6px;}
.btn{padding:7px 14px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;transition:all .2s;}
.btn-primary{background:var(--accent);color:#000;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;flex-wrap:wrap;margin-top:14px;}
table{width:100%;border-collapse:collapse;font-size:13px;}
th{text-align:left;padding:7px 10px;font-size:11px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;}
td{padding:7px 10px;border-bottom:1px solid var(--border);vertical-align:middle;}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
.score-bar{height:8px;border-radius:4px;background:var(--bg3);position:relative;min-width:80px;}
.score-fill{height:8px;border-radius:4px;transition:width .4s;}
.grade-A{color:var(--success);font-weight:700;font-size:18px;}
.grade-B{color:#22d3ee;font-weight:700;font-size:18px;}
.grade-C{color:var(--warn);font-weight:700;font-size:18px;}
.grade-D{color:var(--danger);font-weight:700;font-size:18px;}
.grade-F{color:#ff0033;font-weight:700;font-size:18px;}
.issue-list{font-size:11px;color:var(--warn);}
@media(max-width:800px){.stat-row{grid-template-columns:repeat(3,1fr);}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/">Dashboard</a>
    <a href="/threats">Threats</a>
    <a href="/vulns">Vulns</a>
    <a href="/devices">Devices</a>
    <a href="/eapol">EAPOL</a>
    <a href="/security" class="active">Security</a>
    <a href="/settings">Settings</a>
  </nav>
</div>

<div class="container">
  <h1>Security Posture</h1>
  <p class="subtitle">Multi-dimension security scoring for all observed access points.</p>

  <div class="stat-row">
    <div class="stat">
      <label>Scored APs</label>
      <div class="val" id="st-count" style="color:var(--accent)">0</div>
    </div>
    <div class="stat">
      <label>Grade A/A+</label>
      <div class="val" id="st-a" style="color:var(--success)">0</div>
    </div>
    <div class="stat">
      <label>Grade B/C</label>
      <div class="val" id="st-bc" style="color:var(--warn)">0</div>
    </div>
    <div class="stat">
      <label>Grade D/F</label>
      <div class="val" id="st-df" style="color:var(--danger)">0</div>
    </div>
    <div class="stat">
      <label>Open APs</label>
      <div class="val" id="st-open" style="color:var(--danger)">0</div>
    </div>
  </div>

  <div class="section">
    <h3>AP Security Scores</h3>
    <div style="overflow-x:auto;">
      <table>
        <thead><tr>
          <th>Grade</th><th>SSID</th><th>BSSID</th><th>Score</th>
          <th>Enc</th><th>Auth</th><th>PMF</th><th>Proto</th><th>Issues</th>
        </tr></thead>
        <tbody id="ap-tbody">
          <tr><td colspan="9" style="color:var(--muted);text-align:center">
            Scoring will populate as APs are observed…</td></tr>
        </tbody>
      </table>
    </div>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
    </div>
  </div>

  <div class="section">
    <h3>Scoring Legend</h3>
    <div style="display:grid;grid-template-columns:repeat(auto-fill,minmax(200px,1fr));gap:12px;font-size:13px;">
      <div>
        <div style="color:var(--accent);font-weight:600;margin-bottom:6px">Encryption (40%)</div>
        <div style="color:var(--muted)">Open=0, WEP=15, WPA=50, WPA2=75, WPA3=100</div>
      </div>
      <div>
        <div style="color:var(--accent);font-weight:600;margin-bottom:6px">Authentication (20%)</div>
        <div style="color:var(--muted)">PSK=60, Enterprise=90, SAE=100</div>
      </div>
      <div>
        <div style="color:var(--accent);font-weight:600;margin-bottom:6px">Management (25%)</div>
        <div style="color:var(--muted)">No PMF=0, PMF capable=50, PMF required=100</div>
      </div>
      <div>
        <div style="color:var(--accent);font-weight:600;margin-bottom:6px">Protocol (15%)</div>
        <div style="color:var(--muted)">Legacy=20, 11n=55, 11ac=80, 11ax=100</div>
      </div>
    </div>
  </div>
</div>

<script>
function gradeClass(g) {
  if (g.startsWith('A')) return 'grade-A';
  if (g.startsWith('B')) return 'grade-B';
  if (g.startsWith('C')) return 'grade-C';
  if (g.startsWith('D')) return 'grade-D';
  return 'grade-F';
}

function scoreBar(val, max=100) {
  const pct = Math.round(100 * val / max);
  const col = val >= 75 ? '#22c55e' : val >= 50 ? '#f59e0b' : '#ef4444';
  return `<div class="score-bar" style="width:60px"><div class="score-fill" style="width:${pct}%;background:${col}"></div></div>`;
}

async function refresh() {
  const r = await fetch('/api/scs/all').then(x=>x.json()).catch(()=>({}));
  if (!r.aps) return;

  document.getElementById('st-count').textContent = r.count||0;

  let grades = {A:0,BC:0,DF:0,open:0};
  r.aps.forEach(ap=>{
    if (ap.grade==='A'||ap.grade==='A+') grades.A++;
    else if (ap.grade==='B'||ap.grade==='C') grades.BC++;
    else grades.DF++;
    if (ap.encryption===0) grades.open++;
  });
  document.getElementById('st-a').textContent    = grades.A;
  document.getElementById('st-bc').textContent   = grades.BC;
  document.getElementById('st-df').textContent   = grades.DF;
  document.getElementById('st-open').textContent = grades.open;

  // Sort by overall score ascending (worst first)
  const sorted = [...r.aps].sort((a,b)=>a.overall-b.overall);

  document.getElementById('ap-tbody').innerHTML = sorted.map(ap=>`
    <tr>
      <td><span class="${gradeClass(ap.grade)}">${ap.grade}</span></td>
      <td>${ap.ssid||'<hidden>'}</td>
      <td><code style="font-size:11px">${ap.bssid}</code></td>
      <td>${ap.overall} ${scoreBar(ap.overall)}</td>
      <td>${ap.encryption} ${scoreBar(ap.encryption)}</td>
      <td>${ap.authentication} ${scoreBar(ap.authentication)}</td>
      <td>${ap.management} ${scoreBar(ap.management)}</td>
      <td>${ap.protocol} ${scoreBar(ap.protocol)}</td>
      <td class="issue-list">${ap.issues.slice(0,2).join('<br>')}</td>
    </tr>`).join('');
}

refresh();
setInterval(refresh, 8000);
</script>
</body>
</html>
)rawhtml";

inline void wusec_routes() {
  server.on("/security", HTTP_GET, []() { server.send_P(200, "text/html", WUSEC_PAGE); });
}

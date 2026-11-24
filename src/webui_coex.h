#pragma once
// webui_coex.h — Coexistence detector UI.
// Route: /coex. Prefix: wucxd_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUCXD_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Coexistence — Phantom32</title>
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
.two-col{display:grid;grid-template-columns:1fr 1fr;gap:14px;margin-bottom:14px;}
table{width:100%;border-collapse:collapse;font-size:11px;}
th{text-align:left;padding:8px 10px;border-bottom:2px solid var(--border);color:var(--muted);font-weight:600;font-size:10px;text-transform:uppercase;}
td{padding:7px 10px;border-bottom:1px solid var(--border);}
tr:hover td{background:var(--bg3);}
.score-bar{display:flex;align-items:center;gap:6px;}
.score-track{flex:1;height:7px;background:var(--border);border-radius:4px;}
.score-fill{height:100%;border-radius:4px;}
.flag-pill{padding:1px 5px;border-radius:3px;font-size:8px;font-weight:600;}
.flag-y{background:rgba(239,68,68,.12);color:var(--danger);}
.flag-n{background:rgba(100,116,139,.1);color:var(--muted);}
.bw-pill{padding:2px 6px;border-radius:4px;font-size:9px;font-weight:700;}
.bw-20{background:rgba(100,116,139,.15);color:var(--muted);}
.bw-40{background:rgba(0,212,255,.12);color:var(--accent);}
.bw-80{background:rgba(34,197,94,.12);color:var(--success);}
.bw-160{background:rgba(245,158,11,.1);color:var(--warn);}
.mini-hist{display:flex;align-items:flex-end;gap:1px;height:14px;}
.mini-bar{width:3px;border-radius:1px 1px 0 0;}
.csa-row{padding:6px 10px;border-bottom:1px solid var(--border);font-size:11px;}
.csa-ch{font-size:13px;font-weight:700;}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;margin-top:12px;}
@media(max-width:700px){.hero{grid-template-columns:1fr 1fr};.two-col{grid-template-columns:1fr}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/spectrum">Spectrum</a>
    <a href="/coex" class="active">Coexistence</a>
    <a href="/channel-util">Channel Util</a>
  </nav>
</div>
<div class="container">
  <h1>Coexistence Detector</h1>
  <p class="subtitle">Monitors 20/40MHz coexistence (CCI/ACI), channel switch announcements (CSA), DFS radar events, and 40MHz-intolerant AP detection. Estimates per-AP interference score.</p>

  <div class="hero">
    <div class="stat"><label>Monitored APs</label><div class="val" id="h-ap">0</div></div>
    <div class="stat"><label>CSA Events</label><div class="val" id="h-csa" style="color:var(--warn)">0</div></div>
    <div class="stat"><label>Coex Events</label><div class="val" id="h-coex" style="color:var(--danger)">0</div></div>
    <div class="stat"><label>DFS Channels</label><div class="val" id="h-dfs" style="color:var(--accent)">0</div></div>
  </div>

  <div class="two-col">
    <div class="section">
      <h3>AP Coexistence Table</h3>
      <div style="overflow-x:auto">
      <table><thead><tr>
        <th>SSID</th><th>CH</th><th>BW</th><th>Intol</th><th>OBSS</th><th>DFS</th><th>CSA</th><th>Intf Score</th>
      </tr></thead><tbody id="ap-tbl"></tbody></table>
      </div>
    </div>

    <div class="section">
      <h3>Channel Switch Log</h3>
      <div id="csa-log">
        <div style="color:var(--muted)">No CSA events.</div>
      </div>
    </div>
  </div>

  <div class="action-row">
    <button class="btn btn-ghost" onclick="clearData()">Clear</button>
  </div>
</div>

<script>
function bwClass(bw) {
  if (bw <= 20) return 'bw-20';
  if (bw <= 40) return 'bw-40';
  if (bw <= 80) return 'bw-80';
  return 'bw-160';
}

function scoreColor(s) {
  if (s > 60) return 'var(--danger)';
  if (s > 30) return 'var(--warn)';
  return 'var(--success)';
}

function miniHist(hist) {
  if (!hist || !hist.length) return '';
  const max = Math.max(...hist, 1);
  return '<div class="mini-hist">' + hist.map(v => {
    const h = Math.max(1, Math.round((v/max)*12));
    const c = scoreColor(v);
    return `<div class="mini-bar" style="height:${h}px;background:${c}"></div>`;
  }).join('') + '</div>';
}

function flag(v) {
  return `<span class="flag-pill ${v?'flag-y':'flag-n'}">${v?'Yes':'No'}</span>`;
}

async function refresh() {
  const r = await fetch('/api/cxd/all').then(x => x.json()).catch(()=>({}));
  document.getElementById('h-ap').textContent   = r.count || 0;
  document.getElementById('h-csa').textContent  = r.total_csa || 0;
  document.getElementById('h-coex').textContent = r.total_coex || 0;
  const dfsCount = (r.aps||[]).filter(a => a.dfs).length;
  document.getElementById('h-dfs').textContent  = dfsCount;

  // AP table
  document.getElementById('ap-tbl').innerHTML = (r.aps||[]).length
    ? (r.aps||[]).map(a => `<tr>
        <td>${a.ssid||'Hidden'}</td>
        <td>${a.ch}</td>
        <td><span class="bw-pill ${bwClass(a.bw)}">${a.bw||'?'}MHz</span>
          ${a.bw_changes > 0 ? `<span style="font-size:8px;color:var(--warn)"> ↕${a.bw_changes}</span>` : ''}</td>
        <td>${flag(a.intol40)}</td>
        <td>${flag(a.obss)}</td>
        <td>${flag(a.dfs)}</td>
        <td style="color:${a.csa_count>0?'var(--warn)':'var(--muted)'}">${a.csa_count}</td>
        <td>
          <div class="score-bar">
            <div class="score-track">
              <div class="score-fill" style="width:${a.intf_score}%;background:${scoreColor(a.intf_score)}"></div>
            </div>
            <span style="font-size:9px;color:${scoreColor(a.intf_score)};min-width:28px">${a.intf_score}%</span>
          </div>
          ${miniHist(a.intf_hist)}
        </td>
      </tr>`).join('')
    : `<tr><td colspan="8" style="text-align:center;color:var(--muted)">No APs tracked.</td></tr>`;

  // CSA log
  const swList = r.switches || [];
  document.getElementById('csa-log').innerHTML = swList.length
    ? swList.map(sw => `<div class="csa-row">
        <strong>${sw.ssid||sw.bssid}</strong>
        <span class="csa-ch" style="color:var(--warn);margin:0 8px">CH${sw.from} → CH${sw.to}</span>
        <span style="font-size:9px;color:var(--muted)">mode=${sw.mode} tbtt=${sw.count} trigger=${sw.trigger}</span>
      </div>`).join('')
    : '<div style="color:var(--muted)">No channel switches.</div>';
}

async function clearData() {
  await fetch('/api/cxd/clear', {method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 4000);
</script>
</body>
</html>
)rawhtml";

inline void wucxd_routes() {
  server.on("/coex", HTTP_GET, []() {
    server.send_P(200, "text/html", WUCXD_PAGE);
  });
}

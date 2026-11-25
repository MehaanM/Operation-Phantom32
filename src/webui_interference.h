#pragma once
// webui_interference.h — Channel interference analyzer UI.
// Route: /interference. Prefix: wuchi_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUCHI_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Channel Interference — Phantom32</title>
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
.stat .val{font-size:26px;font-weight:800;color:var(--success);margin-top:3px;}
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:18px;margin-bottom:14px;}
.section h3{font-size:12px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:14px;}
.channel-grid{display:grid;grid-template-columns:repeat(13,1fr);gap:4px;align-items:end;height:120px;}
.ch-bar-wrap{display:flex;flex-direction:column;align-items:center;height:100%;}
.ch-bar{width:100%;border-radius:3px 3px 0 0;min-height:4px;transition:height .3s;}
.ch-num{font-size:9px;color:var(--muted);margin-top:3px;}
.ch-score-num{font-size:9px;font-weight:700;margin-top:1px;}
table{width:100%;border-collapse:collapse;font-size:12px;}
th{text-align:left;padding:7px 8px;font-size:10px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;}
td{padding:7px 8px;border-bottom:1px solid var(--border);}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
.score-bar{display:flex;align-items:center;gap:5px;}
.s-track{height:7px;width:70px;background:var(--bg3);border-radius:4px;}
.s-fill{height:100%;border-radius:4px;}
.best-badge{background:rgba(34,197,94,.15);color:var(--success);border-radius:4px;font-size:10px;padding:1px 6px;font-weight:700;}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;margin-top:12px;}
@media(max-width:700px){.hero{grid-template-columns:1fr};.channel-grid{grid-template-columns:repeat(6,1fr)}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/channels">Channels</a>
    <a href="/interference" class="active">Interference</a>
    <a href="/snr">SNR</a>
    <a href="/heatmap">Heatmap</a>
  </nav>
</div>
<div class="container">
  <h1>Channel Interference Analyzer</h1>
  <p class="subtitle">Estimates per-channel spectral interference using AP density and RSSI. Non-overlapping channels: 1, 6, 11.</p>

  <div class="hero">
    <div class="stat"><label>Best Channel</label><div class="val" id="h-best">—</div></div>
    <div class="stat"><label>Best Score</label><div class="val" id="h-bscore">—</div></div>
    <div class="stat"><label>Non-overlapping Recommendation</label><div class="val" style="font-size:16px" id="h-rec">—</div></div>
  </div>

  <div class="section">
    <h3>Channel Quality Scores (Higher = Better)</h3>
    <div class="channel-grid" id="ch-bars"></div>
  </div>

  <div class="section">
    <h3>Detailed Channel Statistics</h3>
    <table>
      <thead><tr>
        <th>CH</th>
        <th>APs</th>
        <th>Strongest</th>
        <th>Utilization</th>
        <th>Interference</th>
        <th>Retry%</th>
        <th>Score</th>
      </tr></thead>
      <tbody id="ch-tbody">
        <tr><td colspan="7" style="color:var(--muted);text-align:center">Loading…</td></tr>
      </tbody>
    </table>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-ghost" onclick="resetData()">Reset</button>
    </div>
  </div>
</div>

<script>
function score_color(s) {
  if(s>=70) return 'var(--success)';
  if(s>=45) return 'var(--warn)';
  return 'var(--danger)';
}

function intf_color(v) {
  if(v<30) return 'var(--success)';
  if(v<60) return 'var(--warn)';
  return 'var(--danger)';
}

async function refresh() {
  const r = await fetch('/api/chi/all').then(x=>x.json()).catch(()=>({}));
  const chs = r.channels||[];
  const best = r.best_channel||'—';
  const bscore = r.best_score||0;

  document.getElementById('h-best').textContent   = best?`Ch ${best}`:'—';
  document.getElementById('h-bscore').textContent = bscore;

  // Non-overlapping recommendation
  const non_ol = [1,6,11];
  const non_ol_chs = chs.filter(c=>non_ol.includes(c.ch)).sort((a,b)=>b.score-a.score);
  document.getElementById('h-rec').textContent = non_ol_chs.length
    ? non_ol_chs.map(c=>`Ch ${c.ch} (${c.score})`).join(' › ')
    : '—';

  // Bar chart
  document.getElementById('ch-bars').innerHTML = chs.map(c=>{
    const col = score_color(c.score);
    const h = Math.max(4, Math.round(c.score * 1.1));
    return `<div class="ch-bar-wrap">
      <div style="flex:1;display:flex;align-items:flex-end;width:100%">
        <div class="ch-bar" style="background:${col};height:${c.score}%"></div>
      </div>
      <div class="ch-num">${c.ch}</div>
      <div class="ch-score-num" style="color:${col}">${c.score}</div>
    </div>`;
  }).join('');

  // Table
  document.getElementById('ch-tbody').innerHTML = chs.map(c=>`<tr>
    <td><strong>${c.ch}</strong>${[1,6,11].includes(c.ch)?'<span class="best-badge" style="margin-left:4px">NI</span>':''}</td>
    <td>${c.aps}</td>
    <td>${c.rssi>-99?c.rssi+' dBm':'—'}</td>
    <td>
      <div class="score-bar">
        <span style="min-width:30px">${c.util}%</span>
        <div class="s-track"><div class="s-fill" style="width:${c.util}%;background:var(--accent)"></div></div>
      </div>
    </td>
    <td>
      <div class="score-bar">
        <span style="min-width:30px;color:${intf_color(c.interference)}">${c.interference}%</span>
        <div class="s-track"><div class="s-fill" style="width:${c.interference}%;background:${intf_color(c.interference)}"></div></div>
      </div>
    </td>
    <td>${c.retry_pct}%</td>
    <td>
      <div class="score-bar">
        <span style="min-width:30px;color:${score_color(c.score)};font-weight:700">${c.score}</span>
        <div class="s-track"><div class="s-fill" style="width:${c.score}%;background:${score_color(c.score)}"></div></div>
      </div>
    </td>
  </tr>`).join('');
}

async function resetData() {
  await fetch('/api/chi/reset',{method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 3000);
</script>
</body>
</html>
)rawhtml";

inline void wuchi_routes() {
  server.on("/interference", HTTP_GET, []() { server.send_P(200, "text/html", WUCHI_PAGE); });
}

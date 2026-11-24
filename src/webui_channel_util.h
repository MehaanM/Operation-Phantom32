#pragma once
// webui_channel_util.h — Channel utilization UI.
// Route: /channel-util. Prefix: wucut_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUCUT_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Channel Utilization — Phantom32</title>
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
.ch-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(240px,1fr));gap:10px;}
.ch-card{background:var(--bg3);border:1px solid var(--border);border-radius:7px;padding:14px;}
.ch-title{font-size:14px;font-weight:700;color:var(--accent);margin-bottom:6px;}
.ch-freq{font-size:10px;color:var(--muted);margin-bottom:10px;}
.util-meter{margin-bottom:8px;}
.util-label{display:flex;justify-content:space-between;font-size:10px;color:var(--muted);margin-bottom:3px;}
.util-track{height:10px;background:var(--border);border-radius:5px;overflow:hidden;}
.util-fill{height:100%;border-radius:5px;transition:width .3s;}
.breakdown{display:flex;height:5px;border-radius:3px;overflow:hidden;margin-bottom:8px;}
.breakdown-seg{transition:flex .3s;}
.mini-chart{height:30px;display:flex;align-items:flex-end;gap:1px;margin-top:8px;}
.mini-bar{flex:1;border-radius:1px 1px 0 0;min-height:1px;}
.ch-meta{display:flex;justify-content:space-between;font-size:9px;color:var(--muted);margin-top:4px;}
.peak-label{font-size:9px;color:var(--warn);}
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
    <a href="/spectrum">Spectrum</a>
    <a href="/channel-util" class="active">Channel Util</a>
    <a href="/phy-stats">PHY Stats</a>
  </nav>
</div>
<div class="container">
  <h1>Channel Utilization</h1>
  <p class="subtitle">Estimates per-channel CCA busy time, data/management/control airtime breakdown, and historical utilization trends. Useful for identifying congested channels and airtime fairness issues.</p>

  <div class="hero">
    <div class="stat"><label>Active Channels</label><div class="val" id="h-ch">0</div></div>
    <div class="stat"><label>Total Frames</label><div class="val" id="h-fr" style="font-size:14px">0</div></div>
    <div class="stat"><label>Busiest Channel</label><div class="val" id="h-busiest" style="font-size:14px">—</div></div>
    <div class="stat"><label>Peak Util</label><div class="val" id="h-peak" style="color:var(--warn)">—</div></div>
  </div>

  <div class="section">
    <h3>Per-Channel Utilization</h3>
    <div class="ch-grid" id="ch-grid">
      <div style="color:var(--muted)">No channel data yet.</div>
    </div>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>
</div>

<script>
function utilColor(pct) {
  if (pct > 70) return 'var(--danger)';
  if (pct > 40) return 'var(--warn)';
  return 'var(--success)';
}

function miniChart(hist) {
  if (!hist || !hist.length) return '';
  const max = Math.max(...hist, 1);
  return hist.map(v => {
    const h = Math.max(2, Math.round((v/max)*28));
    return `<div class="mini-bar" style="height:${h}px;background:var(--accent);opacity:${0.3+v/max*0.7}"></div>`;
  }).join('');
}

function formatNum(n) {
  if (n > 1e6) return (n/1e6).toFixed(1)+'M';
  if (n > 1e3) return (n/1e3).toFixed(1)+'K';
  return String(n);
}

async function refresh() {
  const r = await fetch('/api/cut/all').then(x => x.json()).catch(()=>({}));
  const channels = r.channels || [];
  document.getElementById('h-ch').textContent = r.count || 0;
  document.getElementById('h-fr').textContent = formatNum(r.total_frames||0);

  if (channels.length) {
    const busiest = channels.reduce((a,b) => b.cca_busy > a.cca_busy ? b : a, channels[0]);
    document.getElementById('h-busiest').textContent = 'CH ' + busiest.ch;
    document.getElementById('h-peak').textContent = busiest.peak_util + '%';
  }

  document.getElementById('ch-grid').innerHTML = channels.length
    ? channels.map(c => `<div class="ch-card">
        <div class="ch-title">Channel ${c.ch}</div>
        <div class="ch-freq">${c.freq} MHz &nbsp;·&nbsp; ${c.ap_count} APs &nbsp;·&nbsp; ${c.client_count} Clients</div>
        <div class="util-meter">
          <div class="util-label"><span>CCA Busy</span><span style="color:${utilColor(c.cca_busy)}">${c.cca_busy}%</span></div>
          <div class="util-track"><div class="util-fill" style="width:${c.cca_busy}%;background:${utilColor(c.cca_busy)}"></div></div>
        </div>
        <div class="breakdown" title="Data/Mgmt/Ctrl split">
          <div class="breakdown-seg" style="flex:${c.data_pct||0};background:#22c55e"></div>
          <div class="breakdown-seg" style="flex:${c.mgmt_pct||0};background:#f59e0b"></div>
          <div class="breakdown-seg" style="flex:${c.ctrl_pct||0};background:#64748b"></div>
        </div>
        <div class="ch-meta">
          <span>Data: ${c.data_pct}%</span>
          <span>Mgmt: ${c.mgmt_pct}%</span>
          <span>Ctrl: ${c.ctrl_pct}%</span>
        </div>
        <div class="mini-chart">${miniChart(c.util_hist)}</div>
        <div class="ch-meta">
          <span>Peak: <span class="peak-label">${c.peak_util}%</span></span>
          <span>Retry: ${c.retry}</span>
          <span>Bcast: ${c.bcast}</span>
        </div>
      </div>`)
      .join('')
    : '<div style="color:var(--muted);grid-column:1/-1;text-align:center;padding:20px">No channels observed yet.</div>';
}

async function clearData() {
  await fetch('/api/cut/clear', {method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 2000);
</script>
</body>
</html>
)rawhtml";

inline void wucut_routes() {
  server.on("/channel-util", HTTP_GET, []() {
    server.send_P(200, "text/html", WUCUT_PAGE);
  });
}

#pragma once
// webui_pwr_save.h — Power save tracker UI.
// Route: /power-save. Prefix: wupst_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUPST_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Power Save — Phantom32</title>
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
.ap-block{background:var(--bg3);border:1px solid var(--border);border-radius:7px;padding:14px;margin-bottom:10px;}
.ap-header{display:flex;align-items:center;gap:12px;margin-bottom:12px;}
.ap-title{font-size:14px;font-weight:700;color:var(--accent);}
.ap-bssid{font-size:9px;color:var(--muted);font-family:monospace;}
.ap-meta{font-size:10px;color:var(--muted);margin-left:auto;}
.mini-chart{display:flex;align-items:flex-end;gap:1px;height:20px;width:120px;}
.mini-bar{flex:1;border-radius:1px 1px 0 0;min-height:1px;background:var(--accent);}
.client-table{width:100%;border-collapse:collapse;font-size:10px;margin-top:8px;}
.client-table th{text-align:left;padding:5px 8px;border-bottom:1px solid var(--border);color:var(--muted);font-weight:600;font-size:9px;text-transform:uppercase;}
.client-table td{padding:5px 8px;border-bottom:1px solid rgba(42,48,69,.5);}
.mode-pill{padding:1px 5px;border-radius:3px;font-size:8px;font-weight:600;}
.m-Legacy{background:rgba(100,116,139,.15);color:var(--muted);}
.m-UAPSD{background:rgba(0,212,255,.12);color:var(--accent);}
.m-WMM-PS{background:rgba(34,197,94,.12);color:var(--success);}
.m-TWT{background:rgba(245,158,11,.1);color:var(--warn);}
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
    <a href="/stations">Stations</a>
    <a href="/power-save" class="active">Power Save</a>
    <a href="/phy-stats">PHY Stats</a>
  </nav>
</div>
<div class="container">
  <h1>Power Save Tracker</h1>
  <p class="subtitle">Monitors 802.11 power-save state changes: PS-Poll, U-APSD, WMM-PS, and TWT. Tracks per-client doze duration, duty cycle, and TIM delivery statistics per AP.</p>

  <div class="hero">
    <div class="stat"><label>Tracked APs</label><div class="val" id="h-ap">0</div></div>
    <div class="stat"><label>PS Events</label><div class="val" id="h-ev" style="font-size:14px">0</div></div>
    <div class="stat"><label>Clients in PS</label><div class="val" id="h-now" style="color:var(--success)">0</div></div>
    <div class="stat"><label>TIM Deliveries</label><div class="val" id="h-tim" style="color:var(--accent);font-size:14px">0</div></div>
  </div>

  <div class="section">
    <h3>AP Power Save Overview</h3>
    <div id="ap-list">
      <div style="color:var(--muted)">No AP data yet.</div>
    </div>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>
</div>

<script>
function miniChart(hist) {
  if (!hist || !hist.length) return '';
  const max = Math.max(...hist, 1);
  return '<div class="mini-chart">' + hist.map(v => {
    const h = Math.max(1, Math.round((v/max)*18));
    return `<div class="mini-bar" style="height:${h}px;opacity:${0.4+v/max*0.6}"></div>`;
  }).join('') + '</div>';
}

function modeClass(m) {
  return 'm-' + (m || 'None').replace(/\s/g,'-');
}

function msToTime(ms) {
  if (!ms) return '—';
  if (ms < 1000) return ms + 'ms';
  return (ms/1000).toFixed(1) + 's';
}

function rssiColor(r) {
  return r >= -60 ? 'var(--success)' : r >= -75 ? 'var(--warn)' : 'var(--danger)';
}

async function refresh() {
  const r = await fetch('/api/pst/all').then(x => x.json()).catch(()=>({}));
  const aps = r.aps || [];
  document.getElementById('h-ap').textContent  = r.count || 0;
  document.getElementById('h-ev').textContent  = (r.global_events||0).toLocaleString();

  let totalNow = 0, totalTim = 0;
  aps.forEach(a => { totalNow += a.ps_clients_now||0; totalTim += a.tim_frames||0; });
  document.getElementById('h-now').textContent = totalNow;
  document.getElementById('h-tim').textContent = totalTim.toLocaleString();

  document.getElementById('ap-list').innerHTML = aps.length
    ? aps.map(a => {
        const clientRows = (a.clients||[]).map(c => `<tr>
          <td style="font-family:monospace">${c.mac}</td>
          <td><span class="mode-pill ${modeClass(c.mode)}">${c.mode||'None'}</span></td>
          <td>${c.enter}</td>
          <td>${c.exit}</td>
          <td>${c.polls}</td>
          <td>${msToTime(c.avg_doze)}</td>
          <td>${msToTime(c.total_doze)}</td>
        </tr>`).join('');
        return `<div class="ap-block">
          <div class="ap-header">
            <div>
              <div class="ap-title">${a.ssid||'Hidden'}</div>
              <div class="ap-bssid">${a.bssid}</div>
            </div>
            <div class="ap-meta">
              CH ${a.ch} &nbsp;·&nbsp;
              <span style="color:${rssiColor(a.rssi)}">${a.rssi} dBm</span> &nbsp;·&nbsp;
              DTIM/${a.dtim_period}
            </div>
            ${miniChart(a.ps_hist)}
          </div>
          <div style="font-size:10px;color:var(--muted);margin-bottom:8px">
            Clients in PS: <strong style="color:var(--text)">${a.ps_clients_now}</strong> &nbsp;·&nbsp;
            Events: <strong style="color:var(--text)">${a.ps_events}</strong> &nbsp;·&nbsp;
            TIM: <strong style="color:var(--accent)">${a.tim_frames}</strong>
          </div>
          ${clientRows ? `<table class="client-table"><thead><tr>
            <th>MAC</th><th>Mode</th><th>Enter</th><th>Exit</th><th>PS-Poll</th><th>Avg Doze</th><th>Total Doze</th>
          </tr></thead><tbody>${clientRows}</tbody></table>` : '<div style="font-size:10px;color:var(--muted)">No PS clients observed.</div>'}
        </div>`;
      }).join('')
    : '<div style="color:var(--muted)">No APs tracked yet.</div>';
}

async function clearData() {
  await fetch('/api/pst/clear', {method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 3000);
</script>
</body>
</html>
)rawhtml";

inline void wupst_routes() {
  server.on("/power-save", HTTP_GET, []() { server.send_P(200, "text/html", WUPST_PAGE); });
}

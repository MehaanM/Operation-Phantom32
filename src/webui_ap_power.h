#pragma once
// webui_ap_power.h — AP relative power / RSSI signal strength visualization UI.
// Route: /ap-power. Prefix: wuapp_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUAPP_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>AP Power Map — Phantom32</title>
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
.power-card{background:var(--bg3);border:1px solid var(--border);border-radius:7px;padding:12px;margin-bottom:8px;}
.pc-header{display:flex;gap:10px;align-items:center;margin-bottom:8px;}
.pc-ssid{font-weight:600;font-size:13px;}
.pc-bssid{font-size:10px;color:var(--muted);font-family:monospace;}
.power-meter{position:relative;height:14px;background:var(--bg2);border-radius:7px;overflow:hidden;}
.power-fill{height:100%;border-radius:7px;transition:width .4s;}
.pc-stats{display:grid;grid-template-columns:repeat(5,1fr);gap:4px;margin-top:8px;font-size:10px;}
.pc-stat label{color:var(--muted);display:block;}
.pc-stat .v{font-weight:600;}
.trend-up{color:var(--success);}
.trend-dn{color:var(--danger);}
.trend-flat{color:var(--muted);}
.spark{display:inline-flex;align-items:flex-end;gap:1px;height:14px;vertical-align:middle;margin-top:4px;}
.spark-bar{width:3px;border-radius:1px;}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;margin-top:12px;}
@media(max-width:700px){.hero{grid-template-columns:1fr};.pc-stats{grid-template-columns:1fr 1fr}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/rssi-tracker">RSSI Trends</a>
    <a href="/ap-power" class="active">AP Power</a>
    <a href="/heatmap">Heatmap</a>
    <a href="/analytics">Analytics</a>
  </nav>
</div>
<div class="container">
  <h1>AP Signal Power Map</h1>
  <p class="subtitle">Relative received signal strength for each AP based on averaged RSSI history. Trend arrows show if signal is growing or fading.</p>

  <div class="hero">
    <div class="stat"><label>APs Tracked</label><div class="val" id="h-cnt">0</div></div>
    <div class="stat"><label>Strongest AP</label><div class="val" id="h-best" style="font-size:12px;margin-top:5px">—</div></div>
    <div class="stat"><label>Weakest AP</label><div class="val" id="h-worst" style="font-size:12px;margin-top:5px">—</div></div>
  </div>

  <div class="section">
    <h3>Signal Power Comparison</h3>
    <div id="power-cards">
      <div style="color:var(--muted);text-align:center;padding:20px;">No APs. Waiting for beacons…</div>
    </div>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>
</div>

<script>
function rssiColor(rssi) {
  if (rssi >= -60) return 'var(--success)';
  if (rssi >= -75) return 'var(--warn)';
  return 'var(--danger)';
}

function sparkline(hist) {
  if (!hist || !hist.length) return '';
  const max = Math.max(...hist.map(v=>-v), 1);
  return '<div class="spark">' + hist.map(v=>{
    const h = Math.round(((-v)/max)*12)+2;
    return `<div class="spark-bar" style="height:${h}px;background:${rssiColor(v)}"></div>`;
  }).join('') + '</div>';
}

function trendArrow(d) {
  if (d > 1) return '<span class="trend-up">▲</span>';
  if (d < -1) return '<span class="trend-dn">▼</span>';
  return '<span class="trend-flat">→</span>';
}

async function refresh() {
  const r = await fetch('/api/app/all').then(x=>x.json()).catch(()=>({}));
  const aps = r.aps||[];
  document.getElementById('h-cnt').textContent = r.count||0;

  if (aps.length) {
    const best  = aps.reduce((a,b)=>a.rssi_avg>b.rssi_avg?a:b);
    const worst = aps.reduce((a,b)=>a.rssi_avg<b.rssi_avg?a:b);
    document.getElementById('h-best').innerHTML  = `<span style="font-size:14px">${best.ssid||best.bssid.substring(0,8)}</span><br><span style="font-size:12px;color:var(--success)">${best.rssi_avg}dBm</span>`;
    document.getElementById('h-worst').innerHTML = `<span style="font-size:14px">${worst.ssid||worst.bssid.substring(0,8)}</span><br><span style="font-size:12px;color:var(--danger)">${worst.rssi_avg}dBm</span>`;
  }

  const sorted = [...aps].sort((a,b)=>b.relative_power-a.relative_power);
  document.getElementById('power-cards').innerHTML = sorted.length
    ? sorted.map(a=>`
      <div class="power-card">
        <div class="pc-header">
          <div>
            <div class="pc-ssid">${a.ssid||'Hidden'} ${trendArrow(a.trend)}</div>
            <div class="pc-bssid">${a.bssid}</div>
          </div>
          <div style="font-size:18px;font-weight:800;color:${rssiColor(a.rssi_avg)};margin-left:auto">${a.rssi_avg}dBm</div>
        </div>
        <div class="power-meter">
          <div class="power-fill" style="width:${a.relative_power}%;background:${rssiColor(a.rssi_avg)}"></div>
        </div>
        <div class="pc-stats">
          <div class="pc-stat"><label>Min</label><div class="v" style="color:var(--danger)">${a.rssi_min}dBm</div></div>
          <div class="pc-stat"><label>Avg</label><div class="v">${a.rssi_avg}dBm</div></div>
          <div class="pc-stat"><label>Max</label><div class="v" style="color:var(--success)">${a.rssi_max}dBm</div></div>
          <div class="pc-stat"><label>σ</label><div class="v">${a.rssi_sigma}dB</div></div>
          <div class="pc-stat"><label>CH</label><div class="v">${a.ch}</div></div>
        </div>
        <div>${sparkline(a.hist)}</div>
      </div>`).join('')
    : '<div style="color:var(--muted);text-align:center;padding:20px;">No APs.</div>';
}

async function clearData() {
  await fetch('/api/app/clear',{method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 2500);
</script>
</body>
</html>
)rawhtml";

inline void wuapp_routes() {
  server.on("/ap-power", HTTP_GET, []() {
    server.send_P(200, "text/html", WUAPP_PAGE);
  });
}

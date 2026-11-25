#pragma once
// webui_timing.h — Timing anomaly detector UI.
// Route: /timing. Prefix: wutad_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUTAD_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Timing — Phantom32</title>
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
.two-col{display:grid;grid-template-columns:1.4fr 1fr;gap:14px;margin-bottom:14px;}
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:18px;margin-bottom:14px;}
.section h3{font-size:12px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:14px;}
table{width:100%;border-collapse:collapse;font-size:10px;}
th{text-align:left;padding:8px 10px;border-bottom:2px solid var(--border);color:var(--muted);font-weight:600;font-size:9px;text-transform:uppercase;}
td{padding:7px 10px;border-bottom:1px solid var(--border);}
tr:hover td{background:var(--bg3);}
.anom-pill{padding:1px 6px;border-radius:3px;font-size:8px;font-weight:700;color:#000;}
.a-BeaconJitter{background:#f59e0b;}
.a-TSFJump{background:#8b5cf6;}
.a-FastProbe{background:#06b6d4;}
.a-AuthFlood{background:#ef4444;}
.a-DeauthBurst{background:#ef4444;}
.a-FrameGap{background:#64748b;}
.a-ClockSkew{background:#8b5cf6;}
.mini-chart{display:flex;align-items:flex-end;gap:1px;height:14px;}
.mini-bar{flex:1;border-radius:1px 1px 0 0;min-height:1px;}
.anom-row{padding:5px 8px;border-bottom:1px solid var(--border);font-size:9px;display:flex;gap:8px;align-items:center;flex-wrap:wrap;}
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
    <a href="/timing" class="active">Timing</a>
    <a href="/beacon-rates">Beacons</a>
    <a href="/ds-state">DS State</a>
  </nav>
</div>
<div class="container">
  <h1>Timing Anomaly Detector</h1>
  <p class="subtitle">Monitors 802.11 timing parameters for suspicious patterns: beacon jitter, TSF drift and jumps, rapid auth/deauth bursts, probe sweep timing (scanner detection), and clock skew anomalies.</p>

  <div class="hero">
    <div class="stat"><label>Monitored APs</label><div class="val" id="h-ap">0</div></div>
    <div class="stat"><label>Total Anomalies</label><div class="val" id="h-anom" style="color:var(--danger)">0</div></div>
    <div class="stat"><label>Probe Sweeps</label><div class="val" id="h-sweep" style="color:var(--warn)">0</div></div>
    <div class="stat"><label>High Jitter APs</label><div class="val" id="h-jitter" style="color:var(--warn)">0</div></div>
  </div>

  <div class="two-col">
    <div class="section">
      <h3>AP Timing Profiles</h3>
      <div style="overflow-x:auto">
      <table>
        <thead><tr>
          <th>BSSID</th>
          <th>SSID</th>
          <th>CH</th>
          <th>Interval</th>
          <th>Last Jitter</th>
          <th>Max Jitter</th>
          <th>TSF ppm</th>
          <th>Jitter Hist</th>
        </tr></thead>
        <tbody id="ap-body"></tbody>
      </table>
      </div>
      <div class="action-row">
        <button class="btn btn-ghost" onclick="clearData()">Clear</button>
      </div>
    </div>

    <div class="section">
      <h3>Anomaly Log</h3>
      <div id="anom-log">
        <div style="color:var(--muted)">No anomalies detected.</div>
      </div>
    </div>
  </div>
</div>

<script>
const ANOM_COLORS = {
  BeaconJitter:'#f59e0b', TSFJump:'#8b5cf6', FastProbe:'#06b6d4',
  AuthFlood:'#ef4444', DeauthBurst:'#ef4444', FrameGap:'#64748b',
  ClockSkew:'#8b5cf6', Unknown:'#64748b'
};

function miniChart(hist) {
  if (!hist || !hist.length) return '';
  const max = Math.max(...hist, 1);
  return hist.map(v => {
    const h = Math.max(1, Math.round((v/max)*12));
    const pct = v/max;
    const c = pct > .7 ? 'var(--danger)' : pct > .4 ? 'var(--warn)' : 'var(--accent)';
    return `<div class="mini-bar" style="height:${h}px;background:${c}"></div>`;
  }).join('');
}

function jitterColor(j) {
  if (j > 50) return 'var(--danger)';
  if (j > 20) return 'var(--warn)';
  return 'var(--success)';
}

function ppmColor(p) {
  if (p > 500) return 'var(--danger)';
  if (p > 200) return 'var(--warn)';
  return 'var(--muted)';
}

async function refresh() {
  const r = await fetch('/api/tad/all').then(x => x.json()).catch(()=>({}));
  const aps = r.aps || [];
  document.getElementById('h-ap').textContent     = r.ap_count || 0;
  document.getElementById('h-anom').textContent   = r.total_anomalies || 0;
  document.getElementById('h-sweep').textContent  = r.probe_sweeps || 0;
  document.getElementById('h-jitter').textContent = aps.filter(a => a.jitter_events > 0).length;

  document.getElementById('ap-body').innerHTML = aps.length
    ? aps.map(a => `<tr>
        <td style="font-family:monospace;font-size:9px">${a.bssid}</td>
        <td>${a.ssid||'—'}</td>
        <td>${a.ch}</td>
        <td style="color:var(--muted)">${a.interval_ms}ms</td>
        <td style="color:${jitterColor(a.last_jitter)}">${a.last_jitter}ms</td>
        <td style="color:${jitterColor(a.max_jitter)}">${a.max_jitter}ms</td>
        <td style="color:${ppmColor(a.tsf_drift_ppm)}">${a.tsf_drift_ppm}</td>
        <td><div class="mini-chart">${miniChart(a.jitter_hist)}</div></td>
      </tr>`).join('')
    : `<tr><td colspan="8" style="text-align:center;color:var(--muted);padding:16px">No AP timing data.</td></tr>`;

  const anoms = r.anomalies || [];
  document.getElementById('anom-log').innerHTML = anoms.length
    ? anoms.map(e => {
        const c = ANOM_COLORS[e.type] || '#64748b';
        return `<div class="anom-row">
          <span style="padding:1px 6px;border-radius:3px;font-size:8px;font-weight:700;color:#000;background:${c}">${e.type}</span>
          <span style="font-family:monospace;font-size:9px">${e.mac}</span>
          <span style="color:var(--muted)">CH${e.ch}</span>
          <span style="color:var(--warn)">${e.delta}${e.type==='AuthFlood'?'/s':'ms'}</span>
          <span style="color:var(--muted);margin-left:auto">${e.ts}s</span>
        </div>`;
      }).join('')
    : '<div style="color:var(--muted);padding:8px">No anomalies detected.</div>';
}

async function clearData() {
  await fetch('/api/tad/clear', {method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 2000);
</script>
</body>
</html>
)rawhtml";

inline void wutad_routes() {
  server.on("/timing", HTTP_GET, []() { server.send_P(200, "text/html", WUTAD_PAGE); });
}

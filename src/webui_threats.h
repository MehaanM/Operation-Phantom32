#pragma once
// webui_threats.h — Threat intelligence & live alert dashboard.
// Real-time threat feed, alert timeline, IOC table, severity heat-ring.
// Prefix: wut_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUT_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Phantom32 · Threat Intel</title>
<style>
:root{--bg:#0a0e1a;--surface:#111827;--surface2:#1a2235;--border:#2d3748;--accent:#00d4ff;--red:#ef4444;--orange:#f97316;--yellow:#f59e0b;--green:#10b981;--muted:#64748b;--text:#e2e8f0;}
*{margin:0;padding:0;box-sizing:border-box;}
body{background:var(--bg);color:var(--text);font-family:'Segoe UI',system-ui,sans-serif;font-size:13px;overflow-x:hidden;}
header{background:var(--surface);border-bottom:1px solid var(--border);padding:10px 16px;display:flex;align-items:center;gap:12px;flex-wrap:wrap;}
header h1{font-size:16px;font-weight:700;color:var(--accent);}
.badge{background:var(--red);color:#fff;padding:2px 8px;border-radius:12px;font-size:11px;}
nav a{color:var(--muted);text-decoration:none;padding:6px 10px;border-radius:6px;font-size:12px;}
nav a:hover{background:var(--surface2);color:var(--accent);}
.grid{display:grid;grid-template-columns:1fr 1fr 1fr 1fr;gap:12px;padding:14px;}
@media(max-width:900px){.grid{grid-template-columns:1fr 1fr;}}
@media(max-width:500px){.grid{grid-template-columns:1fr;}}
.stat{background:var(--surface);border:1px solid var(--border);border-radius:10px;padding:14px;text-align:center;}
.stat .n{font-size:28px;font-weight:700;line-height:1;}
.stat .l{font-size:11px;color:var(--muted);margin-top:4px;}
.stat.red .n{color:var(--red);}
.stat.orange .n{color:var(--orange);}
.stat.yellow .n{color:var(--yellow);}
.stat.green .n{color:var(--green);}
.panels{display:grid;grid-template-columns:1fr 1fr;gap:12px;padding:0 14px 14px;}
@media(max-width:800px){.panels{grid-template-columns:1fr;}}
.panel{background:var(--surface);border:1px solid var(--border);border-radius:10px;overflow:hidden;}
.panel-h{padding:10px 14px;border-bottom:1px solid var(--border);font-size:12px;font-weight:600;color:var(--accent);display:flex;align-items:center;justify-content:space-between;}
.panel-h .dot{width:8px;height:8px;border-radius:50%;background:var(--red);animation:pulse 1.5s infinite;margin-right:6px;}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:.3}}
.alert-item{padding:9px 14px;border-bottom:1px solid var(--border);display:flex;gap:10px;align-items:flex-start;cursor:pointer;}
.alert-item:hover{background:var(--surface2);}
.alert-item:last-child{border-bottom:none;}
.alert-icon{font-size:16px;line-height:1.3;flex-shrink:0;}
.alert-title{font-weight:600;font-size:12px;}
.alert-meta{color:var(--muted);font-size:11px;margin-top:2px;}
.sev-dot{width:8px;height:8px;border-radius:50%;flex-shrink:0;margin-top:4px;}
.sev-critical{background:var(--red);}
.sev-high{background:var(--orange);}
.sev-medium{background:var(--yellow);}
.sev-low{background:var(--green);}
.sev-info{background:var(--muted);}
canvas{display:block;}
.ioc-table{width:100%;border-collapse:collapse;font-size:12px;}
.ioc-table th{padding:7px 10px;border-bottom:2px solid var(--border);text-align:left;color:var(--muted);font-size:11px;}
.ioc-table td{padding:7px 10px;border-bottom:1px solid var(--border);}
.ioc-table tr:hover td{background:var(--surface2);}
.type-chip{display:inline-block;padding:1px 7px;border-radius:10px;font-size:10px;font-weight:600;}
.chip-mac{background:#00d4ff1a;color:var(--accent);border:1px solid #00d4ff30;}
.chip-ssid{background:#7c3aed1a;color:#a78bfa;border:1px solid #7c3aed30;}
.chip-ch{background:#10b9811a;color:var(--green);border:1px solid #10b98130;}
.chip-ip{background:#f59e0b1a;color:var(--yellow);border:1px solid #f59e0b30;}
.timeline{padding:10px 14px;}
.tl-item{display:flex;gap:10px;margin-bottom:12px;position:relative;}
.tl-item::before{content:'';position:absolute;left:12px;top:24px;bottom:-12px;width:1px;background:var(--border);}
.tl-item:last-child::before{display:none;}
.tl-dot{width:24px;height:24px;border-radius:50%;flex-shrink:0;display:flex;align-items:center;justify-content:center;font-size:12px;border:2px solid;}
.tl-dot.critical{background:#ef44441a;border-color:var(--red);}
.tl-dot.high{background:#f974631a;border-color:var(--orange);}
.tl-dot.medium{background:#f59e0b1a;border-color:var(--yellow);}
.tl-dot.low{background:#10b9811a;border-color:var(--green);}
.tl-body{flex:1;}
.tl-title{font-size:12px;font-weight:600;}
.tl-sub{font-size:11px;color:var(--muted);margin-top:2px;}
.tl-ts{font-size:10px;color:var(--muted);margin-top:2px;}
.heatmap{display:grid;grid-template-columns:repeat(14,1fr);gap:4px;padding:10px 14px;}
.hm-cell{aspect-ratio:1;border-radius:4px;display:flex;align-items:center;justify-content:center;font-size:9px;color:rgba(255,255,255,.7);transition:transform .2s;}
.hm-cell:hover{transform:scale(1.3);z-index:5;}
.top-attackers{padding:0;}
.att-item{padding:8px 14px;border-bottom:1px solid var(--border);display:flex;align-items:center;gap:10px;}
.att-item:last-child{border-bottom:none;}
.att-rank{width:20px;text-align:right;color:var(--muted);font-size:11px;}
.att-mac{font-family:monospace;font-size:11px;flex:1;}
.att-bar-bg{width:80px;height:6px;background:var(--surface2);border-radius:3px;overflow:hidden;}
.att-bar{height:100%;border-radius:3px;background:var(--red);}
.att-cnt{font-size:11px;color:var(--muted);width:40px;text-align:right;}
.empty-msg{padding:30px;text-align:center;color:var(--muted);}
</style>
</head>
<body>
<header>
  <h1>⚡ Phantom32</h1>
  <span class="badge">THREAT INTEL</span>
  <span id="live-badge" style="color:var(--green);font-size:11px;margin-left:4px;">● LIVE</span>
  <nav style="display:flex;gap:2px;margin-left:auto;">
    <a href="/">Home</a>
    <a href="/vulns">Vulns</a>
    <a href="/analytics">Analytics</a>
    <a href="/forensics">Forensics</a>
    <a href="/cli">CLI</a>
  </nav>
</header>

<div class="grid">
  <div class="stat red"><div class="n" id="cnt-critical">0</div><div class="l">Critical Alerts</div></div>
  <div class="stat orange"><div class="n" id="cnt-high">0</div><div class="l">High Alerts</div></div>
  <div class="stat yellow"><div class="n" id="cnt-deauth">0</div><div class="l">Deauth Events</div></div>
  <div class="stat green"><div class="n" id="cnt-pmkid">0</div><div class="l">PMKID Captured</div></div>
</div>

<div class="panels">
  <div class="panel" style="grid-column:1/-1;">
    <div class="panel-h">
      <span><span class="dot"></span>Alert Timeline</span>
      <button onclick="clearAlerts()" style="background:none;border:1px solid var(--border);color:var(--muted);padding:3px 8px;border-radius:4px;cursor:pointer;font-size:11px;">Clear</button>
    </div>
    <div id="timelineWrap" style="max-height:220px;overflow-y:auto;">
      <div class="timeline" id="timeline"><div class="empty-msg">No alerts yet — scanning…</div></div>
    </div>
  </div>

  <div class="panel">
    <div class="panel-h">Channel Threat Heatmap</div>
    <div class="heatmap" id="heatmap"></div>
    <div style="padding:4px 14px 10px;display:flex;gap:8px;align-items:center;font-size:10px;color:var(--muted);">
      <div style="width:12px;height:12px;background:#1a3a2a;border-radius:2px;"></div>Low
      <div style="width:12px;height:12px;background:#f59e0b88;border-radius:2px;"></div>Med
      <div style="width:12px;height:12px;background:#ef444488;border-radius:2px;"></div>High
    </div>
  </div>

  <div class="panel">
    <div class="panel-h">Top Threat Sources</div>
    <div class="top-attackers" id="topAttackers">
      <div class="empty-msg">Collecting data…</div>
    </div>
  </div>

  <div class="panel" style="grid-column:1/-1;">
    <div class="panel-h">
      <span>Indicators of Compromise (IOC)</span>
      <button onclick="exportIoc()" style="background:none;border:1px solid var(--border);color:var(--muted);padding:3px 8px;border-radius:4px;cursor:pointer;font-size:11px;">Export</button>
    </div>
    <div style="overflow-x:auto;">
      <table class="ioc-table">
        <thead><tr>
          <th>Type</th><th>Value</th><th>Threat</th><th>Count</th><th>Last Seen</th>
        </tr></thead>
        <tbody id="iocTbody"><tr><td colspan="5" class="empty-msg">No IOCs recorded</td></tr></tbody>
      </table>
    </div>
  </div>
</div>

<script>
const SEV_ICONS = {critical:'🔴', high:'🟠', medium:'🟡', low:'🟢', info:'⚪'};
let alerts = [];
let iocs = {};
let channelThreats = new Array(14).fill(0);

function timeAgo(ts) {
  const s = Math.floor(Date.now()/1000) - ts;
  if (s < 60) return s + 's ago';
  if (s < 3600) return Math.floor(s/60) + 'm ago';
  return Math.floor(s/3600) + 'h ago';
}

function addAlert(a) {
  alerts.unshift(a);
  if (alerts.length > 100) alerts.pop();

  if (a.severity === 'critical' || a.severity === 'high') {
    const mac = a.mac || a.bssid;
    if (mac) {
      if (!iocs[mac]) iocs[mac] = {type:'mac', value:mac, threat:a.type, count:0, last_seen:0};
      iocs[mac].count++;
      iocs[mac].last_seen = a.ts;
    }
  }
  if (a.channel >= 1 && a.channel <= 14) {
    channelThreats[a.channel - 1]++;
  }
}

function renderTimeline() {
  if (!alerts.length) {
    document.getElementById('timeline').innerHTML = '<div class="empty-msg">No alerts yet — scanning…</div>';
    return;
  }
  document.getElementById('timeline').innerHTML = alerts.slice(0, 30).map(a =>
    `<div class="tl-item">
      <div class="tl-dot ${a.severity}">${SEV_ICONS[a.severity]||'⚪'}</div>
      <div class="tl-body">
        <div class="tl-title">${escHtml(a.type||'Unknown Alert')}</div>
        <div class="tl-sub">${a.mac||a.bssid ? '<span style="font-family:monospace">' + escHtml(a.mac||a.bssid||'') + '</span>' : ''}${a.ssid ? ' &nbsp;· SSID: <em>' + escHtml(a.ssid) + '</em>' : ''}${a.channel ? ' &nbsp;· CH' + a.channel : ''}</div>
        <div class="tl-ts">${timeAgo(a.ts)}</div>
      </div>
    </div>`
  ).join('');
}

function renderHeatmap() {
  const max = Math.max(1, ...channelThreats);
  document.getElementById('heatmap').innerHTML = channelThreats.map((v, i) => {
    const ratio = v / max;
    const bg = ratio < 0.1 ? '#1a3a2a' :
               ratio < 0.4 ? `rgba(245,158,11,${0.3 + ratio * 0.5})` :
               `rgba(239,68,68,${0.3 + ratio * 0.7})`;
    return `<div class="hm-cell" style="background:${bg}" title="CH${i+1}: ${v} threats">${i+1}</div>`;
  }).join('');
}

function renderTopAttackers() {
  const entries = Object.values(iocs)
    .filter(x => x.type === 'mac')
    .sort((a,b) => b.count - a.count)
    .slice(0, 8);
  if (!entries.length) {
    document.getElementById('topAttackers').innerHTML = '<div class="empty-msg">No threats recorded</div>';
    return;
  }
  const max = entries[0].count;
  document.getElementById('topAttackers').innerHTML = entries.map((e, i) =>
    `<div class="att-item">
      <div class="att-rank">${i+1}</div>
      <div class="att-mac">${escHtml(e.value)}</div>
      <div class="att-bar-bg"><div class="att-bar" style="width:${(e.count/max*100).toFixed(0)}%"></div></div>
      <div class="att-cnt">${e.count}</div>
    </div>`
  ).join('');
}

function renderIoc() {
  const entries = Object.values(iocs).sort((a,b) => b.count - a.count).slice(0, 50);
  if (!entries.length) {
    document.getElementById('iocTbody').innerHTML = '<tr><td colspan="5" class="empty-msg">No IOCs recorded</td></tr>';
    return;
  }
  document.getElementById('iocTbody').innerHTML = entries.map(e =>
    `<tr>
      <td><span class="type-chip chip-${e.type}">${e.type.toUpperCase()}</span></td>
      <td style="font-family:monospace">${escHtml(e.value)}</td>
      <td>${escHtml(e.threat)}</td>
      <td>${e.count}</td>
      <td>${e.last_seen ? timeAgo(e.last_seen) : '—'}</td>
    </tr>`
  ).join('');
}

function escHtml(s) {
  return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');
}

function updateCounts() {
  document.getElementById('cnt-critical').textContent = alerts.filter(a => a.severity === 'critical').length;
  document.getElementById('cnt-high').textContent     = alerts.filter(a => a.severity === 'high').length;
  document.getElementById('cnt-deauth').textContent   = alerts.filter(a => (a.type||'').toLowerCase().includes('deauth')).length;
  document.getElementById('cnt-pmkid').textContent    = alerts.filter(a => (a.type||'').toLowerCase().includes('pmkid')).length;
}

function clearAlerts() {
  alerts = [];
  iocs = {};
  channelThreats = new Array(14).fill(0);
  renderAll();
}

function renderAll() {
  renderTimeline();
  renderHeatmap();
  renderTopAttackers();
  renderIoc();
  updateCounts();
}

async function fetchAlerts() {
  try {
    const r = await fetch('/api/ti/events?limit=50');
    const data = await r.json();
    const events = data.events || [];
    alerts = [];
    channelThreats = new Array(14).fill(0);
    iocs = {};
    events.forEach(addAlert);
    renderAll();
  } catch(e) {}

  try {
    const r2 = await fetch('/api/ddt/status');
    const d = await r2.json();
    const storms = d.storms || d.alerts || [];
    storms.forEach(s => {
      addAlert({
        severity: s.count > 100 ? 'critical' : s.count > 30 ? 'high' : 'medium',
        type: 'Deauth Storm',
        mac: s.attacker || s.mac,
        bssid: s.target,
        channel: s.channel,
        ts: s.last_seen || Math.floor(Date.now()/1000)
      });
    });
  } catch(e) {}

  try {
    const r3 = await fetch('/api/pmk/entries');
    const d3 = await r3.json();
    const entries = d3.entries || [];
    entries.forEach(e => {
      addAlert({
        severity: 'high',
        type: e.has_handshake ? 'Full Handshake Captured' : 'PMKID Captured',
        bssid: e.bssid,
        ssid: e.ssid,
        channel: 0,
        ts: e.last_seen || Math.floor(Date.now()/1000)
      });
    });
  } catch(e) {}

  renderAll();
}

function exportIoc() {
  const data = JSON.stringify(Object.values(iocs), null, 2);
  const blob = new Blob([data], {type:'application/json'});
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url; a.download = 'phantom32-iocs.json'; a.click();
}

fetchAlerts();
setInterval(fetchAlerts, 10000);
</script>
</body>
</html>
)rawhtml";

inline void wut_routes() {
  server.on("/threats", HTTP_GET, []() {
    server.send_P(200, "text/html", WUT_PAGE);
  });
}

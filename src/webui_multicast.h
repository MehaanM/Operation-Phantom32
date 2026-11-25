#pragma once
// webui_multicast.h — Multicast / broadcast frame tracker UI.
// Route: /multicast. Prefix: wumct_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUMCT_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Multicast — Phantom32</title>
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
.type-pill{padding:2px 6px;border-radius:4px;font-size:9px;font-weight:700;}
.tp-mDNS{background:rgba(0,212,255,.12);color:var(--accent);}
.tp-SSDP{background:rgba(245,158,11,.12);color:var(--warn);}
.tp-NTP{background:rgba(34,197,94,.12);color:var(--success);}
.tp-Broadcast{background:rgba(239,68,68,.12);color:var(--danger);}
.tp-ARP{background:rgba(167,139,250,.12);color:#a78bfa;}
.tp-IGMP{background:rgba(100,116,139,.1);color:var(--muted);}
.tp-IPv6{background:rgba(99,102,241,.12);color:#818cf8;}
.tp-EAPOL{background:rgba(239,68,68,.2);color:var(--danger);}
.storm-badge{display:inline-flex;align-items:center;gap:4px;padding:2px 7px;border-radius:4px;
  background:rgba(239,68,68,.15);color:var(--danger);font-size:8px;font-weight:700;}
table{width:100%;border-collapse:collapse;font-size:10px;}
th{text-align:left;padding:8px 10px;border-bottom:2px solid var(--border);color:var(--muted);font-weight:600;font-size:9px;text-transform:uppercase;cursor:pointer;}
td{padding:7px 10px;border-bottom:1px solid var(--border);}
tr:hover td{background:var(--bg3);}
.mini-chart{display:flex;align-items:flex-end;gap:1px;height:14px;}
.mini-bar{flex:1;border-radius:1px 1px 0 0;min-height:1px;}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;margin-top:12px;}
@media(max-width:700px){.hero{grid-template-columns:1fr 1fr};table{font-size:9px}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/multicast" class="active">Multicast</a>
    <a href="/ssid-pool">SSID Pool</a>
    <a href="/channel-util">Channel Util</a>
  </nav>
</div>
<div class="container">
  <h1>Multicast &amp; Broadcast Tracker</h1>
  <p class="subtitle">Monitors 802.11 multicast group traffic including mDNS, SSDP/UPnP, NTP, ARP, IGMP, and IPv6 multicast. Detects broadcast storms when any group exceeds 100 frames/sec.</p>

  <div class="hero">
    <div class="stat"><label>Broadcast Frames</label><div class="val" id="h-bc">0</div></div>
    <div class="stat"><label>Multicast Frames</label><div class="val" id="h-mc">0</div></div>
    <div class="stat"><label>Active Groups</label><div class="val" id="h-grp" style="color:var(--success)">0</div></div>
    <div class="stat"><label>Storms Detected</label><div class="val" id="h-st" style="color:var(--danger)">0</div></div>
  </div>

  <div class="section">
    <h3>ARP Stats</h3>
    <div style="display:flex;gap:24px;font-size:11px;color:var(--muted)">
      <span>Requests: <strong id="arp-req" style="color:var(--warn)">0</strong></span>
      <span>Replies: <strong id="arp-rep" style="color:var(--success)">0</strong></span>
      <span>Broadcast rate: <strong id="bc-rate" style="color:var(--accent)">0</strong>/s</span>
      <span>Multicast rate: <strong id="mc-rate" style="color:var(--accent)">0</strong>/s</span>
    </div>
  </div>

  <div class="section">
    <h3>Multicast / Broadcast Groups</h3>
    <div style="overflow-x:auto">
    <table>
      <thead><tr>
        <th onclick="sortBy('addr')">Address</th>
        <th onclick="sortBy('type')">Type</th>
        <th onclick="sortBy('total')">Frames</th>
        <th onclick="sortBy('bytes')">Bytes</th>
        <th onclick="sortBy('rate')">Rate/s</th>
        <th onclick="sortBy('peak')">Peak</th>
        <th onclick="sortBy('sources')">Sources</th>
        <th>Storm</th>
        <th>Rate History</th>
      </tr></thead>
      <tbody id="tbl-body"></tbody>
    </table>
    </div>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>
</div>

<script>
let g_data = [];
let g_sort = {key:'total', asc:false};

function sortBy(k) {
  if (g_sort.key === k) g_sort.asc = !g_sort.asc;
  else { g_sort.key = k; g_sort.asc = false; }
  renderTable();
}

function typeClass(t) {
  const cls = {
    'mDNS':'tp-mDNS','SSDP/UPnP':'tp-SSDP','NTP':'tp-NTP',
    'Broadcast':'tp-Broadcast','ARP':'tp-ARP','IGMP':'tp-IGMP',
    'IPv6 MC':'tp-IPv6','EAPOL':'tp-EAPOL'
  };
  return cls[t] || 'tp-IGMP';
}

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

function humanBytes(b) {
  if (b > 1048576) return (b/1048576).toFixed(1) + ' MB';
  if (b > 1024)    return (b/1024).toFixed(1) + ' KB';
  return b + ' B';
}

function renderTable() {
  const rows = [...g_data];
  const k = g_sort.key, asc = g_sort.asc;
  rows.sort((a,b) => {
    const va = typeof a[k] === 'string' ? a[k] : Number(a[k]||0);
    const vb = typeof b[k] === 'string' ? b[k] : Number(b[k]||0);
    return asc ? (va > vb ? 1 : -1) : (vb > va ? 1 : -1);
  });

  document.getElementById('tbl-body').innerHTML = rows.length
    ? rows.map(g => `<tr>
        <td style="font-family:monospace;font-size:9px">${g.addr}</td>
        <td><span class="type-pill ${typeClass(g.type)}">${g.type}</span></td>
        <td>${g.total.toLocaleString()}</td>
        <td style="color:var(--muted)">${humanBytes(g.bytes)}</td>
        <td style="color:${g.rate>50?'var(--danger)':g.rate>10?'var(--warn)':'var(--text)'}">${g.rate}/s</td>
        <td style="color:var(--muted)">${g.peak}/s</td>
        <td>${g.sources}</td>
        <td>${g.storm ? `<span class="storm-badge">STORM×${g.storm_events}</span>` : '—'}</td>
        <td><div class="mini-chart">${miniChart(g.rate_hist)}</div></td>
      </tr>`).join('')
    : `<tr><td colspan="9" style="text-align:center;color:var(--muted);padding:20px">No multicast data yet.</td></tr>`;
}

async function refresh() {
  const r = await fetch('/api/mct/all').then(x => x.json()).catch(()=>({}));
  const groups = r.groups || [];
  document.getElementById('h-bc').textContent   = (r.total_broadcast||0).toLocaleString();
  document.getElementById('h-mc').textContent   = (r.total_multicast||0).toLocaleString();
  document.getElementById('h-grp').textContent  = r.count || 0;
  document.getElementById('h-st').textContent   = groups.filter(g => g.storm_events > 0).length;
  document.getElementById('arp-req').textContent = r.arp_req || 0;
  document.getElementById('arp-rep').textContent = r.arp_rep || 0;
  document.getElementById('bc-rate').textContent = r.broadcast_rate || 0;
  document.getElementById('mc-rate').textContent = r.multicast_rate || 0;
  g_data = groups;
  renderTable();
}

async function clearData() {
  await fetch('/api/mct/clear', {method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 2000);
</script>
</body>
</html>
)rawhtml";

inline void wumct_routes() {
  server.on("/multicast", HTTP_GET, []() { server.send_P(200, "text/html", WUMCT_PAGE); });
}

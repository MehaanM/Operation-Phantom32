#pragma once
// webui_traffic.h — Traffic classifier UI.
// Route: /traffic. Prefix: wutfc_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUTFC_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Traffic Classifier — Phantom32</title>
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
.two-col{display:grid;grid-template-columns:1fr 1fr;gap:14px;margin-bottom:14px;}
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:18px;margin-bottom:14px;}
.section h3{font-size:12px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:14px;}
.breakdown-row{display:flex;align-items:center;gap:8px;padding:5px 0;border-bottom:1px solid var(--border);}
.breakdown-row:last-child{border-bottom:none;}
.breakdown-label{flex:1;font-size:11px;}
.breakdown-cnt{font-size:11px;font-weight:600;color:var(--accent);min-width:60px;text-align:right;}
.breakdown-bar{flex:2;height:5px;background:var(--border);border-radius:3px;}
.breakdown-fill{height:100%;border-radius:3px;}
canvas{width:100%;border-radius:5px;}
.proto-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(120px,1fr));gap:8px;margin-top:4px;}
.proto-card{background:var(--bg3);border:1px solid var(--border);border-radius:6px;padding:10px;}
.proto-card .pname{font-size:10px;color:var(--muted);font-weight:600;text-transform:uppercase;}
.proto-card .pcnt{font-size:18px;font-weight:800;margin-top:3px;}
table{width:100%;border-collapse:collapse;font-size:11px;}
th{text-align:left;padding:7px 8px;font-size:9px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;}
td{padding:6px 8px;border-bottom:1px solid var(--border);}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
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
    <a href="/frame-inject-log">Frame Log</a>
    <a href="/traffic" class="active">Traffic</a>
    <a href="/multicast">Multicast</a>
  </nav>
</div>
<div class="container">
  <h1>Traffic Classifier</h1>
  <p class="subtitle">Classifies observed 802.11 frames by type, subtype, and application protocol. Builds a real-time breakdown of management, control, and data frames.</p>

  <div class="hero">
    <div class="stat"><label>Total Frames</label><div class="val" id="h-pkts">0</div></div>
    <div class="stat"><label>Total Bytes</label><div class="val" id="h-bytes" style="font-size:14px">0</div></div>
    <div class="stat"><label>Mgmt Frames</label><div class="val" id="h-mgmt">0</div></div>
    <div class="stat"><label>Data Frames</label><div class="val" id="h-data">0</div></div>
  </div>

  <div class="two-col">
    <div class="section">
      <h3>Frame Type Breakdown</h3>
      <canvas id="type-pie" height="160"></canvas>
    </div>
    <div class="section">
      <h3>Protocol Distribution</h3>
      <div id="proto-grid" class="proto-grid"></div>
    </div>
  </div>

  <div class="two-col">
    <div class="section">
      <h3>Management Subtypes</h3>
      <div id="mgmt-break"></div>
    </div>
    <div class="section">
      <h3>Data Subtypes</h3>
      <div id="data-break"></div>
    </div>
  </div>

  <div class="section">
    <h3>Flow Table</h3>
    <table>
      <thead>
        <tr><th>Src MAC</th><th>Dst MAC</th><th>Proto</th><th>CH</th><th>RSSI</th><th>Packets</th><th>Bytes</th></tr>
      </thead>
      <tbody id="flow-tbody">
        <tr><td colspan="7" style="color:var(--muted);text-align:center">No flows.</td></tr>
      </tbody>
    </table>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>
</div>

<script>
const PIE_COLORS = ['#00d4ff','#22c55e','#f59e0b','#ef4444','#a855f7','#f97316'];
const PROTO_COLORS = {
  'DNS':'#00d4ff','DHCP':'#a855f7','ARP':'#22c55e','HTTP':'#f59e0b',
  'HTTPS':'#4b9eff','mDNS':'#06b6d4','NTP':'#84cc16','IGMP':'#64748b',
  'EAPOL':'#ef4444','Multicast':'#f97316','Broadcast':'#ec4899',
  'Data':'#64748b','Mgmt':'#334155','Ctrl':'#1e293b','QoS':'#0ea5e9'
};

function drawPie(canvas, slices) {
  const ctx = canvas.getContext('2d');
  const W = canvas.width = canvas.offsetWidth;
  const H = canvas.height = 160;
  ctx.clearRect(0,0,W,H);
  const total = slices.reduce((s,v) => s + v.val, 0) || 1;
  const cx = W/2, cy = H/2 - 10, r = Math.min(cx, cy) - 10;
  let angle = -Math.PI/2;
  slices.forEach((s, i) => {
    const sweep = (s.val / total) * 2 * Math.PI;
    ctx.beginPath();
    ctx.moveTo(cx, cy);
    ctx.arc(cx, cy, r, angle, angle + sweep);
    ctx.closePath();
    ctx.fillStyle = PIE_COLORS[i % PIE_COLORS.length];
    ctx.fill();
    angle += sweep;
  });
  // Legend
  ctx.font = '10px system-ui';
  slices.forEach((s, i) => {
    const pct = (s.val / total * 100).toFixed(1);
    ctx.fillStyle = PIE_COLORS[i % PIE_COLORS.length];
    ctx.fillRect(4, H - 24 + i * 0 + (i * 14) - slices.length * 7 + H/2 + 10, 8, 8);
  });
}

function breakdownRows(pairs, maxVal) {
  if (!maxVal) return '<div style="color:var(--muted);font-size:11px">No data.</div>';
  return pairs.map(([label, val]) => {
    const pct = (val / maxVal * 100).toFixed(0);
    const color = val === maxVal ? 'var(--accent)' : 'var(--muted)';
    return `<div class="breakdown-row">
      <span class="breakdown-label">${label}</span>
      <span class="breakdown-cnt" style="color:${color}">${val.toLocaleString()}</span>
      <div class="breakdown-bar"><div class="breakdown-fill" style="width:${pct}%;background:${color}"></div></div>
    </div>`;
  }).join('');
}

function formatBytes(b) {
  if (b > 1048576) return (b/1048576).toFixed(1) + 'MB';
  if (b > 1024)    return (b/1024).toFixed(1) + 'KB';
  return b + 'B';
}

function rssiColor(r) {
  return r >= -60 ? 'var(--success)' : r >= -75 ? 'var(--warn)' : 'var(--danger)';
}

async function refresh() {
  const r = await fetch('/api/tfc/all').then(x => x.json()).catch(() => ({}));
  document.getElementById('h-pkts').textContent  = (r.total_packets||0).toLocaleString();
  document.getElementById('h-bytes').textContent = formatBytes(r.total_bytes||0);
  document.getElementById('h-mgmt').textContent  = (r.mgmt_total||0).toLocaleString();
  document.getElementById('h-data').textContent  = (r.data_total||0).toLocaleString();

  // Pie
  const pie = document.getElementById('type-pie');
  const slices = [
    {label:'Mgmt', val: r.mgmt_total||0},
    {label:'Data', val: r.data_total||0},
    {label:'Ctrl', val: r.ctrl_total||0}
  ];
  drawPie(pie, slices);

  // Proto grid
  const protos = r.protos || [];
  document.getElementById('proto-grid').innerHTML = protos.map(p => {
    const color = PROTO_COLORS[p.name] || 'var(--accent)';
    return `<div class="proto-card">
      <div class="pname">${p.name}</div>
      <div class="pcnt" style="color:${color}">${p.packets.toLocaleString()}</div>
      <div style="font-size:9px;color:var(--muted)">${formatBytes(p.bytes)}</div>
    </div>`;
  }).join('') || '<div style="color:var(--muted)">No protocol data.</div>';

  // Mgmt breakdown
  const mgmtPairs = [
    ['Beacon',    r.mgmt_beacon||0],
    ['Probe Req', r.mgmt_probe_req||0],
    ['Probe Rsp', r.mgmt_probe_rsp||0],
    ['Auth',      r.mgmt_auth||0],
    ['Assoc',     r.mgmt_assoc||0],
    ['Deauth',    r.mgmt_deauth||0],
    ['Action',    r.mgmt_action||0],
  ].filter(p => p[1] > 0);
  const mgmtMax = Math.max(...mgmtPairs.map(p => p[1]), 1);
  document.getElementById('mgmt-break').innerHTML = breakdownRows(mgmtPairs, mgmtMax);

  // Data breakdown
  const dataPairs = [
    ['Unicast',   r.data_unicast||0],
    ['QoS',       r.data_qos||0],
    ['Multicast', r.data_multicast||0],
    ['Broadcast', r.data_broadcast||0],
    ['DNS',       r.data_dns||0],
    ['DHCP',      r.data_dhcp||0],
    ['ARP',       r.data_arp||0],
    ['EAPOL',     r.data_eapol||0],
    ['mDNS',      r.data_mdns||0],
    ['NTP',       r.data_ntp||0],
  ].filter(p => p[1] > 0);
  const dataMax = Math.max(...dataPairs.map(p => p[1]), 1);
  document.getElementById('data-break').innerHTML = breakdownRows(dataPairs, dataMax);

  // Flow table
  const flows = (r.flows || []).sort((a,b) => b.packets - a.packets).slice(0, 32);
  document.getElementById('flow-tbody').innerHTML = flows.length
    ? flows.map(f => `<tr>
        <td><code style="font-size:9px">${f.src}</code></td>
        <td><code style="font-size:9px">${f.dst}</code></td>
        <td><span style="color:${PROTO_COLORS[f.proto]||'var(--accent)'};">${f.proto}</span></td>
        <td>${f.ch}</td>
        <td style="color:${rssiColor(f.rssi)}">${f.rssi}</td>
        <td style="font-weight:600">${f.packets.toLocaleString()}</td>
        <td style="color:var(--muted)">${formatBytes(f.bytes)}</td>
      </tr>`).join('')
    : '<tr><td colspan="7" style="color:var(--muted);text-align:center">No flows.</td></tr>';
}

async function clearData() {
  await fetch('/api/tfc/clear', {method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 2000);
</script>
</body>
</html>
)rawhtml";

inline void wutfc_routes() {
  server.on("/traffic", HTTP_GET, []() {
    server.send_P(200, "text/html", WUTFC_PAGE);
  });
}

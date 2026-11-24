#pragma once
// webui_rate_tracker.h — 802.11 Data Rate Tracker UI.
// Route: /rate-tracker. Prefix: wurtt_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WURTT_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Rate Tracker — Phantom32</title>
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
.container{max-width:1200px;margin:0 auto;padding:20px 16px;}
h1{font-size:22px;font-weight:700;margin-bottom:4px;}
.subtitle{color:var(--muted);font-size:13px;margin-bottom:20px;}
.hero{display:grid;grid-template-columns:repeat(5,1fr);gap:10px;margin-bottom:16px;}
.stat{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:13px;}
.stat label{font-size:10px;color:var(--muted);text-transform:uppercase;}
.stat .val{font-size:22px;font-weight:800;color:var(--accent);margin-top:3px;}
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:18px;margin-bottom:14px;}
.section h3{font-size:12px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:14px;}
table{width:100%;border-collapse:collapse;font-size:10px;}
th{text-align:left;padding:8px 10px;border-bottom:2px solid var(--border);color:var(--muted);font-weight:600;font-size:9px;text-transform:uppercase;}
td{padding:7px 10px;border-bottom:1px solid var(--border);}
tr:hover td{background:var(--bg3);}
.badge{padding:1px 5px;border-radius:3px;font-size:8px;font-weight:700;}
.b-ht{background:rgba(0,212,255,.1);color:var(--accent);}
.b-vht{background:rgba(34,197,94,.1);color:var(--success);}
.b-he{background:rgba(245,158,11,.1);color:var(--warn);}
.mini-hist{display:flex;align-items:flex-end;gap:1px;height:20px;}
.mh-bar{width:4px;min-height:1px;border-radius:1px 1px 0 0;}
.rate-bar{display:inline-block;height:5px;border-radius:2px;vertical-align:middle;}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;margin-top:12px;}
@media(max-width:800px){.hero{grid-template-columns:repeat(3,1fr)}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/rate-tracker" class="active">Rate Tracker</a>
    <a href="/channel-analyzer">Channels</a>
    <a href="/signal-heatmap">Signal Map</a>
  </nav>
</div>
<div class="container">
  <h1>802.11 Rate Tracker</h1>
  <p class="subtitle">Passively monitors 802.11n/ac/ax MCS indices, spatial streams, channel widths, and computed data rates from observed frames. Identify AP capabilities and peak throughput in the air.</p>

  <div class="hero">
    <div class="stat"><label>APs Tracked</label><div class="val" id="h-aps">0</div></div>
    <div class="stat"><label>Total Samples</label><div class="val" id="h-total">0</div></div>
    <div class="stat"><label>Peak Rate</label><div class="val" id="h-peak" style="color:var(--success)">0</div><div style="font-size:9px;color:var(--muted)">Mbps</div></div>
    <div class="stat"><label>HE (ax) Frames</label><div class="val" id="h-he" style="color:var(--warn)">0</div></div>
    <div class="stat"><label>VHT (ac) Frames</label><div class="val" id="h-vht" style="color:var(--accent)">0</div></div>
  </div>

  <div class="section">
    <h3>AP Rate Table</h3>
    <div style="overflow-x:auto">
    <table>
      <thead><tr>
        <th>SSID</th><th>CH</th><th>Samples</th>
        <th>Avg Rate</th><th>Peak Rate</th><th>MCS</th><th>NSS</th><th>BW</th>
        <th>PHY</th><th>BW Distribution</th><th>Rate History</th>
      </tr></thead>
      <tbody id="ap-body"></tbody>
    </table>
    </div>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>
</div>

<script>
function fmt(n) { return Number(n).toLocaleString(); }

function rateColor(r) {
  if (r >= 400000) return 'var(--warn)';
  if (r >= 150000) return 'var(--success)';
  if (r >= 50000)  return 'var(--accent)';
  return 'var(--muted)';
}

function rateMbps(r) { return (r/1000).toFixed(1) + ' Mbps'; }

function miniHist(hist) {
  if (!hist || !hist.length) return '';
  const max = Math.max(...hist, 1);
  return `<div class="mini-hist">` +
    hist.map(v => `<div class="mh-bar" style="height:${Math.max(1,(v/max)*20)}px;background:${rateColor(v)}"></div>`).join('') +
    `</div>`;
}

function bwBar(bw20, bw40, bw80, bw160) {
  const total = bw20 + bw40 + bw80 + bw160 || 1;
  const pct20 = bw20/total*100, pct40 = bw40/total*100,
        pct80 = bw80/total*100, pct160 = bw160/total*100;
  return `<div style="display:flex;height:5px;border-radius:2px;overflow:hidden;min-width:60px">
    <div style="width:${pct20}%;background:var(--muted)" title="20MHz: ${bw20}"></div>
    <div style="width:${pct40}%;background:var(--accent)" title="40MHz: ${bw40}"></div>
    <div style="width:${pct80}%;background:var(--success)" title="80MHz: ${bw80}"></div>
    <div style="width:${pct160}%;background:var(--warn)" title="160MHz: ${bw160}"></div>
  </div>`;
}

async function refresh() {
  const r = await fetch('/api/rtt/all').then(x=>x.json()).catch(()=>({}));
  const aps = r.aps || [];

  document.getElementById('h-aps').textContent   = r.ap_count || 0;
  document.getElementById('h-total').textContent = fmt(r.total || 0);
  document.getElementById('h-peak').textContent  = r.peak_kbps ? (r.peak_kbps/1000).toFixed(0) : '0';
  document.getElementById('h-he').textContent    = fmt(r.he_frames || 0);
  document.getElementById('h-vht').textContent   = fmt(r.vht_frames || 0);

  document.getElementById('ap-body').innerHTML = aps.length
    ? aps.map(a => {
        const phyBadge = a.he  ? `<span class="badge b-he">ax</span> ` :
                         a.vht ? `<span class="badge b-vht">ac</span> ` :
                         a.ht  ? `<span class="badge b-ht">n</span> ` : '—';
        return `<tr>
          <td style="max-width:120px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap" title="${a.bssid}">${a.ssid||'—'}</td>
          <td>CH${a.ch}</td>
          <td>${fmt(a.samples)}</td>
          <td style="color:${rateColor(a.avg_kbps)}">${rateMbps(a.avg_kbps)}</td>
          <td style="color:${rateColor(a.peak_kbps)};font-weight:700">${rateMbps(a.peak_kbps)}</td>
          <td>${a.peak_mcs}</td>
          <td>${a.peak_nss}×${a.peak_bw}MHz</td>
          <td>${a.peak_bw}</td>
          <td>${phyBadge} <span style="font-size:9px;color:var(--muted)">${a.peak_phy||''}</span></td>
          <td>${bwBar(a.bw20,a.bw40,a.bw80,a.bw160)}</td>
          <td>${miniHist(a.hist)}</td>
        </tr>`;
      }).join('')
    : '<tr><td colspan="11" style="text-align:center;color:var(--muted);padding:16px">No rate data yet — passive 802.11n/ac/ax frames required.</td></tr>';
}

async function clearData() {
  await fetch('/api/rtt/clear', {method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 2000);
</script>
</body>
</html>
)rawhtml";

inline void wurtt_routes() {
  server.on("/rate-tracker", HTTP_GET, []() {
    server.send_P(200, "text/html", WURTT_PAGE);
  });
}

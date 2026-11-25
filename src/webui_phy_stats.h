#pragma once
// webui_phy_stats.h — PHY statistics UI.
// Route: /phy-stats. Prefix: wuphy_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUPHY_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>PHY Stats — Phantom32</title>
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
.bar-row{display:flex;align-items:center;gap:8px;margin-bottom:5px;}
.bar-label{font-size:10px;width:70px;flex-shrink:0;}
.bar-track{flex:1;height:8px;background:var(--border);border-radius:4px;}
.bar-fill{height:100%;border-radius:4px;}
.bar-cnt{font-size:9px;color:var(--muted);min-width:40px;text-align:right;}
.ch-cards{display:grid;grid-template-columns:repeat(auto-fill,minmax(220px,1fr));gap:8px;}
.ch-card{background:var(--bg3);border:1px solid var(--border);border-radius:7px;padding:12px;}
.ch-title{font-size:13px;font-weight:700;color:var(--accent);margin-bottom:6px;}
.ch-metric{display:flex;justify-content:space-between;font-size:10px;padding:2px 0;}
.ch-metric span{color:var(--text);font-weight:600;}
.mini-hist{display:flex;align-items:flex-end;gap:1px;height:16px;margin-top:4px;}
.mini-bar{flex:1;border-radius:1px 1px 0 0;background:var(--accent);min-width:2px;}
.gen-badges{display:flex;gap:4px;flex-wrap:wrap;margin-top:6px;}
.gen-badge{padding:2px 6px;border-radius:3px;font-size:8px;font-weight:600;}
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
    <a href="/phy-stats" class="active">PHY Stats</a>
    <a href="/frame-aggr">Aggregation</a>
    <a href="/spectrum">Spectrum</a>
  </nav>
</div>
<div class="container">
  <h1>PHY Layer Statistics</h1>
  <p class="subtitle">Monitors 802.11 PHY layer: MCS index usage, channel bandwidth, spatial streams, guard intervals, and retry rates. Shows Wi-Fi generation breakdown per channel.</p>

  <div class="hero">
    <div class="stat"><label>Active Channels</label><div class="val" id="h-ch">0</div></div>
    <div class="stat"><label>Total Frames</label><div class="val" id="h-fr" style="font-size:14px">0</div></div>
    <div class="stat"><label>Retry Rate</label><div class="val" id="h-rtr" style="color:var(--warn)">—</div></div>
    <div class="stat"><label>Dominant Gen</label><div class="val" id="h-gen" style="font-size:14px">—</div></div>
  </div>

  <div class="two-col">
    <div class="section">
      <h3>MCS Index Distribution (Global)</h3>
      <div id="mcs-bars"></div>
    </div>
    <div class="section">
      <h3>Bandwidth Usage</h3>
      <div id="bw-bars"></div>
    </div>
  </div>

  <div class="two-col">
    <div class="section">
      <h3>Spatial Streams</h3>
      <div id="ss-bars"></div>
    </div>
    <div class="section">
      <h3>Wi-Fi Generation</h3>
      <div id="gen-bars"></div>
    </div>
  </div>

  <div class="section">
    <h3>Per-Channel Stats</h3>
    <div class="ch-cards" id="ch-cards">
      <div style="color:var(--muted)">No channel data.</div>
    </div>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>
</div>

<script>
const MCS_COLORS  = ['#64748b','#4b9eff','#00d4ff','#22c55e','#84cc16','#f59e0b','#f97316','#ef4444','#c026d3','#a855f7'];
const BW_LABELS   = ['20MHz','40MHz','80MHz','160MHz','80+80'];
const BW_COLORS   = ['#64748b','#4b9eff','#00d4ff','#22c55e','#f59e0b'];
const GEN_LABELS  = ['Legacy','HT (n)','VHT (ac)','HE (ax)'];
const GEN_COLORS  = ['#64748b','#4b9eff','#00d4ff','#22c55e'];
const SS_COLORS   = ['#334155','#4b9eff','#00d4ff','#22c55e','#84cc16','#f59e0b','#f97316','#ef4444'];

function barRows(values, labels, colors, total) {
  if (!values || !values.length) return '<div style="color:var(--muted)">No data.</div>';
  const max = Math.max(...values, 1);
  return values.map((v, i) => {
    const pct = (v / max * 100).toFixed(0);
    const pct2 = total ? (v / total * 100).toFixed(1) + '%' : v.toLocaleString();
    return `<div class="bar-row">
      <div class="bar-label">${labels ? labels[i] : 'MCS'+i}</div>
      <div class="bar-track"><div class="bar-fill" style="width:${pct}%;background:${colors[i%colors.length]}"></div></div>
      <div class="bar-cnt">${pct2}</div>
    </div>`;
  }).join('');
}

function formatNum(n) {
  if (n > 1e6) return (n/1e6).toFixed(1) + 'M';
  if (n > 1e3) return (n/1e3).toFixed(1) + 'K';
  return String(n);
}

function miniHist(hist) {
  if (!hist || !hist.length) return '';
  const max = Math.max(...hist, 1);
  return '<div class="mini-hist">' + hist.map(v => {
    const h = Math.round((v/max)*14)+2;
    return `<div class="mini-bar" style="height:${h}px"></div>`;
  }).join('') + '</div>';
}

function genBadges(gen, genLabels, genColors) {
  if (!gen) return '';
  const total = gen.reduce((s,v) => s+v, 0) || 1;
  return gen.map((v, i) => {
    if (v === 0) return '';
    const pct = (v/total*100).toFixed(0);
    return `<span class="gen-badge" style="background:${genColors[i]}22;color:${genColors[i]}">${genLabels[i]}: ${pct}%</span>`;
  }).filter(Boolean).join('');
}

async function refresh() {
  const r = await fetch('/api/phy/all').then(x => x.json()).catch(() => ({}));
  document.getElementById('h-ch').textContent  = r.active_channels || 0;
  document.getElementById('h-fr').textContent  = formatNum(r.total_frames||0);
  const retryPct = r.total_frames > 0 ? ((r.total_retries||0)/r.total_frames*100).toFixed(1) + '%' : '—';
  document.getElementById('h-rtr').textContent = retryPct;

  // Dominant gen
  const genTotal = r.gen_total || [0,0,0,0];
  const domGen = genTotal.indexOf(Math.max(...genTotal));
  document.getElementById('h-gen').textContent = domGen >= 0 ? GEN_LABELS[domGen] : '—';

  // Global distributions
  const mcsLabels = Array.from({length:10}, (_,i) => 'MCS '+i);
  const totalFrames = r.total_frames || 1;
  document.getElementById('mcs-bars').innerHTML = barRows(r.mcs_total||[], mcsLabels, MCS_COLORS, totalFrames);
  document.getElementById('bw-bars').innerHTML  = barRows(r.bw_total||[], BW_LABELS, BW_COLORS, totalFrames);
  const ssLabels = Array.from({length:8}, (_,i) => (i+1)+' Stream'+(i>0?'s':''));
  document.getElementById('ss-bars').innerHTML  = barRows(r.ss_total||[], ssLabels, SS_COLORS, totalFrames);
  document.getElementById('gen-bars').innerHTML = barRows(genTotal, GEN_LABELS, GEN_COLORS, totalFrames);

  // Per-channel cards
  const channels = r.channels || [];
  document.getElementById('ch-cards').innerHTML = channels.length
    ? channels.map(c => `<div class="ch-card">
        <div class="ch-title">Channel ${c.ch}</div>
        <div class="ch-metric">Frames <span>${(c.frames||0).toLocaleString()}</span></div>
        <div class="ch-metric">Retries <span style="color:${c.retry_pct>20?'var(--danger)':c.retry_pct>5?'var(--warn)':'var(--success)'}">${c.retry_pct||0}%</span></div>
        <div class="ch-metric">Avg MCS <span>${c.avg_mcs||0}</span></div>
        <div class="gen-badges">${genBadges(c.gen, GEN_LABELS, GEN_COLORS)}</div>
        ${miniHist(c.pkt_hist)}
      </div>`).join('')
    : '<div style="color:var(--muted)">No channel data.</div>';
}

async function clearData() {
  await fetch('/api/phy/clear', {method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 3000);
</script>
</body>
</html>
)rawhtml";

inline void wuphy_routes() {
  server.on("/phy-stats", HTTP_GET, []() { server.send_P(200, "text/html", WUPHY_PAGE); });
}

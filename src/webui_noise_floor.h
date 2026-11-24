#pragma once
// webui_noise_floor.h — Noise floor estimation UI.
// Route: /noise-floor. Prefix: wunfl_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUNFL_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Noise Floor — Phantom32</title>
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
.ch-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(220px,1fr));gap:10px;}
.ch-card{background:var(--bg3);border:1px solid var(--border);border-radius:7px;padding:12px;}
.ch-card.busy{border-left:3px solid var(--danger);}
.ch-card.mid{border-left:3px solid var(--warn);}
.ch-card.clear{border-left:3px solid var(--success);}
.ch-num{font-size:22px;font-weight:800;color:var(--accent);}
.mini-hist{display:flex;align-items:flex-end;gap:1px;height:24px;margin-top:8px;}
.mini-bar{flex:1;border-radius:1px 1px 0 0;}
.rssi-gauge{position:relative;height:8px;border-radius:4px;background:var(--border);margin:6px 0 2px;}
.rssi-track{position:absolute;height:100%;border-radius:4px;}
.gauge-labels{display:flex;justify-content:space-between;font-size:9px;color:var(--muted);}
.stat-row{display:flex;justify-content:space-between;font-size:11px;color:var(--muted);margin-top:4px;}
table{width:100%;border-collapse:collapse;font-size:12px;}
th{text-align:left;padding:7px 8px;font-size:10px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;cursor:pointer;}
th:hover{color:var(--text);}
td{padding:7px 8px;border-bottom:1px solid var(--border);}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
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
    <a href="/channel-util">CH Util</a>
    <a href="/noise-floor" class="active">Noise Floor</a>
    <a href="/phy-stats">PHY Stats</a>
  </nav>
</div>
<div class="container">
  <h1>Noise Floor Estimator</h1>
  <p class="subtitle">Per-channel RSSI distribution analysis. The 10th-percentile RSSI approximates the channel noise floor. High congestion % indicates heavy traffic or interference.</p>

  <div class="hero">
    <div class="stat"><label>Channels</label><div class="val" id="h-cnt">0</div></div>
    <div class="stat"><label>Best Channel</label><div class="val" id="h-best" style="font-size:16px">—</div></div>
    <div class="stat"><label>Busy (&gt;60%)</label><div class="val" id="h-busy" style="color:var(--danger)">0</div></div>
    <div class="stat"><label>Avg Noise Floor</label><div class="val" id="h-noise" style="font-size:16px">—</div></div>
  </div>

  <div class="section">
    <h3>Channel Cards</h3>
    <div class="ch-grid" id="ch-grid">
      <div style="color:var(--muted);text-align:center;grid-column:1/-1;padding:20px">Waiting for RF data…</div>
    </div>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>

  <div class="section">
    <h3>Channel Comparison Table</h3>
    <table>
      <thead>
        <tr>
          <th onclick="sort('ch')">CH</th>
          <th onclick="sort('noise_floor')">Noise Floor</th>
          <th onclick="sort('median_rssi')">Median RSSI</th>
          <th onclick="sort('peak_rssi')">Peak RSSI</th>
          <th onclick="sort('congestion_pct')">Congestion %</th>
          <th onclick="sort('total_samples')">Samples</th>
          <th>Mgmt / Ctrl / Data</th>
        </tr>
      </thead>
      <tbody id="ch-tbody"></tbody>
    </table>
  </div>
</div>

<script>
let g_chs = [];
let g_sort = {key:'congestion_pct', asc:false};

const RSSI_LABELS = ['-100','-95','-90','-85','-80','-75','-70','-65','-60','-55'];

function congestionClass(pct) {
  if (pct >= 60) return 'busy';
  if (pct >= 30) return 'mid';
  return 'clear';
}

function congestionColor(pct) {
  if (pct >= 60) return 'var(--danger)';
  if (pct >= 30) return 'var(--warn)';
  return 'var(--success)';
}

function miniHist(hist) {
  if (!hist || !hist.length) return '';
  const max = Math.max(...hist, 1);
  return hist.map(v => {
    const h = Math.round((v / max) * 22) + 2;
    const color = v > max * 0.7 ? 'var(--danger)' : v > max * 0.4 ? 'var(--warn)' : 'var(--accent)';
    return `<div class="mini-bar" style="height:${h}px;background:${color}"></div>`;
  }).join('');
}

function rssiGauge(noise, med, peak) {
  // Map -100 to -50 → 0 to 100%
  const pct = v => Math.max(0, Math.min(100, (v + 100) * 2));
  const nP = pct(noise), mP = pct(med), peP = pct(peak);
  return `<div class="rssi-gauge">
    <div class="rssi-track" style="width:${peP}%;background:rgba(0,212,255,.2)"></div>
    <div class="rssi-track" style="width:${mP}%;background:rgba(0,212,255,.4)"></div>
    <div style="position:absolute;left:${nP}%;top:0;width:3px;height:100%;background:var(--warn);border-radius:2px"></div>
  </div>
  <div class="gauge-labels"><span>-100</span><span>-75</span><span>-50</span></div>`;
}

function renderCards() {
  document.getElementById('ch-grid').innerHTML = g_chs.length
    ? g_chs.map(c => `<div class="ch-card ${congestionClass(c.congestion_pct)}">
        <div style="display:flex;align-items:center;gap:6px">
          <div class="ch-num">${c.ch}</div>
          <div>
            <div style="font-size:9px;color:var(--muted)">CH</div>
            <div style="font-size:11px;color:${congestionColor(c.congestion_pct)}">${c.congestion_pct}% busy</div>
          </div>
        </div>
        ${rssiGauge(c.noise_floor, c.median_rssi, c.peak_rssi)}
        <div class="stat-row">
          <span>Noise: <strong style="color:var(--warn)">${c.noise_floor} dBm</strong></span>
          <span>Median: <strong>${c.median_rssi} dBm</strong></span>
          <span>Peak: <strong style="color:var(--success)">${c.peak_rssi} dBm</strong></span>
        </div>
        <div style="font-size:10px;color:var(--muted);margin-top:6px">
          M:${c.mgmt} C:${c.ctrl} D:${c.data} — ${c.total_samples} samples
        </div>
        <div class="mini-hist">${miniHist(c.sample_hist)}</div>
      </div>`).join('')
    : '<div style="color:var(--muted);text-align:center;grid-column:1/-1;padding:20px">No data.</div>';
}

function sort(key) {
  if (g_sort.key === key) g_sort.asc = !g_sort.asc;
  else { g_sort.key = key; g_sort.asc = false; }
  renderTable();
}

function renderTable() {
  const sorted = [...g_chs].sort((a,b) => {
    const va = a[g_sort.key], vb = b[g_sort.key];
    return g_sort.asc ? va - vb : vb - va;
  });
  document.getElementById('ch-tbody').innerHTML = sorted.length
    ? sorted.map(c => `<tr>
        <td><strong>${c.ch}</strong></td>
        <td style="color:var(--warn)">${c.noise_floor} dBm</td>
        <td>${c.median_rssi} dBm</td>
        <td style="color:var(--success)">${c.peak_rssi} dBm</td>
        <td style="color:${congestionColor(c.congestion_pct)}">${c.congestion_pct}%</td>
        <td>${c.total_samples.toLocaleString()}</td>
        <td style="font-size:10px;color:var(--muted)">${c.mgmt} / ${c.ctrl} / ${c.data}</td>
      </tr>`).join('')
    : '<tr><td colspan="7" style="color:var(--muted);text-align:center">No data.</td></tr>';
}

async function refresh() {
  const r = await fetch('/api/nfl/all').then(x => x.json()).catch(() => ({}));
  g_chs = r.channels || [];
  document.getElementById('h-cnt').textContent = r.count || 0;
  document.getElementById('h-busy').textContent = g_chs.filter(c => c.congestion_pct >= 60).length;
  const best = g_chs.length ? g_chs.reduce((a,b) => a.congestion_pct < b.congestion_pct ? a : b) : null;
  document.getElementById('h-best').textContent = best ? `CH ${best.ch}` : '—';
  const avg = g_chs.length ? Math.round(g_chs.reduce((s,c) => s + c.noise_floor, 0) / g_chs.length) : 0;
  document.getElementById('h-noise').textContent = g_chs.length ? avg + ' dBm' : '—';
  renderCards();
  renderTable();
}

async function clearData() {
  await fetch('/api/nfl/clear', {method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 3000);
</script>
</body>
</html>
)rawhtml";

inline void wunfl_routes() {
  server.on("/noise-floor", HTTP_GET, []() {
    server.send_P(200, "text/html", WUNFL_PAGE);
  });
}

#pragma once
// webui_spatial.h — Spatial reuse / BSS coloring UI.
// Route: /spatial-reuse. Prefix: wusru_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUSRU_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Spatial Reuse (BSS Color) — Phantom32</title>
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
table{width:100%;border-collapse:collapse;font-size:12px;}
th{text-align:left;padding:7px 8px;font-size:10px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;}
td{padding:7px 8px;border-bottom:1px solid var(--border);}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
.badge{padding:2px 7px;border-radius:4px;font-size:10px;font-weight:600;display:inline-block;}
.badge-he{background:rgba(0,212,255,.12);color:var(--accent);}
.badge-legacy{background:rgba(100,116,139,.12);color:var(--muted);}
.badge-collision{background:rgba(239,68,68,.12);color:var(--danger);}
.badge-ok{background:rgba(34,197,94,.12);color:var(--success);}
.badge-disabled{background:rgba(245,158,11,.12);color:var(--warn);}
.color-dot{width:14px;height:14px;border-radius:50%;display:inline-block;vertical-align:middle;margin-right:5px;}
.sr-banner{padding:12px 16px;border-radius:7px;font-size:13px;font-weight:600;margin-bottom:16px;display:flex;align-items:center;gap:8px;}
.sr-active{background:rgba(34,197,94,.12);border:1px solid rgba(34,197,94,.3);color:var(--success);}
.sr-inactive{background:rgba(100,116,139,.1);border:1px solid var(--border);color:var(--muted);}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;margin-top:12px;}
@media(max-width:700px){.hero{grid-template-columns:repeat(2,1fr)}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/fingerprint">Fingerprint</a>
    <a href="/spatial-reuse" class="active">Spatial Reuse</a>
    <a href="/interference">Interference</a>
    <a href="/snr">SNR</a>
  </nav>
</div>
<div class="container">
  <h1>Spatial Reuse (BSS Coloring)</h1>
  <p class="subtitle">802.11ax/Wi-Fi 6 BSS Color analysis. Detects co-channel color collisions that negate spatial reuse benefits.</p>

  <div id="sr-banner" class="sr-banner sr-inactive">
    <span id="sr-icon">●</span>
    <span id="sr-text">Spatial reuse status unknown — no Wi-Fi 6 APs detected.</span>
  </div>

  <div class="hero">
    <div class="stat"><label>Total APs</label><div class="val" id="h-total">0</div></div>
    <div class="stat"><label>Wi-Fi 6 (HE)</label><div class="val" id="h-he" style="color:var(--accent)">0</div></div>
    <div class="stat"><label>Color Collisions</label><div class="val" id="h-coll" style="color:var(--danger)">0</div></div>
    <div class="stat"><label>Colors in Use</label><div class="val" id="h-colors">0</div></div>
  </div>

  <div class="section">
    <h3>AP BSS Color Assignment</h3>
    <table>
      <thead><tr>
        <th>BSSID</th>
        <th>SSID</th>
        <th>Standard</th>
        <th>CH</th>
        <th>RSSI</th>
        <th>BSS Color</th>
        <th>Collision</th>
      </tr></thead>
      <tbody id="ap-tbody">
        <tr><td colspan="7" style="color:var(--muted);text-align:center">Loading…</td></tr>
      </tbody>
    </table>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>

  <div class="section">
    <h3>Color Utilization Map</h3>
    <div id="color-map" style="display:flex;flex-wrap:wrap;gap:8px;"></div>
  </div>
</div>

<script>
const PALETTE = [
  '#00d4ff','#22c55e','#f59e0b','#ef4444','#a855f7','#ec4899',
  '#14b8a6','#f97316','#84cc16','#06b6d4','#8b5cf6','#f43f5e',
  '#10b981','#eab308','#6366f1','#78716c','#0ea5e9','#d946ef',
];

function color_hue(c) {
  return PALETTE[(c-1) % PALETTE.length];
}

async function refresh() {
  const r = await fetch('/api/sru/all').then(x=>x.json()).catch(()=>({}));
  const aps = r.aps||[];
  const stats = r.color_stats||[];

  document.getElementById('h-total').textContent  = r.ap_count||0;
  document.getElementById('h-he').textContent     = r.he_count||0;
  document.getElementById('h-coll').textContent   = r.collisions||0;
  document.getElementById('h-colors').textContent = stats.length;

  // Banner
  const banner = document.getElementById('sr-banner');
  const bannerText = document.getElementById('sr-text');
  if(r.spatial_reuse) {
    banner.className='sr-banner sr-active';
    bannerText.textContent='Spatial reuse is ACTIVE — all Wi-Fi 6 APs have unique non-conflicting BSS colors.';
  } else if(r.he_count > 0 && r.collisions > 0) {
    banner.className='sr-banner';
    banner.style.cssText='padding:12px 16px;border-radius:7px;font-size:13px;font-weight:600;margin-bottom:16px;display:flex;align-items:center;gap:8px;background:rgba(239,68,68,.1);border:1px solid rgba(239,68,68,.3);color:var(--danger)';
    bannerText.textContent=`Spatial reuse DEGRADED — ${r.collisions} BSS color collision(s) detected on same channels.`;
  } else if(r.he_count > 0) {
    banner.className='sr-banner sr-active';
    bannerText.textContent='Wi-Fi 6 APs detected with valid BSS colors. No collisions.';
  } else {
    banner.className='sr-banner sr-inactive';
    bannerText.textContent='No Wi-Fi 6 (802.11ax/HE) APs detected. BSS coloring not in use.';
  }

  // Find colliding colors (same channel+color across >1 AP)
  const color_ch_map = {};
  aps.forEach(ap=>{
    if(!ap.color) return;
    const key = `${ap.ch}:${ap.color}`;
    if(!color_ch_map[key]) color_ch_map[key] = 0;
    color_ch_map[key]++;
  });

  document.getElementById('ap-tbody').innerHTML = aps.length
    ? aps.map(ap=>{
        const he = ap.he?'<span class="badge badge-he">Wi-Fi 6</span>':'<span class="badge badge-legacy">Legacy</span>';
        const colorStr = ap.color
          ? `<span style="display:flex;align-items:center"><span class="color-dot" style="background:${color_hue(ap.color)}"></span>${ap.color}</span>`
          : '<span style="color:var(--muted)">None</span>';
        const key = `${ap.ch}:${ap.color}`;
        const is_coll = ap.color && color_ch_map[key] > 1;
        const coll_badge = ap.disabled
          ? '<span class="badge badge-disabled">Disabled</span>'
          : (is_coll ? '<span class="badge badge-collision">COLLISION</span>' : (ap.color ? '<span class="badge badge-ok">OK</span>' : '—'));

        return `<tr>
          <td><code style="font-size:10px">${ap.bssid}</code></td>
          <td>${ap.ssid||'—'}</td>
          <td>${he}</td>
          <td>${ap.ch}</td>
          <td>${ap.rssi} dBm</td>
          <td>${colorStr}</td>
          <td>${coll_badge}</td>
        </tr>`;
      }).join('')
    : '<tr><td colspan="7" style="color:var(--muted);text-align:center">No APs.</td></tr>';

  // Color map
  document.getElementById('color-map').innerHTML = stats.map(c=>{
    const col = color_hue(c.color);
    const coll = c.collision ? '⚠' : '';
    return `<div style="background:var(--bg3);border:1px solid ${c.collision?'var(--danger)':'var(--border)'};border-radius:6px;padding:8px 10px;min-width:70px;text-align:center">
      <div style="width:16px;height:16px;border-radius:50%;background:${col};margin:0 auto 4px"></div>
      <div style="font-size:11px;font-weight:700">Color ${c.color}</div>
      <div style="font-size:10px;color:var(--muted)">${c.aps} AP${c.aps!==1?'s':''} ${coll}</div>
    </div>`;
  }).join('') || '<div style="color:var(--muted);font-size:12px">No colored APs yet.</div>';
}

async function clearData() {
  await fetch('/api/sru/clear',{method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 5000);
</script>
</body>
</html>
)rawhtml";

inline void wusru_routes() {
  server.on("/spatial-reuse", HTTP_GET, []() { server.send_P(200, "text/html", WUSRU_PAGE); });
}

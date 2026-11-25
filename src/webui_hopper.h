#pragma once
// webui_hopper.h — Channel hopper control UI.
// Route: /hopper. Prefix: wufhp_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUFHP_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Channel Hopper — Phantom32</title>
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
.container{max-width:900px;margin:0 auto;padding:20px 16px;}
h1{font-size:22px;font-weight:700;margin-bottom:4px;}
.subtitle{color:var(--muted);font-size:13px;margin-bottom:20px;}
.hero{display:grid;grid-template-columns:repeat(3,1fr);gap:10px;margin-bottom:16px;}
.stat{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:13px;}
.stat label{font-size:10px;color:var(--muted);text-transform:uppercase;}
.stat .val{font-size:28px;font-weight:800;color:var(--accent);margin-top:3px;}
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:18px;margin-bottom:14px;}
.section h3{font-size:12px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:14px;}
.ctrl-row{display:flex;gap:10px;flex-wrap:wrap;align-items:center;margin-bottom:14px;}
label{font-size:12px;color:var(--muted);}
select,input{background:var(--bg3);border:1px solid var(--border);border-radius:5px;padding:5px 10px;color:var(--text);font-size:12px;}
.btn{padding:7px 14px;border:none;border-radius:5px;font-size:12px;font-weight:600;cursor:pointer;}
.btn-primary{background:var(--accent);color:#000;}
.btn-primary:hover{opacity:.9;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.ch-grid{display:grid;grid-template-columns:repeat(13,1fr);gap:4px;}
.ch-tile{padding:8px 4px;border-radius:5px;text-align:center;border:1px solid var(--border);cursor:pointer;transition:all .15s;}
.ch-tile:hover{border-color:var(--accent);}
.ch-tile.current{border-color:var(--accent);background:rgba(0,212,255,.12);}
.ch-num{font-size:11px;font-weight:700;}
.ch-fps{font-size:9px;color:var(--muted);margin-top:2px;}
.ch-bar{margin-top:4px;height:3px;background:var(--border);border-radius:2px;}
.ch-fill{height:100%;border-radius:2px;background:var(--accent);}
.ch-dwell{font-size:9px;color:var(--accent);margin-top:2px;}
@media(max-width:700px){.hero{grid-template-columns:1fr};.ch-grid{grid-template-columns:repeat(7,1fr)}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/channels">Channels</a>
    <a href="/hopper" class="active">Hopper</a>
    <a href="/interference">Interference</a>
    <a href="/frame-rates">Frame Rates</a>
  </nav>
</div>
<div class="container">
  <h1>Channel Hopper</h1>
  <p class="subtitle">Intelligent channel-hopping control. Adaptive mode learns which channels have the most activity and allocates more dwell time there.</p>

  <div class="hero">
    <div class="stat"><label>Current Channel</label><div class="val" id="h-ch">—</div></div>
    <div class="stat"><label>Mode</label><div class="val" id="h-mode" style="font-size:16px;padding-top:4px">—</div></div>
    <div class="stat"><label>Total Hops</label><div class="val" id="h-hops">0</div></div>
  </div>

  <div class="section">
    <h3>Hop Control</h3>
    <div class="ctrl-row">
      <div>
        <label>Mode</label><br>
        <select id="ctrl-mode">
          <option value="0">Sequential (1→13)</option>
          <option value="1">Non-Overlap (1/6/11)</option>
          <option value="2" selected>Adaptive</option>
          <option value="3">Fixed Channel</option>
        </select>
      </div>
      <div>
        <label>Dwell (ms)</label><br>
        <input id="ctrl-dwell" type="number" value="100" min="10" max="2000" style="width:80px">
      </div>
      <div>
        <label>Fixed Channel</label><br>
        <input id="ctrl-fixed-ch" type="number" value="6" min="1" max="13" style="width:60px">
      </div>
      <button class="btn btn-primary" onclick="applySettings()">Apply</button>
    </div>
  </div>

  <div class="section">
    <h3>Channel Dwell Times</h3>
    <div class="ch-grid" id="ch-grid"></div>
  </div>
</div>

<script>
const MODE_NAMES = ['Sequential','Non-Overlap','Adaptive','Fixed'];

async function applySettings() {
  const mode = document.getElementById('ctrl-mode').value;
  const dwell = document.getElementById('ctrl-dwell').value;
  const ch = document.getElementById('ctrl-fixed-ch').value;

  if(mode === '3') {
    await fetch(`/api/fhp/set_channel?ch=${ch}`, {method:'POST'});
  } else {
    await fetch(`/api/fhp/set_mode?mode=${mode}&dwell=${dwell}`, {method:'POST'});
  }
  refresh();
}

async function jumpToChannel(ch) {
  await fetch(`/api/fhp/set_channel?ch=${ch}`, {method:'POST'});
  refresh();
}

async function refresh() {
  const r = await fetch('/api/fhp/status').then(x=>x.json()).catch(()=>({}));
  document.getElementById('h-ch').textContent   = r.current_ch||'—';
  document.getElementById('h-mode').textContent = MODE_NAMES[r.mode]||'—';
  document.getElementById('h-hops').textContent = r.total_hops||0;

  const channels = r.channels||[];
  const max_dwell = Math.max(...channels.map(c=>c.dwell_ms||0), 1);
  document.getElementById('ch-grid').innerHTML = channels.map(c=>`
    <div class="ch-tile ${c.ch===r.current_ch?'current':''}" onclick="jumpToChannel(${c.ch})">
      <div class="ch-num" style="color:${c.ch===r.current_ch?'var(--accent)':'var(--text)'}">${c.ch}</div>
      <div class="ch-fps">${c.aps} APs</div>
      <div class="ch-bar"><div class="ch-fill" style="width:${Math.round((c.dwell_ms/max_dwell)*100)}%"></div></div>
      <div class="ch-dwell">${c.dwell_ms}ms</div>
    </div>`).join('');
}

refresh();
setInterval(refresh, 1000);
</script>
</body>
</html>
)rawhtml";

inline void wufhp_routes() {
  server.on("/hopper", HTTP_GET, []() { server.send_P(200, "text/html", WUFHP_PAGE); });
}

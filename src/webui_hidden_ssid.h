#pragma once
// webui_hidden_ssid.h — Hidden SSID discovery UI.
// Route: /hidden-ssid. Prefix: wuhss_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUHSS_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Hidden SSID — Phantom32</title>
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
.container{max-width:1000px;margin:0 auto;padding:20px 16px;}
h1{font-size:22px;font-weight:700;margin-bottom:4px;}
.subtitle{color:var(--muted);font-size:13px;margin-bottom:20px;}
.hero{display:grid;grid-template-columns:repeat(4,1fr);gap:10px;margin-bottom:16px;}
.stat{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:13px;}
.stat label{font-size:10px;color:var(--muted);text-transform:uppercase;}
.stat .val{font-size:22px;font-weight:800;color:var(--accent);margin-top:3px;}
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:18px;margin-bottom:14px;}
.section h3{font-size:12px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:14px;}
.card-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(260px,1fr));gap:10px;}
.hap-card{background:var(--bg3);border:1px solid var(--border);border-radius:7px;padding:14px;}
.hap-card.confirmed{border-left:3px solid var(--success);}
.hap-card.probable{border-left:3px solid var(--accent);}
.hap-card.partial{border-left:3px solid var(--warn);}
.hap-card.hidden{border-left:3px solid var(--border);}
.reveal-badge{padding:2px 7px;border-radius:4px;font-size:9px;font-weight:700;display:inline-block;margin-bottom:7px;}
.r-Confirmed{background:rgba(34,197,94,.12);color:var(--success);}
.r-Probable{background:rgba(0,212,255,.1);color:var(--accent);}
.r-Partial{background:rgba(245,158,11,.1);color:var(--warn);}
.r-Hidden{background:rgba(100,116,139,.1);color:var(--muted);}
.card-ssid{font-size:16px;font-weight:700;margin-bottom:4px;overflow:hidden;text-overflow:ellipsis;}
.card-ssid.hidden-ssid{color:var(--muted);font-style:italic;}
.card-bssid{font-size:9px;color:var(--muted);font-family:monospace;margin-bottom:8px;}
.card-meta{font-size:10px;color:var(--muted);margin-bottom:8px;}
.card-meta span{color:var(--text);font-weight:600;}
.guess-list{margin-top:8px;}
.guess-label{font-size:9px;color:var(--muted);text-transform:uppercase;margin-bottom:4px;}
.guess-pill{padding:1px 6px;border-radius:3px;font-size:9px;background:var(--bg2);border:1px solid var(--border);display:inline-block;margin:1px;}
.guess-pill.assoc{color:var(--success);border-color:rgba(34,197,94,.3);}
.guess-pill.probe_rsp{color:var(--accent);border-color:rgba(0,212,255,.3);}
.guess-pill.probe_req{color:var(--muted);}
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
    <a href="/rogue-ap">Rogue AP</a>
    <a href="/hidden-ssid" class="active">Hidden SSID</a>
    <a href="/probe-flood">Probe Flood</a>
  </nav>
</div>
<div class="container">
  <h1>Hidden SSID Discovery</h1>
  <p class="subtitle">Passively reveals hidden SSIDs from probe responses, association requests, and directed probes. The more clients associate with the hidden AP, the more likely the SSID is confirmed.</p>

  <div class="hero">
    <div class="stat"><label>Hidden APs</label><div class="val" id="h-cnt">0</div></div>
    <div class="stat"><label>Resolved</label><div class="val" id="h-res" style="color:var(--success)">0</div></div>
    <div class="stat"><label>Probable</label><div class="val" id="h-prob" style="color:var(--accent)">0</div></div>
    <div class="stat"><label>Still Hidden</label><div class="val" id="h-unk" style="color:var(--muted)">0</div></div>
  </div>

  <div class="section">
    <h3>Hidden AP Discovery</h3>
    <div class="card-grid" id="card-grid">
      <div style="color:var(--muted);grid-column:1/-1;text-align:center;padding:20px">No hidden SSIDs detected.</div>
    </div>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>
</div>

<script>
let g_aps = [];

const EV_NAMES = {1:'probe_req', 2:'probe_rsp', 3:'assoc', 4:'direct'};
const CONF_ORDER = {Confirmed:4, Probable:3, Partial:2, Hidden:1};

function guessClass(ev) {
  if (ev >= 3) return 'assoc';
  if (ev >= 2) return 'probe_rsp';
  return 'probe_req';
}

function guessLabel(ev) {
  return EV_NAMES[ev] || 'unknown';
}

function rssiColor(r) {
  return r >= -60 ? 'var(--success)' : r >= -75 ? 'var(--warn)' : 'var(--danger)';
}

async function refresh() {
  const r = await fetch('/api/hss/all').then(x => x.json()).catch(() => ({}));
  g_aps = r.aps || [];
  document.getElementById('h-cnt').textContent  = r.count || 0;
  document.getElementById('h-res').textContent  = r.total_resolved || 0;
  document.getElementById('h-prob').textContent = g_aps.filter(a => a.reveal === 'Probable').length;
  document.getElementById('h-unk').textContent  = g_aps.filter(a => a.reveal === 'Hidden').length;
  renderCards();
}

function renderCards() {
  const sorted = [...g_aps].sort((a,b) => (CONF_ORDER[b.reveal]||0) - (CONF_ORDER[a.reveal]||0));
  document.getElementById('card-grid').innerHTML = sorted.length
    ? sorted.map(a => {
        const revClass = a.reveal.toLowerCase();
        const ssidDisplay = a.revealed_ssid
          ? `<div class="card-ssid">${a.revealed_ssid}</div>`
          : `<div class="card-ssid hidden-ssid">[Hidden — ${a.ssid_len} chars]</div>`;
        const guesses = (a.guesses||[]).map(g =>
          `<span class="guess-pill ${guessClass(g.ev)}" title="${guessLabel(g.ev)}">${g.ssid}</span>`
        ).join('');
        return `<div class="hap-card ${revClass}">
          <span class="reveal-badge r-${a.reveal}">${a.reveal}</span>
          ${ssidDisplay}
          <div class="card-bssid">${a.bssid}</div>
          <div class="card-meta">
            CH <span>${a.ch}</span> &nbsp;
            RSSI <span style="color:${rssiColor(a.rssi)}">${a.rssi} dBm</span> &nbsp;
            Beacons <span>${a.beacons}</span>
            ${a.assocs ? `&nbsp; Assocs <span style="color:var(--success)">${a.assocs}</span>` : ''}
          </div>
          ${guesses ? `<div class="guess-list">
            <div class="guess-label">Candidates</div>
            ${guesses}
          </div>` : ''}
        </div>`;
      }).join('')
    : '<div style="color:var(--muted);grid-column:1/-1;text-align:center;padding:20px">No hidden APs detected.</div>';
}

async function clearData() {
  await fetch('/api/hss/clear', {method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 4000);
</script>
</body>
</html>
)rawhtml";

inline void wuhss_routes() {
  server.on("/hidden-ssid", HTTP_GET, []() { server.send_P(200, "text/html", WUHSS_PAGE); });
}

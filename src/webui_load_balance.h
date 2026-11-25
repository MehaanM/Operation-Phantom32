#pragma once
// webui_load_balance.h — Load balance monitor UI.
// Route: /load-balance. Prefix: wulbm_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WULBM_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Load Balance — Phantom32</title>
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
.ap-card{background:var(--bg3);border:1px solid var(--border);border-radius:7px;padding:14px;margin-bottom:8px;}
.ap-card.congested{border-left:3px solid var(--danger);}
.ap-card.saturated{border-left:3px solid var(--warn);}
.ap-header{display:flex;align-items:center;gap:10px;margin-bottom:10px;}
.ap-title{font-size:14px;font-weight:700;color:var(--accent);}
.ap-bssid{font-size:9px;color:var(--muted);font-family:monospace;}
.load-row{margin-bottom:8px;}
.load-label{display:flex;justify-content:space-between;font-size:10px;color:var(--muted);margin-bottom:3px;}
.load-track{height:9px;background:var(--border);border-radius:5px;overflow:hidden;}
.load-fill{height:100%;border-radius:5px;transition:width .3s;}
.ap-stats{display:flex;gap:16px;font-size:10px;color:var(--muted);flex-wrap:wrap;}
.ap-stats span{color:var(--text);font-weight:600;}
.mini-chart{display:flex;align-items:flex-end;gap:1px;height:14px;margin-top:8px;}
.mini-bar{flex:1;border-radius:1px 1px 0 0;min-height:1px;}
.btm-row{padding:6px 10px;border-bottom:1px solid var(--border);font-size:10px;}
.btm-type{padding:1px 5px;border-radius:3px;font-size:8px;font-weight:600;}
.t-Request{background:rgba(0,212,255,.12);color:var(--accent);}
.t-Response{background:rgba(34,197,94,.12);color:var(--success);}
.t-Query{background:rgba(100,116,139,.1);color:var(--muted);}
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
    <a href="/load-balance" class="active">Load Balance</a>
    <a href="/roaming">Roaming</a>
    <a href="/channel-util">Channel Util</a>
  </nav>
</div>
<div class="container">
  <h1>Load Balance Monitor</h1>
  <p class="subtitle">Monitors 802.11e BSS Load IE (station count, channel utilization, admission capacity) and 802.11v BSS Transition Management (BTM) events for load-balancing behavior.</p>

  <div class="hero">
    <div class="stat"><label>Monitored APs</label><div class="val" id="h-ap">0</div></div>
    <div class="stat"><label>BTM Events</label><div class="val" id="h-btm" style="color:var(--accent)">0</div></div>
    <div class="stat"><label>Congested APs</label><div class="val" id="h-cong" style="color:var(--danger)">0</div></div>
    <div class="stat"><label>Saturated APs</label><div class="val" id="h-sat" style="color:var(--warn)">0</div></div>
  </div>

  <div class="two-col">
    <div class="section">
      <h3>AP Load Overview</h3>
      <div id="ap-list">
        <div style="color:var(--muted)">No AP load data.</div>
      </div>
    </div>

    <div class="section">
      <h3>BSS Transition Events</h3>
      <div id="btm-log">
        <div style="color:var(--muted)">No BTM events.</div>
      </div>
    </div>
  </div>

  <div class="action-row">
    <button class="btn btn-ghost" onclick="clearData()">Clear</button>
  </div>
</div>

<script>
function utilColor(u) {
  const pct = u/255*100;
  if (pct > 75) return 'var(--danger)';
  if (pct > 40) return 'var(--warn)';
  return 'var(--success)';
}

function miniChart(hist, colorFn) {
  if (!hist || !hist.length) return '';
  const max = Math.max(...hist, 1);
  return hist.map(v => {
    const h = Math.max(1, Math.round((v/max)*12));
    return `<div class="mini-bar" style="height:${h}px;background:${colorFn(v)}"></div>`;
  }).join('');
}

function utilPct(u) { return (u/255*100).toFixed(1) + '%'; }

function rssiColor(r) {
  return r >= -60 ? 'var(--success)' : r >= -75 ? 'var(--warn)' : 'var(--danger)';
}

async function refresh() {
  const r = await fetch('/api/lbm/all').then(x => x.json()).catch(()=>({}));
  const aps = r.aps || [];
  document.getElementById('h-ap').textContent   = r.count || 0;
  document.getElementById('h-btm').textContent  = r.total_btm || 0;
  document.getElementById('h-cong').textContent = aps.filter(a => a.congested).length;
  document.getElementById('h-sat').textContent  = aps.filter(a => a.saturated).length;

  document.getElementById('ap-list').innerHTML = aps.length
    ? aps.map(a => {
        const utilPctNum = Math.round(a.ch_util/255*100);
        const cls = a.saturated ? 'ap-card saturated' : a.congested ? 'ap-card congested' : 'ap-card';
        return `<div class="${cls}">
          <div class="ap-header">
            <div>
              <div class="ap-title">${a.ssid||'Hidden'}</div>
              <div class="ap-bssid">${a.bssid}</div>
            </div>
            <div style="margin-left:auto;font-size:10px;color:${rssiColor(a.rssi)}">${a.rssi} dBm CH${a.ch}</div>
          </div>
          <div class="load-row">
            <div class="load-label"><span>Channel Util</span><span style="color:${utilColor(a.ch_util)}">${utilPctNum}%</span></div>
            <div class="load-track"><div class="load-fill" style="width:${utilPctNum}%;background:${utilColor(a.ch_util)}"></div></div>
          </div>
          <div class="load-row">
            <div class="load-label"><span>Stations</span><span>${a.station_count} (peak: ${a.peak_stations})</span></div>
            <div class="load-track"><div class="load-fill" style="width:${Math.min(a.station_count/50*100,100)}%;background:var(--accent)"></div></div>
          </div>
          <div class="ap-stats">
            <span>BTM Req: <span>${a.btm_req}</span></span>
            <span>Accept: <span style="color:var(--success)">${a.btm_accept}</span></span>
            <span>Reject: <span style="color:var(--danger)">${a.btm_reject}</span></span>
            <span>Avail Adm: <span>${a.avail_adm}</span></span>
            ${a.saturated ? '<span style="color:var(--warn)">SATURATED</span>' : ''}
          </div>
          <div class="mini-chart">${miniChart(a.util_hist, utilColor)}</div>
        </div>`;
      }).join('')
    : '<div style="color:var(--muted)">No APs with BSS Load IE observed.</div>';

  const evts = r.events || [];
  document.getElementById('btm-log').innerHTML = evts.length
    ? evts.map(e => `<div class="btm-row">
        <span class="btm-type t-${e.type}">${e.type}</span>
        &nbsp;<strong>${e.mac}</strong>
        &nbsp;→ ${e.ap}
        ${e.disassoc_timer ? `&nbsp;DT:${e.disassoc_timer}` : ''}
        ${e.type==='Response' ? `&nbsp;<span style="color:${e.status===0?'var(--success)':'var(--danger)'}">status=${e.status}</span>` : ''}
        ${e.candidate!=='00:00:00:00:00:00' ? `&nbsp;cand:${e.candidate}` : ''}
      </div>`).join('')
    : '<div style="color:var(--muted)">No BTM events observed.</div>';
}

async function clearData() {
  await fetch('/api/lbm/clear', {method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 3000);
</script>
</body>
</html>
)rawhtml";

inline void wulbm_routes() {
  server.on("/load-balance", HTTP_GET, []() { server.send_P(200, "text/html", WULBM_PAGE); });
}

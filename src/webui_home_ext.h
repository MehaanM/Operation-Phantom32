#pragma once
// webui_home_ext.h — Extended home dashboard with live RF heatmap and system health panel.
// Route: /dashboard. Prefix: wuhe_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUHE_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Dashboard — Phantom32</title>
<style>
:root{--bg:#0a0c10;--bg2:#12151c;--bg3:#1a1f2e;--border:#2a3045;
  --accent:#00d4ff;--success:#22c55e;--warn:#f59e0b;--danger:#ef4444;
  --text:#e2e8f0;--muted:#64748b;--radius:8px;}
*{box-sizing:border-box;margin:0;padding:0;}
body{font-family:'Inter',system-ui,sans-serif;background:var(--bg);color:var(--text);min-height:100vh;}
.top-bar{display:flex;align-items:center;gap:12px;padding:14px 20px;
  background:var(--bg2);border-bottom:1px solid var(--border);position:sticky;top:0;z-index:10;}
.logo{font-size:18px;font-weight:700;color:var(--accent);letter-spacing:2px;}
.logo-pulse{display:inline-block;width:8px;height:8px;border-radius:50%;background:var(--success);
  margin-right:6px;animation:pulse 2s infinite;}
@keyframes pulse{0%,100%{opacity:1;transform:scale(1)}50%{opacity:.6;transform:scale(.8)}}
.nav-links{display:flex;gap:6px;margin-left:auto;flex-wrap:wrap;}
.nav-links a{color:var(--muted);text-decoration:none;font-size:12px;padding:5px 10px;border-radius:5px;transition:all .2s;}
.nav-links a:hover{color:var(--text);background:var(--bg3);}
.nav-links a.active{color:var(--accent);background:rgba(0,212,255,.08);}
.container{max-width:1300px;margin:0 auto;padding:20px 16px;}
.hero-row{display:grid;grid-template-columns:repeat(5,1fr);gap:10px;margin-bottom:16px;}
.stat{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:14px;position:relative;overflow:hidden;}
.stat::after{content:'';position:absolute;bottom:0;left:0;right:0;height:2px;background:var(--accent);opacity:.3;}
.stat label{font-size:10px;color:var(--muted);text-transform:uppercase;letter-spacing:.5px;}
.stat .val{font-size:26px;font-weight:800;color:var(--accent);margin:4px 0 2px;}
.stat .delta{font-size:11px;color:var(--success);}
.grid-2{display:grid;grid-template-columns:1fr 1fr;gap:16px;margin-bottom:16px;}
.grid-3{display:grid;grid-template-columns:2fr 1fr;gap:16px;margin-bottom:16px;}
.card{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:18px;}
.card h3{font-size:12px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:12px;}
canvas{display:block;width:100%;border-radius:4px;}
.ap-list{max-height:260px;overflow-y:auto;}
.ap-row{display:flex;align-items:center;gap:10px;padding:7px 0;border-bottom:1px solid var(--border);}
.ap-row:last-child{border-bottom:none;}
.ap-ssid{flex:1;font-size:13px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;}
.ap-badges{display:flex;gap:4px;}
.badge{padding:1px 6px;border-radius:4px;font-size:10px;font-weight:600;}
.badge-wpa3{background:rgba(34,197,94,.2);color:var(--success);}
.badge-wpa2{background:rgba(0,212,255,.15);color:var(--accent);}
.badge-wpa{background:rgba(245,158,11,.2);color:var(--warn);}
.badge-open{background:rgba(239,68,68,.2);color:var(--danger);}
.badge-ht{background:rgba(100,116,139,.2);color:var(--muted);}
.ap-rssi{font-size:12px;min-width:55px;text-align:right;}
.alert-row{padding:5px 0;border-bottom:1px solid var(--border);font-size:12px;}
.alert-row:last-child{border-bottom:none;}
.alert-time{color:var(--muted);font-size:11px;margin-right:6px;}
.sysbar{height:14px;border-radius:7px;background:var(--bg3);overflow:hidden;margin:4px 0;}
.sysbar-fill{height:100%;border-radius:7px;transition:width .5s;}
.sysbar-label{display:flex;justify-content:space-between;font-size:11px;color:var(--muted);margin-top:2px;}
.cli-box{background:var(--bg3);border-radius:5px;padding:10px;font-family:monospace;font-size:12px;min-height:80px;max-height:120px;overflow-y:auto;color:#a0f0a0;}
.cli-input-row{display:flex;gap:6px;margin-top:8px;}
.cli-input{flex:1;background:var(--bg3);border:1px solid var(--border);border-radius:4px;color:var(--text);font-family:monospace;font-size:12px;padding:5px 8px;outline:none;}
.cli-input:focus{border-color:var(--accent);}
.btn{padding:6px 12px;border:none;border-radius:4px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-primary{background:var(--accent);color:#000;}
.btn-primary:hover{opacity:.85;}
.threat-pill{display:inline-block;padding:3px 10px;border-radius:12px;font-size:11px;font-weight:600;margin:2px;}
.threat-high{background:rgba(239,68,68,.2);color:var(--danger);}
.threat-med{background:rgba(245,158,11,.2);color:var(--warn);}
.threat-low{background:rgba(100,116,139,.2);color:var(--muted);}
@media(max-width:900px){.hero-row{grid-template-columns:repeat(3,1fr)}.grid-2,.grid-3{grid-template-columns:1fr}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo"><span class="logo-pulse"></span>PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard" class="active">Dashboard</a>
    <a href="/analytics">Analytics</a>
    <a href="/devices">Devices</a>
    <a href="/ssids">SSIDs</a>
    <a href="/security">Security</a>
    <a href="/threats">Threats</a>
    <a href="/capture">Capture</a>
    <a href="/eapol">EAPOL</a>
    <a href="/probes">Probes</a>
    <a href="/bandwidth">BW</a>
    <a href="/channels">Channels</a>
    <a href="/noise">Noise</a>
    <a href="/watchlist">Watch</a>
    <a href="/associations">Assoc</a>
    <a href="/scheduler">Sched</a>
    <a href="/settings">Settings</a>
  </nav>
</div>

<div class="container">
  <div class="hero-row">
    <div class="stat"><label>APs Seen</label><div class="val" id="h-aps">0</div><div class="delta" id="h-aps-d"></div></div>
    <div class="stat"><label>Clients</label><div class="val" id="h-stas">0</div><div class="delta" id="h-stas-d"></div></div>
    <div class="stat"><label>Frames/s</label><div class="val" id="h-fps">0</div></div>
    <div class="stat"><label>Uptime</label><div class="val" id="h-uptime">—</div></div>
    <div class="stat"><label>Heap Free</label><div class="val" id="h-heap">—</div></div>
  </div>

  <div class="grid-3">
    <!-- AP list -->
    <div class="card">
      <h3>Nearby Access Points</h3>
      <div class="ap-list" id="ap-list"><div style="color:var(--muted)">Scanning…</div></div>
    </div>

    <!-- System health -->
    <div class="card">
      <h3>System Health</h3>
      <div style="font-size:11px;color:var(--muted);margin-bottom:8px">Heap Usage</div>
      <div class="sysbar"><div class="sysbar-fill" id="heap-bar" style="background:var(--success)"></div></div>
      <div class="sysbar-label"><span>Used</span><span id="heap-used-pct">0%</span></div>

      <div style="font-size:11px;color:var(--muted);margin:10px 0 4px">CPU Freq</div>
      <div style="font-size:18px;font-weight:700;color:var(--accent)" id="cpu-freq">—</div>

      <div style="font-size:11px;color:var(--muted);margin:10px 0 4px">Flash</div>
      <div style="font-size:14px;font-weight:700;color:var(--text)" id="flash-size">—</div>

      <div style="font-size:11px;color:var(--muted);margin:10px 0 4px">Reset Reason</div>
      <div style="font-size:12px;color:var(--warn)" id="reset-reason">—</div>

      <div style="font-size:11px;color:var(--muted);margin:10px 0 4px">Session</div>
      <div style="font-size:12px" id="session-id">—</div>
    </div>
  </div>

  <div class="grid-2">
    <!-- RF spectrum canvas -->
    <div class="card">
      <h3>RF Spectrum (Channels 1–13)</h3>
      <canvas id="rf-canvas" height="100"></canvas>
    </div>

    <!-- Recent alerts -->
    <div class="card">
      <h3>Recent Alerts</h3>
      <div class="ap-list" id="alerts-list" style="max-height:120px">
        <div style="color:var(--muted)">No alerts yet.</div>
      </div>
    </div>
  </div>

  <!-- CLI -->
  <div class="card">
    <h3>Quick CLI</h3>
    <div class="cli-box" id="cli-output">Type a command and press Enter or click Run.<br>&gt; </div>
    <div class="cli-input-row">
      <span style="color:var(--accent);font-family:monospace;padding-top:5px">&gt;</span>
      <input class="cli-input" id="cli-input" placeholder="uptime, heap, routes, channel 6, lfs, tasks…"
             onkeydown="if(event.key==='Enter')runCli()">
      <button class="btn btn-primary" onclick="runCli()">Run</button>
    </div>
  </div>
</div>

<script>
let g_prev_aps = 0, g_prev_stas = 0;

function fmt_uptime(s) {
  if (s < 60) return s+'s';
  if (s < 3600) return Math.floor(s/60)+'m '+s%60+'s';
  return Math.floor(s/3600)+'h '+ Math.floor((s%3600)/60)+'m';
}
function fmt_bytes(b) {
  if (b < 1024) return b+'B';
  if (b < 1048576) return (b/1024).toFixed(0)+'KB';
  return (b/1048576).toFixed(1)+'MB';
}

function sec_badge(wpa3,wpa2,wpa,open) {
  if (wpa3)  return '<span class="badge badge-wpa3">WPA3</span>';
  if (wpa2)  return '<span class="badge badge-wpa2">WPA2</span>';
  if (wpa)   return '<span class="badge badge-wpa">WPA</span>';
  if (open)  return '<span class="badge badge-open">OPEN</span>';
  return '<span class="badge badge-ht">?</span>';
}

function rssi_col(r) {
  if (r > -60) return 'var(--success)';
  if (r > -75) return 'var(--warn)';
  return 'var(--danger)';
}

async function refresh() {
  // Session stats
  const ses = await fetch('/api/ses/status').then(x=>x.json()).catch(()=>({}));
  if (ses.uptime_s !== undefined) {
    document.getElementById('h-uptime').textContent = fmt_uptime(ses.uptime_s);
    document.getElementById('h-heap').textContent   = fmt_bytes(ses.heap_free||0);
    document.getElementById('cpu-freq').textContent = (ses.cpu_freq||240)+' MHz';
    document.getElementById('flash-size').textContent = fmt_bytes(ses.flash_size||4194304);
    document.getElementById('session-id').textContent = 'Boot #'+(ses.boot_count||0)+' · ID '+(ses.session_id||0);

    const total = 327680;
    const used  = total - (ses.heap_free||0);
    const pct   = Math.round(100*used/total);
    document.getElementById('heap-bar').style.width = pct+'%';
    document.getElementById('heap-bar').style.background = pct>80?'var(--danger)':pct>60?'var(--warn)':'var(--success)';
    document.getElementById('heap-used-pct').textContent = pct+'%';
  }

  // AP/STA counts from various endpoints
  const tracker = await fetch('/api/dt/status').then(x=>x.json()).catch(()=>({}));
  const aps  = tracker.ap_count  || 0;
  const stas = tracker.sta_count || 0;
  const fps  = tracker.pkt_rate  || 0;
  document.getElementById('h-aps').textContent   = aps;
  document.getElementById('h-stas').textContent  = stas;
  document.getElementById('h-fps').textContent   = fps;
  document.getElementById('h-aps-d').textContent  = aps > g_prev_aps ? '+'+( aps-g_prev_aps) : '';
  document.getElementById('h-stas-d').textContent = stas > g_prev_stas ? '+'+(stas-g_prev_stas) : '';
  g_prev_aps = aps; g_prev_stas = stas;

  // AP list from scan results
  const scan = await fetch('/api/scan/results').then(x=>x.json()).catch(()=>({}));
  const ap_list = (scan.aps||[]).slice(0,20);
  if (ap_list.length) {
    document.getElementById('ap-list').innerHTML = ap_list.map(ap=>`
      <div class="ap-row">
        <div class="ap-ssid">${ap.ssid||'<hidden>'}</div>
        <div class="ap-badges">
          ${sec_badge(ap.wpa3,ap.wpa2,ap.wpa,ap.open)}
          ${ap.ht?'<span class="badge badge-ht">HT</span>':''}
          ${ap.vht?'<span class="badge badge-wpa3">VHT</span>':''}
        </div>
        <div class="ap-rssi" style="color:${rssi_col(ap.rssi)}">${ap.rssi} dBm</div>
      </div>`).join('');
  }

  // Alerts from watchlist
  const awl = await fetch('/api/awl/status').then(x=>x.json()).catch(()=>({}));
  const alerts = (awl.recent_alerts||[]).slice(0,8);
  if (alerts.length) {
    document.getElementById('alerts-list').innerHTML = alerts.reverse().map(a=>`
      <div class="alert-row">
        <span class="alert-time">${new Date(a.ts*1000).toLocaleTimeString()}</span>
        Rule #${a.rule_id}: ${a.ssid||'?'} @ ${a.bssid}
      </div>`).join('');
  }

  // RF spectrum from channel stats
  drawSpectrum();
}

async function drawSpectrum() {
  const r = await fetch('/api/chs/all').then(x=>x.json()).catch(()=>({}));
  const channels = r.channels || [];
  const canvas = document.getElementById('rf-canvas');
  const ctx = canvas.getContext('2d');
  const W = canvas.clientWidth || 600;
  canvas.width = W;

  ctx.fillStyle = '#1a1f2e';
  ctx.fillRect(0,0,W,100);

  if (!channels.length) return;

  const ch_w = Math.floor((W-20)/14);
  channels.slice(0,13).forEach((c,i)=>{
    const util = c.util_pct || 0;
    const h = Math.round(70 * util / 100);
    const x = 10 + i*ch_w;
    const heat = util/100;
    const r_ = Math.round(heat*239 + (1-heat)*34);
    const g_ = Math.round(heat*68  + (1-heat)*197);
    ctx.fillStyle = `rgba(${r_},${g_},68,0.7)`;
    ctx.fillRect(x, 80-h, ch_w-2, h);
    ctx.fillStyle = '#64748b';
    ctx.font = '9px system-ui';
    ctx.fillText(c.ch||i+1, x+2, 95);
  });
}

async function runCli() {
  const inp = document.getElementById('cli-input');
  const cmd = inp.value.trim();
  if (!cmd) return;
  inp.value = '';

  const params = new URLSearchParams({cmd});
  const r = await fetch('/api/cli', {
    method:'POST',
    body: params,
    headers: {'Content-Type':'application/x-www-form-urlencoded'}
  }).then(x=>x.json()).catch(()=>({ok:false,output:'Network error'}));

  const box = document.getElementById('cli-output');
  box.innerHTML += '<span style="color:var(--accent)">&gt; '+cmd+'</span><br>'
                +  r.output.replace(/\n/g,'<br>') + '<br>';
  box.scrollTop = box.scrollHeight;
}

refresh();
setInterval(refresh, 3000);
</script>
</body>
</html>
)rawhtml";

inline void wuhe_routes() {
  server.on("/dashboard", HTTP_GET, []() { server.send_P(200, "text/html", WUHE_PAGE); });
}

#pragma once
// webui_sysmon.h — System monitor web UI with heap, tasks, and alerts.
// Route: /sysmon. Prefix: wusm_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUSM_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>System Monitor — Phantom32</title>
<style>
:root {
  --bg: #0a0c10; --bg2: #12151c; --bg3: #1a1f2e; --border: #2a3045;
  --accent: #00d4ff; --success: #22c55e; --warn: #f59e0b; --danger: #ef4444;
  --text: #e2e8f0; --muted: #64748b; --radius: 8px;
}
* { box-sizing:border-box; margin:0; padding:0; }
body { font-family:'Inter',system-ui,sans-serif; background:var(--bg); color:var(--text); }
.top-bar { display:flex; align-items:center; gap:12px; padding:14px 20px;
           background:var(--bg2); border-bottom:1px solid var(--border);
           position:sticky; top:0; z-index:10; }
.logo { font-size:18px; font-weight:700; color:var(--accent); letter-spacing:2px; }
.nav-links { display:flex; gap:8px; margin-left:auto; }
.nav-links a { color:var(--muted); text-decoration:none; font-size:13px; padding:5px 10px;
               border-radius:5px; transition:all .2s; }
.nav-links a:hover { color:var(--text); background:var(--bg3); }
.nav-links a.active { color:var(--accent); background:rgba(0,212,255,.08); }
.container { max-width:1200px; margin:0 auto; padding:24px 16px; }
h1 { font-size:22px; font-weight:700; margin-bottom:4px; }
.subtitle { color:var(--muted); font-size:14px; margin-bottom:24px; }
.grid4 { display:grid; grid-template-columns:repeat(4,1fr); gap:12px; margin-bottom:20px; }
.grid2 { display:grid; grid-template-columns:1fr 1fr; gap:16px; margin-bottom:16px; }
.card { background:var(--bg2); border:1px solid var(--border); border-radius:var(--radius); padding:16px; }
.card h3 { font-size:11px; color:var(--muted); text-transform:uppercase; margin-bottom:8px; }
.card .big { font-size:28px; font-weight:700; color:var(--accent); }
.card .unit { font-size:13px; color:var(--muted); }
.bar-label { display:flex; justify-content:space-between; font-size:12px; color:var(--muted); margin-bottom:4px; }
.bar-track { height:8px; background:var(--bg3); border-radius:4px; overflow:hidden; }
.bar-fill { height:100%; border-radius:4px; transition:width .4s; }
.bar-fill.ok { background:var(--success); }
.bar-fill.warn { background:var(--warn); }
.bar-fill.crit { background:var(--danger); }
.section { background:var(--bg2); border:1px solid var(--border); border-radius:var(--radius);
           padding:20px; margin-bottom:16px; }
.section h2 { font-size:14px; font-weight:600; color:var(--accent); text-transform:uppercase;
              letter-spacing:.5px; margin-bottom:14px; }
canvas { width:100%; height:120px; display:block; }
table { width:100%; border-collapse:collapse; font-size:13px; }
th { text-align:left; padding:6px 10px; font-size:11px; color:var(--muted);
     text-transform:uppercase; border-bottom:1px solid var(--border); }
td { padding:8px 10px; border-bottom:1px solid rgba(42,48,69,.5); }
tr:last-child td { border-bottom:none; }
.badge { display:inline-block; padding:2px 8px; border-radius:10px; font-size:11px; font-weight:500; }
.badge-run  { background:rgba(34,197,94,.15); color:var(--success); }
.badge-blk  { background:rgba(245,158,11,.15); color:var(--warn); }
.badge-sus  { background:rgba(100,116,139,.15); color:var(--muted); }
.badge-del  { background:rgba(239,68,68,.15); color:var(--danger); }
.alert-row { display:flex; align-items:flex-start; gap:10px; padding:8px 0;
             border-bottom:1px solid var(--border); font-size:13px; }
.alert-row:last-child { border-bottom:none; }
.dot { width:8px; height:8px; border-radius:50%; flex-shrink:0; margin-top:4px; }
.dot.ok { background:var(--success); }
.dot.warn { background:var(--warn); }
.dot.crit { background:var(--danger); }
.refresh-info { font-size:11px; color:var(--muted); margin-bottom:12px; }
.btn { padding:7px 14px; border:none; border-radius:5px; font-size:13px; cursor:pointer; }
.btn-ghost { background:transparent; border:1px solid var(--border); color:var(--text); }
.btn-warn  { background:var(--warn); color:#000; }
.action-row { display:flex; gap:8px; flex-wrap:wrap; margin-top:12px; }
@media(max-width:700px) { .grid4 { grid-template-columns:1fr 1fr; } .grid2 { grid-template-columns:1fr; } }
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/">Dashboard</a>
    <a href="/analytics">Analytics</a>
    <a href="/forensics">Forensics</a>
    <a href="/ble-iot">BLE/IoT</a>
    <a href="/cli">CLI</a>
    <a href="/threats">Threats</a>
    <a href="/vulns">Vulns</a>
    <a href="/map">Map</a>
    <a href="/sysmon" class="active">SysMon</a>
    <a href="/settings">Settings</a>
  </nav>
</div>

<div class="container">
  <h1>System Monitor</h1>
  <p class="subtitle">Real-time ESP32 health, task states, heap, and alerts.</p>
  <p class="refresh-info" id="refresh-info">Refreshing every 2 seconds…</p>

  <div class="grid4" id="stat-cards">
    <div class="card"><h3>Uptime</h3><div class="big" id="stat-uptime">—</div><div class="unit">seconds</div></div>
    <div class="card"><h3>Free Heap</h3><div class="big" id="stat-heap">—</div><div class="unit">bytes</div></div>
    <div class="card"><h3>Min Heap</h3><div class="big" id="stat-heap-min">—</div><div class="unit">ever seen</div></div>
    <div class="card"><h3>Tasks</h3><div class="big" id="stat-tasks">—</div><div class="unit">running</div></div>
  </div>

  <div class="grid2">
    <div class="section">
      <h2>Heap Usage</h2>
      <div class="bar-label"><span>Free Heap</span><span id="heap-pct-label">—</span></div>
      <div class="bar-track"><div class="bar-fill ok" id="heap-bar" style="width:0%"></div></div>
      <div style="margin-top:12px;">
        <canvas id="heap-chart"></canvas>
      </div>
    </div>
    <div class="section">
      <h2>Filesystem</h2>
      <div class="bar-label"><span>Used / Total</span><span id="fs-pct-label">—</span></div>
      <div class="bar-track"><div class="bar-fill ok" id="fs-bar" style="width:0%"></div></div>
      <div style="margin-top:12px;font-size:13px;color:var(--muted);line-height:2" id="fs-details">—</div>
    </div>
  </div>

  <div class="section">
    <h2>FreeRTOS Tasks</h2>
    <table>
      <thead>
        <tr><th>Name</th><th>State</th><th>Priority</th><th>Stack HWM</th><th>Stack Health</th></tr>
      </thead>
      <tbody id="task-table">
        <tr><td colspan="5" style="color:var(--muted)">Loading…</td></tr>
      </tbody>
    </table>
  </div>

  <div class="section">
    <h2>System Alerts</h2>
    <div id="alert-list"><p style="color:var(--muted);font-size:13px">No alerts.</p></div>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="clearAlerts()">Clear Alerts</button>
    </div>
  </div>

  <div class="section">
    <h2>Chip Info</h2>
    <div id="chip-info" style="font-size:13px;color:var(--muted);line-height:2">Loading…</div>
    <div class="action-row">
      <button class="btn btn-warn" onclick="reboot()">Reboot</button>
    </div>
  </div>
</div>

<script>
const heapHist = new Array(60).fill(0);
let totalHeap = 1;

function formatBytes(b) {
  if (b >= 1048576) return (b/1048576).toFixed(1)+'MB';
  if (b >= 1024)    return (b/1024).toFixed(1)+'KB';
  return b+'B';
}

function formatUptime(s) {
  const h = Math.floor(s/3600), m = Math.floor((s%3600)/60), sec = s%60;
  if (h>0) return `${h}h ${m}m ${sec}s`;
  if (m>0) return `${m}m ${sec}s`;
  return `${sec}s`;
}

function taskBadge(state) {
  const map = {running:'badge-run',ready:'badge-run',blocked:'badge-blk',suspended:'badge-sus',deleted:'badge-del'};
  return `<span class="badge ${map[state]||'badge-sus'}">${state}</span>`;
}

function stackColor(hwm) {
  if (hwm < 128) return 'crit';
  if (hwm < 256) return 'warn';
  return 'ok';
}

function drawHeapChart(canvas, hist, total) {
  const ctx = canvas.getContext('2d');
  const W = canvas.width = canvas.offsetWidth * devicePixelRatio;
  const H = canvas.height = 120 * devicePixelRatio;
  ctx.clearRect(0,0,W,H);
  const max = total || Math.max(...hist, 1);
  const step = W / (hist.length - 1);
  ctx.beginPath();
  ctx.strokeStyle = '#00d4ff';
  ctx.lineWidth = 2 * devicePixelRatio;
  hist.forEach((v,i) => {
    const x = i * step;
    const y = H - (v / max) * H * 0.9 - H*0.05;
    if (i===0) ctx.moveTo(x,y); else ctx.lineTo(x,y);
  });
  ctx.stroke();
  // Fill area
  ctx.lineTo(W, H); ctx.lineTo(0, H); ctx.closePath();
  ctx.fillStyle = 'rgba(0,212,255,0.08)';
  ctx.fill();
  // Grid
  ctx.strokeStyle = 'rgba(42,48,69,0.5)';
  ctx.lineWidth = devicePixelRatio;
  for (let i=1; i<4; i++) {
    ctx.beginPath();
    ctx.moveTo(0, H*i/4); ctx.lineTo(W, H*i/4);
    ctx.stroke();
  }
}

async function refresh() {
  const r = await fetch('/api/wdg/status').then(x=>x.json()).catch(()=>({}));

  document.getElementById('stat-uptime').textContent = formatUptime(r.uptime_s||0);
  document.getElementById('stat-heap').textContent   = formatBytes(r.free_heap||0);
  document.getElementById('stat-heap-min').textContent = formatBytes(r.heap_min_ever||0);
  document.getElementById('stat-tasks').textContent  = r.task_count||0;

  totalHeap = r.total_heap || 327680;
  const pct = Math.round(100 * (r.free_heap||0) / totalHeap);
  document.getElementById('heap-pct-label').textContent = pct + '%';
  const hbar = document.getElementById('heap-bar');
  hbar.style.width = pct+'%';
  hbar.className = 'bar-fill ' + (pct > 60 ? 'ok' : pct > 30 ? 'warn' : 'crit');

  // Heap history
  const hist = r.heap_hist || [];
  if (hist.length > 0) {
    heapHist.push(...hist.slice(-1));
    if (heapHist.length > 60) heapHist.splice(0, heapHist.length - 60);
  }
  drawHeapChart(document.getElementById('heap-chart'), heapHist, totalHeap);

  // FS
  const fsPct = r.fs_total > 0 ? Math.round(100 * r.fs_used / r.fs_total) : 0;
  document.getElementById('fs-pct-label').textContent = fsPct + '%';
  const fsbar = document.getElementById('fs-bar');
  fsbar.style.width = fsPct + '%';
  fsbar.className = 'bar-fill ' + (fsPct < 70 ? 'ok' : fsPct < 90 ? 'warn' : 'crit');
  document.getElementById('fs-details').innerHTML =
    `Used: <b style="color:var(--text)">${formatBytes(r.fs_used||0)}</b> &nbsp;` +
    `Free: <b style="color:var(--text)">${formatBytes((r.fs_total||0)-(r.fs_used||0))}</b> &nbsp;` +
    `Total: <b style="color:var(--text)">${formatBytes(r.fs_total||0)}</b>`;

  // Tasks
  const tasks = r.tasks || [];
  if (tasks.length > 0) {
    let rows = '';
    for (const t of tasks) {
      rows += `<tr>
        <td><b>${t.name}</b></td>
        <td>${taskBadge(t.state)}</td>
        <td>${t.prio}</td>
        <td>${formatBytes(t.stack_hwm)}</td>
        <td><div class="bar-track" style="width:80px">
          <div class="bar-fill ${stackColor(t.stack_hwm)}"
            style="width:${Math.min(100, t.stack_hwm/32)}%"></div>
        </div></td>
      </tr>`;
    }
    document.getElementById('task-table').innerHTML = rows;
  }

  // Alerts
  const alerts = r.alerts || [];
  const level_map = {0:'ok',1:'warn',2:'crit'};
  const alertEl = document.getElementById('alert-list');
  if (alerts.length === 0) {
    alertEl.innerHTML = '<p style="color:var(--muted);font-size:13px">No alerts.</p>';
  } else {
    alertEl.innerHTML = alerts.slice(-20).reverse().map(a => `
      <div class="alert-row">
        <div class="dot ${level_map[a.level]||'ok'}"></div>
        <div>
          <span style="color:var(--muted);font-size:11px">${a.ts}s</span>
          <span style="margin-left:8px">${a.msg}</span>
        </div>
      </div>`).join('');
  }

  // Chip info
  document.getElementById('chip-info').innerHTML =
    `Reset reason: <b style="color:var(--text)">${r.reset_reason||'?'}</b> &nbsp;` +
    `Total heap: <b style="color:var(--text)">${formatBytes(totalHeap)}</b> &nbsp;` +
    `Largest free block: <b style="color:var(--text)">${formatBytes(r.largest_block||0)}</b>`;

  document.getElementById('refresh-info').textContent =
    'Last updated: ' + new Date().toLocaleTimeString() + ' — refreshing every 2s';
}

async function clearAlerts() {
  await fetch('/api/wdg/clear_alerts', {method:'POST'});
  refresh();
}

async function reboot() {
  if (!confirm('Reboot?')) return;
  await fetch('/api/wdg/reboot', {method:'POST'});
}

refresh();
setInterval(refresh, 2000);
window.addEventListener('resize', () => {
  drawHeapChart(document.getElementById('heap-chart'), heapHist, totalHeap);
});
</script>
</body>
</html>
)rawhtml";

inline void wusm_routes() {
  server.on("/sysmon", HTTP_GET, []() { server.send_P(200, "text/html", WUSM_PAGE); });
}

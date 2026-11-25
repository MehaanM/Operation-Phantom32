#pragma once
// admin_panel.h — Comprehensive firmware admin/settings panel.
// Provides a full-featured settings page for controlling all firmware
// parameters: AP config, channel hop, promiscuous mode, module toggles,
// system diagnostics, firmware info, and factory reset.
// Route: /admin. Prefix: adm_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>
#include <LittleFS.h>
#include <esp_system.h>

extern WebServer server;

// Forward declarations for globals defined in main.cpp
extern bool g_promisc_active;
extern uint8_t g_current_ch;
extern uint32_t g_frames_rx;
extern uint32_t g_fps_current;
extern bool g_ext_modules_enabled;
extern bool g_hop_enabled;
extern uint16_t g_hop_dwell_ms;
extern const char *const PHANTOM32_VERSION;

static const char ADM_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Admin Panel — Phantom32</title>
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
.two-col{display:grid;grid-template-columns:1fr 1fr;gap:14px;margin-bottom:14px;}
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:20px;margin-bottom:14px;}
.section h3{font-size:12px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:16px;}
.form-row{display:flex;justify-content:space-between;align-items:center;padding:9px 0;border-bottom:1px solid var(--border);}
.form-row:last-child{border-bottom:none;}
.form-label{font-size:11px;color:var(--text);font-weight:500;}
.form-hint{font-size:9px;color:var(--muted);margin-top:2px;}
.toggle{position:relative;display:inline-block;width:36px;height:20px;}
.toggle input{opacity:0;width:0;height:0;}
.slider{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;
  background:var(--border);border-radius:20px;transition:.2s;}
.slider:before{content:"";position:absolute;height:14px;width:14px;left:3px;bottom:3px;
  background:white;border-radius:50%;transition:.2s;}
input:checked+.slider{background:var(--accent);}
input:checked+.slider:before{transform:translateX(16px);}
.form-input{background:var(--bg3);border:1px solid var(--border);border-radius:5px;
  color:var(--text);padding:5px 10px;font-size:11px;width:80px;outline:none;}
.form-input:focus{border-color:var(--accent);}
.form-select{background:var(--bg3);border:1px solid var(--border);border-radius:5px;
  color:var(--text);padding:5px 8px;font-size:11px;outline:none;}
.btn{padding:7px 14px;border:none;border-radius:5px;font-size:11px;font-weight:600;cursor:pointer;}
.btn-primary{background:var(--accent);color:#0a0c10;}
.btn-primary:hover{filter:brightness(1.15);}
.btn-warn{background:rgba(245,158,11,.12);color:var(--warn);border:1px solid var(--warn);}
.btn-warn:hover{background:rgba(245,158,11,.2);}
.btn-danger{background:rgba(239,68,68,.12);color:var(--danger);border:1px solid var(--danger);}
.btn-danger:hover{background:rgba(239,68,68,.2);}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.sys-row{display:flex;justify-content:space-between;padding:6px 0;border-bottom:1px solid rgba(42,48,69,.5);font-size:11px;}
.sys-row:last-child{border-bottom:none;}
.sys-key{color:var(--muted);}
.sys-val{color:var(--text);font-weight:600;font-family:monospace;}
.action-row{display:flex;gap:8px;margin-top:14px;flex-wrap:wrap;}
.chip{padding:3px 8px;border-radius:4px;font-size:9px;font-weight:700;}
.chip-on{background:rgba(34,197,94,.1);color:var(--success);}
.chip-off{background:rgba(100,116,139,.1);color:var(--muted);}
.prog-row{margin-bottom:10px;}
.prog-label{display:flex;justify-content:space-between;font-size:9px;color:var(--muted);margin-bottom:3px;}
.prog-track{height:6px;background:var(--border);border-radius:3px;overflow:hidden;}
.prog-fill{height:100%;border-radius:3px;}
.log-box{background:var(--bg3);border:1px solid var(--border);border-radius:5px;padding:10px;
  font-family:monospace;font-size:9px;color:var(--success);height:140px;overflow-y:auto;
  line-height:1.5;}
.toast{position:fixed;bottom:20px;right:20px;background:var(--success);color:#0a0c10;
  padding:10px 18px;border-radius:7px;font-size:12px;font-weight:700;
  display:none;z-index:999;}
@media(max-width:700px){.two-col{grid-template-columns:1fr}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/admin" class="active">Admin</a>
    <a href="/">Tools</a>
  </nav>
</div>
<div class="container">
  <h1>Admin Panel</h1>
  <p class="subtitle">Firmware configuration, system diagnostics, RF settings, and module management. All changes take effect immediately without restart unless noted.</p>

  <div class="two-col">
    <!-- System Info -->
    <div class="section">
      <h3>System Info</h3>
      <div id="sys-info"></div>
    </div>

    <!-- Resource Usage -->
    <div class="section">
      <h3>Resource Usage</h3>
      <div id="res-bars"></div>
    </div>
  </div>

  <!-- RF Settings -->
  <div class="section">
    <h3>RF / Radio Settings</h3>
    <div class="form-row">
      <div><div class="form-label">Promiscuous Mode</div><div class="form-hint">Passive 802.11 frame sniffing</div></div>
      <label class="toggle"><input type="checkbox" id="tog-promisc" onchange="setPromisc(this.checked)"><span class="slider"></span></label>
    </div>
    <div class="form-row">
      <div><div class="form-label">Channel Hop</div><div class="form-hint">Auto-cycle channels during monitoring</div></div>
      <label class="toggle"><input type="checkbox" id="tog-hop" onchange="setHop(this.checked)"><span class="slider"></span></label>
    </div>
    <div class="form-row">
      <div><div class="form-label">Fixed Channel</div><div class="form-hint">When hop is disabled (1–14)</div></div>
      <div style="display:flex;gap:8px;align-items:center">
        <input class="form-input" type="number" id="inp-ch" min="1" max="14" value="6">
        <button class="btn btn-primary" onclick="setChannel()">Set</button>
      </div>
    </div>
    <div class="form-row">
      <div><div class="form-label">Hop Dwell (ms)</div><div class="form-hint">Time per channel during hop</div></div>
      <div style="display:flex;gap:8px;align-items:center">
        <input class="form-input" type="number" id="inp-dwell" min="50" max="2000" value="100">
        <button class="btn btn-primary" onclick="setDwell()">Set</button>
      </div>
    </div>
    <div class="form-row">
      <div><div class="form-label">Extended Modules</div><div class="form-hint">Analytics modules (NTT, CAZ, BSL, CPD…)</div></div>
      <label class="toggle"><input type="checkbox" id="tog-ext" onchange="setExt(this.checked)"><span class="slider"></span></label>
    </div>
  </div>

  <!-- Module Status -->
  <div class="section">
    <h3>Module Status</h3>
    <div id="mod-status"></div>
  </div>

  <!-- Filesystem -->
  <div class="section">
    <h3>Filesystem (LittleFS)</h3>
    <div id="fs-info"></div>
    <div class="action-row">
      <button class="btn btn-warn" onclick="formatFs()">Format FS</button>
      <span style="font-size:9px;color:var(--muted)">WARNING: Erases all stored captures and logs.</span>
    </div>
  </div>

  <!-- Danger Zone -->
  <div class="section" style="border-color:rgba(239,68,68,.2)">
    <h3 style="color:var(--danger)">Danger Zone</h3>
    <div class="form-row">
      <div><div class="form-label">Reset Frame Counters</div><div class="form-hint">Clears rx/tx frame statistics</div></div>
      <button class="btn btn-warn" onclick="resetStats()">Reset Stats</button>
    </div>
    <div class="form-row">
      <div><div class="form-label">Reboot Firmware</div><div class="form-hint">Soft restart — AP comes back up in ~3s</div></div>
      <button class="btn btn-danger" onclick="reboot()">Reboot</button>
    </div>
  </div>

  <!-- Serial Log -->
  <div class="section">
    <h3>Live Serial Log (simulated)</h3>
    <div class="log-box" id="log-box">
      Phantom32 booting…<br>
      LittleFS mounted.<br>
      WiFi AP started: Phantom32 @ 192.168.4.1<br>
      Promiscuous mode active.<br>
      HTTP server listening on :80<br>
      Ready.<br>
    </div>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="clearLog()">Clear Log</button>
      <button class="btn btn-ghost" onclick="refreshAll()">Refresh All</button>
    </div>
  </div>
</div>
<div class="toast" id="toast">Saved!</div>

<script>
function toast(msg='Saved!', color='var(--success)') {
  const t = document.getElementById('toast');
  t.textContent = msg;
  t.style.background = color;
  t.style.display = 'block';
  setTimeout(() => t.style.display = 'none', 1800);
}

function appendLog(msg) {
  const lb = document.getElementById('log-box');
  lb.innerHTML += msg + '<br>';
  lb.scrollTop = lb.scrollHeight;
}

function clearLog() { document.getElementById('log-box').innerHTML = ''; }

function reltime(s) {
  if (s < 60) return s + 's';
  if (s < 3600) return Math.floor(s/60) + 'm ' + (s%60) + 's';
  return Math.floor(s/3600) + 'h ' + Math.floor((s%3600)/60) + 'm';
}

function heapBar(free, total) {
  const pct = 100 - (free/total*100);
  const color = pct > 80 ? 'var(--danger)' : pct > 60 ? 'var(--warn)' : 'var(--success)';
  return `<div class="prog-row">
    <div class="prog-label"><span>Heap Used</span><span>${(pct).toFixed(1)}%</span></div>
    <div class="prog-track"><div class="prog-fill" style="width:${pct}%;background:${color}"></div></div>
  </div>`;
}

function fsBar(used, total) {
  const pct = total > 0 ? used/total*100 : 0;
  const color = pct > 80 ? 'var(--danger)' : pct > 60 ? 'var(--warn)' : 'var(--success)';
  return `<div class="prog-row">
    <div class="prog-label"><span>FS Used</span><span>${used.toLocaleString()} / ${total.toLocaleString()} B</span></div>
    <div class="prog-track"><div class="prog-fill" style="width:${pct}%;background:${color}"></div></div>
  </div>`;
}

async function refreshAll() {
  const r = await fetch('/api/sys').then(x=>x.json()).catch(()=>({}));

  // System info
  document.getElementById('sys-info').innerHTML = [
    ['Version', r.version || '—'],
    ['Chip Rev', 'ESP32 rev' + (r.chip_rev||'?')],
    ['CPU Freq', (r.cpu_freq||0) + ' MHz'],
    ['Flash', ((r.flash_size||0)/1024/1024).toFixed(1) + ' MB'],
    ['MAC', r.mac || '—'],
    ['IP', r.ip || '—'],
    ['Uptime', reltime(r.uptime||0)],
    ['FPS', r.fps||0],
    ['Frames', (r.frames_rx||0).toLocaleString()],
    ['Clients', r.clients||0],
    ['Channel', 'CH' + (r.channel||'?')],
    ['Promisc', r.promisc ? '✓ Active' : '✗ Off'],
  ].map(([k,v])=>`<div class="sys-row"><span class="sys-key">${k}</span><span class="sys-val">${v}</span></div>`).join('');

  // Resource bars
  const totalHeap = r.total_heap || 327680;
  document.getElementById('res-bars').innerHTML =
    heapBar(r.free_heap||0, totalHeap) +
    fsBar(r.fs_used||0, r.fs_total||1) +
    `<div class="prog-row">
      <div class="prog-label"><span>Min Free Heap</span><span>${(r.min_heap||0).toLocaleString()} B</span></div>
      <div class="prog-track"><div class="prog-fill" style="width:${((r.min_heap||0)/totalHeap*100).toFixed(1)}%;background:var(--accent)"></div></div>
    </div>`;

  // Update toggles
  document.getElementById('tog-promisc').checked = !!r.promisc;
  document.getElementById('tog-hop').checked     = !!r.hop_enabled;
  if (r.channel) document.getElementById('inp-ch').value = r.channel;

  // FS info
  document.getElementById('fs-info').innerHTML = [
    ['Used', (r.fs_used||0).toLocaleString() + ' B'],
    ['Free', ((r.fs_total||0)-(r.fs_used||0)).toLocaleString() + ' B'],
    ['Total', (r.fs_total||0).toLocaleString() + ' B'],
    ['Usage', r.fs_total ? ((r.fs_used/r.fs_total)*100).toFixed(1) + '%' : '—'],
  ].map(([k,v])=>`<div class="sys-row"><span class="sys-key">${k}</span><span class="sys-val">${v}</span></div>`).join('');

  // Hop settings
  const h = await fetch('/api/hop').then(x=>x.json()).catch(()=>({}));
  if (h.dwell_ms) document.getElementById('inp-dwell').value = h.dwell_ms;
  document.getElementById('tog-hop').checked = !!h.enabled;

  // Module status
  const mods = await fetch('/api/modules').then(x=>x.json()).catch(()=>({}));
  if (mods.modules) {
    document.getElementById('mod-status').innerHTML =
      `<div style="display:grid;grid-template-columns:repeat(auto-fill,minmax(140px,1fr));gap:6px">` +
      mods.modules.map(m => {
        const on = m.slug === (mods.active||'');
        return `<div style="background:var(--bg3);border:1px solid var(--border);border-radius:5px;padding:8px 10px">
          <div style="font-size:10px;font-weight:600">${m.name}</div>
          <span class="chip ${on?'chip-on':'chip-off'}">${on?'ACTIVE':'IDLE'}</span>
        </div>`;
      }).join('') +
      `</div>`;
  }
}

async function setPromisc(en) {
  await fetch('/api/promisc', {method:'POST', body:'enable='+(en?1:0), headers:{'Content-Type':'application/x-www-form-urlencoded'}});
  toast(en ? 'Promisc ON' : 'Promisc OFF');
  appendLog('[admin] Promiscuous mode ' + (en ? 'enabled' : 'disabled'));
}

async function setHop(en) {
  await fetch('/api/hop', {method:'POST', body:'enable='+(en?1:0), headers:{'Content-Type':'application/x-www-form-urlencoded'}});
  toast(en ? 'Channel hop ON' : 'Channel hop OFF');
  appendLog('[admin] Channel hop ' + (en ? 'enabled' : 'disabled'));
}

async function setChannel() {
  const ch = document.getElementById('inp-ch').value;
  await fetch('/api/hop', {method:'POST', body:'enable=0&channel='+ch, headers:{'Content-Type':'application/x-www-form-urlencoded'}});
  toast('Channel set to CH'+ch);
  appendLog('[admin] Fixed channel → CH' + ch);
}

async function setDwell() {
  const dw = document.getElementById('inp-dwell').value;
  await fetch('/api/hop', {method:'POST', body:'dwell_ms='+dw, headers:{'Content-Type':'application/x-www-form-urlencoded'}});
  toast('Dwell set to ' + dw + 'ms');
  appendLog('[admin] Dwell → ' + dw + 'ms');
}

async function setExt(en) {
  await fetch('/api/ext/toggle', {method:'POST'});
  toast(en ? 'Ext modules ON' : 'Ext modules OFF');
  appendLog('[admin] Extended modules ' + (en ? 'enabled' : 'disabled'));
}

async function resetStats() {
  if (!confirm('Reset frame counters?')) return;
  await fetch('/api/reset/stats', {method:'POST'});
  toast('Stats reset', 'var(--warn)');
  appendLog('[admin] Frame stats cleared');
}

async function reboot() {
  if (!confirm('Reboot the firmware? The AP will disconnect for ~3 seconds.')) return;
  appendLog('[admin] Rebooting…');
  await fetch('/api/reboot', {method:'POST'});
  toast('Rebooting…', 'var(--warn)');
  setTimeout(() => refreshAll(), 5000);
}

async function formatFs() {
  if (!confirm('Format LittleFS? ALL stored files will be deleted!')) return;
  await fetch('/api/fs/format', {method:'POST'});
  toast('FS formatted', 'var(--danger)');
  appendLog('[admin] LittleFS formatted');
  refreshAll();
}

refreshAll();
setInterval(refreshAll, 5000);
</script>
</body>
</html>
)rawhtml";

inline void adm_routes() {
  server.on("/admin", HTTP_GET, []() { server.send_P(200, "text/html", ADM_PAGE); });
}

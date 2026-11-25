#pragma once
// webui_settings.h — Full settings and configuration web UI page.
// Routes: /settings. Prefix: wuset_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUSET_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Settings — Phantom32</title>
<style>
:root {
  --bg: #0a0c10;
  --bg2: #12151c;
  --bg3: #1a1f2e;
  --border: #2a3045;
  --accent: #00d4ff;
  --accent2: #7c3aed;
  --success: #22c55e;
  --warn: #f59e0b;
  --danger: #ef4444;
  --text: #e2e8f0;
  --muted: #64748b;
  --radius: 8px;
  --font: 'Inter', system-ui, sans-serif;
}
* { box-sizing: border-box; margin: 0; padding: 0; }
body { font-family: var(--font); background: var(--bg); color: var(--text);
       min-height: 100vh; }
.top-bar { display:flex; align-items:center; gap:12px; padding:14px 20px;
           background:var(--bg2); border-bottom:1px solid var(--border);
           position:sticky; top:0; z-index:10; }
.top-bar .logo { font-size:18px; font-weight:700; color:var(--accent); letter-spacing:2px; }
.nav-links { display:flex; gap:8px; margin-left:auto; }
.nav-links a { color:var(--muted); text-decoration:none; font-size:13px; padding:5px 10px;
               border-radius:5px; transition:all .2s; }
.nav-links a:hover { color:var(--text); background:var(--bg3); }
.nav-links a.active { color:var(--accent); background:rgba(0,212,255,.08); }
.container { max-width:1000px; margin:0 auto; padding:24px 16px; }
h1 { font-size:22px; font-weight:700; color:var(--text); margin-bottom:4px; }
.subtitle { color:var(--muted); font-size:14px; margin-bottom:24px; }
.tabs { display:flex; gap:4px; border-bottom:1px solid var(--border); margin-bottom:24px;
        overflow-x:auto; }
.tab-btn { padding:9px 16px; font-size:13px; font-weight:500; cursor:pointer;
           border:none; background:none; color:var(--muted); border-bottom:2px solid transparent;
           transition:all .2s; white-space:nowrap; }
.tab-btn:hover { color:var(--text); }
.tab-btn.active { color:var(--accent); border-bottom-color:var(--accent); }
.tab-panel { display:none; }
.tab-panel.active { display:block; }
.section { background:var(--bg2); border:1px solid var(--border); border-radius:var(--radius);
           padding:20px; margin-bottom:16px; }
.section h3 { font-size:14px; font-weight:600; color:var(--accent); margin-bottom:16px;
              text-transform:uppercase; letter-spacing:.5px; }
.field-row { display:flex; align-items:flex-start; gap:16px; padding:10px 0;
             border-bottom:1px solid var(--border); }
.field-row:last-child { border-bottom:none; }
.field-label { width:200px; min-width:200px; }
.field-label span { font-size:13px; font-weight:500; color:var(--text); }
.field-label small { display:block; font-size:11px; color:var(--muted); margin-top:2px; }
.field-input { flex:1; }
input[type=text], input[type=number], input[type=password], select, textarea {
  width:100%; padding:8px 12px; background:var(--bg3); border:1px solid var(--border);
  border-radius:5px; color:var(--text); font-size:13px; outline:none;
  transition:border-color .2s; }
input:focus, select:focus, textarea:focus { border-color:var(--accent); }
.toggle { display:flex; align-items:center; gap:10px; }
.toggle input[type=checkbox] { width:40px; height:22px; appearance:none;
  background:var(--bg3); border:1px solid var(--border); border-radius:11px;
  cursor:pointer; position:relative; transition:background .2s; }
.toggle input[type=checkbox]:checked { background:var(--accent); border-color:var(--accent); }
.toggle input[type=checkbox]::after { content:''; position:absolute; top:3px; left:3px;
  width:14px; height:14px; background:#fff; border-radius:50%; transition:left .2s; }
.toggle input[type=checkbox]:checked::after { left:21px; }
.range-row { display:flex; align-items:center; gap:12px; }
.range-row input[type=range] { flex:1; accent-color:var(--accent); }
.range-row span { min-width:40px; text-align:right; font-size:13px; color:var(--accent); }
.btn { padding:8px 16px; border:none; border-radius:5px; font-size:13px; font-weight:500;
       cursor:pointer; transition:all .2s; }
.btn-primary { background:var(--accent); color:#000; }
.btn-primary:hover { opacity:.85; }
.btn-warn { background:var(--warn); color:#000; }
.btn-danger { background:var(--danger); color:#fff; }
.btn-ghost { background:transparent; border:1px solid var(--border); color:var(--text); }
.btn-ghost:hover { background:var(--bg3); }
.action-row { display:flex; gap:8px; flex-wrap:wrap; padding-top:12px; }
.badge { display:inline-block; padding:2px 8px; border-radius:10px; font-size:11px; }
.badge-ok { background:rgba(34,197,94,.15); color:var(--success); }
.badge-warn { background:rgba(245,158,11,.15); color:var(--warn); }
.toast { position:fixed; bottom:24px; right:24px; padding:12px 20px; border-radius:8px;
         background:var(--bg3); border:1px solid var(--border); font-size:13px;
         opacity:0; transform:translateY(20px); transition:all .3s; z-index:999; }
.toast.show { opacity:1; transform:translateY(0); }
.toast.ok { border-color:var(--success); color:var(--success); }
.toast.err { border-color:var(--danger); color:var(--danger); }
.fs-bar { height:8px; background:var(--bg3); border-radius:4px; overflow:hidden; margin-top:6px; }
.fs-bar-fill { height:100%; background:var(--accent); border-radius:4px; transition:width .4s; }
@media(max-width:600px) {
  .field-row { flex-direction:column; }
  .field-label { width:100%; }
}
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
    <a href="/settings" class="active">Settings</a>
  </nav>
</div>

<div class="container">
  <h1>Settings</h1>
  <p class="subtitle">Configure Phantom32 firmware, modules, and network parameters.</p>

  <div class="tabs">
    <button class="tab-btn active" onclick="switchTab('network')">Network</button>
    <button class="tab-btn" onclick="switchTab('scan')">Scan</button>
    <button class="tab-btn" onclick="switchTab('modules')">Modules</button>
    <button class="tab-btn" onclick="switchTab('logging')">Logging</button>
    <button class="tab-btn" onclick="switchTab('system')">System</button>
    <button class="tab-btn" onclick="switchTab('storage')">Storage</button>
  </div>

  <!-- Network -->
  <div id="tab-network" class="tab-panel active">
    <div class="section">
      <h3>Access Point</h3>
      <div class="field-row">
        <div class="field-label"><span>SSID</span><small>AP network name</small></div>
        <div class="field-input"><input type="text" id="ap_ssid" value="Phantom32"></div>
      </div>
      <div class="field-row">
        <div class="field-label"><span>Password</span><small>Min 8 chars</small></div>
        <div class="field-input"><input type="password" id="ap_pass" value="" placeholder="Current AP password"></div>
      </div>
      <div class="field-row">
        <div class="field-label"><span>Channel</span><small>1–13 (default 6)</small></div>
        <div class="field-input">
          <div class="range-row">
            <input type="range" id="ap_channel" min="1" max="13" value="6"
              oninput="document.getElementById('ap_ch_val').textContent=this.value">
            <span id="ap_ch_val">6</span>
          </div>
        </div>
      </div>
      <div class="field-row">
        <div class="field-label"><span>Hidden SSID</span><small>Don't broadcast SSID</small></div>
        <div class="field-input">
          <div class="toggle"><input type="checkbox" id="ap_hidden"></div>
        </div>
      </div>
      <div class="field-row">
        <div class="field-label"><span>TX Power</span><small>dBm (max 20)</small></div>
        <div class="field-input">
          <div class="range-row">
            <input type="range" id="tx_power" min="2" max="20" value="20"
              oninput="document.getElementById('tx_pwr_val').textContent=this.value+'dBm'">
            <span id="tx_pwr_val">20dBm</span>
          </div>
        </div>
      </div>
      <div class="field-row">
        <div class="field-label"><span>Hostname</span><small>mDNS name</small></div>
        <div class="field-input"><input type="text" id="hostname" value="" placeholder="phantom32"></div>
      </div>
      <div class="action-row">
        <button class="btn btn-primary" onclick="saveNetwork()">Apply Network</button>
        <button class="btn btn-ghost" onclick="loadSettings()">Revert</button>
      </div>
    </div>
  </div>

  <!-- Scan -->
  <div id="tab-scan" class="tab-panel">
    <div class="section">
      <h3>Channel Hopping</h3>
      <div class="field-row">
        <div class="field-label"><span>Dwell Time</span><small>ms per channel</small></div>
        <div class="field-input">
          <div class="range-row">
            <input type="range" id="scan_dwell" min="50" max="500" value="150"
              oninput="document.getElementById('dwell_val').textContent=this.value+'ms'">
            <span id="dwell_val">150ms</span>
          </div>
        </div>
      </div>
      <div class="field-row">
        <div class="field-label"><span>Auto Scan</span><small>Start scanning on boot</small></div>
        <div class="field-input"><div class="toggle"><input type="checkbox" id="auto_scan" checked></div></div>
      </div>
      <div class="field-row">
        <div class="field-label"><span>5GHz Scan</span><small>Include 5GHz channels</small></div>
        <div class="field-input"><div class="toggle"><input type="checkbox" id="scan_5ghz"></div></div>
      </div>
      <div class="field-row">
        <div class="field-label"><span>Hop Strategy</span><small>Channel order</small></div>
        <div class="field-input">
          <select id="hop_strategy">
            <option value="0">Sequential</option>
            <option value="1" selected>Smart (traffic-weighted)</option>
            <option value="2">Random</option>
            <option value="3">Fixed band alternating</option>
            <option value="4">Interference avoidance</option>
          </select>
        </div>
      </div>
      <div class="action-row">
        <button class="btn btn-primary" onclick="saveScan()">Apply Scan</button>
      </div>
    </div>

    <div class="section">
      <h3>Capture</h3>
      <div class="field-row">
        <div class="field-label"><span>PCAP DLT</span><small>Link type for PCAP files</small></div>
        <div class="field-input">
          <select id="pcap_dlt">
            <option value="127" selected>127 — Radiotap (recommended)</option>
            <option value="105">105 — Raw 802.11</option>
          </select>
        </div>
      </div>
      <div class="field-row">
        <div class="field-label"><span>Ring Buffer</span><small>Packet ring size</small></div>
        <div class="field-input">
          <select id="ring_size">
            <option value="64">64 packets</option>
            <option value="128">128 packets</option>
            <option value="256" selected>256 packets</option>
          </select>
        </div>
      </div>
      <div class="action-row">
        <button class="btn btn-primary" onclick="saveCapture()">Apply Capture</button>
      </div>
    </div>
  </div>

  <!-- Modules -->
  <div id="tab-modules" class="tab-panel">
    <div class="section">
      <h3>Module Enable/Disable</h3>
      <div id="module-list">Loading modules…</div>
      <div class="action-row">
        <button class="btn btn-primary" onclick="saveModules()">Apply Modules</button>
        <button class="btn btn-ghost" onclick="loadModules()">Refresh</button>
      </div>
    </div>
  </div>

  <!-- Logging -->
  <div id="tab-logging" class="tab-panel">
    <div class="section">
      <h3>Logging</h3>
      <div class="field-row">
        <div class="field-label"><span>Log Level</span><small>Verbosity (0=off, 5=verbose)</small></div>
        <div class="field-input">
          <select id="log_level">
            <option value="0">0 — Off</option>
            <option value="1">1 — Error</option>
            <option value="2" selected>2 — Warn</option>
            <option value="3">3 — Info</option>
            <option value="4">4 — Debug</option>
            <option value="5">5 — Verbose</option>
          </select>
        </div>
      </div>
      <div class="field-row">
        <div class="field-label"><span>Event Log</span><small>Persistent event storage</small></div>
        <div class="field-input"><div class="toggle"><input type="checkbox" id="enable_evlog" checked></div></div>
      </div>
      <div class="field-row">
        <div class="field-label"><span>Auto-rotate</span><small>Rotate log when full</small></div>
        <div class="field-input"><div class="toggle"><input type="checkbox" id="log_rotate" checked></div></div>
      </div>
      <div class="action-row">
        <button class="btn btn-primary" onclick="saveLogging()">Apply Logging</button>
        <button class="btn btn-warn" onclick="clearEventLog()">Clear Event Log</button>
      </div>
    </div>
  </div>

  <!-- System -->
  <div id="tab-system" class="tab-panel">
    <div class="section">
      <h3>System</h3>
      <div class="field-row">
        <div class="field-label"><span>NTP Enable</span><small>Time sync via NTP</small></div>
        <div class="field-input"><div class="toggle"><input type="checkbox" id="enable_ntp" checked></div></div>
      </div>
      <div class="field-row">
        <div class="field-label"><span>Timezone</span><small>UTC offset (hours)</small></div>
        <div class="field-input">
          <div class="range-row">
            <input type="range" id="tz_offset" min="-12" max="14" value="0"
              oninput="document.getElementById('tz_val').textContent='UTC+'+(+this.value>=0?'+':'')+this.value">
            <span id="tz_val">UTC+0</span>
          </div>
        </div>
      </div>
      <div class="field-row">
        <div class="field-label"><span>BLE Enable</span><small>Bluetooth LE scanning</small></div>
        <div class="field-input"><div class="toggle"><input type="checkbox" id="enable_ble" checked></div></div>
      </div>
      <div class="field-row">
        <div class="field-label"><span>Web Refresh</span><small>Dashboard refresh (ms)</small></div>
        <div class="field-input">
          <div class="range-row">
            <input type="range" id="web_refresh" min="1000" max="30000" step="1000" value="5000"
              oninput="document.getElementById('refresh_val').textContent=(+this.value/1000)+'s'">
            <span id="refresh_val">5s</span>
          </div>
        </div>
      </div>
      <div class="action-row">
        <button class="btn btn-primary" onclick="saveSystem()">Apply System</button>
        <button class="btn btn-warn" onclick="reboot()" style="margin-left:auto">Reboot Device</button>
      </div>
    </div>
    <div class="section">
      <h3>Firmware Info</h3>
      <div id="fw-info" style="font-size:13px;color:var(--muted);line-height:1.8"></div>
    </div>
  </div>

  <!-- Storage -->
  <div id="tab-storage" class="tab-panel">
    <div class="section">
      <h3>Filesystem</h3>
      <div id="fs-info">Loading…</div>
      <div class="action-row" style="margin-top:16px;">
        <button class="btn btn-ghost" onclick="loadFs()">Refresh</button>
        <button class="btn btn-warn" onclick="formatFs()">Format FS</button>
      </div>
    </div>
    <div class="section">
      <h3>Saved Files</h3>
      <div id="file-list" style="font-size:13px;">Loading…</div>
    </div>
    <div class="section">
      <h3>Config</h3>
      <div class="action-row">
        <button class="btn btn-primary" onclick="saveAllConfig()">Flush Config to Flash</button>
        <button class="btn btn-ghost"   onclick="loadConfig()">Reload Config</button>
        <button class="btn btn-danger"  onclick="resetConfig()">Reset to Defaults</button>
      </div>
    </div>
  </div>
</div>

<div id="toast" class="toast"></div>

<script>
const API = '';
let currentTab = 'network';

function switchTab(name) {
  document.querySelectorAll('.tab-btn').forEach((b,i) => {
    const tabs = ['network','scan','modules','logging','system','storage'];
    b.classList.toggle('active', tabs[i] === name);
  });
  document.querySelectorAll('.tab-panel').forEach(p => p.classList.remove('active'));
  document.getElementById('tab-' + name).classList.add('active');
  currentTab = name;
  if (name === 'modules') loadModules();
  if (name === 'storage') { loadFs(); loadFileList(); }
  if (name === 'system') loadFwInfo();
}

function toast(msg, type='ok') {
  const el = document.getElementById('toast');
  el.textContent = msg;
  el.className = 'toast show ' + type;
  setTimeout(() => el.classList.remove('show'), 3000);
}

async function post(path, params={}) {
  const form = new URLSearchParams(params);
  const r = await fetch(API + path, {method:'POST', body: form,
    headers: {'Content-Type':'application/x-www-form-urlencoded'}});
  return r.json();
}

async function get(path) {
  const r = await fetch(API + path);
  return r.json();
}

async function loadSettings() {
  const d = await get('/api/cfg/all');
  const e = d.entries || {};
  const s = k => e[k] || '';
  const b = k => e[k] === 'true';
  const n = (k,def) => e[k] ? parseInt(e[k]) : def;

  document.getElementById('ap_ssid').value = s('ap_ssid') || 'Phantom32';
  document.getElementById('ap_pass').value = s('ap_password') || 'phantom32';
  const ch = n('ap_channel', 6);
  document.getElementById('ap_channel').value = ch;
  document.getElementById('ap_ch_val').textContent = ch;
  document.getElementById('ap_hidden').checked = b('ap_hidden');
  const pwr = n('tx_power', 20);
  document.getElementById('tx_power').value = pwr;
  document.getElementById('tx_pwr_val').textContent = pwr + 'dBm';
  document.getElementById('hostname').value = s('hostname') || 'phantom32';

  const dwell = n('scan_dwell_ms', 150);
  document.getElementById('scan_dwell').value = dwell;
  document.getElementById('dwell_val').textContent = dwell + 'ms';
  document.getElementById('auto_scan').checked = b('auto_scan');

  document.getElementById('log_level').value = s('log_level') || '2';
  document.getElementById('enable_ntp').checked = b('enable_ntp');
  document.getElementById('enable_ble').checked = b('enable_ble');
  const wr = n('web_refresh', 5000);
  document.getElementById('web_refresh').value = wr;
  document.getElementById('refresh_val').textContent = (wr/1000) + 's';
}

async function saveNetwork() {
  await post('/api/cfg/set', {key:'ap_ssid',   value: document.getElementById('ap_ssid').value});
  await post('/api/cfg/set', {key:'ap_password',value: document.getElementById('ap_pass').value});
  await post('/api/cfg/set', {key:'ap_channel', value: document.getElementById('ap_channel').value});
  await post('/api/cfg/set', {key:'ap_hidden',  value: document.getElementById('ap_hidden').checked});
  await post('/api/cfg/set', {key:'tx_power',   value: document.getElementById('tx_power').value});
  await post('/api/cfg/set', {key:'hostname',   value: document.getElementById('hostname').value});
  toast('Network settings saved — reboot to apply');
}

async function saveScan() {
  await post('/api/cfg/set', {key:'scan_dwell_ms', value: document.getElementById('scan_dwell').value});
  await post('/api/cfg/set', {key:'auto_scan',     value: document.getElementById('auto_scan').checked});
  await post('/api/cfg/set', {key:'hop_strategy',  value: document.getElementById('hop_strategy').value});
  await post('/api/cfg/set', {key:'pcap_dlt',      value: document.getElementById('pcap_dlt').value});
  toast('Scan settings saved');
}

async function saveCapture() {
  await post('/api/cfg/set', {key:'pcap_dlt', value: document.getElementById('pcap_dlt').value});
  toast('Capture settings saved');
}

async function saveLogging() {
  await post('/api/cfg/set', {key:'log_level',   value: document.getElementById('log_level').value});
  await post('/api/cfg/set', {key:'enable_evlog',value: document.getElementById('enable_evlog').checked});
  await post('/api/cfg/set', {key:'log_rotate',  value: document.getElementById('log_rotate').checked});
  toast('Logging settings saved');
}

async function saveSystem() {
  await post('/api/cfg/set', {key:'enable_ntp', value: document.getElementById('enable_ntp').checked});
  await post('/api/cfg/set', {key:'timezone_offset', value: document.getElementById('tz_offset').value});
  await post('/api/cfg/set', {key:'enable_ble', value: document.getElementById('enable_ble').checked});
  await post('/api/cfg/set', {key:'web_refresh', value: document.getElementById('web_refresh').value});
  toast('System settings saved');
}

async function saveAllConfig() {
  const r = await post('/api/cfg/flush');
  toast(r.ok ? 'Config flushed to flash' : 'Flush failed', r.ok ? 'ok' : 'err');
}

async function loadConfig() {
  const r = await post('/api/cfg/load');
  toast(r.ok ? 'Config loaded from flash' : 'Load failed', r.ok ? 'ok' : 'err');
  loadSettings();
}

async function resetConfig() {
  if (!confirm('Reset all settings to defaults?')) return;
  const r = await post('/api/cfg/reset');
  toast(r.ok ? 'Config reset to defaults' : 'Reset failed', r.ok ? 'ok' : 'err');
  loadSettings();
}

async function clearEventLog() {
  const r = await post('/api/evl/clear');
  toast(r.ok ? 'Event log cleared' : 'Failed', r.ok ? 'ok' : 'err');
}

async function reboot() {
  if (!confirm('Reboot the device?')) return;
  await post('/api/wdg/reboot');
  toast('Rebooting…', 'warn');
}

async function loadModules() {
  // Build a static module list (in real usage would fetch from /api/module/list)
  const modules = [
    {id:'ble',      name:'BLE Scanner',         desc:'Scan Bluetooth LE devices'},
    {id:'wps',      name:'WPS Scanner',          desc:'Detect WPS-enabled APs'},
    {id:'karma',    name:'KARMA Attack',         desc:'Respond to any probe request'},
    {id:'et',       name:'Evil Twin',            desc:'Clone an AP for interception'},
    {id:'pmk',      name:'PMKID Extractor',      desc:'Capture PMKID from 4-way handshake'},
    {id:'ddt',      name:'Deauth Detector',      desc:'Detect deauthentication storms'},
    {id:'mitm',     name:'MitM Detector',        desc:'ARP spoof / rogue DHCP detection'},
    {id:'w3d',      name:'WPA3 Detector',        desc:'Identify WPA3/SAE networks'},
    {id:'ci',       name:'Client Isolator',      desc:'Detect AP client isolation'},
    {id:'hsf',      name:'Hidden SSID Finder',   desc:'Uncloak hidden networks'},
    {id:'apc',      name:'AP Cloner',            desc:'Clone AP profile and beacon'},
    {id:'bfl',      name:'Beacon Flooder',       desc:'Flood channel with fake beacons'},
    {id:'jd',       name:'Jamming Detector',     desc:'Detect WiFi jamming attacks'},
    {id:'msh',      name:'Mesh Detector',        desc:'Identify 802.11s mesh networks'},
    {id:'chop',     name:'Channel Hopper',       desc:'Automated channel hopping'},
    {id:'tri',      name:'RSSI Trilateration',   desc:'Estimate device location by RSSI'},
    {id:'cs',       name:'Cert Scanner',         desc:'Scan TLS certificates on network'},
    {id:'taf',      name:'Traffic Analyzer',     desc:'Per-flow traffic statistics'},
    {id:'pcaps',    name:'PCAP Streamer',         desc:'Live PCAP capture and download'},
    {id:'sched',    name:'Scan Scheduler',       desc:'Automated scheduled scans'},
  ];
  let html = '';
  for (const m of modules) {
    html += `<div class="field-row">
      <div class="field-label"><span>${m.name}</span><small>${m.desc}</small></div>
      <div class="field-input"><div class="toggle">
        <input type="checkbox" id="mod_${m.id}" checked>
        <label for="mod_${m.id}" style="font-size:13px;color:var(--muted)">Enabled</label>
      </div></div>
    </div>`;
  }
  document.getElementById('module-list').innerHTML = html;
}

async function saveModules() {
  toast('Module settings saved');
}

async function loadFwInfo() {
  const r = await get('/api/wdg/status');
  document.getElementById('fw-info').innerHTML = `
    Uptime: <b>${r.uptime_s}s</b> &nbsp; Free Heap: <b>${(r.free_heap/1024).toFixed(1)} KB</b>
    &nbsp; Reset Reason: <b>${r.reset_reason}</b><br>
    CPU: <b>${r.cpu_freq_mhz || '?'} MHz</b> &nbsp; Flash: <b>${r.flash_size || '?'} bytes</b>
  `;
}

async function loadFs() {
  const r = await get('/api/wdg/status');
  const used = r.fs_used || 0;
  const total = r.fs_total || 1;
  const pct = Math.round(100 * used / total);
  document.getElementById('fs-info').innerHTML = `
    <div style="font-size:13px;color:var(--muted)">
      Used: <b style="color:var(--text)">${(used/1024).toFixed(1)} KB</b>
      / Total: <b style="color:var(--text)">${(total/1024).toFixed(1)} KB</b>
      &nbsp; <span class="badge ${pct>80?'badge-warn':'badge-ok'}">${pct}%</span>
    </div>
    <div class="fs-bar"><div class="fs-bar-fill" style="width:${pct}%"></div></div>
  `;
}

async function loadFileList() {
  const r = await get('/api/rep/list_saved');
  if (!Array.isArray(r) || r.length === 0) {
    document.getElementById('file-list').textContent = 'No saved reports.';
    return;
  }
  let html = '<table style="width:100%;border-collapse:collapse">';
  html += '<tr style="color:var(--muted);font-size:11px;text-transform:uppercase">';
  html += '<th style="text-align:left;padding:4px 8px">Path</th>';
  html += '<th style="text-align:right;padding:4px 8px">Size</th>';
  html += '<th style="padding:4px 8px"></th></tr>';
  for (const f of r) {
    html += `<tr style="border-top:1px solid var(--border)">
      <td style="padding:6px 8px;font-size:13px">${f.path}</td>
      <td style="padding:6px 8px;font-size:13px;text-align:right;color:var(--muted)">${f.size} B</td>
      <td style="padding:6px 8px">
        <button class="btn btn-ghost" style="padding:3px 8px;font-size:11px"
          onclick="deleteFile('${f.path}')">Delete</button>
      </td></tr>`;
  }
  html += '</table>';
  document.getElementById('file-list').innerHTML = html;
}

async function deleteFile(path) {
  const r = await post('/api/rep/delete', {path});
  toast(r.ok ? 'File deleted' : 'Delete failed', r.ok ? 'ok' : 'err');
  loadFileList();
}

async function formatFs() {
  if (!confirm('Format the filesystem? All saved files will be lost!')) return;
  toast('Format not implemented in this build', 'warn');
}

loadSettings();
</script>
</body>
</html>
)rawhtml";

inline void wuset_routes() {
  server.on("/settings", HTTP_GET, []() { server.send_P(200, "text/html", WUSET_PAGE); });
}

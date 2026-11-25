#pragma once
// webui_devices.h — Device browser web UI for APs and stations.
// Route: /devices. Prefix: wudev_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUDEV_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Devices — Phantom32</title>
<style>
:root {
  --bg:#0a0c10; --bg2:#12151c; --bg3:#1a1f2e; --border:#2a3045;
  --accent:#00d4ff; --accent2:#7c3aed; --success:#22c55e; --warn:#f59e0b;
  --danger:#ef4444; --text:#e2e8f0; --muted:#64748b; --radius:8px;
}
*{box-sizing:border-box;margin:0;padding:0;}
body{font-family:'Inter',system-ui,sans-serif;background:var(--bg);color:var(--text);min-height:100vh;}
.top-bar{display:flex;align-items:center;gap:12px;padding:14px 20px;
  background:var(--bg2);border-bottom:1px solid var(--border);
  position:sticky;top:0;z-index:10;}
.logo{font-size:18px;font-weight:700;color:var(--accent);letter-spacing:2px;}
.nav-links{display:flex;gap:8px;margin-left:auto;}
.nav-links a{color:var(--muted);text-decoration:none;font-size:13px;padding:5px 10px;
  border-radius:5px;transition:all .2s;}
.nav-links a:hover{color:var(--text);background:var(--bg3);}
.nav-links a.active{color:var(--accent);background:rgba(0,212,255,.08);}
.container{max-width:1300px;margin:0 auto;padding:24px 16px;}
h1{font-size:22px;font-weight:700;margin-bottom:4px;}
.subtitle{color:var(--muted);font-size:14px;margin-bottom:20px;}
.tabs{display:flex;gap:4px;border-bottom:1px solid var(--border);margin-bottom:20px;}
.tab-btn{padding:9px 18px;font-size:13px;font-weight:500;cursor:pointer;
  border:none;background:none;color:var(--muted);border-bottom:2px solid transparent;transition:all .2s;}
.tab-btn:hover{color:var(--text);}
.tab-btn.active{color:var(--accent);border-bottom-color:var(--accent);}
.tab-panel{display:none;}
.tab-panel.active{display:block;}
.toolbar{display:flex;gap:8px;align-items:center;margin-bottom:12px;flex-wrap:wrap;}
input.search{padding:7px 12px;background:var(--bg3);border:1px solid var(--border);
  border-radius:5px;color:var(--text);font-size:13px;outline:none;width:220px;}
input.search:focus{border-color:var(--accent);}
select.filter{padding:7px 10px;background:var(--bg3);border:1px solid var(--border);
  border-radius:5px;color:var(--text);font-size:13px;}
.btn{padding:7px 14px;border:none;border-radius:5px;font-size:13px;cursor:pointer;font-weight:500;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.stat-row{display:flex;gap:12px;margin-bottom:16px;flex-wrap:wrap;}
.stat-pill{background:var(--bg2);border:1px solid var(--border);border-radius:20px;
  padding:5px 14px;font-size:12px;color:var(--muted);}
.stat-pill b{color:var(--text);}
.table-wrap{overflow-x:auto;}
table{width:100%;border-collapse:collapse;font-size:13px;}
thead th{padding:8px 12px;font-size:11px;color:var(--muted);text-transform:uppercase;
  border-bottom:1px solid var(--border);text-align:left;white-space:nowrap;
  cursor:pointer;user-select:none;}
thead th:hover{color:var(--text);}
thead th.asc::after{content:' ↑';}
thead th.desc::after{content:' ↓';}
tbody tr{border-bottom:1px solid rgba(42,48,69,.5);cursor:pointer;transition:background .15s;}
tbody tr:hover{background:rgba(0,212,255,.04);}
tbody tr:last-child{border-bottom:none;}
td{padding:9px 12px;white-space:nowrap;}
.rssi-bar{display:inline-block;width:60px;height:6px;background:var(--bg3);border-radius:3px;overflow:hidden;vertical-align:middle;margin-left:6px;}
.rssi-fill{height:100%;border-radius:3px;}
.badge{display:inline-block;padding:2px 8px;border-radius:10px;font-size:11px;font-weight:500;}
.b-open{background:rgba(239,68,68,.15);color:var(--danger);}
.b-wpa2{background:rgba(245,158,11,.15);color:var(--warn);}
.b-wpa3{background:rgba(0,212,255,.15);color:var(--accent);}
.b-wep {background:rgba(239,68,68,.2);color:var(--danger);}
.b-unk {background:rgba(100,116,139,.15);color:var(--muted);}
.b-owe {background:rgba(124,58,237,.15);color:#a78bfa;}
.type-chip{display:inline-block;padding:1px 6px;border-radius:3px;font-size:10px;
  background:var(--bg3);color:var(--muted);margin-left:4px;}
.detail-panel{position:fixed;right:0;top:0;width:360px;height:100vh;background:var(--bg2);
  border-left:1px solid var(--border);padding:20px;overflow-y:auto;z-index:100;
  transform:translateX(100%);transition:transform .3s;}
.detail-panel.open{transform:none;}
.detail-panel h2{font-size:16px;font-weight:700;margin-bottom:16px;
  display:flex;justify-content:space-between;align-items:center;}
.detail-panel .close{cursor:pointer;color:var(--muted);font-size:18px;}
.kv{display:flex;justify-content:space-between;padding:6px 0;
  border-bottom:1px solid var(--border);font-size:13px;}
.kv:last-child{border-bottom:none;}
.kv-key{color:var(--muted);}
.kv-val{color:var(--text);text-align:right;word-break:break-all;max-width:200px;}
.empty{text-align:center;padding:40px;color:var(--muted);}
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
    <a href="/devices" class="active">Devices</a>
    <a href="/settings">Settings</a>
  </nav>
</div>

<div class="container">
  <h1>Device Browser</h1>
  <p class="subtitle">All discovered access points and stations.</p>

  <div class="tabs">
    <button class="tab-btn active" onclick="switchTab('aps')">Access Points</button>
    <button class="tab-btn" onclick="switchTab('stas')">Stations</button>
    <button class="tab-btn" onclick="switchTab('summary')">Summary</button>
  </div>

  <!-- APs Tab -->
  <div id="tab-aps" class="tab-panel active">
    <div class="toolbar">
      <input class="search" id="ap-search" placeholder="Search SSID / BSSID…" oninput="renderAPs()">
      <select class="filter" id="ap-sec-filter" onchange="renderAPs()">
        <option value="">All Security</option>
        <option value="open">Open</option>
        <option value="wep">WEP</option>
        <option value="wpa">WPA</option>
        <option value="wpa2">WPA2</option>
        <option value="wpa3">WPA3</option>
        <option value="owe">OWE</option>
      </select>
      <select class="filter" id="ap-ch-filter" onchange="renderAPs()">
        <option value="">All Channels</option>
      </select>
      <button class="btn btn-ghost" onclick="loadData()">Refresh</button>
    </div>
    <div class="stat-row" id="ap-stats"></div>
    <div class="table-wrap">
      <table id="ap-table">
        <thead>
          <tr>
            <th onclick="sortAPs('ssid')">SSID</th>
            <th onclick="sortAPs('bssid')">BSSID</th>
            <th onclick="sortAPs('rssi')">RSSI</th>
            <th onclick="sortAPs('ch')">Ch</th>
            <th onclick="sortAPs('sec')">Security</th>
            <th onclick="sortAPs('clients')">Clients</th>
            <th>Caps</th>
            <th onclick="sortAPs('dist_m')">Distance</th>
            <th onclick="sortAPs('beacons')">Beacons</th>
          </tr>
        </thead>
        <tbody id="ap-tbody">
          <tr><td colspan="9" class="empty">Loading…</td></tr>
        </tbody>
      </table>
    </div>
  </div>

  <!-- STAs Tab -->
  <div id="tab-stas" class="tab-panel">
    <div class="toolbar">
      <input class="search" id="sta-search" placeholder="Search MAC / SSID…" oninput="renderSTAs()">
      <select class="filter" id="sta-pattern-filter" onchange="renderSTAs()">
        <option value="">All Patterns</option>
        <option value="idle">Idle</option>
        <option value="browsing">Browsing</option>
        <option value="streaming">Streaming</option>
        <option value="scanning">Scanning</option>
        <option value="voip">VoIP</option>
      </select>
      <button class="btn btn-ghost" onclick="loadData()">Refresh</button>
    </div>
    <div class="stat-row" id="sta-stats"></div>
    <div class="table-wrap">
      <table id="sta-table">
        <thead>
          <tr>
            <th onclick="sortSTAs('mac')">MAC</th>
            <th onclick="sortSTAs('vendor')">Vendor</th>
            <th onclick="sortSTAs('rssi')">RSSI</th>
            <th onclick="sortSTAs('ch')">Ch</th>
            <th onclick="sortSTAs('pattern')">Pattern</th>
            <th onclick="sortSTAs('probes')">Probes</th>
            <th onclick="sortSTAs('bytes')">Traffic</th>
            <th onclick="sortSTAs('active_s')">Active</th>
            <th>Flags</th>
          </tr>
        </thead>
        <tbody id="sta-tbody">
          <tr><td colspan="9" class="empty">Loading…</td></tr>
        </tbody>
      </table>
    </div>
  </div>

  <!-- Summary Tab -->
  <div id="tab-summary" class="tab-panel">
    <div id="summary-content" style="font-size:14px;color:var(--muted)">Loading…</div>
  </div>
</div>

<!-- Detail Panel -->
<div id="detail-panel" class="detail-panel">
  <h2><span id="detail-title">Details</span> <span class="close" onclick="closeDetail()">✕</span></h2>
  <div id="detail-body"></div>
</div>

<script>
let apData = [], staData = [];
let apSort = {key:'rssi',dir:-1}, staSort = {key:'rssi',dir:-1};

function switchTab(name) {
  document.querySelectorAll('.tab-btn').forEach((b,i)=>{
    b.classList.toggle('active', ['aps','stas','summary'][i]===name);
  });
  document.querySelectorAll('.tab-panel').forEach(p=>p.classList.remove('active'));
  document.getElementById('tab-'+name).classList.add('active');
  if (name==='summary') renderSummary();
}

function secBadge(s) {
  const map = {open:'b-open',wep:'b-wep',wpa:'b-wpa2',wpa2:'b-wpa2',
               wpa3:'b-wpa3',wpa3_t:'b-wpa3',owe:'b-owe',wpa2_e:'b-wpa2'};
  return `<span class="badge ${map[s]||'b-unk'}">${s.toUpperCase()}</span>`;
}

function rssiBar(rssi) {
  const pct = Math.min(100, Math.max(0, (rssi+100)*2));
  const color = pct>60?'#22c55e':pct>35?'#f59e0b':'#ef4444';
  return `<span style="color:${color}">${rssi}</span>
    <span class="rssi-bar"><span class="rssi-fill" style="width:${pct}%;background:${color}"></span></span>`;
}

function formatBytes(b) {
  if(b>=1048576) return (b/1048576).toFixed(1)+'MB';
  if(b>=1024) return (b/1024).toFixed(1)+'KB';
  return b+'B';
}

function caps(ap) {
  let c='';
  if(ap.ht)  c+='<span class="type-chip">n</span>';
  if(ap.vht) c+='<span class="type-chip">ac</span>';
  if(ap.he)  c+='<span class="type-chip">ax</span>';
  if(ap.wps) c+='<span class="type-chip">WPS</span>';
  if(ap.pmf) c+='<span class="type-chip">PMF</span>';
  return c||'—';
}

async function loadData() {
  const [aps, stas] = await Promise.all([
    fetch('/api/apdb/all').then(r=>r.json()).catch(()=>({aps:[]})),
    fetch('/api/stap/all').then(r=>r.json()).catch(()=>({stas:[]}))
  ]);
  apData = (aps.aps||[]);
  staData = (stas.stas||[]);

  // Populate channel filter
  const chs = [...new Set(apData.map(a=>a.ch))].sort((a,b)=>a-b);
  const cf = document.getElementById('ap-ch-filter');
  cf.innerHTML = '<option value="">All Channels</option>' +
    chs.map(c=>`<option value="${c}">Ch ${c}</option>`).join('');

  renderAPs();
  renderSTAs();
}

function renderAPs() {
  const q = document.getElementById('ap-search').value.toLowerCase();
  const sec = document.getElementById('ap-sec-filter').value;
  const ch = document.getElementById('ap-ch-filter').value;

  let list = apData.filter(ap=> {
    if(q && !(ap.ssid.toLowerCase().includes(q)||ap.bssid.toLowerCase().includes(q))) return false;
    if(sec && !ap.sec.includes(sec)) return false;
    if(ch && ap.ch !== +ch) return false;
    return true;
  });

  list.sort((a,b) => {
    const av=a[apSort.key], bv=b[apSort.key];
    return typeof av==='number'?(av-bv)*apSort.dir:(String(av).localeCompare(String(bv)))*apSort.dir;
  });

  const stats = document.getElementById('ap-stats');
  const open = apData.filter(a=>a.sec==='open').length;
  const wpa3 = apData.filter(a=>a.sec==='wpa3'||a.sec==='wpa3_t').length;
  stats.innerHTML = `
    <div class="stat-pill">Total: <b>${apData.length}</b></div>
    <div class="stat-pill">Open: <b style="color:var(--danger)">${open}</b></div>
    <div class="stat-pill">WPA3: <b style="color:var(--accent)">${wpa3}</b></div>
    <div class="stat-pill">Shown: <b>${list.length}</b></div>`;

  if(list.length===0) {
    document.getElementById('ap-tbody').innerHTML = '<tr><td colspan="9" class="empty">No matching APs.</td></tr>';
    return;
  }

  document.getElementById('ap-tbody').innerHTML = list.map(ap=>`
    <tr onclick="showAPDetail(${JSON.stringify(ap).replace(/'/g,'&apos;')})">
      <td><b>${ap.ssid||'<hidden>'}</b></td>
      <td style="font-family:monospace;color:var(--muted)">${ap.bssid}</td>
      <td>${rssiBar(ap.rssi)}</td>
      <td>${ap.ch}</td>
      <td>${secBadge(ap.sec)}</td>
      <td>${ap.clients}</td>
      <td>${caps(ap)}</td>
      <td>${ap.dist_m?ap.dist_m.toFixed(0)+'m':'—'}</td>
      <td style="color:var(--muted)">${ap.beacons}</td>
    </tr>`).join('');
}

function sortAPs(key) {
  if(apSort.key===key) apSort.dir*=-1; else {apSort.key=key;apSort.dir=-1;}
  renderAPs();
}

function renderSTAs() {
  const q = document.getElementById('sta-search').value.toLowerCase();
  const pat = document.getElementById('sta-pattern-filter').value;

  let list = staData.filter(s=>{
    if(q && !(s.mac.toLowerCase().includes(q)||(s.probed_ssids||[]).some(x=>x.toLowerCase().includes(q)))) return false;
    if(pat && s.pattern!==pat) return false;
    return true;
  });

  list.sort((a,b)=>{
    const av=a[staSort.key],bv=b[staSort.key];
    return typeof av==='number'?(av-bv)*staSort.dir:(String(av).localeCompare(String(bv)))*staSort.dir;
  });

  const stats = document.getElementById('sta-stats');
  const scanners = staData.filter(s=>s.pattern==='scanning').length;
  stats.innerHTML = `
    <div class="stat-pill">Total: <b>${staData.length}</b></div>
    <div class="stat-pill">Scanners: <b style="color:var(--warn)">${scanners}</b></div>
    <div class="stat-pill">Shown: <b>${list.length}</b></div>`;

  if(list.length===0){
    document.getElementById('sta-tbody').innerHTML='<tr><td colspan="9" class="empty">No stations.</td></tr>';
    return;
  }

  document.getElementById('sta-tbody').innerHTML = list.map(s=>`
    <tr onclick="showSTADetail(${JSON.stringify(s).replace(/'/g,'&apos;')})">
      <td style="font-family:monospace">${s.mac}</td>
      <td>${s.vendor||'—'}</td>
      <td>${rssiBar(s.rssi)}</td>
      <td>${s.ch||'—'}</td>
      <td><span class="badge b-unk">${s.pattern||'?'}</span></td>
      <td>${s.probes||0}</td>
      <td>${formatBytes(s.bytes||0)}</td>
      <td>${s.active_s||0}s</td>
      <td>
        ${s.ht?'<span class="type-chip">n</span>':''}
        ${s.vht?'<span class="type-chip">ac</span>':''}
        ${s.pmf?'<span class="type-chip">PMF</span>':''}
        ${s.ps?'<span class="type-chip">PS</span>':''}
      </td>
    </tr>`).join('');
}

function sortSTAs(key) {
  if(staSort.key===key)staSort.dir*=-1; else{staSort.key=key;staSort.dir=-1;}
  renderSTAs();
}

function showAPDetail(ap) {
  document.getElementById('detail-title').textContent = ap.ssid || '<hidden>';
  document.getElementById('detail-body').innerHTML = Object.entries({
    BSSID: ap.bssid, Channel: ap.ch, RSSI: ap.rssi+'dBm',
    Security: ap.sec.toUpperCase(), Beacons: ap.beacons,
    Clients: ap.clients, Distance: (ap.dist_m||0).toFixed(0)+'m',
    HT: ap.ht?'Yes':'No', VHT: ap.vht?'Yes':'No', HE: ap.he?'Yes':'No',
    WPS: ap.wps?'Yes':'No', PMF: ap.pmf?'Required':'No',
    Hidden: ap.hidden?'Yes':'No', 'Anomaly Score': ap.anomaly,
    'First Seen': ap.first_seen+'s', 'Last Seen': ap.last_seen+'s',
    'Active Seconds': ap.active_s+'s'
  }).map(([k,v])=>`<div class="kv"><span class="kv-key">${k}</span><span class="kv-val">${v}</span></div>`).join('');
  document.getElementById('detail-panel').classList.add('open');
}

function showSTADetail(s) {
  document.getElementById('detail-title').textContent = s.mac;
  document.getElementById('detail-body').innerHTML = Object.entries({
    MAC: s.mac, Vendor: s.vendor||'Unknown', RSSI: s.rssi+'dBm',
    Channel: s.ch||'?', Pattern: s.pattern||'?',
    'Probe Requests': s.probes, 'Data Frames': s.data,
    'Mgmt Frames': s.mgmt, 'Traffic': formatBytes(s.bytes||0),
    'Active': s.active_s+'s', Mobility: s.mobility,
    Aggressiveness: s.aggression,
    'Power Save': s.ps?'Yes':'No', PMF: s.pmf?'Yes':'No',
    HT: s.ht?'Yes':'No', VHT: s.vht?'Yes':'No',
    'Probed SSIDs': (s.probed_ssids||[]).join(', ')||'None'
  }).map(([k,v])=>`<div class="kv"><span class="kv-key">${k}</span><span class="kv-val">${v}</span></div>`).join('');
  document.getElementById('detail-panel').classList.add('open');
}

function closeDetail() {
  document.getElementById('detail-panel').classList.remove('open');
}

function renderSummary() {
  const open = apData.filter(a=>a.sec==='open').length;
  const wpa3 = apData.filter(a=>a.sec.startsWith('wpa3')).length;
  const scanners = staData.filter(s=>s.pattern==='scanning').length;
  document.getElementById('summary-content').innerHTML = `
    <div style="display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:16px">
      <div style="background:var(--bg2);border:1px solid var(--border);border-radius:8px;padding:16px">
        <div style="font-size:11px;color:var(--muted);margin-bottom:8px">ACCESS POINTS</div>
        <div style="font-size:28px;font-weight:700;color:var(--accent)">${apData.length}</div>
      </div>
      <div style="background:var(--bg2);border:1px solid var(--border);border-radius:8px;padding:16px">
        <div style="font-size:11px;color:var(--muted);margin-bottom:8px">STATIONS</div>
        <div style="font-size:28px;font-weight:700;color:var(--accent)">${staData.length}</div>
      </div>
      <div style="background:var(--bg2);border:1px solid var(--border);border-radius:8px;padding:16px">
        <div style="font-size:11px;color:var(--muted);margin-bottom:8px">OPEN APs</div>
        <div style="font-size:28px;font-weight:700;color:var(--danger)">${open}</div>
      </div>
      <div style="background:var(--bg2);border:1px solid var(--border);border-radius:8px;padding:16px">
        <div style="font-size:11px;color:var(--muted);margin-bottom:8px">WPA3 APs</div>
        <div style="font-size:28px;font-weight:700;color:var(--accent)">${wpa3}</div>
      </div>
      <div style="background:var(--bg2);border:1px solid var(--border);border-radius:8px;padding:16px">
        <div style="font-size:11px;color:var(--muted);margin-bottom:8px">SCANNING STAs</div>
        <div style="font-size:28px;font-weight:700;color:var(--warn)">${scanners}</div>
      </div>
    </div>`;
}

loadData();
setInterval(loadData, 10000);
</script>
</body>
</html>
)rawhtml";

inline void wudev_routes() {
  server.on("/devices", HTTP_GET, []() { server.send_P(200, "text/html", WUDEV_PAGE); });
}

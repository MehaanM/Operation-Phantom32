#pragma once
// webui_ble_iot.h — BLE device browser + IoT fingerprint viewer.
// Full-featured UI with device cards, service breakdown, signal strength bars,
// manufacturer lookup, and category filtering. Prefix: wubi_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUBI_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Phantom32 · BLE/IoT</title>
<style>
:root{--bg:#0a0e1a;--surface:#111827;--surface2:#1a2235;--border:#2d3748;--accent:#00d4ff;--accent2:#7c3aed;--accent3:#10b981;--accent4:#f59e0b;--accent5:#ef4444;--text:#e2e8f0;--muted:#64748b;}
*{margin:0;padding:0;box-sizing:border-box;}
body{background:var(--bg);color:var(--text);font-family:'Segoe UI',system-ui,sans-serif;font-size:13px;}
header{background:var(--surface);border-bottom:1px solid var(--border);padding:10px 16px;display:flex;align-items:center;gap:12px;flex-wrap:wrap;}
header h1{font-size:16px;font-weight:700;color:var(--accent);}
.badge{background:var(--accent2);color:#fff;padding:2px 8px;border-radius:12px;font-size:11px;}
nav a{color:var(--muted);text-decoration:none;padding:6px 10px;border-radius:6px;font-size:12px;}
nav a:hover,nav a.active{background:var(--surface2);color:var(--accent);}
.toolbar{display:flex;gap:8px;padding:10px 14px;background:var(--surface);border-bottom:1px solid var(--border);align-items:center;flex-wrap:wrap;}
input[type=text]{background:var(--surface2);border:1px solid var(--border);color:var(--text);padding:5px 10px;border-radius:6px;font-size:12px;width:200px;}
input[type=text]:focus{outline:none;border-color:var(--accent);}
select{background:var(--surface2);border:1px solid var(--border);color:var(--text);padding:5px 8px;border-radius:6px;font-size:12px;}
.btn{background:var(--surface2);border:1px solid var(--border);color:var(--text);padding:5px 12px;border-radius:6px;cursor:pointer;font-size:12px;transition:all .2s;}
.btn:hover{background:var(--border);}
.btn.active{background:var(--accent);color:#000;border-color:var(--accent);}
.tabs{display:flex;gap:2px;padding:0 14px;background:var(--surface);border-bottom:1px solid var(--border);}
.tab{padding:8px 16px;cursor:pointer;color:var(--muted);border-bottom:2px solid transparent;font-size:13px;transition:all .2s;}
.tab.active{color:var(--accent);border-bottom-color:var(--accent);}
.content{padding:14px;display:grid;grid-template-columns:repeat(auto-fill,minmax(280px,1fr));gap:12px;}
.device-card{background:var(--surface);border:1px solid var(--border);border-radius:10px;padding:14px;cursor:pointer;transition:all .2s;position:relative;}
.device-card:hover{border-color:var(--accent);transform:translateY(-1px);}
.device-card.selected{border-color:var(--accent);background:var(--surface2);}
.device-card.iot{border-left:3px solid var(--accent3);}
.device-card.phone{border-left:3px solid var(--accent);}
.device-card.wearable{border-left:3px solid var(--accent4);}
.device-card.unknown{border-left:3px solid var(--muted);}
.device-header{display:flex;align-items:flex-start;gap:10px;margin-bottom:10px;}
.device-icon{font-size:28px;line-height:1;}
.device-info{flex:1;}
.device-name{font-size:13px;font-weight:700;color:var(--text);word-break:break-all;}
.device-mac{font-size:11px;color:var(--muted);font-family:monospace;}
.device-category{font-size:10px;color:var(--accent2);font-weight:600;text-transform:uppercase;margin-top:2px;}
.rssi-bar-row{display:flex;align-items:center;gap:8px;margin-top:6px;}
.rssi-label{font-size:10px;color:var(--muted);width:50px;}
.rssi-bg{flex:1;height:6px;background:var(--surface2);border-radius:3px;overflow:hidden;}
.rssi-fill{height:100%;border-radius:3px;transition:width .4s;}
.rssi-val{font-size:11px;color:var(--text);width:50px;text-align:right;}
.device-tags{display:flex;gap:4px;flex-wrap:wrap;margin-top:8px;}
.tag{padding:2px 6px;border-radius:4px;font-size:10px;font-weight:600;}
.tag-ble{background:#3b82f61a;color:#3b82f6;}
.tag-adv{background:#10b9811a;color:#10b981;}
.tag-ibeacon{background:#7c3aed1a;color:#7c3aed;}
.tag-findmy{background:#f59e0b1a;color:#f59e0b;}
.tag-airdrop{background:#00d4ff1a;color:#00d4ff;}
.tag-eddystone{background:#ef44441a;color:#ef4444;}
.device-services{margin-top:8px;font-size:11px;color:var(--muted);}
.device-services strong{color:var(--text);}
.device-mfr{margin-top:4px;font-size:11px;color:var(--accent4);}
.last-seen{font-size:10px;color:var(--muted);position:absolute;top:10px;right:10px;}
.detail-modal{display:none;position:fixed;top:0;left:0;right:0;bottom:0;background:#0009;z-index:100;align-items:center;justify-content:center;}
.detail-modal.show{display:flex;}
.modal-box{background:var(--surface);border:1px solid var(--border);border-radius:12px;width:min(560px,95vw);max-height:80vh;overflow-y:auto;padding:20px;}
.modal-box h2{font-size:15px;font-weight:700;margin-bottom:14px;color:var(--accent);}
.modal-close{float:right;cursor:pointer;color:var(--muted);font-size:18px;line-height:1;}
.modal-close:hover{color:var(--text);}
.detail-row{display:flex;gap:10px;padding:5px 0;border-bottom:1px solid var(--border);}
.detail-key{color:var(--muted);width:150px;flex-shrink:0;font-size:12px;}
.detail-val{color:var(--text);font-size:12px;word-break:break-all;font-family:monospace;}
.iot-content{padding:14px;}
.iot-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(240px,1fr));gap:10px;}
.iot-card{background:var(--surface);border:1px solid var(--border);border-radius:10px;padding:12px;}
.iot-vendor{font-size:10px;color:var(--muted);text-transform:uppercase;letter-spacing:.5px;}
.iot-model{font-size:14px;font-weight:700;margin-top:2px;}
.iot-class{font-size:11px;color:var(--accent3);margin-top:2px;}
.iot-confidence{margin-top:8px;display:flex;align-items:center;gap:8px;}
.conf-bg{flex:1;height:4px;background:var(--surface2);border-radius:2px;overflow:hidden;}
.conf-fill{height:100%;border-radius:2px;background:var(--accent3);}
.conf-pct{font-size:11px;color:var(--accent3);width:35px;text-align:right;}
.iot-probes{margin-top:8px;font-size:11px;color:var(--muted);}
.stat-summary{display:flex;gap:16px;padding:8px 14px;background:var(--surface);border-bottom:1px solid var(--border);font-size:12px;}
.stat-summary .s{display:flex;gap:6px;align-items:center;}
.stat-summary .s .n{font-size:16px;font-weight:700;color:var(--accent);}
.empty-state{grid-column:1/-1;text-align:center;padding:60px 20px;color:var(--muted);}
.empty-state .icon{font-size:48px;margin-bottom:12px;}
.scanning-dot{display:inline-block;width:8px;height:8px;border-radius:50%;background:var(--accent3);animation:pulse 1.5s infinite;margin-right:6px;}
@keyframes pulse{0%,100%{opacity:1;}50%{opacity:.2;}}
</style>
</head>
<body>
<header>
  <h1>⚡ Phantom32</h1>
  <span class="badge">BLE / IoT</span>
  <span id="scan-state" style="color:var(--muted);font-size:12px;"></span>
  <nav style="display:flex;gap:2px;margin-left:auto;">
    <a href="/">Home</a>
    <a href="/analytics">Analytics</a>
    <a href="/forensics">Forensics</a>
    <a href="/ble-iot" class="active">BLE/IoT</a>
    <a href="/cli">CLI</a>
  </nav>
</header>

<div class="toolbar">
  <input type="text" id="searchInput" placeholder="Search name / MAC / vendor" oninput="renderDevices()">
  <select id="catFilter" onchange="renderDevices()">
    <option value="">All Categories</option>
    <option value="phone">Phone</option>
    <option value="computer">Computer</option>
    <option value="wearable">Wearable</option>
    <option value="iot">IoT</option>
    <option value="audio">Audio</option>
    <option value="vehicle">Vehicle</option>
    <option value="beacon">Beacon</option>
    <option value="unknown">Unknown</option>
  </select>
  <select id="sortBy" onchange="renderDevices()">
    <option value="rssi">Sort: RSSI</option>
    <option value="name">Sort: Name</option>
    <option value="last">Sort: Last seen</option>
    <option value="cat">Sort: Category</option>
  </select>
  <button class="btn active" id="bleBtn" onclick="toggleBle()"><span class="scanning-dot"></span>BLE Scan</button>
  <button class="btn" onclick="clearBle()">Clear</button>
  <button class="btn" onclick="exportBle()">Export JSON</button>
</div>

<div class="stat-summary" id="statSummary">
  <div class="s"><div class="n" id="total-ble">0</div><div>BLE Devices</div></div>
  <div class="s"><div class="n" id="total-iot">0</div><div>IoT Devices</div></div>
  <div class="s"><div class="n" id="total-phone">0</div><div>Phones</div></div>
  <div class="s"><div class="n" id="total-beacon">0</div><div>Beacons</div></div>
  <div class="s"><div class="n" id="total-findmy">0</div><div>FindMy</div></div>
</div>

<div class="tabs">
  <div class="tab active" id="tab-ble" onclick="switchTab('ble')">BLE Devices</div>
  <div class="tab" id="tab-iot" onclick="switchTab('iot')">WiFi IoT</div>
</div>

<div id="ble-view">
  <div class="content" id="bleGrid">
    <div class="empty-state">
      <div class="icon">📡</div>
      <div>BLE scanning active...</div>
      <div style="margin-top:8px;font-size:12px;">Devices will appear as they advertise</div>
    </div>
  </div>
</div>

<div id="iot-view" style="display:none;">
  <div class="iot-content">
    <div class="iot-grid" id="iotGrid">
      <div class="empty-state">
        <div class="icon">🔌</div>
        <div>No IoT devices fingerprinted yet</div>
      </div>
    </div>
  </div>
</div>

<div class="detail-modal" id="detailModal">
  <div class="modal-box">
    <span class="modal-close" onclick="closeModal()">✕</span>
    <h2 id="modal-title">Device Details</h2>
    <div id="modal-body"></div>
  </div>
</div>

<script>
let bleDevices = {};
let iotDevices = [];
let bleScanning = true;
let timer = null;
let currentTab = 'ble';

const CATEGORY_ICONS = {
  phone: '📱', computer: '💻', wearable: '⌚', audio: '🎧',
  iot: '🔌', vehicle: '🚗', beacon: '📍', fitness: '🏃',
  keyboard: '⌨️', mouse: '🖱️', tv: '📺', camera: '📷', unknown: '📶'
};

const RSSI_COLOR = rssi => {
  if (rssi >= -60) return '#10b981';
  if (rssi >= -75) return '#f59e0b';
  if (rssi >= -85) return '#ef4444';
  return '#64748b';
};

function rssiPct(rssi) {
  return Math.max(0, Math.min(100, ((rssi + 100) / 60) * 100));
}

function switchTab(tab) {
  currentTab = tab;
  document.getElementById('tab-ble').className = 'tab' + (tab === 'ble' ? ' active' : '');
  document.getElementById('tab-iot').className = 'tab' + (tab === 'iot' ? ' active' : '');
  document.getElementById('ble-view').style.display = tab === 'ble' ? '' : 'none';
  document.getElementById('iot-view').style.display = tab === 'iot' ? '' : 'none';
}

function renderDevices() {
  const search = document.getElementById('searchInput').value.toLowerCase();
  const cat = document.getElementById('catFilter').value;
  const sort = document.getElementById('sortBy').value;

  let devs = Object.values(bleDevices);

  if (search) devs = devs.filter(d =>
    (d.name || '').toLowerCase().includes(search) ||
    (d.mac || '').toLowerCase().includes(search) ||
    (d.manufacturer || '').toLowerCase().includes(search)
  );
  if (cat) devs = devs.filter(d => (d.category || 'unknown') === cat);

  if (sort === 'rssi') devs.sort((a, b) => (b.rssi || -100) - (a.rssi || -100));
  else if (sort === 'name') devs.sort((a, b) => (a.name || a.mac).localeCompare(b.name || b.mac));
  else if (sort === 'last') devs.sort((a, b) => (b.last_seen || 0) - (a.last_seen || 0));
  else if (sort === 'cat') devs.sort((a, b) => (a.category || '').localeCompare(b.category || ''));

  if (!devs.length) {
    document.getElementById('bleGrid').innerHTML = `<div class="empty-state"><div class="icon">📡</div><div>No devices match filter</div></div>`;
    return;
  }

  document.getElementById('bleGrid').innerHTML = devs.map(d => {
    const icon = CATEGORY_ICONS[d.category] || CATEGORY_ICONS.unknown;
    const col = RSSI_COLOR(d.rssi || -100);
    const pct = rssiPct(d.rssi || -100);
    const tags = [];
    if (d.is_ibeacon) tags.push('<span class="tag tag-ibeacon">iBeacon</span>');
    if (d.is_findmy) tags.push('<span class="tag tag-findmy">FindMy</span>');
    if (d.is_airdrop) tags.push('<span class="tag tag-airdrop">AirDrop</span>');
    if (d.is_eddystone) tags.push('<span class="tag tag-eddystone">Eddystone</span>');
    if (d.connectable) tags.push('<span class="tag tag-adv">Connectable</span>');
    tags.push('<span class="tag tag-ble">BLE</span>');

    const ls = d.last_seen ? new Date(d.last_seen * 1000).toLocaleTimeString() : '—';

    return `<div class="device-card ${d.category || 'unknown'}" onclick="showDetail('${d.mac}')">
      <span class="last-seen">${ls}</span>
      <div class="device-header">
        <div class="device-icon">${icon}</div>
        <div class="device-info">
          <div class="device-name">${d.name || 'Unknown Device'}</div>
          <div class="device-mac">${d.mac || ''}</div>
          <div class="device-category">${d.category || 'unknown'}</div>
        </div>
      </div>
      <div class="rssi-bar-row">
        <div class="rssi-label">Signal</div>
        <div class="rssi-bg"><div class="rssi-fill" style="width:${pct}%;background:${col}"></div></div>
        <div class="rssi-val" style="color:${col}">${d.rssi || 0} dBm</div>
      </div>
      ${d.manufacturer ? `<div class="device-mfr">🏭 ${d.manufacturer}</div>` : ''}
      ${d.services && d.services.length ? `<div class="device-services"><strong>Services:</strong> ${d.services.slice(0,3).join(', ')}${d.services.length > 3 ? ' +' + (d.services.length-3) : ''}</div>` : ''}
      <div class="device-tags">${tags.join('')}</div>
    </div>`;
  }).join('');

  updateStats(Object.values(bleDevices));
}

function renderIot() {
  if (!iotDevices.length) {
    document.getElementById('iotGrid').innerHTML = `<div class="empty-state"><div class="icon">🔌</div><div>No IoT devices fingerprinted</div></div>`;
    return;
  }
  document.getElementById('iotGrid').innerHTML = iotDevices.map(d => `
    <div class="iot-card">
      <div class="iot-vendor">${d.vendor || 'Unknown Vendor'}</div>
      <div class="iot-model">${d.model || 'Unknown Model'}</div>
      <div class="iot-class">${d.classification || 'Unknown'}</div>
      <div class="iot-confidence">
        <div class="conf-bg"><div class="conf-fill" style="width:${d.confidence || 0}%"></div></div>
        <div class="conf-pct">${d.confidence || 0}%</div>
      </div>
      <div style="font-size:11px;color:var(--muted);margin-top:6px;font-family:monospace;">${d.mac || ''}</div>
      ${d.probes && d.probes.length ? `<div class="iot-probes">Probes: ${d.probes.join(', ')}</div>` : ''}
    </div>`
  ).join('');
}

function updateStats(devs) {
  document.getElementById('total-ble').textContent = devs.length;
  document.getElementById('total-iot').textContent = devs.filter(d => d.category === 'iot').length;
  document.getElementById('total-phone').textContent = devs.filter(d => d.category === 'phone').length;
  document.getElementById('total-beacon').textContent = devs.filter(d => d.is_ibeacon || d.is_eddystone).length;
  document.getElementById('total-findmy').textContent = devs.filter(d => d.is_findmy).length;
}

function showDetail(mac) {
  const d = bleDevices[mac];
  if (!d) return;
  document.getElementById('modal-title').textContent = (d.name || 'Unknown') + ' — ' + mac;
  const fields = [
    ['MAC Address', d.mac || '—'],
    ['Name', d.name || '<none>'],
    ['Category', d.category || 'unknown'],
    ['Manufacturer', d.manufacturer || '—'],
    ['RSSI', (d.rssi || 0) + ' dBm'],
    ['TX Power', d.tx_power !== undefined ? d.tx_power + ' dBm' : '—'],
    ['Connectable', d.connectable ? 'Yes' : 'No'],
    ['iBeacon', d.is_ibeacon ? 'Yes' : 'No'],
    ['FindMy', d.is_findmy ? 'Yes' : 'No'],
    ['AirDrop', d.is_airdrop ? 'Yes' : 'No'],
    ['Eddystone', d.is_eddystone ? 'Yes' : 'No'],
    ['UUID (iBeacon)', d.uuid || '—'],
    ['Services', (d.services || []).join(', ') || '—'],
    ['Adv Data (hex)', d.adv_hex || '—'],
    ['First seen', d.first_seen ? new Date(d.first_seen * 1000).toLocaleString() : '—'],
    ['Last seen', d.last_seen ? new Date(d.last_seen * 1000).toLocaleString() : '—'],
    ['Seen count', d.seen_count || 1],
  ];
  document.getElementById('modal-body').innerHTML = fields.map(([k, v]) =>
    `<div class="detail-row"><div class="detail-key">${k}</div><div class="detail-val">${v}</div></div>`
  ).join('');
  document.getElementById('detailModal').className = 'detail-modal show';
}

function closeModal() {
  document.getElementById('detailModal').className = 'detail-modal';
}

document.getElementById('detailModal').addEventListener('click', e => {
  if (e.target === e.currentTarget) closeModal();
});

function toggleBle() {
  bleScanning = !bleScanning;
  const btn = document.getElementById('bleBtn');
  btn.className = bleScanning ? 'btn active' : 'btn';
  btn.innerHTML = bleScanning ? '<span class="scanning-dot"></span>BLE Scan' : '▶ Start Scan';
  if (bleScanning) startPolling(); else clearInterval(timer);

  fetch('/api/ble/' + (bleScanning ? 'start' : 'stop'), { method: 'POST' }).catch(() => {});
}

function clearBle() {
  bleDevices = {};
  iotDevices = [];
  renderDevices();
  renderIot();
  fetch('/api/ble/clear', { method: 'POST' }).catch(() => {});
}

function exportBle() {
  const data = { ble: Object.values(bleDevices), iot: iotDevices, timestamp: Date.now() };
  const blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = `phantom32-ble-${Date.now()}.json`;
  a.click();
}

async function fetchBle() {
  try {
    const [bleR, iotR] = await Promise.all([
      fetch('/api/ble/devices').then(r => r.json()).catch(() => ({devices:[]})),
      fetch('/api/iot/devices').then(r => r.json()).catch(() => ({devices:[]})),
    ]);

    (bleR.devices || []).forEach(d => {
      if (d.mac) bleDevices[d.mac] = { ...bleDevices[d.mac], ...d };
    });
    iotDevices = iotR.devices || [];

    document.getElementById('scan-state').textContent = bleScanning
      ? '🟢 Scanning — ' + Object.keys(bleDevices).length + ' devices'
      : '⏸ Paused — ' + Object.keys(bleDevices).length + ' devices';

    renderDevices();
    renderIot();
  } catch (e) {}
}

function startPolling() {
  clearInterval(timer);
  fetchBle();
  timer = setInterval(fetchBle, 2000);
}

startPolling();
</script>
</body>
</html>
)rawhtml";

inline void wubi_routes() {
  server.on("/ble-iot", HTTP_GET, []() { server.send_P(200, "text/html", WUBI_PAGE); });
}

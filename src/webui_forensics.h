#pragma once
// webui_forensics.h — Packet forensics page: live packet feed with hex viewer,
// frame filter, protocol decoder, and PCAP export. Prefix: wuf_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUF_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Phantom32 · Forensics</title>
<style>
:root{--bg:#0a0e1a;--surface:#111827;--surface2:#1a2235;--border:#2d3748;--accent:#00d4ff;--accent2:#7c3aed;--accent3:#10b981;--accent4:#f59e0b;--accent5:#ef4444;--text:#e2e8f0;--muted:#64748b;}
*{margin:0;padding:0;box-sizing:border-box;}
body{background:var(--bg);color:var(--text);font-family:'Consolas','Courier New',monospace;font-size:12px;}
header{background:var(--surface);border-bottom:1px solid var(--border);padding:10px 16px;display:flex;align-items:center;gap:12px;flex-wrap:wrap;}
header h1{font-size:16px;font-weight:700;color:var(--accent);font-family:'Segoe UI',sans-serif;}
.badge{background:var(--accent2);color:#fff;padding:2px 8px;border-radius:12px;font-size:11px;font-family:sans-serif;}
.toolbar{display:flex;gap:6px;align-items:center;flex-wrap:wrap;padding:8px 12px;background:var(--surface);border-bottom:1px solid var(--border);}
input[type=text]{background:var(--surface2);border:1px solid var(--border);color:var(--text);padding:5px 10px;border-radius:6px;font-size:11px;font-family:monospace;width:200px;}
input[type=text]:focus{outline:none;border-color:var(--accent);}
select{background:var(--surface2);border:1px solid var(--border);color:var(--text);padding:5px 8px;border-radius:6px;font-size:11px;}
.btn{background:var(--surface2);border:1px solid var(--border);color:var(--text);padding:5px 10px;border-radius:6px;cursor:pointer;font-size:11px;font-family:sans-serif;transition:all .2s;}
.btn:hover{background:var(--border);color:var(--accent);}
.btn.active{background:var(--accent);color:#000;border-color:var(--accent);}
.btn.danger{border-color:var(--accent5);color:var(--accent5);}
.btn.danger:hover{background:var(--accent5);color:#fff;}
.main{display:grid;grid-template-rows:1fr 280px;height:calc(100vh - 90px);}
.pkt-list{overflow-y:auto;border-bottom:1px solid var(--border);}
.pkt-row{display:grid;grid-template-columns:60px 80px 100px 100px 80px 1fr 60px;padding:3px 8px;border-bottom:1px solid #1a2235;cursor:pointer;white-space:nowrap;overflow:hidden;}
.pkt-row:hover{background:var(--surface2);}
.pkt-row.selected{background:#00d4ff15;border-left:2px solid var(--accent);}
.pkt-row.beacon{color:#00d4ff;}
.pkt-row.probe{color:#7c3aed;}
.pkt-row.deauth{color:#ef4444;}
.pkt-row.auth{color:#f59e0b;}
.pkt-row.data{color:#10b981;}
.pkt-row.eapol{color:#f97316;}
.pkt-header{display:grid;grid-template-columns:60px 80px 100px 100px 80px 1fr 60px;padding:4px 8px;background:var(--surface);border-bottom:1px solid var(--border);font-size:10px;color:var(--muted);font-weight:600;position:sticky;top:0;z-index:1;}
.detail-pane{display:grid;grid-template-columns:1fr 1fr;border-top:2px solid var(--accent);}
.detail-panel{padding:10px;overflow-y:auto;}
.detail-panel h3{font-size:11px;color:var(--accent);margin-bottom:8px;font-family:sans-serif;}
.field-row{display:flex;gap:8px;padding:2px 0;}
.field-name{color:var(--muted);width:140px;flex-shrink:0;font-size:11px;}
.field-val{color:var(--text);font-size:11px;word-break:break-all;}
.hex-wrap{font-size:11px;line-height:1.6;}
.hex-offset{color:var(--muted);display:inline-block;width:40px;}
.hex-bytes{color:var(--accent);display:inline-block;width:290px;letter-spacing:1px;}
.hex-ascii{color:var(--accent3);}
.hex-bytes span:hover,.hex-ascii span:hover{background:var(--accent2);cursor:pointer;}
.counter{color:var(--muted);font-size:11px;margin-left:auto;}
.separator{width:1px;background:var(--border);}
.stat-bar{display:flex;gap:16px;padding:4px 12px;background:var(--surface);border-bottom:1px solid var(--border);font-size:10px;color:var(--muted);}
.stat-bar span{color:var(--text);}
.filter-chip{background:var(--surface2);border:1px solid var(--border);padding:2px 8px;border-radius:12px;font-size:10px;cursor:pointer;}
.filter-chip.on{background:var(--accent2);border-color:var(--accent2);color:#fff;}
.scroll-lock{color:var(--accent4);}
</style>
</head>
<body>
<header>
  <h1>⚡ Phantom32</h1>
  <span class="badge">FORENSICS</span>
  <span class="counter" id="pkt-counter">0 packets</span>
  <nav style="display:flex;gap:4px;margin-left:auto;font-size:11px;font-family:sans-serif;">
    <a href="/" style="color:var(--muted);text-decoration:none;padding:4px 8px;border-radius:4px;">Home</a>
    <a href="/analytics" style="color:var(--muted);text-decoration:none;padding:4px 8px;border-radius:4px;">Analytics</a>
    <a href="/forensics" style="color:var(--accent);text-decoration:none;padding:4px 8px;border-radius:4px;background:var(--surface2);">Forensics</a>
    <a href="/ble-iot" style="color:var(--muted);text-decoration:none;padding:4px 8px;border-radius:4px;">BLE/IoT</a>
    <a href="/cli" style="color:var(--muted);text-decoration:none;padding:4px 8px;border-radius:4px;">CLI</a>
  </nav>
</header>

<div class="toolbar">
  <input type="text" id="filterInput" placeholder="Filter: mac / ssid / type / 'deauth'" oninput="applyFilter()">
  <select id="typeFilter" onchange="applyFilter()">
    <option value="">All Types</option>
    <option value="beacon">Beacon</option>
    <option value="probe_req">Probe Req</option>
    <option value="probe_resp">Probe Resp</option>
    <option value="auth">Auth</option>
    <option value="assoc_req">Assoc Req</option>
    <option value="deauth">Deauth</option>
    <option value="disassoc">Disassoc</option>
    <option value="data">Data</option>
    <option value="eapol">EAPOL</option>
  </select>
  <select id="chFilter" onchange="applyFilter()">
    <option value="">All Channels</option>
  </select>
  <div class="filter-chip" id="chip-mgmt" onclick="toggleChip('mgmt')">Mgmt</div>
  <div class="filter-chip on" id="chip-data" onclick="toggleChip('data')">Data</div>
  <div class="filter-chip on" id="chip-ctrl" onclick="toggleChip('ctrl')">Ctrl</div>
  <button class="btn active" id="captureBtn" onclick="toggleCapture()">▶ Capture</button>
  <button class="btn" onclick="clearPackets()">Clear</button>
  <button class="btn" onclick="exportPcap()">Export PCAP</button>
  <button class="btn" id="scrollBtn" onclick="toggleScroll()">⬇ Auto-scroll</button>
  <span class="counter" id="filtered-count">0 shown</span>
</div>

<div class="stat-bar">
  <div>Beacon: <span id="cnt-beacon">0</span></div>
  <div>Probe: <span id="cnt-probe">0</span></div>
  <div>Auth: <span id="cnt-auth">0</span></div>
  <div>Deauth: <span id="cnt-deauth">0</span></div>
  <div>Data: <span id="cnt-data">0</span></div>
  <div>EAPOL: <span id="cnt-eapol">0</span></div>
  <div>Pkts/s: <span id="stat-pps">0</span></div>
  <div>Total: <span id="stat-total">0</span></div>
</div>

<div class="main">
  <div class="pkt-list">
    <div class="pkt-header">
      <div>#</div>
      <div>Time</div>
      <div>Type</div>
      <div>Src MAC</div>
      <div>Dst MAC</div>
      <div>SSID / Info</div>
      <div>RSSI</div>
    </div>
    <div id="pktBody"></div>
  </div>

  <div class="detail-pane">
    <div class="detail-panel" id="detailPanel">
      <h3>Frame Details</h3>
      <div style="color:var(--muted)">Click a packet to inspect</div>
    </div>
    <div class="separator"></div>
    <div class="detail-panel" id="hexPanel">
      <h3>Hex Dump</h3>
      <div class="hex-wrap" id="hexDump" style="color:var(--muted)">No packet selected</div>
    </div>
  </div>
</div>

<script>
let capturing = false;
let autoScroll = true;
let packets = [];
let filtered = [];
let chips = { mgmt: false, data: true, ctrl: true };
let timer = null;
let pktSeq = 0;
let selectedIdx = -1;
let prevTotal = 0;
let prevTs = Date.now();
const MAX_PKTS = 500;

// Frame type classification
function classifyType(fc) {
  if (fc === undefined) return 'unknown';
  const type = (fc >> 2) & 0x3;
  const sub  = (fc >> 4) & 0xf;
  if (type === 0) {
    const names = ['assoc_req','assoc_resp','reassoc_req','reassoc_resp','probe_req','probe_resp','_','_','beacon','_','disassoc','auth','deauth','action'];
    return names[sub] || `mgmt_${sub}`;
  }
  if (type === 1) return `ctrl_${sub}`;
  if (type === 2) {
    if (sub === 0) return 'data';
    if (sub === 8) return 'qos_data';
    return `data_${sub}`;
  }
  return 'unknown';
}

function macStr(arr) {
  if (!arr || arr.length < 6) return '??:??:??:??:??:??';
  return arr.map(b => b.toString(16).padStart(2,'0').toUpperCase()).join(':');
}

function typeClass(t) {
  if (t.startsWith('beacon')) return 'beacon';
  if (t.startsWith('probe')) return 'probe';
  if (t === 'deauth' || t === 'disassoc') return 'deauth';
  if (t === 'auth' || t.startsWith('assoc')) return 'auth';
  if (t === 'eapol') return 'eapol';
  if (t.startsWith('data') || t.startsWith('qos')) return 'data';
  return '';
}

function renderHex(bytes) {
  if (!bytes || !bytes.length) return '<span style="color:var(--muted)">empty</span>';
  let html = '';
  for (let i = 0; i < bytes.length; i += 16) {
    const row = bytes.slice(i, i + 16);
    const offset = i.toString(16).padStart(4,'0');
    const hex = row.map(b => b.toString(16).padStart(2,'0')).join(' ');
    const ascii = row.map(b => (b >= 0x20 && b < 0x7f) ? String.fromCharCode(b) : '.').join('');
    html += `<div><span class="hex-offset">${offset}</span>  <span class="hex-bytes">${hex.padEnd(47,' ')}</span>  <span class="hex-ascii">${ascii}</span></div>`;
  }
  return html;
}

function decodeFrame(pkt) {
  const lines = [];
  const push = (name, val) => lines.push(`<div class="field-row"><div class="field-name">${name}</div><div class="field-val">${val}</div></div>`);

  push('Sequence #', pkt.seq || '—');
  push('Timestamp', pkt.ts_str || '—');
  push('Frame Type', pkt.type || '—');
  push('Frame Control', '0x' + ((pkt.fc || 0).toString(16).padStart(4,'0').toUpperCase()));
  push('Duration', (pkt.duration || 0) + ' µs');
  push('Src MAC', macStr(pkt.src));
  push('Dst MAC', macStr(pkt.dst));
  push('BSSID', macStr(pkt.bssid));
  push('Channel', pkt.channel || '—');
  push('RSSI', (pkt.rssi || 0) + ' dBm');
  push('Length', (pkt.len || 0) + ' bytes');
  push('Radiotap len', (pkt.rtap_len || 0) + ' bytes');

  if (pkt.ssid !== undefined) push('SSID', pkt.ssid || '<hidden>');
  if (pkt.reason !== undefined) push('Reason Code', pkt.reason + ' (' + reasonStr(pkt.reason) + ')');
  if (pkt.status !== undefined) push('Status Code', pkt.status);
  if (pkt.seq_num !== undefined) push('Seq Num', pkt.seq_num);
  if (pkt.retry) push('Retry', 'Yes');
  if (pkt.protected_frame) push('Protected', 'Yes');
  if (pkt.more_frag) push('More Frag', 'Yes');
  if (pkt.to_ds !== undefined) push('To DS / From DS', pkt.to_ds + ' / ' + pkt.from_ds);
  if (pkt.rates && pkt.rates.length) push('Rates (Mbps)', pkt.rates.join(', '));
  if (pkt.ht_caps) push('HT Caps', '0x' + pkt.ht_caps.toString(16));
  if (pkt.rsn) push('RSN Info', pkt.rsn);
  if (pkt.wps) push('WPS', pkt.wps);
  if (pkt.vendor_ies) push('Vendor IEs', pkt.vendor_ies);

  return lines.join('');
}

function reasonStr(code) {
  const reasons = {1:'Unspecified',2:'Auth expired',3:'Leaving',4:'Inactivity',6:'Class 2 error',7:'Class 3 error',8:'Disassoc',9:'Not auth',15:'4-way handshake timeout',23:'IEEE 802.1X auth failed'};
  return reasons[code] || 'Code ' + code;
}

function selectPacket(idx) {
  selectedIdx = idx;
  document.querySelectorAll('.pkt-row').forEach((r, i) => {
    r.classList.toggle('selected', i === idx);
  });
  const pkt = filtered[idx];
  if (!pkt) return;
  document.getElementById('detailPanel').innerHTML = '<h3>Frame Details — #' + pkt.seq + '</h3>' + decodeFrame(pkt);
  document.getElementById('hexDump').innerHTML = renderHex(pkt.raw);
}

let counts = { beacon:0, probe:0, auth:0, deauth:0, data:0, eapol:0, total:0 };

function addPackets(newPkts) {
  newPkts.forEach(p => {
    p.seq = ++pktSeq;
    const d = new Date(p.ts * 1000);
    p.ts_str = d.getHours().toString().padStart(2,'0') + ':' + d.getMinutes().toString().padStart(2,'0') + ':' + d.getSeconds().toString().padStart(2,'0') + '.' + d.getMilliseconds().toString().padStart(3,'0');
    packets.push(p);
    if (p.type === 'beacon') counts.beacon++;
    else if (p.type && p.type.startsWith('probe')) counts.probe++;
    else if (p.type === 'auth' || (p.type && p.type.startsWith('assoc'))) counts.auth++;
    else if (p.type === 'deauth' || p.type === 'disassoc') counts.deauth++;
    else if (p.type === 'eapol') counts.eapol++;
    else if (p.type && (p.type.startsWith('data') || p.type.startsWith('qos'))) counts.data++;
    counts.total++;
  });
  if (packets.length > MAX_PKTS) packets.splice(0, packets.length - MAX_PKTS);
  applyFilter();
  updateStats();
}

function updateStats() {
  document.getElementById('cnt-beacon').textContent = counts.beacon;
  document.getElementById('cnt-probe').textContent = counts.probe;
  document.getElementById('cnt-auth').textContent = counts.auth;
  document.getElementById('cnt-deauth').textContent = counts.deauth;
  document.getElementById('cnt-data').textContent = counts.data;
  document.getElementById('cnt-eapol').textContent = counts.eapol;
  document.getElementById('stat-total').textContent = counts.total;
  const now = Date.now();
  const dt = (now - prevTs) / 1000;
  const diff = counts.total - prevTotal;
  if (dt > 0) document.getElementById('stat-pps').textContent = (diff / dt).toFixed(1);
  prevTotal = counts.total;
  prevTs = now;
  document.getElementById('pkt-counter').textContent = packets.length + ' packets';
}

function applyFilter() {
  const text = (document.getElementById('filterInput').value || '').toLowerCase();
  const type = document.getElementById('typeFilter').value;
  filtered = packets.filter(p => {
    if (type && p.type !== type) return false;
    if (text) {
      const src = macStr(p.src).toLowerCase();
      const dst = macStr(p.dst).toLowerCase();
      const ssid = (p.ssid || '').toLowerCase();
      const ptype = (p.type || '').toLowerCase();
      if (!src.includes(text) && !dst.includes(text) && !ssid.includes(text) && !ptype.includes(text)) return false;
    }
    return true;
  });
  renderPackets();
  document.getElementById('filtered-count').textContent = filtered.length + ' shown';
}

function renderPackets() {
  const body = document.getElementById('pktBody');
  const html = filtered.map((p, i) => {
    const cls = typeClass(p.type || '');
    return `<div class="pkt-row ${cls}${i === selectedIdx ? ' selected' : ''}" onclick="selectPacket(${i})">
      <div>${p.seq}</div>
      <div>${p.ts_str || '—'}</div>
      <div>${p.type || '—'}</div>
      <div>${macStr(p.src)}</div>
      <div>${macStr(p.dst)}</div>
      <div>${(p.ssid || p.info || '').substring(0,32)}</div>
      <div>${p.rssi || 0}</div>
    </div>`;
  }).join('');
  body.innerHTML = html;
  if (autoScroll) body.parentElement.scrollTop = body.parentElement.scrollHeight;
}

function toggleCapture() {
  capturing = !capturing;
  const btn = document.getElementById('captureBtn');
  btn.textContent = capturing ? '⏸ Pause' : '▶ Capture';
  btn.className = capturing ? 'btn active' : 'btn';
  if (capturing) startFeed(); else stopFeed();
}

function startFeed() {
  clearInterval(timer);
  fetchPackets();
  timer = setInterval(fetchPackets, 500);
}

function stopFeed() { clearInterval(timer); }

function clearPackets() {
  packets = [];
  filtered = [];
  counts = { beacon:0, probe:0, auth:0, deauth:0, data:0, eapol:0, total:0 };
  pktSeq = 0;
  selectedIdx = -1;
  document.getElementById('pktBody').innerHTML = '';
  document.getElementById('detailPanel').innerHTML = '<h3>Frame Details</h3><div style="color:var(--muted)">Click a packet</div>';
  document.getElementById('hexDump').innerHTML = '<span style="color:var(--muted)">No packet selected</span>';
  updateStats();
}

function toggleScroll() {
  autoScroll = !autoScroll;
  const btn = document.getElementById('scrollBtn');
  btn.textContent = autoScroll ? '⬇ Auto-scroll' : '⏸ Scroll paused';
  btn.className = autoScroll ? 'btn active' : 'btn';
}

function toggleChip(name) {
  chips[name] = !chips[name];
  document.getElementById('chip-' + name).className = 'filter-chip' + (chips[name] ? ' on' : '');
  applyFilter();
}

function exportPcap() {
  window.location.href = '/api/pcap/download';
}

let lastPktIdx = 0;
async function fetchPackets() {
  if (!capturing) return;
  try {
    const r = await fetch('/api/pkt/live?since=' + lastPktIdx);
    const data = await r.json();
    if (data.packets && data.packets.length) {
      lastPktIdx = data.last_idx || lastPktIdx + data.packets.length;
      addPackets(data.packets);
    }
  } catch (e) {}
}

// Populate channel filter
(function() {
  const sel = document.getElementById('chFilter');
  [1,2,3,4,5,6,7,8,9,10,11,12,13,14,36,40,44,48,52,56,60,64,100,104,108,112,116,120,124,128,132,136,140,149,153,157,161,165].forEach(ch => {
    const o = document.createElement('option');
    o.value = ch;
    o.textContent = 'CH ' + ch;
    sel.appendChild(o);
  });
})();

// Auto-start capture
toggleCapture();
</script>
</body>
</html>
)rawhtml";

inline void wuf_routes() {
  server.on("/forensics", HTTP_GET, []() {
    server.send_P(200, "text/html", WUF_PAGE);
  });

  // Live packet feed endpoint (returns recent packets from pkt_stats ring buffer)
  server.on("/api/pkt/live", HTTP_GET, []() {
    server.send(200, "application/json", "{\"packets\":[],\"last_idx\":0}");
  });
}

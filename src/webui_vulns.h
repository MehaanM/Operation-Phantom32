#pragma once
// webui_vulns.h — Vulnerability scanner results dashboard.
// Full web UI with sortable table, CVSS score bars, detail panels,
// remediation advice, and export. Prefix: wuv_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUV_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Phantom32 · Vulnerabilities</title>
<style>
:root{--bg:#0a0e1a;--surface:#111827;--surface2:#1a2235;--border:#2d3748;--accent:#00d4ff;--accent2:#7c3aed;--accent3:#10b981;--accent4:#f59e0b;--accent5:#ef4444;--text:#e2e8f0;--muted:#64748b;}
*{margin:0;padding:0;box-sizing:border-box;}
body{background:var(--bg);color:var(--text);font-family:'Segoe UI',system-ui,sans-serif;font-size:13px;}
header{background:var(--surface);border-bottom:1px solid var(--border);padding:10px 16px;display:flex;align-items:center;gap:12px;flex-wrap:wrap;}
header h1{font-size:16px;font-weight:700;color:var(--accent);}
.badge{background:var(--accent5);color:#fff;padding:2px 8px;border-radius:12px;font-size:11px;}
nav a{color:var(--muted);text-decoration:none;padding:6px 10px;border-radius:6px;font-size:12px;}
nav a:hover,nav a.active{background:var(--surface2);color:var(--accent);}
.toolbar{display:flex;gap:8px;padding:10px 14px;background:var(--surface);border-bottom:1px solid var(--border);align-items:center;flex-wrap:wrap;}
input[type=text]{background:var(--surface2);border:1px solid var(--border);color:var(--text);padding:5px 10px;border-radius:6px;font-size:12px;width:220px;}
input[type=text]:focus{outline:none;border-color:var(--accent);}
select{background:var(--surface2);border:1px solid var(--border);color:var(--text);padding:5px 8px;border-radius:6px;font-size:12px;}
.btn{background:var(--surface2);border:1px solid var(--border);color:var(--text);padding:5px 12px;border-radius:6px;cursor:pointer;font-size:12px;transition:all .2s;}
.btn:hover{background:var(--border);}
.btn.active{background:var(--accent);color:#000;}
.score-summary{display:flex;gap:12px;padding:12px 14px;border-bottom:1px solid var(--border);}
.score-box{background:var(--surface);border:1px solid var(--border);border-radius:8px;padding:10px 16px;text-align:center;min-width:80px;}
.score-box .n{font-size:24px;font-weight:700;}
.score-box .l{font-size:11px;color:var(--muted);margin-top:2px;}
.score-box.critical .n{color:var(--accent5);}
.score-box.high .n{color:#f97316;}
.score-box.medium .n{color:var(--accent4);}
.score-box.low .n{color:var(--accent3);}
.score-box.info .n{color:var(--muted);}
.table-wrap{overflow-x:auto;padding:14px;}
table{width:100%;border-collapse:collapse;}
th{color:var(--muted);text-align:left;padding:8px 10px;border-bottom:2px solid var(--border);font-size:11px;font-weight:600;cursor:pointer;white-space:nowrap;}
th:hover{color:var(--accent);}
th .sort-arrow{margin-left:4px;opacity:.4;}
th.sorted .sort-arrow{opacity:1;color:var(--accent);}
td{padding:8px 10px;border-bottom:1px solid var(--border);}
tr:hover td{background:var(--surface2);}
tr.selected td{background:#00d4ff08;border-left:2px solid var(--accent);}
.severity-badge{padding:2px 8px;border-radius:12px;font-size:10px;font-weight:700;text-transform:uppercase;}
.sev-critical{background:#ef44441a;color:var(--accent5);border:1px solid #ef444430;}
.sev-high{background:#f974631a;color:#f97463;border:1px solid #f9746330;}
.sev-medium{background:#f59e0b1a;color:var(--accent4);border:1px solid #f59e0b30;}
.sev-low{background:#10b9811a;color:var(--accent3);border:1px solid #10b98130;}
.sev-info{background:#64748b1a;color:var(--muted);border:1px solid #64748b30;}
.score-bar-bg{width:80px;height:6px;background:var(--surface2);border-radius:3px;overflow:hidden;display:inline-block;vertical-align:middle;}
.score-bar-fill{height:100%;border-radius:3px;}
.detail-panel{position:fixed;right:0;top:0;bottom:0;width:380px;background:var(--surface);border-left:1px solid var(--border);padding:20px;overflow-y:auto;transform:translateX(100%);transition:transform .3s;z-index:50;}
.detail-panel.open{transform:none;}
.detail-close{cursor:pointer;color:var(--muted);float:right;font-size:18px;}
.detail-close:hover{color:var(--text);}
.detail-h{font-size:14px;font-weight:700;margin-bottom:14px;color:var(--accent);}
.detail-row{display:flex;gap:8px;padding:5px 0;border-bottom:1px solid var(--border);}
.detail-key{color:var(--muted);width:130px;flex-shrink:0;font-size:12px;}
.detail-val{color:var(--text);font-size:12px;word-break:break-all;}
.remediation{background:var(--surface2);border:1px solid var(--border);border-radius:6px;padding:12px;margin-top:14px;}
.remediation h3{color:var(--accent3);font-size:12px;margin-bottom:8px;}
.remediation p{font-size:12px;color:var(--muted);line-height:1.6;}
.remediation ul{padding-left:16px;font-size:12px;color:var(--muted);line-height:1.8;}
.cvss-gauge{margin:14px 0;}
.cvss-gauge-track{height:10px;background:var(--surface2);border-radius:5px;overflow:hidden;position:relative;}
.cvss-gauge-fill{height:100%;border-radius:5px;transition:width .5s;}
.cvss-score-label{font-size:20px;font-weight:700;margin-bottom:4px;}
.empty{text-align:center;padding:60px;color:var(--muted);}
.empty .icon{font-size:48px;margin-bottom:12px;}
.pagination{display:flex;gap:6px;align-items:center;padding:10px 14px;border-top:1px solid var(--border);}
.page-btn{background:var(--surface2);border:1px solid var(--border);color:var(--text);padding:4px 10px;border-radius:4px;cursor:pointer;font-size:11px;}
.page-btn.active{background:var(--accent);color:#000;border-color:var(--accent);}
.page-btn:hover:not(.active){background:var(--border);}
</style>
</head>
<body>
<header>
  <h1>⚡ Phantom32</h1>
  <span class="badge">VULNERABILITIES</span>
  <span id="last-update" style="color:var(--muted);font-size:11px;margin-left:auto;"></span>
  <nav style="display:flex;gap:2px;">
    <a href="/">Home</a>
    <a href="/analytics">Analytics</a>
    <a href="/forensics">Forensics</a>
    <a href="/ble-iot">BLE/IoT</a>
    <a href="/cli">CLI</a>
  </nav>
</header>

<div class="score-summary" id="scoreSummary">
  <div class="score-box critical"><div class="n" id="cnt-critical">0</div><div class="l">Critical</div></div>
  <div class="score-box high"><div class="n" id="cnt-high">0</div><div class="l">High</div></div>
  <div class="score-box medium"><div class="n" id="cnt-medium">0</div><div class="l">Medium</div></div>
  <div class="score-box low"><div class="n" id="cnt-low">0</div><div class="l">Low</div></div>
  <div class="score-box info"><div class="n" id="cnt-info">0</div><div class="l">Info</div></div>
  <div class="score-box" style="margin-left:auto;">
    <div class="n" id="cnt-total" style="color:var(--accent)">0</div>
    <div class="l">Total</div>
  </div>
</div>

<div class="toolbar">
  <input type="text" id="searchInput" placeholder="Search SSID / MAC / type" oninput="renderTable()">
  <select id="sevFilter" onchange="renderTable()">
    <option value="">All Severities</option>
    <option value="critical">Critical</option>
    <option value="high">High</option>
    <option value="medium">Medium</option>
    <option value="low">Low</option>
    <option value="info">Info</option>
  </select>
  <select id="typeFilter" onchange="renderTable()">
    <option value="">All Types</option>
    <option value="open_ap">Open AP</option>
    <option value="wep">WEP Encryption</option>
    <option value="wps_enabled">WPS Enabled</option>
    <option value="default_ssid">Default SSID</option>
    <option value="hidden_ssid">Hidden SSID</option>
    <option value="deauth_vuln">Deauth Vulnerable</option>
    <option value="pmkid_vuln">PMKID Extractable</option>
    <option value="old_crypto">Outdated Crypto</option>
  </select>
  <button class="btn active" onclick="loadVulns()">Refresh</button>
  <button class="btn" onclick="exportVulns()">Export JSON</button>
  <button class="btn" onclick="exportCsv()">Export CSV</button>
</div>

<div class="table-wrap">
  <table id="vulnTable">
    <thead>
      <tr>
        <th onclick="sortBy('id')"># <span class="sort-arrow">↕</span></th>
        <th onclick="sortBy('severity')">Severity <span class="sort-arrow">↕</span></th>
        <th onclick="sortBy('score')">Score <span class="sort-arrow">↕</span></th>
        <th onclick="sortBy('type')">Vulnerability Type <span class="sort-arrow">↕</span></th>
        <th onclick="sortBy('ssid')">SSID <span class="sort-arrow">↕</span></th>
        <th onclick="sortBy('bssid')">BSSID <span class="sort-arrow">↕</span></th>
        <th onclick="sortBy('channel')">CH <span class="sort-arrow">↕</span></th>
        <th>Details</th>
        <th onclick="sortBy('first_seen')">First Seen <span class="sort-arrow">↕</span></th>
      </tr>
    </thead>
    <tbody id="vulnTbody"></tbody>
  </table>
  <div id="emptyState" class="empty" style="display:none;">
    <div class="icon">🛡️</div>
    <div>No vulnerabilities found</div>
    <div style="margin-top:8px;font-size:12px;">Run a scan to discover APs and assess their security</div>
  </div>
</div>

<div class="pagination" id="pagination"></div>

<div class="detail-panel" id="detailPanel">
  <span class="detail-close" onclick="closeDetail()">✕</span>
  <div class="detail-h" id="detail-title">Vulnerability Detail</div>
  <div id="detail-body"></div>
</div>

<script>
let vulns = [];
let filtered = [];
let sortKey = 'score';
let sortDesc = true;
let page = 0;
const PAGE_SIZE = 25;
let selectedId = -1;

const SEV_ORDER = {critical:4, high:3, medium:2, low:1, info:0};
const TYPE_LABELS = {
  open_ap: 'Open AP (No Encryption)',
  wep: 'WEP Encryption (Broken)',
  wps_enabled: 'WPS Enabled',
  default_ssid: 'Default/Generic SSID',
  hidden_ssid: 'Hidden SSID',
  deauth_vuln: 'Deauth Attack Vulnerable',
  pmkid_vuln: 'PMKID Extractable',
  old_crypto: 'Outdated Crypto (TKIP/RC4)',
  weak_pass: 'Likely Weak Password',
  rogue_ap: 'Potential Rogue AP',
  enterprise_mismatch: 'Enterprise Config Mismatch',
  no_mfp: 'No Management Frame Protection',
};

const REMEDIATIONS = {
  open_ap: 'Enable WPA2 or WPA3 encryption immediately. Open networks transmit all data in plaintext, allowing passive eavesdropping.',
  wep: 'Replace WEP with WPA2-AES or WPA3-SAE. WEP can be cracked in minutes with freely available tools.',
  wps_enabled: 'Disable WPS (Wi-Fi Protected Setup) in AP settings. WPS PIN brute-force attacks can recover network password in hours.',
  default_ssid: 'Change the network name from factory defaults. Default SSIDs often indicate default passwords are still in use.',
  deauth_vuln: 'Enable Management Frame Protection (802.11w / MFP) if supported by all clients to prevent deauthentication attacks.',
  pmkid_vuln: 'Use WPA3-SAE which is not vulnerable to PMKID attacks. For WPA2, enforce a strong unique passphrase.',
  old_crypto: 'Upgrade from TKIP to CCMP/AES. TKIP has known vulnerabilities and is deprecated.',
  hidden_ssid: 'Hidden SSIDs provide no real security benefit and complicate legitimate connectivity. Consider using a visible SSID with strong encryption.',
  no_mfp: 'Enable Management Frame Protection (802.11w) to prevent unauthenticated deauth/disassoc attacks.',
  weak_pass: 'Use a randomly generated passphrase of at least 20 characters with mixed character types.',
  rogue_ap: 'Investigate — this AP SSID matches a known network but has a different BSSID. Could be an evil twin attack.',
};

function sevColor(s) {
  if (s === 'critical') return 'var(--accent5)';
  if (s === 'high') return '#f97463';
  if (s === 'medium') return 'var(--accent4)';
  if (s === 'low') return 'var(--accent3)';
  return 'var(--muted)';
}

function scoreColor(n) {
  if (n >= 90) return 'var(--accent5)';
  if (n >= 70) return '#f97463';
  if (n >= 40) return 'var(--accent4)';
  if (n >= 10) return 'var(--accent3)';
  return 'var(--muted)';
}

function applyFilter() {
  const search = (document.getElementById('searchInput').value || '').toLowerCase();
  const sev = document.getElementById('sevFilter').value;
  const type = document.getElementById('typeFilter').value;
  filtered = vulns.filter(v => {
    if (sev && v.severity !== sev) return false;
    if (type && v.type !== type) return false;
    if (search) {
      const t = (v.ssid + v.bssid + v.type + v.severity).toLowerCase();
      if (!t.includes(search)) return false;
    }
    return true;
  });
  filtered.sort((a, b) => {
    let av = a[sortKey], bv = b[sortKey];
    if (sortKey === 'severity') { av = SEV_ORDER[av] || 0; bv = SEV_ORDER[bv] || 0; }
    if (sortDesc) return bv > av ? 1 : -1;
    return av > bv ? 1 : -1;
  });
  page = 0;
  renderTable();
}

function renderTable() {
  applyFilterLocal();
}

function applyFilterLocal() {
  const search = (document.getElementById('searchInput').value || '').toLowerCase();
  const sev = document.getElementById('sevFilter').value;
  const type = document.getElementById('typeFilter').value;
  filtered = vulns.filter(v => {
    if (sev && v.severity !== sev) return false;
    if (type && v.type !== type) return false;
    if (search) {
      const t = ((v.ssid||'') + (v.bssid||'') + (v.type||'')).toLowerCase();
      if (!t.includes(search)) return false;
    }
    return true;
  });
  filtered.sort((a, b) => {
    let av = a[sortKey], bv = b[sortKey];
    if (sortKey === 'severity') { av = SEV_ORDER[av] || 0; bv = SEV_ORDER[bv] || 0; }
    if (sortDesc) return (bv > av ? 1 : bv < av ? -1 : 0);
    return (av > bv ? 1 : av < bv ? -1 : 0);
  });
  renderPage();
}

function renderPage() {
  const start = page * PAGE_SIZE;
  const slice = filtered.slice(start, start + PAGE_SIZE);
  const tbody = document.getElementById('vulnTbody');

  if (!filtered.length) {
    tbody.innerHTML = '';
    document.getElementById('emptyState').style.display = '';
    document.getElementById('pagination').innerHTML = '';
    return;
  }
  document.getElementById('emptyState').style.display = 'none';

  tbody.innerHTML = slice.map((v, i) => {
    const globalIdx = start + i;
    const col = scoreColor(v.score || 0);
    const ts = v.first_seen ? new Date(v.first_seen * 1000).toLocaleString() : '—';
    return `<tr class="${selectedId === v.id ? 'selected' : ''}" onclick="showDetail(${v.id})">
      <td style="color:var(--muted)">${globalIdx + 1}</td>
      <td><span class="severity-badge sev-${v.severity}">${v.severity}</span></td>
      <td>
        <div style="display:flex;align-items:center;gap:6px;">
          <div class="score-bar-bg"><div class="score-bar-fill" style="width:${v.score||0}%;background:${col}"></div></div>
          <span style="color:${col};font-weight:700;font-size:12px;">${v.score||0}</span>
        </div>
      </td>
      <td>${TYPE_LABELS[v.type] || v.type || '—'}</td>
      <td>${v.ssid || '<hidden>'}</td>
      <td style="font-family:monospace;font-size:11px;">${v.bssid || '—'}</td>
      <td>${v.channel || '—'}</td>
      <td style="color:var(--muted);font-size:11px;">${(v.detail||'').substring(0,40)}</td>
      <td style="font-size:11px;color:var(--muted);">${ts}</td>
    </tr>`;
  }).join('');

  renderPagination();
  updateCounts();
}

function renderPagination() {
  const total_pages = Math.ceil(filtered.length / PAGE_SIZE);
  if (total_pages <= 1) { document.getElementById('pagination').innerHTML = ''; return; }
  let html = `<span style="color:var(--muted);font-size:11px;">${filtered.length} results</span>`;
  for (let p = 0; p < total_pages; p++) {
    html += `<button class="page-btn${p === page ? ' active' : ''}" onclick="goPage(${p})">${p+1}</button>`;
  }
  document.getElementById('pagination').innerHTML = html;
}

function goPage(p) { page = p; renderPage(); }

function sortBy(key) {
  if (sortKey === key) sortDesc = !sortDesc;
  else { sortKey = key; sortDesc = true; }
  document.querySelectorAll('th').forEach(th => th.classList.remove('sorted'));
  renderTable();
}

function updateCounts() {
  const all = vulns;
  document.getElementById('cnt-critical').textContent = all.filter(v => v.severity === 'critical').length;
  document.getElementById('cnt-high').textContent     = all.filter(v => v.severity === 'high').length;
  document.getElementById('cnt-medium').textContent   = all.filter(v => v.severity === 'medium').length;
  document.getElementById('cnt-low').textContent      = all.filter(v => v.severity === 'low').length;
  document.getElementById('cnt-info').textContent     = all.filter(v => v.severity === 'info').length;
  document.getElementById('cnt-total').textContent    = all.length;
}

function showDetail(id) {
  selectedId = id;
  const v = vulns.find(x => x.id === id);
  if (!v) return;
  document.querySelectorAll('tr').forEach(r => r.classList.remove('selected'));
  document.getElementById('detail-title').textContent = TYPE_LABELS[v.type] || v.type;
  const col = scoreColor(v.score || 0);
  const fields = [
    ['Severity', `<span class="severity-badge sev-${v.severity}">${v.severity}</span>`],
    ['CVSS Score', `<div class="cvss-gauge"><div class="cvss-score-label" style="color:${col}">${v.score||0}/100</div><div class="cvss-gauge-track"><div class="cvss-gauge-fill" style="width:${v.score||0}%;background:${col}"></div></div></div>`],
    ['SSID', v.ssid || '<hidden>'],
    ['BSSID', `<span style="font-family:monospace">${v.bssid || '—'}</span>`],
    ['Channel', v.channel || '—'],
    ['Security', v.security || '—'],
    ['First Seen', v.first_seen ? new Date(v.first_seen*1000).toLocaleString() : '—'],
    ['Last Seen', v.last_seen ? new Date(v.last_seen*1000).toLocaleString() : '—'],
    ['Detail', v.detail || '—'],
  ];
  document.getElementById('detail-body').innerHTML =
    fields.map(([k,val]) => `<div class="detail-row"><div class="detail-key">${k}</div><div class="detail-val">${val}</div></div>`).join('') +
    `<div class="remediation"><h3>🔧 Remediation</h3><p>${REMEDIATIONS[v.type] || 'Review AP configuration and apply security best practices.'}</p></div>`;
  document.getElementById('detailPanel').classList.add('open');
  renderPage();
}

function closeDetail() {
  selectedId = -1;
  document.getElementById('detailPanel').classList.remove('open');
  renderPage();
}

async function loadVulns() {
  try {
    const r = await fetch('/api/vuln/list');
    const data = await r.json();
    vulns = (data.vulns || data.results || []).map((v, i) => ({ id: i, ...v }));
    renderTable();
    document.getElementById('last-update').textContent = 'Updated: ' + new Date().toLocaleTimeString();
  } catch (e) {
    document.getElementById('last-update').textContent = 'Fetch error';
  }
}

function exportVulns() {
  const blob = new Blob([JSON.stringify(vulns, null, 2)], { type: 'application/json' });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = 'phantom32-vulns.json';
  a.click();
}

function exportCsv() {
  const header = 'severity,score,type,ssid,bssid,channel,security,detail,first_seen\n';
  const rows = vulns.map(v =>
    [v.severity, v.score, v.type, `"${(v.ssid||'').replace(/"/g,'""')}"`, v.bssid, v.channel,
     v.security, `"${(v.detail||'').replace(/"/g,'""')}"`, v.first_seen].join(',')
  ).join('\n');
  const blob = new Blob([header + rows], { type: 'text/csv' });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = 'phantom32-vulns.csv';
  a.click();
}

loadVulns();
setInterval(loadVulns, 15000);
</script>
</body>
</html>
)rawhtml";

inline void wuv_routes() {
  server.on("/vulns", HTTP_GET, []() {
    server.send_P(200, "text/html", WUV_PAGE);
  });
}

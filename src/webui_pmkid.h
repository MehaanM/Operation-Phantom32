#pragma once
// webui_pmkid.h — PMKID harvester UI.
// Route: /pmkid. Prefix: wupmk_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUPMK_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>PMKID Harvester — Phantom32</title>
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
.hero{display:grid;grid-template-columns:repeat(3,1fr);gap:10px;margin-bottom:16px;}
.stat{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:13px;}
.stat label{font-size:10px;color:var(--muted);text-transform:uppercase;}
.stat .val{font-size:22px;font-weight:800;color:var(--accent);margin-top:3px;}
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:18px;margin-bottom:14px;}
.section h3{font-size:12px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:14px;}
.warning-box{background:rgba(239,68,68,.06);border:1px solid rgba(239,68,68,.3);border-radius:7px;padding:12px;margin-bottom:14px;font-size:12px;line-height:1.6;}
table{width:100%;border-collapse:collapse;font-size:12px;}
th{text-align:left;padding:7px 8px;font-size:10px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;}
td{padding:7px 8px;border-bottom:1px solid var(--border);}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
tr.has-pmkid{background:rgba(34,197,94,.04);}
tr.has-pmkid:hover td{background:rgba(34,197,94,.08);}
.badge{padding:2px 7px;border-radius:4px;font-size:9px;font-weight:700;display:inline-block;}
.badge-have{background:rgba(34,197,94,.18);color:var(--success);}
.badge-eapol{background:rgba(0,212,255,.12);color:var(--accent);}
.pmkid-hex{font-family:monospace;font-size:9px;word-break:break-all;color:var(--success);}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.btn-export{background:rgba(0,212,255,.12);border:1px solid var(--accent);color:var(--accent);}
.btn-export:hover{background:rgba(0,212,255,.2);}
.action-row{display:flex;gap:8px;margin-top:12px;flex-wrap:wrap;}
@media(max-width:700px){.hero{grid-template-columns:1fr}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/pmf">PMF/WPA3</a>
    <a href="/pmkid" class="active">PMKID</a>
    <a href="/security-audit">Audit</a>
  </nav>
</div>
<div class="container">
  <h1>PMKID Harvester</h1>
  <p class="subtitle">Passive collection of PMKIDs from EAPOL key frame 1/4. PMKID = HMAC-SHA1-128(PMK, "PMK Name" ∥ AP_MAC ∥ Client_MAC). Captured without deauthentication.</p>

  <div class="warning-box">
    <strong style="color:var(--danger)">Authorization Required:</strong> PMKID capture and offline cracking requires explicit authorization from the network owner.
    PMKIDs are captured passively during normal client authentication. Use only on networks you own or are authorized to test.
  </div>

  <div class="hero">
    <div class="stat"><label>EAPOL Sessions</label><div class="val" id="h-cnt">0</div></div>
    <div class="stat"><label>PMKIDs Captured</label><div class="val" id="h-pmk" style="color:var(--success)">0</div></div>
    <div class="stat"><label>Total Seen</label><div class="val" id="h-total">0</div></div>
  </div>

  <div class="section">
    <h3>Captured PMKID Records</h3>
    <table>
      <thead>
        <tr>
          <th>AP BSSID</th><th>SSID</th><th>Client</th>
          <th>CH</th><th>RSSI</th><th>AKM</th><th>Cipher</th>
          <th>Status</th><th>PMKID</th>
        </tr>
      </thead>
      <tbody id="pmk-tbody">
        <tr><td colspan="9" style="color:var(--muted);text-align:center">Waiting for EAPOL frames…</td></tr>
      </tbody>
    </table>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-export" onclick="exportHcxpcapng()">Export hashcat format</button>
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>
</div>

<script>
let g_records = [];

function rssiColor(r) {
  if (r >= -60) return 'var(--success)';
  if (r >= -75) return 'var(--warn)';
  return 'var(--danger)';
}

async function refresh() {
  const r = await fetch('/api/pmk/all').then(x => x.json()).catch(() => ({}));
  g_records = r.records || [];
  document.getElementById('h-cnt').textContent   = r.count || 0;
  document.getElementById('h-total').textContent = r.total_seen || 0;
  document.getElementById('h-pmk').textContent   = g_records.filter(r => r.has_pmkid).length;

  document.getElementById('pmk-tbody').innerHTML = g_records.length
    ? g_records.map(r => `<tr class="${r.has_pmkid ? 'has-pmkid' : ''}">
        <td><code style="font-size:10px">${r.ap}</code></td>
        <td>${r.ssid || '<em style="color:var(--muted)">Hidden</em>'}</td>
        <td><code style="font-size:10px">${r.client}</code></td>
        <td>${r.ch}</td>
        <td style="color:${rssiColor(r.rssi)}">${r.rssi}</td>
        <td style="font-size:10px">${r.akm}</td>
        <td style="font-size:10px">${r.pairwise_cipher}</td>
        <td>${r.has_pmkid
          ? '<span class="badge badge-have">PMKID ✓</span>'
          : '<span class="badge badge-eapol">EAPOL only</span>'}</td>
        <td>${r.has_pmkid
          ? `<div class="pmkid-hex">${r.pmkid.match(/.{1,8}/g).join(' ')}</div>`
          : '<span style="color:var(--muted);font-size:10px">—</span>'}</td>
      </tr>`).join('')
    : '<tr><td colspan="9" style="color:var(--muted);text-align:center">No data.</td></tr>';
}

function exportHcxpcapng() {
  const pmkids = g_records.filter(r => r.has_pmkid);
  if (!pmkids.length) {
    alert('No PMKIDs captured yet.');
    return;
  }
  // Export in hashcat WPA-PMKID format: pmkid*ap_mac*client_mac*ssid_hex
  const lines = pmkids.map(r => {
    const apMac  = r.ap.replace(/:/g,'');
    const cliMac = r.client.replace(/:/g,'');
    const ssidHex = Array.from(r.ssid).map(c=>c.charCodeAt(0).toString(16).padStart(2,'0')).join('');
    return `${r.pmkid}*${apMac}*${cliMac}*${ssidHex}`;
  });
  const blob = new Blob([lines.join('\n')], {type:'text/plain'});
  const a = document.createElement('a');
  a.href = URL.createObjectURL(blob);
  a.download = 'phantom32_pmkids.hc22000';
  a.click();
}

async function clearData() {
  await fetch('/api/pmk/clear', {method: 'POST'});
  refresh();
}

refresh();
setInterval(refresh, 4000);
</script>
</body>
</html>
)rawhtml";

inline void wupmk_routes() {
  server.on("/pmkid", HTTP_GET, []() { server.send_P(200, "text/html", WUPMK_PAGE); });
}

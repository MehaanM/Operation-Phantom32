#pragma once
// webui_vendor.h — OUI / Vendor Lookup UI.
// Route: /vendor-lookup. Prefix: wuvnd_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUVND_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Vendor Lookup — Phantom32</title>
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
.container{max-width:900px;margin:0 auto;padding:20px 16px;}
h1{font-size:22px;font-weight:700;margin-bottom:4px;}
.subtitle{color:var(--muted);font-size:13px;margin-bottom:20px;}
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:20px;margin-bottom:14px;}
.section h3{font-size:12px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:14px;}
.search-row{display:flex;gap:8px;margin-bottom:16px;}
.search-input{flex:1;background:var(--bg3);border:1px solid var(--border);border-radius:6px;
  color:var(--text);padding:10px 14px;font-size:13px;outline:none;}
.search-input:focus{border-color:var(--accent);}
.btn{padding:10px 16px;border:none;border-radius:6px;font-size:12px;font-weight:600;cursor:pointer;}
.btn-primary{background:var(--accent);color:#0a0c10;}
.btn-primary:hover{filter:brightness(1.15);}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.result-card{background:var(--bg3);border:1px solid var(--border);border-radius:7px;padding:16px;margin-bottom:10px;}
.result-mac{font-size:18px;font-weight:700;color:var(--accent);font-family:monospace;}
.result-vendor{font-size:14px;color:var(--text);margin-top:4px;}
.result-oui{font-size:10px;color:var(--muted);margin-top:2px;}
.hint{font-size:10px;color:var(--muted);margin-top:6px;}
table{width:100%;border-collapse:collapse;font-size:10px;}
th{text-align:left;padding:7px 10px;border-bottom:2px solid var(--border);color:var(--muted);font-size:9px;text-transform:uppercase;}
td{padding:7px 10px;border-bottom:1px solid var(--border);font-family:monospace;}
td.vendor{font-family:'Inter',system-ui,sans-serif;}
tr:hover td{background:rgba(42,48,69,.5);}
.vendor-apple{color:#a8b0c0;}
.vendor-samsung{color:#2196f3;}
.vendor-cisco{color:#049fd9;}
.vendor-intel{color:#00c7fd;}
.vendor-netgear{color:#84cc16;}
.vendor-xiaomi{color:#f59e0b;}
.vendor-espressif{color:#ef4444;}
.stat-box{display:grid;grid-template-columns:1fr 1fr 1fr;gap:10px;margin-bottom:16px;}
.sb{background:var(--bg3);border:1px solid var(--border);border-radius:6px;padding:12px;text-align:center;}
.sb label{font-size:9px;color:var(--muted);text-transform:uppercase;}
.sb .v{font-size:18px;font-weight:800;color:var(--accent);margin-top:3px;}
textarea{width:100%;background:var(--bg3);border:1px solid var(--border);border-radius:6px;
  color:var(--text);padding:10px;font-size:11px;font-family:monospace;resize:vertical;outline:none;}
textarea:focus{border-color:var(--accent);}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/vendor-lookup" class="active">Vendor Lookup</a>
    <a href="/scan-cache">Scan Cache</a>
    <a href="/topology">Topology</a>
  </nav>
</div>
<div class="container">
  <h1>OUI / Vendor Lookup</h1>
  <p class="subtitle">Identify network device manufacturers from MAC addresses (OUI — Organizationally Unique Identifier). Database covers 800+ OUI entries for major vendors including Apple, Samsung, Cisco, Intel, NETGEAR, Xiaomi, and Espressif.</p>

  <div class="stat-box">
    <div class="sb"><label>OUI Database</label><div class="v" id="s-oui">—</div></div>
    <div class="sb"><label>Lookups</label><div class="v" id="s-lookups">0</div></div>
    <div class="sb"><label>Matched</label><div class="v" id="s-matched">0</div></div>
  </div>

  <div class="section">
    <h3>Single MAC Lookup</h3>
    <div class="search-row">
      <input class="search-input" id="mac-input" placeholder="AA:BB:CC:DD:EE:FF or AABBCCDDEEFF" spellcheck="false">
      <button class="btn btn-primary" onclick="lookup()">Lookup</button>
    </div>
    <div id="result-area"></div>
    <div class="hint">Accepts colon-separated (AA:BB:CC) or plain hex (AABBCC) format.</div>
  </div>

  <div class="section">
    <h3>Batch Lookup</h3>
    <p style="font-size:11px;color:var(--muted);margin-bottom:10px">Paste multiple MAC addresses (one per line) for bulk identification.</p>
    <textarea id="batch-input" rows="6" placeholder="AA:BB:CC:DD:EE:FF&#10;11:22:33:44:55:66&#10;..."></textarea>
    <div style="display:flex;gap:8px;margin-top:8px">
      <button class="btn btn-primary" onclick="batchLookup()">Lookup All</button>
      <button class="btn btn-ghost" onclick="clearBatch()">Clear</button>
    </div>
    <div id="batch-results" style="margin-top:14px"></div>
  </div>
</div>

<script>
let lookups = 0, matched = 0;

function vendorClass(vendor) {
  if (!vendor) return '';
  const v = vendor.toLowerCase();
  if (v.includes('apple'))     return 'vendor-apple';
  if (v.includes('samsung'))   return 'vendor-samsung';
  if (v.includes('cisco'))     return 'vendor-cisco';
  if (v.includes('intel'))     return 'vendor-intel';
  if (v.includes('netgear'))   return 'vendor-netgear';
  if (v.includes('xiaomi'))    return 'vendor-xiaomi';
  if (v.includes('espressif')) return 'vendor-espressif';
  return '';
}

async function loadInfo() {
  const r = await fetch('/api/vendor/info').then(x=>x.json()).catch(()=>({}));
  if (r.oui_count) document.getElementById('s-oui').textContent = r.oui_count.toLocaleString();
}

async function lookup() {
  const mac = document.getElementById('mac-input').value.trim();
  if (!mac) return;
  const r = await fetch('/api/vendor?mac=' + encodeURIComponent(mac)).then(x=>x.json()).catch(e => ({error:e.toString()}));
  lookups++;
  document.getElementById('s-lookups').textContent = lookups;
  if (r.vendor && r.vendor !== 'Unknown') { matched++; document.getElementById('s-matched').textContent = matched; }
  const known = r.vendor && r.vendor !== 'Unknown';
  document.getElementById('result-area').innerHTML = r.error
    ? `<div style="color:var(--danger);font-size:12px">Error: ${r.error}</div>`
    : `<div class="result-card">
        <div class="result-mac">${r.mac||mac}</div>
        <div class="result-vendor ${vendorClass(r.vendor)}" style="color:${known?'var(--success)':'var(--muted)'}">${r.vendor||'Unknown'}</div>
        <div class="result-oui">OUI: ${r.oui||''}</div>
      </div>`;
}

async function batchLookup() {
  const lines = document.getElementById('batch-input').value;
  if (!lines.trim()) return;
  const r = await fetch('/api/vendor/batch', {method:'POST', body:lines, headers:{'Content-Type':'text/plain'}})
    .then(x=>x.json()).catch(()=>[]);
  lookups += r.length;
  const m = r.filter(x=>x.vendor&&x.vendor!=='Unknown').length;
  matched += m;
  document.getElementById('s-lookups').textContent = lookups;
  document.getElementById('s-matched').textContent = matched;

  document.getElementById('batch-results').innerHTML = r.length
    ? `<table>
        <thead><tr><th>MAC</th><th>OUI</th><th>Vendor</th></tr></thead>
        <tbody>` +
        r.map(x => {
          const known = x.vendor && x.vendor !== 'Unknown';
          return `<tr>
            <td>${x.mac}</td>
            <td>${x.mac.substring(0,8)}</td>
            <td class="vendor ${vendorClass(x.vendor)}" style="color:${known?'var(--text)':'var(--muted)'}">${x.vendor||'Unknown'}</td>
          </tr>`;
        }).join('') +
        `</tbody></table>`
    : '<div style="color:var(--muted);font-size:11px">No results.</div>';
}

function clearBatch() {
  document.getElementById('batch-input').value = '';
  document.getElementById('batch-results').innerHTML = '';
}

document.getElementById('mac-input').addEventListener('keydown', e => {
  if (e.key === 'Enter') lookup();
});

loadInfo();
</script>
</body>
</html>
)rawhtml";

inline void wuvnd_routes() {
  server.on("/vendor-lookup", HTTP_GET, []() { server.send_P(200, "text/html", WUVND_PAGE); });
}

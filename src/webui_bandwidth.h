#pragma once
// webui_bandwidth.h — Client bandwidth monitoring web UI.
// Route: /bandwidth. Prefix: wubw_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUBW_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Bandwidth — Phantom32</title>
<style>
:root{--bg:#0a0c10;--bg2:#12151c;--bg3:#1a1f2e;--border:#2a3045;
  --accent:#00d4ff;--success:#22c55e;--warn:#f59e0b;--danger:#ef4444;
  --text:#e2e8f0;--muted:#64748b;--radius:8px;}
*{box-sizing:border-box;margin:0;padding:0;}
body{font-family:'Inter',system-ui,sans-serif;background:var(--bg);color:var(--text);min-height:100vh;}
.top-bar{display:flex;align-items:center;gap:12px;padding:14px 20px;
  background:var(--bg2);border-bottom:1px solid var(--border);position:sticky;top:0;z-index:10;}
.logo{font-size:18px;font-weight:700;color:var(--accent);letter-spacing:2px;}
.nav-links{display:flex;gap:8px;margin-left:auto;}
.nav-links a{color:var(--muted);text-decoration:none;font-size:13px;padding:5px 10px;border-radius:5px;transition:all .2s;}
.nav-links a:hover{color:var(--text);background:var(--bg3);}
.nav-links a.active{color:var(--accent);background:rgba(0,212,255,.08);}
.container{max-width:1100px;margin:0 auto;padding:24px 16px;}
h1{font-size:22px;font-weight:700;margin-bottom:4px;}
.subtitle{color:var(--muted);font-size:14px;margin-bottom:24px;}
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:20px;margin-bottom:16px;}
.section h3{font-size:13px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:16px;}
.stat-row{display:grid;grid-template-columns:repeat(4,1fr);gap:12px;margin-bottom:20px;}
.stat{background:var(--bg2);border:1px solid var(--border);border-radius:8px;padding:14px;}
.stat label{font-size:11px;color:var(--muted);text-transform:uppercase;}
.stat .val{font-size:22px;font-weight:700;color:var(--accent);margin-top:4px;}
.btn{padding:7px 14px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;transition:all .2s;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.btn-danger{background:var(--danger);color:#fff;}
.action-row{display:flex;gap:8px;flex-wrap:wrap;margin-top:14px;}
table{width:100%;border-collapse:collapse;font-size:13px;}
th{text-align:left;padding:7px 10px;font-size:11px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;}
td{padding:8px 10px;border-bottom:1px solid var(--border);vertical-align:middle;}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
.bar-bg{height:8px;border-radius:4px;background:var(--bg3);min-width:60px;}
.bar-tx{height:8px;border-radius:4px;background:#22c55e;}
.bar-rx{height:8px;border-radius:4px;background:#00d4ff;margin-top:2px;}
canvas{display:block;width:100%;border-radius:5px;background:var(--bg3);}
@media(max-width:700px){.stat-row{grid-template-columns:1fr 1fr;}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/">Dashboard</a>
    <a href="/analytics">Analytics</a>
    <a href="/devices">Devices</a>
    <a href="/channels">Channels</a>
    <a href="/bandwidth" class="active">Bandwidth</a>
    <a href="/settings">Settings</a>
  </nav>
</div>

<div class="container">
  <h1>Client Bandwidth</h1>
  <p class="subtitle">Per-client TX/RX rate estimates derived from observed frame sizes.</p>

  <div class="stat-row">
    <div class="stat"><label>Monitored Clients</label><div class="val" id="st-count">0</div></div>
    <div class="stat"><label>Total Bytes</label><div class="val" id="st-bytes">0</div></div>
    <div class="stat"><label>Top TX Rate</label><div class="val" id="st-tx">0 B/s</div></div>
    <div class="stat"><label>Top RX Rate</label><div class="val" id="st-rx">0 B/s</div></div>
  </div>

  <div class="section">
    <h3>Top Clients by Bandwidth</h3>
    <div style="overflow-x:auto;">
      <table>
        <thead><tr>
          <th>Client MAC</th><th>AP BSSID</th><th>TX Rate</th><th>RX Rate</th>
          <th>Total</th><th>Peak TX</th><th>TX/RX Bars</th><th>RSSI</th>
        </tr></thead>
        <tbody id="bw-tbody">
          <tr><td colspan="8" style="color:var(--muted);text-align:center">
            Waiting for data frames…</td></tr>
        </tbody>
      </table>
    </div>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-danger" onclick="clearAll()">Clear</button>
    </div>
  </div>

  <div class="section">
    <h3>Channel Quality</h3>
    <div id="cq-grid" style="display:grid;grid-template-columns:repeat(auto-fill,minmax(90px,1fr));gap:8px;"></div>
  </div>
</div>

<script>
function fmtBytes(b) {
  if (!b || b < 1000) return Math.round(b||0) + ' B/s';
  if (b < 1e6) return (b/1000).toFixed(1) + ' KB/s';
  return (b/1e6).toFixed(2) + ' MB/s';
}

function fmtBytesTotal(b) {
  if (!b || b < 1000) return b + ' B';
  if (b < 1e6) return (b/1000).toFixed(0) + ' KB';
  if (b < 1e9) return (b/1e6).toFixed(1) + ' MB';
  return (b/1e9).toFixed(2) + ' GB';
}

function rssiColor(r) {
  if (r > -60) return '#22c55e';
  if (r > -75) return '#f59e0b';
  return '#ef4444';
}

function txRxBars(txRate, rxRate, maxRate) {
  const txPct = maxRate > 0 ? Math.round(100 * txRate / maxRate) : 0;
  const rxPct = maxRate > 0 ? Math.round(100 * rxRate / maxRate) : 0;
  return `<div>
    <div title="TX ${fmtBytes(txRate)}" class="bar-bg" style="width:80px"><div class="bar-tx" style="width:${txPct}%"></div></div>
    <div title="RX ${fmtBytes(rxRate)}" class="bar-bg" style="width:80px"><div class="bar-rx" style="width:${rxPct}%"></div></div>
  </div>`;
}

async function refresh() {
  const r = await fetch('/api/cbw/top').then(x=>x.json()).catch(()=>[]);
  const all = await fetch('/api/cbw/all').then(x=>x.json()).catch(()=>({}));

  document.getElementById('st-count').textContent = all.count||0;
  document.getElementById('st-bytes').textContent = fmtBytesTotal(all.total_bytes||0);

  if (r.length) {
    const maxRate = Math.max(...r.map(c=>c.total_rate||0), 1);
    document.getElementById('st-tx').textContent = fmtBytes(Math.max(...r.map(c=>c.tx_rate||0)));
    document.getElementById('st-rx').textContent = fmtBytes(Math.max(...r.map(c=>c.rx_rate||0)));

    document.getElementById('bw-tbody').innerHTML = r.map(c=>`
      <tr>
        <td><code style="font-size:11px">${c.mac}</code></td>
        <td><code style="font-size:11px">${c.bssid}</code></td>
        <td style="color:#22c55e">${fmtBytes(c.tx_rate)}</td>
        <td style="color:#00d4ff">${fmtBytes(c.rx_rate)}</td>
        <td style="color:var(--accent)">${fmtBytes(c.total_rate)}</td>
        <td style="color:var(--warn)">${fmtBytes(c.peak_tx)}</td>
        <td>${txRxBars(c.tx_rate,c.rx_rate,maxRate)}</td>
        <td style="color:${rssiColor(c.rssi)}">${c.rssi} dBm</td>
      </tr>`).join('');
  } else {
    document.getElementById('bw-tbody').innerHTML =
      '<tr><td colspan="8" style="color:var(--muted);text-align:center">No data.</td></tr>';
  }

  // Channel quality grid
  const cq = await fetch('/api/cq/all').then(x=>x.json()).catch(()=>({}));
  if (cq.channels) {
    document.getElementById('cq-grid').innerHTML = cq.channels.map(c=>{
      const q = c.quality||0;
      const col = q>75?'#22c55e':q>50?'#f59e0b':'#ef4444';
      return `<div style="background:var(--bg3);border:1px solid var(--border);border-radius:6px;padding:10px;text-align:center">
        <div style="font-size:18px;font-weight:700;color:var(--accent)">ch${c.ch}</div>
        <div style="font-size:11px;color:var(--muted)">Quality</div>
        <div style="font-size:16px;font-weight:700;color:${col}">${q}</div>
        <div style="font-size:10px;color:var(--muted)">${(c.airtime||0).toFixed(0)}% air</div>
      </div>`;
    }).join('');
  }
}

async function clearAll() {
  if (!confirm('Clear bandwidth stats?')) return;
  await fetch('/api/cbw/clear',{method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 3000);
</script>
</body>
</html>
)rawhtml";

inline void wubw_routes() {
  server.on("/bandwidth", HTTP_GET, []() { server.send_P(200, "text/html", WUBW_PAGE); });
}

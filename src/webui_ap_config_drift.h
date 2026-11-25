#pragma once
// webui_ap_config_drift.h — AP configuration drift UI.
// Route: /ap-config-drift. Prefix: wuacd_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUACD_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>AP Config Drift — Phantom32</title>
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
.hero{display:grid;grid-template-columns:repeat(4,1fr);gap:10px;margin-bottom:16px;}
.stat{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:13px;}
.stat label{font-size:10px;color:var(--muted);text-transform:uppercase;}
.stat .val{font-size:22px;font-weight:800;color:var(--accent);margin-top:3px;}
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:18px;margin-bottom:14px;}
.section h3{font-size:12px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:14px;}
table{width:100%;border-collapse:collapse;font-size:12px;}
th{text-align:left;padding:7px 8px;font-size:10px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;}
td{padding:7px 8px;border-bottom:1px solid var(--border);}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
.change-row{background:var(--bg3);border:1px solid var(--border);border-radius:6px;padding:10px;margin-bottom:6px;font-size:12px;}
.change-row.chg-sec{border-left:3px solid var(--danger);}
.change-row.chg-ch{border-left:3px solid var(--warn);}
.change-row.chg-pmf{border-left:3px solid var(--accent);}
.change-row.chg-wps{border-left:3px solid #f97316;}
.change-row.chg-other{border-left:3px solid var(--muted);}
.badge{padding:2px 7px;border-radius:4px;font-size:9px;font-weight:700;display:inline-block;}
.chg-sec-b{background:rgba(239,68,68,.2);color:var(--danger);}
.chg-ch-b{background:rgba(245,158,11,.18);color:var(--warn);}
.chg-pmf-b{background:rgba(0,212,255,.12);color:var(--accent);}
.chg-wps-b{background:rgba(249,115,22,.2);color:#f97316;}
.chg-other-b{background:rgba(100,116,139,.18);color:var(--muted);}
.old-new{display:flex;align-items:center;gap:8px;margin-top:4px;font-size:11px;}
.old-val{color:var(--danger);text-decoration:line-through;}
.arrow{color:var(--muted);}
.new-val{color:var(--success);}
.tabs{display:flex;gap:4px;margin-bottom:14px;}
.tab{padding:5px 12px;border-radius:5px;font-size:12px;cursor:pointer;border:1px solid var(--border);color:var(--muted);}
.tab.active{background:rgba(0,212,255,.12);color:var(--accent);border-color:var(--accent);}
.btn{padding:6px 12px;border:none;border-radius:5px;font-size:12px;font-weight:500;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;margin-top:12px;}
.cap-pill{font-size:9px;padding:1px 5px;border-radius:3px;background:rgba(0,212,255,.1);color:var(--accent);margin-right:2px;}
@media(max-width:700px){.hero{grid-template-columns:1fr 1fr}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/security-audit">Security Audit</a>
    <a href="/ap-config-drift" class="active">Config Drift</a>
    <a href="/ap-vendors">AP Vendors</a>
  </nav>
</div>
<div class="container">
  <h1>AP Configuration Drift Monitor</h1>
  <p class="subtitle">Detects when APs change channel, security mode, PMF settings, or capability flags over time — may indicate misconfiguration or attack.</p>

  <div class="hero">
    <div class="stat"><label>APs Watched</label><div class="val" id="h-cnt">0</div></div>
    <div class="stat"><label>Changed APs</label><div class="val" id="h-changed" style="color:var(--warn)">0</div></div>
    <div class="stat"><label>Total Changes</label><div class="val" id="h-total">0</div></div>
    <div class="stat"><label>Sec Changes</label><div class="val" id="h-sec" style="color:var(--danger)">0</div></div>
  </div>

  <div class="section">
    <div class="tabs">
      <div class="tab active" onclick="showTab('ap',this)">AP Summary</div>
      <div class="tab" onclick="showTab('log',this)">Change Log</div>
    </div>

    <div id="tab-ap">
      <table>
        <thead>
          <tr>
            <th>BSSID</th><th>SSID</th><th>CH</th><th>Security</th>
            <th>Caps</th><th>PMF</th><th>WPS</th><th>Changes</th>
          </tr>
        </thead>
        <tbody id="ap-tbody">
          <tr><td colspan="8" style="color:var(--muted);text-align:center">No data.</td></tr>
        </tbody>
      </table>
    </div>

    <div id="tab-log" style="display:none">
      <div id="change-log"><div style="color:var(--muted);text-align:center;padding:20px;">No changes logged.</div></div>
    </div>

    <div class="action-row">
      <button class="btn btn-ghost" onclick="refresh()">Refresh</button>
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>
</div>

<script>
const SEC_NAMES = ['Open','WEP','WPA','WPA2','WPA3'];
const CHG_CLASS = {
  Security:'chg-sec', Channel:'chg-ch', PMF:'chg-pmf', WPS:'chg-wps'
};
const BADGE_CLASS = {
  Security:'chg-sec-b', Channel:'chg-ch-b', PMF:'chg-pmf-b', WPS:'chg-wps-b'
};

function showTab(name, el) {
  document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
  el.classList.add('active');
  document.getElementById('tab-ap').style.display  = name === 'ap'  ? '' : 'none';
  document.getElementById('tab-log').style.display = name === 'log' ? '' : 'none';
}

function capPills(c) {
  if (!c) return '';
  const caps = [];
  if (c.he)  caps.push('WiFi 6');
  if (c.vht) caps.push('AC');
  if (c.ht)  caps.push('N');
  return caps.map(cp => `<span class="cap-pill">${cp}</span>`).join('');
}

function pmfLabel(c) {
  if (!c) return '—';
  if (!c.pmf) return '<span style="color:var(--muted)">None</span>';
  if (c.pmf_req) return '<span style="color:var(--success)">Required</span>';
  return '<span style="color:var(--warn)">Optional</span>';
}

async function refresh() {
  const r = await fetch('/api/acd/all').then(x => x.json()).catch(() => ({}));
  const aps = r.aps || [];
  const changes = r.recent_changes || [];

  document.getElementById('h-cnt').textContent     = r.count || 0;
  document.getElementById('h-changed').textContent = aps.filter(a => a.changes > 0).length;
  document.getElementById('h-total').textContent   = r.total_changes || 0;
  document.getElementById('h-sec').textContent     = changes.filter(c => c.type === 'Security').length;

  document.getElementById('ap-tbody').innerHTML = aps.length
    ? [...aps].sort((a,b) => b.changes - a.changes).map(a => `<tr>
        <td><code style="font-size:10px">${a.bssid}</code></td>
        <td>${a.ssid || '<em style="color:var(--muted)">Hidden</em>'}</td>
        <td>${a.ch}</td>
        <td>${SEC_NAMES[a.cur?.sec] || '?'}</td>
        <td>${capPills(a.cur)}</td>
        <td>${pmfLabel(a.cur)}</td>
        <td style="color:${a.cur?.wps?'var(--warn)':'var(--muted)'}">${a.cur?.wps ? 'On' : 'Off'}</td>
        <td style="color:${a.changes>0?'var(--warn)':'var(--muted)'}">${a.changes}</td>
      </tr>`).join('')
    : '<tr><td colspan="8" style="color:var(--muted);text-align:center">No APs.</td></tr>';

  document.getElementById('change-log').innerHTML = changes.length
    ? [...changes].reverse().map(c => {
        const cls = CHG_CLASS[c.type] || 'chg-other';
        const badgeCls = BADGE_CLASS[c.type] || 'chg-other-b';
        return `<div class="change-row ${cls}">
          <div style="display:flex;align-items:center;gap:8px">
            <span class="badge ${badgeCls}">${c.type}</span>
            <strong>${c.ssid || c.bssid}</strong>
            <code style="font-size:9px;color:var(--muted)">${c.bssid}</code>
            <span style="margin-left:auto;font-size:10px;color:var(--muted)">${new Date(c.ts*1000).toLocaleTimeString()}</span>
          </div>
          <div class="old-new">
            <span class="old-val">${c.old}</span>
            <span class="arrow">→</span>
            <span class="new-val">${c.new}</span>
          </div>
        </div>`;
      }).join('')
    : '<div style="color:var(--muted);text-align:center;padding:20px;">No changes logged yet.</div>';
}

async function clearData() {
  await fetch('/api/acd/clear', {method: 'POST'});
  refresh();
}

refresh();
setInterval(refresh, 5000);
</script>
</body>
</html>
)rawhtml";

inline void wuacd_routes() {
  server.on("/ap-config-drift", HTTP_GET, []() { server.send_P(200, "text/html", WUACD_PAGE); });
}

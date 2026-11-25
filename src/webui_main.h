#pragma once
#include <Arduino.h>
#include <WebServer.h>

// ── Main Dashboard SPA ────────────────────────────────────────────────────────
// Full single-page application served at "/" with live stats, AP/STA tables,
// vulnerability list, threat feed, attack controls, and PCAP export.
// Requires: ui_dashboard.h (for /api/sys/*), analytics.h, threat_intel.h,
//           vuln_scanner.h, channel_scanner.h, station_tracker.h,
//           attack_scheduler.h, pcap_writer.h, report_gen.h
// ─────────────────────────────────────────────────────────────────────────────

extern WebServer server;

static const char WUIM_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Phantom32</title>
<style>
:root{
  --bg:#0d1117;--bg2:#161b22;--bg3:#1c2130;
  --border:#21262d;--border2:#30363d;
  --text:#c9d1d9;--text2:#8b949e;--text3:#484f58;
  --blue:#1f6feb;--blue2:#388bfd;--blue3:#58a6ff;
  --green:#238636;--green2:#3fb950;
  --yellow:#9e6a03;--yellow2:#e3b341;
  --red:#8f1f26;--red2:#ff7b72;
  --purple:#6e40c9;--purple2:#bc8cff;
  --accent:#0d2b1f;
}
*{box-sizing:border-box;margin:0;padding:0}
html,body{height:100%;background:var(--bg);color:var(--text);font-family:system-ui,-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;font-size:14px;line-height:1.5}
a{color:var(--blue3);text-decoration:none}
code,pre{font-family:'SF Mono',Menlo,Consolas,monospace;font-size:0.85em}

/* Layout */
.layout{display:flex;height:100vh;overflow:hidden}
.sidebar{width:220px;background:var(--bg2);border-right:1px solid var(--border);display:flex;flex-direction:column;flex-shrink:0;transition:width 0.2s}
.sidebar.collapsed{width:52px}
.sidebar-logo{display:flex;align-items:center;gap:0.5rem;padding:0.8rem 1rem;border-bottom:1px solid var(--border);overflow:hidden}
.sidebar-logo-icon{width:28px;height:28px;border-radius:6px;background:linear-gradient(135deg,var(--blue),var(--purple));display:flex;align-items:center;justify-content:center;color:#fff;font-weight:700;font-size:0.85rem;flex-shrink:0}
.sidebar-logo-text{font-weight:700;font-size:0.95rem;color:#f0f6fc;white-space:nowrap;overflow:hidden}
.sidebar-nav{flex:1;overflow-y:auto;padding:0.5rem 0}
.nav-section{padding:0.3rem 0.8rem;font-size:0.65rem;text-transform:uppercase;letter-spacing:0.08em;color:var(--text3);white-space:nowrap;overflow:hidden;margin-top:0.3rem}
.nav-item{display:flex;align-items:center;gap:0.6rem;padding:0.5rem 1rem;cursor:pointer;border-radius:0;transition:background 0.15s;white-space:nowrap;overflow:hidden;user-select:none}
.nav-item:hover{background:rgba(255,255,255,0.05)}
.nav-item.active{background:rgba(31,111,235,0.15);border-right:2px solid var(--blue2)}
.nav-icon{font-size:1rem;flex-shrink:0;width:20px;text-align:center}
.nav-label{font-size:0.82rem;overflow:hidden;text-overflow:ellipsis}
.nav-badge{background:var(--red);color:#fff;border-radius:8px;padding:0 0.35rem;font-size:0.65rem;font-weight:700;margin-left:auto;flex-shrink:0}
.sidebar-footer{padding:0.6rem 1rem;border-top:1px solid var(--border);font-size:0.72rem;color:var(--text3);overflow:hidden}
.main{flex:1;display:flex;flex-direction:column;overflow:hidden}
.topbar{display:flex;align-items:center;gap:0.6rem;padding:0 1rem;height:46px;background:var(--bg2);border-bottom:1px solid var(--border);flex-shrink:0}
.topbar-title{font-size:0.9rem;font-weight:600;color:#f0f6fc;flex:1}
.topbar-btn{background:var(--bg3);border:1px solid var(--border2);color:var(--text);border-radius:5px;padding:0.3rem 0.7rem;cursor:pointer;font-size:0.78rem;white-space:nowrap}
.topbar-btn:hover{background:var(--border2)}
.topbar-btn.primary{background:var(--blue);border-color:var(--blue);color:#fff}
.topbar-btn.danger{background:var(--red);border-color:var(--red);color:#fff}
.status-dot{width:8px;height:8px;border-radius:50%;background:var(--green2);display:inline-block;margin-right:0.3rem}
.status-dot.warn{background:var(--yellow2)}
.status-dot.err{background:var(--red2)}
.content{flex:1;overflow-y:auto;padding:1rem}
.page{display:none}
.page.active{display:block}

/* Cards */
.stat-row{display:grid;grid-template-columns:repeat(auto-fill,minmax(160px,1fr));gap:0.7rem;margin-bottom:1rem}
.stat-card{background:var(--bg2);border:1px solid var(--border);border-radius:8px;padding:0.8rem 1rem}
.stat-label{font-size:0.72rem;color:var(--text2);text-transform:uppercase;letter-spacing:0.05em;margin-bottom:0.3rem}
.stat-val{font-size:1.6rem;font-weight:700;line-height:1;color:#f0f6fc}
.stat-sub{font-size:0.72rem;color:var(--text3);margin-top:0.2rem}
.stat-delta{font-size:0.75rem;margin-top:0.2rem}
.stat-delta.up{color:var(--green2)}
.stat-delta.down{color:var(--red2)}
.card{background:var(--bg2);border:1px solid var(--border);border-radius:8px;margin-bottom:1rem}
.card-head{display:flex;align-items:center;justify-content:space-between;padding:0.7rem 1rem;border-bottom:1px solid var(--border)}
.card-title{font-size:0.85rem;font-weight:600;color:#f0f6fc}
.card-actions{display:flex;gap:0.4rem}
.card-body{padding:0.5rem 0}

/* Tables */
.tbl{width:100%;border-collapse:collapse;font-size:0.8rem}
.tbl th{background:var(--bg3);color:var(--text2);text-align:left;padding:0.4rem 1rem;font-size:0.7rem;text-transform:uppercase;letter-spacing:0.04em;white-space:nowrap}
.tbl td{padding:0.45rem 1rem;border-bottom:1px solid var(--border)}
.tbl tr:last-child td{border-bottom:none}
.tbl tbody tr:hover td{background:rgba(255,255,255,0.02)}
.tbl-wrap{overflow-x:auto}

/* Badges */
.badge{display:inline-flex;align-items:center;padding:0.1rem 0.5rem;border-radius:10px;font-size:0.7rem;font-weight:600;white-space:nowrap}
.b-open{background:#0d2b1f;color:var(--green2)}
.b-wpa{background:#1a2033;color:var(--blue3)}
.b-wep{background:#2d1b1b;color:var(--red2)}
.b-wpa3{background:#1a1a33;color:var(--purple2)}
.b-crit{background:#2d1b1b;color:var(--red2)}
.b-high{background:#2d2300;color:var(--yellow2)}
.b-med{background:#1a2033;color:var(--blue3)}
.b-low{background:#0d2b1f;color:var(--green2)}
.b-info{background:var(--bg3);color:var(--text2)}
.b-active{background:#0d2b1f;color:var(--green2)}
.b-idle{background:var(--bg3);color:var(--text3)}

/* Forms */
.form-row{display:flex;gap:0.6rem;align-items:center;flex-wrap:wrap;margin-bottom:0.5rem}
.form-label{font-size:0.78rem;color:var(--text2);min-width:70px}
input[type=text],input[type=number],input[type=password],select,textarea{
  background:var(--bg);border:1px solid var(--border2);border-radius:4px;
  color:var(--text);padding:0.35rem 0.6rem;font-size:0.8rem;
}
input:focus,select:focus,textarea:focus{outline:none;border-color:var(--blue2)}
.btn{background:var(--bg3);color:var(--text);border:1px solid var(--border2);border-radius:5px;padding:0.35rem 0.8rem;cursor:pointer;font-size:0.8rem}
.btn:hover{background:var(--border2);color:#f0f6fc}
.btn.sm{padding:0.2rem 0.5rem;font-size:0.72rem}
.btn.primary{background:var(--blue);border-color:var(--blue);color:#fff}
.btn.primary:hover{background:var(--blue2);border-color:var(--blue2)}
.btn.danger{background:var(--red);border-color:var(--red);color:#fff}
.btn.success{background:var(--green);border-color:var(--green);color:var(--green2)}

/* Misc */
.empty{text-align:center;padding:2rem;color:var(--text3);font-size:0.82rem}
.spinner{display:inline-block;width:16px;height:16px;border:2px solid var(--border2);border-top-color:var(--blue2);border-radius:50%;animation:spin 0.7s linear infinite;vertical-align:middle}
@keyframes spin{to{transform:rotate(360deg)}}
.mini-bar{height:4px;border-radius:2px;background:var(--bg3);overflow:hidden;margin-top:4px}
.mini-bar-fill{height:100%;background:var(--blue2);transition:width 0.5s}
.mini-bar-fill.warn{background:var(--yellow2)}
.mini-bar-fill.crit{background:var(--red2)}
.flex{display:flex}
.gap{gap:0.5rem}
.mt1{margin-top:0.5rem}
.mt2{margin-top:1rem}
.mono{font-family:'SF Mono',Menlo,Consolas,monospace;font-size:0.82rem}

/* Toast */
#toast{position:fixed;bottom:1rem;right:1rem;background:var(--bg2);border:1px solid var(--border);border-radius:6px;padding:0.6rem 1rem;font-size:0.8rem;box-shadow:0 4px 12px rgba(0,0,0,0.5);z-index:9999;display:none;max-width:320px;animation:fadeUp 0.2s ease}
@keyframes fadeUp{from{transform:translateY(10px);opacity:0}to{transform:none;opacity:1}}
#toast.ok{border-color:var(--green)}
#toast.err{border-color:var(--red)}

/* Modals */
.modal-bg{position:fixed;inset:0;background:rgba(0,0,0,0.6);z-index:1000;display:flex;align-items:center;justify-content:center;display:none}
.modal{background:var(--bg2);border:1px solid var(--border);border-radius:10px;padding:1.2rem;min-width:360px;max-width:520px;max-height:90vh;overflow-y:auto;box-shadow:0 8px 32px rgba(0,0,0,0.6)}
.modal-title{font-size:0.95rem;font-weight:600;color:#f0f6fc;margin-bottom:1rem;display:flex;justify-content:space-between;align-items:center}
.modal-close{cursor:pointer;color:var(--text3);font-size:1.1rem;line-height:1}
.modal-close:hover{color:var(--text)}

/* Event feed */
.event-item{display:flex;align-items:flex-start;gap:0.5rem;padding:0.4rem 1rem;border-bottom:1px solid var(--border);font-size:0.78rem}
.event-item:last-child{border-bottom:none}
.ev-type{font-weight:700;min-width:48px;font-size:0.68rem;text-transform:uppercase}
.ev-info .ev-type{color:var(--blue3)}
.ev-warn .ev-type{color:var(--yellow2)}
.ev-alert .ev-type{color:var(--red2)}
.ev-critical .ev-type{color:var(--red2)}
.ev-time{color:var(--text3);font-size:0.68rem;white-space:nowrap}
.ev-src{color:var(--text2);min-width:60px;font-family:monospace;font-size:0.72rem}

/* Channel bar */
.chan-bar-row{display:flex;align-items:center;gap:0.5rem;margin-bottom:0.3rem}
.chan-num{width:24px;text-align:right;font-size:0.72rem;color:var(--text2)}
.chan-bar{flex:1;height:12px;background:var(--bg3);border-radius:3px;overflow:hidden}
.chan-fill{height:100%;background:var(--blue2);border-radius:3px;transition:width 0.5s}
.chan-cnt{font-size:0.7rem;color:var(--text3);min-width:30px}

/* Mobile */
@media(max-width:600px){
  .sidebar{position:fixed;top:0;left:-220px;height:100%;z-index:200;transition:left 0.2s}
  .sidebar.open{left:0}
  .content{padding:0.6rem}
  .stat-row{grid-template-columns:1fr 1fr}
}
</style>
</head>
<body>
<div class="layout">
<!-- SIDEBAR -->
<nav class="sidebar" id="sidebar">
  <div class="sidebar-logo">
    <div class="sidebar-logo-icon">P</div>
    <div class="sidebar-logo-text">Phantom32</div>
  </div>
  <div class="sidebar-nav">
    <div class="nav-section">Overview</div>
    <div class="nav-item active" onclick="showPage('dashboard',this)"><span class="nav-icon">🏠</span><span class="nav-label">Dashboard</span></div>
    <div class="nav-item" onclick="showPage('events',this)"><span class="nav-icon">🔔</span><span class="nav-label">Events</span><span class="nav-badge" id="evBadge" style="display:none">0</span></div>

    <div class="nav-section">Scanning</div>
    <div class="nav-item" onclick="showPage('aps',this)"><span class="nav-icon">📡</span><span class="nav-label">Access Points</span></div>
    <div class="nav-item" onclick="showPage('stas',this)"><span class="nav-icon">💻</span><span class="nav-label">Stations</span></div>
    <div class="nav-item" onclick="showPage('spectrum',this)"><span class="nav-icon">📊</span><span class="nav-label">Spectrum</span></div>
    <div class="nav-item" onclick="window.open('/topology','_blank')"><span class="nav-icon">🗺</span><span class="nav-label">Topology ↗</span></div>

    <div class="nav-section">Analysis</div>
    <div class="nav-item" onclick="showPage('vulns',this)"><span class="nav-icon">🔍</span><span class="nav-label">Vulnerabilities</span></div>
    <div class="nav-item" onclick="showPage('threats',this)"><span class="nav-icon">⚠️</span><span class="nav-label">Threat Intel</span></div>
    <div class="nav-item" onclick="showPage('fingerprint',this)"><span class="nav-icon">🧬</span><span class="nav-label">Fingerprints</span></div>
    <div class="nav-item" onclick="showPage('eap',this)"><span class="nav-icon">🔑</span><span class="nav-label">EAP / PMKID</span></div>
    <div class="nav-item" onclick="showPage('pktstats',this)"><span class="nav-icon">📈</span><span class="nav-label">Pkt Stats</span></div>

    <div class="nav-section">Tools</div>
    <div class="nav-item" onclick="window.open('/wizard','_blank')"><span class="nav-icon">⚡</span><span class="nav-label">Attack Wizard ↗</span></div>
    <div class="nav-item" onclick="showPage('pcap',this)"><span class="nav-icon">💾</span><span class="nav-label">PCAP Capture</span></div>
    <div class="nav-item" onclick="showPage('oui',this)"><span class="nav-icon">🔎</span><span class="nav-label">OUI Lookup</span></div>
    <div class="nav-item" onclick="showPage('portscan',this)"><span class="nav-icon">🔌</span><span class="nav-label">Port Scanner</span></div>
    <div class="nav-item" onclick="showPage('report',this)"><span class="nav-icon">📋</span><span class="nav-label">Report</span></div>
    <div class="nav-item" onclick="window.open('/settings','_blank')"><span class="nav-icon">⚙️</span><span class="nav-label">Settings ↗</span></div>
  </div>
  <div class="sidebar-footer" id="sbFooter">
    <span class="status-dot" id="statusDot"></span>
    <span id="statusTxt">Connecting...</span>
  </div>
</nav>

<!-- MAIN -->
<div class="main">
  <div class="topbar">
    <button class="topbar-btn" onclick="toggleSidebar()" style="border:none;background:none;font-size:1.1rem;padding:0.2rem">☰</button>
    <span class="topbar-title" id="pageTitle">Dashboard</span>
    <span style="font-size:0.75rem;color:var(--text3)" id="uptime">—</span>
    <button class="topbar-btn" onclick="triggerScan()">🔍 Scan</button>
    <button class="topbar-btn danger" id="estopBtn" onclick="estopAll()">🛑 E-Stop</button>
  </div>

  <div class="content">

    <!-- DASHBOARD PAGE -->
    <div class="page active" id="page-dashboard">
      <div class="stat-row" id="statRow">
        <div class="stat-card"><div class="stat-label">Access Points</div><div class="stat-val" id="cntAps">—</div><div class="stat-sub">on all channels</div></div>
        <div class="stat-card"><div class="stat-label">Stations</div><div class="stat-val" id="cntStas">—</div><div class="stat-sub">active devices</div></div>
        <div class="stat-card"><div class="stat-label">Vulnerabilities</div><div class="stat-val" id="cntVulns" style="color:var(--red2)">—</div><div class="stat-sub" id="subVulns">none found</div></div>
        <div class="stat-card"><div class="stat-label">Threats</div><div class="stat-val" id="cntThreats" style="color:var(--yellow2)">—</div><div class="stat-sub">active alerts</div></div>
        <div class="stat-card"><div class="stat-label">Free Heap</div><div class="stat-val" id="freeHeap">—</div><div class="stat-sub">bytes</div><div class="mini-bar"><div class="mini-bar-fill" id="heapBar" style="width:0%"></div></div></div>
        <div class="stat-card"><div class="stat-label">Frames / sec</div><div class="stat-val" id="fpsStat">—</div><div class="stat-sub" id="totalFrames">total: 0</div></div>
      </div>

      <!-- Channel Activity -->
      <div class="card">
        <div class="card-head"><span class="card-title">Channel Activity</span><button class="btn sm" onclick="loadChannels()">↺</button></div>
        <div class="card-body" style="padding:0.6rem 1rem" id="chanBars">
          <div class="empty">Loading...</div>
        </div>
      </div>

      <!-- Top APs quick view -->
      <div class="card">
        <div class="card-head"><span class="card-title">Top Access Points</span><button class="btn sm" onclick="showPage('aps',null)">View All</button></div>
        <div class="tbl-wrap">
          <table class="tbl"><thead><tr><th>BSSID</th><th>SSID</th><th>CH</th><th>RSSI</th><th>ENC</th><th>Clients</th><th>Score</th></tr></thead>
          <tbody id="dashApBody"><tr><td colspan="7" class="empty">Loading…</td></tr></tbody>
          </table>
        </div>
      </div>

      <!-- Recent events -->
      <div class="card">
        <div class="card-head"><span class="card-title">Recent Events</span><button class="btn sm" onclick="showPage('events',null)">View All</button></div>
        <div id="dashEvents"><div class="empty">Loading…</div></div>
      </div>
    </div>

    <!-- ACCESS POINTS PAGE -->
    <div class="page" id="page-aps">
      <div class="card-head" style="margin-bottom:0.7rem;background:none;border:none;padding:0">
        <span class="card-title">Access Points</span>
        <div class="card-actions">
          <input type="text" id="apFilter" placeholder="Filter SSID/BSSID…" oninput="filterAps()" style="width:160px">
          <button class="btn" onclick="loadAps()">↺ Refresh</button>
          <button class="btn" onclick="triggerScan()">🔍 Scan</button>
        </div>
      </div>
      <div class="card">
        <div class="tbl-wrap">
          <table class="tbl"><thead><tr>
            <th>BSSID</th><th>SSID</th><th>CH</th><th>RSSI</th><th>ENC</th>
            <th>Clients</th><th>Score</th><th>Vuln</th><th>Age</th><th>Action</th>
          </tr></thead>
          <tbody id="apBody"><tr><td colspan="10" class="empty">No APs yet — run a scan</td></tr></tbody>
          </table>
        </div>
      </div>
    </div>

    <!-- STATIONS PAGE -->
    <div class="page" id="page-stas">
      <div class="card-head" style="margin-bottom:0.7rem;background:none;border:none;padding:0">
        <span class="card-title">Stations</span>
        <div class="card-actions">
          <input type="text" id="staFilter" placeholder="Filter MAC/vendor…" oninput="filterStas()" style="width:160px">
          <button class="btn" onclick="loadStas()">↺ Refresh</button>
          <select id="staSort" onchange="loadStas()" style="font-size:0.78rem">
            <option value="rssi">Sort: RSSI</option>
            <option value="probes">Sort: Probes</option>
            <option value="ts">Sort: Newest</option>
          </select>
        </div>
      </div>
      <div class="card">
        <div class="tbl-wrap">
          <table class="tbl"><thead><tr>
            <th>MAC</th><th>Vendor</th><th>Assoc AP</th><th>RSSI</th><th>Probes</th>
            <th>State</th><th>Alerts</th><th>Last Seen</th><th>Action</th>
          </tr></thead>
          <tbody id="staBody"><tr><td colspan="9" class="empty">No stations captured yet</td></tr></tbody>
          </table>
        </div>
      </div>
    </div>

    <!-- SPECTRUM PAGE -->
    <div class="page" id="page-spectrum">
      <div class="card">
        <div class="card-head"><span class="card-title">Channel Heatmap</span><button class="btn sm" onclick="loadSpectrum()">↺</button></div>
        <div class="card-body" style="padding:0.8rem 1rem" id="spectrumBody">
          <div class="empty">Loading…</div>
        </div>
      </div>
      <div class="card">
        <div class="card-head"><span class="card-title">RSSI Distribution</span></div>
        <div class="card-body" style="padding:0.8rem 1rem" id="rssiDistBody">
          <div class="empty">Loading…</div>
        </div>
      </div>
      <p style="font-size:0.75rem;color:var(--text3);margin-top:0.5rem">
        Open <a href="/spectrum" target="_blank">full waterfall ↗</a> for real-time spectrum analysis.
      </p>
    </div>

    <!-- VULNERABILITIES PAGE -->
    <div class="page" id="page-vulns">
      <div class="card-head" style="margin-bottom:0.7rem;background:none;border:none;padding:0">
        <span class="card-title">Vulnerabilities</span>
        <div class="card-actions">
          <select id="vulnSev" onchange="loadVulns()" style="font-size:0.78rem">
            <option value="all">All Severities</option>
            <option value="4">Critical</option>
            <option value="3">High</option>
            <option value="2">Medium</option>
            <option value="1">Low</option>
          </select>
          <button class="btn" onclick="loadVulns()">↺</button>
        </div>
      </div>
      <div class="card">
        <div class="tbl-wrap">
          <table class="tbl"><thead><tr>
            <th>BSSID</th><th>SSID</th><th>Issue</th><th>Sev</th>
            <th>Score</th><th>Detail</th>
          </tr></thead>
          <tbody id="vulnBody"><tr><td colspan="6" class="empty">No vulnerabilities found</td></tr></tbody>
          </table>
        </div>
      </div>
    </div>

    <!-- THREATS PAGE -->
    <div class="page" id="page-threats">
      <div class="card-head" style="margin-bottom:0.7rem;background:none;border:none;padding:0">
        <span class="card-title">Threat Intel</span>
        <div class="card-actions">
          <button class="btn" onclick="loadThreats()">↺ Refresh</button>
          <button class="btn danger" onclick="clearThreats()">🗑 Clear</button>
        </div>
      </div>
      <div class="stat-row" id="threatStats" style="margin-bottom:0.7rem"></div>
      <div class="card">
        <div class="tbl-wrap">
          <table class="tbl"><thead><tr>
            <th>BSSID</th><th>SSID</th><th>Score</th><th>Flags</th><th>Evil Twin</th><th>Detail</th>
          </tr></thead>
          <tbody id="threatBody"><tr><td colspan="6" class="empty">No threats detected</td></tr></tbody>
          </table>
        </div>
      </div>
    </div>

    <!-- FINGERPRINT PAGE -->
    <div class="page" id="page-fingerprint">
      <div class="card-head" style="margin-bottom:0.7rem;background:none;border:none;padding:0">
        <span class="card-title">Device Fingerprints</span>
        <div class="card-actions"><button class="btn" onclick="loadFingerprints()">↺</button></div>
      </div>
      <div class="card">
        <div class="tbl-wrap">
          <table class="tbl"><thead><tr>
            <th>MAC</th><th>Vendor</th><th>OS Guess</th><th>Device Type</th>
            <th>IEs Seen</th><th>Confidence</th><th>Last Seen</th>
          </tr></thead>
          <tbody id="fpBody"><tr><td colspan="7" class="empty">No fingerprints yet</td></tr></tbody>
          </table>
        </div>
      </div>
    </div>

    <!-- EAP PAGE -->
    <div class="page" id="page-eap">
      <div class="card-head" style="margin-bottom:0.7rem;background:none;border:none;padding:0">
        <span class="card-title">EAP / EAPOL Sessions</span>
        <div class="card-actions">
          <button class="btn" onclick="loadEap()">↺</button>
          <button class="btn danger" onclick="clearEap()">🗑</button>
        </div>
      </div>
      <div class="card">
        <div class="tbl-wrap">
          <table class="tbl"><thead><tr>
            <th>BSSID</th><th>Station</th><th>State</th><th>M1</th><th>M2</th>
            <th>PMKID</th><th>Age</th>
          </tr></thead>
          <tbody id="eapBody"><tr><td colspan="7" class="empty">No EAP sessions captured</td></tr></tbody>
          </table>
        </div>
      </div>
    </div>

    <!-- PKT STATS PAGE -->
    <div class="page" id="page-pktstats">
      <div class="card-head" style="margin-bottom:0.7rem;background:none;border:none;padding:0">
        <span class="card-title">Packet Statistics</span>
        <div class="card-actions">
          <button class="btn" onclick="loadPktStats()">↺</button>
          <button class="btn danger" onclick="resetPktStats()">Reset</button>
        </div>
      </div>
      <div class="stat-row" id="pktStatRow"></div>
      <div class="card">
        <div class="card-head"><span class="card-title">Frame Types</span></div>
        <div class="card-body" id="frameTypeBody" style="padding:0.6rem 1rem"><div class="empty">Loading…</div></div>
      </div>
      <div class="card">
        <div class="card-head"><span class="card-title">Top BSSIDs by Frames</span></div>
        <div class="tbl-wrap">
          <table class="tbl"><thead><tr><th>BSSID</th><th>Frames</th><th>Avg RSSI</th><th>Last Ch</th></tr></thead>
          <tbody id="bssidBody"><tr><td colspan="4" class="empty">No data</td></tr></tbody>
          </table>
        </div>
      </div>
    </div>

    <!-- PCAP PAGE -->
    <div class="page" id="page-pcap">
      <div class="card">
        <div class="card-head"><span class="card-title">PCAP Capture</span></div>
        <div class="card-body" style="padding:0.8rem 1rem">
          <div class="form-row">
            <span class="form-label">File path</span>
            <input type="text" id="pcapFile" value="/capture.pcap" style="width:180px">
            <select id="pcapFilter" style="width:120px">
              <option value="all">All frames</option>
              <option value="mgmt">Mgmt only</option>
              <option value="data">Data only</option>
              <option value="beacon">Beacons only</option>
              <option value="probe">Probes only</option>
            </select>
          </div>
          <div class="form-row">
            <button class="btn primary" onclick="startCapture()">▶ Start</button>
            <button class="btn danger" onclick="stopCapture()">⏹ Stop</button>
            <button class="btn" onclick="downloadPcap()">⬇ Download</button>
            <span id="pcapStatus" style="font-size:0.78rem;color:var(--text2)">Idle</span>
          </div>
          <div id="pcapStats" style="margin-top:0.5rem;font-size:0.78rem;color:var(--text2)"></div>
        </div>
      </div>
    </div>

    <!-- OUI LOOKUP PAGE -->
    <div class="page" id="page-oui">
      <div class="card">
        <div class="card-head"><span class="card-title">OUI / Vendor Lookup</span></div>
        <div class="card-body" style="padding:0.8rem 1rem">
          <div class="form-row">
            <input type="text" id="ouiInput" placeholder="AA:BB:CC or AA:BB:CC:DD:EE:FF" style="width:240px" onkeydown="if(event.key==='Enter')ouiLookup()">
            <button class="btn primary" onclick="ouiLookup()">Lookup</button>
          </div>
          <div id="ouiResult" style="margin-top:0.7rem;font-size:0.82rem"></div>
        </div>
      </div>
      <div class="card">
        <div class="card-head"><span class="card-title">Extended Vendor Stats</span></div>
        <div class="card-body" id="ouiStats" style="padding:0.6rem 1rem"><div class="empty">Loading…</div></div>
      </div>
    </div>

    <!-- PORT SCAN PAGE -->
    <div class="page" id="page-portscan">
      <div class="card">
        <div class="card-head"><span class="card-title">Port Scanner</span></div>
        <div class="card-body" style="padding:0.8rem 1rem">
          <div class="form-row">
            <span class="form-label">Subnet</span>
            <input type="text" id="psSubnet" value="192.168.4" style="width:130px">
            <button class="btn primary" onclick="startPortScan()">▶ Scan Subnet</button>
            <span id="psStatus" style="font-size:0.78rem;color:var(--text2)">Idle</span>
          </div>
        </div>
      </div>
      <div class="card">
        <div class="card-head"><span class="card-title">Results</span><button class="btn sm" onclick="loadPortResults()">↺</button></div>
        <div class="tbl-wrap">
          <table class="tbl"><thead><tr>
            <th>Host</th><th>Alive</th><th>Open Ports</th><th>Services</th>
          </tr></thead>
          <tbody id="psBody"><tr><td colspan="4" class="empty">No scan results</td></tr></tbody>
          </table>
        </div>
      </div>
    </div>

    <!-- EVENTS PAGE -->
    <div class="page" id="page-events">
      <div class="card-head" style="margin-bottom:0.7rem;background:none;border:none;padding:0">
        <span class="card-title">System Events</span>
        <div class="card-actions">
          <select id="evFilter" onchange="loadEvents()" style="font-size:0.78rem">
            <option value="all">All</option>
            <option value="info">Info</option>
            <option value="warn">Warn</option>
            <option value="alert">Alert</option>
            <option value="critical">Critical</option>
          </select>
          <button class="btn" onclick="loadEvents()">↺</button>
        </div>
      </div>
      <div class="card">
        <div id="evFeed"><div class="empty">Loading…</div></div>
      </div>
    </div>

    <!-- REPORT PAGE -->
    <div class="page" id="page-report">
      <div class="card">
        <div class="card-head"><span class="card-title">Security Report</span></div>
        <div class="card-body" style="padding:0.8rem 1rem">
          <div class="form-row">
            <span class="form-label">Title</span>
            <input type="text" id="rptTitle" value="Phantom32 WiFi Audit" style="width:220px">
          </div>
          <div class="form-row">
            <button class="btn primary" onclick="genReport()">📋 Generate HTML Report</button>
            <button class="btn" onclick="downloadReport()">⬇ Download</button>
            <span id="rptStatus" style="font-size:0.78rem;color:var(--text2)"></span>
          </div>
        </div>
      </div>
      <div class="card" id="rptPreview" style="display:none">
        <div class="card-head"><span class="card-title">Preview</span><a href="/report" target="_blank" class="btn sm">Open ↗</a></div>
        <div id="rptBody" style="padding:0.6rem 1rem;font-size:0.78rem;color:var(--text2)"></div>
      </div>
    </div>

  </div><!-- content -->
</div><!-- main -->
</div><!-- layout -->

<div id="toast"></div>

<!-- AP Detail Modal -->
<div class="modal-bg" id="apModal">
  <div class="modal">
    <div class="modal-title">AP Details <span class="modal-close" onclick="closeModal('apModal')">✕</span></div>
    <div id="apModalBody"></div>
  </div>
</div>

<!-- Station Detail Modal -->
<div class="modal-bg" id="staModal">
  <div class="modal">
    <div class="modal-title">Station Details <span class="modal-close" onclick="closeModal('staModal')">✕</span></div>
    <div id="staModalBody"></div>
  </div>
</div>

<script>
// ── Global state ──────────────────────────────────────────────────────────────
var currentPage='dashboard';
var allAps=[],allStas=[];
var sysInfo={};
var newEvCount=0;
var autoRefreshInterval=null;

// ── Utility ───────────────────────────────────────────────────────────────────
function api(url,opts){
  return fetch(url,opts||{}).then(function(r){
    if(!r.ok) throw new Error('HTTP '+r.status);
    return r.json();
  });
}

function toast(msg,type){
  var t=document.getElementById('toast');
  t.textContent=msg;
  t.className=type||'';
  t.style.display='block';
  clearTimeout(t._tid);
  t._tid=setTimeout(function(){t.style.display='none';},3000);
}

function fmt(n){if(n===undefined||n===null) return '—';if(n>=1e6) return (n/1e6).toFixed(1)+'M';if(n>=1000) return (n/1000).toFixed(1)+'K';return String(n);}
function fmtAge(ms){var s=Math.floor(ms/1000);if(s<60) return s+'s';if(s<3600) return Math.floor(s/60)+'m';return Math.floor(s/3600)+'h';}
function enc2badge(e){
  var n=parseInt(e,10);
  if(n===0) return '<span class="badge b-open">Open</span>';
  if(n===5) return '<span class="badge b-wep">WEP</span>';
  if(n===8||e==='wpa3') return '<span class="badge b-wpa3">WPA3</span>';
  return '<span class="badge b-wpa">WPA2</span>';
}
function sev2badge(s){
  var mp={4:'<span class="badge b-crit">CRIT</span>',
          3:'<span class="badge b-high">HIGH</span>',
          2:'<span class="badge b-med">MED</span>',
          1:'<span class="badge b-low">LOW</span>'};
  return mp[s]||'<span class="badge b-info">INFO</span>';
}
function score2color(s){
  if(s>=75) return 'var(--red2)';
  if(s>=50) return 'var(--yellow2)';
  if(s>=25) return 'var(--blue3)';
  return 'var(--green2)';
}

function showPage(name,el){
  document.querySelectorAll('.page').forEach(function(p){p.classList.remove('active')});
  var pg=document.getElementById('page-'+name);
  if(pg) pg.classList.add('active');
  document.querySelectorAll('.nav-item').forEach(function(n){n.classList.remove('active')});
  if(el) el.classList.add('active');
  currentPage=name;
  var titles={dashboard:'Dashboard',aps:'Access Points',stas:'Stations',
               spectrum:'Spectrum',vulns:'Vulnerabilities',threats:'Threat Intel',
               fingerprint:'Device Fingerprints',eap:'EAP / PMKID',pktstats:'Packet Stats',
               pcap:'PCAP Capture',oui:'OUI Lookup',portscan:'Port Scanner',
               events:'Events',report:'Security Report'};
  document.getElementById('pageTitle').textContent=titles[name]||name;
  if(name==='aps') loadAps();
  else if(name==='stas') loadStas();
  else if(name==='vulns') loadVulns();
  else if(name==='threats') loadThreats();
  else if(name==='fingerprint') loadFingerprints();
  else if(name==='eap') loadEap();
  else if(name==='pktstats') loadPktStats();
  else if(name==='events') loadEvents();
  else if(name==='spectrum') loadSpectrum();
  else if(name==='oui') loadOuiStats();
  else if(name==='portscan') loadPortResults();
}

function toggleSidebar(){
  document.getElementById('sidebar').classList.toggle('collapsed');
  document.getElementById('sidebar').classList.toggle('open');
}

function closeModal(id){
  document.getElementById(id).style.display='none';
}

// ── Dashboard ─────────────────────────────────────────────────────────────────
function loadDashboard(){
  api('/api/sys/summary').then(function(d){
    sysInfo=d;
    var heap=d.free_heap||0;
    var heapPct=Math.min(100,Math.round(heap/300000*100));
    document.getElementById('freeHeap').textContent=fmt(heap);
    var bar=document.getElementById('heapBar');
    bar.style.width=heapPct+'%';
    bar.className='mini-bar-fill'+(heapPct<30?' crit':heapPct<50?' warn':'');
    document.getElementById('fpsStat').textContent=d.fps||0;
    document.getElementById('totalFrames').textContent='total: '+fmt(d.total_frames);
    var up=d.uptime_s||0;
    var h=Math.floor(up/3600),m=Math.floor((up%3600)/60),s=up%60;
    document.getElementById('uptime').textContent='up '+h+'h '+m+'m '+s+'s';
    var dot=document.getElementById('statusDot');
    dot.className='status-dot'+(heap<50000?' err':heap<100000?' warn':'');
    document.getElementById('statusTxt').textContent=heap<50000?'Low memory':heap<100000?'Degraded':'OK';
  }).catch(function(){});

  api('/api/vs/summary').then(function(d){
    document.getElementById('cntVulns').textContent=d.total||0;
    if((d.critical||0)>0) document.getElementById('subVulns').textContent=(d.critical)+' critical';
    else if((d.high||0)>0) document.getElementById('subVulns').textContent=(d.high)+' high';
    else document.getElementById('subVulns').textContent='none critical';
  }).catch(function(){document.getElementById('cntVulns').textContent='?'});

  api('/api/ti/summary').then(function(d){
    document.getElementById('cntThreats').textContent=d.total||0;
  }).catch(function(){document.getElementById('cntThreats').textContent='?'});

  loadChannels();
  loadDashAps();
  loadDashEvents();
}

function loadChannels(){
  api('/api/anl/heatmap').then(function(d){
    var chans=(d.channels||[]).slice(0,13);
    if(!chans.length){document.getElementById('chanBars').innerHTML='<div class="empty">No channel data</div>';return;}
    var maxF=1;
    chans.forEach(function(c){if((c.frames||0)>maxF) maxF=c.frames;});
    var html='';
    for(var i=1;i<=13;i++){
      var c=chans.find(function(x){return x.ch===i});
      var frames=c?c.frames:0;
      var rssi=c?c.avg_rssi:-100;
      var pct=Math.round(frames/maxF*100);
      html+='<div class="chan-bar-row">'+
        '<span class="chan-num">'+i+'</span>'+
        '<div class="chan-bar"><div class="chan-fill" style="width:'+pct+'%"></div></div>'+
        '<span class="chan-cnt">'+fmt(frames)+(rssi>-99?' / '+rssi+'dBm':'')+'</span>'+
        '</div>';
    }
    document.getElementById('chanBars').innerHTML=html;
  }).catch(function(){});
}

function loadDashAps(){
  api('/api/cs/aps?per=10').then(function(d){
    var aps=d.aps||[];
    document.getElementById('cntAps').textContent=d.total||aps.length;
    if(!aps.length){document.getElementById('dashApBody').innerHTML='<tr><td colspan="7" class="empty">No APs</td></tr>';return;}
    document.getElementById('dashApBody').innerHTML=aps.slice(0,8).map(function(ap){
      return '<tr onclick="openApModal(\''+ap.bssid+'\')" style="cursor:pointer">'+
        '<td class="mono">'+ap.bssid+'</td>'+
        '<td>'+(ap.ssid||'<span style="color:var(--text3)">hidden</span>')+'</td>'+
        '<td>'+ap.ch+'</td>'+
        '<td>'+ap.rssi+' dBm</td>'+
        '<td>'+enc2badge(ap.enc)+'</td>'+
        '<td>'+(ap.clients||0)+'</td>'+
        '<td style="color:'+score2color(ap.score||0)+'">'+Math.round(ap.score||0)+'</td>'+
        '</tr>';
    }).join('');
  }).catch(function(){document.getElementById('cntAps').textContent='?'});
}

function loadDashEvents(){
  api('/api/sys/events?n=10').then(function(d){
    var evs=d.events||[];
    if(!evs.length){document.getElementById('dashEvents').innerHTML='<div class="empty">No events</div>';return;}
    document.getElementById('dashEvents').innerHTML=evs.map(function(e){
      return '<div class="event-item ev-'+e.type+'">'+
        '<span class="ev-type">'+e.type+'</span>'+
        '<span class="ev-src">'+e.source+'</span>'+
        '<span style="flex:1">'+e.msg+'</span>'+
        '<span class="ev-time">+'+fmtAge(sysInfo.uptime_ms-(e.ts||0))+'</span>'+
        '</div>';
    }).join('');
  }).catch(function(){});
}

// ── APs ────────────────────────────────────────────────────────────────────────
function loadAps(){
  api('/api/cs/aps?per=200').then(function(d){
    allAps=d.aps||[];
    renderAps(allAps);
  }).catch(function(){document.getElementById('apBody').innerHTML='<tr><td colspan="10" class="empty">Error loading APs</td></tr>'});
}

function renderAps(aps){
  if(!aps.length){document.getElementById('apBody').innerHTML='<tr><td colspan="10" class="empty">No APs</td></tr>';return;}
  document.getElementById('apBody').innerHTML=aps.map(function(ap){
    return '<tr>'+
      '<td class="mono" style="font-size:0.78rem">'+ap.bssid+'</td>'+
      '<td>'+(ap.ssid||'<span style="color:var(--text3)">hidden</span>')+'</td>'+
      '<td>'+ap.ch+'</td>'+
      '<td>'+ap.rssi+' dBm</td>'+
      '<td>'+enc2badge(ap.enc)+'</td>'+
      '<td>'+(ap.clients||0)+'</td>'+
      '<td style="color:'+score2color(ap.score||0)+'">'+Math.round(ap.score||0)+'</td>'+
      '<td>'+(ap.vuln_count||0)+'</td>'+
      '<td style="color:var(--text3)">'+fmtAge(ap.age_ms||0)+'</td>'+
      '<td><button class="btn sm" onclick="openApModal(\''+ap.bssid+'\')">Info</button> '+
          '<button class="btn sm danger" onclick="quickDeauth(\''+ap.bssid+'\','+ap.ch+')">Deauth</button></td>'+
      '</tr>';
  }).join('');
}

function filterAps(){
  var q=document.getElementById('apFilter').value.toLowerCase();
  renderAps(allAps.filter(function(ap){return !q||(ap.bssid.toLowerCase().includes(q)||(ap.ssid||'').toLowerCase().includes(q))}));
}

function openApModal(bssid){
  var ap=allAps.find(function(a){return a.bssid===bssid});
  if(!ap){api('/api/cs/ap?bssid='+encodeURIComponent(bssid)).then(function(d){renderApModal(d)}).catch(function(){});}
  else renderApModal(ap);
  document.getElementById('apModal').style.display='flex';
}

function renderApModal(ap){
  var rows=[['BSSID',ap.bssid],['SSID',ap.ssid||'hidden'],['Channel',ap.ch],
    ['RSSI',ap.rssi+' dBm'],['Encryption',enc2badge(ap.enc)],
    ['Clients',ap.clients||0],['Vuln Score',ap.score||0],
    ['Country',ap.country||'—'],['Vendor',ap.vendor||'—']];
  var html=rows.map(function(r){
    return '<div style="display:flex;justify-content:space-between;padding:0.3rem 0;border-bottom:1px solid var(--border);font-size:0.82rem">'+
      '<span style="color:var(--text2)">'+r[0]+'</span><span>'+r[1]+'</span></div>';
  }).join('');
  html+='<div style="margin-top:0.7rem;display:flex;gap:0.4rem">'+
    '<button class="btn primary" onclick="quickDeauth(\''+ap.bssid+'\','+ap.ch+');closeModal(\'apModal\')">⚡ Deauth</button>'+
    '<button class="btn" onclick="closeModal(\'apModal\')">Close</button></div>';
  document.getElementById('apModalBody').innerHTML=html;
}

// ── Stations ──────────────────────────────────────────────────────────────────
function loadStas(){
  api('/api/st/stas?per=200').then(function(d){
    allStas=d.stas||[];
    renderStas(allStas);
    document.getElementById('cntStas').textContent=d.total||allStas.length;
  }).catch(function(){});
}

function renderStas(stas){
  if(!stas.length){document.getElementById('staBody').innerHTML='<tr><td colspan="9" class="empty">No stations</td></tr>';return;}
  var stateNames=['unknown','seen','probing','assoc','disconn','roaming','gone'];
  document.getElementById('staBody').innerHTML=stas.map(function(s){
    var stateStr=stateNames[s.state||0]||'—';
    var stBadge=(s.state===3)?'<span class="badge b-active">assoc</span>':'<span class="badge b-idle">'+stateStr+'</span>';
    return '<tr>'+
      '<td class="mono" style="font-size:0.78rem">'+s.mac+'</td>'+
      '<td style="font-size:0.78rem">'+(s.vendor||'—')+'</td>'+
      '<td class="mono" style="font-size:0.78rem">'+(s.ap||'—')+'</td>'+
      '<td>'+s.rssi+' dBm</td>'+
      '<td>'+s.probes+'</td>'+
      '<td>'+stBadge+'</td>'+
      '<td style="color:var(--red2)">'+(s.alerts||0)+'</td>'+
      '<td style="color:var(--text3)">'+fmtAge(s.age_ms||0)+'</td>'+
      '<td><button class="btn sm" onclick="openStaModal(\''+s.mac+'\')">Info</button></td>'+
      '</tr>';
  }).join('');
}

function filterStas(){
  var q=document.getElementById('staFilter').value.toLowerCase();
  renderStas(allStas.filter(function(s){return !q||(s.mac.toLowerCase().includes(q)||(s.vendor||'').toLowerCase().includes(q))}));
}

function openStaModal(mac){
  var s=allStas.find(function(x){return x.mac===mac});
  if(!s) return;
  var rows=[['MAC',s.mac],['Vendor',s.vendor||'—'],['Assoc AP',s.ap||'—'],
    ['RSSI',s.rssi+' dBm'],['Probes',s.probes],['Alerts',s.alerts||0]];
  var html=rows.map(function(r){
    return '<div style="display:flex;justify-content:space-between;padding:0.3rem 0;border-bottom:1px solid var(--border);font-size:0.82rem">'+
      '<span style="color:var(--text2)">'+r[0]+'</span><span>'+r[1]+'</span></div>';
  }).join('');
  html+='<div style="margin-top:0.7rem"><button class="btn" onclick="closeModal(\'staModal\')">Close</button></div>';
  document.getElementById('staModalBody').innerHTML=html;
  document.getElementById('staModal').style.display='flex';
}

// ── Vulnerabilities ───────────────────────────────────────────────────────────
function loadVulns(){
  api('/api/vs/vulns?per=200').then(function(d){
    var vulns=d.vulns||[];
    if(!vulns.length){document.getElementById('vulnBody').innerHTML='<tr><td colspan="6" class="empty">No vulnerabilities found</td></tr>';return;}
    document.getElementById('vulnBody').innerHTML=vulns.map(function(v){
      return '<tr>'+
        '<td class="mono" style="font-size:0.78rem">'+v.bssid+'</td>'+
        '<td>'+(v.ssid||'—')+'</td>'+
        '<td>'+v.issue+'</td>'+
        '<td>'+sev2badge(v.sev)+'</td>'+
        '<td style="color:'+score2color(v.score)+'">'+Math.round(v.score)+'</td>'+
        '<td style="font-size:0.75rem;color:var(--text2)">'+v.detail+'</td>'+
        '</tr>';
    }).join('');
  }).catch(function(){});
}

// ── Threat Intel ──────────────────────────────────────────────────────────────
function loadThreats(){
  api('/api/ti/threats?per=100').then(function(d){
    var threats=d.threats||[];
    var stats=d.summary||{};
    var sc=document.getElementById('threatStats');
    sc.innerHTML=[
      ['Rogue APs',stats.rogue||0,'var(--red2)'],
      ['Evil Twins',stats.evil_twin||0,'var(--red2)'],
      ['Deauth Floods',stats.deauth_flood||0,'var(--yellow2)'],
      ['Anomalies',stats.anomaly||0,'var(--yellow2)']
    ].map(function(r){
      return '<div class="stat-card"><div class="stat-label">'+r[0]+'</div><div class="stat-val" style="color:'+r[2]+'">'+r[1]+'</div></div>';
    }).join('');
    if(!threats.length){document.getElementById('threatBody').innerHTML='<tr><td colspan="6" class="empty">No threats</td></tr>';return;}
    document.getElementById('threatBody').innerHTML=threats.map(function(t){
      return '<tr>'+
        '<td class="mono" style="font-size:0.78rem">'+t.bssid+'</td>'+
        '<td>'+(t.ssid||'—')+'</td>'+
        '<td style="color:'+score2color(t.score)+'">'+Math.round(t.score)+'</td>'+
        '<td style="font-size:0.72rem">'+(t.flags||[]).join(', ')+'</td>'+
        '<td>'+(t.evil_twin?'<span class="badge b-crit">YES</span>':'<span class="badge b-info">No</span>')+'</td>'+
        '<td style="font-size:0.75rem;color:var(--text2)">'+(t.detail||'')+'</td>'+
        '</tr>';
    }).join('');
  }).catch(function(){});
}

function clearThreats(){
  api('/api/ti/reset',{method:'POST'}).then(function(){toast('Threats cleared','ok');loadThreats();});
}

// ── Fingerprints ──────────────────────────────────────────────────────────────
function loadFingerprints(){
  api('/api/fp/devices?per=100').then(function(d){
    var devs=d.devices||[];
    if(!devs.length){document.getElementById('fpBody').innerHTML='<tr><td colspan="7" class="empty">No fingerprints</td></tr>';return;}
    document.getElementById('fpBody').innerHTML=devs.map(function(f){
      return '<tr>'+
        '<td class="mono" style="font-size:0.78rem">'+f.mac+'</td>'+
        '<td style="font-size:0.78rem">'+(f.vendor||'—')+'</td>'+
        '<td>'+(f.os||'—')+'</td>'+
        '<td>'+(f.device_type||'—')+'</td>'+
        '<td>'+(f.ie_count||0)+'</td>'+
        '<td>'+(f.confidence||0)+'%</td>'+
        '<td style="color:var(--text3)">'+fmtAge(f.age_ms||0)+'</td>'+
        '</tr>';
    }).join('');
  }).catch(function(){document.getElementById('fpBody').innerHTML='<tr><td colspan="7" class="empty">Error</td></tr>'});
}

// ── EAP ───────────────────────────────────────────────────────────────────────
function loadEap(){
  api('/api/eap/sessions').then(function(d){
    var sessions=d.sessions||[];
    if(!sessions.length){document.getElementById('eapBody').innerHTML='<tr><td colspan="7" class="empty">No EAP sessions</td></tr>';return;}
    document.getElementById('eapBody').innerHTML=sessions.map(function(s){
      return '<tr>'+
        '<td class="mono" style="font-size:0.78rem">'+s.bssid+'</td>'+
        '<td class="mono" style="font-size:0.78rem">'+s.sta+'</td>'+
        '<td>'+s.state+'</td>'+
        '<td>'+(s.got_m1?'✅':'—')+'</td>'+
        '<td>'+(s.got_m2?'✅':'—')+'</td>'+
        '<td class="mono" style="font-size:0.68rem">'+(s.pmkid||'—')+'</td>'+
        '<td style="color:var(--text3)">'+fmtAge(s.age_ms||0)+'</td>'+
        '</tr>';
    }).join('');
  }).catch(function(){});
}

function clearEap(){
  api('/api/eap/clear',{method:'POST'}).then(function(){toast('EAP sessions cleared','ok');loadEap();});
}

// ── Pkt Stats ─────────────────────────────────────────────────────────────────
function loadPktStats(){
  api('/api/pks/summary').then(function(d){
    var r=document.getElementById('pktStatRow');
    r.innerHTML=[
      ['Total Frames',fmt(d.total)],
      ['Mgmt',fmt(d.mgmt)],
      ['Ctrl',fmt(d.ctrl)],
      ['Data',fmt(d.data)],
      ['Beacons',fmt(d.beacon)],
      ['Probes',fmt(d.probe_req)]
    ].map(function(x){
      return '<div class="stat-card"><div class="stat-label">'+x[0]+'</div><div class="stat-val" style="font-size:1.2rem">'+x[1]+'</div></div>';
    }).join('');
    var types=['beacon','probe_req','probe_resp','assoc_req','assoc_resp','deauth','disassoc','auth','action','data','null_data','qos_data'];
    var html='';
    var maxV=1;
    types.forEach(function(t){if((d[t]||0)>maxV)maxV=d[t];});
    types.forEach(function(t){
      var v=d[t]||0;
      var pct=Math.round(v/maxV*100);
      html+='<div class="chan-bar-row">'+
        '<span style="min-width:90px;font-size:0.72rem;color:var(--text2)">'+t+'</span>'+
        '<div class="chan-bar"><div class="chan-fill" style="width:'+pct+'%"></div></div>'+
        '<span class="chan-cnt">'+fmt(v)+'</span></div>';
    });
    document.getElementById('frameTypeBody').innerHTML=html||'<div class="empty">No data</div>';
  }).catch(function(){});

  api('/api/pks/bssids').then(function(d){
    var bssids=d.bssids||[];
    if(!bssids.length){document.getElementById('bssidBody').innerHTML='<tr><td colspan="4" class="empty">No data</td></tr>';return;}
    document.getElementById('bssidBody').innerHTML=bssids.slice(0,20).map(function(b){
      return '<tr><td class="mono" style="font-size:0.78rem">'+b.bssid+'</td>'+
        '<td>'+fmt(b.frames)+'</td><td>'+b.rssi+'</td><td>'+b.ch+'</td></tr>';
    }).join('');
  }).catch(function(){});
}

function resetPktStats(){
  api('/api/pks/reset',{method:'POST'}).then(function(){toast('Stats reset','ok');loadPktStats();});
}

// ── Spectrum ──────────────────────────────────────────────────────────────────
function loadSpectrum(){
  api('/api/anl/heatmap').then(function(d){
    var chans=d.channels||[];
    var maxF=1;
    chans.forEach(function(c){if(c.frames>maxF)maxF=c.frames;});
    var html='';
    for(var i=1;i<=13;i++){
      var c=chans.find(function(x){return x.ch===i})||{ch:i,frames:0,avg_rssi:-100};
      var pct=Math.round(c.frames/maxF*100);
      var rssiPct=Math.max(0,Math.min(100,(c.avg_rssi+100)/70*100));
      html+='<div class="chan-bar-row">'+
        '<span class="chan-num">'+i+'</span>'+
        '<div class="chan-bar" style="height:20px"><div class="chan-fill" style="width:'+pct+'%;background:hsl('+(rssiPct*1.2)+',70%,45%)"></div></div>'+
        '<span class="chan-cnt" style="min-width:80px">'+c.frames+' fr, '+c.avg_rssi+'dBm</span></div>';
    }
    document.getElementById('spectrumBody').innerHTML=html||'<div class="empty">No data</div>';
  }).catch(function(){});

  api('/api/pks/summary').then(function(d){
    var buckets=d.rssi_hist||[];
    if(!buckets.length){document.getElementById('rssiDistBody').innerHTML='<div class="empty">No RSSI data</div>';return;}
    var maxV=1;
    buckets.forEach(function(b){if(b>maxV)maxV=b;});
    var labels=['-100','-94','-88','-82','-76','-70','-64','-58','-52','-46','-40','-34','-28','-22','-16','-10'];
    var html='';
    buckets.forEach(function(v,i){
      var pct=Math.round(v/maxV*100);
      html+='<div class="chan-bar-row">'+
        '<span class="chan-num" style="min-width:32px">'+labels[i]+'</span>'+
        '<div class="chan-bar"><div class="chan-fill" style="width:'+pct+'%"></div></div>'+
        '<span class="chan-cnt">'+v+'</span></div>';
    });
    document.getElementById('rssiDistBody').innerHTML=html;
  }).catch(function(){});
}

// ── OUI ───────────────────────────────────────────────────────────────────────
function ouiLookup(){
  var mac=document.getElementById('ouiInput').value.trim();
  if(!mac){toast('Enter a MAC address','err');return;}
  Promise.all([
    api('/api/oui/lookup?mac='+encodeURIComponent(mac)),
    api('/api/ev/lookup?mac='+encodeURIComponent(mac))
  ]).then(function(results){
    var o=results[0],e=results[1];
    var html='<div style="background:var(--bg3);border-radius:6px;padding:0.7rem">';
    html+='<div style="display:flex;justify-content:space-between;border-bottom:1px solid var(--border);padding-bottom:0.4rem;margin-bottom:0.4rem">'+
      '<span style="color:var(--text2)">OUI Vendor</span><strong>'+(o.vendor||'Unknown')+'</strong></div>';
    html+='<div style="display:flex;justify-content:space-between;border-bottom:1px solid var(--border);padding-bottom:0.4rem;margin-bottom:0.4rem">'+
      '<span style="color:var(--text2)">Extended Vendor</span><strong>'+(e.vendor||'—')+'</strong></div>';
    html+='<div style="display:flex;justify-content:space-between">'+
      '<span style="color:var(--text2)">Device Category</span><strong>'+(e.category||'—')+'</strong></div>';
    html+='</div>';
    document.getElementById('ouiResult').innerHTML=html;
  }).catch(function(){document.getElementById('ouiResult').innerHTML='<span style="color:var(--red2)">Lookup failed</span>'});
}

function loadOuiStats(){
  api('/api/ev/stats').then(function(d){
    var cats=d.categories||{};
    var html='';
    var total=0;
    Object.keys(cats).forEach(function(k){total+=cats[k]});
    Object.keys(cats).sort(function(a,b){return cats[b]-cats[a]}).forEach(function(k){
      var pct=total?Math.round(cats[k]/total*100):0;
      html+='<div class="chan-bar-row">'+
        '<span style="min-width:90px;font-size:0.72rem;color:var(--text2)">'+k+'</span>'+
        '<div class="chan-bar"><div class="chan-fill" style="width:'+pct+'%"></div></div>'+
        '<span class="chan-cnt">'+cats[k]+'</span></div>';
    });
    document.getElementById('ouiStats').innerHTML=html||'<div class="empty">No data</div>';
  }).catch(function(){});
}

// ── PCAP ──────────────────────────────────────────────────────────────────────
function startCapture(){
  var file=document.getElementById('pcapFile').value||'/capture.pcap';
  api('/api/pcap/start?file='+encodeURIComponent(file),{method:'POST'}).then(function(d){
    document.getElementById('pcapStatus').textContent='● Capturing to '+file;
    document.getElementById('pcapStatus').style.color='var(--green2)';
    toast('Capture started','ok');
  }).catch(function(){toast('Failed to start','err')});
}

function stopCapture(){
  api('/api/pcap/stop',{method:'POST'}).then(function(d){
    document.getElementById('pcapStatus').textContent='Stopped. Frames: '+(d.frames||0);
    document.getElementById('pcapStatus').style.color='var(--text2)';
    toast('Capture stopped','ok');
  }).catch(function(){toast('Failed','err')});
}

function downloadPcap(){
  window.location='/api/pcap/download';
}

// ── Port Scan ─────────────────────────────────────────────────────────────────
function startPortScan(){
  var subnet=document.getElementById('psSubnet').value||'192.168.4';
  document.getElementById('psStatus').textContent='Scanning… (may take 30s)';
  api('/api/ps/scan?subnet='+encodeURIComponent(subnet),{method:'POST'}).then(function(){
    setTimeout(loadPortResults,5000);
  }).catch(function(){toast('Scan failed','err')});
}

function loadPortResults(){
  api('/api/ps/results').then(function(d){
    var hosts=d.hosts||[];
    document.getElementById('psStatus').textContent=hosts.length+' hosts scanned';
    if(!hosts.length){document.getElementById('psBody').innerHTML='<tr><td colspan="4" class="empty">No results</td></tr>';return;}
    document.getElementById('psBody').innerHTML=hosts.map(function(h){
      var ports=(h.ports||[]).filter(function(p){return p.open}).map(function(p){return p.port}).join(', ');
      var svcs=(h.ports||[]).filter(function(p){return p.open&&p.svc}).map(function(p){return p.svc}).join(', ');
      return '<tr>'+
        '<td class="mono">'+h.ip+'</td>'+
        '<td>'+(h.alive?'<span class="badge b-active">alive</span>':'<span class="badge b-idle">dead</span>')+'</td>'+
        '<td class="mono" style="font-size:0.78rem">'+(ports||'—')+'</td>'+
        '<td style="font-size:0.78rem">'+(svcs||'—')+'</td>'+
        '</tr>';
    }).join('');
  }).catch(function(){});
}

// ── Events ────────────────────────────────────────────────────────────────────
function loadEvents(){
  api('/api/sys/events?n=64').then(function(d){
    var evs=d.events||[];
    if(!evs.length){document.getElementById('evFeed').innerHTML='<div class="empty">No events</div>';return;}
    var filt=document.getElementById('evFilter').value;
    if(filt!=='all') evs=evs.filter(function(e){return e.type===filt});
    document.getElementById('evFeed').innerHTML=evs.map(function(e){
      return '<div class="event-item ev-'+e.type+'">'+
        '<span class="ev-type">'+e.type+'</span>'+
        '<span class="ev-src">'+e.source+'</span>'+
        '<span style="flex:1">'+e.msg+'</span>'+
        '<span class="ev-time" title="ms since boot">'+e.ts+'ms</span>'+
        '</div>';
    }).join('');
  }).catch(function(){});
}

// ── Report ────────────────────────────────────────────────────────────────────
function genReport(){
  var title=document.getElementById('rptTitle').value||'Phantom32 WiFi Audit';
  document.getElementById('rptStatus').textContent='Generating…';
  api('/api/rg/generate?title='+encodeURIComponent(title),{method:'POST'}).then(function(d){
    document.getElementById('rptStatus').textContent='Done! Bytes: '+(d.size||0);
    document.getElementById('rptPreview').style.display='block';
    document.getElementById('rptBody').innerHTML='Report generated: <a href="/report" target="_blank">Open report ↗</a>';
    toast('Report ready','ok');
  }).catch(function(){document.getElementById('rptStatus').textContent='Failed';toast('Report failed','err')});
}

function downloadReport(){
  window.location='/api/rg/download';
}

// ── Attack ────────────────────────────────────────────────────────────────────
function quickDeauth(bssid,ch){
  if(!confirm('Send deauth burst to '+bssid+'?')) return;
  api('/api/sched/deauth?bssid='+encodeURIComponent(bssid)+'&ch='+ch+'&pps=10&dur=5000',{method:'POST'})
    .then(function(d){toast('Deauth job #'+(d.job_id||'?')+' started','ok')})
    .catch(function(){toast('Deauth failed','err')});
}

function triggerScan(){
  api('/api/sys/scan_start',{method:'POST'}).then(function(){
    toast('Scan started (3s)','ok');
    setTimeout(loadDashAps,3500);
  });
}

function estopAll(){
  if(!confirm('Emergency stop — cancel ALL running attack jobs?')) return;
  api('/api/sched/estop',{method:'POST'}).then(function(){toast('E-STOP sent','ok')});
}

// ── Auto refresh ──────────────────────────────────────────────────────────────
function refreshCurrentPage(){
  if(currentPage==='dashboard') loadDashboard();
  else if(currentPage==='aps') loadAps();
  else if(currentPage==='stas') loadStas();
  else if(currentPage==='vulns') loadVulns();
  else if(currentPage==='threats') loadThreats();
  else if(currentPage==='pktstats') loadPktStats();
  else if(currentPage==='events') loadEvents();
}

loadDashboard();
autoRefreshInterval=setInterval(refreshCurrentPage,5000);
</script>
</body>
</html>
)rawhtml";

static void wuim_routes() {
  server.on("/", HTTP_GET, []() { server.send_P(200, "text/html", WUIM_PAGE); });
  server.on("/index.html", HTTP_GET, []() { server.send_P(200, "text/html", WUIM_PAGE); });
}

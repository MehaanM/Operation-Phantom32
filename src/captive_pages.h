#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <string.h>
#include <stdint.h>

// ── Captive Portal Pages ──────────────────────────────────────────────────────
//
// Self-contained HTML/CSS/JS portal pages stored in PROGMEM.
// Served by the WebServer for the Phantom32 captive portal, dashboard,
// login simulation, and device info pages.

// ── Forward declarations ──────────────────────────────────────────────────────
extern WebServer server;

// ─────────────────────────────────────────────────────────────────────────────
// PAGE 1: Main Dashboard
// ─────────────────────────────────────────────────────────────────────────────
static const char CP_DASHBOARD[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Phantom32 &mdash; Dashboard</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#0a0e1a;color:#c9d1d9;font-family:system-ui,-apple-system,sans-serif;min-height:100vh}
.top-bar{background:#161b22;border-bottom:1px solid #21262d;padding:0.6rem 1.2rem;display:flex;align-items:center;justify-content:space-between}
.logo{font-size:1rem;font-weight:700;color:#f0f6fc;letter-spacing:-0.02em}
.logo span{color:#58a6ff}
.top-bar-right{display:flex;gap:0.6rem;align-items:center}
.led{width:8px;height:8px;border-radius:50%;background:#3fb950;box-shadow:0 0 6px #3fb950}
.led.orange{background:#ffa657;box-shadow:0 0 6px #ffa657}
.uptime{font-size:0.75rem;color:#8b949e}
.sidebar{position:fixed;left:0;top:41px;bottom:0;width:220px;background:#0d1117;border-right:1px solid #21262d;padding:1rem 0;overflow-y:auto;z-index:10}
.nav-section{padding:0.3rem 1rem 0.1rem;font-size:0.65rem;text-transform:uppercase;letter-spacing:0.08em;color:#484f58;font-weight:600}
.nav-item{display:flex;align-items:center;gap:0.5rem;padding:0.5rem 1rem;cursor:pointer;color:#8b949e;font-size:0.85rem;transition:all 0.15s}
.nav-item:hover,.nav-item.active{background:#161b22;color:#f0f6fc}
.nav-item.active{border-right:2px solid #58a6ff}
.nav-icon{width:16px;text-align:center;font-size:0.9rem}
.main{margin-left:220px;padding:1.2rem;padding-top:53px}
.page{display:none}.page.active{display:block}
.stat-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:0.8rem;margin-bottom:1.2rem}
.stat-card{background:#161b22;border:1px solid #21262d;border-radius:8px;padding:0.9rem}
.stat-card .val{font-size:1.6rem;font-weight:700;color:#f0f6fc}
.stat-card .lbl{font-size:0.7rem;color:#8b949e;text-transform:uppercase;letter-spacing:0.05em}
.stat-card .delta{font-size:0.7rem;color:#3fb950;margin-top:0.1rem}
.card{background:#161b22;border:1px solid #21262d;border-radius:8px;padding:1rem;margin-bottom:1rem}
.card-header{font-size:0.85rem;font-weight:600;color:#f0f6fc;margin-bottom:0.7rem;padding-bottom:0.5rem;border-bottom:1px solid #21262d;display:flex;align-items:center;justify-content:space-between}
.btn{background:#21262d;color:#c9d1d9;border:1px solid #30363d;border-radius:6px;padding:0.35rem 0.8rem;cursor:pointer;font-size:0.8rem;transition:all 0.15s}
.btn:hover{background:#30363d;color:#f0f6fc}
.btn.primary{background:#1f6feb;border-color:#1f6feb;color:#fff}.btn.primary:hover{background:#388bfd}
.btn.danger{background:#8f1f26;border-color:#8f1f26;color:#fff}.btn.danger:hover{background:#b91c1c}
.btn.success{background:#1a4731;border-color:#1a4731;color:#3fb950}.btn.success:hover{background:#206040}
.table-wrap{overflow-x:auto}
table{width:100%;border-collapse:collapse;font-size:0.8rem}
th{background:#1c2130;color:#8b949e;text-align:left;padding:0.5rem 0.7rem;border-bottom:1px solid #21262d;font-weight:600;font-size:0.7rem;text-transform:uppercase;letter-spacing:0.04em}
td{padding:0.5rem 0.7rem;border-bottom:1px solid #21262d;color:#c9d1d9}
tr:hover td{background:rgba(255,255,255,0.02)}
.badge{display:inline-block;padding:0.15rem 0.5rem;border-radius:10px;font-size:0.7rem;font-weight:600}
.b-open{background:#0d2b1f;color:#3fb950;border:1px solid #1a4731}
.b-wep{background:#2d1b1b;color:#ff7b72;border:1px solid #6e1f1f}
.b-wpa{background:#1a2033;color:#58a6ff;border:1px solid #1f4068}
.b-wpa3{background:#1a2a1a;color:#3fb950;border:1px solid #1a4731}
.channel-bar{display:flex;align-items:flex-end;gap:3px;height:60px}
.ch-bar{flex:1;background:#21262d;border-radius:2px 2px 0 0;transition:height 0.3s;cursor:pointer;position:relative}
.ch-bar:hover{background:#388bfd}
.ch-num{font-size:0.55rem;color:#484f58;text-align:center;margin-top:2px}
.rssi-gauge{width:100%;height:8px;background:#21262d;border-radius:4px;overflow:hidden;margin:0.3rem 0}
.rssi-fill{height:100%;background:linear-gradient(90deg,#ff7b72,#e3b341,#3fb950);transition:width 0.5s}
.log-box{background:#0d1117;border:1px solid #21262d;border-radius:4px;height:160px;overflow-y:auto;padding:0.5rem;font-size:0.75rem;font-family:monospace;color:#8b949e}
.log-box .err{color:#ff7b72}
.log-box .ok{color:#3fb950}
.log-box .info{color:#58a6ff}
.form-row{display:flex;gap:0.6rem;align-items:center;margin-bottom:0.5rem;flex-wrap:wrap}
input[type=text],input[type=number],select{background:#0d1117;border:1px solid #30363d;border-radius:4px;color:#c9d1d9;padding:0.4rem 0.6rem;font-size:0.8rem}
input:focus,select:focus{outline:none;border-color:#58a6ff}
.tool-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:0.8rem}
.tool-card{background:#0d1117;border:1px solid #21262d;border-radius:8px;padding:0.9rem}
.tool-card h4{font-size:0.85rem;color:#f0f6fc;margin-bottom:0.3rem}
.tool-card p{font-size:0.75rem;color:#8b949e;margin-bottom:0.7rem}
.spinner{display:inline-block;width:14px;height:14px;border:2px solid #30363d;border-top-color:#58a6ff;border-radius:50%;animation:spin 0.8s linear infinite;vertical-align:middle;margin-left:6px}
@keyframes spin{to{transform:rotate(360deg)}}
.hidden{display:none!important}
.sep{color:#30363d;padding:0 0.3rem}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">&#x1F47B; <span>Phantom</span>32</div>
  <div class="top-bar-right">
    <div class="led" id="wifiLed"></div>
    <span class="uptime" id="uptimeLbl">...</span>
  </div>
</div>

<div class="sidebar">
  <div class="nav-section">Monitor</div>
  <div class="nav-item active" onclick="showPage('dashboard')"><span class="nav-icon">&#x1F4CA;</span> Dashboard</div>
  <div class="nav-item" onclick="showPage('aps')"><span class="nav-icon">&#x1F4F6;</span> Access Points</div>
  <div class="nav-item" onclick="showPage('stations')"><span class="nav-icon">&#x1F4BB;</span> Stations</div>
  <div class="nav-item" onclick="showPage('netmap')"><span class="nav-icon">&#x1F5FA;</span> Network Map</div>
  <div class="nav-section">Analyze</div>
  <div class="nav-item" onclick="showPage('vulns')"><span class="nav-icon">&#x26A0;</span> Vulnerabilities</div>
  <div class="nav-item" onclick="showPage('threats')"><span class="nav-icon">&#x1F6A8;</span> Threats</div>
  <div class="nav-item" onclick="showPage('fingerprint')"><span class="nav-icon">&#x1F50D;</span> Fingerprint</div>
  <div class="nav-item" onclick="showPage('signals')"><span class="nav-icon">&#x1F4E1;</span> Signals</div>
  <div class="nav-section">Tools</div>
  <div class="nav-item" onclick="showPage('tools')"><span class="nav-icon">&#x1F527;</span> Attack Tools</div>
  <div class="nav-item" onclick="showPage('capture')"><span class="nav-icon">&#x1F4BE;</span> PCAP Capture</div>
  <div class="nav-item" onclick="showPage('report')"><span class="nav-icon">&#x1F4C4;</span> Reports</div>
  <div class="nav-section">System</div>
  <div class="nav-item" onclick="showPage('oui')"><span class="nav-icon">&#x1F3F7;</span> OUI Lookup</div>
  <div class="nav-item" onclick="showPage('settings')"><span class="nav-icon">&#x2699;</span> Settings</div>
</div>

<div class="main">
  <!-- Dashboard -->
  <div class="page active" id="page-dashboard">
    <div class="stat-grid" id="dashStats"></div>
    <div style="display:grid;grid-template-columns:1fr 1fr;gap:1rem;margin-bottom:1rem">
      <div class="card">
        <div class="card-header">Channel Activity</div>
        <div class="channel-bar" id="chanBars"></div>
        <div style="display:flex;gap:3px;margin-top:2px" id="chanNums"></div>
      </div>
      <div class="card">
        <div class="card-header">Recent Events</div>
        <div class="log-box" id="eventLog"></div>
      </div>
    </div>
  </div>

  <!-- APs -->
  <div class="page" id="page-aps">
    <div class="card">
      <div class="card-header">Access Points
        <button class="btn" onclick="loadAps()">&#x1F504; Refresh</button>
      </div>
      <div class="table-wrap">
        <table id="apTable">
          <thead><tr><th>BSSID</th><th>SSID</th><th>CH</th><th>RSSI</th><th>ENC</th><th>PMF</th><th>Vendor</th></tr></thead>
          <tbody></tbody>
        </table>
      </div>
    </div>
  </div>

  <!-- Stations -->
  <div class="page" id="page-stations">
    <div class="card">
      <div class="card-header">Observed Stations
        <button class="btn" onclick="loadStas()">&#x1F504; Refresh</button>
      </div>
      <div class="table-wrap">
        <table id="staTable">
          <thead><tr><th>MAC</th><th>Vendor</th><th>OS</th><th>RSSI</th><th>Probes</th><th>Age</th></tr></thead>
          <tbody></tbody>
        </table>
      </div>
    </div>
  </div>

  <!-- Vulnerabilities -->
  <div class="page" id="page-vulns">
    <div class="card">
      <div class="card-header">Findings
        <button class="btn" onclick="loadVulns()">&#x1F504; Refresh</button>
      </div>
      <div class="table-wrap">
        <table id="vulnTable">
          <thead><tr><th>BSSID</th><th>SSID</th><th>Issue</th><th>Sev</th><th>Score</th><th>Remediation</th></tr></thead>
          <tbody></tbody>
        </table>
      </div>
    </div>
  </div>

  <!-- Tools -->
  <div class="page" id="page-tools">
    <div class="tool-grid">
      <div class="tool-card">
        <h4>&#x1F4A5; Deauth</h4>
        <p>Send 802.11 deauth frames to disconnect clients.</p>
        <div class="form-row">
          <input type="text" id="daBssid" placeholder="AA:BB:CC:DD:EE:FF" size="17">
          <input type="number" id="daCh" placeholder="CH" value="6" style="width:50px">
        </div>
        <div class="form-row">
          <input type="number" id="daPps" placeholder="PPS" value="10" style="width:60px">
          <input type="number" id="daDur" placeholder="ms" value="5000" style="width:70px">
          <button class="btn danger" onclick="doDeauth()">&#x26A1; Send</button>
        </div>
        <div id="daResult" style="font-size:0.75rem;margin-top:0.4rem;color:#8b949e"></div>
      </div>
      <div class="tool-card">
        <h4>&#x1F4E1; Probe Flood</h4>
        <p>Send probe requests with random SSIDs.</p>
        <div class="form-row">
          <input type="text" id="pfSsid" placeholder="SSID (blank=random)" size="20">
          <button class="btn primary" onclick="doProbe()">&#x1F50E; Send</button>
        </div>
      </div>
      <div class="tool-card">
        <h4>&#x1F4E2; Beacon Spam</h4>
        <p>Broadcast fake beacon frames.</p>
        <div class="form-row">
          <input type="text" id="bsSsid" placeholder="SSID" size="20">
          <input type="number" id="bsCh" placeholder="CH" value="6" style="width:50px">
          <button class="btn primary" onclick="doBeacon()">&#x1F4E2; Spam</button>
        </div>
      </div>
      <div class="tool-card">
        <h4>&#x1F527; Job Scheduler</h4>
        <p>View and manage scheduled attack jobs.</p>
        <div id="jobList" style="font-size:0.75rem;color:#8b949e;min-height:40px">Loading...</div>
        <button class="btn danger" onclick="doEstop()" style="margin-top:0.5rem">&#x1F6D1; E-STOP</button>
        <button class="btn" onclick="loadJobs()" style="margin-left:0.4rem">&#x1F504; Refresh</button>
      </div>
    </div>
  </div>

  <!-- PCAP -->
  <div class="page" id="page-capture">
    <div class="card">
      <div class="card-header">PCAP Capture</div>
      <div class="form-row">
        <input type="text" id="pcapPath" value="/cap.pcap" size="20">
        <button class="btn primary" onclick="startCapture()">&#x25CF; Start</button>
        <button class="btn" onclick="stopCapture()">&#x25A0; Stop</button>
        <button class="btn" onclick="listPcap()">&#x1F4C1; List</button>
      </div>
      <div id="pcapFiles" style="font-size:0.8rem;margin-top:0.7rem;color:#8b949e"></div>
    </div>
  </div>

  <!-- Report -->
  <div class="page" id="page-report">
    <div class="card">
      <div class="card-header">Generate Security Report</div>
      <p style="font-size:0.85rem;color:#8b949e;margin-bottom:0.8rem">
        Generates a full HTML security audit report from all scan data.
      </p>
      <button class="btn primary" onclick="genReport()">&#x1F4C4; Generate Report</button>
      <a id="reportLink" class="btn success hidden" href="/report" target="_blank" style="margin-left:0.5rem">&#x1F517; Open Report</a>
      <div id="reportStatus" style="font-size:0.8rem;margin-top:0.7rem;color:#8b949e"></div>
    </div>
  </div>

  <!-- OUI -->
  <div class="page" id="page-oui">
    <div class="card">
      <div class="card-header">OUI / MAC Vendor Lookup</div>
      <div class="form-row">
        <input type="text" id="ouiMac" placeholder="AA:BB:CC:DD:EE:FF" size="20">
        <button class="btn primary" onclick="ouiLookup()">&#x1F50D; Lookup</button>
      </div>
      <div id="ouiResult" style="font-size:0.85rem;margin-top:0.7rem;color:#8b949e;min-height:40px"></div>
    </div>
  </div>

  <!-- Settings -->
  <div class="page" id="page-settings">
    <div class="card">
      <div class="card-header">Device Settings</div>
      <div id="settingsInfo" style="font-size:0.8rem;color:#8b949e">Loading...</div>
    </div>
  </div>

  <!-- Net Map / Fingerprint / Threats / Signals placeholder pages -->
  <div class="page" id="page-netmap"><div class="card"><div class="card-header">Network Map</div>
  <p style="color:#8b949e;font-size:0.85rem">Nodes and edges from passive observation. Use GET /api/nm/nodes and /api/nm/edges for raw data.</p></div></div>
  <div class="page" id="page-fingerprint"><div class="card"><div class="card-header">Device Fingerprint</div>
  <div id="fpContent" style="font-size:0.8rem;color:#8b949e">Loading...</div></div></div>
  <div class="page" id="page-threats"><div class="card"><div class="card-header">Threat Intelligence</div>
  <div id="tiContent" style="font-size:0.8rem;color:#8b949e">Loading...</div></div></div>
  <div class="page" id="page-signals"><div class="card"><div class="card-header">Signal Analysis</div>
  <div id="saContent" style="font-size:0.8rem;color:#8b949e">Loading...</div></div></div>
</div>

<script>
var curPage='dashboard';
function showPage(id){
  document.querySelectorAll('.page').forEach(function(p){p.classList.remove('active')});
  document.querySelectorAll('.nav-item').forEach(function(n){n.classList.remove('active')});
  var el=document.getElementById('page-'+id);
  if(el) el.classList.add('active');
  curPage=id;
  if(id==='dashboard') refreshDash();
  if(id==='aps') loadAps();
  if(id==='stations') loadStas();
  if(id==='vulns') loadVulns();
  if(id==='tools') loadJobs();
  if(id==='fingerprint') loadFp();
  if(id==='threats') loadTi();
  if(id==='signals') loadSa();
  if(id==='settings') loadSettings();
}
function api(url,cb,opt){
  fetch(url,opt||{}).then(function(r){return r.json()}).then(cb).catch(function(e){console.warn(url,e)});
}
function enc_badge(enc){
  if(!enc||enc==='open') return '<span class="badge b-open">Open</span>';
  if(enc.indexOf('WEP')>=0) return '<span class="badge b-wep">WEP</span>';
  if(enc.indexOf('WPA3')>=0||enc.indexOf('SAE')>=0) return '<span class="badge b-wpa3">WPA3</span>';
  return '<span class="badge b-wpa">'+enc+'</span>';
}
function sev_badge(s){
  var m={CRITICAL:'#ff7b72',HIGH:'#ffa657',MEDIUM:'#e3b341',LOW:'#3fb950',INFO:'#58a6ff'};
  var c=m[s]||'#8b949e';
  return '<span class="badge" style="background:'+c+';color:#fff">'+s+'</span>';
}

function refreshDash(){
  api('/api/anl/summary',function(d){
    var html='';
    html+='<div class="stat-card"><div class="val">'+(d.ap_count||0)+'</div><div class="lbl">APs</div></div>';
    html+='<div class="stat-card"><div class="val">'+(d.sta_count||0)+'</div><div class="lbl">Stations</div></div>';
    html+='<div class="stat-card"><div class="val">'+(d.total_frames||0)+'</div><div class="lbl">Frames</div></div>';
    html+='<div class="stat-card"><div class="val">'+(d.pps||0)+'</div><div class="lbl">Frame/s</div></div>';
    document.getElementById('dashStats').innerHTML=html;
  });
  api('/api/anl/heatmap',function(d){
    var bars=document.getElementById('chanBars');
    var nums=document.getElementById('chanNums');
    if(!bars||!d.channels) return;
    var maxf=1;
    for(var i=0;i<d.channels.length;i++) if(d.channels[i].frames>maxf) maxf=d.channels[i].frames;
    var bh=''; var nh='';
    for(var i=0;i<d.channels.length;i++){
      var c=d.channels[i];
      var pct=Math.round((c.frames/maxf)*100);
      bh+='<div class="ch-bar" style="height:'+pct+'%" title="Ch '+c.ch+': '+c.frames+' frames"></div>';
      nh+='<div style="flex:1;font-size:0.55rem;color:#484f58;text-align:center">'+c.ch+'</div>';
    }
    bars.innerHTML=bh; nums.innerHTML=nh;
  });
  document.getElementById('uptimeLbl').textContent='Uptime: '+Math.round(performance.now()/1000)+'s';
}

function loadAps(){
  api('/api/cs/aps',function(d){
    var tb=document.querySelector('#apTable tbody');
    if(!tb) return;
    var html='';
    var aps=d.aps||[];
    for(var i=0;i<aps.length;i++){
      var a=aps[i];
      html+='<tr><td><code>'+a.bssid+'</code></td><td>'+(a.ssid||'')+'</td><td>'+a.ch+'</td>';
      html+='<td>'+(a.rssi||0)+' dBm</td><td>'+enc_badge(a.enc)+'</td>';
      html+='<td>'+(a.pmf?'<span class="badge b-open">Yes</span>':'<span class="badge b-wep">No</span>')+'</td>';
      html+='<td style="color:#8b949e">'+(a.vendor||'')+'</td></tr>';
    }
    tb.innerHTML=html||'<tr><td colspan="7" style="color:#484f58;text-align:center">No APs found</td></tr>';
  });
}

function loadStas(){
  api('/api/fp/profiles',function(d){
    var tb=document.querySelector('#staTable tbody');
    if(!tb) return;
    var html='';
    var ps=d.profiles||[];
    for(var i=0;i<ps.length;i++){
      var p=ps[i];
      html+='<tr><td><code>'+p.mac+'</code></td>';
      html+='<td style="color:#8b949e">'+(p.vendor||'')+'</td>';
      html+='<td>'+(p.os||'Unknown')+'</td>';
      html+='<td>'+(p.rssi||0)+' dBm</td>';
      html+='<td>'+(p.probe_count||0)+'</td>';
      html+='<td style="color:#8b949e">'+(p.age_s||0)+'s</td></tr>';
    }
    tb.innerHTML=html||'<tr><td colspan="6" style="color:#484f58;text-align:center">No stations found</td></tr>';
  });
}

function loadVulns(){
  api('/api/vs/findings',function(d){
    var tb=document.querySelector('#vulnTable tbody');
    if(!tb) return;
    var html='';
    var fs=d.findings||[];
    for(var i=0;i<fs.length;i++){
      var f=fs[i];
      html+='<tr><td><code>'+f.bssid+'</code></td><td>'+(f.ssid||'')+'</td>';
      html+='<td>'+(f.issue||'')+'</td><td>'+sev_badge(f.severity)+'</td>';
      html+='<td>'+f.score+'</td><td style="font-size:0.75rem;color:#8b949e">'+(f.remediation||'')+'</td></tr>';
    }
    tb.innerHTML=html||'<tr><td colspan="6" style="color:#484f58;text-align:center">No findings</td></tr>';
  });
}

function doDeauth(){
  var bssid=document.getElementById('daBssid').value;
  var ch=document.getElementById('daCh').value;
  var pps=document.getElementById('daPps').value;
  var dur=document.getElementById('daDur').value;
  if(!bssid){document.getElementById('daResult').textContent='BSSID required';return;}
  fetch('/api/sched/deauth?bssid='+encodeURIComponent(bssid)+'&ch='+ch+'&pps='+pps+'&dur='+dur,{method:'POST'})
    .then(function(r){return r.json()}).then(function(d){
      document.getElementById('daResult').textContent=d.job_id?'Job '+d.job_id+' started':(d.error||'Error');
    });
}
function doProbe(){
  var ssid=document.getElementById('pfSsid').value;
  fetch('/api/pf/probe?ssid='+encodeURIComponent(ssid),{method:'POST'})
    .then(function(r){return r.json()}).then(function(d){console.log(d)});
}
function doBeacon(){
  var ssid=document.getElementById('bsSsid').value;
  var ch=document.getElementById('bsCh').value;
  if(!ssid) return;
  fetch('/api/pf/beacon?ssid='+encodeURIComponent(ssid)+'&ch='+ch,{method:'POST'})
    .then(function(r){return r.json()}).then(function(d){console.log(d)});
}
function doEstop(){
  if(!confirm('Emergency stop all jobs?')) return;
  fetch('/api/sched/estop',{method:'POST'}).then(function(){loadJobs()});
}
function loadJobs(){
  api('/api/sched/jobs',function(d){
    var jobs=d.jobs||[];
    var html=jobs.length?jobs.map(function(j){
      var states=['pending','active','paused','done','failed','cancelled'];
      return '<div style="margin-bottom:0.3rem"><code>#'+j.id+'</code> '+j.type+
             ' <span style="color:#58a6ff">['+states[j.state]+']</span>'+
             ' '+j.sent+'pkts sent'+
             (j.state===1?'<button class="btn" onclick="cancelJob('+j.id+')" style="margin-left:0.4rem;font-size:0.7rem;padding:0.1rem 0.4rem">Cancel</button>':'')+
             '</div>';
    }).join(''):'<span style="color:#484f58">No jobs</span>';
    document.getElementById('jobList').innerHTML=html;
  });
}
function cancelJob(id){
  fetch('/api/sched/cancel?id='+id,{method:'POST'}).then(function(){loadJobs()});
}
function startCapture(){
  var path=document.getElementById('pcapPath').value||'/cap.pcap';
  fetch('/api/pcw/open?path='+encodeURIComponent(path),{method:'POST'}).then(function(){listPcap()});
}
function stopCapture(){
  fetch('/api/pcw/stop',{method:'POST'}).then(function(){listPcap()});
}
function listPcap(){
  api('/api/pcw/list',function(d){
    var html='<strong>Active:</strong> '+(d.active||'none')+'<br>';
    html+='<strong>Total frames:</strong> '+d.total_frames+'<br>';
    var fs=d.files||[];
    if(fs.length){
      html+='<ul style="margin-top:0.4rem;padding-left:1rem">';
      fs.forEach(function(f){
        html+='<li><a href="/api/pcw/download?path='+encodeURIComponent(f.path)+'">'+f.path+'</a> &mdash; '+f.frames+' frames, '+f.bytes+' bytes</li>';
      });
      html+='</ul>';
    }
    document.getElementById('pcapFiles').innerHTML=html;
  });
}
function genReport(){
  document.getElementById('reportStatus').textContent='Generating...';
  fetch('/api/rg/generate',{method:'POST'}).then(function(r){return r.json()}).then(function(d){
    if(d.ok){
      document.getElementById('reportStatus').textContent='Generated: '+d.path;
      document.getElementById('reportLink').classList.remove('hidden');
    } else {
      document.getElementById('reportStatus').textContent='Error: '+(d.error||'unknown');
    }
  });
}
function ouiLookup(){
  var mac=document.getElementById('ouiMac').value;
  if(!mac) return;
  api('/api/ouf/mac?mac='+encodeURIComponent(mac),function(d){
    document.getElementById('ouiResult').innerHTML=
      '<pre style="white-space:pre-wrap;font-size:0.78rem">'+JSON.stringify(d,null,2)+'</pre>';
  });
}
function loadFp(){
  api('/api/fp/summary',function(d){
    document.getElementById('fpContent').innerHTML=
      '<pre style="white-space:pre-wrap;font-size:0.78rem">'+JSON.stringify(d,null,2)+'</pre>';
  });
}
function loadTi(){
  api('/api/ti/summary',function(d){
    document.getElementById('tiContent').innerHTML=
      '<pre style="white-space:pre-wrap;font-size:0.78rem">'+JSON.stringify(d,null,2)+'</pre>';
  });
}
function loadSa(){
  api('/api/sa/tracks',function(d){
    document.getElementById('saContent').innerHTML=
      '<pre style="white-space:pre-wrap;font-size:0.78rem">'+JSON.stringify(d,null,2)+'</pre>';
  });
}
function loadSettings(){
  document.getElementById('settingsInfo').innerHTML=
    '<p>Hostname: Phantom32 &nbsp;|&nbsp; IP: 192.168.4.1</p>'+
    '<p style="margin-top:0.3rem">Channel: controlled by scanner &nbsp;|&nbsp; Mode: AP+STA</p>';
}

setInterval(function(){if(curPage==='dashboard')refreshDash();},3000);
refreshDash();
</script>
</body>
</html>
)rawhtml";

// ─────────────────────────────────────────────────────────────────────────────
// PAGE 2: Captive portal login simulation page
// ─────────────────────────────────────────────────────────────────────────────
static const char CP_PORTAL[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Network Login</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#f0f2f5;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;min-height:100vh;display:flex;align-items:center;justify-content:center}
.card{background:#fff;border-radius:12px;box-shadow:0 4px 24px rgba(0,0,0,0.12);padding:2.5rem;width:100%;max-width:400px;margin:1rem}
.logo{text-align:center;margin-bottom:1.5rem}
.logo h1{font-size:1.4rem;font-weight:700;color:#1a1a2e}
.logo p{color:#666;font-size:0.85rem;margin-top:0.3rem}
.form-group{margin-bottom:1rem}
label{display:block;font-size:0.85rem;font-weight:500;color:#333;margin-bottom:0.3rem}
input[type=text],input[type=password]{width:100%;padding:0.7rem 0.9rem;border:1px solid #ddd;border-radius:8px;font-size:0.9rem;color:#333;background:#fafafa;transition:border-color 0.2s}
input:focus{outline:none;border-color:#1a73e8;background:#fff}
.btn-submit{width:100%;padding:0.8rem;background:#1a73e8;color:#fff;border:none;border-radius:8px;font-size:0.95rem;font-weight:600;cursor:pointer;transition:background 0.2s;margin-top:0.5rem}
.btn-submit:hover{background:#1557b0}
.footer-note{text-align:center;font-size:0.75rem;color:#999;margin-top:1rem}
.spinner{display:none;text-align:center;margin-top:1rem;color:#1a73e8}
.error-msg{background:#fff3cd;border:1px solid #ffc107;border-radius:6px;padding:0.6rem 0.8rem;font-size:0.82rem;color:#856404;margin-bottom:1rem;display:none}
.tos{font-size:0.75rem;color:#888;margin-top:0.8rem;line-height:1.4}
</style>
</head>
<body>
<div class="card">
  <div class="logo">
    <h1>&#x1F4F6; Network Access</h1>
    <p>Sign in to continue</p>
  </div>
  <div class="error-msg" id="errMsg"></div>
  <form id="loginForm">
    <div class="form-group">
      <label>Username or Email</label>
      <input type="text" id="username" autocomplete="username" placeholder="you@example.com">
    </div>
    <div class="form-group">
      <label>Password</label>
      <input type="password" id="password" autocomplete="current-password" placeholder="&bull;&bull;&bull;&bull;&bull;&bull;&bull;&bull;">
    </div>
    <button type="submit" class="btn-submit">Sign In</button>
    <div class="tos">By signing in, you agree to the network terms of service and usage policy.</div>
  </form>
  <div class="spinner" id="spinner">Authenticating&#x2026;</div>
  <div class="footer-note">Having trouble? Contact network support.</div>
</div>
<script>
document.getElementById('loginForm').addEventListener('submit',function(e){
  e.preventDefault();
  var user=document.getElementById('username').value;
  var pass=document.getElementById('password').value;
  document.getElementById('spinner').style.display='block';
  document.querySelector('.btn-submit').disabled=true;
  fetch('/api/portal/submit',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:'user='+encodeURIComponent(user)+'&pass='+encodeURIComponent(pass)})
  .then(function(r){return r.json()}).then(function(d){
    document.getElementById('spinner').style.display='none';
    if(d.ok){window.location.href='/portal/success';}
    else{var e=document.getElementById('errMsg');e.textContent=d.error||'Authentication failed.';e.style.display='block';document.querySelector('.btn-submit').disabled=false;}
  }).catch(function(){
    document.getElementById('spinner').style.display='none';
    document.querySelector('.btn-submit').disabled=false;
  });
});
</script>
</body>
</html>
)rawhtml";

// ─────────────────────────────────────────────────────────────────────────────
// PAGE 3: Success / redirect page
// ─────────────────────────────────────────────────────────────────────────────
static const char CP_SUCCESS[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html>
<head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<meta http-equiv="refresh" content="5;url=http://google.com">
<title>Connected</title>
<style>
body{background:#0a0;font-family:system-ui;display:flex;align-items:center;justify-content:center;min-height:100vh;color:#fff}
.box{text-align:center;padding:2rem}
.icon{font-size:4rem}
h2{margin:0.5rem 0;font-size:1.4rem}
p{opacity:0.8;font-size:0.9rem}
</style>
</head>
<body>
<div class="box">
  <div class="icon">&#x2705;</div>
  <h2>Connected!</h2>
  <p>You are now connected to the network.</p>
  <p style="margin-top:0.5rem;font-size:0.8rem">Redirecting in 5 seconds&hellip;</p>
</div>
</body>
</html>
)rawhtml";

// ─────────────────────────────────────────────────────────────────────────────
// PAGE 4: 404 / error page
// ─────────────────────────────────────────────────────────────────────────────
static const char CP_404[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html>
<head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>404 &mdash; Not Found</title>
<style>
body{background:#0a0e1a;color:#c9d1d9;font-family:system-ui;display:flex;align-items:center;justify-content:center;min-height:100vh}
.box{text-align:center;padding:2rem}
.code{font-size:5rem;font-weight:700;color:#21262d}
h2{color:#8b949e;font-size:1.1rem;font-weight:400}
a{color:#58a6ff;text-decoration:none;margin-top:1rem;display:inline-block;font-size:0.85rem}
</style>
</head>
<body>
<div class="box">
  <div class="code">404</div>
  <h2>Page not found</h2>
  <a href="/">&#x2190; Back to dashboard</a>
</div>
</body>
</html>
)rawhtml";

// ─────────────────────────────────────────────────────────────────────────────
// Route registration
// ─────────────────────────────────────────────────────────────────────────────
static void cp_routes() {
  // Main dashboard
  server.on("/",HTTP_GET,[](){
    server.send_P(200,"text/html",CP_DASHBOARD);
  });
  server.on("/index.html",HTTP_GET,[](){
    server.send_P(200,"text/html",CP_DASHBOARD);
  });

  // Portal pages
  server.on("/portal",HTTP_GET,[](){
    server.send_P(200,"text/html",CP_PORTAL);
  });
  server.on("/portal/",HTTP_GET,[](){
    server.send_P(200,"text/html",CP_PORTAL);
  });
  server.on("/portal/success",HTTP_GET,[](){
    server.send_P(200,"text/html",CP_SUCCESS);
  });

  // Captive portal detection responses (iOS, Android, Windows probes)
  server.on("/generate_204",HTTP_GET,[](){
    server.send(302,"text/plain","");
    server.sendHeader("Location","http://192.168.4.1/portal",true);
  });
  server.on("/hotspot-detect.html",HTTP_GET,[](){
    server.send_P(200,"text/html",CP_PORTAL);
  });
  server.on("/ncsi.txt",HTTP_GET,[](){
    server.send(200,"text/plain","Microsoft NCSI");
  });
  server.on("/connecttest.txt",HTTP_GET,[](){
    server.send(200,"text/plain","Microsoft Connect Test");
  });
  server.on("/redirect",HTTP_GET,[](){
    server.sendHeader("Location","http://192.168.4.1/portal",true);
    server.send(302,"text/plain","");
  });
  server.on("/portal/submit",HTTP_POST,[](){
    // Log credentials if present
    String user=server.hasArg("user")?server.arg("user"):"";
    String pass=server.hasArg("pass")?server.arg("pass"):"";
    Serial.printf("[PORTAL] Credentials: user=%s pass=%s\n",user.c_str(),pass.c_str());
    // Always succeed after a brief delay simulation
    server.send(200,"application/json","{\"ok\":1}");
  });

  // 404 handler
  server.onNotFound([](){
    server.send_P(404,"text/html",CP_404);
  });
}

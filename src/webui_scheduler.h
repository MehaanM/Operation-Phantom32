#pragma once
// webui_scheduler.h — Scheduler management web UI.
// Route: /scheduler. Prefix: wusch_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUSCH_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Scheduler — Phantom32</title>
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
.btn-primary{background:var(--accent);color:#000;}
.btn-primary:hover{opacity:.85;}
.btn-danger{background:var(--danger);color:#fff;}
.btn-warn{background:var(--warn);color:#000;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.btn-sm{padding:4px 9px;font-size:11px;}
.action-row{display:flex;gap:8px;flex-wrap:wrap;margin-top:14px;}
.field-row{display:flex;gap:16px;align-items:flex-start;padding:10px 0;border-bottom:1px solid var(--border);}
.field-row:last-child{border-bottom:none;}
.field-label{width:150px;min-width:150px;font-size:13px;color:var(--text);padding-top:2px;}
.field-label small{display:block;font-size:11px;color:var(--muted);margin-top:2px;}
.field-input{flex:1;}
input[type=text],input[type=number],select{width:100%;padding:7px 10px;background:var(--bg3);
  border:1px solid var(--border);border-radius:5px;color:var(--text);font-size:13px;outline:none;}
input:focus,select:focus{border-color:var(--accent);}
table{width:100%;border-collapse:collapse;font-size:13px;}
th{text-align:left;padding:8px 12px;font-size:11px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;}
td{padding:9px 12px;border-bottom:1px solid var(--border);vertical-align:top;}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
.badge{display:inline-block;padding:2px 8px;border-radius:12px;font-size:11px;font-weight:600;}
.badge-active{background:rgba(34,197,94,.15);color:var(--success);}
.badge-disabled{background:rgba(100,116,139,.15);color:var(--muted);}
.badge-warn{background:rgba(245,158,11,.15);color:var(--warn);}
.log-feed{background:var(--bg3);border-radius:5px;padding:12px;height:180px;overflow-y:auto;font-family:monospace;font-size:12px;}
.log-line{padding:2px 0;border-bottom:1px solid rgba(42,48,69,.5);}
.log-line:last-child{border-bottom:none;}
@media(max-width:700px){.stat-row{grid-template-columns:1fr 1fr;}}
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
    <a href="/devices">Devices</a>
    <a href="/capture">Capture</a>
    <a href="/scheduler" class="active">Scheduler</a>
    <a href="/settings">Settings</a>
  </nav>
</div>

<div class="container">
  <h1>Task Scheduler</h1>
  <p class="subtitle">Automate recurring scans, syncs, and diagnostics on a timed schedule.</p>

  <div class="stat-row">
    <div class="stat"><label>Total Jobs</label><div class="val" id="st-total">0</div></div>
    <div class="stat"><label>Active</label><div class="val" id="st-active">0</div></div>
    <div class="stat"><label>Completed Runs</label><div class="val" id="st-runs">0</div></div>
    <div class="stat"><label>Log Entries</label><div class="val" id="st-logs">0</div></div>
  </div>

  <div class="section">
    <h3>Add Scheduled Job</h3>
    <div class="field-row">
      <div class="field-label">Job Type</div>
      <div class="field-input">
        <select id="add-type">
          <option value="0">NTP Sync</option>
          <option value="1">Config Flush</option>
          <option value="2">Frequency Scan</option>
          <option value="3">BLE Scan</option>
          <option value="4">Channel Hop Reset</option>
          <option value="5">Report Generate</option>
          <option value="6">AP Database Prune</option>
          <option value="7">Event Log Rotate</option>
          <option value="8">Watchdog Check</option>
          <option value="9">PCAP Ring Clear</option>
          <option value="10">Probe Fuzzer Run</option>
          <option value="11">Custom (no-op)</option>
        </select>
      </div>
    </div>
    <div class="field-row">
      <div class="field-label">Trigger Type</div>
      <div class="field-input">
        <select id="add-trigger" onchange="onTriggerChange()">
          <option value="0">Once (after delay)</option>
          <option value="1" selected>Interval (every N seconds)</option>
          <option value="2">Hourly (at minute X)</option>
          <option value="3">Daily (at HH:MM)</option>
        </select>
      </div>
    </div>
    <div id="trigger-params">
      <div class="field-row">
        <div class="field-label">Interval (seconds)<small>How often to run</small></div>
        <div class="field-input"><input type="number" id="add-interval" value="300" min="10" max="86400"></div>
      </div>
    </div>
    <div class="action-row">
      <button class="btn btn-primary" onclick="addJob()">+ Add Job</button>
    </div>
  </div>

  <div class="section">
    <h3>Scheduled Jobs</h3>
    <div style="overflow-x:auto;">
      <table>
        <thead><tr>
          <th>ID</th><th>Type</th><th>Trigger</th><th>Next Run</th><th>Runs</th><th>Status</th><th>Actions</th>
        </tr></thead>
        <tbody id="job-tbody"><tr><td colspan="7" style="color:var(--muted);text-align:center">Loading…</td></tr></tbody>
      </table>
    </div>
  </div>

  <div class="section">
    <h3>Execution Log</h3>
    <div class="log-feed" id="log-feed">
      <div style="color:var(--muted)">No execution history.</div>
    </div>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="loadJobs()">Refresh</button>
    </div>
  </div>
</div>

<script>
const JOB_TYPES = ['NTP Sync','Config Flush','Freq Scan','BLE Scan','Ch Hop Reset','Report Gen','AP Prune','Log Rotate','WDG Check','PCAP Clear','Probe Fuzz','Custom'];
const TRIGGERS  = ['Once','Interval','Hourly','Daily'];

function onTriggerChange() {
  const t = parseInt(document.getElementById('add-trigger').value);
  let html = '';
  if (t === 0) {
    html = `<div class="field-row"><div class="field-label">Delay (sec)</div>
      <div class="field-input"><input type="number" id="add-interval" value="60" min="1"></div></div>`;
  } else if (t === 1) {
    html = `<div class="field-row"><div class="field-label">Interval (sec)</div>
      <div class="field-input"><input type="number" id="add-interval" value="300" min="10"></div></div>`;
  } else if (t === 2) {
    html = `<div class="field-row"><div class="field-label">Minute (0–59)</div>
      <div class="field-input"><input type="number" id="add-interval" value="0" min="0" max="59"></div></div>`;
  } else if (t === 3) {
    html = `<div class="field-row"><div class="field-label">Time (HH:MM)</div>
      <div class="field-input"><input type="text" id="add-interval" value="02:00" placeholder="HH:MM"></div></div>`;
  }
  document.getElementById('trigger-params').innerHTML = html;
}

async function addJob() {
  const type    = document.getElementById('add-type').value;
  const trigger = document.getElementById('add-trigger').value;
  const param   = document.getElementById('add-interval')?.value || '300';

  const params = new URLSearchParams({type, trigger, param});
  const r = await fetch('/api/sched/add', {method:'POST', body:params,
    headers:{'Content-Type':'application/x-www-form-urlencoded'}}).then(x=>x.json()).catch(()=>({}));

  if (r.ok) loadJobs();
  else alert('Failed to add job: ' + (r.error||'unknown'));
}

async function toggleJob(id, enable) {
  const ep = enable ? '/api/sched/enable' : '/api/sched/disable';
  const r = await fetch(ep, {method:'POST', body:'id='+id,
    headers:{'Content-Type':'application/x-www-form-urlencoded'}}).then(x=>x.json());
  if (r.ok) loadJobs();
}

async function removeJob(id) {
  if (!confirm('Remove job #'+id+'?')) return;
  const r = await fetch('/api/sched/remove', {method:'POST', body:'id='+id,
    headers:{'Content-Type':'application/x-www-form-urlencoded'}}).then(x=>x.json());
  if (r.ok) loadJobs();
}

function fmtSecs(s) {
  if (!s) return '—';
  if (s < 60) return s+'s';
  if (s < 3600) return Math.floor(s/60)+'m';
  return Math.floor(s/3600)+'h';
}

async function loadJobs() {
  const r = await fetch('/api/sched/all').then(x=>x.json()).catch(()=>({}));
  if (!r.jobs) return;

  document.getElementById('st-total').textContent  = r.total_jobs||0;
  document.getElementById('st-active').textContent = r.active_jobs||0;
  document.getElementById('st-runs').textContent   = r.total_runs||0;
  document.getElementById('st-logs').textContent   = r.log_count||0;

  const tbody = document.getElementById('job-tbody');
  if (!r.jobs.length) {
    tbody.innerHTML = '<tr><td colspan="7" style="color:var(--muted);text-align:center">No jobs configured.</td></tr>';
  } else {
    tbody.innerHTML = r.jobs.map(j => {
      const type    = JOB_TYPES[j.type]  || 'type#'+j.type;
      const trigger = TRIGGERS[j.trigger]|| 'trig#'+j.trigger;
      const badge   = j.enabled
        ? '<span class="badge badge-active">enabled</span>'
        : '<span class="badge badge-disabled">disabled</span>';
      const nextRun = j.next_run_in > 0 ? 'in '+fmtSecs(j.next_run_in) : '—';
      return `<tr>
        <td>${j.id}</td>
        <td>${type}</td>
        <td>${trigger}<br><small style="color:var(--muted)">${j.param||''}</small></td>
        <td>${nextRun}</td>
        <td>${j.run_count}</td>
        <td>${badge}</td>
        <td style="white-space:nowrap">
          <button class="btn btn-ghost btn-sm" onclick="toggleJob(${j.id},${!j.enabled})">
            ${j.enabled?'Disable':'Enable'}
          </button>
          <button class="btn btn-danger btn-sm" style="margin-left:4px" onclick="removeJob(${j.id})">Del</button>
        </td>
      </tr>`;
    }).join('');
  }

  // Load log
  if (r.log && r.log.length) {
    const logLines = r.log.slice(-40).reverse().map(e=>{
      const ts = new Date(e.ts * 1000).toLocaleTimeString();
      const col = e.ok ? '#22c55e' : '#ef4444';
      const type = JOB_TYPES[e.type]||e.type;
      return `<div class="log-line" style="color:${col}">[${ts}] Job #${e.job_id} (${type}): ${e.ok?'OK':'FAIL'}</div>`;
    }).join('');
    document.getElementById('log-feed').innerHTML = logLines;
  }
}

onTriggerChange();
loadJobs();
setInterval(loadJobs, 5000);
</script>
</body>
</html>
)rawhtml";

inline void wusch_routes() {
  server.on("/scheduler", HTTP_GET, []() {
    server.send_P(200, "text/html", WUSCH_PAGE);
  });
}

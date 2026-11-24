#pragma once
// webui_capture_live.h — Live PCAP capture control UI.
// Route: /capture-live. Prefix: wupcl_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUPCL_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Live Capture — Phantom32</title>
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
.hero{display:grid;grid-template-columns:repeat(4,1fr);gap:10px;margin-bottom:16px;}
.stat{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:13px;}
.stat label{font-size:10px;color:var(--muted);text-transform:uppercase;}
.stat .val{font-size:22px;font-weight:800;color:var(--accent);margin-top:3px;}
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:18px;margin-bottom:14px;}
.section h3{font-size:12px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:14px;}
.status-row{display:flex;align-items:center;gap:14px;padding:12px;
  background:var(--bg3);border-radius:7px;margin-bottom:14px;}
.status-dot{width:12px;height:12px;border-radius:50%;}
.status-dot.on{background:var(--success);box-shadow:0 0 8px var(--success);}
.status-dot.off{background:var(--muted);}
.status-label{font-weight:600;}
.btn{padding:8px 16px;border:none;border-radius:5px;font-size:12px;font-weight:600;cursor:pointer;}
.btn-primary{background:var(--accent);color:#0a0c10;}
.btn-primary:hover{background:#00b8e0;}
.btn-stop{background:var(--danger);color:#fff;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.btn-row{display:flex;gap:10px;flex-wrap:wrap;}
table{width:100%;border-collapse:collapse;font-size:12px;}
th{text-align:left;padding:7px 8px;font-size:10px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;text-transform:uppercase;}
td{padding:7px 8px;border-bottom:1px solid var(--border);}
tr:last-child td{border-bottom:none;}
tr:hover td{background:var(--bg3);}
.note{font-size:11px;color:var(--muted);margin-top:8px;}
@media(max-width:700px){.hero{grid-template-columns:repeat(2,1fr)}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/capture">Capture</a>
    <a href="/capture-live" class="active">Live PCAP</a>
    <a href="/forensics">Forensics</a>
    <a href="/analytics">Analytics</a>
  </nav>
</div>
<div class="container">
  <h1>Live PCAP Capture</h1>
  <p class="subtitle">Write raw 802.11 frames with radiotap headers to LittleFS. Download for Wireshark.</p>

  <div class="hero">
    <div class="stat"><label>Captured</label><div class="val" id="h-pkts">0</div></div>
    <div class="stat"><label>Dropped</label><div class="val" id="h-drop" style="color:var(--warn)">0</div></div>
    <div class="stat"><label>File Size</label><div class="val" id="h-size">0</div></div>
    <div class="stat"><label>Rotations</label><div class="val" id="h-rots">0</div></div>
  </div>

  <div class="section">
    <h3>Capture Control</h3>
    <div class="status-row">
      <div class="status-dot" id="status-dot"></div>
      <span class="status-label" id="status-label">—</span>
      <span style="color:var(--muted);font-size:12px" id="status-file"></span>
    </div>
    <div class="btn-row">
      <button class="btn btn-primary" id="btn-start" onclick="startCapture()">Start Capture</button>
      <button class="btn btn-stop" id="btn-stop" onclick="stopCapture()" disabled>Stop</button>
      <a id="btn-dl" class="btn btn-ghost" href="/api/pcl/download" download="capture.pcap">Download Latest</a>
    </div>
    <p class="note">Files rotate at 256 KB. Up to 5 files stored (oldest overwritten). Max ~1.25 MB total.</p>
  </div>

  <div class="section">
    <h3>Capture Files on Flash</h3>
    <table>
      <thead><tr><th>File</th><th>Size</th><th>Download</th></tr></thead>
      <tbody id="files-tbody">
        <tr><td colspan="3" style="color:var(--muted);text-align:center">Loading…</td></tr>
      </tbody>
    </table>
  </div>
</div>

<script>
function fmt_bytes(b) {
  if(b>1024) return (b/1024).toFixed(1)+' KB';
  return b+' B';
}

async function startCapture() {
  document.getElementById('btn-start').disabled = true;
  await fetch('/api/pcl/start',{method:'POST'});
  refresh();
}

async function stopCapture() {
  await fetch('/api/pcl/stop',{method:'POST'});
  refresh();
}

async function refresh() {
  const st = await fetch('/api/pcl/status').then(x=>x.json()).catch(()=>({}));
  const files = await fetch('/api/pcl/list').then(x=>x.json()).catch(()=>[]);

  document.getElementById('h-pkts').textContent = st.pkt_captured||0;
  document.getElementById('h-drop').textContent = st.pkt_dropped||0;
  document.getElementById('h-size').textContent = fmt_bytes(st.bytes_written||0);
  document.getElementById('h-rots').textContent = st.rotations||0;

  const on = st.capturing;
  const dot   = document.getElementById('status-dot');
  dot.className = 'status-dot ' + (on?'on':'off');
  document.getElementById('status-label').textContent = on ? 'Capturing…' : 'Stopped';
  document.getElementById('status-file').textContent  = st.cur_file||'';
  document.getElementById('btn-start').disabled = !!on;
  document.getElementById('btn-stop').disabled  = !on;

  document.getElementById('files-tbody').innerHTML = files.length
    ? files.map(f=>`<tr>
        <td><code style="font-size:11px">${f.file}</code></td>
        <td>${fmt_bytes(f.size)}</td>
        <td><a href="/api/pcl/download?file=${encodeURIComponent(f.file)}"
              class="btn btn-ghost" style="padding:3px 10px;font-size:11px" download>⬇ Download</a></td>
      </tr>`).join('')
    : '<tr><td colspan="3" style="color:var(--muted);text-align:center">No capture files.</td></tr>';
}

refresh();
setInterval(refresh, 2000);
</script>
</body>
</html>
)rawhtml";

inline void wupcl_routes() {
  server.on("/capture-live", HTTP_GET, []() {
    server.send_P(200, "text/html", WUPCL_PAGE);
  });
}

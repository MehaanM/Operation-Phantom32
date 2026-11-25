#pragma once
// webui_cli.h — Full terminal emulator web UI for Phantom32 CLI.
// VT100-style output, command history, autocomplete, macros, scripting.
// Prefix: wucli_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

static const char WUCLI_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Phantom32 · CLI Terminal</title>
<style>
:root{--bg:#0d0d0d;--surface:#111;--surface2:#1a1a1a;--border:#2a2a2a;--accent:#00ff88;--accent2:#00aaff;--accent3:#ffaa00;--accent4:#ff4444;--text:#e0e0e0;--muted:#666;--prompt:#00ff88;}
*{margin:0;padding:0;box-sizing:border-box;}
body{background:var(--bg);color:var(--text);font-family:'Consolas','Courier New',monospace;font-size:13px;height:100vh;display:flex;flex-direction:column;overflow:hidden;}
header{background:#111;border-bottom:1px solid #222;padding:8px 14px;display:flex;align-items:center;gap:10px;flex-shrink:0;}
header h1{font-size:14px;font-weight:700;color:var(--accent);letter-spacing:1px;}
.badge{background:#1a1a1a;border:1px solid #333;color:var(--accent);padding:2px 8px;border-radius:4px;font-size:11px;}
.hdr-links{margin-left:auto;display:flex;gap:6px;}
.hdr-links a{color:var(--muted);text-decoration:none;padding:4px 8px;border-radius:4px;font-size:11px;}
.hdr-links a:hover{color:var(--accent);}
.sidebar{position:fixed;right:0;top:38px;bottom:0;width:220px;background:#111;border-left:1px solid #222;padding:10px;overflow-y:auto;transform:translateX(100%);transition:transform .3s;z-index:50;}
.sidebar.open{transform:none;}
.sidebar h3{color:var(--accent);font-size:12px;margin-bottom:8px;text-transform:uppercase;letter-spacing:.5px;}
.sidebar .cmd-item{padding:4px 6px;border-radius:4px;cursor:pointer;font-size:11px;color:var(--muted);transition:all .15s;}
.sidebar .cmd-item:hover{background:#222;color:var(--text);}
.sidebar .cmd-item .cmd-name{color:var(--accent2);font-weight:600;}
.sidebar .cmd-item .cmd-desc{font-size:10px;margin-top:1px;}
.macro-list{margin-top:12px;}
.macro-item{display:flex;gap:6px;align-items:center;padding:3px 0;}
.macro-name{color:var(--accent3);font-size:11px;width:60px;}
.macro-cmd{color:var(--muted);font-size:11px;flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;}
.sidebar-btn{background:transparent;border:none;cursor:pointer;color:var(--accent);font-size:16px;padding:4px;}
.terminal-wrap{flex:1;display:flex;flex-direction:column;overflow:hidden;padding:0;}
.terminal-out{flex:1;overflow-y:auto;padding:12px 14px;line-height:1.5;}
.terminal-out::-webkit-scrollbar{width:6px;}
.terminal-out::-webkit-scrollbar-track{background:#0d0d0d;}
.terminal-out::-webkit-scrollbar-thumb{background:#333;border-radius:3px;}
.line{white-space:pre-wrap;word-break:break-all;}
.line.cmd-echo{color:var(--accent);}
.line.error{color:var(--accent4);}
.line.warn{color:var(--accent3);}
.line.info{color:var(--accent2);}
.line.success{color:#00ff88;}
.line.muted{color:var(--muted);}
.line.header-line{color:var(--accent3);font-weight:700;}
.line.sep{color:#333;}
.prompt-row{display:flex;align-items:center;padding:6px 14px;border-top:1px solid #1a1a1a;flex-shrink:0;background:#0d0d0d;}
.prompt-str{color:var(--prompt);flex-shrink:0;white-space:nowrap;user-select:none;}
.prompt-input{flex:1;background:transparent;border:none;outline:none;color:var(--text);font-family:inherit;font-size:13px;caret-color:var(--accent);padding:0 8px;}
.autocomplete-box{position:absolute;bottom:42px;left:0;right:0;background:#1a1a1a;border:1px solid #333;border-radius:4px;overflow:hidden;max-height:160px;overflow-y:auto;z-index:20;}
.ac-item{padding:4px 12px;cursor:pointer;color:var(--muted);font-size:12px;display:flex;gap:10px;}
.ac-item:hover,.ac-item.active{background:#2a2a2a;color:var(--text);}
.ac-item .ac-name{color:var(--accent2);min-width:80px;}
.ac-item .ac-desc{color:var(--muted);font-size:11px;}
.toolbar-row{display:flex;gap:6px;padding:4px 14px;border-top:1px solid #1a1a1a;flex-shrink:0;background:#0d0d0d;flex-wrap:wrap;}
.tbtn{background:#1a1a1a;border:1px solid #2a2a2a;color:var(--muted);padding:3px 8px;border-radius:4px;cursor:pointer;font-size:11px;font-family:monospace;transition:all .15s;}
.tbtn:hover{background:#2a2a2a;color:var(--text);}
.tbtn.active{background:var(--accent);color:#000;}
.script-modal{display:none;position:fixed;top:0;left:0;right:0;bottom:0;background:#0009;z-index:100;align-items:center;justify-content:center;}
.script-modal.show{display:flex;}
.script-box{background:#111;border:1px solid #333;border-radius:8px;width:min(500px,90vw);padding:16px;}
.script-box h2{color:var(--accent);font-size:14px;margin-bottom:10px;}
textarea.script-ta{width:100%;background:#0d0d0d;border:1px solid #333;color:var(--text);padding:8px;border-radius:4px;font-family:monospace;font-size:12px;resize:vertical;min-height:120px;outline:none;}
textarea.script-ta:focus{border-color:var(--accent2);}
.script-actions{display:flex;gap:8px;margin-top:10px;justify-content:flex-end;}
.sbtn{background:#1a1a1a;border:1px solid #333;color:var(--text);padding:5px 12px;border-radius:4px;cursor:pointer;font-size:12px;}
.sbtn.primary{background:var(--accent2);border-color:var(--accent2);color:#000;}
.sbtn:hover{opacity:.85;}
.cursor-blink{display:inline-block;width:8px;height:14px;background:var(--accent);animation:blink 1s step-end infinite;vertical-align:middle;}
@keyframes blink{0%,100%{opacity:1;}50%{opacity:0;}}
.status-bar{display:flex;gap:16px;padding:2px 14px;background:#0a0a0a;border-top:1px solid #1a1a1a;font-size:10px;color:var(--muted);flex-shrink:0;}
.status-bar span{color:var(--text);}
</style>
</head>
<body>
<header>
  <h1>PHANTOM32 // TERMINAL</h1>
  <span class="badge">CLI v2.0</span>
  <div class="hdr-links">
    <a href="/">Home</a>
    <a href="/analytics">Analytics</a>
    <a href="/forensics">Forensics</a>
    <a href="/ble-iot">BLE/IoT</a>
    <button class="sidebar-btn" onclick="toggleSidebar()" title="Commands">⚙</button>
  </div>
</header>

<div class="sidebar" id="sidebar">
  <h3>Commands</h3>
  <div id="cmdList"></div>
  <div class="macro-list">
    <h3 style="margin-top:12px;">Macros</h3>
    <div id="macroList"></div>
    <button class="tbtn" style="margin-top:8px;width:100%;" onclick="openScriptModal()">+ New Script</button>
  </div>
</div>

<div class="terminal-wrap">
  <div class="terminal-out" id="termOut" onclick="focusInput()"></div>
  <div style="position:relative;">
    <div class="autocomplete-box" id="acBox" style="display:none;"></div>
    <div class="toolbar-row">
      <button class="tbtn" onclick="quickCmd('help')">help</button>
      <button class="tbtn" onclick="quickCmd('scan aps')">scan aps</button>
      <button class="tbtn" onclick="quickCmd('scan stas')">scan stas</button>
      <button class="tbtn" onclick="quickCmd('scan ble')">scan ble</button>
      <button class="tbtn" onclick="quickCmd('status')">status</button>
      <button class="tbtn" onclick="quickCmd('channels')">channels</button>
      <button class="tbtn" onclick="quickCmd('vulns')">vulns</button>
      <button class="tbtn" onclick="quickCmd('threats')">threats</button>
      <button class="tbtn" onclick="quickCmd('report')">report</button>
      <button class="tbtn" onclick="quickCmd('pcap start')">pcap start</button>
      <button class="tbtn" onclick="quickCmd('pcap stop')">pcap stop</button>
      <button class="tbtn" onclick="clearTerm()">clear</button>
      <button class="tbtn" id="scriptBtn" onclick="openScriptModal()">script</button>
    </div>
    <div class="prompt-row">
      <span class="prompt-str">phantom32@esp32:~$ </span>
      <input class="prompt-input" id="cmdInput" type="text" autocomplete="off" spellcheck="false"
        placeholder="enter command..." onkeydown="onKey(event)" oninput="onInput(event)">
    </div>
  </div>
</div>

<div class="status-bar">
  <div>AP: <span id="sb-ap">—</span></div>
  <div>STA: <span id="sb-sta">—</span></div>
  <div>CH: <span id="sb-ch">—</span></div>
  <div>Pkts: <span id="sb-pkt">—</span></div>
  <div>Uptime: <span id="sb-up">—</span></div>
  <div>Free heap: <span id="sb-heap">—</span></div>
  <div id="sb-time" style="margin-left:auto;"></div>
</div>

<div class="script-modal" id="scriptModal">
  <div class="script-box">
    <h2>Script Editor</h2>
    <div style="font-size:11px;color:var(--muted);margin-bottom:6px;">One command per line. Lines starting with # are comments.</div>
    <textarea class="script-ta" id="scriptTA" placeholder="# Example script&#10;status&#10;scan aps&#10;wait 2000&#10;vulns&#10;report"></textarea>
    <div class="script-actions">
      <button class="sbtn" onclick="closeScriptModal()">Cancel</button>
      <button class="sbtn" onclick="saveScript()">Save Macro</button>
      <button class="sbtn primary" onclick="runScript()">Run Script</button>
    </div>
  </div>
</div>

<script>
// ─── State ───────────────────────────────────────────────────────────────────
let history = [];
let histIdx = -1;
let acSuggestions = [];
let acIdx = -1;
let scriptRunning = false;
let macros = JSON.parse(localStorage.getItem('ph32_macros') || '{}');

// ─── Known commands (populated from /api/cli/commands) ───────────────────────
let COMMANDS = [
  { name: 'help', desc: 'Show help for all commands', usage: 'help [cmd]' },
  { name: 'status', desc: 'Show system status', usage: 'status' },
  { name: 'scan', desc: 'Scan APs/stations/BLE', usage: 'scan aps|stas|ble|all' },
  { name: 'channels', desc: 'List channel statistics', usage: 'channels' },
  { name: 'ch', desc: 'Set/get active channel', usage: 'ch [1-14]' },
  { name: 'ap', desc: 'List/detail APs', usage: 'ap list|info <mac>' },
  { name: 'sta', desc: 'List/detail stations', usage: 'sta list|info <mac>' },
  { name: 'oui', desc: 'OUI vendor lookup', usage: 'oui <mac-prefix>' },
  { name: 'vuln', desc: 'List vulnerability scores', usage: 'vuln list|detail <id>' },
  { name: 'vulns', desc: 'Alias for vuln list', usage: 'vulns' },
  { name: 'threat', desc: 'Threat intelligence', usage: 'threat list|clear' },
  { name: 'threats', desc: 'Alias for threat list', usage: 'threats' },
  { name: 'fingerprint', desc: 'Device fingerprints', usage: 'fingerprint list' },
  { name: 'eap', desc: 'EAP/PMKID capture', usage: 'eap list|export' },
  { name: 'pcap', desc: 'PCAP capture control', usage: 'pcap start|stop|status|dl' },
  { name: 'report', desc: 'Generate security report', usage: 'report [txt|json]' },
  { name: 'karma', desc: 'KARMA attack control', usage: 'karma start|stop|status' },
  { name: 'evil', desc: 'Evil twin control', usage: 'evil start <ssid>|stop|creds' },
  { name: 'replay', desc: 'Packet replay', usage: 'replay start|stop|load <file>' },
  { name: 'iot', desc: 'IoT fingerprint list', usage: 'iot list|detail <mac>' },
  { name: 'ble', desc: 'BLE scan control', usage: 'ble start|stop|list' },
  { name: 'wps', desc: 'WPS scan results', usage: 'wps list|detail <mac>' },
  { name: 'ddt', desc: 'Deauth detector status', usage: 'ddt status|alerts' },
  { name: 'set', desc: 'Set runtime config', usage: 'set <key> <value>' },
  { name: 'get', desc: 'Get runtime config', usage: 'get <key>' },
  { name: 'ls', desc: 'List filesystem files', usage: 'ls [path]' },
  { name: 'cat', desc: 'Show file contents', usage: 'cat <path>' },
  { name: 'rm', desc: 'Remove file', usage: 'rm <path>' },
  { name: 'df', desc: 'Filesystem usage', usage: 'df' },
  { name: 'reboot', desc: 'Reboot device', usage: 'reboot' },
  { name: 'uptime', desc: 'Show uptime and heap', usage: 'uptime' },
  { name: 'wait', desc: 'Wait milliseconds', usage: 'wait <ms>' },
  { name: 'echo', desc: 'Print text', usage: 'echo <text>' },
  { name: 'clear', desc: 'Clear terminal', usage: 'clear' },
  { name: 'history', desc: 'Show command history', usage: 'history' },
];

// ─── Terminal output ─────────────────────────────────────────────────────────
const out = document.getElementById('termOut');

function writeLine(text, cls = '') {
  const d = document.createElement('div');
  d.className = 'line ' + cls;
  d.textContent = text;
  out.appendChild(d);
  out.scrollTop = out.scrollHeight;
}

function writeLines(text, cls = '') {
  text.split('\n').forEach(l => writeLine(l, cls));
}

function writeHTML(html) {
  const d = document.createElement('div');
  d.className = 'line';
  d.innerHTML = html;
  out.appendChild(d);
  out.scrollTop = out.scrollHeight;
}

function clearTerm() {
  out.innerHTML = '';
  writeLine('Terminal cleared.', 'muted');
}

function focusInput() {
  document.getElementById('cmdInput').focus();
}

// ─── Boot banner ─────────────────────────────────────────────────────────────
function printBanner() {
  const lines = [
    ' ____  _           _                   _____ ____',
    '|  _ \\| |__   __ _| |_ ___  _ __ _____|___ /|___ \\',
    '| |_) | \'_ \\ / _` | __/ _ \\| \'_ \\_____| |_ \\ __) |',
    '|  __/| | | | (_| | || (_) | | | |   |___) / __/',
    '|_|   |_| |_|\\__,_|\\__\\___/|_| |_|  |____/|_____|',
    '',
    '  ESP32 WiFi Security Toolkit  •  CLI Terminal v2.0',
    '  Type "help" for commands  •  Tab for autocomplete',
    '━'.repeat(54),
    '',
  ];
  lines.forEach(l => writeLine(l, l.startsWith(' ') || l.startsWith('|') ? 'header-line' : l === '' ? 'muted' : 'sep'));
}

// ─── Command execution ───────────────────────────────────────────────────────
async function runCmd(cmd) {
  cmd = cmd.trim();
  if (!cmd) return;

  history.unshift(cmd);
  if (history.length > 100) history.pop();
  histIdx = -1;
  localStorage.setItem('ph32_history', JSON.stringify(history));

  writeLine('phantom32@esp32:~$ ' + cmd, 'cmd-echo');

  if (cmd === 'clear') { clearTerm(); return; }
  if (cmd === 'history') {
    history.forEach((h, i) => writeLine(`  ${(history.length - i).toString().padStart(3)} ${h}`, 'muted'));
    return;
  }

  try {
    const r = await fetch('/api/cli/run?cmd=' + encodeURIComponent(cmd));
    const data = await r.json();
    if (data.output) {
      const lines = data.output.split('\n');
      lines.forEach(l => {
        let cls = '';
        if (l.startsWith('ERROR') || l.startsWith('error') || l.startsWith('[ERROR]')) cls = 'error';
        else if (l.startsWith('WARN') || l.startsWith('[WARN]')) cls = 'warn';
        else if (l.startsWith('INFO') || l.startsWith('[INFO]')) cls = 'info';
        else if (l.startsWith('OK') || l.startsWith('[OK]') || l.startsWith('SUCCESS')) cls = 'success';
        else if (l.startsWith('─') || l.startsWith('═') || l.startsWith('━')) cls = 'sep';
        writeLine(l, cls);
      });
    }
    if (data.error) writeLine('Error: ' + data.error, 'error');
  } catch (e) {
    writeLine('Connection error: ' + e.message, 'error');
  }
}

function quickCmd(cmd) {
  document.getElementById('cmdInput').value = cmd;
  focusInput();
  runCmd(cmd);
  document.getElementById('cmdInput').value = '';
}

// ─── Autocomplete ─────────────────────────────────────────────────────────────
function onInput(e) {
  const val = document.getElementById('cmdInput').value;
  updateAutocomplete(val);
}

function updateAutocomplete(val) {
  const box = document.getElementById('acBox');
  if (!val || val.includes(' ')) { box.style.display = 'none'; return; }
  const matches = COMMANDS.filter(c => c.name.startsWith(val.toLowerCase()));
  if (!matches.length) { box.style.display = 'none'; return; }
  acSuggestions = matches;
  acIdx = -1;
  box.innerHTML = matches.slice(0, 8).map((c, i) =>
    `<div class="ac-item" id="ac-${i}" onclick="applyAc(${i})">
      <span class="ac-name">${c.name}</span>
      <span class="ac-desc">${c.desc}</span>
    </div>`
  ).join('');
  box.style.display = 'block';
}

function applyAc(idx) {
  const c = acSuggestions[idx];
  if (c) document.getElementById('cmdInput').value = c.name + ' ';
  document.getElementById('acBox').style.display = 'none';
  focusInput();
}

// ─── Key handling ─────────────────────────────────────────────────────────────
function onKey(e) {
  const inp = document.getElementById('cmdInput');
  const box = document.getElementById('acBox');

  if (e.key === 'Enter') {
    box.style.display = 'none';
    const cmd = inp.value;
    inp.value = '';
    runCmd(cmd);
    return;
  }

  if (e.key === 'Tab') {
    e.preventDefault();
    if (acSuggestions.length === 1) {
      applyAc(0);
    } else if (acSuggestions.length > 1) {
      acIdx = (acIdx + 1) % acSuggestions.length;
      document.querySelectorAll('.ac-item').forEach((el, i) => el.className = 'ac-item' + (i === acIdx ? ' active' : ''));
      if (acIdx >= 0) inp.value = acSuggestions[acIdx].name + ' ';
    }
    return;
  }

  if (e.key === 'ArrowUp') {
    e.preventDefault();
    if (box.style.display !== 'none') {
      acIdx = Math.max(0, acIdx - 1);
      document.querySelectorAll('.ac-item').forEach((el, i) => el.className = 'ac-item' + (i === acIdx ? ' active' : ''));
      if (acSuggestions[acIdx]) inp.value = acSuggestions[acIdx].name + ' ';
    } else {
      histIdx = Math.min(history.length - 1, histIdx + 1);
      inp.value = history[histIdx] || '';
    }
    return;
  }

  if (e.key === 'ArrowDown') {
    e.preventDefault();
    if (box.style.display !== 'none') {
      acIdx = Math.min(acSuggestions.length - 1, acIdx + 1);
      document.querySelectorAll('.ac-item').forEach((el, i) => el.className = 'ac-item' + (i === acIdx ? ' active' : ''));
      if (acSuggestions[acIdx]) inp.value = acSuggestions[acIdx].name + ' ';
    } else {
      histIdx = Math.max(-1, histIdx - 1);
      inp.value = histIdx >= 0 ? history[histIdx] : '';
    }
    return;
  }

  if (e.key === 'Escape') {
    box.style.display = 'none';
    inp.value = '';
    return;
  }

  if (e.key === 'l' && e.ctrlKey) { e.preventDefault(); clearTerm(); return; }
  if (e.key === 'c' && e.ctrlKey) { e.preventDefault(); inp.value = ''; writeLine('^C', 'muted'); return; }
}

// ─── Sidebar ─────────────────────────────────────────────────────────────────
function toggleSidebar() {
  document.getElementById('sidebar').classList.toggle('open');
}

function populateSidebar() {
  document.getElementById('cmdList').innerHTML = COMMANDS.map(c =>
    `<div class="cmd-item" onclick="quickCmd('${c.name}')">
      <div class="cmd-name">${c.name}</div>
      <div class="cmd-desc">${c.desc}</div>
    </div>`
  ).join('');
  renderMacroList();
}

function renderMacroList() {
  const list = document.getElementById('macroList');
  const keys = Object.keys(macros);
  if (!keys.length) { list.innerHTML = '<div style="color:var(--muted);font-size:11px;">No macros saved</div>'; return; }
  list.innerHTML = keys.map(k =>
    `<div class="macro-item">
      <span class="macro-name">${k}</span>
      <span class="macro-cmd">${macros[k].split('\n')[0]}</span>
      <span style="cursor:pointer;color:var(--accent2);font-size:12px;" onclick="runMacro('${k}')">▶</span>
      <span style="cursor:pointer;color:var(--accent4);font-size:12px;" onclick="deleteMacro('${k}')">✕</span>
    </div>`
  ).join('');
}

// ─── Script editor ───────────────────────────────────────────────────────────
function openScriptModal() { document.getElementById('scriptModal').className = 'script-modal show'; }
function closeScriptModal() { document.getElementById('scriptModal').className = 'script-modal'; }

async function runScript() {
  const lines = document.getElementById('scriptTA').value.split('\n').map(l => l.trim()).filter(l => l && !l.startsWith('#'));
  closeScriptModal();
  writeLine('Running script (' + lines.length + ' commands)...', 'info');
  for (const line of lines) {
    if (line.startsWith('wait ')) {
      const ms = parseInt(line.slice(5)) || 1000;
      writeLine('Waiting ' + ms + 'ms...', 'muted');
      await new Promise(r => setTimeout(r, ms));
    } else {
      await runCmd(line);
    }
  }
  writeLine('Script complete.', 'success');
}

function saveScript() {
  const name = prompt('Macro name:');
  if (!name) return;
  macros[name] = document.getElementById('scriptTA').value;
  localStorage.setItem('ph32_macros', JSON.stringify(macros));
  renderMacroList();
  closeScriptModal();
}

function runMacro(name) {
  document.getElementById('scriptTA').value = macros[name] || '';
  runScript();
}

function deleteMacro(name) {
  delete macros[name];
  localStorage.setItem('ph32_macros', JSON.stringify(macros));
  renderMacroList();
}

// ─── Status bar ──────────────────────────────────────────────────────────────
async function updateStatusBar() {
  try {
    const r = await fetch('/api/cli/stats').then(x => x.json()).catch(() => ({}));
    document.getElementById('sb-ap').textContent = r.ap_count || '—';
    document.getElementById('sb-sta').textContent = r.sta_count || '—';
    document.getElementById('sb-ch').textContent = r.channel || '—';
    document.getElementById('sb-pkt').textContent = r.pkt_total || '—';
    document.getElementById('sb-heap').textContent = r.free_heap ? Math.round(r.free_heap / 1024) + 'K' : '—';
    const up = r.uptime_s || 0;
    const h = Math.floor(up / 3600), m = Math.floor((up % 3600) / 60), s = up % 60;
    document.getElementById('sb-up').textContent = `${h}h${m}m${s}s`;
  } catch (e) {}
  document.getElementById('sb-time').textContent = new Date().toLocaleTimeString();
}

// ─── Load saved history ───────────────────────────────────────────────────────
function loadHistory() {
  try { history = JSON.parse(localStorage.getItem('ph32_history') || '[]'); } catch (e) {}
}

// ─── Init ────────────────────────────────────────────────────────────────────
loadHistory();
printBanner();
writeLine('Ready. Type "help" for available commands.', 'info');
populateSidebar();
setInterval(updateStatusBar, 3000);
updateStatusBar();
document.getElementById('cmdInput').focus();

// Try to load commands from API
fetch('/api/cli/commands').then(r => r.json()).then(data => {
  if (data.commands && data.commands.length) {
    COMMANDS = data.commands;
    populateSidebar();
  }
}).catch(() => {});
</script>
</body>
</html>
)rawhtml";

inline void wucli_routes() {
  server.on("/cli", HTTP_GET, []() { server.send_P(200, "text/html", WUCLI_PAGE); });
}

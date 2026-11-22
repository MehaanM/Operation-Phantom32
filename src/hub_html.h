#pragma once
static const char HUB_HTML[] = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Mission: Phantom32_Protocol</title>
<style>
:root{--bg:#0b0d12;--bg2:#111318;--bg3:#181b22;--fg:#dde3ec;--dim:#687080;--acc:#e8233a;--ok:#00e676;--blu:#3f9eff;--orn:#ff9500;--bdr:#1e2430;--red2:#2a0a0e;--grn2:#0a1f12}
*{box-sizing:border-box;margin:0;padding:0}
html,body{height:100%;overflow:hidden;font-family:system-ui,-apple-system,sans-serif;background:var(--bg);color:var(--fg);font-size:14px}
/* ── Header ── */
#hdr{background:var(--bg2);border-bottom:2px solid var(--acc);padding:8px 16px;display:flex;align-items:center;gap:12px;flex-shrink:0;position:relative;z-index:50}
#hdr-logo{font-weight:800;font-size:1.1em;color:var(--acc);letter-spacing:3px}
#hdr-logo sup{font-size:.55em;color:var(--dim);letter-spacing:0;vertical-align:super}
#hdr-stats{display:flex;gap:16px;font-size:.72em;color:var(--dim);margin-left:auto;flex-wrap:wrap}
.hstat span{color:var(--fg);font-weight:600;font-family:monospace}
#active-pill{background:var(--red2);border:1px solid var(--acc);color:var(--acc);padding:2px 10px;border-radius:12px;font-size:.68em;font-weight:700;display:none;animation:blink 1.5s infinite}
@keyframes blink{0%,100%{opacity:1}50%{opacity:.5}}
/* ── Nav ── */
#nav{background:var(--bg2);border-bottom:1px solid var(--bdr);display:flex;overflow-x:auto;flex-shrink:0;scrollbar-width:none}
#nav::-webkit-scrollbar{display:none}
.ntab{background:none;border:none;border-bottom:2px solid transparent;color:var(--dim);padding:9px 18px;cursor:pointer;font-size:.8em;font-weight:600;white-space:nowrap;letter-spacing:.4px;transition:all .15s}
.ntab:hover{color:var(--fg);background:var(--bg3)}
.ntab.on{color:var(--acc);border-bottom-color:var(--acc)}
/* ── Pages ── */
#wrap{flex:1;overflow:hidden;display:flex;flex-direction:column}
.page{display:none;flex:1;overflow-y:auto;padding:14px 16px}
.page.on{display:block}
#pg-aegis{padding:0;overflow:hidden;display:none;flex:1}
#pg-aegis.on{display:flex;flex-direction:column}
#iframe-aegis{flex:1;border:0;width:100%}
/* ── Grid ── */
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(300px,1fr));gap:12px}
/* ── Cards ── */
.card{background:var(--bg2);border:1px solid var(--bdr);border-radius:10px;overflow:hidden;transition:border-color .2s}
.card.run{border-color:var(--ok)!important}
.card-hd{padding:11px 14px;display:flex;align-items:center;gap:10px;cursor:pointer;user-select:none;background:var(--bg3);transition:background .15s}
.card-hd:hover{background:#1c2030}
.card-ico{font-size:1.3em;min-width:28px;text-align:center}
.card-info{flex:1;min-width:0}
.card-info h3{font-size:.88em;font-weight:700;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.card-info p{font-size:.68em;color:var(--dim);margin-top:2px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.badge{font-size:.6em;padding:2px 7px;border-radius:8px;font-weight:700;flex-shrink:0}
.bw{background:#0d1f2a;color:#3f9eff}.bb{background:#0d122a;color:#7c8fff}.bp{background:#1a1226;color:#aa7fff}.bh{background:#1f1a0d;color:var(--orn)}
.chev{color:var(--dim);font-size:.7em;margin-left:4px;transition:transform .2s;flex-shrink:0}
.card.open>.card-hd .chev{transform:rotate(90deg)}
/* ── Card body ── */
.card-bd{display:none;padding:12px 14px;border-top:1px solid var(--bdr)}
.card.open>.card-bd{display:block}
.fgrid{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-bottom:10px}
.fgrid.w1{grid-template-columns:1fr}
.fld label{display:block;font-size:.65em;color:var(--dim);margin-bottom:3px;text-transform:uppercase;letter-spacing:.6px;font-weight:600}
.fld input,.fld select{width:100%;background:var(--bg);color:var(--fg);border:1px solid var(--bdr);border-radius:5px;padding:6px 8px;font-size:.8em;transition:border-color .15s}
.fld input:focus,.fld select:focus{outline:none;border-color:var(--acc)}
.fld select option{background:var(--bg);color:var(--fg)}
.fld input[type=checkbox]{width:auto;margin-right:6px;accent-color:var(--acc)}
.chk-row{display:flex;align-items:center;padding-top:16px}
/* ── Buttons ── */
.btn-row{display:flex;gap:7px;flex-wrap:wrap}
.btn{border:none;border-radius:6px;padding:7px 14px;cursor:pointer;font-size:.78em;font-weight:700;letter-spacing:.3px;transition:opacity .15s,transform .1s;white-space:nowrap}
.btn:hover{opacity:.85}.btn:active{transform:scale(.97)}
.bs{background:var(--grn2);color:var(--ok);border:1px solid #1a4a2a}
.bd{background:var(--red2);color:var(--acc);border:1px solid #4a1a1a}
.bx{background:var(--bg3);color:var(--dim);border:1px solid var(--bdr)}
/* ── Status bar ── */
.card-st{padding:7px 14px;background:#090b0f;border-top:1px solid var(--bdr);display:flex;align-items:center;gap:8px;font-size:.72em}
.dot{width:7px;height:7px;border-radius:50%;background:#222;flex-shrink:0}
.dot.on{background:var(--ok);box-shadow:0 0 6px var(--ok);animation:pulse 1.5s infinite}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:.35}}
.st-txt{color:var(--dim);flex:1;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.st-cnt{color:var(--fg);font-weight:700;font-family:monospace;flex-shrink:0}
/* ── Scan bar ── */
#scanbar{background:var(--bg2);border:1px solid var(--bdr);border-radius:8px;padding:9px 13px;margin-bottom:13px;display:flex;flex-wrap:wrap;gap:8px;align-items:center}
#scanbar label{font-size:.72em;color:var(--dim);font-weight:600}
#scan-sel{flex:1;min-width:200px;background:var(--bg);color:var(--fg);border:1px solid var(--bdr);border-radius:5px;padding:5px 7px;font-size:.78em;display:none}
/* ── Dashboard ── */
.dash-row{display:grid;grid-template-columns:repeat(auto-fill,minmax(140px,1fr));gap:10px;margin-bottom:16px}
.dcard{background:var(--bg2);border:1px solid var(--bdr);border-radius:8px;padding:14px;text-align:center}
.dval{font-size:1.9em;font-weight:800;color:var(--ok);font-family:monospace;line-height:1}
.dlbl{font-size:.65em;color:var(--dim);margin-top:5px;text-transform:uppercase;letter-spacing:.6px}
.dcard.a{border-color:var(--acc)}.dcard.a .dval{color:var(--acc)}
.sec-ttl{font-size:.7em;font-weight:700;color:var(--dim);text-transform:uppercase;letter-spacing:1px;margin:16px 0 8px;display:flex;align-items:center;gap:8px}
.sec-ttl::after{content:'';flex:1;height:1px;background:var(--bdr)}
/* ── File manager ── */
.ftbl{width:100%;border-collapse:collapse;font-size:.8em}
.ftbl th{text-align:left;padding:7px 10px;color:var(--dim);border-bottom:1px solid var(--bdr);font-size:.7em;text-transform:uppercase;letter-spacing:.5px}
.ftbl td{padding:7px 10px;border-bottom:1px solid #131620}
.ftbl tr:hover td{background:var(--bg3)}
.fsbar{background:var(--bdr);border-radius:4px;height:6px;margin:6px 0 14px;overflow:hidden}
.fsfill{background:linear-gradient(90deg,var(--ok),var(--orn));height:100%;transition:width .4s}
/* ── Toast ── */
#toasts{position:fixed;bottom:16px;right:16px;display:flex;flex-direction:column-reverse;gap:8px;z-index:9999;pointer-events:none}
.toast{background:var(--bg2);border:1px solid var(--bdr);border-left:3px solid var(--ok);border-radius:7px;padding:10px 14px;font-size:.78em;max-width:280px;animation:tIn .2s ease;pointer-events:auto}
.toast.e{border-left-color:var(--acc)}.toast.w{border-left-color:var(--orn)}
@keyframes tIn{from{transform:translateX(20px);opacity:0}to{transform:none;opacity:1}}
/* ── CLI ── */
.cli-wrap{display:flex;gap:0;height:calc(100vh - 115px);overflow:hidden}
.cli-sidebar{width:220px;min-width:160px;background:#0a0c10;border:1px solid var(--bdr);border-radius:8px 0 0 8px;overflow-y:auto;flex-shrink:0;scrollbar-width:thin;scrollbar-color:#1e2430 transparent}
.cli-main{flex:1;display:flex;flex-direction:column;min-width:0}
#cli-out{flex:1;background:#000;border:1px solid var(--bdr);border-left:none;border-radius:0 8px 0 0;padding:10px 14px;overflow-y:auto;font-family:'SF Mono','Fira Code',Menlo,Consolas,monospace;font-size:.78em;line-height:1.6;word-break:break-all}
.co{display:block;white-space:pre-wrap}
.co.in{color:#3fffd4;font-weight:700}
.co.ok{color:#88ff88}
.co.er{color:#ff5f5f}
.co.hd{color:#ffb347;font-weight:700}
.co.ex{color:#a8d8ff}
.co.us{color:#c3f0ca}
.co.dm{color:#4a5568}
.co.sep{color:#1e3040;user-select:none}
.cli-row{display:flex;background:#000;border:1px solid var(--bdr);border-left:none;border-top:none;border-radius:0 0 8px 0;padding:7px 12px;align-items:center}
.cli-ps{color:#3fc;font-family:'SF Mono',Menlo,monospace;font-size:.85em;margin-right:6px;flex-shrink:0}
#cli-in{flex:1;background:none;border:none;color:#88ff88;font-family:'SF Mono','Fira Code',Menlo,Consolas,monospace;font-size:.82em;outline:none}
/* Sidebar */
.csb-cat{padding:7px 12px 3px;font-size:.6em;text-transform:uppercase;letter-spacing:1px;color:#3a4a5a;font-weight:700;border-top:1px solid #0f1520}
.csb-cmd{display:block;padding:4px 10px 4px 14px;font-size:.72em;color:#5a7a8a;cursor:pointer;text-decoration:none;transition:background .1s,color .1s;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.csb-cmd:hover{background:#0d1f2a;color:#3fffd4}
.csb-cmd b{color:#7ec8e3;font-weight:600}
/* Autocomplete */
#ac-box{position:absolute;background:var(--bg2);border:1px solid var(--bdr);border-radius:6px;min-width:200px;max-height:160px;overflow-y:auto;z-index:200;display:none;box-shadow:0 4px 20px rgba(0,0,0,.5)}
.ac-item{padding:5px 12px;font-size:.8em;font-family:monospace;cursor:pointer;color:var(--fg)}
.ac-item:hover,.ac-item.sel{background:var(--bg3);color:var(--acc)}
.cli-wrap-outer{position:relative}
@media(max-width:620px){.cli-sidebar{display:none}}
/* ── Misc ── */
.mt8{margin-top:8px}.mb4{margin-bottom:4px}
.empty{color:var(--dim);font-size:.82em;padding:24px;text-align:center}
/* ── Modal ── */
#modal-overlay{position:fixed;inset:0;background:rgba(0,0,0,.7);z-index:9000;display:none;align-items:center;justify-content:center}
#modal-overlay.on{display:flex}
#modal-box{background:var(--bg2);border:1px solid var(--bdr);border-radius:12px;min-width:300px;max-width:520px;width:90%;max-height:70vh;overflow:hidden;display:flex;flex-direction:column}
#modal-hd{display:flex;align-items:center;padding:12px 16px;border-bottom:1px solid var(--bdr)}
#modal-hd h2{flex:1;font-size:.95em}
#modal-close{background:none;border:none;color:var(--dim);cursor:pointer;font-size:1.3em;padding:0 4px}
#modal-body{padding:14px 16px;overflow-y:auto;font-family:monospace;font-size:.78em;white-space:pre-wrap;color:var(--fg)}
/* ── Board badge ── */
#board-badge{font-size:.65em;background:var(--bg3);border:1px solid var(--bdr);border-radius:6px;padding:2px 8px;color:var(--dim);white-space:nowrap}
@media(max-width:580px){.fgrid{grid-template-columns:1fr}.grid{grid-template-columns:1fr}.dash-row{grid-template-columns:1fr 1fr}}
</style></head>
<body style="display:flex;flex-direction:column;height:100%">

<div id="hdr">
  <div id="hdr-logo">PHANTOM<sup>32</sup></div>
  <div id="board-badge">—</div>
  <div id="active-pill">● ACTIVE</div>
  <div id="hdr-stats">
    <div class="hstat">Heap <span id="s-heap">—</span>k</div>
    <div class="hstat">Up <span id="s-up">—</span>s</div>
    <div class="hstat">Active <span id="s-act">none</span></div>
  </div>
</div>

<div id="nav">
  <button class="ntab on" onclick="tab('dash')">⬡ Dashboard</button>
  <button class="ntab" onclick="tab('wifi')">📡 WiFi Attacks</button>
  <button class="ntab" onclick="tab('ble')">🔵 BLE</button>
  <button class="ntab" onclick="tab('passive')">👁 Passive</button>
  <button class="ntab" onclick="tab('hw')">⚙ Hardware</button>
  <button class="ntab" onclick="tab('files')">🗂 Files</button>
  <button class="ntab" onclick="tab('aegis')">🛡 Aegis</button>
  <button class="ntab" onclick="tab('cli')">⌨ CLI</button>
</div>

<div id="wrap">
  <div id="pg-dash" class="page on">
    <div id="dash-row" class="dash-row">
      <div class="dcard"><div class="dval" id="d-heap">—</div><div class="dlbl">Heap (KB)</div></div>
      <div class="dcard"><div class="dval" id="d-up">—</div><div class="dlbl">Uptime (s)</div></div>
      <div class="dcard"><div class="dval" id="d-tools">—</div><div class="dlbl">Tools</div></div>
      <div class="dcard a"><div class="dval" id="d-act">none</div><div class="dlbl">Active</div></div>
      <div class="dcard"><div class="dval" id="d-board" style="font-size:1em">—</div><div class="dlbl">Board</div></div>
    </div>
    <div class="sec-ttl">Quick Start</div>
    <div style="display:flex;flex-wrap:wrap;gap:8px;margin-bottom:16px">
      <button class="btn bs" onclick="quickStart('deauth')">⚡ Deauth BC</button>
      <button class="btn bs" onclick="quickStart('beacon')">📶 Beacon Flood</button>
      <button class="btn bs" onclick="quickStart('chanhop')">🌀 Chan Hop Deauth</button>
      <button class="btn bs" onclick="quickStart('blespam')">📱 BLE Spam</button>
      <button class="btn bs" onclick="quickStart('karma')">🎭 KARMA</button>
      <button class="btn bs" onclick="window.open('/pwnface','_blank')">🤖 Pwngotchi</button>
      <button class="btn bd" onclick="stopAll()">⏹ Stop All</button>
    </div>
    <div class="sec-ttl">Tool Status</div>
    <div id="dash-status" class="grid"></div>
  </div>

  <div id="pg-wifi" class="page">
    <div id="scanbar">
      <label>📡 Scan:</label>
      <button class="btn bs" id="scan-btn" onclick="runScan(false)">Scan APs</button>
      <button class="btn bx" title="Force live WiFi scan (may briefly drop connection)" onclick="runScan(true)">⚡ Live</button>
      <select id="scan-sel" onchange="applyScan(this.value)">
        <option value="">— select AP to auto-fill —</option>
      </select>
    </div>
    <div id="grid-wifi" class="grid"></div>
  </div>

  <div id="pg-ble" class="page">
    <div id="grid-ble" class="grid"></div>
  </div>

  <div id="pg-passive" class="page">
    <div id="grid-passive" class="grid"></div>
  </div>

  <div id="pg-hw" class="page">
    <div id="grid-hw" class="grid"></div>
  </div>

  <div id="pg-files" class="page">
    <div class="sec-ttl">Flash Storage</div>
    <div class="fsbar"><div class="fsfill" id="fs-fill" style="width:0%"></div></div>
    <div id="fs-info" style="font-size:.75em;color:var(--dim);margin-bottom:14px"></div>
    <div class="sec-ttl">PCAP Captures</div>
    <table class="ftbl"><thead><tr><th>File</th><th>Size</th><th>Actions</th></tr></thead>
    <tbody id="pcap-list"><tr><td colspan="3" class="empty">No captures yet</td></tr></tbody></table>
  </div>

  <div id="pg-aegis" class="page" style="padding:0;overflow:hidden;height:calc(100vh - 115px)">
    <iframe id="iframe-aegis" src="/aegis" style="width:100%;height:100%;border:0;display:block"></iframe>
  </div>

  <div id="pg-cli" class="page" style="padding:10px 12px">
    <div class="cli-wrap-outer">
      <div id="ac-box"></div>
      <div class="cli-wrap">
        <div class="cli-sidebar" id="cli-sidebar"></div>
        <div class="cli-main">
          <div id="cli-out"></div>
          <div class="cli-row">
            <span class="cli-ps">doom&gt;</span>
            <input id="cli-in" autocomplete="off" spellcheck="false" autocapitalize="off" placeholder="type a command, or click one →">
          </div>
        </div>
      </div>
    </div>
  </div>
</div>
<div id="toasts"></div>
<div id="modal-overlay" onclick="closeModal(event)">
  <div id="modal-box">
    <div id="modal-hd"><h2 id="modal-title">Info</h2><button id="modal-close" onclick="closeModal()">✕</button></div>
    <div id="modal-body"></div>
  </div>
</div>

<script>
// ── Modal ────────────────────────────────────────────────────────────────────
function showModal(title,body){
  document.getElementById('modal-title').textContent=title;
  document.getElementById('modal-body').textContent=body;
  document.getElementById('modal-overlay').classList.add('on');
}
function closeModal(e){
  if(e&&e.target!==document.getElementById('modal-overlay'))return;
  document.getElementById('modal-overlay').classList.remove('on');
}

// ── Routing ──────────────────────────────────────────────────────────────────
function tab(id){
  document.querySelectorAll('.ntab').forEach((b,i)=>{b.classList.toggle('on',b.onclick.toString().includes("'"+id+"'"))});
  document.querySelectorAll('.page').forEach(p=>p.classList.toggle('on',p.id==='pg-'+id));
  if(id==='cli')setTimeout(()=>document.getElementById('cli-in').focus(),50);
  if(id==='files')loadFiles();
}

// ── Toast ─────────────────────────────────────────────────────────────────────
function toast(msg,type=''){
  const d=document.createElement('div');d.className='toast'+(type?' '+type:'');d.textContent=msg;
  const c=document.getElementById('toasts');c.appendChild(d);
  setTimeout(()=>{d.style.opacity='0';d.style.transition='opacity .3s';setTimeout(()=>d.remove(),300)},3000);
}

// ── Tool definitions ──────────────────────────────────────────────────────────
// [field_id, label, type, default, options_or_placeholder]
// types: text, mac, num, ch, bool, sel
const CH_OPTS=[[1],[2],[3],[4],[5],[6],[7],[8],[9],[10],[11],[12],[13]].map(([v])=>[String(v),'Ch '+v]);
const TOOLS={
  deauth:{name:'Deauth',ico:'💥',cat:'wifi',desc:'802.11 deauth/disassoc injection',
    fields:[['bssid','Target BSSID','mac','FF:FF:FF:FF:FF:FF'],['ch','Channel','ch','6'],
      ['reason','Reason','sel','7',[['7','7 – Class 3'],['1','1 – Unspecified'],['2','2 – Auth invalid'],['3','3 – Deauth leaving'],['4','4 – Inactivity'],['6','6 – Class 2']]],
      ['broadcast','Broadcast all stations','bool','1']]},
  beacon:{name:'Beacon Spam',ico:'📶',cat:'wifi',desc:'Flood beacon frames',
    fields:[['ssid','SSID','text','Free WiFi'],['count','Count (0=∞)','num','200'],['random','Random SSIDs','bool','1']]},
  probe:{name:'Probe Flood',ico:'🔎',cat:'wifi',desc:'Randomised probe request flood',
    fields:[['ssid','SSID','text','Free WiFi'],['count','Count','num','500']]},
  eviltwin:{name:'Evil Twin',ico:'👥',cat:'wifi',desc:'Rogue AP + deauth + captive portal',
    fields:[['ssid','Clone SSID','text','Free WiFi'],['bssid','Target BSSID','mac',''],['ch','Channel','ch','6']]},
  authflood:{name:'Auth Flood',ico:'🌊',cat:'wifi',desc:'Auth/assoc/disassoc table flood',
    fields:[['bssid','Target BSSID','mac',''],['ch','Channel','ch','6'],
      ['mode','Mode','sel','2',[['0','0 – Auth only'],['1','1 – Assoc only'],['2','2 – Auth+Assoc'],['3','3 – Disassoc BC'],['4','4 – All three']]]]},
  karma:{name:'KARMA',ico:'🎭',cat:'wifi',desc:'Respond to all probe requests with matching AP',
    fields:[['wpa','Encryption','sel','0',[['0','Open (no password)'],['1','WPA2']]]]},
  navflood:{name:'NAV Flood',ico:'📵',cat:'wifi',desc:'CTS/RTS NAV DoS – virtual carrier-sense',
    fields:[['target','Target MAC','mac','FF:FF:FF:FF:FF:FF'],['ch','Channel','ch','6'],
      ['mode','Frame type','sel','cts',[['cts','CTS (NAV=32767μs)'],['rts','RTS flood'],['both','CTS+RTS']]]]},
  csa:{name:'CSA Attack',ico:'📺',cat:'wifi',desc:'Channel switch + BTM + Neighbor report',
    fields:[['bssid','Target BSSID','mac',''],['ssid','Target SSID','text','target'],
      ['ch','Current ch','ch','6'],['target_ch','Target ch','ch','14']]},
  espnow:{name:'ESP-NOW Flood',ico:'📨',cat:'wifi',desc:'ESP-NOW broadcast/unicast flood',
    fields:[['target','Target MAC','mac','FF:FF:FF:FF:FF:FF']]},
  beaconadv:{name:'Adv Frames',ico:'🧪',cat:'wifi',desc:'WPS/fuzz/TKIP/WDS injection',
    fields:[['mode','Mode','sel','wps',[['wps','WPS beacon'],['fuzz','Frame fuzzer'],['overflow','IE overflow'],['hidden','Hidden SSID'],['bigIE','Big vendor IE'],['ssidOverflow','SSID overflow'],['tkipMic','TKIP MIC fail'],['dupSeq','Dup seq#'],['quiet','Quiet IE'],['wds','WDS 4-addr']]],
      ['ssid','SSID','text','Free WiFi'],['ch','Channel','ch','6'],['count','Count (0=∞)','num','0']]},
  wpa3flood:{name:'WPA3 Flood',ico:'🔐',cat:'wifi',desc:'SAE/WPA3 flood + transition-mode deauth',
    fields:[['bssid','Target BSSID','mac',''],['ch','Channel','ch','6'],
      ['mode','Mode','sel','commit',[['commit','SAE Commit flood'],['confirm','SAE Confirm flood'],['both','Commit + Confirm'],['eapstart','EAP-Start flood'],['transition','Transition Deauth (WPA2 clients)'],['pmkid','PMKID Exhaust']]]]},
  chanhop:{name:'Chan Hop Deauth',ico:'🌀',cat:'wifi',desc:'All-channel deauth sweep — finds & kills all APs',
    fields:[]},
  btmflood:{name:'BTM Flood',ico:'📡',cat:'wifi',desc:'802.11v BSS Transition — forces iOS/Android/Windows to roam',
    fields:[['bssid','AP BSSID','mac',''],['ch','AP Channel','ch','6'],
      ['sta','Target STA (blank=broadcast)','mac',''],['target_ch','Redirect to ch','ch','14'],
      ['broadcast','Broadcast all stations','bool','1'],
      ['mode','Mode','sel','btm',[['btm','BTM Request'],['neighbor','Neighbor Report'],['both','BTM + Neighbor']]]]},
  eapflood:{name:'EAP Flood',ico:'🔑',cat:'wifi',desc:'WPA Enterprise EAPOL state machine exhaustion',
    fields:[['bssid','Target BSSID','mac',''],['ch','Channel','ch','6'],
      ['mode','EAP Mode','sel','start',[['start','EAPOL Start'],['identity','EAP Identity'],['m1','EAPOL M1'],['all','All types']]]]},
  nullflood:{name:'Null Flood',ico:'💤',cat:'wifi',desc:'Power-save null/QoS-null/BAR confusion',
    fields:[['bssid','Target AP BSSID','mac',''],['ch','Channel','ch','6'],
      ['sta','Target STA (blank=random)','mac',''],['broadcast','Random STAs','bool','1'],
      ['mode','Mode','sel','null',[['null','Null+QoS-Null'],['pspoll','PS-Poll flood'],['bar','BAR flood'],['all','All types']]]]},
  mgtflood:{name:'Mgmt Spray',ico:'🌊',cat:'wifi',desc:'All 802.11 management subtypes in rapid rotation',
    fields:[['bssid','Target BSSID','mac',''],['ch','Channel','ch','6'],['ssid','SSID','text','target']]},
  multideauth:{name:'Multi Deauth',ico:'💣',cat:'wifi',desc:'Deauth up to 8 BSSIDs simultaneously on their channels',
    fields:[['bssid0','Target 1 BSSID','mac',''],['ch0','Ch 1','ch','6'],
      ['bssid1','Target 2 BSSID','mac',''],['ch1','Ch 2','ch','6'],
      ['bssid2','Target 3 BSSID','mac',''],['ch2','Ch 3','ch','6'],
      ['bssid3','Target 4 BSSID','mac',''],['ch3','Ch 4','ch','6']]},
  actionflood:{name:'Action Flood',ico:'⚡',cat:'wifi',desc:'802.11 action frame spray across all categories',
    fields:[['bssid','Target BSSID','mac',''],['ch','Channel','ch','6'],
      ['broadcast','Broadcast','bool','1'],
      ['mode','Category','sel','random',[['random','Random (all cats)'],['spectrum','Spectrum Mgmt'],['blockack','Block ACK ADDBA'],['saquery','SA Query'],['wnm','WNM Sleep Mode']]]]},
  pwnface:{name:'Pwngotchi',ico:'🤖',cat:'passive',desc:'Autonomous handshake hunter — UCB AI channel selection, deauths, EAPOL detection',
    fields:[]},
  probeharvest:{name:'Probe Harvest',ico:'🕵️',cat:'passive',desc:'Passive sniffer — logs nearby devices + their saved network history (no TX)',
    fields:[]},
  stormdeauth:{name:'Deauth Storm',ico:'💥',cat:'wifi',desc:'Multi-vector WPA2+WPA3 deauth (deauth+disassoc+SAE+PMKID+BTM — 8 attack vectors)',
    fields:[['bssid','Target BSSID','mac',''],['ch','Channel','ch','6'],['sta','Target STA','mac',''],['broadcast','Broadcast','bool','1']]},
  pmkid:{name:'PMKID Hunt',ico:'🔑',cat:'passive',desc:'Active PMKID extraction → PCAP (no client needed)',
    fields:[['bssid','Target BSSID','mac',''],['ch','Channel','ch','6']]},
  handshake:{name:'Handshake Grab',ico:'🤝',cat:'passive',desc:'EAPOL 4-way handshake capture → PCAP',
    fields:[['bssid','Target BSSID','mac',''],['ch','Channel','ch','6'],['force','Force deauth','bool','1']]},
  portal:{name:'Evil Portal',ico:'🌐',cat:'passive',desc:'Captive portal credential capture',
    fields:[['ssid','AP SSID','text','Free WiFi']]},
  honeypot:{name:'Honeypot',ico:'🍯',cat:'passive',desc:'TCP 22/80/2323 connection logger',
    fields:[]},
  blespam:{name:'BLE Spam',ico:'📱',cat:'ble',desc:'Trigger device popups across Apple/Android/Windows',
    fields:[['mode','Mode','sel','apple',[['apple','Apple (random)'],['continuity','Continuity (all)'],['airdrop','AirDrop flood'],['handoff','Handoff/Clipboard'],['android','Android FastPair'],['googlenearby','Google Nearby'],['windows','Windows SwiftPair'],['swiftpair','SwiftPair (named)'],['samsung','Samsung BLE'],['airtag','AirTag clone'],['ibeacon','iBeacon'],['eddystone','Eddystone URL'],['namespam','Device name spam'],['scan','BLE scan flood'],['rapidflood','Random BLE flood']]]]},
  i2c:{name:'I2C Sniff',ico:'🔌',cat:'hw',desc:'I2C slave mode logger',
    fields:[['sda','SDA pin','num','21'],['scl','SCL pin','num','22'],['addr','Address','text','0x50']]},
  spi:{name:'SPI Sniff',ico:'🔌',cat:'hw',desc:'SPI MOSI sampler (ISR)',
    fields:[['clk','CLK pin','num','18'],['mosi','MOSI pin','num','23'],['cs','CS pin','num','5']]},
  uart:{name:'UART Bridge',ico:'🔌',cat:'hw',desc:'UART1 ↔ USB serial bridge',
    fields:[['rx','RX pin','num','16'],['tx','TX pin','num','17'],
      ['baud','Baud rate','sel','115200',[['9600','9600'],['19200','19200'],['38400','38400'],['57600','57600'],['115200','115200'],['230400','230400'],['460800','460800'],['921600','921600']]]]},
};

// ── Form builder ─────────────────────────────────────────────────────────────
function buildForm(slug){
  const t=TOOLS[slug];if(!t||!t.fields.length)return '<div style="color:var(--dim);font-size:.78em;padding:4px 0">No parameters required</div>';
  const halves=[];
  t.fields.forEach(([id,lbl,type,def,opts])=>{
    const fid=slug+'_'+id;
    let inp='';
    if(type==='bool'){
      inp=`<div class="chk-row"><input type="checkbox" id="${fid}" ${def==='1'?'checked':''}><label for="${fid}" style="text-transform:none;letter-spacing:0;font-size:.78em">${lbl}</label></div>`;
      halves.push(`<div class="fld">${inp}</div>`);return;
    }
    if(type==='sel'&&opts){
      inp=`<div class="fld"><label>${lbl}</label><select id="${fid}">${opts.map(([v,l])=>`<option value="${v}"${v===def?' selected':''}>${l}</option>`).join('')}</select></div>`;
    } else if(type==='ch'){
      inp=`<div class="fld"><label>${lbl}</label><select id="${fid}">${CH_OPTS.map(([v,l])=>`<option value="${v}"${v===def?' selected':''}>${l}</option>`).join('')}</select></div>`;
    } else if(type==='num'){
      inp=`<div class="fld"><label>${lbl}</label><input type="number" id="${fid}" value="${def}" min="0"></div>`;
    } else {
      const ph=type==='mac'?' placeholder="AA:BB:CC:DD:EE:FF"':'';
      inp=`<div class="fld"><label>${lbl}</label><input type="text" id="${fid}" value="${def}"${ph}></div>`;
    }
    halves.push(inp);
  });
  // Pair into 2-col grid rows
  let html='<div class="fgrid">';
  halves.forEach((h,i)=>{
    if(i>0&&i%2===0)html+='</div><div class="fgrid">';
    html+=h;
  });
  if(halves.length%2===1)html+='<div class="fld"></div>';
  html+='</div>';
  return html;
}

function extraBtns(slug){
  if(slug==='handshake'||slug==='pmkid')return `<button class="btn bx" onclick="dlPcap()">⬇ PCAP</button>`;
  if(slug==='eviltwin')return `<button class="btn bx" onclick="showCreds()">📋 Creds</button>`;
  if(slug==='chanhop')return `<button class="btn bx" onclick="showChHopAPs()">📋 APs found</button>`;
  if(slug==='pwnface')return `<button class="btn bx" onclick="window.open('/pwnface','_blank')">👁 Face</button>`;
  if(slug==='probeharvest')return `<button class="btn bx" onclick="showProbeHarvest()">📋 Devices</button>`;
  return '';
}

function mkCard(slug,isActive){
  const t=TOOLS[slug];if(!t)return '';
  return `<div class="card${isActive?' run':''}" id="card-${slug}">
    <div class="card-hd" onclick="toggleCard('${slug}')">
      <span class="card-ico">${t.ico}</span>
      <div class="card-info"><h3>${t.name}</h3><p>${t.desc}</p></div>
      <span class="badge ${t.cat==='wifi'?'bw':t.cat==='ble'?'bb':t.cat==='hw'?'bh':'bp'}">${t.cat.toUpperCase()}</span>
      <span class="chev">▶</span>
    </div>
    <div class="card-bd">
      ${buildForm(slug)}
      <div class="btn-row mt8">
        <button class="btn bs" onclick="go('${slug}','start')">▶ Start</button>
        <button class="btn bd" onclick="go('${slug}','stop')">■ Stop</button>
        ${extraBtns(slug)}
      </div>
    </div>
    <div class="card-st"><span class="dot" id="dot-${slug}"></span><span class="st-txt" id="st-${slug}">idle</span></div>
  </div>`;
}

function toggleCard(s){
  document.getElementById('card-'+s).classList.toggle('open');
}

// ── Rendering ─────────────────────────────────────────────────────────────────
let g_active='';
function renderGrids(mods){
  const cats={wifi:[],ble:[],passive:[],hw:[]};
  mods.forEach(m=>{
    if(m.slug==='aegis')return;
    const t=TOOLS[m.slug];
    const cat=t?t.cat:(m.radio===2?'ble':m.radio===0?'hw':'wifi');
    if(cats[cat])cats[cat].push(m.slug);
    else cats.wifi.push(m.slug);
  });
  ['wifi','ble','passive','hw'].forEach(c=>{
    const el=document.getElementById('grid-'+c);
    if(el)el.innerHTML=cats[c].map(s=>mkCard(s,s===g_active)).join('')||'<div class="empty">No tools in this category</div>';
  });
  // Dash mini-cards
  const ds=document.getElementById('dash-status');
  if(ds){
    ds.innerHTML=mods.filter(m=>m.slug!=='aegis').map(m=>{
      const t=TOOLS[m.slug];
      return `<div class="card" id="dsc-${m.slug}" style="cursor:pointer" onclick="tab('${t?t.cat:'wifi'}')">
        <div class="card-st" style="padding:10px 12px">
          <span class="dot" id="ddot-${m.slug}"></span>
          <span style="flex:1;font-weight:600;font-size:.8em">${t?t.ico:''} ${m.name}</span>
        </div>
        <div style="padding:4px 12px 8px;font-size:.7em;color:var(--dim)" id="dst-${m.slug}">idle</div>
      </div>`;
    }).join('');
  }
}

// ── API helpers ───────────────────────────────────────────────────────────────
let g_mods=[];
async function loadModules(){
  try{
    const d=await(await fetch('/api/modules')).json();
    g_active=d.active||'';
    g_mods=d.modules;
    document.getElementById('s-heap').textContent=Math.round(d.heap/1024);
    document.getElementById('s-up').textContent=d.uptime;
    document.getElementById('s-act').textContent=d.active||'none';
    document.getElementById('d-heap').textContent=Math.round(d.heap/1024);
    document.getElementById('d-up').textContent=d.uptime;
    document.getElementById('d-tools').textContent=d.modules.length;
    document.getElementById('d-act').textContent=d.active||'none';
    const pill=document.getElementById('active-pill');
    if(d.active){pill.textContent='● '+d.active.toUpperCase();pill.style.display='inline-block';}
    else pill.style.display='none';
    renderGrids(d.modules);
    // Re-mark active card
    document.querySelectorAll('.card').forEach(c=>{
      const s=c.id.replace('card-','');
      c.classList.toggle('run',s===g_active);
    });
  }catch(e){console.error(e)}
}

async function go(slug,action){
  let url='/api/'+slug+'/'+action;
  if(action==='start'){
    const p=new URLSearchParams();
    const t=TOOLS[slug];
    (t?t.fields:[]).forEach(([id,,type])=>{
      const el=document.getElementById(slug+'_'+id);
      if(!el)return;
      if(type==='bool') p.set(id,el.checked?'1':'0');
      else if(el.value.trim()) p.set(id,el.value.trim());
    });
    if([...p].length)url+='?'+p.toString();
  }
  try{
    const d=await(await fetch(url,{method:'POST'})).json();
    if(d.ok)toast(slug+' '+action,(action==='start'?'':''));
    else toast(slug+': '+(d.err||'error'),'e');
  }catch(e){toast('network: '+e,'e')}
  await loadModules();
}

async function quickStart(slug){
  // Open the card and scroll to it, then start
  const card=document.getElementById('card-'+slug);
  if(!card){
    // Not in current page; find the category and switch
    const t=TOOLS[slug];
    if(t)tab(t.cat);
    await new Promise(r=>setTimeout(r,80));
  }
  await go(slug,'start');
}

async function stopAll(){
  if(g_active)await go(g_active,'stop');
  toast('All stopped');
}

// ── Polling ───────────────────────────────────────────────────────────────────
async function pollStatus(){
  for(const m of g_mods){
    if(m.slug==='aegis')continue;
    try{
      const d=await(await fetch('/api/'+m.slug+'/status')).json();
      const on=!!d.running,msg=d.msg||(on?'running':'idle');
      // Cards in grids
      const dot=document.getElementById('dot-'+m.slug);
      const st=document.getElementById('st-'+m.slug);
      if(dot)dot.classList.toggle('on',on);
      if(st)st.textContent=msg;
      // Dash mini-cards
      const ddot=document.getElementById('ddot-'+m.slug);
      const dst=document.getElementById('dst-'+m.slug);
      if(ddot)ddot.classList.toggle('on',on);
      if(dst)dst.textContent=msg;
    }catch(e){}
  }
  setTimeout(pollStatus,2500);
}

// ── Scan APs ──────────────────────────────────────────────────────────────────
async function runScan(live){
  const btn=document.getElementById('scan-btn');
  btn.textContent='Loading…';btn.disabled=true;
  try{
    const url=live?'/api/scan?live=1':'/api/scan';
    const aps=await(await fetch(url)).json();
    const sel=document.getElementById('scan-sel');
    if(!aps.length&&!live){
      // Cache empty on first boot — try live scan
      btn.textContent='Scanning…';
      const aps2=await(await fetch('/api/scan?live=1')).json();
      populateScanSel(aps2);
    } else {
      populateScanSel(aps);
    }
  }catch(e){toast('Scan failed: '+e,'e')}
  btn.textContent='Scan APs';btn.disabled=false;
}
function populateScanSel(aps){
  const sel=document.getElementById('scan-sel');
  sel.innerHTML='<option value="">— select AP to auto-fill —</option>'+
    aps.sort((a,b)=>b.rssi-a.rssi)
       .map(a=>`<option value="${a.bssid}:${a.ch}:${encodeURIComponent(a.ssid)}">${a.ssid||'(hidden)'} [${a.bssid}] ch${a.ch} ${a.rssi}dBm${a.enc?' ('+a.enc+')':''}</option>`)
       .join('');
  sel.style.display='inline';
  toast('Found '+aps.length+' APs'+(aps.length&&aps[0].pmf!==undefined?' (Aegis data)':''));
}

function applyScan(val){
  if(!val)return;
  const[bssid,ch,ssidEnc]=val.split(':');
  const ssid=decodeURIComponent(ssidEnc||'');
  document.querySelectorAll('[id$="_bssid"],[id$="_target"]').forEach(el=>el.value=bssid);
  document.querySelectorAll('[id$="_ch"]').forEach(el=>{if(el.tagName==='SELECT'){for(const o of el.options)if(o.value===ch){o.selected=true;break}}else el.value=ch});
  document.querySelectorAll('[id$="_ssid"]').forEach(el=>el.value=ssid);
  toast('Applied: '+ssid+' ch'+ch);
}

// ── Extras ───────────────────────────────────────────────────────────────────
function dlPcap(){tab('files');toast('Opening Files tab — download PCAPs there');}

async function showCreds(){
  try{
    const d=await(await fetch('/api/eviltwin/creds')).json();
    showModal('Captured Credentials',d.lines||'(none captured yet)');
  }catch(e){toast('Fetch failed: '+e,'e')}
}
async function showChHopAPs(){
  try{
    const d=await(await fetch('/api/chanhop/aps')).json();
    const txt=d.length?d.map((a,i)=>`${i+1}. ${a.bssid}  ch${a.ch}  ${a.ssid||'(hidden)'}`).join('\n'):'No APs discovered yet';
    showModal('Chan Hop — Discovered APs ('+d.length+')',txt);
  }catch(e){toast('Fetch failed: '+e,'e')}
}

async function showProbeHarvest(){
  try{
    const d=await(await fetch('/api/probeharvest/results')).json();
    if(!d.length){showModal('Probe Harvest','No probe requests captured yet.');return;}
    // Group by STA MAC
    const map={};
    d.forEach(e=>{if(!map[e.sta])map[e.sta]={rssi:e.rssi,ssids:[]};map[e.sta].ssids.push(e.ssid);});
    const lines=Object.entries(map).map(([mac,v])=>`${mac}  (${v.rssi}dBm)\n  → ${v.ssids.join(', ')}`);
    showModal('Probe Harvest — '+d.length+' entries, '+lines.length+' devices',lines.join('\n\n'));
  }catch(e){toast('Fetch failed: '+e,'e')}
}
// ── Board detection ───────────────────────────────────────────────────────────
let g_board={};
async function loadBoard(){
  try{
    g_board=await(await fetch('/api/board')).json();
    const b=g_board;
    const bstr=(b.board||b.model||'ESP32')+' · '+b.flash_mb+'MB flash'+(b.psram_kb>0?' · '+b.psram_kb+'KB PSRAM':'')+(b.bt5?' · BLE5':'');
    document.getElementById('board-badge').textContent=bstr;
    // Apply board pin defaults
    if(b.pins){
      const P=b.pins;
      setDefault('i2c_sda',P.i2c_sda);setDefault('i2c_scl',P.i2c_scl);
      setDefault('spi_clk',P.spi_clk);setDefault('spi_mosi',P.spi_mosi);setDefault('spi_cs',P.spi_cs);
      setDefault('uart_rx',P.uart_rx);setDefault('uart_tx',P.uart_tx);
    }
    // Show BLE5 badge on blespam if supported
    if(b.bt5){
      const c=document.getElementById('card-blespam');
      if(c){const p=c.querySelector('.card-info p');if(p)p.textContent+=' · BLE5 2M PHY';}
    }
    // Dash board card
    const db=document.getElementById('d-board');if(db)db.textContent=b.board||b.model||'?';
  }catch(e){}
}
function setDefault(field,val){
  if(val===undefined||val===null)return;
  document.querySelectorAll('[id$="_'+field+'"]').forEach(el=>el.value=val);
}

// ── File manager ──────────────────────────────────────────────────────────────
async function loadFiles(){
  try{
    const[fs,pcaps]=await Promise.all([
      fetch('/api/fs').then(r=>r.json()),
      fetch('/api/pcaps').then(r=>r.json())
    ]);
    const pct=Math.round(fs.used/fs.total*100);
    document.getElementById('fs-fill').style.width=pct+'%';
    document.getElementById('fs-info').textContent=
      `${(fs.used/1024).toFixed(1)} KB used of ${(fs.total/1024).toFixed(1)} KB (${pct}% full)`;
    const tbody=document.getElementById('pcap-list');
    if(!pcaps.length){tbody.innerHTML='<tr><td colspan="3" class="empty">No PCAP captures yet</td></tr>';return;}
    tbody.innerHTML=pcaps.map(f=>`
      <tr>
        <td style="font-family:monospace;font-size:.8em">${f.name}</td>
        <td style="color:var(--dim)">${(f.size/1024).toFixed(1)} KB</td>
        <td style="display:flex;gap:6px">
          <button class="btn bs" style="padding:4px 10px;font-size:.72em" onclick="window.open('/api/pcap/dl?f=${encodeURIComponent(f.name)}')">⬇ DL</button>
          <button class="btn bd" style="padding:4px 10px;font-size:.72em" onclick="delPcap('${f.name}')">🗑 Del</button>
        </td>
      </tr>`).join('');
  }catch(e){toast('File load failed: '+e,'e')}
}

async function delPcap(name){
  if(!confirm('Delete '+name+'?'))return;
  try{
    const d=await(await fetch('/api/pcap/del',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'f='+encodeURIComponent(name)})).json();
    if(d.ok)toast('Deleted '+name);else toast('Delete failed','e');
    loadFiles();
  }catch(e){toast(e,'e')}
}

// ── CLI ───────────────────────────────────────────────────────────────────────
let cliHist=JSON.parse(localStorage.getItem('ch')||'[]'),cliHistIdx=cliHist.length;
let cliCmds=[],acIdx=-1;

const CLI_SIDEBAR=[
  {cat:'Core',cmds:[['help','show all commands'],['help <cmd>','detailed help'],['status','heap/uptime/active'],['modules','list tools'],['start <slug>','start a tool'],['stop','stop active tool'],['reboot','restart ESP32']]},
  {cat:'WiFi',cmds:[['scan','show cached APs'],['deauth <bssid>','deauth clients'],['beacon <ssid>','beacon flood'],['probe <ssid>','probe flood'],['handshake <bssid>','capture EAPOL'],['portal <ssid>','evil portal']]},
  {cat:'BLE',cmds:[['ble apple','Apple popups'],['ble android','Android FastPair'],['ble windows','Windows SwiftPair'],['ble rapidflood','random BLE flood']]},
  {cat:'Defense',cmds:[['aegis','IDS summary'],['clients','nearby devices'],['events','IDS event log'],['attackers','deauth attackers'],['channels','channel heatmap'],['trust <ssid> <mac> <ch>','set trusted AP']]},
  {cat:'Safety',cmds:[['safety','show limits'],['safety ast=30','auto-stop 30s'],['block <mac>','block BSSID'],['unblock','clear blocklist']]},
  {cat:'Diag',cmds:[['diag channel','channel map'],['diag heatmap','RSSI histogram'],['diag clients','client inventory'],['diag ies <mac>','decode IE fields']]},
];

function cliRenderSidebar(){
  const sb=document.getElementById('cli-sidebar');if(!sb)return;
  sb.innerHTML=CLI_SIDEBAR.map(g=>`
    <div class="csb-cat">${g.cat}</div>
    ${g.cmds.map(([c,d])=>`<a class="csb-cmd" onclick="cliInsert('${c.replace(/'/g,"\\'")}')"><b>${c.split(' ')[0]}</b>${c.includes(' ')?' <span style="color:#2a3a4a">'+c.substring(c.indexOf(' '))+'</span>':''}</a>`).join('')}
  `).join('');
}

function cliInsert(cmd){
  const inp=document.getElementById('cli-in');inp.value=cmd;inp.focus();
  inp.setSelectionRange(cmd.length,cmd.length);
}

function cliLine(txt,cls){
  const o=document.getElementById('cli-out');
  if(!txt)return;
  txt.split('\n').forEach(line=>{
    if(!line)return;
    const s=document.createElement('span');
    s.className='co '+(cls||autoClass(line));
    s.textContent=line;
    o.appendChild(s);
  });
  o.scrollTop=o.scrollHeight;
}

function autoClass(line){
  if(line.startsWith('doom> '))return 'in';
  if(line.startsWith('> =='))return 'hd';
  if(line.startsWith('>  Examples'))return 'ex';
  if(line.startsWith('>  Arguments'))return 'us';
  if(line.startsWith('>  '))return 'us';
  if(line.startsWith('> Usage:'))return 'us';
  if(line.startsWith('> ==='))return 'sep';
  if(line.startsWith('> ESP32 DOOM CLI')||line.startsWith('> Available'))return 'hd';
  if(line.startsWith('>    '))return 'ex';
  if(line.startsWith('> '))return 'ok';
  if(line.includes('error')||line.includes('Error')||line.includes('could not')||line.includes('unknown'))return 'er';
  return 'ok';
}

function cliBanner(){
  const lines=[
    '> ╔═══════════════════════════════╗',
    '> ║      E S P 3 2 - D O O M     ║',
    '> ╚═══════════════════════════════╝',
    '>  WiFi/BLE Security Research Toolkit',
    '>',
    '>  Type  help          → all commands',
    '>  Type  help deauth   → command detail',
    '>  Type  help wifi     → WiFi category',
    '>  Type  status        → system info',
    '>  Click any command → to run it',
    '>',
  ];
  lines.forEach(l=>cliLine(l,'hd'));
}

async function cliRun(raw){
  const cmd=raw.trim();
  cliLine('doom> '+raw,'in');
  if(!cmd)return;
  cliHist.push(cmd);cliHistIdx=cliHist.length;
  try{localStorage.setItem('ch',JSON.stringify(cliHist.slice(-200)));}catch(e){}
  if(cmd==='clear'){document.getElementById('cli-out').innerHTML='';cliBanner();return;}
  hideAC();
  try{
    const r=await fetch('/api/cli',{method:'POST',body:cmd,headers:{'Content-Type':'text/plain'}});
    const d=await r.json();
    if(d.ok){if(d.out)cliLine(d.out);}
    else cliLine((d.err||'error'),'er');
  }catch(e){cliLine('network error: '+e,'er');}
}

// ── Autocomplete ──────────────────────────────────────────────────────────────
function showAC(matches,inp){
  const box=document.getElementById('ac-box');
  if(!matches.length){hideAC();return;}
  box.innerHTML=matches.slice(0,10).map((m,i)=>`<div class="ac-item" data-i="${i}" onclick="pickAC('${m}')">${m}</div>`).join('');
  const r=inp.getBoundingClientRect(),pr=inp.closest('.cli-wrap-outer').getBoundingClientRect();
  box.style.left=(r.left-pr.left)+'px';
  box.style.bottom=(pr.bottom-r.top+2)+'px';
  box.style.display='block';acIdx=-1;
}
function hideAC(){document.getElementById('ac-box').style.display='none';acIdx=-1;}
function pickAC(val){
  const inp=document.getElementById('cli-in');
  const p=inp.value.split(' ');p[p.length-1]=val;inp.value=p.join(' ')+' ';
  inp.focus();hideAC();
}

document.addEventListener('DOMContentLoaded',()=>{
  const inp=document.getElementById('cli-in');
  if(!inp)return;

  inp.addEventListener('keydown',e=>{
    const box=document.getElementById('ac-box');
    const items=box.querySelectorAll('.ac-item');
    if(e.key==='Tab'||e.key==='ArrowDown'&&box.style.display!=='none'){
      e.preventDefault();
      if(box.style.display!=='none'&&items.length){
        acIdx=(acIdx+1)%items.length;
        items.forEach((el,i)=>el.classList.toggle('sel',i===acIdx));
        if(acIdx>=0)inp.value=items[acIdx].textContent+' ';
      } else {
        const p=inp.value.split(' '),last=p[p.length-1];
        const m=cliCmds.filter(x=>x.startsWith(last));
        showAC(m,inp);
      }
      return;
    }
    if(e.key==='ArrowUp'){
      e.preventDefault();
      if(box.style.display!=='none'&&items.length){
        acIdx=(acIdx-1+items.length)%items.length;
        items.forEach((el,i)=>el.classList.toggle('sel',i===acIdx));
        if(acIdx>=0)inp.value=items[acIdx].textContent+' ';
      } else {
        if(cliHistIdx>0){cliHistIdx--;inp.value=cliHist[cliHistIdx]||'';}
      }
      return;
    }
    if(e.key==='Escape'){hideAC();return;}
    if(e.key==='Enter'){
      e.preventDefault();const v=inp.value;inp.value='';hideAC();cliRun(v);return;
    }
  });

  inp.addEventListener('input',()=>{
    const p=inp.value.split(' '),last=p[p.length-1];
    if(!last){hideAC();return;}
    const m=cliCmds.filter(x=>x.startsWith(last)&&x!==last);
    showAC(m,inp);
  });

  inp.addEventListener('blur',()=>setTimeout(hideAC,150));

  fetch('/api/cli/complete?q=').then(r=>r.json()).then(d=>cliCmds=d).catch(()=>{});
  cliRenderSidebar();
  cliBanner();
  loadModules();
  loadBoard();
  setTimeout(pollStatus,1000);
});
</script>
</body></html>)HTML";

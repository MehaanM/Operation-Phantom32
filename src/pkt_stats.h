#pragma once
// pkt_stats.h — Global packet-rate statistics tracker with sparkline UI.
// Computes per-second and per-minute rates for all ingested 802.11 frames,
// broken down by type group: management, data, control. Tracks peak PPS,
// moving averages, and maintains ring-buffer history for sparkline rendering.
// Exposes /api/pktstats (JSON) and /pkt-stats (web UI with live sparklines).
// Prefix: pkts_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

#define PKTS_HISTORY_S  60   // 60 seconds of per-second rate history
#define PKTS_HISTORY_M  60   // 60 minutes of per-minute rate history

struct PktsSecBucket {
  uint32_t total;
  uint32_t mgmt;
  uint32_t data;
  uint32_t ctrl;
};

static struct {
  PktsSecBucket  sec_hist[PKTS_HISTORY_S];  // ring of 1s buckets
  uint32_t       min_hist[PKTS_HISTORY_M];  // ring of 1m totals
  uint8_t        sec_head;
  uint8_t        min_head;
  uint32_t       sec_start_ms;
  uint32_t       min_start_ms;
  uint64_t       grand_total;
  uint32_t       grand_mgmt;
  uint32_t       grand_data;
  uint32_t       grand_ctrl;
  uint32_t       cur_sec_total;
  uint32_t       cur_sec_mgmt;
  uint32_t       cur_sec_data;
  uint32_t       cur_sec_ctrl;
  uint32_t       cur_min_total;
  uint32_t       peak_pps;
  uint32_t       avg_pps_x100;    // * 100 for fixed-point
} g_pkts = {};

static inline void pkts_init() {
  g_pkts.sec_start_ms = millis();
  g_pkts.min_start_ms = millis();
}

// Call for every ingested frame: type_group 0=mgmt, 1=data, 2=ctrl
static inline void pkts_ingest(uint8_t type_group) {
  g_pkts.grand_total++;
  g_pkts.cur_sec_total++;
  g_pkts.cur_min_total++;
  if      (type_group == 0) { g_pkts.grand_mgmt++; g_pkts.cur_sec_mgmt++; }
  else if (type_group == 1) { g_pkts.grand_data++; g_pkts.cur_sec_data++; }
  else                      { g_pkts.grand_ctrl++; g_pkts.cur_sec_ctrl++; }
}

static inline void pkts_tick() {
  uint32_t now = millis();

  if (now - g_pkts.sec_start_ms >= 1000) {
    PktsSecBucket &b = g_pkts.sec_hist[g_pkts.sec_head % PKTS_HISTORY_S];
    b.total = g_pkts.cur_sec_total;
    b.mgmt  = g_pkts.cur_sec_mgmt;
    b.data  = g_pkts.cur_sec_data;
    b.ctrl  = g_pkts.cur_sec_ctrl;
    if (b.total > g_pkts.peak_pps) g_pkts.peak_pps = b.total;
    // Exponential moving average (alpha ≈ 0.1)
    g_pkts.avg_pps_x100 = (g_pkts.avg_pps_x100 * 9 + b.total * 100) / 10;
    g_pkts.sec_head = (g_pkts.sec_head + 1) % PKTS_HISTORY_S;
    g_pkts.cur_sec_total = g_pkts.cur_sec_mgmt = 0;
    g_pkts.cur_sec_data  = g_pkts.cur_sec_ctrl = 0;
    g_pkts.sec_start_ms  = now;
  }

  if (now - g_pkts.min_start_ms >= 60000) {
    g_pkts.min_hist[g_pkts.min_head % PKTS_HISTORY_M] = g_pkts.cur_min_total;
    g_pkts.min_head      = (g_pkts.min_head + 1) % PKTS_HISTORY_M;
    g_pkts.cur_min_total = 0;
    g_pkts.min_start_ms  = now;
  }
}

static const char PKTS_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Packet Stats — Phantom32</title>
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
.container{max-width:1000px;margin:0 auto;padding:20px 16px;}
h1{font-size:22px;font-weight:700;margin-bottom:4px;}
.subtitle{color:var(--muted);font-size:13px;margin-bottom:20px;}
.hero{display:grid;grid-template-columns:repeat(5,1fr);gap:10px;margin-bottom:16px;}
.stat{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:13px;}
.stat label{font-size:10px;color:var(--muted);text-transform:uppercase;}
.stat .val{font-size:22px;font-weight:800;color:var(--accent);margin-top:3px;}
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:18px;margin-bottom:14px;}
.section h3{font-size:12px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:14px;}
canvas{width:100%;border-radius:5px;}
.two-col{display:grid;grid-template-columns:1fr 1fr;gap:14px;margin-bottom:14px;}
@media(max-width:700px){.hero{grid-template-columns:repeat(3,1fr)};.two-col{grid-template-columns:1fr}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/pkt-stats" class="active">Pkt Stats</a>
    <a href="/phy-analysis">PHY</a>
    <a href="/frame-exporter">Frames</a>
  </nav>
</div>
<div class="container">
  <h1>Packet Rate Statistics</h1>
  <p class="subtitle">Live packet-per-second rates, exponential moving averages, and 60-second / 60-minute sparklines. Frame types: Management (beacons, probes, auth), Data (actual traffic), Control (ACK, RTS, CTS).</p>

  <div class="hero">
    <div class="stat"><label>Current PPS</label><div class="val" id="h-pps">0</div></div>
    <div class="stat"><label>Avg PPS</label><div class="val" id="h-avg">0</div></div>
    <div class="stat"><label>Peak PPS</label><div class="val" id="h-peak" style="color:var(--warn)">0</div></div>
    <div class="stat"><label>Total Frames</label><div class="val" id="h-total" style="font-size:14px">0</div></div>
    <div class="stat"><label>Uptime</label><div class="val" id="h-up" style="font-size:14px">—</div></div>
  </div>

  <div class="two-col">
    <div class="section">
      <h3>Mgmt Frames/s (60s)</h3>
      <canvas id="c-mgmt" height="80"></canvas>
    </div>
    <div class="section">
      <h3>Data Frames/s (60s)</h3>
      <canvas id="c-data" height="80"></canvas>
    </div>
  </div>

  <div class="section">
    <h3>Total Frames/s — last 60 seconds</h3>
    <canvas id="c-total" height="100"></canvas>
  </div>

  <div class="section">
    <h3>Frames/min — last 60 minutes</h3>
    <canvas id="c-min" height="80"></canvas>
  </div>
</div>

<script>
function reltime(s){if(s<60)return s+'s';if(s<3600)return Math.floor(s/60)+'m';return Math.floor(s/3600)+'h'+Math.floor((s%3600)/60)+'m';}

function drawSparkline(id, data, color) {
  const canvas = document.getElementById(id);
  if (!canvas || !data.length) return;
  const ctx = canvas.getContext('2d');
  const W = canvas.clientWidth || 400, H = canvas.height;
  canvas.width = W;
  ctx.clearRect(0,0,W,H);
  const max = Math.max(...data, 1);
  const bw = W / data.length;
  ctx.fillStyle = color + '22';
  ctx.beginPath();
  ctx.moveTo(0, H);
  data.forEach((v,i) => { ctx.lineTo(i*bw + bw/2, H - (v/max)*(H-4) - 2); });
  ctx.lineTo(W, H);
  ctx.closePath();
  ctx.fill();
  ctx.strokeStyle = color;
  ctx.lineWidth = 1.5;
  ctx.beginPath();
  data.forEach((v,i) => {
    const x = i*bw + bw/2, y = H - (v/max)*(H-4) - 2;
    i===0 ? ctx.moveTo(x,y) : ctx.lineTo(x,y);
  });
  ctx.stroke();
}

async function refresh() {
  const r = await fetch('/api/pktstats').then(x=>x.json()).catch(()=>({}));
  const up = Math.floor((Date.now() - (r.boot_ms||Date.now())) / 1000);
  document.getElementById('h-pps').textContent   = r.cur_pps||0;
  document.getElementById('h-avg').textContent   = ((r.avg_pps_x100||0)/100).toFixed(1);
  document.getElementById('h-peak').textContent  = r.peak_pps||0;
  document.getElementById('h-total').textContent = (r.grand_total||0).toLocaleString();
  document.getElementById('h-up').textContent    = reltime(up||0);

  drawSparkline('c-mgmt',  (r.sec_hist_mgmt||[]),  '#00d4ff');
  drawSparkline('c-data',  (r.sec_hist_data||[]),  '#22c55e');
  drawSparkline('c-total', (r.sec_hist||[]),        '#00d4ff');
  drawSparkline('c-min',   (r.min_hist||[]),        '#f59e0b');
}

refresh();
setInterval(refresh, 1000);
</script>
</body>
</html>
)rawhtml";

static inline void pkts_routes() {
  server.on("/pkt-stats", HTTP_GET, []() {
    server.send_P(200, "text/html", PKTS_PAGE);
  });

  server.on("/api/pktstats", HTTP_GET, []() {
    String out;
    out.reserve(2048);
    out += "{\"grand_total\":";   out += (uint32_t)g_pkts.grand_total;
    out += ",\"grand_mgmt\":";    out += g_pkts.grand_mgmt;
    out += ",\"grand_data\":";    out += g_pkts.grand_data;
    out += ",\"grand_ctrl\":";    out += g_pkts.grand_ctrl;
    out += ",\"peak_pps\":";      out += g_pkts.peak_pps;
    out += ",\"cur_pps\":";       out += g_pkts.cur_sec_total;
    out += ",\"avg_pps_x100\":";  out += g_pkts.avg_pps_x100;
    out += ",\"boot_ms\":0";      // placeholder; real boot_ms comes from main
    out += ",\"sec_hist\":[";
    for (uint8_t i = 0; i < PKTS_HISTORY_S; i++) {
      if (i>0) out += ',';
      out += g_pkts.sec_hist[(g_pkts.sec_head + i) % PKTS_HISTORY_S].total;
    }
    out += "],\"sec_hist_mgmt\":[";
    for (uint8_t i = 0; i < PKTS_HISTORY_S; i++) {
      if (i>0) out += ',';
      out += g_pkts.sec_hist[(g_pkts.sec_head + i) % PKTS_HISTORY_S].mgmt;
    }
    out += "],\"sec_hist_data\":[";
    for (uint8_t i = 0; i < PKTS_HISTORY_S; i++) {
      if (i>0) out += ',';
      out += g_pkts.sec_hist[(g_pkts.sec_head + i) % PKTS_HISTORY_S].data;
    }
    out += "],\"min_hist\":[";
    for (uint8_t i = 0; i < PKTS_HISTORY_M; i++) {
      if (i>0) out += ',';
      out += g_pkts.min_hist[(g_pkts.min_head + i) % PKTS_HISTORY_M];
    }
    out += "]}";
    server.send(200, "application/json", out);
  });
}

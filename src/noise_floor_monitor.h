#pragma once
// noise_floor_monitor.h — Per-channel RF noise floor estimation and tracking.
// Estimates background noise level per channel by tracking the lowest RSSI
// values seen over rolling windows, flagging channels with elevated noise,
// and maintaining a 60-sample noise history for each 2.4GHz channel (1-14).
// Routes: /api/nfm/all, /api/nfm/clear, /noise-monitor (UI). Prefix: nfm_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

#define NFM_CHANNELS 14
#define NFM_HISTORY 60   // samples per channel
#define NFM_WINDOW_S 10  // noise estimation window in seconds

struct NfmChannel {
  int8_t noise_est;          // current estimated noise floor (dBm)
  int8_t noise_min;          // historical minimum noise (quietest)
  int8_t noise_max;          // historical maximum noise (noisiest)
  int8_t hist[NFM_HISTORY];  // rolling noise history
  uint8_t hist_head;
  uint32_t sample_count;
  uint32_t window_low_sum;  // sum of lowest 10% RSSIs in window
  uint16_t window_low_cnt;
  uint32_t last_update_ms;
  bool is_noisy;  // flagged above threshold
};

static struct {
  NfmChannel ch[NFM_CHANNELS];
  int8_t noise_threshold;  // above this = noisy (default -85 dBm)
  bool enabled;
} g_nfm = {.noise_threshold = -85, .enabled = true};

static inline void nfm_init() {
  for (uint8_t c = 0; c < NFM_CHANNELS; c++) {
    g_nfm.ch[c].noise_est = -100;
    g_nfm.ch[c].noise_min = -30;
    g_nfm.ch[c].noise_max = -100;
    for (uint8_t h = 0; h < NFM_HISTORY; h++)
      g_nfm.ch[c].hist[h] = -100;
  }
}

// Called per received frame; use lowest-percentile RSSI as noise estimate
static inline void nfm_ingest(uint8_t ch, int8_t rssi) {
  if (!g_nfm.enabled || ch < 1 || ch > NFM_CHANNELS)
    return;
  NfmChannel &c = g_nfm.ch[ch - 1];

  // Keep track of very-low RSSI readings for noise estimation
  // The noise floor is the median of the bottom 10% of RSSI readings
  if (rssi < -75) {
    c.window_low_sum += (-rssi);  // store absolute value
    c.window_low_cnt++;
  }

  c.sample_count++;

  // Rotate window every NFM_WINDOW_S seconds
  uint32_t now = millis();
  if (now - c.last_update_ms >= (uint32_t)NFM_WINDOW_S * 1000) {
    if (c.window_low_cnt > 0) {
      int8_t est = -(int8_t)(c.window_low_sum / c.window_low_cnt);
      c.noise_est = est;
      if (est < c.noise_min)
        c.noise_min = est;
      if (est > c.noise_max)
        c.noise_max = est;
      c.is_noisy = (est > g_nfm.noise_threshold);
      c.hist[c.hist_head % NFM_HISTORY] = est;
      c.hist_head = (c.hist_head + 1) % NFM_HISTORY;
    }
    c.window_low_sum = 0;
    c.window_low_cnt = 0;
    c.last_update_ms = now;
  }
}

static const char NFM_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Noise Floor — Phantom32</title>
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
.hero{display:grid;grid-template-columns:repeat(4,1fr);gap:10px;margin-bottom:16px;}
.stat{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:13px;}
.stat label{font-size:10px;color:var(--muted);text-transform:uppercase;}
.stat .val{font-size:22px;font-weight:800;color:var(--accent);margin-top:3px;}
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:18px;margin-bottom:14px;}
.section h3{font-size:12px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:14px;}
.ch-grid{display:grid;grid-template-columns:repeat(7,1fr);gap:8px;}
.ch-card{background:var(--bg3);border:1px solid var(--border);border-radius:6px;padding:12px 8px;text-align:center;}
.ch-card.noisy{border-color:var(--danger);}
.ch-num{font-size:9px;color:var(--muted);text-transform:uppercase;margin-bottom:4px;}
.ch-noise{font-size:16px;font-weight:800;margin-bottom:2px;}
.ch-range{font-size:8px;color:var(--muted);}
.noise-bar{height:4px;border-radius:2px;margin-top:6px;background:var(--border);}
.noise-fill{height:100%;border-radius:2px;}
table{width:100%;border-collapse:collapse;font-size:10px;}
th{text-align:left;padding:7px 10px;border-bottom:2px solid var(--border);color:var(--muted);font-size:9px;text-transform:uppercase;}
td{padding:6px 10px;border-bottom:1px solid var(--border);}
tr:hover td{background:var(--bg3);}
.mini-hist{display:flex;align-items:flex-end;gap:1px;height:16px;}
.mh-bar{width:3px;min-height:1px;border-radius:1px 1px 0 0;}
.noisy-pill{padding:2px 6px;border-radius:3px;font-size:8px;font-weight:700;background:rgba(239,68,68,.12);color:var(--danger);}
.ok-pill{padding:2px 6px;border-radius:3px;font-size:8px;font-weight:700;background:rgba(34,197,94,.12);color:var(--success);}
.btn{padding:7px 13px;border:none;border-radius:5px;font-size:11px;font-weight:600;cursor:pointer;}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.action-row{display:flex;gap:8px;margin-top:12px;}
@media(max-width:700px){.ch-grid{grid-template-columns:repeat(4,1fr)};.hero{grid-template-columns:repeat(2,1fr)}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/noise-monitor" class="active">Noise Floor</a>
    <a href="/phy-analysis">PHY</a>
    <a href="/channel-analyzer">Channels</a>
  </nav>
</div>
<div class="container">
  <h1>RF Noise Floor Monitor</h1>
  <p class="subtitle">Per-channel background noise level estimation using the bottom-percentile RSSI of all received frames. Noisy channels (above -85 dBm noise floor) are flagged, with 60-sample rolling history per channel.</p>

  <div class="hero">
    <div class="stat"><label>Cleanest CH</label><div class="val" id="h-clean" style="color:var(--success)">—</div></div>
    <div class="stat"><label>Noisiest CH</label><div class="val" id="h-noisy" style="color:var(--danger)">—</div></div>
    <div class="stat"><label>Noisy Channels</label><div class="val" id="h-noisy-cnt" style="color:var(--warn)">0</div></div>
    <div class="stat"><label>Threshold</label><div class="val" id="h-thresh" style="font-size:14px">-85 dBm</div></div>
  </div>

  <div class="section">
    <h3>Channel Noise Overview</h3>
    <div class="ch-grid" id="ch-grid"></div>
  </div>

  <div class="section">
    <h3>Detailed Table</h3>
    <table>
      <thead><tr>
        <th>CH</th><th>Noise Est.</th><th>Min Noise</th><th>Max Noise</th>
        <th>Samples</th><th>Status</th><th>History</th>
      </tr></thead>
      <tbody id="ch-body"></tbody>
    </table>
    <div class="action-row">
      <button class="btn btn-ghost" onclick="clearData()">Clear</button>
    </div>
  </div>
</div>

<script>
function noiseColor(n) {
  if (n <= -90) return 'var(--success)';
  if (n <= -80) return 'var(--warn)';
  return 'var(--danger)';
}

function noisePct(n) {
  // Map -100 (quietest) to -60 (loudest) → 0–100%
  return Math.max(0, Math.min(100, (n + 100) / 40 * 100));
}

function miniHist(hist) {
  if (!hist || !hist.length) return '';
  const vals = hist.filter(v => v < -30);
  if (!vals.length) return '';
  const min = Math.min(...vals), max = Math.max(...vals);
  const range = max - min || 1;
  return `<div class="mini-hist">` +
    hist.map(v => v >= -30 ? `<div class="mh-bar" style="height:1px;background:var(--border)"></div>` :
      `<div class="mh-bar" style="height:${Math.max(2,(v-min)/range*16+1)}px;background:${noiseColor(v)}"></div>`).join('') +
    `</div>`;
}

async function refresh() {
  const r = await fetch('/api/nfm/all').then(x=>x.json()).catch(()=>({}));
  const chs = r.channels || [];
  if (!chs.length) return;

  const active = chs.filter(c => c.samples > 0);
  const cleanest = active.reduce((a,b) => b.noise_est < a.noise_est ? b : a, active[0]||{ch:0,noise_est:-100});
  const noisiest = active.reduce((a,b) => b.noise_est > a.noise_est ? b : a, active[0]||{ch:0,noise_est:-100});
  const noisyCnt = active.filter(c => c.is_noisy).length;

  document.getElementById('h-clean').textContent   = cleanest.ch ? 'CH'+cleanest.ch : '—';
  document.getElementById('h-noisy').textContent   = noisiest.ch ? 'CH'+noisiest.ch : '—';
  document.getElementById('h-noisy-cnt').textContent = noisyCnt;

  document.getElementById('ch-grid').innerHTML = chs.map(c =>
    `<div class="ch-card ${c.is_noisy?'noisy':''}">
      <div class="ch-num">CH${c.ch}</div>
      <div class="ch-noise" style="color:${noiseColor(c.noise_est)}">${c.noise_est}</div>
      <div class="ch-range" style="color:var(--muted)">${c.noise_min}/${c.noise_max}</div>
      <div class="noise-bar"><div class="noise-fill" style="width:${noisePct(c.noise_est)}%;background:${noiseColor(c.noise_est)}"></div></div>
    </div>`).join('');

  document.getElementById('ch-body').innerHTML = chs.map(c =>
    `<tr>
      <td><b>${c.ch}</b></td>
      <td style="color:${noiseColor(c.noise_est)};font-weight:700">${c.noise_est} dBm</td>
      <td style="color:var(--success)">${c.noise_min} dBm</td>
      <td style="color:var(--danger)">${c.noise_max} dBm</td>
      <td style="color:var(--muted)">${c.samples.toLocaleString()}</td>
      <td>${c.is_noisy?'<span class="noisy-pill">NOISY</span>':'<span class="ok-pill">Clean</span>'}</td>
      <td>${miniHist(c.hist)}</td>
    </tr>`).join('');
}

async function clearData() {
  await fetch('/api/nfm/clear', {method:'POST'});
  refresh();
}

refresh();
setInterval(refresh, 3000);
</script>
</body>
</html>
)rawhtml";

static inline void nfm_routes() {
  server.on("/noise-monitor", HTTP_GET, []() { server.send_P(200, "text/html", NFM_PAGE); });

  server.on("/api/nfm/all", HTTP_GET, []() {
    String out;
    out.reserve(2048);
    out += "{\"threshold\":";
    out += g_nfm.noise_threshold;
    out += ",\"channels\":[";
    for (uint8_t c = 0; c < NFM_CHANNELS; c++) {
      const NfmChannel &ch = g_nfm.ch[c];
      if (c > 0)
        out += ',';
      out += "{\"ch\":";
      out += (c + 1);
      out += ",\"noise_est\":";
      out += ch.noise_est;
      out += ",\"noise_min\":";
      out += ch.noise_min;
      out += ",\"noise_max\":";
      out += ch.noise_max;
      out += ",\"samples\":";
      out += ch.sample_count;
      out += ",\"is_noisy\":";
      out += (ch.is_noisy ? "true" : "false");
      out += ",\"hist\":[";
      for (uint8_t h = 0; h < NFM_HISTORY; h++) {
        if (h > 0)
          out += ',';
        out += ch.hist[(ch.hist_head + h) % NFM_HISTORY];
      }
      out += "]}";
    }
    out += "]}";
    server.send(200, "application/json", out);
  });

  server.on("/api/nfm/clear", HTTP_POST, []() {
    for (uint8_t c = 0; c < NFM_CHANNELS; c++) {
      NfmChannel &ch = g_nfm.ch[c];
      ch.noise_est = -100;
      ch.noise_min = -30;
      ch.noise_max = -100;
      ch.sample_count = 0;
      ch.window_low_sum = 0;
      ch.window_low_cnt = 0;
      ch.is_noisy = false;
      ch.hist_head = 0;
      for (uint8_t h = 0; h < NFM_HISTORY; h++)
        ch.hist[h] = -100;
    }
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

#pragma once
// frame_exporter.h — 802.11 Frame Logger and Exporter.
// Maintains a ring buffer of captured frame summaries with structured JSON export,
// field decoding, type classification, and CSV download via web API.
// Routes: /api/fexp/frames, /api/fexp/clear, /api/fexp/csv, /frame-exporter (UI)
// Prefix: fexp_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

#define FEXP_MAX_FRAMES 256
#define FEXP_SSID_LEN 33
#define FEXP_MAC_LEN 18

// Frame type classification
enum FexpType : uint8_t {
  FEXP_T_BEACON = 0,
  FEXP_T_PROBE_REQ = 1,
  FEXP_T_PROBE_RESP = 2,
  FEXP_T_AUTH = 3,
  FEXP_T_DEAUTH = 4,
  FEXP_T_ASSOC_REQ = 5,
  FEXP_T_ASSOC_RESP = 6,
  FEXP_T_DISASSOC = 7,
  FEXP_T_DATA = 8,
  FEXP_T_QOSDATA = 9,
  FEXP_T_NULL = 10,
  FEXP_T_CTRL_ACK = 11,
  FEXP_T_CTRL_RTS = 12,
  FEXP_T_CTRL_CTS = 13,
  FEXP_T_EAPOL = 14,
  FEXP_T_OTHER = 15,
  FEXP_T_COUNT = 16
};

static const char *const FEXP_TYPE_NAMES[FEXP_T_COUNT] = {
    "Beacon", "ProbeReq", "ProbeResp", "Auth", "Deauth", "AssocReq", "AssocResp", "Disassoc",
    "Data",   "QoSData",  "Null",      "ACK",  "RTS",    "CTS",      "EAPOL",     "Other"};

struct FexpFrame {
  uint32_t ts_ms;            // millis() capture time
  uint8_t ch;                // channel
  int8_t rssi;               // signal strength
  FexpType type;             // frame classification
  uint8_t src[6];            // source MAC
  uint8_t dst[6];            // dest MAC
  uint8_t bssid[6];          // BSSID (management frames)
  char ssid[FEXP_SSID_LEN];  // SSID (beacons/probes)
  uint16_t seq_num;          // 802.11 sequence number
  uint8_t retry : 1;         // retry bit
  uint8_t to_ds : 1;         // to-DS bit
  uint8_t from_ds : 1;       // from-DS bit
  uint8_t fcs_ok : 1;        // FCS validity
  uint8_t protected_f : 1;   // protected frame bit
  uint16_t len;              // frame length in bytes
};

static struct {
  FexpFrame buf[FEXP_MAX_FRAMES];
  uint16_t head;                    // ring write pointer
  uint16_t total;                   // total frames ingested
  uint32_t per_type[FEXP_T_COUNT];  // cumulative per-type counts
  bool running;
} g_fexp = {};

// MAC as colon-separated string into out[18]
static inline void fexp_mac_str(const uint8_t *m, char *out) {
  snprintf(out, FEXP_MAC_LEN, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

// Classify 802.11 frame type/subtype from FC bytes
static inline FexpType fexp_classify(uint8_t fc0, uint8_t fc1, const uint8_t *body, uint16_t len) {
  uint8_t type = (fc0 >> 2) & 0x03;
  uint8_t subtype = (fc0 >> 4) & 0x0F;
  if (type == 0) {  // Management
    switch (subtype) {
      case 8:
        return FEXP_T_BEACON;
      case 4:
        return FEXP_T_PROBE_REQ;
      case 5:
        return FEXP_T_PROBE_RESP;
      case 11:
        return FEXP_T_AUTH;
      case 12:
        return FEXP_T_DEAUTH;
      case 0:
        return FEXP_T_ASSOC_REQ;
      case 1:
        return FEXP_T_ASSOC_RESP;
      case 10:
        return FEXP_T_DISASSOC;
      default:
        return FEXP_T_OTHER;
    }
  } else if (type == 1) {  // Control
    switch (subtype) {
      case 13:
        return FEXP_T_ACK;
      case 11:
        return FEXP_T_RTS;
      case 12:
        return FEXP_T_CTS;
      default:
        return FEXP_T_OTHER;
    }
  } else {  // Data
    if (len > 32) {
      // Check LLC/SNAP for EAPOL (0x888E)
      const uint8_t *llc = body + 24;
      if (len > 32 && llc[0] == 0xAA && llc[1] == 0xAA && llc[2] == 0x03 && llc[6] == 0x88 &&
          llc[7] == 0x8E)
        return FEXP_T_EAPOL;
    }
    if (subtype == 4 || subtype == 12)
      return FEXP_T_NULL;
    if (subtype & 8)
      return FEXP_T_QOSDATA;
    return FEXP_T_DATA;
  }
}

// Ingest a raw frame (called from promiscuous callback)
static inline void fexp_ingest(const uint8_t *payload, uint16_t len, int8_t rssi, uint8_t ch) {
  if (!g_fexp.running || len < 24)
    return;
  FexpFrame &f = g_fexp.buf[g_fexp.head % FEXP_MAX_FRAMES];
  memset(&f, 0, sizeof(f));

  uint8_t fc0 = payload[0], fc1 = payload[1];
  uint8_t type = (fc0 >> 2) & 0x03;

  f.ts_ms = millis();
  f.ch = ch;
  f.rssi = rssi;
  f.retry = (fc1 & 0x08) ? 1 : 0;
  f.to_ds = (fc1 & 0x01) ? 1 : 0;
  f.from_ds = (fc1 & 0x02) ? 1 : 0;
  f.protected_f = (fc1 & 0x40) ? 1 : 0;
  f.fcs_ok = 1;
  f.len = len;
  f.seq_num = payload[22] | (payload[23] << 8);
  f.seq_num = (f.seq_num >> 4) & 0x0FFF;
  f.type = fexp_classify(fc0, fc1, payload, len);

  // Extract addresses based on frame type
  memcpy(f.dst, payload + 4, 6);
  memcpy(f.src, payload + 10, 6);
  memcpy(f.bssid, payload + 16, 6);

  // Extract SSID from management frames (beacon, probe req/resp, assoc req)
  if (type == 0 && len > 36) {
    uint8_t stype = (fc0 >> 4) & 0x0F;
    uint16_t ie_off = (stype == 8 || stype == 5) ? 36 : 24;
    if (ie_off + 2 <= len) {
      uint16_t pos = ie_off;
      while (pos + 2 <= len) {
        uint8_t ie_id = payload[pos];
        uint8_t ie_len = payload[pos + 1];
        if (pos + 2 + ie_len > len)
          break;
        if (ie_id == 0 && ie_len < FEXP_SSID_LEN) {
          memcpy(f.ssid, payload + pos + 2, ie_len);
          f.ssid[ie_len] = '\0';
          break;
        }
        pos += 2 + ie_len;
      }
    }
  }

  g_fexp.per_type[f.type]++;
  g_fexp.head = (g_fexp.head + 1) % FEXP_MAX_FRAMES;
  g_fexp.total++;
}

// Clear ring buffer
static inline void fexp_clear() {
  memset(&g_fexp, 0, sizeof(g_fexp));
  g_fexp.running = true;
}

// Append compact JSON object for one frame into String
static inline void fexp_frame_json(const FexpFrame &f, String &out, bool first) {
  char sm[FEXP_MAC_LEN], dm[FEXP_MAC_LEN], bm[FEXP_MAC_LEN];
  fexp_mac_str(f.src, sm);
  fexp_mac_str(f.dst, dm);
  fexp_mac_str(f.bssid, bm);
  if (!first)
    out += ',';
  out += "{\"ts\":";
  out += f.ts_ms;
  out += ",\"ch\":";
  out += f.ch;
  out += ",\"rssi\":";
  out += f.rssi;
  out += ",\"type\":\"";
  out += FEXP_TYPE_NAMES[f.type];
  out += '"';
  out += ",\"src\":\"";
  out += sm;
  out += '"';
  out += ",\"dst\":\"";
  out += dm;
  out += '"';
  out += ",\"bssid\":\"";
  out += bm;
  out += '"';
  if (f.ssid[0]) {
    out += ",\"ssid\":\"";
    out += f.ssid;
    out += '"';
  }
  out += ",\"seq\":";
  out += f.seq_num;
  out += ",\"len\":";
  out += f.len;
  out += ",\"flags\":\"";
  if (f.retry)
    out += 'R';
  if (f.to_ds)
    out += 'T';
  if (f.from_ds)
    out += 'F';
  if (f.protected_f)
    out += 'P';
  out += '"';
  out += '}';
}

static const char FEXP_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Frame Exporter — Phantom32</title>
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
.container{max-width:1400px;margin:0 auto;padding:20px 16px;}
h1{font-size:22px;font-weight:700;margin-bottom:4px;}
.subtitle{color:var(--muted);font-size:13px;margin-bottom:20px;}
.hero{display:grid;grid-template-columns:repeat(5,1fr);gap:10px;margin-bottom:16px;}
.stat{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:13px;}
.stat label{font-size:10px;color:var(--muted);text-transform:uppercase;}
.stat .val{font-size:22px;font-weight:800;color:var(--accent);margin-top:3px;}
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:18px;margin-bottom:14px;}
.section h3{font-size:12px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:14px;}
.filter-row{display:flex;gap:8px;margin-bottom:12px;flex-wrap:wrap;align-items:center;}
.filter-input{background:var(--bg3);border:1px solid var(--border);border-radius:5px;color:var(--text);padding:7px 12px;font-size:11px;outline:none;min-width:180px;}
.filter-input:focus{border-color:var(--accent);}
.filter-sel{background:var(--bg3);border:1px solid var(--border);border-radius:5px;color:var(--text);padding:7px 10px;font-size:11px;outline:none;}
table{width:100%;border-collapse:collapse;font-size:10px;}
th{text-align:left;padding:7px 10px;border-bottom:2px solid var(--border);color:var(--muted);font-size:9px;text-transform:uppercase;cursor:pointer;user-select:none;}
th:hover{color:var(--text);}
td{padding:5px 10px;border-bottom:1px solid var(--border);font-family:monospace;}
td.nm{font-family:'Inter',system-ui,sans-serif;}
tr:hover td{background:var(--bg3);}
.type-badge{padding:2px 6px;border-radius:3px;font-size:8px;font-weight:700;}
.t-Beacon{background:rgba(0,212,255,.12);color:var(--accent);}
.t-ProbeReq{background:rgba(129,140,248,.12);color:#818cf8;}
.t-ProbeResp{background:rgba(129,140,248,.08);color:#a5b4fc;}
.t-Auth{background:rgba(34,197,94,.1);color:var(--success);}
.t-Deauth{background:rgba(239,68,68,.12);color:var(--danger);}
.t-AssocReq{background:rgba(34,197,94,.08);color:#86efac;}
.t-AssocResp{background:rgba(34,197,94,.06);color:#86efac;}
.t-Disassoc{background:rgba(239,68,68,.08);color:#fca5a5;}
.t-Data{background:rgba(245,158,11,.08);color:var(--warn);}
.t-QoSData{background:rgba(245,158,11,.12);color:#fbbf24;}
.t-EAPOL{background:rgba(239,68,68,.2);color:#ff6b6b;border:1px solid rgba(239,68,68,.3);}
.t-Other{background:rgba(100,116,139,.1);color:var(--muted);}
.flag-bit{display:inline-block;padding:0 3px;border-radius:2px;font-size:8px;font-weight:700;margin-right:1px;}
.f-R{background:rgba(239,68,68,.1);color:var(--danger);}
.f-T{background:rgba(0,212,255,.08);color:var(--accent);}
.f-F{background:rgba(34,197,94,.08);color:var(--success);}
.f-P{background:rgba(129,140,248,.1);color:#818cf8;}
.btn{padding:7px 13px;border:none;border-radius:5px;font-size:11px;font-weight:600;cursor:pointer;}
.btn-primary{background:var(--accent);color:#0a0c10;}
.btn-primary:hover{filter:brightness(1.15);}
.btn-ghost{background:transparent;border:1px solid var(--border);color:var(--text);}
.btn-ghost:hover{background:var(--bg3);}
.btn-success{background:rgba(34,197,94,.12);color:var(--success);border:1px solid var(--success);}
.action-row{display:flex;gap:8px;margin-top:12px;align-items:center;flex-wrap:wrap;}
.ts-label{font-size:9px;color:var(--muted);margin-left:auto;}
.type-chart{display:flex;flex-wrap:wrap;gap:6px;}
.tc-item{background:var(--bg3);border:1px solid var(--border);border-radius:5px;padding:8px 12px;min-width:90px;}
.tc-cnt{font-size:16px;font-weight:800;color:var(--accent);}
.tc-name{font-size:9px;color:var(--muted);text-transform:uppercase;margin-top:1px;}
.rssi-pill{padding:1px 5px;border-radius:3px;font-size:9px;font-weight:700;}
@media(max-width:900px){.hero{grid-template-columns:repeat(3,1fr)}}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/frame-exporter" class="active">Frames</a>
    <a href="/phy-analysis">PHY</a>
    <a href="/clients">Clients</a>
    <a href="/rate-tracker">Rates</a>
  </nav>
</div>
<div class="container">
  <h1>Frame Exporter</h1>
  <p class="subtitle">Ring-buffer of the last 256 captured 802.11 frames with field decoding, type classification, flag display, and CSV export. Live refresh every 2 seconds.</p>

  <div class="hero">
    <div class="stat"><label>Total Captured</label><div class="val" id="h-total">0</div></div>
    <div class="stat"><label>Management</label><div class="val" id="h-mgmt" style="color:var(--accent)">0</div></div>
    <div class="stat"><label>Data</label><div class="val" id="h-data" style="color:var(--warn)">0</div></div>
    <div class="stat"><label>Control</label><div class="val" id="h-ctrl" style="color:var(--muted)">0</div></div>
    <div class="stat"><label>EAPOL</label><div class="val" id="h-eapol" style="color:var(--danger)">0</div></div>
  </div>

  <div class="section">
    <h3>Frame Type Distribution</h3>
    <div class="type-chart" id="type-chart"></div>
  </div>

  <div class="section">
    <h3>Captured Frames (last 256)</h3>
    <div class="filter-row">
      <input class="filter-input" id="filter-mac" placeholder="Filter by MAC / SSID…" oninput="applyFilter()">
      <select class="filter-sel" id="filter-type" onchange="applyFilter()">
        <option value="">All Types</option>
        <option value="Beacon">Beacon</option>
        <option value="ProbeReq">Probe Req</option>
        <option value="ProbeResp">Probe Resp</option>
        <option value="Auth">Auth</option>
        <option value="Deauth">Deauth</option>
        <option value="EAPOL">EAPOL</option>
        <option value="Data">Data</option>
        <option value="QoSData">QoS Data</option>
      </select>
      <select class="filter-sel" id="filter-ch" onchange="applyFilter()">
        <option value="">All Channels</option>
      </select>
    </div>
    <div style="overflow-x:auto">
    <table>
      <thead><tr>
        <th>Time</th><th>Type</th><th>CH</th><th>RSSI</th>
        <th>Source MAC</th><th>Dest MAC</th><th>BSSID</th>
        <th>SSID</th><th>Seq</th><th>Len</th><th>Flags</th>
      </tr></thead>
      <tbody id="frame-body"></tbody>
    </table>
    </div>
    <div class="action-row">
      <button class="btn btn-success" onclick="exportCsv()">↓ CSV</button>
      <button class="btn btn-ghost" onclick="clearFrames()">Clear</button>
      <button class="btn btn-ghost" onclick="pauseToggle()" id="pause-btn">Pause</button>
      <span class="ts-label">Buf: <span id="buf-use">0</span>/256 | Total: <span id="ts-total">0</span> | Updated: <span id="ts-time">—</span></span>
    </div>
  </div>
</div>

<script>
let allFrames = [], paused = false;

function rssiColor(r) {
  if (r >= -55) return 'var(--success)';
  if (r >= -70) return 'var(--warn)';
  return 'var(--danger)';
}

function ageStr(ms) {
  const s = Math.floor((Date.now() - ms) / 1000);
  if (s < 60) return s + 's';
  return Math.floor(s/60) + 'm' + (s%60) + 's';
}

function flagBits(flags) {
  if (!flags) return '';
  return [...flags].map(c => `<span class="flag-bit f-${c}">${c}</span>`).join('');
}

function populateChFilter(frames) {
  const chs = [...new Set(frames.map(f => f.ch))].sort((a,b)=>a-b);
  const sel = document.getElementById('filter-ch');
  const cur = sel.value;
  sel.innerHTML = '<option value="">All Channels</option>' +
    chs.map(c => `<option value="${c}" ${cur==c?'selected':''}>${c}</option>`).join('');
}

function renderTable(frames) {
  const mac_f  = document.getElementById('filter-mac').value.toLowerCase();
  const type_f = document.getElementById('filter-type').value;
  const ch_f   = document.getElementById('filter-ch').value;

  const rows = frames.filter(f => {
    if (type_f && f.type !== type_f) return false;
    if (ch_f && f.ch != ch_f) return false;
    if (mac_f) {
      const s = (f.src+f.dst+f.bssid+(f.ssid||'')).toLowerCase();
      if (!s.includes(mac_f)) return false;
    }
    return true;
  });

  document.getElementById('frame-body').innerHTML = rows.length
    ? rows.slice(0, 200).map(f =>
        `<tr>
          <td>${ageStr(f.ts)}</td>
          <td><span class="type-badge t-${f.type}">${f.type}</span></td>
          <td>${f.ch}</td>
          <td><span class="rssi-pill" style="background:${rssiColor(f.rssi)}22;color:${rssiColor(f.rssi)}">${f.rssi}</span></td>
          <td>${f.src}</td>
          <td style="color:var(--muted)">${f.dst}</td>
          <td style="color:var(--muted)">${f.bssid}</td>
          <td class="nm" style="color:var(--text);max-width:120px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap">${f.ssid||''}</td>
          <td>${f.seq}</td>
          <td style="color:var(--muted)">${f.len}</td>
          <td>${flagBits(f.flags)}</td>
        </tr>`).join('')
    : '<tr><td colspan="11" style="text-align:center;color:var(--muted);padding:16px">No frames captured yet.</td></tr>';
}

function applyFilter() { renderTable(allFrames); }

function pauseToggle() {
  paused = !paused;
  document.getElementById('pause-btn').textContent = paused ? 'Resume' : 'Pause';
}

async function refresh() {
  if (paused) return;
  const r = await fetch('/api/fexp/frames').then(x=>x.json()).catch(()=>({}));
  allFrames = r.frames || [];
  const total = r.total || 0;

  document.getElementById('h-total').textContent = total.toLocaleString();
  document.getElementById('h-mgmt').textContent  = (r.mgmt||0).toLocaleString();
  document.getElementById('h-data').textContent  = (r.data_cnt||0).toLocaleString();
  document.getElementById('h-ctrl').textContent  = (r.ctrl||0).toLocaleString();
  document.getElementById('h-eapol').textContent = (r.eapol||0).toLocaleString();
  document.getElementById('buf-use').textContent = allFrames.length;
  document.getElementById('ts-total').textContent = total.toLocaleString();
  document.getElementById('ts-time').textContent  = new Date().toLocaleTimeString();

  // Type distribution chart
  const types = r.per_type || {};
  document.getElementById('type-chart').innerHTML =
    Object.entries(types).filter(([,v])=>v>0).sort(([,a],[,b])=>b-a).map(([k,v]) =>
      `<div class="tc-item"><div class="tc-cnt">${v.toLocaleString()}</div><div class="tc-name">${k}</div></div>`
    ).join('');

  populateChFilter(allFrames);
  renderTable(allFrames);
}

function exportCsv() {
  const hdr = 'ts_ms,ch,rssi,type,src,dst,bssid,ssid,seq,len,flags\n';
  const rows = allFrames.map(f =>
    [f.ts,f.ch,f.rssi,f.type,f.src,f.dst,f.bssid,f.ssid||'',f.seq,f.len,f.flags||''].join(',')
  ).join('\n');
  const blob = new Blob([hdr+rows], {type:'text/csv'});
  const a = document.createElement('a');
  a.href = URL.createObjectURL(blob);
  a.download = 'phantom32_frames_' + Date.now() + '.csv';
  a.click();
}

async function clearFrames() {
  await fetch('/api/fexp/clear', {method:'POST'});
  allFrames = [];
  renderTable([]);
}

refresh();
setInterval(refresh, 2000);
</script>
</body>
</html>
)rawhtml";

inline void fexp_routes() {
  g_fexp.running = true;

  server.on("/frame-exporter", HTTP_GET, []() { server.send_P(200, "text/html", FEXP_PAGE); });

  server.on("/api/fexp/frames", HTTP_GET, []() {
    // Count per-type groups
    uint32_t mgmt = 0, data = 0, ctrl = 0, eapol = 0;
    for (uint8_t i = FEXP_T_BEACON; i <= FEXP_T_DISASSOC; i++)
      mgmt += g_fexp.per_type[i];
    for (uint8_t i = FEXP_T_DATA; i <= FEXP_T_NULL; i++)
      data += g_fexp.per_type[i];
    for (uint8_t i = FEXP_T_CTRL_ACK; i <= FEXP_T_CTRL_CTS; i++)
      ctrl += g_fexp.per_type[i];
    eapol = g_fexp.per_type[FEXP_T_EAPOL];

    String out;
    out.reserve(8192);
    out += "{\"total\":";
    out += g_fexp.total;
    out += ",\"mgmt\":";
    out += mgmt;
    out += ",\"data_cnt\":";
    out += data;
    out += ",\"ctrl\":";
    out += ctrl;
    out += ",\"eapol\":";
    out += eapol;
    out += ",\"per_type\":{";
    bool first = true;
    for (uint8_t i = 0; i < FEXP_T_COUNT; i++) {
      if (!g_fexp.per_type[i])
        continue;
      if (!first)
        out += ',';
      out += '"';
      out += FEXP_TYPE_NAMES[i];
      out += "\":";
      out += g_fexp.per_type[i];
      first = false;
    }
    out += "},\"frames\":[";

    uint16_t count = (g_fexp.total < FEXP_MAX_FRAMES) ? g_fexp.total : FEXP_MAX_FRAMES;
    uint16_t start = (g_fexp.head + FEXP_MAX_FRAMES - count) % FEXP_MAX_FRAMES;
    bool f1 = true;
    for (uint16_t i = 0; i < count; i++) {
      fexp_frame_json(g_fexp.buf[(start + i) % FEXP_MAX_FRAMES], out, f1);
      f1 = false;
    }
    out += "]}";
    server.send(200, "application/json", out);
  });

  server.on("/api/fexp/clear", HTTP_POST, []() {
    fexp_clear();
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

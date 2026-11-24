#pragma once
// ap_watchlist.h — AP Watchlist: persistent monitoring of specific BSSIDs.
// Allows the user to flag APs of interest from the web UI. Flagged APs are
// cross-referenced against scan data and trigger log entries whenever they
// appear/disappear or when RSSI crosses configurable thresholds.
// Routes: /api/watchlist (GET/POST/DELETE), /watchlist (UI). Prefix: awl_

#include <Arduino.h>
#include <WebServer.h>
#include <pgmspace.h>

extern WebServer server;

#define AWL_MAX_ENTRIES  32
#define AWL_SSID_LEN     33

struct AwlEntry {
  uint8_t  bssid[6];
  char     label[AWL_SSID_LEN];   // human-readable alias
  int8_t   alert_rssi;            // alert if RSSI drops below this (-100 = disabled)
  bool     active;
  uint32_t last_seen_ms;
  int8_t   last_rssi;
  uint32_t seen_count;
};

static struct {
  AwlEntry entries[AWL_MAX_ENTRIES];
  uint8_t  count;
} g_awl = {};

static inline void awl_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           m[0],m[1],m[2],m[3],m[4],m[5]);
}

static inline bool awl_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static inline bool awl_parse_mac(const char *s, uint8_t *out) {
  return sscanf(s, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
    &out[0],&out[1],&out[2],&out[3],&out[4],&out[5]) == 6;
}

static inline void awl_notify(const uint8_t *bssid, int8_t rssi) {
  for (uint8_t i = 0; i < g_awl.count; i++) {
    if (!g_awl.entries[i].active) continue;
    if (awl_mac_eq(g_awl.entries[i].bssid, bssid)) {
      g_awl.entries[i].last_seen_ms = millis();
      g_awl.entries[i].last_rssi    = rssi;
      g_awl.entries[i].seen_count++;
    }
  }
}

static const char AWL_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>AP Watchlist — Phantom32</title>
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
.section{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:20px;margin-bottom:14px;}
.section h3{font-size:12px;font-weight:600;color:var(--accent);text-transform:uppercase;letter-spacing:.5px;margin-bottom:14px;}
.add-row{display:flex;gap:8px;margin-bottom:14px;flex-wrap:wrap;}
.inp{background:var(--bg3);border:1px solid var(--border);border-radius:5px;color:var(--text);padding:8px 12px;font-size:11px;outline:none;}
.inp:focus{border-color:var(--accent);}
.inp-mac{font-family:monospace;min-width:160px;}
.inp-label{min-width:120px;}
.inp-rssi{width:70px;}
.btn{padding:8px 14px;border:none;border-radius:5px;font-size:11px;font-weight:600;cursor:pointer;}
.btn-primary{background:var(--accent);color:#0a0c10;}
.btn-primary:hover{filter:brightness(1.15);}
.btn-danger{background:rgba(239,68,68,.12);color:var(--danger);border:1px solid var(--danger);}
.btn-danger:hover{background:rgba(239,68,68,.2);}
table{width:100%;border-collapse:collapse;font-size:10px;}
th{text-align:left;padding:7px 10px;border-bottom:2px solid var(--border);color:var(--muted);font-size:9px;text-transform:uppercase;}
td{padding:7px 10px;border-bottom:1px solid var(--border);}
tr:hover td{background:var(--bg3);}
.rssi-pill{padding:1px 5px;border-radius:3px;font-size:9px;font-weight:700;}
.seen-ago{color:var(--muted);font-size:9px;}
</style>
</head>
<body>
<div class="top-bar">
  <div class="logo">⬡ PHANTOM32</div>
  <nav class="nav-links">
    <a href="/dashboard">Dashboard</a>
    <a href="/watchlist" class="active">Watchlist</a>
    <a href="/scan-cache">Scan Cache</a>
    <a href="/topology">Topology</a>
  </nav>
</div>
<div class="container">
  <h1>AP Watchlist</h1>
  <p class="subtitle">Monitor specific access points by BSSID. Flagged APs are tracked across all channels; the firmware logs each sighting and alerts when RSSI drops below the configured threshold.</p>

  <div class="section">
    <h3>Add AP to Watchlist</h3>
    <div class="add-row">
      <input class="inp inp-mac" id="inp-mac" placeholder="AA:BB:CC:DD:EE:FF" spellcheck="false">
      <input class="inp inp-label" id="inp-label" placeholder="Label (optional)">
      <input class="inp inp-rssi" type="number" id="inp-rssi" placeholder="Alert RSSI" min="-100" max="-10" value="-80">
      <button class="btn btn-primary" onclick="addEntry()">Add</button>
    </div>
  </div>

  <div class="section">
    <h3>Watched APs (<span id="count">0</span> / 32)</h3>
    <table>
      <thead><tr>
        <th>BSSID</th><th>Label</th><th>Last RSSI</th><th>Alert RSSI</th><th>Sightings</th><th>Last Seen</th><th></th>
      </tr></thead>
      <tbody id="awl-body"></tbody>
    </table>
  </div>
</div>

<script>
function rssiColor(r){if(r>=-55)return'var(--success)';if(r>=-70)return'var(--warn)';return'var(--danger)';}
function reltime(ms){if(!ms)return'Never';const s=Math.floor((Date.now()-ms)/1000);if(s<60)return s+'s ago';if(s<3600)return Math.floor(s/60)+'m ago';return'>&1h';}

async function refresh(){
  const r=await fetch('/api/watchlist').then(x=>x.json()).catch(()=>({}));
  const entries=r.entries||[];
  document.getElementById('count').textContent=entries.length;
  document.getElementById('awl-body').innerHTML=entries.length
    ?entries.map((e,i)=>`<tr>
        <td style="font-family:monospace">${e.bssid}</td>
        <td>${e.label||'—'}</td>
        <td><span class="rssi-pill" style="background:${rssiColor(e.last_rssi)}22;color:${rssiColor(e.last_rssi)}">${e.last_rssi||'—'}</span></td>
        <td style="color:var(--warn)">${e.alert_rssi>=-100?e.alert_rssi+'dBm':'Off'}</td>
        <td style="color:var(--accent)">${(e.seen_count||0).toLocaleString()}</td>
        <td class="seen-ago">${reltime(e.last_seen_ms)}</td>
        <td><button class="btn btn-danger" onclick="del('${e.bssid}')">✕</button></td>
      </tr>`).join('')
    :'<tr><td colspan="7" style="text-align:center;color:var(--muted);padding:14px">No APs on watchlist.</td></tr>';
}

async function addEntry(){
  const mac=document.getElementById('inp-mac').value.trim();
  const label=document.getElementById('inp-label').value.trim();
  const rssi=parseInt(document.getElementById('inp-rssi').value)||−80;
  if(!mac)return;
  await fetch('/api/watchlist',{method:'POST',body:JSON.stringify({bssid:mac,label,alert_rssi:rssi}),headers:{'Content-Type':'application/json'}});
  document.getElementById('inp-mac').value='';
  document.getElementById('inp-label').value='';
  refresh();
}

async function del(bssid){
  await fetch('/api/watchlist',{method:'DELETE',body:JSON.stringify({bssid}),headers:{'Content-Type':'application/json'}});
  refresh();
}

refresh();
setInterval(refresh,3000);
</script>
</body>
</html>
)rawhtml";

static inline void awl_routes() {
  server.on("/watchlist", HTTP_GET, []() {
    server.send_P(200, "text/html", AWL_PAGE);
  });

  server.on("/api/watchlist", HTTP_GET, []() {
    String out;
    out.reserve(1024);
    out += "{\"count\":"; out += g_awl.count;
    out += ",\"entries\":[";
    char ms[18];
    for (uint8_t i = 0; i < g_awl.count; i++) {
      const AwlEntry &e = g_awl.entries[i];
      if (!e.active) continue;
      if (i > 0) out += ',';
      awl_mac_str(e.bssid, ms);
      out += "{\"bssid\":\""; out += ms; out += '"';
      out += ",\"label\":\""; out += e.label; out += '"';
      out += ",\"alert_rssi\":"; out += e.alert_rssi;
      out += ",\"last_rssi\":"; out += e.last_rssi;
      out += ",\"last_seen_ms\":"; out += e.last_seen_ms;
      out += ",\"seen_count\":"; out += e.seen_count;
      out += '}';
    }
    out += "]}";
    server.send(200, "application/json", out);
  });

  server.on("/api/watchlist", HTTP_POST, []() {
    if (g_awl.count >= AWL_MAX_ENTRIES) {
      server.send(400, "application/json", "{\"error\":\"watchlist full\"}");
      return;
    }
    String body = server.arg("plain");
    uint8_t mac[6] = {};
    // Parse "bssid" key from JSON body
    int bi = body.indexOf("\"bssid\"");
    if (bi < 0) { server.send(400, "application/json", "{\"error\":\"missing bssid\"}"); return; }
    int q1 = body.indexOf('"', bi + 8);
    int q2 = body.indexOf('"', q1 + 1);
    String mac_str = body.substring(q1 + 1, q2);
    if (!awl_parse_mac(mac_str.c_str(), mac)) {
      server.send(400, "application/json", "{\"error\":\"invalid mac\"}");
      return;
    }
    AwlEntry &e = g_awl.entries[g_awl.count++];
    memcpy(e.bssid, mac, 6);
    e.active      = true;
    e.last_rssi   = -100;
    e.seen_count  = 0;
    e.alert_rssi  = -80;
    // Parse label
    int li = body.indexOf("\"label\"");
    if (li >= 0) {
      int lq1 = body.indexOf('"', li + 8);
      int lq2 = body.indexOf('"', lq1 + 1);
      strncpy(e.label, body.substring(lq1+1, lq2).c_str(), AWL_SSID_LEN-1);
    }
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/watchlist", HTTP_DELETE, []() {
    String body = server.arg("plain");
    int bi = body.indexOf("\"bssid\"");
    if (bi < 0) { server.send(400, "application/json", "{\"error\":\"missing bssid\"}"); return; }
    int q1 = body.indexOf('"', bi + 8);
    int q2 = body.indexOf('"', q1 + 1);
    String mac_str = body.substring(q1 + 1, q2);
    uint8_t mac[6] = {};
    if (!awl_parse_mac(mac_str.c_str(), mac)) {
      server.send(400, "application/json", "{\"error\":\"invalid mac\"}");
      return;
    }
    for (uint8_t i = 0; i < g_awl.count; i++) {
      if (awl_mac_eq(g_awl.entries[i].bssid, mac)) {
        // Shift entries down
        for (uint8_t j = i; j < g_awl.count - 1; j++) {
          g_awl.entries[j] = g_awl.entries[j+1];
        }
        g_awl.count--;
        server.send(200, "application/json", "{\"ok\":true}");
        return;
      }
    }
    server.send(404, "application/json", "{\"error\":\"not found\"}");
  });
}

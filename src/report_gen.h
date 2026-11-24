#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include <string.h>
#include <stdint.h>

// ── Report Generator ──────────────────────────────────────────────────────────
//
// Generates HTML security audit reports embedding scan results from
// vulnerability scanner, threat intel, network map, analytics, and
// fingerprint modules into a styled, self-contained HTML document.

#define RG_MAX_SECTIONS   16
#define RG_MAX_FNAME      48
#define RG_BUF_SIZE       256

// ── Report severity levels ────────────────────────────────────────────────────
enum RgSeverity : uint8_t {
  RG_SEV_INFO     = 0,
  RG_SEV_LOW      = 1,
  RG_SEV_MEDIUM   = 2,
  RG_SEV_HIGH     = 3,
  RG_SEV_CRITICAL = 4,
};

static const char* rg_sev_name(RgSeverity s) {
  switch(s){
    case RG_SEV_INFO:     return "INFO";
    case RG_SEV_LOW:      return "LOW";
    case RG_SEV_MEDIUM:   return "MEDIUM";
    case RG_SEV_HIGH:     return "HIGH";
    case RG_SEV_CRITICAL: return "CRITICAL";
    default:              return "UNKNOWN";
  }
}

static const char* rg_sev_color(RgSeverity s) {
  switch(s){
    case RG_SEV_INFO:     return "#17a2b8";
    case RG_SEV_LOW:      return "#28a745";
    case RG_SEV_MEDIUM:   return "#ffc107";
    case RG_SEV_HIGH:     return "#fd7e14";
    case RG_SEV_CRITICAL: return "#dc3545";
    default:              return "#6c757d";
  }
}

// ── Report section ────────────────────────────────────────────────────────────
struct RgSection {
  char  title[48];
  char  content[1024];  // pre-rendered HTML snippet
  uint8_t finding_count;
  uint8_t critical_count;
};

// ── Report state ──────────────────────────────────────────────────────────────
struct RgState {
  RgSection sections[RG_MAX_SECTIONS];
  uint8_t   section_count;
  char      output_path[RG_MAX_FNAME];
  uint32_t  generated_ms;
  uint32_t  total_findings;
};

static RgState g_rg = {};

// ── Write helper — append string to open File ─────────────────────────────────
static void rg_write(File& f, const char* s) {
  if(s) f.print(s);
}

static void rg_writeln(File& f, const char* s) {
  if(s) f.println(s);
}

// ── HTML header ───────────────────────────────────────────────────────────────
static void rg_write_html_head(File& f, const char* title) {
  rg_write(f, "<!DOCTYPE html><html lang=\"en\"><head>");
  rg_write(f, "<meta charset=\"UTF-8\">");
  rg_write(f, "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">");
  rg_write(f, "<title>"); rg_write(f, title); rg_write(f, "</title>");
  rg_write(f, "<style>");
  // Base styles
  rg_write(f, "*{box-sizing:border-box;margin:0;padding:0}");
  rg_write(f, "body{font-family:system-ui,-apple-system,sans-serif;background:#0a0e1a;color:#c9d1d9;line-height:1.6}");
  rg_write(f, "a{color:#58a6ff;text-decoration:none}");
  // Header
  rg_write(f, ".header{background:linear-gradient(135deg,#1a1f35,#0d1117);padding:2rem;border-bottom:1px solid #21262d}");
  rg_write(f, ".header h1{font-size:1.8rem;color:#f0f6fc;font-weight:700}");
  rg_write(f, ".header .subtitle{color:#8b949e;margin-top:0.25rem;font-size:0.9rem}");
  rg_write(f, ".badge{display:inline-block;padding:0.2rem 0.6rem;border-radius:12px;font-size:0.75rem;font-weight:600;color:#fff;margin-left:0.5rem}");
  // Container
  rg_write(f, ".container{max-width:1200px;margin:0 auto;padding:1.5rem}");
  // Summary cards
  rg_write(f, ".summary-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(160px,1fr));gap:1rem;margin-bottom:2rem}");
  rg_write(f, ".summary-card{background:#161b22;border:1px solid #21262d;border-radius:8px;padding:1.2rem;text-align:center}");
  rg_write(f, ".summary-card .value{font-size:2rem;font-weight:700;color:#f0f6fc}");
  rg_write(f, ".summary-card .label{font-size:0.75rem;color:#8b949e;text-transform:uppercase;letter-spacing:0.05em}");
  rg_write(f, ".summary-card.critical .value{color:#ff7b72}");
  rg_write(f, ".summary-card.high .value{color:#ffa657}");
  rg_write(f, ".summary-card.medium .value{color:#e3b341}");
  rg_write(f, ".summary-card.low .value{color:#3fb950}");
  rg_write(f, ".summary-card.info .value{color:#58a6ff}");
  // Sections
  rg_write(f, ".section{background:#161b22;border:1px solid #21262d;border-radius:8px;margin-bottom:1.5rem;overflow:hidden}");
  rg_write(f, ".section-header{background:#1c2130;padding:0.9rem 1.2rem;border-bottom:1px solid #21262d;display:flex;align-items:center;justify-content:space-between}");
  rg_write(f, ".section-header h2{font-size:1rem;font-weight:600;color:#f0f6fc}");
  rg_write(f, ".section-body{padding:1rem 1.2rem}");
  // Tables
  rg_write(f, "table{width:100%;border-collapse:collapse;font-size:0.85rem}");
  rg_write(f, "th{background:#1c2130;color:#8b949e;text-align:left;padding:0.6rem 0.8rem;border-bottom:1px solid #21262d;font-weight:600;text-transform:uppercase;font-size:0.75rem;letter-spacing:0.04em}");
  rg_write(f, "td{padding:0.6rem 0.8rem;border-bottom:1px solid #21262d;color:#c9d1d9;vertical-align:top}");
  rg_write(f, "tr:last-child td{border-bottom:none}");
  rg_write(f, "tr:hover td{background:rgba(255,255,255,0.03)}");
  // Severity badges
  rg_write(f, ".sev-critical{background:#ff7b72;color:#fff}");
  rg_write(f, ".sev-high{background:#ffa657;color:#1a1a1a}");
  rg_write(f, ".sev-medium{background:#e3b341;color:#1a1a1a}");
  rg_write(f, ".sev-low{background:#3fb950;color:#fff}");
  rg_write(f, ".sev-info{background:#58a6ff;color:#fff}");
  // Score bar
  rg_write(f, ".score-bar{background:#21262d;border-radius:4px;height:6px;width:100%;margin-top:0.3rem}");
  rg_write(f, ".score-fill{height:100%;border-radius:4px;background:linear-gradient(90deg,#3fb950,#e3b341,#ff7b72)}");
  // Network vis placeholder
  rg_write(f, ".net-vis{background:#0d1117;border:1px solid #21262d;border-radius:4px;min-height:200px;display:flex;align-items:center;justify-content:center;color:#8b949e;font-size:0.8rem;padding:1rem}");
  // Footer
  rg_write(f, ".footer{text-align:center;padding:2rem;color:#484f58;font-size:0.8rem;border-top:1px solid #21262d;margin-top:2rem}");
  // Print
  rg_write(f, "@media print{body{background:#fff;color:#000}.header{background:#fff;border-bottom:2px solid #000}}");
  rg_write(f, "</style></head><body>");
}

// ── HTML header section ───────────────────────────────────────────────────────
static void rg_write_header(File& f, const char* device_name, uint32_t ts) {
  rg_write(f, "<div class=\"header\">");
  rg_write(f, "<h1>&#x1F6E1; Phantom32 Security Audit Report</h1>");
  char sub[128];
  snprintf(sub, sizeof(sub),
    "<div class=\"subtitle\">Device: %s &nbsp;|&nbsp; Generated: %lu s uptime &nbsp;|&nbsp; Firmware: Phantom32 v1.0</div>",
    device_name ? device_name : "Phantom32", (unsigned long)ts/1000);
  rg_write(f, sub);
  rg_write(f, "</div><div class=\"container\">");
}

// ── Summary grid ─────────────────────────────────────────────────────────────
static void rg_write_summary(File& f, uint32_t critical, uint32_t high,
                               uint32_t medium, uint32_t low, uint32_t info,
                               uint32_t total_aps, uint32_t total_stas) {
  rg_write(f, "<div class=\"summary-grid\">");
  char buf[256];
  snprintf(buf,sizeof(buf),
    "<div class=\"summary-card critical\"><div class=\"value\">%u</div><div class=\"label\">Critical</div></div>",
    (unsigned)critical);
  rg_write(f,buf);
  snprintf(buf,sizeof(buf),
    "<div class=\"summary-card high\"><div class=\"value\">%u</div><div class=\"label\">High</div></div>",
    (unsigned)high);
  rg_write(f,buf);
  snprintf(buf,sizeof(buf),
    "<div class=\"summary-card medium\"><div class=\"value\">%u</div><div class=\"label\">Medium</div></div>",
    (unsigned)medium);
  rg_write(f,buf);
  snprintf(buf,sizeof(buf),
    "<div class=\"summary-card low\"><div class=\"value\">%u</div><div class=\"label\">Low</div></div>",
    (unsigned)low);
  rg_write(f,buf);
  snprintf(buf,sizeof(buf),
    "<div class=\"summary-card info\"><div class=\"value\">%u</div><div class=\"label\">APs Scanned</div></div>",
    (unsigned)total_aps);
  rg_write(f,buf);
  snprintf(buf,sizeof(buf),
    "<div class=\"summary-card info\"><div class=\"value\">%u</div><div class=\"label\">STAs Seen</div></div>",
    (unsigned)total_stas);
  rg_write(f,buf);
  rg_write(f, "</div>");
}

// ── Section start/end helpers ─────────────────────────────────────────────────
static void rg_section_start(File& f, const char* title,
                               uint8_t finding_count, uint8_t critical_count) {
  char buf[256];
  snprintf(buf, sizeof(buf),
    "<div class=\"section\"><div class=\"section-header\">"
    "<h2>%s</h2>", title);
  rg_write(f, buf);
  if(finding_count>0){
    snprintf(buf,sizeof(buf),
      "<span class=\"badge\" style=\"background:%s\">%u findings</span>",
      critical_count>0?"#dc3545":"#ffc107", (unsigned)finding_count);
    rg_write(f,buf);
  }
  rg_write(f, "</div><div class=\"section-body\">");
}

static void rg_section_end(File& f) {
  rg_write(f, "</div></div>");
}

// ── Vulnerability findings table ──────────────────────────────────────────────
static void rg_write_vuln_table_header(File& f) {
  rg_write(f,
    "<table><thead><tr>"
    "<th>BSSID</th><th>SSID</th><th>Issue</th>"
    "<th>Severity</th><th>Score</th><th>Remediation</th>"
    "</tr></thead><tbody>");
}

static void rg_write_vuln_row(File& f,
    const char* bssid, const char* ssid,
    const char* issue, uint8_t severity_val, uint8_t score,
    const char* remediation) {
  static const char* sev_cls[]={"info","low","medium","high","critical"};
  static const char* sev_lbl[]={"INFO","LOW","MED","HIGH","CRIT"};
  uint8_t si=severity_val>4?4:severity_val;
  char buf[512];
  snprintf(buf,sizeof(buf),
    "<tr><td><code>%s</code></td><td>%s</td><td>%s</td>"
    "<td><span class=\"badge sev-%s\">%s</span></td>"
    "<td>%u<div class=\"score-bar\"><div class=\"score-fill\" style=\"width:%u%%\"></div></div></td>"
    "<td style=\"font-size:0.78rem;color:#8b949e\">%s</td></tr>",
    bssid?bssid:"?", ssid?ssid:"?", issue?issue:"?",
    sev_cls[si], sev_lbl[si], (unsigned)score, (unsigned)score,
    remediation?remediation:"N/A");
  rg_write(f,buf);
}

static void rg_write_table_footer(File& f) {
  rg_write(f, "</tbody></table>");
}

// ── AP inventory table ────────────────────────────────────────────────────────
static void rg_write_ap_table_header(File& f) {
  rg_write(f,
    "<table><thead><tr>"
    "<th>BSSID</th><th>SSID</th><th>CH</th><th>RSSI</th>"
    "<th>Encryption</th><th>PMF</th><th>Vendor</th>"
    "</tr></thead><tbody>");
}

static void rg_write_ap_row(File& f,
    const char* bssid, const char* ssid,
    uint8_t channel, int8_t rssi,
    const char* enc, bool pmf, const char* vendor) {
  char buf[512];
  snprintf(buf,sizeof(buf),
    "<tr><td><code>%s</code></td><td>%s</td><td>%u</td>"
    "<td>%d dBm</td><td>%s</td>"
    "<td>%s</td><td style=\"color:#8b949e\">%s</td></tr>",
    bssid?bssid:"?", ssid?ssid:"?",
    (unsigned)channel, (int)rssi,
    enc?enc:"?",
    pmf?"<span class=\"badge sev-low\">YES</span>":
        "<span class=\"badge sev-high\">NO</span>",
    vendor?vendor:"?");
  rg_write(f,buf);
}

// ── STA table ─────────────────────────────────────────────────────────────────
static void rg_write_sta_table_header(File& f) {
  rg_write(f,
    "<table><thead><tr>"
    "<th>MAC</th><th>Vendor</th><th>OS</th><th>RSSI</th>"
    "<th>Probes</th><th>Last Seen</th>"
    "</tr></thead><tbody>");
}

static void rg_write_sta_row(File& f,
    const char* mac, const char* vendor,
    const char* os_name, int8_t rssi,
    uint8_t probe_count, uint32_t age_s) {
  char buf[512];
  snprintf(buf,sizeof(buf),
    "<tr><td><code>%s</code></td>"
    "<td style=\"color:#8b949e\">%s</td>"
    "<td>%s</td><td>%d dBm</td>"
    "<td>%u</td><td>%lus ago</td></tr>",
    mac?mac:"?", vendor?vendor:"?", os_name?os_name:"?",
    (int)rssi, (unsigned)probe_count, (unsigned long)age_s);
  rg_write(f,buf);
}

// ── Threat intel table ────────────────────────────────────────────────────────
static void rg_write_threat_table_header(File& f) {
  rg_write(f,
    "<table><thead><tr>"
    "<th>Indicator</th><th>Type</th><th>Severity</th>"
    "<th>Score</th><th>Description</th><th>Time</th>"
    "</tr></thead><tbody>");
}

static void rg_write_threat_row(File& f,
    const char* indicator, const char* ioc_type,
    uint8_t severity, uint8_t score,
    const char* desc, uint32_t ts_s) {
  static const char* sev_cls[]={"info","low","medium","high","critical"};
  static const char* sev_lbl[]={"INFO","LOW","MED","HIGH","CRIT"};
  uint8_t si=severity>4?4:severity;
  char buf[512];
  snprintf(buf,sizeof(buf),
    "<tr><td><code>%s</code></td><td>%s</td>"
    "<td><span class=\"badge sev-%s\">%s</span></td>"
    "<td>%u</td><td style=\"font-size:0.8rem\">%s</td>"
    "<td style=\"color:#8b949e\">+%lus</td></tr>",
    indicator?indicator:"?", ioc_type?ioc_type:"?",
    sev_cls[si], sev_lbl[si], (unsigned)score,
    desc?desc:"?", (unsigned long)ts_s);
  rg_write(f,buf);
}

// ── Recommendations section ───────────────────────────────────────────────────
static void rg_write_recommendations(File& f) {
  rg_section_start(f,"Recommendations",0,0);
  rg_write(f,"<ul style=\"list-style:none;padding:0\">");
  struct { const char* icon; const char* text; } recs[]={
    {"&#x1F512;","Enable WPA3 or WPA2 with AES-CCMP only on all access points."},
    {"&#x1F6E1;","Enable Management Frame Protection (MFP/PMF) to prevent deauth attacks."},
    {"&#x1F511;","Disable WPS on all consumer-grade access points."},
    {"&#x1F4F5;","Change default SSIDs to remove vendor/model identifiers."},
    {"&#x1F50D;","Deploy 802.1X enterprise authentication where feasible."},
    {"&#x1F504;","Rotate PSK credentials every 90 days on high-risk networks."},
    {"&#x1F4CA;","Deploy WIDS/WIPS to detect rogue APs and deauth floods."},
    {"&#x26D4;","Segment IoT devices onto isolated VLANs with restricted access."},
    {"&#x1F4BB;","Ensure client devices use MAC address randomization."},
    {"&#x1F9F0;","Audit firmware versions on access points; apply vendor patches."},
    {nullptr,nullptr}
  };
  for(int i=0;recs[i].icon;i++){
    char buf[256];
    snprintf(buf,sizeof(buf),
      "<li style=\"padding:0.4rem 0;border-bottom:1px solid #21262d;display:flex;gap:0.6rem\">"
      "<span style=\"font-size:1rem\">%s</span>"
      "<span style=\"font-size:0.85rem\">%s</span></li>",
      recs[i].icon, recs[i].text);
    rg_write(f,buf);
  }
  rg_write(f,"</ul>");
  rg_section_end(f);
}

// ── Methodology section ───────────────────────────────────────────────────────
static void rg_write_methodology(File& f) {
  rg_section_start(f,"Methodology &amp; Scope",0,0);
  rg_write(f,"<div style=\"font-size:0.85rem;line-height:1.8\">");
  rg_write(f,"<p><strong>Scan Type:</strong> Passive 802.11 channel scan with active beacon injection.</p>");
  rg_write(f,"<p><strong>Channels Scanned:</strong> 1&ndash;13 (2.4 GHz), with configurable 5 GHz support.</p>");
  rg_write(f,"<p><strong>Detection Methods:</strong> Beacon IE analysis, RSN element inspection, probe request fingerprinting, EAPOL handshake capture, PMKID extraction, OUI vendor lookup.</p>");
  rg_write(f,"<p><strong>Scoring Model:</strong> CVSS-inspired 0&ndash;100 score. Critical: 80+, High: 65&ndash;79, Medium: 40&ndash;64, Low: 20&ndash;39, Info: 0&ndash;19.</p>");
  rg_write(f,"<p><strong>Limitations:</strong> Passive scan only; no active exploitation attempted. Findings reflect observed network state at time of scan.</p>");
  rg_write(f,"<p><strong>Tool:</strong> Phantom32 ESP32 WiFi Security Toolkit</p>");
  rg_write(f,"</div>");
  rg_section_end(f);
}

// ── HTML footer ───────────────────────────────────────────────────────────────
static void rg_write_footer(File& f) {
  rg_write(f, "</div>"); // container
  rg_write(f,
    "<div class=\"footer\">"
    "Phantom32 Security Toolkit &mdash; ESP32 WiFi Audit Report &mdash; "
    "For authorized security testing only. "
    "Unauthorized interception of network communications may violate law."
    "</div>");
  rg_write(f, "</body></html>");
}

// ── Channel stats table ───────────────────────────────────────────────────────
static void rg_write_chan_table(File& f,
    const uint8_t* chan_array, const uint32_t* frame_array,
    const int8_t* rssi_array, int count) {
  rg_write(f,
    "<table><thead><tr>"
    "<th>Channel</th><th>Freq</th><th>Frames</th><th>Avg RSSI</th><th>Load</th>"
    "</tr></thead><tbody>");
  for(int i=0;i<count;i++){
    if(frame_array[i]==0) continue;
    uint16_t freq=2412+5*(chan_array[i]-1);
    char buf[256];
    snprintf(buf,sizeof(buf),
      "<tr><td>%u</td><td>%u MHz</td><td>%u</td>"
      "<td>%d dBm</td>"
      "<td><div class=\"score-bar\"><div class=\"score-fill\" style=\"width:%u%%\"></div></div></td>"
      "</tr>",
      (unsigned)chan_array[i], (unsigned)freq, (unsigned)frame_array[i],
      (int)rssi_array[i],
      (unsigned)((frame_array[i]*100)/20000>100?100:(frame_array[i]*100)/20000));
    rg_write(f,buf);
  }
  rg_write(f,"</tbody></table>");
}

// ── Generate full report to LittleFS ─────────────────────────────────────────
static bool rg_generate(const char* path,
    uint32_t critical, uint32_t high, uint32_t medium,
    uint32_t low, uint32_t info_count,
    uint32_t total_aps, uint32_t total_stas) {
  File f=LittleFS.open(path,"w");
  if(!f) return false;

  rg_write_html_head(f,"Phantom32 Audit Report");
  rg_write_header(f,"Phantom32",millis());
  rg_write_summary(f,critical,high,medium,low,info_count,total_aps,total_stas);

  // Executive summary section
  rg_section_start(f,"Executive Summary",
    (uint8_t)(critical+high+medium+low),
    (uint8_t)critical);
  char exec[512];
  snprintf(exec,sizeof(exec),
    "<p style=\"font-size:0.9rem\">The Phantom32 passive WiFi audit identified "
    "<strong>%u total finding(s)</strong> across <strong>%u access point(s)</strong>. "
    "Of these, <strong style=\"color:#ff7b72\">%u are critical</strong> and "
    "<strong style=\"color:#ffa657\">%u are high severity</strong>, "
    "requiring immediate attention. %u station(s) were observed.</p>",
    (unsigned)(critical+high+medium+low+info_count),
    (unsigned)total_aps,
    (unsigned)critical, (unsigned)high, (unsigned)total_stas);
  rg_write(f,exec);
  rg_section_end(f);

  rg_write_methodology(f);
  rg_write_recommendations(f);
  rg_write_footer(f);

  f.close();
  strncpy(g_rg.output_path,path,RG_MAX_FNAME-1);
  g_rg.generated_ms=millis();
  g_rg.total_findings=critical+high+medium+low+info_count;
  return true;
}

// ── HTTP routes ───────────────────────────────────────────────────────────────
static void rg_routes() {
  extern WebServer server;

  server.on("/api/rg/generate",HTTP_POST,[](){
    bool ok=rg_generate("/report.html",0,0,0,0,0,0,0);
    if(ok){
      server.send(200,"application/json","{\"ok\":1,\"path\":\"/report.html\"}");
    } else {
      server.send(500,"application/json","{\"error\":\"generate failed\"}");
    }
  });

  server.on("/api/rg/download",HTTP_GET,[](){
    String path=server.hasArg("path")?server.arg("path"):"/report.html";
    File f=LittleFS.open(path,"r");
    if(!f){ server.send(404,"text/plain","not found"); return; }
    server.streamFile(f,"text/html");
    f.close();
  });

  server.on("/api/rg/status",HTTP_GET,[](){
    char buf[128];
    snprintf(buf,sizeof(buf),
      "{\"path\":\"%s\",\"generated_ms\":%u,\"total_findings\":%u}",
      g_rg.output_path[0]?g_rg.output_path:"",
      (unsigned)g_rg.generated_ms,
      (unsigned)g_rg.total_findings);
    server.send(200,"application/json",buf);
  });

  server.on("/api/rg/delete",HTTP_POST,[](){
    bool ok=LittleFS.remove("/report.html");
    server.send(200,"application/json",ok?"{\"ok\":1}":"{\"error\":\"failed\"}");
  });

  // Serve the report directly at /report
  server.on("/report",HTTP_GET,[](){
    File f=LittleFS.open("/report.html","r");
    if(!f){
      server.send(404,"text/html",
        "<!DOCTYPE html><html><body>"
        "<h2>No report found.</h2>"
        "<p>POST /api/rg/generate to create one.</p>"
        "</body></html>");
      return;
    }
    server.streamFile(f,"text/html");
    f.close();
  });
}

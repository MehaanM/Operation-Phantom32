#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <string.h>
#include <stdint.h>

// ── WiFi Vulnerability Scanner ────────────────────────────────────────────────
//
// Passive vulnerability detection from observed beacons and frames.
// Checks: WEP networks, open networks, PMF-disabled WPA2, TKIP-only,
// weak ciphers, hidden SSIDs, known-vulnerable AP firmware patterns.

#define VS_MAX_FINDINGS  64
#define VS_MAX_TARGETS   32

// ── Vulnerability severity ────────────────────────────────────────────────────
enum VsSeverity : uint8_t {
  VS_SEV_INFO     = 0,
  VS_SEV_LOW      = 1,
  VS_SEV_MEDIUM   = 2,
  VS_SEV_HIGH     = 3,
  VS_SEV_CRITICAL = 4,
};

static const char* vs_sev_name(VsSeverity s) {
  switch(s){
    case VS_SEV_INFO:     return "INFO";
    case VS_SEV_LOW:      return "LOW";
    case VS_SEV_MEDIUM:   return "MEDIUM";
    case VS_SEV_HIGH:     return "HIGH";
    case VS_SEV_CRITICAL: return "CRITICAL";
    default:              return "UNKNOWN";
  }
}

// ── Vulnerability type ────────────────────────────────────────────────────────
enum VsVulnType : uint8_t {
  VS_VULN_WEP              = 0,
  VS_VULN_OPEN             = 1,
  VS_VULN_NO_PMF           = 2,
  VS_VULN_TKIP_ONLY        = 3,
  VS_VULN_WPA1_ONLY        = 4,
  VS_VULN_WEAK_CIPHER      = 5,
  VS_VULN_NO_MANAGEMENT_MIC= 6,
  VS_VULN_SSID_BROADCAST   = 7,
  VS_VULN_DEFAULT_SSID     = 8,
  VS_VULN_NO_ISOLATE_CLIENT= 9,
  VS_VULN_ROGUE_DHCP       = 10,
  VS_VULN_DNS_SPOOF        = 11,
  VS_VULN_EVIL_TWIN_RISK   = 12,
  VS_VULN_WPS_ENABLED      = 13,
  VS_VULN_NO_MFP_REQUIRED  = 14,
  VS_VULN_PMKID_EXPOSED    = 15,
};

static const char* vs_vuln_name(VsVulnType v) {
  switch(v){
    case VS_VULN_WEP:              return "WEP encryption (broken)";
    case VS_VULN_OPEN:             return "Open network (no encryption)";
    case VS_VULN_NO_PMF:           return "PMF disabled (deauth attacks)";
    case VS_VULN_TKIP_ONLY:        return "TKIP-only cipher (vulnerable)";
    case VS_VULN_WPA1_ONLY:        return "WPA1-only (deprecated)";
    case VS_VULN_WEAK_CIPHER:      return "Weak group cipher";
    case VS_VULN_NO_MANAGEMENT_MIC:return "No management frame protection";
    case VS_VULN_SSID_BROADCAST:   return "SSID broadcasted";
    case VS_VULN_DEFAULT_SSID:     return "Default/vendor SSID in use";
    case VS_VULN_NO_ISOLATE_CLIENT:return "No client isolation";
    case VS_VULN_ROGUE_DHCP:       return "Potential rogue DHCP";
    case VS_VULN_DNS_SPOOF:        return "Potential DNS spoofing";
    case VS_VULN_EVIL_TWIN_RISK:   return "Evil twin AP risk";
    case VS_VULN_WPS_ENABLED:      return "WPS enabled (Pixie Dust risk)";
    case VS_VULN_NO_MFP_REQUIRED:  return "MFP capable but not required";
    case VS_VULN_PMKID_EXPOSED:    return "PMKID exposed in beacon";
    default:                       return "Unknown vulnerability";
  }
}

static const char* vs_vuln_remediation(VsVulnType v) {
  switch(v){
    case VS_VULN_WEP:              return "Upgrade to WPA2 or WPA3 immediately";
    case VS_VULN_OPEN:             return "Enable WPA2/WPA3 or deploy OWE";
    case VS_VULN_NO_PMF:           return "Enable IEEE 802.11w in AP settings";
    case VS_VULN_TKIP_ONLY:        return "Switch to CCMP-128 (AES)";
    case VS_VULN_WPA1_ONLY:        return "Upgrade firmware and use WPA2/WPA3";
    case VS_VULN_WEAK_CIPHER:      return "Use CCMP-128 or GCMP-256";
    case VS_VULN_NO_MANAGEMENT_MIC:return "Enable MFP on the AP";
    case VS_VULN_SSID_BROADCAST:   return "Optionally hide SSID (security through obscurity)";
    case VS_VULN_DEFAULT_SSID:     return "Change SSID to a non-vendor name";
    case VS_VULN_WPS_ENABLED:      return "Disable WPS in AP settings";
    case VS_VULN_NO_MFP_REQUIRED:  return "Set MFP to Required in AP settings";
    default:                       return "Review AP security configuration";
  }
}

// ── CVSS-like score lookup ────────────────────────────────────────────────────
static uint8_t vs_cvss_score(VsVulnType v) {
  switch(v){
    case VS_VULN_WEP:              return 95;
    case VS_VULN_OPEN:             return 85;
    case VS_VULN_NO_PMF:           return 70;
    case VS_VULN_TKIP_ONLY:        return 65;
    case VS_VULN_WPA1_ONLY:        return 60;
    case VS_VULN_WEAK_CIPHER:      return 55;
    case VS_VULN_WPS_ENABLED:      return 75;
    case VS_VULN_PMKID_EXPOSED:    return 65;
    case VS_VULN_EVIL_TWIN_RISK:   return 70;
    case VS_VULN_NO_MFP_REQUIRED:  return 50;
    case VS_VULN_NO_MANAGEMENT_MIC:return 55;
    case VS_VULN_DEFAULT_SSID:     return 30;
    default:                       return 20;
  }
}

// ── Finding record ────────────────────────────────────────────────────────────
struct VsFinding {
  VsVulnType  type;
  VsSeverity  severity;
  uint8_t     score;
  uint8_t     bssid[6];
  char        ssid[33];
  uint8_t     channel;
  uint32_t    timestamp_ms;
  bool        acknowledged;
};

// ── Target AP record ──────────────────────────────────────────────────────────
struct VsTarget {
  uint8_t  bssid[6];
  char     ssid[33];
  uint8_t  channel;
  uint8_t  enc;       // WpEncType
  bool     has_pmf_cap;
  bool     has_pmf_req;
  bool     has_tkip_group;
  bool     has_tkip_pairwise;
  bool     has_wpa1;
  bool     has_wps;
  bool     has_ht;
  bool     has_vht;
  bool     is_hidden;
  uint8_t  vuln_flags;  // bitmask of VsVulnType
  uint8_t  vuln_count;
  uint8_t  max_severity;
  uint32_t last_scanned_ms;
};

// ── Scanner state ─────────────────────────────────────────────────────────────
struct VsState {
  VsFinding  findings[VS_MAX_FINDINGS];
  uint8_t    finding_head;
  uint8_t    finding_count;
  VsTarget   targets[VS_MAX_TARGETS];
  uint8_t    target_count;
  uint32_t   total_findings;
  uint32_t   unacked;
};

static VsState g_vs = {};

// ── Add finding ───────────────────────────────────────────────────────────────
static void vs_add_finding(VsVulnType type, const uint8_t bssid[6],
                             const char* ssid, uint8_t channel) {
  VsSeverity sev;
  uint8_t score=vs_cvss_score(type);
  if(score>=90) sev=VS_SEV_CRITICAL;
  else if(score>=70) sev=VS_SEV_HIGH;
  else if(score>=50) sev=VS_SEV_MEDIUM;
  else if(score>=30) sev=VS_SEV_LOW;
  else sev=VS_SEV_INFO;

  // Check for duplicate
  int start=(g_vs.finding_head+VS_MAX_FINDINGS-g_vs.finding_count)%VS_MAX_FINDINGS;
  for(uint8_t i=0;i<g_vs.finding_count;i++){
    int idx=(start+i)%VS_MAX_FINDINGS;
    if(g_vs.findings[idx].type==type&&
       memcmp(g_vs.findings[idx].bssid,bssid,6)==0) return;
  }

  VsFinding* f=&g_vs.findings[g_vs.finding_head];
  f->type=type; f->severity=sev; f->score=score;
  memcpy(f->bssid,bssid,6);
  if(ssid){ strncpy(f->ssid,ssid,32); f->ssid[32]=0; } else f->ssid[0]=0;
  f->channel=channel;
  f->timestamp_ms=millis();
  f->acknowledged=false;
  g_vs.finding_head=(g_vs.finding_head+1)%VS_MAX_FINDINGS;
  if(g_vs.finding_count<VS_MAX_FINDINGS) g_vs.finding_count++;
  g_vs.total_findings++;
  g_vs.unacked++;
}

// ── Default SSID patterns ─────────────────────────────────────────────────────
static const char* VS_DEFAULT_SSIDS[]={
  "NETGEAR","linksys","default","Wireless","HOME-","xfinitywifi",
  "DIRECT-","HP-Print","HUAWEI-","TP-LINK","TL-WR","Dlink","ASUS",
  "Archer","BrightHouse","Cox","Spectrum","att","verizon",nullptr
};

static bool vs_is_default_ssid(const char* ssid) {
  if(!ssid) return false;
  for(int i=0;VS_DEFAULT_SSIDS[i];i++){
    if(strncasecmp(ssid,VS_DEFAULT_SSIDS[i],strlen(VS_DEFAULT_SSIDS[i]))==0) return true;
  }
  return false;
}

// ── Scan one beacon/probe response ───────────────────────────────────────────
static void vs_scan_beacon(const uint8_t bssid[6], const char* ssid,
                             uint8_t channel, uint8_t enc_type,
                             bool pmf_cap, bool pmf_req,
                             bool has_tkip, bool has_wpa1,
                             bool has_wps, bool is_hidden) {
  // Upsert target
  VsTarget* t=nullptr;
  for(uint8_t i=0;i<g_vs.target_count;i++){
    if(memcmp(g_vs.targets[i].bssid,bssid,6)==0){t=&g_vs.targets[i];break;}
  }
  if(!t){
    if(g_vs.target_count>=VS_MAX_TARGETS) return;
    t=&g_vs.targets[g_vs.target_count++];
    memset(t,0,sizeof(*t));
    memcpy(t->bssid,bssid,6);
  }
  if(ssid){ strncpy(t->ssid,ssid,32); t->ssid[32]=0; }
  t->channel=channel; t->enc=(uint8_t)enc_type;
  t->has_pmf_cap=pmf_cap; t->has_pmf_req=pmf_req;
  t->has_tkip_group=has_tkip; t->has_wpa1=has_wpa1;
  t->has_wps=has_wps; t->is_hidden=is_hidden;
  t->last_scanned_ms=millis();

  // Check vulnerabilities
  if(enc_type==1){ // WEP
    vs_add_finding(VS_VULN_WEP,bssid,ssid,channel);
    t->vuln_count++;
  }
  if(enc_type==0){ // Open
    vs_add_finding(VS_VULN_OPEN,bssid,ssid,channel);
    t->vuln_count++;
  }
  if(enc_type>=3&&!pmf_cap){ // WPA2+ without PMF
    vs_add_finding(VS_VULN_NO_PMF,bssid,ssid,channel);
    t->vuln_count++;
  }
  if(enc_type>=3&&pmf_cap&&!pmf_req){
    vs_add_finding(VS_VULN_NO_MFP_REQUIRED,bssid,ssid,channel);
    t->vuln_count++;
  }
  if(has_tkip){
    vs_add_finding(VS_VULN_TKIP_ONLY,bssid,ssid,channel);
    t->vuln_count++;
  }
  if(has_wpa1&&enc_type<3){
    vs_add_finding(VS_VULN_WPA1_ONLY,bssid,ssid,channel);
    t->vuln_count++;
  }
  if(has_wps){
    vs_add_finding(VS_VULN_WPS_ENABLED,bssid,ssid,channel);
    t->vuln_count++;
  }
  if(ssid&&vs_is_default_ssid(ssid)){
    vs_add_finding(VS_VULN_DEFAULT_SSID,bssid,ssid,channel);
    t->vuln_count++;
  }
}

// ── Target JSON ───────────────────────────────────────────────────────────────
static String vs_targets_json(int page, int per) {
  if(per<1||per>50) per=20;
  int start=page*per, end=start+per;
  if(end>(int)g_vs.target_count) end=g_vs.target_count;
  String j="{\"total\":"+String(g_vs.target_count)+",\"targets\":[";
  for(int i=start;i<end;i++){
    if(i>start) j+=",";
    VsTarget& t=g_vs.targets[i];
    char mac[18];
    snprintf(mac,sizeof(mac),"%02X:%02X:%02X:%02X:%02X:%02X",
      t.bssid[0],t.bssid[1],t.bssid[2],t.bssid[3],t.bssid[4],t.bssid[5]);
    j+="{\"bssid\":\""; j+=mac; j+="\"";
    j+=",\"ssid\":\""; j+=t.ssid; j+="\"";
    j+=",\"ch\":"; j+=t.channel;
    j+=",\"enc\":"; j+=t.enc;
    j+=",\"pmf_cap\":"; j+=t.has_pmf_cap?1:0;
    j+=",\"pmf_req\":"; j+=t.has_pmf_req?1:0;
    j+=",\"vulns\":"; j+=t.vuln_count;
    j+=",\"tkip\":"; j+=t.has_tkip_group?1:0;
    j+=",\"wps\":"; j+=t.has_wps?1:0;
    j+="}";
  }
  j+="]}";
  return j;
}

// ── Findings JSON ─────────────────────────────────────────────────────────────
static String vs_findings_json(int max_n) {
  if(max_n<1||max_n>VS_MAX_FINDINGS) max_n=VS_MAX_FINDINGS;
  String j="{\"total\":"+String(g_vs.total_findings);
  j+=",\"unacked\":"+String(g_vs.unacked);
  j+=",\"findings\":[";
  int shown=0;
  int start=(g_vs.finding_head+VS_MAX_FINDINGS-g_vs.finding_count)%VS_MAX_FINDINGS;
  for(int i=g_vs.finding_count-1;i>=0&&shown<max_n;i--){
    int idx=(start+i)%VS_MAX_FINDINGS;
    if(shown) j+=",";
    VsFinding& f=g_vs.findings[idx];
    char mac[18];
    snprintf(mac,sizeof(mac),"%02X:%02X:%02X:%02X:%02X:%02X",
      f.bssid[0],f.bssid[1],f.bssid[2],f.bssid[3],f.bssid[4],f.bssid[5]);
    j+="{\"type\":"+String((int)f.type);
    j+=",\"name\":\""+String(vs_vuln_name(f.type))+"\"";
    j+=",\"sev\":\""+String(vs_sev_name(f.severity))+"\"";
    j+=",\"score\":"+String(f.score);
    j+=",\"bssid\":\""; j+=mac; j+="\"";
    j+=",\"ssid\":\""; j+=f.ssid; j+="\"";
    j+=",\"ch\":"+String(f.channel);
    j+=",\"fix\":\""; j+=vs_vuln_remediation(f.type); j+="\"";
    j+=",\"ts\":"+String(f.timestamp_ms);
    j+=",\"ack\":"+String(f.acknowledged?1:0);
    j+="}";
    shown++;
  }
  j+="]}";
  return j;
}

// ── Severity distribution ─────────────────────────────────────────────────────
static String vs_severity_dist_json() {
  uint16_t counts[5]={};
  int start=(g_vs.finding_head+VS_MAX_FINDINGS-g_vs.finding_count)%VS_MAX_FINDINGS;
  for(uint8_t i=0;i<g_vs.finding_count;i++){
    int idx=(start+i)%VS_MAX_FINDINGS;
    uint8_t sev=(uint8_t)g_vs.findings[idx].severity;
    if(sev<5) counts[sev]++;
  }
  String j="{\"critical\":"+String(counts[4]);
  j+=",\"high\":"+String(counts[3]);
  j+=",\"medium\":"+String(counts[2]);
  j+=",\"low\":"+String(counts[1]);
  j+=",\"info\":"+String(counts[0]);
  j+="}";
  return j;
}

// ── HTTP routes ───────────────────────────────────────────────────────────────
static void vs_routes() {
  extern WebServer server;

  server.on("/api/vs/findings",HTTP_GET,[](){
    int n=server.hasArg("n")?server.arg("n").toInt():20;
    server.send(200,"application/json",vs_findings_json(n));
  });

  server.on("/api/vs/targets",HTTP_GET,[](){
    int page=server.hasArg("page")?server.arg("page").toInt():0;
    int per =server.hasArg("per") ?server.arg("per").toInt() :20;
    server.send(200,"application/json",vs_targets_json(page,per));
  });

  server.on("/api/vs/severity",HTTP_GET,[](){
    server.send(200,"application/json",vs_severity_dist_json());
  });

  server.on("/api/vs/ack",HTTP_POST,[](){
    int start=(g_vs.finding_head+VS_MAX_FINDINGS-g_vs.finding_count)%VS_MAX_FINDINGS;
    for(uint8_t i=0;i<g_vs.finding_count;i++){
      int idx=(start+i)%VS_MAX_FINDINGS;
      g_vs.findings[idx].acknowledged=true;
    }
    g_vs.unacked=0;
    server.send(200,"application/json","{\"ok\":1}");
  });

  server.on("/api/vs/clear",HTTP_POST,[](){
    memset(&g_vs,0,sizeof(g_vs));
    server.send(200,"application/json","{\"ok\":1}");
  });

  server.on("/api/vs/summary",HTTP_GET,[](){
    char buf[256];
    snprintf(buf,sizeof(buf),
      "{\"targets\":%u,\"findings\":%u,\"unacked\":%u}",
      g_vs.target_count, (unsigned)g_vs.total_findings, (unsigned)g_vs.unacked);
    server.send(200,"application/json",buf);
  });
}

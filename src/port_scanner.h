#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <string.h>
#include <stdint.h>

// ── Port Scanner ──────────────────────────────────────────────────────────────
//
// TCP port scanner for connected clients on the AP subnet.
// Uses non-blocking connect with configurable timeout. Identifies
// common services by port number.

#define PS_MAX_HOSTS 16
#define PS_MAX_PORTS_PER 32
#define PS_MAX_RESULTS 256
#define PS_CONNECT_TIMEOUT_MS 800

// ── Scan state ────────────────────────────────────────────────────────────────
enum PsScanState : uint8_t {
  PS_SCAN_IDLE = 0,
  PS_SCAN_RUNNING = 1,
  PS_SCAN_DONE = 2,
  PS_SCAN_FAILED = 3,
};

// ── Port result ───────────────────────────────────────────────────────────────
struct PsPortResult {
  uint16_t port;
  bool open;
  uint32_t rtt_ms;
  char banner[48];  // grabbed if open
};

// ── Host result ───────────────────────────────────────────────────────────────
struct PsHostResult {
  char ip[16];
  uint32_t scan_start_ms;
  uint32_t scan_end_ms;
  PsPortResult ports[PS_MAX_PORTS_PER];
  uint8_t port_count;
  uint8_t open_count;
  bool alive;
};

// ── Well-known port table ──────────────────────────────────────────────────────
struct PsWellKnown {
  uint16_t port;
  const char *service;
  const char *description;
};

static const PsWellKnown PS_WELL_KNOWN[] = {{21, "FTP", "File Transfer Protocol"},
                                            {22, "SSH", "Secure Shell"},
                                            {23, "Telnet", "Telnet remote login"},
                                            {25, "SMTP", "Simple Mail Transfer"},
                                            {53, "DNS", "Domain Name System"},
                                            {80, "HTTP", "Hypertext Transfer"},
                                            {110, "POP3", "Post Office Protocol"},
                                            {135, "MSRPC", "Microsoft RPC"},
                                            {139, "NetBIOS", "NetBIOS Session"},
                                            {143, "IMAP", "Internet Message Access"},
                                            {161, "SNMP", "Simple Network Mgmt"},
                                            {443, "HTTPS", "HTTP over TLS"},
                                            {445, "SMB", "Server Message Block"},
                                            {554, "RTSP", "Real Time Stream"},
                                            {587, "SMTP", "SMTP Submission"},
                                            {623, "IPMI", "Intelligent Platform Mgmt"},
                                            {631, "IPP", "Internet Printing"},
                                            {993, "IMAPS", "IMAP over TLS"},
                                            {995, "POP3S", "POP3 over TLS"},
                                            {1080, "SOCKS", "SOCKS Proxy"},
                                            {1433, "MSSQL", "MS SQL Server"},
                                            {1723, "PPTP", "Point-to-Point Tunneling"},
                                            {1900, "UPnP", "Universal Plug and Play"},
                                            {2049, "NFS", "Network File System"},
                                            {2375, "Docker", "Docker daemon"},
                                            {3000, "HTTP", "HTTP alt / Node.js"},
                                            {3306, "MySQL", "MySQL Database"},
                                            {3389, "RDP", "Remote Desktop Protocol"},
                                            {4444, "Metasploit", "Metasploit default"},
                                            {4567, "HTTP", "HTTP alt"},
                                            {5000, "HTTP", "HTTP alt / Flask"},
                                            {5432, "Postgres", "PostgreSQL Database"},
                                            {5555, "ADB", "Android Debug Bridge"},
                                            {5900, "VNC", "Virtual Network Computing"},
                                            {5985, "WinRM", "Windows Remote Mgmt"},
                                            {6379, "Redis", "Redis key-value store"},
                                            {7070, "HTTP", "HTTP alt"},
                                            {7547, "TR-069", "CWMP Router Mgmt"},
                                            {8080, "HTTP", "HTTP alternate"},
                                            {8443, "HTTPS", "HTTPS alternate"},
                                            {8888, "HTTP", "HTTP alt / Jupyter"},
                                            {9000, "HTTP", "HTTP alt / PHP-FPM"},
                                            {9090, "HTTP", "Prometheus / Cockpit"},
                                            {9200, "HTTP", "Elasticsearch"},
                                            {10000, "Webmin", "Webmin admin panel"},
                                            {27017, "MongoDB", "MongoDB Database"},
                                            {0, nullptr, nullptr}};

static const char *ps_service_name(uint16_t port) {
  for (int i = 0; PS_WELL_KNOWN[i].service; i++) {
    if (PS_WELL_KNOWN[i].port == port)
      return PS_WELL_KNOWN[i].service;
  }
  return nullptr;
}

static const char *ps_service_desc(uint16_t port) {
  for (int i = 0; PS_WELL_KNOWN[i].description; i++) {
    if (PS_WELL_KNOWN[i].port == port)
      return PS_WELL_KNOWN[i].description;
  }
  return nullptr;
}

// ── Default port list ──────────────────────────────────────────────────────────
static const uint16_t PS_DEFAULT_PORTS[] = {
    21,   22,   23,   25,   53,   80,   110,  135,  139,  143,  443,   445,   554,  587,
    631,  993,  995,  1080, 1433, 1723, 1900, 2049, 2375, 3000, 3306,  3389,  4444, 5000,
    5432, 5555, 5900, 5985, 6379, 7547, 8080, 8443, 8888, 9200, 10000, 27017, 0};

static uint16_t ps_default_port_count() {
  uint16_t c = 0;
  while (PS_DEFAULT_PORTS[c])
    c++;
  return c;
}

// ── Scanner state ──────────────────────────────────────────────────────────────
struct PsState {
  PsHostResult results[PS_MAX_HOSTS];
  uint8_t result_count;
  PsScanState state;
  char current_ip[16];
  uint8_t current_host_idx;
  uint32_t scan_start_ms;
  uint32_t total_scanned;
  uint32_t total_open;
  char target_ips[PS_MAX_HOSTS][16];
  uint8_t target_count;
};

static PsState g_ps = {};

// ── Banner grab ───────────────────────────────────────────────────────────────
static void ps_grab_banner(const char *ip, uint16_t port, char *banner, size_t banner_sz) {
  banner[0] = 0;
  WiFiClient c;
  if (!c.connect(ip, port, 500))
    return;
  uint32_t t = millis();
  while (millis() - t < 400 && !c.available())
    delay(10);
  int n = 0;
  while (c.available() && n < (int)banner_sz - 1) {
    char b = c.read();
    if (b == '\r' || b == '\n' || b < 0x20)
      break;
    banner[n++] = b;
  }
  banner[n] = 0;
  c.stop();
}

// ── Scan one port (blocking, short timeout) ───────────────────────────────────
static bool ps_scan_port(const char *ip, uint16_t port, uint32_t *rtt_out, char *banner,
                         size_t banner_sz) {
  WiFiClient c;
  uint32_t t0 = millis();
  bool ok = c.connect(ip, port, PS_CONNECT_TIMEOUT_MS);
  uint32_t rtt = millis() - t0;
  if (rtt_out)
    *rtt_out = rtt;
  if (ok) {
    banner[0] = 0;
    // Try quick banner grab for text services
    uint32_t t = millis();
    while (millis() - t < 250 && !c.available())
      delay(5);
    int n = 0;
    while (c.available() && n < (int)banner_sz - 1) {
      char b = c.read();
      if (b == '\r' || b == '\n')
        break;
      if (b >= 0x20)
        banner[n++] = b;
    }
    banner[n] = 0;
    c.stop();
    return true;
  }
  c.stop();
  return false;
}

// ── Scan single host ──────────────────────────────────────────────────────────
static PsHostResult ps_scan_host(const char *ip, const uint16_t *ports, uint8_t port_count) {
  PsHostResult r;
  memset(&r, 0, sizeof(r));
  strncpy(r.ip, ip, 15);
  r.ip[15] = 0;
  r.scan_start_ms = millis();

  // Quick alive check on port 80
  r.alive = false;
  WiFiClient c;
  if (c.connect(ip, 80, 300) || c.connect(ip, 443, 300)) {
    r.alive = true;
    c.stop();
  } else {
    // Try ICMP-like via TCP
    if (c.connect(ip, 22, 300) || c.connect(ip, 23, 300)) {
      r.alive = true;
      c.stop();
    }
  }

  for (uint8_t i = 0; i < port_count && i < PS_MAX_PORTS_PER; i++) {
    uint32_t rtt = 0;
    char banner[48] = "";
    bool open = ps_scan_port(ip, ports[i], &rtt, banner, sizeof(banner));
    r.ports[r.port_count].port = ports[i];
    r.ports[r.port_count].open = open;
    r.ports[r.port_count].rtt_ms = rtt;
    strncpy(r.ports[r.port_count].banner, banner, 47);
    if (open) {
      r.open_count++;
      r.alive = true;
    }
    r.port_count++;
    delay(10);  // yield to OS
  }
  r.scan_end_ms = millis();
  return r;
}

// ── Add target IP ─────────────────────────────────────────────────────────────
static void ps_add_target(const char *ip) {
  if (g_ps.target_count >= PS_MAX_HOSTS)
    return;
  strncpy(g_ps.target_ips[g_ps.target_count++], ip, 15);
}

// ── Clear targets ─────────────────────────────────────────────────────────────
static void ps_clear_targets() {
  g_ps.target_count = 0;
}

// ── Scan all targets (blocking — run in task) ─────────────────────────────────
static void ps_scan_all(const uint16_t *ports, uint8_t port_count) {
  g_ps.state = PS_SCAN_RUNNING;
  g_ps.result_count = 0;
  g_ps.total_open = 0;
  g_ps.scan_start_ms = millis();

  for (uint8_t i = 0; i < g_ps.target_count && g_ps.result_count < PS_MAX_HOSTS; i++) {
    strncpy(g_ps.current_ip, g_ps.target_ips[i], 15);
    PsHostResult r = ps_scan_host(g_ps.target_ips[i], ports, port_count);
    g_ps.results[g_ps.result_count++] = r;
    g_ps.total_scanned++;
    g_ps.total_open += r.open_count;
  }
  g_ps.state = PS_SCAN_DONE;
}

// ── Scan subnet (192.168.4.1-254) ─────────────────────────────────────────────
static void ps_scan_subnet(const char *base_ip) {
  ps_clear_targets();
  char ip[16];
  for (int i = 2; i <= 10; i++) {  // scan only .2-.10 to avoid blocking too long
    snprintf(ip, sizeof(ip), "%s.%d", base_ip, i);
    ps_add_target(ip);
  }
}

// ── Results JSON ──────────────────────────────────────────────────────────────
static String ps_results_json() {
  String j = "{\"state\":" + String((int)g_ps.state);
  j += ",\"total_scanned\":" + String(g_ps.total_scanned);
  j += ",\"total_open\":" + String(g_ps.total_open);
  j += ",\"results\":[";
  for (uint8_t i = 0; i < g_ps.result_count; i++) {
    if (i)
      j += ",";
    PsHostResult &r = g_ps.results[i];
    j += "{\"ip\":\"";
    j += r.ip;
    j += "\"";
    j += ",\"alive\":";
    j += r.alive ? 1 : 0;
    j += ",\"open_count\":";
    j += r.open_count;
    j += ",\"scan_ms\":";
    j += r.scan_end_ms - r.scan_start_ms;
    j += ",\"ports\":[";
    bool first_port = true;
    for (uint8_t p = 0; p < r.port_count; p++) {
      if (!r.ports[p].open)
        continue;
      if (!first_port)
        j += ",";
      first_port = false;
      PsPortResult &pr = r.ports[p];
      const char *svc = ps_service_name(pr.port);
      j += "{\"port\":";
      j += pr.port;
      j += ",\"service\":\"";
      j += (svc ? svc : "unknown");
      j += "\"";
      j += ",\"rtt\":";
      j += pr.rtt_ms;
      if (pr.banner[0]) {
        j += ",\"banner\":\"";
        j += pr.banner;
        j += "\"";
      }
      j += "}";
    }
    j += "]}";
  }
  j += "]}";
  return j;
}

// ── HTTP routes ───────────────────────────────────────────────────────────────
static void ps_routes() {
  extern WebServer server;

  server.on("/api/ps/scan", HTTP_POST, []() {
    String ip_arg = server.hasArg("ip") ? server.arg("ip") : "";
    if (ip_arg.length() == 0 || ip_arg == "subnet") {
      ps_scan_subnet("192.168.4");
    } else {
      ps_clear_targets();
      ps_add_target(ip_arg.c_str());
    }
    // Scan top-20 ports for a quick result
    static const uint16_t quick_ports[] = {21,   22,   23, 80,  443, 445,  3389,  8080, 8443, 3306,
                                           5432, 5900, 22, 139, 135, 1433, 27017, 6379, 5000, 9200};
    ps_scan_all(quick_ports, 20);
    server.send(200, "application/json", ps_results_json());
  });

  server.on("/api/ps/results", HTTP_GET,
            []() { server.send(200, "application/json", ps_results_json()); });

  server.on("/api/ps/status", HTTP_GET, []() {
    char buf[128];
    snprintf(buf, sizeof(buf),
             "{\"state\":%d,\"current_ip\":\"%s\",\"total_scanned\":%u,\"total_open\":%u}",
             (int)g_ps.state, g_ps.current_ip, (unsigned)g_ps.total_scanned,
             (unsigned)g_ps.total_open);
    server.send(200, "application/json", buf);
  });

  server.on("/api/ps/services", HTTP_GET, []() {
    String j = "{\"services\":[";
    for (int i = 0; PS_WELL_KNOWN[i].service; i++) {
      if (i)
        j += ",";
      j += "{\"port\":";
      j += PS_WELL_KNOWN[i].port;
      j += ",\"name\":\"";
      j += PS_WELL_KNOWN[i].service;
      j += "\"";
      j += ",\"desc\":\"";
      j += PS_WELL_KNOWN[i].description;
      j += "\"";
      j += "}";
    }
    j += "]}";
    server.send(200, "application/json", j);
  });

  server.on("/api/ps/clear", HTTP_POST, []() {
    memset(&g_ps, 0, sizeof(g_ps));
    server.send(200, "application/json", "{\"ok\":1}");
  });
}

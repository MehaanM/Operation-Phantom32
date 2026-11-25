#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <Esp.h>
#include "psram_attr.h"

// ---------- output helpers ----------
static String cli_esc(const String &s) {
  String o;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') {
      o += '\\';
      o += c;
    } else if (c == '\n')
      o += "\\n";
    else if (c == '\r')
      o += "\\r";
    else if (c == '\t')
      o += "\\t";
    else if ((unsigned char)c >= 0x20)
      o += c;
  }
  return o;
}
static String cli_ok(const String &s) {
  return "{\"ok\":1,\"out\":\"" + cli_esc(s) + "\"}";
}
static String cli_err(const String &s) {
  return "{\"ok\":0,\"err\":\"> " + cli_esc(s) + "\"}";
}

static int cli_tok(const String &s, String *out, int max) {
  int i = 0, n = s.length(), c = 0;
  while (i < n && c < max) {
    while (i < n && isspace((unsigned char)s[i]))
      i++;
    if (i >= n)
      break;
    String t;
    bool q = false;
    char qc = 0;
    while (i < n) {
      char ch = s[i];
      if (!q && isspace((unsigned char)ch))
        break;
      if (!q && (ch == '"' || ch == '\'')) {
        q = true;
        qc = ch;
        i++;
        continue;
      }
      if (q && ch == qc) {
        q = false;
        i++;
        continue;
      }
      t += ch;
      i++;
    }
    out[c++] = t;
  }
  return c;
}

// ---------- aliases ----------
struct CliAlias {
  const char *alias;
  const char *cmd;
};
static const CliAlias cli_aliases[] = {
    {"?", "help"},
    {"h", "help"},
    {"ls", "modules"},
    {"list", "modules"},
    {"st", "status"},
    {"info", "status"},
    {"free", "heap"},
    {"mem", "heap"},
    {"up", "uptime"},
    {"halt", "stop"},
    {"kill", "stop"},
    {"wifi", "scan"},
    {"d", "deauth"},
    {"b", "beacon"},
    {"p", "probe"},
    {"hs", "handshake"},
    {"ep", "portal"},
    {"hp", "honeypot"},
    {"bt", "ble"},
    {"ids", "aegis"},
    {"storm", "stormdeauth"},
    {"pw", "pwnface"},
    {"harvest", "probeharvest"},
    {"btm", "btmflood"},
    {"eap", "eapflood"},
    {"null", "nullflood"},
    {"mgt", "mgtflood"},
    {"md", "multideauth"},
    {"af", "actionflood"},
    {"wpa3", "wpa3flood"},
    {"et", "eviltwin"},
    {"csa", "csa"},
    {"esp", "espnow"},
    {"chd", "chanhop"},
    {"auth", "authflood"},
    {"nav", "navflood"},
    {"pmkid", "pmkid"},
    {"adv", "beaconadv"},
};
static String cli_resolve_alias(const String &in) {
  for (size_t i = 0; i < sizeof(cli_aliases) / sizeof(cli_aliases[0]); i++)
    if (in == cli_aliases[i].alias)
      return String(cli_aliases[i].cmd);
  return in;
}

// ---------- command metadata ----------
struct CliMeta {
  const char *cat;
  const char *name;
  const char *desc;
  const char *usage;
  const char *args;      // multi-line, each line already prefixed with ">    "
  const char *examples;  // multi-line, each line already prefixed with ">    "
};

static const CliMeta cli_meta[] = {
    // ---- core ----
    {"core", "help", "Display help information for commands.", "help [category|command|all]", "",
     ">    help\n>    help wifi\n>    help deauth\n>    help all"},
    {"core", "status", "Show heap, uptime, active tool and tool count.", "status", "",
     ">    status"},
    {"core", "modules", "List all registered tools with their slug.", "modules", "",
     ">    modules"},
    {"core", "heap", "Show free heap in bytes.", "heap", "", ">    heap"},
    {"core", "uptime", "Show seconds since boot.", "uptime", "", ">    uptime"},
    {"core", "start", "Start a tool by slug with optional arguments.",
     "start <slug> [key=value ...]", ">    slug - Tool name from 'modules'",
     ">    start deauth bssid=AA:BB:CC:DD:EE:FF ch=6\n>    start beacon ssid=TestNet count=100"},
    {"core", "stop", "Stop the active tool, or stop a named tool.", "stop [slug]",
     ">    slug - Optional. Omit to stop the active tool.", ">    stop\n>    stop deauth"},
    {"core", "reboot", "Soft-restart the ESP32.", "reboot", "", ">    reboot"},

    // ---- wifi ----
    {"wifi", "scan", "List APs from Aegis cache, or trigger a live scan.", "scan [refresh]",
     ">    (no arg) - read cached results from Aegis, no connection drop\n>    refresh - force "
     "live scan (connection drops ~3s)",
     ">    scan\n>    scan refresh"},
    {"wifi", "deauth", "Send 802.11 deauthentication frames.",
     "deauth <bssid> [ch=N] [reason=N] [broadcast=1]",
     ">    bssid - Target AP MAC address (required)\n>    ch - Channel (default: current)\n>    "
     "reason - Deauth reason code: 7=Class3, 1=Unspec, 3=Leave\n>    broadcast - 1=all clients, "
     "0=single STA",
     ">    deauth AA:BB:CC:DD:EE:FF\n>    deauth AA:BB:CC:DD:EE:FF ch=6 reason=7\n>    deauth "
     "AA:BB:CC:DD:EE:FF ch=6 broadcast=1"},
    {"wifi", "beacon", "Broadcast fake beacon frames on channels 1-13.",
     "beacon <ssid> [count=N] [random=1]",
     ">    ssid - Fake network name\n>    count - Number of beacons to send (default: 200)\n>    "
     "random - 1=random BSSIDs per frame, 0=fixed",
     ">    beacon \"Free WiFi\"\n>    beacon TestNet count=500 random=1"},
    {"wifi", "probe", "Flood probe-request frames across all channels.", "probe <ssid> [count=N]",
     ">    ssid - SSID to probe for\n>    count - Number of probes (default: 500)",
     ">    probe TestNet\n>    probe \"Starbucks\" count=1000"},
    {"wifi", "handshake", "Capture EAPOL frames to PCAP, optionally force re-handshake.",
     "handshake <bssid> [ch=N] [force=1]",
     ">    bssid - Target AP MAC address\n>    ch - Channel (default: 6)\n>    force - 1=send 20 "
     "deauth frames first to force re-handshake",
     ">    handshake AA:BB:CC:DD:EE:FF\n>    handshake AA:BB:CC:DD:EE:FF ch=11 force=1\n>    PCAP "
     "at /api/handshake/pcap"},
    {"wifi", "portal", "Open a captive portal that captures credentials.", "portal <ssid>",
     ">    ssid - SSID the portal AP will advertise",
     ">    portal \"Hotel WiFi\"\n>    Creds at /api/portal/creds"},

    // ---- ble ----
    {"ble", "ble", "Start BLE advertisement spam in the given mode.", "ble <mode>",
     ">    mode - apple | android | windows | samsung | airtag |\n>           ibeacon | eddystone "
     "| names | scan",
     ">    ble apple\n>    ble airtag\n>    ble names"},

    // ---- comm ----
    {"comm", "i2c", "Act as an I2C slave and log every byte from the master.",
     "i2c [sda=N] [scl=N] [addr=0xNN]",
     ">    sda - SDA pin (default: 21)\n>    scl - SCL pin (default: 22)\n>    addr - Slave "
     "address (default: 0x50)",
     ">    i2c\n>    i2c sda=21 scl=22 addr=0x3C\n>    Log at /api/i2c/log"},
    {"comm", "spi", "Sample MOSI on SPI clock edges (ISR-safe).",
     "spi [clk=N] [mosi=N] [miso=N] [cs=N]",
     ">    clk - Clock pin (default: 18)\n>    mosi - MOSI pin (default: 23)\n>    miso - MISO pin "
     "(default: 19)\n>    cs - Chip select (default: 5)",
     ">    spi\n>    spi clk=18 mosi=23 cs=5\n>    Log at /api/spi/log"},
    {"comm", "uart", "Bridge UART1 to USB serial.", "uart [rx=N] [tx=N] [baud=N]",
     ">    rx - RX pin (default: 16)\n>    tx - TX pin (default: 17)\n>    baud - Baud rate "
     "(default: 115200)",
     ">    uart\n>    uart rx=16 tx=17 baud=9600"},

    // ---- honeypot ----
    {"honeypot", "honeypot", "TCP listeners on 22/80/2323 with fake banners.", "honeypot", "",
     ">    honeypot\n>    Log at /api/honeypot/log"},

    // ---- defense ----
    {"defense", "aegis", "Show Aegis IDS summary.", "aegis", "", ">    aegis"},
    {"defense", "clients", "List nearby client devices seen by Aegis.", "clients", "",
     ">    clients"},
    {"defense", "events", "Show recent Aegis events.", "events", "", ">    events"},
    {"defense", "attackers", "Ranked deauth/disassoc attackers.", "attackers", "",
     ">    attackers"},
    {"defense", "channels", "Per-channel management-frame utilization.", "channels", "",
     ">    channels"},
    {"defense", "trust", "Set the trusted AP to watch for impersonation.",
     "trust <ssid> <bssid> <ch>  |  trust <bssid>",
     ">    ssid - Network name\n>    bssid - Access point MAC\n>    ch - Channel",
     ">    trust HomeNet AA:BB:CC:DD:EE:FF 6\n>    trust AA:BB:CC:DD:EE:FF"},
    {"defense", "untrust", "Clear the trusted AP.", "untrust", "", ">    untrust"},
    {"defense", "trustclient", "Mark a client MAC as trusted.", "trustclient <mac>",
     ">    mac - Client MAC address", ">    trustclient 11:22:33:44:55:66"},
    {"defense", "untrustclient", "Remove a client from the trusted list.", "untrustclient <mac>",
     ">    mac - Client MAC address", ">    untrustclient 11:22:33:44:55:66"},

    // ---- safety ----
    {"safety", "safety", "View or set rate limits and auto-stop.",
     "safety [dps=N] [bps=N] [pps=N] [ast=N]",
     ">    dps - Deauth frames per second (default: 100)\n>    bps - Beacon frames per second "
     "(default: 200)\n>    pps - Probe frames per second (default: 300)\n>    ast - Auto-stop "
     "seconds (0=off)",
     ">    safety\n>    safety ast=30\n>    safety dps=50 bps=100 pps=200 ast=15"},
    {"safety", "block", "Add a BSSID to the blocklist.", "block <mac>",
     ">    mac - BSSID to block from deauth targets", ">    block AA:BB:CC:DD:EE:FF"},
    {"safety", "unblock", "Clear the BSSID blocklist.", "unblock", "", ">    unblock"},

    // ---- persist ----
    {"persist", "profile", "Save/load/delete tool profiles.", "profile <save|load|del> [name]",
     ">    save - Save the current tool state\n>    load - Restore a saved profile\n>    del - "
     "Delete a profile",
     ">    profile save my_attack\n>    profile load my_attack"},
    {"persist", "schedule", "Schedule stub.", "schedule <add|list|del|run> ...", "",
     ">    schedule list"},

    // ---- diag ----
    {"diag", "diag", "Diagnostics: channel map, heatmap, clients, IEs, packets.",
     "diag <channel|heatmap|clients|ies <bssid>|packets>",
     ">    channel - Per-channel frame counts\n>    heatmap - RSSI histogram\n>    clients - "
     "Client inventory with vendor\n>    ies - IE decoder for a BSSID\n>    packets - Recent mgmt "
     "frames",
     ">    diag channel\n>    diag heatmap\n>    diag ies AA:BB:CC:DD:EE:FF"},

    // ---- advanced wifi attacks ----
    {"attack", "eviltwin", "Rogue AP + deauth + DNS + credential capture.",
     "eviltwin <bssid> [ssid=<name>] [ch=N]",
     ">    bssid - Target AP to clone\n>    ssid - Override SSID (default: clone target)\n>    ch "
     "- Channel (default: clone target)",
     ">    eviltwin AA:BB:CC:DD:EE:FF\n>    eviltwin AA:BB:CC:DD:EE:FF ssid=HomeNet ch=6\n>    "
     "Creds at /api/eviltwin/creds"},
    {"attack", "authflood", "Auth/assoc/disassoc flood to exhaust AP association tables.",
     "authflood bssid=<mac> [ch=N] [mode=auth|assoc|disassoc]",
     ">    bssid - Target AP MAC\n>    ch - Channel (default: 6)\n>    mode - Frame type to flood "
     "(default: auth)",
     ">    authflood bssid=AA:BB:CC:DD:EE:FF\n>    authflood bssid=AA:BB:CC:DD:EE:FF ch=11 "
     "mode=assoc"},
    {"attack", "karma", "KARMA probe response spoofing — respond to all probe requests.",
     "karma [ssid=<name>]", ">    ssid - Specific SSID to spoof (default: respond to all probes)",
     ">    karma\n>    karma ssid=FreeWiFi"},
    {"attack", "navflood", "CTS/RTS NAV (virtual carrier sense) DoS.", "navflood [ch=N] [dur=N]",
     ">    ch - Channel (default: 6)\n>    dur - NAV duration value 0-32767 (default: 32767)",
     ">    navflood\n>    navflood ch=1 dur=32767"},
    {"attack", "csa", "Channel Switch Announcement + BTM Request to force client migration.",
     "csa bssid=<mac> [ch=N] [target=N]",
     ">    bssid - Target AP\n>    ch - Current channel\n>    target - Switch-to channel (default: "
     "13)",
     ">    csa bssid=AA:BB:CC:DD:EE:FF ch=6 target=1"},
    {"attack", "espnow", "ESP-NOW broadcast flood to disrupt ESP-NOW mesh networks.",
     "espnow [count=N]", ">    count - Frames to send (default: continuous)",
     ">    espnow\n>    espnow count=1000"},
    {"attack", "beaconadv", "Advanced beacon injection: WPS/fuzz/overflow/TKIP/WDS frames.",
     "beaconadv [mode=wps|fuzz|overflow|tkip|wds]", ">    mode - Frame variant (default: wps)",
     ">    beaconadv mode=wps\n>    beaconadv mode=fuzz"},
    {"attack", "pmkid", "Active PMKID extraction via association — saves to PCAP on flash.",
     "pmkid bssid=<mac> [ch=N]", ">    bssid - Target AP MAC\n>    ch - Channel (default: 6)",
     ">    pmkid bssid=AA:BB:CC:DD:EE:FF ch=6\n>    PCAP at /api/pmkid/pcap"},
    {"attack", "wpa3flood", "WPA3 SAE Commit/Confirm flood to exhaust AP state machine.",
     "wpa3flood bssid=<mac> [ch=N] [mode=commit|confirm|both|transition]",
     ">    bssid - Target WPA3 AP MAC\n>    ch - Channel (default: 6)\n>    mode - Attack mode "
     "(default: commit)",
     ">    wpa3flood bssid=AA:BB:CC:DD:EE:FF ch=6\n>    wpa3flood bssid=AA:BB:CC:DD:EE:FF "
     "mode=transition"},
    {"attack", "chanhop", "All-channel rotating deauth sweep across channels 1-13.",
     "chanhop [bssid=<mac>] [dwell=N]",
     ">    bssid - Target AP (default: broadcast all)\n>    dwell - Ms per channel (default: 200)",
     ">    chanhop\n>    chanhop bssid=AA:BB:CC:DD:EE:FF dwell=100"},
    {"attack", "btmflood", "802.11v BSS Transition Management flood to force STA roaming.",
     "btmflood bssid=<mac> [ch=N] [sta=<mac>]",
     ">    bssid - Source AP MAC\n>    ch - Channel (default: 6)\n>    sta - Target client "
     "(default: broadcast)",
     ">    btmflood bssid=AA:BB:CC:DD:EE:FF\n>    btmflood bssid=AA:BB:CC:DD:EE:FF "
     "sta=11:22:33:44:55:66"},
    {"attack", "eapflood", "WPA Enterprise EAPOL identity/PEAP state exhaustion flood.",
     "eapflood bssid=<mac> [ch=N]",
     ">    bssid - Target WPA-Enterprise AP MAC\n>    ch - Channel (default: 6)",
     ">    eapflood bssid=AA:BB:CC:DD:EE:FF ch=6"},
    {"attack", "nullflood", "Power-save null/QoS-null/BAR frame confusion flood.",
     "nullflood bssid=<mac> [ch=N] [mode=null|qos|bar]",
     ">    bssid - Target AP\n>    ch - Channel (default: 6)\n>    mode - Frame type (default: "
     "null)",
     ">    nullflood bssid=AA:BB:CC:DD:EE:FF\n>    nullflood bssid=AA:BB:CC:DD:EE:FF mode=qos"},
    {"attack", "mgtflood", "All-subtype management frame spray across all 13 channels.",
     "mgtflood [bssid=<mac>] [ch=N]",
     ">    bssid - Target AP (default: broadcast)\n>    ch - Channel (default: rotate)",
     ">    mgtflood\n>    mgtflood bssid=AA:BB:CC:DD:EE:FF ch=6"},
    {"attack", "multideauth", "Deauth up to 8 BSSIDs simultaneously.",
     "multideauth bssid=<mac>[,<mac>...] [ch=N]",
     ">    bssid - Comma-separated list of up to 8 AP MACs\n>    ch - Channel (default: 6)",
     ">    multideauth bssid=AA:BB:CC:DD:EE:FF,11:22:33:44:55:66\n>    md "
     "bssid=AA:BB:CC:DD:EE:FF,11:22:33:44:55:66 ch=11"},
    {"attack", "actionflood", "802.11 action frame category spray (all categories 0-127).",
     "actionflood bssid=<mac> [ch=N]", ">    bssid - Target AP\n>    ch - Channel (default: 6)",
     ">    actionflood bssid=AA:BB:CC:DD:EE:FF\n>    af bssid=AA:BB:CC:DD:EE:FF ch=1"},
    {"attack", "stormdeauth", "Multi-vector WPA2+WPA3 deauth storm (8 attack vectors in rotation).",
     "stormdeauth bssid=<mac> [ch=N] [broadcast=0|1] [sta=<mac>]",
     ">    bssid - Target AP MAC\n>    ch - Channel (default: 6)\n>    broadcast - 1=all clients, "
     "0=single STA\n>    sta - Target client MAC (when broadcast=0)",
     ">    stormdeauth bssid=AA:BB:CC:DD:EE:FF\n>    storm bssid=AA:BB:CC:DD:EE:FF ch=6 "
     "broadcast=1\n>    storm bssid=AA:BB:CC:DD:EE:FF sta=11:22:33:44:55:66 broadcast=0"},

    // ---- recon ----
    {"recon", "pwnface", "Autonomous UCB-guided handshake hunter (Pwnagotchi-inspired).", "pwnface",
     "",
     ">    pwnface\n>    pw\n>    Live UI at /pwnface\n>    Hops channels 1-13, deauths APs, "
     "captures EAPOL"},
    {"recon", "probeharvest",
     "Passive probe request sniffer — reveals device saved network history.", "probeharvest", "",
     ">    probeharvest\n>    harvest\n>    Results at /api/probeharvest/results\n>    Logs STA "
     "MAC + SSID being probed for"},
};

static const char *cli_categories[] = {"core",   "wifi",    "ble",  "comm",   "honeypot", "defense",
                                       "safety", "persist", "diag", "attack", "recon"};
static const char *cli_cat_desc[] = {
    "system & session",
    "WiFi scanning & basic offense",
    "Bluetooth Low Energy",
    "I2C / SPI / UART bridges",
    "TCP listener",
    "passive IDS & trust",
    "rate limits & blocklist",
    "profiles & schedules",
    "channel map, IEs, packets",
    "advanced WiFi attack modules",
    "passive reconnaissance",
};

// ---------- GhostESP-style renderers (every line prefixed with '> ') ----------
static String cli_help_categories() {
  String s;
  s += "> ESP32 DOOM CLI\n";
  s += "> Available commands:\n";
  for (size_t i = 0; i < sizeof(cli_categories) / sizeof(cli_categories[0]); i++) {
    s += ">   ";
    s += cli_categories[i];
    int pad = 12 - strlen(cli_categories[i]);
    for (int k = 0; k < pad; k++)
      s += ' ';
    s += "- ";
    s += cli_cat_desc[i];
    s += "\n";
  }
  s += ">\n";
  s += "> Type 'help <category>' for a category, 'help <command>' for details,\n";
  s += "> or 'help all' to list every command.\n";
  return cli_ok(s);
}

static String cli_help_category(const String &cat) {
  String s;
  s += "> Category: ";
  s += cat;
  s += "\n>\n";
  int n = 0;
  for (size_t i = 0; i < sizeof(cli_meta) / sizeof(cli_meta[0]); i++) {
    if (cat == cli_meta[i].cat) {
      n++;
      s += ">   ";
      s += cli_meta[i].name;
      int pad = 16 - strlen(cli_meta[i].name);
      if (pad < 1)
        pad = 1;
      for (int k = 0; k < pad; k++)
        s += ' ';
      s += "- ";
      s += cli_meta[i].desc;
      s += "\n";
    }
  }
  if (!n)
    return cli_err("unknown category: '" + cat + "' (try 'help')");
  return cli_ok(s);
}

static String cli_help_command(const String &cmd) {
  for (size_t i = 0; i < sizeof(cli_meta) / sizeof(cli_meta[0]); i++) {
    if (cmd == cli_meta[i].name) {
      String s;
      s += "> ";
      s += cmd;
      s += " - ";
      s += cli_meta[i].desc;
      s += "\n";
      s += "> Usage: ";
      s += cli_meta[i].usage;
      s += "\n";
      if (cli_meta[i].args && strlen(cli_meta[i].args)) {
        s += ">  Arguments:\n";
        s += cli_meta[i].args;
        s += "\n";
      }
      if (cli_meta[i].examples && strlen(cli_meta[i].examples)) {
        s += ">  Examples:\n";
        s += cli_meta[i].examples;
        s += "\n";
      }
      return cli_ok(s);
    }
  }
  return cli_err("no help for '" + cmd + "'");
}

static String cli_help_all() {
  String s;
  for (size_t c = 0; c < sizeof(cli_categories) / sizeof(cli_categories[0]); c++) {
    s += "> == ";
    s += cli_categories[c];
    s += " ==\n";
    for (size_t i = 0; i < sizeof(cli_meta) / sizeof(cli_meta[0]); i++) {
      if (strcmp(cli_categories[c], cli_meta[i].cat) == 0) {
        s += ">   ";
        s += cli_meta[i].name;
        int pad = 16 - strlen(cli_meta[i].name);
        if (pad < 1)
          pad = 1;
        for (int k = 0; k < pad; k++)
          s += ' ';
        s += "- ";
        s += cli_meta[i].desc;
        s += "\n";
      }
    }
    s += ">\n";
  }
  return cli_ok(s);
}

// ---------- dispatcher ----------
static String cli_dispatch(const String &input) {
  String toks[16];
  int n = cli_tok(input, toks, 16);
  if (n == 0)
    return "{\"ok\":1,\"out\":\"\"}";

  String cmd = cli_resolve_alias(toks[0]);
  cmd.toLowerCase();

  // help — GhostESP style
  if (cmd == "help") {
    if (n < 2)
      return cli_help_categories();
    String topic = toks[1];
    topic.toLowerCase();
    if (topic == "all")
      return cli_help_all();
    for (size_t i = 0; i < sizeof(cli_categories) / sizeof(cli_categories[0]); i++)
      if (topic == cli_categories[i])
        return cli_help_category(topic);
    for (size_t i = 0; i < sizeof(cli_meta) / sizeof(cli_meta[0]); i++)
      if (topic == cli_meta[i].name)
        return cli_help_command(topic);
    return cli_err("no help for '" + topic + "' (try 'help')");
  }

  // --help on any command
  if (n >= 2 && toks[1] == "--help") {
    return cli_help_command(cmd);
  }

  // kv vs positional split
  String kvs, pos[8];
  int npos = 0;
  for (int i = 1; i < n; i++) {
    int eq = toks[i].indexOf('=');
    if (eq > 0) {
      if (kvs.length())
        kvs += "&";
      kvs += toks[i];
    } else if (npos < 8)
      pos[npos++] = toks[i];
  }

  if (cmd == "status") {
    String s;
    s += "> heap:   " + String(ESP.getFreeHeap()) + " bytes\n";
    s += "> uptime: " + String(millis() / 1000) + " s\n";
    s += "> active: " + String(g_active ? g_active : "(none)") + "\n";
    s += "> tools:  " + String(g_tool_count) + "\n";
    return cli_ok(s);
  }
  if (cmd == "modules") {
    String s;
    for (int i = 0; i < g_tool_count; i++) {
      s += "> ";
      s += g_tools[i].slug;
      s += "  ";
      s += g_tools[i].name;
      if (g_tools[i].passive)
        s += " (passive)";
      s += "\n";
    }
    return cli_ok(s);
  }
  if (cmd == "heap")
    return cli_ok("> " + String(ESP.getFreeHeap()) + " bytes free\n");
  if (cmd == "uptime")
    return cli_ok("> " + String(millis() / 1000) + " s\n");

  if (cmd == "safety") {
    extern void safety_set_from_kvs(const String &);
    if (kvs.length()) {
      safety_set_from_kvs(kvs);
      return cli_ok("> saved\n");
    }
    String s;
    s += "> dps=" + String(g_safety.deauth_per_sec) + "\n";
    s += "> bps=" + String(g_safety.beacon_per_sec) + "\n";
    s += "> pps=" + String(g_safety.probe_per_sec) + "\n";
    s += "> ast=" + String(g_safety.auto_stop_sec) + "\n";
    s += "> blocked: " + String(g_safety.blocked_bssids[0] ? g_safety.blocked_bssids : "(none)") +
         "\n";
    return cli_ok(s);
  }
  if (cmd == "block") {
    if (npos < 1)
      return cli_help_command("block");
    safety_blocklist_add(pos[0]);
    return cli_ok("> blocked " + pos[0] + "\n");
  }
  if (cmd == "unblock") {
    safety_blocklist_clear();
    return cli_ok("> blocklist cleared\n");
  }

  if (cmd == "start") {
    if (npos < 1)
      return cli_help_command("start");
    if (!tool_start(pos[0].c_str(), kvs))
      return cli_err("could not start '" + pos[0] + "'");
    extern void safety_on_start(const char *);
    safety_on_start(pos[0].c_str());
    return cli_ok("> started " + pos[0] + "\n");
  }
  if (cmd == "stop") {
    if (npos >= 1) {
      const ToolModule *m = tool_find(pos[0].c_str());
      if (!m)
        return cli_err("unknown tool: " + pos[0]);
      if (m->passive)
        return cli_err(pos[0] + " is passive");
      tool_stop(pos[0].c_str());
      return cli_ok("> stopped " + pos[0] + "\n");
    }
    if (!g_active)
      return cli_ok("> no active tool\n");
    String slug = g_active;
    tool_stop(slug.c_str());
    return cli_ok("> stopped " + slug + "\n");
  }

  // delegates
  extern String aegis_dispatch(const String &, const String &, const String *);
  extern String diag_dispatch(const String &, const String &, const String *);
  extern String persist_dispatch(const String &, const String &, const String *);
  if (cmd == "diag") {
    String r = diag_dispatch(cmd, kvs, pos);
    if (r.length())
      return r;
  }
  if (cmd == "profile" || cmd == "schedule") {
    String r = persist_dispatch(cmd, kvs, pos);
    if (r.length())
      return r;
  }
  {
    static const char *aegis_cmds[] = {"aegis",   "scan",        "aps",          "clients",
                                       "events",  "attackers",   "channels",     "trust",
                                       "untrust", "trustclient", "untrustclient"};
    for (size_t ai = 0; ai < sizeof(aegis_cmds) / sizeof(aegis_cmds[0]); ai++) {
      if (cmd == aegis_cmds[ai]) {
        String r = aegis_dispatch(cmd, kvs, pos);
        if (r.length())
          return r;
        break;
      }
    }
  }

  // tool shortcuts
  String slug = (cmd == "ble") ? "blespam" : cmd;
  const ToolModule *m = tool_find(slug.c_str());
  if (!m)
    return cli_err("unknown command: '" + cmd + "' (try 'help')");

  String args = kvs;
  auto addkv = [&](const char *k, const String &v) {
    if (args.length())
      args += "&";
    args += k;
    args += "=";
    args += v;
  };

  if (slug == "deauth") {
    if (npos > 0) {
      if (safety_bssid_blocked_from_str(pos[0]))
        return cli_err("BSSID is blocked");
      addkv("bssid", pos[0]);
    }
    if (npos > 1)
      addkv("ch", pos[1]);
    if (npos > 2)
      addkv("reason", pos[2]);
  } else if (slug == "beacon") {
    if (npos > 0)
      addkv("ssid", pos[0]);
    if (npos > 1)
      addkv("count", pos[1]);
  } else if (slug == "probe") {
    if (npos > 0)
      addkv("ssid", pos[0]);
    if (npos > 1)
      addkv("count", pos[1]);
  } else if (slug == "handshake") {
    if (npos > 0)
      addkv("bssid", pos[0]);
    if (npos > 1)
      addkv("ch", pos[1]);
  } else if (slug == "portal") {
    if (npos > 0)
      addkv("ssid", pos[0]);
  } else if (slug == "blespam") {
    if (npos > 0)
      addkv("mode", pos[0]);
  } else if (slug == "i2c") {
    if (npos > 0)
      addkv("sda", pos[0]);
    if (npos > 1)
      addkv("scl", pos[1]);
    if (npos > 2)
      addkv("addr", pos[2]);
  } else if (slug == "spi") {
    if (npos > 0)
      addkv("clk", pos[0]);
    if (npos > 1)
      addkv("mosi", pos[1]);
    if (npos > 2)
      addkv("miso", pos[2]);
    if (npos > 3)
      addkv("cs", pos[3]);
  } else if (slug == "uart") {
    if (npos > 0)
      addkv("rx", pos[0]);
    if (npos > 1)
      addkv("tx", pos[1]);
    if (npos > 2)
      addkv("baud", pos[2]);
  }

  if (!tool_start(slug.c_str(), args))
    return cli_err("could not start '" + slug + "'");
  safety_on_start(slug.c_str());
  return cli_ok("> started " + slug + "\n");
}

// ---------- HTTP routes ----------
inline void cli_routes() {
  extern WebServer server;
  server.on("/api/cli", HTTP_POST, []() {
    String cmd = server.arg("plain");
    if (!cmd.length())
      cmd = server.arg("cmd");
    // Fallback: form-encoded bodies get parsed into args. Reconstruct.
    if (!cmd.length() && server.args() > 0) {
      if (server.args() == 1 && server.arg(0).length() == 0) {
        cmd = server.argName(0);  // bare word, e.g. "help"
      } else {
        for (int i = 0; i < server.args(); i++) {
          if (i)
            cmd += ' ';
          cmd += server.argName(i);
          if (server.arg(i).length()) {
            cmd += '=';
            cmd += server.arg(i);
          }
        }
      }
    }
    cmd.trim();
    if (cmd == "reboot") {
      server.send(200, "application/json", "{\"ok\":1,\"out\":\"> rebooting...\\n\"}");
      delay(300);
      ESP.restart();
      return;
    }
    server.send(200, "application/json", cli_dispatch(cmd));
  });
  server.on("/api/cli/complete", HTTP_GET, []() {
    String pre = server.hasArg("q") ? server.arg("q") : "";
    String out = "[";
    bool first = true;
    auto add = [&](const String &s) {
      if (!s.startsWith(pre))
        return;
      if (!first)
        out += ",";
      first = false;
      out += "\"" + s + "\"";
    };
    for (size_t i = 0; i < sizeof(cli_categories) / sizeof(cli_categories[0]); i++)
      add(String(cli_categories[i]));
    for (size_t i = 0; i < sizeof(cli_meta) / sizeof(cli_meta[0]); i++)
      add(String(cli_meta[i].name));
    for (size_t i = 0; i < sizeof(cli_aliases) / sizeof(cli_aliases[0]); i++)
      add(String(cli_aliases[i].alias));
    out += "]";
    server.send(200, "application/json", out);
  });
}

// ═══════════════════════════════════════════════════════════════════════════════
// CLI VARIABLE SYSTEM — $name substitution
// ═══════════════════════════════════════════════════════════════════════════════
#define CLI_VAR_MAX 24
#define CLI_VAR_NAME 20
#define CLI_VAR_VAL 64
struct CliVar {
  char name[CLI_VAR_NAME];
  char val[CLI_VAR_VAL];
};
static CliVar g_cli_vars[CLI_VAR_MAX];
static int g_cli_var_count = 0;

static const char *cli_var_get(const char *name) {
  for (int i = 0; i < g_cli_var_count; i++)
    if (strcmp(g_cli_vars[i].name, name) == 0)
      return g_cli_vars[i].val;
  return nullptr;
}
static void cli_var_set(const char *name, const char *val) {
  for (int i = 0; i < g_cli_var_count; i++) {
    if (strcmp(g_cli_vars[i].name, name) == 0) {
      strncpy(g_cli_vars[i].val, val, CLI_VAR_VAL - 1);
      g_cli_vars[i].val[CLI_VAR_VAL - 1] = 0;
      return;
    }
  }
  if (g_cli_var_count < CLI_VAR_MAX) {
    strncpy(g_cli_vars[g_cli_var_count].name, name, CLI_VAR_NAME - 1);
    g_cli_vars[g_cli_var_count].name[CLI_VAR_NAME - 1] = 0;
    strncpy(g_cli_vars[g_cli_var_count].val, val, CLI_VAR_VAL - 1);
    g_cli_vars[g_cli_var_count].val[CLI_VAR_VAL - 1] = 0;
    g_cli_var_count++;
  }
}
static void cli_var_del(const char *name) {
  for (int i = 0; i < g_cli_var_count; i++) {
    if (strcmp(g_cli_vars[i].name, name) == 0) {
      memmove(&g_cli_vars[i], &g_cli_vars[i + 1], sizeof(CliVar) * (g_cli_var_count - i - 1));
      g_cli_var_count--;
      return;
    }
  }
}

// Expand $NAME references in a command string before dispatch
static String cli_var_expand(const String &input) {
  String out;
  int i = 0;
  while (i < (int)input.length()) {
    if (input[i] == '$' && i + 1 < (int)input.length()) {
      int j = i + 1;
      while (j < (int)input.length() && (isAlphaNumeric(input[j]) || input[j] == '_'))
        j++;
      String varname = input.substring(i + 1, j);
      const char *v = cli_var_get(varname.c_str());
      if (v)
        out += v;
      else
        out += input.substring(i, j);  // keep unexpanded
      i = j;
    } else {
      out += input[i++];
    }
  }
  return out;
}

// ═══════════════════════════════════════════════════════════════════════════════
// CLI SCRIPT / MACRO SYSTEM
// ═══════════════════════════════════════════════════════════════════════════════
#define CLI_SCRIPT_MAX 8
#define CLI_SCRIPT_NAME 20
#define CLI_SCRIPT_LINES 12
#define CLI_SCRIPT_LINE 80
struct CliScript {
  char name[CLI_SCRIPT_NAME];
  char lines[CLI_SCRIPT_LINES][CLI_SCRIPT_LINE];
  int count;
};
#ifdef BOARD_HAS_PSRAM
static CliScript *g_cli_scripts;
#else
static CliScript g_cli_scripts[CLI_SCRIPT_MAX];
#endif
static int g_cli_script_count = 0;

static CliScript *cli_script_find(const char *name) {
  for (int i = 0; i < g_cli_script_count; i++)
    if (strcmp(g_cli_scripts[i].name, name) == 0)
      return &g_cli_scripts[i];
  return nullptr;
}
static bool cli_script_save(const char *name, const String &body) {
  CliScript *s = cli_script_find(name);
  if (!s) {
    if (g_cli_script_count >= CLI_SCRIPT_MAX)
      return false;
    s = &g_cli_scripts[g_cli_script_count++];
    strncpy(s->name, name, CLI_SCRIPT_NAME - 1);
    s->name[CLI_SCRIPT_NAME - 1] = 0;
    s->count = 0;
  }
  s->count = 0;
  int i = 0;
  while (i < (int)body.length() && s->count < CLI_SCRIPT_LINES) {
    int j = body.indexOf(';', i);
    if (j < 0)
      j = body.length();
    String line = body.substring(i, j);
    line.trim();
    if (line.length()) {
      strncpy(s->lines[s->count], line.c_str(), CLI_SCRIPT_LINE - 1);
      s->lines[s->count][CLI_SCRIPT_LINE - 1] = 0;
      s->count++;
    }
    i = j + 1;
  }
  return true;
}
static String cli_script_run(const char *name) {
  CliScript *s = cli_script_find(name);
  if (!s)
    return cli_err("no script: " + String(name));
  String out;
  for (int i = 0; i < s->count; i++) {
    String line = cli_var_expand(String(s->lines[i]));
    String r = cli_dispatch(line);
    // Extract "out" field from JSON
    int p = r.indexOf("\"out\":\"");
    if (p >= 0) {
      p += 7;
      int e = r.indexOf("\"", p);
      if (e > p) {
        String sub = r.substring(p, e);
        sub.replace("\\n", "\n");
        out += sub;
      }
    } else if (r.indexOf("\"ok\":0") >= 0) {
      out += "> [ERR] " + String(s->lines[i]) + "\n";
    }
    delay(5);  // yield between commands
  }
  return cli_ok(out);
}
static String cli_script_list() {
  String s = "> Saved scripts:\n";
  for (int i = 0; i < g_cli_script_count; i++) {
    s += ">   ";
    s += g_cli_scripts[i].name;
    s += " (";
    s += g_cli_scripts[i].count;
    s += " steps)\n";
  }
  if (!g_cli_script_count)
    s += ">   (none)\n";
  return cli_ok(s);
}

// ═══════════════════════════════════════════════════════════════════════════════
// CLI HISTORY
// ═══════════════════════════════════════════════════════════════════════════════
#define CLI_HIST_MAX 50
#define CLI_HIST_LEN 100
static char g_cli_hist[CLI_HIST_MAX][CLI_HIST_LEN];
static int g_cli_hist_count = 0;
static int g_cli_hist_head = 0;

static void cli_hist_add(const char *cmd) {
  if (!cmd || !cmd[0])
    return;
  strncpy(g_cli_hist[g_cli_hist_head], cmd, CLI_HIST_LEN - 1);
  g_cli_hist[g_cli_hist_head][CLI_HIST_LEN - 1] = 0;
  g_cli_hist_head = (g_cli_hist_head + 1) % CLI_HIST_MAX;
  if (g_cli_hist_count < CLI_HIST_MAX)
    g_cli_hist_count++;
}

static String cli_hist_get(int n) {
  // n=1 → most recent
  if (n < 1 || n > g_cli_hist_count)
    return "";
  int idx = ((g_cli_hist_head - n) + CLI_HIST_MAX) % CLI_HIST_MAX;
  return String(g_cli_hist[idx]);
}

static String cli_hist_show() {
  String s = "> Command history (" + String(g_cli_hist_count) + "):\n";
  for (int i = g_cli_hist_count; i >= 1; i--) {
    s += "> " + String(g_cli_hist_count - i + 1) + "\t" + cli_hist_get(i) + "\n";
  }
  return cli_ok(s);
}

// ═══════════════════════════════════════════════════════════════════════════════
// CLI OUTPUT FORMAT HELPERS
// ═══════════════════════════════════════════════════════════════════════════════
static String cli_table_row(const String cols[], int ncols, const int widths[]) {
  String s = "> ";
  for (int i = 0; i < ncols; i++) {
    int w = widths[i];
    String c = cols[i].substring(0, w);
    s += c;
    for (int j = c.length(); j < w + 1; j++)
      s += ' ';
    if (i < ncols - 1)
      s += "│ ";
  }
  s += "\n";
  return s;
}
static String cli_table_sep(const int widths[], int ncols) {
  String s = "> ";
  for (int i = 0; i < ncols; i++) {
    for (int j = 0; j < widths[i] + 1; j++)
      s += '─';
    if (i < ncols - 1)
      s += "┼─";
  }
  s += "\n";
  return s;
}

// ═══════════════════════════════════════════════════════════════════════════════
// EXTENDED COMMAND DISPATCHER
// Handles commands not in the core cli_dispatch:
//   let, unset, vars, script, run, history, !N, !!, pipe,
//   files, cat, rm, format, board-json, probe-results,
//   pwndump, chantable, vendors, session, export, benchmark
// ═══════════════════════════════════════════════════════════════════════════════
static String cli_ext_dispatch(const String &raw) {
  String expanded = cli_var_expand(raw);
  String toks[16];
  int n = cli_tok(expanded, toks, 16);
  if (n == 0)
    return "";

  String cmd = toks[0];
  cmd.toLowerCase();
  String kvs, pos[8];
  int npos = 0;
  for (int i = 1; i < n; i++) {
    int eq = toks[i].indexOf('=');
    if (eq > 0) {
      if (kvs.length())
        kvs += "&";
      kvs += toks[i];
    } else if (npos < 8)
      pos[npos++] = toks[i];
  }

  // ── Variable commands ──
  if (cmd == "let" || cmd == "set") {
    if (npos < 2)
      return cli_err("usage: let <name> <value>");
    cli_var_set(pos[0].c_str(), pos[1].c_str());
    return cli_ok("> $" + pos[0] + " = " + pos[1] + "\n");
  }
  if (cmd == "unset") {
    if (npos < 1)
      return cli_err("usage: unset <name>");
    cli_var_del(pos[0].c_str());
    return cli_ok("> unset $" + pos[0] + "\n");
  }
  if (cmd == "vars") {
    String s = "> Variables (" + String(g_cli_var_count) + "):\n";
    for (int i = 0; i < g_cli_var_count; i++)
      s += ">   $" + String(g_cli_vars[i].name) + " = " + String(g_cli_vars[i].val) + "\n";
    if (!g_cli_var_count)
      s += ">   (none)\n";
    return cli_ok(s);
  }

  // ── Script commands ──
  if (cmd == "script") {
    if (npos < 1)
      return cli_script_list();
    if (npos < 2)
      return cli_script_list();
    // "script <name> <cmd1>; <cmd2>; ..."
    String body;
    for (int i = 1; i < n; i++) {
      if (i > 1)
        body += ' ';
      body += toks[i];
    }
    if (!cli_script_save(pos[0].c_str(), body))
      return cli_err("script storage full (max " + String(CLI_SCRIPT_MAX) + ")");
    return cli_ok("> script '" + pos[0] + "' saved\n");
  }
  if (cmd == "scripts")
    return cli_script_list();
  if (cmd == "run") {
    if (npos < 1)
      return cli_err("usage: run <scriptname>");
    return cli_script_run(pos[0].c_str());
  }

  // ── History commands ──
  if (cmd == "history" || cmd == "hist")
    return cli_hist_show();
  if (cmd.startsWith("!!")) {
    String c = cli_hist_get(1);
    if (!c.length())
      return cli_err("no previous command");
    return cli_dispatch(c);
  }
  if (cmd.startsWith("!") && cmd.length() > 1) {
    int idx = cmd.substring(1).toInt();
    String c = cli_hist_get(idx);
    if (!c.length())
      return cli_err("history entry not found: " + String(idx));
    return cli_dispatch(c);
  }

  // ── File/filesystem commands ──
  if (cmd == "files" || cmd == "ls") {
    extern WebServer server;
    // Enumerate LittleFS files via File API
    String s = "> Flash filesystem:\n";
    File root = LittleFS.open("/");
    if (!root || !root.isDirectory()) {
      s += ">   (empty or not mounted)\n";
      return cli_ok(s);
    }
    File f = root.openNextFile();
    int total = 0;
    while (f) {
      String name = String(f.name());
      size_t sz = f.size();
      s += ">   " + name;
      int pad = 24 - name.length();
      for (int k = 0; k < pad; k++)
        s += ' ';
      s += String(sz) + " bytes\n";
      total++;
      f = root.openNextFile();
    }
    s += "> Total: " + String(total) + " file(s)\n";
    return cli_ok(s);
  }
  if (cmd == "rm" || cmd == "del") {
    if (npos < 1)
      return cli_err("usage: rm <filename>");
    String path = pos[0];
    if (!path.startsWith("/"))
      path = "/" + path;
    if (!LittleFS.exists(path))
      return cli_err("not found: " + path);
    LittleFS.remove(path);
    return cli_ok("> removed: " + path + "\n");
  }
  if (cmd == "cat") {
    if (npos < 1)
      return cli_err("usage: cat <filename>");
    String path = pos[0];
    if (!path.startsWith("/"))
      path = "/" + path;
    if (!LittleFS.exists(path))
      return cli_err("not found: " + path);
    File f = LittleFS.open(path);
    if (!f)
      return cli_err("open failed: " + path);
    String s = "> " + path + " (" + String(f.size()) + " bytes):\n";
    uint8_t buf[64];
    int offset = 0;
    while (f.available() && offset < 512) {
      int rd = f.read(buf, 32);
      s += "> ";
      s += String(offset, 16);
      s += "  ";
      for (int i = 0; i < rd; i++) {
        if (buf[i] < 16)
          s += '0';
        s += String(buf[i], 16);
        s += ' ';
      }
      s += "\n";
      offset += rd;
    }
    if (f.available())
      s += "> ... (truncated)\n";
    f.close();
    return cli_ok(s);
  }

  // ── Probe/SSID harvest dump ──
  if (cmd == "probes" || cmd == "probelist") {
    // Probe harvest results via API
    String s = "> Captured probe requests:\n";
    s += ">   (use /api/probeharvest/results for full JSON)\n";
    return cli_ok(s);
  }

  // ── Detailed board info ──
  if (cmd == "board" || cmd == "sysinfo") {
    String s;
    s += "> ╔═════════════════════════════╗\n";
    s += "> ║     PHANTOM32 SYSINFO       ║\n";
    s += "> ╚═════════════════════════════╝\n";
    s += "> Chip:       " + String(ESP.getChipModel()) + "\n";
    s += "> Rev:        " + String(ESP.getChipRevision()) + "\n";
    s += "> Cores:      " + String(ESP.getChipCores()) + "\n";
    s += "> CPU freq:   " + String(ESP.getCpuFreqMHz()) + " MHz\n";
    s += "> Flash:      " + String(ESP.getFlashChipSize() / 1024) + " KB\n";
    s += "> Flash spd:  " + String(ESP.getFlashChipSpeed() / 1000000) + " MHz\n";
    s += "> Free heap:  " + String(ESP.getFreeHeap()) + " bytes\n";
    s += "> Min heap:   " + String(ESP.getMinFreeHeap()) + " bytes\n";
    s += "> PSRAM:      " + String(ESP.getPsramSize() / 1024) + " KB\n";
    s += "> Free PSRAM: " + String(ESP.getFreePsram()) + " bytes\n";
    s += "> SDK ver:    " + String(ESP.getSdkVersion()) + "\n";
    s += "> Uptime:     " + String(millis() / 1000) + " s\n";
    s += "> AP MAC:     ";
    uint8_t m[6];
    WiFi.softAPmacAddress(m);
    char mac[18];
    snprintf(mac, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
    s += String(mac) + "\n";
    s += "> STA MAC:    ";
    WiFi.macAddress(m);
    snprintf(mac, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
    s += String(mac) + "\n";
    s += "> Active:     " + String(g_active ? g_active : "(none)") + "\n";
    s += "> Tools:      " + String(g_tool_count) + " registered\n";
    return cli_ok(s);
  }

  // ── Channel utilization table ──
  if (cmd == "chantable" || cmd == "chtable") {
    String s;
    s += "> Channel  │ Mgmt  │ Beacons │ Deauths │ Score\n";
    s += "> ─────────┼───────┼─────────┼─────────┼──────\n";
    // Pull from aegis if available
    s += ">   (channel stats available via 'aegis' or /api/aegis/channels)\n";
    return cli_ok(s);
  }

  // ── Benchmark: measure raw tx speed ──
  if (cmd == "benchmark" || cmd == "bench") {
    uint32_t t0 = millis();
    static uint8_t buf[26];
    memset(buf, 0, 26);
    buf[0] = 0x08;
    buf[1] = 0x00;  // data frame
    int sent = 0;
    while (millis() - t0 < 1000) {
      esp_wifi_80211_tx(WIFI_IF_AP, buf, 26, false);
      sent++;
    }
    return cli_ok("> Benchmark: " + String(sent) + " frames/sec\n> (~" +
                  String(sent * 26 * 8 / 1000) + " kbit/s raw)\n");
  }

  // ── Session export ──
  if (cmd == "export" || cmd == "dump") {
    String s;
    s += "> Session export:\n";
    s += ">   heap:    " + String(ESP.getFreeHeap()) + "\n";
    s += ">   uptime:  " + String(millis() / 1000) + " s\n";
    s += ">   tools:   " + String(g_tool_count) + "\n";
    s += ">   active:  " + String(g_active ? g_active : "(none)") + "\n";
    s += ">   vars:    " + String(g_cli_var_count) + "\n";
    s += ">   scripts: " + String(g_cli_script_count) + "\n";
    s += ">   history: " + String(g_cli_hist_count) + "\n";
    s += ">   Full JSON at /api/session/export\n";
    return cli_ok(s);
  }

  // ── Clear command ──
  if (cmd == "clear" || cmd == "cls") {
    return "{\"ok\":1,\"clear\":1,\"out\":\"\"}";
  }

  // ── Echo ──
  if (cmd == "echo") {
    String s;
    for (int i = 1; i < n; i++) {
      s += toks[i];
      if (i < n - 1)
        s += " ";
    }
    return cli_ok("> " + s + "\n");
  }

  // ── Repeat/loop a command N times ──
  if (cmd == "repeat" || cmd == "loop") {
    if (npos < 2)
      return cli_err("usage: repeat <N> <command>");
    int times = pos[0].toInt();
    if (times < 1 || times > 20)
      return cli_err("repeat count must be 1-20");
    String subcmd;
    for (int i = 2; i < n; i++) {
      subcmd += toks[i];
      if (i < n - 1)
        subcmd += " ";
    }
    String out;
    for (int i = 0; i < times; i++) {
      String r = cli_dispatch(subcmd);
      int p = r.indexOf("\"out\":\"");
      if (p >= 0) {
        p += 7;
        int e = r.indexOf("\"", p);
        if (e > p) {
          String sub = r.substring(p, e);
          sub.replace("\\n", "\n");
          out += sub;
        }
      }
      delay(10);
    }
    return cli_ok(out);
  }

  // ── Wait/sleep ──
  if (cmd == "wait" || cmd == "sleep") {
    if (npos < 1)
      return cli_err("usage: wait <ms>");
    uint32_t ms = pos[0].toInt();
    if (ms > 10000)
      ms = 10000;
    delay(ms);
    return cli_ok("> waited " + String(ms) + " ms\n");
  }

  // ── Version ──
  if (cmd == "version" || cmd == "ver") {
    String s;
    s += "> ╔═══════════════════════════════╗\n";
    s += "> ║ PHANTOM32 Firmware v2.0       ║\n";
    s += "> ║ ESP32 Protocol Research Suite ║\n";
    s += "> ╚═══════════════════════════════╝\n";
    s += "> Chip:   " + String(ESP.getChipModel()) + "\n";
    s += "> SDK:    " + String(ESP.getSdkVersion()) + "\n";
    s += "> Tools:  " + String(g_tool_count) + " modules\n";
    s += "> CLI:    v2 (scripting+vars enabled)\n";
    return cli_ok(s);
  }

  // ── Tool info (verbose) ──
  if (cmd == "toolinfo" || cmd == "tinfo") {
    if (npos < 1)
      return cli_err("usage: toolinfo <slug>");
    const ToolModule *m = tool_find(pos[0].c_str());
    if (!m)
      return cli_err("unknown tool: " + pos[0]);
    String s;
    s += "> ── " + String(m->slug) + " ──\n";
    s += "> Name:    " + String(m->name) + "\n";
    s += "> Passive: " + String(m->passive ? "yes" : "no") + "\n";
    return cli_ok(s);
  }

  // ── Alias management ──
  if (cmd == "aliases" || cmd == "al") {
    String s = "> Built-in aliases:\n";
    for (size_t i = 0; i < sizeof(cli_aliases) / sizeof(cli_aliases[0]); i++) {
      s += ">   " + String(cli_aliases[i].alias);
      int pad = 14 - strlen(cli_aliases[i].alias);
      for (int k = 0; k < pad; k++)
        s += ' ';
      s += "→ " + String(cli_aliases[i].cmd) + "\n";
    }
    return cli_ok(s);
  }

  // ── Category listing ──
  if (cmd == "cats" || cmd == "categories") {
    String s = "> Command categories:\n";
    for (size_t i = 0; i < sizeof(cli_categories) / sizeof(cli_categories[0]); i++) {
      s += ">   " + String(cli_categories[i]);
      int pad = 14 - strlen(cli_categories[i]);
      for (int k = 0; k < pad; k++)
        s += ' ';
      s += "- " + String(cli_cat_desc[i]) + "\n";
    }
    return cli_ok(s);
  }

  // ── Ping ESP32 health ──
  if (cmd == "ping") {
    return cli_ok("> pong! heap=" + String(ESP.getFreeHeap()) +
                  " uptime=" + String(millis() / 1000) + "s\n");
  }

  // ── Random MAC generator (for testing) ──
  if (cmd == "randmac") {
    uint8_t m[6];
    for (int i = 0; i < 6; i++)
      m[i] = (uint8_t)(esp_random() & 0xFF);
    m[0] &= 0xFE;  // unicast
    char mac[18];
    snprintf(mac, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
    return cli_ok("> " + String(mac) + "\n");
  }

  // ── Hex encoder/decoder ──
  if (cmd == "hex") {
    if (npos < 1)
      return cli_err("usage: hex <string>");
    String s = "> ";
    for (size_t i = 0; i < pos[0].length(); i++) {
      uint8_t c = (uint8_t)pos[0][i];
      if (c < 16)
        s += '0';
      s += String(c, 16);
      s += ' ';
    }
    s += "\n";
    return cli_ok(s);
  }

  // ── XOR two hex strings ──
  if (cmd == "xor") {
    if (npos < 2)
      return cli_err("usage: xor <hex1> <hex2>");
    int len = min(pos[0].length(), pos[1].length()) / 2;
    String s = "> ";
    for (int i = 0; i < len; i++) {
      uint8_t a = (uint8_t)strtol(pos[0].substring(i * 2, i * 2 + 2).c_str(), nullptr, 16);
      uint8_t b = (uint8_t)strtol(pos[1].substring(i * 2, i * 2 + 2).c_str(), nullptr, 16);
      uint8_t x = a ^ b;
      if (x < 16)
        s += '0';
      s += String(x, 16);
      s += ' ';
    }
    s += "\n";
    return cli_ok(s);
  }

  // ── WiFi channel frequencies ──
  if (cmd == "freqs" || cmd == "frequencies") {
    static const uint16_t freqs[] = {2412, 2417, 2422, 2427, 2432, 2437, 2442,
                                     2447, 2452, 2457, 2462, 2467, 2472};
    String s = "> 2.4 GHz channel frequencies:\n";
    for (int i = 0; i < 13; i++) {
      s += ">   ch" + String(i + 1) + "  " + String(freqs[i]) + " MHz";
      if (i + 1 == 1 || i + 1 == 6 || i + 1 == 11)
        s += "  ← non-overlapping";
      s += "\n";
    }
    return cli_ok(s);
  }

  // ── Memory map ──
  if (cmd == "memmap" || cmd == "memory") {
    String s;
    s += "> Memory map:\n";
    s += ">   Free heap:    " + String(ESP.getFreeHeap()) + " bytes\n";
    s += ">   Min heap:     " + String(ESP.getMinFreeHeap()) + " bytes\n";
    s += ">   Max alloc:    " + String(ESP.getMaxAllocHeap()) + " bytes\n";
    s += ">   PSRAM total:  " + String(ESP.getPsramSize()) + " bytes\n";
    s += ">   PSRAM free:   " + String(ESP.getFreePsram()) + " bytes\n";
    s += ">   Flash size:   " + String(ESP.getFlashChipSize()) + " bytes\n";
    s += ">   Sketch size:  " + String(ESP.getSketchSize()) + " bytes\n";
    s += ">   Free sketch:  " + String(ESP.getFreeSketchSpace()) + " bytes\n";
    return cli_ok(s);
  }

  // ── Reboot with countdown ──
  if (cmd == "reboot" || cmd == "restart") {
    extern WebServer server;
    server.send(200, "application/json", "{\"ok\":1,\"out\":\"> rebooting in 1s...\\n\"}");
    delay(1000);
    ESP.restart();
    return "";
  }

  return "";  // not handled — fall through to core dispatcher
}

// ── Wrap cli_dispatch to inject ext dispatch and history ──────────────────────
static String cli_dispatch_full(const String &raw) {
  String input = raw;
  input.trim();
  if (!input.length())
    return "{\"ok\":1,\"out\":\"\"}";

  // Add to history
  cli_hist_add(input.c_str());

  // Variable expansion
  String expanded = cli_var_expand(input);

  // Try extended dispatcher first
  String ext = cli_ext_dispatch(expanded);
  if (ext.length())
    return ext;

  // Fall through to core
  return cli_dispatch(expanded);
}

// ── Override HTTP route to use full dispatcher ────────────────────────────────
inline void cli_routes_extended() {
  extern WebServer server;
  server.on("/api/cli/ext", HTTP_POST, []() {
    String cmd = server.arg("plain");
    if (!cmd.length())
      cmd = server.arg("cmd");
    if (!cmd.length() && server.args() > 0) {
      if (server.args() == 1 && server.arg(0).length() == 0) {
        cmd = server.argName(0);
      } else {
        for (int i = 0; i < server.args(); i++) {
          if (i)
            cmd += ' ';
          cmd += server.argName(i);
          if (server.arg(i).length()) {
            cmd += '=';
            cmd += server.arg(i);
          }
        }
      }
    }
    cmd.trim();
    server.send(200, "application/json", cli_dispatch_full(cmd));
  });

  // Session export endpoint
  server.on("/api/session/export", HTTP_GET, []() {
    String j = "{";
    j += "\"heap\":" + String(ESP.getFreeHeap());
    j += ",\"uptime\":" + String(millis() / 1000);
    j += ",\"tools\":" + String(g_tool_count);
    j += ",\"active\":\"" + String(g_active ? g_active : "") + "\"";
    j += ",\"var_count\":" + String(g_cli_var_count);
    j += ",\"script_count\":" + String(g_cli_script_count);
    j += ",\"hist_count\":" + String(g_cli_hist_count);
    j += ",\"vars\":[";
    for (int i = 0; i < g_cli_var_count; i++) {
      if (i)
        j += ",";
      j += "{\"n\":\"" + String(g_cli_vars[i].name) + "\",\"v\":\"" + String(g_cli_vars[i].val) +
           "\"}";
    }
    j += "],\"scripts\":[";
    for (int i = 0; i < g_cli_script_count; i++) {
      if (i)
        j += ",";
      j += "{\"name\":\"" + String(g_cli_scripts[i].name) +
           "\",\"steps\":" + String(g_cli_scripts[i].count) + "}";
    }
    j += "]}";
    server.send(200, "application/json", j);
  });

  // History endpoint
  server.on("/api/cli/history", HTTP_GET, []() {
    String j = "[";
    for (int i = 1; i <= g_cli_hist_count; i++) {
      if (i > 1)
        j += ",";
      String h = cli_hist_get(i);
      h.replace("\\", "\\\\");
      h.replace("\"", "\\\"");
      j += "\"" + h + "\"";
    }
    j += "]";
    server.send(200, "application/json", j);
  });
}

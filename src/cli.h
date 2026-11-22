#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <ESP.h>

// ---------- output helpers ----------
static String cli_esc(const String& s){
  String o;
  for (size_t i=0;i<s.length();i++){
    char c=s[i];
    if (c=='"'||c=='\\'){o+='\\';o+=c;}
    else if (c=='\n') o+="\\n";
    else if (c=='\r') o+="\\r";
    else if (c=='\t') o+="\\t";
    else if ((unsigned char)c>=0x20) o+=c;
  }
  return o;
}
static String cli_ok(const String& s)  { return "{\"ok\":1,\"out\":\"" + cli_esc(s) + "\"}"; }
static String cli_err(const String& s) { return "{\"ok\":0,\"err\":\"> " + cli_esc(s) + "\"}"; }

static int cli_tok(const String& s, String* out, int max){
  int i=0, n=s.length(), c=0;
  while (i<n && c<max){
    while (i<n && isspace((unsigned char)s[i])) i++;
    if (i>=n) break;
    String t; bool q=false; char qc=0;
    while (i<n){
      char ch=s[i];
      if (!q && isspace((unsigned char)ch)) break;
      if (!q && (ch=='"'||ch=='\'')){q=true;qc=ch;i++;continue;}
      if (q && ch==qc){q=false;i++;continue;}
      t+=ch;i++;
    }
    out[c++]=t;
  }
  return c;
}

// ---------- aliases ----------
struct CliAlias { const char* alias; const char* cmd; };
static const CliAlias cli_aliases[] = {
  {"?","help"}, {"h","help"}, {"ls","modules"}, {"list","modules"},
  {"st","status"}, {"info","status"}, {"free","heap"}, {"mem","heap"},
  {"up","uptime"}, {"halt","stop"}, {"kill","stop"},
  {"wifi","scan"}, {"d","deauth"}, {"b","beacon"},
  {"p","probe"}, {"hs","handshake"}, {"ep","portal"}, {"hp","honeypot"},
  {"bt","ble"}, {"ids","aegis"},
  {"storm","stormdeauth"}, {"pw","pwnface"}, {"harvest","probeharvest"},
  {"btm","btmflood"}, {"eap","eapflood"}, {"null","nullflood"},
  {"mgt","mgtflood"}, {"md","multideauth"}, {"af","actionflood"},
  {"wpa3","wpa3flood"}, {"et","eviltwin"}, {"csa","csa"},
  {"esp","espnow"}, {"chd","chanhop"}, {"auth","authflood"},
  {"nav","navflood"}, {"pmkid","pmkid"}, {"adv","beaconadv"},
};
static String cli_resolve_alias(const String& in){
  for (size_t i=0;i<sizeof(cli_aliases)/sizeof(cli_aliases[0]);i++)
    if (in == cli_aliases[i].alias) return String(cli_aliases[i].cmd);
  return in;
}

// ---------- command metadata ----------
struct CliMeta {
  const char* cat;
  const char* name;
  const char* desc;
  const char* usage;
  const char* args;      // multi-line, each line already prefixed with ">    "
  const char* examples;  // multi-line, each line already prefixed with ">    "
};

static const CliMeta cli_meta[] = {
  // ---- core ----
  {"core","help","Display help information for commands.","help [category|command|all]",
   "",
   ">    help\n>    help wifi\n>    help deauth\n>    help all"},
  {"core","status","Show heap, uptime, active tool and tool count.","status",
   "",
   ">    status"},
  {"core","modules","List all registered tools with their slug.","modules",
   "",
   ">    modules"},
  {"core","heap","Show free heap in bytes.","heap",
   "",
   ">    heap"},
  {"core","uptime","Show seconds since boot.","uptime",
   "",
   ">    uptime"},
  {"core","start","Start a tool by slug with optional arguments.","start <slug> [key=value ...]",
   ">    slug - Tool name from 'modules'",
   ">    start deauth bssid=AA:BB:CC:DD:EE:FF ch=6\n>    start beacon ssid=TestNet count=100"},
  {"core","stop","Stop the active tool, or stop a named tool.","stop [slug]",
   ">    slug - Optional. Omit to stop the active tool.",
   ">    stop\n>    stop deauth"},
  {"core","reboot","Soft-restart the ESP32.","reboot",
   "",
   ">    reboot"},

  // ---- wifi ----
  {"wifi","scan","List APs from Aegis cache, or trigger a live scan.","scan [refresh]",
   ">    (no arg) - read cached results from Aegis, no connection drop\n>    refresh - force live scan (connection drops ~3s)",
   ">    scan\n>    scan refresh"},
  {"wifi","deauth","Send 802.11 deauthentication frames.","deauth <bssid> [ch=N] [reason=N] [broadcast=1]",
   ">    bssid - Target AP MAC address (required)\n>    ch - Channel (default: current)\n>    reason - Deauth reason code: 7=Class3, 1=Unspec, 3=Leave\n>    broadcast - 1=all clients, 0=single STA",
   ">    deauth AA:BB:CC:DD:EE:FF\n>    deauth AA:BB:CC:DD:EE:FF ch=6 reason=7\n>    deauth AA:BB:CC:DD:EE:FF ch=6 broadcast=1"},
  {"wifi","beacon","Broadcast fake beacon frames on channels 1-13.","beacon <ssid> [count=N] [random=1]",
   ">    ssid - Fake network name\n>    count - Number of beacons to send (default: 200)\n>    random - 1=random BSSIDs per frame, 0=fixed",
   ">    beacon \"Free WiFi\"\n>    beacon TestNet count=500 random=1"},
  {"wifi","probe","Flood probe-request frames across all channels.","probe <ssid> [count=N]",
   ">    ssid - SSID to probe for\n>    count - Number of probes (default: 500)",
   ">    probe TestNet\n>    probe \"Starbucks\" count=1000"},
  {"wifi","handshake","Capture EAPOL frames to PCAP, optionally force re-handshake.","handshake <bssid> [ch=N] [force=1]",
   ">    bssid - Target AP MAC address\n>    ch - Channel (default: 6)\n>    force - 1=send 20 deauth frames first to force re-handshake",
   ">    handshake AA:BB:CC:DD:EE:FF\n>    handshake AA:BB:CC:DD:EE:FF ch=11 force=1\n>    PCAP at /api/handshake/pcap"},
  {"wifi","portal","Open a captive portal that captures credentials.","portal <ssid>",
   ">    ssid - SSID the portal AP will advertise",
   ">    portal \"Hotel WiFi\"\n>    Creds at /api/portal/creds"},

  // ---- ble ----
  {"ble","ble","Start BLE advertisement spam in the given mode.","ble <mode>",
   ">    mode - apple | android | windows | samsung | airtag |\n>           ibeacon | eddystone | names | scan",
   ">    ble apple\n>    ble airtag\n>    ble names"},

  // ---- comm ----
  {"comm","i2c","Act as an I2C slave and log every byte from the master.","i2c [sda=N] [scl=N] [addr=0xNN]",
   ">    sda - SDA pin (default: 21)\n>    scl - SCL pin (default: 22)\n>    addr - Slave address (default: 0x50)",
   ">    i2c\n>    i2c sda=21 scl=22 addr=0x3C\n>    Log at /api/i2c/log"},
  {"comm","spi","Sample MOSI on SPI clock edges (ISR-safe).","spi [clk=N] [mosi=N] [miso=N] [cs=N]",
   ">    clk - Clock pin (default: 18)\n>    mosi - MOSI pin (default: 23)\n>    miso - MISO pin (default: 19)\n>    cs - Chip select (default: 5)",
   ">    spi\n>    spi clk=18 mosi=23 cs=5\n>    Log at /api/spi/log"},
  {"comm","uart","Bridge UART1 to USB serial.","uart [rx=N] [tx=N] [baud=N]",
   ">    rx - RX pin (default: 16)\n>    tx - TX pin (default: 17)\n>    baud - Baud rate (default: 115200)",
   ">    uart\n>    uart rx=16 tx=17 baud=9600"},

  // ---- honeypot ----
  {"honeypot","honeypot","TCP listeners on 22/80/2323 with fake banners.","honeypot",
   "",
   ">    honeypot\n>    Log at /api/honeypot/log"},

  // ---- defense ----
  {"defense","aegis","Show Aegis IDS summary.","aegis",
   "",
   ">    aegis"},
  {"defense","clients","List nearby client devices seen by Aegis.","clients",
   "",
   ">    clients"},
  {"defense","events","Show recent Aegis events.","events",
   "",
   ">    events"},
  {"defense","attackers","Ranked deauth/disassoc attackers.","attackers",
   "",
   ">    attackers"},
  {"defense","channels","Per-channel management-frame utilization.","channels",
   "",
   ">    channels"},
  {"defense","trust","Set the trusted AP to watch for impersonation.","trust <ssid> <bssid> <ch>  |  trust <bssid>",
   ">    ssid - Network name\n>    bssid - Access point MAC\n>    ch - Channel",
   ">    trust HomeNet AA:BB:CC:DD:EE:FF 6\n>    trust AA:BB:CC:DD:EE:FF"},
  {"defense","untrust","Clear the trusted AP.","untrust",
   "",
   ">    untrust"},
  {"defense","trustclient","Mark a client MAC as trusted.","trustclient <mac>",
   ">    mac - Client MAC address",
   ">    trustclient 11:22:33:44:55:66"},
  {"defense","untrustclient","Remove a client from the trusted list.","untrustclient <mac>",
   ">    mac - Client MAC address",
   ">    untrustclient 11:22:33:44:55:66"},

  // ---- safety ----
  {"safety","safety","View or set rate limits and auto-stop.","safety [dps=N] [bps=N] [pps=N] [ast=N]",
   ">    dps - Deauth frames per second (default: 100)\n>    bps - Beacon frames per second (default: 200)\n>    pps - Probe frames per second (default: 300)\n>    ast - Auto-stop seconds (0=off)",
   ">    safety\n>    safety ast=30\n>    safety dps=50 bps=100 pps=200 ast=15"},
  {"safety","block","Add a BSSID to the blocklist.","block <mac>",
   ">    mac - BSSID to block from deauth targets",
   ">    block AA:BB:CC:DD:EE:FF"},
  {"safety","unblock","Clear the BSSID blocklist.","unblock",
   "",
   ">    unblock"},

  // ---- persist ----
  {"persist","profile","Save/load/delete tool profiles.","profile <save|load|del> [name]",
   ">    save - Save the current tool state\n>    load - Restore a saved profile\n>    del - Delete a profile",
   ">    profile save my_attack\n>    profile load my_attack"},
  {"persist","schedule","Schedule stub.","schedule <add|list|del|run> ...",
   "",
   ">    schedule list"},

  // ---- diag ----
  {"diag","diag","Diagnostics: channel map, heatmap, clients, IEs, packets.","diag <channel|heatmap|clients|ies <bssid>|packets>",
   ">    channel - Per-channel frame counts\n>    heatmap - RSSI histogram\n>    clients - Client inventory with vendor\n>    ies - IE decoder for a BSSID\n>    packets - Recent mgmt frames",
   ">    diag channel\n>    diag heatmap\n>    diag ies AA:BB:CC:DD:EE:FF"},

  // ---- advanced wifi attacks ----
  {"attack","eviltwin","Rogue AP + deauth + DNS + credential capture.","eviltwin <bssid> [ssid=<name>] [ch=N]",
   ">    bssid - Target AP to clone\n>    ssid - Override SSID (default: clone target)\n>    ch - Channel (default: clone target)",
   ">    eviltwin AA:BB:CC:DD:EE:FF\n>    eviltwin AA:BB:CC:DD:EE:FF ssid=HomeNet ch=6\n>    Creds at /api/eviltwin/creds"},
  {"attack","authflood","Auth/assoc/disassoc flood to exhaust AP association tables.","authflood bssid=<mac> [ch=N] [mode=auth|assoc|disassoc]",
   ">    bssid - Target AP MAC\n>    ch - Channel (default: 6)\n>    mode - Frame type to flood (default: auth)",
   ">    authflood bssid=AA:BB:CC:DD:EE:FF\n>    authflood bssid=AA:BB:CC:DD:EE:FF ch=11 mode=assoc"},
  {"attack","karma","KARMA probe response spoofing — respond to all probe requests.","karma [ssid=<name>]",
   ">    ssid - Specific SSID to spoof (default: respond to all probes)",
   ">    karma\n>    karma ssid=FreeWiFi"},
  {"attack","navflood","CTS/RTS NAV (virtual carrier sense) DoS.","navflood [ch=N] [dur=N]",
   ">    ch - Channel (default: 6)\n>    dur - NAV duration value 0-32767 (default: 32767)",
   ">    navflood\n>    navflood ch=1 dur=32767"},
  {"attack","csa","Channel Switch Announcement + BTM Request to force client migration.","csa bssid=<mac> [ch=N] [target=N]",
   ">    bssid - Target AP\n>    ch - Current channel\n>    target - Switch-to channel (default: 13)",
   ">    csa bssid=AA:BB:CC:DD:EE:FF ch=6 target=1"},
  {"attack","espnow","ESP-NOW broadcast flood to disrupt ESP-NOW mesh networks.","espnow [count=N]",
   ">    count - Frames to send (default: continuous)",
   ">    espnow\n>    espnow count=1000"},
  {"attack","beaconadv","Advanced beacon injection: WPS/fuzz/overflow/TKIP/WDS frames.","beaconadv [mode=wps|fuzz|overflow|tkip|wds]",
   ">    mode - Frame variant (default: wps)",
   ">    beaconadv mode=wps\n>    beaconadv mode=fuzz"},
  {"attack","pmkid","Active PMKID extraction via association — saves to PCAP on flash.","pmkid bssid=<mac> [ch=N]",
   ">    bssid - Target AP MAC\n>    ch - Channel (default: 6)",
   ">    pmkid bssid=AA:BB:CC:DD:EE:FF ch=6\n>    PCAP at /api/pmkid/pcap"},
  {"attack","wpa3flood","WPA3 SAE Commit/Confirm flood to exhaust AP state machine.","wpa3flood bssid=<mac> [ch=N] [mode=commit|confirm|both|transition]",
   ">    bssid - Target WPA3 AP MAC\n>    ch - Channel (default: 6)\n>    mode - Attack mode (default: commit)",
   ">    wpa3flood bssid=AA:BB:CC:DD:EE:FF ch=6\n>    wpa3flood bssid=AA:BB:CC:DD:EE:FF mode=transition"},
  {"attack","chanhop","All-channel rotating deauth sweep across channels 1-13.","chanhop [bssid=<mac>] [dwell=N]",
   ">    bssid - Target AP (default: broadcast all)\n>    dwell - Ms per channel (default: 200)",
   ">    chanhop\n>    chanhop bssid=AA:BB:CC:DD:EE:FF dwell=100"},
  {"attack","btmflood","802.11v BSS Transition Management flood to force STA roaming.","btmflood bssid=<mac> [ch=N] [sta=<mac>]",
   ">    bssid - Source AP MAC\n>    ch - Channel (default: 6)\n>    sta - Target client (default: broadcast)",
   ">    btmflood bssid=AA:BB:CC:DD:EE:FF\n>    btmflood bssid=AA:BB:CC:DD:EE:FF sta=11:22:33:44:55:66"},
  {"attack","eapflood","WPA Enterprise EAPOL identity/PEAP state exhaustion flood.","eapflood bssid=<mac> [ch=N]",
   ">    bssid - Target WPA-Enterprise AP MAC\n>    ch - Channel (default: 6)",
   ">    eapflood bssid=AA:BB:CC:DD:EE:FF ch=6"},
  {"attack","nullflood","Power-save null/QoS-null/BAR frame confusion flood.","nullflood bssid=<mac> [ch=N] [mode=null|qos|bar]",
   ">    bssid - Target AP\n>    ch - Channel (default: 6)\n>    mode - Frame type (default: null)",
   ">    nullflood bssid=AA:BB:CC:DD:EE:FF\n>    nullflood bssid=AA:BB:CC:DD:EE:FF mode=qos"},
  {"attack","mgtflood","All-subtype management frame spray across all 13 channels.","mgtflood [bssid=<mac>] [ch=N]",
   ">    bssid - Target AP (default: broadcast)\n>    ch - Channel (default: rotate)",
   ">    mgtflood\n>    mgtflood bssid=AA:BB:CC:DD:EE:FF ch=6"},
  {"attack","multideauth","Deauth up to 8 BSSIDs simultaneously.","multideauth bssid=<mac>[,<mac>...] [ch=N]",
   ">    bssid - Comma-separated list of up to 8 AP MACs\n>    ch - Channel (default: 6)",
   ">    multideauth bssid=AA:BB:CC:DD:EE:FF,11:22:33:44:55:66\n>    md bssid=AA:BB:CC:DD:EE:FF,11:22:33:44:55:66 ch=11"},
  {"attack","actionflood","802.11 action frame category spray (all categories 0-127).","actionflood bssid=<mac> [ch=N]",
   ">    bssid - Target AP\n>    ch - Channel (default: 6)",
   ">    actionflood bssid=AA:BB:CC:DD:EE:FF\n>    af bssid=AA:BB:CC:DD:EE:FF ch=1"},
  {"attack","stormdeauth","Multi-vector WPA2+WPA3 deauth storm (8 attack vectors in rotation).","stormdeauth bssid=<mac> [ch=N] [broadcast=0|1] [sta=<mac>]",
   ">    bssid - Target AP MAC\n>    ch - Channel (default: 6)\n>    broadcast - 1=all clients, 0=single STA\n>    sta - Target client MAC (when broadcast=0)",
   ">    stormdeauth bssid=AA:BB:CC:DD:EE:FF\n>    storm bssid=AA:BB:CC:DD:EE:FF ch=6 broadcast=1\n>    storm bssid=AA:BB:CC:DD:EE:FF sta=11:22:33:44:55:66 broadcast=0"},

  // ---- recon ----
  {"recon","pwnface","Autonomous UCB-guided handshake hunter (Pwnagotchi-inspired).","pwnface",
   "",
   ">    pwnface\n>    pw\n>    Live UI at /pwnface\n>    Hops channels 1-13, deauths APs, captures EAPOL"},
  {"recon","probeharvest","Passive probe request sniffer — reveals device saved network history.","probeharvest",
   "",
   ">    probeharvest\n>    harvest\n>    Results at /api/probeharvest/results\n>    Logs STA MAC + SSID being probed for"},
};

static const char* cli_categories[] = {"core","wifi","ble","comm","honeypot","defense","safety","persist","diag","attack","recon"};
static const char* cli_cat_desc[]   = {
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
static String cli_help_categories(){
  String s;
  s += "> ESP32 DOOM CLI\n";
  s += "> Available commands:\n";
  for (size_t i=0;i<sizeof(cli_categories)/sizeof(cli_categories[0]);i++){
    s += ">   ";
    s += cli_categories[i];
    int pad = 12 - strlen(cli_categories[i]);
    for (int k=0;k<pad;k++) s += ' ';
    s += "- ";
    s += cli_cat_desc[i];
    s += "\n";
  }
  s += ">\n";
  s += "> Type 'help <category>' for a category, 'help <command>' for details,\n";
  s += "> or 'help all' to list every command.\n";
  return cli_ok(s);
}

static String cli_help_category(const String& cat){
  String s;
  s += "> Category: ";
  s += cat;
  s += "\n>\n";
  int n = 0;
  for (size_t i=0;i<sizeof(cli_meta)/sizeof(cli_meta[0]);i++){
    if (cat == cli_meta[i].cat){
      n++;
      s += ">   ";
      s += cli_meta[i].name;
      int pad = 16 - strlen(cli_meta[i].name);
      if (pad < 1) pad = 1;
      for (int k=0;k<pad;k++) s += ' ';
      s += "- ";
      s += cli_meta[i].desc;
      s += "\n";
    }
  }
  if (!n) return cli_err("unknown category: '"+cat+"' (try 'help')");
  return cli_ok(s);
}

static String cli_help_command(const String& cmd){
  for (size_t i=0;i<sizeof(cli_meta)/sizeof(cli_meta[0]);i++){
    if (cmd == cli_meta[i].name){
      String s;
      s += "> ";
      s += cmd;
      s += " - ";
      s += cli_meta[i].desc;
      s += "\n";
      s += "> Usage: ";
      s += cli_meta[i].usage;
      s += "\n";
      if (cli_meta[i].args && strlen(cli_meta[i].args)){
        s += ">  Arguments:\n";
        s += cli_meta[i].args;
        s += "\n";
      }
      if (cli_meta[i].examples && strlen(cli_meta[i].examples)){
        s += ">  Examples:\n";
        s += cli_meta[i].examples;
        s += "\n";
      }
      return cli_ok(s);
    }
  }
  return cli_err("no help for '"+cmd+"'");
}

static String cli_help_all(){
  String s;
  for (size_t c=0;c<sizeof(cli_categories)/sizeof(cli_categories[0]);c++){
    s += "> == ";
    s += cli_categories[c];
    s += " ==\n";
    for (size_t i=0;i<sizeof(cli_meta)/sizeof(cli_meta[0]);i++){
      if (strcmp(cli_categories[c], cli_meta[i].cat) == 0){
        s += ">   ";
        s += cli_meta[i].name;
        int pad = 16 - strlen(cli_meta[i].name);
        if (pad < 1) pad = 1;
        for (int k=0;k<pad;k++) s += ' ';
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
static String cli_dispatch(const String& input){
  String toks[16];
  int n = cli_tok(input, toks, 16);
  if (n == 0) return "{\"ok\":1,\"out\":\"\"}";

  String cmd = cli_resolve_alias(toks[0]);
  cmd.toLowerCase();

  // help — GhostESP style
  if (cmd == "help"){
    if (n < 2) return cli_help_categories();
    String topic = toks[1];
    topic.toLowerCase();
    if (topic == "all") return cli_help_all();
    for (size_t i=0;i<sizeof(cli_categories)/sizeof(cli_categories[0]);i++)
      if (topic == cli_categories[i]) return cli_help_category(topic);
    for (size_t i=0;i<sizeof(cli_meta)/sizeof(cli_meta[0]);i++)
      if (topic == cli_meta[i].name) return cli_help_command(topic);
    return cli_err("no help for '"+topic+"' (try 'help')");
  }

  // --help on any command
  if (n >= 2 && toks[1] == "--help"){
    return cli_help_command(cmd);
  }

  // kv vs positional split
  String kvs, pos[8]; int npos = 0;
  for (int i=1;i<n;i++){
    int eq = toks[i].indexOf('=');
    if (eq > 0) { if (kvs.length()) kvs += "&"; kvs += toks[i]; }
    else if (npos < 8) pos[npos++] = toks[i];
  }

  if (cmd == "status"){
    String s;
    s += "> heap:   " + String(ESP.getFreeHeap()) + " bytes\n";
    s += "> uptime: " + String(millis()/1000) + " s\n";
    s += "> active: " + String(g_active ? g_active : "(none)") + "\n";
    s += "> tools:  " + String(g_tool_count) + "\n";
    return cli_ok(s);
  }
  if (cmd == "modules"){
    String s;
    for (int i=0;i<g_tool_count;i++){
      s += "> ";
      s += g_tools[i].slug;
      s += "  ";
      s += g_tools[i].name;
      if (g_tools[i].passive) s += " (passive)";
      s += "\n";
    }
    return cli_ok(s);
  }
  if (cmd == "heap")   return cli_ok("> " + String(ESP.getFreeHeap()) + " bytes free\n");
  if (cmd == "uptime") return cli_ok("> " + String(millis()/1000) + " s\n");

  if (cmd == "safety"){
    extern void safety_set_from_kvs(const String&);
    if (kvs.length()){ safety_set_from_kvs(kvs); return cli_ok("> saved\n"); }
    String s;
    s += "> dps=" + String(g_safety.deauth_per_sec) + "\n";
    s += "> bps=" + String(g_safety.beacon_per_sec) + "\n";
    s += "> pps=" + String(g_safety.probe_per_sec) + "\n";
    s += "> ast=" + String(g_safety.auto_stop_sec) + "\n";
    s += "> blocked: " + String(g_safety.blocked_bssids[0]?g_safety.blocked_bssids:"(none)") + "\n";
    return cli_ok(s);
  }
  if (cmd == "block"){
    if (npos < 1) return cli_help_command("block");
    safety_blocklist_add(pos[0]);
    return cli_ok("> blocked " + pos[0] + "\n");
  }
  if (cmd == "unblock"){ safety_blocklist_clear(); return cli_ok("> blocklist cleared\n"); }

  if (cmd == "start"){
    if (npos < 1) return cli_help_command("start");
    if (!tool_start(pos[0].c_str(), kvs)) return cli_err("could not start '"+pos[0]+"'");
    extern void safety_on_start(const char*);
    safety_on_start(pos[0].c_str());
    return cli_ok("> started " + pos[0] + "\n");
  }
  if (cmd == "stop"){
    if (npos >= 1){
      const ToolModule* m = tool_find(pos[0].c_str());
      if (!m) return cli_err("unknown tool: " + pos[0]);
      if (m->passive) return cli_err(pos[0] + " is passive");
      tool_stop(pos[0].c_str());
      return cli_ok("> stopped " + pos[0] + "\n");
    }
    if (!g_active) return cli_ok("> no active tool\n");
    String slug = g_active;
    tool_stop(slug.c_str());
    return cli_ok("> stopped " + slug + "\n");
  }

  // delegates
  extern String aegis_dispatch(const String&, const String&, const String*);
  extern String diag_dispatch(const String&, const String&, const String*);
  extern String persist_dispatch(const String&, const String&, const String*);
  if (cmd == "diag"){
    String r = diag_dispatch(cmd, kvs, pos);
    if (r.length()) return r;
  }
  if (cmd == "profile" || cmd == "schedule"){
    String r = persist_dispatch(cmd, kvs, pos);
    if (r.length()) return r;
  }
  {
    static const char* aegis_cmds[] = {"aegis","scan","aps","clients","events","attackers","channels","trust","untrust","trustclient","untrustclient"};
    for (size_t ai=0; ai<sizeof(aegis_cmds)/sizeof(aegis_cmds[0]); ai++){
      if (cmd == aegis_cmds[ai]){
        String r = aegis_dispatch(cmd, kvs, pos);
        if (r.length()) return r;
        break;
      }
    }
  }

  // tool shortcuts
  String slug = (cmd == "ble") ? "blespam" : cmd;
  const ToolModule* m = tool_find(slug.c_str());
  if (!m) return cli_err("unknown command: '"+cmd+"' (try 'help')");

  String args = kvs;
  auto addkv = [&](const char* k, const String& v){
    if (args.length()) args += "&";
    args += k; args += "="; args += v;
  };

  if (slug == "deauth"){
    if (npos>0){ if (safety_bssid_blocked_from_str(pos[0])) return cli_err("BSSID is blocked"); addkv("bssid", pos[0]); }
    if (npos>1) addkv("ch", pos[1]);
    if (npos>2) addkv("reason", pos[2]);
  } else if (slug == "beacon"){
    if (npos>0) addkv("ssid", pos[0]);
    if (npos>1) addkv("count", pos[1]);
  } else if (slug == "probe"){
    if (npos>0) addkv("ssid", pos[0]);
    if (npos>1) addkv("count", pos[1]);
  } else if (slug == "handshake"){
    if (npos>0) addkv("bssid", pos[0]);
    if (npos>1) addkv("ch", pos[1]);
  } else if (slug == "portal"){
    if (npos>0) addkv("ssid", pos[0]);
  } else if (slug == "blespam"){
    if (npos>0) addkv("mode", pos[0]);
  } else if (slug == "i2c"){
    if (npos>0) addkv("sda", pos[0]);
    if (npos>1) addkv("scl", pos[1]);
    if (npos>2) addkv("addr", pos[2]);
  } else if (slug == "spi"){
    if (npos>0) addkv("clk", pos[0]);
    if (npos>1) addkv("mosi", pos[1]);
    if (npos>2) addkv("miso", pos[2]);
    if (npos>3) addkv("cs", pos[3]);
  } else if (slug == "uart"){
    if (npos>0) addkv("rx", pos[0]);
    if (npos>1) addkv("tx", pos[1]);
    if (npos>2) addkv("baud", pos[2]);
  }

  if (!tool_start(slug.c_str(), args)) return cli_err("could not start '"+slug+"'");
  safety_on_start(slug.c_str());
  return cli_ok("> started " + slug + "\n");
}

// ---------- HTTP routes ----------
inline void cli_routes(){
  extern WebServer server;
  server.on("/api/cli", HTTP_POST, [](){
    String cmd = server.arg("plain");
    if (!cmd.length()) cmd = server.arg("cmd");
    // Fallback: form-encoded bodies get parsed into args. Reconstruct.
    if (!cmd.length() && server.args() > 0){
      if (server.args() == 1 && server.arg(0).length() == 0){
        cmd = server.argName(0);              // bare word, e.g. "help"
      } else {
        for (int i = 0; i < server.args(); i++){
          if (i) cmd += ' ';
          cmd += server.argName(i);
          if (server.arg(i).length()){ cmd += '='; cmd += server.arg(i); }
        }
      }
    }
    cmd.trim();
    if (cmd == "reboot"){
      server.send(200,"application/json","{\"ok\":1,\"out\":\"> rebooting...\\n\"}");
      delay(300);
      ESP.restart();
      return;
    }
    server.send(200,"application/json", cli_dispatch(cmd));
  });
  server.on("/api/cli/complete", HTTP_GET, [](){
    String pre = server.hasArg("q") ? server.arg("q") : "";
    String out = "[";
    bool first = true;
    auto add = [&](const String& s){
      if (!s.startsWith(pre)) return;
      if (!first) out += ",";
      first = false;
      out += "\"" + s + "\"";
    };
    for (size_t i=0;i<sizeof(cli_categories)/sizeof(cli_categories[0]);i++) add(String(cli_categories[i]));
    for (size_t i=0;i<sizeof(cli_meta)/sizeof(cli_meta[0]);i++) add(String(cli_meta[i].name));
    for (size_t i=0;i<sizeof(cli_aliases)/sizeof(cli_aliases[0]);i++) add(String(cli_aliases[i].alias));
    out += "]";
    server.send(200,"application/json",out);
  });
}

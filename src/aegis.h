/*
 * Aegis  —  passive WiFi defense monitor (IDS) for ESP32-WROOM-32D
 * ---------------------------------------------------------------------
 * Browser dashboard at http://192.168.4.1. Watches 2.4 GHz WiFi and warns
 * about attacks and weaknesses WITHOUT ever transmitting an attack frame.
 *
 * PASSIVE / DEFENSIVE ONLY — every feature is a listener.
 *
 * Detection & analysis (all passive):
 *   - AP scan + proximity radar; per-AP RSSI anomaly range
 *   - Beacon-IE analysis: PMF/802.11w, WPS, cipher (TKIP/CCMP/WEP/open),
 *     802.11 standard (b/g/n/ac/ax), hidden SSID, beacon interval, CSA
 *   - Deauth/disassoc detector: burst alarm, attacker ranking, victim + reason
 *   - Auth-flood, assoc-flood, probe-flood detectors
 *   - CSA (channel-switch) attack detection
 *   - Evil-twin (vendor/enc aware) + karma/multi-SSID AP detection
 *   - Trusted-AP watchlist (saved in flash): impersonation / disappeared /
 *     channel-change alerts
 *   - Nearby-client monitor: probe PNL, randomized-MAC, tracking-exposure meter
 *   - Jamming / RF-collapse detection
 *   - Vendor (OUI) names, event log + CSV, on-board LED alert, JSON API
 *
 * Board: ESP32 Dev Module (WROOM-32/32D). Core: Espressif esp32. Libs: none.
 * Needs index_html.h beside this file.
 *
 * Use only where you are permitted to observe the local RF environment.
 */

#include <WiFi.h>
#include <WebServer.h>
#include <esp_wifi.h>
#include <Preferences.h>
#include <LittleFS.h>
#include "types.h"
#include "frames.h"

#if defined(ARDUINO_SEEED_XIAO_ESP32S3)
const int LED_PIN = 21;  // XIAO S3 LED, active LOW
#define LED_ON digitalWrite(LED_PIN, LOW)
#define LED_OFF digitalWrite(LED_PIN, HIGH)
#else
const int LED_PIN = 2;
#define LED_ON digitalWrite(LED_PIN, HIGH)
#define LED_OFF digitalWrite(LED_PIN, LOW)
#endif

// ---- detection thresholds (defaults; editable live from the dashboard, saved in flash) ----
uint32_t BEACON_FLOOD_RATE = 1500;  // beacons/s, sustained >=3s
uint32_t DEAUTH_ALERT_MIN = 8;      // deauth+disassoc / 10s
uint32_t AUTH_FLOOD_RATE = 40;      // auth frames/s
uint32_t ASSOC_FLOOD_RATE = 40;     // assoc+reassoc frames/s
uint32_t PROBE_FLOOD_RATE = 120;    // probe-request frames/s
void loadCfg() {
  Preferences p;
  p.begin("gwcfg", true);
  BEACON_FLOOD_RATE = p.getUInt("bf", 1500);
  DEAUTH_ALERT_MIN = p.getUInt("da", 8);
  AUTH_FLOOD_RATE = p.getUInt("af", 40);
  ASSOC_FLOOD_RATE = p.getUInt("sf", 40);
  PROBE_FLOOD_RATE = p.getUInt("pf", 120);
  p.end();
}
void saveCfg() {
  Preferences p;
  p.begin("gwcfg", false);
  p.putUInt("bf", BEACON_FLOOD_RATE);
  p.putUInt("da", DEAUTH_ALERT_MIN);
  p.putUInt("af", AUTH_FLOOD_RATE);
  p.putUInt("sf", ASSOC_FLOOD_RATE);
  p.putUInt("pf", PROBE_FLOOD_RATE);
  p.end();
}

Preferences prefs;

// ---- scan state ----
String scanJson = "[]";
int apCount = 0, strongestRssi = -127;
String strongestSsid = "";
unsigned long lastScanDone = 0;
bool scanRunning = false, baselineDone = false;
const unsigned long SCAN_INTERVAL =
    30000;  // only re-scan every 30s, never while clients are connected

// ---- known-BSSID baseline (new-AP alerts) ----
const int MAXB = 96;
String knownBssid[MAXB];
int knownN = 0;
bool bssidKnown(const String &b) {
  for (int i = 0; i < knownN; i++)
    if (knownBssid[i] == b)
      return true;
  return false;
}
void addKnown(const String &b) {
  if (knownN < MAXB)
    knownBssid[knownN++] = b;
}

// ---- beacon-IE enrichment table (keyed by BSSID; ApInfo in types.h) ----
const int MAXAP = 56;
ApInfo apinfo[MAXAP];
ApInfo *apFind(const uint8_t *b) {
  int fi = -1;
  for (int i = 0; i < MAXAP; i++) {
    if (apinfo[i].used && !memcmp(apinfo[i].bssid, b, 6))
      return &apinfo[i];
    if (!apinfo[i].used && fi < 0)
      fi = i;
  }
  if (fi < 0) {
    unsigned long o = ~0UL;
    for (int i = 0; i < MAXAP; i++)
      if (apinfo[i].last < o) {
        o = apinfo[i].last;
        fi = i;
      }
  }
  ApInfo *a = &apinfo[fi];
  memset(a, 0, sizeof(ApInfo));
  memcpy(a->bssid, b, 6);
  a->used = true;
  a->rmin = 0;
  a->rmax = -127;
  a->std = 0;
  return a;
}
String bkey(const uint8_t *b) {
  char s[18];
  snprintf(s, sizeof(s), "%02X:%02X:%02X:%02X:%02X:%02X", b[0], b[1], b[2], b[3], b[4], b[5]);
  return String(s);
}

// ---- detector counters (set in sniffer) ----
volatile uint32_t deauthCount = 0, disassocCount = 0, beaconCount = 0, probeReqCount = 0,
                  authCount = 0, assocCount = 0, eapolStartCount = 0;
const uint32_t EAPOL_FLOOD_RATE = 20;  // EAPOL-Start frames/s = 802.1X handshake DoS
volatile unsigned long lastAttackMs = 0, lastCsaMs = 0;
volatile int lastAttackRssi = 0;
volatile bool lastWasDeauth = false;
volatile uint16_t lastReason = 0;
uint8_t lastAttackSrc[6] = {0}, lastAttackDst[6] = {0};
volatile uint32_t chanCount[15] = {0};
volatile uint32_t seqAnomalies = 0;

// ---- attacker table (Atk in types.h) ----
const int MAXA = 16;
Atk atk[MAXA];
void noteAttacker(const uint8_t *m, int rssi) {
  int fi = -1;
  for (int i = 0; i < MAXA; i++) {
    if (atk[i].used && !memcmp(atk[i].mac, m, 6)) {
      atk[i].count++;
      atk[i].rssi = rssi;
      return;
    }
    if (!atk[i].used && fi < 0)
      fi = i;
  }
  if (fi < 0) {
    uint32_t lo = ~0UL;
    for (int i = 0; i < MAXA; i++)
      if (atk[i].count < lo) {
        lo = atk[i].count;
        fi = i;
      }
  }
  memcpy(atk[fi].mac, m, 6);
  atk[fi].count = 1;
  atk[fi].rssi = rssi;
  atk[fi].used = true;
}

// ---- client table (NearDev in types.h) ----
const int MAXC = 48;
NearDev clients[MAXC];
void noteClient(const uint8_t *m, int rssi, const char *ssid) {
  int fi = -1;
  for (int i = 0; i < MAXC; i++) {
    if (clients[i].used && !memcmp(clients[i].mac, m, 6)) {
      clients[i].rssi = rssi;
      clients[i].last = millis();
      if (ssid && ssid[0])
        strncpy(clients[i].ssid, ssid, 32);
      return;
    }
    if (!clients[i].used && fi < 0)
      fi = i;
  }
  if (fi < 0) {
    unsigned long o = ~0UL;
    for (int i = 0; i < MAXC; i++)
      if (clients[i].last < o) {
        o = clients[i].last;
        fi = i;
      }
  }
  memcpy(clients[fi].mac, m, 6);
  clients[fi].rssi = rssi;
  clients[fi].last = millis();
  clients[fi].rand = (m[0] & 0x02);
  clients[fi].ssid[0] = 0;
  if (ssid && ssid[0])
    strncpy(clients[fi].ssid, ssid, 32);
  clients[fi].used = true;
}

// ---- trusted-client whitelist (mark your own devices; flag close unknowns) ----
String trustedClients = "";
void loadTrustedClients() {
  Preferences p;
  p.begin("gwcli", true);
  trustedClients = p.getString("list", "");
  p.end();
}
bool clientTrusted(const String &mac) {
  return mac.length() == 17 && trustedClients.indexOf(mac) >= 0;
}
void setClientTrust(const String &mac, bool on) {
  String cur = trustedClients, out = "";
  int start = 0;
  while (start < (int)cur.length()) {
    int c = cur.indexOf(',', start);
    String tok = (c < 0) ? cur.substring(start) : cur.substring(start, c);
    tok.trim();
    if (tok.length() && tok != mac)
      out += (out.length() ? "," : "") + tok;
    if (c < 0)
      break;
    start = c + 1;
  }
  if (on && out.indexOf(mac) < 0)
    out += (out.length() ? "," : "") + mac;
  Preferences p;
  p.begin("gwcli", false);
  p.putString("list", out);
  p.end();
  trustedClients = out;
}

// ---- event log (Event in types.h) ----
const int MAXE = 64;
Event events[MAXE];
int evHead = 0;
String pendingHook = "";  // station webhook consumes this on 'a' (critical) events
bool fsOk = false;        // LittleFS mounted — persists alert/warning events across reboots

void persistAppend(unsigned long t, char sev, const String &m) {
  if (!fsOk || (sev != 'a' && sev != 'w'))
    return;  // only persist alerts/warnings (limits flash wear)
  File f = LittleFS.open("/events.log", "a");
  if (!f)
    return;
  f.print(t);
  f.print('|');
  f.print(sev);
  f.print('|');
  f.println(m);
  size_t sz = f.size();
  f.close();
  if (sz > 24576) {
    File r = LittleFS.open("/events.log", "r");
    if (r) {
      if (sz > 8192)
        r.seek(sz - 8192);
      String tail = r.readString();
      r.close();
      int nl = tail.indexOf('\n');
      if (nl >= 0)
        tail = tail.substring(nl + 1);
      File w = LittleFS.open("/events.log", "w");
      if (w) {
        w.print(tail);
        w.close();
      }
    }
  }
}
void persistLoad() {
  if (!fsOk)
    return;
  File f = LittleFS.open("/events.log", "r");
  if (!f)
    return;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() < 5)
      continue;
    int a = line.indexOf('|'), b = line.indexOf('|', a + 1);
    if (a < 1 || b < a + 1)
      continue;
    events[evHead].t = line.substring(0, a).toInt();
    events[evHead].sev = line.charAt(a + 1);
    events[evHead].msg = "(saved) " + line.substring(b + 1);
    events[evHead].used = true;
    evHead = (evHead + 1) % MAXE;
  }
  f.close();
}
void logEvent(char sev, const String &m) {
  unsigned long tt = millis() / 1000;
  events[evHead].t = tt;
  events[evHead].sev = sev;
  events[evHead].msg = m;
  events[evHead].used = true;
  evHead = (evHead + 1) % MAXE;
  if (sev == 'a')
    pendingHook = m;
  Serial.printf("[%c] %s\n", sev, m.c_str());
  persistAppend(tt, sev, m);
}

// ---- per-second rates / windows ----
uint32_t beaconRate = 0, lastBeaconCount = 0, authRate = 0, lastAuth = 0, assocRate = 0,
         lastAssoc = 0, probeRate = 0, lastProbe = 0, mgmtRate = 0, lastMgmtTotal = 0,
         eapolRate = 0, lastEapol = 0;
unsigned long lastRateMs = 0;
int floodStreak = 0;
bool floodOn = false;
uint32_t atkBuckets[10] = {0};
int atkIdx = 0;
uint32_t lastAtkTotal = 0, atkWindow = 0;
bool burstLogged = false;
int jamStreak = 0;
bool jamOn = false;

// ---- trusted-AP watchlist ----
String tSsid = "";
String tBssid = "";
int tCh = 0;
bool hasTrust = false;
int trustMiss = 0;
bool alertForeign = false, alertGone = false, alertChan = false;
String foreignBssid = "";
void loadTrust() {
  prefs.begin("gw", true);
  tSsid = prefs.getString("ts", "");
  tBssid = prefs.getString("tb", "");
  tCh = prefs.getInt("tc", 0);
  prefs.end();
  hasTrust = tSsid.length() > 0;
}
void saveTrust(const String &s, const String &b, int c) {
  prefs.begin("gw", false);
  prefs.putString("ts", s);
  prefs.putString("tb", b);
  prefs.putInt("tc", c);
  prefs.end();
  tSsid = s;
  tBssid = b;
  tCh = c;
  hasTrust = s.length() > 0;
  trustMiss = 0;
  alertForeign = alertGone = alertChan = false;
  foreignBssid = "";
  logEvent('i', "Trusted AP set: \"" + s + "\" (" + b + ")");
}
void clearTrust() {
  prefs.begin("gw", false);
  prefs.clear();
  prefs.end();
  tSsid = "";
  tBssid = "";
  tCh = 0;
  hasTrust = false;
  alertForeign = alertGone = alertChan = false;
  logEvent('i', "Trusted AP cleared");
}

// ---- diag packet-capture hook (set by diag.h) ----
static void (*g_diag_pkt_hook)(uint8_t fc0, uint8_t fc1, const uint8_t *sa, const uint8_t *da,
                               const uint8_t *bssid) = nullptr;

// ---- promiscuous RX ----
uint32_t fnvHash(const char *s, int n) {
  uint32_t h = 2166136261u;
  for (int i = 0; i < n; i++) {
    h ^= (uint8_t)s[i];
    h *= 16777619u;
  }
  return h;
}
void aegisSniff(void *buf, wifi_promiscuous_pkt_type_t type) {
  const wifi_promiscuous_pkt_t *p = (const wifi_promiscuous_pkt_t *)buf;
  const uint8_t *pl = p->payload;
  int len = p->rx_ctrl.sig_len;
  if (type == WIFI_PKT_DATA) {  // lean path: only count EAPOL-Start (802.1X) frames
    if (len < 34)
      return;
    uint8_t fc0 = pl[0];
    if (((fc0 & 0x0C) >> 2) != 2)
      return;
    int hdr = 24;
    if ((fc0 & 0x03) == 0x03)
      hdr += 6;
    if (((fc0 & 0xF0) >> 4) >= 8)
      hdr += 2;  // addr4 + QoS
    if (hdr + 9 > len)
      return;
    if (pl[hdr] == 0xAA && pl[hdr + 1] == 0xAA && pl[hdr + 6] == 0x88 && pl[hdr + 7] == 0x8E &&
        pl[hdr + 9] == 1)
      eapolStartCount++;
    return;
  }
  if (type != WIFI_PKT_MGMT)
    return;
  int rssi = p->rx_ctrl.rssi;
  uint8_t ch = p->rx_ctrl.channel;
  if (ch >= 1 && ch <= 14)
    chanCount[ch]++;
  uint8_t fc = pl[0];
  if (((fc & 0x0C) >> 2) != 0)
    return;
  uint8_t subtype = (fc & 0xF0) >> 4;
  if (g_diag_pkt_hook && len >= 24)
    g_diag_pkt_hook(fc, pl[1], pl + 10, pl + 4, pl + 16);
  switch (subtype) {
    case 12:
    case 10: {
      if (subtype == 12) {
        deauthCount++;
        lastWasDeauth = true;
      } else {
        disassocCount++;
        lastWasDeauth = false;
      }
      lastAttackMs = millis();
      lastAttackRssi = rssi;
      for (int i = 0; i < 6; i++) {
        lastAttackSrc[i] = pl[10 + i];
        lastAttackDst[i] = pl[4 + i];
      }
      if (len >= 26)
        lastReason = pl[24] | (pl[25] << 8);
      noteAttacker(pl + 10, rssi);
    } break;
    case 11:
      authCount++;
      break;  // authentication
    case 0:
    case 2:
      assocCount++;
      break;  // assoc / reassoc request
    case 4: {
      probeReqCount++;
      char s[33] = {0};
      if (len >= 26 && pl[24] == 0) {
        int l = pl[25];
        if (l > 32)
          l = 32;
        for (int i = 0; i < l; i++) {
          char c = pl[26 + i];
          s[i] = (c >= 32 && c < 127) ? c : '.';
        }
        s[l] = 0;
      }
      noteClient(pl + 10, rssi, s);
    } break;
    case 8:
    case 5: {  // beacon / probe response -> parse IEs
      beaconCount++;
      ApInfo *a = apFind(pl + 10);
      a->last = millis();
      if (rssi < a->rmin || a->rmin == 0)
        a->rmin = rssi;
      if (rssi > a->rmax)
        a->rmax = rssi;
      if (len >= 36) {
        a->bint = pl[32] | (pl[33] << 8);
        uint16_t cap = pl[34] | (pl[35] << 8);
        a->wep = (cap & 0x10) ? a->wep : false;
        bool priv = cap & 0x10;
        bool ht = false, vht = false, he = false, rsn = false;
        int q = 36;
        char ssid[33] = {0};
        int slen = 0;
        while (q + 2 <= len) {
          uint8_t tag = pl[q], tl = pl[q + 1];
          if (q + 2 + tl > len)
            break;
          const uint8_t *d = pl + q + 2;
          if (tag == 0) {
            slen = tl > 32 ? 32 : tl;
            for (int i = 0; i < slen; i++) {
              char c = d[i];
              ssid[i] = (c >= 32 && c < 127) ? c : '.';
            }
            ssid[slen] = 0;
            a->hidden = (tl == 0);
          } else if (tag == 37) {
            a->csa = true;
            lastCsaMs = millis();
          } else if (tag == 45)
            ht = true;
          else if (tag == 191)
            vht = true;
          else if (tag == 255) {
            if (tl >= 1 && d[0] == 35)
              he = true;
          } else if (tag == 48) {
            rsn = true;  // RSN IE -> cipher + PMF
            if (tl >= 8) {
              int r = 2;
              uint8_t gt = d[r + 3];
              r += 4;  // group cipher
              if (gt == 2)
                a->tkip = true;
              if (gt == 4)
                a->ccmp = true;
              if (r + 2 <= tl) {
                int pc = d[r] | (d[r + 1] << 8);
                r += 2;
                for (int i = 0; i < pc && r + 4 <= tl; i++) {
                  uint8_t t = d[r + 3];
                  if (t == 2)
                    a->tkip = true;
                  if (t == 4)
                    a->ccmp = true;
                  r += 4;
                }
                if (r + 2 <= tl) {
                  int ac = d[r] | (d[r + 1] << 8);
                  r += 2;
                  r += ac * 4;
                  if (r + 2 <= tl) {
                    uint16_t rc = d[r] | (d[r + 1] << 8);
                    a->pmfC = rc & 0x80;
                    a->pmfR = rc & 0x40;
                  }
                }
              }
            }
          } else if (tag == 221 && tl >= 4) {
            if (d[0] == 0x00 && d[1] == 0x50 && d[2] == 0xF2) {
              if (d[3] == 0x04)
                a->wps = true;
              if (d[3] == 0x01)
                a->tkip = true;
            }
          }
          q += 2 + tl;
        }
        a->std = he ? 5 : (vht ? 4 : (ht ? 3 : 2));
        if (priv && !rsn && !a->tkip && !a->ccmp)
          a->wep = true;  // privacy but no RSN/WPA -> WEP
        // distinct-SSID tracking (karma / known-beacon)
        if (slen > 0) {
          uint32_t h = fnvHash(ssid, slen);
          bool seen = false;
          for (int i = 0; i < a->ssidCount && i < 4; i++)
            if (a->ssidHash[i] == h)
              seen = true;
          if (!seen && a->ssidCount < 4) {
            a->ssidHash[a->ssidCount++] = h;
          }
        }
      }
    } break;
  }
}
void enableDetector() {
  esp_wifi_set_promiscuous_rx_cb(aegisSniff);
  esp_wifi_set_promiscuous(true);
  wifi_promiscuous_filter_t f = {};
  f.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA;
  esp_wifi_set_promiscuous_filter(&f);
}

// ---- helpers ----
String jsonEscape(const String &s) {
  String o;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') {
      o += '\\';
      o += c;
    } else if (c == '\n' || c == '\r' || c == '\t')
      o += ' ';
    else if ((uint8_t)c < 0x20) {
    } else
      o += c;
  }
  return o;
}
String macStr(const uint8_t *m) {
  char b[18];
  snprintf(b, sizeof(b), "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
  return String(b);
}
String encName(wifi_auth_mode_t e) {
  switch (e) {
    case WIFI_AUTH_OPEN:
      return "OPEN";
    case WIFI_AUTH_WEP:
      return "WEP";
    case WIFI_AUTH_WPA_PSK:
      return "WPA";
    case WIFI_AUTH_WPA2_PSK:
      return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:
      return "WPA/2";
    case WIFI_AUTH_WPA2_ENTERPRISE:
      return "WPA2-E";
    case WIFI_AUTH_WPA3_PSK:
      return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK:
      return "WPA2/3";
    default:
      return "?";
  }
}
const char *stdName(uint8_t s) {
  switch (s) {
    case 5:
      return "ax";
    case 4:
      return "ac";
    case 3:
      return "n";
    case 2:
      return "g";
    default:
      return "";
  }
}

#include "station.h"  // optional station-mode: LAN inventory, ARP/DHCP IDS, NTP, webhook, DLNA cast
#include "aegis_ble.h"  // passive BLE: inventory + spam + Find My/AirTag detection

void buildScanJson(int n) {
  String j = "[";
  apCount = n < 0 ? 0 : n;
  strongestRssi = -127;
  strongestSsid = "";
  bool trustSeen = false;
  for (int i = 0; i < n; i++) {
    if (i)
      j += ",";
    String ssid = WiFi.SSID(i);
    if (ssid.length() == 0)
      ssid = "(hidden)";
    String bssid = WiFi.BSSIDstr(i);
    int rssi = WiFi.RSSI(i);
    int chan = WiFi.channel(i);
    if (rssi > strongestRssi) {
      strongestRssi = rssi;
      strongestSsid = ssid;
    }
    if (baselineDone && !bssidKnown(bssid)) {
      addKnown(bssid);
      logEvent('i', "New AP appeared: \"" + ssid + "\" (" + bssid + ")");
    } else if (!bssidKnown(bssid))
      addKnown(bssid);
    // evil-twin: the classic signature is a clone that's OPEN while the real one
    // is secured (or vice-versa). Same-name networks that share security level
    // are legit mesh / extenders / guest+IoT APs, so they are NOT flagged.
    wifi_auth_mode_t encI = WiFi.encryptionType(i);
    int evil = 0;
    bool thisOpen = (encI == WIFI_AUTH_OPEN);
    for (int k = 0; k < n; k++) {
      if (k == i)
        continue;
      if (WiFi.SSID(k).length() == 0)
        continue;
      if (WiFi.SSID(k) == WiFi.SSID(i) && WiFi.BSSIDstr(k) != bssid) {
        if (thisOpen != (WiFi.encryptionType(k) == WIFI_AUTH_OPEN)) {
          evil = 1;
          break;
        }
      }
    }
    // enrichment lookup
    uint8_t bb[6];
    {
      unsigned v[6];
      sscanf(bssid.c_str(), "%x:%x:%x:%x:%x:%x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]);
      for (int z = 0; z < 6; z++)
        bb[z] = v[z];
    }
    ApInfo *a = 0;
    for (int z = 0; z < MAXAP; z++)
      if (apinfo[z].used && !memcmp(apinfo[z].bssid, bb, 6)) {
        a = &apinfo[z];
        break;
      }
    int pmf = a ? (a->pmfR ? 2 : (a->pmfC ? 1 : 0)) : -1;  // -1 unknown,0 none,1 capable,2 required
    String cipher = a ? (a->ccmp ? (a->tkip ? "TKIP+CCMP" : "CCMP")
                                 : (a->tkip ? "TKIP" : (a->wep ? "WEP" : "")))
                      : "";
    int wps = a ? (a->wps ? 1 : 0) : 0;
    int csa = a ? (a->csa ? 1 : 0) : 0;
    int multi = a ? (a->ssidCount >= 3 ? 1 : 0) : 0;
    String st = a ? stdName(a->std) : "";
    int rspread = (a && a->rmax > a->rmin && a->rmin != 0) ? (a->rmax - a->rmin) : 0;
    // trusted-AP checks
    if (hasTrust && ssid == tSsid) {
      trustSeen = true;
      if (bssid != tBssid) {
        if (!alertForeign) {
          alertForeign = true;
          foreignBssid = bssid;
          logEvent('a', "Impersonation: \"" + tSsid + "\" seen on foreign BSSID " + bssid);
        }
      }
      if (chan != tCh && tCh != 0) {
        if (!alertChan) {
          alertChan = true;
          logEvent('w', "Trusted AP channel changed to " + String(chan));
        }
      }
    }
    j += "{\"ssid\":\"" + jsonEscape(ssid) + "\",\"bssid\":\"" + bssid +
         "\",\"rssi\":" + String(rssi) + ",\"ch\":" + String(chan) + ",\"enc\":\"" + encName(encI) +
         "\",\"evil\":" + String(evil) + ",\"pmf\":" + String(pmf) + ",\"cipher\":\"" + cipher +
         "\",\"wps\":" + String(wps) + ",\"csa\":" + String(csa) + ",\"multi\":" + String(multi) +
         ",\"std\":\"" + st + "\",\"rspread\":" + String(rspread) +
         ",\"trusted\":" + String((hasTrust && bssid == tBssid) ? 1 : 0) + "}";
  }
  j += "]";
  scanJson = j;
  if (hasTrust) {
    if (!trustSeen) {
      if (++trustMiss >= 3 && !alertGone) {
        alertGone = true;
        logEvent('a', "Trusted AP \"" + tSsid + "\" disappeared (possible jamming/takedown)");
      }
    } else {
      trustMiss = 0;
      alertGone = false;
    }
  }
  if (!baselineDone)
    baselineDone = true;
}

// ---- HTTP ----
void handleRoot();
void handleScan() {
  server.send(200, "application/json", scanJson);
}
void handleClients() {
  String j = "[";
  bool f = true;
  unsigned long now = millis();
  for (int i = 0; i < MAXC; i++) {
    if (!clients[i].used)
      continue;
    unsigned long ago = (now - clients[i].last) / 1000;
    if (ago > 120)
      continue;
    if (!f)
      j += ",";
    f = false;
    String cm = macStr(clients[i].mac);
    j += "{\"mac\":\"" + cm + "\",\"rssi\":" + String(clients[i].rssi) + ",\"ago\":" + String(ago) +
         ",\"rand\":" + String(clients[i].rand ? 1 : 0) + ",\"ssid\":\"" +
         jsonEscape(String(clients[i].ssid)) +
         "\",\"trusted\":" + String(clientTrusted(cm) ? 1 : 0) + "}";
  }
  j += "]";
  server.send(200, "application/json", j);
}
void handleTrustClient() {
  String mac = server.arg("mac");
  bool on = (server.arg("on") != "0");
  if (mac.length() != 17) {
    server.send(400, "application/json", "{\"ok\":0}");
    return;
  }
  setClientTrust(mac, on);
  logEvent('i', String(on ? "Trusted" : "Untrusted") + " client " + mac);
  server.send(200, "application/json", "{\"ok\":1}");
}
void handleAttackers() {
  String j = "[";
  bool f = true;
  for (int i = 0; i < MAXA; i++) {
    if (!atk[i].used)
      continue;
    if (!f)
      j += ",";
    f = false;
    j += "{\"mac\":\"" + macStr(atk[i].mac) + "\",\"count\":" + String(atk[i].count) +
         ",\"rssi\":" + String(atk[i].rssi) + "}";
  }
  j += "]";
  server.send(200, "application/json", j);
}
void handleChannels() {
  String j = "[";
  for (int c = 1; c <= 14; c++) {
    if (c > 1)
      j += ",";
    j += String(chanCount[c]);
  }
  j += "]";
  server.send(200, "application/json", j);
}
void handleEvents() {
  String j = "[";
  bool f = true;
  for (int k = 0; k < MAXE; k++) {
    int idx = (evHead - 1 - k + MAXE * 2) % MAXE;
    if (!events[idx].used)
      continue;
    if (!f)
      j += ",";
    f = false;
    j += "{\"t\":" + String(events[idx].t) + ",\"sev\":\"" + String(events[idx].sev) +
         "\",\"msg\":\"" + jsonEscape(events[idx].msg) + "\"}";
  }
  j += "]";
  server.send(200, "application/json", j);
}
void handleEventsCsv() {
  String c = "time_s,severity,event\n";
  for (int k = 0; k < MAXE; k++) {
    int idx = (evHead - 1 - k + MAXE * 2) % MAXE;
    if (!events[idx].used)
      continue;
    String m = events[idx].msg;
    m.replace("\"", "'");
    c += String(events[idx].t) + "," + String(events[idx].sev) + ",\"" + m + "\"\n";
  }
  server.sendHeader("Content-Disposition", "attachment; filename=ghostwatch_events.csv");
  server.send(200, "text/csv", c);
}
void handleTrust() {
  String s = server.arg("ssid"), b = server.arg("bssid");
  int c = server.arg("ch").toInt();
  if (s.length() && b.length()) {
    saveTrust(s, b, c);
    server.send(200, "application/json", "{\"ok\":1}");
  } else
    server.send(400, "application/json", "{\"ok\":0}");
}
void handleUntrust() {
  clearTrust();
  server.send(200, "application/json", "{\"ok\":1}");
}
String stationStats();
String bleStats();
void handleStats() {
  long ago = (lastAttackMs == 0) ? -1 : (long)((millis() - lastAttackMs) / 1000);
  long csaAgo = (lastCsaMs == 0) ? -1 : (long)((millis() - lastCsaMs) / 1000);
  int clientN = 0, randN = 0;
  unsigned long now = millis();
  for (int i = 0; i < MAXC; i++)
    if (clients[i].used && (now - clients[i].last) / 1000 <= 120) {
      clientN++;
      if (clients[i].rand)
        randN++;
    }
  int expo = clientN ? (100 - (randN * 100 / clientN)) : 0;  // % identifiable (non-randomized)
  String s = "{";
  s += "\"count\":" + String(apCount) + ",\"strongestRssi\":" + String(strongestRssi) +
       ",\"strongestSsid\":\"" + jsonEscape(strongestSsid) + "\",";
  s += "\"clients\":" + String(clientN) + ",\"expo\":" + String(expo) + ",";
  s += "\"deauth\":" + String(deauthCount) + ",\"disassoc\":" + String(disassocCount) +
       ",\"atkWindow\":" + String(atkWindow) +
       ",\"attack\":" + String(atkWindow >= DEAUTH_ALERT_MIN ? 1 : 0) + ",";
  s += "\"lastAgo\":" + String(ago) + ",\"lastType\":\"" +
       String(lastWasDeauth ? "deauth" : "disassoc") + "\",\"lastSrc\":\"" + macStr(lastAttackSrc) +
       "\",\"lastDst\":\"" + macStr(lastAttackDst) + "\",\"reason\":" + String(lastReason) +
       ",\"lastRssi\":" + String(lastAttackRssi) + ",";
  s += "\"beaconRate\":" + String(beaconRate) + ",\"flood\":" + String(floodOn ? 1 : 0) + ",";
  s += "\"authRate\":" + String(authRate) +
       ",\"authFlood\":" + String(authRate > AUTH_FLOOD_RATE ? 1 : 0) + ",";
  s += "\"assocRate\":" + String(assocRate) +
       ",\"assocFlood\":" + String(assocRate > ASSOC_FLOOD_RATE ? 1 : 0) + ",";
  s += "\"probeRate\":" + String(probeRate) +
       ",\"probeFlood\":" + String(probeRate > PROBE_FLOOD_RATE ? 1 : 0) + ",";
  s += "\"csaAgo\":" + String(csaAgo) + ",\"jam\":" + String(jamOn ? 1 : 0) + ",";
  s += "\"trust\":\"" + jsonEscape(tSsid) + "\",\"trustBssid\":\"" + tBssid +
       "\",\"foreign\":" + String(alertForeign ? 1 : 0) + ",\"gone\":" + String(alertGone ? 1 : 0) +
       ",\"chan\":" + String(alertChan ? 1 : 0) + ",\"foreignBssid\":\"" + foreignBssid + "\",";
  s += stationStats() + ",";
  s += bleStats() + ",";
  s += "\"heap\":" + String(ESP.getFreeHeap()) + ",\"uptime\":" + String(millis() / 1000) + "}";
  server.send(200, "application/json", s);
}
void handleConfig() {
  bool ch = false;
  if (server.hasArg("bf")) {
    BEACON_FLOOD_RATE = server.arg("bf").toInt();
    ch = true;
  }
  if (server.hasArg("da")) {
    DEAUTH_ALERT_MIN = server.arg("da").toInt();
    ch = true;
  }
  if (server.hasArg("af")) {
    AUTH_FLOOD_RATE = server.arg("af").toInt();
    ch = true;
  }
  if (server.hasArg("sf")) {
    ASSOC_FLOOD_RATE = server.arg("sf").toInt();
    ch = true;
  }
  if (server.hasArg("pf")) {
    PROBE_FLOOD_RATE = server.arg("pf").toInt();
    ch = true;
  }
  if (ch) {
    saveCfg();
    logEvent('i', "Detection thresholds updated");
  }
  server.send(200, "application/json",
              "{\"bf\":" + String(BEACON_FLOOD_RATE) + ",\"da\":" + String(DEAUTH_ALERT_MIN) +
                  ",\"af\":" + String(AUTH_FLOOD_RATE) + ",\"sf\":" + String(ASSOC_FLOOD_RATE) +
                  ",\"pf\":" + String(PROBE_FLOOD_RATE) + "}");
}
void handleNotFound() {
  server.send(404, "text/plain", "not found");
}

void aegisSetup() {
  pinMode(LED_PIN, OUTPUT);
  LED_OFF;
  for (int i = 0; i < MAXC; i++)
    clients[i].used = false;
  for (int i = 0; i < MAXA; i++)
    atk[i].used = false;
  for (int i = 0; i < MAXE; i++)
    events[i].used = false;
  for (int i = 0; i < MAXAP; i++)
    apinfo[i].used = false;
  if (LittleFS.begin(true)) {
    fsOk = true;
    persistLoad();
  }  // restore saved alert history
  loadCfg();
  loadTrust();
  loadTrustedClients();

  stationSetup();  // joins the user's network if they configured one
  enableDetector();
  bleSetup();  // start passive BLE scanning (coexists with WiFi)
  // Delay initial scan so the AP is fully up before we take the radio
  lastScanDone = millis();
  lastRateMs = millis();
  scanRunning = false;
  logEvent('i', "Aegis started — passive listening");
}

void aegisRoutes() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/scan", HTTP_GET, handleScan);
  server.on("/api/clients", HTTP_GET, handleClients);
  server.on("/api/attackers", HTTP_GET, handleAttackers);
  server.on("/api/channels", HTTP_GET, handleChannels);
  server.on("/api/events", HTTP_GET, handleEvents);
  server.on("/events.csv", HTTP_GET, handleEventsCsv);
  server.on("/api/trust", HTTP_GET, handleTrust);
  server.on("/api/untrust", HTTP_GET, handleUntrust);
  server.on("/api/stats", HTTP_GET, handleStats);
  server.on("/api/lan", HTTP_GET, handleLan);
  server.on("/api/wifi", HTTP_GET, handleWifiCfg);
  server.on("/api/cast", HTTP_GET, handleCast);
  server.on("/api/youtube", HTTP_GET, handleYouTube);
  server.on("/api/power", HTTP_GET, handlePower);
  server.on("/api/config", HTTP_GET, handleConfig);
  server.on("/api/trustclient", HTTP_GET, handleTrustClient);
  server.on("/api/ble", HTTP_GET, handleBle);
}

void aegisLoop() {
  stationLoop();

  if (millis() - lastRateMs >= 1000) {
    lastRateMs = millis();
    beaconRate = beaconCount - lastBeaconCount;
    lastBeaconCount = beaconCount;
    authRate = authCount - lastAuth;
    lastAuth = authCount;
    assocRate = assocCount - lastAssoc;
    lastAssoc = assocCount;
    probeRate = probeReqCount - lastProbe;
    lastProbe = probeReqCount;
    uint32_t mtot =
        beaconCount + authCount + assocCount + probeReqCount + deauthCount + disassocCount;
    mgmtRate = mtot - lastMgmtTotal;
    lastMgmtTotal = mtot;
    // beacon flood (sustained)
    if (beaconRate > BEACON_FLOOD_RATE)
      floodStreak++;
    else
      floodStreak = 0;
    bool nf = floodStreak >= 3;
    if (nf && !floodOn)
      logEvent('w', "Beacon flood: " + String(beaconRate) + "/s sustained");
    floodOn = nf;
    // flood edge events
    static bool afl = false, asl = false, pfl = false;
    if (authRate > AUTH_FLOOD_RATE) {
      if (!afl) {
        logEvent('a', "Auth flood: " + String(authRate) + "/s");
        afl = true;
      }
    } else
      afl = false;
    if (assocRate > ASSOC_FLOOD_RATE) {
      if (!asl) {
        logEvent('a', "Assoc flood: " + String(assocRate) + "/s");
        asl = true;
      }
    } else
      asl = false;
    if (probeRate > PROBE_FLOOD_RATE) {
      if (!pfl) {
        logEvent('w', "Probe flood: " + String(probeRate) + "/s");
        pfl = true;
      }
    } else
      pfl = false;
    eapolRate = eapolStartCount - lastEapol;
    lastEapol = eapolStartCount;
    static bool efl = false;
    if (eapolRate > EAPOL_FLOOD_RATE) {
      if (!efl) {
        logEvent('a', "EAPOL-Start flood: " + String(eapolRate) + "/s (802.1X handshake DoS)");
        efl = true;
      }
    } else
      efl = false;
    // deauth burst
    uint32_t total = deauthCount + disassocCount;
    atkBuckets[atkIdx] = total - lastAtkTotal;
    lastAtkTotal = total;
    atkIdx = (atkIdx + 1) % 10;
    uint32_t w = 0;
    for (int i = 0; i < 10; i++)
      w += atkBuckets[i];
    atkWindow = w;
    if (atkWindow >= DEAUTH_ALERT_MIN && !burstLogged) {
      logEvent('a', "Deauth burst: " + String(atkWindow) + "/10s, src " + macStr(lastAttackSrc) +
                        " -> victim " + macStr(lastAttackDst) + " (reason " + String(lastReason) +
                        ")");
      burstLogged = true;
    } else if (atkWindow == 0)
      burstLogged = false;
    // jamming: traffic collapse while APs are present
    if (apCount > 0 && mgmtRate == 0)
      jamStreak++;
    else
      jamStreak = 0;
    bool nj = jamStreak >= 3;
    if (nj && !jamOn)
      logEvent('a', "Possible jamming: no WiFi mgmt frames for 3s though " + String(apCount) +
                        " APs known");
    jamOn = nj;
    // BLE spam + unwanted-tracker detection
    bleTick();
    static bool bsp = false;
    if (bleSpam) {
      if (!bsp) {
        logEvent('w', "BLE spam: " + String(bleRate) + " adv/s (pairing-popup flood)");
        bsp = true;
      }
    } else
      bsp = false;
    // Find My/AirTag devices are shown + counted in the BLE panel, not auto-alerted
    // (your own Apple gear advertises Find My, so alerting on every one would be noise)
    // unknown-device-very-close alert — conservative: real (non-random) MAC, strong signal, not
    // whitelisted
    for (int i = 0; i < MAXC; i++) {
      if (!clients[i].used || clients[i].alerted || clients[i].rand)
        continue;
      if ((millis() - clients[i].last) / 1000 > 8)
        continue;
      if (clients[i].rssi > -50 && !clientTrusted(macStr(clients[i].mac))) {
        logEvent('w', "Unknown device very close: " + macStr(clients[i].mac) + " (" +
                          String(clients[i].rssi) + " dBm)");
        clients[i].alerted = true;
      }
    }
  }
  // LED: blink during deauth burst OR jamming
  if (atkWindow >= DEAUTH_ALERT_MIN || jamOn)
    digitalWrite(LED_PIN, (millis() / 200) % 2);
  else
    LED_OFF;

  // Always check for completed scan, regardless of who started it
  int scanN = WiFi.scanComplete();
  if (scanN >= 0) {
    buildScanJson(scanN);
    WiFi.scanDelete();
    scanRunning = false;
    lastScanDone = millis();
    enableDetector();
  } else if (scanN == WIFI_SCAN_FAILED) {
    WiFi.scanDelete();
    scanRunning = false;
    lastScanDone = millis();
    enableDetector();
  } else if (scanN == WIFI_SCAN_RUNNING) {
    // scan in progress — do not trigger another
  } else if (millis() - lastScanDone > SCAN_INTERVAL && WiFi.softAPgetStationNum() == 0 &&
             millis() > 60000) {
    WiFi.scanNetworks(true, true);
    scanRunning = true;
    lastScanDone = millis();
  }  // don't scan while a dashboard client is connected (scans drop the AP in AP+STA mode)
}

#include "index_html.h"
#include "index_gz.h"
void handleRoot() {
  server.sendHeader("Content-Encoding", "gzip");
  server.sendHeader("Cache-Control", "public, max-age=86400");
  server.send_P(200, "text/html", (const char *)INDEX_GZ, INDEX_GZ_LEN);
}

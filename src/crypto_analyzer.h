#pragma once
// crypto_analyzer.h — 802.11 crypto suite analyzer: RSN/WPA cipher suite
// distribution, AKM suite tracking, PMF stats, cipher downgrade detection.
// Prefix: cra_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

// ─── Cipher suite types ───────────────────────────────────────────────────────
enum CraCipher {
  CRA_CIPHER_UNKNOWN = 0,
  CRA_CIPHER_NONE    = 1,  // open
  CRA_CIPHER_WEP40   = 2,
  CRA_CIPHER_TKIP    = 3,
  CRA_CIPHER_CCMP    = 4,  // AES-128-CCMP
  CRA_CIPHER_WEP104  = 5,
  CRA_CIPHER_BIP     = 6,  // management frame protection
  CRA_CIPHER_GCMP128 = 7,
  CRA_CIPHER_GCMP256 = 8,
  CRA_CIPHER_CCMP256 = 9,
};

static const char* CRA_CIPHER_NAMES[] = {
  "unknown","none","wep40","tkip","ccmp","wep104","bip","gcmp128","gcmp256","ccmp256"
};

// ─── AKM suite types ──────────────────────────────────────────────────────────
enum CraAKM {
  CRA_AKM_UNKNOWN = 0,
  CRA_AKM_8021X   = 1,  // 802.1X (Enterprise)
  CRA_AKM_PSK     = 2,  // WPA2-Personal
  CRA_AKM_FT_8021X = 3,
  CRA_AKM_FT_PSK  = 4,
  CRA_AKM_8021X_SHA256 = 5,
  CRA_AKM_PSK_SHA256 = 6,
  CRA_AKM_TDLS    = 7,
  CRA_AKM_SAE     = 8,  // WPA3
  CRA_AKM_FT_SAE  = 9,
  CRA_AKM_OWE     = 18,
};

// ─── Per-AP crypto profile ────────────────────────────────────────────────────
#define CRA_MAX_APS 64

struct CraAP {
  bool    active;
  uint8_t bssid[6];
  char    ssid[33];
  int8_t  rssi;
  uint8_t channel;

  // Pairwise ciphers (what clients use)
  CraCipher pairwise[4];
  uint8_t   pairwise_count;

  // Group cipher (broadcast)
  CraCipher group;

  // AKM suites
  CraAKM    akm[4];
  uint8_t   akm_count;

  // PMF
  bool    pmf_required;
  bool    pmf_capable;

  // Flags
  bool    downgrade_risk;  // TKIP in pairwise list → downgrade possible
  bool    legacy_wep;
  bool    open;

  // Score: how strong is this AP's crypto?
  uint8_t crypto_score; // 0-100

  uint32_t first_ts;
  uint32_t last_ts;
};

// ─── Global distribution ──────────────────────────────────────────────────────
static struct {
  CraAP   aps[CRA_MAX_APS];
  uint8_t ap_count;

  // Global cipher distribution (count of APs using each)
  uint32_t cipher_dist[10];
  uint32_t akm_dist[20];

  // Totals
  uint32_t open_count;
  uint32_t wep_count;
  uint32_t wpa_tkip_count;
  uint32_t wpa2_ccmp_count;
  uint32_t wpa3_sae_count;
  uint32_t pmf_capable_count;
  uint32_t pmf_required_count;
  uint32_t downgrade_risk_count;
  uint32_t enterprise_count;
} g_cra = {};

// ─── Helpers ─────────────────────────────────────────────────────────────────
static bool cra_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static CraAP* cra_find_or_alloc(const uint8_t *bssid) {
  for (int i = 0; i < CRA_MAX_APS; i++)
    if (g_cra.aps[i].active && cra_mac_eq(g_cra.aps[i].bssid, bssid))
      return &g_cra.aps[i];
  for (int i = 0; i < CRA_MAX_APS; i++)
    if (!g_cra.aps[i].active) {
      CraAP &a = g_cra.aps[i];
      memset(&a, 0, sizeof(a));
      a.active = true;
      memcpy(a.bssid, bssid, 6);
      a.first_ts = millis()/1000;
      g_cra.ap_count++;
      return &a;
    }
  uint32_t oldest = UINT32_MAX; int idx = 0;
  for (int i = 0; i < CRA_MAX_APS; i++)
    if (g_cra.aps[i].last_ts < oldest) { oldest = g_cra.aps[i].last_ts; idx = i; }
  memset(&g_cra.aps[idx], 0, sizeof(g_cra.aps[0]));
  g_cra.aps[idx].active = true;
  memcpy(g_cra.aps[idx].bssid, bssid, 6);
  g_cra.aps[idx].first_ts = millis()/1000;
  return &g_cra.aps[idx];
}

static uint8_t cra_compute_score(const CraAP &ap) {
  if (ap.open)       return 0;
  if (ap.legacy_wep) return 5;

  uint8_t score = 30; // base WPA2-PSK

  // AKM score
  for (int i = 0; i < ap.akm_count; i++) {
    if (ap.akm[i] == CRA_AKM_SAE) score += 25;
    else if (ap.akm[i] == CRA_AKM_8021X) score += 20;
    else if (ap.akm[i] == CRA_AKM_8021X_SHA256) score += 25;
    else if (ap.akm[i] == CRA_AKM_PSK_SHA256) score += 15;
    else if (ap.akm[i] == CRA_AKM_OWE) score += 15;
  }

  // Cipher score
  for (int i = 0; i < ap.pairwise_count; i++) {
    if (ap.pairwise[i] == CRA_CIPHER_TKIP) score -= 15; // downgrade risk
    if (ap.pairwise[i] == CRA_CIPHER_GCMP128 || ap.pairwise[i] == CRA_CIPHER_GCMP256) score += 5;
  }

  // PMF
  if (ap.pmf_required) score += 15;
  else if (ap.pmf_capable) score += 5;

  if (ap.downgrade_risk) score -= 10;

  if (score > 100) score = 100;
  if (score < 0)   score = 0;
  return (uint8_t)score;
}

// ─── Ingest from beacon/probe response ───────────────────────────────────────
void cra_ingest(const uint8_t *bssid, const char *ssid,
                int8_t rssi, uint8_t channel,
                bool open, bool wep,
                const CraCipher *pairwise, uint8_t pc_count,
                CraCipher group,
                const CraAKM *akms, uint8_t akm_count,
                bool pmf_req, bool pmf_cap) {
  CraAP *ap = cra_find_or_alloc(bssid);
  if (!ap) return;

  if (ssid && ssid[0]) strncpy(ap->ssid, ssid, 32);
  ap->rssi    = rssi;
  ap->channel = channel;
  ap->open    = open;
  ap->legacy_wep = wep;
  ap->group   = group;
  ap->pmf_required = pmf_req;
  ap->pmf_capable  = pmf_cap;
  ap->last_ts = millis()/1000;

  ap->pairwise_count = (pc_count < 4) ? pc_count : 4;
  if (pairwise) memcpy(ap->pairwise, pairwise, ap->pairwise_count * sizeof(CraCipher));

  ap->akm_count = (akm_count < 4) ? akm_count : 4;
  if (akms) memcpy(ap->akm, akms, ap->akm_count * sizeof(CraAKM));

  // Downgrade check
  ap->downgrade_risk = false;
  for (int i = 0; i < ap->pairwise_count; i++)
    if (ap->pairwise[i] == CRA_CIPHER_TKIP) { ap->downgrade_risk = true; break; }

  ap->crypto_score = cra_compute_score(*ap);

  // Update global distribution
  if (open)  g_cra.open_count++;
  if (wep)   g_cra.wep_count++;
  if (pmf_cap)  g_cra.pmf_capable_count++;
  if (pmf_req)  g_cra.pmf_required_count++;
  if (ap->downgrade_risk) g_cra.downgrade_risk_count++;
  for (int i = 0; i < akm_count && i < 4; i++) {
    if (akms[i] == CRA_AKM_8021X || akms[i] == CRA_AKM_8021X_SHA256)
      g_cra.enterprise_count++;
    if (akms[i] == CRA_AKM_SAE) g_cra.wpa3_sae_count++;
    if ((uint8_t)akms[i] < 20) g_cra.akm_dist[(uint8_t)akms[i]]++;
  }
  for (int i = 0; i < pc_count && i < 4; i++)
    if ((uint8_t)pairwise[i] < 10) g_cra.cipher_dist[(uint8_t)pairwise[i]]++;
}

// ─── JSON ─────────────────────────────────────────────────────────────────────
static void cra_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String cra_json_all() {
  char mac[18];
  String s = "{\"ap_count\":" + String(g_cra.ap_count);
  s += ",\"open\":" + String(g_cra.open_count);
  s += ",\"wep\":" + String(g_cra.wep_count);
  s += ",\"wpa3\":" + String(g_cra.wpa3_sae_count);
  s += ",\"enterprise\":" + String(g_cra.enterprise_count);
  s += ",\"pmf_capable\":" + String(g_cra.pmf_capable_count);
  s += ",\"pmf_required\":" + String(g_cra.pmf_required_count);
  s += ",\"downgrade_risk\":" + String(g_cra.downgrade_risk_count);
  s += ",\"aps\":[";
  bool first = true;
  for (int i = 0; i < CRA_MAX_APS; i++) {
    CraAP &ap = g_cra.aps[i];
    if (!ap.active) continue;
    if (!first) s += ",";
    first = false;
    cra_mac_str(ap.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(ap.ssid) + "\"";
    s += ",\"ch\":" + String(ap.channel);
    s += ",\"rssi\":" + String(ap.rssi);
    s += ",\"score\":" + String(ap.crypto_score);
    s += ",\"open\":" + String(ap.open?"true":"false");
    s += ",\"wep\":" + String(ap.legacy_wep?"true":"false");
    s += ",\"pmf_req\":" + String(ap.pmf_required?"true":"false");
    s += ",\"pmf_cap\":" + String(ap.pmf_capable?"true":"false");
    s += ",\"downgrade\":" + String(ap.downgrade_risk?"true":"false");
    s += ",\"group\":\"" + String(CRA_CIPHER_NAMES[ap.group<10?(int)ap.group:0]) + "\"";
    s += ",\"akms\":[";
    for (int j = 0; j < ap.akm_count; j++) {
      if (j) s += ",";
      s += String((uint8_t)ap.akm[j]);
    }
    s += "]}";
  }
  s += "]}";
  return s;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void cra_routes() {
  server.on("/api/cra/all", HTTP_GET, []() {
    server.send(200, "application/json", cra_json_all());
  });

  server.on("/api/cra/worst", HTTP_GET, []() {
    // Return 10 worst-scoring APs
    char mac[18];
    String s = "[";
    // Simple sort: collect all, sort by score ascending
    int indices[CRA_MAX_APS];
    int count = 0;
    for (int i = 0; i < CRA_MAX_APS; i++)
      if (g_cra.aps[i].active) indices[count++] = i;
    // Bubble sort by score (worst first)
    for (int i = 0; i < count-1; i++)
      for (int j = i+1; j < count; j++)
        if (g_cra.aps[indices[j]].crypto_score < g_cra.aps[indices[i]].crypto_score) {
          int tmp = indices[i]; indices[i] = indices[j]; indices[j] = tmp;
        }
    bool first = true;
    for (int k = 0; k < count && k < 10; k++) {
      CraAP &ap = g_cra.aps[indices[k]];
      if (!first) s += ",";
      first = false;
      cra_mac_str(ap.bssid, mac);
      s += "{\"bssid\":\"" + String(mac) + "\"";
      s += ",\"ssid\":\"" + String(ap.ssid) + "\"";
      s += ",\"score\":" + String(ap.crypto_score) + "}";
    }
    s += "]";
    server.send(200, "application/json", s);
  });

  server.on("/api/cra/clear", HTTP_POST, []() {
    memset(&g_cra, 0, sizeof(g_cra));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

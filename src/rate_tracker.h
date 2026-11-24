#pragma once
// rate_tracker.h — Per-AP 802.11 data rate and MCS index tracker.
// Tracks observed MCS indices, guard interval, channel width, spatial streams
// and data rate for each AP/station pair seen in promiscuous mode.
// Prefix: rtt_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define RTT_MAX_APS      48
#define RTT_MAX_HISTORY  32

// 802.11 PHY modes
enum RttPhy : uint8_t {
  RTT_PHY_LEGACY  = 0,  // 802.11a/b/g
  RTT_PHY_HT      = 1,  // 802.11n (HT)
  RTT_PHY_VHT     = 2,  // 802.11ac (VHT)
  RTT_PHY_HE      = 3,  // 802.11ax (HE)
  RTT_PHY_EHT     = 4,  // 802.11be (EHT)
};

// Guard interval
enum RttGI : uint8_t {
  RTT_GI_LONG    = 0,  // 800ns
  RTT_GI_SHORT   = 1,  // 400ns
  RTT_GI_HE0_8   = 2,  // 0.8μs HE
  RTT_GI_HE1_6   = 3,  // 1.6μs HE
  RTT_GI_HE3_2   = 4,  // 3.2μs HE
};

// Per-frame rate sample
struct RttSample {
  uint32_t ts;
  uint16_t rate_kbps;    // in kbps
  uint8_t  mcs;          // MCS index
  uint8_t  nss;          // spatial streams
  uint8_t  bw;           // channel width in MHz
  RttPhy   phy;
  RttGI    gi;
  int8_t   rssi;
};

// Per-AP rate statistics
struct RttAP {
  bool     active;
  uint8_t  bssid[6];
  char     ssid[33];
  uint8_t  ch;

  // Rate history (ring buffer)
  RttSample hist[RTT_MAX_HISTORY];
  uint8_t   hist_head;
  uint32_t  sample_count;

  // Peak observed
  uint16_t  peak_rate_kbps;
  uint8_t   peak_mcs;
  uint8_t   peak_nss;
  uint8_t   peak_bw;
  RttPhy    peak_phy;

  // Averages
  uint32_t  avg_rate_kbps;   // moving average

  // Capability flags (inferred from observed frames)
  bool      seen_ht;
  bool      seen_vht;
  bool      seen_he;
  bool      seen_short_gi;
  bool      seen_4ss;         // 4 spatial streams seen

  // Stream count histogram
  uint16_t  nss_count[5];     // [0]=unknown, [1]=1SS, [2]=2SS, [3]=3SS, [4]=4SS

  // BW histogram
  uint16_t  bw20_count;
  uint16_t  bw40_count;
  uint16_t  bw80_count;
  uint16_t  bw160_count;

  uint32_t  first_ts;
  uint32_t  last_ts;
};

static struct {
  RttAP    aps[RTT_MAX_APS];
  uint8_t  ap_count;
  uint32_t total_samples;
  uint32_t total_frames_ht;
  uint32_t total_frames_vht;
  uint32_t total_frames_he;
  uint32_t total_legacy;
  uint16_t global_peak_kbps;
} g_rtt = {};

static bool rtt_mac_eq(const uint8_t *a, const uint8_t *b) { return memcmp(a,b,6)==0; }

static RttAP* rtt_find(const uint8_t *bssid) {
  for (int i = 0; i < RTT_MAX_APS; i++)
    if (g_rtt.aps[i].active && rtt_mac_eq(g_rtt.aps[i].bssid, bssid))
      return &g_rtt.aps[i];
  return nullptr;
}

static RttAP* rtt_alloc(const uint8_t *bssid) {
  for (int i = 0; i < RTT_MAX_APS; i++) {
    if (!g_rtt.aps[i].active) {
      memset(&g_rtt.aps[i], 0, sizeof(RttAP));
      g_rtt.aps[i].active = true;
      memcpy(g_rtt.aps[i].bssid, bssid, 6);
      g_rtt.ap_count++;
      return &g_rtt.aps[i];
    }
  }
  // LRU eviction
  uint32_t oldest = UINT32_MAX; int idx = 0;
  for (int i = 0; i < RTT_MAX_APS; i++)
    if (g_rtt.aps[i].last_ts < oldest) { oldest = g_rtt.aps[i].last_ts; idx = i; }
  memset(&g_rtt.aps[idx], 0, sizeof(RttAP));
  g_rtt.aps[idx].active = true;
  memcpy(g_rtt.aps[idx].bssid, bssid, 6);
  return &g_rtt.aps[idx];
}

// Compute theoretical 802.11n/ac/ax data rate in kbps
// Very simplified: single-stream 20MHz lookup
static uint16_t rtt_compute_rate_kbps(RttPhy phy, uint8_t mcs, uint8_t nss, uint8_t bw, RttGI gi) {
  if (nss < 1) nss = 1;
  if (nss > 4) nss = 4;

  // HT (802.11n) base rates in Mbps for 1SS 20MHz long GI
  // MCS 0-7
  static const uint16_t ht_base_20[8] = {6500, 13000, 19500, 26000, 39000, 52000, 58500, 65000};
  static const uint16_t ht_base_40[8] = {13500, 27000, 40500, 54000, 81000, 108000, 121500, 135000};

  // VHT (802.11ac) 1SS 20MHz base rates in kbps
  static const uint16_t vht_base_20[10] = {6500, 13000, 19500, 26000, 39000, 52000, 58500, 65000, 78000, 86700};
  static const uint16_t vht_base_40[10] = {13500, 27000, 40500, 54000, 81000, 108000, 121500, 135000, 162000, 180000};
  static const uint16_t vht_base_80[10] = {29300, 58500, 87800, 117000, 175500, 234000, 263300, 292500, 351000, 390000};
  static const uint16_t vht_base_160[10] = {58500, 117000, 175500, 234000, 351000, 468000, 526500, 585000, 702000, 780000};

  uint16_t base = 0;
  if (phy == RTT_PHY_HT) {
    if (mcs > 7) mcs = 7;
    base = (bw >= 40) ? ht_base_40[mcs] : ht_base_20[mcs];
  } else if (phy == RTT_PHY_VHT || phy == RTT_PHY_HE) {
    if (mcs > 9) mcs = 9;
    if (bw >= 160)      base = vht_base_160[mcs];
    else if (bw >= 80)  base = vht_base_80[mcs];
    else if (bw >= 40)  base = vht_base_40[mcs];
    else                base = vht_base_20[mcs];
    if (phy == RTT_PHY_HE) base = (uint16_t)(base * 12 / 10); // HE ~20% improvement
  } else {
    // Legacy fallback
    return 0;
  }

  uint16_t rate = base * nss;
  if (gi == RTT_GI_SHORT || gi == RTT_GI_HE0_8) rate = (uint16_t)(rate * 11 / 10);
  return rate;
}

void rtt_ingest(const uint8_t *bssid, const char *ssid, uint8_t ch,
                int8_t rssi, RttPhy phy, uint8_t mcs, uint8_t nss,
                uint8_t bw, RttGI gi) {
  if (!bssid) return;

  RttAP *ap = rtt_find(bssid);
  if (!ap) ap = rtt_alloc(bssid);
  if (!ap) return;

  if (ssid && ssid[0]) strncpy(ap->ssid, ssid, 32);
  ap->ch = ch;

  uint32_t now = millis()/1000;
  if (!ap->first_ts) ap->first_ts = now;
  ap->last_ts = now;

  uint16_t rate = rtt_compute_rate_kbps(phy, mcs, nss, bw, gi);

  // Update sample history
  RttSample &s = ap->hist[ap->hist_head % RTT_MAX_HISTORY];
  s.ts         = now;
  s.rate_kbps  = rate;
  s.mcs        = mcs;
  s.nss        = nss;
  s.bw         = bw;
  s.phy        = phy;
  s.gi         = gi;
  s.rssi       = rssi;
  ap->hist_head = (ap->hist_head + 1) % RTT_MAX_HISTORY;
  ap->sample_count++;

  // Update peak
  if (rate > ap->peak_rate_kbps) {
    ap->peak_rate_kbps = rate;
    ap->peak_mcs       = mcs;
    ap->peak_nss       = nss;
    ap->peak_bw        = bw;
    ap->peak_phy       = phy;
  }

  // Moving average
  ap->avg_rate_kbps = (ap->avg_rate_kbps * 9 + rate) / 10;

  // Capability tracking
  if (phy == RTT_PHY_HT)  { ap->seen_ht  = true; g_rtt.total_frames_ht++; }
  if (phy == RTT_PHY_VHT) { ap->seen_vht = true; g_rtt.total_frames_vht++; }
  if (phy == RTT_PHY_HE)  { ap->seen_he  = true; g_rtt.total_frames_he++; }
  if (phy == RTT_PHY_LEGACY) g_rtt.total_legacy++;
  if (gi == RTT_GI_SHORT) ap->seen_short_gi = true;
  if (nss >= 4) ap->seen_4ss = true;

  // NSS histogram
  if (nss <= 4) ap->nss_count[nss]++;
  else          ap->nss_count[0]++;

  // BW histogram
  if      (bw >= 160) ap->bw160_count++;
  else if (bw >= 80)  ap->bw80_count++;
  else if (bw >= 40)  ap->bw40_count++;
  else                ap->bw20_count++;

  g_rtt.total_samples++;
  if (rate > g_rtt.global_peak_kbps) g_rtt.global_peak_kbps = rate;
}

static const char* rtt_phy_name(RttPhy p) {
  switch (p) {
    case RTT_PHY_LEGACY: return "Legacy";
    case RTT_PHY_HT:     return "HT (802.11n)";
    case RTT_PHY_VHT:    return "VHT (802.11ac)";
    case RTT_PHY_HE:     return "HE (802.11ax)";
    case RTT_PHY_EHT:    return "EHT (802.11be)";
  }
  return "Unknown";
}

static void rtt_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String rtt_json_all() {
  String s = "{";
  s += "\"ap_count\":"   + String(g_rtt.ap_count);
  s += ",\"total\":"     + String(g_rtt.total_samples);
  s += ",\"ht_frames\":" + String(g_rtt.total_frames_ht);
  s += ",\"vht_frames\":" + String(g_rtt.total_frames_vht);
  s += ",\"he_frames\":" + String(g_rtt.total_frames_he);
  s += ",\"legacy\":"    + String(g_rtt.total_legacy);
  s += ",\"peak_kbps\":" + String(g_rtt.global_peak_kbps);
  s += ",\"aps\":[";
  char mac[18]; bool first = true;
  for (int i = 0; i < RTT_MAX_APS; i++) {
    RttAP &ap = g_rtt.aps[i];
    if (!ap.active) continue;
    if (!first) s += ","; first = false;
    rtt_mac_str(ap.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(ap.ssid) + "\"";
    s += ",\"ch\":" + String(ap.ch);
    s += ",\"samples\":" + String(ap.sample_count);
    s += ",\"avg_kbps\":" + String(ap.avg_rate_kbps);
    s += ",\"peak_kbps\":" + String(ap.peak_rate_kbps);
    s += ",\"peak_mcs\":" + String(ap.peak_mcs);
    s += ",\"peak_nss\":" + String(ap.peak_nss);
    s += ",\"peak_bw\":" + String(ap.peak_bw);
    s += ",\"peak_phy\":\"" + String(rtt_phy_name(ap.peak_phy)) + "\"";
    s += ",\"ht\":" + String(ap.seen_ht ? "true" : "false");
    s += ",\"vht\":" + String(ap.seen_vht ? "true" : "false");
    s += ",\"he\":" + String(ap.seen_he ? "true" : "false");
    s += ",\"bw20\":" + String(ap.bw20_count);
    s += ",\"bw40\":" + String(ap.bw40_count);
    s += ",\"bw80\":" + String(ap.bw80_count);
    s += ",\"bw160\":" + String(ap.bw160_count);
    s += ",\"nss1\":" + String(ap.nss_count[1]);
    s += ",\"nss2\":" + String(ap.nss_count[2]);
    s += ",\"nss3\":" + String(ap.nss_count[3]);
    s += ",\"nss4\":" + String(ap.nss_count[4]);
    s += ",\"hist\":[";
    for (int j = 0; j < RTT_MAX_HISTORY; j++) {
      uint8_t idx = (ap.hist_head + j) % RTT_MAX_HISTORY;
      if (!ap.hist[idx].ts) { s += "0"; } else {
        s += String(ap.hist[idx].rate_kbps);
      }
      if (j < RTT_MAX_HISTORY - 1) s += ",";
    }
    s += "]}";
  }
  s += "]}";
  return s;
}

inline void rtt_routes() {
  server.on("/api/rtt/all", HTTP_GET, []() {
    server.send(200, "application/json", rtt_json_all());
  });
  server.on("/api/rtt/clear", HTTP_POST, []() {
    memset(&g_rtt, 0, sizeof(g_rtt));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

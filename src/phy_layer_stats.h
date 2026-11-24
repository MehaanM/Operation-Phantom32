#pragma once
// phy_layer_stats.h — 802.11 PHY layer statistics tracker.
// Tracks modulation type (DSSS/OFDM/HT/VHT/HE), guard interval, bandwidth, and MCS per AP.
// Prefix: phy_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define PHY_MAX_APS    64
#define PHY_HIST_LEN   30

enum PhyModulation : uint8_t {
  PHY_MOD_UNKNOWN = 0,
  PHY_MOD_DSSS    = 1,  // 802.11b (1,2,5.5,11 Mbps)
  PHY_MOD_OFDM    = 2,  // 802.11a/g legacy
  PHY_MOD_HT      = 3,  // 802.11n (MCS 0-31)
  PHY_MOD_VHT     = 4,  // 802.11ac (MCS 0-9, NSS 1-8)
  PHY_MOD_HE      = 5,  // 802.11ax (MCS 0-11)
  PHY_MOD_EHT     = 6,  // 802.11be (future)
};

enum PhyBandwidth : uint8_t {
  PHY_BW_20   = 0,
  PHY_BW_40   = 1,
  PHY_BW_80   = 2,
  PHY_BW_160  = 3,
};

struct PhyModCount {
  uint32_t dsss;
  uint32_t ofdm;
  uint32_t ht;
  uint32_t vht;
  uint32_t he;
  uint32_t unknown;
};

struct PhyMcsDist {
  uint16_t mcs_counts[12];  // MCS 0-11 (HT: 0-7, VHT/HE: 0-9/0-11)
};

struct PhyAP {
  bool    active;
  uint8_t bssid[6];
  char    ssid[33];
  uint8_t ch;
  int8_t  rssi;
  uint32_t first_ts;
  uint32_t last_ts;
  uint32_t frame_count;

  // Modulation distribution
  PhyModCount mods;

  // Current and last seen PHY attributes
  PhyModulation last_mod;
  PhyBandwidth  last_bw;
  uint8_t       last_mcs;
  uint8_t       last_nss;   // number of spatial streams
  bool          last_sgi;   // short guard interval

  // Bandwidth distribution
  uint32_t bw_20;
  uint32_t bw_40;
  uint32_t bw_80;
  uint32_t bw_160;

  // MCS distribution (HT/VHT/HE)
  PhyMcsDist mcs_dist;

  // Rolling MCS history (last N frames)
  uint8_t  mcs_hist[PHY_HIST_LEN];
  uint8_t  mcs_hist_head;

  // Computed dominant modulation
  PhyModulation dominant_mod;
  uint8_t  mode_pct;  // percentage of dominant mode
};

static struct {
  PhyAP   aps[PHY_MAX_APS];
  uint8_t count;
} g_phy = {};

static bool phy_mac_eq(const uint8_t *a, const uint8_t *b) { return memcmp(a,b,6)==0; }

static PhyAP* phy_find(const uint8_t *bssid) {
  for (int i = 0; i < PHY_MAX_APS; i++)
    if (g_phy.aps[i].active && phy_mac_eq(g_phy.aps[i].bssid, bssid))
      return &g_phy.aps[i];
  return nullptr;
}

static PhyAP* phy_alloc(const uint8_t *bssid) {
  for (int i = 0; i < PHY_MAX_APS; i++) {
    if (!g_phy.aps[i].active) {
      PhyAP &a = g_phy.aps[i];
      memset(&a, 0, sizeof(a));
      a.active = true;
      memcpy(a.bssid, bssid, 6);
      a.first_ts = millis()/1000;
      g_phy.count++;
      return &a;
    }
  }
  uint32_t oldest = UINT32_MAX; int idx = 0;
  for (int i = 0; i < PHY_MAX_APS; i++)
    if (g_phy.aps[i].last_ts < oldest) { oldest = g_phy.aps[i].last_ts; idx = i; }
  memset(&g_phy.aps[idx], 0, sizeof(g_phy.aps[0]));
  g_phy.aps[idx].active = true;
  memcpy(g_phy.aps[idx].bssid, bssid, 6);
  g_phy.aps[idx].first_ts = millis()/1000;
  return &g_phy.aps[idx];
}

static void phy_update_dominant(PhyAP &a) {
  uint32_t total = a.mods.dsss + a.mods.ofdm + a.mods.ht + a.mods.vht + a.mods.he;
  if (!total) return;

  struct { uint32_t cnt; PhyModulation mod; } best[] = {
    {a.mods.he,  PHY_MOD_HE},
    {a.mods.vht, PHY_MOD_VHT},
    {a.mods.ht,  PHY_MOD_HT},
    {a.mods.ofdm,PHY_MOD_OFDM},
    {a.mods.dsss,PHY_MOD_DSSS},
  };
  uint32_t mx = 0; PhyModulation dm = PHY_MOD_UNKNOWN;
  for (auto &b : best) if (b.cnt > mx) { mx = b.cnt; dm = b.mod; }
  a.dominant_mod = dm;
  a.mode_pct = (uint8_t)((mx * 100) / total);
}

void phy_ingest(const uint8_t *bssid, const char *ssid, uint8_t ch, int8_t rssi,
                PhyModulation mod, PhyBandwidth bw, uint8_t mcs, uint8_t nss, bool sgi) {
  PhyAP *a = phy_find(bssid);
  if (!a) a = phy_alloc(bssid);
  if (!a) return;

  if (ssid && ssid[0]) strncpy(a->ssid, ssid, 32);
  a->ch = ch;
  if (rssi < 0) a->rssi = rssi;
  a->last_ts = millis()/1000;
  a->frame_count++;
  a->last_mod = mod;
  a->last_bw  = bw;
  a->last_mcs = mcs;
  a->last_nss = nss;
  a->last_sgi = sgi;

  switch(mod) {
    case PHY_MOD_DSSS: a->mods.dsss++; break;
    case PHY_MOD_OFDM: a->mods.ofdm++; break;
    case PHY_MOD_HT:   a->mods.ht++;   break;
    case PHY_MOD_VHT:  a->mods.vht++;  break;
    case PHY_MOD_HE:   a->mods.he++;   break;
    default:           a->mods.unknown++; break;
  }

  switch(bw) {
    case PHY_BW_20:  a->bw_20++;  break;
    case PHY_BW_40:  a->bw_40++;  break;
    case PHY_BW_80:  a->bw_80++;  break;
    case PHY_BW_160: a->bw_160++; break;
  }

  if ((mod == PHY_MOD_HT || mod == PHY_MOD_VHT || mod == PHY_MOD_HE) && mcs < 12) {
    a->mcs_dist.mcs_counts[mcs]++;
  }
  a->mcs_hist[a->mcs_hist_head % PHY_HIST_LEN] = mcs;
  a->mcs_hist_head = (a->mcs_hist_head + 1) % PHY_HIST_LEN;

  if (a->frame_count % 10 == 0) phy_update_dominant(*a);
}

static const char* phy_mod_str(PhyModulation m) {
  switch(m) {
    case PHY_MOD_DSSS: return "DSSS(b)";
    case PHY_MOD_OFDM: return "OFDM(g)";
    case PHY_MOD_HT:   return "HT(n)";
    case PHY_MOD_VHT:  return "VHT(ac)";
    case PHY_MOD_HE:   return "HE(ax)";
    case PHY_MOD_EHT:  return "EHT(be)";
    default:           return "?";
  }
}

static const char* phy_bw_str(PhyBandwidth b) {
  switch(b) { case PHY_BW_40: return "40M"; case PHY_BW_80: return "80M";
              case PHY_BW_160: return "160M"; default: return "20M"; }
}

static void phy_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String phy_json_all() {
  String s = "{\"count\":" + String(g_phy.count);
  s += ",\"aps\":[";
  char mac[18]; bool first = true;
  for (int i = 0; i < PHY_MAX_APS; i++) {
    PhyAP &a = g_phy.aps[i];
    if (!a.active) continue;
    if (!first) s += ","; first = false;
    phy_mac_str(a.bssid, mac);
    s += "{\"bssid\":\"" + String(mac) + "\"";
    s += ",\"ssid\":\"" + String(a.ssid) + "\"";
    s += ",\"ch\":" + String(a.ch);
    s += ",\"rssi\":" + String(a.rssi);
    s += ",\"frames\":" + String(a.frame_count);
    s += ",\"dom_mod\":\"" + String(phy_mod_str(a.dominant_mod)) + "\"";
    s += ",\"dom_pct\":" + String(a.mode_pct);
    s += ",\"last_mod\":\"" + String(phy_mod_str(a.last_mod)) + "\"";
    s += ",\"last_bw\":\"" + String(phy_bw_str(a.last_bw)) + "\"";
    s += ",\"last_mcs\":" + String(a.last_mcs);
    s += ",\"last_nss\":" + String(a.last_nss);
    s += ",\"last_sgi\":" + String(a.last_sgi?"true":"false");
    s += ",\"dsss\":" + String(a.mods.dsss);
    s += ",\"ofdm\":" + String(a.mods.ofdm);
    s += ",\"ht\":" + String(a.mods.ht);
    s += ",\"vht\":" + String(a.mods.vht);
    s += ",\"he\":" + String(a.mods.he);
    s += ",\"bw\":[" + String(a.bw_20) + "," + String(a.bw_40)
       + "," + String(a.bw_80) + "," + String(a.bw_160) + "]";
    s += ",\"mcs_dist\":[";
    for (int j = 0; j < 12; j++) {
      if (j) s += ",";
      s += String(a.mcs_dist.mcs_counts[j]);
    }
    s += "]}";
  }
  s += "]}";
  return s;
}

inline void phy_routes() {
  server.on("/api/phy/all", HTTP_GET, []() {
    server.send(200, "application/json", phy_json_all());
  });
  server.on("/api/phy/clear", HTTP_POST, []() {
    memset(&g_phy, 0, sizeof(g_phy));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

#pragma once
// beacon_analyzer.h — Deep beacon frame field analysis and anomaly detection.
// Parses every IE field, tracks changes, detects malformed/anomalous beacons.
// Prefix: bca_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define BCA_MAX_APS     48
#define BCA_SSID_LEN    33
#define BCA_ALERT_MAX   32
#define BCA_IE_MAX      32   // max IEs to store per AP

// ─── IE summary ───────────────────────────────────────────────────────────────
struct BcaIE {
  uint8_t  tag;
  uint8_t  len;
  uint8_t  data[16]; // first 16 bytes
};

// ─── Anomaly flags ────────────────────────────────────────────────────────────
struct BcaAnomalies {
  bool malformed_ie;      // IE length overflows frame
  bool ssid_overflow;     // SSID IE length > 32
  bool ssid_nonprint;     // non-printable chars in SSID
  bool ssid_changed;      // SSID changed between beacons
  bool channel_mismatch;  // DS IE channel != radiotap channel
  bool tim_malformed;     // TIM IE too short
  bool ht_malformed;      // HT caps IE wrong length
  bool rsn_malformed;     // RSN IE malformed
  bool beacon_interval_low; // interval < 10 TU (suspicious)
  bool beacon_interval_high; // interval > 1000 TU
  bool short_preamble_set;
  bool spurious_ies;      // duplicate IEs
  bool phy_mismatch;      // advertised HT/VHT but rate fields inconsistent
};

// ─── AP record ────────────────────────────────────────────────────────────────
struct BcaAP {
  bool    active;
  uint8_t bssid[6];
  char    ssid[BCA_SSID_LEN];
  char    prev_ssid[BCA_SSID_LEN]; // for change detection
  uint8_t channel;
  uint8_t ds_channel;    // from DS Parameter Set IE
  int8_t  rssi;
  uint16_t beacon_interval;  // in TU (1 TU = 1024 us)
  uint16_t capability;
  uint8_t  dtim_period;
  uint8_t  supported_rates;  // count
  bool     ht;               // 802.11n HT caps IE present
  bool     vht;              // 802.11ac VHT caps IE
  bool     he;               // 802.11ax HE caps IE
  bool     wps;
  bool     rsn;
  bool     wpa;

  // HT capability fields
  uint16_t ht_cap_info;
  uint8_t  rx_ss;         // spatial streams
  uint8_t  max_amsdu;

  // VHT capability
  uint8_t  vht_chan_width;

  // RSN
  uint8_t  group_cipher;
  uint8_t  pair_cipher;
  uint8_t  akm;
  uint16_t rsn_caps;

  // IE store
  BcaIE   ies[BCA_IE_MAX];
  uint8_t ie_count;

  BcaAnomalies anomalies;
  uint8_t  anomaly_score;   // 0-100

  uint32_t beacon_count;
  uint32_t first_seen;
  uint32_t last_seen;
};

// ─── Alert ────────────────────────────────────────────────────────────────────
struct BcaAlert {
  bool     used;
  uint32_t ts;
  uint8_t  bssid[6];
  char     msg[64];
  uint8_t  score;
};

// ─── State ────────────────────────────────────────────────────────────────────
static struct {
  BcaAP    aps[BCA_MAX_APS];
  uint8_t  count;
  BcaAlert alerts[BCA_ALERT_MAX];
  uint8_t  alert_head;
  uint32_t total_beacons;
  uint32_t total_anomalies;
} g_bca = {};

// ─── Helpers ─────────────────────────────────────────────────────────────────
static bool bca_mac_eq(const uint8_t *a, const uint8_t *b) { return memcmp(a,b,6)==0; }

static BcaAP* bca_find(const uint8_t *bssid) {
  for (int i = 0; i < BCA_MAX_APS; i++) {
    if (g_bca.aps[i].active && bca_mac_eq(g_bca.aps[i].bssid, bssid))
      return &g_bca.aps[i];
  }
  return nullptr;
}

static BcaAP* bca_alloc(const uint8_t *bssid) {
  for (int i = 0; i < BCA_MAX_APS; i++) {
    if (!g_bca.aps[i].active) {
      BcaAP &ap = g_bca.aps[i];
      memset(&ap, 0, sizeof(ap));
      memcpy(ap.bssid, bssid, 6);
      ap.active = true;
      ap.first_seen = millis() / 1000;
      g_bca.count++;
      return &ap;
    }
  }
  return nullptr;
}

// ─── Log alert ────────────────────────────────────────────────────────────────
static void bca_log(const uint8_t *bssid, uint8_t score, const char *msg) {
  BcaAlert &a = g_bca.alerts[g_bca.alert_head % BCA_ALERT_MAX];
  a.used = true;
  a.ts = millis() / 1000;
  if (bssid) memcpy(a.bssid, bssid, 6);
  a.score = score;
  strncpy(a.msg, msg, 63);
  g_bca.alert_head++;
  g_bca.total_anomalies++;
}

// ─── IE parser ───────────────────────────────────────────────────────────────
static void bca_parse_ies(BcaAP *ap, const uint8_t *body, int body_len) {
  ap->ie_count = 0;
  bool seen_tags[256] = {};

  int pos = 0;
  while (pos + 1 < body_len) {
    uint8_t tag = body[pos];
    uint8_t len = body[pos + 1];

    if (pos + 2 + len > body_len) {
      ap->anomalies.malformed_ie = true;
      break;
    }

    const uint8_t *data = body + pos + 2;

    // Store IE
    if (ap->ie_count < BCA_IE_MAX) {
      BcaIE &ie = ap->ies[ap->ie_count++];
      ie.tag = tag;
      ie.len = len;
      memcpy(ie.data, data, min((int)len, 16));
    }

    // Duplicate IE check
    if (seen_tags[tag]) ap->anomalies.spurious_ies = true;
    seen_tags[tag] = true;

    switch (tag) {
      case 0: // SSID
        if (len > 32) { ap->anomalies.ssid_overflow = true; break; }
        memset(ap->ssid, 0, BCA_SSID_LEN);
        memcpy(ap->ssid, data, len);
        for (int i = 0; i < len; i++) {
          if ((uint8_t)ap->ssid[i] < 0x20 || (uint8_t)ap->ssid[i] > 0x7E)
            ap->anomalies.ssid_nonprint = true;
        }
        break;

      case 1: // Supported Rates
        ap->supported_rates = len;
        break;

      case 3: // DS Parameter Set
        if (len >= 1) ap->ds_channel = data[0];
        break;

      case 5: // TIM
        if (len < 4) ap->anomalies.tim_malformed = true;
        else ap->dtim_period = data[1];
        break;

      case 45: // HT Capabilities
        if (len < 26) { ap->anomalies.ht_malformed = true; break; }
        ap->ht = true;
        ap->ht_cap_info = (data[0] | (data[1] << 8));
        ap->rx_ss = ((data[7] >> 2) & 0x3F) ? 2 : 1; // simplified
        break;

      case 48: // RSN
        ap->rsn = true;
        if (len < 4) { ap->anomalies.rsn_malformed = true; break; }
        {
          int rpos = 0;
          rpos += 2; // version
          if (rpos + 4 <= len) { ap->group_cipher = data[rpos+3]; rpos += 4; }
          if (rpos + 2 <= len) {
            uint16_t pc = (data[rpos]|(data[rpos+1]<<8));
            rpos += 2;
            if (pc > 0 && rpos + 4 <= len) { ap->pair_cipher = data[rpos+3]; rpos += 4; }
          }
          if (rpos + 2 <= len) {
            uint16_t akmc = (data[rpos]|(data[rpos+1]<<8));
            rpos += 2;
            if (akmc > 0 && rpos + 4 <= len) { ap->akm = data[rpos+3]; }
          }
          if (rpos + 2 <= len) ap->rsn_caps = data[rpos]|(data[rpos+1]<<8);
        }
        break;

      case 191: // VHT Capabilities
        ap->vht = true;
        ap->vht_chan_width = len >= 1 ? (data[0] >> 2) & 0x03 : 0;
        break;

      case 255: // HE Capabilities (Extension ID in next byte)
        if (len >= 1 && data[0] == 35) ap->he = true;
        break;

      case 221: // Vendor Specific
        if (len >= 4) {
          if (data[0]==0x00 && data[1]==0x50 && data[2]==0xF2 && data[3]==0x01) ap->wpa = true;
          if (data[0]==0x00 && data[1]==0x50 && data[2]==0xF2 && data[3]==0x04) ap->wps = true;
        }
        break;
    }

    pos += 2 + len;
  }
}

// ─── Score anomalies ─────────────────────────────────────────────────────────
static void bca_score(BcaAP *ap) {
  uint8_t score = 0;
  BcaAnomalies &a = ap->anomalies;

  if (a.malformed_ie)       score += 25;
  if (a.ssid_overflow)      score += 20;
  if (a.ssid_nonprint)      score += 15;
  if (a.ssid_changed)       score += 10;
  if (a.channel_mismatch)   score += 20;
  if (a.tim_malformed)      score += 10;
  if (a.ht_malformed)       score += 10;
  if (a.rsn_malformed)      score += 15;
  if (a.beacon_interval_low) score += 20;
  if (a.beacon_interval_high) score += 5;
  if (a.spurious_ies)       score += 5;

  ap->anomaly_score = min(score, (uint8_t)100);

  if (score >= 25) {
    char msg[64];
    snprintf(msg, sizeof(msg), "AP %s anomaly score %d", ap->ssid, score);
    bca_log(ap->bssid, score, msg);
  }
}

// ─── Ingest beacon ───────────────────────────────────────────────────────────
void bca_ingest(const uint8_t *raw, int len, const uint8_t *bssid,
                int8_t rssi, uint8_t channel) {
  if (len < 36) return; // too short for beacon

  // Beacon body starts at offset 8 (radiotap) + 24 (mach) + 12 (fixed params)
  // For raw 802.11 without radiotap: offset 24 + 12
  // We work with post-MAC header body
  const int FIXED = 12; // timestamp(8) + interval(2) + caps(2)
  int hdr_len = 24; // MAC header

  if (hdr_len + FIXED >= len) return;

  uint16_t interval = raw[hdr_len]   | (raw[hdr_len+1]<<8);
  uint16_t caps     = raw[hdr_len+8] | (raw[hdr_len+9]<<8);

  BcaAP *ap = bca_find(bssid);
  if (!ap) ap = bca_alloc(bssid);
  if (!ap) return;

  g_bca.total_beacons++;
  ap->beacon_count++;
  ap->last_seen = millis() / 1000;
  ap->rssi = rssi;
  ap->channel = channel;
  ap->capability = caps;
  ap->beacon_interval = interval;

  // Anomaly: beacon interval
  if (interval < 10)   ap->anomalies.beacon_interval_low = true;
  if (interval > 1000) ap->anomalies.beacon_interval_high = true;

  // Save previous SSID for change detection
  memcpy(ap->prev_ssid, ap->ssid, BCA_SSID_LEN);

  // Parse IEs
  int ie_offset = hdr_len + FIXED;
  bca_parse_ies(ap, raw + ie_offset, len - ie_offset);

  // Check SSID change
  if (ap->beacon_count > 1 && strcmp(ap->ssid, ap->prev_ssid) != 0 &&
      ap->prev_ssid[0] != '\0') {
    ap->anomalies.ssid_changed = true;
  }

  // Channel mismatch
  if (ap->ds_channel != 0 && ap->ds_channel != channel)
    ap->anomalies.channel_mismatch = true;

  // Score every 10 beacons
  if (ap->beacon_count % 10 == 1) bca_score(ap);
}

// ─── JSON ─────────────────────────────────────────────────────────────────────
static void bca_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String bca_json_ap(const BcaAP &ap) {
  char mac[18];
  bca_mac_str(ap.bssid, mac);
  String s = "{\"bssid\":\"" + String(mac) + "\"";
  s += ",\"ssid\":\"" + String(ap.ssid) + "\"";
  s += ",\"ch\":" + String(ap.channel);
  s += ",\"rssi\":" + String(ap.rssi);
  s += ",\"interval\":" + String(ap.beacon_interval);
  s += ",\"beacons\":" + String(ap.beacon_count);
  s += ",\"anomaly_score\":" + String(ap.anomaly_score);
  s += ",\"ht\":" + String(ap.ht?"true":"false");
  s += ",\"vht\":" + String(ap.vht?"true":"false");
  s += ",\"he\":" + String(ap.he?"true":"false");
  s += ",\"rsn\":" + String(ap.rsn?"true":"false");
  s += ",\"wps\":" + String(ap.wps?"true":"false");
  const BcaAnomalies &a = ap.anomalies;
  s += ",\"anom\":{";
  s += "\"malformed_ie\":" + String(a.malformed_ie?"true":"false");
  s += ",\"ssid_overflow\":" + String(a.ssid_overflow?"true":"false");
  s += ",\"ssid_nonprint\":" + String(a.ssid_nonprint?"true":"false");
  s += ",\"ssid_changed\":" + String(a.ssid_changed?"true":"false");
  s += ",\"ch_mismatch\":" + String(a.channel_mismatch?"true":"false");
  s += ",\"spurious_ies\":" + String(a.spurious_ies?"true":"false");
  s += "}}";
  return s;
}

static String bca_json_all() {
  String s = "{\"count\":" + String(g_bca.count);
  s += ",\"total_beacons\":" + String(g_bca.total_beacons);
  s += ",\"total_anomalies\":" + String(g_bca.total_anomalies);
  s += ",\"aps\":[";
  bool first = true;
  for (int i = 0; i < BCA_MAX_APS; i++) {
    if (!g_bca.aps[i].active) continue;
    if (!first) s += ",";
    first = false;
    s += bca_json_ap(g_bca.aps[i]);
  }
  s += "]}";
  return s;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void bca_routes() {
  server.on("/api/bca/all", HTTP_GET, []() {
    server.send(200, "application/json", bca_json_all());
  });

  server.on("/api/bca/alerts", HTTP_GET, []() {
    String s = "[";
    bool first = true;
    char mac[18];
    for (int i = 0; i < BCA_ALERT_MAX; i++) {
      int idx = (g_bca.alert_head - BCA_ALERT_MAX + i + BCA_ALERT_MAX) % BCA_ALERT_MAX;
      BcaAlert &a = g_bca.alerts[idx];
      if (!a.used) continue;
      if (!first) s += ",";
      first = false;
      bca_mac_str(a.bssid, mac);
      s += "{\"ts\":" + String(a.ts);
      s += ",\"bssid\":\"" + String(mac) + "\"";
      s += ",\"score\":" + String(a.score);
      s += ",\"msg\":\"" + String(a.msg) + "\"";
      s += "}";
    }
    s += "]";
    server.send(200, "application/json", s);
  });

  server.on("/api/bca/clear", HTTP_POST, []() {
    memset(&g_bca, 0, sizeof(g_bca));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

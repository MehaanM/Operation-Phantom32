#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <string.h>
#include <stdint.h>

// ── WiFi Protocols Parser ─────────────────────────────────────────────────────
//
// Parses 802.11 information elements, RSN suites, cipher suites,
// vendor IEs, and frame body fields. Provides human-readable output
// for all major WiFi protocol constructs.

// ── IE parse result ───────────────────────────────────────────────────────────
struct WpIe {
  uint8_t  tag;
  uint8_t  len;
  const uint8_t* data;
};

// ── Cipher suite OUIs ─────────────────────────────────────────────────────────
#define WP_OUI_DOT11  0x000FAC   // IEEE 802.11 OUI
#define WP_OUI_MS     0x0050F2   // Microsoft OUI (WPA1)
#define WP_OUI_ESPNOW 0x18FE34   // Espressif ESP-NOW

// ── RSN cipher suite types (00:0F:AC) ─────────────────────────────────────────
#define WP_CIPHER_USE_GROUP    0
#define WP_CIPHER_WEP40        1
#define WP_CIPHER_TKIP         2
#define WP_CIPHER_CCMP_128     4
#define WP_CIPHER_WEP104       5
#define WP_CIPHER_BIP_CMAC128  6
#define WP_CIPHER_GCMP_128     8
#define WP_CIPHER_GCMP_256     9
#define WP_CIPHER_CCMP_256     10
#define WP_CIPHER_BIP_GMAC128  11
#define WP_CIPHER_BIP_GMAC256  12
#define WP_CIPHER_BIP_CMAC256  13

// ── AKM suite types ───────────────────────────────────────────────────────────
#define WP_AKM_8021X           1
#define WP_AKM_PSK             2
#define WP_AKM_FT_8021X        3
#define WP_AKM_FT_PSK          4
#define WP_AKM_8021X_SHA256    5
#define WP_AKM_PSK_SHA256      6
#define WP_AKM_TDLS            7
#define WP_AKM_SAE             8
#define WP_AKM_FT_SAE          9
#define WP_AKM_OWE             18
#define WP_AKM_FILS_SHA256     14
#define WP_AKM_FILS_SHA384     15

// ── Authentication algorithms ─────────────────────────────────────────────────
#define WP_AUTH_OPEN           0
#define WP_AUTH_SHARED         1
#define WP_AUTH_FT             2
#define WP_AUTH_SAE            3
#define WP_AUTH_FILS_SK        4
#define WP_AUTH_FILS_SK_PFS    5
#define WP_AUTH_FILS_PK        6

// ── 802.11 IE tag numbers ─────────────────────────────────────────────────────
#define WP_IE_SSID             0
#define WP_IE_RATES            1
#define WP_IE_FH_PARAM         2
#define WP_IE_DS_PARAM         3
#define WP_IE_CF_PARAM         4
#define WP_IE_TIM              5
#define WP_IE_IBSS_PARAM       6
#define WP_IE_COUNTRY          7
#define WP_IE_HOP_PATTERN      8
#define WP_IE_HOP_TABLE        9
#define WP_IE_REQ              10
#define WP_IE_BSS_LOAD         11
#define WP_IE_EDCA             12
#define WP_IE_TSPEC            13
#define WP_IE_TCLAS            14
#define WP_IE_SCHEDULE         15
#define WP_IE_CHALLENGE        16
#define WP_IE_PWR_CONSTRAINT   32
#define WP_IE_PWR_CAP          33
#define WP_IE_TPC_REQ          34
#define WP_IE_TPC_REPORT       35
#define WP_IE_SUPPORTED_CHANS  36
#define WP_IE_CH_SWITCH_ANN    37
#define WP_IE_MEAS_REQ         38
#define WP_IE_MEAS_REPORT      39
#define WP_IE_QUIET            40
#define WP_IE_IBSS_DFS         41
#define WP_IE_ERP              42
#define WP_IE_TS_DELAY         43
#define WP_IE_TCLAS_PROC       44
#define WP_IE_HT_CAP           45
#define WP_IE_QOS_CAP          46
#define WP_IE_RSN              48
#define WP_IE_EXT_RATES        50
#define WP_IE_AP_CH_REPORT     51
#define WP_IE_NEIGHBOR_REPORT  52
#define WP_IE_RCPI             53
#define WP_IE_MDE              54
#define WP_IE_FTE              55
#define WP_IE_TIE              56
#define WP_IE_RDE              57
#define WP_IE_DSE_REG          58
#define WP_IE_SUPP_OP_CLASSES  59
#define WP_IE_EXT_CH_SWITCH    60
#define WP_IE_HT_OP            61
#define WP_IE_SEC_CH_OFFSET    62
#define WP_IE_BSS_AVG_DELAY    63
#define WP_IE_ANTENNA          64
#define WP_IE_RSNI             65
#define WP_IE_MEAS_PILOT       66
#define WP_IE_BSS_AVAIL_ADM    67
#define WP_IE_BSS_AC_DELAY     68
#define WP_IE_TIME_ADV         69
#define WP_IE_RRM_CAP          70
#define WP_IE_MUL_BSSID        71
#define WP_IE_20_40_BSS        72
#define WP_IE_OBSS_SCAN        74
#define WP_IE_RIC_DESCRIPTOR   75
#define WP_IE_MGMT_MIC         76
#define WP_IE_EXT_CAP          127
#define WP_IE_MESH_CONFIG      113
#define WP_IE_MESH_ID          114
#define WP_IE_MESH_LINK_MET    115
#define WP_IE_CONGESTION_NTF   116
#define WP_IE_MESH_CHANNEL     118
#define WP_IE_MIC              140
#define WP_IE_VHT_CAP          191
#define WP_IE_VHT_OP           192
#define WP_IE_EXT_BSS_LOAD     193
#define WP_IE_WIDE_BW_CH       194
#define WP_IE_VHT_TX_PWR       195
#define WP_IE_CH_SWITCH_WRAP   196
#define WP_IE_AID              197
#define WP_IE_QUIET_CHANNEL    198
#define WP_IE_OP_MODE_NTF      199
#define WP_IE_UPSID            201
#define WP_IE_RNR              201
#define WP_IE_VENDOR           221

// ── Encryption classification result ─────────────────────────────────────────
enum WpEncType : uint8_t {
  WP_ENC_OPEN   = 0,
  WP_ENC_WEP    = 1,
  WP_ENC_WPA    = 2,
  WP_ENC_WPA2   = 3,
  WP_ENC_WPA3   = 4,
  WP_ENC_WPA2_WPA3 = 5,
  WP_ENC_OWE    = 6,
  WP_ENC_ENT    = 7,  // 802.1X enterprise
  WP_ENC_UNKNOWN = 0xFF,
};

// ── Parsed RSN info ───────────────────────────────────────────────────────────
struct WpRsn {
  uint8_t  group_cipher;
  uint8_t  pairwise_ciphers[4];
  uint8_t  pairwise_count;
  uint8_t  akm_suites[4];
  uint8_t  akm_count;
  uint16_t rsn_caps;
  bool     pmf_capable;
  bool     pmf_required;
  bool     has_pmkid;
};

// ── Parsed HT capabilities ───────────────────────────────────────────────────
struct WpHtCap {
  bool     ldpc;
  bool     chan40;
  bool     greenfield;
  bool     sgi20;
  bool     sgi40;
  bool     tx_stbc;
  uint8_t  rx_stbc_streams;
  uint8_t  max_amsdu;    // 0=3839, 1=7935
  uint8_t  rx_mcs_set;  // bitmask of supported MCS indices 0-7
};

// ── Parsed VHT capabilities ──────────────────────────────────────────────────
struct WpVhtCap {
  uint8_t  max_mpdu;     // 0=3895, 1=7991, 2=11454
  uint8_t  chan_width;   // 0=80, 1=160, 2=80+80
  bool     rx_ldpc;
  bool     sgi80;
  bool     sgi160;
  bool     tx_stbc;
  uint8_t  rx_ant_pattern;
  uint8_t  tx_ant_pattern;
  uint16_t rx_mcs_map;
  uint16_t tx_mcs_map;
};

// ── Channel info parsed from DS param + HT OP ────────────────────────────────
struct WpChannelInfo {
  uint8_t  primary;
  uint8_t  secondary;    // 0 if none
  bool     is_40;
  bool     is_80;
  bool     is_160;
  int8_t   sec_offset;   // +1 (above) or -1 (below)
};

// ── BSS Load info ─────────────────────────────────────────────────────────────
struct WpBssLoad {
  uint16_t station_count;
  uint8_t  utilization;  // 0-255 = 0-100%
  uint16_t available_admission;
};

// ── Parsed frame body info ────────────────────────────────────────────────────
struct WpBeaconInfo {
  char    ssid[33];
  uint8_t ssid_len;
  bool    ssid_hidden;
  WpEncType enc;
  WpRsn   rsn;
  WpHtCap ht;
  WpVhtCap vht;
  WpChannelInfo chan;
  WpBssLoad load;
  uint16_t capability;
  uint16_t interval;
  int8_t   rssi;
  bool     has_rsn;
  bool     has_ht;
  bool     has_vht;
  bool     has_bss_load;
  bool     is_mesh;
};

// ── IE walker ─────────────────────────────────────────────────────────────────
// Iterates over IEs in a buffer; calls cb(tag, len, data, ctx) for each
typedef void (*WpIeCb)(uint8_t tag, uint8_t len, const uint8_t* data, void* ctx);

static void wp_walk_ies(const uint8_t* buf, int len, WpIeCb cb, void* ctx) {
  int pos=0;
  while(pos+1<len) {
    uint8_t tag=buf[pos]; uint8_t elen=buf[pos+1];
    if(pos+2+elen>len) break;
    cb(tag,elen,buf+pos+2,ctx);
    pos+=2+elen;
  }
}

// ── Find first IE with given tag ──────────────────────────────────────────────
static const uint8_t* wp_find_ie(const uint8_t* buf, int len, uint8_t tag, uint8_t* out_len) {
  int pos=0;
  while(pos+1<len) {
    uint8_t t=buf[pos]; uint8_t l=buf[pos+1];
    if(pos+2+l>len) break;
    if(t==tag) { if(out_len) *out_len=l; return buf+pos+2; }
    pos+=2+l;
  }
  return nullptr;
}

// ── Cipher suite name ─────────────────────────────────────────────────────────
static const char* wp_cipher_name(uint8_t suite) {
  switch(suite) {
    case WP_CIPHER_USE_GROUP:  return "UseGroup";
    case WP_CIPHER_WEP40:      return "WEP-40";
    case WP_CIPHER_TKIP:       return "TKIP";
    case WP_CIPHER_CCMP_128:   return "CCMP-128";
    case WP_CIPHER_WEP104:     return "WEP-104";
    case WP_CIPHER_BIP_CMAC128:return "BIP-CMAC-128";
    case WP_CIPHER_GCMP_128:   return "GCMP-128";
    case WP_CIPHER_GCMP_256:   return "GCMP-256";
    case WP_CIPHER_CCMP_256:   return "CCMP-256";
    case WP_CIPHER_BIP_GMAC128:return "BIP-GMAC-128";
    case WP_CIPHER_BIP_GMAC256:return "BIP-GMAC-256";
    case WP_CIPHER_BIP_CMAC256:return "BIP-CMAC-256";
    default: return "Unknown";
  }
}

// ── AKM suite name ────────────────────────────────────────────────────────────
static const char* wp_akm_name(uint8_t akm) {
  switch(akm) {
    case WP_AKM_8021X:      return "802.1X";
    case WP_AKM_PSK:        return "PSK";
    case WP_AKM_FT_8021X:   return "FT/802.1X";
    case WP_AKM_FT_PSK:     return "FT/PSK";
    case WP_AKM_8021X_SHA256:return "802.1X/SHA256";
    case WP_AKM_PSK_SHA256: return "PSK/SHA256";
    case WP_AKM_TDLS:       return "TDLS";
    case WP_AKM_SAE:        return "SAE";
    case WP_AKM_FT_SAE:     return "FT/SAE";
    case WP_AKM_OWE:        return "OWE";
    case WP_AKM_FILS_SHA256:return "FILS/SHA256";
    case WP_AKM_FILS_SHA384:return "FILS/SHA384";
    default: return "Unknown";
  }
}

// ── Encryption type name ──────────────────────────────────────────────────────
static const char* wp_enc_name(WpEncType e) {
  switch(e) {
    case WP_ENC_OPEN:     return "Open";
    case WP_ENC_WEP:      return "WEP";
    case WP_ENC_WPA:      return "WPA";
    case WP_ENC_WPA2:     return "WPA2";
    case WP_ENC_WPA3:     return "WPA3";
    case WP_ENC_WPA2_WPA3:return "WPA2/WPA3";
    case WP_ENC_OWE:      return "OWE";
    case WP_ENC_ENT:      return "Enterprise";
    default:              return "Unknown";
  }
}

// ── Parse RSN element ─────────────────────────────────────────────────────────
static bool wp_parse_rsn(const uint8_t* data, uint8_t len, WpRsn* out) {
  if(!data||len<2) return false;
  memset(out,0,sizeof(*out));
  int pos=0;
  if(len<2) return false;
  uint16_t ver=(uint16_t)(data[pos]|(data[pos+1]<<8)); pos+=2;
  if(ver!=1) return false;
  if(pos+4>len) return true; // minimal RSN
  // Group cipher
  out->group_cipher=data[pos+3]; pos+=4;
  // Pairwise ciphers
  if(pos+2>len) return true;
  uint16_t pw_count=(uint16_t)(data[pos]|(data[pos+1]<<8)); pos+=2;
  for(uint16_t i=0;i<pw_count&&i<4;i++){
    if(pos+4>len) break;
    out->pairwise_ciphers[out->pairwise_count++]=data[pos+3]; pos+=4;
  }
  // AKM suites
  if(pos+2>len) return true;
  uint16_t akm_count=(uint16_t)(data[pos]|(data[pos+1]<<8)); pos+=2;
  for(uint16_t i=0;i<akm_count&&i<4;i++){
    if(pos+4>len) break;
    out->akm_suites[out->akm_count++]=data[pos+3]; pos+=4;
  }
  // RSN capabilities
  if(pos+2>len) return true;
  out->rsn_caps=(uint16_t)(data[pos]|(data[pos+1]<<8)); pos+=2;
  out->pmf_capable  = (out->rsn_caps>>7)&1;
  out->pmf_required = (out->rsn_caps>>6)&1;
  // PMKID count
  if(pos+2>len) return true;
  uint16_t pmkid_count=(uint16_t)(data[pos]|(data[pos+1]<<8));
  out->has_pmkid=(pmkid_count>0);
  return true;
}

// ── Classify encryption from RSN + capability ─────────────────────────────────
static WpEncType wp_classify_enc(bool has_rsn, bool has_wpa1,
                                  bool privacy_bit, const WpRsn* rsn) {
  if(!privacy_bit && !has_rsn && !has_wpa1) return WP_ENC_OPEN;
  if(!has_rsn && !has_wpa1) return WP_ENC_WEP;
  if(has_wpa1 && !has_rsn) return WP_ENC_WPA;
  if(has_rsn) {
    bool has_sae=false, has_psk=false, has_owe=false, has_8021x=false;
    for(uint8_t i=0;i<rsn->akm_count;i++){
      if(rsn->akm_suites[i]==WP_AKM_SAE||rsn->akm_suites[i]==WP_AKM_FT_SAE) has_sae=true;
      if(rsn->akm_suites[i]==WP_AKM_PSK||rsn->akm_suites[i]==WP_AKM_FT_PSK||
         rsn->akm_suites[i]==WP_AKM_PSK_SHA256) has_psk=true;
      if(rsn->akm_suites[i]==WP_AKM_OWE) has_owe=true;
      if(rsn->akm_suites[i]==WP_AKM_8021X||rsn->akm_suites[i]==WP_AKM_FT_8021X||
         rsn->akm_suites[i]==WP_AKM_8021X_SHA256) has_8021x=true;
    }
    if(has_owe) return WP_ENC_OWE;
    if(has_8021x) return WP_ENC_ENT;
    if(has_sae && has_psk) return WP_ENC_WPA2_WPA3;
    if(has_sae) return WP_ENC_WPA3;
    return WP_ENC_WPA2;
  }
  return WP_ENC_UNKNOWN;
}

// ── Parse HT Capabilities IE ─────────────────────────────────────────────────
static bool wp_parse_ht_cap(const uint8_t* data, uint8_t len, WpHtCap* out) {
  if(!data||len<26) return false;
  uint16_t cap=(uint16_t)(data[0]|(data[1]<<8));
  out->ldpc            = (cap>>0)&1;
  out->chan40          = (cap>>1)&1;
  // SM PS: bits 2-3
  out->greenfield      = (cap>>4)&1;
  out->sgi20           = (cap>>5)&1;
  out->sgi40           = (cap>>6)&1;
  out->tx_stbc         = (cap>>7)&1;
  out->rx_stbc_streams = (cap>>8)&3;
  out->max_amsdu       = (cap>>11)&1;
  out->rx_mcs_set      = data[3]; // MCS indices 0-7 in first byte of MCS set
  return true;
}

// ── Parse VHT Capabilities IE ────────────────────────────────────────────────
static bool wp_parse_vht_cap(const uint8_t* data, uint8_t len, WpVhtCap* out) {
  if(!data||len<12) return false;
  uint32_t cap=(uint32_t)(data[0]|(data[1]<<8)|(data[2]<<16)|(data[3]<<24));
  out->max_mpdu        = cap&3;
  out->chan_width      = (cap>>2)&3;
  out->rx_ldpc         = (cap>>4)&1;
  out->sgi80           = (cap>>5)&1;
  out->sgi160          = (cap>>6)&1;
  out->tx_stbc         = (cap>>7)&1;
  out->rx_ant_pattern  = (cap>>28)&1;
  out->tx_ant_pattern  = (cap>>29)&1;
  out->rx_mcs_map = (uint16_t)(data[4]|(data[5]<<8));
  out->tx_mcs_map = (uint16_t)(data[8]|(data[9]<<8));
  return true;
}

// ── Parse BSS Load IE ─────────────────────────────────────────────────────────
static bool wp_parse_bss_load(const uint8_t* data, uint8_t len, WpBssLoad* out) {
  if(!data||len<5) return false;
  out->station_count       = (uint16_t)(data[0]|(data[1]<<8));
  out->utilization         = data[2];
  out->available_admission = (uint16_t)(data[3]|(data[4]<<8));
  return true;
}

// ── Parse full beacon/probe resp IEs into WpBeaconInfo ───────────────────────
static void wp_parse_beacon(const uint8_t* ies, int ies_len,
                              uint16_t capability, uint16_t interval,
                              int8_t rssi, WpBeaconInfo* out) {
  memset(out,0,sizeof(*out));
  out->capability=capability;
  out->interval=interval;
  out->rssi=rssi;
  bool has_wpa1=false;

  int pos=0;
  while(pos+1<ies_len){
    uint8_t tag=ies[pos]; uint8_t elen=ies[pos+1];
    if(pos+2+elen>ies_len) break;
    const uint8_t* d=ies+pos+2;
    switch(tag){
      case WP_IE_SSID:
        out->ssid_len=elen<32?elen:32;
        memcpy(out->ssid,d,out->ssid_len);
        out->ssid[out->ssid_len]=0;
        out->ssid_hidden=(elen==0);
        break;
      case WP_IE_DS_PARAM:
        if(elen>=1) out->chan.primary=d[0];
        break;
      case WP_IE_RSN:
        out->has_rsn=true;
        wp_parse_rsn(d,elen,&out->rsn);
        break;
      case WP_IE_HT_CAP:
        out->has_ht=true;
        wp_parse_ht_cap(d,elen,&out->ht);
        break;
      case WP_IE_HT_OP:
        if(elen>=5){
          out->chan.primary=d[0];
          uint8_t info1=d[1];
          out->chan.sec_offset=(info1&3)==1?1:(info1&3)==3?-1:0;
          out->chan.is_40 = (info1>>2)&1;
          if(out->chan.sec_offset>0) out->chan.secondary=out->chan.primary+4;
          else if(out->chan.sec_offset<0) out->chan.secondary=out->chan.primary-4;
        }
        break;
      case WP_IE_VHT_CAP:
        out->has_vht=true;
        wp_parse_vht_cap(d,elen,&out->vht);
        break;
      case WP_IE_BSS_LOAD:
        out->has_bss_load=true;
        wp_parse_bss_load(d,elen,&out->load);
        break;
      case WP_IE_MESH_CONFIG:
        out->is_mesh=true;
        break;
      case WP_IE_VENDOR:
        if(elen>=4){
          if(d[0]==0x00&&d[1]==0x50&&d[2]==0xF2&&d[3]==0x01) has_wpa1=true;
        }
        break;
    }
    pos+=2+elen;
  }
  bool privacy=(capability&0x0010)!=0;
  out->enc=wp_classify_enc(out->has_rsn,has_wpa1,privacy,&out->rsn);
  if(out->has_vht) out->chan.is_80=(out->vht.chan_width==0);
}

// ── MCS set parser ────────────────────────────────────────────────────────────
static int wp_max_spatial_streams(const uint8_t mcs_set[16]) {
  // Find highest MCS byte with bits set
  for(int i=3;i>=0;i--){
    if(mcs_set[i]) return i+1;
  }
  return 1;
}

// ── Channel to frequency ──────────────────────────────────────────────────────
static uint16_t wp_ch_to_freq(uint8_t ch) {
  if(ch>=1&&ch<=13)  return 2412+5*(ch-1);
  if(ch==14)         return 2484;
  if(ch>=36&&ch<=64) return 5180+5*(ch-36);
  if(ch>=100&&ch<=144) return 5500+5*(ch-100);
  if(ch>=149&&ch<=165) return 5745+5*(ch-149);
  if(ch>=1&&ch<=233)   return 5950+5*(ch-1); // 6GHz (simplified)
  return 0;
}

// ── Frequency to channel ──────────────────────────────────────────────────────
static uint8_t wp_freq_to_ch(uint16_t freq) {
  if(freq>=2412&&freq<=2472) return (freq-2412)/5+1;
  if(freq==2484) return 14;
  if(freq>=5180&&freq<=5320) return (freq-5180)/5+36;
  if(freq>=5500&&freq<=5720) return (freq-5500)/5+100;
  if(freq>=5745&&freq<=5825) return (freq-5745)/5+149;
  return 0;
}

// ── Beacon interval to ms ─────────────────────────────────────────────────────
static float wp_interval_ms(uint16_t interval) {
  return interval * 1.024f; // 1 TU = 1024 μs
}

// ── Capability flags parser ───────────────────────────────────────────────────
static String wp_cap_flags_str(uint16_t cap) {
  String s="";
  if(cap&0x0001) s+="ESS ";
  if(cap&0x0002) s+="IBSS ";
  if(cap&0x0010) s+="Privacy ";
  if(cap&0x0020) s+="ShortPreamble ";
  if(cap&0x0400) s+="ShortSlot ";
  if(cap&0x0800) s+="APSD ";
  if(cap&0x1000) s+="RadioMeas ";
  if(cap&0x2000) s+="DSSS-OFDM ";
  if(cap&0x4000) s+="DelayedBA ";
  if(cap&0x8000) s+="ImmediateBA ";
  return s;
}

// ── Build JSON for a parsed beacon ───────────────────────────────────────────
static String wp_beacon_json(const WpBeaconInfo* b, const uint8_t bssid[6]) {
  char mac[18];
  snprintf(mac,sizeof(mac),"%02X:%02X:%02X:%02X:%02X:%02X",
    bssid[0],bssid[1],bssid[2],bssid[3],bssid[4],bssid[5]);
  String j="{";
  j+="\"bssid\":\""; j+=mac; j+="\"";
  j+=",\"ssid\":\"";
  // Escape SSID
  for(int i=0;i<b->ssid_len;i++){
    char c=b->ssid[i];
    if(c=='"'||c=='\\'||c<0x20) { j+="\\x"; char h[3]; snprintf(h,3,"%02X",(uint8_t)c); j+=h; }
    else j+=c;
  }
  j+="\"";
  j+=",\"hidden\":"; j+=b->ssid_hidden?1:0;
  j+=",\"channel\":"; j+=b->chan.primary;
  j+=",\"freq\":"; j+=wp_ch_to_freq(b->chan.primary);
  j+=",\"rssi\":"; j+=b->rssi;
  j+=",\"enc\":\""; j+=wp_enc_name(b->enc); j+="\"";
  j+=",\"interval\":"; j+=b->interval;
  j+=",\"ht\":"; j+=b->has_ht?1:0;
  j+=",\"vht\":"; j+=b->has_vht?1:0;
  j+=",\"mesh\":"; j+=b->is_mesh?1:0;
  if(b->has_rsn){
    j+=",\"pmf_cap\":"; j+=b->rsn.pmf_capable?1:0;
    j+=",\"pmf_req\":"; j+=b->rsn.pmf_required?1:0;
    j+=",\"akm\":\"";
    for(int i=0;i<b->rsn.akm_count;i++){
      if(i) j+=",";
      j+=wp_akm_name(b->rsn.akm_suites[i]);
    }
    j+="\"";
    j+=",\"cipher\":\""; j+=wp_cipher_name(b->rsn.group_cipher); j+="\"";
  }
  if(b->has_bss_load){
    j+=",\"sta_count\":"; j+=b->load.station_count;
    j+=",\"utilization\":"; j+=b->load.utilization;
  }
  j+="}";
  return j;
}

// ── EAPOL key frame parser ────────────────────────────────────────────────────
struct __attribute__((packed)) WpEapolKey {
  uint8_t  version;
  uint8_t  type;        // 0x03 = key
  uint16_t length_be;
  uint8_t  descriptor;  // 0x02=RSN
  uint16_t key_info_be;
  uint16_t key_len_be;
  uint64_t replay_counter;
  uint8_t  key_nonce[32];
  uint8_t  key_iv[16];
  uint64_t key_rsc;
  uint64_t reserved;
  uint8_t  key_mic[16];
  uint16_t key_data_len_be;
  // key data follows
};

static bool wp_is_eapol_key(const uint8_t* data, int len) {
  if(len<sizeof(WpEapolKey)) return false;
  return data[0]==0x01 && data[1]==0x03;
}

static bool wp_is_m1(const WpEapolKey* k) {
  uint16_t ki=__builtin_bswap16(k->key_info_be);
  return (ki&0x0008)&&!(ki&0x0100); // Key ACK set, no Key MIC
}
static bool wp_is_m2(const WpEapolKey* k) {
  uint16_t ki=__builtin_bswap16(k->key_info_be);
  return (ki&0x0100)&&!(ki&0x0008); // Key MIC set, no Key ACK
}
static bool wp_is_m3(const WpEapolKey* k) {
  uint16_t ki=__builtin_bswap16(k->key_info_be);
  return (ki&0x0008)&&(ki&0x0100)&&(ki&0x0040); // ACK+MIC+Install
}
static bool wp_is_m4(const WpEapolKey* k) {
  uint16_t ki=__builtin_bswap16(k->key_info_be);
  return (ki&0x0100)&&!(ki&0x0008)&&!(ki&0x0040); // MIC only, tiny data
}

static const char* wp_eapol_msg_name(const WpEapolKey* k){
  if(wp_is_m1(k)) return "M1";
  if(wp_is_m2(k)) return "M2";
  if(wp_is_m3(k)) return "M3";
  if(wp_is_m4(k)) return "M4";
  return "??";
}

// ── PMKID extraction from M1 ─────────────────────────────────────────────────
// PMKID is in the RSN IE appended to key data of M1 (WPA2)
static bool wp_extract_pmkid(const uint8_t* keydata, uint16_t kd_len, uint8_t out[16]) {
  uint8_t ie_len=0;
  const uint8_t* rsn=wp_find_ie(keydata,kd_len,WP_IE_RSN,&ie_len);
  if(!rsn||ie_len<22) return false;
  // Check PMKID count after AKMs
  int pos=2; // skip version
  if(pos+4>ie_len) return false;
  pos+=4; // group cipher
  uint16_t pw_cnt=(uint16_t)(rsn[pos]|(rsn[pos+1]<<8)); pos+=2+pw_cnt*4;
  if(pos+2>ie_len) return false;
  uint16_t akm_cnt=(uint16_t)(rsn[pos]|(rsn[pos+1]<<8)); pos+=2+akm_cnt*4;
  if(pos+2>ie_len) return false;
  pos+=2; // skip RSN caps
  if(pos+2>ie_len) return false;
  uint16_t pmkid_cnt=(uint16_t)(rsn[pos]|(rsn[pos+1]<<8)); pos+=2;
  if(pmkid_cnt>0 && pos+16<=ie_len){
    memcpy(out,rsn+pos,16);
    return true;
  }
  return false;
}

// ── Rates IE to Mbps array ───────────────────────────────────────────────────
static int wp_parse_rates(const uint8_t* data, uint8_t len, float* rates_mbps, int max_rates) {
  int count=0;
  for(int i=0;i<len&&count<max_rates;i++){
    float r=((data[i]&0x7F)*0.5f);
    rates_mbps[count++]=r;
  }
  return count;
}

// ── Supported rates → max rate ────────────────────────────────────────────────
static float wp_max_rate(const uint8_t* rates_data, uint8_t rates_len,
                          const uint8_t* ext_rates_data, uint8_t ext_rates_len) {
  float max=0.0f;
  for(int i=0;i<rates_len;i++){
    float r=((rates_data[i]&0x7F)*0.5f);
    if(r>max) max=r;
  }
  for(int i=0;i<ext_rates_len;i++){
    float r=((ext_rates_data[i]&0x7F)*0.5f);
    if(r>max) max=r;
  }
  return max;
}

// ── Country IE parser ─────────────────────────────────────────────────────────
static bool wp_parse_country(const uint8_t* data, uint8_t len, char cc[3]) {
  if(!data||len<3) return false;
  cc[0]=(char)data[0]; cc[1]=(char)data[1]; cc[2]=0;
  return true;
}

// ── ERP IE analysis ───────────────────────────────────────────────────────────
static bool wp_erp_uses_protection(const uint8_t* data, uint8_t len) {
  return data&&len>=1&&(data[0]&0x02);
}
static bool wp_erp_has_nonerpresent(const uint8_t* data, uint8_t len) {
  return data&&len>=1&&(data[0]&0x01);
}

// ── TIM IE parser ─────────────────────────────────────────────────────────────
struct WpTim {
  uint8_t dtim_count;
  uint8_t dtim_period;
  uint8_t bitmap_control;
};
static bool wp_parse_tim(const uint8_t* data, uint8_t len, WpTim* out) {
  if(!data||len<3) return false;
  out->dtim_count  =data[0];
  out->dtim_period =data[1];
  out->bitmap_control=data[2];
  return true;
}

// ── Frame type description ────────────────────────────────────────────────────
static const char* wp_frame_type_name(uint8_t type, uint8_t stype) {
  if(type==0) {
    static const char* mgmt[]={
      "Assoc Req","Assoc Resp","Reassoc Req","Reassoc Resp",
      "Probe Req","Probe Resp","Timing Adv","[Resv]",
      "Beacon","ATIM","Disassoc","Auth",
      "Deauth","Action","Action NoAck","[Resv]"
    };
    if(stype<16) return mgmt[stype];
  }
  if(type==1) {
    static const char* ctrl[]={
      "[Resv]","[Resv]","[Resv]","[Resv]",
      "[Resv]","[Resv]","[Resv]","Ctrl Wrapper",
      "BAR","BA","PS-Poll","RTS","CTS","ACK","CF-End","CF-End+ACK"
    };
    if(stype<16) return ctrl[stype];
  }
  if(type==2) {
    static const char* data[]={
      "Data","Data+CF-ACK","Data+CF-Poll","Data+CF-ACK+Poll",
      "Null","CF-ACK","CF-Poll","CF-ACK+Poll",
      "QoS Data","QoS Data+CF-ACK","QoS Data+CF-Poll","QoS Data+CF-ACK+Poll",
      "QoS Null","[Resv]","QoS CF-Poll","QoS CF-ACK+Poll"
    };
    if(stype<16) return data[stype];
  }
  return "Unknown";
}

// ── Frame control decoder ─────────────────────────────────────────────────────
struct WpFcDecoded {
  uint8_t type;
  uint8_t stype;
  bool    to_ds;
  bool    from_ds;
  bool    more_frags;
  bool    retry;
  bool    pwr_mgmt;
  bool    more_data;
  bool    protected_frame;
  bool    htc_order;
  const char* frame_name;
};

static WpFcDecoded wp_decode_fc(uint16_t fc) {
  WpFcDecoded d;
  // 802.11 FC: bits 2-3=type, bits 4-7=subtype, bit 8=to_ds, ...
  uint16_t f = __builtin_bswap16(fc); // native endian
  d.type    = (f>>2)&3;
  d.stype   = (f>>4)&0xF;
  d.to_ds   = (f>>8)&1;
  d.from_ds = (f>>9)&1;
  d.more_frags    = (f>>10)&1;
  d.retry         = (f>>11)&1;
  d.pwr_mgmt      = (f>>12)&1;
  d.more_data     = (f>>13)&1;
  d.protected_frame=(f>>14)&1;
  d.htc_order     = (f>>15)&1;
  d.frame_name=wp_frame_type_name(d.type,d.stype);
  return d;
}

// ── Channel bandwidth description ────────────────────────────────────────────
static const char* wp_bw_str(const WpChannelInfo& c) {
  if(c.is_160) return "160 MHz";
  if(c.is_80)  return "80 MHz";
  if(c.is_40)  return "40 MHz";
  return "20 MHz";
}

// ── Deauth reason name ────────────────────────────────────────────────────────
static const char* wp_reason_name(uint16_t reason) {
  switch(reason){
    case 1: return "Unspecified";
    case 2: return "Auth no longer valid";
    case 3: return "Deauth leaving";
    case 4: return "Disassoc inactivity";
    case 5: return "AP busy";
    case 6: return "Class2 from non-auth";
    case 7: return "Class3 from non-assoc";
    case 8: return "STA left BSS";
    case 9: return "STA not auth";
    case 13: return "Invalid IE";
    case 14: return "MIC failure";
    case 15: return "4-way handshake timeout";
    case 16: return "Group key timeout";
    case 17: return "IE diff";
    case 18: return "Invalid group cipher";
    case 19: return "Invalid pairwise cipher";
    case 20: return "Invalid AKMP";
    case 21: return "Unsupported RSNE";
    case 22: return "Invalid RSNE caps";
    case 23: return "802.1X auth failed";
    case 24: return "Cipher rejected";
    case 39: return "Timeout";
    case 46: return "Cipher no match";
    default: return "Unknown";
  }
}

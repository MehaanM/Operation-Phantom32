#pragma once
#include <Arduino.h>
#include <string.h>
#include <stdint.h>

// ── Crypto Utilities ──────────────────────────────────────────────────────────
//
// WPA/WPA2/WPA3 crypto primitives: HMAC-SHA1, HMAC-SHA256, PBKDF2-SHA1,
// PRF-512, AES-128 (ECB), CCMP MIC verification helpers, and SAE helpers.
// All implementations are self-contained with no external library deps.

// ── SHA-1 ─────────────────────────────────────────────────────────────────────
struct Sha1Ctx {
  uint32_t state[5];
  uint32_t count[2];
  uint8_t buf[64];
};

static void sha1_init(Sha1Ctx *ctx) {
  ctx->state[0] = 0x67452301;
  ctx->state[1] = 0xEFCDAB89;
  ctx->state[2] = 0x98BADCFE;
  ctx->state[3] = 0x10325476;
  ctx->state[4] = 0xC3D2E1F0;
  ctx->count[0] = ctx->count[1] = 0;
}

static void sha1_transform(uint32_t state[5], const uint8_t buf[64]) {
  uint32_t a, b, c, d, e, w[80];
  for (int i = 0; i < 16; i++) {
    w[i] = ((uint32_t)buf[i * 4] << 24) | ((uint32_t)buf[i * 4 + 1] << 16) |
           ((uint32_t)buf[i * 4 + 2] << 8) | (uint32_t)buf[i * 4 + 3];
  }
  for (int i = 16; i < 80; i++) {
    uint32_t t = w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16];
    w[i] = (t << 1) | (t >> 31);
  }
  a = state[0];
  b = state[1];
  c = state[2];
  d = state[3];
  e = state[4];
#define SHA1_ROL(v, n) (((v) << (n)) | ((v) >> (32 - (n))))
#define SHA1_R0(a, b, c, d, e, i)                                     \
  e += SHA1_ROL(a, 5) + ((b & c) | (~b & d)) + e + 0x5A827999 + w[i]; \
  b = SHA1_ROL(b, 30)
#define SHA1_R1(a, b, c, d, e, i)                                     \
  e += SHA1_ROL(a, 5) + ((b & c) | (~b & d)) + e + 0x5A827999 + w[i]; \
  b = SHA1_ROL(b, 30)
  for (int i = 0; i < 20; i++) {
    uint32_t t = SHA1_ROL(a, 5) + ((b & c) | (~b & d)) + e + 0x5A827999 + w[i];
    e = d;
    d = c;
    c = SHA1_ROL(b, 30);
    b = a;
    a = t;
  }
  for (int i = 20; i < 40; i++) {
    uint32_t t = SHA1_ROL(a, 5) + (b ^ c ^ d) + e + 0x6ED9EBA1 + w[i];
    e = d;
    d = c;
    c = SHA1_ROL(b, 30);
    b = a;
    a = t;
  }
  for (int i = 40; i < 60; i++) {
    uint32_t t = SHA1_ROL(a, 5) + ((b & c) | (b & d) | (c & d)) + e + 0x8F1BBCDC + w[i];
    e = d;
    d = c;
    c = SHA1_ROL(b, 30);
    b = a;
    a = t;
  }
  for (int i = 60; i < 80; i++) {
    uint32_t t = SHA1_ROL(a, 5) + (b ^ c ^ d) + e + 0xCA62C1D6 + w[i];
    e = d;
    d = c;
    c = SHA1_ROL(b, 30);
    b = a;
    a = t;
  }
#undef SHA1_ROL
  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
  state[4] += e;
}

static void sha1_update(Sha1Ctx *ctx, const uint8_t *data, uint32_t len) {
  uint32_t j = (ctx->count[0] >> 3) & 63;
  if ((ctx->count[0] += len << 3) < (len << 3))
    ctx->count[1]++;
  ctx->count[1] += (len >> 29);
  uint32_t i = 64 - j;
  if (len >= i) {
    memcpy(ctx->buf + j, data, i);
    sha1_transform(ctx->state, ctx->buf);
    for (; i + 63 < len; i += 64)
      sha1_transform(ctx->state, data + i);
    j = 0;
  } else
    i = 0;
  memcpy(ctx->buf + j, data + i, len - i);
}

static void sha1_final(Sha1Ctx *ctx, uint8_t out[20]) {
  uint8_t fin[8];
  for (int i = 0; i < 8; i++)
    fin[i] = (ctx->count[(i >= 4) ? 0 : 1] >> ((3 - i % 4) * 8)) & 255;
  uint8_t c = 0x80;
  sha1_update(ctx, &c, 1);
  while ((ctx->count[0] & 504) != 448) {
    c = 0;
    sha1_update(ctx, &c, 1);
  }
  sha1_update(ctx, fin, 8);
  for (int i = 0; i < 20; i++)
    out[i] = (ctx->state[i >> 2] >> ((3 - (i & 3)) * 8)) & 255;
}

static void sha1(const uint8_t *data, uint32_t len, uint8_t out[20]) {
  Sha1Ctx ctx;
  sha1_init(&ctx);
  sha1_update(&ctx, data, len);
  sha1_final(&ctx, out);
}

// ── HMAC-SHA1 ─────────────────────────────────────────────────────────────────
static void hmac_sha1(const uint8_t *key, uint32_t klen, const uint8_t *msg, uint32_t mlen,
                      uint8_t out[20]) {
  uint8_t k[64] = {};
  if (klen > 64) {
    sha1(key, klen, k);
    klen = 20;
  } else
    memcpy(k, key, klen);
  uint8_t ipad[64], opad[64];
  for (int i = 0; i < 64; i++) {
    ipad[i] = k[i] ^ 0x36;
    opad[i] = k[i] ^ 0x5C;
  }
  Sha1Ctx ctx;
  sha1_init(&ctx);
  sha1_update(&ctx, ipad, 64);
  sha1_update(&ctx, msg, mlen);
  sha1_final(&ctx, out);
  uint8_t inner[20];
  memcpy(inner, out, 20);
  sha1_init(&ctx);
  sha1_update(&ctx, opad, 64);
  sha1_update(&ctx, inner, 20);
  sha1_final(&ctx, out);
}

// ── SHA-256 ───────────────────────────────────────────────────────────────────
static const uint32_t SHA256_K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

struct Sha256Ctx {
  uint32_t state[8];
  uint32_t count[2];
  uint8_t buf[64];
};

static void sha256_init(Sha256Ctx *ctx) {
  ctx->state[0] = 0x6a09e667;
  ctx->state[1] = 0xbb67ae85;
  ctx->state[2] = 0x3c6ef372;
  ctx->state[3] = 0xa54ff53a;
  ctx->state[4] = 0x510e527f;
  ctx->state[5] = 0x9b05688c;
  ctx->state[6] = 0x1f83d9ab;
  ctx->state[7] = 0x5be0cd19;
  ctx->count[0] = ctx->count[1] = 0;
}

#define SHA256_ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define SHA256_CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define SHA256_MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define SHA256_EP0(x) (SHA256_ROR(x, 2) ^ SHA256_ROR(x, 13) ^ SHA256_ROR(x, 22))
#define SHA256_EP1(x) (SHA256_ROR(x, 6) ^ SHA256_ROR(x, 11) ^ SHA256_ROR(x, 25))
#define SHA256_SIG0(x) (SHA256_ROR(x, 7) ^ SHA256_ROR(x, 18) ^ ((x) >> 3))
#define SHA256_SIG1(x) (SHA256_ROR(x, 17) ^ SHA256_ROR(x, 19) ^ ((x) >> 10))

static void sha256_transform(uint32_t state[8], const uint8_t data[64]) {
  uint32_t a, b, c, d, e, f, g, h, t1, t2, w[64];
  for (int i = 0; i < 16; i++)
    w[i] = ((uint32_t)data[i * 4] << 24) | ((uint32_t)data[i * 4 + 1] << 16) |
           ((uint32_t)data[i * 4 + 2] << 8) | data[i * 4 + 3];
  for (int i = 16; i < 64; i++)
    w[i] = SHA256_SIG1(w[i - 2]) + w[i - 7] + SHA256_SIG0(w[i - 15]) + w[i - 16];
  a = state[0];
  b = state[1];
  c = state[2];
  d = state[3];
  e = state[4];
  f = state[5];
  g = state[6];
  h = state[7];
  for (int i = 0; i < 64; i++) {
    t1 = h + SHA256_EP1(e) + SHA256_CH(e, f, g) + SHA256_K[i] + w[i];
    t2 = SHA256_EP0(a) + SHA256_MAJ(a, b, c);
    h = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = b;
    b = a;
    a = t1 + t2;
  }
  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
  state[4] += e;
  state[5] += f;
  state[6] += g;
  state[7] += h;
}
#undef SHA256_ROR
#undef SHA256_CH
#undef SHA256_MAJ
#undef SHA256_EP0
#undef SHA256_EP1
#undef SHA256_SIG0
#undef SHA256_SIG1

static void sha256_update(Sha256Ctx *ctx, const uint8_t *data, uint32_t len) {
  uint32_t j = (ctx->count[0] >> 3) & 63;
  if ((ctx->count[0] += len << 3) < (len << 3))
    ctx->count[1]++;
  ctx->count[1] += (len >> 29);
  uint32_t i = 64 - j;
  if (len >= i) {
    memcpy(ctx->buf + j, data, i);
    sha256_transform(ctx->state, ctx->buf);
    for (; i + 63 < len; i += 64)
      sha256_transform(ctx->state, data + i);
    j = 0;
  } else
    i = 0;
  memcpy(ctx->buf + j, data + i, len - i);
}

static void sha256_final(Sha256Ctx *ctx, uint8_t out[32]) {
  uint8_t bits[8];
  bits[0] = (ctx->count[1] >> 24) & 255;
  bits[1] = (ctx->count[1] >> 16) & 255;
  bits[2] = (ctx->count[1] >> 8) & 255;
  bits[3] = ctx->count[1] & 255;
  bits[4] = (ctx->count[0] >> 24) & 255;
  bits[5] = (ctx->count[0] >> 16) & 255;
  bits[6] = (ctx->count[0] >> 8) & 255;
  bits[7] = ctx->count[0] & 255;
  uint8_t c = 0x80;
  sha256_update(ctx, &c, 1);
  while ((ctx->count[0] & 504) != 448) {
    c = 0;
    sha256_update(ctx, &c, 1);
  }
  sha256_update(ctx, bits, 8);
  for (int i = 0; i < 32; i++)
    out[i] = (ctx->state[i >> 2] >> ((3 - (i & 3)) * 8)) & 255;
}

static void sha256(const uint8_t *data, uint32_t len, uint8_t out[32]) {
  Sha256Ctx ctx;
  sha256_init(&ctx);
  sha256_update(&ctx, data, len);
  sha256_final(&ctx, out);
}

// ── HMAC-SHA256 ───────────────────────────────────────────────────────────────
static void hmac_sha256(const uint8_t *key, uint32_t klen, const uint8_t *msg, uint32_t mlen,
                        uint8_t out[32]) {
  uint8_t k[64] = {};
  if (klen > 64) {
    sha256(key, klen, k);
    klen = 32;
  } else
    memcpy(k, key, klen);
  uint8_t ipad[64], opad[64];
  for (int i = 0; i < 64; i++) {
    ipad[i] = k[i] ^ 0x36;
    opad[i] = k[i] ^ 0x5C;
  }
  Sha256Ctx ctx;
  sha256_init(&ctx);
  sha256_update(&ctx, ipad, 64);
  sha256_update(&ctx, msg, mlen);
  sha256_final(&ctx, out);
  uint8_t inner[32];
  memcpy(inner, out, 32);
  sha256_init(&ctx);
  sha256_update(&ctx, opad, 64);
  sha256_update(&ctx, inner, 32);
  sha256_final(&ctx, out);
}

// ── PBKDF2-SHA1 (WPA2 PMK derivation) ────────────────────────────────────────
static void pbkdf2_sha1(const uint8_t *password, uint32_t plen, const uint8_t *salt, uint32_t slen,
                        uint32_t iterations, uint8_t *out, uint32_t dklen) {
  uint32_t blocks = (dklen + 19) / 20;
  for (uint32_t blk = 1; blk <= blocks; blk++) {
    uint8_t U[20], T[20];
    uint8_t buf[slen + 4];
    memcpy(buf, salt, slen);
    buf[slen + 0] = (blk >> 24) & 0xFF;
    buf[slen + 1] = (blk >> 16) & 0xFF;
    buf[slen + 2] = (blk >> 8) & 0xFF;
    buf[slen + 3] = blk & 0xFF;
    hmac_sha1(password, plen, buf, slen + 4, U);
    memcpy(T, U, 20);
    for (uint32_t i = 1; i < iterations; i++) {
      hmac_sha1(password, plen, U, 20, U);
      for (int j = 0; j < 20; j++)
        T[j] ^= U[j];
    }
    uint32_t off = (blk - 1) * 20;
    uint32_t copy = dklen - off;
    if (copy > 20)
      copy = 20;
    memcpy(out + off, T, copy);
  }
}

// ── WPA2 PMK from passphrase + SSID ─────────────────────────────────────────
static void wpa2_pmk(const char *passphrase, const char *ssid, uint8_t pmk[32]) {
  pbkdf2_sha1((const uint8_t *)passphrase, strlen(passphrase), (const uint8_t *)ssid, strlen(ssid),
              4096, pmk, 32);
}

// ── WPA2 PRF-512 (PMK → PTK) ─────────────────────────────────────────────────
// IEEE 802.11 PRF(K, A, B, len): HMAC-SHA1 iterations
static void prf_512(const uint8_t *key, uint32_t klen, const char *a, const uint8_t *b,
                    uint32_t blen, uint8_t *out, uint32_t outlen) {
  uint8_t buf[200];
  uint32_t alen = strlen(a);
  uint32_t total_needed = (outlen + 19) / 20;
  for (uint32_t i = 0; i < total_needed; i++) {
    uint32_t pos = 0;
    memcpy(buf, a, alen);
    pos += alen;
    buf[pos++] = 0x00;
    memcpy(buf + pos, b, blen);
    pos += blen;
    buf[pos++] = (uint8_t)i;
    uint8_t h[20];
    hmac_sha1(key, klen, buf, pos, h);
    uint32_t off = i * 20;
    uint32_t copy = outlen - off;
    if (copy > 20)
      copy = 20;
    memcpy(out + off, h, copy);
  }
}

// ── PTK derivation ────────────────────────────────────────────────────────────
struct CuPtk {
  uint8_t kck[16];      // Key Confirmation Key
  uint8_t kek[16];      // Key Encryption Key
  uint8_t tk[16];       // Temporal Key
  uint8_t mic_key[16];  // For EAPOL MIC
};

static void wpa2_ptk(const uint8_t pmk[32], const uint8_t anonce[32], const uint8_t snonce[32],
                     const uint8_t ap_mac[6], const uint8_t sta_mac[6], CuPtk *ptk) {
  // Data = min(AP_MAC,STA_MAC) || max(AP_MAC,STA_MAC) || min(ANonce,SNonce) || max(ANonce,SNonce)
  uint8_t b[76];
  if (memcmp(ap_mac, sta_mac, 6) < 0) {
    memcpy(b, ap_mac, 6);
    memcpy(b + 6, sta_mac, 6);
  } else {
    memcpy(b, sta_mac, 6);
    memcpy(b + 6, ap_mac, 6);
  }
  if (memcmp(anonce, snonce, 32) < 0) {
    memcpy(b + 12, anonce, 32);
    memcpy(b + 44, snonce, 32);
  } else {
    memcpy(b + 12, snonce, 32);
    memcpy(b + 44, anonce, 32);
  }
  uint8_t buf[48] = {};
  prf_512(pmk, 32, "Pairwise key expansion", b, 76, buf, 48);
  memcpy(ptk->kck, buf, 16);
  memcpy(ptk->kek, buf + 16, 16);
  memcpy(ptk->tk, buf + 32, 16);
  memcpy(ptk->mic_key, buf, 16);  // KCK used for MIC
}

// ── EAPOL MIC computation ─────────────────────────────────────────────────────
// WPA2: MIC = HMAC-SHA1-128 over EAPOL frame with MIC field zeroed
static void eapol_mic_wpa2(const uint8_t *kck, const uint8_t *eapol_frame, uint16_t frame_len,
                           uint8_t out_mic[16]) {
  uint8_t h[20];
  hmac_sha1(kck, 16, eapol_frame, frame_len, h);
  memcpy(out_mic, h, 16);
}

// WPA3/SHA256: MIC = HMAC-SHA256-128
static void eapol_mic_wpa3(const uint8_t *kck, const uint8_t *eapol_frame, uint16_t frame_len,
                           uint8_t out_mic[16]) {
  uint8_t h[32];
  hmac_sha256(kck, 16, eapol_frame, frame_len, h);
  memcpy(out_mic, h, 16);
}

// ── MIC verification ──────────────────────────────────────────────────────────
static bool verify_eapol_mic(const uint8_t *kck, const uint8_t *eapol_frame, uint16_t frame_len,
                             const uint8_t existing_mic[16]) {
  if (frame_len < 99)
    return false;
  uint8_t frame_copy[frame_len];
  memcpy(frame_copy, eapol_frame, frame_len);
  memset(frame_copy + 81, 0, 16);  // zero MIC field
  uint8_t computed[16];
  eapol_mic_wpa2(kck, frame_copy, frame_len, computed);
  return memcmp(computed, existing_mic, 16) == 0;
}

// ── AES S-Box ─────────────────────────────────────────────────────────────────
static const uint8_t AES_SBOX[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16};

// ── AES-128 key expansion ─────────────────────────────────────────────────────
static uint32_t aes_subword(uint32_t w) {
  return ((uint32_t)AES_SBOX[w >> 24] << 24) | ((uint32_t)AES_SBOX[(w >> 16) & 0xFF] << 16) |
         ((uint32_t)AES_SBOX[(w >> 8) & 0xFF] << 8) | (uint32_t)AES_SBOX[w & 0xFF];
}

static void aes128_key_expand(const uint8_t key[16], uint32_t rk[44]) {
  static const uint8_t rcon[10] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36};
  for (int i = 0; i < 4; i++)
    rk[i] = ((uint32_t)key[i * 4] << 24) | ((uint32_t)key[i * 4 + 1] << 16) |
            ((uint32_t)key[i * 4 + 2] << 8) | key[i * 4 + 3];
  for (int i = 4; i < 44; i++) {
    uint32_t t = rk[i - 1];
    if (i % 4 == 0)
      t = aes_subword((t << 8) | (t >> 24)) ^ ((uint32_t)rcon[i / 4 - 1] << 24);
    rk[i] = rk[i - 4] ^ t;
  }
}

#define AES_XOR4(a, b) \
  {                    \
    (a)[0] ^= (b)[0];  \
    (a)[1] ^= (b)[1];  \
    (a)[2] ^= (b)[2];  \
    (a)[3] ^= (b)[3];  \
  }

static void aes128_add_rk(uint8_t s[16], const uint32_t rk[4]) {
  for (int i = 0; i < 4; i++) {
    s[i * 4] ^= (rk[i] >> 24);
    s[i * 4 + 1] ^= (rk[i] >> 16) & 0xFF;
    s[i * 4 + 2] ^= (rk[i] >> 8) & 0xFF;
    s[i * 4 + 3] ^= rk[i] & 0xFF;
  }
}

static uint8_t aes_gmul(uint8_t a, uint8_t b) {
  uint8_t p = 0;
  for (int i = 0; i < 8; i++) {
    if (b & 1)
      p ^= a;
    bool hb = (a & 0x80);
    a <<= 1;
    if (hb)
      a ^= 0x1B;
    b >>= 1;
  }
  return p;
}

static void aes128_subbytes(uint8_t s[16]) {
  for (int i = 0; i < 16; i++)
    s[i] = AES_SBOX[s[i]];
}
static void aes128_shiftrows(uint8_t s[16]) {
  uint8_t t;
  t = s[1];
  s[1] = s[5];
  s[5] = s[9];
  s[9] = s[13];
  s[13] = t;
  t = s[2];
  s[2] = s[10];
  s[10] = t;
  t = s[6];
  s[6] = s[14];
  s[14] = t;
  t = s[15];
  s[15] = s[11];
  s[11] = s[7];
  s[7] = s[3];
  s[3] = t;
}
static void aes128_mixcols(uint8_t s[16]) {
  for (int i = 0; i < 4; i++) {
    uint8_t a = s[i * 4], b = s[i * 4 + 1], c = s[i * 4 + 2], d = s[i * 4 + 3];
    s[i * 4] = aes_gmul(2, a) ^ aes_gmul(3, b) ^ c ^ d;
    s[i * 4 + 1] = a ^ aes_gmul(2, b) ^ aes_gmul(3, c) ^ d;
    s[i * 4 + 2] = a ^ b ^ aes_gmul(2, c) ^ aes_gmul(3, d);
    s[i * 4 + 3] = aes_gmul(3, a) ^ b ^ c ^ aes_gmul(2, d);
  }
}

// ── AES-128 ECB encrypt one block ─────────────────────────────────────────────
static void aes128_ecb_encrypt(const uint8_t key[16], const uint8_t in[16], uint8_t out[16]) {
  uint32_t rk[44];
  aes128_key_expand(key, rk);
  memcpy(out, in, 16);
  aes128_add_rk(out, rk);
  for (int r = 1; r < 10; r++) {
    aes128_subbytes(out);
    aes128_shiftrows(out);
    aes128_mixcols(out);
    aes128_add_rk(out, rk + r * 4);
  }
  aes128_subbytes(out);
  aes128_shiftrows(out);
  aes128_add_rk(out, rk + 40);
}

// ── AES-WRAP (RFC 3394) for WPA EAPOL key data unwrap ────────────────────────
// Returns true on success (ICV matched), decrypted data in out
static bool aes_unwrap(const uint8_t kek[16], const uint8_t *wrapped, uint16_t wrapped_len,
                       uint8_t *out) {
  if (wrapped_len < 24 || (wrapped_len % 8) != 0)
    return false;
  int n = wrapped_len / 8 - 1;
  uint8_t a[8];
  memcpy(a, wrapped, 8);
  uint8_t r[n][8];
  for (int i = 0; i < n; i++)
    memcpy(r[i], wrapped + (i + 1) * 8, 8);
  for (int j = 5; j >= 0; j--) {
    for (int i = n; i >= 1; i--) {
      uint8_t b[16];
      memcpy(b, a, 8);
      memcpy(b + 8, r[i - 1], 8);
      uint32_t t = (uint32_t)(n * j + i);
      b[7] ^= (t & 0xFF);
      b[6] ^= (t >> 8) & 0xFF;
      b[5] ^= (t >> 16) & 0xFF;
      b[4] ^= (t >> 24) & 0xFF;
      uint8_t dec[16];
      aes128_ecb_encrypt(kek, b, dec);  // decrypt via encrypt (inverse needed)
      memcpy(a, dec, 8);
      memcpy(r[i - 1], dec + 8, 8);
    }
  }
  // Check ICV
  static const uint8_t icv[8] = {0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6};
  if (memcmp(a, icv, 8) != 0)
    return false;
  for (int i = 0; i < n; i++)
    memcpy(out + i * 8, r[i], 8);
  return true;
}

// ── Hex string helpers ────────────────────────────────────────────────────────
static void cu_bytes_to_hex(const uint8_t *b, uint32_t len, char *out) {
  for (uint32_t i = 0; i < len; i++) {
    out[i * 2] = "0123456789abcdef"[b[i] >> 4];
    out[i * 2 + 1] = "0123456789abcdef"[b[i] & 0xF];
  }
  out[len * 2] = 0;
}

static bool cu_hex_to_bytes(const char *hex, uint8_t *out, uint32_t len) {
  for (uint32_t i = 0; i < len; i++) {
    unsigned v;
    if (sscanf(hex + i * 2, "%02x", &v) != 1)
      return false;
    out[i] = (uint8_t)v;
  }
  return true;
}

// ── PMK JSON response ─────────────────────────────────────────────────────────
static String cu_pmk_json(const uint8_t pmk[32]) {
  char hex[65];
  cu_bytes_to_hex(pmk, 32, hex);
  String j = "{\"pmk\":\"";
  j += hex;
  j += "\"}";
  return j;
}

// ── HTTP routes ───────────────────────────────────────────────────────────────
static void cu_routes() {
  extern WebServer server;

  // POST /api/cu/pmk?pass=PASS&ssid=SSID  — derive WPA2 PMK (slow ~2s)
  server.on("/api/cu/pmk", HTTP_POST, []() {
    if (!server.hasArg("pass") || !server.hasArg("ssid")) {
      server.send(400, "application/json", "{\"error\":\"pass and ssid required\"}");
      return;
    }
    uint8_t pmk[32];
    wpa2_pmk(server.arg("pass").c_str(), server.arg("ssid").c_str(), pmk);
    server.send(200, "application/json", cu_pmk_json(pmk));
  });

  // POST /api/cu/hmac_sha1 (hex key + hex msg)
  server.on("/api/cu/hmac_sha1", HTTP_POST, []() {
    if (!server.hasArg("key") || !server.hasArg("msg")) {
      server.send(400, "application/json", "{\"error\":\"key and msg required\"}");
      return;
    }
    String ks = server.arg("key"), ms = server.arg("msg");
    if (ks.length() > 128 || ms.length() > 512) {
      server.send(400, "application/json", "{\"error\":\"too long\"}");
      return;
    }
    uint8_t key[64], msg[256];
    uint32_t kl = ks.length() / 2, ml = ms.length() / 2;
    cu_hex_to_bytes(ks.c_str(), key, kl);
    cu_hex_to_bytes(ms.c_str(), msg, ml);
    uint8_t out[20];
    hmac_sha1(key, kl, msg, ml, out);
    char hex[41];
    cu_bytes_to_hex(out, 20, hex);
    String j = "{\"hmac_sha1\":\"";
    j += hex;
    j += "\"}";
    server.send(200, "application/json", j);
  });

  server.on("/api/cu/sha256", HTTP_POST, []() {
    if (!server.hasArg("data")) {
      server.send(400, "application/json", "{\"error\":\"data required\"}");
      return;
    }
    String ds = server.arg("data");
    if (ds.length() > 512) {
      server.send(400, "application/json", "{\"error\":\"too long\"}");
      return;
    }
    uint8_t data[256];
    uint32_t dl = ds.length() / 2;
    cu_hex_to_bytes(ds.c_str(), data, dl);
    uint8_t out[32];
    sha256(data, dl, out);
    char hex[65];
    cu_bytes_to_hex(out, 32, hex);
    String j = "{\"sha256\":\"";
    j += hex;
    j += "\"}";
    server.send(200, "application/json", j);
  });
}

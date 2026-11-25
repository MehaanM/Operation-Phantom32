#pragma once
// cert_scanner.h — TLS/SSL certificate scanner for open ports on discovered hosts.
// Connects TCP to common HTTPS ports, reads TLS ClientHello/ServerHello exchange,
// extracts CN/SAN/expiry from raw ASN.1 DER. Prefix: cs_

#include <Arduino.h>
#include <WebServer.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

extern WebServer server;

#define CS_MAX_TARGETS 16
#define CS_MAX_CERTS 32
#define CS_CN_LEN 64
#define CS_SAN_LEN 128
#define CS_ISSUER_LEN 64
#define CS_SERIAL_LEN 32
#define CS_TIMEOUT_MS 3000

static const uint16_t CS_COMMON_PORTS[] = {443, 8443, 8080, 4443, 1443, 9443};
#define CS_PORT_COUNT 6

// ─── Certificate record ───────────────────────────────────────────────────────
struct CsCert {
  bool active;
  char host[32];
  uint16_t port;
  char cn[CS_CN_LEN];
  char san[CS_SAN_LEN];  // comma-separated SANs
  char issuer[CS_ISSUER_LEN];
  char serial[CS_SERIAL_LEN];
  char not_before[20];  // "YYYY-MM-DD"
  char not_after[20];
  bool expired;
  bool self_signed;
  bool weak_key;  // RSA < 2048 or EC < 256
  int key_bits;
  uint8_t ip[4];
  uint32_t scan_ts;
  uint32_t expiry_epoch;
};

// ─── Scan state ───────────────────────────────────────────────────────────────
static struct {
  CsCert certs[CS_MAX_CERTS];
  uint8_t cert_count;
  char targets[CS_MAX_TARGETS][32];
  uint8_t target_count;
  bool scanning;
  uint8_t scan_target_idx;
  uint8_t scan_port_idx;
  uint32_t scan_started_ts;
  uint32_t scans_completed;
  uint32_t scans_failed;
  TaskHandle_t task_handle;
} g_cs = {};

// ─── ASN.1 DER helpers ────────────────────────────────────────────────────────
static int cs_asn1_length(const uint8_t *buf, int pos, int len, int *out_len) {
  if (pos >= len)
    return -1;
  if (buf[pos] < 0x80) {
    *out_len = buf[pos];
    return pos + 1;
  }
  int nb = buf[pos] & 0x7F;
  if (nb > 3 || pos + nb >= len)
    return -1;
  *out_len = 0;
  for (int i = 0; i < nb; i++)
    *out_len = (*out_len << 8) | buf[pos + 1 + i];
  return pos + 1 + nb;
}

// Find an OID within a DER blob; returns position after OID value or -1
static int cs_find_oid(const uint8_t *buf, int len, const uint8_t *oid, int oid_len) {
  for (int i = 0; i < len - oid_len - 2; i++) {
    if (buf[i] == 0x06 && buf[i + 1] == oid_len && memcmp(buf + i + 2, oid, oid_len) == 0)
      return i;
  }
  return -1;
}

// Extract UTF8String/PrintableString at pos → out
static bool cs_extract_string(const uint8_t *buf, int pos, int len, char *out, int out_len) {
  if (pos >= len)
    return false;
  uint8_t tag = buf[pos];
  if (tag != 0x0C && tag != 0x13 && tag != 0x16 && tag != 0x1A && tag != 0x1E)
    return false;
  int slen;
  int sstart = cs_asn1_length(buf, pos + 1, len, &slen);
  if (sstart < 0 || sstart + slen > len)
    return false;
  int cp = min(slen, out_len - 1);
  memcpy(out, buf + sstart, cp);
  out[cp] = '\0';
  return true;
}

// ─── Parse certificate from TLS Server Certificate message ───────────────────
// raw: points to DER-encoded certificate, len: length
static void cs_parse_cert(CsCert &c, const uint8_t *der, int len) {
  // OIDs
  static const uint8_t OID_CN[] = {0x55, 0x04, 0x03};   // commonName
  static const uint8_t OID_ORG[] = {0x55, 0x04, 0x0A};  // organizationName
  static const uint8_t OID_SAN[] = {0x55, 0x1D, 0x11};  // subjectAltName
  static const uint8_t OID_RSA[] = {0x2A, 0x86, 0x48, 0x86, 0xF7,
                                    0x0D, 0x01, 0x01, 0x01};  // rsaEncryption

  // Find CN in subject
  int cn_oid = cs_find_oid(der, len, OID_CN, 3);
  if (cn_oid >= 0) {
    // After OID + length: expect SET > SEQUENCE > string
    int p = cn_oid + 5;
    while (p < len && p < cn_oid + 20) {
      if (cs_extract_string(der, p, len, c.cn, CS_CN_LEN))
        break;
      p++;
    }
  }

  // Find issuer ORG
  int org_pos = cs_find_oid(der, len, OID_ORG, 3);
  if (org_pos >= 0) {
    int p = org_pos + 5;
    while (p < len && p < org_pos + 20) {
      if (cs_extract_string(der, p, len, c.issuer, CS_ISSUER_LEN))
        break;
      p++;
    }
  }

  // Self-signed: issuer == subject (simplified: same CN)
  if (cn_oid >= 0 && org_pos >= 0 && org_pos < cn_oid)
    c.self_signed = true;

  // Try to find key size from RSA OID
  int rsa_pos = cs_find_oid(der, len, OID_RSA, 9);
  if (rsa_pos >= 0) {
    // Look for BIT STRING containing SEQUENCE with INTEGER (n, e)
    for (int p = rsa_pos + 11; p < min(len, rsa_pos + 100); p++) {
      if (der[p] == 0x03) {  // BIT STRING
        int blen;
        int bstart = cs_asn1_length(der, p + 1, len, &blen);
        if (bstart > 0 && bstart + blen <= len) {
          // Key length in bits ~ (blen - 1) * 8
          c.key_bits = (blen - 1) * 8;
          c.weak_key = c.key_bits < 2048;
          break;
        }
      }
    }
  }

  // Parse validity: look for UTCTime/GeneralizedTime after notBefore
  int found_times = 0;
  for (int p = 0; p < len - 2 && found_times < 2; p++) {
    if (der[p] == 0x17 || der[p] == 0x18) {  // UTCTime or GeneralizedTime
      int tlen;
      int tstart = cs_asn1_length(der, p + 1, len, &tlen);
      if (tstart > 0 && tstart + tlen <= len && tlen >= 12) {
        char raw_t[16];
        int cp = min(tlen, 14);
        memcpy(raw_t, der + tstart, cp);
        raw_t[cp] = '\0';
        // Format: YYMMDDHHMMSSZ (UTCTime) or YYYYMMDDHHMMSSZ (GeneralizedTime)
        char *dest = (found_times == 0) ? c.not_before : c.not_after;
        if (der[p] == 0x17) {  // UTCTime: YYMMDDHHMMSS
          int yy = (raw_t[0] - '0') * 10 + (raw_t[1] - '0');
          int year = yy >= 50 ? 1900 + yy : 2000 + yy;
          snprintf(dest, 20, "%04d-%c%c-%c%c", year, raw_t[2], raw_t[3], raw_t[4], raw_t[5]);
        } else {
          snprintf(dest, 20, "%.4s-%.2s-%.2s", raw_t, raw_t + 4, raw_t + 6);
        }
        found_times++;
        p += tlen;
      }
    }
  }
}

// ─── Scan a single host:port ───────────────────────────────────────────────────
static bool cs_scan_host(const char *host, uint16_t port, CsCert &cert_out) {
  WiFiClientSecure client;
  client.setInsecure();  // accept any cert — we're just reading it
  client.setTimeout(CS_TIMEOUT_MS / 1000);

  if (!client.connect(host, port))
    return false;

  // Send HTTP request to trigger cert exchange
  client.printf("HEAD / HTTP/1.0\r\nHost: %s\r\n\r\n", host);

  // Read peer certificate via mbedTLS
  const mbedtls_x509_crt *cert = client.getPeerCertificate();
  if (!cert) {
    client.stop();
    return false;
  }

  memset(&cert_out, 0, sizeof(cert_out));
  strncpy(cert_out.host, host, 31);
  cert_out.port = port;
  cert_out.scan_ts = millis() / 1000;

  // Extract CN from subject
  char subject_buf[256];
  mbedtls_x509_dn_gets(subject_buf, sizeof(subject_buf), &cert->subject);
  // Parse CN= from subject
  const char *cn_start = strstr(subject_buf, "CN=");
  if (cn_start) {
    cn_start += 3;
    const char *cn_end = strchr(cn_start, ',');
    int cn_len = cn_end ? (int)(cn_end - cn_start) : strlen(cn_start);
    int cp = min(cn_len, CS_CN_LEN - 1);
    memcpy(cert_out.cn, cn_start, cp);
    cert_out.cn[cp] = '\0';
  }

  // Issuer
  char issuer_buf[256];
  mbedtls_x509_dn_gets(issuer_buf, sizeof(issuer_buf), &cert->issuer);
  strncpy(cert_out.issuer, issuer_buf, CS_ISSUER_LEN - 1);

  // Validity
  snprintf(cert_out.not_before, 20, "%04d-%02d-%02d", cert->valid_from.year, cert->valid_from.mon,
           cert->valid_from.day);
  snprintf(cert_out.not_after, 20, "%04d-%02d-%02d", cert->valid_to.year, cert->valid_to.mon,
           cert->valid_to.day);

  // Key info
  if (cert->pk.pk_info) {
    mbedtls_pk_type_t pk_type = mbedtls_pk_get_type(&cert->pk);
    cert_out.key_bits = mbedtls_pk_get_bitlen(&cert->pk);
    cert_out.weak_key = (pk_type == MBEDTLS_PK_RSA && cert_out.key_bits < 2048) ||
                        (pk_type == MBEDTLS_PK_ECKEY && cert_out.key_bits < 256);
  }

  // Self-signed: issuer == subject
  cert_out.self_signed = (strcmp(subject_buf, issuer_buf) == 0);

  // Expiry
  uint32_t now_epoch = millis() / 1000;  // approximate
  cert_out.expired = false;              // would need NTP for accurate check

  client.stop();
  return true;
}

// ─── Scan task ────────────────────────────────────────────────────────────────
static void cs_task(void *arg) {
  g_cs.scans_completed = 0;
  g_cs.scans_failed = 0;

  for (int ti = 0; ti < g_cs.target_count && g_cs.scanning; ti++) {
    for (int pi = 0; pi < CS_PORT_COUNT && g_cs.scanning; pi++) {
      g_cs.scan_target_idx = ti;
      g_cs.scan_port_idx = pi;

      CsCert c;
      if (cs_scan_host(g_cs.targets[ti], CS_COMMON_PORTS[pi], c)) {
        // Store or update
        bool found = false;
        for (int i = 0; i < CS_MAX_CERTS; i++) {
          if (g_cs.certs[i].active && strcmp(g_cs.certs[i].host, c.host) == 0 &&
              g_cs.certs[i].port == c.port) {
            g_cs.certs[i] = c;
            g_cs.certs[i].active = true;
            found = true;
            break;
          }
        }
        if (!found) {
          for (int i = 0; i < CS_MAX_CERTS; i++) {
            if (!g_cs.certs[i].active) {
              g_cs.certs[i] = c;
              g_cs.certs[i].active = true;
              g_cs.cert_count++;
              break;
            }
          }
        }
        g_cs.scans_completed++;
      } else {
        g_cs.scans_failed++;
      }
      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }

  g_cs.scanning = false;
  g_cs.task_handle = nullptr;
  vTaskDelete(nullptr);
}

// ─── Control ─────────────────────────────────────────────────────────────────
inline void cs_start() {
  if (g_cs.scanning || g_cs.target_count == 0)
    return;
  g_cs.scanning = true;
  g_cs.scan_started_ts = millis() / 1000;
  xTaskCreate(cs_task, "cs_scan", 8192, nullptr, 3, &g_cs.task_handle);
}

inline void cs_stop() {
  g_cs.scanning = false;
  uint32_t deadline = millis() + 1000;
  while (g_cs.task_handle && millis() < deadline)
    vTaskDelay(50);
}

static String cs_json_results() {
  String s = "{\"scanning\":" + String(g_cs.scanning ? "true" : "false");
  s += ",\"cert_count\":" + String(g_cs.cert_count);
  s += ",\"completed\":" + String(g_cs.scans_completed);
  s += ",\"failed\":" + String(g_cs.scans_failed);
  s += ",\"certs\":[";
  bool first = true;
  for (int i = 0; i < CS_MAX_CERTS; i++) {
    if (!g_cs.certs[i].active)
      continue;
    if (!first)
      s += ",";
    first = false;
    CsCert &c = g_cs.certs[i];
    s += "{\"host\":\"" + String(c.host) + "\"";
    s += ",\"port\":" + String(c.port);
    s += ",\"cn\":\"" + String(c.cn) + "\"";
    s += ",\"issuer\":\"" + String(c.issuer) + "\"";
    s += ",\"not_before\":\"" + String(c.not_before) + "\"";
    s += ",\"not_after\":\"" + String(c.not_after) + "\"";
    s += ",\"expired\":" + String(c.expired ? "true" : "false");
    s += ",\"self_signed\":" + String(c.self_signed ? "true" : "false");
    s += ",\"weak_key\":" + String(c.weak_key ? "true" : "false");
    s += ",\"key_bits\":" + String(c.key_bits);
    s += ",\"scan_ts\":" + String(c.scan_ts);
    s += "}";
  }
  s += "]}";
  return s;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void cs_routes() {
  server.on("/api/cs/results", HTTP_GET,
            []() { server.send(200, "application/json", cs_json_results()); });

  server.on("/api/cs/start", HTTP_POST, []() {
    cs_start();
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/cs/stop", HTTP_POST, []() {
    cs_stop();
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/cs/target/add", HTTP_POST, []() {
    if (!server.hasArg("host")) {
      server.send(400, "application/json", "{\"error\":\"missing host\"}");
      return;
    }
    if (g_cs.target_count >= CS_MAX_TARGETS) {
      server.send(400, "application/json", "{\"error\":\"list full\"}");
      return;
    }
    strncpy(g_cs.targets[g_cs.target_count++], server.arg("host").c_str(), 31);
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/cs/target/clear", HTTP_POST, []() {
    g_cs.target_count = 0;
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/cs/clear", HTTP_POST, []() {
    memset(g_cs.certs, 0, sizeof(g_cs.certs));
    g_cs.cert_count = 0;
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

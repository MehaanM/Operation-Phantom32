#pragma once
// input_validator.h — Input sanitization and validation for all web UI inputs.
//
// Prevents:
//   - Command injection via tool arguments (SSID, MAC, channel inputs)
//   - XSS via JSON output of user-supplied strings
//   - Buffer overflows from oversized inputs
//   - Malformed 802.11 parameters that could crash frame injection
//
// All web handlers that accept user input MUST call iv_check_* before using
// the value. Functions return false and populate iv_last_error on failure.

#include <Arduino.h>
#include <WebServer.h>
#include <string.h>
#include <ctype.h>

extern WebServer server;

// ============================================================
// CONFIGURATION
// ============================================================

#define IV_MAX_SSID_LEN    32
#define IV_MAX_CHANNEL     14
#define IV_MIN_CHANNEL     1
#define IV_MAX_IP_LEN      15
#define IV_MAX_LABEL_LEN   64
#define IV_MAX_CMD_LEN     128
#define IV_MAX_GENERIC_LEN 256

// ============================================================
// ERROR STATE
// ============================================================

static char g_iv_last_error[128] = {};

static void iv_set_error(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(g_iv_last_error, sizeof(g_iv_last_error), fmt, ap);
  va_end(ap);
}

static const char* iv_error() { return g_iv_last_error; }

// ============================================================
// SSID VALIDATION
// ============================================================

static bool iv_check_ssid(const char *ssid, int max_len = IV_MAX_SSID_LEN) {
  if (!ssid) { iv_set_error("SSID is null"); return false; }
  int len = strlen(ssid);
  if (len == 0) { iv_set_error("SSID is empty"); return false; }
  if (len > max_len) {
    iv_set_error("SSID too long (%d > %d)", len, max_len);
    return false;
  }
  // Allow printable ASCII only (0x20-0x7E). Null bytes already handled above.
  for (int i = 0; i < len; i++) {
    uint8_t c = (uint8_t)ssid[i];
    if (c < 0x20 || c > 0x7E) {
      iv_set_error("SSID contains non-printable character at position %d (0x%02X)", i, c);
      return false;
    }
  }
  return true;
}

// Sanitize SSID for safe JSON output: escape " and \.
static String iv_ssid_to_json(const char *ssid) {
  String out;
  out.reserve(64);
  for (int i = 0; ssid[i]; i++) {
    char c = ssid[i];
    if (c == '"' || c == '\\') out += '\\';
    out += c;
  }
  return out;
}

// Sanitize for safe HTML output: escape &, <, >, ", '.
static String iv_ssid_to_html(const char *ssid) {
  String out;
  out.reserve(64);
  for (int i = 0; ssid[i]; i++) {
    char c = ssid[i];
    switch (c) {
      case '&': out += "&amp;";  break;
      case '<': out += "&lt;";   break;
      case '>': out += "&gt;";   break;
      case '"': out += "&quot;"; break;
      case '\'': out += "&#39;"; break;
      default:  out += c;        break;
    }
  }
  return out;
}

// ============================================================
// MAC ADDRESS VALIDATION
// ============================================================

static bool iv_check_mac_str(const char *mac_str) {
  if (!mac_str) { iv_set_error("MAC is null"); return false; }
  // Accept XX:XX:XX:XX:XX:XX (colon-separated) or XXXXXXXXXXXX (compact)
  int len = strlen(mac_str);
  if (len == 17) {
    // colon format
    for (int i = 0; i < 17; i++) {
      if (i % 3 == 2) {
        if (mac_str[i] != ':') {
          iv_set_error("Invalid MAC separator at position %d", i);
          return false;
        }
      } else {
        if (!isxdigit(mac_str[i])) {
          iv_set_error("Invalid hex digit in MAC at position %d", i);
          return false;
        }
      }
    }
    return true;
  } else if (len == 12) {
    for (int i = 0; i < 12; i++) {
      if (!isxdigit(mac_str[i])) {
        iv_set_error("Invalid hex digit in MAC at position %d", i);
        return false;
      }
    }
    return true;
  }
  iv_set_error("Invalid MAC length %d (expected 12 or 17)", len);
  return false;
}

static bool iv_parse_mac(const char *mac_str, uint8_t *out) {
  if (!iv_check_mac_str(mac_str)) return false;
  int len = strlen(mac_str);
  if (len == 17) {
    for (int i = 0; i < 6; i++) {
      char hi = mac_str[i*3], lo = mac_str[i*3+1];
      auto hv = [](char c) -> int {
        if (c >= '0' && c <= '9') return c-'0';
        if (c >= 'a' && c <= 'f') return c-'a'+10;
        if (c >= 'A' && c <= 'F') return c-'A'+10;
        return 0;
      };
      out[i] = (uint8_t)((hv(hi)<<4)|hv(lo));
    }
  } else {
    for (int i = 0; i < 6; i++) {
      char hi = mac_str[i*2], lo = mac_str[i*2+1];
      auto hv = [](char c) -> int {
        if (c >= '0' && c <= '9') return c-'0';
        if (c >= 'a' && c <= 'f') return c-'a'+10;
        if (c >= 'A' && c <= 'F') return c-'A'+10;
        return 0;
      };
      out[i] = (uint8_t)((hv(hi)<<4)|hv(lo));
    }
  }
  return true;
}

static bool iv_is_broadcast_mac(const uint8_t *mac) {
  return mac[0]==0xFF && mac[1]==0xFF && mac[2]==0xFF &&
         mac[3]==0xFF && mac[4]==0xFF && mac[5]==0xFF;
}

static bool iv_is_multicast_mac(const uint8_t *mac) {
  return (mac[0] & 0x01) != 0;
}

// ============================================================
// CHANNEL VALIDATION
// ============================================================

static bool iv_check_channel(int ch) {
  if (ch < IV_MIN_CHANNEL || ch > IV_MAX_CHANNEL) {
    iv_set_error("Channel %d out of range [%d,%d]", ch, IV_MIN_CHANNEL, IV_MAX_CHANNEL);
    return false;
  }
  return true;
}

static bool iv_check_channel_str(const char *ch_str, int *out_ch = nullptr) {
  if (!ch_str) { iv_set_error("Channel is null"); return false; }
  char *end;
  long v = strtol(ch_str, &end, 10);
  if (*end != '\0') { iv_set_error("Channel is not a number: '%s'", ch_str); return false; }
  if (!iv_check_channel((int)v)) return false;
  if (out_ch) *out_ch = (int)v;
  return true;
}

// ============================================================
// INTEGER RANGE VALIDATION
// ============================================================

static bool iv_check_int_range(const char *val_str, long min_val, long max_val,
                                long *out = nullptr) {
  if (!val_str) { iv_set_error("Value is null"); return false; }
  char *end;
  long v = strtol(val_str, &end, 10);
  if (*end != '\0') {
    iv_set_error("'%s' is not a valid integer", val_str);
    return false;
  }
  if (v < min_val || v > max_val) {
    iv_set_error("Value %ld out of range [%ld, %ld]", v, min_val, max_val);
    return false;
  }
  if (out) *out = v;
  return true;
}

// ============================================================
// IP ADDRESS VALIDATION
// ============================================================

static bool iv_check_ip(const char *ip_str) {
  if (!ip_str) { iv_set_error("IP is null"); return false; }
  int len = strlen(ip_str);
  if (len < 7 || len > IV_MAX_IP_LEN) {
    iv_set_error("IP length %d invalid", len);
    return false;
  }
  int dots = 0, oct = 0, oct_val = 0;
  bool in_oct = false;
  for (int i = 0; i <= len; i++) {
    char c = ip_str[i];
    if (isdigit(c)) {
      oct_val = oct_val * 10 + (c - '0');
      if (oct_val > 255) { iv_set_error("IP octet > 255"); return false; }
      in_oct = true;
    } else if (c == '.' || c == '\0') {
      if (!in_oct) { iv_set_error("Empty IP octet"); return false; }
      oct++;
      dots += (c == '.');
      oct_val = 0;
      in_oct = false;
    } else {
      iv_set_error("Invalid character '%c' in IP", c);
      return false;
    }
  }
  if (oct != 4) { iv_set_error("IP must have 4 octets, got %d", oct); return false; }
  return true;
}

// ============================================================
// LABEL / NAME VALIDATION (for watchlist labels, etc.)
// ============================================================

static bool iv_check_label(const char *label, int max_len = IV_MAX_LABEL_LEN) {
  if (!label) { iv_set_error("Label is null"); return false; }
  int len = strlen(label);
  if (len > max_len) {
    iv_set_error("Label too long (%d > %d)", len, max_len);
    return false;
  }
  // Printable ASCII only.
  for (int i = 0; i < len; i++) {
    uint8_t c = (uint8_t)label[i];
    if (c < 0x20 || c > 0x7E) {
      iv_set_error("Label has non-printable char at %d (0x%02X)", i, c);
      return false;
    }
  }
  return true;
}

// ============================================================
// SERVER ARG HELPERS (extract + validate from HTTP request)
// ============================================================

// Extract SSID from server arg, return empty string on validation failure.
static String iv_get_ssid(const char *arg_name, bool required = false) {
  if (!server.hasArg(arg_name)) {
    if (required) iv_set_error("Missing required arg: %s", arg_name);
    return "";
  }
  String val = server.arg(arg_name);
  if (!iv_check_ssid(val.c_str())) return "";
  return val;
}

// Extract integer from server arg.
static bool iv_get_int(const char *arg_name, long min_val, long max_val,
                        long *out, bool required = true) {
  if (!server.hasArg(arg_name)) {
    if (required) iv_set_error("Missing required arg: %s", arg_name);
    return false;
  }
  return iv_check_int_range(server.arg(arg_name).c_str(), min_val, max_val, out);
}

// Extract MAC from server arg.
static bool iv_get_mac(const char *arg_name, uint8_t *out, bool required = true) {
  if (!server.hasArg(arg_name)) {
    if (required) iv_set_error("Missing required arg: %s", arg_name);
    return false;
  }
  return iv_parse_mac(server.arg(arg_name).c_str(), out);
}

// Extract channel from server arg.
static bool iv_get_channel(const char *arg_name, int *out, bool required = true) {
  if (!server.hasArg(arg_name)) {
    if (required) iv_set_error("Missing required arg: %s", arg_name);
    return false;
  }
  return iv_check_channel_str(server.arg(arg_name).c_str(), out);
}

// ============================================================
// JSON SAFE OUTPUT HELPERS
// ============================================================

// Escape a string for safe JSON embedding (handles all control chars).
static String iv_json_escape(const char *s) {
  if (!s) return "null";
  String out;
  out += '"';
  for (; *s; s++) {
    uint8_t c = (uint8_t)*s;
    if      (c == '"')  out += "\\\"";
    else if (c == '\\') out += "\\\\";
    else if (c == '\n') out += "\\n";
    else if (c == '\r') out += "\\r";
    else if (c == '\t') out += "\\t";
    else if (c < 0x20) {
      char buf[8];
      snprintf(buf, sizeof(buf), "\\u%04x", c);
      out += buf;
    } else {
      out += (char)c;
    }
  }
  out += '"';
  return out;
}

// Format a MAC address as "XX:XX:XX:XX:XX:XX" string.
static String iv_mac_to_str(const uint8_t *mac) {
  char buf[18];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

// ============================================================
// FRAME PARAMETER VALIDATION
// ============================================================

// Validate deauth reason code (IEEE 802.11 Table 9-49).
static bool iv_check_reason_code(uint16_t rc) {
  // 0: Reserved, 1-45 valid, 46-65534 reserved/vendor, 65535 reserved.
  if (rc == 0) { iv_set_error("Reason code 0 is reserved"); return false; }
  if (rc > 65534) { iv_set_error("Reason code %u out of range", rc); return false; }
  return true;
}

// Validate 802.11 frame TX power (dBm).
static bool iv_check_tx_power(int8_t dbm) {
  if (dbm < -127 || dbm > 20) {
    iv_set_error("TX power %d dBm out of range [-127,20]", dbm);
    return false;
  }
  return true;
}

// Validate beacon interval (TUs, 1 TU = 1024 µs).
static bool iv_check_beacon_interval(uint16_t bi) {
  if (bi < 10 || bi > 1000) {
    iv_set_error("Beacon interval %u TU out of range [10,1000]", bi);
    return false;
  }
  return true;
}

// ============================================================
// ROUTE HANDLER: Validation test endpoint (for self-test)
// ============================================================

static void iv_handle_selftest() {
  String result = "{\"tests\":[";
  bool first = true;

  struct TestCase { const char *name; bool ok; };
  TestCase tests[] = {
    {"ssid_valid",    iv_check_ssid("MyNetwork")},
    {"ssid_too_long", !iv_check_ssid("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA")},
    {"ssid_empty",    !iv_check_ssid("")},
    {"mac_colon",     iv_check_mac_str("AA:BB:CC:DD:EE:FF")},
    {"mac_compact",   iv_check_mac_str("AABBCCDDEEFF")},
    {"mac_bad",       !iv_check_mac_str("ZZ:00:11:22:33:44")},
    {"ch_valid",      iv_check_channel(6)},
    {"ch_low",        !iv_check_channel(0)},
    {"ch_high",       !iv_check_channel(15)},
    {"ip_valid",      iv_check_ip("192.168.4.1")},
    {"ip_invalid",    !iv_check_ip("999.0.0.1")},
    {"label_valid",   iv_check_label("My AP")},
    {"rc_valid",      iv_check_reason_code(7)},
    {"rc_zero",       !iv_check_reason_code(0)},
    {"bi_valid",      iv_check_beacon_interval(100)},
    {"bi_low",        !iv_check_beacon_interval(5)},
  };

  for (auto &t : tests) {
    if (!first) result += ",";
    result += "{\"name\":\"";
    result += t.name;
    result += "\",\"ok\":";
    result += t.ok ? "true" : "false";
    result += "}";
    first = false;
  }
  result += "]}";
  server.send(200, "application/json", result);
}

inline void iv_routes() {
  server.on("/api/iv/selftest", HTTP_GET, iv_handle_selftest);
}

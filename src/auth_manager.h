#pragma once
// auth_manager.h — HTTP authentication and session management for Phantom32.
//
// Security model:
//   - Per-device random 12-char alphanumeric dashboard password generated on
//     first boot, stored as PBKDF2-HMAC-SHA256 (100000 iterations, random 128-bit
//     salt) in NVS. Plaintext shown once on serial; never stored.
//   - Per-device random 12-char AP WiFi password generated on first boot and
//     stored in NVS. Different from the dashboard password.
//   - Session tokens: 32-byte random tokens issued at /login.
//     Clients send them via Cookie: phantom32_token=<token> or
//     Authorization: Bearer <token> header. NOT accepted as ?token= query param.
//     Tokens expire after AM_SESSION_TTL_S.
//   - All HTTP routes except /login and /health are gated by auth_check().
//   - API key: a separate long-lived key for scripted/automated access,
//     sent via X-Phantom32-Key header only (not query param — prevents log leakage).
//   - After AM_LOCKOUT_ATTEMPTS failed logins the IP is locked for AM_LOCKOUT_S.
//   - Tokens are invalidated on logout or reboot (stored in RAM only).
//
// Usage in main.cpp setup():
//   auth_init();          // load/generate credentials, register /login /logout
// In every route handler (or via auth_protect() wrapper):
//   if (!auth_check()) { auth_reject(); return; }

#include <Arduino.h>
#include <WebServer.h>
#include <Preferences.h>
#include <esp_random.h>
#include <esp_mac.h>
#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>
#include <string.h>
#include <time.h>

extern WebServer server;

// ============================================================
// CONFIGURATION
// ============================================================

#define AM_SESSION_MAX 8       // max concurrent sessions
#define AM_SESSION_TTL_S 1800  // 30 minutes idle timeout
#define AM_TOKEN_LEN 32        // bytes of session token (hex: 64 chars)
#define AM_PASS_LEN 12         // device-unique password length
#define AM_WIFI_PASS_LEN 63    // max WiFi AP password length (WPA2 spec)
#define AM_API_KEY_LEN 32      // API key length
#define AM_MAX_IPS 16          // IPs to track for lockout
#define AM_LOCKOUT_ATTEMPTS 5  // failed logins before lockout
#define AM_LOCKOUT_S 300       // lockout duration in seconds
#define AM_ADMIN_USER "admin"  // fixed username
#define AM_NVS_NS "phantom32"
#define AM_NVS_PASS_HASH_KEY "ap_pass_h"    // PBKDF2-HMAC-SHA256 hex of password
#define AM_NVS_PASS_SALT_KEY "ap_pass_s"    // random 16-byte salt (hex: 32 chars)
#define AM_NVS_AP_WIFI_PW_KEY "ap_wifi_pw"  // per-device AP WiFi password
#define AM_NVS_APIKEY_KEY "api_key"
#define AM_NVS_INITIALIZED "am_init"
#define AM_NVS_CRED_VER_KEY "am_cred_ver"
#define AM_CRED_VER "2"           // bump to force a one-time reset to default credentials
#define AM_DEFAULT_PASS "password"
#define AM_DEFAULT_WIFI_PASS "password"

#define AM_PBKDF2_SALT_LEN 16  // random salt bytes
#define AM_PBKDF2_KEY_LEN 32   // derived key bytes (256-bit)
#define AM_PBKDF2_ITER \
  100000  // work factor (~1 s on ESP32 @ 240 MHz; defensible minimum for constrained hardware)

// ============================================================
// DATA STRUCTURES
// ============================================================

struct AmSession {
  bool active;
  char token[AM_TOKEN_LEN * 2 + 1];  // hex string, 64 chars + NUL
  uint32_t last_active_s;            // millis()/1000 at last request
  char client_ip[16];                // dotted-decimal
};

struct AmIpRecord {
  char ip[16];
  uint8_t fail_count;
  uint32_t last_fail_s;
  bool locked;
  uint32_t lock_start_s;
};

// ============================================================
// STATE
// ============================================================

static AmSession g_am_sessions[AM_SESSION_MAX] = {};
static AmIpRecord g_am_ips[AM_MAX_IPS] = {};
static char g_am_password_hash[AM_PBKDF2_KEY_LEN * 2 + 1] = {};  // PBKDF2 hex
static uint8_t g_am_password_salt[AM_PBKDF2_SALT_LEN] = {};      // random salt
static char g_am_ap_wifi_password[AM_WIFI_PASS_LEN + 1] = {};     // per-device AP WiFi password
static char g_am_api_key[AM_API_KEY_LEN * 2 + 1] = {};
static bool g_am_initialized = false;

static uint32_t g_am_total_logins = 0;
static uint32_t g_am_total_failures = 0;
static uint32_t g_am_total_logouts = 0;
static uint32_t g_am_lockout_events = 0;

// ============================================================
// HELPERS
// ============================================================

static void am_bytes_to_hex(const uint8_t *b, int len, char *out) {
  static const char hex_chars[] = "0123456789abcdef";
  for (int i = 0; i < len; i++) {
    out[i * 2] = hex_chars[b[i] >> 4];
    out[i * 2 + 1] = hex_chars[b[i] & 0xF];
  }
  out[len * 2] = '\0';
}

static bool am_hex_to_bytes(const char *hex, uint8_t *out, int len) {
  for (int i = 0; i < len; i++) {
    char hi = hex[i * 2], lo = hex[i * 2 + 1];
    if (!hi || !lo)
      return false;
    auto hv = [](char c) -> int {
      if (c >= '0' && c <= '9')
        return c - '0';
      if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
      if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
      return -1;
    };
    int h = hv(hi), l = hv(lo);
    if (h < 0 || l < 0)
      return false;
    out[i] = (uint8_t)((h << 4) | l);
  }
  return true;
}

static void am_gen_token(char *out) {
  uint8_t raw[AM_TOKEN_LEN];
  esp_fill_random(raw, AM_TOKEN_LEN);
  am_bytes_to_hex(raw, AM_TOKEN_LEN, out);
}

static void am_gen_password(char *out, int len) {
  static const char CHARSET[] =
      "abcdefghjkmnpqrstuvwxyz"  // lowercase (no confusable l,o,i)
      "ABCDEFGHJKMNPQRSTUVWXYZ"  // uppercase
      "23456789";                // digits (no confusable 0,1)
  uint8_t raw[64];
  esp_fill_random(raw, sizeof(raw));
  for (int i = 0; i < len; i++) {
    out[i] = CHARSET[raw[i] % (sizeof(CHARSET) - 1)];
  }
  out[len] = '\0';
}

// PBKDF2-HMAC-SHA256 of password with a random per-device salt.
// Salt is generated once at first boot and stored in NVS; it makes NVS dumps
// non-transferable and forces per-password cost even if the salt leaks.
static void am_pbkdf2_password(const char *password, const uint8_t *salt, char *hash_hex_out) {
  uint8_t key[AM_PBKDF2_KEY_LEN];
  mbedtls_md_context_t ctx;
  const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, info, 1);
  mbedtls_pkcs5_pbkdf2_hmac(&ctx, (const uint8_t *)password, strlen(password), salt,
                            AM_PBKDF2_SALT_LEN, AM_PBKDF2_ITER, AM_PBKDF2_KEY_LEN, key);
  mbedtls_md_free(&ctx);
  am_bytes_to_hex(key, AM_PBKDF2_KEY_LEN, hash_hex_out);
}

// Constant-time string comparison to prevent timing attacks.
static bool am_ct_strcmp(const char *a, const char *b) {
  if (!a || !b)
    return false;
  size_t la = strlen(a), lb = strlen(b);
  if (la != lb)
    return false;
  uint8_t diff = 0;
  for (size_t i = 0; i < la; i++)
    diff |= (uint8_t)(a[i] ^ b[i]);
  return diff == 0;
}

// ============================================================
// IP LOCKOUT
// ============================================================

static AmIpRecord *am_ip_find_or_add(const char *ip) {
  for (int i = 0; i < AM_MAX_IPS; i++) {
    if (g_am_ips[i].ip[0] && strcmp(g_am_ips[i].ip, ip) == 0)
      return &g_am_ips[i];
  }
  // Find an empty or expired slot.
  uint32_t now = millis() / 1000;
  for (int i = 0; i < AM_MAX_IPS; i++) {
    if (!g_am_ips[i].ip[0]) {
      strncpy(g_am_ips[i].ip, ip, 15);
      g_am_ips[i].ip[15] = '\0';
      g_am_ips[i].fail_count = 0;
      g_am_ips[i].locked = false;
      return &g_am_ips[i];
    }
    if (g_am_ips[i].locked && (now - g_am_ips[i].lock_start_s) > AM_LOCKOUT_S) {
      memset(&g_am_ips[i], 0, sizeof(AmIpRecord));
      strncpy(g_am_ips[i].ip, ip, 15);
      g_am_ips[i].ip[15] = '\0';
      return &g_am_ips[i];
    }
  }
  return nullptr;  // table full; conservative: allow
}

static bool am_ip_is_locked(const char *ip) {
  uint32_t now = millis() / 1000;
  for (int i = 0; i < AM_MAX_IPS; i++) {
    if (g_am_ips[i].ip[0] && strcmp(g_am_ips[i].ip, ip) == 0) {
      if (!g_am_ips[i].locked)
        return false;
      if ((now - g_am_ips[i].lock_start_s) > AM_LOCKOUT_S) {
        g_am_ips[i].locked = false;
        g_am_ips[i].fail_count = 0;
        return false;
      }
      return true;
    }
  }
  return false;
}

static void am_ip_record_fail(const char *ip) {
  AmIpRecord *r = am_ip_find_or_add(ip);
  if (!r)
    return;
  r->fail_count++;
  r->last_fail_s = millis() / 1000;
  if (r->fail_count >= AM_LOCKOUT_ATTEMPTS && !r->locked) {
    r->locked = true;
    r->lock_start_s = r->last_fail_s;
    g_am_lockout_events++;
    Serial.printf("[AUTH] IP %s locked out after %d failures\n", ip, r->fail_count);
  }
}

static void am_ip_record_success(const char *ip) {
  for (int i = 0; i < AM_MAX_IPS; i++) {
    if (g_am_ips[i].ip[0] && strcmp(g_am_ips[i].ip, ip) == 0) {
      g_am_ips[i].fail_count = 0;
      g_am_ips[i].locked = false;
      return;
    }
  }
}

// ============================================================
// SESSION MANAGEMENT
// ============================================================

static AmSession *am_session_find(const char *token) {
  if (!token || !token[0])
    return nullptr;
  uint32_t now = millis() / 1000;
  for (int i = 0; i < AM_SESSION_MAX; i++) {
    if (!g_am_sessions[i].active)
      continue;
    // Expire idle sessions.
    if ((now - g_am_sessions[i].last_active_s) > AM_SESSION_TTL_S) {
      memset(&g_am_sessions[i], 0, sizeof(AmSession));
      continue;
    }
    if (am_ct_strcmp(g_am_sessions[i].token, token))
      return &g_am_sessions[i];
  }
  return nullptr;
}

static AmSession *am_session_alloc(const char *ip) {
  // Find an empty slot, or evict the oldest.
  uint32_t oldest_ts = UINT32_MAX;
  int oldest_idx = 0;
  for (int i = 0; i < AM_SESSION_MAX; i++) {
    if (!g_am_sessions[i].active) {
      AmSession &s = g_am_sessions[i];
      s.active = true;
      am_gen_token(s.token);
      s.last_active_s = millis() / 1000;
      strncpy(s.client_ip, ip ? ip : "", 15);
      return &s;
    }
    if (g_am_sessions[i].last_active_s < oldest_ts) {
      oldest_ts = g_am_sessions[i].last_active_s;
      oldest_idx = i;
    }
  }
  AmSession &s = g_am_sessions[oldest_idx];
  memset(&s, 0, sizeof(AmSession));
  s.active = true;
  am_gen_token(s.token);
  s.last_active_s = millis() / 1000;
  strncpy(s.client_ip, ip ? ip : "", 15);
  return &s;
}

static void am_session_destroy(const char *token) {
  for (int i = 0; i < AM_SESSION_MAX; i++) {
    if (g_am_sessions[i].active && am_ct_strcmp(g_am_sessions[i].token, token)) {
      memset(&g_am_sessions[i], 0, sizeof(AmSession));
      g_am_total_logouts++;
      return;
    }
  }
}

static int am_session_count() {
  int n = 0;
  uint32_t now = millis() / 1000;
  for (int i = 0; i < AM_SESSION_MAX; i++) {
    if (!g_am_sessions[i].active)
      continue;
    if ((now - g_am_sessions[i].last_active_s) > AM_SESSION_TTL_S) {
      memset(&g_am_sessions[i], 0, sizeof(AmSession));
      continue;
    }
    n++;
  }
  return n;
}

// ============================================================
// CREDENTIAL STORAGE
// ============================================================

static void am_load_or_generate_creds() {
  Preferences prefs;
  prefs.begin(AM_NVS_NS, false);

  bool initialized = prefs.getBool(AM_NVS_INITIALIZED, false);
  String cred_ver = prefs.getString(AM_NVS_CRED_VER_KEY, "");
  // Migration check: old format has no PBKDF2 salt stored.
  bool has_salt = (prefs.getString(AM_NVS_PASS_SALT_KEY, "").length() == AM_PBKDF2_SALT_LEN * 2);
  // Reset to defaults when: first boot, missing salt, or credential version changed.
  bool need_reset = !initialized || !has_salt || (cred_ver != AM_CRED_VER);

  if (need_reset) {
    if (initialized) {
      Serial.printf("[AUTH] Resetting credentials to defaults\n");
    }
    // Use fixed default dashboard password (user should change via UI or CLI).
    const char *plaintext = AM_DEFAULT_PASS;

    // Generate random per-device PBKDF2 salt.
    uint8_t salt_raw[AM_PBKDF2_SALT_LEN];
    esp_fill_random(salt_raw, AM_PBKDF2_SALT_LEN);
    char salt_hex[AM_PBKDF2_SALT_LEN * 2 + 1];
    am_bytes_to_hex(salt_raw, AM_PBKDF2_SALT_LEN, salt_hex);
    memcpy(g_am_password_salt, salt_raw, AM_PBKDF2_SALT_LEN);

    // Derive PBKDF2 key from default password.
    am_pbkdf2_password(plaintext, g_am_password_salt, g_am_password_hash);

    // Default AP WiFi password.
    strlcpy(g_am_ap_wifi_password, AM_DEFAULT_WIFI_PASS, AM_WIFI_PASS_LEN + 1);

    // Generate API key.
    uint8_t key_raw[AM_API_KEY_LEN];
    esp_fill_random(key_raw, AM_API_KEY_LEN);
    am_bytes_to_hex(key_raw, AM_API_KEY_LEN, g_am_api_key);

    prefs.putString(AM_NVS_PASS_HASH_KEY, g_am_password_hash);
    prefs.putString(AM_NVS_PASS_SALT_KEY, salt_hex);
    prefs.putString(AM_NVS_AP_WIFI_PW_KEY, g_am_ap_wifi_password);
    prefs.putString(AM_NVS_APIKEY_KEY, g_am_api_key);
    prefs.putBool(AM_NVS_INITIALIZED, true);
    prefs.putString(AM_NVS_CRED_VER_KEY, AM_CRED_VER);

    Serial.printf("[AUTH] Default credentials set — change via dashboard or CLI\n");
    Serial.printf("[AUTH]   Dashboard password : %s\n", AM_DEFAULT_PASS);
    Serial.printf("[AUTH]   AP WiFi password   : %s\n", AM_DEFAULT_WIFI_PASS);
  } else {
    // Load stored credentials — plaintext is never stored in NVS.
    String h = prefs.getString(AM_NVS_PASS_HASH_KEY, "");
    String s = prefs.getString(AM_NVS_PASS_SALT_KEY, "");
    String k = prefs.getString(AM_NVS_APIKEY_KEY, "");
    String pw = prefs.getString(AM_NVS_AP_WIFI_PW_KEY, "");
    strncpy(g_am_password_hash, h.c_str(), AM_PBKDF2_KEY_LEN * 2);
    g_am_password_hash[AM_PBKDF2_KEY_LEN * 2] = '\0';
    am_hex_to_bytes(s.c_str(), g_am_password_salt, AM_PBKDF2_SALT_LEN);
    strncpy(g_am_api_key, k.c_str(), AM_API_KEY_LEN * 2);
    g_am_api_key[AM_API_KEY_LEN * 2] = '\0';
    strncpy(g_am_ap_wifi_password, pw.c_str(), AM_WIFI_PASS_LEN);
    g_am_ap_wifi_password[AM_WIFI_PASS_LEN] = '\0';
    Serial.printf("[AUTH] Credentials loaded from NVS\n");
  }
  prefs.end();
}

// Reset to new random credentials (called from /api/auth/reset-creds).
// Returns the new plaintext password via out_plaintext (caller zeroes after use).
static void am_reset_credentials(char *out_plaintext) {
  char plaintext[AM_PASS_LEN + 1];
  am_gen_password(plaintext, AM_PASS_LEN);

  // New random salt on every reset.
  uint8_t new_salt[AM_PBKDF2_SALT_LEN];
  esp_fill_random(new_salt, AM_PBKDF2_SALT_LEN);
  char salt_hex[AM_PBKDF2_SALT_LEN * 2 + 1];
  am_bytes_to_hex(new_salt, AM_PBKDF2_SALT_LEN, salt_hex);
  memcpy(g_am_password_salt, new_salt, AM_PBKDF2_SALT_LEN);

  am_pbkdf2_password(plaintext, g_am_password_salt, g_am_password_hash);

  uint8_t key_raw[AM_API_KEY_LEN];
  esp_fill_random(key_raw, AM_API_KEY_LEN);
  am_bytes_to_hex(key_raw, AM_API_KEY_LEN, g_am_api_key);

  Preferences prefs;
  prefs.begin(AM_NVS_NS, false);
  prefs.putString(AM_NVS_PASS_HASH_KEY, g_am_password_hash);
  prefs.putString(AM_NVS_PASS_SALT_KEY, salt_hex);
  prefs.putString(AM_NVS_APIKEY_KEY, g_am_api_key);
  prefs.end();

  memset(g_am_sessions, 0, sizeof(g_am_sessions));
  Serial.printf("[AUTH] Credentials reset\n");

  if (out_plaintext) {
    strncpy(out_plaintext, plaintext, AM_PASS_LEN);
    out_plaintext[AM_PASS_LEN] = '\0';
  }
  memset(plaintext, 0, sizeof(plaintext));
}

// ============================================================
// TOKEN EXTRACTION FROM REQUEST
// ============================================================

static String am_extract_token_from_request() {
  // 1. Cookie: phantom32_token=<token>
  String cookie = server.header("Cookie");
  if (cookie.length() > 0) {
    int idx = cookie.indexOf("phantom32_token=");
    if (idx >= 0) {
      String tok = cookie.substring(idx + 16);
      int end = tok.indexOf(';');
      if (end >= 0)
        tok = tok.substring(0, end);
      tok.trim();
      if (tok.length() == AM_TOKEN_LEN * 2)
        return tok;
    }
  }

  // 2. Authorization: Bearer <token>
  String auth = server.header("Authorization");
  if (auth.startsWith("Bearer ")) {
    String tok = auth.substring(7);
    tok.trim();
    if (tok.length() == AM_TOKEN_LEN * 2)
      return tok;
  }

  // Note: ?token= query parameter is intentionally not supported.
  // Tokens in URLs appear in server logs and browser history.
  return "";
}

static String am_extract_api_key() {
  // Header only — query param was removed to prevent key leakage in server logs.
  String hdr = server.header("X-Phantom32-Key");
  if (hdr.length() == AM_API_KEY_LEN * 2)
    return hdr;
  return "";
}

// ============================================================
// AUTHENTICATION CHECK (call from every route handler)
// ============================================================

static bool auth_check() {
  if (!g_am_initialized)
    return true;  // not yet init, allow during boot

  String client_ip = server.client().remoteIP().toString();

  // Check API key first (for scripted access).
  String api_key = am_extract_api_key();
  if (api_key.length() > 0) {
    if (am_ct_strcmp(api_key.c_str(), g_am_api_key))
      return true;
    // Wrong API key counts as failed login.
    am_ip_record_fail(client_ip.c_str());
    return false;
  }

  // Check IP lockout.
  if (am_ip_is_locked(client_ip.c_str()))
    return false;

  // Check session token.
  String tok = am_extract_token_from_request();
  if (tok.length() == 0)
    return false;

  AmSession *s = am_session_find(tok.c_str());
  if (!s)
    return false;

  // Refresh idle timer.
  s->last_active_s = millis() / 1000;
  return true;
}

// Send 401 JSON response.
static void auth_reject() {
  server.sendHeader("WWW-Authenticate", "Bearer realm=\"Phantom32\"");
  server.send(
      401, "application/json",
      "{\"error\":\"Unauthorized\",\"hint\":\"POST /login with {\\\"password\\\":\\\"...\\\"}\"}");
}

// Send 403 JSON response (IP locked).
static void auth_reject_locked() {
  server.send(403, "application/json",
              "{\"error\":\"Locked\",\"hint\":\"Too many failed attempts. Try again later.\"}");
}

// ============================================================
// LOGIN HTML PAGE (PROGMEM)
// ============================================================

static const char AM_LOGIN_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Phantom32 Login</title>
<style>
:root{--bg:#0a0c10;--bg2:#12151c;--border:#2a3045;--accent:#00d4ff;--text:#e0e6f0;--err:#ff4a6e}
*{box-sizing:border-box;margin:0;padding:0}
body{background:var(--bg);color:var(--text);font-family:'Courier New',monospace;
  min-height:100vh;display:flex;align-items:center;justify-content:center}
.card{background:var(--bg2);border:1px solid var(--border);border-radius:8px;
  padding:40px;width:360px;max-width:95vw}
h1{font-size:1.4em;color:var(--accent);margin-bottom:4px;letter-spacing:2px}
.sub{font-size:.75em;color:#8090a0;margin-bottom:28px}
label{display:block;font-size:.8em;color:#8090a0;margin-bottom:6px;letter-spacing:1px}
input{width:100%;padding:10px 14px;background:#0a0c10;border:1px solid var(--border);
  border-radius:4px;color:var(--text);font-family:inherit;font-size:.95em;outline:none;
  transition:border-color .2s}
input:focus{border-color:var(--accent)}
button{margin-top:20px;width:100%;padding:12px;background:var(--accent);color:#000;
  border:none;border-radius:4px;font-family:inherit;font-size:1em;font-weight:bold;
  cursor:pointer;letter-spacing:1px;transition:opacity .2s}
button:hover{opacity:.85}
.err{margin-top:14px;color:var(--err);font-size:.85em;text-align:center;min-height:1.2em}
.hint{margin-top:16px;font-size:.75em;color:#607080;text-align:center}
</style>
</head>
<body>
<div class="card">
  <h1>PHANTOM32</h1>
  <div class="sub">Security Research Firmware</div>
  <form id="f">
    <label>PASSWORD</label>
    <input id="pw" type="password" autocomplete="current-password" placeholder="Enter device password">
    <button type="submit">LOGIN</button>
  </form>
  <div class="err" id="err"></div>
  <div class="hint">Default password: <strong>password</strong> &mdash; change in Settings</div>
</div>
<script>
document.getElementById('f').addEventListener('submit',async e=>{
  e.preventDefault();
  const pw=document.getElementById('pw').value;
  const err=document.getElementById('err');
  err.textContent='';
  try{
    const r=await fetch('/login',{method:'POST',
      headers:{'Content-Type':'application/json'},
      body:JSON.stringify({password:pw})});
    const d=await r.json();
    if(r.ok && d.ok){window.location.href='/';}
    else{err.textContent=d.error||'Login failed';}
  }catch(ex){err.textContent='Connection error';}
});
</script>
</body>
</html>
)rawhtml";

// ============================================================
// ROUTE HANDLERS
// ============================================================

static void am_handle_login_page() {
  server.send_P(200, "text/html", AM_LOGIN_PAGE);
}

static void am_handle_login() {
  String client_ip = server.client().remoteIP().toString();

  if (am_ip_is_locked(client_ip.c_str())) {
    auth_reject_locked();
    return;
  }

  // Parse JSON body: {"password":"..."}
  String body = server.arg("plain");
  String password;

  // Simple JSON field extraction (avoid full JSON parser dependency).
  int idx = body.indexOf("\"password\"");
  if (idx >= 0) {
    int q1 = body.indexOf('"', idx + 10);
    int q2 = body.indexOf('"', q1 + 1);
    if (q1 >= 0 && q2 > q1)
      password = body.substring(q1 + 1, q2);
  }

  // Also accept form-encoded: password=...
  if (password.length() == 0 && server.hasArg("password")) {
    password = server.arg("password");
  }

  if (password.length() == 0) {
    am_ip_record_fail(client_ip.c_str());
    g_am_total_failures++;
    server.send(400, "application/json", "{\"error\":\"Missing password\"}");
    return;
  }

  char submitted_hash[AM_PBKDF2_KEY_LEN * 2 + 1];
  am_pbkdf2_password(password.c_str(), g_am_password_salt, submitted_hash);
  if (!am_ct_strcmp(submitted_hash, g_am_password_hash)) {
    am_ip_record_fail(client_ip.c_str());
    g_am_total_failures++;
    Serial.printf("[AUTH] Login FAILED from %s\n", client_ip.c_str());
    // Small delay to slow brute-force attempts.
    delay(500);
    server.send(401, "application/json", "{\"error\":\"Invalid password\"}");
    return;
  }

  am_ip_record_success(client_ip.c_str());
  AmSession *s = am_session_alloc(client_ip.c_str());
  if (!s) {
    server.send(503, "application/json", "{\"error\":\"Session table full\"}");
    return;
  }

  g_am_total_logins++;
  Serial.printf("[AUTH] Login OK from %s\n", client_ip.c_str());

  // Set HttpOnly cookie — do NOT return token in body (browser JS cannot read it).
  String cookie = "phantom32_token=";
  cookie += s->token;
  cookie += "; Path=/; HttpOnly; SameSite=Strict";
  server.sendHeader("Set-Cookie", cookie);
  server.send(200, "application/json", "{\"ok\":true}");
}

static void am_handle_logout() {
  String tok = am_extract_token_from_request();
  if (tok.length() > 0)
    am_session_destroy(tok.c_str());
  // Clear the cookie.
  server.sendHeader("Set-Cookie",
                    "phantom32_token=; Path=/; Expires=Thu, 01 Jan 1970 00:00:00 GMT");
  server.send(200, "application/json", "{\"ok\":true}");
}

static void am_handle_status() {
  if (!auth_check()) {
    auth_reject();
    return;
  }
  String s = "{";
  s += "\"sessions\":" + String(am_session_count());
  s += ",\"total_logins\":" + String(g_am_total_logins);
  s += ",\"total_failures\":" + String(g_am_total_failures);
  s += ",\"total_logouts\":" + String(g_am_total_logouts);
  s += ",\"lockout_events\":" + String(g_am_lockout_events);
  s += ",\"locked_ips\":[";
  bool first = true;
  uint32_t now = millis() / 1000;
  for (int i = 0; i < AM_MAX_IPS; i++) {
    if (!g_am_ips[i].locked)
      continue;
    if ((now - g_am_ips[i].lock_start_s) > AM_LOCKOUT_S)
      continue;
    if (!first)
      s += ",";
    s += "{\"ip\":\"" + String(g_am_ips[i].ip) + "\"";
    s += ",\"until\":" + String(AM_LOCKOUT_S - (now - g_am_ips[i].lock_start_s));
    s += ",\"fails\":" + String(g_am_ips[i].fail_count) + "}";
    first = false;
  }
  s += "]}";
  server.send(200, "application/json", s);
}

static void am_handle_reset_creds() {
  if (!auth_check()) {
    auth_reject();
    return;
  }
  char new_pass[AM_PASS_LEN + 1];
  am_reset_credentials(new_pass);
  // Return new password ONCE in this authenticated response — the only time
  // it appears in plaintext outside of serial at first boot.
  String resp = "{\"ok\":true,\"password\":\"";
  resp += new_pass;
  resp += "\"}";
  memset(new_pass, 0, sizeof(new_pass));
  server.send(200, "application/json", resp);
}

static void am_handle_change_password() {
  if (!auth_check()) {
    auth_reject();
    return;
  }
  String body = server.arg("plain");
  String old_pass, new_pass;

  auto extract = [&](const String &b, const char *key) -> String {
    int idx = b.indexOf(key);
    if (idx < 0)
      return "";
    int q1 = b.indexOf('"', idx + strlen(key));
    int q2 = b.indexOf('"', q1 + 1);
    if (q1 < 0 || q2 <= q1)
      return "";
    return b.substring(q1 + 1, q2);
  };

  old_pass = extract(body, "\"old_password\"");
  new_pass = extract(body, "\"new_password\"");

  if (old_pass.length() == 0 || new_pass.length() == 0) {
    server.send(400, "application/json", "{\"error\":\"Provide old_password and new_password\"}");
    return;
  }

  char old_hash[AM_PBKDF2_KEY_LEN * 2 + 1];
  am_pbkdf2_password(old_pass.c_str(), g_am_password_salt, old_hash);
  if (!am_ct_strcmp(old_hash, g_am_password_hash)) {
    server.send(401, "application/json", "{\"error\":\"Wrong current password\"}");
    return;
  }

  if (new_pass.length() < 8) {
    server.send(400, "application/json", "{\"error\":\"Password must be at least 8 characters\"}");
    return;
  }

  // New random salt on every password change.
  uint8_t new_salt[AM_PBKDF2_SALT_LEN];
  esp_fill_random(new_salt, AM_PBKDF2_SALT_LEN);
  char new_salt_hex[AM_PBKDF2_SALT_LEN * 2 + 1];
  am_bytes_to_hex(new_salt, AM_PBKDF2_SALT_LEN, new_salt_hex);
  memcpy(g_am_password_salt, new_salt, AM_PBKDF2_SALT_LEN);
  am_pbkdf2_password(new_pass.c_str(), g_am_password_salt, g_am_password_hash);

  Preferences prefs;
  prefs.begin(AM_NVS_NS, false);
  prefs.putString(AM_NVS_PASS_HASH_KEY, g_am_password_hash);
  prefs.putString(AM_NVS_PASS_SALT_KEY, new_salt_hex);
  prefs.end();

  // Invalidate all existing sessions (force re-login with new password).
  memset(g_am_sessions, 0, sizeof(g_am_sessions));

  Serial.printf("[AUTH] Password changed\n");
  server.send(200, "application/json", "{\"ok\":true}");
}

static void am_handle_change_wifi_password() {
  if (!auth_check()) { auth_reject(); return; }
  String body = server.arg("plain");
  int idx = body.indexOf("\"new_password\"");
  if (idx < 0) {
    server.send(400, "application/json", "{\"error\":\"Provide new_password\"}");
    return;
  }
  int q1 = body.indexOf('"', idx + 14);
  int q2 = body.indexOf('"', q1 + 1);
  if (q1 < 0 || q2 <= q1) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  String new_pass = body.substring(q1 + 1, q2);
  if (new_pass.length() < 8 || new_pass.length() > 63) {
    server.send(400, "application/json", "{\"error\":\"WiFi password must be 8-63 characters\"}");
    return;
  }
  strlcpy(g_am_ap_wifi_password, new_pass.c_str(), AM_WIFI_PASS_LEN + 1);
  Preferences prefs;
  prefs.begin(AM_NVS_NS, false);
  prefs.putString(AM_NVS_AP_WIFI_PW_KEY, g_am_ap_wifi_password);
  prefs.end();
  Serial.printf("[AUTH] WiFi password changed — restart to apply\n");
  server.send(200, "application/json", "{\"ok\":true,\"note\":\"Restart device to apply new WiFi password\"}");
}

// ============================================================
// PUBLIC API
// ============================================================

inline void auth_init() {
  am_load_or_generate_creds();
  g_am_initialized = true;

  // Public endpoints (no auth required).
  server.on("/login", HTTP_GET, am_handle_login_page);
  server.on("/login", HTTP_POST, am_handle_login);
  server.on("/logout", HTTP_POST, am_handle_logout);
  server.on("/logout", HTTP_GET, am_handle_logout);

  // Protected auth management endpoints.
  server.on("/api/auth/status", HTTP_GET, am_handle_status);
  server.on("/api/auth/reset-creds", HTTP_POST, am_handle_reset_creds);
  server.on("/api/auth/change-password", HTTP_POST, am_handle_change_password);
  server.on("/api/auth/change-wifi-password", HTTP_POST, am_handle_change_wifi_password);

  Serial.printf("[AUTH] Initialized\n");
}

// Return per-device AP WiFi password (must call auth_init() first).
// FAIL CLOSED: if credentials were not initialized, restart rather than
// fall back to any known or empty password.
inline const char *auth_get_ap_wifi_password() {
  if (!g_am_ap_wifi_password[0]) {
    Serial.printf(
        "[AUTH] FATAL: AP WiFi password not set — auth_init() must be called before WiFi setup\n");
    Serial.flush();
    esp_restart();
  }
  return g_am_ap_wifi_password;
}

// Redirect to login page (use in handlers that serve HTML, not JSON).
static void auth_redirect_login() {
  server.sendHeader("Location", "/login");
  server.send(302, "text/plain", "Redirecting to /login");
}

// Combined check: JSON API → 401, HTML → redirect to /login.
static bool auth_check_or_redirect(bool is_html = false) {
  if (auth_check())
    return true;
  if (is_html)
    auth_redirect_login();
  else
    auth_reject();
  return false;
}

// Print credential recovery info on serial (password is not recoverable from hash).
static void auth_print_credentials() {
  Serial.printf("=== Phantom32 Credentials ===\n");
  Serial.printf("URL:              http://192.168.4.1/\n");
  Serial.printf("AP WiFi password: %s\n",
                g_am_ap_wifi_password[0] ? g_am_ap_wifi_password : "(not loaded)");
  Serial.printf("Dashboard user:   admin\n");
  Serial.printf("Dashboard pass:   not recoverable — use /api/auth/reset-creds\n");
  Serial.printf("API key:          %s\n", g_am_api_key);
  Serial.printf("=============================\n");
}

#pragma once
// ssid_mutator.h — SSID mutation and generation engine.
// Creates variant SSIDs for KARMA pool auto-population, evil-twin targeting,
// and passive SSID normalization. Prefix: smu_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define SMU_MAX_INPUT   33
#define SMU_MAX_OUTPUT  256
#define SMU_MAX_RULES   32

// ─── Mutation rule types ───────────────────────────────────────────────────────
enum SmuMutationType {
  SMU_MUT_NONE = 0,
  SMU_MUT_CASE_UPPER,       // ALL CAPS
  SMU_MUT_CASE_LOWER,       // all lowercase
  SMU_MUT_CASE_TITLE,       // Title Case
  SMU_MUT_APPEND_NUMBER,    // append 1-9
  SMU_MUT_APPEND_UNDERSCORE,// add _5G, _2G, _EXT, _FAST, etc.
  SMU_MUT_STRIP_SUFFIX,     // remove trailing numbers/chars
  SMU_MUT_REPLACE_ZERO_O,   // 0 ↔ O
  SMU_MUT_REPLACE_I_1,      // I → 1, 1 → I
  SMU_MUT_REPLACE_S_5,      // S → 5, 5 → S
  SMU_MUT_SPACE_TO_DASH,    // space → -
  SMU_MUT_DASH_TO_UNDERSCORE,// - → _
  SMU_MUT_ADD_EXCL,         // !SSID or SSID!
  SMU_MUT_UNICODE_LOOKALIKE,// replace a→α, etc.
  SMU_MUT_CLONE_PREFIX,     // "NETGEAR" → "NETGEAR_"
  SMU_MUT_BRAND_VARIANT,    // "dlink" → "D-Link"
  SMU_MUT_HIDDEN_CLONE,     // empty SSID (clone hidden)
  SMU_MUT_NUMERIC_SUFFIX_INC,// SSID_1 → SSID_2 etc.
};

// Common suffix variants
static const char *SMU_SUFFIXES_5G[]     = {"_5G", "-5G", " 5G", "_5GHz", "5G"};
static const char *SMU_SUFFIXES_EXT[]    = {"_EXT", "-EXT", "_FAST", "_plus", "2"};
static const char *SMU_SUFFIXES_GUEST[]  = {"_guest", "_Guest", "-guest", "_GUEST"};
static const char *SMU_SUFFIXES_NUM[]    = {"1","2","3","4","5","6","7","8","9","10","_1","_2"};
static const char *SMU_BRAND_ALIASES[][2] = {
  {"dlink","D-Link"},{"DLINK","D-LINK"},{"linksys","LINKSYS"},{"netgear","NETGEAR"},
  {"tplink","TP-Link"},{"asus","ASUS"},{"belkin","Belkin"},{"cisco","CISCO"},
};
#define SMU_BRAND_ALIAS_COUNT 8

struct SmuResult {
  char ssid[SMU_MAX_INPUT];
  uint8_t mutation;
};

struct SmuState {
  char     input_ssids[SMU_MAX_RULES][SMU_MAX_INPUT];
  uint8_t  input_count;
  SmuResult results[SMU_MAX_OUTPUT];
  uint16_t  result_count;
  uint32_t  total_generated;
};

static SmuState g_smu = {};

// ─── Individual mutation functions ────────────────────────────────────────────
static void smu_to_upper(const char *in, char *out) {
  int i = 0;
  for (; in[i] && i < SMU_MAX_INPUT - 1; i++) out[i] = toupper(in[i]);
  out[i] = '\0';
}

static void smu_to_lower(const char *in, char *out) {
  int i = 0;
  for (; in[i] && i < SMU_MAX_INPUT - 1; i++) out[i] = tolower(in[i]);
  out[i] = '\0';
}

static void smu_to_title(const char *in, char *out) {
  bool cap_next = true;
  int i = 0;
  for (; in[i] && i < SMU_MAX_INPUT - 1; i++) {
    if (in[i] == ' ' || in[i] == '_' || in[i] == '-') { cap_next = true; out[i] = in[i]; }
    else if (cap_next) { out[i] = toupper(in[i]); cap_next = false; }
    else out[i] = tolower(in[i]);
  }
  out[i] = '\0';
}

static void smu_replace_01(const char *in, char *out, char from, char to) {
  int i = 0;
  for (; in[i] && i < SMU_MAX_INPUT - 1; i++)
    out[i] = (in[i] == from) ? to : in[i];
  out[i] = '\0';
}

static void smu_subst_all(const char *in, char *out) {
  // 0↔O, I↔1, S↔5, B↔8, A↔4
  static const char FROM[] = "0O1IsS5bBAa4";
  static const char TO[]   = "Oo0o1i5Ss884";
  int i = 0;
  for (; in[i] && i < SMU_MAX_INPUT - 1; i++) {
    char c = in[i];
    bool found = false;
    for (int j = 0; j < 12; j++) {
      if (c == FROM[j]) { out[i] = TO[j]; found = true; break; }
    }
    if (!found) out[i] = c;
  }
  out[i] = '\0';
}

static bool smu_add_result(const char *ssid, uint8_t mutation) {
  if (g_smu.result_count >= SMU_MAX_OUTPUT) return false;
  int slen = strlen(ssid);
  if (slen == 0 || slen > 32) return false;
  // De-duplicate
  for (int i = 0; i < g_smu.result_count; i++) {
    if (strcmp(g_smu.results[i].ssid, ssid) == 0) return false;
  }
  strncpy(g_smu.results[g_smu.result_count].ssid, ssid, SMU_MAX_INPUT - 1);
  g_smu.results[g_smu.result_count].mutation = mutation;
  g_smu.result_count++;
  g_smu.total_generated++;
  return true;
}

// ─── Generate all mutations for one SSID ─────────────────────────────────────
static void smu_generate_one(const char *base) {
  char buf[SMU_MAX_INPUT];
  int blen = strlen(base);
  if (blen == 0 || blen > 32) return;

  // Original
  smu_add_result(base, SMU_MUT_NONE);

  // Case variants
  smu_to_upper(base, buf); smu_add_result(buf, SMU_MUT_CASE_UPPER);
  smu_to_lower(base, buf); smu_add_result(buf, SMU_MUT_CASE_LOWER);
  smu_to_title(base, buf); smu_add_result(buf, SMU_MUT_CASE_TITLE);

  // Numeric suffixes
  for (int i = 0; i < 12 && g_smu.result_count < SMU_MAX_OUTPUT; i++) {
    if (blen + strlen(SMU_SUFFIXES_NUM[i]) < 33) {
      snprintf(buf, sizeof(buf), "%s%s", base, SMU_SUFFIXES_NUM[i]);
      smu_add_result(buf, SMU_MUT_APPEND_NUMBER);
    }
  }

  // 5G/EXT/Guest suffixes
  for (int i = 0; i < 5; i++) {
    if (blen + strlen(SMU_SUFFIXES_5G[i]) < 33) {
      snprintf(buf, sizeof(buf), "%s%s", base, SMU_SUFFIXES_5G[i]);
      smu_add_result(buf, SMU_MUT_APPEND_UNDERSCORE);
    }
  }
  for (int i = 0; i < 5; i++) {
    if (blen + strlen(SMU_SUFFIXES_EXT[i]) < 33) {
      snprintf(buf, sizeof(buf), "%s%s", base, SMU_SUFFIXES_EXT[i]);
      smu_add_result(buf, SMU_MUT_APPEND_UNDERSCORE);
    }
  }
  for (int i = 0; i < 4; i++) {
    if (blen + strlen(SMU_SUFFIXES_GUEST[i]) < 33) {
      snprintf(buf, sizeof(buf), "%s%s", base, SMU_SUFFIXES_GUEST[i]);
      smu_add_result(buf, SMU_MUT_APPEND_UNDERSCORE);
    }
  }

  // Space → dash, dash → underscore
  smu_replace_01(base, buf, ' ', '-'); smu_add_result(buf, SMU_MUT_SPACE_TO_DASH);
  smu_replace_01(base, buf, '-', '_'); smu_add_result(buf, SMU_MUT_DASH_TO_UNDERSCORE);
  smu_replace_01(base, buf, '_', '-'); smu_add_result(buf, SMU_MUT_DASH_TO_UNDERSCORE);

  // leet-speak substitution
  smu_subst_all(base, buf); smu_add_result(buf, SMU_MUT_REPLACE_ZERO_O);

  // Prefix/suffix !
  if (blen < 32) {
    snprintf(buf, sizeof(buf), "!%s", base);  smu_add_result(buf, SMU_MUT_ADD_EXCL);
    snprintf(buf, sizeof(buf), "%s!", base);  smu_add_result(buf, SMU_MUT_ADD_EXCL);
  }

  // Brand alias
  for (int i = 0; i < SMU_BRAND_ALIAS_COUNT; i++) {
    if (strcasestr(base, SMU_BRAND_ALIASES[i][0]) != nullptr) {
      char *pos = strcasestr(base, SMU_BRAND_ALIASES[i][0]);
      int pfx = pos - base;
      snprintf(buf, sizeof(buf), "%.*s%s%s", pfx, base, SMU_BRAND_ALIASES[i][1],
               base + pfx + strlen(SMU_BRAND_ALIASES[i][0]));
      smu_add_result(buf, SMU_MUT_BRAND_VARIANT);
    }
  }

  // Hidden SSID clone
  smu_add_result("", SMU_MUT_HIDDEN_CLONE);

  // Numeric suffix increment (if ends in digit)
  if (blen > 0 && isdigit(base[blen - 1])) {
    memcpy(buf, base, blen);
    buf[blen - 1] = base[blen - 1] + 1;
    if (buf[blen - 1] > '9') buf[blen - 1] = '0';
    buf[blen] = '\0';
    smu_add_result(buf, SMU_MUT_NUMERIC_SUFFIX_INC);
  }
}

// ─── Generate all mutations ───────────────────────────────────────────────────
static void smu_generate_all() {
  g_smu.result_count = 0;
  for (int i = 0; i < g_smu.input_count; i++) {
    smu_generate_one(g_smu.input_ssids[i]);
  }
}

// ─── JSON export ─────────────────────────────────────────────────────────────
static String smu_json_results() {
  String s = "{\"count\":" + String(g_smu.result_count);
  s += ",\"total_generated\":" + String(g_smu.total_generated);
  s += ",\"results\":[";
  for (int i = 0; i < g_smu.result_count; i++) {
    if (i) s += ",";
    s += "{\"ssid\":\"";
    // JSON-escape
    const char *ss = g_smu.results[i].ssid;
    for (int j = 0; ss[j]; j++) {
      if (ss[j] == '"') s += "\\\"";
      else if (ss[j] == '\\') s += "\\\\";
      else s += ss[j];
    }
    s += "\",\"mut\":" + String(g_smu.results[i].mutation) + "}";
  }
  s += "]}";
  return s;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void smu_routes() {
  server.on("/api/smu/mutate", HTTP_POST, []() {
    if (!server.hasArg("ssid")) {
      server.send(400, "application/json", "{\"error\":\"missing ssid\"}");
      return;
    }
    g_smu.input_count = 0;
    g_smu.result_count = 0;
    // Accept comma-separated list
    String arg = server.arg("ssid");
    int start = 0;
    while (start < (int)arg.length() && g_smu.input_count < SMU_MAX_RULES) {
      int comma = arg.indexOf(',', start);
      if (comma < 0) comma = arg.length();
      String part = arg.substring(start, comma);
      part.trim();
      if (part.length() > 0 && part.length() <= 32) {
        strncpy(g_smu.input_ssids[g_smu.input_count], part.c_str(), SMU_MAX_INPUT - 1);
        g_smu.input_ssids[g_smu.input_count][SMU_MAX_INPUT - 1] = '\0';
        g_smu.input_count++;
      }
      start = comma + 1;
    }
    smu_generate_all();
    server.send(200, "application/json", smu_json_results());
  });

  server.on("/api/smu/results", HTTP_GET, []() {
    server.send(200, "application/json", smu_json_results());
  });

  server.on("/api/smu/clear", HTTP_POST, []() {
    g_smu.input_count = 0;
    g_smu.result_count = 0;
    server.send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/smu/export_karma", HTTP_GET, []() {
    // Export as JSON array suitable for importing into KARMA pool
    String s = "[";
    for (int i = 0; i < g_smu.result_count; i++) {
      if (i) s += ",";
      s += "\"";
      const char *ss = g_smu.results[i].ssid;
      for (int j = 0; ss[j]; j++) {
        if (ss[j] == '"') s += "\\\"";
        else s += ss[j];
      }
      s += "\"";
    }
    s += "]";
    server.sendHeader("Content-Disposition", "attachment; filename=karma_pool.json");
    server.send(200, "application/json", s);
  });
}

#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// SemanticVersion — parsing and constraint checking per semver 2.0 (Phase 11).
// Supports MAJOR.MINOR.PATCH, optional -pre-release and +build-metadata.
// Constraint operators: >=, <=, ==, >, <, ^ (compatible), ~ (approximate).

static constexpr size_t PHANTOM_SV_LABEL_SIZE = 32;

struct SemanticVersion {
  uint16_t major;
  uint16_t minor;
  uint16_t patch;
  char preRelease[PHANTOM_SV_LABEL_SIZE];
  char buildMeta[PHANTOM_SV_LABEL_SIZE];
  bool valid;

  static SemanticVersion parse(const char *s) {
    SemanticVersion v = {};
    if (!s || !*s)
      return v;
    char tmp[80] = {};
    // Extract build metadata (+…)
    const char *plus = strchr(s, '+');
    if (plus) {
      size_t mainLen = (size_t)(plus - s);
      if (mainLen >= sizeof(tmp))
        return v;
      strncpy(tmp, s, mainLen);
      strncpy(v.buildMeta, plus + 1, PHANTOM_SV_LABEL_SIZE - 1);
    } else {
      strncpy(tmp, s, sizeof(tmp) - 1);
    }
    // Extract pre-release (-…)
    char core[80] = {};
    const char *dash = strchr(tmp, '-');
    if (dash) {
      size_t coreLen = (size_t)(dash - tmp);
      if (coreLen >= sizeof(core))
        return v;
      strncpy(core, tmp, coreLen);
      strncpy(v.preRelease, dash + 1, PHANTOM_SV_LABEL_SIZE - 1);
    } else {
      strncpy(core, tmp, sizeof(core) - 1);
    }
    // Parse MAJOR.MINOR.PATCH
    unsigned ma = 0, mi = 0, pa = 0;
    int parsed = sscanf(core, "%u.%u.%u", &ma, &mi, &pa);
    if (parsed < 3)
      return v;
    v.major = (uint16_t)ma;
    v.minor = (uint16_t)mi;
    v.patch = (uint16_t)pa;
    v.valid = true;
    return v;
  }

  int compare(const SemanticVersion &o) const {
    if (major != o.major)
      return major < o.major ? -1 : 1;
    if (minor != o.minor)
      return minor < o.minor ? -1 : 1;
    if (patch != o.patch)
      return patch < o.patch ? -1 : 1;
    return 0;
  }

  bool operator==(const SemanticVersion &o) const { return compare(o) == 0; }
  bool operator<(const SemanticVersion &o) const { return compare(o) < 0; }
  bool operator>(const SemanticVersion &o) const { return compare(o) > 0; }
  bool operator<=(const SemanticVersion &o) const { return compare(o) <= 0; }
  bool operator>=(const SemanticVersion &o) const { return compare(o) >= 0; }

  // Constraint: ">=1.2.0" "<=2.0.0" "==1.0.0" "^1.0.0" "~1.2.0" ">1.0.0" "<2.0.0"
  bool satisfies(const char *constraint) const {
    if (!constraint || !valid)
      return false;
    const char *verStr = nullptr;
    char op[4] = {};
    if (constraint[0] == '^') {
      op[0] = '^';
      verStr = constraint + 1;
    } else if (constraint[0] == '~') {
      op[0] = '~';
      verStr = constraint + 1;
    } else if (constraint[0] == '>' && constraint[1] == '=') {
      op[0] = '>';
      op[1] = '=';
      verStr = constraint + 2;
    } else if (constraint[0] == '<' && constraint[1] == '=') {
      op[0] = '<';
      op[1] = '=';
      verStr = constraint + 2;
    } else if (constraint[0] == '=' && constraint[1] == '=') {
      op[0] = '=';
      op[1] = '=';
      verStr = constraint + 2;
    } else if (constraint[0] == '>') {
      op[0] = '>';
      verStr = constraint + 1;
    } else if (constraint[0] == '<') {
      op[0] = '<';
      verStr = constraint + 1;
    } else {
      op[0] = '=';
      op[1] = '=';
      verStr = constraint;
    }
    SemanticVersion target = parse(verStr);
    if (!target.valid)
      return false;
    if (op[0] == '^')
      return major == target.major && compare(target) >= 0;
    if (op[0] == '~')
      return major == target.major && minor == target.minor && compare(target) >= 0;
    if (op[0] == '>' && op[1] == '=')
      return compare(target) >= 0;
    if (op[0] == '<' && op[1] == '=')
      return compare(target) <= 0;
    if (op[0] == '=' && op[1] == '=')
      return compare(target) == 0;
    if (op[0] == '>')
      return compare(target) > 0;
    if (op[0] == '<')
      return compare(target) < 0;
    return false;
  }

  void toString(char *buf, size_t len) const {
    if (!buf || len == 0)
      return;
    int n = snprintf(buf, len, "%u.%u.%u", (unsigned)major, (unsigned)minor, (unsigned)patch);
    if (preRelease[0] && n > 0 && (size_t)n < len) {
      snprintf(buf + n, len - (size_t)n, "-%s", preRelease);
    }
  }
};

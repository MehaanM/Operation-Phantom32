#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// ConfigManager — central schema-validated configuration store (Phase 6).
//
// All configuration fields are declared up-front with a type and default value.
// Writes are validated against optional range, enum, and cross-field constraints
// before being accepted. Locked fields cannot be changed at runtime. Profiles
// save and restore complete snapshots. A CRC-like checksum and dirty-flag system
// let callers detect and act on changes. Change listeners fire on every accepted
// write.

static constexpr size_t PHANTOM_CM_MAX_FIELDS = 32;
static constexpr size_t PHANTOM_CM_MAX_ENUM_VALS = 8;
static constexpr size_t PHANTOM_CM_MAX_PROFILES = 4;
static constexpr size_t PHANTOM_CM_MAX_LISTENERS = 8;
static constexpr size_t PHANTOM_CM_MAX_CROSS_VALIDATORS = 8;
static constexpr size_t PHANTOM_CM_STRING_SIZE = 32;

enum class ConfigType : uint8_t { BOOL, INT32, FLOAT32, STRING };

union ConfigRaw {
  bool b;
  int32_t i;
  float f;
  char s[PHANTOM_CM_STRING_SIZE];
};

struct ConfigValue {
  ConfigType type;
  ConfigRaw raw;
};

struct ConfigField {
  char key[32];
  ConfigType type;
  ConfigValue defaultVal;
  ConfigValue currentVal;
  bool hasRange;
  int32_t minI, maxI;
  float minF, maxF;
  bool hasEnum;
  int32_t enumVals[PHANTOM_CM_MAX_ENUM_VALS];
  uint8_t enumCount;
  bool locked;
  bool dirty;
  bool defined;
};

typedef void (*ConfigChangeFn)(const char *key, const ConfigValue &oldVal,
                               const ConfigValue &newVal);
typedef bool (*CrossValidatorFn)(const char *key, const ConfigValue &proposed, void *ctx);

class ConfigManager {
 public:
  // --- Schema definition ---

  bool define(const char *key, bool defaultVal) {
    ConfigValue v;
    v.type = ConfigType::BOOL;
    v.raw.b = defaultVal;
    return defineField(key, v);
  }
  bool define(const char *key, int32_t defaultVal) {
    ConfigValue v;
    v.type = ConfigType::INT32;
    v.raw.i = defaultVal;
    return defineField(key, v);
  }
  bool define(const char *key, float defaultVal) {
    ConfigValue v;
    v.type = ConfigType::FLOAT32;
    v.raw.f = defaultVal;
    return defineField(key, v);
  }
  bool define(const char *key, const char *defaultVal) {
    ConfigValue v;
    v.type = ConfigType::STRING;
    if (defaultVal)
      strncpy(v.raw.s, defaultVal, sizeof(v.raw.s) - 1);
    else
      v.raw.s[0] = '\0';
    return defineField(key, v);
  }

  bool setRange(const char *key, int32_t min, int32_t max) {
    ConfigField *f = findField(key);
    if (!f || f->type != ConfigType::INT32)
      return false;
    f->hasRange = true;
    f->minI = min;
    f->maxI = max;
    return true;
  }
  bool setRangeF(const char *key, float min, float max) {
    ConfigField *f = findField(key);
    if (!f || f->type != ConfigType::FLOAT32)
      return false;
    f->hasRange = true;
    f->minF = min;
    f->maxF = max;
    return true;
  }
  bool setEnum(const char *key, const int32_t *vals, uint8_t count) {
    ConfigField *f = findField(key);
    if (!f || f->type != ConfigType::INT32 || !vals || count == 0 ||
        count > PHANTOM_CM_MAX_ENUM_VALS)
      return false;
    f->hasEnum = true;
    f->enumCount = count;
    for (uint8_t i = 0; i < count; ++i)
      f->enumVals[i] = vals[i];
    return true;
  }
  bool lock(const char *key) {
    ConfigField *f = findField(key);
    if (!f)
      return false;
    f->locked = true;
    return true;
  }

  bool addCrossValidator(CrossValidatorFn fn, void *ctx = nullptr) {
    if (crossValCount_ >= PHANTOM_CM_MAX_CROSS_VALIDATORS || !fn)
      return false;
    crossVals_[crossValCount_].fn = fn;
    crossVals_[crossValCount_].ctx = ctx;
    ++crossValCount_;
    return true;
  }

  // --- Setters ---

  bool setBool(const char *key, bool val) {
    ConfigValue v;
    v.type = ConfigType::BOOL;
    v.raw.b = val;
    return setField(key, v);
  }
  bool setInt(const char *key, int32_t val) {
    ConfigValue v;
    v.type = ConfigType::INT32;
    v.raw.i = val;
    return setField(key, v);
  }
  bool setFloat(const char *key, float val) {
    ConfigValue v;
    v.type = ConfigType::FLOAT32;
    v.raw.f = val;
    return setField(key, v);
  }
  bool setString(const char *key, const char *val) {
    if (!val)
      return false;
    ConfigValue v;
    v.type = ConfigType::STRING;
    strncpy(v.raw.s, val, sizeof(v.raw.s) - 1);
    v.raw.s[sizeof(v.raw.s) - 1] = '\0';
    return setField(key, v);
  }

  // --- Getters ---

  bool getBool(const char *key, bool *out) const {
    const ConfigField *f = findField(key);
    if (!f || f->type != ConfigType::BOOL || !out)
      return false;
    *out = f->currentVal.raw.b;
    return true;
  }
  bool getInt(const char *key, int32_t *out) const {
    const ConfigField *f = findField(key);
    if (!f || f->type != ConfigType::INT32 || !out)
      return false;
    *out = f->currentVal.raw.i;
    return true;
  }
  bool getFloat(const char *key, float *out) const {
    const ConfigField *f = findField(key);
    if (!f || f->type != ConfigType::FLOAT32 || !out)
      return false;
    *out = f->currentVal.raw.f;
    return true;
  }
  bool getString(const char *key, char *out, size_t outLen) const {
    const ConfigField *f = findField(key);
    if (!f || f->type != ConfigType::STRING || !out || outLen == 0)
      return false;
    strncpy(out, f->currentVal.raw.s, outLen - 1);
    out[outLen - 1] = '\0';
    return true;
  }

  // --- Validation ---

  bool validate(const char *key, const ConfigValue &val) const {
    const ConfigField *f = findField(key);
    if (!f || val.type != f->type)
      return false;
    if (f->type == ConfigType::INT32) {
      if (f->hasRange && (val.raw.i < f->minI || val.raw.i > f->maxI))
        return false;
      if (f->hasEnum) {
        bool found = false;
        for (uint8_t i = 0; i < f->enumCount; ++i)
          if (f->enumVals[i] == val.raw.i) {
            found = true;
            break;
          }
        if (!found)
          return false;
      }
    } else if (f->type == ConfigType::FLOAT32) {
      if (f->hasRange && (val.raw.f < f->minF || val.raw.f > f->maxF))
        return false;
    }
    for (size_t i = 0; i < crossValCount_; ++i)
      if (!crossVals_[i].fn(key, val, crossVals_[i].ctx))
        return false;
    return true;
  }

  bool validateAll() const {
    for (size_t i = 0; i < count_; ++i)
      if (!validate(fields_[i].key, fields_[i].currentVal))
        return false;
    return true;
  }

  // --- Versioning ---

  uint32_t version() const { return version_; }
  void setVersion(uint32_t v) { version_ = v; }

  // --- Profiles ---

  bool saveProfile(uint8_t slot) {
    if (slot >= PHANTOM_CM_MAX_PROFILES)
      return false;
    profiles_[slot].valid = true;
    profiles_[slot].fieldCount = count_;
    for (size_t i = 0; i < count_; ++i)
      profiles_[slot].vals[i] = fields_[i].currentVal;
    return true;
  }
  bool loadProfile(uint8_t slot) {
    if (slot >= PHANTOM_CM_MAX_PROFILES || !profiles_[slot].valid)
      return false;
    size_t n = profiles_[slot].fieldCount;
    if (n > count_)
      n = count_;
    for (size_t i = 0; i < n; ++i)
      setField(fields_[i].key, profiles_[slot].vals[i]);
    return true;
  }
  bool resetToDefaults() {
    for (size_t i = 0; i < count_; ++i) {
      if (!fields_[i].locked) {
        ConfigValue old = fields_[i].currentVal;
        fields_[i].currentVal = fields_[i].defaultVal;
        fields_[i].dirty = true;
        notifyListeners(fields_[i].key, old, fields_[i].currentVal);
      }
    }
    return true;
  }

  // --- Checksum ---

  uint32_t checksum() const {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < count_; ++i) {
      const uint8_t *p = reinterpret_cast<const uint8_t *>(&fields_[i].currentVal);
      for (size_t b = 0; b < sizeof(ConfigValue); ++b)
        h = (h ^ p[b]) * 16777619u;
    }
    return h;
  }

  // --- Dirty tracking ---

  size_t dirtyCount() const {
    size_t n = 0;
    for (size_t i = 0; i < count_; ++i)
      if (fields_[i].dirty)
        ++n;
    return n;
  }
  void clearDirty() {
    for (size_t i = 0; i < count_; ++i)
      fields_[i].dirty = false;
  }
  bool isDirty(const char *key) const {
    const ConfigField *f = findField(key);
    return f ? f->dirty : false;
  }

  // --- Change listeners ---

  bool addListener(ConfigChangeFn fn) {
    if (listenerCount_ >= PHANTOM_CM_MAX_LISTENERS || !fn)
      return false;
    listeners_[listenerCount_++] = fn;
    return true;
  }

  // --- Inspection ---

  size_t fieldCount() const { return count_; }
  const ConfigField *fieldFor(const char *key) const { return findField(key); }
  bool isLocked(const char *key) const {
    const ConfigField *f = findField(key);
    return f ? f->locked : false;
  }

 private:
  struct CrossValidator {
    CrossValidatorFn fn;
    void *ctx;
  };
  struct Profile {
    bool valid = false;
    size_t fieldCount = 0;
    ConfigValue vals[PHANTOM_CM_MAX_FIELDS] = {};
  };

  ConfigField fields_[PHANTOM_CM_MAX_FIELDS] = {};
  size_t count_ = 0;
  uint32_t version_ = 1;
  ConfigChangeFn listeners_[PHANTOM_CM_MAX_LISTENERS] = {};
  size_t listenerCount_ = 0;
  CrossValidator crossVals_[PHANTOM_CM_MAX_CROSS_VALIDATORS] = {};
  size_t crossValCount_ = 0;
  Profile profiles_[PHANTOM_CM_MAX_PROFILES] = {};

  bool defineField(const char *key, const ConfigValue &defaultVal) {
    if (!key)
      return false;
    ConfigField *existing = findField(key);
    if (existing) {
      existing->defaultVal = defaultVal;
      existing->currentVal = defaultVal;
      return true;
    }
    if (count_ >= PHANTOM_CM_MAX_FIELDS)
      return false;
    ConfigField &f = fields_[count_++];
    memset(&f, 0, sizeof(f));
    strncpy(f.key, key, sizeof(f.key) - 1);
    f.type = defaultVal.type;
    f.defaultVal = defaultVal;
    f.currentVal = defaultVal;
    f.defined = true;
    return true;
  }

  bool setField(const char *key, const ConfigValue &val) {
    if (!key)
      return false;
    ConfigField *f = findField(key);
    if (!f || f->locked)
      return false;
    if (!validate(key, val))
      return false;
    ConfigValue old = f->currentVal;
    f->currentVal = val;
    f->dirty = true;
    notifyListeners(key, old, val);
    return true;
  }

  ConfigField *findField(const char *key) {
    if (!key)
      return nullptr;
    for (size_t i = 0; i < count_; ++i)
      if (strncmp(fields_[i].key, key, sizeof(fields_[i].key)) == 0)
        return &fields_[i];
    return nullptr;
  }
  const ConfigField *findField(const char *key) const {
    if (!key)
      return nullptr;
    for (size_t i = 0; i < count_; ++i)
      if (strncmp(fields_[i].key, key, sizeof(fields_[i].key)) == 0)
        return &fields_[i];
    return nullptr;
  }

  void notifyListeners(const char *key, const ConfigValue &oldVal, const ConfigValue &newVal) {
    for (size_t i = 0; i < listenerCount_; ++i)
      listeners_[i](key, oldVal, newVal);
  }
};

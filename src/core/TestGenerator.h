#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// TestGenerator — produces test skeleton descriptors from module interfaces (Phase 19).
// Generates happy-path, null-input, boundary, error-path, and state-machine test cases.
// generateFile() writes a compilable Unity .cpp to disk.

static constexpr size_t PHANTOM_TG_MAX_SPECS = 16;
static constexpr size_t PHANTOM_TG_MAX_CASES = 32;

enum class TestCaseType : uint8_t { HAPPY_PATH, NULL_INPUT, BOUNDARY, ERROR_PATH, STATE_MACHINE };

struct TestCaseSpec {
  char name[48];
  char description[80];
  char setupHint[64];
  TestCaseType type;
  bool valid;
};

struct ModuleTestSpec {
  char moduleName[32];
  char interfaceName[32];
  TestCaseSpec cases[PHANTOM_TG_MAX_CASES];
  size_t caseCount;
  bool valid;
};

struct TestGenerator {
  bool createSpec(const char *moduleName, const char *interfaceName) {
    if (!moduleName || !moduleName[0])
      return false;
    for (size_t i = 0; i < specCount_; ++i) {
      if (strncmp(specs_[i].moduleName, moduleName, 31) == 0) {
        if (interfaceName)
          strncpy(specs_[i].interfaceName, interfaceName, 31);
        return true;
      }
    }
    if (specCount_ >= PHANTOM_TG_MAX_SPECS)
      return false;
    ModuleTestSpec &s = specs_[specCount_++];
    memset(&s, 0, sizeof(s));
    strncpy(s.moduleName, moduleName, 31);
    if (interfaceName)
      strncpy(s.interfaceName, interfaceName, 31);
    s.valid = true;
    return true;
  }

  bool addTestCase(const char *moduleName, TestCaseType type, const char *name,
                   const char *description, const char *setupHint) {
    if (!moduleName || !name || !name[0])
      return false;
    ModuleTestSpec *s = findSpec(moduleName);
    if (!s)
      return false;
    if (s->caseCount >= PHANTOM_TG_MAX_CASES)
      return false;
    TestCaseSpec &c = s->cases[s->caseCount++];
    memset(&c, 0, sizeof(c));
    strncpy(c.name, name, 47);
    if (description)
      strncpy(c.description, description, 79);
    if (setupHint)
      strncpy(c.setupHint, setupHint, 63);
    c.type = type;
    c.valid = true;
    return true;
  }

  bool generateFromInterface(const char *moduleName, const char *method, bool hasNullParams,
                             bool hasStateMachine) {
    if (!moduleName || !method || !method[0])
      return false;
    ModuleTestSpec *s = findSpec(moduleName);
    if (!s)
      return false;
    char buf[48];
    snprintf(buf, sizeof(buf), "test_%s_ok", method);
    addCaseToSpec(*s, TestCaseType::HAPPY_PATH, buf, "Happy path", "");
    if (hasNullParams) {
      snprintf(buf, sizeof(buf), "test_%s_null_fails", method);
      addCaseToSpec(*s, TestCaseType::NULL_INPUT, buf, "Null input returns false", "Pass nullptr");
    }
    if (hasStateMachine) {
      snprintf(buf, sizeof(buf), "test_%s_invalid_state", method);
      addCaseToSpec(*s, TestCaseType::STATE_MACHINE, buf, "Invalid state transition",
                    "Set wrong state first");
    }
    return true;
  }

  size_t caseCount(const char *moduleName) const {
    if (!moduleName)
      return 0;
    for (size_t i = 0; i < specCount_; ++i)
      if (strncmp(specs_[i].moduleName, moduleName, 31) == 0)
        return specs_[i].caseCount;
    return 0;
  }

  const ModuleTestSpec *specFor(const char *moduleName) const {
    if (!moduleName)
      return nullptr;
    for (size_t i = 0; i < specCount_; ++i)
      if (strncmp(specs_[i].moduleName, moduleName, 31) == 0)
        return &specs_[i];
    return nullptr;
  }

  size_t specCount() const { return specCount_; }

  // Writes a compilable Unity test .cpp to outputPath.
  // The file includes unity.h and includeHeader, setUp/tearDown stubs,
  // one test function per case with the setupHint as a comment, and a
  // main() that runs all of them. Returns false on null params or file error.
  bool generateFile(const char *moduleName, const char *includeHeader,
                    const char *outputPath) const {
    if (!moduleName || !includeHeader || !outputPath)
      return false;
    const ModuleTestSpec *s = specFor(moduleName);
    if (!s)
      return false;
    FILE *f = fopen(outputPath, "w");
    if (!f)
      return false;

    fprintf(f, "#include \"unity.h\"\n");
    fprintf(f, "#include \"%s\"\n\n", includeHeader);
    fprintf(f, "void setUp() {}\n");
    fprintf(f, "void tearDown() {}\n\n");

    for (size_t i = 0; i < s->caseCount; ++i) {
      fprintf(f, "static void %s() {\n", s->cases[i].name);
      if (s->cases[i].setupHint[0])
        fprintf(f, "  // Setup: %s\n", s->cases[i].setupHint);
      fprintf(f, "  // %s\n", s->cases[i].description);
      fprintf(f, "  TEST_PASS(); // TODO: implement\n");
      fprintf(f, "}\n\n");
    }

    fprintf(f, "int main() {\n");
    fprintf(f, "  UNITY_BEGIN();\n");
    for (size_t i = 0; i < s->caseCount; ++i)
      fprintf(f, "  RUN_TEST(%s);\n", s->cases[i].name);
    fprintf(f, "  return UNITY_END();\n");
    fprintf(f, "}\n");

    fclose(f);
    return true;
  }

  void printSkeleton(const char *moduleName, FILE *out) const {
    if (!out)
      return;
    const ModuleTestSpec *s = specFor(moduleName);
    if (!s) {
      fprintf(out, "// No spec found for module: %s\n", moduleName ? moduleName : "(null)");
      return;
    }
    fprintf(out, "// Test skeleton for %s (%s)\n", s->moduleName, s->interfaceName);
    for (size_t i = 0; i < s->caseCount; ++i) {
      fprintf(out, "static void %s() {\n", s->cases[i].name);
      if (s->cases[i].setupHint[0])
        fprintf(out, "  // Setup: %s\n", s->cases[i].setupHint);
      fprintf(out, "  // %s\n}\n\n", s->cases[i].description);
    }
  }

 private:
  ModuleTestSpec specs_[PHANTOM_TG_MAX_SPECS] = {};
  size_t specCount_ = 0;

  ModuleTestSpec *findSpec(const char *name) {
    for (size_t i = 0; i < specCount_; ++i)
      if (strncmp(specs_[i].moduleName, name, 31) == 0)
        return &specs_[i];
    return nullptr;
  }

  static void addCaseToSpec(ModuleTestSpec &s, TestCaseType type, const char *name,
                            const char *desc, const char *hint) {
    if (s.caseCount >= PHANTOM_TG_MAX_CASES)
      return;
    TestCaseSpec &c = s.cases[s.caseCount++];
    memset(&c, 0, sizeof(c));
    strncpy(c.name, name, 47);
    strncpy(c.description, desc, 79);
    strncpy(c.setupHint, hint, 63);
    c.type = type;
    c.valid = true;
  }
};

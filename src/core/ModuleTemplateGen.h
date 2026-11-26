#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// ModuleTemplateGen — module template descriptor and completeness validation (Phase 11).
// Tracks which sections (lifecycle, config, tests, docs, etc.) are present in a module
// and whether required sections are complete, producing a 0–100 completeness score.

static constexpr size_t PHANTOM_MTG_MAX_SECTIONS = 16;
static constexpr size_t PHANTOM_MTG_MAX_DEPS = 8;

enum class TemplateSectionStatus : uint8_t { MISSING, STUB, COMPLETE };

struct TemplateSection {
  char name[32];
  bool required;
  TemplateSectionStatus status;
};

struct ModuleTemplateDescriptor {
  char moduleName[32];
  char version[16];
  TemplateSection sections[PHANTOM_MTG_MAX_SECTIONS];
  size_t sectionCount;
  char dependencies[PHANTOM_MTG_MAX_DEPS][32];
  size_t depCount;
  uint32_t memoryBudgetBytes;
  bool valid;
};

class ModuleTemplateGen {
 public:
  static ModuleTemplateDescriptor create(const char *name) {
    ModuleTemplateDescriptor d = {};
    if (!name || !*name)
      return d;
    strncpy(d.moduleName, name, sizeof(d.moduleName) - 1);
    strncpy(d.version, "0.1.0", sizeof(d.version) - 1);
    d.memoryBudgetBytes = 4096;
    const char *names[] = {"lifecycle", "config_schema", "init", "tick",
                           "shutdown",  "tests",         "docs"};
    const bool required[] = {true, true, true, true, true, true, false};
    for (size_t i = 0; i < 7 && d.sectionCount < PHANTOM_MTG_MAX_SECTIONS; ++i) {
      strncpy(d.sections[d.sectionCount].name, names[i], 31);
      d.sections[d.sectionCount].required = required[i];
      d.sections[d.sectionCount].status = TemplateSectionStatus::MISSING;
      ++d.sectionCount;
    }
    d.valid = true;
    return d;
  }

  bool addSection(ModuleTemplateDescriptor &d, const char *name, bool required) const {
    if (!name || d.sectionCount >= PHANTOM_MTG_MAX_SECTIONS)
      return false;
    strncpy(d.sections[d.sectionCount].name, name, 31);
    d.sections[d.sectionCount].required = required;
    d.sections[d.sectionCount].status = TemplateSectionStatus::MISSING;
    ++d.sectionCount;
    return true;
  }

  bool markComplete(ModuleTemplateDescriptor &d, const char *sectionName) const {
    if (!sectionName)
      return false;
    for (size_t i = 0; i < d.sectionCount; ++i) {
      if (strcmp(d.sections[i].name, sectionName) == 0) {
        d.sections[i].status = TemplateSectionStatus::COMPLETE;
        return true;
      }
    }
    return false;
  }

  bool markStub(ModuleTemplateDescriptor &d, const char *sectionName) const {
    if (!sectionName)
      return false;
    for (size_t i = 0; i < d.sectionCount; ++i) {
      if (strcmp(d.sections[i].name, sectionName) == 0) {
        d.sections[i].status = TemplateSectionStatus::STUB;
        return true;
      }
    }
    return false;
  }

  bool validate(const ModuleTemplateDescriptor &d) const {
    return d.valid && missingRequiredCount(d) == 0;
  }

  size_t missingRequiredCount(const ModuleTemplateDescriptor &d) const {
    size_t count = 0;
    for (size_t i = 0; i < d.sectionCount; ++i) {
      if (d.sections[i].required && d.sections[i].status == TemplateSectionStatus::MISSING) {
        ++count;
      }
    }
    return count;
  }

  uint8_t completenessScore(const ModuleTemplateDescriptor &d) const {
    if (d.sectionCount == 0)
      return 0;
    size_t complete = 0;
    for (size_t i = 0; i < d.sectionCount; ++i) {
      if (d.sections[i].status == TemplateSectionStatus::COMPLETE)
        ++complete;
    }
    return (uint8_t)((complete * 100) / d.sectionCount);
  }

  bool addDependency(ModuleTemplateDescriptor &d, const char *dep) const {
    if (!dep || d.depCount >= PHANTOM_MTG_MAX_DEPS)
      return false;
    strncpy(d.dependencies[d.depCount], dep, 31);
    ++d.depCount;
    return true;
  }
};

#pragma once
// P32_EXT_BSS is a no-op; large analytics structs on PSRAM builds are
// placed in PSRAM via ps_calloc() in psram_alloc_all() (extended_modules.h).
// The Arduino 2.0.14 linker scripts map .ext_ram.bss to internal DRAM, so
// the section-attribute approach does not work with this framework version.
#define P32_EXT_BSS

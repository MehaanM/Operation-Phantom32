# ADR-002: Static ToolModule registry pattern

**Status:** Accepted  
**Date:** 2026-11-25  
**Deciders:** Mehaan Minocha

---

## Context

Phantom32 needs to support 31 research tools (29 Wi-Fi + 2 BLE), each with:
- An optional one-time `setup()` call during firmware initialization
- An optional per-iteration `loop()` call
- An optional `routes()` call that registers HTTP endpoints with the WebServer

The options for structuring this were:

1. **Monolithic `main.cpp`** — inline all tool code directly in `main.cpp`
   with `#ifdef` feature flags. Common in ESP32 projects. Results in a file
   that is effectively unreadable past a few tools; impossible to test
   individual tools in isolation.

2. **Dynamic plugin system** — a runtime-registered plugin table with heap
   allocation. Allows adding tools at runtime but introduces dynamic allocation
   into code that must operate under a constrained 320 KB DRAM budget.

3. **Static ToolModule registry** — define a `ToolModule` struct with function
   pointers for `setup`, `loop`, and `routes`. Declare a global array
   `g_tools[]` in `main.cpp`. Each tool is a header file that populates one
   struct instance. `setup()`, `loop()`, and `routes()` iterate over the
   array.

## Decision

Option 3: static `ToolModule` registry.

```cpp
struct ToolModule {
  const char *name;
  void (*setup)();
  void (*loop)();
  void (*routes)();
  bool  active;
};
```

`g_tools[]` in `main.cpp` holds all modules. Any function pointer can be
`nullptr`; iterators skip null entries. The array is statically allocated
at compile time; there is no dynamic registration.

## Consequences

**Positive:**
- Each tool is a self-contained header file. Adding a tool means adding one
  file and one entry in `g_tools[]`. Removing a tool means the reverse.
- Tools cannot accidentally call each other's internals. They share state
  only through explicitly named globals (channel state, TX budget, the
  promiscuous callback output).
- The pattern has zero dynamic allocation overhead. All tool structs are
  in rodata (const function pointers) or BSS (the `active` flag).
- Compilation errors in one tool do not interfere with others.
- The registry is inspectable at runtime via `/api/tools`, which iterates
  `g_tools[]` to report active/inactive state for each module.

**Negative:**
- Adding a new tool requires editing `main.cpp` to add the struct entry.
  This creates a minor merge-conflict surface if two contributors add tools
  simultaneously.
- All tools are compiled into every firmware build. Tools the user does not
  need still consume flash space. This was accepted as a tradeoff: the binary
  budget is tracked via `BinaryBudget.h` and CI reports flash usage per
  environment. If flash becomes critical, `#ifdef` guards per-tool can be
  added without changing the registry pattern.

## Alternatives not taken

**Monolithic main.cpp:** Rejected early in the project when the tool count
exceeded ~10. The file became unmaintainable and untestable. The module
boundary makes it possible to write per-tool test suites.

**Dynamic registration:** Rejected because it introduces `malloc()` into
the firmware initialization path and complicates stack/heap analysis. At
31 tools, a static array is the right data structure.

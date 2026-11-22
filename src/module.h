#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include <cstring>

static void (*tool_wifi_suspend)() = nullptr;
static void (*tool_wifi_resume)()  = nullptr;

struct ToolModule {
  const char* slug; const char* name; const char* desc;
  uint8_t radio; bool passive;
  void (*setup)(); void (*loop)(); void (*routes)();
  bool (*start)(const String&); void (*stop)();
};

#define MAX_TOOLS 32
static ToolModule g_tools[MAX_TOOLS];
static int g_tool_count = 0;
static const char* g_active = nullptr;

inline void tool_register(const ToolModule& m){
  if(g_tool_count<MAX_TOOLS) g_tools[g_tool_count++]=m;
}
inline const ToolModule* tool_find(const char* s){
  for(int i=0;i<g_tool_count;i++) if(!strcmp(g_tools[i].slug,s)) return &g_tools[i];
  return nullptr;
}
inline bool tool_start(const char* s, const String& a){
  const ToolModule* m=tool_find(s);
  if(!m||m->passive||!m->start) return false;
  if(g_active) return false;                                 // any active tool blocks start
  if(m->radio==1 && tool_wifi_suspend) tool_wifi_suspend();  // only suspend WiFi for radio=1 tools
  if(!m->start(a)){ if(m->radio==1 && tool_wifi_resume) tool_wifi_resume(); return false; }
  g_active=s; return true;
}
inline void tool_stop(const char* s){
  const ToolModule* m=tool_find(s); if(!m||!m->stop) return;
  m->stop();
  if(m->radio==1 && tool_wifi_resume) tool_wifi_resume();
  if(g_active&&!strcmp(g_active,s)) g_active=nullptr;
}

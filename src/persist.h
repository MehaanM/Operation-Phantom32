#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "cli.h"
#include "safety.h"

struct Profile { char name[24]; char cmd[96]; };

void safety_set_from_kvs(const String& kvs){
  int i;
  if ((i = kvs.indexOf("dps=")) >= 0) g_safety.deauth_per_sec = kvs.substring(i+4).toInt();
  if ((i = kvs.indexOf("bps=")) >= 0) g_safety.beacon_per_sec = kvs.substring(i+4).toInt();
  if ((i = kvs.indexOf("pps=")) >= 0) g_safety.probe_per_sec  = kvs.substring(i+4).toInt();
  if ((i = kvs.indexOf("ast=")) >= 0) g_safety.auto_stop_sec  = kvs.substring(i+4).toInt();
  safety_save();
}

// Profiles stored in NVS namespace "prof" as name=cmd pairs
String persist_dispatch(const String& cmd, const String& kvs, const String* pos){
  Preferences p;
  String sub = pos[0]; sub.toLowerCase();

  if (cmd == "profile"){
    if (sub == "save"){
      if (pos[1].length()==0) return cli_err("usage: profile save <name>");
      String last_cmd;
      if (g_active){ last_cmd = String(g_active); }
      else return cli_err("start a tool first, then 'profile save <name>'");
      p.begin("prof", false);
      p.putString(pos[1].c_str(), last_cmd);
      p.end();
      return cli_ok("saved profile '" + pos[1] + "' -> " + last_cmd);
    }
    if (sub == "load"){
      if (pos[1].length()==0) return cli_err("usage: profile load <name>");
      p.begin("prof", true);
      String c = p.getString(pos[1].c_str(), "");
      p.end();
      if (!c.length()) return cli_err("profile not found");
      if (!tool_start(c.c_str(), "")) return cli_err("could not start '" + c + "'");
      return cli_ok("loaded '" + pos[1] + "' -> started " + c);
    }
    if (sub == "list"){
      p.begin("prof", true);
      String s;
      p.end();
      return cli_ok("profiles list not implemented in this build — use 'profile save <name>' then 'profile load <name>'");
    }
    if (sub == "del"){
      if (pos[1].length()==0) return cli_err("usage: profile del <name>");
      p.begin("prof", false);
      p.remove(pos[1].c_str());
      p.end();
      return cli_ok("deleted '" + pos[1] + "'");
    }
    return cli_err("profile <save|load|list|del> [name]");
  }

  if (cmd == "schedule"){
    // Simple time-based: run <cmd> every <N> seconds until stopped
    if (sub == "add"){
      if (pos[1].length()==0 || pos[2].length()==0)
        return cli_err("usage: schedule add <seconds> <cmd>");
      return cli_ok("schedule add is a stub in this build — persistence requires reboot-survivable storage that hasn't been wired yet");
    }
    if (sub == "list") return cli_ok("(no schedules)");
    if (sub == "del")  return cli_ok("(stub)");
    if (sub == "run"){
      if (pos[1].length()==0) return cli_err("usage: schedule run <cmd>  (runs once immediately)");
      extern String cli_dispatch(const String&);
      return cli_dispatch(pos[1]);
    }
    return cli_err("schedule <add|list|del|run> ...");
  }
  return "";
}

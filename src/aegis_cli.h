#pragma once
// CLI bridge to Aegis defensive data. Included AFTER aegis.h.

String aegis_dispatch(const String &cmd, const String &kvs, const String *pos) {
  (void)kvs;

  if (cmd == "aegis") {
    long ago = (lastAttackMs == 0) ? -1 : (millis() - lastAttackMs) / 1000;
    String s;
    s += "Aegis IDS\n";
    s += "APs seen:      " + String(apCount) + "\n";
    s += "deauth:        " + String(deauthCount) + "\n";
    s += "disassoc:      " + String(disassocCount) + "\n";
    s += "beacon/s:      " + String(beaconRate) + "\n";
    s += "probe/s:       " + String(probeRate) + "\n";
    s += "auth/s:        " + String(authRate) + "\n";
    s += "assoc/s:       " + String(assocRate) + "\n";
    s += "attack:        " + String(atkWindow >= DEAUTH_ALERT_MIN ? "YES" : "no") + " (" +
         String(atkWindow) + "/10s)\n";
    s += "last attack:   " + (ago < 0 ? String("none") : String(ago) + "s ago") + "\n";
    s += "jamming:       " + String(jamOn ? "YES" : "no") + "\n";
    s += "trusted AP:    " + String(hasTrust ? tSsid : "(none)") + "\n";
    s += "trusted cli:   " + (trustedClients.length() ? trustedClients : String("(none)")) + "\n";
    return cli_ok(s);
  }

  if (cmd == "scan" || cmd == "aps") {
    bool force = (kvs.indexOf("refresh") >= 0) || (pos[0] == "refresh");

    // Default: read Aegis's cached results (no scan, no connection drop)
    if (!force) {
      if (scanJson.length() > 2 && scanJson != "[]") {
        String s = "> Cached AP list (run 'scan refresh' for live scan):\n";
        int start = 0;
        while (true) {
          int objStart = scanJson.indexOf("{", start);
          if (objStart < 0)
            break;
          int objEnd = scanJson.indexOf("},", objStart);
          if (objEnd < 0) {
            objEnd = scanJson.indexOf("}", objStart);
            if (objEnd < 0)
              break;
          }
          String obj = scanJson.substring(objStart, objEnd + 1);
          String ssid, bssid, ch, rssi;
          int i;
          if ((i = obj.indexOf("\"ssid\":\"")) >= 0) {
            i += 8;
            int e = obj.indexOf("\"", i);
            if (e > 0)
              ssid = obj.substring(i, e);
          }
          if ((i = obj.indexOf("\"bssid\":\"")) >= 0) {
            i += 9;
            int e = obj.indexOf("\"", i);
            if (e > 0)
              bssid = obj.substring(i, e);
          }
          if ((i = obj.indexOf("\"ch\":")) >= 0) {
            i += 5;
            int e = i;
            while (e < (int)obj.length() && (isdigit(obj[e]) || obj[e] == '-'))
              e++;
            ch = obj.substring(i, e);
          }
          if ((i = obj.indexOf("\"rssi\":")) >= 0) {
            i += 7;
            int e = i;
            while (e < (int)obj.length() && (isdigit(obj[e]) || obj[e] == '-'))
              e++;
            rssi = obj.substring(i, e);
          }
          if (bssid.length()) {
            if (!ssid.length())
              ssid = "(hidden)";
            s += "> " + ssid + "  " + bssid + "  ch" + ch + "  " + rssi + "dBm\n";
          }
          start = objEnd + 1;
        }
        return cli_ok(s);
      }
      return cli_ok(
          "> No cached scan yet. Run 'scan refresh' to trigger a live scan.\n> Your WiFi will drop "
          "for ~3s during the scan.\n");
    }

    // Force refresh: blocking scan (result likely lost due to AP drop, but cache updated)
    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING) {
      return cli_ok("> Scan already in progress. Reconnect in ~5s, run 'scan' to see results.\n");
    }
    if (n == WIFI_SCAN_FAILED) {
      WiFi.scanDelete();
    }
    WiFi.scanNetworks(false, true);  // synchronous, show hidden
    int count = WiFi.scanComplete();
    if (count > 0) {
      // Update Aegis cache so next 'scan' shows results
      buildScanJson(count);
      String s = "> Live scan (" + String(count) + " APs):\n";
      for (int i = 0; i < count; i++) {
        String ssid = WiFi.SSID(i);
        if (!ssid.length())
          ssid = "(hidden)";
        s += "> " + ssid + "  " + WiFi.BSSIDstr(i) + "  ch" + String(WiFi.channel(i)) + "  " +
             String(WiFi.RSSI(i)) + "dBm\n";
      }
      WiFi.scanDelete();
      return cli_ok(s);
    }
    WiFi.scanDelete();
    return cli_err("scan returned no results");
  }

  if (cmd == "clients") {
    String s;
    int n = 0;
    unsigned long now = millis();
    for (int i = 0; i < MAXC; i++) {
      if (!clients[i].used)
        continue;
      if ((now - clients[i].last) / 1000 > 120)
        continue;
      n++;
      char mac[18];
      snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X", clients[i].mac[0],
               clients[i].mac[1], clients[i].mac[2], clients[i].mac[3], clients[i].mac[4],
               clients[i].mac[5]);
      s += String(mac) + "  " + String(clients[i].rssi) + "dBm";
      if (clients[i].ssid[0])
        s += "  [" + String(clients[i].ssid) + "]";
      if (clients[i].rand)
        s += "  (rand)";
      s += "\n";
    }
    if (!n)
      s = "no clients in last 120s";
    return cli_ok(s);
  }

  if (cmd == "events") {
    String s;
    int n = 0;
    for (int k = 0; k < MAXE; k++) {
      int idx = (evHead - 1 - k + MAXE * 2) % MAXE;
      if (!events[idx].used)
        continue;
      n++;
      s += "[" + String(events[idx].sev) + "] " + String(events[idx].t) + "s  " + events[idx].msg +
           "\n";
    }
    if (!n)
      s = "no events yet";
    return cli_ok(s);
  }

  if (cmd == "attackers") {
    String s;
    int n = 0;
    for (int i = 0; i < MAXA; i++) {
      if (!atk[i].used)
        continue;
      n++;
      char mac[18];
      snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X", atk[i].mac[0], atk[i].mac[1],
               atk[i].mac[2], atk[i].mac[3], atk[i].mac[4], atk[i].mac[5]);
      s += String(mac) + "  " + String(atk[i].count) + " frames  " + String(atk[i].rssi) + "dBm\n";
    }
    if (!n)
      s = "no attackers detected";
    return cli_ok(s);
  }

  if (cmd == "channels") {
    uint32_t total = 0;
    for (int c = 1; c <= 14; c++)
      total += chanCount[c];
    String s;
    for (int c = 1; c <= 14; c++) {
      int pct = total ? (int)((chanCount[c] * 100) / total) : 0;
      s += "ch" + String(c) + ": " + String(chanCount[c]) + " (" + String(pct) + "%)\n";
    }
    return cli_ok(s);
  }

  if (cmd == "trust") {
    if (pos[0].length() && pos[1].length() && pos[2].length()) {
      saveTrust(pos[0], pos[1], pos[2].toInt());
      return cli_ok("trusted '" + pos[0] + "' (" + pos[1] + ") ch" + pos[2]);
    }
    if (pos[0].length() == 17 && !pos[1].length()) {
      int n = WiFi.scanComplete();
      if (n < 0) {
        WiFi.scanNetworks(true);
        return cli_ok("scanning, retry in a moment");
      }
      for (int i = 0; i < n; i++) {
        if (WiFi.BSSIDstr(i) == pos[0]) {
          String ssid = WiFi.SSID(i);
          int ch = WiFi.channel(i);
          WiFi.scanDelete();
          saveTrust(ssid, pos[0], ch);
          return cli_ok("trusted '" + ssid + "' (" + pos[0] + ") ch" + String(ch));
        }
      }
      WiFi.scanDelete();
      return cli_err("BSSID " + pos[0] + " not in current scan");
    }
    return cli_err("usage: trust <ssid> <bssid> <ch>  OR  trust <bssid>");
  }

  if (cmd == "untrust") {
    clearTrust();
    return cli_ok("trusted AP cleared");
  }

  if (cmd == "trustclient") {
    if (pos[0].length() == 17) {
      setClientTrust(pos[0], true);
      return cli_ok("trusted client " + pos[0]);
    }
    return cli_err("usage: trustclient <mac>");
  }

  if (cmd == "untrustclient") {
    if (pos[0].length() == 17) {
      setClientTrust(pos[0], false);
      return cli_ok("untrusted client " + pos[0]);
    }
    return cli_err("usage: untrustclient <mac>");
  }

  return "";
}

#pragma once
#include <Arduino.h>
struct ApInfo {
  uint8_t bssid[6];
  bool used = false;
  unsigned long last = 0;
  int rmin = 0, rmax = -127;
  uint16_t bint = 0;
  bool wep = false, tkip = false, ccmp = false, wps = false, csa = false, hidden = false;
  uint8_t pmfC = 0, pmfR = 0, std = 2;
  uint8_t ssidCount = 0;
  uint32_t ssidHash[4] = {0};
};
struct Atk {
  uint8_t mac[6];
  uint32_t count = 0;
  int rssi = 0;
  bool used = false;
};
struct NearDev {
  uint8_t mac[6];
  int rssi = 0;
  unsigned long last = 0;
  bool rand = false, alerted = false;
  char ssid[33] = {0};
  bool used = false;
};
struct Event {
  unsigned long t = 0;
  char sev = 'i';
  String msg;
  bool used = false;
};

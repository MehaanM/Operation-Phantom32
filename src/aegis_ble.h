#pragma once
#include <NimBLEDevice.h>

/*
 * Aegis BLE module (passive): inventories nearby Bluetooth-LE devices, detects
 * BLE advertising floods (pairing-popup spam), and flags Find My / AirTag-style
 * unwanted trackers. Listen-only. Uses NimBLE for light WiFi/BLE coexistence.
 */

struct BleDev { char addr[18]; char name[24]; int rssi; bool tracker; unsigned long last; bool used; };
const int MAXBLE = 32;
BleDev bdev[MAXBLE];
volatile uint32_t bleAdvCount = 0;
uint32_t bleRate = 0, lastBleAdv = 0; bool bleSpam = false; int bleTrackerCount = 0, bleCount = 0;

BleDev* bleFind(const char* a) {
  int fi = -1;
  for (int i = 0; i < MAXBLE; i++) { if (bdev[i].used && strcmp(bdev[i].addr, a) == 0) return &bdev[i]; if (!bdev[i].used && fi < 0) fi = i; }
  if (fi < 0) { unsigned long o = ~0UL; for (int i = 0; i < MAXBLE; i++) if (bdev[i].last < o) { o = bdev[i].last; fi = i; } }
  BleDev* d = &bdev[fi]; memset(d, 0, sizeof(BleDev)); strncpy(d->addr, a, 17); d->used = true; return d;
}

class AegisBleCB : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice* ad) override {
    bleAdvCount++;
    std::string as = ad->getAddress().toString();
    BleDev* d = bleFind(as.c_str());
    d->rssi = ad->getRSSI(); d->last = millis();
    std::string nm = ad->getName(); if (!nm.empty()) strncpy(d->name, nm.c_str(), 23);
    // Find My / AirTag: Apple company 0x004C, Find My offline-finding type 0x12
    std::string md = ad->getManufacturerData();
    if (md.size() >= 3 && (uint8_t)md[0] == 0x4C && (uint8_t)md[1] == 0x00 && (uint8_t)md[2] == 0x12) d->tracker = true;
  }
};
AegisBleCB bleCB;

void bleSetup() {
  for (int i = 0; i < MAXBLE; i++) bdev[i].used = false;
  NimBLEDevice::init("");
  NimBLEScan* s = NimBLEDevice::getScan();
  s->setAdvertisedDeviceCallbacks(&bleCB, false);
  s->setActiveScan(false);           // passive scan — lighter, better WiFi coexistence
  s->setInterval(1000); s->setWindow(50);   // low duty cycle — keep the radio free for WiFi/AP
  s->start(0, nullptr, false);       // continuous
}

void bleTick() {                     // call ~once a second
  bleRate = bleAdvCount - lastBleAdv; lastBleAdv = bleAdvCount;
  bleSpam = (bleRate > 400);         // very high advertising rate = BLE/pairing spam
  int tc = 0, c = 0; unsigned long now = millis();
  for (int i = 0; i < MAXBLE; i++) if (bdev[i].used && (now - bdev[i].last) / 1000 <= 60) { c++; if (bdev[i].tracker) tc++; }
  bleTrackerCount = tc; bleCount = c;
}

void handleBle() {
  String j = "["; bool f = true; unsigned long now = millis();
  for (int i = 0; i < MAXBLE; i++) {
    if (!bdev[i].used) continue; unsigned long ago = (now - bdev[i].last) / 1000; if (ago > 60) continue;
    if (!f) j += ","; f = false;
    j += "{\"addr\":\"" + String(bdev[i].addr) + "\",\"name\":\"" + String(bdev[i].name) + "\",\"rssi\":" + String(bdev[i].rssi) + ",\"tracker\":" + String(bdev[i].tracker ? 1 : 0) + ",\"ago\":" + String(ago) + "}";
  }
  j += "]"; server.send(200, "application/json", j);
}

String bleStats() {
  return "\"ble\":" + String(bleCount) + ",\"bleRate\":" + String(bleRate) + ",\"bleSpam\":" + String(bleSpam ? 1 : 0) + ",\"bleTrackers\":" + String(bleTrackerCount);
}

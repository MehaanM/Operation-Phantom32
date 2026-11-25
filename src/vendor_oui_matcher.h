#pragma once
// vendor_oui_matcher.h — Fast OUI vendor matching with known device categories.
// Provides quick category classification beyond raw OUI lookup.
// Prefix: vom_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

// ─── Device categories ────────────────────────────────────────────────────────
enum VomCategory {
  VOM_CAT_UNKNOWN = 0,
  VOM_CAT_PHONE = 1,
  VOM_CAT_LAPTOP = 2,
  VOM_CAT_AP = 3,
  VOM_CAT_IOT = 4,
  VOM_CAT_CAMERA = 5,
  VOM_CAT_PRINTER = 6,
  VOM_CAT_TV = 7,
  VOM_CAT_WEARABLE = 8,
  VOM_CAT_VEHICLE = 9,
  VOM_CAT_MEDICAL = 10,
  VOM_CAT_INFRA = 11,
};

static const char *VOM_CAT_NAMES[] = {"unknown",  "phone",   "laptop",  "ap",
                                      "iot",      "camera",  "printer", "tv",
                                      "wearable", "vehicle", "medical", "infra"};

// ─── Compact OUI table entry ──────────────────────────────────────────────────
struct VomEntry {
  uint8_t oui[3];
  VomCategory cat;
  char vendor[20];
};

// ─── Known OUI → category table (compact, authoritative subset) ───────────────
static const VomEntry VOM_TABLE[] PROGMEM = {
    // Apple
    {{0x00, 0x0A, 0x27}, VOM_CAT_LAPTOP, "Apple"},
    {{0x00, 0x0D, 0x93}, VOM_CAT_LAPTOP, "Apple"},
    {{0x00, 0x11, 0x24}, VOM_CAT_PHONE, "Apple"},
    {{0x00, 0x16, 0xCB}, VOM_CAT_LAPTOP, "Apple"},
    {{0x00, 0x17, 0xF2}, VOM_CAT_AP, "Apple Airport"},
    {{0x00, 0x1B, 0x63}, VOM_CAT_LAPTOP, "Apple"},
    {{0x00, 0x1C, 0xB3}, VOM_CAT_LAPTOP, "Apple"},
    {{0x00, 0x1E, 0x52}, VOM_CAT_LAPTOP, "Apple"},
    {{0x00, 0x1F, 0x5B}, VOM_CAT_LAPTOP, "Apple"},
    {{0x00, 0x21, 0xE9}, VOM_CAT_LAPTOP, "Apple"},
    {{0x00, 0x22, 0x41}, VOM_CAT_PHONE, "Apple iPhone"},
    {{0x00, 0x23, 0x12}, VOM_CAT_LAPTOP, "Apple"},
    {{0x00, 0x23, 0x32}, VOM_CAT_LAPTOP, "Apple"},
    {{0x00, 0x25, 0x00}, VOM_CAT_LAPTOP, "Apple"},
    {{0x00, 0x25, 0xBC}, VOM_CAT_LAPTOP, "Apple"},
    {{0x00, 0x26, 0xB9}, VOM_CAT_LAPTOP, "Apple"},
    {{0x00, 0x26, 0xBB}, VOM_CAT_AP, "Apple Airport"},
    // Samsung
    {{0x00, 0x07, 0xAB}, VOM_CAT_PHONE, "Samsung"},
    {{0x00, 0x12, 0x47}, VOM_CAT_PHONE, "Samsung"},
    {{0x00, 0x15, 0x99}, VOM_CAT_PHONE, "Samsung"},
    {{0x00, 0x16, 0x32}, VOM_CAT_TV, "Samsung TV"},
    {{0x00, 0x1A, 0x8A}, VOM_CAT_PHONE, "Samsung"},
    {{0x00, 0x1D, 0x25}, VOM_CAT_PHONE, "Samsung"},
    {{0x00, 0x21, 0x19}, VOM_CAT_PHONE, "Samsung"},
    {{0x00, 0x24, 0x54}, VOM_CAT_PHONE, "Samsung"},
    {{0x00, 0x26, 0x37}, VOM_CAT_PHONE, "Samsung"},
    // Cisco / networking
    {{0x00, 0x00, 0x0C}, VOM_CAT_INFRA, "Cisco"},
    {{0x00, 0x01, 0x42}, VOM_CAT_INFRA, "Cisco"},
    {{0x00, 0x01, 0x43}, VOM_CAT_INFRA, "Cisco"},
    {{0x00, 0x01, 0x63}, VOM_CAT_INFRA, "Cisco"},
    {{0x00, 0x01, 0x64}, VOM_CAT_AP, "Cisco AP"},
    {{0x00, 0x0B, 0x85}, VOM_CAT_AP, "Cisco AP"},
    {{0x00, 0x0D, 0xBD}, VOM_CAT_AP, "Cisco AP"},
    {{0x00, 0x11, 0x20}, VOM_CAT_AP, "Cisco AP"},
    {{0x00, 0x12, 0xDA}, VOM_CAT_AP, "Cisco AP"},
    {{0x00, 0x13, 0x19}, VOM_CAT_AP, "Cisco AP"},
    // Ubiquiti
    {{0x00, 0x15, 0x6D}, VOM_CAT_AP, "Ubiquiti"},
    {{0x00, 0x27, 0x22}, VOM_CAT_AP, "Ubiquiti"},
    {{0x04, 0x18, 0xD6}, VOM_CAT_AP, "Ubiquiti"},
    {{0x24, 0xA4, 0x3C}, VOM_CAT_AP, "Ubiquiti"},
    {{0x44, 0xD9, 0xE7}, VOM_CAT_AP, "Ubiquiti"},
    {{0x68, 0x72, 0x51}, VOM_CAT_AP, "Ubiquiti"},
    {{0x78, 0x8A, 0x20}, VOM_CAT_AP, "Ubiquiti"},
    {{0xDC, 0x9F, 0xDB}, VOM_CAT_AP, "Ubiquiti"},
    {{0xF0, 0x9F, 0xC2}, VOM_CAT_AP, "Ubiquiti"},
    // Google
    {{0x3C, 0x5A, 0xB4}, VOM_CAT_IOT, "Google"},
    {{0x54, 0x60, 0x09}, VOM_CAT_IOT, "Google"},
    {{0x94, 0x65, 0x2D}, VOM_CAT_IOT, "Google"},
    {{0xA4, 0x77, 0x33}, VOM_CAT_PHONE, "Google Pixel"},
    // Amazon
    {{0x00, 0xFC, 0x8B}, VOM_CAT_IOT, "Amazon Echo"},
    {{0x34, 0xD2, 0x70}, VOM_CAT_IOT, "Amazon Echo"},
    {{0x40, 0xB4, 0xCD}, VOM_CAT_IOT, "Amazon"},
    {{0x68, 0x37, 0xE9}, VOM_CAT_IOT, "Amazon"},
    {{0xA0, 0x02, 0xDC}, VOM_CAT_IOT, "Amazon Echo"},
    {{0xBC, 0xA8, 0xA6}, VOM_CAT_IOT, "Amazon Echo"},
    {{0xCC, 0xF7, 0x35}, VOM_CAT_IOT, "Amazon"},
    {{0xF0, 0x81, 0x73}, VOM_CAT_IOT, "Amazon"},
    // Philips Hue / smart home
    {{0x00, 0x17, 0x88}, VOM_CAT_IOT, "Philips Hue"},
    {{0xEC, 0xB5, 0xFA}, VOM_CAT_IOT, "Philips Hue"},
    // Nest / Google Home
    {{0x18, 0xB4, 0x30}, VOM_CAT_IOT, "Nest"},
    {{0x64, 0x16, 0x66}, VOM_CAT_IOT, "Nest"},
    // HP printers
    {{0x00, 0x01, 0xE6}, VOM_CAT_PRINTER, "HP"},
    {{0x00, 0x11, 0x0A}, VOM_CAT_PRINTER, "HP"},
    {{0x00, 0x12, 0x79}, VOM_CAT_PRINTER, "HP"},
    {{0x00, 0x14, 0x38}, VOM_CAT_PRINTER, "HP"},
    {{0x00, 0x17, 0xA4}, VOM_CAT_PRINTER, "HP"},
    {{0x00, 0x18, 0xFE}, VOM_CAT_PRINTER, "HP"},
    {{0x00, 0x1E, 0x0B}, VOM_CAT_PRINTER, "HP"},
    {{0x00, 0x21, 0x5A}, VOM_CAT_PRINTER, "HP"},
    {{0x00, 0x24, 0x81}, VOM_CAT_PRINTER, "HP"},
    {{0x3C, 0xD9, 0x2B}, VOM_CAT_PRINTER, "HP"},
    {{0x9C, 0x8E, 0x99}, VOM_CAT_PRINTER, "HP"},
    {{0xA0, 0xD3, 0xC1}, VOM_CAT_PRINTER, "HP"},
    {{0xA4, 0xCA, 0xA0}, VOM_CAT_PRINTER, "HP"},
    {{0xB0, 0x5A, 0xDA}, VOM_CAT_PRINTER, "HP"},
    {{0xBC, 0xEA, 0xFA}, VOM_CAT_PRINTER, "HP"},
    {{0xC4, 0x34, 0x6B}, VOM_CAT_PRINTER, "HP"},
    {{0xD8, 0x08, 0x2F}, VOM_CAT_PRINTER, "HP"},
    {{0xEC, 0x9B, 0x8B}, VOM_CAT_PRINTER, "HP"},
    {{0xF4, 0x81, 0x39}, VOM_CAT_PRINTER, "HP"},
    {{0xF8, 0x0D, 0xAC}, VOM_CAT_PRINTER, "HP"},
    // Sentinel
    {{0x00, 0x00, 0x00}, VOM_CAT_UNKNOWN, ""}};

#define VOM_TABLE_SIZE (sizeof(VOM_TABLE) / sizeof(VOM_TABLE[0]) - 1)

// ─── Lookup ───────────────────────────────────────────────────────────────────
static VomCategory vom_lookup(const uint8_t *mac, char vendor_out[20]) {
  for (size_t i = 0; i < VOM_TABLE_SIZE; i++) {
    VomEntry e;
    memcpy_P(&e, &VOM_TABLE[i], sizeof(e));
    if (mac[0] == e.oui[0] && mac[1] == e.oui[1] && mac[2] == e.oui[2]) {
      if (vendor_out)
        strlcpy(vendor_out, e.vendor, 20);
      return e.cat;
    }
  }
  if (vendor_out)
    vendor_out[0] = 0;
  return VOM_CAT_UNKNOWN;
}

// ─── Stats ────────────────────────────────────────────────────────────────────
static struct {
  uint32_t total_lookups;
  uint32_t hits;
} g_vom = {};

VomCategory vom_query(const uint8_t *mac, char *vendor_out) {
  g_vom.total_lookups++;
  VomCategory c = vom_lookup(mac, vendor_out);
  if (c != VOM_CAT_UNKNOWN)
    g_vom.hits++;
  return c;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void vom_routes() {
  server.on("/api/vom/lookup", HTTP_GET, []() {
    if (!server.hasArg("mac")) {
      server.send(400, "application/json", "{\"error\":\"missing mac\"}");
      return;
    }
    uint8_t mac[6] = {};
    sscanf(server.arg("mac").c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &mac[0], &mac[1], &mac[2],
           &mac[3], &mac[4], &mac[5]);
    char vendor[20];
    VomCategory cat = vom_query(mac, vendor);
    String s = "{\"category\":\"" + String(VOM_CAT_NAMES[cat]) + "\"";
    s += ",\"vendor\":\"" + String(vendor) + "\"}";
    server.send(200, "application/json", s);
  });

  server.on("/api/vom/stats", HTTP_GET, []() {
    String s = "{\"lookups\":" + String(g_vom.total_lookups);
    s += ",\"hits\":" + String(g_vom.hits);
    s += ",\"table_size\":" + String((int)VOM_TABLE_SIZE) + "}";
    server.send(200, "application/json", s);
  });
}

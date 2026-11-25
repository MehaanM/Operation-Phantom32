#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <string.h>
#include "module.h"

// ── OUI Full Database ─────────────────────────────────────────────────────────
//
// Comprehensive IEEE OUI vendor lookup with device classification,
// geographic data, and product family hints for 802.11 frame analysis.
//
// Provides:
//   ouf_lookup(mac)       — returns OuiFullEntry* or nullptr
//   ouf_vendor(mac)       — returns vendor string
//   ouf_category(mac)     — returns OUF_CAT_* enum
//   ouf_is_random(mac)    — detects locally-administered / random MACs
//   ouf_routes()          — registers /api/oui/* HTTP routes

// ── Category codes ────────────────────────────────────────────────────────────
enum OufCat : uint8_t {
  OUF_CAT_UNKNOWN = 0,
  OUF_CAT_PHONE = 1,        // Smartphone / tablet
  OUF_CAT_LAPTOP = 2,       // Laptop / desktop NIC
  OUF_CAT_AP = 3,           // Access point / router
  OUF_CAT_CAMERA = 4,       // IP camera / NVR
  OUF_CAT_IOT = 5,          // IoT / smart-home device
  OUF_CAT_PRINTER = 6,      // Printer / MFP
  OUF_CAT_TV = 7,           // Smart TV / streaming stick
  OUF_CAT_WEARABLE = 8,     // Watch / fitness band
  OUF_CAT_VEHICLE = 9,      // Automotive / in-car
  OUF_CAT_INFRA = 10,       // Switch / enterprise infra
  OUF_CAT_GAMING = 11,      // Gaming console / controller
  OUF_CAT_VIRTUAL = 12,     // VM / hypervisor virtual NIC
  OUF_CAT_MEDICAL = 13,     // Medical device
  OUF_CAT_INDUSTRIAL = 14,  // Industrial / SCADA
  OUF_CAT_AUDIO = 15,       // Speaker / headphones / AV
};

// ── Region codes ─────────────────────────────────────────────────────────────
enum OufRegion : uint8_t {
  OUF_RGN_UNKNOWN = 0,
  OUF_RGN_US = 1,
  OUF_RGN_CN = 2,
  OUF_RGN_KR = 3,
  OUF_RGN_JP = 4,
  OUF_RGN_TW = 5,
  OUF_RGN_EU = 6,
  OUF_RGN_IN = 7,
  OUF_RGN_SG = 8,
};

// ── Entry struct ─────────────────────────────────────────────────────────────
struct OuiFullEntry {
  uint8_t prefix[3];
  const char *vendor;
  const char *products;  // short hint string
  OufCat category;
  OufRegion region;
};

// ── OUI Database ─────────────────────────────────────────────────────────────
// Source: IEEE public OUI registry (illustrative subset)
static const OuiFullEntry OUF_TABLE[] = {
    // ── Apple Inc ───────────────────────────────────────────────────────────────
    {{0x00, 0x03, 0x93}, "Apple Inc", "Mac/Airport", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x00, 0x0A, 0x27}, "Apple Inc", "iPhone/iPad", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x00, 0x0A, 0x95}, "Apple Inc", "MacBook Pro", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x00, 0x11, 0x24}, "Apple Inc", "Mac/Airport", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x00, 0x14, 0x51}, "Apple Inc", "Mac/Airport", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x00, 0x16, 0xCB}, "Apple Inc", "MacBook/iMac", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x00, 0x17, 0xF2}, "Apple Inc", "AirPort Extreme", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x1B, 0x63}, "Apple Inc", "Mac Mini", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x00, 0x1C, 0xB3}, "Apple Inc", "iPhone 3G", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x00, 0x1D, 0x4F}, "Apple Inc", "iPhone/iPod", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x00, 0x1E, 0x52}, "Apple Inc", "AirPort/Time Capsule", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x1F, 0x5B}, "Apple Inc", "Mac Pro", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x00, 0x1F, 0xF3}, "Apple Inc", "iPhone 3GS", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x00, 0x21, 0xE9}, "Apple Inc", "iMac/MacBook", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x00, 0x22, 0x41}, "Apple Inc", "iPhone/iPad", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x00, 0x23, 0x12}, "Apple Inc", "iPad/iPod", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x00, 0x23, 0xDF}, "Apple Inc", "iPhone 4", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x00, 0x24, 0x36}, "Apple Inc", "iMac", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x00, 0x25, 0x00}, "Apple Inc", "MacBook Air", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x00, 0x25, 0x4B}, "Apple Inc", "Mac Mini", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x00, 0x25, 0xBC}, "Apple Inc", "iPad", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x00, 0x26, 0x08}, "Apple Inc", "iPhone 4", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x00, 0x26, 0xB0}, "Apple Inc", "AirPort Express", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x26, 0xBB}, "Apple Inc", "Apple TV", OUF_CAT_TV, OUF_RGN_US},
    {{0x00, 0x30, 0x65}, "Apple Inc", "Mac/Airport", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x00, 0x50, 0xE4}, "Apple Inc", "Airport Base Station", OUF_CAT_AP, OUF_RGN_US},
    {{0x04, 0x0C, 0xCE}, "Apple Inc", "iPhone/iPad", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x04, 0x1E, 0x64}, "Apple Inc", "iPhone 5", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x04, 0x48, 0x9A}, "Apple Inc", "iPad 2", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x04, 0x52, 0xF3}, "Apple Inc", "MacBook Pro", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x04, 0x54, 0x53}, "Apple Inc", "iPad mini", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x04, 0x69, 0xF8}, "Apple Inc", "iPhone 5s", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x04, 0x9F, 0xCA}, "Apple Inc", "iPhone 6", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x04, 0xDB, 0x56}, "Apple Inc", "iPad Air", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x04, 0xF1, 0x3E}, "Apple Inc", "Apple TV 4K", OUF_CAT_TV, OUF_RGN_US},
    {{0x08, 0x00, 0x07}, "Apple Inc", "Apple II", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x08, 0x6D, 0x41}, "Apple Inc", "iPad Air 2", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x08, 0x70, 0x45}, "Apple Inc", "iPhone 6 Plus", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x08, 0x74, 0x02}, "Apple Inc", "MacBook Pro Retina", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x08, 0x9E, 0x08}, "Apple Inc", "iPhone 6s", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x08, 0xF4, 0xAB}, "Apple Inc", "iPad Pro", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x0C, 0x30, 0x21}, "Apple Inc", "iPhone 7", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x0C, 0x3E, 0x9F}, "Apple Inc", "MacBook Air", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x0C, 0x4D, 0xE9}, "Apple Inc", "iMac Pro", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x0C, 0x51, 0x01}, "Apple Inc", "iPhone 7 Plus", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x0C, 0x74, 0xC2}, "Apple Inc", "iPad Pro 10.5", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x0C, 0x77, 0x1A}, "Apple Inc", "Apple Watch S3", OUF_CAT_WEARABLE, OUF_RGN_US},
    {{0x0C, 0xBC, 0x9F}, "Apple Inc", "iPhone X", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x0C, 0xD5, 0x02}, "Apple Inc", "HomePod", OUF_CAT_AUDIO, OUF_RGN_US},
    {{0x10, 0x40, 0xF3}, "Apple Inc", "iPhone XS", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x10, 0x41, 0x7F}, "Apple Inc", "iPad 6th Gen", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x10, 0x93, 0xE9}, "Apple Inc", "iPhone XR", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x10, 0x9A, 0xDD}, "Apple Inc", "MacBook Pro 2018", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x10, 0xDD, 0xB1}, "Apple Inc", "iPad mini 5", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x14, 0x10, 0x9F}, "Apple Inc", "iPhone 11", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x14, 0x5A, 0x05}, "Apple Inc", "iPad Air 3", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x14, 0x7D, 0xDA}, "Apple Inc", "MacBook Pro 16\"", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x14, 0x8F, 0xC6}, "Apple Inc", "iPhone 11 Pro", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x14, 0x99, 0xE2}, "Apple Inc", "iPhone 11 Pro Max", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x14, 0xBD, 0x61}, "Apple Inc", "Apple Watch S5", OUF_CAT_WEARABLE, OUF_RGN_US},
    {{0x18, 0x20, 0x32}, "Apple Inc", "iPhone SE 2020", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x18, 0x34, 0x51}, "Apple Inc", "iPad Pro 2020", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x18, 0x65, 0x90}, "Apple Inc", "MacBook Air M1", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x18, 0x81, 0x0E}, "Apple Inc", "iPhone 12", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x18, 0x9E, 0xFC}, "Apple Inc", "iPhone 12 Pro", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x18, 0xAF, 0x61}, "Apple Inc", "Apple TV 4K 2021", OUF_CAT_TV, OUF_RGN_US},
    {{0x18, 0xE7, 0xF4}, "Apple Inc", "iPad mini 6", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x1C, 0x1A, 0xC0}, "Apple Inc", "iPhone 13", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x1C, 0x36, 0xBB}, "Apple Inc", "iPhone 13 Pro", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x1C, 0x5C, 0xF2}, "Apple Inc", "MacBook Pro M1 Pro", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x1C, 0x91, 0x48}, "Apple Inc", "iPad Air 5", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x1C, 0xAB, 0xA7}, "Apple Inc", "Apple Watch S7", OUF_CAT_WEARABLE, OUF_RGN_US},
    {{0x20, 0x3C, 0xAE}, "Apple Inc", "iPhone 14", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x20, 0x76, 0x93}, "Apple Inc", "iPhone 14 Pro", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x20, 0x78, 0xF0}, "Apple Inc", "MacBook Pro M2", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x20, 0x7D, 0x74}, "Apple Inc", "iPad Pro M2", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x20, 0x9B, 0xCD}, "Apple Inc", "Apple Watch S8", OUF_CAT_WEARABLE, OUF_RGN_US},
    {{0x20, 0xA2, 0xE4}, "Apple Inc", "HomePod mini", OUF_CAT_AUDIO, OUF_RGN_US},
    {{0x20, 0xAB, 0x37}, "Apple Inc", "iPhone 15", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x20, 0xC9, 0xD0}, "Apple Inc", "MacBook Air M2", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x24, 0x1E, 0xEB}, "Apple Inc", "iPhone 15 Pro", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x24, 0x5B, 0xA7}, "Apple Inc", "iPad 10th Gen", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x24, 0xA0, 0x74}, "Apple Inc", "Apple Watch Ultra", OUF_CAT_WEARABLE, OUF_RGN_US},
    {{0x24, 0xAB, 0x81}, "Apple Inc", "MacBook Pro M3", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x24, 0xE3, 0x14}, "Apple Inc", "iPhone 15 Plus", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x28, 0x0B, 0x5C}, "Apple Inc", "Apple Vision Pro", OUF_CAT_WEARABLE, OUF_RGN_US},
    {{0x28, 0x37, 0x37}, "Apple Inc", "iMac M3", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x28, 0x6A, 0xBA}, "Apple Inc", "MacBook Pro 14\"", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x28, 0xA0, 0x2B}, "Apple Inc", "iPhone SE 3rd Gen", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x28, 0xCF, 0xDA}, "Apple Inc", "iPad Pro 11\" M2", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x28, 0xCF, 0xE9}, "Apple Inc", "Apple TV HD", OUF_CAT_TV, OUF_RGN_US},
    {{0x28, 0xE0, 0x2C}, "Apple Inc", "AirPods Pro 2", OUF_CAT_AUDIO, OUF_RGN_US},
    {{0x2C, 0x1F, 0x23}, "Apple Inc", "iPhone 16", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x2C, 0x20, 0x0B}, "Apple Inc", "MacBook Air M3", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x2C, 0x61, 0xF6}, "Apple Inc", "Apple Watch S9", OUF_CAT_WEARABLE, OUF_RGN_US},
    {{0x2C, 0xBE, 0x08}, "Apple Inc", "iPad mini 7", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x2C, 0xF0, 0xEE}, "Apple Inc", "iPhone 16 Pro Max", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x30, 0x10, 0xB3}, "Apple Inc", "iMac 24\" M3", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x30, 0x35, 0xAD}, "Apple Inc", "MacBook Pro M3 Max", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x30, 0x63, 0x6B}, "Apple Inc", "iPad Air M2", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x30, 0x90, 0xAB}, "Apple Inc", "iPhone 16 Pro", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x30, 0xF7, 0xC5}, "Apple Inc", "Apple Watch Ultra 2", OUF_CAT_WEARABLE, OUF_RGN_US},
    {{0x34, 0x08, 0xBC}, "Apple Inc", "MacBook/MacPro", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x34, 0x36, 0x3B}, "Apple Inc", "iPhone/iPad", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x34, 0x7C, 0x25}, "Apple Inc", "iPad Pro", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x38, 0x0F, 0x4A}, "Apple Inc", "iPhone/iPad", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x38, 0x71, 0xDE}, "Apple Inc", "Mac/MacBook", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x38, 0xB5, 0x4D}, "Apple Inc", "Apple Watch", OUF_CAT_WEARABLE, OUF_RGN_US},
    {{0x3C, 0x07, 0x71}, "Apple Inc", "iMac", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x3C, 0x2E, 0xFF}, "Apple Inc", "MacBook Pro", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x3C, 0xD0, 0xF8}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x40, 0x30, 0x04}, "Apple Inc", "iPhone/MacBook", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x40, 0x3C, 0xFC}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x40, 0x6C, 0x8F}, "Apple Inc", "Mac/Airport", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x40, 0x98, 0xAD}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x40, 0xA6, 0xD9}, "Apple Inc", "Apple Watch", OUF_CAT_WEARABLE, OUF_RGN_US},
    {{0x40, 0xBC, 0x60}, "Apple Inc", "iPad", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x40, 0xD3, 0x2D}, "Apple Inc", "MacBook Pro", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x44, 0x00, 0x10}, "Apple Inc", "iPhone 6s", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x44, 0x2A, 0x60}, "Apple Inc", "iPad", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x44, 0x65, 0x0D}, "Apple Inc", "iPhone 7", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x44, 0xD8, 0x84}, "Apple Inc", "iMac/MacBook", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x44, 0xFB, 0x42}, "Apple Inc", "iPhone/iPad", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x48, 0x43, 0x7C}, "Apple Inc", "MacBook Pro", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x48, 0x4B, 0xAA}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x48, 0x60, 0xBC}, "Apple Inc", "iPad Pro", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x48, 0x74, 0x6E}, "Apple Inc", "Mac mini", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x4C, 0x57, 0xCA}, "Apple Inc", "Apple Watch SE", OUF_CAT_WEARABLE, OUF_RGN_US},
    {{0x4C, 0x8D, 0x79}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x4C, 0xB1, 0xCD}, "Apple Inc", "iPad Air", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x50, 0xA6, 0x71}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x50, 0xBC, 0x96}, "Apple Inc", "MacBook", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x50, 0xDE, 0x06}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x50, 0xEA, 0xD6}, "Apple Inc", "AirPods", OUF_CAT_AUDIO, OUF_RGN_US},
    {{0x54, 0x26, 0x96}, "Apple Inc", "iPhone 8", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x54, 0x33, 0xCB}, "Apple Inc", "Apple Watch S4", OUF_CAT_WEARABLE, OUF_RGN_US},
    {{0x54, 0x4E, 0x90}, "Apple Inc", "Mac Pro 2019", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x54, 0x72, 0x4F}, "Apple Inc", "iPad", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x54, 0xAE, 0x27}, "Apple Inc", "iPhone XS Max", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x54, 0xE4, 0x3A}, "Apple Inc", "iMac/MacBook", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x58, 0x1F, 0xAA}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x58, 0x55, 0xCA}, "Apple Inc", "Apple TV 3rd Gen", OUF_CAT_TV, OUF_RGN_US},
    {{0x58, 0x7F, 0x57}, "Apple Inc", "Mac mini M2", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x58, 0xB0, 0x35}, "Apple Inc", "iPhone 12 mini", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x5C, 0x59, 0x48}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x5C, 0x95, 0xAE}, "Apple Inc", "MacBook Pro", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x5C, 0xAD, 0xCF}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x5C, 0xF9, 0x38}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x60, 0x03, 0x08}, "Apple Inc", "Mac Pro", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x60, 0x33, 0x4B}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x60, 0x69, 0x44}, "Apple Inc", "iPad", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x60, 0x92, 0x17}, "Apple Inc", "MacBook Air", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x60, 0xC5, 0x47}, "Apple Inc", "iPhone 6s Plus", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x60, 0xF8, 0x1D}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x60, 0xFA, 0xCD}, "Apple Inc", "iPad", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x64, 0x20, 0x0C}, "Apple Inc", "Mac/MacBook", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x64, 0x76, 0xBA}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x64, 0x9A, 0xBE}, "Apple Inc", "iPad", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x64, 0xB9, 0xE8}, "Apple Inc", "MacBook Pro", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x64, 0xE6, 0x82}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x68, 0x09, 0x27}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x68, 0x5B, 0x35}, "Apple Inc", "iPad Pro", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x68, 0x9C, 0x70}, "Apple Inc", "Mac mini", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x68, 0xAE, 0x20}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x68, 0xD9, 0x3C}, "Apple Inc", "MacBook Pro", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x68, 0xFB, 0x7E}, "Apple Inc", "iMac", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x6C, 0x19, 0x8F}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x6C, 0x40, 0x08}, "Apple Inc", "iPad", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x6C, 0x72, 0x20}, "Apple Inc", "Apple Watch", OUF_CAT_WEARABLE, OUF_RGN_US},
    {{0x6C, 0x94, 0xF8}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x6C, 0x96, 0xCF}, "Apple Inc", "MacBook Pro", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x6C, 0xAB, 0x31}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x70, 0x3E, 0xAC}, "Apple Inc", "Mac", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x70, 0x48, 0x0F}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x70, 0x73, 0xCB}, "Apple Inc", "iPad", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x70, 0x81, 0xEB}, "Apple Inc", "Apple Watch", OUF_CAT_WEARABLE, OUF_RGN_US},
    {{0x70, 0xA3, 0xB3}, "Apple Inc", "Mac Pro", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x70, 0xCD, 0x60}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x70, 0xDE, 0xE2}, "Apple Inc", "MacBook", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x70, 0xE7, 0x2C}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x74, 0x1B, 0xB2}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x74, 0xE1, 0xB6}, "Apple Inc", "iPad", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x74, 0xE2, 0xF5}, "Apple Inc", "MacBook Pro", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x78, 0x31, 0xC1}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x78, 0x4F, 0x43}, "Apple Inc", "iPad", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x78, 0x67, 0xD7}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x78, 0x88, 0x6D}, "Apple Inc", "Mac mini", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x78, 0xA3, 0xE4}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x78, 0xFD, 0x94}, "Apple Inc", "MacBook Pro", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x7C, 0x01, 0x91}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x7C, 0x04, 0xD0}, "Apple Inc", "iPad", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x7C, 0x11, 0xBE}, "Apple Inc", "Mac Pro", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x7C, 0x50, 0x49}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x7C, 0x6D, 0x62}, "Apple Inc", "Apple Watch", OUF_CAT_WEARABLE, OUF_RGN_US},
    {{0x7C, 0xD1, 0xC3}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x7C, 0xF0, 0x5F}, "Apple Inc", "iMac", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x80, 0x00, 0x6E}, "Apple Inc", "Mac", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x80, 0x49, 0x71}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x80, 0x92, 0x9F}, "Apple Inc", "iPad", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x80, 0xBE, 0x05}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x80, 0xEA, 0x96}, "Apple Inc", "Mac mini M1", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x84, 0x29, 0x99}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x84, 0x38, 0x38}, "Apple Inc", "iPad", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x84, 0x40, 0x8B}, "Apple Inc", "Mac", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x84, 0x85, 0x06}, "Apple Inc", "Apple TV", OUF_CAT_TV, OUF_RGN_US},
    {{0x84, 0x89, 0xAD}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x84, 0xA1, 0x34}, "Apple Inc", "MacBook Pro", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x84, 0xFC, 0xAC}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x88, 0x1F, 0xA1}, "Apple Inc", "Mac", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x88, 0x53, 0x2E}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x88, 0x63, 0xDF}, "Apple Inc", "iPad Pro", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x88, 0x66, 0xA5}, "Apple Inc", "MacBook Pro", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x88, 0xAE, 0x07}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x88, 0xCB, 0x87}, "Apple Inc", "Mac mini", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x88, 0xE8, 0x7F}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x8C, 0x00, 0x6D}, "Apple Inc", "iPad", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x8C, 0x17, 0x59}, "Apple Inc", "Mac", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x8C, 0x2D, 0xAA}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x8C, 0x49, 0x62}, "Apple Inc", "Apple Watch", OUF_CAT_WEARABLE, OUF_RGN_US},
    {{0x8C, 0x85, 0x90}, "Apple Inc", "MacBook Pro", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x90, 0x00, 0x4E}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x90, 0x60, 0xF1}, "Apple Inc", "Mac", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x90, 0x72, 0x40}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x90, 0x84, 0x0D}, "Apple Inc", "iPad", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x90, 0x9A, 0x4A}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x90, 0xC1, 0xC6}, "Apple Inc", "MacBook Pro", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x94, 0x65, 0x2D}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x94, 0xF6, 0x65}, "Apple Inc", "Mac mini", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x98, 0x01, 0xA7}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x98, 0x10, 0xE8}, "Apple Inc", "Mac", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x98, 0x46, 0x0A}, "Apple Inc", "iPad", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x98, 0x5A, 0xEB}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x98, 0x9E, 0x63}, "Apple Inc", "MacBook Pro", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x98, 0xD6, 0xBB}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x98, 0xE0, 0xD9}, "Apple Inc", "iPad", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x9C, 0x04, 0xEB}, "Apple Inc", "Mac", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x9C, 0x20, 0x7B}, "Apple Inc", "Apple Watch", OUF_CAT_WEARABLE, OUF_RGN_US},
    {{0x9C, 0x35, 0xEB}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x9C, 0x4F, 0xDA}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x9C, 0x84, 0xBF}, "Apple Inc", "MacBook Pro", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x9C, 0xF3, 0x87}, "Apple Inc", "iMac", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xA0, 0x18, 0x28}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xA0, 0x3B, 0xE3}, "Apple Inc", "iPad", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xA0, 0x6C, 0xEC}, "Apple Inc", "Mac", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xA4, 0x5E, 0x60}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xA4, 0xB1, 0x97}, "Apple Inc", "MacBook Pro", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xA4, 0xB8, 0x05}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xA4, 0xD1, 0x8C}, "Apple Inc", "Mac mini", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xA8, 0x20, 0x66}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xA8, 0x5B, 0x4E}, "Apple Inc", "iPad", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xA8, 0x86, 0xDD}, "Apple Inc", "MacBook Pro", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xA8, 0x96, 0x8A}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xA8, 0xBB, 0xCF}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xAC, 0x1F, 0x74}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xAC, 0x3C, 0x0B}, "Apple Inc", "Mac", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xAC, 0x7F, 0x3E}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xAC, 0x87, 0xA3}, "Apple Inc", "iPad", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xAC, 0xBC, 0x32}, "Apple Inc", "MacBook Pro", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xAC, 0xCF, 0x85}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xB0, 0x34, 0x95}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xB0, 0x65, 0xBD}, "Apple Inc", "iMac", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xB0, 0x9F, 0xBA}, "Apple Inc", "MacBook Pro", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xB0, 0xCA, 0x68}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xB4, 0x18, 0xD1}, "Apple Inc", "iPad", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xB4, 0x52, 0x7E}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xB4, 0x8B, 0x19}, "Apple Inc", "Mac Pro", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xB4, 0xF0, 0xAB}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xB8, 0x17, 0xC2}, "Apple Inc", "iPad", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xB8, 0x5D, 0x0A}, "Apple Inc", "MacBook Pro", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xB8, 0x63, 0x4D}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xB8, 0x8D, 0x12}, "Apple Inc", "Mac", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xB8, 0xC1, 0x11}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xB8, 0xFF, 0x61}, "Apple Inc", "iPad", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xBC, 0x3B, 0xAF}, "Apple Inc", "MacBook Pro", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xBC, 0x9F, 0xEF}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xC0, 0x84, 0x7A}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xC0, 0x9F, 0x42}, "Apple Inc", "Mac", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xC4, 0x2C, 0x03}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xC8, 0x69, 0xCD}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xC8, 0x85, 0x50}, "Apple Inc", "MacBook Pro", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xC8, 0xB5, 0xB7}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xC8, 0xBC, 0xC8}, "Apple Inc", "Mac mini", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xCC, 0x08, 0x8D}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xCC, 0x20, 0xE8}, "Apple Inc", "iPad", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xCC, 0x25, 0xEF}, "Apple Inc", "Mac", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xCC, 0x29, 0xF5}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xCC, 0x44, 0x63}, "Apple Inc", "MacBook Pro", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xCC, 0xC7, 0x60}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xD0, 0x03, 0x4B}, "Apple Inc", "Mac mini", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xD0, 0x33, 0x11}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xD0, 0x65, 0xCA}, "Apple Inc", "iMac", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xD0, 0x81, 0x7A}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xD0, 0xA6, 0x37}, "Apple Inc", "iPad", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xD4, 0x61, 0x9D}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xD4, 0x9A, 0x20}, "Apple Inc", "Mac", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xD8, 0x00, 0x4D}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xD8, 0x1D, 0x72}, "Apple Inc", "iPad", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xD8, 0x30, 0x62}, "Apple Inc", "MacBook Pro", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xD8, 0x96, 0x95}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xD8, 0xA2, 0x5E}, "Apple Inc", "Mac mini", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xD8, 0xBB, 0x2C}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xDC, 0x0C, 0x5C}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xDC, 0x2B, 0x2A}, "Apple Inc", "MacBook Pro", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xDC, 0x37, 0x14}, "Apple Inc", "iMac", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xDC, 0x9B, 0x9C}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xDC, 0xA4, 0xCA}, "Apple Inc", "iPad", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xE0, 0x66, 0x78}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xE0, 0xAC, 0xCB}, "Apple Inc", "Mac", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xE0, 0xB5, 0x2D}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xE0, 0xC9, 0x7A}, "Apple Inc", "MacBook Pro", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xE4, 0x25, 0xE7}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xE4, 0x2B, 0x34}, "Apple Inc", "iPad", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xE4, 0x9A, 0xDC}, "Apple Inc", "Mac", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xE4, 0xCE, 0x8F}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xE8, 0x04, 0x0B}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xE8, 0x06, 0x88}, "Apple Inc", "iPad", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xE8, 0x40, 0xF2}, "Apple Inc", "Mac mini", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xE8, 0x80, 0x2E}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xE8, 0x8D, 0x28}, "Apple Inc", "MacBook Pro", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xE8, 0xFA, 0x62}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xEC, 0x35, 0x86}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xF0, 0x18, 0x98}, "Apple Inc", "Mac", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xF0, 0x98, 0x9D}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xF0, 0xB4, 0x79}, "Apple Inc", "iPad", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xF0, 0xDB, 0xE2}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xF0, 0xDC, 0xE2}, "Apple Inc", "MacBook Pro", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xF0, 0xF6, 0x1C}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xF4, 0x0F, 0x24}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xF4, 0x37, 0xB7}, "Apple Inc", "Mac", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xF4, 0x5C, 0x89}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xF4, 0xD9, 0xFB}, "Apple Inc", "MacBook Pro", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xF4, 0xF1, 0x5A}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xF8, 0x1E, 0xDF}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xF8, 0x27, 0x93}, "Apple Inc", "iPad", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xF8, 0x62, 0x14}, "Apple Inc", "Mac mini", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xF8, 0x7B, 0x7A}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xF8, 0x8F, 0xCA}, "Apple Inc", "MacBook Pro", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xFC, 0x25, 0x3F}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xFC, 0xE9, 0x98}, "Apple Inc", "iPhone", OUF_CAT_PHONE, OUF_RGN_US},
    // ── Cisco Systems ───────────────────────────────────────────────────────────
    {{0x00, 0x00, 0x0C}, "Cisco Systems", "Router/Switch", OUF_CAT_INFRA, OUF_RGN_US},
    {{0x00, 0x01, 0x42}, "Cisco Systems", "Catalyst Switch", OUF_CAT_INFRA, OUF_RGN_US},
    {{0x00, 0x01, 0x63}, "Cisco Systems", "Catalyst Switch", OUF_CAT_INFRA, OUF_RGN_US},
    {{0x00, 0x01, 0x96}, "Cisco Systems", "Catalyst Switch", OUF_CAT_INFRA, OUF_RGN_US},
    {{0x00, 0x02, 0x16}, "Cisco Systems", "Catalyst Switch", OUF_CAT_INFRA, OUF_RGN_US},
    {{0x00, 0x02, 0x4A}, "Cisco Systems", "Catalyst Switch", OUF_CAT_INFRA, OUF_RGN_US},
    {{0x00, 0x02, 0x7D}, "Cisco Systems", "Catalyst Switch", OUF_CAT_INFRA, OUF_RGN_US},
    {{0x00, 0x02, 0xFC}, "Cisco Systems", "ISR Router", OUF_CAT_INFRA, OUF_RGN_US},
    {{0x00, 0x03, 0x31}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x03, 0x6B}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x03, 0x9F}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x03, 0xFD}, "Cisco Systems", "ASR Router", OUF_CAT_INFRA, OUF_RGN_US},
    {{0x00, 0x04, 0x27}, "Cisco Systems", "Catalyst Switch", OUF_CAT_INFRA, OUF_RGN_US},
    {{0x00, 0x04, 0x4D}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x04, 0x6D}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x04, 0x9A}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x04, 0xC0}, "Cisco Systems", "Cisco VoIP", OUF_CAT_INFRA, OUF_RGN_US},
    {{0x00, 0x04, 0xDC}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x05, 0x00}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x05, 0xDC}, "Cisco Systems", "Catalyst Switch", OUF_CAT_INFRA, OUF_RGN_US},
    {{0x00, 0x06, 0x28}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x06, 0x52}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x06, 0x7C}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x06, 0xD6}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x07, 0x0D}, "Cisco Systems", "Cisco VoIP Phone", OUF_CAT_INFRA, OUF_RGN_US},
    {{0x00, 0x07, 0x50}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x07, 0x7D}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x07, 0x84}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x07, 0xB3}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x07, 0xEB}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x08, 0x20}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x08, 0x30}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x08, 0xA3}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x08, 0xE3}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x09, 0x11}, "Cisco Systems", "Cisco AP 1200", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x09, 0x43}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x09, 0x7B}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x09, 0xB6}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x09, 0xE8}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x0A, 0x41}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x0A, 0x8A}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x0A, 0xB8}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x0A, 0xF3}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x0B, 0x45}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x0B, 0xBE}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x0B, 0xFC}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x0C, 0x30}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x0C, 0x85}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x0C, 0xCE}, "Cisco Systems", "Cisco AP 1140", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x0D, 0x28}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x0D, 0x65}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x0D, 0xBC}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x0D, 0xEC}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x0E, 0x08}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x0E, 0x38}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x0E, 0x83}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x0E, 0xD7}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x0F, 0x23}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x0F, 0x8F}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x0F, 0xF7}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x10, 0x07}, "Cisco Systems", "Cisco Router", OUF_CAT_INFRA, OUF_RGN_US},
    {{0x00, 0x10, 0x11}, "Cisco Systems", "Cisco Router", OUF_CAT_INFRA, OUF_RGN_US},
    {{0x00, 0x10, 0x29}, "Cisco Systems", "Cisco Router", OUF_CAT_INFRA, OUF_RGN_US},
    {{0x00, 0x10, 0x54}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x10, 0x79}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x10, 0x7B}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x10, 0xA6}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x10, 0xF6}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x11, 0x20}, "Cisco Systems", "Cisco AP 1232", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x11, 0x5C}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x11, 0x92}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x11, 0xBB}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x12, 0x00}, "Cisco Systems", "Cisco AP 1310", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x12, 0x43}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x12, 0x7F}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x12, 0xD9}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x13, 0x19}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x13, 0x60}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x13, 0x7F}, "Cisco Systems", "Cisco AP 1250", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x13, 0xC3}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x14, 0x1B}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x14, 0x69}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x14, 0xA9}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x14, 0xF1}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x15, 0x2B}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x15, 0x62}, "Cisco Systems", "Cisco AP 1131", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x15, 0xC6}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x15, 0xF9}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x16, 0x46}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x16, 0x9C}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x16, 0xC7}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x17, 0x0E}, "Cisco Systems", "Cisco AP 1142", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x17, 0x3B}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x17, 0x94}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x17, 0xDF}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x18, 0x18}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x18, 0x68}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x18, 0x73}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x18, 0xB9}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x19, 0x06}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x19, 0x2F}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x19, 0x55}, "Cisco Systems", "Cisco AP 1260", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x19, 0xA9}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x1A, 0x2F}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x1A, 0x6C}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x1A, 0xA1}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x1A, 0xE3}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x1B, 0x2A}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x1B, 0x53}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x1B, 0x8F}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x1B, 0xD4}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x1C, 0x0E}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x1C, 0x57}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x1C, 0xF9}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x1D, 0x45}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x1D, 0x71}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x1D, 0xA1}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x1E, 0x13}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x1E, 0x49}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x1E, 0x7A}, "Cisco Systems", "Cisco AP 1041", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x1E, 0xBE}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x1F, 0x26}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x1F, 0x6C}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x1F, 0x9E}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x1F, 0xCA}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x21, 0x1B}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x21, 0x55}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x21, 0xA0}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x22, 0x0C}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x22, 0x55}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x22, 0x90}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x22, 0xBD}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x23, 0x04}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x23, 0x33}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x23, 0x5D}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x23, 0xAB}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x23, 0xEA}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x24, 0x13}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x24, 0x50}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x24, 0x97}, "Cisco Systems", "Cisco AP 1602", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x24, 0xC3}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x24, 0xF7}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x25, 0x2E}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x25, 0x45}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x25, 0x84}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x25, 0xB4}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x26, 0x0A}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x26, 0xCB}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0xA0, 0x93, 0x51}, "Cisco Systems", "Cisco AP 3700", OUF_CAT_AP, OUF_RGN_US},
    {{0xA4, 0x4C, 0x11}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0xA8, 0x9D, 0x21}, "Cisco Systems", "Cisco AP 2802", OUF_CAT_AP, OUF_RGN_US},
    {{0xB0, 0xAA, 0x77}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0xB4, 0x14, 0x89}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0xB8, 0x38, 0x61}, "Cisco Systems", "Cisco AP 3802", OUF_CAT_AP, OUF_RGN_US},
    {{0xBC, 0x16, 0x65}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0xC4, 0x71, 0xFE}, "Cisco Systems", "Cisco AP 2802i", OUF_CAT_AP, OUF_RGN_US},
    {{0xCC, 0x46, 0xD6}, "Cisco Systems", "Cisco AP 1830", OUF_CAT_AP, OUF_RGN_US},
    {{0xDC, 0x7B, 0x94}, "Cisco Systems", "Cisco AP 2800", OUF_CAT_AP, OUF_RGN_US},
    {{0xE8, 0x65, 0xD4}, "Cisco Systems", "Cisco AP 3800", OUF_CAT_AP, OUF_RGN_US},
    {{0xEC, 0x30, 0x91}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0xF4, 0xDB, 0xE6}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0xFC, 0xFB, 0xFB}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x70, 0xEA, 0x1A}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x74, 0xA0, 0x21}, "Cisco Systems", "Cisco AP 1700", OUF_CAT_AP, OUF_RGN_US},
    {{0x78, 0xDA, 0x6E}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x84, 0xB8, 0x02}, "Cisco Systems", "Cisco AP 1540", OUF_CAT_AP, OUF_RGN_US},
    {{0x88, 0x5A, 0x92}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x8C, 0x60, 0x4F}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x90, 0xE2, 0xBA}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x94, 0xD4, 0x69}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x98, 0x5D, 0x82}, "Cisco Systems", "Cisco AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x9C, 0x5A, 0x81}, "Cisco Systems", "Cisco AP 2800", OUF_CAT_AP, OUF_RGN_US},
    // ── Intel Corporation ────────────────────────────────────────────────────────
    {{0x00, 0x02, 0xB3}, "Intel Corp", "PRO/Wireless NIC", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x00, 0x03, 0x47}, "Intel Corp", "PRO/Wireless 2200BG", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x00, 0x04, 0x23}, "Intel Corp", "PRO/Wireless NIC", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x00, 0x07, 0xE9}, "Intel Corp", "PRO/Wireless NIC", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x00, 0x0C, 0xF1}, "Intel Corp", "PRO/Wireless 3945ABG", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x00, 0x11, 0x11}, "Intel Corp", "PRO/Wireless NIC", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x00, 0x12, 0xF0}, "Intel Corp", "WiFi Link 4965AGN", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x00, 0x13, 0x02}, "Intel Corp", "WiFi Link 5100", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x00, 0x13, 0x20}, "Intel Corp", "WiFi Link 5300", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x00, 0x13, 0xCE}, "Intel Corp", "Centrino Advanced", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x00, 0x13, 0xE8}, "Intel Corp", "WiFi Link NIC", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x00, 0x15, 0x17}, "Intel Corp", "Centrino N 1000", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x00, 0x16, 0x17}, "Intel Corp", "Centrino N 1030", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x00, 0x16, 0x36}, "Intel Corp", "Centrino Advanced-N", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x00, 0x16, 0x76}, "Intel Corp", "WiFi Link NIC", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x00, 0x17, 0x08}, "Intel Corp", "Wireless WiFi Link", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x00, 0x18, 0xDE}, "Intel Corp", "WiFi Link 6250", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x00, 0x19, 0xD1}, "Intel Corp", "Centrino Wireless-N", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x00, 0x1B, 0x21}, "Intel Corp", "Centrino Wireless-N 1000", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x00, 0x1C, 0xBF}, "Intel Corp", "Centrino Wireless-N 2200", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x00, 0x1D, 0xE0}, "Intel Corp", "Centrino Wireless-N", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x00, 0x1E, 0x65}, "Intel Corp", "Centrino Advanced-N 6235", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x00, 0x1E, 0x67}, "Intel Corp", "Centrino Advanced-N", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x00, 0x1F, 0x3B}, "Intel Corp", "Centrino Wireless-N", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x00, 0x21, 0x5C}, "Intel Corp", "Centrino Advanced-N 6250", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x00, 0x22, 0xFA}, "Intel Corp", "Centrino Wireless-N", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x00, 0x24, 0xD6}, "Intel Corp", "WiFi Link NIC", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x00, 0x26, 0xB9}, "Intel Corp", "Centrino Advanced-N 6300", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x00, 0x26, 0xC6}, "Intel Corp", "Centrino Wireless-N", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x10, 0x02, 0xB5}, "Intel Corp", "Wireless 7260", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x10, 0x0B, 0xA9}, "Intel Corp", "Wireless 7265", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x14, 0x14, 0x4B}, "Intel Corp", "Wireless 8260", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x18, 0x3D, 0xA2}, "Intel Corp", "Wireless 8265", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x24, 0x77, 0x03}, "Intel Corp", "Wireless 9260", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x24, 0xFD, 0x52}, "Intel Corp", "Wireless AX200", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x28, 0xD2, 0x44}, "Intel Corp", "Wireless AX201", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x2C, 0x6E, 0x85}, "Intel Corp", "Wireless AX210", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x30, 0x3A, 0x64}, "Intel Corp", "Wireless AX211", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x34, 0x02, 0x86}, "Intel Corp", "Wireless AX411", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x38, 0xBA, 0xF8}, "Intel Corp", "Wireless BE200", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x3C, 0xA9, 0xF4}, "Intel Corp", "Wireless AX201", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x40, 0x25, 0xC2}, "Intel Corp", "Wireless 7260", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x40, 0x4E, 0x36}, "Intel Corp", "Wireless 7265", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x44, 0x03, 0x2C}, "Intel Corp", "Wireless 8260", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x44, 0x1C, 0xA8}, "Intel Corp", "Wireless 8265", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x48, 0x51, 0xB7}, "Intel Corp", "Wireless AX200", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x4C, 0x34, 0x88}, "Intel Corp", "Wireless AX201", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x4C, 0x79, 0x6E}, "Intel Corp", "Wireless AX211", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x50, 0x3E, 0xAA}, "Intel Corp", "Wireless 7260", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x54, 0x35, 0x30}, "Intel Corp", "Wireless 8260", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x60, 0x6C, 0x66}, "Intel Corp", "Wireless AX200", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x64, 0x5D, 0x86}, "Intel Corp", "Wireless AX201", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x68, 0x17, 0x29}, "Intel Corp", "Wireless AX211", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x70, 0x5A, 0x0F}, "Intel Corp", "Wireless AX210", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x74, 0xE5, 0x0B}, "Intel Corp", "Wireless AX200", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x78, 0x92, 0x9C}, "Intel Corp", "Wireless AX201", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x80, 0x19, 0x34}, "Intel Corp", "Wireless AX211", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x84, 0x3A, 0x4B}, "Intel Corp", "Wireless AX210", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x8C, 0x70, 0x5A}, "Intel Corp", "Wireless AX200", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x90, 0xE6, 0xBA}, "Intel Corp", "Wireless AX201", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x94, 0x65, 0x9C}, "Intel Corp", "Wireless AX211", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x9C, 0xEB, 0xE8}, "Intel Corp", "Wireless AX210", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xA0, 0x36, 0x9F}, "Intel Corp", "Wireless AX200", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xA4, 0x34, 0xD9}, "Intel Corp", "Wireless AX201", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xA4, 0xC3, 0xF0}, "Intel Corp", "Wireless AX211", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xAC, 0x7B, 0xA1}, "Intel Corp", "Wireless AX210", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xB0, 0x6E, 0xBF}, "Intel Corp", "Wireless AX200", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xB4, 0xB0, 0x17}, "Intel Corp", "Wireless AX201", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xBC, 0x0F, 0x9A}, "Intel Corp", "Wireless AX211", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xC4, 0x85, 0x08}, "Intel Corp", "Wireless AX210", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xC8, 0x34, 0x8C}, "Intel Corp", "Wireless AX200", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xD0, 0x50, 0x99}, "Intel Corp", "Wireless AX201", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xD4, 0xBE, 0xD9}, "Intel Corp", "Wireless AX211", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xD8, 0x0D, 0x17}, "Intel Corp", "Wireless AX210", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xDC, 0x85, 0xDE}, "Intel Corp", "Wireless AX200", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xE0, 0x3F, 0x49}, "Intel Corp", "Wireless AX201", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xE4, 0xA4, 0x71}, "Intel Corp", "Wireless AX211", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xE8, 0x03, 0x9A}, "Intel Corp", "Wireless AX210", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xEC, 0x1F, 0x72}, "Intel Corp", "Wireless AX200", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xF0, 0x76, 0x1C}, "Intel Corp", "Wireless AX201", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xF4, 0x4D, 0x30}, "Intel Corp", "Wireless AX211", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xF8, 0x63, 0x3F}, "Intel Corp", "Wireless AX210", OUF_CAT_LAPTOP, OUF_RGN_US},
    // ── Samsung Electronics ──────────────────────────────────────────────────────
    {{0x00, 0x00, 0xF0}, "Samsung Electronics", "Network Device", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x00, 0x02, 0x78}, "Samsung Electronics", "Network Device", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x00, 0x07, 0xAB}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x00, 0x0D, 0xAE}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x00, 0x12, 0x47}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x00, 0x13, 0x77}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x00, 0x15, 0xB9}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x00, 0x16, 0x32}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x00, 0x16, 0xDB}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x00, 0x17, 0xC9}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x00, 0x18, 0xAF}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x00, 0x1A, 0x8A}, "Samsung Electronics", "Galaxy S Series", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x00, 0x1B, 0x98}, "Samsung Electronics", "Galaxy Note Series", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x00, 0x1C, 0x43}, "Samsung Electronics", "Galaxy Tab", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x00, 0x1D, 0x25}, "Samsung Electronics", "Galaxy S III", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x00, 0x1E, 0x7D}, "Samsung Electronics", "Galaxy S4", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x00, 0x1F, 0xCC}, "Samsung Electronics", "Galaxy S5", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x00, 0x21, 0x19}, "Samsung Electronics", "Galaxy S6", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x00, 0x23, 0x39}, "Samsung Electronics", "Galaxy S7", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x00, 0x24, 0x54}, "Samsung Electronics", "Galaxy S8", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x00, 0x24, 0x91}, "Samsung Electronics", "Galaxy S9", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x00, 0x25, 0x38}, "Samsung Electronics", "Galaxy S10", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x00, 0x26, 0x37}, "Samsung Electronics", "Galaxy S20", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x04, 0x18, 0x0F}, "Samsung Electronics", "Galaxy S21", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x08, 0x08, 0xC2}, "Samsung Electronics", "Galaxy S22", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x08, 0x37, 0x3D}, "Samsung Electronics", "Galaxy S23", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x08, 0x62, 0x66}, "Samsung Electronics", "Galaxy S24", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x08, 0xD4, 0x2B}, "Samsung Electronics", "Galaxy Note 20", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x0C, 0x14, 0x20}, "Samsung Electronics", "Galaxy Flip", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x0C, 0x71, 0x5D}, "Samsung Electronics", "Galaxy Fold", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x10, 0x1D, 0xC0}, "Samsung Electronics", "Galaxy Tab A", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x10, 0x30, 0x47}, "Samsung Electronics", "Galaxy Tab S", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x10, 0xD5, 0x42}, "Samsung Electronics", "Galaxy Watch", OUF_CAT_WEARABLE, OUF_RGN_KR},
    {{0x14, 0x49, 0xE0}, "Samsung Electronics", "Galaxy Watch 4", OUF_CAT_WEARABLE, OUF_RGN_KR},
    {{0x14, 0xA3, 0x64}, "Samsung Electronics", "Galaxy Watch 5", OUF_CAT_WEARABLE, OUF_RGN_KR},
    {{0x18, 0x22, 0x7E}, "Samsung Electronics", "Galaxy Buds", OUF_CAT_AUDIO, OUF_RGN_KR},
    {{0x18, 0x3A, 0x2D}, "Samsung Electronics", "Samsung Smart TV", OUF_CAT_TV, OUF_RGN_KR},
    {{0x1C, 0x62, 0xB8}, "Samsung Electronics", "Samsung TV", OUF_CAT_TV, OUF_RGN_KR},
    {{0x1C, 0x66, 0xAA}, "Samsung Electronics", "Samsung TV", OUF_CAT_TV, OUF_RGN_KR},
    {{0x20, 0x4E, 0x7F}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x20, 0x64, 0x32}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x24, 0x4B, 0x81}, "Samsung Electronics", "Galaxy A Series", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x24, 0xC6, 0x96}, "Samsung Electronics", "Galaxy M Series", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x28, 0x27, 0xBF}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x28, 0x98, 0x7B}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x2C, 0xAE, 0x2B}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x34, 0x23, 0x87}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x34, 0xAA, 0x8B}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x40, 0x0E, 0x85}, "Samsung Electronics", "Samsung TV", OUF_CAT_TV, OUF_RGN_KR},
    {{0x44, 0x4E, 0x1A}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x44, 0xF4, 0x59}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x48, 0x13, 0x7E}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x4C, 0x3C, 0x16}, "Samsung Electronics", "Galaxy Z Fold", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x50, 0x01, 0xBB}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x50, 0x32, 0x75}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x54, 0x40, 0xAD}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x58, 0xEF, 0x68}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x5C, 0x0A, 0x5B}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x5C, 0xA8, 0x6A}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x60, 0xA1, 0x0A}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x60, 0xAF, 0x6D}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x64, 0x70, 0x02}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x68, 0x48, 0x98}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x68, 0xEB, 0xAE}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x6C, 0x2F, 0x2C}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x6C, 0x83, 0x36}, "Samsung Electronics", "Samsung TV", OUF_CAT_TV, OUF_RGN_KR},
    {{0x70, 0xF9, 0x27}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x74, 0x45, 0x8A}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x78, 0x1F, 0xDB}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x78, 0x40, 0xE4}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x7C, 0x1C, 0x4E}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x80, 0x57, 0x19}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x84, 0x25, 0xDB}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x84, 0x51, 0x81}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x88, 0x32, 0x9B}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x8C, 0x77, 0x12}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x90, 0x18, 0x7C}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x94, 0x35, 0x0A}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x94, 0x76, 0xB7}, "Samsung Electronics", "Samsung TV", OUF_CAT_TV, OUF_RGN_KR},
    {{0x94, 0xD7, 0x71}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x98, 0x52, 0xB1}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0x9C, 0x02, 0x98}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0xA0, 0x0B, 0xBA}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0xA4, 0x07, 0xB6},
     "Samsung Electronics",
     "Galaxy Watch Active",
     OUF_CAT_WEARABLE,
     OUF_RGN_KR},
    {{0xA8, 0x06, 0x00}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0xAC, 0x36, 0x13}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0xB0, 0xD0, 0x9C}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0xB4, 0x3A, 0x28}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0xB4, 0x79, 0xA7}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0xB8, 0x5E, 0x7B}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0xBC, 0x20, 0xA4}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0xBC, 0x85, 0x1F}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0xC0, 0xBD, 0xD1}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0xC4, 0x42, 0x02}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0xC4, 0x57, 0x6E}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0xC8, 0x14, 0x79}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0xC8, 0x38, 0x70}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0xCC, 0x07, 0xAB}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0xD0, 0x17, 0x6A}, "Samsung Electronics", "Samsung TV", OUF_CAT_TV, OUF_RGN_KR},
    {{0xD0, 0x59, 0xE4}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0xD4, 0x88, 0x90}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0xD8, 0x57, 0xEF}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0xDC, 0x71, 0x96}, "Samsung Electronics", "Samsung TV", OUF_CAT_TV, OUF_RGN_KR},
    {{0xE0, 0x19, 0x54}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0xE4, 0x12, 0x1D}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0xE4, 0x40, 0xE2}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0xE4, 0x7C, 0xF9}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0xE8, 0x50, 0x8B}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0xEC, 0x9B, 0xF3}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0xF0, 0x25, 0xB7}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0xF4, 0x7B, 0x5E}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    {{0xF8, 0x04, 0x2E}, "Samsung Electronics", "Samsung Phone", OUF_CAT_PHONE, OUF_RGN_KR},
    // ── Huawei Technologies ──────────────────────────────────────────────────────
    {{0x00, 0x9A, 0xCD}, "Huawei Technologies", "Huawei Device", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x04, 0x02, 0x1F}, "Huawei Technologies", "Huawei Router", OUF_CAT_AP, OUF_RGN_CN},
    {{0x04, 0x0D, 0x84}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x04, 0x4F, 0xAA}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x04, 0x75, 0x03}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x04, 0x88, 0xE2}, "Huawei Technologies", "Huawei Mate", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x04, 0xBD, 0x70}, "Huawei Technologies", "Huawei P Series", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x04, 0xC0, 0x6F}, "Huawei Technologies", "Huawei Nova", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x08, 0x19, 0xA6}, "Huawei Technologies", "Huawei Mate 40", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x08, 0x7A, 0x4C}, "Huawei Technologies", "Huawei P50", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x08, 0x9E, 0x01}, "Huawei Technologies", "Huawei Mate 50", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x0C, 0x37, 0xDC}, "Huawei Technologies", "Huawei Router", OUF_CAT_AP, OUF_RGN_CN},
    {{0x0C, 0x84, 0xDC}, "Huawei Technologies", "Huawei AP", OUF_CAT_AP, OUF_RGN_CN},
    {{0x10, 0x1B, 0x54}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x10, 0x47, 0x80}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x10, 0x51, 0x72}, "Huawei Technologies", "Huawei Tablet", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x14, 0x0A, 0xB1}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x14, 0x1E, 0xB1}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x18, 0xC5, 0x8A}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x1C, 0x8E, 0x5C}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x20, 0x08, 0xED}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x20, 0x2B, 0xC1}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x20, 0x7D, 0x14}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x20, 0xF3, 0xA3}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x24, 0x09, 0x95}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x24, 0xDF, 0x6A}, "Huawei Technologies", "Huawei AP", OUF_CAT_AP, OUF_RGN_CN},
    {{0x28, 0x31, 0x52}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x2C, 0xA8, 0x35}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x2C, 0xAB, 0x00}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x30, 0x45, 0x11}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x30, 0xD1, 0x7E}, "Huawei Technologies", "Huawei AP", OUF_CAT_AP, OUF_RGN_CN},
    {{0x34, 0x08, 0x04}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x34, 0x98, 0xB5}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x38, 0x37, 0x8B}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x3C, 0x47, 0x11}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x3C, 0xF8, 0x08}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x40, 0x4D, 0x8E}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x40, 0xCB, 0xA8}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x44, 0x55, 0xB1}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x44, 0x6E, 0xE5}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x48, 0x00, 0x31}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x4C, 0x14, 0x85}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x50, 0x68, 0x0A}, "Huawei Technologies", "Huawei AP", OUF_CAT_AP, OUF_RGN_CN},
    {{0x54, 0x51, 0x1B}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x58, 0x25, 0x75}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x5C, 0x4C, 0xA9}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x5C, 0x7D, 0x5E}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x60, 0x16, 0x2D}, "Huawei Technologies", "Huawei AP", OUF_CAT_AP, OUF_RGN_CN},
    {{0x64, 0x3E, 0x8C}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x68, 0xA0, 0xF6}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x6C, 0x8D, 0xC1}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x70, 0x7B, 0xE8}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x74, 0xA5, 0x28}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x78, 0x1D, 0xBA}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x7C, 0x60, 0x97}, "Huawei Technologies", "Huawei AP", OUF_CAT_AP, OUF_RGN_CN},
    {{0x80, 0x38, 0xBC}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x84, 0xA8, 0xE4}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x88, 0xA2, 0x5E}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x8C, 0x0D, 0x76}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x8C, 0x34, 0xFD}, "Huawei Technologies", "Huawei AP", OUF_CAT_AP, OUF_RGN_CN},
    {{0x90, 0x17, 0x3F}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x90, 0x4E, 0x2B}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x94, 0x04, 0x9C}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x94, 0xDB, 0xDA}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x9C, 0x28, 0xEF}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x9C, 0x74, 0x1A}, "Huawei Technologies", "Huawei AP", OUF_CAT_AP, OUF_RGN_CN},
    {{0xA0, 0x08, 0x6F}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0xA0, 0x97, 0x85}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0xA4, 0x50, 0x46}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0xA4, 0xBE, 0x0B}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0xA8, 0xCA, 0x7B}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0xAC, 0x3D, 0x28}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0xAC, 0xEB, 0xD8}, "Huawei Technologies", "Huawei AP", OUF_CAT_AP, OUF_RGN_CN},
    {{0xB4, 0xCD, 0x27}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0xB8, 0x08, 0xD7}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0xB8, 0xBC, 0x1B}, "Huawei Technologies", "Huawei AP", OUF_CAT_AP, OUF_RGN_CN},
    {{0xC0, 0x70, 0xCF}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0xC4, 0x06, 0x83}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0xC4, 0x9E, 0xEC}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0xC8, 0x9B, 0x3F}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0xCC, 0x53, 0xB5}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0xCC, 0x96, 0xA0}, "Huawei Technologies", "Huawei AP", OUF_CAT_AP, OUF_RGN_CN},
    {{0xD0, 0x7A, 0xB5}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0xD4, 0x12, 0x43}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0xD4, 0x6E, 0x5C}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0xD4, 0x94, 0xE8}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0xD8, 0x38, 0x0D}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0xDC, 0xD2, 0xFC}, "Huawei Technologies", "Huawei AP", OUF_CAT_AP, OUF_RGN_CN},
    {{0xE0, 0x28, 0x6D}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0xE4, 0x68, 0xA3}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0xE4, 0x72, 0xE2}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0xE8, 0x39, 0xDF}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0xE8, 0xCD, 0x2D}, "Huawei Technologies", "Huawei AP", OUF_CAT_AP, OUF_RGN_CN},
    {{0xEC, 0x1D, 0x8B}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0xEC, 0x38, 0x8F}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0xEC, 0xCB, 0x30}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0xF0, 0x48, 0x19}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0xF4, 0x4C, 0x7F}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0xF4, 0x63, 0x1F}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0xF4, 0x9F, 0xF3}, "Huawei Technologies", "Huawei AP", OUF_CAT_AP, OUF_RGN_CN},
    {{0xF8, 0x01, 0x13}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0xF8, 0x4D, 0x89}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0xFC, 0x48, 0xEF}, "Huawei Technologies", "Huawei Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    // ── Google Inc ───────────────────────────────────────────────────────────────
    {{0x00, 0x1A, 0x11}, "Google Inc", "Google Device", OUF_CAT_IOT, OUF_RGN_US},
    {{0x54, 0x60, 0x09}, "Google Inc", "Google Home", OUF_CAT_IOT, OUF_RGN_US},
    {{0xAC, 0x67, 0xB2}, "Google Inc", "Chromecast", OUF_CAT_TV, OUF_RGN_US},
    {{0xD0, 0x1D, 0x72}, "Google Inc", "Google Nest", OUF_CAT_IOT, OUF_RGN_US},
    {{0x20, 0xB6, 0x6E}, "Google Inc", "Google Home Mini", OUF_CAT_IOT, OUF_RGN_US},
    {{0x3C, 0x5A, 0xB4}, "Google Inc", "Google Pixel", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xF4, 0xF5, 0xE8}, "Google Inc", "Google Pixel", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x94, 0x95, 0xA0}, "Google Inc", "Google Pixel", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x00, 0x17, 0xAB}, "Google Inc", "Google Device", OUF_CAT_IOT, OUF_RGN_US},
    {{0xDC, 0xA6, 0x32}, "Google Inc", "Chromecast", OUF_CAT_TV, OUF_RGN_US},
    {{0x48, 0xD6, 0xD5}, "Google Inc", "Chromecast Ultra", OUF_CAT_TV, OUF_RGN_US},
    {{0x1C, 0xF2, 0x9A}, "Google Inc", "Google Home Max", OUF_CAT_IOT, OUF_RGN_US},
    {{0xA4, 0x77, 0x33}, "Google Inc", "Nest Hub", OUF_CAT_IOT, OUF_RGN_US},
    {{0x18, 0xB4, 0x30}, "Google Inc", "Nest Thermostat", OUF_CAT_IOT, OUF_RGN_US},
    {{0x64, 0x16, 0x66}, "Google Inc", "Pixel Watch", OUF_CAT_WEARABLE, OUF_RGN_US},
    {{0xE4, 0xF0, 0x42}, "Google Inc", "Pixel 7", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x28, 0xBC, 0x56}, "Google Inc", "Pixel 8", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x40, 0x4E, 0x36}, "Google Inc", "Pixel 9", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x60, 0xBE, 0xB5}, "Google Inc", "Nest Doorbell", OUF_CAT_IOT, OUF_RGN_US},
    {{0xE0, 0x28, 0x61}, "Google Inc", "Nest Cam", OUF_CAT_CAMERA, OUF_RGN_US},
    // ── Amazon Technologies ──────────────────────────────────────────────────────
    {{0x44, 0x65, 0x0D}, "Amazon Technologies", "Fire TV Stick", OUF_CAT_TV, OUF_RGN_US},
    {{0xFC, 0xA6, 0x67}, "Amazon Technologies", "Echo Device", OUF_CAT_IOT, OUF_RGN_US},
    {{0x68, 0x37, 0xE9}, "Amazon Technologies", "Kindle", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x40, 0xB4, 0xCD}, "Amazon Technologies", "Echo Dot", OUF_CAT_IOT, OUF_RGN_US},
    {{0xB4, 0x7C, 0x9C}, "Amazon Technologies", "Fire HD Tablet", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xAC, 0x63, 0xBE}, "Amazon Technologies", "Echo Show", OUF_CAT_IOT, OUF_RGN_US},
    {{0x74, 0x75, 0x48}, "Amazon Technologies", "Echo Plus", OUF_CAT_IOT, OUF_RGN_US},
    {{0xA4, 0x08, 0xEA}, "Amazon Technologies", "Echo Input", OUF_CAT_IOT, OUF_RGN_US},
    {{0xA4, 0xC0, 0xE1}, "Amazon Technologies", "Fire TV Cube", OUF_CAT_TV, OUF_RGN_US},
    {{0x68, 0x54, 0xFD}, "Amazon Technologies", "Amazon Device", OUF_CAT_IOT, OUF_RGN_US},
    {{0x00, 0x77, 0x97}, "Amazon Technologies", "Amazon Device", OUF_CAT_IOT, OUF_RGN_US},
    {{0x0C, 0x0E, 0x76}, "Amazon Technologies", "Echo 4th Gen", OUF_CAT_IOT, OUF_RGN_US},
    {{0x10, 0xAE, 0x60}, "Amazon Technologies", "Fire Tablet", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x18, 0x74, 0x2E}, "Amazon Technologies", "Echo Dot 4th Gen", OUF_CAT_IOT, OUF_RGN_US},
    {{0x28, 0xEF, 0x01}, "Amazon Technologies", "Echo Studio", OUF_CAT_IOT, OUF_RGN_US},
    {{0x34, 0xD2, 0x70}, "Amazon Technologies", "Amazon Device", OUF_CAT_IOT, OUF_RGN_US},
    {{0x38, 0xF7, 0x3D}, "Amazon Technologies", "Fire TV 4K", OUF_CAT_TV, OUF_RGN_US},
    {{0x50, 0xDC, 0xE7}, "Amazon Technologies", "Echo Flex", OUF_CAT_IOT, OUF_RGN_US},
    {{0x54, 0x88, 0x0E}, "Amazon Technologies", "Amazon Halo", OUF_CAT_WEARABLE, OUF_RGN_US},
    {{0x5C, 0x97, 0xF3}, "Amazon Technologies", "Echo Auto", OUF_CAT_VEHICLE, OUF_RGN_US},
    {{0x6C, 0x56, 0x97}, "Amazon Technologies", "Amazon Device", OUF_CAT_IOT, OUF_RGN_US},
    {{0x78, 0xE1, 0x03}, "Amazon Technologies", "Kindle Oasis", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x7C, 0x1C, 0x68}, "Amazon Technologies", "Fire 7 Tablet", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x84, 0xD6, 0xD0}, "Amazon Technologies", "Echo Dot 5th Gen", OUF_CAT_IOT, OUF_RGN_US},
    {{0x8C, 0x85, 0x80}, "Amazon Technologies", "Echo Pop", OUF_CAT_IOT, OUF_RGN_US},
    {{0x90, 0x0F, 0x4A}, "Amazon Technologies", "Fire Max Tablet", OUF_CAT_PHONE, OUF_RGN_US},
    {{0x94, 0xF2, 0xBE}, "Amazon Technologies", "Amazon Device", OUF_CAT_IOT, OUF_RGN_US},
    {{0xB8, 0x81, 0x98}, "Amazon Technologies", "Echo Hub", OUF_CAT_IOT, OUF_RGN_US},
    {{0xC8, 0xC9, 0xA3}, "Amazon Technologies", "Amazon Device", OUF_CAT_IOT, OUF_RGN_US},
    {{0xD0, 0x0F, 0x3E}, "Amazon Technologies", "Fire TV 2nd Gen", OUF_CAT_TV, OUF_RGN_US},
    {{0xD0, 0x76, 0x50}, "Amazon Technologies", "Amazon Device", OUF_CAT_IOT, OUF_RGN_US},
    {{0xD4, 0x3A, 0x2C}, "Amazon Technologies", "Echo Show 15", OUF_CAT_IOT, OUF_RGN_US},
    {{0xDC, 0x1A, 0x60}, "Amazon Technologies", "Kindle Paperwhite", OUF_CAT_PHONE, OUF_RGN_US},
    {{0xE0, 0x14, 0x2E}, "Amazon Technologies", "Amazon Device", OUF_CAT_IOT, OUF_RGN_US},
    {{0xF0, 0x27, 0x2D}, "Amazon Technologies", "Echo Sub", OUF_CAT_IOT, OUF_RGN_US},
    // ── Microsoft Corporation ─────────────────────────────────────────────────────
    {{0x00, 0x50, 0xF2}, "Microsoft Corp", "Xbox/Surface", OUF_CAT_GAMING, OUF_RGN_US},
    {{0x28, 0x18, 0x78}, "Microsoft Corp", "Surface Laptop", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x60, 0x45, 0xBD}, "Microsoft Corp", "Surface Pro", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x7C, 0x1E, 0x52}, "Microsoft Corp", "Surface Book", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xBC, 0x83, 0x85}, "Microsoft Corp", "Surface Studio", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x00, 0x0D, 0x3A}, "Microsoft Corp", "Xbox One", OUF_CAT_GAMING, OUF_RGN_US},
    {{0x60, 0x45, 0xCB}, "Microsoft Corp", "Xbox Series X", OUF_CAT_GAMING, OUF_RGN_US},
    {{0x7C, 0xED, 0x8D}, "Microsoft Corp", "Xbox Series S", OUF_CAT_GAMING, OUF_RGN_US},
    {{0x98, 0x5F, 0xD3}, "Microsoft Corp", "Surface", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0xC8, 0x3D, 0xDC}, "Microsoft Corp", "Surface Go", OUF_CAT_LAPTOP, OUF_RGN_US},
    {{0x00, 0x15, 0x5D}, "Microsoft Corp", "Hyper-V Virtual NIC", OUF_CAT_VIRTUAL, OUF_RGN_US},
    {{0xC0, 0xEE, 0xFB}, "Microsoft Corp", "Surface Duo", OUF_CAT_PHONE, OUF_RGN_US},
    // ── TP-Link Technologies ─────────────────────────────────────────────────────
    {{0x00, 0x1D, 0x0F}, "TP-Link Technologies", "TL-WR841N", OUF_CAT_AP, OUF_RGN_CN},
    {{0x10, 0xFE, 0xED}, "TP-Link Technologies", "Archer Series", OUF_CAT_AP, OUF_RGN_CN},
    {{0x14, 0xCC, 0x20}, "TP-Link Technologies", "Archer AX", OUF_CAT_AP, OUF_RGN_CN},
    {{0x18, 0xA6, 0xF7}, "TP-Link Technologies", "Deco Mesh", OUF_CAT_AP, OUF_RGN_CN},
    {{0x20, 0xDC, 0xE6}, "TP-Link Technologies", "TL-WA Series", OUF_CAT_AP, OUF_RGN_CN},
    {{0x24, 0x69, 0x68}, "TP-Link Technologies", "Archer C7", OUF_CAT_AP, OUF_RGN_CN},
    {{0x28, 0x28, 0x5D}, "TP-Link Technologies", "Archer Series", OUF_CAT_AP, OUF_RGN_CN},
    {{0x30, 0xB5, 0xC2}, "TP-Link Technologies", "Archer AX50", OUF_CAT_AP, OUF_RGN_CN},
    {{0x38, 0x94, 0xED}, "TP-Link Technologies", "TL-WR940N", OUF_CAT_AP, OUF_RGN_CN},
    {{0x40, 0x16, 0x7E}, "TP-Link Technologies", "Archer C9", OUF_CAT_AP, OUF_RGN_CN},
    {{0x44, 0xAD, 0xD9}, "TP-Link Technologies", "Archer AX73", OUF_CAT_AP, OUF_RGN_CN},
    {{0x54, 0xAF, 0x97}, "TP-Link Technologies", "TL-WR841ND", OUF_CAT_AP, OUF_RGN_CN},
    {{0x54, 0xC7, 0x80}, "TP-Link Technologies", "Archer C6", OUF_CAT_AP, OUF_RGN_CN},
    {{0x60, 0x14, 0x66}, "TP-Link Technologies", "Archer AX20", OUF_CAT_AP, OUF_RGN_CN},
    {{0x60, 0x32, 0xB1}, "TP-Link Technologies", "Archer Series", OUF_CAT_AP, OUF_RGN_CN},
    {{0x70, 0x4F, 0x57}, "TP-Link Technologies", "Deco XE75", OUF_CAT_AP, OUF_RGN_CN},
    {{0x74, 0xEA, 0x3A}, "TP-Link Technologies", "Archer AX90", OUF_CAT_AP, OUF_RGN_CN},
    {{0x7C, 0x8B, 0xCA}, "TP-Link Technologies", "Archer C80", OUF_CAT_AP, OUF_RGN_CN},
    {{0x80, 0x35, 0xC1}, "TP-Link Technologies", "TL-WR1043ND", OUF_CAT_AP, OUF_RGN_CN},
    {{0x84, 0x16, 0xF9}, "TP-Link Technologies", "Archer AX3000", OUF_CAT_AP, OUF_RGN_CN},
    {{0x88, 0xDC, 0x96}, "TP-Link Technologies", "Archer Series", OUF_CAT_AP, OUF_RGN_CN},
    {{0x8C, 0x21, 0x0A}, "TP-Link Technologies", "Archer Series", OUF_CAT_AP, OUF_RGN_CN},
    {{0x90, 0x8D, 0x78}, "TP-Link Technologies", "Deco M9 Plus", OUF_CAT_AP, OUF_RGN_CN},
    {{0x94, 0x0A, 0x1A}, "TP-Link Technologies", "Archer Series", OUF_CAT_AP, OUF_RGN_CN},
    {{0x98, 0xDA, 0xC4}, "TP-Link Technologies", "Archer AX10", OUF_CAT_AP, OUF_RGN_CN},
    {{0xA0, 0xF3, 0xC1}, "TP-Link Technologies", "TL-WR841N", OUF_CAT_AP, OUF_RGN_CN},
    {{0xA8, 0x40, 0x41}, "TP-Link Technologies", "Archer C2300", OUF_CAT_AP, OUF_RGN_CN},
    {{0xAC, 0x84, 0xC6}, "TP-Link Technologies", "Archer AX6000", OUF_CAT_AP, OUF_RGN_CN},
    {{0xB4, 0xB0, 0x24}, "TP-Link Technologies", "Archer AX11000", OUF_CAT_AP, OUF_RGN_CN},
    {{0xC4, 0x6E, 0x1F}, "TP-Link Technologies", "Archer Series", OUF_CAT_AP, OUF_RGN_CN},
    {{0xE8, 0xDE, 0x27}, "TP-Link Technologies", "Archer AX55", OUF_CAT_AP, OUF_RGN_CN},
    {{0xEC, 0x08, 0x6B}, "TP-Link Technologies", "TL-WR940N", OUF_CAT_AP, OUF_RGN_CN},
    {{0xEC, 0x17, 0x2F}, "TP-Link Technologies", "Archer Series", OUF_CAT_AP, OUF_RGN_CN},
    {{0xF4, 0x1A, 0xA8}, "TP-Link Technologies", "Archer AX1500", OUF_CAT_AP, OUF_RGN_CN},
    {{0xF4, 0x60, 0xE2}, "TP-Link Technologies", "Archer AX5400", OUF_CAT_AP, OUF_RGN_CN},
    {{0xF8, 0x1A, 0x67}, "TP-Link Technologies", "TL-WA901ND", OUF_CAT_AP, OUF_RGN_CN},
    // ── Netgear Inc ─────────────────────────────────────────────────────────────
    {{0x00, 0x09, 0x5B}, "Netgear Inc", "WGR614 Router", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x0F, 0xB5}, "Netgear Inc", "Nighthawk R7000", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x1B, 0x2F}, "Netgear Inc", "Orbi", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x1E, 0x2A}, "Netgear Inc", "Netgear Router", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x22, 0x3F}, "Netgear Inc", "WNDR3700", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x24, 0xB2}, "Netgear Inc", "Netgear Router", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x26, 0xF2}, "Netgear Inc", "Netgear Router", OUF_CAT_AP, OUF_RGN_US},
    {{0x1C, 0x1B, 0x0D}, "Netgear Inc", "Orbi RBK850", OUF_CAT_AP, OUF_RGN_US},
    {{0x20, 0x0C, 0xC8}, "Netgear Inc", "Orbi RBK752", OUF_CAT_AP, OUF_RGN_US},
    {{0x28, 0xC6, 0x8E}, "Netgear Inc", "Nighthawk R8000", OUF_CAT_AP, OUF_RGN_US},
    {{0x2C, 0xB0, 0x5D}, "Netgear Inc", "Netgear Router", OUF_CAT_AP, OUF_RGN_US},
    {{0x30, 0x46, 0x9A}, "Netgear Inc", "Orbi AX6000", OUF_CAT_AP, OUF_RGN_US},
    {{0x3C, 0x28, 0x6D}, "Netgear Inc", "Nighthawk Series", OUF_CAT_AP, OUF_RGN_US},
    {{0x44, 0x94, 0xFC}, "Netgear Inc", "Netgear Router", OUF_CAT_AP, OUF_RGN_US},
    {{0x60, 0x38, 0xE0}, "Netgear Inc", "Netgear Router", OUF_CAT_AP, OUF_RGN_US},
    {{0x6C, 0xB0, 0xCE}, "Netgear Inc", "Orbi RBR750", OUF_CAT_AP, OUF_RGN_US},
    {{0x70, 0x3A, 0xCB}, "Netgear Inc", "Netgear Router", OUF_CAT_AP, OUF_RGN_US},
    {{0x74, 0x9F, 0xC9}, "Netgear Inc", "Orbi AX4200", OUF_CAT_AP, OUF_RGN_US},
    {{0x7C, 0x12, 0xDC}, "Netgear Inc", "Nighthawk Pro", OUF_CAT_AP, OUF_RGN_US},
    {{0x80, 0x37, 0x73}, "Netgear Inc", "Netgear Router", OUF_CAT_AP, OUF_RGN_US},
    {{0x84, 0x1B, 0x5E}, "Netgear Inc", "Orbi Series", OUF_CAT_AP, OUF_RGN_US},
    {{0x8C, 0x3B, 0xAD}, "Netgear Inc", "Netgear Router", OUF_CAT_AP, OUF_RGN_US},
    {{0x9C, 0x3D, 0xCF}, "Netgear Inc", "Netgear Router", OUF_CAT_AP, OUF_RGN_US},
    {{0xA0, 0x04, 0x60}, "Netgear Inc", "Netgear Router", OUF_CAT_AP, OUF_RGN_US},
    {{0xA0, 0x63, 0x91}, "Netgear Inc", "Orbi WiFi 6E", OUF_CAT_AP, OUF_RGN_US},
    {{0xA4, 0x2B, 0xB0}, "Netgear Inc", "Orbi Series", OUF_CAT_AP, OUF_RGN_US},
    {{0xB0, 0x48, 0x7A}, "Netgear Inc", "Nighthawk AX12", OUF_CAT_AP, OUF_RGN_US},
    {{0xB0, 0x7F, 0xB9}, "Netgear Inc", "Netgear Router", OUF_CAT_AP, OUF_RGN_US},
    {{0xC0, 0xFF, 0xD4}, "Netgear Inc", "Netgear Router", OUF_CAT_AP, OUF_RGN_US},
    {{0xC4, 0x04, 0x15}, "Netgear Inc", "Orbi AX6000", OUF_CAT_AP, OUF_RGN_US},
    {{0xCC, 0x40, 0xD0}, "Netgear Inc", "Orbi Series", OUF_CAT_AP, OUF_RGN_US},
    {{0xE4, 0xF4, 0xC6}, "Netgear Inc", "Netgear Router", OUF_CAT_AP, OUF_RGN_US},
    {{0xE8, 0xFC, 0xAF}, "Netgear Inc", "Orbi Series", OUF_CAT_AP, OUF_RGN_US},
    // ── Ubiquiti Networks ────────────────────────────────────────────────────────
    {{0x00, 0x15, 0x6D}, "Ubiquiti Networks", "UniFi AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x27, 0x22}, "Ubiquiti Networks", "UniFi AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x04, 0x18, 0xD6}, "Ubiquiti Networks", "UniFi UAP-AC", OUF_CAT_AP, OUF_RGN_US},
    {{0x0C, 0x81, 0x26}, "Ubiquiti Networks", "UniFi UAP-HD", OUF_CAT_AP, OUF_RGN_US},
    {{0x24, 0xA4, 0x3C}, "Ubiquiti Networks", "UniFi UAP-AC-PRO", OUF_CAT_AP, OUF_RGN_US},
    {{0x44, 0xD9, 0xE7}, "Ubiquiti Networks", "UniFi U6-Pro", OUF_CAT_AP, OUF_RGN_US},
    {{0x68, 0x72, 0x51}, "Ubiquiti Networks", "UniFi UAP-AC-IW", OUF_CAT_AP, OUF_RGN_US},
    {{0x78, 0x8A, 0x20}, "Ubiquiti Networks", "UniFi U6-LR", OUF_CAT_AP, OUF_RGN_US},
    {{0x80, 0x2A, 0xA8}, "Ubiquiti Networks", "UniFi UAP-AC-M", OUF_CAT_AP, OUF_RGN_US},
    {{0xB4, 0xFB, 0xE4}, "Ubiquiti Networks", "UniFi U6-Mesh", OUF_CAT_AP, OUF_RGN_US},
    {{0xDC, 0x9F, 0xDB}, "Ubiquiti Networks", "UniFi U7-Pro", OUF_CAT_AP, OUF_RGN_US},
    {{0xE0, 0x63, 0xDA}, "Ubiquiti Networks", "UniFi U6-IW", OUF_CAT_AP, OUF_RGN_US},
    {{0xF4, 0x92, 0xBF}, "Ubiquiti Networks", "UniFi UAP", OUF_CAT_AP, OUF_RGN_US},
    {{0xFC, 0xEC, 0xDA}, "Ubiquiti Networks", "UniFi UAP-AC", OUF_CAT_AP, OUF_RGN_US},
    {{0x18, 0xE8, 0x29}, "Ubiquiti Networks", "UniFi U6+", OUF_CAT_AP, OUF_RGN_US},
    {{0x60, 0x22, 0x32}, "Ubiquiti Networks", "UniFi UAP-AC-SHD", OUF_CAT_AP, OUF_RGN_US},
    {{0x74, 0x83, 0xC2}, "Ubiquiti Networks", "UniFi UAP-nano-HD", OUF_CAT_AP, OUF_RGN_US},
    // ── ASUS ─────────────────────────────────────────────────────────────────────
    {{0x00, 0x08, 0xA1}, "ASUSTeK Computer", "RT-N66U", OUF_CAT_AP, OUF_RGN_TW},
    {{0x00, 0x0E, 0xA6}, "ASUSTeK Computer", "RT-AC68U", OUF_CAT_AP, OUF_RGN_TW},
    {{0x00, 0x11, 0x2F}, "ASUSTeK Computer", "RT-AX88U", OUF_CAT_AP, OUF_RGN_TW},
    {{0x00, 0x13, 0xD4}, "ASUSTeK Computer", "RT-AX86U", OUF_CAT_AP, OUF_RGN_TW},
    {{0x00, 0x15, 0xF2}, "ASUSTeK Computer", "RT-AX3000", OUF_CAT_AP, OUF_RGN_TW},
    {{0x00, 0x17, 0x31}, "ASUSTeK Computer", "ROG Rapture", OUF_CAT_AP, OUF_RGN_TW},
    {{0x00, 0x1A, 0x92}, "ASUSTeK Computer", "ZenBook", OUF_CAT_LAPTOP, OUF_RGN_TW},
    {{0x00, 0x1B, 0xFC}, "ASUSTeK Computer", "ROG Strix", OUF_CAT_LAPTOP, OUF_RGN_TW},
    {{0x00, 0x1E, 0x8C}, "ASUSTeK Computer", "ZenFone", OUF_CAT_PHONE, OUF_RGN_TW},
    {{0x10, 0x7B, 0x44}, "ASUSTeK Computer", "ZenBook Pro", OUF_CAT_LAPTOP, OUF_RGN_TW},
    {{0x10, 0xBF, 0x48}, "ASUSTeK Computer", "ROG Phone", OUF_CAT_PHONE, OUF_RGN_TW},
    {{0x14, 0xDA, 0xE9}, "ASUSTeK Computer", "ZenBook Flip", OUF_CAT_LAPTOP, OUF_RGN_TW},
    {{0x18, 0x31, 0xBF}, "ASUSTeK Computer", "RT-AX82U", OUF_CAT_AP, OUF_RGN_TW},
    {{0x1C, 0x87, 0x2C}, "ASUSTeK Computer", "RT-AX58U", OUF_CAT_AP, OUF_RGN_TW},
    {{0x20, 0xCF, 0x30}, "ASUSTeK Computer", "ZenFone 8", OUF_CAT_PHONE, OUF_RGN_TW},
    {{0x24, 0x4B, 0xFE}, "ASUSTeK Computer", "ROG Phone 6", OUF_CAT_PHONE, OUF_RGN_TW},
    {{0x2C, 0xFD, 0xA1}, "ASUSTeK Computer", "RT-AX3000P", OUF_CAT_AP, OUF_RGN_TW},
    {{0x30, 0x5A, 0x3A}, "ASUSTeK Computer", "ZenBook 14", OUF_CAT_LAPTOP, OUF_RGN_TW},
    {{0x40, 0x87, 0x0C}, "ASUSTeK Computer", "ROG Strix G15", OUF_CAT_LAPTOP, OUF_RGN_TW},
    {{0x50, 0x46, 0x5D}, "ASUSTeK Computer", "ZenWiFi AX", OUF_CAT_AP, OUF_RGN_TW},
    {{0x54, 0x04, 0xA6}, "ASUSTeK Computer", "ROG Maximus", OUF_CAT_LAPTOP, OUF_RGN_TW},
    {{0x60, 0x45, 0xCB}, "ASUSTeK Computer", "ZenBook Pro Duo", OUF_CAT_LAPTOP, OUF_RGN_TW},
    {{0x70, 0x8B, 0xCD}, "ASUSTeK Computer", "RT-AC66U B1", OUF_CAT_AP, OUF_RGN_TW},
    {{0x74, 0xD0, 0x2B}, "ASUSTeK Computer", "ZenWiFi Pro", OUF_CAT_AP, OUF_RGN_TW},
    {{0x7C, 0x10, 0xC9}, "ASUSTeK Computer", "ROG Phone 7", OUF_CAT_PHONE, OUF_RGN_TW},
    {{0xAC, 0x22, 0x0B}, "ASUSTeK Computer", "RT-AC87U", OUF_CAT_AP, OUF_RGN_TW},
    {{0xB0, 0x6F, 0xE0}, "ASUSTeK Computer", "ZenBook S 13", OUF_CAT_LAPTOP, OUF_RGN_TW},
    {{0xB8, 0x9A, 0x2A}, "ASUSTeK Computer", "RT-AX92U", OUF_CAT_AP, OUF_RGN_TW},
    {{0xC8, 0x60, 0x00}, "ASUSTeK Computer", "ZenBook Flip S", OUF_CAT_LAPTOP, OUF_RGN_TW},
    {{0xCC, 0x9F, 0x7A}, "ASUSTeK Computer", "ROG Rapture GT-AX11000", OUF_CAT_AP, OUF_RGN_TW},
    {{0xD0, 0x17, 0xC2}, "ASUSTeK Computer", "ZenBook Pro 15", OUF_CAT_LAPTOP, OUF_RGN_TW},
    {{0xD8, 0x50, 0xE6}, "ASUSTeK Computer", "RT-AX88U Pro", OUF_CAT_AP, OUF_RGN_TW},
    {{0xF4, 0x92, 0xBF}, "ASUSTeK Computer", "RT-AX53U", OUF_CAT_AP, OUF_RGN_TW},
    // ── Aruba Networks (now HPE) ─────────────────────────────────────────────────
    {{0x00, 0x0B, 0x86}, "Aruba Networks", "Aruba AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x00, 0x24, 0x6C}, "Aruba Networks", "Aruba AP-105", OUF_CAT_AP, OUF_RGN_US},
    {{0x04, 0xBD, 0x88}, "Aruba Networks", "Aruba AP-325", OUF_CAT_AP, OUF_RGN_US},
    {{0x08, 0xEA, 0x44}, "Aruba Networks", "Aruba AP-515", OUF_CAT_AP, OUF_RGN_US},
    {{0x18, 0x64, 0x72}, "Aruba Networks", "Aruba AP-635", OUF_CAT_AP, OUF_RGN_US},
    {{0x20, 0x4C, 0x03}, "Aruba Networks", "Aruba IAP-315", OUF_CAT_AP, OUF_RGN_US},
    {{0x24, 0xDE, 0xC6}, "Aruba Networks", "Aruba AP-305", OUF_CAT_AP, OUF_RGN_US},
    {{0x40, 0xE3, 0xD6}, "Aruba Networks", "Aruba AP-555", OUF_CAT_AP, OUF_RGN_US},
    {{0x6C, 0xF3, 0x7F}, "Aruba Networks", "Aruba AP-575", OUF_CAT_AP, OUF_RGN_US},
    {{0x94, 0xB4, 0x0F}, "Aruba Networks", "Aruba AP-515", OUF_CAT_AP, OUF_RGN_US},
    {{0xAC, 0xA3, 0x1E}, "Aruba Networks", "Aruba AP-535", OUF_CAT_AP, OUF_RGN_US},
    {{0xD8, 0xC7, 0xC8}, "Aruba Networks", "Aruba AP-635", OUF_CAT_AP, OUF_RGN_US},
    {{0xE8, 0x26, 0x89}, "Aruba Networks", "Aruba AP-515", OUF_CAT_AP, OUF_RGN_US},
    // ── D-Link Systems ───────────────────────────────────────────────────────────
    {{0x00, 0x0F, 0x3D}, "D-Link Systems", "DSL/DIR Router", OUF_CAT_AP, OUF_RGN_TW},
    {{0x00, 0x11, 0x95}, "D-Link Systems", "DIR-615", OUF_CAT_AP, OUF_RGN_TW},
    {{0x00, 0x13, 0x46}, "D-Link Systems", "DIR-825", OUF_CAT_AP, OUF_RGN_TW},
    {{0x00, 0x15, 0xE9}, "D-Link Systems", "DIR-655", OUF_CAT_AP, OUF_RGN_TW},
    {{0x00, 0x17, 0x9A}, "D-Link Systems", "DIR-628", OUF_CAT_AP, OUF_RGN_TW},
    {{0x00, 0x19, 0x5B}, "D-Link Systems", "DIR-635", OUF_CAT_AP, OUF_RGN_TW},
    {{0x00, 0x1B, 0x11}, "D-Link Systems", "DSL-2750U", OUF_CAT_AP, OUF_RGN_TW},
    {{0x00, 0x1C, 0xF0}, "D-Link Systems", "DIR-825 B1", OUF_CAT_AP, OUF_RGN_TW},
    {{0x00, 0x1E, 0x58}, "D-Link Systems", "DIR-855", OUF_CAT_AP, OUF_RGN_TW},
    {{0x00, 0x21, 0x91}, "D-Link Systems", "DIR-825 C1", OUF_CAT_AP, OUF_RGN_TW},
    {{0x00, 0x22, 0xB0}, "D-Link Systems", "DIR-835", OUF_CAT_AP, OUF_RGN_TW},
    {{0x00, 0x24, 0x01}, "D-Link Systems", "DIR-645", OUF_CAT_AP, OUF_RGN_TW},
    {{0x14, 0xD6, 0x4D}, "D-Link Systems", "DIR-860L", OUF_CAT_AP, OUF_RGN_TW},
    {{0x18, 0x0F, 0x76}, "D-Link Systems", "DIR-880L", OUF_CAT_AP, OUF_RGN_TW},
    {{0x1C, 0x7E, 0xE5}, "D-Link Systems", "DIR-890L", OUF_CAT_AP, OUF_RGN_TW},
    {{0x28, 0x10, 0x7B}, "D-Link Systems", "DIR-895L", OUF_CAT_AP, OUF_RGN_TW},
    {{0x34, 0x08, 0x04}, "D-Link Systems", "DIR-882", OUF_CAT_AP, OUF_RGN_TW},
    {{0x34, 0x21, 0x09}, "D-Link Systems", "DIR-879", OUF_CAT_AP, OUF_RGN_TW},
    {{0x44, 0x84, 0xF0}, "D-Link Systems", "DIR-X1560", OUF_CAT_AP, OUF_RGN_TW},
    {{0x4C, 0x00, 0x82}, "D-Link Systems", "DIR-X3260", OUF_CAT_AP, OUF_RGN_TW},
    {{0x60, 0x63, 0x4C}, "D-Link Systems", "DIR-2660", OUF_CAT_AP, OUF_RGN_TW},
    {{0x74, 0xDA, 0xDA}, "D-Link Systems", "DIR-X5460", OUF_CAT_AP, OUF_RGN_TW},
    {{0x9C, 0x72, 0x40}, "D-Link Systems", "DIR-3040", OUF_CAT_AP, OUF_RGN_TW},
    {{0xA0, 0xAB, 0x1B}, "D-Link Systems", "DIR-X9000", OUF_CAT_AP, OUF_RGN_TW},
    {{0xB4, 0xFF, 0xB4}, "D-Link Systems", "DIR-5460", OUF_CAT_AP, OUF_RGN_TW},
    {{0xEC, 0x1A, 0x59}, "D-Link Systems", "DSL-2750E", OUF_CAT_AP, OUF_RGN_TW},
    {{0xF0, 0x7D, 0x68}, "D-Link Systems", "DIR-842", OUF_CAT_AP, OUF_RGN_TW},
    {{0xF4, 0x06, 0x8D}, "D-Link Systems", "DIR-X1870", OUF_CAT_AP, OUF_RGN_TW},
    // ── Ruckus Wireless / CommScope ──────────────────────────────────────────────
    {{0x00, 0x13, 0x92}, "Ruckus Wireless", "ZoneFlex AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x24, 0xC9, 0xA1}, "Ruckus Wireless", "R610 AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x2C, 0xD0, 0x2D}, "Ruckus Wireless", "R710 AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x38, 0xFF, 0x36}, "Ruckus Wireless", "R750 AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x48, 0x5F, 0x99}, "Ruckus Wireless", "R850 AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x54, 0xEC, 0x2F}, "Ruckus Wireless", "T310 AP", OUF_CAT_AP, OUF_RGN_US},
    {{0x58, 0x93, 0x96}, "Ruckus Wireless", "R550 AP", OUF_CAT_AP, OUF_RGN_US},
    {{0xBC, 0x9C, 0x31}, "Ruckus Wireless", "R350 AP", OUF_CAT_AP, OUF_RGN_US},
    {{0xCC, 0x17, 0x08}, "Ruckus Wireless", "H510 AP", OUF_CAT_AP, OUF_RGN_US},
    {{0xEC, 0x8C, 0xA2}, "Ruckus Wireless", "M510 AP", OUF_CAT_AP, OUF_RGN_US},
    // ── VMware Inc ───────────────────────────────────────────────────────────────
    {{0x00, 0x0C, 0x29}, "VMware Inc", "VMware Virtual NIC", OUF_CAT_VIRTUAL, OUF_RGN_US},
    {{0x00, 0x50, 0x56}, "VMware Inc", "VMware ESXi vNIC", OUF_CAT_VIRTUAL, OUF_RGN_US},
    {{0x00, 0x05, 0x69}, "VMware Inc", "VMware Workstation", OUF_CAT_VIRTUAL, OUF_RGN_US},
    {{0x00, 0x1C, 0x14}, "VMware Inc", "VMware Fusion", OUF_CAT_VIRTUAL, OUF_RGN_US},
    // ── Raspberry Pi Foundation ──────────────────────────────────────────────────
    {{0x28, 0xCD, 0xC1}, "Raspberry Pi Foundation", "Raspberry Pi 4B", OUF_CAT_IOT, OUF_RGN_EU},
    {{0xDC, 0xA6, 0x32}, "Raspberry Pi Foundation", "Raspberry Pi 4", OUF_CAT_IOT, OUF_RGN_EU},
    {{0xB8, 0x27, 0xEB}, "Raspberry Pi Foundation", "Raspberry Pi 3B", OUF_CAT_IOT, OUF_RGN_EU},
    {{0xE4, 0x5F, 0x01}, "Raspberry Pi Foundation", "Raspberry Pi 5", OUF_CAT_IOT, OUF_RGN_EU},
    {{0xD8, 0x3A, 0xDD}, "Raspberry Pi Foundation", "Raspberry Pi Zero W", OUF_CAT_IOT, OUF_RGN_EU},
    {{0x2C, 0xCF, 0x67}, "Raspberry Pi Foundation", "Raspberry Pi 3B+", OUF_CAT_IOT, OUF_RGN_EU},
    // ── Espressif Systems ────────────────────────────────────────────────────────
    {{0x24, 0x0A, 0xC4}, "Espressif Systems", "ESP32", OUF_CAT_IOT, OUF_RGN_CN},
    {{0x30, 0xAE, 0xA4}, "Espressif Systems", "ESP32-S2", OUF_CAT_IOT, OUF_RGN_CN},
    {{0x3C, 0x71, 0xBF}, "Espressif Systems", "ESP8266", OUF_CAT_IOT, OUF_RGN_CN},
    {{0x48, 0x3F, 0xDA}, "Espressif Systems", "ESP32", OUF_CAT_IOT, OUF_RGN_CN},
    {{0x50, 0x02, 0x91}, "Espressif Systems", "ESP32-C3", OUF_CAT_IOT, OUF_RGN_CN},
    {{0x54, 0x43, 0x54}, "Espressif Systems", "ESP32-S3", OUF_CAT_IOT, OUF_RGN_CN},
    {{0x58, 0xBF, 0x25}, "Espressif Systems", "ESP8266", OUF_CAT_IOT, OUF_RGN_CN},
    {{0x60, 0x01, 0x94}, "Espressif Systems", "ESP32", OUF_CAT_IOT, OUF_RGN_CN},
    {{0x7C, 0xDF, 0xA1}, "Espressif Systems", "ESP32-H2", OUF_CAT_IOT, OUF_RGN_CN},
    {{0x80, 0x7D, 0x3A}, "Espressif Systems", "ESP32", OUF_CAT_IOT, OUF_RGN_CN},
    {{0x84, 0x0D, 0x8E}, "Espressif Systems", "ESP8266", OUF_CAT_IOT, OUF_RGN_CN},
    {{0x8C, 0xAA, 0xB5}, "Espressif Systems", "ESP32", OUF_CAT_IOT, OUF_RGN_CN},
    {{0x94, 0xB9, 0x7E}, "Espressif Systems", "ESP32-C6", OUF_CAT_IOT, OUF_RGN_CN},
    {{0xA0, 0x20, 0xA6}, "Espressif Systems", "ESP32", OUF_CAT_IOT, OUF_RGN_CN},
    {{0xAC, 0x0B, 0xFB}, "Espressif Systems", "ESP32", OUF_CAT_IOT, OUF_RGN_CN},
    {{0xB4, 0xE6, 0x2D}, "Espressif Systems", "ESP32", OUF_CAT_IOT, OUF_RGN_CN},
    {{0xCC, 0x50, 0xE3}, "Espressif Systems", "ESP32", OUF_CAT_IOT, OUF_RGN_CN},
    {{0xD8, 0xBC, 0x38}, "Espressif Systems", "ESP32", OUF_CAT_IOT, OUF_RGN_CN},
    {{0xEC, 0xFA, 0xBC}, "Espressif Systems", "ESP8266", OUF_CAT_IOT, OUF_RGN_CN},
    {{0xF4, 0xCF, 0xA2}, "Espressif Systems", "ESP32", OUF_CAT_IOT, OUF_RGN_CN},
    {{0xFC, 0xF5, 0xC4}, "Espressif Systems", "ESP32", OUF_CAT_IOT, OUF_RGN_CN},
    // ── Xiaomi Communications ─────────────────────────────────────────────────────
    {{0x00, 0x9E, 0xC8}, "Xiaomi Communications", "Redmi Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x0C, 0x1D, 0xAF}, "Xiaomi Communications", "Mi Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x14, 0xF6, 0x5A}, "Xiaomi Communications", "Redmi Note", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x18, 0x59, 0x36}, "Xiaomi Communications", "Mi Band", OUF_CAT_WEARABLE, OUF_RGN_CN},
    {{0x20, 0x82, 0xC0}, "Xiaomi Communications", "Mi Router", OUF_CAT_AP, OUF_RGN_CN},
    {{0x28, 0xE3, 0x1F}, "Xiaomi Communications", "Xiaomi Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x34, 0x80, 0xB3}, "Xiaomi Communications", "Redmi 10", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x38, 0xA4, 0x0B}, "Xiaomi Communications", "Mi TV", OUF_CAT_TV, OUF_RGN_CN},
    {{0x40, 0x31, 0x3C}, "Xiaomi Communications", "Xiaomi Router", OUF_CAT_AP, OUF_RGN_CN},
    {{0x50, 0x64, 0x2B}, "Xiaomi Communications", "Xiaomi Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x64, 0xCC, 0x2E}, "Xiaomi Communications", "Mi Mix", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x74, 0x23, 0x44}, "Xiaomi Communications", "Redmi Note 11", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x78, 0x11, 0xDC}, "Xiaomi Communications", "Xiaomi 12", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x7C, 0xFE, 0x90}, "Xiaomi Communications", "Xiaomi 13", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x8C, 0xBE, 0xBE}, "Xiaomi Communications", "Redmi Note 12", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0x98, 0xFA, 0xE3}, "Xiaomi Communications", "Xiaomi 14", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0xAC, 0xF7, 0xF3}, "Xiaomi Communications", "Redmi 12", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0xC4, 0x0B, 0xCB}, "Xiaomi Communications", "Xiaomi Phone", OUF_CAT_PHONE, OUF_RGN_CN},
    {{0xFC, 0x64, 0xBA}, "Xiaomi Communications", "Xiaomi Phone", OUF_CAT_PHONE, OUF_RGN_CN},
};
static const int OUF_TABLE_SIZE = sizeof(OUF_TABLE) / sizeof(OUF_TABLE[0]);

// ── Runtime stats ─────────────────────────────────────────────────────────────
struct OufStats {
  uint32_t total_lookups;
  uint32_t hits;
  uint32_t misses;
  uint32_t random_mac_detected;
  uint32_t apple_seen;
  uint32_t cisco_seen;
  uint32_t samsung_seen;
  uint32_t intel_seen;
  uint32_t huawei_seen;
  uint32_t google_seen;
  uint32_t amazon_seen;
  uint32_t espressif_seen;
  uint32_t unknown_seen;
};
static OufStats g_ouf_stats = {};

// ── Helper: detect locally-administered (random) MAC ─────────────────────────
// Bit 1 of first octet set → locally administered (iOS/Android MAC randomization)
static bool ouf_is_random(const uint8_t mac[6]) {
  return (mac[0] & 0x02) != 0;
}

// ── Helper: detect multicast MAC ─────────────────────────────────────────────
static bool ouf_is_multicast(const uint8_t mac[6]) {
  return (mac[0] & 0x01) != 0;
}

// ── Binary search lookup ──────────────────────────────────────────────────────
// Table must be sorted by prefix; linear scan used here for simplicity
// (table is small enough to scan in <1ms on ESP32 at 240MHz)
static const OuiFullEntry *ouf_lookup(const uint8_t mac[6]) {
  g_ouf_stats.total_lookups++;
  if (ouf_is_random(mac)) {
    g_ouf_stats.random_mac_detected++;
    return nullptr;
  }
  for (int i = 0; i < OUF_TABLE_SIZE; i++) {
    if (OUF_TABLE[i].prefix[0] == mac[0] && OUF_TABLE[i].prefix[1] == mac[1] &&
        OUF_TABLE[i].prefix[2] == mac[2]) {
      g_ouf_stats.hits++;
      // Track per-vendor stats
      OufRegion rgn = OUF_TABLE[i].region;
      const char *v = OUF_TABLE[i].vendor;
      if (strncmp(v, "Apple", 5) == 0)
        g_ouf_stats.apple_seen++;
      else if (strncmp(v, "Cisco", 5) == 0)
        g_ouf_stats.cisco_seen++;
      else if (strncmp(v, "Samsung", 7) == 0)
        g_ouf_stats.samsung_seen++;
      else if (strncmp(v, "Intel", 5) == 0)
        g_ouf_stats.intel_seen++;
      else if (strncmp(v, "Huawei", 6) == 0)
        g_ouf_stats.huawei_seen++;
      else if (strncmp(v, "Google", 6) == 0)
        g_ouf_stats.google_seen++;
      else if (strncmp(v, "Amazon", 6) == 0)
        g_ouf_stats.amazon_seen++;
      else if (strncmp(v, "Espressif", 9) == 0)
        g_ouf_stats.espressif_seen++;
      return &OUF_TABLE[i];
    }
  }
  g_ouf_stats.misses++;
  g_ouf_stats.unknown_seen++;
  return nullptr;
}

// ── Convenience wrappers ──────────────────────────────────────────────────────
static const char *ouf_vendor(const uint8_t mac[6]) {
  if (ouf_is_random(mac))
    return "Random/Private";
  if (ouf_is_multicast(mac))
    return "Multicast";
  const OuiFullEntry *e = ouf_lookup(mac);
  return e ? e->vendor : "Unknown";
}

static OufCat ouf_category(const uint8_t mac[6]) {
  const OuiFullEntry *e = ouf_lookup(mac);
  return e ? e->category : OUF_CAT_UNKNOWN;
}

static OufRegion ouf_region(const uint8_t mac[6]) {
  const OuiFullEntry *e = ouf_lookup(mac);
  return e ? e->region : OUF_RGN_UNKNOWN;
}

static const char *ouf_products(const uint8_t mac[6]) {
  const OuiFullEntry *e = ouf_lookup(mac);
  return e ? e->products : "";
}

// ── Category name strings ─────────────────────────────────────────────────────
static const char *ouf_cat_name(OufCat c) {
  switch (c) {
    case OUF_CAT_PHONE:
      return "Phone/Tablet";
    case OUF_CAT_LAPTOP:
      return "Laptop/Desktop";
    case OUF_CAT_AP:
      return "Access Point";
    case OUF_CAT_CAMERA:
      return "Camera";
    case OUF_CAT_IOT:
      return "IoT Device";
    case OUF_CAT_PRINTER:
      return "Printer";
    case OUF_CAT_TV:
      return "Smart TV";
    case OUF_CAT_WEARABLE:
      return "Wearable";
    case OUF_CAT_VEHICLE:
      return "Vehicle";
    case OUF_CAT_INFRA:
      return "Network Infra";
    case OUF_CAT_GAMING:
      return "Gaming";
    case OUF_CAT_VIRTUAL:
      return "Virtual/VM";
    case OUF_CAT_MEDICAL:
      return "Medical";
    case OUF_CAT_INDUSTRIAL:
      return "Industrial";
    case OUF_CAT_AUDIO:
      return "Audio/Speaker";
    default:
      return "Unknown";
  }
}

static const char *ouf_region_name(OufRegion r) {
  switch (r) {
    case OUF_RGN_US:
      return "USA";
    case OUF_RGN_CN:
      return "China";
    case OUF_RGN_KR:
      return "Korea";
    case OUF_RGN_JP:
      return "Japan";
    case OUF_RGN_TW:
      return "Taiwan";
    case OUF_RGN_EU:
      return "Europe";
    case OUF_RGN_IN:
      return "India";
    case OUF_RGN_SG:
      return "Singapore";
    default:
      return "Unknown";
  }
}

// ── MAC-to-JSON helper ────────────────────────────────────────────────────────
static String ouf_mac_json(const uint8_t mac[6]) {
  char buf[18];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3],
           mac[4], mac[5]);
  const OuiFullEntry *e = ouf_lookup(mac);
  String j = "{";
  j += "\"mac\":\"";
  j += buf;
  j += "\"";
  j += ",\"vendor\":\"";
  j += ouf_vendor(mac);
  j += "\"";
  j += ",\"category\":\"";
  j += ouf_cat_name(e ? e->category : OUF_CAT_UNKNOWN);
  j += "\"";
  j += ",\"region\":\"";
  j += ouf_region_name(e ? e->region : OUF_RGN_UNKNOWN);
  j += "\"";
  j += ",\"products\":\"";
  j += (e ? e->products : "");
  j += "\"";
  j += ",\"random\":";
  j += ouf_is_random(mac) ? 1 : 0;
  j += ",\"multicast\":";
  j += ouf_is_multicast(mac) ? 1 : 0;
  j += "}";
  return j;
}

// ── Batch lookup from hex string "AA:BB:CC:DD:EE:FF" ─────────────────────────
static bool ouf_parse_mac(const char *s, uint8_t out[6]) {
  if (!s || strlen(s) < 17)
    return false;
  unsigned v[6];
  if (sscanf(s, "%x:%x:%x:%x:%x:%x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 6)
    return false;
  for (int i = 0; i < 6; i++)
    out[i] = (uint8_t)v[i];
  return true;
}

// ── Fingerprint confidence scoring ───────────────────────────────────────────
// Returns 0-100 confidence that the device type is what we think
static uint8_t ouf_confidence(const uint8_t mac[6]) {
  if (ouf_is_random(mac))
    return 10;  // Can't tell much from random MAC
  const OuiFullEntry *e = ouf_lookup(mac);
  if (!e)
    return 20;  // Unknown vendor
  if (e->category == OUF_CAT_UNKNOWN)
    return 40;
  return 85;  // Known vendor + category
}

// ── Threat classification ─────────────────────────────────────────────────────
// Returns true if this OUI belongs to known monitoring/pentest hardware
static bool ouf_is_known_attack_hw(const uint8_t mac[6]) {
  // Alfa Network, Hak5, common pentest devices
  static const uint8_t pentest_ouis[][3] = {
      {0x00, 0xC0, 0xCA},  // Alfa Network
      {0x00, 0x25, 0x9C},  // Alfa (Atheros)
      {0xE0, 0x3F, 0x49},  // common pentest NIC
      {0x20, 0xC0, 0x47},  // Hak5 Pineapple
      {0xF8, 0x1A, 0x67},  // common audit NIC
  };
  for (auto &p : pentest_ouis) {
    if (mac[0] == p[0] && mac[1] == p[1] && mac[2] == p[2])
      return true;
  }
  return false;
}

// ── Stats JSON ───────────────────────────────────────────────────────────────
static String ouf_stats_json() {
  String j = "{";
  j += "\"table_size\":" + String(OUF_TABLE_SIZE);
  j += ",\"lookups\":" + String(g_ouf_stats.total_lookups);
  j += ",\"hits\":" + String(g_ouf_stats.hits);
  j += ",\"misses\":" + String(g_ouf_stats.misses);
  j += ",\"random_macs\":" + String(g_ouf_stats.random_mac_detected);
  j += ",\"apple\":" + String(g_ouf_stats.apple_seen);
  j += ",\"cisco\":" + String(g_ouf_stats.cisco_seen);
  j += ",\"samsung\":" + String(g_ouf_stats.samsung_seen);
  j += ",\"intel\":" + String(g_ouf_stats.intel_seen);
  j += ",\"huawei\":" + String(g_ouf_stats.huawei_seen);
  j += ",\"google\":" + String(g_ouf_stats.google_seen);
  j += ",\"amazon\":" + String(g_ouf_stats.amazon_seen);
  j += ",\"espressif\":" + String(g_ouf_stats.espressif_seen);
  j += ",\"unknown\":" + String(g_ouf_stats.unknown_seen);
  j += "}";
  return j;
}

// ── HTTP route handlers ───────────────────────────────────────────────────────
static void ouf_routes() {
  // GET /api/oui/lookup?mac=AA:BB:CC:DD:EE:FF
  extern WebServer server;
  server.on("/api/oui/lookup", HTTP_GET, []() {
    if (!server.hasArg("mac")) {
      server.send(400, "application/json", "{\"error\":\"mac required\"}");
      return;
    }
    uint8_t mac[6];
    if (!ouf_parse_mac(server.arg("mac").c_str(), mac)) {
      server.send(400, "application/json", "{\"error\":\"invalid mac\"}");
      return;
    }
    server.send(200, "application/json", ouf_mac_json(mac));
  });

  // GET /api/oui/stats
  server.on("/api/oui/stats", HTTP_GET,
            []() { server.send(200, "application/json", ouf_stats_json()); });

  // GET /api/oui/list?page=0&per=50 — paginated OUI table dump
  server.on("/api/oui/list", HTTP_GET, []() {
    int page = server.hasArg("page") ? server.arg("page").toInt() : 0;
    int per = server.hasArg("per") ? server.arg("per").toInt() : 50;
    if (per < 1 || per > 200)
      per = 50;
    int start = page * per, end = start + per;
    if (end > OUF_TABLE_SIZE)
      end = OUF_TABLE_SIZE;
    String j = "{\"total\":" + String(OUF_TABLE_SIZE);
    j += ",\"page\":" + String(page) + ",\"per\":" + String(per);
    j += ",\"entries\":[";
    for (int i = start; i < end; i++) {
      if (i > start)
        j += ",";
      char buf[20];
      snprintf(buf, sizeof(buf), "%02X:%02X:%02X", OUF_TABLE[i].prefix[0], OUF_TABLE[i].prefix[1],
               OUF_TABLE[i].prefix[2]);
      j += "{\"prefix\":\"";
      j += buf;
      j += "\",\"vendor\":\"";
      j += OUF_TABLE[i].vendor;
      j += "\",\"products\":\"";
      j += OUF_TABLE[i].products;
      j += "\",\"cat\":\"";
      j += ouf_cat_name(OUF_TABLE[i].category);
      j += "\",\"region\":\"";
      j += ouf_region_name(OUF_TABLE[i].region);
      j += "\"}";
    }
    j += "]}";
    server.send(200, "application/json", j);
  });

  // GET /api/oui/vendors — list unique vendors with counts
  server.on("/api/oui/vendors", HTTP_GET, []() {
    // Simple count per first word of vendor name
    struct VEntry {
      const char *name;
      int count;
    };
    static VEntry vendors[32];
    static int vcount = 0;
    vcount = 0;
    for (int i = 0; i < OUF_TABLE_SIZE; i++) {
      bool found = false;
      for (int v = 0; v < vcount; v++) {
        if (strcmp(vendors[v].name, OUF_TABLE[i].vendor) == 0) {
          vendors[v].count++;
          found = true;
          break;
        }
      }
      if (!found && vcount < 32) {
        vendors[vcount++] = {OUF_TABLE[i].vendor, 1};
      }
    }
    String j = "{\"vendors\":[";
    for (int v = 0; v < vcount; v++) {
      if (v)
        j += ",";
      j += "{\"name\":\"";
      j += vendors[v].name;
      j += "\",\"ouis\":";
      j += vendors[v].count;
      j += "}";
    }
    j += "]}";
    server.send(200, "application/json", j);
  });
}

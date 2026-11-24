#pragma once
// vendor_stats.h — Vendor distribution statistics engine.
// Tracks per-vendor device counts from OUI observations.
// Prefix: vst_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define VST_MAX_VENDORS  64
#define VST_MAX_DEVICES  128

struct VstVendor {
  bool    active;
  char    name[32];
  uint8_t oui[3];
  uint32_t device_count;
  uint32_t frame_count;
  int8_t   avg_rssi;     // EMA
  uint8_t  category;     // 0-11 mirroring VomCategory
  uint32_t first_ts;
  uint32_t last_ts;
};

static struct {
  VstVendor vendors[VST_MAX_VENDORS];
  uint8_t   vendor_count;
  uint32_t  total_obs;
  uint32_t  unknown_count;

  // Category distribution (count of devices per category)
  uint32_t  cat_count[12];

  // Channel spread per vendor (which channels a vendor appeared on)
  // Using bitmask: bit (ch-1) set if seen on that channel
  uint16_t  vendor_ch_mask[VST_MAX_VENDORS];
} g_vst = {};

// ─── Category names ───────────────────────────────────────────────────────────
static const char* VST_CAT_NAMES[] = {
  "Unknown","Apple","Samsung","Cisco","Ubiquiti",
  "Google","Amazon","IoT","Camera","Printer","Router","Other"
};

// ─── OUI lookup (small inline table for common vendors) ───────────────────────
struct VstOuiEntry {
  uint8_t oui[3];
  uint8_t category;
  const char* name;
};

static const VstOuiEntry VST_OUI_TABLE[] = {
  {{0x3C,0x06,0x30}, 1, "Apple"},
  {{0xF0,0xDB,0xE2}, 1, "Apple"},
  {{0xBC,0x52,0xB7}, 1, "Apple"},
  {{0x60,0x03,0x08}, 1, "Apple"},
  {{0xCC,0x08,0x8D}, 1, "Apple"},
  {{0x2C,0x61,0xF6}, 1, "Apple"},
  {{0xE8,0xBB,0xA8}, 2, "Samsung"},
  {{0x14,0x1A,0x97}, 2, "Samsung"},
  {{0x00,0x16,0x32}, 2, "Samsung"},
  {{0x40,0xB0,0x76}, 2, "Samsung"},
  {{0x00,0x1A,0x73}, 3, "Cisco"},
  {{0x00,0x0F,0x24}, 3, "Cisco"},
  {{0x24,0xBE,0x05}, 3, "Cisco"},
  {{0x00,0x50,0x56}, 3, "Cisco"},
  {{0x24,0xA4,0x3C}, 4, "Ubiquiti"},
  {{0x00,0x27,0x22}, 4, "Ubiquiti"},
  {{0xFC,0xEC,0xDA}, 4, "Ubiquiti"},
  {{0x68,0xD7,0x9A}, 4, "Ubiquiti"},
  {{0x94,0x65,0x9C}, 5, "Google"},
  {{0xF4,0xF5,0xDB}, 5, "Google"},
  {{0x48,0xD6,0xD5}, 5, "Google/Nest"},
  {{0xFC,0xA1,0x83}, 6, "Amazon"},
  {{0x68,0x37,0xE9}, 6, "Amazon"},
  {{0x00,0x50,0xF2}, 11, "Microsoft"},
  {{0xB8,0x27,0xEB}, 11, "Raspberry Pi"},
  {{0xDC,0xA6,0x32}, 11, "Raspberry Pi"},
  {{0x28,0xD2,0x44}, 7,  "Espressif/IoT"},
  {{0x24,0x0A,0xC4}, 7,  "Espressif/IoT"},
  {{0x30,0xAE,0xA4}, 7,  "Espressif/IoT"},
  {{0xAC,0x67,0xB2}, 7,  "Espressif/IoT"},
  {{0x00,0x0C,0xE7}, 8,  "IP Camera"},
  {{0xC0,0xC9,0xE3}, 8,  "Hikvision"},
  {{0x44,0x19,0xB6}, 8,  "Dahua"},
  {{0x00,0x11,0xBC}, 9,  "HP Printer"},
  {{0x64,0x51,0x06}, 9,  "HP Printer"},
  {{0x1C,0xC1,0xDE}, 10, "TP-Link Router"},
  {{0x50,0xC7,0xBF}, 10, "TP-Link Router"},
  {{0xE8,0x65,0xD4}, 10, "Netgear"},
  {{0xC4,0x04,0x15}, 10, "Netgear"},
};
static const int VST_OUI_TABLE_LEN = sizeof(VST_OUI_TABLE)/sizeof(VST_OUI_TABLE[0]);

static bool vst_oui_eq(const uint8_t *a, const uint8_t *b) {
  return a[0]==b[0] && a[1]==b[1] && a[2]==b[2];
}

static const VstOuiEntry* vst_lookup_oui(const uint8_t *mac) {
  for (int i = 0; i < VST_OUI_TABLE_LEN; i++)
    if (vst_oui_eq(mac, VST_OUI_TABLE[i].oui))
      return &VST_OUI_TABLE[i];
  return nullptr;
}

// ─── Find or allocate a vendor slot ──────────────────────────────────────────
static VstVendor* vst_find_or_alloc(const uint8_t *oui, const char *name, uint8_t cat) {
  for (int i = 0; i < VST_MAX_VENDORS; i++)
    if (g_vst.vendors[i].active && vst_oui_eq(g_vst.vendors[i].oui, oui))
      return &g_vst.vendors[i];

  for (int i = 0; i < VST_MAX_VENDORS; i++) {
    if (!g_vst.vendors[i].active) {
      VstVendor &v = g_vst.vendors[i];
      memset(&v, 0, sizeof(v));
      v.active = true;
      memcpy(v.oui, oui, 3);
      strncpy(v.name, name, 31);
      v.category = cat;
      v.avg_rssi = -80;
      v.first_ts = millis()/1000;
      g_vst.vendor_count++;
      return &v;
    }
  }
  return nullptr;  // table full
}

// ─── Ingest ───────────────────────────────────────────────────────────────────
void vst_ingest(const uint8_t *mac, int8_t rssi, uint8_t channel, uint32_t frames = 1) {
  g_vst.total_obs++;

  const VstOuiEntry *entry = vst_lookup_oui(mac);
  if (!entry) {
    g_vst.unknown_count++;
    return;
  }

  VstVendor *v = vst_find_or_alloc(mac, entry->name, entry->category);
  if (!v) return;

  v->frame_count += frames;
  v->device_count++;  // may overcount, but serves as proxy for activity
  v->last_ts = millis()/1000;

  // EMA RSSI (alpha=0.2)
  v->avg_rssi = (int8_t)((v->avg_rssi * 8 + rssi * 2) / 10);

  // Channel mask
  if (channel > 0 && channel <= 14) {
    int idx = (int)(v - g_vst.vendors);
    if (idx >= 0 && idx < VST_MAX_VENDORS)
      g_vst.vendor_ch_mask[idx] |= (1 << (channel-1));
  }

  if (entry->category < 12)
    g_vst.cat_count[entry->category]++;
}

// ─── JSON ─────────────────────────────────────────────────────────────────────
static String vst_json_summary() {
  String s = "{\"vendor_count\":" + String(g_vst.vendor_count);
  s += ",\"total_obs\":" + String(g_vst.total_obs);
  s += ",\"unknown\":" + String(g_vst.unknown_count);
  s += ",\"categories\":[";
  for (int i = 0; i < 12; i++) {
    if (i) s += ",";
    s += "{\"name\":\"" + String(VST_CAT_NAMES[i]) + "\"";
    s += ",\"count\":" + String(g_vst.cat_count[i]) + "}";
  }
  s += "],\"vendors\":[";
  bool first = true;
  for (int i = 0; i < VST_MAX_VENDORS; i++) {
    VstVendor &v = g_vst.vendors[i];
    if (!v.active) continue;
    if (!first) s += ",";
    first = false;
    s += "{\"oui\":\"" + String(v.oui[0],HEX) + ":" + String(v.oui[1],HEX) + ":" + String(v.oui[2],HEX) + "\"";
    s += ",\"name\":\"" + String(v.name) + "\"";
    s += ",\"cat\":\"" + String(VST_CAT_NAMES[v.category<12?v.category:0]) + "\"";
    s += ",\"frames\":" + String(v.frame_count);
    s += ",\"rssi\":" + String(v.avg_rssi);
    s += ",\"ts\":" + String(v.last_ts);
    s += "}";
  }
  s += "]}";
  return s;
}

// ─── Routes ──────────────────────────────────────────────────────────────────
inline void vst_routes() {
  server.on("/api/vst/summary", HTTP_GET, []() {
    server.send(200, "application/json", vst_json_summary());
  });
  server.on("/api/vst/clear", HTTP_POST, []() {
    memset(&g_vst, 0, sizeof(g_vst));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

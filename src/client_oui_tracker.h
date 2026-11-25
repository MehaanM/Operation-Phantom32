#pragma once
// client_oui_tracker.h — Client device OUI / manufacturer tracking.
// Builds a frequency table of device manufacturers from client MACs.
// Prefix: cot_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

#define COT_MAX_VENDORS 48
#define COT_MAX_CLIENTS 64

// Known client OUI entries
struct CotOuiEntry {
  uint8_t oui[3];
  const char *vendor;
  const char *category;  // Phone, Laptop, IoT, Gaming, etc.
};

static const CotOuiEntry COT_OUI_TABLE[] = {
    {{0xAC, 0xBC, 0x32}, "Apple", "Phone"},
    {{0x68, 0x96, 0xBC}, "Apple", "Phone"},
    {{0xC4, 0xB3, 0x01}, "Apple", "Phone"},
    {{0xDC, 0xA4, 0xCA}, "Apple", "Laptop"},
    {{0x28, 0xCF, 0xDA}, "Apple", "Laptop"},
    {{0x3C, 0x22, 0xFB}, "Apple", "Watch"},
    {{0x40, 0x49, 0x0F}, "Samsung", "Phone"},
    {{0x34, 0xAA, 0x8B}, "Samsung", "Phone"},
    {{0xA4, 0x0E, 0x2B}, "Samsung", "TV"},
    {{0xC4, 0x93, 0xD9}, "Google", "Phone"},
    {{0x28, 0x6D, 0x97}, "Google", "Phone"},
    {{0xF4, 0xF5, 0xDB}, "Google", "Home"},
    {{0xB8, 0x27, 0xEB}, "Raspberry Pi", "IoT"},
    {{0xE4, 0x5F, 0x01}, "Raspberry Pi", "IoT"},
    {{0x00, 0x0C, 0xE7}, "Intel", "Laptop"},
    {{0xCC, 0x20, 0xE8}, "Intel", "Laptop"},
    {{0x8C, 0xC8, 0x4B}, "Intel", "Laptop"},
    {{0xEC, 0xF4, 0xBB}, "Amazon", "Echo"},
    {{0x74, 0xC2, 0x46}, "Amazon", "Fire TV"},
    {{0xFC, 0x65, 0xDE}, "Amazon", "Kindle"},
    {{0x60, 0x38, 0xE0}, "Xiaomi", "Phone"},
    {{0x50, 0x87, 0xB8}, "Xiaomi", "Phone"},
    {{0xB0, 0xEC, 0x71}, "OnePlus", "Phone"},
    {{0xAC, 0x64, 0x62}, "LG", "Phone"},
    {{0x40, 0xBD, 0x32}, "LG", "TV"},
    {{0xCC, 0xBB, 0xFE}, "Microsoft", "Surface"},
    {{0xC0, 0x33, 0x5E}, "Microsoft", "Xbox"},
    {{0x00, 0x50, 0xF2}, "Microsoft", "IoT"},
    {{0xA4, 0xC3, 0xF0}, "Sony", "PlayStation"},
    {{0x00, 0xD9, 0xD1}, "Sony", "TV"},
    {{0x44, 0x65, 0x0D}, "Nintendo", "Switch"},
    {{0x08, 0xA1, 0x89}, "Nintendo", "Switch"},
};

static const int COT_OUI_COUNT = sizeof(COT_OUI_TABLE) / sizeof(COT_OUI_TABLE[0]);

struct CotVendor {
  bool active;
  char name[24];
  char category[16];
  uint32_t device_count;  // unique MACs seen
  uint32_t frame_count;
  int8_t avg_rssi;
};

struct CotClient {
  bool active;
  uint8_t mac[6];
  uint8_t oui[3];
  char vendor[24];
  char category[16];
  uint8_t channel;
  int8_t rssi;
  uint32_t frame_count;
  uint32_t last_ts;
};

static struct {
  CotVendor vendors[COT_MAX_VENDORS];
  uint8_t vendor_count;
  CotClient clients[COT_MAX_CLIENTS];
  uint8_t client_count;

  uint32_t total_clients;
  uint32_t identified;
  uint32_t unidentified;
} g_cot = {};

static bool cot_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static const CotOuiEntry *cot_lookup_oui(const uint8_t *mac) {
  for (int i = 0; i < COT_OUI_COUNT; i++) {
    if (mac[0] == COT_OUI_TABLE[i].oui[0] && mac[1] == COT_OUI_TABLE[i].oui[1] &&
        mac[2] == COT_OUI_TABLE[i].oui[2])
      return &COT_OUI_TABLE[i];
  }
  return nullptr;
}

static CotVendor *cot_find_or_alloc_vendor(const char *name, const char *cat) {
  for (int i = 0; i < COT_MAX_VENDORS; i++) {
    if (g_cot.vendors[i].active && strcmp(g_cot.vendors[i].name, name) == 0)
      return &g_cot.vendors[i];
  }
  for (int i = 0; i < COT_MAX_VENDORS; i++) {
    if (!g_cot.vendors[i].active) {
      CotVendor &v = g_cot.vendors[i];
      memset(&v, 0, sizeof(v));
      v.active = true;
      strncpy(v.name, name, 23);
      strncpy(v.category, cat, 15);
      g_cot.vendor_count++;
      return &v;
    }
  }
  return nullptr;
}

void cot_ingest(const uint8_t *mac, uint8_t channel, int8_t rssi) {
  if (!mac || mac[0] & 0x01)
    return;  // skip multicast/broadcast

  // Find existing client
  for (int i = 0; i < COT_MAX_CLIENTS; i++) {
    if (g_cot.clients[i].active && cot_mac_eq(g_cot.clients[i].mac, mac)) {
      g_cot.clients[i].frame_count++;
      g_cot.clients[i].rssi = rssi;
      g_cot.clients[i].channel = channel;
      g_cot.clients[i].last_ts = millis() / 1000;
      // Update vendor stat
      CotVendor *v = cot_find_or_alloc_vendor(g_cot.clients[i].vendor, g_cot.clients[i].category);
      if (v) {
        v->frame_count++;
        v->avg_rssi = (int8_t)((v->avg_rssi * 3 + rssi) / 4);
      }
      return;
    }
  }

  // New client
  for (int i = 0; i < COT_MAX_CLIENTS; i++) {
    if (!g_cot.clients[i].active) {
      CotClient &c = g_cot.clients[i];
      memset(&c, 0, sizeof(c));
      c.active = true;
      memcpy(c.mac, mac, 6);
      memcpy(c.oui, mac, 3);
      c.channel = channel;
      c.rssi = rssi;
      c.frame_count = 1;
      c.last_ts = millis() / 1000;

      const CotOuiEntry *e = cot_lookup_oui(mac);
      if (e) {
        strncpy(c.vendor, e->vendor, 23);
        strncpy(c.category, e->category, 15);
        g_cot.identified++;
      } else {
        strncpy(c.vendor, "Unknown", 23);
        strncpy(c.category, "Unknown", 15);
        g_cot.unidentified++;
      }

      CotVendor *v = cot_find_or_alloc_vendor(c.vendor, c.category);
      if (v) {
        v->device_count++;
        v->frame_count++;
        v->avg_rssi = rssi;
      }

      g_cot.client_count++;
      g_cot.total_clients++;
      return;
    }
  }
}

static void cot_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static String cot_json_summary() {
  String s = "{\"total_clients\":" + String(g_cot.total_clients);
  s += ",\"identified\":" + String(g_cot.identified);
  s += ",\"unidentified\":" + String(g_cot.unidentified);
  s += ",\"vendor_count\":" + String(g_cot.vendor_count);
  s += ",\"vendors\":[";
  bool first = true;
  for (int i = 0; i < COT_MAX_VENDORS; i++) {
    CotVendor &v = g_cot.vendors[i];
    if (!v.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    s += "{\"name\":\"" + String(v.name) + "\"";
    s += ",\"category\":\"" + String(v.category) + "\"";
    s += ",\"devices\":" + String(v.device_count);
    s += ",\"frames\":" + String(v.frame_count);
    s += ",\"avg_rssi\":" + String(v.avg_rssi);
    s += "}";
  }
  s += "]}";
  return s;
}

static String cot_json_clients() {
  String s = "[";
  char mac[18];
  bool first = true;
  for (int i = 0; i < COT_MAX_CLIENTS; i++) {
    CotClient &c = g_cot.clients[i];
    if (!c.active)
      continue;
    if (!first)
      s += ",";
    first = false;
    cot_mac_str(c.mac, mac);
    s += "{\"mac\":\"" + String(mac) + "\"";
    s += ",\"vendor\":\"" + String(c.vendor) + "\"";
    s += ",\"category\":\"" + String(c.category) + "\"";
    s += ",\"ch\":" + String(c.channel);
    s += ",\"rssi\":" + String(c.rssi);
    s += ",\"frames\":" + String(c.frame_count);
    s += ",\"last_ts\":" + String(c.last_ts);
    s += "}";
  }
  s += "]";
  return s;
}

inline void cot_routes() {
  server.on("/api/cot/summary", HTTP_GET,
            []() { server.send(200, "application/json", cot_json_summary()); });
  server.on("/api/cot/clients", HTTP_GET,
            []() { server.send(200, "application/json", cot_json_clients()); });
  server.on("/api/cot/clear", HTTP_POST, []() {
    memset(&g_cot, 0, sizeof(g_cot));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

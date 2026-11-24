#pragma once
// ble_scanner.h — Passive BLE advertisement scanner.
// Captures BLE device advertisements: device name, MAC, RSSI, services, TX power,
// manufacturer data (vendor OUI), and appearance. Tracks device history and proximity.
// Prefix: ble_

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

// BLE scanning uses the ESP32 BT controller in passive scan mode.
// We use the esp_bt_* APIs. BLE scanning does NOT require WiFi to be stopped.

#define BLE_MAX_DEVICES    64
#define BLE_MAX_SVCS        8
#define BLE_HIST_LEN       30
#define BLE_MAX_MFR_LEN    32

// Known BLE service UUIDs (16-bit short form for common services)
static const struct { uint16_t uuid; const char *name; } BLE_SVC_NAMES[] = {
  {0x180D, "Heart Rate"},
  {0x180F, "Battery"},
  {0x1800, "Generic Access"},
  {0x1801, "Generic Attribute"},
  {0x180A, "Device Info"},
  {0x1816, "Cycling Speed"},
  {0x1818, "Cycling Power"},
  {0x180E, "Phone Alert"},
  {0x1803, "Link Loss"},
  {0x1802, "Immediate Alert"},
  {0x1808, "Glucose"},
  {0xFEA0, "Google"},
  {0xFD5A, "DASH"},
  {0xFFFE, "Proprietary"},
  {0x0000, nullptr}
};

// BLE address types
enum BleAddrType : uint8_t {
  BLE_ADDR_PUBLIC = 0,
  BLE_ADDR_RANDOM_STATIC = 1,
  BLE_ADDR_RANDOM_RESOLVABLE = 2,
  BLE_ADDR_RANDOM_NON_RESOLVABLE = 3
};

// BLE advertisement event types
enum BleAdvType : uint8_t {
  BLE_ADV_IND = 0,        // connectable undirected
  BLE_ADV_DIRECT = 1,     // connectable directed
  BLE_ADV_SCAN_IND = 2,   // scannable undirected
  BLE_ADV_NON_CONN = 3,   // non-connectable undirected
  BLE_ADV_SCAN_RSP = 4    // scan response
};

struct BleDevice {
  bool    active;
  uint8_t addr[6];
  BleAddrType addr_type;
  char    name[33];
  int8_t  rssi;
  int8_t  tx_power;        // 127 = not present
  uint8_t appearance;      // BLE appearance category

  // Services advertised (up to BLE_MAX_SVCS)
  uint16_t svcs[BLE_MAX_SVCS];
  uint8_t  svc_count;

  // Manufacturer specific data
  uint16_t mfr_id;         // company identifier (0xFFFF = not present)
  uint8_t  mfr_data[BLE_MAX_MFR_LEN];
  uint8_t  mfr_data_len;

  // Connection / scan status flags
  bool     connectable;
  bool     scannable;
  BleAdvType adv_type;

  // Activity tracking
  uint32_t adv_count;
  uint32_t last_ts;
  uint32_t first_ts;

  // RSSI history (last BLE_HIST_LEN samples)
  int8_t   rssi_hist[BLE_HIST_LEN];
  uint8_t  rssi_head;
  uint8_t  rssi_count;
};

// Known BLE company IDs (Bluetooth SIG assigned numbers excerpt)
static const struct { uint16_t id; const char *name; } BLE_COMPANY[] = {
  {0x004C, "Apple"},
  {0x0006, "Microsoft"},
  {0x000F, "Broadcom"},
  {0x0059, "Nordic Semiconductor"},
  {0x0075, "Samsung"},
  {0x00E0, "Google"},
  {0x0171, "Amazon"},
  {0x02E5, "Fitbit"},
  {0x038F, "Xiaomi"},
  {0x0499, "Ruuvi"},
  {0xFFFF, nullptr}
};

static struct {
  BleDevice devices[BLE_MAX_DEVICES];
  uint16_t  count;
  uint32_t  total_adverts;
  bool      scanning;
} g_ble = {};

static bool ble_mac_eq(const uint8_t *a, const uint8_t *b) { return memcmp(a,b,6)==0; }

static BleDevice* ble_find(const uint8_t *addr) {
  for (int i = 0; i < BLE_MAX_DEVICES; i++)
    if (g_ble.devices[i].active && ble_mac_eq(g_ble.devices[i].addr, addr))
      return &g_ble.devices[i];
  return nullptr;
}

static BleDevice* ble_alloc(const uint8_t *addr) {
  for (int i = 0; i < BLE_MAX_DEVICES; i++) {
    if (!g_ble.devices[i].active) {
      BleDevice &d = g_ble.devices[i];
      memset(&d, 0, sizeof(d));
      d.active    = true;
      d.tx_power  = 127;
      d.mfr_id    = 0xFFFF;
      memcpy(d.addr, addr, 6);
      d.first_ts = millis()/1000;
      g_ble.count++;
      return &d;
    }
  }
  // LRU eviction: evict oldest non-connectable device
  uint32_t oldest = UINT32_MAX; int idx = 0;
  for (int i = 0; i < BLE_MAX_DEVICES; i++)
    if (!g_ble.devices[i].connectable && g_ble.devices[i].last_ts < oldest)
      { oldest = g_ble.devices[i].last_ts; idx = i; }
  memset(&g_ble.devices[idx], 0, sizeof(BleDevice));
  g_ble.devices[idx].active = true;
  g_ble.devices[idx].tx_power = 127;
  g_ble.devices[idx].mfr_id  = 0xFFFF;
  memcpy(g_ble.devices[idx].addr, addr, 6);
  g_ble.devices[idx].first_ts = millis()/1000;
  return &g_ble.devices[idx];
}

// Called from BLE scan callback — simulated here, real integration via esp_ble_gap_* APIs
void ble_ingest(const uint8_t *addr, BleAddrType addr_type,
                const char *name, int8_t rssi, int8_t tx_power,
                uint16_t mfr_id, const uint8_t *mfr_data, uint8_t mfr_len,
                const uint16_t *svcs, uint8_t svc_count,
                bool connectable, BleAdvType adv_type) {
  BleDevice *d = ble_find(addr);
  if (!d) d = ble_alloc(addr);
  if (!d) return;

  d->addr_type   = addr_type;
  d->connectable = connectable;
  d->adv_type    = adv_type;
  d->rssi        = rssi;
  if (tx_power != 127) d->tx_power = tx_power;
  d->last_ts     = millis()/1000;
  d->adv_count++;
  g_ble.total_adverts++;

  if (name && name[0] && d->name[0] == 0)
    strncpy(d->name, name, 32);

  if (mfr_id != 0xFFFF) {
    d->mfr_id = mfr_id;
    if (mfr_data && mfr_len > 0) {
      uint8_t copy_len = mfr_len < BLE_MAX_MFR_LEN ? mfr_len : BLE_MAX_MFR_LEN;
      memcpy(d->mfr_data, mfr_data, copy_len);
      d->mfr_data_len = copy_len;
    }
  }

  if (svcs && svc_count > 0 && d->svc_count == 0) {
    uint8_t cnt = svc_count < BLE_MAX_SVCS ? svc_count : BLE_MAX_SVCS;
    memcpy(d->svcs, svcs, cnt * sizeof(uint16_t));
    d->svc_count = cnt;
  }

  // Rolling RSSI history
  d->rssi_hist[d->rssi_head % BLE_HIST_LEN] = rssi;
  d->rssi_head = (d->rssi_head + 1) % BLE_HIST_LEN;
  if (d->rssi_count < BLE_HIST_LEN) d->rssi_count++;
}

static const char* ble_company_name(uint16_t id) {
  for (int i = 0; BLE_COMPANY[i].name; i++)
    if (BLE_COMPANY[i].id == id) return BLE_COMPANY[i].name;
  return nullptr;
}

static const char* ble_svc_name(uint16_t uuid) {
  for (int i = 0; BLE_SVC_NAMES[i].name; i++)
    if (BLE_SVC_NAMES[i].uuid == uuid) return BLE_SVC_NAMES[i].name;
  return nullptr;
}

static const char* ble_addrtype_str(BleAddrType t) {
  switch (t) {
    case BLE_ADDR_PUBLIC:              return "Public";
    case BLE_ADDR_RANDOM_STATIC:       return "Random Static";
    case BLE_ADDR_RANDOM_RESOLVABLE:   return "Resolvable";
    case BLE_ADDR_RANDOM_NON_RESOLVABLE: return "Non-resolvable";
  }
  return "Unknown";
}

static void ble_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           m[0],m[1],m[2],m[3],m[4],m[5]);
}

static String ble_rssi_hist_json(const BleDevice &d) {
  String s = "[";
  for (int i = 0; i < d.rssi_count; i++) {
    int idx = (d.rssi_head - d.rssi_count + i + BLE_HIST_LEN) % BLE_HIST_LEN;
    if (i) s += ",";
    s += String(d.rssi_hist[idx]);
  }
  return s + "]";
}

static String ble_json_all() {
  String s = "{\"count\":" + String(g_ble.count);
  s += ",\"total_adverts\":" + String(g_ble.total_adverts);
  s += ",\"scanning\":" + String(g_ble.scanning ? "true" : "false");
  s += ",\"devices\":[";
  char mac[18]; bool first = true;
  for (int i = 0; i < BLE_MAX_DEVICES; i++) {
    BleDevice &d = g_ble.devices[i];
    if (!d.active) continue;
    if (!first) s += ","; first = false;
    ble_mac_str(d.addr, mac);
    s += "{\"addr\":\"" + String(mac) + "\"";
    s += ",\"addr_type\":\"" + String(ble_addrtype_str(d.addr_type)) + "\"";
    s += ",\"name\":\"" + String(d.name) + "\"";
    s += ",\"rssi\":" + String(d.rssi);
    s += ",\"tx_power\":" + String(d.tx_power);
    s += ",\"connectable\":" + String(d.connectable ? "true" : "false");
    s += ",\"adv_count\":" + String(d.adv_count);
    s += ",\"first_ts\":" + String(d.first_ts);
    s += ",\"last_ts\":" + String(d.last_ts);
    if (d.mfr_id != 0xFFFF) {
      s += ",\"mfr_id\":" + String(d.mfr_id);
      const char *cn = ble_company_name(d.mfr_id);
      s += ",\"mfr_name\":\"" + String(cn ? cn : "") + "\"";
    }
    s += ",\"svcs\":[";
    for (int j = 0; j < d.svc_count; j++) {
      if (j) s += ",";
      const char *sn = ble_svc_name(d.svcs[j]);
      s += "{\"uuid\":" + String(d.svcs[j]);
      s += ",\"name\":\"" + String(sn ? sn : "") + "\"}";
    }
    s += "]";
    s += ",\"rssi_hist\":" + ble_rssi_hist_json(d) + "}";
  }
  s += "]}";
  return s;
}

inline void ble_routes() {
  server.on("/api/ble/all", HTTP_GET, []() {
    server.send(200, "application/json", ble_json_all());
  });
  server.on("/api/ble/clear", HTTP_POST, []() {
    memset(&g_ble, 0, sizeof(g_ble));
    server.send(200, "application/json", "{\"ok\":true}");
  });
}

#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <string.h>
#include <stdint.h>

// ── IoT Device Fingerprinting Module ──────────────────────────────────────────
// Fingerprints IoT devices using behavioral patterns:
//   - Fixed probe SSIDs (home network names)
//   - Specific IE tag ordering and vendor OUIs
//   - Traffic patterns (data frame timing, payload sizes)
//   - Known IoT OUI prefixes with model matching
// Prefix: "iot_"
// ─────────────────────────────────────────────────────────────────────────────

extern WebServer server;

#define IOT_MAX_DEVICES 64
#define IOT_MAX_LOG 128
#define IOT_MAX_PROBES 8

enum IotClass : uint8_t {
  IOT_CLASS_UNKNOWN = 0,
  IOT_CLASS_SMART_SPEAKER = 1,
  IOT_CLASS_SMART_TV = 2,
  IOT_CLASS_SMART_PLUG = 3,
  IOT_CLASS_CAMERA = 4,
  IOT_CLASS_THERMOSTAT = 5,
  IOT_CLASS_LIGHT = 6,
  IOT_CLASS_DOORBELL = 7,
  IOT_CLASS_LOCK = 8,
  IOT_CLASS_SENSOR = 9,
  IOT_CLASS_ROUTER = 10,
  IOT_CLASS_PRINTER = 11,
  IOT_CLASS_NAS = 12,
  IOT_CLASS_GAMING = 13,
  IOT_CLASS_STREAMING = 14,
  IOT_CLASS_VACUUM = 15,
  IOT_CLASS_APPLIANCE = 16,
  IOT_CLASS_MEDICAL = 17,
  IOT_CLASS_INDUSTRIAL = 18,
};

struct IotSignature {
  const char *oui_prefix;  // first 3 bytes as "XX:XX:XX"
  IotClass cls;
  const char *vendor;
  const char *model_hint;
  uint8_t confidence;  // base confidence 0-100
};

// ── IoT OUI/signature database ────────────────────────────────────────────────
static const IotSignature IOT_SIGS[] = {
    // Smart Speakers
    {"50:DC:E7", IOT_CLASS_SMART_SPEAKER, "Amazon", "Echo", 90},
    {"44:65:0D", IOT_CLASS_SMART_SPEAKER, "Amazon", "Echo", 90},
    {"FC:65:DE", IOT_CLASS_SMART_SPEAKER, "Amazon", "Echo", 90},
    {"34:D2:70", IOT_CLASS_SMART_SPEAKER, "Amazon", "Echo", 85},
    {"4C:EF:C0", IOT_CLASS_SMART_SPEAKER, "Google", "Home/Nest Audio", 90},
    {"F4:F5:D8", IOT_CLASS_SMART_SPEAKER, "Google", "Home Mini", 88},
    {"48:D6:D5", IOT_CLASS_SMART_SPEAKER, "Apple", "HomePod", 88},
    {"48:43:7C", IOT_CLASS_SMART_SPEAKER, "Sonos", "One/Beam", 90},
    {"78:28:CA", IOT_CLASS_SMART_SPEAKER, "Sonos", "Play", 88},
    {"94:9F:3E", IOT_CLASS_SMART_SPEAKER, "Sonos", "Move", 88},

    // Smart TV / Streaming
    {"8C:77:12", IOT_CLASS_SMART_TV, "Samsung", "Smart TV", 85},
    {"E8:65:49", IOT_CLASS_SMART_TV, "LG", "Smart TV", 85},
    {"00:16:EB", IOT_CLASS_STREAMING, "Apple", "Apple TV", 88},
    {"A0:99:9B", IOT_CLASS_STREAMING, "Apple", "Apple TV 4K", 90},
    {"B4:FB:E4", IOT_CLASS_STREAMING, "Google", "Chromecast", 88},
    {"6C:AD:F8", IOT_CLASS_STREAMING, "Google", "Chromecast", 88},
    {"DC:3A:5E", IOT_CLASS_STREAMING, "Roku", "Streaming Stick", 88},
    {"B8:3E:59", IOT_CLASS_STREAMING, "Roku", "Premiere", 85},
    {"CC:6D:A0", IOT_CLASS_STREAMING, "Amazon", "Fire TV", 88},
    {"84:D6:D0", IOT_CLASS_STREAMING, "Amazon", "Fire Stick", 88},

    // Cameras
    {"78:8A:20", IOT_CLASS_CAMERA, "Ring", "Doorbell", 88},
    {"B0:09:DA", IOT_CLASS_CAMERA, "Ring", "Camera", 85},
    {"2C:AA:8E", IOT_CLASS_CAMERA, "Nest", "Camera", 90},
    {"18:B4:30", IOT_CLASS_CAMERA, "Nest", "Dropcam", 88},
    {"00:62:6E", IOT_CLASS_CAMERA, "Hikvision", "IP Camera", 88},
    {"C4:2F:90", IOT_CLASS_CAMERA, "Dahua", "NVR/Camera", 85},
    {"2C:F4:32", IOT_CLASS_CAMERA, "Wyze", "Cam", 88},
    {"F8:AE:DF", IOT_CLASS_CAMERA, "Arlo", "Camera", 88},
    {"20:34:FB", IOT_CLASS_CAMERA, "Reolink", "Camera", 85},

    // Smart Home
    {"18:B4:30", IOT_CLASS_THERMOSTAT, "Nest", "Thermostat", 90},
    {"64:16:66", IOT_CLASS_THERMOSTAT, "Ecobee", "Thermostat", 88},
    {"00:17:88", IOT_CLASS_LIGHT, "Philips", "Hue Bridge", 90},
    {"EC:B5:FA", IOT_CLASS_LIGHT, "Philips", "Hue", 88},
    {"50:C7:BF", IOT_CLASS_LIGHT, "TP-Link", "Kasa Bulb", 85},
    {"60:01:94", IOT_CLASS_LIGHT, "LIFX", "Bulb", 88},
    {"AC:CF:23", IOT_CLASS_SMART_PLUG, "TP-Link", "Kasa Plug", 88},
    {"50:C7:BF", IOT_CLASS_SMART_PLUG, "TP-Link", "Smart Plug", 85},
    {"44:67:55", IOT_CLASS_SMART_PLUG, "Belkin", "WeMo Plug", 88},
    {"EC:1A:59", IOT_CLASS_SMART_PLUG, "Belkin", "WeMo", 85},

    // Doorbell / Lock
    {"2C:36:F8", IOT_CLASS_DOORBELL, "Ring", "Video Doorbell", 90},
    {"60:38:E0", IOT_CLASS_LOCK, "August", "Smart Lock", 88},
    {"DC:EF:09", IOT_CLASS_LOCK, "Schlage", "Encode Lock", 85},

    // Routers / APs
    {"A0:40:A0", IOT_CLASS_ROUTER, "Ubiquiti", "UniFi AP", 90},
    {"78:8A:20", IOT_CLASS_ROUTER, "Ubiquiti", "EdgeRouter", 88},
    {"24:A4:3C", IOT_CLASS_ROUTER, "Ubiquiti", "AmpliFi", 88},
    {"00:0C:E7", IOT_CLASS_ROUTER, "Ubiquiti", "Bullet M", 85},
    {"DC:9F:DB", IOT_CLASS_ROUTER, "Netgear", "Orbi", 88},
    {"E0:91:F5", IOT_CLASS_ROUTER, "eero", "eero Pro", 88},
    {"50:C4:DD", IOT_CLASS_ROUTER, "Amazon", "eero", 85},

    // Printers
    {"00:21:5A", IOT_CLASS_PRINTER, "HP", "LaserJet", 88},
    {"E8:9C:25", IOT_CLASS_PRINTER, "HP", "OfficeJet", 85},
    {"80:92:9F", IOT_CLASS_PRINTER, "Canon", "PIXMA", 88},
    {"00:1E:8F", IOT_CLASS_PRINTER, "Epson", "EcoTank", 85},
    {"00:00:48", IOT_CLASS_PRINTER, "Epson", "WorkForce", 82},

    // NAS
    {"00:11:32", IOT_CLASS_NAS, "Synology", "DiskStation", 90},
    {"00:08:9B", IOT_CLASS_NAS, "QNAP", "NAS", 88},
    {"00:50:43", IOT_CLASS_NAS, "Western Digital", "My Cloud", 85},

    // Gaming
    {"0C:FE:45", IOT_CLASS_GAMING, "Sony", "PlayStation 5", 90},
    {"BC:60:A7", IOT_CLASS_GAMING, "Sony", "PS5", 88},
    {"E4:95:6E", IOT_CLASS_GAMING, "Sony", "PS4", 88},
    {"58:49:BA", IOT_CLASS_GAMING, "Microsoft", "Xbox", 90},
    {"98:5F:D3", IOT_CLASS_GAMING, "Nintendo", "Switch", 88},
    {"E8:4E:84", IOT_CLASS_GAMING, "Nintendo", "Switch", 85},

    // Vacuums
    {"78:88:6D", IOT_CLASS_VACUUM, "Roomba", "iRobot", 88},
    {"40:ED:00", IOT_CLASS_VACUUM, "Ecovacs", "Deebot", 85},
    {"6C:72:20", IOT_CLASS_VACUUM, "Roborock", "Vacuum", 88},

    // Sensors / Environmental
    {"E0:28:6D", IOT_CLASS_SENSOR, "Eve", "Eve Sensor", 85},
    {"00:17:95", IOT_CLASS_SENSOR, "Bosch", "Environmental", 80},
    {"34:81:C4", IOT_CLASS_SENSOR, "Aqara", "Hub/Sensor", 88},

    // Industrial / Professional
    {"00:30:DB", IOT_CLASS_INDUSTRIAL, "Siemens", "SIMATIC", 88},
    {"00:0C:DB", IOT_CLASS_INDUSTRIAL, "ABB", "Industrial", 85},
    {"00:0A:9C", IOT_CLASS_INDUSTRIAL, "Rockwell", "ControlLogix", 88},
    {"00:1D:9C", IOT_CLASS_INDUSTRIAL, "Cisco", "Industrial Switch", 85},
    {"E0:3F:49", IOT_CLASS_INDUSTRIAL, "Advantech", "IoT Gateway", 82},

    // Medical
    {"00:04:4B", IOT_CLASS_MEDICAL, "Welch Allyn", "Patient Monitor", 85},
    {"00:0E:8F", IOT_CLASS_MEDICAL, "GE Healthcare", "Monitor", 82},
};
static const int IOT_SIG_COUNT = sizeof(IOT_SIGS) / sizeof(IOT_SIGS[0]);

// ── Probe SSID patterns that indicate IoT ─────────────────────────────────────
static const struct {
  const char *pattern;
  IotClass cls;
  uint8_t conf;
} IOT_PROBE_PATTERNS[] = {
    {"Amazon-", IOT_CLASS_SMART_SPEAKER, 10},
    {"GoogleHome", IOT_CLASS_SMART_SPEAKER, 10},
    {"NEST-", IOT_CLASS_THERMOSTAT, 10},
    {"Ring-", IOT_CLASS_CAMERA, 10},
    {"Hue-", IOT_CLASS_LIGHT, 10},
    {"Kasa-", IOT_CLASS_SMART_PLUG, 10},
    {"WeMo.", IOT_CLASS_SMART_PLUG, 10},
    {"Chromecast", IOT_CLASS_STREAMING, 10},
    {"Fire TV", IOT_CLASS_STREAMING, 10},
    {"HP-Print", IOT_CLASS_PRINTER, 10},
    {"DIRECT-", IOT_CLASS_PRINTER, 5},  // Wi-Fi Direct often printers
    {"Canon_", IOT_CLASS_PRINTER, 10},
    {"EPSON_", IOT_CLASS_PRINTER, 10},
    {"BRAVIA", IOT_CLASS_SMART_TV, 10},
    {"[LG]", IOT_CLASS_SMART_TV, 10},
    {"Roomba-", IOT_CLASS_VACUUM, 10},
    {"roborock-", IOT_CLASS_VACUUM, 10},
    {"UniFi", IOT_CLASS_ROUTER, 10},
};
static const int IOT_PROBE_PAT_COUNT = sizeof(IOT_PROBE_PATTERNS) / sizeof(IOT_PROBE_PATTERNS[0]);

// ── Traffic behavior heuristics ───────────────────────────────────────────────
// Devices with extremely regular beacon intervals, tiny data frames,
// or DNS-only traffic are likely IoT sensors.

struct IotDevice {
  uint8_t mac[6];
  char ssid_assoc[33];              // AP it's associated to
  char probes[IOT_MAX_PROBES][33];  // SSIDs probed for
  uint8_t probe_count;
  IotClass classification;
  uint8_t confidence;  // 0-100
  char vendor[33];
  char model[33];
  uint32_t first_seen_ms;
  uint32_t last_seen_ms;
  uint32_t frame_count;
  uint32_t data_bytes;
  uint32_t avg_frame_interval_ms;
  bool has_oui_match;
  bool has_probe_match;
  bool has_behavior_match;
  uint8_t rssi;
};

struct IotLogEntry {
  uint32_t ts_ms;
  uint8_t mac[6];
  uint8_t event;  // 0=new,1=classified,2=confidence_up,3=associated
  char detail[48];
};

struct IotState {
  IotDevice devs[IOT_MAX_DEVICES];
  uint8_t dev_count;
  IotLogEntry log[IOT_MAX_LOG];
  uint8_t log_head;
  uint16_t log_count;
  uint32_t total_frames;
  uint32_t classified_count;
};

static IotState g_iot = {};

// ── Helpers ───────────────────────────────────────────────────────────────────
static void iot_mac_str(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}
static bool iot_mac_eq(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

static void iot_log(const uint8_t *mac, uint8_t event, const char *detail) {
  IotLogEntry *e = &g_iot.log[g_iot.log_head];
  e->ts_ms = millis();
  if (mac)
    memcpy(e->mac, mac, 6);
  else
    memset(e->mac, 0, 6);
  e->event = event;
  strncpy(e->detail, detail ? detail : "", 47);
  g_iot.log_head = (g_iot.log_head + 1) % IOT_MAX_LOG;
  if (g_iot.log_count < IOT_MAX_LOG)
    g_iot.log_count++;
}

static IotDevice *iot_find(const uint8_t *mac) {
  for (uint8_t i = 0; i < g_iot.dev_count; i++)
    if (iot_mac_eq(g_iot.devs[i].mac, mac))
      return &g_iot.devs[i];
  return nullptr;
}

static IotDevice *iot_upsert(const uint8_t *mac) {
  IotDevice *d = iot_find(mac);
  if (d) {
    d->last_seen_ms = millis();
    d->frame_count++;
    return d;
  }
  if (g_iot.dev_count >= IOT_MAX_DEVICES)
    return nullptr;
  d = &g_iot.devs[g_iot.dev_count++];
  memset(d, 0, sizeof(IotDevice));
  memcpy(d->mac, mac, 6);
  d->first_seen_ms = millis();
  d->last_seen_ms = millis();
  d->frame_count = 1;
  return d;
}

// ── OUI matching ──────────────────────────────────────────────────────────────
static bool iot_match_oui(const uint8_t *mac, IotClass *cls, const char **vendor,
                          const char **model, uint8_t *conf) {
  char oui[9];
  snprintf(oui, sizeof(oui), "%02X:%02X:%02X", mac[0], mac[1], mac[2]);
  for (int i = 0; i < IOT_SIG_COUNT; i++) {
    if (strcasecmp(IOT_SIGS[i].oui_prefix, oui) == 0) {
      *cls = IOT_SIGS[i].cls;
      *vendor = IOT_SIGS[i].vendor;
      *model = IOT_SIGS[i].model_hint;
      *conf = IOT_SIGS[i].confidence;
      return true;
    }
  }
  return false;
}

// ── Probe SSID matching ───────────────────────────────────────────────────────
static bool iot_match_probe(const char *ssid, IotClass *cls, uint8_t *conf_boost) {
  if (!ssid)
    return false;
  for (int i = 0; i < IOT_PROBE_PAT_COUNT; i++) {
    if (strncasecmp(ssid, IOT_PROBE_PATTERNS[i].pattern, strlen(IOT_PROBE_PATTERNS[i].pattern)) ==
        0) {
      *cls = IOT_PROBE_PATTERNS[i].cls;
      *conf_boost = IOT_PROBE_PATTERNS[i].conf;
      return true;
    }
  }
  return false;
}

// ── Ingest probe request ──────────────────────────────────────────────────────
static void iot_ingest_probe(const uint8_t *mac, const char *ssid, int8_t rssi) {
  IotDevice *d = iot_upsert(mac);
  if (!d)
    return;
  d->rssi = rssi;
  g_iot.total_frames++;

  // Record probed SSID
  if (ssid && strlen(ssid) > 0) {
    bool dup = false;
    for (uint8_t i = 0; i < d->probe_count; i++)
      if (strcmp(d->probes[i], ssid) == 0) {
        dup = true;
        break;
      }
    if (!dup && d->probe_count < IOT_MAX_PROBES) {
      strncpy(d->probes[d->probe_count++], ssid, 32);
    }
  }

  // OUI match
  if (!d->has_oui_match) {
    IotClass cls;
    const char *vnd;
    const char *mdl;
    uint8_t conf;
    if (iot_match_oui(mac, &cls, &vnd, &mdl, &conf)) {
      d->has_oui_match = true;
      d->classification = cls;
      strncpy(d->vendor, vnd, 32);
      strncpy(d->model, mdl, 32);
      d->confidence = conf;
      iot_log(mac, 1, "classified by OUI");
      g_iot.classified_count++;
    }
  }

  // Probe SSID match
  if (ssid && strlen(ssid) > 0) {
    IotClass cls;
    uint8_t boost;
    if (iot_match_probe(ssid, &cls, &boost)) {
      d->has_probe_match = true;
      if (d->classification == IOT_CLASS_UNKNOWN)
        d->classification = cls;
      d->confidence = (uint8_t)min(100, (int)d->confidence + boost);
      iot_log(mac, 2, "confidence boosted by probe SSID");
    }
  }
}

// ── Ingest data frame ─────────────────────────────────────────────────────────
static void iot_ingest_data(const uint8_t *mac, uint16_t frame_len) {
  IotDevice *d = iot_upsert(mac);
  if (!d)
    return;
  g_iot.total_frames++;
  d->data_bytes += frame_len;

  // Very small data frames (<128 bytes) often indicate IoT sensors
  if (frame_len < 128 && frame_len > 0) {
    d->confidence = (uint8_t)min(100, (int)d->confidence + 2);
    if (d->confidence >= 60 && !d->has_behavior_match) {
      d->has_behavior_match = true;
      iot_log(mac, 2, "small-frame IoT behavior");
    }
  }
}

// ── Ingest association ────────────────────────────────────────────────────────
static void iot_ingest_assoc(const uint8_t *mac, const char *ap_ssid, int8_t rssi) {
  IotDevice *d = iot_upsert(mac);
  if (!d)
    return;
  if (ap_ssid)
    strncpy(d->ssid_assoc, ap_ssid, 32);
  d->rssi = rssi;
  iot_log(mac, 3, ap_ssid ? ap_ssid : "unknown AP");
}

// ── JSON helpers ──────────────────────────────────────────────────────────────
static const char *iot_class_name(IotClass c) {
  static const char *names[] = {
      "unknown",   "smart_speaker", "smart_tv",  "smart_plug", "camera",    "thermostat", "light",
      "doorbell",  "lock",          "sensor",    "router",     "printer",   "nas",        "gaming",
      "streaming", "vacuum",        "appliance", "medical",    "industrial"};
  return (c < 19) ? names[c] : "unknown";
}

static String iot_dev_json(const IotDevice &d) {
  char mac_str[18];
  iot_mac_str(d.mac, mac_str);
  String j = "{";
  j += "\"mac\":\"";
  j += mac_str;
  j += "\"";
  j += ",\"vendor\":\"";
  j += d.vendor;
  j += "\"";
  j += ",\"model\":\"";
  j += d.model;
  j += "\"";
  j += ",\"class\":\"";
  j += iot_class_name(d.classification);
  j += "\"";
  j += ",\"confidence\":";
  j += d.confidence;
  j += ",\"assoc_ap\":\"";
  j += d.ssid_assoc;
  j += "\"";
  j += ",\"rssi\":";
  j += d.rssi;
  j += ",\"probes\":[";
  for (uint8_t i = 0; i < d.probe_count; i++) {
    if (i)
      j += ",";
    j += "\"";
    j += d.probes[i];
    j += "\"";
  }
  j += "]";
  j += ",\"frames\":";
  j += d.frame_count;
  j += ",\"bytes\":";
  j += d.data_bytes;
  j += ",\"oui_match\":";
  j += d.has_oui_match ? 1 : 0;
  j += ",\"probe_match\":";
  j += d.has_probe_match ? 1 : 0;
  j += ",\"behavior_match\":";
  j += d.has_behavior_match ? 1 : 0;
  j += ",\"age_ms\":";
  j += (uint32_t)(millis() - d.first_seen_ms);
  j += "}";
  return j;
}

static String iot_summary_json() {
  uint32_t cats[19] = {};
  uint32_t conf_hi = 0;
  for (uint8_t i = 0; i < g_iot.dev_count; i++) {
    cats[g_iot.devs[i].classification]++;
    if (g_iot.devs[i].confidence >= 70)
      conf_hi++;
  }
  String j = "{";
  j += "\"total\":";
  j += g_iot.dev_count;
  j += ",\"classified\":";
  j += g_iot.classified_count;
  j += ",\"high_conf\":";
  j += conf_hi;
  j += ",\"cameras\":";
  j += cats[IOT_CLASS_CAMERA];
  j += ",\"smart_speakers\":";
  j += cats[IOT_CLASS_SMART_SPEAKER];
  j += ",\"routers\":";
  j += cats[IOT_CLASS_ROUTER];
  j += ",\"smart_home\":";
  j += (cats[IOT_CLASS_SMART_PLUG] + cats[IOT_CLASS_LIGHT] + cats[IOT_CLASS_THERMOSTAT] +
        cats[IOT_CLASS_LOCK]);
  j += ",\"streaming\":";
  j += cats[IOT_CLASS_STREAMING];
  j += ",\"industrial\":";
  j += cats[IOT_CLASS_INDUSTRIAL];
  j += ",\"medical\":";
  j += cats[IOT_CLASS_MEDICAL];
  j += ",\"total_frames\":";
  j += g_iot.total_frames;
  j += "}";
  return j;
}

static String iot_devs_json(int max_n) {
  if (max_n <= 0 || max_n > IOT_MAX_DEVICES)
    max_n = IOT_MAX_DEVICES;
  String j = "{\"count\":";
  j += g_iot.dev_count;
  j += ",\"devices\":[";
  for (uint8_t i = 0; i < g_iot.dev_count && i < (uint8_t)max_n; i++) {
    if (i)
      j += ",";
    j += iot_dev_json(g_iot.devs[i]);
  }
  j += "]}";
  return j;
}

// ── HTTP routes ───────────────────────────────────────────────────────────────
static void iot_routes() {
  server.on("/api/iot/summary", HTTP_GET,
            []() { server.send(200, "application/json", iot_summary_json()); });

  server.on("/api/iot/devices", HTTP_GET, []() {
    int n = server.hasArg("per") ? server.arg("per").toInt() : IOT_MAX_DEVICES;
    server.send(200, "application/json", iot_devs_json(n));
  });

  server.on("/api/iot/filter", HTTP_GET, []() {
    String cls_s = server.hasArg("class") ? server.arg("class") : "";
    int min_conf = server.hasArg("min_conf") ? server.arg("min_conf").toInt() : 0;
    String j = "{\"devices\":[";
    bool first = true;
    for (uint8_t i = 0; i < g_iot.dev_count; i++) {
      IotDevice &d = g_iot.devs[i];
      if (d.confidence < (uint8_t)min_conf)
        continue;
      if (cls_s.length() > 0 && cls_s != iot_class_name(d.classification))
        continue;
      if (!first)
        j += ",";
      first = false;
      j += iot_dev_json(d);
    }
    j += "]}";
    server.send(200, "application/json", j);
  });

  server.on("/api/iot/clear", HTTP_POST, []() {
    memset(&g_iot, 0, sizeof(IotState));
    server.send(200, "application/json", "{\"ok\":1}");
  });

  server.on("/api/iot/log", HTTP_GET, []() {
    int n = server.hasArg("n") ? server.arg("n").toInt() : 32;
    String j = "{\"count\":";
    j += g_iot.log_count;
    j += ",\"entries\":[";
    const char *evnames[] = {"new", "classified", "conf_up", "associated"};
    char mac_str[18];
    int start = (g_iot.log_head + IOT_MAX_LOG - g_iot.log_count) % IOT_MAX_LOG;
    bool first = true;
    for (int i = g_iot.log_count - 1; i >= 0 && i >= g_iot.log_count - n; i--) {
      if (!first)
        j += ",";
      first = false;
      int idx = (start + i) % IOT_MAX_LOG;
      IotLogEntry &e = g_iot.log[idx];
      iot_mac_str(e.mac, mac_str);
      j += "{\"ts\":";
      j += e.ts_ms;
      j += ",\"mac\":\"";
      j += mac_str;
      j += "\"";
      j += ",\"event\":\"";
      j += (e.event < 4 ? evnames[e.event] : "?");
      j += "\"";
      j += ",\"detail\":\"";
      j += e.detail;
      j += "\"";
      j += "}";
    }
    j += "]}";
    server.send(200, "application/json", j);
  });

  // High-risk IoT: cameras and industrial with high confidence
  server.on("/api/iot/risky", HTTP_GET, []() {
    String j = "{\"devices\":[";
    bool first = true;
    for (uint8_t i = 0; i < g_iot.dev_count; i++) {
      IotDevice &d = g_iot.devs[i];
      bool risky =
          (d.classification == IOT_CLASS_CAMERA || d.classification == IOT_CLASS_INDUSTRIAL ||
           d.classification == IOT_CLASS_MEDICAL) &&
          d.confidence >= 65;
      if (!risky)
        continue;
      if (!first)
        j += ",";
      first = false;
      j += iot_dev_json(d);
    }
    j += "]}";
    server.send(200, "application/json", j);
  });
}

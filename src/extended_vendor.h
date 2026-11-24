#pragma once
#include <Arduino.h>
#include <string.h>
#include <stdint.h>

// ── Extended Vendor / OUI Database ────────────────────────────────────────────
//
// Additional OUI prefix entries not covered by oui_full.h.
// Covers IoT devices, industrial equipment, automotive, medical,
// gaming, smart home, and additional networking vendors.

// ── Vendor category ───────────────────────────────────────────────────────────
enum EvCat : uint8_t {
  EV_CAT_ROUTER       = 0,
  EV_CAT_LAPTOP       = 1,
  EV_CAT_PHONE        = 2,
  EV_CAT_IOT          = 3,
  EV_CAT_TV           = 4,
  EV_CAT_PRINTER      = 5,
  EV_CAT_CAMERA       = 6,
  EV_CAT_GAMING       = 7,
  EV_CAT_INDUSTRIAL   = 8,
  EV_CAT_AUTOMOTIVE   = 9,
  EV_CAT_MEDICAL      = 10,
  EV_CAT_SMART_HOME   = 11,
  EV_CAT_ACCESS_POINT = 12,
  EV_CAT_SWITCH       = 13,
  EV_CAT_WEARABLE     = 14,
  EV_CAT_TABLET       = 15,
  EV_CAT_UNKNOWN      = 16,
};

static const char* ev_cat_name(EvCat c) {
  switch(c){
    case EV_CAT_ROUTER:       return "Router";
    case EV_CAT_LAPTOP:       return "Laptop";
    case EV_CAT_PHONE:        return "Phone";
    case EV_CAT_IOT:          return "IoT";
    case EV_CAT_TV:           return "TV/Streaming";
    case EV_CAT_PRINTER:      return "Printer";
    case EV_CAT_CAMERA:       return "Camera";
    case EV_CAT_GAMING:       return "Gaming";
    case EV_CAT_INDUSTRIAL:   return "Industrial";
    case EV_CAT_AUTOMOTIVE:   return "Automotive";
    case EV_CAT_MEDICAL:      return "Medical";
    case EV_CAT_SMART_HOME:   return "Smart Home";
    case EV_CAT_ACCESS_POINT: return "Access Point";
    case EV_CAT_SWITCH:       return "Switch";
    case EV_CAT_WEARABLE:     return "Wearable";
    case EV_CAT_TABLET:       return "Tablet";
    default:                  return "Unknown";
  }
}

// ── Vendor entry ──────────────────────────────────────────────────────────────
struct EvEntry {
  uint8_t    oui[3];
  const char* vendor;
  EvCat      category;
};

// ── Extended OUI table ────────────────────────────────────────────────────────
static const EvEntry EV_TABLE[] = {
  // Smart Home / IoT
  {{0x50,0xC7,0xBF}, "TP-Link Tapo", EV_CAT_IOT},
  {{0x1C,0x61,0xB4}, "TP-Link Tapo", EV_CAT_IOT},
  {{0x60,0x32,0xB1}, "Nest Labs", EV_CAT_SMART_HOME},
  {{0x18,0xB4,0x30}, "Nest Labs", EV_CAT_SMART_HOME},
  {{0x64,0x16,0x66}, "Nest Labs", EV_CAT_SMART_HOME},
  {{0xD0,0x73,0xD5}, "Nest Labs", EV_CAT_SMART_HOME},
  {{0xA4,0xDA,0x32}, "Belkin WeMo", EV_CAT_IOT},
  {{0x94,0x10,0x3E}, "Belkin WeMo", EV_CAT_IOT},
  {{0xEC,0x1A,0x59}, "Belkin WeMo", EV_CAT_IOT},
  {{0x38,0x2C,0x4A}, "Amazon Echo", EV_CAT_SMART_HOME},
  {{0xFC,0x65,0xDE}, "Amazon Echo", EV_CAT_SMART_HOME},
  {{0x68,0x37,0xE9}, "Amazon Fire TV", EV_CAT_TV},
  {{0xCC,0x9E,0xA2}, "Amazon Echo", EV_CAT_SMART_HOME},
  {{0xA4,0x08,0xF5}, "Amazon Echo", EV_CAT_SMART_HOME},
  {{0x40,0xB4,0xCD}, "Amazon Echo", EV_CAT_SMART_HOME},
  {{0x74,0x75,0x48}, "Amazon Echo", EV_CAT_SMART_HOME},
  {{0x34,0xD2,0x70}, "Amazon Echo Dot", EV_CAT_SMART_HOME},
  {{0xBC,0xEE,0x7B}, "Samsung SmartThings", EV_CAT_SMART_HOME},
  {{0x20,0x1F,0x3B}, "Samsung SmartThings", EV_CAT_SMART_HOME},
  {{0xCC,0x2D,0xB7}, "Philips Hue", EV_CAT_SMART_HOME},
  {{0xEC,0xB5,0xFA}, "Philips Hue", EV_CAT_SMART_HOME},
  {{0x00,0x17,0x88}, "Philips Hue", EV_CAT_SMART_HOME},
  {{0xF0,0xB4,0x29}, "Philips Hue", EV_CAT_SMART_HOME},
  {{0xD0,0x28,0xEB}, "Belkin", EV_CAT_ROUTER},
  {{0xE8,0x9F,0x80}, "Belkin", EV_CAT_ROUTER},
  // Ring / Security
  {{0xFC,0x0F,0xE6}, "Ring", EV_CAT_CAMERA},
  {{0xDC,0x0D,0x30}, "Ring", EV_CAT_CAMERA},
  {{0x44,0x65,0x0D}, "Ring", EV_CAT_CAMERA},
  {{0xAC,0x63,0xBE}, "Ring", EV_CAT_CAMERA},
  {{0x0C,0x8B,0x95}, "Ring Doorbell", EV_CAT_CAMERA},
  // Wyze
  {{0x2C,0xAA,0x8E}, "Wyze Labs", EV_CAT_CAMERA},
  {{0xD0,0x9C,0x7A}, "Wyze Labs", EV_CAT_CAMERA},
  // Ecobee
  {{0x44,0x61,0x32}, "Ecobee", EV_CAT_SMART_HOME},
  // August
  {{0xBC,0xAE,0xC5}, "August Home", EV_CAT_SMART_HOME},
  // Lutron
  {{0x00,0x17,0x20}, "Lutron Electronics", EV_CAT_SMART_HOME},
  // Leviton
  {{0xC4,0x93,0x00}, "Leviton Mfg", EV_CAT_SMART_HOME},
  // iRobot
  {{0x50,0x14,0x79}, "iRobot", EV_CAT_IOT},
  {{0x80,0x91,0x33}, "iRobot", EV_CAT_IOT},
  // Sonos
  {{0x94,0x9F,0x3E}, "Sonos", EV_CAT_SMART_HOME},
  {{0x00,0x0E,0x58}, "Sonos", EV_CAT_SMART_HOME},
  {{0x34,0x7E,0x5C}, "Sonos", EV_CAT_SMART_HOME},
  {{0x78,0x28,0xCA}, "Sonos", EV_CAT_SMART_HOME},
  {{0x48,0xA6,0xB8}, "Sonos", EV_CAT_SMART_HOME},
  // Smart TVs
  {{0x6C,0xAD,0xEF}, "LG Electronics", EV_CAT_TV},
  {{0x1C,0x0F,0xAF}, "LG Electronics", EV_CAT_TV},
  {{0x64,0x99,0x5D}, "LG Electronics", EV_CAT_TV},
  {{0xA8,0x23,0xFE}, "LG Electronics", EV_CAT_TV},
  {{0x34,0xDF,0x2A}, "Samsung Smart TV", EV_CAT_TV},
  {{0xCC,0x6E,0xA4}, "Samsung Smart TV", EV_CAT_TV},
  {{0x20,0xD3,0x90}, "Samsung Smart TV", EV_CAT_TV},
  {{0x3C,0xBD,0x3E}, "Samsung Smart TV", EV_CAT_TV},
  {{0x78,0xBD,0xBC}, "TCL/Roku TV", EV_CAT_TV},
  {{0xCC,0x40,0xD0}, "Roku", EV_CAT_TV},
  {{0xDC,0x3A,0x5E}, "Roku", EV_CAT_TV},
  {{0xB0,0xA7,0x37}, "Roku", EV_CAT_TV},
  {{0x08,0x05,0x81}, "Apple TV", EV_CAT_TV},
  {{0xA8,0xBB,0xCF}, "Apple TV", EV_CAT_TV},
  {{0x40,0x6C,0x8F}, "Apple TV", EV_CAT_TV},
  {{0xBC,0x52,0xB7}, "Chromecast", EV_CAT_TV},
  {{0xF4,0xF5,0xDB}, "Chromecast", EV_CAT_TV},
  {{0x54,0x60,0x09}, "Chromecast", EV_CAT_TV},
  {{0xCC,0xF9,0x57}, "Chromecast Ultra", EV_CAT_TV},
  {{0x6C,0x56,0x97}, "Nvidia Shield", EV_CAT_TV},
  {{0xE0,0x76,0xD0}, "Nvidia Shield", EV_CAT_TV},
  // Gaming consoles
  {{0x00,0x04,0x1F}, "Sony PlayStation", EV_CAT_GAMING},
  {{0x00,0x24,0x8D}, "Sony PlayStation", EV_CAT_GAMING},
  {{0x2C,0xCC,0x44}, "Sony PlayStation", EV_CAT_GAMING},
  {{0x70,0x9E,0x29}, "Sony PlayStation 4", EV_CAT_GAMING},
  {{0xBC,0x60,0xA7}, "Sony PlayStation 4", EV_CAT_GAMING},
  {{0x01,0x00,0x5E}, "Multicast", EV_CAT_UNKNOWN},
  {{0x00,0x50,0xF2}, "Microsoft Xbox", EV_CAT_GAMING},
  {{0x00,0x25,0xAE}, "Microsoft Xbox 360", EV_CAT_GAMING},
  {{0x60,0x45,0xCB}, "Microsoft Xbox One", EV_CAT_GAMING},
  {{0x9C,0xED,0x6F}, "Microsoft Xbox", EV_CAT_GAMING},
  {{0x00,0x09,0xBF}, "Nintendo", EV_CAT_GAMING},
  {{0x00,0x16,0x56}, "Nintendo", EV_CAT_GAMING},
  {{0x00,0x19,0xFD}, "Nintendo", EV_CAT_GAMING},
  {{0x00,0x1A,0xE9}, "Nintendo DS", EV_CAT_GAMING},
  {{0x00,0x1B,0x7A}, "Nintendo Wii U", EV_CAT_GAMING},
  {{0x00,0x1C,0xBE}, "Nintendo", EV_CAT_GAMING},
  {{0xE0,0xE7,0x51}, "Nintendo Switch", EV_CAT_GAMING},
  {{0x98,0xB6,0xE9}, "Nintendo Switch", EV_CAT_GAMING},
  {{0x7C,0xBB,0x8A}, "Nintendo Switch", EV_CAT_GAMING},
  {{0x40,0xF4,0x07}, "Valve Steam Link", EV_CAT_GAMING},
  // Printers
  {{0x00,0x00,0xAA}, "HP Printer", EV_CAT_PRINTER},
  {{0xC8,0xD9,0xD2}, "HP Printer", EV_CAT_PRINTER},
  {{0x24,0xFD,0x52}, "HP Printer", EV_CAT_PRINTER},
  {{0xA0,0xD3,0x7A}, "HP Printer", EV_CAT_PRINTER},
  {{0x38,0xEA,0xA7}, "HP Printer", EV_CAT_PRINTER},
  {{0x00,0x0D,0x9D}, "HP Printer", EV_CAT_PRINTER},
  {{0x44,0xD8,0x84}, "Canon Printer", EV_CAT_PRINTER},
  {{0xFC,0xDB,0xB3}, "Canon Printer", EV_CAT_PRINTER},
  {{0x1C,0xC1,0xDE}, "Epson Printer", EV_CAT_PRINTER},
  {{0xFC,0x08,0x4A}, "Epson Printer", EV_CAT_PRINTER},
  {{0x00,0x26,0xAB}, "Epson", EV_CAT_PRINTER},
  {{0xCC,0xFB,0x65}, "Brother Printer", EV_CAT_PRINTER},
  {{0xD4,0x68,0x4D}, "Xerox Printer", EV_CAT_PRINTER},
  {{0x00,0x00,0xAA}, "Xerox Workstation", EV_CAT_PRINTER},
  // Industrial / SCADA
  {{0x00,0x0A,0xE4}, "Siemens Industrial", EV_CAT_INDUSTRIAL},
  {{0x00,0x0A,0xC0}, "Siemens", EV_CAT_INDUSTRIAL},
  {{0x00,0x0A,0xAF}, "Siemens", EV_CAT_INDUSTRIAL},
  {{0x08,0x00,0x06}, "Siemens AG", EV_CAT_INDUSTRIAL},
  {{0x00,0x06,0xC1}, "ABB", EV_CAT_INDUSTRIAL},
  {{0x00,0x0B,0x5D}, "ABB", EV_CAT_INDUSTRIAL},
  {{0x00,0x01,0x2D}, "Advantech", EV_CAT_INDUSTRIAL},
  {{0xBC,0x96,0x80}, "Advantech", EV_CAT_INDUSTRIAL},
  {{0x00,0x00,0xBC}, "Rockwell Automation", EV_CAT_INDUSTRIAL},
  {{0x00,0x00,0xBE}, "Rockwell Automation", EV_CAT_INDUSTRIAL},
  {{0x00,0x0C,0x2A}, "Yokogawa", EV_CAT_INDUSTRIAL},
  {{0x00,0x50,0xEC}, "Beckhoff", EV_CAT_INDUSTRIAL},
  {{0xAC,0x64,0x62}, "Beckhoff Automation", EV_CAT_INDUSTRIAL},
  {{0x00,0x0A,0x35}, "Xilinx", EV_CAT_INDUSTRIAL},
  {{0x20,0x89,0x84}, "General Electric", EV_CAT_INDUSTRIAL},
  {{0x00,0x00,0x2B}, "ABB", EV_CAT_INDUSTRIAL},
  // Automotive
  {{0x28,0xED,0x6A}, "Tesla", EV_CAT_AUTOMOTIVE},
  {{0x98,0xED,0x5C}, "Tesla", EV_CAT_AUTOMOTIVE},
  {{0x34,0x05,0x9E}, "Tesla", EV_CAT_AUTOMOTIVE},
  {{0x60,0x6B,0xBD}, "BMW ConnectedDrive", EV_CAT_AUTOMOTIVE},
  {{0x00,0x03,0xFA}, "Mercedes-Benz", EV_CAT_AUTOMOTIVE},
  {{0x00,0x0B,0x0A}, "Ford SYNC", EV_CAT_AUTOMOTIVE},
  {{0x34,0x34,0xFA}, "Harman Connected Car", EV_CAT_AUTOMOTIVE},
  {{0xC8,0x3D,0xDC}, "Harman JBL", EV_CAT_AUTOMOTIVE},
  {{0x48,0xD7,0x05}, "Bosch", EV_CAT_AUTOMOTIVE},
  // Medical
  {{0x00,0x50,0xC2}, "Freescale (Medical)", EV_CAT_MEDICAL},
  {{0xCC,0x33,0xE2}, "Dräger Medical", EV_CAT_MEDICAL},
  {{0x00,0x50,0x56}, "Philips Medical", EV_CAT_MEDICAL},
  {{0x00,0x0D,0x44}, "Baxter Healthcare", EV_CAT_MEDICAL},
  {{0x00,0x00,0xDB}, "IMC Medical", EV_CAT_MEDICAL},
  // Wearables
  {{0x00,0x9E,0xC8}, "Fitbit", EV_CAT_WEARABLE},
  {{0xC0,0xEE,0x40}, "Fitbit", EV_CAT_WEARABLE},
  {{0x28,0x6C,0x07}, "Garmin", EV_CAT_WEARABLE},
  {{0xBC,0xC2,0x42}, "Garmin", EV_CAT_WEARABLE},
  {{0xCC,0xB9,0xE8}, "Garmin", EV_CAT_WEARABLE},
  {{0x40,0x49,0x0F}, "Polar Electro", EV_CAT_WEARABLE},
  {{0x00,0x24,0x7C}, "Jawbone", EV_CAT_WEARABLE},
  {{0xF8,0x8A,0x3C}, "Apple Watch", EV_CAT_WEARABLE},
  {{0xCC,0x20,0xE8}, "Apple Watch", EV_CAT_WEARABLE},
  {{0x38,0x89,0x2C}, "Samsung Galaxy Watch", EV_CAT_WEARABLE},
  // Tablets
  {{0x38,0x0A,0x94}, "Amazon Kindle", EV_CAT_TABLET},
  {{0xFC,0x03,0x9C}, "Amazon Kindle Fire", EV_CAT_TABLET},
  {{0x74,0xC2,0x46}, "Amazon Kindle", EV_CAT_TABLET},
  {{0x00,0xBB,0x3A}, "Amazon Kindle", EV_CAT_TABLET},
  {{0xF0,0xD5,0xBF}, "Microsoft Surface", EV_CAT_TABLET},
  {{0x60,0x45,0xCB}, "Microsoft Surface", EV_CAT_TABLET},
  {{0xDC,0x53,0x60}, "Microsoft Surface", EV_CAT_TABLET},
  {{0x20,0x68,0x9D}, "Lenovo Tab", EV_CAT_TABLET},
  // Networking — additional
  {{0x00,0x50,0x56}, "VMware", EV_CAT_ROUTER},
  {{0x00,0x0C,0x29}, "VMware", EV_CAT_ROUTER},
  {{0x00,0x05,0x69}, "VMware", EV_CAT_ROUTER},
  {{0x00,0x50,0x43}, "IOGEAR", EV_CAT_ROUTER},
  {{0x00,0x03,0x2E}, "Extreme Networks", EV_CAT_SWITCH},
  {{0x00,0xE0,0x2B}, "Extreme Networks", EV_CAT_SWITCH},
  {{0x00,0x04,0x96}, "Extreme/Avaya ERS", EV_CAT_SWITCH},
  {{0x00,0x01,0xE3}, "Siemens Hirschmann", EV_CAT_SWITCH},
  {{0xA4,0x1F,0x72}, "Palo Alto Networks", EV_CAT_ROUTER},
  {{0xEC,0x48,0x6D}, "Palo Alto Networks", EV_CAT_ROUTER},
  {{0x00,0x50,0xDA}, "SMC", EV_CAT_ROUTER},
  {{0x00,0x04,0xE2}, "SMC Networks", EV_CAT_ROUTER},
  {{0xF8,0x1E,0xDF}, "Actiontec", EV_CAT_ROUTER},
  {{0x00,0x26,0xB8}, "Actiontec", EV_CAT_ROUTER},
  {{0x68,0x7F,0x74}, "Actiontec", EV_CAT_ROUTER},
  {{0x28,0x9E,0xFC}, "Cambium Networks", EV_CAT_ACCESS_POINT},
  {{0x00,0x04,0x56}, "Cambium (Motorola)", EV_CAT_ACCESS_POINT},
  {{0x48,0x2A,0xE3}, "Siklu", EV_CAT_ACCESS_POINT},
  {{0x00,0xC0,0xCA}, "Alfa Network (Pentest)", EV_CAT_ACCESS_POINT},
  {{0x00,0x36,0x76}, "Hak5", EV_CAT_ACCESS_POINT},
  {{0x00,0x13,0x37}, "Hak5 (Custom)", EV_CAT_ACCESS_POINT},
  {{0xE8,0x48,0xB8}, "NETGEAR", EV_CAT_ROUTER},
  {{0x28,0xC6,0x8E}, "NETGEAR", EV_CAT_ROUTER},
  {{0x9C,0xD3,0x6D}, "NETGEAR", EV_CAT_ROUTER},
  {{0x84,0x1B,0x5E}, "NETGEAR", EV_CAT_ROUTER},
  {{0xA0,0x40,0xA0}, "NETGEAR", EV_CAT_ROUTER},
  {{0xCC,0x40,0xD0}, "NETGEAR", EV_CAT_ROUTER},
  {{0x00,0x0F,0xB5}, "NETGEAR", EV_CAT_ROUTER},
  {{0x00,0x14,0x6C}, "NETGEAR", EV_CAT_ROUTER},
  {{0x00,0x18,0x4D}, "NETGEAR", EV_CAT_ROUTER},
  {{0x00,0x1B,0x2F}, "NETGEAR", EV_CAT_ROUTER},
  {{0x00,0x1E,0x2A}, "NETGEAR", EV_CAT_ROUTER},
  {{0x00,0x22,0x3F}, "NETGEAR", EV_CAT_ROUTER},
  {{0x00,0x24,0xB2}, "NETGEAR", EV_CAT_ROUTER},
  {{0x00,0x26,0xF2}, "NETGEAR", EV_CAT_ROUTER},
  {{0x20,0x0C,0xC8}, "NETGEAR", EV_CAT_ROUTER},
  {{0x2C,0xB0,0x5D}, "NETGEAR", EV_CAT_ROUTER},
  {{0x30,0x46,0x9A}, "NETGEAR", EV_CAT_ROUTER},
  {{0x44,0x94,0xFC}, "NETGEAR", EV_CAT_ROUTER},
  {{0x6C,0xB0,0xCE}, "NETGEAR", EV_CAT_ROUTER},
  {{0x74,0x44,0x01}, "NETGEAR", EV_CAT_ROUTER},
  {{0x9C,0xD3,0x6D}, "NETGEAR", EV_CAT_ROUTER},
  // Mikrotik
  {{0xD4,0xCA,0x6D}, "MikroTik", EV_CAT_ROUTER},
  {{0x48,0x8F,0x5A}, "MikroTik", EV_CAT_ROUTER},
  {{0xE4,0x8D,0x8C}, "MikroTik", EV_CAT_ROUTER},
  {{0xCC,0x2D,0xE0}, "MikroTik", EV_CAT_ROUTER},
  {{0x00,0x00,0x5E}, "IANA (VRRP)", EV_CAT_ROUTER},
  {{0x00,0x0C,0xE7}, "MikroTik", EV_CAT_ROUTER},
  {{0xB8,0x69,0xF4}, "MikroTik", EV_CAT_ROUTER},
  {{0xDC,0x2C,0x6E}, "MikroTik", EV_CAT_ROUTER},
  {{0x6C,0x3B,0x6B}, "MikroTik", EV_CAT_ROUTER},
  // pfSense/OPNsense
  {{0x00,0x50,0x56}, "VMware/pfSense", EV_CAT_ROUTER},
  // Cellular modems
  {{0x34,0x64,0xA9}, "Sierra Wireless", EV_CAT_ROUTER},
  {{0x00,0xA0,0xD5}, "Sierra Wireless", EV_CAT_ROUTER},
  {{0xFC,0xDB,0xB3}, "Huawei Modem", EV_CAT_ROUTER},
  {{0x00,0x24,0x9B}, "ZTE", EV_CAT_ROUTER},
  {{0x28,0x2C,0x02}, "ZTE", EV_CAT_ROUTER},
  {{0x00,0x24,0xA5}, "ZTE", EV_CAT_ROUTER},
  {{0x00,0x0D,0x60}, "ZTE", EV_CAT_ROUTER},
  // End marker
  {{0,0,0}, nullptr, EV_CAT_UNKNOWN},
};

static const int EV_TABLE_SIZE = (sizeof(EV_TABLE)/sizeof(EV_TABLE[0]))-1;

// ── OUI lookup in extended table ──────────────────────────────────────────────
static const EvEntry* ev_lookup(const uint8_t mac[6]) {
  for(int i=0;i<EV_TABLE_SIZE;i++){
    if(EV_TABLE[i].oui[0]==mac[0]&&
       EV_TABLE[i].oui[1]==mac[1]&&
       EV_TABLE[i].oui[2]==mac[2]){
      return &EV_TABLE[i];
    }
  }
  return nullptr;
}

// ── Vendor string ──────────────────────────────────────────────────────────────
static const char* ev_vendor(const uint8_t mac[6]) {
  const EvEntry* e=ev_lookup(mac);
  return e?e->vendor:nullptr;
}

static EvCat ev_category(const uint8_t mac[6]) {
  const EvEntry* e=ev_lookup(mac);
  return e?e->category:EV_CAT_UNKNOWN;
}

// ── MAC JSON ──────────────────────────────────────────────────────────────────
static String ev_mac_json(const uint8_t mac[6]) {
  const EvEntry* e=ev_lookup(mac);
  char mac_str[18];
  snprintf(mac_str,sizeof(mac_str),"%02X:%02X:%02X:%02X:%02X:%02X",
    mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
  String j="{\"mac\":\""; j+=mac_str; j+="\"";
  if(e){
    j+=",\"vendor\":\""; j+=e->vendor; j+="\"";
    j+=",\"category\":\""; j+=ev_cat_name(e->category); j+="\"";
    j+=",\"source\":\"extended\"";
  } else {
    j+=",\"vendor\":null,\"source\":\"extended\"";
  }
  j+=",\"randomized\":"; j+=((mac[0]&0x02)?1:0);
  j+=",\"multicast\":"; j+=((mac[0]&0x01)?1:0);
  j+="}";
  return j;
}

// ── HTTP routes ───────────────────────────────────────────────────────────────
static void ev_routes() {
  extern WebServer server;

  server.on("/api/ev/lookup",HTTP_GET,[](){
    if(!server.hasArg("mac")){server.send(400,"application/json","{\"error\":\"mac required\"}");return;}
    uint8_t mac[6];
    unsigned b[6];
    if(sscanf(server.arg("mac").c_str(),"%x:%x:%x:%x:%x:%x",&b[0],&b[1],&b[2],&b[3],&b[4],&b[5])!=6){
      server.send(400,"application/json","{\"error\":\"invalid mac\"}");return;
    }
    for(int i=0;i<6;i++) mac[i]=(uint8_t)b[i];
    server.send(200,"application/json",ev_mac_json(mac));
  });

  server.on("/api/ev/list",HTTP_GET,[](){
    int page=server.hasArg("page")?server.arg("page").toInt():0;
    int per =server.hasArg("per") ?server.arg("per").toInt() :50;
    if(per<1||per>200) per=50;
    int start=page*per;
    int end=start+per;
    if(end>EV_TABLE_SIZE) end=EV_TABLE_SIZE;
    String j="{\"total\":"+String(EV_TABLE_SIZE);
    j+=",\"page\":"+String(page)+",\"per\":"+String(per);
    j+=",\"entries\":[";
    for(int i=start;i<end;i++){
      if(i>start) j+=",";
      char oui[9];
      snprintf(oui,sizeof(oui),"%02X:%02X:%02X",
        EV_TABLE[i].oui[0],EV_TABLE[i].oui[1],EV_TABLE[i].oui[2]);
      j+="{\"oui\":\""; j+=oui; j+="\"";
      j+=",\"vendor\":\""; j+=EV_TABLE[i].vendor?EV_TABLE[i].vendor:""; j+="\"";
      j+=",\"cat\":\""; j+=ev_cat_name(EV_TABLE[i].category); j+="\"";
      j+="}";
    }
    j+="]}";
    server.send(200,"application/json",j);
  });

  server.on("/api/ev/stats",HTTP_GET,[](){
    int cats[17]={};
    for(int i=0;i<EV_TABLE_SIZE;i++){
      int c=(int)EV_TABLE[i].category;
      if(c>=0&&c<17) cats[c]++;
    }
    String j="{\"total\":"+String(EV_TABLE_SIZE)+",\"by_cat\":{";
    for(int i=0;i<17;i++){
      if(i) j+=",";
      j+="\""; j+=ev_cat_name((EvCat)i); j+="\":"; j+=cats[i];
    }
    j+="}}";
    server.send(200,"application/json",j);
  });
}

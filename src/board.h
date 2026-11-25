#pragma once
#include <Arduino.h>
#include <esp_chip_info.h>

// Board detection: returns JSON with chip/hardware info and board-specific pin defaults
static void board_routes() {
  extern WebServer server;
  server.on("/api/board", HTTP_GET, []() {
    esp_chip_info_t ci;
    esp_chip_info(&ci);

    const char *model = "unknown";
    int i2c_sda = 21, i2c_scl = 22;
    int spi_clk = 18, spi_mosi = 23, spi_cs = 5;
    int uart_rx = 16, uart_tx = 17;
    bool has_psram = false, has_bt5 = false;

#if defined(CONFIG_IDF_TARGET_ESP32S3)
    model = "ESP32-S3";
    has_bt5 = true;
    // S3 XIAO default: SDA=4, SCL=5; DevKit: 8/9; we use standard
    i2c_sda = 8;
    i2c_scl = 9;
    spi_clk = 12;
    spi_mosi = 11;
    spi_cs = 10;
    uart_rx = 44;
    uart_tx = 43;
    has_psram = (ci.features & CHIP_FEATURE_EMB_PSRAM) != 0;
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
    model = "ESP32-S2";
    i2c_sda = 8; i2c_scl = 9;
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
    model = "ESP32-C3";
    has_bt5 = true;
    i2c_sda = 8; i2c_scl = 9;
    spi_clk = 4; spi_mosi = 5; spi_cs = 7;
    uart_rx = 20; uart_tx = 21;
#elif defined(BOARD_HAS_PSRAM)
    model = "ESP32-WROVER";
    has_psram = true;
#else
    model = "ESP32";
#endif

    // Detect known boards by compile-time board macro
    const char *board = model;
#ifdef ARDUINO_M5STACK_CORE_ESP32
    board = "M5Stack Basic";
    i2c_sda = 21;
    i2c_scl = 22;
#elif defined(ARDUINO_M5STACK_Core2)
    board = "M5Stack Core2";
    i2c_sda = 21; i2c_scl = 22;
#elif defined(ARDUINO_M5STACK_CORES3)
    board = "M5Stack CoreS3";
    i2c_sda = 12; i2c_scl = 11;
#elif defined(ARDUINO_TTGO_T1)
    board = "TTGO T-Display";
    i2c_sda = 21; i2c_scl = 22;
#elif defined(ARDUINO_HELTEC_WIFI_KIT_32)
    board = "Heltec WiFi Kit 32";
    i2c_sda = 4; i2c_scl = 15;
#elif defined(ARDUINO_NODEMCU_32S)
    board = "NodeMCU-32S";
#elif defined(ARDUINO_SEEED_XIAO_ESP32S3)
    board = "Seeed XIAO ESP32-S3";
    i2c_sda = 5; i2c_scl = 6;
    spi_clk = 7; spi_mosi = 9; spi_cs = 44;
    uart_rx = 44; uart_tx = 43;
#elif defined(ARDUINO_SEEED_XIAO_ESP32C3)
    board = "Seeed XIAO ESP32-C3";
    i2c_sda = 6; i2c_scl = 7;
    uart_rx = 20; uart_tx = 21;
#endif

    String j = "{";
    j += "\"model\":\"" + String(model) + "\"";
    j += ",\"board\":\"" + String(board) + "\"";
    j += ",\"revision\":" + String(ci.revision);
    j += ",\"cores\":" + String(ci.cores);
    j += ",\"flash_mb\":" + String(ESP.getFlashChipSize() / 1024 / 1024);
    j += ",\"psram_kb\":" + String(ESP.getPsramSize() / 1024);
    j += ",\"heap_kb\":" + String(ESP.getFreeHeap() / 1024);
    j += ",\"bt5\":" + String(has_bt5 ? 1 : 0);
    j += ",\"pins\":{";
    j += "\"i2c_sda\":" + String(i2c_sda);
    j += ",\"i2c_scl\":" + String(i2c_scl);
    j += ",\"spi_clk\":" + String(spi_clk);
    j += ",\"spi_mosi\":" + String(spi_mosi);
    j += ",\"spi_cs\":" + String(spi_cs);
    j += ",\"uart_rx\":" + String(uart_rx);
    j += ",\"uart_tx\":" + String(uart_tx);
    j += "}}";
    server.send(200, "application/json", j);
  });
}

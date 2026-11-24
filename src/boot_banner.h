#pragma once
// boot_banner.h — ASCII art boot banner and system info printer. Prefix: bb_

#include <Arduino.h>

static const char BB_BANNER[] PROGMEM =
  "\r\n"
  "  ____  _   _    _    _   _ _____ ___  __  __ _____ ____\r\n"
  " |  _ \\| | | |  / \\  | \\ | |_   _/ _ \\|  \\/  |___  |___ \\\r\n"
  " | |_) | |_| | / _ \\ |  \\| | | || | | | |\\/| |  / /  __) |\r\n"
  " |  __/|  _  |/ ___ \\| |\\  | | || |_| | |  | | / /_ / __/\r\n"
  " |_|   |_| |_/_/   \\_\\_| \\_| |_| \\___/|_|  |_|/___|_____|\r\n"
  "\r\n"
  "  ESP32 WiFi Security Toolkit — Phantom32 Firmware v1.0\r\n"
  "  Build: " __DATE__ " " __TIME__ "\r\n"
  "\r\n";

static const char BB_DIVIDER[] PROGMEM =
  "  ─────────────────────────────────────────────────────────\r\n";

void bb_print() {
  Serial.print(FPSTR(BB_BANNER));
  Serial.print(FPSTR(BB_DIVIDER));
  Serial.printf("  SDK:    %s\r\n", ESP.getSdkVersion());
  Serial.printf("  CPU:    %u MHz  x2 cores\r\n", ESP.getCpuFreqMHz());
  Serial.printf("  Flash:  %u KB   Heap: %u KB free\r\n",
                ESP.getFlashChipSize()/1024, ESP.getFreeHeap()/1024);
  Serial.printf("  AP:     Phantom32  192.168.4.1\r\n");
  Serial.printf("  Web UI: http://192.168.4.1/dashboard\r\n");
  Serial.print(FPSTR(BB_DIVIDER));
  Serial.println();
}

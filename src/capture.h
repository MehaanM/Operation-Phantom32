#pragma once
#include <LittleFS.h>
#include <stdint.h>
#include <string.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static File g_pcap;
static char g_pcapName[40];
static uint32_t g_pcapBytes = 0;
static SemaphoreHandle_t g_fsLock = nullptr;
#define PCAP_MAX_BYTES 900000u

static inline void fsLockInit() {
  if (!g_fsLock)
    g_fsLock = xSemaphoreCreateMutex();
}

static inline bool pcapOpen(const char *tag) {
  fsLockInit();
  if (xSemaphoreTake(g_fsLock, pdMS_TO_TICKS(200)) != pdTRUE)
    return false;
  snprintf(g_pcapName, sizeof(g_pcapName), "/cap_%s_%lu.pcap", tag,
           (unsigned long)(millis() / 1000));
  g_pcap = LittleFS.open(g_pcapName, "w");
  bool ok = (bool)g_pcap;
  g_pcapBytes = 24;
  xSemaphoreGive(g_fsLock);
  if (!ok)
    return false;
  struct {
    uint32_t magic;
    uint16_t vmaj, vmin;
    int32_t tz;
    uint32_t sig;
    uint32_t snap;
    uint32_t link;
  } gh = {0xa1b2c3d4, 2, 4, 0, 0, 65535, 127};
  g_pcap.write((uint8_t *)&gh, sizeof(gh));
  return true;
}

static inline void pcapClose() {
  if (!g_pcap)
    return;
  if (xSemaphoreTake(g_fsLock, pdMS_TO_TICKS(200)) == pdTRUE) {
    g_pcap.close();
    xSemaphoreGive(g_fsLock);
  }
}

static inline void pcapWrite(const uint8_t *frame, uint16_t len) {
  if (!g_pcap || g_pcapBytes > PCAP_MAX_BYTES)
    return;
  if (!g_fsLock || xSemaphoreTake(g_fsLock, 0) != pdTRUE)
    return;
  uint32_t ts = (uint32_t)(esp_timer_get_time() / 1000000ULL);
  uint32_t tus = (uint32_t)(esp_timer_get_time() % 1000000ULL);
  uint32_t incl = len + 8, orig = len + 8;
  g_pcap.write((uint8_t *)&ts, 4);
  g_pcap.write((uint8_t *)&tus, 4);
  g_pcap.write((uint8_t *)&incl, 4);
  g_pcap.write((uint8_t *)&orig, 4);
  static const uint8_t rt[8] = {0, 0, 8, 0, 0, 0, 0, 0};
  g_pcap.write(rt, 8);
  g_pcap.write(frame, len);
  g_pcapBytes += len + 24;
  xSemaphoreGive(g_fsLock);
}

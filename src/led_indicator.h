#pragma once
// led_indicator.h — GPIO LED status indicator for Phantom32.
// Drives an optional LED on GPIO2 (built-in LED on most ESP32 boards) with
// configurable blink patterns to visually indicate firmware state: idle,
// scanning, detecting, AP-mode active, low heap warning, and error.
// Prefix: ledi_

#include <Arduino.h>

#define LEDI_PIN       2      // GPIO2 — built-in LED on ESP32 DevKit v1
#define LEDI_ACTIVE_LOW 1     // Built-in LED is active-low on most boards

enum LediMode : uint8_t {
  LEDI_OFF       = 0,  // LED off
  LEDI_ON        = 1,  // LED steady on
  LEDI_HEARTBEAT = 2,  // slow 1Hz double-blink (idle/AP mode)
  LEDI_SCAN      = 3,  // fast single blink at 4Hz (scanning)
  LEDI_DETECT    = 4,  // rapid triple-blink burst (detection event)
  LEDI_WARN      = 5,  // double-blink at 2Hz (warning: low heap, etc.)
  LEDI_ERROR     = 6,  // SOS pattern (error/fatal)
};

static struct {
  LediMode  mode;
  uint32_t  next_ms;
  uint8_t   phase;
  bool      initialized;
} g_ledi = { LEDI_HEARTBEAT, 0, 0, false };

static inline void ledi_write(bool on) {
#if LEDI_ACTIVE_LOW
  digitalWrite(LEDI_PIN, on ? LOW : HIGH);
#else
  digitalWrite(LEDI_PIN, on ? HIGH : LOW);
#endif
}

static inline void ledi_init() {
  pinMode(LEDI_PIN, OUTPUT);
  ledi_write(false);
  g_ledi.initialized = true;
  g_ledi.next_ms     = millis();
  g_ledi.phase       = 0;
}

static inline void ledi_set(LediMode mode) {
  if (g_ledi.mode == mode) return;
  g_ledi.mode  = mode;
  g_ledi.phase = 0;
  g_ledi.next_ms = millis();
}

static inline void ledi_tick() {
  if (!g_ledi.initialized) return;
  uint32_t now = millis();
  if (now < g_ledi.next_ms) return;

  switch (g_ledi.mode) {
    case LEDI_OFF:
      ledi_write(false);
      g_ledi.next_ms = now + 1000;
      break;
    case LEDI_ON:
      ledi_write(true);
      g_ledi.next_ms = now + 1000;
      break;
    case LEDI_HEARTBEAT:
      // Double-blink: on 50ms, off 150ms, on 50ms, off 750ms
      switch (g_ledi.phase % 4) {
        case 0: ledi_write(true);  g_ledi.next_ms = now + 50;  break;
        case 1: ledi_write(false); g_ledi.next_ms = now + 150; break;
        case 2: ledi_write(true);  g_ledi.next_ms = now + 50;  break;
        case 3: ledi_write(false); g_ledi.next_ms = now + 750; break;
      }
      g_ledi.phase++;
      break;
    case LEDI_SCAN:
      // 4Hz single blink: on 125ms, off 125ms
      ledi_write(g_ledi.phase % 2 == 0);
      g_ledi.next_ms = now + 125;
      g_ledi.phase++;
      break;
    case LEDI_DETECT:
      // Triple burst: 3x (on 60ms, off 60ms) then off 400ms
      if (g_ledi.phase < 6) {
        ledi_write(g_ledi.phase % 2 == 0);
        g_ledi.next_ms = now + 60;
      } else {
        ledi_write(false);
        g_ledi.next_ms = now + 400;
        if (g_ledi.phase >= 7) { g_ledi.phase = 0; return; }
      }
      g_ledi.phase++;
      break;
    case LEDI_WARN:
      // 2Hz double-blink: on 100ms, off 100ms, on 100ms, off 300ms
      switch (g_ledi.phase % 4) {
        case 0: ledi_write(true);  g_ledi.next_ms = now + 100; break;
        case 1: ledi_write(false); g_ledi.next_ms = now + 100; break;
        case 2: ledi_write(true);  g_ledi.next_ms = now + 100; break;
        case 3: ledi_write(false); g_ledi.next_ms = now + 300; break;
      }
      g_ledi.phase++;
      break;
    case LEDI_ERROR:
      // SOS: ... --- ...  (3 short, 3 long, 3 short)
      { static const uint16_t SOS[] = {
          60,60,60,60,60,60+120,          // S: ...
          180,60,180,60,180,60+120,        // O: ---
          60,60,60,60,60,60+500           // S: ...
        };
        static const bool SOS_ON[] = {
          1,0,1,0,1,0, 1,0,1,0,1,0, 1,0,1,0,1,0
        };
        const uint8_t SOS_LEN = 18;
        uint8_t p = g_ledi.phase % SOS_LEN;
        ledi_write(SOS_ON[p]);
        g_ledi.next_ms = now + SOS[p];
        g_ledi.phase++;
      }
      break;
  }
}

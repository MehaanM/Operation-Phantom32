#include <stdint.h>

extern "C" int ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3) {
  (void)arg; (void)arg2; (void)arg3;
  return 0;   // 0 = allow the frame through
}

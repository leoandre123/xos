#include "time.h"
#include "io/rtc.h"
#include "io/timer.h"

static ulong g_boot_timestamp;

void time_init(void) {
  g_boot_timestamp = rtc_unix_timestamp();
}

ulong time_unix(void) {
  return g_boot_timestamp + (ulong)g_timer_ticks / g_timer_frequency;
}

ulong time_unix_millis(void) {
  return g_boot_timestamp * 1000UL + (ulong)g_timer_ticks * 1000 / g_timer_frequency;
}

ulong time_sys_millis(void) {
  return (ulong)g_timer_ticks * 1000 / g_timer_frequency;
}

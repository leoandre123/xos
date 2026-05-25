#pragma once

#include "types.h"
extern int g_timer_ticks;
extern uint g_timer_frequency;
void timer_init(uint frequency);

static inline ulong timer_get_ticks() { return g_timer_ticks; }
static inline ulong timer_ms_to_ticks(ulong ms) { return ms * g_timer_frequency / 1000; }

static inline void ksleep_ms(uint ms) {
  if (g_timer_frequency == 0)
    return;
  int ticks = (int)((ulong)ms * g_timer_frequency / 1000);
  if (ticks == 0)
    ticks = 1;
  int target = g_timer_ticks + ticks;
  while (g_timer_ticks < target)
    __asm__ volatile("sti; hlt");
}
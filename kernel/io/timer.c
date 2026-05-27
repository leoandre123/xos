#include "timer.h"
#include "cpu/idt.h"
#include "io/io.h"
#include "io/xhci.h"
#include "net/networking.h"
#include "perf/perf.h"
#include "scheduler/scheduler.h"
#include "types.h"
#define PIT_COMMAND        0x43
#define PIT_CHANNEL0       0x40
#define PIT_BASE_FREQUENCY 1193182

int g_timer_ticks = 0;
uint g_timer_frequency = 0;

static void timer_handler(interrupt_frame *frame) {
  PERF_IRQ_SCOPE(scheduler_current());
  perf_add_sample(frame->rip);
  g_timer_ticks++;
  sleep_queue_wake(g_timer_ticks);
  if (g_timer_ticks % 50 == 0) {
    PERF_SCOPE("NIC POLLING");
    for (int i = 0; i < MAX_NICS; i++) {
      if (g_nics[i].nic_id) {
        g_nics[i].driver->poll(&g_nics[i]);
      }
    }
  }
  if (g_timer_ticks % 8 == 0)
    xhci_poll();
#ifdef KERNEL_PERF
  if (g_timer_ticks % g_timer_frequency == 0) {
    perf_report();
    perf_reset();
  }
#endif

  if (g_timer_ticks % 10 == 0 && g_scheduler_running)
    schedule();
}

void timer_init(uint frequency) {
  if (frequency == 0) {
    frequency = 100;
  }
  g_timer_frequency = frequency;

  ushort divisor = (ushort)(PIT_BASE_FREQUENCY / frequency);

  // Command byte:
  // 00 = channel 0
  // 11 = access mode lobyte/hibyte
  // 010 = mode 2 (rate generator) or use 011 for square wave
  // 0 = binary mode
  outb(PIT_COMMAND, 0x34);

  // Send divisor low byte then high byte
  outb(PIT_CHANNEL0, (ubyte)(divisor & 0xFF));
  outb(PIT_CHANNEL0, (ubyte)((divisor >> 8) & 0xFF));

  register_interrupt_handler(32, timer_handler);
}
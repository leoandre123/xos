#include "timer.h"
#include "cpu/idt.h"
#include "io/io.h"
#include "io/pic.h"
#include "io/serial.h"
#include "scheduler/scheduler.h"
#include "types.h"
#define PIT_COMMAND        0x43
#define PIT_CHANNEL0       0x40
#define PIT_BASE_FREQUENCY 1193182

int g_timer_ticks = 0;

void timer_handler() {
  g_timer_ticks++;
  // serial_write("-t-");
  schedule();
}

void timer_init(uint frequency) {
  if (frequency == 0) {
    frequency = 100;
  }

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
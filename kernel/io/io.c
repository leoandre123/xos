#include "io.h"

uint8_t inb(uint16_t port) {
  uint8_t value;
  asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
  return value;
}

void outb(uint16_t port, uint8_t value) {
  asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

void io_wait(void) { asm volatile("outb %%al, $0x80" : : "a"(0)); }
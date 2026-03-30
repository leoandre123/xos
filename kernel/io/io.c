#include "io.h"

ubyte inb(ushort port) {
  ubyte value;
  asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
  return value;
}

void outb(ushort port, ubyte value) {
  asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

ushort inw(ushort port) {
  ushort value;
  asm volatile("inw %1, %0" : "=a"(value) : "Nd"(port));
  return value;
}

void io_wait(void) { asm volatile("outb %%al, $0x80" : : "a"(0)); }
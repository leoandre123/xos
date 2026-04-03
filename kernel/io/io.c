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

void outl(ushort port, uint value) { asm volatile("outl %0, %1" : : "a"(value), "Nd"(port)); }
uint inl(ushort port) {
  uint value;
  asm volatile("inl %1, %0" : "=a"(value) : "Nd"(port));
  return value;
}

void io_wait(void) { asm volatile("outb %%al, $0x80" : : "a"(0)); }
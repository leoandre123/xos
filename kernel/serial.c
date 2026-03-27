#include "serial.h"
#include "stdint.h"

static inline void outb(uint16_t port, uint8_t value) {
  __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
  uint8_t ret;
  __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
  return ret;
}

static int serial_ready(void) { return inb(0x3F8 + 5) & 0x20; }

void serial_init(void) {
  outb(0x3F8 + 1, 0x00);
  outb(0x3F8 + 3, 0x80);
  outb(0x3F8 + 0, 0x03);
  outb(0x3F8 + 1, 0x00);
  outb(0x3F8 + 3, 0x03);
  outb(0x3F8 + 2, 0xC7);
  outb(0x3F8 + 4, 0x0B);
}

void serial_write_char(char c) {
  // while (!serial_ready())
  //{
  // }
  if (c == '\n')
    serial_write_char('\r');
  outb(0x3F8, (uint8_t)c);
}

void serial_write(const char *s) {
  while (*s) {
    serial_write_char(*s++);
  }
}
void serial_write_line(const char *s) {
  while (*s) {
    serial_write_char(*s++);
  }
  serial_write_char('\n');
}

void serial_write_ulong(ulong value) {
  char buf[16];
  int i = 0;

  if (value == 0) {
    serial_write_char('0');
    return;
  }

  while (value > 0) {
    buf[i++] = '0' + (value % 10);
    value /= 10;
  }

  while (i--) {
    serial_write_char(buf[i]);
  }
}
void serial_write_hex(ulong value) {
  static const char *hex = "0123456789ABCDEF";
  serial_write("0x");
  for (int i = 15; i >= 0; i--) {
    uint8_t nibble = (value >> (i * 4)) & 0xF;
    serial_write_char(hex[nibble]);
  }
}
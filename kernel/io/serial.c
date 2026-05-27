#include "serial.h"
#include "perf/perf.h"
#include "utils/formatting.h"
#include <stdarg.h>

static inline void outb(ushort port, ubyte value) {
  __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline ubyte inb(ushort port) {
  ubyte ret;
  __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
  return ret;
}

static int serial_ready(void) { return inb(0x3F8 + 5) & 0x20; }

static void serial_write_hex_value(ulong value, ulong byte_count) {
  static const char *hex = "0123456789ABCDEF";
  serial_write("0x");
  for (int i = byte_count * 2 - 1; i >= 0; i--) {
    ubyte nibble = (value >> (i * 4)) & 0xF;
    serial_write_char(hex[nibble]);
  }
}
static void serial_write_bin_value(ulong value, ulong byte_count) {
  static const char *hex = "0123456789ABCDEF";
  serial_write("0b");
  for (int i = (byte_count * 8) - 1; i >= 0; i--) {
    serial_write_char(value >> i & 1 ? '1' : '0');
    if (i && !(i % 4)) {
      serial_write_char(' ');
    }
  }
}

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
  PERF_SCOPE("serial_write_char");
  if (c == '\n')
    serial_write_char('\r');
  outb(0x3F8, (ubyte)c);
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

void serial_write_hex8(ubyte value) { serial_write_hex_value(value, 1); }
void serial_write_hex16(ushort value) { serial_write_hex_value(value, 2); }
void serial_write_hex32(uint value) { serial_write_hex_value(value, 4); }
void serial_write_hex(ulong value) { serial_write_hex_value(value, 8); }
void serial_write_bin8(ubyte value) { serial_write_bin_value(value, 1); }

static void emit_char(char c, void *_) {
  serial_write_char(c);
}

void serial_printf(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  emit_formatted_str(emit_char, 0, fmt, args);
  va_end(args);
}
#include "stdio.h"
#include "syscall.h"
#include <stdarg.h>

// Single character output buffer flushed via sys_print
static char g_buf[1024];
static int g_pos = 0;

static void buf_flush(void) {
  if (g_pos == 0)
    return;
  g_buf[g_pos] = '\0';
  sys_write_fd(1, g_buf, g_pos);
  g_pos = 0;
}

static void buf_putchar(char c) {
  g_buf[g_pos++] = c;
  if (g_pos >= 1023 || c == '\n')
    buf_flush();
}

static void buf_puts(const char *s) {
  while (*s)
    buf_putchar(*s++);
}

static void buf_putulong(unsigned long v, int base, int uppercase, int width,
                         char pad) {
  const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
  char tmp[20];
  int i = 0;
  if (v == 0)
    tmp[i++] = '0';
  while (v > 0) {
    tmp[i++] = digits[v % base];
    v /= base;
  }
  while (i < width)
    tmp[i++] = pad;
  while (i--)
    buf_putchar(tmp[i]);
}

static void buf_putlong(long v, int width, char pad) {
  if (v < 0) {
    buf_putchar('-');
    v = -v;
  }
  buf_putulong((unsigned long)v, 10, 0, width, pad);
}

void putchar(char c) {
  buf_putchar(c);
  buf_flush();
}

void puts(const char *s) {
  buf_puts(s);
  buf_putchar('\n');
  buf_flush();
}

void printf(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  while (*fmt) {
    if (*fmt != '%') {
      buf_putchar(*fmt++);
      continue;
    }
    fmt++; // skip '%'

    char pad = ' ';
    int width = 0;
    if (*fmt == '0') {
      pad = '0';
      fmt++;
    }
    while (*fmt >= '0' && *fmt <= '9') {
      width = width * 10 + (*fmt++ - '0');
    }

    switch (*fmt++) {
    case 's': {
      const char *s = va_arg(args, const char *);
      buf_puts(s ? s : "(null)");
      break;
    }
    case 'c':
      buf_putchar((char)va_arg(args, int));
      break;
    case 'd':
      buf_putlong(va_arg(args, long), width, pad);
      break;
    case 'u':
      buf_putulong(va_arg(args, unsigned long), 10, 0, width, pad);
      break;
    case 'x':
      buf_putulong(va_arg(args, unsigned long), 16, 0, width, pad);
      break;
    case 'X':
      buf_putulong(va_arg(args, unsigned long), 16, 1, width, pad);
      break;
    case '%':
      buf_putchar('%');
      break;
    }
  }

  va_end(args);
  buf_flush();
}

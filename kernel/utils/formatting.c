#include "formatting.h"
#include "types.h"
#include <stdarg.h>

static void emit_str(void (*emit)(char, void *), void *ctx, const char *s) {
  while (*s)
    emit(*s++, ctx);
}

static void emit_uint(void (*emit)(char, void *), void *ctx, ulong v, int base, const char *digits, int width, char pad) {
  char buf[20];
  int i = 0;
  if (v == 0)
    buf[i++] = '0';
  while (v > 0) {
    buf[i++] = digits[v % base];
    v /= base;
  }
  while (i < width)
    buf[i++] = pad;
  while (i--)
    emit(buf[i], ctx);
}

void emit_formatted_str(void (*emit)(char, void *), void *ctx, const char *fmt, va_list args) {
  static const char *hex_lo = "0123456789abcdef";
  static const char *hex_hi = "0123456789ABCDEF";

  while (*fmt) {
    if (*fmt != '%') {
      emit(*fmt++, ctx);
      continue;
    }
    fmt++;

    char pad = ' ';
    int width = 0;
    if (*fmt == '0') {
      pad = '0';
      fmt++;
    }
    while (*fmt >= '0' && *fmt <= '9')
      width = width * 10 + (*fmt++ - '0');

    switch (*fmt++) {
    case 's': {
      const char *s = va_arg(args, const char *);
      if (!s)
        s = "(null)";
      if (width > 0) {
        int len = 0;
        const char *p = s;
        while (*p++)
          len++;
        while (len++ < width)
          emit(pad, ctx);
      }
      emit_str(emit, ctx, s);
      break;
    }
    case 'c':
      emit((char)va_arg(args, int), ctx);
      break;
    case 'd': {
      long v = va_arg(args, long);
      if (v < 0) {
        emit('-', ctx);
        v = -v;
      }
      emit_uint(emit, ctx, (ulong)v, 10, hex_lo, width, pad);
      break;
    }
    case 'u':
      emit_uint(emit, ctx, va_arg(args, ulong), 10, hex_lo, width, pad);
      break;
    case 'x':
      emit_uint(emit, ctx, va_arg(args, ulong), 16, hex_lo, width, pad);
      break;
    case 'X':
      emit_uint(emit, ctx, va_arg(args, ulong), 16, hex_hi, width, pad);
      break;
    case '%':
      emit('%', ctx);
      break;
    }
  }
}
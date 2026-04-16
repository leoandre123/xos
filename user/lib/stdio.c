#include "stdio.h"
#include "syscall.h"
#include <stdarg.h>

// ---------------------------------------------------------------------------
// Generic write context: either a growable fd-backed buffer or a fixed string
// ---------------------------------------------------------------------------

typedef struct {
  char *str;   // non-null → sprintf mode, write here
  int   pos;   // current write position
  int   limit; // max chars (including null terminator) in sprintf mode
  // fd-backed mode
  char  fb[1024];
  int   fb_pos;
} out_ctx;

static void ctx_flush(out_ctx *ctx) {
  if (ctx->fb_pos == 0)
    return;
  ctx->fb[ctx->fb_pos] = '\0';
  sys_write_fd(1, ctx->fb, ctx->fb_pos);
  ctx->fb_pos = 0;
}

static void ctx_putchar(out_ctx *ctx, char c) {
  if (ctx->str) {
    if (ctx->pos < ctx->limit - 1)
      ctx->str[ctx->pos++] = c;
  } else {
    ctx->fb[ctx->fb_pos++] = c;
    if (ctx->fb_pos >= 1023 || c == '\n')
      ctx_flush(ctx);
  }
}

static void ctx_puts(out_ctx *ctx, const char *s) {
  while (*s)
    ctx_putchar(ctx, *s++);
}

static void ctx_putulong(out_ctx *ctx, unsigned long v, int base, int uppercase,
                         int width, char pad) {
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
    ctx_putchar(ctx, tmp[i]);
}

static void ctx_putlong(out_ctx *ctx, long v, int width, char pad) {
  if (v < 0) {
    ctx_putchar(ctx, '-');
    v = -v;
  }
  ctx_putulong(ctx, (unsigned long)v, 10, 0, width, pad);
}

static void do_fmt(out_ctx *ctx, const char *fmt, va_list args) {
  while (*fmt) {
    if (*fmt != '%') {
      ctx_putchar(ctx, *fmt++);
      continue;
    }
    fmt++; // skip '%'

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
      ctx_puts(ctx, s ? s : "(null)");
      break;
    }
    case 'c':
      ctx_putchar(ctx, (char)va_arg(args, int));
      break;
    case 'd':
      ctx_putlong(ctx, va_arg(args, long), width, pad);
      break;
    case 'u':
      ctx_putulong(ctx, va_arg(args, unsigned long), 10, 0, width, pad);
      break;
    case 'x':
      ctx_putulong(ctx, va_arg(args, unsigned long), 16, 0, width, pad);
      break;
    case 'X':
      ctx_putulong(ctx, va_arg(args, unsigned long), 16, 1, width, pad);
      break;
    case '%':
      ctx_putchar(ctx, '%');
      break;
    }
  }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void putchar(char c) {
  out_ctx ctx = {0};
  ctx_putchar(&ctx, c);
  ctx_flush(&ctx);
}

void puts(const char *s) {
  out_ctx ctx = {0};
  ctx_puts(&ctx, s);
  ctx_putchar(&ctx, '\n');
  ctx_flush(&ctx);
}

void printf(const char *fmt, ...) {
  out_ctx ctx = {0};
  va_list args;
  va_start(args, fmt);
  do_fmt(&ctx, fmt, args);
  va_end(args);
  ctx_flush(&ctx);
}

int snprintf(char *buf, int size, const char *fmt, ...) {
  out_ctx ctx = {.str = buf, .limit = size};
  va_list args;
  va_start(args, fmt);
  do_fmt(&ctx, fmt, args);
  va_end(args);
  buf[ctx.pos] = '\0';
  return ctx.pos;
}

int sprintf(char *buf, const char *fmt, ...) {
  out_ctx ctx = {.str = buf, .limit = 0x7fffffff};
  va_list args;
  va_start(args, fmt);
  do_fmt(&ctx, fmt, args);
  va_end(args);
  buf[ctx.pos] = '\0';
  return ctx.pos;
}

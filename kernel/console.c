#include "console.h"
#include "font.h"
#include "serial.h"

static volatile uint32_t *fb;
static uint fb_width;
static uint fb_height;
static uint fb_pitch_pixels;

static uint32_t cursor_x;
static uint32_t cursor_y;

static uint32_t fg = 0x00ffffff;
static uint32_t bg = 0x00000000;

void console_set_fg_color(uint32_t value) { fg = value; }
void console_set_bg_color(uint32_t value) { bg = value; }

static void put_pixel(uint32_t x, uint32_t y, uint32_t color) {
  if (x >= fb_width || y >= fb_height)
    return;
  fb[y * fb_pitch_pixels + x] = color;
}

static void draw_char_at(uint32_t px, uint32_t py, char c, int factor) {
  const char idx = c - 32;
  const uint8_t *glyph = kernel_font[idx < 95 && idx >= 0 ? idx : 31];

  for (uint32_t row = 0; row < 16; row++) {
    for (uint32_t col = 0; col < 8; col++) {
      uint32_t color = (glyph[row] & (1 << (7 - col))) ? fg : bg;
      for (int dx = 0; dx < factor; dx++)
        for (int dy = 0; dy < factor; dy++)
          put_pixel(px + col * factor + dx, py + row * factor + dy, color);
    }
  }
}

void console_init(ulong fb_base, uint width, uint height, uint pitch) {
  fb = (volatile uint32_t *)fb_base;
  fb_width = width;
  fb_height = height;
  fb_pitch_pixels = pitch / 4;

  cursor_x = 0;
  cursor_y = 0;
}

void console_clear(uint32_t color) {
  bg = color;
  for (uint32_t y = 0; y < fb_height; y++) {
    for (uint32_t x = 0; x < fb_width; x++) {
      fb[y * fb_pitch_pixels + x] = color;
    }
  }
  cursor_x = 0;
  cursor_y = 0;
}

void console_putc(char c) {
  serial_write_char(c);
  if (c == '\n') {
    cursor_x = 0;
    cursor_y += 16;
    return;
  }

  draw_char_at(cursor_x, cursor_y, c, 1);
  cursor_x += 8;

  if (cursor_x + 8 > fb_width) {
    cursor_x = 0;
    cursor_y += 16;
  }
}

void console_write(const char *s) {
  while (*s) {
    console_putc(*s++);
  }
}

void console_write_line(const char *s) {
  console_write(s);
  console_putc('\n');
}

void console_write_hex64(uint64_t value) {
  static const char *hex = "0123456789ABCDEF";
  console_write("0x");
  for (int i = 15; i >= 0; i--) {
    uint8_t nibble = (value >> (i * 4)) & 0xF;
    console_putc(hex[nibble]);
  }
}

void console_write_u32(uint32_t value) {
  char buf[16];
  int i = 0;

  if (value == 0) {
    console_putc('0');
    return;
  }

  while (value > 0) {
    buf[i++] = '0' + (value % 10);
    value /= 10;
  }

  while (i--) {
    console_putc(buf[i]);
  }
}

void console_set_cursor(int x, int y) {
  cursor_x = x;
  cursor_y = y;
}

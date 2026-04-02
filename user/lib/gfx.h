#pragma once
#include "font.h"
#include "syscall.h"

typedef struct {
  unsigned int *ptr;   // mapped framebuffer base
  unsigned int width;
  unsigned int height;
  unsigned int pitch;  // bytes per scanline
} fb_info;

static inline void gfx_map(fb_info *fb) {
  syscall(SYS_MAP_FB, (ulong)fb, 0, 0);
}

static inline void gfx_pixel(fb_info *fb, unsigned int x, unsigned int y,
                              unsigned int color) {
  if (x >= fb->width || y >= fb->height)
    return;
  fb->ptr[y * (fb->pitch / 4) + x] = color;
}

static inline void gfx_fill(fb_info *fb, unsigned int color) {
  unsigned int pitch_px = fb->pitch / 4;
  for (unsigned int y = 0; y < fb->height; y++)
    for (unsigned int x = 0; x < fb->width; x++)
      fb->ptr[y * pitch_px + x] = color;
}

static inline void gfx_rect(fb_info *fb, unsigned int x, unsigned int y,
                             unsigned int w, unsigned int h,
                             unsigned int color) {
  for (unsigned int dy = 0; dy < h; dy++)
    for (unsigned int dx = 0; dx < w; dx++)
      gfx_pixel(fb, x + dx, y + dy, color);
}

static inline void gfx_putc(fb_info *fb, unsigned int px, unsigned int py,
                             char c, unsigned int fg, unsigned int bg) {
  int idx = c - FONT_FIRST_CHAR;
  if (idx < 0 || idx >= FONT_GLYPH_COUNT)
    idx = 0;
  const ubyte *glyph = g_font[idx];
  for (unsigned int row = 0; row < FONT_GLYPH_HEIGHT; row++) {
    for (unsigned int col = 0; col < FONT_GLYPH_WIDTH; col++) {
      unsigned int color = (glyph[row] & (1 << (7 - col))) ? fg : bg;
      gfx_pixel(fb, px + col, py + row, color);
    }
  }
}

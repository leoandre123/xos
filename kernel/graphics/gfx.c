#include "gfx.h"

static volatile uint *fb;
static uint fb_width;
static uint fb_height;
static uint fb_pitch_pixels;

void gfx_init(uint *fb_base, uint width, uint height, uint pitch) {
  fb = fb_base;
  fb_width = width;
  fb_height = height;
  fb_pitch_pixels = pitch / 4;
}
void gfx_clear(uint color) {
  for (uint yy = 0; yy < fb_height; yy++) {
    for (uint xx = 0; xx < fb_width; xx++) {
      gfx_pixel(xx, yy, color);
    }
  }
}

void gfx_pixel(uint x, uint y, uint color) {
  fb[y * fb_pitch_pixels + x] = color;
}

void gfx_rect(uint x,
              uint y, uint w, uint h, uint color) {
  for (uint yy = y; yy < y + h; yy++) {
    for (uint xx = x; xx < x + w; xx++) {
      gfx_pixel(xx, yy, color);
    }
  }
}
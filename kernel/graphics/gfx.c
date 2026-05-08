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

void gfx_pixel_blend(uint x, uint y, uint color) {
  volatile uint *dst_ptr = &fb[y * fb_pitch_pixels + x];

  uint dst = *dst_ptr;

  uint src_a = (color >> 24) & 0xFF;

  if (src_a == 255) {
    *dst_ptr = color;
    return;
  }

  if (src_a == 0) {
    return;
  }

  uint src_r = (color >> 16) & 0xFF;
  uint src_g = (color >> 8) & 0xFF;
  uint src_b = color & 0xFF;

  uint dst_r = (dst >> 16) & 0xFF;
  uint dst_g = (dst >> 8) & 0xFF;
  uint dst_b = dst & 0xFF;

  uint inv_a = 255 - src_a;

  uint out_r = (src_r * src_a + dst_r * inv_a) / 255;
  uint out_g = (src_g * src_a + dst_g * inv_a) / 255;
  uint out_b = (src_b * src_a + dst_b * inv_a) / 255;

  *dst_ptr = (out_r << 16) | (out_g << 8) | out_b;
}

void gfx_rect(uint x,
              uint y, uint w, uint h, uint color) {
  for (uint yy = y; yy < y + h; yy++) {
    for (uint xx = x; xx < x + w; xx++) {
      gfx_pixel(xx, yy, color);
    }
  }
}

void gfx_img(uint x, uint y, bitmap *img) {
  for (uint yy = y; yy < y + img->height; yy++) {
    for (uint xx = x; xx < x + img->width; xx++) {
      gfx_pixel_blend(xx, yy, img->data[(yy - y) * img->width + (xx - x)]);
    }
  }
}
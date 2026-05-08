#pragma once
#include "fb_info.h"
#include "font.h"
#include "image.h"
#include "rect.h"
#include "syscall.h"
#include <stdarg.h>
#include <stdio.h>

#define RGB(r, g, b) ((0xff << 24u) | (r << 16u) | (g << 8u) | (b))
#define ARGB(a, r, g, b) ((a << 24u) | (r << 16u) | (g << 8u) | (b))

// Expand the dirty rect to include (x, y, w, h).
static inline void fb_mark_dirty(fb_info *fb, uint x, uint y, uint w, uint h) {
  if (fb->dirty_region.w == 0) {
    fb->dirty_region.x = x;
    fb->dirty_region.y = y;
    fb->dirty_region.w = w;
    fb->dirty_region.h = h;
    return;
  }
  uint x2 = fb->dirty_region.x + fb->dirty_region.w;
  uint y2 = fb->dirty_region.y + fb->dirty_region.h;
  if (x < fb->dirty_region.x)
    fb->dirty_region.x = x;
  if (y < fb->dirty_region.y)
    fb->dirty_region.y = y;
  if (x + w > x2)
    x2 = x + w;
  if (y + h > y2)
    y2 = y + h;
  fb->dirty_region.w = x2 - fb->dirty_region.x;
  fb->dirty_region.h = y2 - fb->dirty_region.y;
}

static inline void fb_clear_dirty(fb_info *fb) { fb->dirty_region.w = 0; }
static inline int fb_is_dirty(fb_info *fb) { return fb->dirty_region.w > 0; }

static inline void gfx_map(fb_info *fb) {
  syscall(SYS_MAP_FB, (ulong)fb, 0, 0);
  fb->dirty_region.w = 0; // start clean
}

static inline void gfx_pixel(fb_info *fb, uint x, uint y, uint color) {
  if (x >= fb->width || y >= fb->height)
    return;
  fb->ptr[y * (fb->pitch / 4) + x] = color;
}

static inline void gfx_pixel_blend(fb_info *fb, uint x, uint y, uint color) {
  if (x >= fb->width || y >= fb->height)
    return;
  uint src_a = (color >> 24) & 0xFF;
  if (src_a == 0)
    return;
  uint pitch_px = fb->pitch / 4;
  if (src_a == 255) {
    fb->ptr[y * pitch_px + x] = color;
    return;
  }
  uint dst = fb->ptr[y * pitch_px + x];
  uint src_r = (color >> 16) & 0xFF;
  uint src_g = (color >> 8) & 0xFF;
  uint src_b = color & 0xFF;
  uint dst_r = (dst >> 16) & 0xFF;
  uint dst_g = (dst >> 8) & 0xFF;
  uint dst_b = dst & 0xFF;
  uint inv_a = 255 - src_a;
  fb->ptr[y * pitch_px + x] = ((src_r * src_a + dst_r * inv_a) / 255) << 16 |
                              ((src_g * src_a + dst_g * inv_a) / 255) << 8 |
                              ((src_b * src_a + dst_b * inv_a) / 255);
}

static inline void gfx_fill(fb_info *fb, uint color) {
  uint pitch_px = fb->pitch / 4;
  for (uint y = 0; y < fb->height; y++)
    for (uint x = 0; x < fb->width; x++)
      fb->ptr[y * pitch_px + x] = color;
  fb_mark_dirty(fb, 0, 0, fb->width, fb->height);
}

static inline void gfx_rect(fb_info *fb, uint x, uint y, uint w, uint h,
                            uint color) {
  for (uint dy = 0; dy < h; dy++)
    for (uint dx = 0; dx < w; dx++)
      gfx_pixel_blend(fb, x + dx, y + dy, color);
  fb_mark_dirty(fb, x, y, w, h);
}

static inline void gfx_putc(fb_info *fb, uint px, uint py, char c, uint fg) {
  int idx = c - FONT_FIRST_CHAR;
  if (idx < 0 || idx >= FONT_GLYPH_COUNT)
    idx = 0;
  const ubyte *glyph = g_font[idx];
  for (uint row = 0; row < FONT_GLYPH_HEIGHT; row++) {
    for (uint col = 0; col < FONT_GLYPH_WIDTH; col++) {
      if ((glyph[row] & (1 << (7 - col)))) {
        gfx_pixel(fb, px + col, py + row, fg);
      }
    }
  }
  fb_mark_dirty(fb, px, py, FONT_GLYPH_WIDTH, FONT_GLYPH_HEIGHT);
}

static inline void gfx_str(fb_info *fb, uint x, uint y, const char *s,
                           uint fg) {
  uint start_x = x;
  for (; *s; s++, x += FONT_GLYPH_WIDTH)
    gfx_putc(fb, x, y, *s, fg);
  if (x > start_x)
    fb_mark_dirty(fb, start_x, y, x - start_x, FONT_GLYPH_HEIGHT);
}

static inline void gfx_strf(fb_info *fb, uint x, uint y, uint fg,
                            const char *fmt, ...) {
  char buf[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  gfx_str(fb, x, y, buf, fg);
}

// --- Off-screen surfaces ---

// Allocate an off-screen pixel buffer. Draw into it with any gfx_* function,
// then flush it to the real framebuffer with gfx_flush.
static inline fb_info gfx_create_surface(uint w, uint h) {
  fb_info fb;
  fb.ptr = (uint *)sys_alloc((ulong)w * h * 4);
  fb.width = w;
  fb.height = h;
  fb.pitch = w * 4;
  fb.dirty_region.w = 0;
  uint total = w * h;
  for (uint i = 0; i < total; i++)
    fb.ptr[i] = 0;
  return fb;
}

// Blit only the dirty region of src onto dst at screen position (dx, dy),
// then clear src's dirty rect. No-op if src is clean.
static inline void gfx_flush(fb_info *dst, int dx, int dy, fb_info *src) {
  if (!fb_is_dirty(src))
    return;
  uint dst_pitch_px = dst->pitch / 4;
  uint src_pitch_px = src->pitch / 4;
  uint sx = src->dirty_region.x, sy = src->dirty_region.y;
  uint sw = src->dirty_region.w, sh = src->dirty_region.h;
  for (uint row = 0; row < sh; row++) {
    int d_row = dy + (int)(sy + row);
    if (d_row < 0 || (uint)d_row >= dst->height)
      continue;
    for (uint col = 0; col < sw; col++) {
      int d_col = dx + (int)(sx + col);
      if (d_col < 0 || (uint)d_col >= dst->width)
        continue;
      dst->ptr[(uint)d_row * dst_pitch_px + (uint)d_col] =
          src->ptr[(sy + row) * src_pitch_px + sx + col];
    }
  }
  fb_clear_dirty(src);
}

// Copy src surface onto dst at pixel offset (dx, dy), clipping to dst bounds.
static inline void gfx_blit(fb_info *dst, int dx, int dy, fb_info *src) {
  uint dst_pitch_px = dst->pitch / 4;
  uint src_pitch_px = src->pitch / 4;
  for (uint row = 0; row < src->height; row++) {
    int d_row = dy + (int)row;
    if (d_row < 0 || (uint)d_row >= dst->height)
      continue;
    for (uint col = 0; col < src->width; col++) {
      int d_col = dx + (int)col;
      if (d_col < 0 || (uint)d_col >= dst->width)
        continue;
      dst->ptr[(uint)d_row * dst_pitch_px + (uint)d_col] =
          src->ptr[row * src_pitch_px + col];
    }
  }
}

static inline uint gfx_isqrt(uint n) {
  if (n == 0)
    return 0;
  uint x = n, y = (x + 1) / 2;
  while (y < x) {
    x = y;
    y = (x + n / x) / 2;
  }
  return x;
}

// Copy a sub-rectangle (sx, sy, sw, sh) from src onto dst at (dx, dy).
// Clips to both src and dst bounds.
static inline void gfx_blit_region(fb_info *dst, int dx, int dy, fb_info *src,
                                   int sx, int sy, uint sw, uint sh) {
  // clip region to src surface
  if (sx < 0) {
    dx -= sx;
    sw += sx;
    sx = 0;
  }
  if (sy < 0) {
    dy -= sy;
    sh += sy;
    sy = 0;
  }
  if ((uint)sx + sw > src->width)
    sw = src->width - (uint)sx;
  if ((uint)sy + sh > src->height)
    sh = src->height - (uint)sy;

  uint dst_pitch_px = dst->pitch / 4;
  uint src_pitch_px = src->pitch / 4;
  for (uint row = 0; row < sh; row++) {
    int d_row = dy + (int)row;
    if (d_row < 0 || (uint)d_row >= dst->height)
      continue;
    for (uint col = 0; col < sw; col++) {
      int d_col = dx + (int)col;
      if (d_col < 0 || (uint)d_col >= dst->width)
        continue;
      dst->ptr[(uint)d_row * dst_pitch_px + (uint)d_col] =
          src->ptr[((uint)sy + row) * src_pitch_px + (uint)sx + col];
    }
  }
}

// Like gfx_blit_region but the rounded-rect mask applies to the full source
// surface. Pixels in src corners (radius r) are skipped, so this only has an
// effect when the subrect overlaps a corner of src.
static inline void gfx_blit_region_rounded(fb_info *dst, int dx, int dy,
                                           fb_info *src, int sx, int sy,
                                           uint sw, uint sh, int r) {
  if (sx < 0) {
    dx -= sx;
    sw += (uint)sx;
    sx = 0;
  }
  if (sy < 0) {
    dy -= sy;
    sh += (uint)sy;
    sy = 0;
  }
  if ((uint)sx + sw > src->width)
    sw = src->width - (uint)sx;
  if ((uint)sy + sh > src->height)
    sh = src->height - (uint)sy;

  uint W = src->width, H = src->height;
  uint dst_pitch_px = dst->pitch / 4;
  uint src_pitch_px = src->pitch / 4;
  uint r2 = (uint)r * r;

  for (uint row = 0; row < sh; row++) {
    uint fy = (uint)sy + row;

    // compute x bounds of the rounded mask in source-surface coordinates
    uint fx0 = 0, fx1 = W;
    if (fy < (uint)r) {
      int oy = (int)fy - r;
      uint half = gfx_isqrt(r2 - (uint)(oy * oy));
      fx0 = (uint)r - half;
      fx1 = W - (uint)r + half;
    } else if (fy >= H - (uint)r) {
      int oy = (int)fy - (int)(H - (uint)r);
      uint half = gfx_isqrt(r2 - (uint)(oy * oy));
      fx0 = (uint)r - half;
      fx1 = W - (uint)r + half;
    }

    // intersect mask bounds with subrect column range [sx, sx+sw)
    int icol0 = (int)fx0 - sx;
    int icol1 = (int)fx1 - sx;
    if (icol0 < 0)
      icol0 = 0;
    if (icol1 > (int)sw)
      icol1 = (int)sw;
    if (icol0 >= icol1)
      continue;
    uint col0 = (uint)icol0, col1 = (uint)icol1;

    int d_row = dy + (int)row;
    if (d_row < 0 || (uint)d_row >= dst->height)
      continue;

    for (uint col = col0; col < col1; col++) {
      int d_col = dx + (int)col;
      if (d_col < 0 || (uint)d_col >= dst->width)
        continue;
      dst->ptr[(uint)d_row * dst_pitch_px + (uint)d_col] =
          src->ptr[fy * src_pitch_px + (uint)sx + col];
    }
  }
}

static inline void gfx_img(fb_info *fb, uint x, uint y, bitmap *img) {
  for (uint yy = y; yy < y + img->height; yy++) {
    for (uint xx = x; xx < x + img->width; xx++) {
      gfx_pixel_blend(fb, xx, yy, img->data[(yy - y) * img->width + (xx - x)]);
    }
  }
}
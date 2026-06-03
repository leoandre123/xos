#pragma once
#include "fb_info.h"
#include "font.h"
#include "image.h"
#include "math.h"
#include "memory.h"
#include "rect.h"
#include "syscall.h"
#include "types.h"
#include <stdarg.h>
#include <stdio.h>

#define RGB(r, g, b)     ((0xffu << 24u) | (r << 16u) | (g << 8u) | (b))
#define ARGB(a, r, g, b) ((a << 24u) | (r << 16u) | (g << 8u) | (b))
#define RED(col)         ((col >> 16) & 0xFF)
#define GREEN(col)       ((col >> 8) & 0xFF)
#define BLUE(col)        ((col) & 0xFF)

extern rect g_clip_rect;

static inline void gfx_set_clip(fb_info *fb, rect rc) {
  g_clip_rect.x = CLAMP(rc.x, 0, fb->width);
  g_clip_rect.y = CLAMP(rc.y, 0, fb->height);
  g_clip_rect.w = CLAMP(rc.w, 0, fb->width - rc.x);
  g_clip_rect.h = CLAMP(rc.h, 0, fb->height - rc.y);
}

static inline rect gfx_get_clip() { return g_clip_rect; }

static inline void gfx_clear_clip(fb_info *fb) {
  g_clip_rect.x = 0;
  g_clip_rect.y = 0;
  g_clip_rect.w = fb->width;
  g_clip_rect.h = fb->height;
}

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
  gfx_clear_clip(fb);
}

static inline void gfx_pixel(fb_info *fb, uint x, uint y, uint color) {
  if (x < g_clip_rect.x || x >= g_clip_rect.x + g_clip_rect.w ||
      y < g_clip_rect.y || y >= g_clip_rect.y + g_clip_rect.h)
    return;
  fb->ptr[y * (fb->pitch / 4) + x] = color;
}

static inline void gfx_pixel_blend(fb_info *fb, uint x, uint y, uint color) {
  if (x < g_clip_rect.x || x >= g_clip_rect.x + g_clip_rect.w ||
      y < g_clip_rect.y || y >= g_clip_rect.y + g_clip_rect.h)
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
  int x1 = g_clip_rect.x, y1 = g_clip_rect.y;
  int x2 = x1 + g_clip_rect.w, y2 = y1 + g_clip_rect.h;
  for (int y = y1; y < y2; y++)
    for (int x = x1; x < x2; x++)
      fb->ptr[(uint)y * pitch_px + (uint)x] = color;
  fb_mark_dirty(fb, (uint)x1, (uint)y1, g_clip_rect.w, g_clip_rect.h);
}

static inline void gfx_rect(fb_info *fb, int x, int y, uint w, uint h,
                            uint color) {
  int x1 = x > g_clip_rect.x ? x : g_clip_rect.x;
  int y1 = y > g_clip_rect.y ? y : g_clip_rect.y;
  int x2 = x + (int)w < g_clip_rect.x + g_clip_rect.w
               ? x + (int)w
               : g_clip_rect.x + g_clip_rect.w;
  int y2 = y + (int)h < g_clip_rect.y + g_clip_rect.h
               ? y + (int)h
               : g_clip_rect.y + g_clip_rect.h;
  if (x1 >= x2 || y1 >= y2)
    return;
  uint pitch_px = fb->pitch / 4;
  uint src_a = (color >> 24) & 0xFF;
  if (src_a == 255) {
    for (int dy = y1; dy < y2; dy++)
      for (int dx = x1; dx < x2; dx++)
        fb->ptr[(uint)dy * pitch_px + (uint)dx] = color;
  } else if (src_a > 0) {
    uint src_r = (color >> 16) & 0xFF;
    uint src_g = (color >> 8) & 0xFF;
    uint src_b = color & 0xFF;
    uint inv_a = 255 - src_a;
    for (int dy = y1; dy < y2; dy++)
      for (int dx = x1; dx < x2; dx++) {
        uint dst = fb->ptr[(uint)dy * pitch_px + (uint)dx];
        fb->ptr[(uint)dy * pitch_px + (uint)dx] =
            ((src_r * src_a + ((dst >> 16) & 0xFF) * inv_a) / 255) << 16 |
            ((src_g * src_a + ((dst >> 8) & 0xFF) * inv_a) / 255) << 8 |
            ((src_b * src_a + (dst & 0xFF) * inv_a) / 255);
      }
  }
  fb_mark_dirty(fb, (uint)x1, (uint)y1, (uint)(x2 - x1), (uint)(y2 - y1));
}

static inline void gfx_rect_gradient(fb_info *fb, int x, int y, uint w, uint h,
                                     uint color1, uint color2, int angle) {
  int x1 = x > g_clip_rect.x ? x : g_clip_rect.x;
  int y1 = y > g_clip_rect.y ? y : g_clip_rect.y;
  int x2 = x + (int)w < g_clip_rect.x + g_clip_rect.w
               ? x + (int)w
               : g_clip_rect.x + g_clip_rect.w;
  int y2 = y + (int)h < g_clip_rect.y + g_clip_rect.h
               ? y + (int)h
               : g_clip_rect.y + g_clip_rect.h;
  if (x1 >= x2 || y1 >= y2)
    return;
  uint pitch_px = fb->pitch / 4;

  for (int dy = y1; dy < y2; dy++) {
    for (int dx = x1; dx < x2; dx++) {

      // uint color = color1 + color2;

      float t = ((float)dx - x) / w;
      int r = (int)LERP((float)RED(color1), (float)RED(color2), t);
      int g = (int)LERP((float)GREEN(color1), (float)GREEN(color2), t);
      int b = (int)LERP((float)BLUE(color1), (float)BLUE(color2), t);

      uint color = RGB(r, g, b);
      uint src_a = (color >> 24) & 0xFF;
      uint src_r = (color >> 16) & 0xFF;
      uint src_g = (color >> 8) & 0xFF;
      uint src_b = color & 0xFF;
      uint inv_a = 255 - src_a;
      if (src_a == 255)
        fb->ptr[(uint)dy * pitch_px + (uint)dx] = color;
      else if (src_a > 0) {
        uint dst = fb->ptr[(uint)dy * pitch_px + (uint)dx];
        fb->ptr[(uint)dy * pitch_px + (uint)dx] =
            ((src_r * src_a + ((dst >> 16) & 0xFF) * inv_a) / 255) << 16 |
            ((src_g * src_a + ((dst >> 8) & 0xFF) * inv_a) / 255) << 8 |
            ((src_b * src_a + (dst & 0xFF) * inv_a) / 255);
      }
    }
  }
  fb_mark_dirty(fb, (uint)x1, (uint)y1, (uint)(x2 - x1), (uint)(y2 - y1));
}

static inline void gfx_rect_outline(fb_info *fb, uint x, uint y, uint w, uint h,
                                    uint color) {
  for (uint dx = 0; dx < w; dx++) {
    gfx_pixel_blend(fb, x + dx, y, color);
    gfx_pixel_blend(fb, x + dx, y + h - 1, color);
  }
  for (uint dy = 1; dy < h - 1; dy++) {
    gfx_pixel_blend(fb, x, y + dy, color);
    gfx_pixel_blend(fb, x + w - 1, y + dy, color);
  }
  fb_mark_dirty(fb, x, y, w, h);
}

static inline void gfx_line(fb_info *fb, int x0, int y0, int x1, int y1,
                            uint color) {
  int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
  int dy = y1 > y0 ? y1 - y0 : y0 - y1, sy = y0 < y1 ? 1 : -1;
  int err = dx - dy;
  int ox = x0, oy = y0;

  for (;;) {
    gfx_pixel(fb, (uint)x0, (uint)y0, color);
    if (x0 == x1 && y0 == y1)
      break;
    int e2 = 2 * err;
    if (e2 > -dy) {
      err -= dy;
      x0 += sx;
    }
    if (e2 < dx) {
      err += dx;
      y0 += sy;
    }
  }

  int min_x = ox < x1 ? ox : x1, min_y = oy < y1 ? oy : y1;
  if (min_x < 0)
    min_x = 0;
  if (min_y < 0)
    min_y = 0;
  fb_mark_dirty(fb, (uint)min_x, (uint)min_y, (uint)(dx + 1), (uint)(dy + 1));
}

static inline void gfx_putc(fb_info *fb, int px, int py, char c, uint fg) {
  int idx = c - FONT_FIRST_CHAR;
  if (idx < 0 || idx >= FONT_GLYPH_COUNT)
    idx = 0;
  const ubyte *glyph = g_font[idx];
  for (int row = 0; row < FONT_GLYPH_HEIGHT; row++) {
    for (int col = 0; col < FONT_GLYPH_WIDTH; col++) {
      if ((glyph[row] & (1 << (7 - col)))) {
        gfx_pixel(fb, (uint)(px + col), (uint)(py + row), fg);
      }
    }
  }
  if (px >= 0 && py >= 0)
    fb_mark_dirty(fb, (uint)px, (uint)py, FONT_GLYPH_WIDTH, FONT_GLYPH_HEIGHT);
}

static inline void gfx_str(fb_info *fb, int x, int y, const char *s, uint fg) {
  int start_x = x;
  for (; *s; s++, x += FONT_GLYPH_WIDTH)
    gfx_putc(fb, x, y, *s, fg);
  if (x > start_x && start_x >= 0 && y >= 0)
    fb_mark_dirty(fb, (uint)start_x, (uint)y, (uint)(x - start_x),
                  FONT_GLYPH_HEIGHT);
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

  if (!fb.ptr) {
    sys_write("ERROR: fb ptr is null");
    for (;;)
      __asm__("hlt");
  }
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
    int d_col = dx + (int)sx;
    if (d_col < 0 || (uint)d_col + sw > dst->width)
      continue;
    memcpy(&dst->ptr[(uint)d_row * dst_pitch_px + (uint)d_col],
           &src->ptr[(sy + row) * src_pitch_px + sx], sw * 4);
  }
  fb_clear_dirty(src);
}

// Copy src surface onto dst at pixel offset (dx, dy), clipping to dst bounds
// and clip rect.
static inline void gfx_blit(fb_info *dst, int dx, int dy, fb_info *src) {
  int src_x0 = 0, src_y0 = 0;
  int dst_x0 = dx, dst_y0 = dy;
  int dst_x1 = dx + (int)src->width, dst_y1 = dy + (int)src->height;
  int cr_x1 = g_clip_rect.x + g_clip_rect.w,
      cr_y1 = g_clip_rect.y + g_clip_rect.h;
  if (dst_x0 < g_clip_rect.x) {
    src_x0 += g_clip_rect.x - dst_x0;
    dst_x0 = g_clip_rect.x;
  }
  if (dst_y0 < g_clip_rect.y) {
    src_y0 += g_clip_rect.y - dst_y0;
    dst_y0 = g_clip_rect.y;
  }
  if (dst_x1 > cr_x1)
    dst_x1 = cr_x1;
  if (dst_y1 > cr_y1)
    dst_y1 = cr_y1;
  if (dst_x0 < 0) {
    src_x0 -= dst_x0;
    dst_x0 = 0;
  }
  if (dst_y0 < 0) {
    src_y0 -= dst_y0;
    dst_y0 = 0;
  }
  if (dst_x1 > (int)dst->width)
    dst_x1 = (int)dst->width;
  if (dst_y1 > (int)dst->height)
    dst_y1 = (int)dst->height;
  if (dst_x0 >= dst_x1 || dst_y0 >= dst_y1)
    return;
  uint dst_pitch_px = dst->pitch / 4;
  uint src_pitch_px = src->pitch / 4;
  uint w = (uint)(dst_x1 - dst_x0), h = (uint)(dst_y1 - dst_y0);
  for (uint row = 0; row < h; row++) {
    uint d_row = (uint)dst_y0 + row, s_row = (uint)src_y0 + row;
    memcpy(&dst->ptr[d_row * dst_pitch_px + (uint)dst_x0],
           &src->ptr[s_row * src_pitch_px + (uint)src_x0], w * 4);
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
  // clamp column range to clip rect and dst bounds once, outside the row loop
  int col0 = dx < g_clip_rect.x ? g_clip_rect.x - dx : 0;
  int col1 = (int)sw;
  if (dx + col1 > g_clip_rect.x + g_clip_rect.w)
    col1 = g_clip_rect.x + g_clip_rect.w - dx;
  if (dx + col1 > (int)dst->width)
    col1 = (int)dst->width - dx;
  if (col0 >= col1)
    return;
  for (uint row = 0; row < sh; row++) {
    int d_row = dy + (int)row;
    if (d_row < 0 || (uint)d_row >= dst->height || d_row < g_clip_rect.y ||
        d_row >= g_clip_rect.y + g_clip_rect.h)
      continue;
    memcpy(&dst->ptr[(uint)d_row * dst_pitch_px + (uint)(dx + col0)],
           &src->ptr[((uint)sy + row) * src_pitch_px + (uint)sx + (uint)col0],
           (uint)(col1 - col0) * 4);
  }
}

// Like gfx_blit_region but the rounded-rect mask applies to the full source
// surface. Pixels in src corners (radius r) are skipped, so this only has an
// effect when the subrect overlaps a corner of src.
static inline void gfx_blit_region_rounded(fb_info *dst, int dx, int dy,
                                           fb_info *src, int sx, int sy,
                                           uint sw, uint sh, int r);

// Like gfx_blit_region_rounded but with independent corner radii (TL, TR, BR,
// BL). The mask is relative to the full src surface, not the subrect.
static inline void gfx_blit_region_rounded4(fb_info *dst, int dx, int dy,
                                            fb_info *src, int sx, int sy,
                                            uint sw, uint sh, int r_tl,
                                            int r_tr, int r_br, int r_bl) {
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

  int W = (int)src->width, H = (int)src->height;
  uint dst_pitch_px = dst->pitch / 4;
  uint src_pitch_px = src->pitch / 4;

  for (uint row = 0; row < sh; row++) {
    int fy = sy + (int)row;

    // left edge of rounded mask in src-surface x coords
    int fx0 = 0;
    if (fy < r_tl) {
      int oy = r_tl - fy;
      int xc = r_tl - (int)gfx_isqrt((uint)(r_tl * r_tl - oy * oy));
      if (xc > fx0)
        fx0 = xc;
    }
    if (r_bl > 0 && fy >= H - r_bl) {
      int oy = fy - (H - r_bl);
      int xc = r_bl - (int)gfx_isqrt((uint)(r_bl * r_bl - oy * oy));
      if (xc > fx0)
        fx0 = xc;
    }

    // right edge of rounded mask in src-surface x coords
    int fx1 = W;
    if (fy < r_tr) {
      int oy = r_tr - fy;
      int xc = W - r_tr + (int)gfx_isqrt((uint)(r_tr * r_tr - oy * oy));
      if (xc < fx1)
        fx1 = xc;
    }
    if (r_br > 0 && fy >= H - r_br) {
      int oy = fy - (H - r_br);
      int xc = W - r_br + (int)gfx_isqrt((uint)(r_br * r_br - oy * oy));
      if (xc < fx1)
        fx1 = xc;
    }

    // intersect mask with subrect column range [sx, sx+sw)
    int icol0 = fx0 - sx;
    int icol1 = fx1 - sx;
    if (icol0 < 0)
      icol0 = 0;
    if (icol1 > (int)sw)
      icol1 = (int)sw;
    if (icol0 >= icol1)
      continue;

    int d_row = dy + (int)row;
    if (d_row < 0 || (uint)d_row >= dst->height || d_row < g_clip_rect.y ||
        d_row >= g_clip_rect.y + (int)g_clip_rect.h)
      continue;

    int d_col0 = dx + icol0;
    int d_col1 = dx + icol1;
    int clip_r = g_clip_rect.x + (int)g_clip_rect.w;
    if (d_col0 < g_clip_rect.x)
      d_col0 = g_clip_rect.x;
    if (d_col1 > clip_r)
      d_col1 = clip_r;
    if (d_col0 < 0)
      d_col0 = 0;
    if (d_col1 > (int)dst->width)
      d_col1 = (int)dst->width;
    if (d_col0 >= d_col1)
      continue;

    uint src_col = (uint)((int)sx + (d_col0 - dx));
    uint span = (uint)(d_col1 - d_col0);
    memcpy(&dst->ptr[(uint)d_row * dst_pitch_px + (uint)d_col0],
           &src->ptr[(uint)fy * src_pitch_px + src_col], span * 4);
  }
}

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
    if (d_row < 0 || (uint)d_row >= dst->height || d_row < g_clip_rect.y ||
        d_row >= g_clip_rect.y + g_clip_rect.h)
      continue;

    for (uint col = col0; col < col1; col++) {
      int d_col = dx + (int)col;
      if (d_col < 0 || (uint)d_col >= dst->width || d_col < g_clip_rect.x ||
          d_col >= g_clip_rect.x + g_clip_rect.w)
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
static inline void gfx_imgs(fb_info *fb, uint x, uint y, uint w, uint h,
                            bitmap *img) {
  for (uint yy = y; yy < y + h; yy++) {
    for (uint xx = x; xx < x + w; xx++) {
      int px = (int)(((float)(xx - x)) / w * img->width);
      int py = (int)(((float)(yy - y)) / h * img->height);
      gfx_pixel_blend(fb, xx, yy, img->data[py * img->width + px]);
    }
  }
}

// Filled rect with independent corner radii (TL, TR, BR, BL — clockwise).
// Pass 0 for a corner to leave it square.
static inline void gfx_rect_rounded(fb_info *fb, int x, int y, int w, int h,
                                    uint color, int r_tl, int r_tr, int r_br,
                                    int r_bl) {
  uint src_a = (color >> 24) & 0xFF;
  if (src_a == 0 || w <= 0 || h <= 0)
    return;
  uint src_r = (color >> 16) & 0xFF;
  uint src_g = (color >> 8) & 0xFF;
  uint src_b = color & 0xFF;
  uint inv_a = 255 - src_a;
  uint pitch_px = fb->pitch / 4;

  int fy0 = y > g_clip_rect.y ? y : g_clip_rect.y;
  int fy1 = y + h < g_clip_rect.y + (int)g_clip_rect.h
                ? y + h
                : g_clip_rect.y + (int)g_clip_rect.h;
  if (fy0 >= fy1)
    return;

  for (int fy = fy0; fy < fy1; fy++) {
    int row = fy - y;

    int xl = 0;
    if (row < r_tl) {
      int oy = r_tl - row;
      int xc = r_tl - (int)gfx_isqrt((uint)(r_tl * r_tl - oy * oy));
      if (xc > xl)
        xl = xc;
    }
    if (r_bl > 0 && row >= h - r_bl) {
      int oy = row - (h - r_bl);
      int xc = r_bl - (int)gfx_isqrt((uint)(r_bl * r_bl - oy * oy));
      if (xc > xl)
        xl = xc;
    }

    int xr = w;
    if (row < r_tr) {
      int oy = r_tr - row;
      int xc = w - r_tr + (int)gfx_isqrt((uint)(r_tr * r_tr - oy * oy));
      if (xc < xr)
        xr = xc;
    }
    if (r_br > 0 && row >= h - r_br) {
      int oy = row - (h - r_br);
      int xc = w - r_br + (int)gfx_isqrt((uint)(r_br * r_br - oy * oy));
      if (xc < xr)
        xr = xc;
    }

    int fx0 = x + xl, fx1 = x + xr;
    if (fx0 < g_clip_rect.x)
      fx0 = g_clip_rect.x;
    if (fx1 > g_clip_rect.x + (int)g_clip_rect.w)
      fx1 = g_clip_rect.x + (int)g_clip_rect.w;
    if (fx0 >= fx1)
      continue;

    if (src_a == 255) {
      for (int fx = fx0; fx < fx1; fx++)
        fb->ptr[(uint)fy * pitch_px + (uint)fx] = color;
    } else {
      for (int fx = fx0; fx < fx1; fx++) {
        uint dst = fb->ptr[(uint)fy * pitch_px + (uint)fx];
        fb->ptr[(uint)fy * pitch_px + (uint)fx] =
            ((src_r * src_a + ((dst >> 16) & 0xFF) * inv_a) / 255) << 16 |
            ((src_g * src_a + ((dst >> 8) & 0xFF) * inv_a) / 255) << 8 |
            ((src_b * src_a + (dst & 0xFF) * inv_a) / 255);
      }
    }
  }

  int dx0 = x < 0 ? 0 : x, dy0 = y < 0 ? 0 : y;
  fb_mark_dirty(fb, (uint)dx0, (uint)dy0, (uint)w, (uint)h);
}

// Blit src surface onto dst at (dx, dy) with independent corner radii (TL,
// TR, BR, BL). Clips to dst bounds and clip rect.
static inline void gfx_blit_rounded(fb_info *dst, int dx, int dy, fb_info *src,
                                    int r_tl, int r_tr, int r_br, int r_bl) {
  int w = (int)src->width, h = (int)src->height;

  int fy0 = dy > g_clip_rect.y ? dy : g_clip_rect.y;
  int fy1 = dy + h < g_clip_rect.y + (int)g_clip_rect.h
                ? dy + h
                : g_clip_rect.y + (int)g_clip_rect.h;
  if (fy0 < 0)
    fy0 = 0;
  if (fy1 > (int)dst->height)
    fy1 = (int)dst->height;
  if (fy0 >= fy1)
    return;

  uint dst_pitch_px = dst->pitch / 4;
  uint src_pitch_px = src->pitch / 4;

  for (int fy = fy0; fy < fy1; fy++) {
    int row = fy - dy;

    int xl = 0;
    if (row < r_tl) {
      int oy = r_tl - row;
      int xc = r_tl - (int)gfx_isqrt((uint)(r_tl * r_tl - oy * oy));
      if (xc > xl)
        xl = xc;
    }
    if (r_bl > 0 && row >= h - r_bl) {
      int oy = row - (h - r_bl);
      int xc = r_bl - (int)gfx_isqrt((uint)(r_bl * r_bl - oy * oy));
      if (xc > xl)
        xl = xc;
    }

    int xr = w;
    if (row < r_tr) {
      int oy = r_tr - row;
      int xc = w - r_tr + (int)gfx_isqrt((uint)(r_tr * r_tr - oy * oy));
      if (xc < xr)
        xr = xc;
    }
    if (r_br > 0 && row >= h - r_br) {
      int oy = row - (h - r_br);
      int xc = w - r_br + (int)gfx_isqrt((uint)(r_br * r_br - oy * oy));
      if (xc < xr)
        xr = xc;
    }

    int fx0 = dx + xl, fx1 = dx + xr;
    if (fx0 < g_clip_rect.x)
      fx0 = g_clip_rect.x;
    if (fx1 > g_clip_rect.x + (int)g_clip_rect.w)
      fx1 = g_clip_rect.x + (int)g_clip_rect.w;
    if (fx0 < 0)
      fx0 = 0;
    if (fx1 > (int)dst->width)
      fx1 = (int)dst->width;
    if (fx0 >= fx1)
      continue;

    memcpy(&dst->ptr[(uint)fy * dst_pitch_px + (uint)fx0],
           &src->ptr[(uint)row * src_pitch_px + (uint)(fx0 - dx)],
           (uint)(fx1 - fx0) * 4);
  }

  int ddx = dx < 0 ? 0 : dx, ddy = dy < 0 ? 0 : dy;
  fb_mark_dirty(dst, (uint)ddx, (uint)ddy, (uint)w, (uint)h);
}
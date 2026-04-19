#include "immediate_ui.h"
#include "font.h"
#include "gfx.h"
#include "types.h"
#include <string.h>

static inline uint isqrt32(uint n) {
  if (n == 0)
    return 0;
  uint x = n, y = (x + 1) / 2;
  while (y < x) {
    x = y;
    y = (x + n / x) / 2;
  }
  return x;
}

static inline bool mouse_in(ui_ctx *ctx, int x, int y, int w, int h) {
  return (ctx->mx >= x && ctx->mx < x + w && ctx->my >= y && ctx->my < y + h);
}

void ui_str(ui_ctx *ctx, int x, int y, const char *str, uint color) {
  gfx_str(&ctx->fb, x, y, str, color);
}
void ui_center_str(ui_ctx *ctx, int x, int y, int w, int h, const char *str,
                   uint color) {
  int str_len = strlen(str);
  int str_width = str_len * FONT_GLYPH_WIDTH;

  int start_x = x + (w - str_width) / 2;
  int start_y = y + (h - FONT_GLYPH_HEIGHT) / 2;

  gfx_str(&ctx->fb, start_x, start_y, str, color);
}

void ui_rect(ui_ctx *ctx, rect rc, int color) {
  gfx_rect(&ctx->fb, rc.x, rc.y, rc.w, rc.h, color);
}

void ui_rounded_rect(ui_ctx *ctx, rect rc, int r, int color) {
  uint *buf = ctx->fb.ptr;
  uint pitch_px = ctx->fb.pitch / 4;
  uint r2 = (uint)r * r;

  for (uint dy = 0; dy < rc.h; dy++) {
    uint x0 = 0, x1 = rc.w;

    if (dy < (uint)r) {
      int oy = (int)dy - r;
      uint half = isqrt32(r2 - (uint)(oy * oy));
      x0 = r - half;
      x1 = rc.w - r + half;
    } else if (dy >= rc.h - (uint)r) {
      int oy = (int)dy - ((int)rc.h - r);
      uint half = isqrt32(r2 - (uint)(oy * oy));
      x0 = r - half;
      x1 = rc.w - r + half;
    }

    uint *row = buf + (rc.y + dy) * pitch_px + rc.x;
    for (uint dx = x0; dx < x1; dx++)
      row[dx] = (uint)color;
  }

  fb_mark_dirty(&ctx->fb, rc.x, rc.y, rc.w, rc.h);
}

bool ui_button(ui_ctx *ctx, int x, int y, int w, int h, const char *label) {
  bool hovered = mouse_in(ctx, x, y, w, h);
  bool clicked = hovered && ctx->clicked;
  if (clicked) {
    ctx->clicked = false;
  }

  uint color = hovered ? 0x005599FF : 0x003366CC;
  gfx_rect(&ctx->fb, x, y, w, h, color);
  ui_center_str(ctx, x, y, w, h, label, 0x00FFFFFF);
  return clicked;
}

bool ui_checkbox(ui_ctx *ctx, int x, int y, const char *label, bool *checked) {
  bool hovered = mouse_in(ctx, x, y, 14, 14);
  bool clicked = hovered && ctx->clicked;
  if (clicked) {
    *checked = !*checked;
    ctx->clicked = false;
  }

  uint border = hovered ? 0x00AAAAAA : 0x00666666;
  uint bg = 0x00222222;
  gfx_rect(&ctx->fb, x, y, 14, 14, border);
  gfx_rect(&ctx->fb, x + 1, y + 1, 12, 12, bg);
  if (*checked)
    gfx_str(&ctx->fb, x + 2, y + 2, "X", 0x00FFFFFF);
  gfx_str(&ctx->fb, x + 18, y + 2, label, 0x00FFFFFF);

  return clicked;
}
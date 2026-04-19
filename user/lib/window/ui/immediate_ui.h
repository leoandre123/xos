#pragma once
#include "gfx.h"
#include "rect.h"
#include "types.h"

typedef struct {
  fb_info fb;
  short mx;
  short my;
  bool clicked;
  rect ui_rect;
} ui_ctx;

void ui_str(ui_ctx *ctx, int x, int y, const char *str, uint color);
void ui_center_str(ui_ctx *ctx, int x, int y, int w, int h, const char *str,
                   uint color);
void ui_rect(ui_ctx *ctx, rect rc, int color);
void ui_rounded_rect(ui_ctx *ctx, rect rc, int r, int color);

bool ui_button(ui_ctx *ctx, int x, int y, int w, int h, const char *label);
bool ui_checkbox(ui_ctx *ctx, int x, int y, const char *label, bool *checked);
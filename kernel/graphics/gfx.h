#pragma once
#include "graphics/image.h"
#include "types.h"

#define RGB(r, g, b) ((r << 16u) | (g << 8u) | (b))

void gfx_init(uint *fb_base, uint width, uint height, uint pitch);
void gfx_clear(uint color);
void gfx_pixel(uint x, uint y, uint color);
void gfx_rect(uint x, uint y, uint w, uint h, uint color);
void gfx_img(uint x, uint y, bitmap *img);
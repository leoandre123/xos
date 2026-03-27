#pragma once
#include "types.h"
void gfx_init(uint *fb_base, uint width, uint height, uint pitch);
void gfx_clear(uint color);
void gfx_pixel(uint x, uint y, uint color);
void gfx_rect(uint x, uint y, uint w, uint h, uint color);
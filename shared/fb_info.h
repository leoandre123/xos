#pragma once

#include "rect.h"
typedef struct {
  uint *ptr; // mapped framebuffer base
  uint width;
  uint height;
  uint pitch; // bytes per scanline
  rect dirty_region;
} fb_info;
#pragma once
#include "types.h"
typedef struct {
  uint width;
  uint height;
  uint data[];
} bitmap;

bitmap *img_load(const char *path);
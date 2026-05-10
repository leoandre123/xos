#pragma once
#include "cdefs.h"
#include "types.h"

EXTERN_C_BEGIN

typedef struct {
  uint width;
  uint height;
  uint data[];
} bitmap;

bitmap *img_load(const char *path);

EXTERN_C_END
#pragma once
#include "cdefs.h"

EXTERN_C_BEGIN

#define MAX(a, b) (b > a ? b : a)
#define MIN(a, b) (b < a ? b : a)
#define CLAMP(value, min, max) (value < min ? min : value > max ? max : value)

#define ALIGN_UP(val, aligntment)                                              \
  (((val) + (aligntment) - 1) / (aligntment) * (aligntment))

#define LERP(a, b, t) ((a) + (t) * ((b) - (a)))
EXTERN_C_END
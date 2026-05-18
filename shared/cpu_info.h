#pragma once
#include "types.h"

typedef struct {
  char brand[49];
  uint base_mhz;
  uint max_mhz;
  int logical_cores;
} cpu_info;

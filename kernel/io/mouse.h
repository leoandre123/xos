#pragma once
#include "types.h"

typedef struct {
  int x, y;
  int buttons; // bit0=left, bit1=right, bit2=middle
  int pending;
} mouse_state;

void mouse_init(void);
mouse_state mouse_read_state(void);

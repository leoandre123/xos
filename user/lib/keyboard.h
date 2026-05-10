#pragma once
#include "cdefs.h"

EXTERN_C_BEGIN

typedef int KeyCode;

typedef struct {
  KeyCode code;
  char character;
} KeyEvent;

EXTERN_C_END

#pragma once
#include <stdint.h>

typedef int KeyCode;

typedef struct {
  KeyCode code;
  char character;
} KeyEvent;

void keyboard_init(void);
KeyEvent keyboard_last(void);